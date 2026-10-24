/*
  Q Light Controller Plus
  focuscalls_test.cpp

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
#include <QtTest>

#include "focuscalls_test.h"
#include "programmercontroller.h"
#include "monitorproperties.h"
#include "venuecorrections.h"
#include "controllerscheme.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "fixturegroup.h"
#include "focussession.h"
#include "stagetarget.h"
#include "qlcphysical.h"
#include "focuscalls.h"
#include "focuscallcue.h"
#include "showtreecontroller.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "doc.h"

namespace
{
/* A 540/270 spot: pan, tilt, dimmer, placed free at (x, y) metres, hung at
   5 m (as focussession_test's). */
Fixture *makeSpot(Doc *doc, const QString &name, float x, float y)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Calls Spot " + name);
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("3ch");
    const QLCChannel::Group groups[] = { QLCChannel::Pan, QLCChannel::Tilt, QLCChannel::Intensity };
    const char *names[] = { "Pan", "Tilt", "Dimmer" };
    for (int k = 0; k < 3; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(names[k]);
        ch->setGroup(groups[k]);
        ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch);
        mode->insertChannel(ch, k);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    QLCFixtureHead head;
    head.addChannel(0); head.addChannel(1); head.addChannel(2);
    mode->insertHead(-1, head);
    def->addMode(mode);

    Fixture *fxi = new Fixture(doc);
    fxi->setName(name);
    fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(0);
    fxi->setAddress(quint32(doc->fixtures().size() * 3));
    if (doc->addFixture(fxi) == false)
        return nullptr;
    doc->monitorProperties()->setFixturePosition(fxi->id(), 0, 0,
                                                 QVector3D(x * 1000.0f, y * 1000.0f, 5.0f));
    return fxi;
}

StageTarget *makeTarget(Doc *doc, const QString &name, const QVector3D &at)
{
    StageTarget *t = doc->monitorProperties()->addStageTarget();
    t->setName(name);
    t->setPosition(at);
    return t;
}

FixtureGroup *makeGroup(Doc *doc, const QString &name, const QList<Fixture *> &members)
{
    FixtureGroup *g = new FixtureGroup(doc);
    g->setName(name);
    g->setSize(QSize(qMax(1, int(members.size())), 1));
    for (int i = 0; i < members.size(); ++i)
        g->assignFixture(members.at(i)->id(), QLCPoint(i, 0));
    doc->addFixtureGroup(g);
    return g;
}

FocusSession::Reference targetRef(quint32 id)
{
    FocusSession::Reference r;
    r.kind = FocusSession::Reference::Target;
    r.id = id;
    return r;
}

QByteArray saveDoc(Doc *doc)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.writeStartDocument();
    doc->saveXML(&w);
    w.writeEndDocument();
    return out;
}
}

void FocusCalls_Test::init()
{
    m_doc = new Doc(this);
    m_dir = new QTemporaryDir();
    m_doc->programmer()->focusSession()->setFlashEnabled(false);
}

void FocusCalls_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
    delete m_dir;
    m_dir = nullptr;
}

void FocusCalls_Test::theControllerSchemeIsOneMap()
{
    using namespace ControllerScheme;
    QCOMPARE(verbFor(GamepadRole::LeftTrigger), PreviousGroup);
    QCOMPARE(verbFor(GamepadRole::RightTrigger), NextGroup);
    QCOMPARE(verbFor(GamepadRole::LB), PreviousItem);
    QCOMPARE(verbFor(GamepadRole::RB), NextItem);
    QCOMPARE(verbFor(GamepadRole::Y), CycleScope);
    QCOMPARE(verbFor(GamepadRole::A), Keep);
    QCOMPARE(verbFor(GamepadRole::B), Revert);
    QCOMPARE(verbFor(GamepadRole::X), Reset);
    QCOMPARE(verbFor(GamepadRole::Back), Undo);
    QCOMPARE(verbFor(GamepadRole::Start), Exit);
    QCOMPARE(verbFor(GamepadRole::LS), Alternate);
    QCOMPARE(verbFor(GamepadRole::RS), WhereAmI);
    QCOMPARE(verbFor(GamepadRole::LeftX), NoVerb);
    // Every verb has its control, and back again.
    for (Verb v : verbs())
        QCOMPARE(verbFor(roleFor(v)), v);
    // The words change with the tool; the kind of thing a control does doesn't.
    QCOMPARE(label(FocusCallsTool, NextGroup), QString("Next focus call"));
    QCOMPARE(label(RefineTool, NextGroup), QString("Next layer"));
    QCOMPARE(label(FocusCallsTool, Keep), QString("Save for this venue"));
    QCOMPARE(label(RefineTool, Keep), QString("Keep in the cue"));
    QVERIFY(label(FocusTool, NextGroup).isEmpty());
    for (Verb v : verbs())
    {
        QVERIFY(!label(RefineTool, v).isEmpty());
        QVERIFY(!label(FocusCallsTool, v).isEmpty());
    }

    // Focus mode's own controls come from the same map.
    QCOMPARE(FocusSession::controlFor(GamepadRole::RightTrigger), FocusSession::NextCall);
    QCOMPARE(FocusSession::controlFor(GamepadRole::Y), FocusSession::CycleScope);
    QCOMPARE(FocusSession::controlFor(GamepadRole::LS), FocusSession::ToggleReference);
    QCOMPARE(FocusSession::controlFor(GamepadRole::RS), FocusSession::WhereAmI);
}

void FocusCalls_Test::aTriggerPressesOnce()
{
    ControllerScheme::TriggerButton t;
    QVERIFY(!t.update(0.2f));
    QVERIFY(t.update(0.9f));
    QVERIFY(!t.update(1.0f));       // held
    QVERIFY(!t.update(0.5f));       // half out: still held
    QVERIFY(t.isDown());
    QVERIFY(!t.update(0.1f));       // released
    QVERIFY(!t.isDown());
    QVERIFY(t.update(0.7f));        // and again
}

void FocusCalls_Test::callsAreSavedWithTheShow()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 4.0f, 2.0f);
    FixtureGroup *dh = makeGroup(m_doc, "DH Tspots", { a, b });
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    StageTarget *solo = makeTarget(m_doc, "Soloist DS", QVector3D(3.0f, 7.0f, 0.0f));

    FocusCalls *calls = m_doc->programmer()->focusCalls();
    QVERIFY(m_doc->isModified() || true);
    m_doc->resetModified();
    FocusCall c1;
    c1.groups << dh->id();
    c1.target = targetRef(dsc->id());
    c1.note = "Check the shutter cut";
    FocusCall c2;
    c2.name = "AT spots -> Soloist";
    c2.fixtures << b->id();
    c2.target = targetRef(solo->id());
    calls->append(c1);
    calls->append(c2);
    QVERIFY(m_doc->isModified());   // they are the show's

    const QByteArray xml = saveDoc(m_doc);
    QVERIFY(xml.contains("<FocusCalls>"));
    QVERIFY(xml.contains("Check the shutter cut"));

    FocusCalls loaded(m_doc);
    QXmlStreamReader r(xml);
    while (!r.atEnd() && !(r.isStartElement() && r.name() == KXMLQLCFocusCalls))
        r.readNext();
    QVERIFY(loaded.loadXML(r));
    QCOMPARE(loaded.count(), 2);
    QVERIFY(loaded.call(0) == c1);
    QVERIFY(loaded.call(1) == c2);

    // Reordered, as the run order.
    QVERIFY(calls->move(1, 0));
    QCOMPARE(calls->call(0).name, QString("AT spots -> Soloist"));

    // A show without calls writes nothing for them.
    calls->clear();
    QVERIFY(!saveDoc(m_doc).contains("FocusCalls"));
}

void FocusCalls_Test::aCallNamesItself()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 4.0f, 2.0f);
    Fixture *c = makeSpot(m_doc, "C", 6.0f, 2.0f);
    FixtureGroup *dh = makeGroup(m_doc, "DH Tspots", { b, a });
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    FocusCall call;
    call.groups << dh->id();
    call.fixtures << a->id() << c->id();
    call.target = targetRef(dsc->id());
    QCOMPARE(FocusCalls::fixturesOf(m_doc, call), QList<quint32>({ b->id(), a->id(), c->id() }));
    QCOMPARE(FocusCalls::displayName(m_doc, call), QString("DH Tspots, A, C \342\206\222 DSC"));
    QCOMPARE(FocusCalls::whoName(m_doc, call), QString("DH Tspots, A, C"));
    call.name = "Front light";
    QCOMPARE(FocusCalls::displayName(m_doc, call), QString("Front light"));
}

void FocusCalls_Test::runningCallsStepsCallsAndFixtures()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 4.0f, 2.0f);
    Fixture *c = makeSpot(m_doc, "C", 6.0f, 2.0f);
    FixtureGroup *dh = makeGroup(m_doc, "DH Tspots", { a, b });
    FixtureGroup *at = makeGroup(m_doc, "AT spots", { c });
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    StageTarget *solo = makeTarget(m_doc, "Soloist DS", QVector3D(1.0f, 7.0f, 0.0f));

    FocusCalls *calls = m_doc->programmer()->focusCalls();
    FocusCall c1;
    c1.groups << dh->id();
    c1.target = targetRef(dsc->id());
    FocusCall c2;
    c2.groups << at->id();
    c2.target = targetRef(solo->id());
    calls->setCalls({ c1, c2 });

    FocusSession *s = m_doc->programmer()->focusSession();
    FocusSession::Options o;
    o.calls = calls->sessionCalls();
    o.order = FocusSession::AsGiven;
    QString why;
    QVERIFY2(s->start(o, &why), qPrintable(why));
    QVERIFY(s->isRunningCalls());
    QCOMPARE(s->callCount(), 2);
    QCOMPARE(s->callIndex(), 0);
    QCOMPARE(s->order(), QList<quint32>({ a->id(), b->id() }));
    QCOMPARE(s->allFixtures().size(), 3);
    QCOMPARE(s->currentFixture(), a->id());
    QVector3D p;
    QVERIFY(s->referencePoint(a->id(), p));
    QCOMPARE(p.x(), 5.0f);
    QCOMPARE(s->referenceName(), QString("DSC"));

    // RB: the next fixture in the call; RT: the next call.
    s->apply(FocusSession::controlFor(GamepadRole::RB));
    QCOMPARE(s->currentFixture(), b->id());
    s->setTrigger(1, 1.0f);
    QCOMPARE(s->callIndex(), 1);
    QCOMPARE(s->currentFixture(), c->id());
    QVERIFY(s->referencePoint(c->id(), p));
    QCOMPARE(p.x(), 1.0f);
    QCOMPARE(s->referenceName(), QString("Soloist DS"));
    // Held, it does not step again; let go and LT goes back.
    s->setTrigger(1, 1.0f);
    QCOMPARE(s->callIndex(), 1);
    s->setTrigger(1, 0.0f);
    s->setTrigger(0, 1.0f);
    QCOMPARE(s->callIndex(), 0);
    QCOMPARE(s->currentFixture(), a->id());

    // The lit head is the one picked; the rest of the call is dark.
    QHash<QPair<int, int>, uchar> w = s->pendingWrites();
    QCOMPARE(int(w.value(qMakePair(0, int(a->address()) + 2))), qRound(0.8 * 255));
    QCOMPARE(int(w.value(qMakePair(0, int(b->address()) + 2))), 0);
    // Y: the whole call is lit.
    s->apply(FocusSession::controlFor(GamepadRole::Y));
    QCOMPARE(s->scope(), FocusSession::GroupScope);
    QCOMPARE(s->scopeFixtures(), QList<quint32>({ a->id(), b->id() }));
    w = s->pendingWrites();
    QCOMPARE(int(w.value(qMakePair(0, int(b->address()) + 2))), qRound(0.8 * 255));
    // A bumper from the whole call picks a fixture.
    s->apply(FocusSession::controlFor(GamepadRole::RB));
    QCOMPARE(s->scope(), FocusSession::FixtureScope);
    s->stop();
}

void FocusCalls_Test::groupScopeWorksTheWholeCall()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 4.0f, 2.0f);
    FixtureGroup *dh = makeGroup(m_doc, "DH Tspots", { a, b });
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    FocusCall c1;
    c1.groups << dh->id();
    c1.target = targetRef(dsc->id());
    m_doc->programmer()->focusCalls()->setCalls({ c1 });
    m_doc->resetModified();

    FocusSession *s = m_doc->programmer()->focusSession();
    FocusSession::Options o;
    o.calls = m_doc->programmer()->focusCalls()->sessionCalls();
    QVERIFY(s->start(o));
    s->setScope(FocusSession::GroupScope);
    // The truss came in a little low: one nudge for the lot.
    s->nudge(0.0f, 1.5f);
    QCOMPARE(s->liveOffset(a->id()).tilt, 1.5f);
    QCOMPARE(s->liveOffset(b->id()).tilt, 1.5f);
    s->save();
    QVERIFY(s->isChecked(a->id()) && s->isChecked(b->id()));
    QVERIFY(m_doc->isModified());   // no venue: the show's calibration
    // Back undoes the save of both.
    QVERIFY(s->undoSave());
    QCOMPARE(s->savedOffset(a->id()).tilt, 0.0f);
    QCOMPARE(s->savedOffset(b->id()).tilt, 0.0f);
    QVERIFY(!s->canUndo());
    s->stop();
}

void FocusCalls_Test::theOperatorIsToldWhereTheyAre()
{
    // The flash: three blinks, then steady.
    QVERIFY(FocusSession::flashLit(0));
    QVERIFY(!FocusSession::flashLit(FocusSession::flashOnMs()));
    QVERIFY(FocusSession::flashLit(FocusSession::flashOnMs() + FocusSession::flashOffMs()));
    QVERIFY(FocusSession::flashLit(FocusSession::flashDurationMs() + 5));

    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 4.0f, 2.0f);
    Fixture *c = makeSpot(m_doc, "C", 6.0f, 2.0f);
    FixtureGroup *dh = makeGroup(m_doc, "DH Tspots", { a, b });
    FixtureGroup *at = makeGroup(m_doc, "AT spots", { c });
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    StageTarget *solo = makeTarget(m_doc, "Soloist DS", QVector3D(1.0f, 7.0f, 0.0f));
    FocusCall c1;
    c1.groups << dh->id();
    c1.target = targetRef(dsc->id());
    FocusCall c2;
    c2.groups << at->id();
    c2.target = targetRef(solo->id());
    m_doc->programmer()->focusCalls()->setCalls({ c1, c2 });

    FocusSession *s = m_doc->programmer()->focusSession();
    QSignalSpy said(m_doc->programmer(), &ProgrammerController::operatorAnnounce);
    FocusSession::Options o;
    o.calls = m_doc->programmer()->focusCalls()->sessionCalls();
    o.order = FocusSession::AsGiven;
    QVERIFY(s->start(o));
    QCOMPARE(said.last().at(0).toString(), QString("Call 1 of 2, DH Tspots to DSC. Fixture 1 of 2."));
    s->next();
    QCOMPARE(said.last().at(0).toString(), QString("Call 1 of 2, DH Tspots to DSC. Fixture 2 of 2."));
    s->cycleScope();
    QCOMPARE(said.last().at(0).toString(), QString("Call 1 of 2, DH Tspots to DSC. All 2, group."));
    s->nextCall();
    QCOMPARE(said.last().at(0).toString(), QString("Call 2 of 2, AT spots to Soloist DS. All 1, group."));
    s->apply(FocusSession::WhereAmI);
    QCOMPARE(said.last().at(0).toString(), QString("Call 2 of 2, AT spots to Soloist DS. All 1, group."));
    s->save();
    QCOMPARE(said.last().at(0).toString(), QString("Saved."));
    s->apply(FocusSession::UndoSave);
    QCOMPARE(said.last().at(0).toString(), QString("Undone."));
    s->apply(FocusSession::UndoSave);
    QCOMPARE(said.last().at(0).toString(), QString("Nothing to undo."));

    // The flash runs on a change, and lets the picked head go dark in it.
    s->setFlashEnabled(true);
    s->flashSelection();
    QVERIFY(s->isFlashing());
    QTRY_VERIFY_WITH_TIMEOUT(!s->isFlashing(), 2000);
    s->stop();
}

void FocusCalls_Test::correctionsGoToTheVenueNotTheShow()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 4.0f, 2.0f);
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    MonitorProperties *props = m_doc->monitorProperties();
    // The show's own calibration on B.
    FixtureRigProps rp = props->fixtureRigProps(b->id());
    rp.panOffsetDeg = 2.0f;
    props->setFixtureRigProps(b->id(), rp);

    const QString show = m_dir->filePath("Spring Show.qlcc");
    VenueCorrections *v = m_doc->programmer()->venueCorrections();
    QCOMPARE(VenueCorrections::sidecarPath(show), m_dir->filePath("Spring Show.venues.xml"));
    v->setShowFile(show, false);
    const QString lincoln = v->addVenue("Lincoln HS");
    QVERIFY(v->setActiveVenue(lincoln));
    const QByteArray before = saveDoc(m_doc);
    m_doc->resetModified();

    FocusSession *s = m_doc->programmer()->focusSession();
    FocusSession::Options o;
    o.fixtures = { a->id(), b->id() };
    o.order = FocusSession::AsGiven;
    o.first = targetRef(dsc->id());
    QVERIFY(s->start(o));
    QVERIFY(s->savesToVenue());
    s->nudge(1.25f, -0.5f);
    s->save();
    s->next();
    s->nudge(0.5f, 0.0f);
    s->save();
    s->stop();

    // In the rig now (the views and the solver read it there)...
    QCOMPARE(props->fixtureRigProps(a->id()).panOffsetDeg, 1.25f);
    QCOMPARE(props->fixtureRigProps(b->id()).panOffsetDeg, 2.5f);
    // ...in the venue and its file...
    QVERIFY(v->hasCorrection(a->id()));
    QCOMPARE(v->correction(b->id()).panOffsetDeg, 2.5f);
    QVERIFY(QFile::exists(v->filePath()));
    QFile f(v->filePath());
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray side = f.readAll();
    QVERIFY(side.contains("Lincoln HS"));
    QVERIFY(side.contains("PanOfs=\"1.25\""));
    // ...and never in the show: not modified, and its XML is as it was.
    QVERIFY(!m_doc->isModified());
    /* What the show file says of the rig is what it said: a fixture the
       session touched may gain an entry of plain defaults, nothing more. */
    auto rigOf = [](const QByteArray &xml) {
        QStringList out;
        QXmlStreamReader r(xml);
        while (r.readNextStartElement() || !r.atEnd())
            if (r.isStartElement() && r.name() == QLatin1String("FixtureRig"))
                for (const QXmlStreamAttribute &a : r.attributes())
                    if (a.name() != QLatin1String("FID") && a.name() != QLatin1String("Truss")
                        && a.name() != QLatin1String("Offset") && a.name() != QLatin1String("Mounting")
                        && !(a.name() == QLatin1String("PanZero") && a.value() == QLatin1String("0.0")))
                        out << a.name().toString() + "=" + a.value().toString();
        return out;
    };
    const QByteArray after = saveDoc(m_doc);
    QCOMPARE(rigOf(after), rigOf(before));
    QCOMPARE(rigOf(after), QStringList({ "PanOfs=2.00" }));
    QVERIFY(saveDoc(m_doc).contains("PanOfs=\"2.00\""));

    /* A restart: the rig as the show file has it (no venue), and the
       sidecar read and applied again on open. */
    v->setActiveVenue(QString());
    QCOMPARE(props->fixtureRigProps(a->id()).panOffsetDeg, 0.0f);
    QCOMPARE(props->fixtureRigProps(b->id()).panOffsetDeg, 2.0f);
    {
        QFile sf(v->filePath());
        QVERIFY(sf.open(QIODevice::ReadWrite));
        // As the sidecar was when the app quit: Lincoln HS applied.
        sf.resize(0);
        sf.write(side);
    }
    VenueCorrections reopened(m_doc);
    reopened.setShowFile(show, true);
    QCOMPARE(reopened.activeVenue(), QString("Lincoln HS"));
    QCOMPARE(props->fixtureRigProps(a->id()).panOffsetDeg, 1.25f);
    QCOMPARE(props->fixtureRigProps(a->id()).tiltOffsetDeg, -0.5f);
    QCOMPARE(props->fixtureRigProps(b->id()).panOffsetDeg, 2.5f);
    // Its show file still says the show's own.
    QVERIFY(saveDoc(m_doc).contains("PanOfs=\"2.00\""));
    QVERIFY(!saveDoc(m_doc).contains("PanOfs=\"1.25\""));
}

void FocusCalls_Test::switchingVenues()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    MonitorProperties *props = m_doc->monitorProperties();
    VenueCorrections *v = m_doc->programmer()->venueCorrections();
    v->setShowFile(m_dir->filePath("Show.qlcc"), false);

    QVERIFY(v->setActiveVenue(v->addVenue("Lincoln HS")));
    FixtureRigProps rp = props->fixtureRigProps(a->id());
    rp.tiltOffsetDeg = 3.0f;
    props->setFixtureRigProps(a->id(), rp);
    QVERIFY(v->record(a->id()));

    QVERIFY(v->setActiveVenue(v->addVenue("Kennedy Center")));
    QCOMPARE(props->fixtureRigProps(a->id()).tiltOffsetDeg, 0.0f);   // the show's
    rp = props->fixtureRigProps(a->id());
    rp.tiltOffsetDeg = -1.0f;
    props->setFixtureRigProps(a->id(), rp);
    QVERIFY(v->record(a->id()));

    QVERIFY(v->setActiveVenue("Lincoln HS"));
    QCOMPARE(props->fixtureRigProps(a->id()).tiltOffsetDeg, 3.0f);
    QVERIFY(v->setActiveVenue(QString()));
    QCOMPARE(props->fixtureRigProps(a->id()).tiltOffsetDeg, 0.0f);
    QVERIFY(!v->isActive());
    QVERIFY(!v->record(a->id()));       // no venue: nowhere to put it

    // Back to the show's own: the correction goes.
    QVERIFY(v->setActiveVenue("Kennedy Center"));
    QVERIFY(v->clearCorrection(a->id()));
    QCOMPARE(props->fixtureRigProps(a->id()).tiltOffsetDeg, 0.0f);
    QCOMPARE(v->correctionCount(), 0);

    // The file round trip keeps both venues and which one is applied.
    VenueCorrections other(m_doc);
    QVERIFY(other.fromXml(v->toXml()));
    QCOMPARE(other.venueNames(), QStringList({ "Lincoln HS", "Kennedy Center" }));
    QCOMPARE(other.activeVenue(), QString("Kennedy Center"));
    QCOMPARE(other.venue("Lincoln HS").fixtures.value(a->id()).tiltOffsetDeg, 3.0f);
    QVERIFY(!other.fromXml("<NotVenues/>"));

    QVERIFY(v->removeVenue("Kennedy Center"));
    QVERIFY(!v->isActive());
    QCOMPARE(v->venueNames(), QStringList({ "Lincoln HS" }));
}

void FocusCalls_Test::venueNames()
{
    const QDateTime when(QDate(2026, 10, 23), QTime(19, 4));
    QCOMPARE(VenueCorrections::defaultVenueName(when), QString("Venue 2026-10-23 19:04"));
    VenueCorrections *v = m_doc->programmer()->venueCorrections();
    const QString first = v->addVenue();
    const QString second = v->addVenue();
    QVERIFY(first.startsWith("Venue "));
    QVERIFY(first != second);
    QCOMPARE(v->addVenue("Lincoln HS"), QString("Lincoln HS"));
    QCOMPARE(v->addVenue("Lincoln HS"), QString("Lincoln HS"));
    QCOMPARE(v->venueNames().size(), 3);
    QVERIFY(v->renameVenue("Lincoln HS", "Lincoln High"));
    QVERIFY(!v->renameVenue("Lincoln High", first));
    // With no show file there is nowhere to write it.
    QString why;
    QVERIFY(!v->save(&why));
    QVERIFY(!why.isEmpty());
}

void FocusCalls_Test::anEditElsewhereGoesToTheVenue()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 4.0f, 2.0f);
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    MonitorProperties *props = m_doc->monitorProperties();
    VenueCorrections *v = m_doc->programmer()->venueCorrections();
    v->setShowFile(m_dir->filePath("Show.qlcc"), false);
    QVERIFY(v->setActiveVenue(v->addVenue("Lincoln HS")));
    m_doc->resetModified();

    // A tilt offset typed in the fixture dialog: the dialog sets the rig.
    FixtureRigProps rp = props->fixtureRigProps(a->id());
    rp.tiltOffsetDeg = 2.5f;
    props->setFixtureRigProps(a->id(), rp);
    QVERIFY(v->hasCorrection(a->id()));
    QCOMPARE(v->correction(a->id()).tiltOffsetDeg, 2.5f);
    QFile f(v->filePath());
    QVERIFY(f.open(QIODevice::ReadOnly));
    QVERIFY(f.readAll().contains("TiltOfs=\"2.50\""));
    f.close();
    // The show keeps its own.
    QVERIFY(!saveDoc(m_doc).contains("TiltOfs"));

    // Moving the fixture is not a calibration: nothing recorded.
    rp = props->fixtureRigProps(b->id());
    rp.trussOffset = 1.5f;
    props->setFixtureRigProps(b->id(), rp);
    QVERIFY(!v->hasCorrection(b->id()));

    // A Focus mode nudge is not recorded until it is saved...
    FocusSession *s = m_doc->programmer()->focusSession();
    FocusSession::Options o;
    o.fixtures = { b->id() };
    o.first = targetRef(dsc->id());
    QVERIFY(s->start(o));
    s->nudge(1.0f, 0.0f);
    QVERIFY(!v->hasCorrection(b->id()));
    // ...and a discarded one never is.
    s->stop();
    QVERIFY(!v->hasCorrection(b->id()));
    QCOMPARE(props->fixtureRigProps(b->id()).panOffsetDeg, 0.0f);

    // Typed back to the show's own value: the correction goes.
    rp = props->fixtureRigProps(a->id());
    rp.tiltOffsetDeg = 0.0f;
    props->setFixtureRigProps(a->id(), rp);
    QVERIFY(!v->hasCorrection(a->id()));

    // With no venue applied, the dialog edits the show, as always.
    QVERIFY(v->setActiveVenue(QString()));
    rp = props->fixtureRigProps(a->id());
    rp.tiltOffsetDeg = 1.0f;
    props->setFixtureRigProps(a->id(), rp);
    QVERIFY(saveDoc(m_doc).contains("TiltOfs=\"1.00\""));
}

void FocusCalls_Test::aFocusCallCueArmsThenStartsOnTheController()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 4.0f, 2.0f);
    Fixture *c = makeSpot(m_doc, "C", 6.0f, 2.0f);
    FixtureGroup *dh = makeGroup(m_doc, "DH Tspots", { a, b });
    FixtureGroup *at = makeGroup(m_doc, "AT spots", { c });
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    FocusCall c1;
    c1.groups << dh->id();
    c1.target = targetRef(dsc->id());
    FocusCall c2;
    c2.groups << at->id();
    c2.target = targetRef(dsc->id());
    m_doc->programmer()->focusCalls()->setCalls({ c1, c2 });

    // A Pre-show list: the focus-call cue, then the show.
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Spring");
    const quint32 list = st->addCueList(prod, "Pre-show");
    const quint32 focus = st->addCue(list, -1, "Focus");
    const quint32 house = st->addCue(list, 0, "House to half");
    QVERIFY(st->setCuesFocusCall({ focus }, true));
    QVERIFY(st->isCueFocusCall(focus));
    QCOMPARE(st->undoText(), QString("Make cue a focus call"));
    QVERIFY(!st->isCueFocusCall(house));

    FocusCallCue *fcc = m_doc->programmer()->focusCallCue();
    FocusSession *s = m_doc->programmer()->focusSession();
    QSignalSpy said(m_doc->programmer(), &ProgrammerController::operatorAnnounce);
    QSignalSpy goOn(fcc, &FocusCallCue::continueRequested);
    m_doc->setMode(Doc::Operate);

    // GO onto it: armed, said once, nothing running yet.
    fcc->cueCurrent(list, focus);
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
    QCOMPARE(said.last().at(0).toString(), QString("Focus calls ready. Press Start to begin."));
    const int saidOnce = said.count();
    fcc->cueCurrent(list, focus);           // the same cue again: no repeat
    QCOMPARE(said.count(), saidOnce);
    QVERIFY(!s->isActive());
    QCOMPARE(FocusCallCue::startRole(), GamepadRole::Start);

    // Start: the calls, in Operate, the first one said.
    QVERIFY(fcc->start());
    QCOMPARE(fcc->state(), FocusCallCue::Running);
    QVERIFY(s->isActive());
    QVERIFY(s->isRunningCalls());
    QCOMPARE(said.last().at(0).toString(), QString("Call 1 of 2, DH Tspots to DSC. Fixture 1 of 2."));
    // Step: the next call.
    s->setTrigger(1, 1.0f);
    s->setTrigger(1, 0.0f);
    QCOMPARE(s->callIndex(), 1);
    // RT on the last call: done -- it asks to leave (the panel asks about
    // anything unsaved, then stops).
    QSignalSpy exit(s, &FocusSession::exitRequested);
    s->setTrigger(1, 1.0f);
    QCOMPARE(exit.count(), 1);
    QCOMPARE(s->callIndex(), 1);
    s->stop();
    // Leaving goes on with the list.
    QCOMPARE(fcc->state(), FocusCallCue::Idle);
    QCOMPARE(goOn.count(), 1);
    QCOMPARE(goOn.first().at(0).toUInt(), st->cueList(list)->chaser);
    // ...and does not arm the same cue again as the list moves off it.
    fcc->cueCurrent(list, focus);
    QCOMPARE(fcc->state(), FocusCallCue::Idle);

    // Armed, then the list moves on before Start: let go.
    fcc->cueCurrent(list, house);
    fcc->cueCurrent(list, focus);
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
    fcc->cueCurrent(list, house);
    QCOMPARE(fcc->state(), FocusCallCue::Idle);

    // No calls in the show: Start says so and stays armed.
    m_doc->programmer()->focusCalls()->clear();
    fcc->cueCurrent(list, focus);
    QString why;
    QVERIFY(!fcc->start(&why));
    QCOMPARE(said.last().at(0).toString(), QString("This show has no focus calls yet."));
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
    m_doc->setMode(Doc::Design);
}

QTEST_GUILESS_MAIN(FocusCalls_Test)
