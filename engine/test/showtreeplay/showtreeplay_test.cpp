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
#include <QRandomGenerator>

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
#include "chaseraction.h"
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

    // The first production made is current (nothing else had GO); the
    // second is not.
    QCOMPARE(st->currentProduction(), pa);
    QCOMPARE(m_doc->currentProduction(), pa);
    QCOMPARE(sc->mainCueList(), st->cueList(la)->chaser);

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
    QVERIFY(st->setCurrentProduction(invalidId()));    // the first was made current
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
   collections, under Productions/<name>/Hits. */
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
    QCOMPARE(fa->path(true), QString("Productions/Dance/Hits"));
    QCOMPARE(fb->path(true), QString("Productions/Corporate/Hits"));
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
    QCOMPARE(st->currentProduction(), pa);      // the first stays current

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

/****************************************************************************
 * P6: sub-cues in the tree
 ****************************************************************************/

void ShowTreePlay_Test::go(Chaser *c)
{
    ChaserAction action;
    action.m_action = ChaserNextStep;
    action.m_stepIndex = -1;
    action.m_masterIntensity = 1.0;
    action.m_stepIntensity = 1.0;
    action.m_fadeMode = Chaser::FromFunction;
    c->setAction(action);
    if (!c->isRunning())
        c->start(m_doc->masterTimer(), FunctionParent::master());
}

QList<quint32> ShowTreePlay_Test::subCueShow()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    FixtureGroup *b = group("B", { m_fixtures[1] });
    const quint32 half = dimmer(127, "Half");
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Dance");
    const quint32 listId = st->addCueList(prod, "Song");
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    const quint32 c3 = st->addCue(listId, 1, "Three");
    st->setTargetLook(c1, groupTarget(a->id()), st->lookForPalette(half));
    st->setTargetLook(c2, groupTarget(a->id()), st->lookForPalette(full));
    st->setTargetLook(c2, groupTarget(b->id()), st->lookForPalette(half));
    const quint32 s1 = st->addSubCue(c2, "Hit");
    const quint32 s2 = st->addSubCue(c2, "Down");
    st->setTargetLook(s1, groupTarget(b->id()), st->lookForPalette(full));
    st->setTargetLook(s2, groupTarget(a->id()), st->lookForPalette(half));
    return { listId, c1, c2, c3, s1, s2 };
}

/* §3C, D9, D25: 2.1 starts from cue 2's output, 2.2 from 2.1's; cue 3
   starts from cue 2's own state, however far Sub GO got. */
void ShowTreePlay_Test::subCuesTrackFromTheirCueAndAreCueOnly()
{
    const QList<quint32> ids = subCueShow();
    ShowTreeController *st = m_doc->showTree();
    const quint32 subList = st->subCueList(ids[2]);
    QVERIFY(subList != invalidId());
    const CueList *subs = st->cueList(subList);
    QVERIFY(subs->isSubCues());
    QCOMPARE(subs->subOf, ids[2]);
    QCOMPARE(subs->cues.count(), 2);
    quint32 parent = 0;
    QVERIFY(st->isSubCue(ids[4], &parent));
    QCOMPARE(parent, ids[2]);
    QVERIFY(!st->isSubCue(ids[2]));
    QCOMPARE(st->cueNumber(*subs, 0), QString("2.1"));
    QCOMPARE(st->cueNumber(*subs, 1), QString("2.2"));
    QCOMPARE(st->cueLabel(*subs, 0), QString("2.1 Hit"));

    const FixtureGroup *a = m_doc->fixtureGroups().at(0);
    const FixtureGroup *b = m_doc->fixtureGroups().at(1);

    const QList<ResolvedCue> rs = st->resolved(*subs);
    QCOMPARE(rs.count(), 2);
    // 2.1: A tracked from cue 2, B its own.
    const ResolvedLayer *a1 = rs[0].layer(groupTarget(a->id()));
    const ResolvedLayer *b1 = rs[0].layer(groupTarget(b->id()));
    QVERIFY(a1 != nullptr && b1 != nullptr);
    QVERIFY(a1->values.value(KeyDimmer).origin == Origin::Tracked);
    QCOMPARE(a1->values.value(KeyDimmer).fromCue, ids[2]);
    QVERIFY(b1->own);
    QVERIFY(b1->values.value(KeyDimmer).origin == Origin::Own);
    // 2.2: B "from sub-cue 2.1", A its own.
    const ResolvedLayer *b2 = rs[1].layer(groupTarget(b->id()));
    QVERIFY(b2->values.value(KeyDimmer).origin == Origin::Tracked);
    QCOMPARE(b2->values.value(KeyDimmer).fromCue, ids[4]);
    QVERIFY(rs[1].layer(groupTarget(a->id()))->own);

    // Cue 3 starts from cue 2's own state: B Half, A Full (not 2.2's Half).
    const CueList *song = st->cueList(ids[0]);
    const QList<ResolvedCue> main = st->resolved(*song);
    QCOMPARE(main.count(), 3);                  // the sub-cues aren't the list's cues
    const ResolvedLayer *a3 = main[2].layer(groupTarget(a->id()));
    const ResolvedLayer *b3 = main[2].layer(groupTarget(b->id()));
    QCOMPARE(a3->values.value(KeyDimmer).fromCue, ids[2]);
    QCOMPARE(b3->values.value(KeyDimmer).fromCue, ids[2]);
    QCOMPARE(a3->values.value(KeyDimmer).look, main[1].layer(groupTarget(a->id()))->values.value(KeyDimmer).look);
}

void ShowTreePlay_Test::subCuesBakeToAHeldChaser()
{
    const QList<quint32> ids = subCueShow();
    ShowTreeController *st = m_doc->showTree();
    const CueList *song = st->cueList(ids[0]);
    const CueList *subs = st->cueList(st->subCueList(ids[2]));
    Chaser *outer = qobject_cast<Chaser *>(m_doc->function(song->chaser));
    QVERIFY(outer != nullptr);
    QCOMPARE(outer->stepsCount(), 3);
    Chaser *inner = qobject_cast<Chaser *>(m_doc->function(subs->chaser));
    QVERIFY(inner != nullptr);
    QVERIFY(inner != outer);
    QVERIFY(!inner->isVisible());               // generated, hidden
    QVERIFY(st->isGenerated(inner->id()));
    QCOMPARE(inner->path(true), QString("Productions/Dance/Song"));
    QCOMPARE(inner->name(), QString("2 Two · sub-cues"));
    // Cue 2's step plays them, held on Sub GO.
    QCOMPARE(outer->steps().at(1).fid, inner->id());
    QCOMPARE(outer->steps().at(1).subCues, ChaserStep::SubGo);
    QVERIFY(outer->isSubGoStep(1));
    QCOMPARE(outer->subCueCount(1), 2);
    QCOMPARE(outer->subCue(1, 0), QString("2.1"));
    QCOMPARE(outer->subCue(1, 1), QString("2.2"));
    QCOMPARE(outer->steps().at(0).fid, song->cues[0].collection);
    // Each sub-cue a Collection holding its full state; holds wait.
    QCOMPARE(inner->stepsCount(), 2);
    QCOMPARE(inner->steps().at(0).fid, subs->cues[0].collection);
    QCOMPARE(inner->steps().at(0).hold, Function::infiniteSpeed());
    const Collection *c21 = qobject_cast<const Collection *>(m_doc->function(subs->cues[0].collection));
    QCOMPARE(c21->name(), QString("2.1-Hit"));
    QCOMPARE(c21->functions().count(), 2);      // A tracked, B its own
    quint32 list = 0, cue = 0;
    QVERIFY(st->generatedNode(inner->id(), &list, &cue));
    QCOMPARE(list, ids[0]);
    QCOMPARE(cue, ids[2]);

    // Auto-follow on 2.2 (0.5 s): 2.1's hold, and SubGoFollow.
    QVERIFY(st->setSubCueFollow(ids[5], true, 500));
    QCOMPARE(st->undoText(), QString("Auto-follow"));
    QCOMPARE(inner->steps().at(0).hold, 500u);
    QCOMPARE(inner->steps().at(1).hold, Function::infiniteSpeed());
    QCOMPARE(outer->steps().at(1).subCues, ChaserStep::SubGoFollow);
    QVERIFY(st->setSubCueFollow(ids[5], false, 0));
    QCOMPARE(outer->steps().at(1).subCues, ChaserStep::SubGo);

    // A bake straight after changes nothing. (An edit replaces the tree:
    // what `song` pointed at is gone, so by id.)
    const quint32 before = inner->id();
    st->bake();
    QCOMPARE(qobject_cast<Chaser *>(m_doc->function(st->cueList(ids[0])->chaser))->steps().at(1).fid, before);

    // A note goes on the step: a sub-cue's on the Sub GO chaser's, a cue's
    // on the list's.
    ShowTree::Cue sub = *st->cue(ids[4]);
    sub.note = "On the downbeat";
    QVERIFY(st->setCue(sub));
    ShowTree::Cue first = *st->cue(ids[1]);
    first.note = "House to half";
    QVERIFY(st->setCue(first));
    Chaser *song2 = qobject_cast<Chaser *>(m_doc->function(st->cueList(ids[0])->chaser));
    Chaser *subs2 = qobject_cast<Chaser *>(m_doc->function(st->cueList(st->subCueList(ids[2]))->chaser));
    QVERIFY(song2 != nullptr && subs2 != nullptr);
    QCOMPARE(subs2->steps().at(0).note, QString("On the downbeat"));
    QVERIFY(subs2->steps().at(1).note.isEmpty());
    QCOMPARE(song2->steps().at(0).note, QString("House to half"));
}

void ShowTreePlay_Test::subGoPlaysThemOnTheRig()
{
    const QList<quint32> ids = subCueShow();
    ShowTreeController *st = m_doc->showTree();
    Chaser *outer = qobject_cast<Chaser *>(m_doc->function(st->cueList(ids[0])->chaser));
    go(outer);
    QHash<quint32, uchar> v = settle();
    QCOMPARE(int(v[0]), 127);                   // cue 1
    QCOMPARE(int(v[1]), 0);
    go(outer);                                  // cue 2 = 2.1 with GO
    v = settle();
    QCOMPARE(outer->currentStepIndex(), 1);
    QCOMPARE(outer->currentSubCueIndex(), 0);
    QCOMPARE(int(v[0]), 255);
    QCOMPARE(int(v[1]), 255);
    outer->subGo();                             // 2.2
    v = settle();
    QCOMPARE(outer->currentSubCueIndex(), 1);
    QCOMPARE(int(v[0]), 127);
    QCOMPARE(int(v[1]), 255);
    outer->subGo();                             // the end: nothing
    v = settle();
    QCOMPARE(outer->currentSubCueIndex(), 1);
    outer->subBack();                           // 2.1 again
    v = settle();
    QCOMPARE(int(v[0]), 255);
    go(outer);                                  // cue 3: from cue 2's own state
    v = settle();
    QCOMPARE(outer->currentStepIndex(), 2);
    QCOMPARE(int(v[0]), 255);
    QCOMPARE(int(v[1]), 127);
}

void ShowTreePlay_Test::autoFollowSubCuesGoOnByThemselves()
{
    const QList<quint32> ids = subCueShow();
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->setSubCueFollow(ids[5], true, 200));
    Chaser *outer = qobject_cast<Chaser *>(m_doc->function(st->cueList(ids[0])->chaser));
    go(outer);
    settle();
    go(outer);                                  // 2.1, then 2.2 by itself 0.2 s later
    QHash<quint32, uchar> v = settle(3);
    QCOMPARE(outer->currentSubCueIndex(), 0);
    QCOMPARE(int(v[0]), 255);
    v = settle(20);
    QCOMPARE(outer->currentSubCueIndex(), 1);
    QCOMPARE(int(v[0]), 127);
    // And it waits there for GO (the last one holds).
    v = settle(30);
    QCOMPARE(outer->currentStepIndex(), 1);
    QCOMPARE(outer->currentSubCueIndex(), 1);
    // Pause holds the auto-follow timer.
    outer->subBack();
    settle(2);
    QCOMPARE(outer->currentSubCueIndex(), 0);
    outer->setPause(true);
    settle(30);
    QCOMPARE(outer->currentSubCueIndex(), 0);
    outer->setPause(false);
    settle(30);
    QCOMPARE(outer->currentSubCueIndex(), 1);
}

/* The hand-over (§4 spike) through a cue with sub-cues: a scene cue 1 and
   2.1 share keeps running across GO (no re-fade, no stop), and one 2.2 and
   cue 3 share keeps running across the GO out of the sub-cues. */
void ShowTreePlay_Test::aSceneSharedThroughSubCuesNeverRefades()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    FixtureGroup *b = group("B", { m_fixtures[1] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Dance");
    const quint32 listId = st->addCueList(prod, "Song");
    const quint32 c1 = st->addCue(listId, -1, "One");
    const quint32 c2 = st->addCue(listId, 0, "Two");
    st->addCue(listId, 1, "Three");
    st->setTargetLook(c1, groupTarget(a->id()), st->lookForPalette(full));
    const quint32 s1 = st->addSubCue(c2, "Hit");
    st->setTargetLook(s1, groupTarget(b->id()), st->lookForPalette(full));
    st->addSubCue(c2, "More");

    const CueList *subs = st->cueList(st->subCueList(c2));
    // A's scene is in cue 1, 2.1, 2.2 and cue 3 (it tracks).
    const quint32 sceneA = st->layerScene(c1, groupTarget(a->id()));
    QVERIFY(sceneA != invalidId());
    QCOMPARE(st->layerScene(subs->cues[0].id, groupTarget(a->id())), sceneA);
    Function *scene = m_doc->function(sceneA);

    Chaser *outer = qobject_cast<Chaser *>(m_doc->function(st->cueList(listId)->chaser));
    go(outer);
    settle();
    QVERIFY(scene->isRunning());
    quint32 last = scene->elapsed();
    QSignalSpy stopped(scene, SIGNAL(stopped(quint32)));
    auto watch = [&](int ticks) {
        for (int i = 0; i < ticks; ++i)
        {
            tick();
            QVERIFY(scene->isRunning());
            QVERIFY2(scene->elapsed() > last, "the shared scene restarted");
            last = scene->elapsed();
            QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
            const int lvl = uchar(ua.first()->preGMValues().at(0));
            m_doc->inputOutputMap()->releaseUniverses(false);
            QCOMPARE(lvl, 255);
        }
    };
    go(outer);                                  // into 2.1
    watch(10);
    outer->subGo();                             // 2.2
    watch(10);
    go(outer);                                  // out to cue 3
    watch(10);
    QCOMPARE(stopped.count(), 0);
    QCOMPARE(outer->currentStepIndex(), 2);
}

void ShowTreePlay_Test::subCuesSaveLoadUndoAndDelete()
{
    const QList<quint32> ids = subCueShow();
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->setSubCueFollow(ids[5], true, 1500));
    const QByteArray xml = engineXml(m_doc);
    QVERIFY(xml.contains("<SubCues"));
    QVERIFY(xml.contains("Follow=\"Auto\" Wait=\"1500\""));
    QVERIFY(xml.contains("SubCues=\"SubGoFollow\""));

    Doc *other = new Doc(this);
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(other->loadXML(r, false));
    ShowTreeController *ost = other->showTree();
    QCOMPARE(ost->subCueList(ids[2]), st->subCueList(ids[2]));
    const CueList *subs = ost->cueList(ost->subCueList(ids[2]));
    QCOMPARE(subs->cues.count(), 2);
    QVERIFY(subs->cues[1].follow);
    QCOMPARE(subs->cues[1].wait, 1500u);
    // No bake on load, and a bake changes nothing.
    QByteArray before;
    {
        QBuffer buf(&before);
        buf.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buf);
        for (Function *f : other->functions())
            f->saveXML(&w);
    }
    ost->bake();
    QByteArray after;
    {
        QBuffer buf(&after);
        buf.open(QIODevice::WriteOnly);
        QXmlStreamWriter w(&buf);
        for (Function *f : other->functions())
            f->saveXML(&w);
    }
    QCOMPARE(after, before);
    delete other;

    // A sub-cue added is one undo step; so is the first (it makes the list).
    const int n = int(st->cueList(st->subCueList(ids[2]))->cues.count());
    const quint32 s3 = st->addSubCue(ids[2], "Three");
    QCOMPARE(st->undoText(), QString("New sub-cue"));
    QCOMPARE(st->cueList(st->subCueList(ids[2]))->cues.count(), n + 1);
    QVERIFY(st->undo());
    QVERIFY(st->cue(s3) == nullptr);
    const quint32 lone = st->addSubCue(ids[3], "Only");
    QVERIFY(st->subCueList(ids[3]) != invalidId());
    QVERIFY(st->undo());
    QCOMPARE(st->subCueList(ids[3]), invalidId());
    QVERIFY(st->cue(lone) == nullptr);

    // Deleting the last sub-cue: the cue plays plainly again.
    const quint32 innerId = st->cueList(st->subCueList(ids[2]))->chaser;
    QVERIFY(st->removeCue(ids[4]));
    QVERIFY(st->removeCue(ids[5]));
    QCOMPARE(st->subCueList(ids[2]), invalidId());
    Chaser *outer = qobject_cast<Chaser *>(m_doc->function(st->cueList(ids[0])->chaser));
    QCOMPARE(outer->steps().at(1).fid, st->cue(ids[2])->collection);
    QCOMPARE(outer->steps().at(1).subCues, ChaserStep::Auto);
    QVERIFY(m_doc->function(innerId) == nullptr);
    // Deleting the cue takes its sub-cues; undo brings both.
    QVERIFY(st->undo());
    QVERIFY(st->undo());
    QVERIFY(st->subCueList(ids[2]) != invalidId());
    QVERIFY(st->removeCue(ids[2]));
    QCOMPARE(st->subCueList(ids[2]), invalidId());
    QVERIFY(st->cue(ids[4]) == nullptr);
    QVERIFY(st->undo());
    QVERIFY(st->cue(ids[4]) != nullptr);
    QCOMPARE(qobject_cast<Chaser *>(m_doc->function(st->cueList(ids[0])->chaser))->steps().at(1).subCues,
             ChaserStep::SubGoFollow);
}

/* D23: a new cue never takes a number a sub-cue has. */
void ShowTreePlay_Test::numberingSkipsSubCueNumbers()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Dance");
    const quint32 listId = st->addCueList(prod, "Song");
    const quint32 c4 = st->addCueNumbered(listId, "4", "Four");
    st->addCueNumbered(listId, "5", "Five");
    for (int i = 0; i < 5; ++i)
        st->addSubCue(c4, QString("s%1").arg(i + 1));
    QCOMPARE(ShowTreeController::takenNumbers(st->productions(), *st->cueList(listId)),
             QStringList({ "4.1", "4.2", "4.3", "4.4", "4.5" }));
    const quint32 between = st->addCue(listId, 0, "Between");
    QCOMPARE(st->cue(between)->number, QString("4.6"));   // 4.5 is a sub-cue's
}

void ShowTreePlay_Test::subCuesAreRefusedWhereTheyCantBe()
{
    const QList<quint32> ids = subCueShow();
    ShowTreeController *st = m_doc->showTree();
    QString why;
    // One level only.
    QCOMPARE(st->addSubCue(ids[4], "Deeper", invalidId(), &why), invalidId());
    QVERIFY(why.contains("one level"));
    // Not in a look.
    const quint32 look = st->addLook(st->cueList(ids[0]) == nullptr ? 0 : st->productions().first().id, "Hit");
    QCOMPARE(st->addSubCue(look, "No", invalidId(), &why), invalidId());
    // No mode, base or includes for a sub-cue.
    const quint32 subList = st->subCueList(ids[2]);
    QVERIFY(!st->setCueMode(ids[4], CueMode::Block));
    QVERIFY(!st->setBaseCue(subList, ids[4]));
    QVERIFY(!st->addInclude(ids[5], ids[4], &why));
    QVERIFY(!st->setListMode(subList, ListMode::NonTracking));
    // Convert and Detach leave a list with sub-cues alone.
    QVERIFY(!st->planConvertToTracking(ids[0]).ok);
    QCOMPARE(st->detachList(ids[0]), Function::invalidId());
    // Sub-cues are not a list GO plays, nor running lists.
    quint32 pid = invalidId();
    st->cueList(ids[0], &pid);
    QVERIFY(!st->playableLists(pid).contains(subList));
    // In this cue list counts what the sub-cues use.
    const quint32 full = m_doc->palettes().at(1)->id();
    QVERIFY(st->usageInList(ids[0]).palettes.value(full) >= 2);
}

void ShowTreePlay_Test::subCueCommands()
{
    const QList<quint32> ids = subCueShow();
    ShowTreeController *st = m_doc->showTree();
    ShowTreeCommand cmd(m_doc, st);
    ShowTreeCommand::Context ctx;
    ctx.production = st->productions().first().id;
    ctx.list = ids[0];
    ctx.cue = ids[3];

    ShowTreeCommand::Result r = cmd.run("sub \"Hit\"", ctx);
    QVERIFY2(r.ok, qPrintable(r.message));
    const quint32 hit = r.context.cue;
    QVERIFY(st->isSubCue(hit));
    QCOMPARE(r.context.list, st->subCueList(ids[3]));
    r = cmd.run("sub + \"Sweep\" follow 0.5", r.context);
    QVERIFY2(r.ok, qPrintable(r.message));
    const Cue *sweep = st->cue(r.context.cue);
    QVERIFY(sweep->follow);
    QCOMPARE(sweep->wait, 500u);
    r = cmd.run("subgo", r.context);
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY(!st->cue(r.context.cue)->follow);
    r = cmd.run("follow 1.5", r.context);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(st->cue(r.context.cue)->wait, 1500u);
    // A layer in a sub-cue, typed as in any cue.
    r = cmd.run("B dimmer Half", r.context);
    QVERIFY2(r.ok, qPrintable(r.message));
    // cue <number> finds a sub-cue.
    r = cmd.run("cue 2.2", r.context);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(r.context.cue, ids[5]);
    QCOMPARE(r.context.list, st->subCueList(ids[2]));
    r = cmd.run("cue 3", r.context);
    QVERIFY2(r.ok, qPrintable(r.message));
    QCOMPARE(r.context.cue, ids[3]);
    // Not on a cue that isn't a sub-cue.
    r = cmd.run("follow 1", r.context);
    QVERIFY(!r.ok);
    // One undo step per command.
    const int steps = int(st->cueList(st->subCueList(ids[3]))->cues.count());
    QCOMPARE(steps, 2);
    QStringList out;
    QVERIFY(cmd.candidates("sub \"x\"", r.context, &out));
    QVERIFY(out.contains("follow"));
}

/****************************************************************************
 * P7: the cue runner
 ****************************************************************************/

/* QLab / OSC / MSC drive the runner through ShowControl (SUBCUES §4):
   /subgo, /subback, GO and LOAD by a sub-cue's number, a request to one
   list by its chaser (a Cue Runner following it). */
void ShowTreePlay_Test::showControlStepsThroughSubCues()
{
    const QList<quint32> ids = subCueShow();
    ShowTreeController *st = m_doc->showTree();
    ShowControl *sc = m_doc->showControl();
    quint32 pid = invalidId();
    st->cueList(ids[0], &pid);
    QVERIFY(st->setCurrentProduction(pid));
    Chaser *outer = qobject_cast<Chaser *>(m_doc->function(st->cueList(ids[0])->chaser));

    // OSC addresses parse.
    auto osc = [](const QByteArray &address) {
        QByteArray d = address;
        d.append('\0');
        while (d.size() % 4)
            d.append('\0');
        d.append(",\0\0\0", 4);
        return d;
    };
    QList<ShowControl::Request> rs = ShowControl::parseOsc(osc("/subgo"));
    QCOMPARE(rs.count(), 1);
    QCOMPARE(int(rs.first().command), int(ShowControl::SubGo));
    rs = ShowControl::parseOsc(osc("/qlcconsole/subback"));
    QCOMPARE(int(rs.first().command), int(ShowControl::SubBack));

    // Sub GO on a stopped list: nothing.
    QVERIFY(!sc->execute({ ShowControl::SubGo, QString(), QString() }));
    // GO 2.2 by number: its cue, then that sub-cue.
    QVERIFY(sc->execute({ ShowControl::Go, "2.2", QString() }));
    settle();
    QCOMPARE(outer->currentStepIndex(), 1);
    QCOMPARE(outer->currentSubCueIndex(), 1);
    QCOMPARE(sc->handleOsc(osc("/subback")), 1);
    settle();
    QCOMPARE(outer->currentSubCueIndex(), 0);
    QVERIFY(!sc->execute({ ShowControl::SubBack, QString(), QString() }));    // the start
    QVERIFY(sc->execute({ ShowControl::SubGo, QString(), QString() }));
    settle();
    QCOMPARE(outer->currentSubCueIndex(), 1);
    QVERIFY(!sc->execute({ ShowControl::SubGo, QString(), QString() }));      // the end

    // LOAD 2.1 then GO: there.
    QVERIFY(sc->execute({ ShowControl::Go, "3", QString() }));
    settle();
    QCOMPARE(outer->currentStepIndex(), 2);
    QVERIFY(sc->execute({ ShowControl::Load, "2.1", QString() }));
    QCOMPARE(sc->loadedStep(outer->id()), 1);
    QCOMPARE(sc->loadedSubCue(outer->id()), 0);
    QVERIFY(sc->execute({ ShowControl::Go, QString(), QString() }));
    settle();
    QCOMPARE(outer->currentStepIndex(), 1);
    QCOMPARE(outer->currentSubCueIndex(), 0);
    QCOMPARE(sc->loadedStep(outer->id()), -1);

    // A request to a list by its chaser (not the main one).
    const quint32 other = st->addCueList(pid, "Encore");
    st->addCue(other, -1, "E1");
    Chaser *enc = qobject_cast<Chaser *>(m_doc->function(st->cueList(other)->chaser));
    ShowControl::Request r { ShowControl::Go, QString(), QString() };
    r.chaser = enc->id();
    QVERIFY(sc->execute(r));
    settle();
    QVERIFY(enc->isRunning());
    QCOMPARE(sc->mainCueList(), outer->id());   // still the main one
}

void ShowTreePlay_Test::whatChangesComesFromTheResolvedTracking()
{
    const QList<quint32> ids = subCueShow();
    ShowTreeController *st = m_doc->showTree();
    // Cue 1 -> cue 2: A Half -> Full, B in at Half.
    QCOMPARE(st->changeLine(ids[1], ids[2]), QString("A → Full; B in → Half"));
    // From dark: everything in.
    QCOMPARE(st->changeLine(invalidId(), ids[1]), QString("A in → Half"));
    // Cue 2 -> 2.1: only B.
    QCOMPARE(st->changeLine(ids[2], ids[4]), QString("B → Full"));
    // 2.1 -> 2.2: only A; B unchanged is not listed.
    QCOMPARE(st->changeLine(ids[4], ids[5]), QString("A → Half"));
    // 2.2 -> 3: back to cue 2's own state (sub-cues are cue-only).
    QCOMPARE(st->changeLine(ids[5], ids[3]), QString("A → Full; B → Half"));
    // Nothing differs.
    QCOMPARE(st->changeLine(ids[2], ids[3]), QString());
    // A layer that goes.
    const FixtureGroup *b = m_doc->fixtureGroups().at(1);
    const quint32 c4 = st->addCue(ids[0], 2, "Block");
    QVERIFY(st->setCueMode(c4, CueMode::Block));
    st->setTargetLook(c4, groupTarget(m_doc->fixtureGroups().at(0)->id()),
                      st->lookForPalette(m_doc->palettes().at(0)->id()));
    QCOMPARE(st->changeLine(ids[3], c4), QString("A → Half; %1 out").arg(b->name()));
}

void ShowTreePlay_Test::cueOfStepMapsTheChaserBack()
{
    const QList<quint32> ids = subCueShow();
    ShowTreeController *st = m_doc->showTree();
    QCOMPARE(st->cueOfStep(ids[0], 0), ids[1]);
    QCOMPARE(st->cueOfStep(ids[0], 1), ids[2]);
    QCOMPARE(st->cueOfStep(ids[0], 1, 0), ids[4]);
    QCOMPARE(st->cueOfStep(ids[0], 1, 1), ids[5]);
    QCOMPARE(st->cueOfStep(ids[0], 1, 2), invalidId());
    QCOMPARE(st->cueOfStep(ids[0], 9), invalidId());
    quint32 listId = invalidId();
    QVERIFY(st->productionOfChaser(st->cueList(ids[0])->chaser, &listId) != invalidId());
    QCOMPARE(listId, ids[0]);
    // The sub-cues' own chaser is not a list of the production.
    QCOMPARE(st->productionOfChaser(st->cueList(st->subCueList(ids[2]))->chaser), invalidId());
}

/* A new production is current when nothing else has GO -- no production
   current, no chaser as the main cue list -- so a first production plays
   without a trip to Make current. It never takes GO from a choice. */
void ShowTreePlay_Test::aNewProductionIsCurrentWhenNothingElseHasGo()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    ShowControl *sc = m_doc->showControl();
    QCOMPARE(st->currentProduction(), invalidId());

    // The first: current, and GO plays its list once it has one.
    quint32 pa = 0;
    const quint32 la = oneListProduction("Dance", a->id(), 1, full, &pa);
    QCOMPARE(st->currentProduction(), pa);
    QCOMPARE(sc->mainCueList(), st->cueList(la)->chaser);

    // Another while one is current: not current.
    const quint32 pb = st->addProduction("Corporate");
    QCOMPARE(st->currentProduction(), pa);

    // "None" leaves the main cue list on Dance's list: GO still plays it, so
    // a new production doesn't take it.
    QVERIFY(st->setCurrentProduction(invalidId()));
    const quint32 pc = st->addProduction("Encore");
    QCOMPARE(st->currentProduction(), invalidId());
    QCOMPARE(sc->mainCueList(), st->cueList(la)->chaser);

    // A plain chaser chosen as the main cue list keeps GO too.
    Chaser *plain = new Chaser(m_doc);
    m_doc->addFunction(plain);
    sc->setMainCueList(plain->id());
    st->addProduction("Matinee");
    QCOMPARE(st->currentProduction(), invalidId());
    QCOMPARE(sc->mainCueList(), plain->id());

    // Nothing has GO once that chaser is gone: the next one made is current,
    // and undoing it leaves none.
    m_doc->deleteFunction(plain->id());
    const quint32 pd = st->addProduction("Gala");
    QCOMPARE(st->currentProduction(), pd);
    QVERIFY(st->undo());
    QVERIFY(st->production(pd) == nullptr);
    QCOMPARE(st->currentProduction(), invalidId());
    Q_UNUSED(pb)
    Q_UNUSED(pc)
}

/****************************************************************************
 * The bake re-plans only what changed; hits fade
 ****************************************************************************/

/* What the tree baked, as the rig would play it: per cue, its chaser step
   and its collection's members, each scene by what it lights and plays
   (groups, fixtures, palettes, what each leaves out, its layout spans),
   and the scene each of its layers plays. Ids of generated scenes are left
   out: two bakes that make the same scenes are the same. */
static QString describeScene(const Doc *doc, quint32 fid)
{
    const Function *f = doc->function(fid);
    const Scene *s = qobject_cast<const Scene *>(f);
    if (s == nullptr)
        return f == nullptr ? QString("-") : QString("f%1").arg(fid);
    auto ids = [](QList<quint32> l, bool sort) {
        if (sort)
            std::sort(l.begin(), l.end());
        QStringList out;
        for (quint32 i : l)
            out << QString::number(i);
        return out.join(',');
    };
    QString d = QString("g[%1] f[%2] p[%3]").arg(ids(s->fixtureGroups(), true), ids(s->fixtures(), true),
                                                 ids(s->palettes(), false));
    for (quint32 pid : s->palettes())
    {
        if (!s->excludedFixtures(pid).isEmpty())
            d += QString(" x%1[%2]").arg(pid).arg(ids(s->excludedFixtures(pid), true));
        const Scene::LayoutSpan span = s->layoutSpan(pid);
        if (!span.groups.isEmpty() || !span.fixtures.isEmpty())
            d += QString(" w%1[%2/%3]").arg(pid).arg(ids(span.groups, true), ids(span.fixtures, true));
    }
    return d;
}

static QStringList bakedState(const Doc *doc, ShowTreeController *st)
{
    QStringList out;
    for (const ShowTree::Production &p : st->productions())
        for (const ShowTree::CueList &l : p.lists)
        {
            const Chaser *ch = qobject_cast<const Chaser *>(doc->function(l.chaser));
            QList<ShowTree::ResolvedCue> rcs = st->resolved(l);
            for (int i = 0; i < l.cues.count(); ++i)
            {
                const ShowTree::Cue &c = l.cues.at(i);
                QString line = QString("%1/%2:").arg(l.name).arg(c.id);
                if (ch != nullptr && i < ch->stepsCount())
                {
                    const ChaserStep step = ch->steps().at(i);
                    line += QString(" step(%1 %2/%3/%4)").arg(step.fid == c.collection ? "own" : "other")
                                .arg(step.fadeIn).arg(step.hold).arg(step.fadeOut);
                }
                const Collection *coll = qobject_cast<const Collection *>(doc->function(c.collection));
                if (coll != nullptr)
                    for (quint32 fid : coll->functions())
                        line += " {" + describeScene(doc, fid) + "}";
                if (i < rcs.count())
                    for (const ShowTree::ResolvedLayer &rl : rcs.at(i).layers)
                        line += QString(" L%1=").arg(rl.target.signature())
                                + describeScene(doc, st->layerScene(c.id, rl.target));
                out << line;
            }
        }
    return out;
}

/* An edit re-plans only the cues whose resolved state it changes (the
   edited cue, and those tracking from it until a block or a hard value),
   and the show bakes exactly as a full bake would -- over a seeded run of
   random edits: looks, offs, modes, releases, includes, mixing, inserts,
   deletes, moves, skips, undo and redo, and a group's members changing
   under the tree. */
void ShowTreePlay_Test::incrementalBakeMatchesAFullBake()
{
    QList<FixtureGroup *> groups;
    groups << group("A", { m_fixtures[0], m_fixtures[1], m_fixtures[2], m_fixtures[3] })
           << group("B", { m_fixtures[2], m_fixtures[3], m_fixtures[4], m_fixtures[5] })
           << group("C", { m_fixtures[4], m_fixtures[5], m_fixtures[6], m_fixtures[7] })
           << group("All", m_fixtures);
    QList<quint32> levels;
    for (int i = 0; i < 4; ++i)
        levels << dimmer(40 + 60 * i, QString("L%1").arg(i));
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Random");
    const quint32 listId = st->addCueList(prod, "Song");
    st->beginBatch("Build");
    for (int i = 0; i < 24; ++i)
    {
        const quint32 c = st->addCue(listId, i - 1, QString("Cue %1").arg(i + 1));
        st->setTargetLook(c, groupTarget(groups.at(i % groups.count())->id()),
                          st->lookForPalette(levels.at(i % levels.count())));
    }
    st->endBatch(true);
    QCOMPARE(st->cueList(listId)->cues.count(), 24);

    QRandomGenerator rng(20261030);
    auto anyCue = [&]() -> quint32 {
        const ShowTree::CueList *l = st->cueList(listId);
        return l->cues.isEmpty() ? ShowTree::invalidId()
                                 : l->cues.at(rng.bounded(int(l->cues.count()))).id;
    };
    auto anyGroup = [&]() { return groups.at(rng.bounded(int(groups.count())))->id(); };
    int planned = 0, cues = 0, edits = 0;
    for (int n = 0; n < 400; ++n)
    {
        const quint32 c = anyCue();
        if (c == ShowTree::invalidId())
            break;
        const int op = rng.bounded(14);
        switch (op)
        {
            case 0: case 1: case 2: case 3:
            {
                // One draw per statement: the order a compiler evaluates
                // call arguments in must not change the sequence.
                const quint32 g = anyGroup();
                const quint32 level = levels.at(rng.bounded(int(levels.count())));
                st->setTargetLook(c, groupTarget(g), st->lookForPalette(level));
            }
            break;
            case 4:
            {
                ShowTree::Look off;
                off.key = ShowTree::KeyDimmer;
                off.none = true;
                st->setTargetLook(c, groupTarget(anyGroup()), off);
            }
            break;
            case 5:
                st->setCueMode(c, ShowTree::CueMode(rng.bounded(4)));
            break;
            case 6:
                st->setTargetRelease(c, groupTarget(anyGroup()), rng.bounded(2) == 1);
            break;
            case 7:
                st->addInclude(c, anyCue());
            break;
            case 8:
                st->setCueMixOverlap(c, rng.bounded(2) == 1);
            break;
            case 9:
            {
                const ShowTree::CueList *l = st->cueList(listId);
                if (rng.bounded(2) == 0 || l->cues.count() < 8)
                {
                    const int at = rng.bounded(int(l->cues.count()));
                    l = nullptr;    // the edits below replace the tree's lists
                    const quint32 added = st->addCue(listId, at, "New");
                    st->setTargetLook(added, groupTarget(anyGroup()), st->lookForPalette(levels.first()));
                }
                else
                    st->removeCue(c);
            }
            break;
            case 10:
                st->moveCue(c, rng.bounded(int(st->cueList(listId)->cues.count())));
            break;
            case 11:
                st->setCuesSkipped({ c }, rng.bounded(3) == 0);
            break;
            case 12:
                if (rng.bounded(2) == 0)
                    st->undo();
                else
                    st->redo();
            break;
            case 13:
            {
                // Under the tree: a group's members change.
                FixtureGroup *g = groups.at(rng.bounded(3));
                const quint32 f = m_fixtures.at(rng.bounded(int(m_fixtures.count())));
                if (g->fixtureList().contains(f) && g->fixtureList().count() > 1)
                    g->resignFixture(f);
                else if (!g->fixtureList().contains(f))
                    g->assignFixture(f);
                st->bake();
            }
            break;
        }
        ++edits;
        if (op != 13)       // the rig changed: every cue is planned again
        {
            planned += st->lastBakePlanned();
            cues += int(st->cueList(listId)->cues.count());
        }
        else
            QCOMPARE(st->lastBakePlanned(), int(st->cueList(listId)->cues.count()));

        const QStringList incremental = bakedState(m_doc, st);
        st->forgetPlans();
        st->bake();
        QCOMPARE(st->lastBakePlanned(), int(st->cueList(listId)->cues.count()));
        const QStringList full = bakedState(m_doc, st);
        if (incremental != full)
        {
            for (int i = 0; i < qMax(incremental.count(), full.count()); ++i)
                if (incremental.value(i) != full.value(i))
                    qWarning().noquote() << "edit" << n << "\n incremental:" << incremental.value(i)
                                         << "\n full:       " << full.value(i);
        }
        QCOMPARE(incremental, full);
    }
    QCOMPARE(edits, 400);
    // An edit re-plans what it reaches, not the list (on 20 cues tracking
    // over 4 overlapping groups, a third of it on average).
    QVERIFY2(planned * 2 < cues, qPrintable(QString("%1 of %2 cues re-planned").arg(planned).arg(cues)));

    // One look changed in the middle of a long tracking run: that cue and
    // the ones it tracks into, not the cues before it.
    // Every edit replaces the tree's lists: work from the ids, never a
    // CueList pointer kept across an edit (it dangles).
    st->forgetPlans();
    st->bake();
    QList<quint32> ids;
    for (const ShowTree::Cue &cue : st->cueList(listId)->cues)
        ids << cue.id;
    QVERIFY(ids.count() >= 8);
    for (quint32 id : ids)
        st->setCueMode(id, ShowTree::CueMode::Track);
    QCOMPARE(st->cueList(listId)->cues.count(), ids.count());
    const quint32 mid = ids.at(ids.count() / 2);
    QVERIFY(st->setTargetLook(mid, groupTarget(groups.first()->id()), st->lookForPalette(levels.last())));
    QVERIFY(st->lastBakePlanned() > 0);
    QVERIFY(st->lastBakePlanned() <= int(ids.count()) - int(ids.count()) / 2);
}

/* A hit fades in and out by its own times (Q180): the collection a VC
   button fires starts its scenes with the hit's fade in, and stops them
   with its fade out. No fade: it snaps, as before. Saved with the show. */
void ShowTreePlay_Test::hitsFadeInAndOut()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Dance");
    const quint32 hit = st->addLook(prod, "Stab");
    st->setLayerLook(hit, st->addLayer(hit, groupTarget(a->id())), st->lookForPalette(full));
    Collection *coll = qobject_cast<Collection *>(m_doc->function(st->cue(hit)->collection));
    QVERIFY(coll != nullptr);

    // No fades: on at once, off at once.
    coll->start(m_doc->masterTimer(), FunctionParent::master());
    QHash<quint32, uchar> v = settle(3);
    QCOMPARE(int(v[0]), 255);
    coll->stop(FunctionParent::master());
    v = settle(3);
    QCOMPARE(int(v[0]), 0);

    // 1 s in, 2 s out.
    ShowTree::Cue c = *st->cue(hit);
    c.fadeIn = 1000;
    c.fadeOut = 2000;
    QVERIFY(st->setCue(c));
    QCOMPARE(coll->hitFadeIn(), 1000u);
    QCOMPARE(coll->hitFadeOut(), 2000u);
    coll->start(m_doc->masterTimer(), FunctionParent::master());
    v = settle(int(MasterTimer::frequency() / 2));          // half a second
    QVERIFY2(v[0] > 60 && v[0] < 200, qPrintable(QString::number(v[0])));
    v = settle(int(MasterTimer::frequency()));
    QCOMPARE(int(v[0]), 255);
    coll->stop(FunctionParent::master());
    v = settle(int(MasterTimer::frequency()));                // one of two seconds
    QVERIFY2(v[0] > 60 && v[0] < 200, qPrintable(QString::number(v[0])));
    v = settle(int(MasterTimer::frequency()) + 10);
    QCOMPARE(int(v[0]), 0);

    // A started-with override (a chaser step, a timeline) still wins.
    coll->start(m_doc->masterTimer(), FunctionParent::master(), 0, 0, 0);
    v = settle(3);
    QCOMPARE(int(v[0]), 255);
    coll->stop(FunctionParent::master());
    settle(3);

    // Undo takes the fades back off the collection.
    QVERIFY(st->undo());
    QCOMPARE(coll->hitFadeIn(), 0u);
    QVERIFY(st->redo());

    // Saved and loaded: the hit keeps them, its collection plays them.
    const QByteArray xml = engineXml(m_doc);
    QVERIFY(xml.contains("FadeIn=\"1000\""));
    Doc *other = new Doc(this);
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(other->loadXML(r, false));
    ShowTreeController *st2 = other->showTree();
    const ShowTree::Cue *loaded = st2->cue(hit);
    QVERIFY(loaded != nullptr);
    QCOMPARE(loaded->fadeIn, 1000u);
    QCOMPARE(loaded->fadeOut, 2000u);
    const Collection *lc = qobject_cast<const Collection *>(other->function(loaded->collection));
    QVERIFY(lc != nullptr);
    QCOMPARE(lc->hitFadeIn(), 1000u);
    QCOMPARE(lc->hitFadeOut(), 2000u);
    delete other;
}

/* Scoped hits (Q154): a hit is armed while its place is live -- its cue
   live, its list running (or the current production's main list with
   nothing running), its production current, always at the top. Of a
   button's hits the narrowest armed one fires. Worked out per GO, never
   per frame: hitArmingChanged() comes on a list's start, stop and cue. */
void ShowTreePlay_Test::hitsAreArmedWhileTheirPlaceIsLive()
{
    FixtureGroup *a = group("A", { m_fixtures[0] });
    FixtureGroup *b = group("B", { m_fixtures[1] });
    const quint32 full = dimmer(255, "Full");
    ShowTreeController *st = m_doc->showTree();
    quint32 pa = 0, pb = 0;
    const quint32 la = oneListProduction("Dance", a->id(), 2, full, &pa);
    const quint32 lb = oneListProduction("Corporate", b->id(), 1, full, &pb);
    const quint32 c1 = st->cueList(la)->cues.at(0).id;
    const quint32 c2 = st->cueList(la)->cues.at(1).id;
    const quint32 onCue2 = st->addHit(HitScope::Cue, c2, "Strobe");
    const quint32 onList = st->addHit(HitScope::List, la, "Red");
    const quint32 onProd = st->addHit(HitScope::Production, pa, "Wash");
    const quint32 everywhere = st->addHit(HitScope::Global, pa, "Blackout");
    const quint32 otherProd = st->addHit(HitScope::Production, pb, "Corp");
    const QList<quint32> button = { everywhere, onProd, onList, onCue2 };
    QCOMPARE(st->currentProduction(), pa);

    // Nothing running: Dance current, its main list "the list we're in".
    QVERIFY(st->isHitArmed(everywhere));
    QVERIFY(st->isHitArmed(onProd));
    QVERIFY(st->isHitArmed(onList));
    QVERIFY(!st->isHitArmed(onCue2));
    QVERIFY(!st->isHitArmed(otherProd));
    QCOMPARE(st->armedHit(button), onList);

    // GO: cue 1 live; GO again: cue 2 live, its hit wins.
    QSignalSpy arming(st, &ShowTreeController::hitArmingChanged);
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(la)->chaser));
    QVERIFY(chaser != nullptr);
    go(chaser);
    settle(3);
    QCoreApplication::processEvents();
    QVERIFY(arming.count() > 0);
    QCOMPARE(st->runningCue(la), c1);
    QVERIFY(!st->isHitArmed(onCue2));
    QCOMPARE(st->armedHit(button), onList);
    arming.clear();
    go(chaser);
    settle(3);
    QCoreApplication::processEvents();
    QVERIFY(arming.count() > 0);
    QCOMPARE(st->runningCue(la), c2);
    QVERIFY(st->isHitArmed(onCue2));
    QCOMPARE(st->armedHit(button), onCue2);
    // Ties: the first given.
    QCOMPARE(st->armedHit({ onProd, everywhere }), onProd);

    // Another list of Dance running: Main isn't, nor its hits.
    chaser->stop(FunctionParent::master());
    settle(3);
    QCoreApplication::processEvents();
    QVERIFY(!st->isHitArmed(onCue2));
    QVERIFY(st->isHitArmed(onList));        // stopped: the main list again
    const quint32 encore = st->addCueList(pa, "Encore");
    const quint32 e1 = st->addCue(encore, -1, "E");
    st->setLayerLook(e1, st->addLayer(e1, groupTarget(a->id())), st->lookForPalette(full));
    Chaser *enc = qobject_cast<Chaser *>(m_doc->function(st->cueList(encore)->chaser));
    go(enc);
    settle(3);
    QCoreApplication::processEvents();
    QVERIFY(!st->isHitArmed(onList));
    QCOMPARE(st->armedHit(button), onProd);
    enc->stop(FunctionParent::master());
    settle(3);

    // Corporate current: Dance's production hits disarm; global stays.
    arming.clear();
    QVERIFY(m_doc->setCurrentProduction(pb));
    QVERIFY(arming.count() > 0);
    QVERIFY(!st->isHitArmed(onProd));
    QVERIFY(!st->isHitArmed(onList));
    QVERIFY(st->isHitArmed(otherProd));
    QCOMPARE(st->armedHit(button), everywhere);
    QCOMPARE(st->armedHit({ onProd, onList, onCue2 }), ShowTree::invalidId());
    Q_UNUSED(lb);
}

QTEST_MAIN(ShowTreePlay_Test)
