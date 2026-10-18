/*
  Q Light Controller Plus
  showtreeplay_test.cpp

  Copyright (C) Branson Matheson

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QSignalSpy>
#include <QBuffer>
#include <QtTest>

#define private public
#define protected public
#include "mastertimer.h"
#include "universe.h"
#undef protected
#undef private
#include "showtreeplay_test.h"
#include "showtreecontroller.h"
#include "showtreecommand.h"
#include "timecodesource.h"
#include "inputoutputmap.h"
#include "functionparent.h"
#include "qlcfixturedefcache.h"
#include "showcontrol.h"
#include "fixturegroup.h"
#include "showtreemodel.h"
#include "qlcpalette.h"
#include "collection.h"
#include "chaserstep.h"
#include "resource_paths.h"
#include "qlcfile.h"
#include "fixture.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"

using namespace ShowTree;

namespace
{

Target groupTarget(quint32 g)
{
    Target t;
    t.groups << g;
    return t;
}

QByteArray engineXml(Doc *doc)
{
    QByteArray xml;
    QBuffer buffer(&xml);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buffer);
    w.setAutoFormatting(true);
    w.writeStartDocument();
    doc->saveXML(&w);
    w.writeEndDocument();
    return xml;
}

} // namespace

void ShowTreePlay_Test::init()
{
    m_doc = new Doc(this);
    QDir dir(INTERNAL_FIXTUREDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(dir));

    m_fixtures.clear();
    for (int i = 0; i < 8; ++i)
    {
        Fixture *fxi = new Fixture(m_doc);
        fxi->setName(QString("Dim %1").arg(i + 1));
        fxi->setAddress(quint32(i));
        fxi->setUniverse(0);
        fxi->setChannels(1);
        m_doc->addFixture(fxi);
        m_fixtures << fxi->id();
    }
}

void ShowTreePlay_Test::cleanup()
{
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    tick(5);
    delete m_doc;
    m_doc = nullptr;
}

quint32 ShowTreePlay_Test::dimmer(int level, const QString &name)
{
    QLCPalette *p = new QLCPalette(QLCPalette::Dimmer, m_doc);
    p->setName(name);
    p->setValue(level);
    m_doc->addPalette(p);
    return p->id();
}

FixtureGroup *ShowTreePlay_Test::group(const QString &name, const QList<quint32> &fixtures)
{
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName(name);
    m_doc->addFixtureGroup(g);
    for (quint32 f : fixtures)
        g->assignFixture(f);
    return g;
}

quint32 ShowTreePlay_Test::oneListProduction(const QString &name, quint32 groupId, int cues, quint32 level,
                                             quint32 *productionId)
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction(name);
    const quint32 listId = st->addCueList(prod, name + " list");
    for (int i = 0; i < cues; ++i)
    {
        const quint32 c = st->addCue(listId, i - 1, QString("Cue %1").arg(i + 1));
        st->setLayerLook(c, st->addLayer(c, groupTarget(groupId)), st->lookForPalette(level));
    }
    if (productionId != nullptr)
        *productionId = prod;
    return listId;
}

void ShowTreePlay_Test::tick(int n)
{
    for (int i = 0; i < n; ++i)
    {
        m_doc->masterTimer()->timerTick();
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        ua.first()->processFaders();
        m_doc->inputOutputMap()->releaseUniverses(false);
    }
}

QHash<quint32, uchar> ShowTreePlay_Test::settle(int ticks)
{
    tick(ticks);
    QHash<quint32, uchar> out;
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    for (int i = 0; i < m_fixtures.count(); ++i)
        out.insert(quint32(i), uchar(ua.first()->preGMValues().at(i)));
    m_doc->inputOutputMap()->releaseUniverses(false);
    return out;
}

/****************************************************************************
 * P5: multiple productions
 ****************************************************************************/

/* The current production is what a GO with no list plays: its main list's
   chaser becomes the show's main cue list -- the footer GO, ⌘↩ and show
   control all go through ShowControl. */
void ShowTreePlay_Test::theCurrentProductionIsWhatGoPlays()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    FixtureGroup *b = group("B", { m_fixtures[1] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    ShowControl *sc = m_doc->showControl();
    quint32 pa = 0, pb = 0;
    const quint32 la = oneListProduction("Dance", a->id(), 2, full, &pa);
    const quint32 lb = oneListProduction("Corporate", b->id(), 2, full, &pb);

    // Nothing current: GO plays the main cue list as set (none: nothing,
    // there are two chasers).
    QCOMPARE(st->currentProduction(), invalidId());
    QCOMPARE(m_doc->currentProduction(), invalidId());
    QVERIFY(sc->cueList(QString()) == nullptr);

    QSignalSpy changed(st, &ShowTreeController::currentProductionChanged);
    QVERIFY(m_doc->setCurrentProduction(pb));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(st->currentProduction(), pb);
    QCOMPARE(sc->mainCueList(), st->cueList(lb)->chaser);
    QVERIFY(sc->execute({ ShowControl::Go, QString(), QString() }));
    QHash<quint32, uchar> v = settle();
    QCOMPARE(int(v.value(1)), 255);                 // Corporate plays
    QCOMPARE(int(v.value(0)), 0);

    // Switching does not stop what runs; the next GO is the new one's.
    QVERIFY(st->setCurrentProduction(pa));
    QCOMPARE(sc->mainCueList(), st->cueList(la)->chaser);
    QVERIFY(qobject_cast<Chaser *>(m_doc->function(st->cueList(lb)->chaser))->isRunning());
    QVERIFY(sc->execute({ ShowControl::Go, QString(), QString() }));
    v = settle();
    QCOMPARE(int(v.value(0)), 255);
    QCOMPARE(int(v.value(1)), 255);

    // A production that doesn't exist is refused; setting the same one
    // again says nothing.
    QVERIFY(!st->setCurrentProduction(9999));
    QVERIFY(st->setCurrentProduction(pa));
    QCOMPARE(changed.count(), 2);
    // Nothing current again: the main cue list stays what it was.
    QVERIFY(st->setCurrentProduction(invalidId()));
    QCOMPARE(sc->mainCueList(), st->cueList(la)->chaser);
}

/* One source of truth: the main cue list chosen in a chaser editor moves
   the current production with it, a plain chaser unsets it. */
void ShowTreePlay_Test::aMainCueListSetElsewhereMovesTheCurrentProduction()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    ShowControl *sc = m_doc->showControl();
    quint32 pa = 0, pb = 0;
    oneListProduction("Dance", a->id(), 1, full, &pa);
    const quint32 lb = oneListProduction("Corporate", a->id(), 1, full, &pb);
    const quint32 lb2 = st->addCueList(pb, "Encore");
    st->addCue(lb2, -1, "One");

    sc->setMainCueList(st->cueList(lb2)->chaser);
    QCOMPARE(st->currentProduction(), pb);
    QCOMPARE(st->mainList(pb), lb2);            // that list is its main now

    Chaser *plain = new Chaser(m_doc);
    m_doc->addFunction(plain);
    sc->setMainCueList(plain->id());
    QCOMPARE(st->currentProduction(), invalidId());
    QCOMPARE(sc->mainCueList(), plain->id());

    // Edits in the tree don't take the main cue list back while nothing
    // is current.
    st->addCue(lb, 0, "Two");
    QCOMPARE(sc->mainCueList(), plain->id());
}

void ShowTreePlay_Test::aProductionsMainList()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    ShowControl *sc = m_doc->showControl();
    quint32 pa = 0;
    const quint32 first = oneListProduction("Dance", a->id(), 1, full, &pa);
    QCOMPARE(st->mainList(pa), first);          // none set: the first list
    const quint32 second = st->addCueList(pa, "Encore");
    st->addCue(second, -1, "One");
    QCOMPARE(st->playableLists(pa), QList<quint32>({ first, second }));
    st->addLook(pa, "Hit");                     // a Looks section is never a main list
    QCOMPARE(st->playableLists(pa).count(), 2);

    QVERIFY(st->setCurrentProduction(pa));
    QCOMPARE(sc->mainCueList(), st->cueList(first)->chaser);

    // One undo step; GO follows at once.
    QVERIFY(st->setMainList(second));
    QCOMPARE(st->undoText(), QString("Main list"));
    QCOMPARE(st->mainList(pa), second);
    QCOMPARE(sc->mainCueList(), st->cueList(second)->chaser);
    QVERIFY(st->undo());
    QCOMPARE(st->mainList(pa), first);
    QCOMPARE(sc->mainCueList(), st->cueList(first)->chaser);
    QVERIFY(st->redo());
    QCOMPARE(sc->mainCueList(), st->cueList(second)->chaser);

    // Its main list deleted: the first one plays.
    QVERIFY(st->removeCueList(second));
    QCOMPARE(st->mainList(pa), first);
    QCOMPARE(sc->mainCueList(), st->cueList(first)->chaser);
    QCOMPARE(st->currentProduction(), pa);
    // A Looks section can't be one.
    QVERIFY(!st->setMainList(st->looksSection(pa)));
}

/* Each production may follow a timecode source of its own while it is
   current; the show's own setting stays as it is (and is what is saved). */
void ShowTreePlay_Test::eachProductionHasItsTimecodeSource()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    TimecodeSource *tc = m_doc->timecodeSource();
    quint32 pa = 0, pb = 0;
    oneListProduction("Dance", a->id(), 1, full, &pa);
    oneListProduction("Corporate", a->id(), 1, full, &pb);
    tc->setSourceUniverse(1);                   // the show's: universe 2

    QVERIFY(st->setProductionTimecode(pa, 2));
    QCOMPARE(st->production(pa)->timecode, 2);
    QCOMPARE(st->undoText(), QString("Timecode source"));
    // Not current: the show's still applies.
    QCOMPARE(tc->effectiveUniverse(), 1);

    QVERIFY(st->setCurrentProduction(pa));
    QCOMPARE(tc->effectiveUniverse(), 2);
    QCOMPARE(tc->sourceUniverse(), 1);          // the show's own is kept
    tc->updateTimeCode(1, 5000, 30);            // the show's universe: ignored now
    QCOMPARE(tc->positionMs(), quint32(0));
    tc->updateTimeCode(2, 6000, 30);
    QCOMPARE(tc->positionMs(), quint32(6000));

    // Corporate has none: the show's setting again.
    QVERIFY(st->setCurrentProduction(pb));
    QCOMPARE(tc->effectiveUniverse(), 1);
    QVERIFY(st->setProductionTimecode(pb, -1));  // any source
    QCOMPARE(tc->effectiveUniverse(), -1);
    // Undo of its setting follows too.
    QVERIFY(st->undo());
    QCOMPARE(tc->effectiveUniverse(), 1);

    // Saved: the show's own source, not the production's.
    QVERIFY(st->setCurrentProduction(pa));
    const QByteArray xml = engineXml(m_doc);
    QVERIFY(xml.contains("<Timecode Source=\"1\""));
    QVERIFY(xml.contains("Timecode=\"2\""));
}

void ShowTreePlay_Test::productionsSaveAndLoad()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    quint32 pa = 0, pb = 0;
    oneListProduction("Dance", a->id(), 1, full, &pa);
    oneListProduction("Corporate", a->id(), 1, full, &pb);
    const quint32 encore = st->addCueList(pb, "Encore");
    st->addCue(encore, -1, "One");
    QVERIFY(st->setMainList(encore));
    QVERIFY(st->setProductionTimecode(pb, 3));
    QVERIFY(st->setCurrentProduction(pb));

    const QByteArray xml = engineXml(m_doc);
    QVERIFY(xml.contains(QString("Current=\"%1\"").arg(pb).toUtf8()));
    QVERIFY(xml.contains(QString("MainList=\"%1\"").arg(encore).toUtf8()));

    Doc *other = new Doc(this);
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(other->loadXML(r, false));
    ShowTreeController *ost = other->showTree();
    QCOMPARE(ost->currentProduction(), pb);
    QCOMPARE(ost->mainList(pb), encore);
    QCOMPARE(ost->production(pb)->timecode, 3);
    QCOMPARE(ost->production(pa)->timecode, -2);
    QCOMPARE(other->showControl()->mainCueList(), ost->cueList(encore)->chaser);
    QCOMPARE(other->timecodeSource()->effectiveUniverse(), 3);
    delete other;

    // An older file (no Current): nothing current.
    QByteArray old = xml;
    old.replace(QString("Current=\"%1\"").arg(pb).toUtf8(), "");
    Doc *third = new Doc(this);
    QXmlStreamReader r3(old);
    r3.readNextStartElement();
    QVERIFY(third->loadXML(r3, false));
    QCOMPARE(third->showTree()->currentProduction(), invalidId());
    QCOMPARE(third->timecodeSource()->effectiveUniverse(), -1);
    delete third;
}

/* Undo is one history across the productions -- and choosing the current
   production is not on it (what GO plays is the operator's choice). */
void ShowTreePlay_Test::oneUndoHistoryAcrossProductions()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    quint32 pa = 0, pb = 0;
    const quint32 la = oneListProduction("Dance", a->id(), 1, full, &pa);
    const quint32 lb = oneListProduction("Corporate", a->id(), 1, full, &pb);
    QVERIFY(st->setCurrentProduction(pa));

    st->addCue(la, 0, "Dance two");
    st->addCue(lb, 0, "Corporate two");
    QVERIFY(st->setCurrentProduction(pb));
    QCOMPARE(st->undoText(), QString("New cue"));
    QVERIFY(st->undo());                        // Corporate's cue
    QCOMPARE(st->cueList(lb)->cues.count(), 1);
    QCOMPARE(st->cueList(la)->cues.count(), 2);
    QCOMPARE(st->currentProduction(), pb);      // untouched by undo
    QVERIFY(st->undo());                        // Dance's
    QCOMPARE(st->cueList(la)->cues.count(), 1);
    QCOMPARE(st->currentProduction(), pb);
    QVERIFY(st->redo());
    QVERIFY(st->redo());
    QCOMPARE(st->cueList(lb)->cues.count(), 2);
    QCOMPARE(m_doc->showControl()->mainCueList(), st->cueList(lb)->chaser);
}

/* Each production has its own Looks section: its looks bake to its own
   collections, under Productions/<name>/Looks. */
void ShowTreePlay_Test::looksSectionsPerProduction()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    quint32 pa = 0, pb = 0;
    oneListProduction("Dance", a->id(), 1, full, &pa);
    oneListProduction("Corporate", a->id(), 1, full, &pb);
    const quint32 hitA = st->addLook(pa, "Hit");
    const quint32 hitB = st->addLook(pb, "Hit");
    QVERIFY(st->looksSection(pa) != st->looksSection(pb));
    QCOMPARE(st->looksIn(st->looksSection(pa), QString()), QList<quint32>({ hitA }));
    QCOMPARE(st->looksIn(st->looksSection(pb), QString()), QList<quint32>({ hitB }));
    st->setLayerLook(hitA, st->addLayer(hitA, groupTarget(a->id())), st->lookForPalette(full));
    st->setLayerLook(hitB, st->addLayer(hitB, groupTarget(a->id())), st->lookForPalette(full));
    const Function *fa = m_doc->function(st->lookFunction(hitA));
    const Function *fb = m_doc->function(st->lookFunction(hitB));
    QVERIFY(fa != nullptr && fb != nullptr && fa != fb);
    QCOMPARE(fa->path(true), QString("Productions/Dance/Looks"));
    QCOMPARE(fb->path(true), QString("Productions/Corporate/Looks"));
    // A look's section is not a list GO plays.
    QVERIFY(!st->playableLists(pa).contains(st->looksSection(pa)));
}

void ShowTreePlay_Test::deletingTheCurrentProduction()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    quint32 pa = 0;
    oneListProduction("Dance", a->id(), 1, full, &pa);
    QVERIFY(st->setCurrentProduction(pa));
    QVERIFY(st->setProductionTimecode(pa, 2));
    QCOMPARE(m_doc->timecodeSource()->effectiveUniverse(), 2);
    QVERIFY(st->removeProduction(pa));
    QCOMPARE(st->currentProduction(), invalidId());
    QCOMPARE(m_doc->timecodeSource()->effectiveUniverse(), -1);
    QCOMPARE(m_doc->showControl()->mainCueList(), quint32(UINT_MAX));
    // Undo brings the production back, not the choice.
    QVERIFY(st->undo());
    QVERIFY(st->production(pa) != nullptr);
    QCOMPARE(st->currentProduction(), invalidId());
}

void ShowTreePlay_Test::productionCommands()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    quint32 pa = 0;
    const quint32 la = oneListProduction("Dance", a->id(), 1, full, &pa);
    ShowTreeCommand cmd(m_doc, st);
    ShowTreeCommand::Context ctx;
    ctx.production = pa;

    ShowTreeCommand::Result r = cmd.run("production \"Corporate Gig\"", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    const quint32 pb = r.context.production;
    QVERIFY(pb != pa);
    QCOMPARE(st->production(pb)->name, QString("Corporate Gig"));
    QCOMPARE(st->currentProduction(), invalidId());

    r = cmd.run("production Dance current", r.context);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->currentProduction(), pa);
    r = cmd.run("production \"Corporate Gig\" current", r.context);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->currentProduction(), pb);
    r = cmd.run("production none", r.context);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->currentProduction(), invalidId());

    ctx = ShowTreeCommand::Context();
    ctx.production = pa;
    ctx.list = la;
    r = cmd.run("list \"Encore\"", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    const quint32 encore = r.context.list;
    r = cmd.run("main", r.context);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->mainList(pa), encore);
    r = cmd.run("production current", r.context);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->currentProduction(), pa);

    QStringList out;
    QVERIFY(cmd.candidates("production", ctx, &out));
    QVERIFY(out.contains("current") && out.contains("Corporate Gig"));
}

QTEST_MAIN(ShowTreePlay_Test)
