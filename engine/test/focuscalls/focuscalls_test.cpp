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
#include <QtCore>

#define protected public                // Universe::processFaders()
#include "universe.h"
#undef protected
#define private public                  // MasterTimer::timerTick(), the joystick tick's clock
#include "mastertimer.h"
#include "programmercontroller.h"
#undef private
#include "focuscalls_test.h"
#include "showcontrol.h"
#include "chaser.h"
#include "inputoutputmap.h"
#include "iopluginstub.h"
#include "qlcioplugin.h"
#include "qlcfile.h"
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
#include "refinesession.h"
#include "qlcpalette.h"
#include "showtreecontroller.h"
#include "qlcchannel.h"
#include "qlccapability.h"
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

/* A spot with a zoom (10-40 degrees) and a focus channel: pan, tilt,
   dimmer, zoom, focus, at @p address. */
Fixture *makeZoomSpot(Doc *doc, const QString &name, float x, float y, quint32 address)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Calls Zoom Spot " + name);
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("5ch");
    const QLCChannel::Group groups[] = { QLCChannel::Pan, QLCChannel::Tilt, QLCChannel::Intensity,
                                         QLCChannel::Beam, QLCChannel::Beam };
    const QLCChannel::Preset presets[] = { QLCChannel::Custom, QLCChannel::Custom, QLCChannel::Custom,
                                           QLCChannel::BeamZoomSmallBig, QLCChannel::BeamFocusNearFar };
    const char *names[] = { "Pan", "Tilt", "Dimmer", "Zoom", "Focus" };
    QLCFixtureHead head;
    for (int k = 0; k < 5; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(names[k]);
        ch->setGroup(groups[k]);
        ch->setControlByte(QLCChannel::MSB);
        if (presets[k] != QLCChannel::Custom)
            ch->setPreset(presets[k]);
        def->addChannel(ch);
        mode->insertChannel(ch, k);
        head.addChannel(k);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    ph.setLensDegreesMin(10.0);
    ph.setLensDegreesMax(40.0);
    mode->setPhysical(ph);
    mode->insertHead(-1, head);
    def->addMode(mode);

    Fixture *fxi = new Fixture(doc);
    fxi->setName(name);
    fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(0);
    fxi->setAddress(address);
    if (doc->addFixture(fxi) == false)
        return nullptr;
    doc->monitorProperties()->setFixturePosition(fxi->id(), 0, 0,
                                                 QVector3D(x * 1000.0f, y * 1000.0f, 5.0f));
    return fxi;
}

/* The controller's tick, @p ticks times, each exactly the timer's nominal
   20 ms: the tick measures the time since the last one (a late tick moves
   as far as the stick was held for), so without this a loaded machine
   would count more time held than the test means. */
void joyTicks(ProgrammerController *pc, int ticks)
{
    for (int i = 0; i < ticks; ++i)
    {
        pc->m_joyTickClock.invalidate();
        pc->applyDesignJoystick();
    }
}

/* The stub plugin as a pad on input universe 0, with @p roles. */
IOPluginStub *plugInStubPad(Doc *doc, const QMap<quint32, QString> &roles)
{
    QDir dir("../iopluginstub");
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    doc->ioPluginCache()->load(dir);
    if (doc->ioPluginCache()->plugins().isEmpty())
        return nullptr;
    IOPluginStub *stub = static_cast<IOPluginStub *>(doc->ioPluginCache()->plugins().at(0));
    stub->m_channelRoles = roles;
    if (!doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0))
        return nullptr;
    doc->programmer()->autoBindFromProfile();
    return stub;
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
    // The D-pad: up is in, down is out; right tightens the focus (its value
    // up), left loosens it -- in every tool.
    QCOMPARE(verbFor(GamepadRole::DpadUp), ZoomNarrower);
    QCOMPARE(verbFor(GamepadRole::DpadDown), ZoomWider);
    QCOMPARE(verbFor(GamepadRole::DpadRight), FocusFarther);
    QCOMPARE(verbFor(GamepadRole::DpadLeft), FocusNearer);
    // The crew's words (Q140).
    QCOMPARE(label(RefineTool, FocusFarther), QString("Sharper (focus)"));
    QCOMPARE(label(RefineTool, FocusNearer), QString("Softer (focus)"));
    QCOMPARE(label(FocusCallsTool, ZoomNarrower), QString("Spot down (zoom narrower)"));
    QCOMPARE(label(FocusTool, ZoomWider), QString("Flood out (zoom wider)"));
    QCOMPARE(crewWord(ZoomNarrower), QString("Spot down."));
    QCOMPARE(crewWord(ZoomWider), QString("Flood out."));
    QCOMPARE(crewWord(FocusFarther), QString("Sharper."));
    QCOMPARE(crewWord(FocusNearer), QString("Softer."));
    QVERIFY(crewWord(Keep).isEmpty());
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

void FocusCalls_Test::aHeldArmRepeats()
{
    using namespace ControllerScheme;
    QVERIFY(repeats(ZoomWider) && repeats(ZoomNarrower) && repeats(FocusNearer) && repeats(FocusFarther));
    QVERIFY(!repeats(NextItem) && !repeats(NextGroup) && !repeats(Keep) && !repeats(CycleScope));
    HoldRepeat h;
    h.press(int(ZoomNarrower), true);
    QCOMPARE(h.due(0.3), 0);                          // not before the delay
    QCOMPARE(h.due(0.1), 1);                          // 0.4 s: the first repeat
    QCOMPARE(h.due(HoldRepeat::interval() * 3), 3);   // then every interval
    h.release(int(ZoomWider));                        // another key's release: no
    QCOMPARE(h.held(), int(ZoomNarrower));
    h.release(int(ZoomNarrower));
    QCOMPARE(h.due(1.0), 0);
    h.press(int(NextItem), false);                    // a bumper: once only
    QCOMPARE(h.due(1.0), 0);
}

void FocusCalls_Test::feedbackIsLightsVoiceOrBoth()
{
    using namespace ControllerScheme;
    QCOMPARE(nextFeedback(BothFeedback), LightsFeedback);
    QCOMPARE(nextFeedback(LightsFeedback), VoiceFeedback);
    QCOMPARE(nextFeedback(VoiceFeedback), BothFeedback);
    QCOMPARE(feedbackFromKey(feedbackKey(VoiceFeedback)), VoiceFeedback);
    QCOMPARE(feedbackFromKey("nonsense"), BothFeedback);
    QCOMPARE(label(FocusTool, WhereAmI), QString("Say the light (hold: feedback)"));

    // RS: a tap says the light; held for holdFor() it changes the feedback.
    LongPress rs;
    rs.press();
    QVERIFY(!rs.due(0.3));
    QVERIFY(rs.release());                        // a tap
    rs.press();
    QVERIFY(!rs.due(0.3));
    QVERIFY(rs.due(0.3));                         // held 0.6 s: once
    QVERIFY(!rs.due(1.0));
    QVERIFY(!rs.release());                       // not a tap as well

    Fixture *a = makeSpot(m_doc, "Spot 3", 2.0f, 2.0f);
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    ProgrammerController *pc = m_doc->programmer();
    QCOMPARE(pc->operatorFeedback(), BothFeedback);     // the default
    FocusSession *s = pc->focusSession();
    s->setFlashEnabled(true);
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);
    FocusSession::Options o;
    o.fixtures = { a->id() };
    o.first = targetRef(dsc->id());
    QVERIFY(s->start(o));
    QVERIFY(s->isFlashing());
    QCOMPARE(said.last().at(0).toString(), QString("Fixture 1, Spot 3."));

    // Lights only: no voice, but Say the light still speaks (asked for).
    pc->cycleOperatorFeedback();
    QCOMPARE(pc->operatorFeedback(), LightsFeedback);
    QCOMPARE(said.last().at(0).toString(), QString("Feedback: lights only."));
    const int count = said.count();
    s->next();
    s->zero();
    QCOMPARE(said.count(), count);
    s->whereAmI();
    QCOMPARE(said.last().at(0).toString(), QString("Spot 3. Fixture 1."));
    QVERIFY(s->isFlashing());

    // Voice only: no blinks on the rig.
    pc->cycleOperatorFeedback();
    QCOMPARE(said.last().at(0).toString(), QString("Feedback: voice only."));
    QTRY_VERIFY_WITH_TIMEOUT(!s->isFlashing(), 2000);
    s->whereAmI();
    QVERIFY(!s->isFlashing());
    // ...nor the save's blink (the panel's lamp still shows it).
    QVERIFY(!s->rigFeedback());
    s->setBlinkTimed(false);
    s->nudge(0.5f, 0.0f);
    s->save();
    QVERIFY(!s->isBlinking());
    pc->cycleOperatorFeedback();
    QCOMPARE(pc->operatorFeedback(), BothFeedback);
    QVERIFY(s->rigFeedback());
    s->nudge(0.5f, 0.0f);
    s->save();
    QVERIFY(s->isBlinking());
    s->stop();
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
    // Verbose: the call and where its target is, on every step.
    m_doc->programmer()->setVoiceDetail(ControllerScheme::VerboseVoice);
    FocusSession::Options o;
    o.calls = m_doc->programmer()->focusCalls()->sessionCalls();
    o.order = FocusSession::AsGiven;
    QVERIFY(s->start(o));
    // Entering a call says where its target is, from the stage marks (Q140).
    const QString atDsc = m_doc->monitorProperties()->stagePositionText(dsc->position(),
                                                                        MonitorProperties::SpokenOffset);
    const QString atSolo = m_doc->monitorProperties()->stagePositionText(solo->position(),
                                                                         MonitorProperties::SpokenOffset);
    QVERIFY(atDsc.startsWith("S") || atDsc.startsWith("CL"));
    QCOMPARE(s->callPositionPhrase(), atDsc);
    QCOMPARE(said.last().at(0).toString(), QString("Call 1 of 2, DH Tspots to DSC, %1. Fixture 1 of 2, A.").arg(atDsc));
    s->next();
    QCOMPARE(said.last().at(0).toString(), QString("Call 1 of 2, DH Tspots to DSC. Fixture 2 of 2, B."));
    s->cycleScope();
    QCOMPARE(said.last().at(0).toString(), QString("Call 1 of 2, DH Tspots to DSC. All 2, group."));
    s->nextCall();
    QCOMPARE(said.last().at(0).toString(), QString("Call 2 of 2, AT spots to Soloist DS, %1. All 1, group.").arg(atSolo));
    // Say the light: the light (here the whole call, by its groups) first.
    s->apply(FocusSession::WhereAmI);
    QCOMPARE(said.last().at(0).toString(), QString("AT spots. Call 2 of 2, AT spots to Soloist DS, %1. All 1, group.").arg(atSolo));
    s->setScope(FocusSession::FixtureScope);
    s->whereAmI();
    QCOMPARE(said.last().at(0).toString(), QString("C. Call 2 of 2, AT spots to Soloist DS, %1. Fixture 1 of 1.").arg(atSolo));
    s->save();
    QCOMPARE(said.last().at(0).toString(), QString("Saved C."));
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
    QCOMPARE(st->undoText(), QString("Make cue a venue focus cue"));
    QVERIFY(!st->isCueFocusCall(house));

    FocusCallCue *fcc = m_doc->programmer()->focusCallCue();
    FocusSession *s = m_doc->programmer()->focusSession();
    QSignalSpy said(m_doc->programmer(), &ProgrammerController::operatorAnnounce);
    QSignalSpy goOn(fcc, &FocusCallCue::continueRequested);
    quint32 running = focus;                 // the list is on the focus cue
    fcc->setRunningCueProbe([&running](quint32) { return running; });
    m_doc->setMode(Doc::Operate);

    // GO onto it: armed, said once, nothing running yet.
    fcc->cueCurrent(list, focus);
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
    QCOMPARE(said.last().at(0).toString(), QString("Venue focus ready. Press Start to begin."));
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
    QCOMPARE(said.last().at(0).toString(), QString("Call 1, DH Tspots to DSC. Fixture 1, A."));
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
    s->finish();
    // Leaving goes on with the list.
    QCOMPARE(fcc->state(), FocusCallCue::Idle);
    QCOMPARE(goOn.count(), 1);
    QCOMPARE(goOn.first().at(0).toUInt(), st->cueList(list)->chaser);
    // ...and does not arm the same cue again as the list moves off it.
    fcc->cueCurrent(list, focus);
    QCOMPARE(fcc->state(), FocusCallCue::Idle);

    // Gap 5: the list moved on by hand meanwhile -- no GO (it would skip).
    fcc->cueCurrent(list, house);
    fcc->cueCurrent(list, focus);
    QVERIFY(fcc->start());
    running = house;
    s->finish();
    QCOMPARE(goOn.count(), 1);
    // Stopped from outside (not Menu): said, and no GO.
    running = focus;
    fcc->cueCurrent(list, house);
    fcc->cueCurrent(list, focus);
    QVERIFY(fcc->start());
    s->stop();
    QCOMPARE(goOn.count(), 1);
    QCOMPARE(said.last().at(0).toString(), QString("Venue focus ended."));
    QCOMPARE(said.last().at(1).toInt(), int(ControllerScheme::ImportantPriority));
    fcc->cueCurrent(list, house);

    // Armed, then the list moves on before Start: let go.
    fcc->cueCurrent(list, house);
    fcc->cueCurrent(list, focus);
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
    fcc->cueCurrent(list, house);
    QCOMPARE(fcc->state(), FocusCallCue::Idle);

    /* Q127: the console's Feedback holds throughout -- Lights only, the armed
       cue says nothing (the cue page, Run tab and footer still show it). */
    fcc->cueCurrent(list, house);
    m_doc->programmer()->setOperatorFeedback(ControllerScheme::LightsFeedback);
    const int quiet = said.count();
    fcc->cueCurrent(list, focus);
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
    QCOMPARE(said.count(), quiet);
    fcc->cueCurrent(list, house);
    m_doc->programmer()->setOperatorFeedback(ControllerScheme::VoiceFeedback);
    fcc->cueCurrent(list, focus);
    QCOMPARE(said.last().at(0).toString(), QString("Venue focus ready. Press Start to begin."));
    m_doc->programmer()->setOperatorFeedback(ControllerScheme::BothFeedback);
    fcc->cueCurrent(list, house);

    // No calls in the show: Start says so and stays armed.
    m_doc->programmer()->focusCalls()->clear();
    fcc->cueCurrent(list, focus);
    QString why;
    QVERIFY(!fcc->start(&why));
    QCOMPARE(said.last().at(0).toString(), QString("This show has no focus calls yet."));
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
    fcc->setRunningCueProbe({});
    m_doc->setMode(Doc::Design);
}

void FocusCalls_Test::aFocusCallCueAsksForTheVenue()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    FixtureGroup *dh = makeGroup(m_doc, "DH Tspots", { a });
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    FocusCall c1;
    c1.groups << dh->id();
    c1.target = targetRef(dsc->id());
    m_doc->programmer()->focusCalls()->setCalls({ c1 });
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("Spring"), "Pre-show");
    const quint32 focus = st->addCue(list, -1, "Focus");
    QVERIFY(st->setCuesFocusCall({ focus }, true));
    VenueCorrections *v = m_doc->programmer()->venueCorrections();
    v->setShowFile(m_dir->filePath("Spring.qlcc"), false);   // a show with a file, no venue

    FocusCallCue *fcc = m_doc->programmer()->focusCallCue();
    FocusSession *s = m_doc->programmer()->focusSession();
    QSignalSpy said(m_doc->programmer(), &ProgrammerController::operatorAnnounce);
    QSignalSpy asked(fcc, &FocusCallCue::venueNeeded);
    m_doc->setMode(Doc::Operate);

    // Start with no venue: the laptop asks (said), nothing runs yet.
    fcc->cueCurrent(list, focus);
    QVERIFY(fcc->start());
    QCOMPARE(fcc->state(), FocusCallCue::NamingVenue);
    QCOMPARE(asked.count(), 1);
    QCOMPARE(asked.first().at(1).toInt(), 8000);
    QCOMPARE(said.last().at(0).toString(), QString("Name this venue on the laptop, or wait for today's date."));
    QVERIFY(!s->isActive());
    // A name: that venue, and the calls begin.
    QVERIFY(fcc->useVenue("Lincoln HS"));
    QCOMPARE(v->activeVenue(), QString("Lincoln HS"));
    QCOMPARE(fcc->state(), FocusCallCue::Running);
    QVERIFY(s->isActive());
    s->stop();

    // Nobody answers: today's date, said.
    QVERIFY(v->setActiveVenue(QString()));
    fcc->setVenueAskTimeout(50);
    fcc->cueCurrent(list, st->addCue(list, 0, "House"));
    fcc->cueCurrent(list, focus);
    QVERIFY(fcc->start());
    QCOMPARE(fcc->state(), FocusCallCue::NamingVenue);
    QTRY_COMPARE_WITH_TIMEOUT(fcc->state(), FocusCallCue::Running, 2000);
    QVERIFY(v->activeVenue().startsWith("Venue "));
    bool dated = false;
    for (const QList<QVariant> &l : said)
        dated = dated || l.at(0).toString() == QLatin1String("New venue, named for today.");
    QVERIFY(dated);
    s->stop();

    /* Q129: a venue applied is ALWAYS confirmed -- "Still at ...?", and
       said in Lights only too; the answer keeps it... */
    QVERIFY(v->setActiveVenue(v->addVenue("Lincoln HS")));
    const int venues = v->venueNames().size();
    fcc->setVenueAskTimeout(8000);
    m_doc->programmer()->setOperatorFeedback(ControllerScheme::LightsFeedback);
    fcc->cueCurrent(list, st->addCue(list, 0, "House 2"));
    fcc->cueCurrent(list, focus);
    QVERIFY(fcc->start());
    QCOMPARE(fcc->state(), FocusCallCue::NamingVenue);
    QCOMPARE(asked.last().at(2).toString(), QString("Lincoln HS"));
    QCOMPARE(said.last().at(0).toString(), QString("Still at Lincoln HS? Answer on the laptop, or wait for a new venue."));
    m_doc->programmer()->setOperatorFeedback(ControllerScheme::BothFeedback);
    QVERIFY(fcc->useVenue("Lincoln HS"));
    QCOMPARE(v->activeVenue(), QString("Lincoln HS"));
    QCOMPARE(v->venueNames().size(), venues);
    QVERIFY(said.last().at(0).toString().startsWith("Call 1, DH Tspots to DSC. "));
    QVERIFY(said.last().at(0).toString().contains(" Fixture 1, "));
    s->stop();
    // ...and nobody answering is a new venue, never the old one.
    fcc->setVenueAskTimeout(50);
    fcc->cueCurrent(list, st->addCue(list, 0, "House 3"));
    fcc->cueCurrent(list, focus);
    QVERIFY(fcc->start());
    QTRY_COMPARE_WITH_TIMEOUT(fcc->state(), FocusCallCue::Running, 2000);
    QVERIFY(v->activeVenue() != QLatin1String("Lincoln HS"));
    QCOMPARE(v->venueNames().size(), venues + 1);
    s->stop();
    m_doc->setMode(Doc::Design);
}

void FocusCalls_Test::thePadAndTheVoiceTellYouMore()
{
    Fixture *a = makeSpot(m_doc, "Spot 3", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "Spot 7", 4.0f, 2.0f);
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    ProgrammerController *pc = m_doc->programmer();
    FocusSession *s = pc->focusSession();
    s->setBlinkTimed(false);
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);
    QSignalSpy buzz(pc, &ProgrammerController::padRumbled);
    FocusSession::Options o;
    o.fixtures = { a->id(), b->id() };
    o.order = FocusSession::AsGiven;
    o.first = targetRef(dsc->id());
    QVERIFY(s->start(o));

    // 1. The light bar is the lamp: green, yellow (this one), red (another).
    // Gap 10: colour-blind safe -- blue, yellow, orange.
    const QColor green = ControllerScheme::savedColour(), yellow = ControllerScheme::thisUnsavedColour(),
                 red = ControllerScheme::othersUnsavedColour();
    QCOMPARE(s->saveStatus(), FocusSession::AllSaved);
    QCOMPARE(pc->padLight(), green);
    s->nudge(2.5f, -1.0f);
    QCOMPARE(s->saveStatus(), FocusSession::ThisUnsaved);
    QCOMPARE(pc->padLight(), yellow);

    // 4. What changed since the save, and the ghost where the saved one lands.
    QCOMPARE(s->deltaText(a->id()), QString("+2.5\u00b0 pan, \u22121.0\u00b0 tilt"));
    QVector3D ghost, ref;
    QVERIFY(s->savedGhost(a->id(), ghost));
    QVERIFY(s->referencePoint(a->id(), ref));
    QVERIFY((ghost - ref).length() > 0.05f);
    QVERIFY(qAbs(ghost.z() - ref.z()) < 0.01f);
    QVERIFY(!s->savedGhost(b->id(), ghost));            // only the head being worked

    s->next();
    QCOMPARE(s->saveStatus(), FocusSession::OthersUnsaved);
    QCOMPARE(pc->padLight(), red);
    s->nudge(1.0f, 0.0f);
    QCOMPARE(s->saveStatus(), FocusSession::BothUnsaved);

    // 2 + 3. A save buzzes twice (another light is still unsaved), and says it.
    s->save();
    QCOMPARE(buzz.last().at(0).toInt(), 2);
    QCOMPARE(said.last().at(0).toString(), QString("Saved fixture 2."));
    QVERIFY(s->deltaText(b->id()).isEmpty());

    // 6. Both bumpers: to the next light not saved, said.
    QVERIFY(s->nextUnsaved());
    QCOMPARE(s->currentFixture(), a->id());
    QCOMPARE(said.last().at(0).toString(), QString("Spot 3, not saved."));

    // 3. Exit with something unsaved says what.
    QSignalSpy exit(s, &FocusSession::exitRequested);
    s->apply(FocusSession::Exit);
    QCOMPARE(said.last().at(0).toString(), QString("Spot 3 not saved."));
    QCOMPARE(exit.count(), 1);

    // 5. Hold B: this light back to its last save; hold A: all saved.
    s->revertLight();
    QCOMPARE(said.last().at(0).toString(), QString("Fixture 1 reverted."));
    QVERIFY(!s->isDirty(a->id()));
    s->nudge(0.5f, 0.0f);
    s->next();
    s->nudge(0.5f, 0.0f);
    s->saveAllHeld();
    QCOMPARE(said.last().at(0).toString(), QString("Saved all, 2 lights."));
    QVERIFY(!s->hasUnsaved());
    QCOMPARE(pc->padLight(), green);
    QVERIFY(!s->nextUnsaved());
    QCOMPARE(said.last().at(0).toString(), QString("All saved."));
    s->nudge(0.5f, 0.0f);
    s->save();
    QCOMPARE(buzz.last().at(0).toInt(), 1);             // nothing else unsaved: once
    s->stop();
    QVERIFY(!pc->padLight().isValid());                 // back to the pad's own

    // B's hold: 0.6 s, as RS (A's is a second, Q132).
    ControllerScheme::LongPress hold;
    hold.press();
    QVERIFY(!hold.due(0.5));
    QVERIFY(hold.due(0.1));
}

void FocusCalls_Test::theControllerDoesItThroughTheProgrammer()
{
    QDir dir("../iopluginstub");
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY(m_doc->ioPluginCache()->plugins().size() > 0);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub in ../iopluginstub");
    IOPluginStub *stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_channelRoles = { { 0, "lx" }, { 1, "ly" }, { 2, "rx" }, { 3, "ry" },
                             { 10, "a" }, { 11, "b" }, { 14, "lb" }, { 15, "rb" },
                             { 16, "back" }, { 17, "start" }, { 18, "rs" } };
    stub->m_feedbackParams.clear();
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, stub->name(), QString(), 0));
    ProgrammerController *pc = m_doc->programmer();
    pc->autoBindFromProfile();
    auto down = [iom](quint32 ch) { emit iom->inputValueChanged(0, ch, 255, QString()); };
    auto up = [iom](quint32 ch) { emit iom->inputValueChanged(0, ch, 0, QString()); };
    // Each tick its nominal 20 ms: "not yet at 0.6 s" must not depend on the
    // machine's load (a late tick counts as up to 0.1 s held).
    auto held = [pc](int ticks) { joyTicks(pc, ticks); };

    Fixture *a = makeSpot(m_doc, "Spot 3", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "Spot 7", 4.0f, 2.0f);
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    FocusSession *s = pc->focusSession();
    s->setBlinkTimed(false);
    FocusSession::Options o;
    o.fixtures = { a->id(), b->id() };
    o.order = FocusSession::AsGiven;
    o.first = targetRef(dsc->id());
    QVERIFY(s->start(o));
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);

    // The pad's own light bar, through the plugin: the lamp's green.
    QVERIFY(!stub->m_feedbackParams.isEmpty());
    QVariantMap fb = stub->m_feedbackParams.last().toMap();
    QCOMPARE(fb.value("gamepad").toString(), QString("light"));
    QCOMPARE(fb.value("rgb").toString(), ControllerScheme::savedColour().name());

    // Both bumpers: the next light not saved.
    s->nudge(1.0f, 0.0f);
    s->next();
    s->nudge(1.0f, 0.0f);
    down(14);
    down(15);                                  // RB with LB held
    up(15);
    up(14);
    QCOMPARE(said.last().at(0).toString(), QString("Spot 3, not saved."));

    // A held a second (Q132): save all (the press saved this one first).
    down(10);
    held(30);
    QVERIFY(s->hasUnsaved());                  // not yet at 0.6 s
    held(25);
    up(10);
    QVERIFY(!s->hasUnsaved());
    QCOMPARE(said.last().at(0).toString(), QString("Saved all, 2 lights."));
    fb = stub->m_feedbackParams.last().toMap();          // ... and the buzz went out
    bool buzzed = false;
    for (const QVariant &v : stub->m_feedbackParams)
        buzzed = buzzed || v.toMap().value("gamepad").toString() == QLatin1String("rumble");
    QVERIFY(buzzed);

    // B held: this light back to its last save.
    s->nudge(0.5f, 0.0f);
    down(11);
    held(40);
    up(11);
    QCOMPARE(said.last().at(0).toString(), QString("Fixture 1 reverted."));

    // RS: a tap says the light; held, the feedback changes.
    down(18);
    up(18);
    QVERIFY(said.last().at(0).toString().startsWith("Spot 3. "));
    down(18);
    held(40);
    up(18);
    QCOMPARE(pc->operatorFeedback(), ControllerScheme::LightsFeedback);
    pc->setOperatorFeedback(ControllerScheme::BothFeedback);
    s->stop();
    QVERIFY(!stub->m_feedbackParams.last().toMap().value("rgb").toString().size());   // its own colour back
    iom->setInputPatch(0, QString(), QString(), 0);
}

void FocusCalls_Test::importantLinesAlwaysSpeak()
{
    Fixture *a = makeSpot(m_doc, "Spot 3", 2.0f, 2.0f);
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    ProgrammerController *pc = m_doc->programmer();
    FocusSession *s = pc->focusSession();
    s->setFlashEnabled(true);
    s->setBlinkTimed(false);
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);
    QSignalSpy buzz(pc, &ProgrammerController::padRumbled);
    FocusSession::Options o;
    o.fixtures = { a->id() };
    o.order = FocusSession::AsGiven;
    o.first = targetRef(dsc->id());
    QVERIFY(s->start(o));
    pc->setOperatorFeedback(ControllerScheme::LightsFeedback);

    // Lights only: a where line and a confirmation are not said...
    said.clear();
    s->nudge(1.0f, 0.0f);
    s->whereAmI();                             // (Say the light is asked for: said)
    said.clear();
    s->revert();
    QCOMPARE(said.count(), 0);
    QVERIFY(!s->isWarningFlash());
    // ...but a warning is, marked important, with the long blink and buzz.
    s->nudge(1.0f, 0.0f);
    s->apply(FocusSession::Exit);
    QCOMPARE(said.count(), 1);
    QCOMPARE(said.last().at(0).toString(), QString("Spot 3 not saved."));
    QCOMPARE(said.last().at(1).toInt(), int(ControllerScheme::ImportantPriority));
    QVERIFY(s->isWarningFlash());
    QCOMPARE(buzz.last().at(0).toInt(), 1);
    QCOMPARE(buzz.last().at(1).toInt(), ControllerScheme::rumbleLongMs());
    // The long blink: dark well past the selection's quick ones, then lit.
    QVERIFY(!FocusSession::warnLit(FocusSession::flashDurationMs()));
    QVERIFY(FocusSession::warnLit(FocusSession::warnDarkMs()));
    // A save buzzes short, and its blink is the double one.
    s->save();
    QCOMPARE(buzz.last().at(1).toInt(), ControllerScheme::rumbleShortMs());
    s->discardAll();
    s->stop();

    // A refusal on the armed cue, in Lights only: said too.
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("Spring"), "Pre-show");
    const quint32 focus = st->addCue(list, -1, "Focus");
    QVERIFY(st->setCuesFocusCall({ focus }, true));
    FocusCallCue *fcc = pc->focusCallCue();
    m_doc->setMode(Doc::Operate);
    said.clear();
    fcc->cueCurrent(list, focus);
    QCOMPARE(said.count(), 0);                 // "ready" is not a warning: quiet
    QVERIFY(!fcc->start());
    QCOMPARE(said.last().at(0).toString(), QString("This show has no focus calls yet."));
    QCOMPARE(said.last().at(1).toInt(), int(ControllerScheme::ImportantPriority));
    pc->setOperatorFeedback(ControllerScheme::BothFeedback);
    m_doc->setMode(Doc::Design);
}

void FocusCalls_Test::theControllerLostFreezesEverything()
{
    QDir dir("../iopluginstub");
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY(m_doc->ioPluginCache()->plugins().size() > 0);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub in ../iopluginstub");
    IOPluginStub *stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_channelRoles = { { 0, "lx" }, { 1, "ly" }, { 2, "rx" }, { 3, "ry" },
                             { 10, "a" }, { 11, "b" }, { 14, "lb" }, { 15, "rb" },
                             { 16, "back" }, { 17, "start" }, { 18, "rs" } };
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, stub->name(), QString(), 0));
    ProgrammerController *pc = m_doc->programmer();
    pc->autoBindFromProfile();
    auto value = [iom](quint32 ch, uchar v) { emit iom->inputValueChanged(0, ch, v, QString()); };
    auto held = [pc](int ticks) {
        for (int i = 0; i < ticks; ++i)
            pc->applyDesignJoystick();
    };
    auto drop = [iom, stub]() {
        iom->setInputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine());
        emit iom->inputDeviceDisconnected(0, stub->name(), QString("Stub pad"));
    };
    auto back = [iom, stub]() {
        iom->setInputPatch(0, stub->name(), QString(), 0);
        emit iom->inputDeviceConnected(0, stub->name(), QString("Stub pad"));
    };

    Fixture *a = makeSpot(m_doc, "Spot 3", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "Spot 7", 4.0f, 2.0f);
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    FocusSession *s = pc->focusSession();
    s->setBlinkTimed(false);
    FocusSession::Options o;
    o.fixtures = { a->id(), b->id() };
    o.order = FocusSession::AsGiven;
    o.first = targetRef(dsc->id());
    QVERIFY(s->start(o));
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);
    QSignalSpy lost(pc, &ProgrammerController::controllerLost);
    s->next();
    s->nudge(1.0f, 0.0f);                      // Spot 7 unsaved: what a keep-all would save
    s->previous();

    // The left stick full over, and A down: the light moves, A's hold runs.
    value(0, 255);
    held(10);
    QVERIFY(s->liveOffset(a->id()).pan > 0.5f);
    value(10, 255);
    held(10);
    // The pad drops mid-move, mid-hold.
    drop();
    QCOMPARE(lost.count(), 1);
    QCOMPARE(said.last().at(0).toString(), QString("Controller lost. Lights held."));
    QCOMPARE(said.last().at(1).toInt(), int(ControllerScheme::ImportantPriority));
    const FocusSession::Offset frozen = s->liveOffset(a->id());
    held(80);                                  // well past the keep-all hold
    QCOMPARE(s->liveOffset(a->id()), frozen);  // nothing moves
    QVERIFY(s->isDirty(b->id()));              // no keep-all fired
    for (const QList<QVariant> &l : said)
        QVERIFY(!l.at(0).toString().contains("lights kept"));
    // Back: said; it moves only when touched again.
    back();
    QCOMPARE(said.last().at(0).toString(), QString("Controller back."));
    QCOMPARE(lost.last().at(0).toBool(), false);
    held(20);
    QCOMPARE(s->liveOffset(a->id()), frozen);
    s->discardAll();
    s->stop();

    // Refine: the same.
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("Spring"), "Main");
    const quint32 cue = st->addCue(list, -1, "Ballad");
    FixtureGroup *g = makeGroup(m_doc, "Spots", { a, b });
    ShowTree::Target t;
    t.groups << g->id();
    const quint32 layer = st->addLayer(cue, t);
    ShowTree::Look look;
    look.key = ShowTree::KeyPosition;
    look.palettes << st->createInlinePalette(QLCPalette::PanTilt, { 270, 135 }, "Centre");
    QVERIFY(st->setLayerLook(cue, layer, look));
    RefineSession *r = pc->refineSession();
    r->setFlashEnabled(false);
    QVERIFY(r->start(cue));
    value(0, 255);
    held(10);
    float pan = 0.0f, tilt = 0.0f;
    QVERIFY(r->pendingPosition(a->id(), pan, tilt));
    QVERIFY(pan > 270.5f);
    value(10, 255);                            // A: kept; its hold runs
    held(5);
    drop();
    QCOMPARE(said.last().at(0).toString(), QString("Controller lost. Lights held."));
    float p0 = 0.0f, t0 = 0.0f;
    const bool had = r->pendingPosition(a->id(), p0, t0);
    held(80);
    float p1 = 0.0f, t1 = 0.0f;
    QCOMPARE(r->pendingPosition(a->id(), p1, t1), had);
    QCOMPARE(p1, p0);
    back();
    QCOMPARE(said.last().at(0).toString(), QString("Controller back."));
    r->revert();
    r->stop();
    iom->setInputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine());
}

void FocusCalls_Test::aToolStartedWithNoControllerSaysSoAndTheWokenPadIsBack()
{
    /* RC2: the pad asleep when Focus mode began -- nothing said, and when it
       woke nothing said either (only a pad lost mid-session was "back"). */
    QDir dir("../iopluginstub");
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub in ../iopluginstub");
    IOPluginStub *stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_channelRoles = { { 0, "lx" }, { 1, "ly" }, { 10, "a" }, { 11, "b" } };
    InputOutputMap *iom = m_doc->inputOutputMap();
    iom->setInputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine());
    ProgrammerController *pc = m_doc->programmer();
    // Important: said with Feedback on lights only too (Q131).
    const ControllerScheme::Feedback feedback = pc->operatorFeedback();
    pc->setOperatorFeedback(ControllerScheme::LightsFeedback);
    QVERIFY(!pc->focusController().present);

    Fixture *a = makeSpot(m_doc, "Spot 3", 2.0f, 2.0f);
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    FocusSession *s = pc->focusSession();
    s->setBlinkTimed(false);
    FocusSession::Options o;
    o.fixtures = { a->id() };
    o.first = targetRef(dsc->id());
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);
    QVERIFY(s->start(o));
    auto saidLine = [&said](const QString &text) {
        for (const QList<QVariant> &l : said)
            if (l.at(0).toString() == text)
                return l.at(1).toInt() == int(ControllerScheme::ImportantPriority);
        return false;
    };
    QVERIFY(saidLine("No controller connected. Using the keyboard."));

    // Woken: said, as a pad lost and back would be.
    said.clear();
    QVERIFY(iom->setInputPatch(0, stub->name(), QString(), 0));
    emit iom->inputDeviceConnected(0, stub->name(), QString("Stub pad"));
    QVERIFY(pc->focusController().present);
    QVERIFY(saidLine("Controller back."));
    QVERIFY(!pc->isControllerLost());
    s->stop();

    // A pad there at the start: no such line.
    said.clear();
    QVERIFY(s->start(o));
    QVERIFY(!saidLine("No controller connected. Using the keyboard."));
    s->stop();
    iom->setInputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine());
    pc->setOperatorFeedback(feedback);
}

void FocusCalls_Test::anOldVenueIsConfirmed()
{
    makeSpot(m_doc, "A", 2.0f, 2.0f);
    const QString show = m_dir->filePath("Spring.qlcc");
    const QDateTime now = QDateTime::currentDateTime();
    auto write = [&](const QDateTime &modified) {
        QFile f(VenueCorrections::sidecarPath(show));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QString("<QlcVenues Version=\"1\" Show=\"Spring.qlcc\" Active=\"Lincoln HS\">"
                        "<Venue Name=\"Lincoln HS\" Created=\"%1\" Modified=\"%1\"/></QlcVenues>")
                .arg(modified.toString(Qt::ISODate)).toUtf8());
    };
    VenueCorrections *v = m_doc->programmer()->venueCorrections();
    // Changed two days ago: asked.
    write(now.addDays(-2));
    v->setShowFile(show, true);
    QCOMPARE(v->activeVenue(), QString("Lincoln HS"));
    QVERIFY(v->needsConfirm());
    QVERIFY(!v->needsConfirm(now.addDays(-2).addSecs(3600)));    // an hour after: not old
    v->confirmVenue();
    QVERIFY(!v->needsConfirm());
    // Opened again: asked again.
    v->setShowFile(show, true);
    QVERIFY(v->needsConfirm());
    // Choosing a venue is confirming it.
    QVERIFY(v->setActiveVenue(QString()));
    QVERIFY(!v->needsConfirm());
    QVERIFY(v->setActiveVenue("Lincoln HS"));
    QVERIFY(!v->needsConfirm());
    // Changed this morning: not asked.
    write(now.addSecs(-3 * 3600));
    v->setShowFile(show, true);
    QVERIFY(!v->needsConfirm());
}

void FocusCalls_Test::unsavedWorkIsNeverDiscarded()
{
    Fixture *a = makeSpot(m_doc, "Spot 3", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "Spot 7", 4.0f, 2.0f);
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    ProgrammerController *pc = m_doc->programmer();
    FocusSession *s = pc->focusSession();
    RefineSession *r = pc->refineSession();
    r->setFlashEnabled(false);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("Spring"), "Main");
    const quint32 cue = st->addCue(list, -1, "Ballad");
    FixtureGroup *g = makeGroup(m_doc, "Spots", { a, b });
    ShowTree::Target t;
    t.groups << g->id();
    const quint32 layer = st->addLayer(cue, t);
    ShowTree::Look look;
    look.key = ShowTree::KeyPosition;
    look.palettes << st->createInlinePalette(QLCPalette::PanTilt, { 270, 135 }, "Centre");
    QVERIFY(st->setLayerLook(cue, layer, look));

    FocusSession::Options o;
    o.fixtures = { a->id(), b->id() };
    o.order = FocusSession::AsGiven;
    o.first = targetRef(dsc->id());
    QVERIFY(s->start(o));
    s->nudge(1.0f, 0.0f);
    // Refine refused while Focus mode has a light not saved; Focus kept.
    QString why;
    QVERIFY(!r->start(cue, 0xFFFFFFFF, &why));
    QCOMPARE(why, QString("Quick focus has 1 light not saved: save it (A) or put it back (B) first."));
    QVERIFY(s->isActive());
    QVERIFY(s->isDirty(a->id()));
    // Focus mode itself again: refused too.
    QVERIFY(!s->start(o, &why));
    QVERIFY(s->isDirty(a->id()));
    // Saved: Refine starts, and Focus mode gives way.
    s->save();
    QVERIFY(r->start(cue));
    QVERIFY(!s->isActive());
    // Refine with a change pending: Focus mode refused, Refine kept.
    QVERIFY(r->nudgePosition(2.0f, 0.0f));
    QVERIFY(!s->start(o, &why));
    QCOMPARE(why, QString("Focus this cue has 1 change not kept: keep it (A) or go back to the cue (B) first."));
    QVERIFY(r->isActive());
    QVERIFY(r->hasPending());
    r->revert();
    QVERIFY(s->start(o));
    QVERIFY(!r->isActive());
    s->stop();
}

void FocusCalls_Test::aSwappedFixtureNeedsFocus()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 4.0f, 2.0f);
    Fixture *other = makeSpot(m_doc, "Other", 6.0f, 2.0f);   // another type, for the swap
    MonitorProperties *props = m_doc->monitorProperties();
    const QString show = m_dir->filePath("Spring.qlcc");
    VenueCorrections *v = m_doc->programmer()->venueCorrections();
    v->setShowFile(show, false);
    QVERIFY(v->setActiveVenue(v->addVenue("Lincoln HS")));
    for (Fixture *f : { a, b })
    {
        FixtureRigProps rp = props->fixtureRigProps(f->id());
        rp.panOffsetDeg = 3.0f;
        props->setFixtureRigProps(f->id(), rp);       // recorded into the venue (Q111)
    }
    QVERIFY(v->hasCorrection(a->id()));
    QFile f(v->filePath());
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray xml = f.readAll();
    f.close();
    QVERIFY(xml.contains("Model=\"Calls Spot A\""));
    QVERIFY(xml.contains("Mode=\"3ch\""));

    // A is swapped for another fixture type: its correction is skipped.
    QVERIFY(v->setActiveVenue(QString()));
    a->setFixtureDefinition(other->fixtureDef(), other->fixtureMode());
    QVERIFY(v->setActiveVenue("Lincoln HS"));
    QCOMPARE(v->needsFocus(), QList<quint32>({ a->id() }));
    QCOMPARE(props->fixtureRigProps(a->id()).panOffsetDeg, 0.0f);   // not applied
    QCOMPARE(props->fixtureRigProps(b->id()).panOffsetDeg, 3.0f);
    QVERIFY(v->correctedAs(a->id()).contains("Calls Spot A"));
    // Focus mode says so on it.
    FocusSession *s = m_doc->programmer()->focusSession();
    FocusSession::Options o;
    o.fixtures = { a->id() };
    o.first = targetRef(makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f))->id());
    QVERIFY(s->start(o));
    QVERIFY(s->wherePhrase().endsWith("Needs focus: the fixture changed."));
    // Focused again and saved: its new type is what the venue keeps.
    s->nudge(1.0f, 0.0f);
    s->save();
    s->stop();
    QVERIFY(!v->needsFocus(a->id()));
    QVERIFY(v->correctedAs(a->id()).contains("Calls Spot Other"));

    // B deleted: its correction is kept, and listed as orphaned.
    const quint32 bid = b->id();
    QVERIFY(m_doc->deleteFixture(bid));
    QCOMPARE(v->orphaned(), QList<quint32>({ bid }));
    QVERIFY(v->venue("Lincoln HS").fixtures.contains(bid));

    // A file from before fixture types: applied, said once, then remembered.
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QString("<QlcVenues Version=\"1\" Show=\"Spring.qlcc\" Active=\"Kennedy\">"
                    "<Venue Name=\"Kennedy\"><Fixture ID=\"%1\" PanOfs=\"4.00\" TiltOfs=\"0.00\" PanZero=\"0.0\"/>"
                    "</Venue></QlcVenues>").arg(a->id()).toUtf8());
    f.close();
    QSignalSpy note(v, &VenueCorrections::note);
    v->setShowFile(show, true);
    QCOMPARE(v->activeVenue(), QString("Kennedy"));
    QCOMPARE(props->fixtureRigProps(a->id()).panOffsetDeg, 4.0f);
    QCOMPARE(note.count(), 1);
    v->setShowFile(show, true);                         // again: no note now
    QCOMPARE(note.count(), 1);
    QVERIFY(v->needsFocus().isEmpty());
}

void FocusCalls_Test::aVenueHasItsOwnTrimsAndTargets()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 4.0f, 2.0f);
    FixtureGroup *g = makeGroup(m_doc, "Spots", { a, b });
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    MonitorProperties *props = m_doc->monitorProperties();
    Truss *truss = props->addTruss();
    truss->setName("LX1");
    truss->setOrigin(QVector3D(0.0f, 3.0f, 6.0f));
    FocusCall c1;
    c1.groups << g->id();
    c1.target = targetRef(dsc->id());
    m_doc->programmer()->focusCalls()->setCalls({ c1 });
    VenueCorrections *v = m_doc->programmer()->venueCorrections();
    v->setShowFile(m_dir->filePath("Spring.qlcc"), false);
    QVERIFY(v->setActiveVenue(v->addVenue("Lincoln HS")));

    // The target page: the stick moves DSC, the heads aimed at it follow.
    FocusSession *s = m_doc->programmer()->focusSession();
    s->setBlinkTimed(false);
    FocusSession::Options o;
    o.calls = m_doc->programmer()->focusCalls()->sessionCalls();
    o.order = FocusSession::AsGiven;
    QVERIFY(s->start(o));
    QVector3D before;
    QVERIFY(s->referencePoint(a->id(), before));
    s->apply(FocusSession::ToggleReference);           // LS click in the calls
    QVERIFY(s->isTargetPage());
    s->nudge(10.0f, 5.0f);                              // 1 m across, 0.5 m upstage
    QVERIFY((dsc->position() - QVector3D(6.0f, 6.5f, 0.0f)).length() < 0.001f);
    QVector3D after;
    QVERIFY(s->referencePoint(a->id(), after));
    QVERIFY((after - before).length() > 0.5f);          // the head follows
    QVERIFY(!s->isDirty(a->id()));                      // its calibration untouched
    // Not kept: leaving the page or stepping is refused.
    QVERIFY(!s->toggleTargetPage());
    QVERIFY(s->isTargetPage());
    s->apply(FocusSession::Save);                       // A: kept for the venue
    QVERIFY(v->hasTargetOverride(dsc->id()));
    QVERIFY(s->toggleTargetPage());
    QVERIFY(!s->isTargetPage());
    s->stop();
    // The show keeps its own DSC; the venue's file has the room's.
    QByteArray xml = saveDoc(m_doc);
    QVERIFY(xml.contains("X=\"5.000\""));
    QVERIFY(!xml.contains("X=\"6.000\""));
    QVERIFY(v->toXml().contains("<Target ID="));

    // A trim in this room: the venue's, not the show's.
    QVector3D o2 = truss->origin();
    o2.setZ(5.5f);
    truss->setOrigin(o2);
    QVERIFY(v->recordTrim(truss->id()));
    QVERIFY(v->hasTrimOverride(truss->id()));
    xml = saveDoc(m_doc);
    QVERIFY(xml.contains("OriginZ=\"6.000\""));
    // None: the show's own back on the rig...
    QVERIFY(v->setActiveVenue(QString()));
    QCOMPARE(truss->origin().z(), 6.0f);
    QVERIFY((dsc->position() - QVector3D(5.0f, 6.0f, 0.0f)).length() < 0.001f);
    // ...and the venue's again when it is applied.
    QVERIFY(v->setActiveVenue("Lincoln HS"));
    QCOMPARE(truss->origin().z(), 5.5f);
    QVERIFY((dsc->position() - QVector3D(6.0f, 6.5f, 0.0f)).length() < 0.001f);
    // Read back from the file.
    VenueCorrections copy(nullptr);
    QVERIFY(copy.fromXml(v->toXml()));
    QCOMPARE(copy.venue("Lincoln HS").trims.value(truss->id()), 5.5f);
    // Cleared: the show's again.
    QVERIFY(v->clearTarget(dsc->id()));
    QVERIFY(!v->hasTargetOverride(dsc->id()));
    QVERIFY((dsc->position() - QVector3D(5.0f, 6.0f, 0.0f)).length() < 0.001f);
}

void FocusCalls_Test::theVenueFileTravels()
{
    QCOMPARE(VenueCorrections::showStem("/x/DanceHysteria-0.6.qlcc"), QString("DanceHysteria"));
    QCOMPARE(VenueCorrections::showStem("/x/DanceHysteria-0.5.venues.xml"), QString("DanceHysteria"));
    QCOMPARE(VenueCorrections::showStem("/x/Spring Show copy 2.qlcc"), QString("Spring Show"));
    QCOMPARE(VenueCorrections::showStem("/x/Spring Show (3).qlcc"), QString("Spring Show"));
    QCOMPARE(VenueCorrections::showStem("/x/Spring Show.qlcc"), QString("Spring Show"));

    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    VenueCorrections *v = m_doc->programmer()->venueCorrections();
    const QString v5 = m_dir->filePath("Dance-0.5.qlcc");
    v->setShowFile(v5, false);
    QVERIFY(v->setActiveVenue(v->addVenue("Lincoln HS")));
    FixtureRigProps rp = m_doc->monitorProperties()->fixtureRigProps(a->id());
    rp.panOffsetDeg = 2.0f;
    m_doc->monitorProperties()->setFixtureRigProps(a->id(), rp);
    QVERIFY(v->hasCorrection(a->id()));

    // Collect for Transfer: a copy beside the copied show, named for it.
    QDir(m_dir->path()).mkdir("Transfer");
    const QString moved = m_dir->filePath("Transfer/Dance-0.5.qlcc");
    QVERIFY(v->writeSidecarFor(moved));
    QFile t(VenueCorrections::sidecarPath(moved));
    QVERIFY(t.open(QIODevice::ReadOnly));
    QVERIFY(t.readAll().contains("Lincoln HS"));

    // Saved As 0.6 elsewhere (no sidecar): 0.5's is offered, and imported.
    const QString v6 = m_dir->filePath("Dance-0.6.qlcc");
    QCOMPARE(VenueCorrections::importCandidates(v6), QStringList({ VenueCorrections::sidecarPath(v5) }));
    QVERIFY(VenueCorrections::importCandidates(v5).isEmpty());          // has its own
    QVERIFY(VenueCorrections::importCandidates(m_dir->filePath("Other.qlcc")).isEmpty());
    v->setShowFile(v6, true);
    QVERIFY(!v->isActive());
    QVERIFY(v->importFrom(VenueCorrections::sidecarPath(v5)));
    QCOMPARE(v->activeVenue(), QString("Lincoln HS"));
    QCOMPARE(m_doc->monitorProperties()->fixtureRigProps(a->id()).panOffsetDeg, 2.0f);
    QVERIFY(QFile::exists(VenueCorrections::sidecarPath(v6)));
    QVERIFY(v->toXml().contains("Show=\"Dance-0.6.qlcc\""));
}

void FocusCalls_Test::theUnsavedLightBreathesAndViewRsCounts()
{
    QCOMPARE(FocusSession::breathLevel(0), 1.0f);
    QVERIFY(qAbs(FocusSession::breathLevel(FocusSession::breathPeriodMs() / 2) - FocusSession::breathLow()) < 0.001f);

    QDir dir("../iopluginstub");
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub in ../iopluginstub");
    IOPluginStub *stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_channelRoles = { { 0, "lx" }, { 1, "ly" }, { 10, "a" }, { 11, "b" },
                             { 16, "back" }, { 17, "start" }, { 18, "rs" } };
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, stub->name(), QString(), 0));
    ProgrammerController *pc = m_doc->programmer();
    pc->autoBindFromProfile();
    auto down = [iom](quint32 ch) { emit iom->inputValueChanged(0, ch, 255, QString()); };
    auto up = [iom](quint32 ch) { emit iom->inputValueChanged(0, ch, 0, QString()); };

    Fixture *a = makeSpot(m_doc, "Spot 3", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "Spot 7", 4.0f, 2.0f);
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    FocusSession *s = pc->focusSession();
    s->setFlashEnabled(true);
    s->setBlinkTimed(false);
    FocusSession::Options o;
    o.fixtures = { a->id(), b->id() };
    o.order = FocusSession::AsGiven;
    o.first = targetRef(dsc->id());
    QVERIFY(s->start(o));
    QVERIFY(!s->isBreathing());
    s->nudge(1.0f, 0.0f);
    QVERIFY(s->isBreathing());                       // unsaved: it breathes
    pc->setOperatorFeedback(ControllerScheme::VoiceFeedback);
    QVERIFY(!s->isBreathing());                      // voice only: no lights
    pc->setOperatorFeedback(ControllerScheme::BothFeedback);
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);
    s->next();
    s->nudge(1.0f, 0.0f);

    // View + RS: how many are not saved; View then does not undo.
    s->save();                                       // Spot 7 saved: undo would bring it back
    down(16);
    down(18);
    QCOMPARE(said.last().at(0).toString(), QString("1 not saved."));
    up(18);
    up(16);
    QCOMPARE(s->savedOffset(b->id()).pan, 1.0f);     // not undone
    // View alone: undoes, on its release.
    down(16);
    QCOMPARE(s->savedOffset(b->id()).pan, 1.0f);
    up(16);
    QCOMPARE(s->savedOffset(b->id()).pan, 0.0f);
    s->discardAll();
    s->stop();
    iom->setInputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine());
}

void FocusCalls_Test::theFirstPadOwnsIt()
{
    QDir dir("../iopluginstub");
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub in ../iopluginstub");
    IOPluginStub *stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_channelRoles = { { 0, "lx" }, { 1, "ly" }, { 10, "a" }, { 11, "b" },
                             { 16, "back" }, { 17, "start" }, { 18, "rs" } };
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, stub->name(), QString(), 0));    // pad 1
    QVERIFY(iom->setInputPatch(1, stub->name(), QString(), 1));    // pad 2
    ProgrammerController *pc = m_doc->programmer();
    pc->autoBindFromProfile();
    auto value = [iom](quint32 u, quint32 ch, uchar v) { emit iom->inputValueChanged(u, ch, v, QString()); };

    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    FixtureGroup *g = makeGroup(m_doc, "Spots", { a });
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    FocusCall c1;
    c1.groups << g->id();
    c1.target = targetRef(dsc->id());
    pc->focusCalls()->setCalls({ c1 });
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("Spring"), "Pre-show");
    const quint32 focus = st->addCue(list, -1, "Focus");
    QVERIFY(st->setCuesFocusCall({ focus }, true));
    FocusCallCue *fcc = pc->focusCallCue();
    FocusSession *s = pc->focusSession();
    m_doc->setMode(Doc::Operate);
    fcc->cueCurrent(list, focus);
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);

    // Pad 2 presses Start first: it owns the calls.
    value(1, 17, 255);
    value(1, 17, 0);
    QCOMPARE(fcc->state(), FocusCallCue::Running);
    QCOMPARE(pc->controllerOwner(), quint32(1));
    QCOMPARE(pc->controllerOwnerName(), QString("2: Stub 2"));
    // Pad 1: ignored, and told so -- its A saves nothing, its stick moves nothing.
    s->nudge(1.0f, 0.0f);
    value(0, 10, 255);
    QCOMPARE(said.last().at(0).toString(), QString("Another controller is in use."));
    value(0, 10, 0);
    QVERIFY(s->isDirty(a->id()));
    const FocusSession::Offset before = s->liveOffset(a->id());
    value(0, 0, 255);
    for (int i = 0; i < 10; ++i)
        pc->applyDesignJoystick();
    QCOMPARE(s->liveOffset(a->id()), before);
    // Pad 2's A saves.
    value(1, 10, 255);
    value(1, 10, 0);
    QVERIFY(!s->isDirty(a->id()));
    s->stop();
    QCOMPARE(pc->controllerOwner(), quint32(0xFFFFFFFF));             // let go when done
    m_doc->setMode(Doc::Design);
    iom->setInputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine());
    iom->setInputPatch(1, QString(), QString(), QLCIOPlugin::invalidLine());
}

void FocusCalls_Test::inAVenueKeptSaysWhereAndXIsTheShows()
{
    Fixture *a = makeSpot(m_doc, "Spot 3", 2.0f, 2.0f);
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    MonitorProperties *props = m_doc->monitorProperties();
    FixtureRigProps rp = props->fixtureRigProps(a->id());
    rp.panOffsetDeg = 2.0f;                                    // the show's own
    props->setFixtureRigProps(a->id(), rp);
    VenueCorrections *v = m_doc->programmer()->venueCorrections();
    v->setShowFile(m_dir->filePath("Spring.qlcc"), false);
    QVERIFY(v->setActiveVenue(v->addVenue("Lincoln HS")));
    FocusSession *s = m_doc->programmer()->focusSession();
    QSignalSpy said(m_doc->programmer(), &ProgrammerController::operatorAnnounce);
    FocusSession::Options o;
    o.fixtures = { a->id() };
    o.first = targetRef(dsc->id());
    QVERIFY(s->start(o));
    s->nudge(1.0f, 0.0f);
    s->save();
    QCOMPARE(said.last().at(0).toString(), QString("Saved fixture 1 to Lincoln HS."));
    // Never the sidecar's name: the venue's.
    for (const QList<QVariant> &l : said)
        QVERIFY2(!l.at(0).toString().contains("venues.xml") && !l.at(0).toString().contains("Spring"),
                 qPrintable(l.at(0).toString()));
    // Hold A in a venue: confirmed, with where they went.
    s->nudge(0.5f, 0.0f);
    s->saveAllHeld();
    QCOMPARE(said.last().at(0).toString(), QString("Saved all to Lincoln HS, 1 light."));
    s->zero();
    QCOMPARE(s->liveOffset(a->id()).pan, 2.0f);               // the show's, not 0
    QCOMPARE(said.last().at(0).toString(), QString("Back to the show's calibration."));
    s->discardAll();
    s->stop();
}

void FocusCalls_Test::nothingReachingTheRigIsSaid()
{
    Fixture *a = makeSpot(m_doc, "Spot 3", 2.0f, 2.0f);
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    ProgrammerController *pc = m_doc->programmer();
    InputOutputMap *iom = m_doc->inputOutputMap();
    FocusSession *s = pc->focusSession();
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);
    FocusSession::Options o;
    o.fixtures = { a->id() };
    o.first = targetRef(dsc->id());
    QVERIFY(pc->rigOutputProblem(a->id()).isEmpty());       // no outputs at all: previewing
    iom->setBlackout(true);
    QVERIFY(s->start(o));
    bool told = false;
    for (const QList<QVariant> &l : said)
        told = told || (l.at(0).toString() == QLatin1String("Blackout is on: the rig shows nothing.")
                        && l.at(1).toInt() == int(ControllerScheme::ImportantPriority));
    QVERIFY(told);
    iom->setBlackout(false);
    QVERIFY(pc->rigOutputProblem(a->id()).isEmpty());
    iom->setGrandMasterValue(0);
    QCOMPARE(pc->rigOutputProblem(a->id()), QString("The Grand Master is at zero: the rig shows nothing."));
    QCOMPARE(said.last().at(0).toString(), QString("The Grand Master is at zero: the rig shows nothing."));
    iom->setGrandMasterValue(255);

    // Another universe has an output, Spot 3's has none.
    QDir dir("../iopluginstub");
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub in ../iopluginstub");
    IOPluginStub *stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(iom->setOutputPatch(1, stub->name(), QString(), 0));
    QCOMPARE(pc->rigOutputProblem(a->id()), QString("Spot 3's universe has no output: it shows nothing."));
    s->stop();
    iom->setOutputPatch(1, QString(), QString(), QLCIOPlugin::invalidLine());
}

QTEST_GUILESS_MAIN(FocusCalls_Test)

void FocusCalls_Test::holdBPutsEveryLightBack()
{
    QDir dir("../iopluginstub");
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY(m_doc->ioPluginCache()->plugins().size() > 0);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub in ../iopluginstub");
    IOPluginStub *stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_channelRoles = { { 0, "lx" }, { 1, "ly" }, { 2, "rx" }, { 3, "ry" },
                             { 10, "a" }, { 11, "b" }, { 14, "lb" }, { 15, "rb" },
                             { 16, "back" }, { 17, "start" }, { 18, "rs" } };
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, stub->name(), QString(), 0));
    ProgrammerController *pc = m_doc->programmer();
    pc->autoBindFromProfile();
    auto down = [iom](quint32 ch) { emit iom->inputValueChanged(0, ch, 255, QString()); };
    auto up = [iom](quint32 ch) { emit iom->inputValueChanged(0, ch, 0, QString()); };
    auto held = [pc](int ticks) {
        for (int i = 0; i < ticks; ++i)
            pc->applyDesignJoystick();
    };

    Fixture *a = makeSpot(m_doc, "Spot 3", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "Spot 7", 4.0f, 2.0f);
    Fixture *c = makeSpot(m_doc, "Spot 9", 6.0f, 2.0f);
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    FocusSession *s = pc->focusSession();
    s->setBlinkTimed(false);
    FocusSession::Options o;
    o.fixtures = { a->id(), b->id(), c->id() };
    o.order = FocusSession::AsGiven;
    o.first = targetRef(dsc->id());
    QVERIFY(s->start(o));
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);

    // A saved, then nudged again; B and C nudged, never saved.
    s->nudge(1.0f, 0.0f);
    s->save();
    s->nudge(0.5f, 0.0f);
    s->next();
    s->nudge(2.0f, 1.0f);
    s->next();
    s->nudge(-1.0f, 0.0f);
    s->setIndex(0);
    QCOMPARE(s->unsavedFixtures().size(), 3);
    const FocusSession::Offset liveB = s->liveOffset(b->id());

    // B held: the press puts this one back, the hold every other one.
    down(11);
    QVERIFY(!s->isDirty(a->id()));
    QVERIFY(s->isDirty(b->id()));
    held(40);
    up(11);
    QVERIFY(!s->hasUnsaved());
    QCOMPARE(said.last().at(0).toString(), QString("Reverted all, 3 lights."));
    QCOMPARE(s->liveOffset(a->id()).pan, 1.0f);          // its save, not zero
    QCOMPARE(s->liveOffset(b->id()).pan, 0.0f);
    QCOMPARE(m_doc->monitorProperties()->fixtureRigProps(b->id()).panOffsetDeg, 0.0f);

    // View: all three back, still not saved -- one step.
    down(16);
    up(16);
    QCOMPARE(s->unsavedFixtures().size(), 3);
    QCOMPARE(s->liveOffset(b->id()), liveB);
    QCOMPARE(s->liveOffset(a->id()).pan, 1.5f);
    QCOMPARE(said.last().at(0).toString(), QString("Undone: the changes are back, not saved."));
    // The save before it is still there to undo.
    QVERIFY(s->canUndo());

    // Nothing unsaved: said so.
    s->saveAll();
    s->revertAllHeld();
    QCOMPARE(said.last().at(0).toString(), QString("Nothing to put back: all saved."));
    s->stop();
    iom->setInputPatch(0, QString(), QString(), 0);
}

void FocusCalls_Test::theSmallIdeas()
{
    // The bare beam to focus: gobo open, prism out, no frost.
    {
        QLCChannel gobo;
        gobo.setGroup(QLCChannel::Gobo);
        gobo.addCapability(new QLCCapability(0, 9, "Open"));
        gobo.addCapability(new QLCCapability(10, 20, "Gobo 1"));
        uchar v = 99;
        QVERIFY(FocusSession::openBeamValue(&gobo, v));
        QCOMPARE(int(v), 4);
        QLCChannel prism;
        prism.setName("Prism");
        prism.setGroup(QLCChannel::Prism);
        prism.addCapability(new QLCCapability(0, 127, "Open"));
        prism.addCapability(new QLCCapability(128, 255, "Prism"));
        QVERIFY(FocusSession::openBeamValue(&prism, v));
        QCOMPARE(int(v), 63);
        QLCChannel frost;
        frost.setName("Frost");
        frost.setGroup(QLCChannel::Beam);
        QVERIFY(FocusSession::openBeamValue(&frost, v));
        QCOMPARE(int(v), 0);
        QLCChannel index;
        index.setGroup(QLCChannel::Gobo);
        index.setPreset(QLCChannel::GoboIndex);
        QVERIFY(!FocusSession::openBeamValue(&index, v));
        QLCChannel zoom;
        zoom.setName("Zoom");
        zoom.setGroup(QLCChannel::Beam);
        QVERIFY(!FocusSession::openBeamValue(&zoom, v));
    }

    QDir dir("../iopluginstub");
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY(m_doc->ioPluginCache()->plugins().size() > 0);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub in ../iopluginstub");
    IOPluginStub *stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_channelRoles = { { 0, "lx" }, { 1, "ly" }, { 2, "rx" }, { 3, "ry" },
                             { 10, "a" }, { 11, "b" }, { 12, "x" }, { 14, "lb" }, { 15, "rb" },
                             { 16, "back" }, { 17, "start" }, { 18, "rs" }, { 19, "ls" } };
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, stub->name(), QString(), 0));
    ProgrammerController *pc = m_doc->programmer();
    pc->autoBindFromProfile();
    auto down = [iom](quint32 ch) { emit iom->inputValueChanged(0, ch, 255, QString()); };
    auto up = [iom](quint32 ch) { emit iom->inputValueChanged(0, ch, 0, QString()); };
    auto axis = [iom](quint32 ch, uchar v) { emit iom->inputValueChanged(0, ch, v, QString()); };
    auto held = [pc](int ticks) {
        for (int i = 0; i < ticks; ++i)
            pc->applyDesignJoystick();
    };

    Fixture *a = makeSpot(m_doc, "Spot 3", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "Spot 7", 4.0f, 2.0f);
    Fixture *c = makeSpot(m_doc, "Spot 9", 6.0f, 2.0f);
    FixtureGroup *dh = makeGroup(m_doc, "Movers", { a, b });
    FixtureGroup *at = makeGroup(m_doc, "AT spots", { c });
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    StageTarget *solo = makeTarget(m_doc, "Soloist", QVector3D(1.0f, 7.0f, 0.0f));
    FocusCall c1;
    c1.groups << dh->id();
    c1.target = targetRef(dsc->id());
    FocusCall c2;
    c2.groups << at->id();
    c2.target = targetRef(solo->id());
    m_doc->programmer()->focusCalls()->setCalls({ c1, c2 });
    VenueCorrections *v = pc->venueCorrections();
    v->setShowFile(m_dir->filePath("Tour.qlcc"), false);

    FocusSession *s = pc->focusSession();
    s->setBlinkTimed(false);
    FocusSession::Options o;
    o.calls = pc->focusCalls()->sessionCalls();
    o.order = FocusSession::AsGiven;
    QVERIFY(s->start(o));
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);

    // View + X with no venue: refused, and said.
    down(16);
    down(12);
    up(12);
    up(16);
    QCOMPARE(said.last().at(0).toString(), QString("Apply a venue to mark a light dead."));
    QVERIFY(!s->canUndo());
    s->stop();
    QVERIFY(v->setActiveVenue(v->addVenue("Lincoln HS")));
    QVERIFY(s->start(o));
    // View + X: dead, and the View release does not undo.
    down(16);
    down(12);
    up(12);
    up(16);
    QVERIFY(v->isDead(a->id()));
    QCOMPARE(said.last().at(0).toString(), QString("Spot 3 marked dead."));
    QVERIFY(s->wherePhrase().endsWith("Marked dead."));
    // ...and again: not dead.
    QVERIFY(s->toggleDead());
    QVERIFY(!v->isDead(a->id()));
    QCOMPARE(said.last().at(0).toString(), QString("Spot 3 not dead."));

    // A light picked on the plot: its call, at fixture scope.
    s->cycleScope();
    QVERIFY(s->jumpTo(c->id()));
    QCOMPARE(s->callIndex(), 1);
    QCOMPARE(s->currentFixture(), c->id());
    QCOMPARE(s->scope(), FocusSession::FixtureScope);
    QVERIFY(s->jumpTo(b->id()));
    QCOMPARE(s->callIndex(), 0);
    QCOMPARE(s->currentFixture(), b->id());
    QVERIFY(!s->jumpTo(0xFFFF));

    // LS alone: on its release, the target page (the stick clicks can chord).
    down(19);
    QVERIFY(!s->isTargetPage());
    up(19);
    QVERIFY(s->isTargetPage());
    down(19);
    up(19);
    QVERIFY(!s->isTargetPage());

    // Both stick clicks: every light holds.
    const FocusSession::Offset before = s->liveOffset(b->id());
    QSignalSpy holding(pc, &ProgrammerController::holdingChanged);
    down(18);
    down(19);
    QVERIFY(pc->isHolding());
    QVERIFY(s->isHolding());
    QCOMPARE(holding.count(), 1);
    QCOMPARE(said.last().at(0).toString(), QString("Holding."));
    up(19);
    up(18);
    // No feedback cycle from RS's hold, no target page from LS.
    QCOMPARE(pc->operatorFeedback(), ControllerScheme::BothFeedback);
    QVERIFY(!s->isTargetPage());
    // The stick, A, the bumpers: nothing moves.
    axis(0, 255);
    held(25);
    axis(0, 128);
    down(10);
    up(10);
    down(15);
    up(15);
    QCOMPARE(s->liveOffset(b->id()), before);
    QCOMPARE(s->currentFixture(), b->id());
    QVERIFY(!s->isBreathing());
    // Again: released.
    down(19);
    down(18);
    QVERIFY(!pc->isHolding());
    QVERIFY(!s->isHolding());
    QCOMPARE(said.last().at(0).toString(), QString("Released."));
    up(18);
    up(19);
    axis(0, 255);
    held(10);
    axis(0, 128);
    QVERIFY(s->liveOffset(b->id()) != before);
    // A tool that ends while holding leaves nothing held.
    pc->setHolding(true);
    s->discardAll();
    s->stop();
    QVERIFY(!pc->isHolding());
    iom->setInputPatch(0, QString(), QString(), 0);
}

/* CHANGELOG [0.2.2] "Crew words": the D-pad's beam presses are spoken in
   the words the crew uses -- up "Spot down.", down "Flood out.", right
   "Sharper.", left "Softer." -- once per press, and only where the light
   has the zoom or focus to move. */
void FocusCalls_Test::theDpadSaysTheCrewsWords()
{
    IOPluginStub *stub = plugInStubPad(m_doc, { { 0, "lx" }, { 1, "ly" }, { 10, "a" }, { 11, "b" },
                                                { 14, "lb" }, { 15, "rb" },
                                                { 16, "back" }, { 17, "start" },
                                                { 20, "dpad_up" }, { 21, "dpad_down" },
                                                { 22, "dpad_left" }, { 23, "dpad_right" } });
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    InputOutputMap *iom = m_doc->inputOutputMap();
    ProgrammerController *pc = m_doc->programmer();
    auto press = [iom](quint32 ch) {
        emit iom->inputValueChanged(0, ch, 255, QString());
        emit iom->inputValueChanged(0, ch, 0, QString());
    };

    Fixture *z = makeZoomSpot(m_doc, "Spot 5", 2.0f, 2.0f, 100);
    Fixture *plain = makeSpot(m_doc, "Spot 8", 4.0f, 2.0f);
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    FocusSession *s = pc->focusSession();
    s->setBlinkTimed(false);
    FocusSession::Options o;
    o.fixtures = { z->id(), plain->id() };
    o.order = FocusSession::AsGiven;
    o.first = targetRef(dsc->id());
    QVERIFY(s->start(o));
    QCOMPARE(s->currentFixture(), z->id());
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);

    const struct { quint32 ch; const char *word; } presses[] = {
        { 20, "Spot down." }, { 21, "Flood out." }, { 23, "Sharper." }, { 22, "Softer." } };
    for (const auto &p : presses)
    {
        const int before = said.count();
        press(p.ch);
        QCOMPARE(said.count(), before + 1);         // once, not per repeat
        QCOMPARE(said.last().at(0).toString(), QString(p.word));
    }

    // A light with no zoom or focus: the arms say nothing.
    press(15);                                      // RB: the next light
    QCOMPARE(s->currentFixture(), plain->id());
    said.clear();
    for (const auto &p : presses)
        press(p.ch);
    const QStringList words = { "Spot down.", "Flood out.", "Sharper.", "Softer." };
    for (int i = 0; i < said.count(); ++i)
        QVERIFY2(!words.contains(said.at(i).at(0).toString()), qPrintable(said.at(i).at(0).toString()));

    s->stop();
    iom->setInputPatch(0, QString(), QString(), 0);
}

/* The changelog: "A focus-call cue arms on GO and starts on the
   controller's Start" -- here through real playback: ShowControl's GO, the
   cue list's chaser stepped by the MasterTimer, FocusCallCue watching it
   (MasterTimer::functionStarted, Chaser::currentStepChanged). Leaving the
   calls goes on with the list; a list moved on by hand lets it go; a
   skipped focus-call cue is passed over by GO. */
void FocusCalls_Test::aFocusCallCueArmsOnARealGo()
{
    IOPluginStub *stub = plugInStubPad(m_doc, { { 0, "lx" }, { 1, "ly" }, { 10, "a" }, { 11, "b" },
                                                { 16, "back" }, { 17, "start" } });
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    InputOutputMap *iom = m_doc->inputOutputMap();
    auto press = [iom](quint32 ch) {
        emit iom->inputValueChanged(0, ch, 255, QString());
        emit iom->inputValueChanged(0, ch, 0, QString());
    };
    ProgrammerController *pc = m_doc->programmer();

    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 4.0f, 2.0f);
    FixtureGroup *dh = makeGroup(m_doc, "DH Tspots", { a, b });
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    FocusCall c1;
    c1.groups << dh->id();
    c1.target = targetRef(dsc->id());
    pc->focusCalls()->setCalls({ c1 });

    // Pre-show: House, the focus-call cue, then the show -- each with a look.
    QLCPalette *half = new QLCPalette(QLCPalette::Dimmer, m_doc);
    half->setName("Half");
    half->setValue(127);
    m_doc->addPalette(half);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("Spring"), "Pre-show");
    const quint32 house = st->addCue(list, -1, "House");
    const quint32 focus = st->addCue(list, 0, "Focus");
    const quint32 show = st->addCue(list, 1, "Show");
    ShowTree::Target t;
    t.groups << dh->id();
    for (quint32 c : { house, focus, show })
        QVERIFY(st->setTargetLook(c, t, st->lookForPalette(half->id())));
    QVERIFY(st->setCuesFocusCall({ focus }, true));
    const quint32 chaser = st->cueList(list)->chaser;
    QVERIFY(qobject_cast<Chaser *>(m_doc->function(chaser)) != nullptr);

    ShowControl *sc = m_doc->showControl();
    auto command = [sc, chaser](ShowControl::Command c) {
        ShowControl::Request r;
        r.command = c;
        r.chaser = chaser;
        return sc->execute(r);
    };
    // The MasterTimer's ticks, and the main thread's queue (the watch is queued).
    auto play = [this](int ticks) {
        for (int i = 0; i < ticks; ++i)
        {
            m_doc->masterTimer()->timerTick();
            QCoreApplication::processEvents();
        }
    };
    FocusCallCue *fcc = pc->focusCallCue();
    FocusSession *s = pc->focusSession();
    QSignalSpy states(fcc, &FocusCallCue::stateChanged);
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);
    QSignalSpy goOn(fcc, &FocusCallCue::continueRequested);
    auto readySaid = [&said]() {
        int n = 0;
        for (const QList<QVariant> &l : said)
            n += l.at(0).toString() == FocusCallCue::armedPhrase() ? 1 : 0;
        return n;
    };
    m_doc->setMode(Doc::Operate);

    // GO: the house cue -- nothing armed.
    QVERIFY(command(ShowControl::Go));
    play(5);
    QCOMPARE(st->runningCue(list), house);
    QCOMPARE(fcc->state(), FocusCallCue::Idle);
    // GO onto the focus-call cue: armed, said once however long it plays.
    QVERIFY(command(ShowControl::Go));
    play(5);
    QCOMPARE(st->runningCue(list), focus);
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
    QCOMPARE(fcc->cueId(), focus);
    QCOMPARE(fcc->listId(), list);
    play(50);
    QCOMPARE(readySaid(), 1);
    // Nothing starts until Start; the list waits on the cue.
    QVERIFY(!s->isActive());
    QCOMPARE(st->runningCue(list), focus);

    // The controller's Start: the calls, in Operate.
    press(17);
    QCOMPARE(fcc->state(), FocusCallCue::Running);
    QVERIFY(s->isActive());
    QVERIFY(s->isRunningCalls());
    play(5);
    QCOMPARE(st->runningCue(list), focus);
    // RT on the last call asks to leave; leaving (the panel's finish) goes
    // on with the list -- a real GO, the next cue playing.
    QSignalSpy exit(s, &FocusSession::exitRequested);
    s->setTrigger(1, 1.0f);
    s->setTrigger(1, 0.0f);
    QCOMPARE(exit.count(), 1);
    s->finish();
    QCOMPARE(goOn.count(), 1);
    QCOMPARE(goOn.first().at(0).toUInt(), chaser);
    play(5);
    QCOMPARE(st->runningCue(list), show);
    QCOMPARE(fcc->state(), FocusCallCue::Idle);
    QCOMPARE(readySaid(), 1);                   // not armed again on the way off

    // BACK onto it by hand: armed again; GO by hand before Start lets it go.
    QVERIFY(command(ShowControl::Back));
    play(5);
    QCOMPARE(st->runningCue(list), focus);
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
    QVERIFY(command(ShowControl::Go));
    play(5);
    QCOMPARE(st->runningCue(list), show);
    QCOMPARE(fcc->state(), FocusCallCue::Idle);
    QVERIFY(!s->isActive());

    // Skipped, the focus-call cue is passed over by GO: never armed.
    QVERIFY(st->setCuesSkipped({ focus }, true));
    QVERIFY(command(ShowControl::Stop));
    play(5);
    states.clear();
    QVERIFY(command(ShowControl::Go));
    play(5);
    QCOMPARE(st->runningCue(list), house);
    QVERIFY(command(ShowControl::Go));
    play(5);
    QCOMPARE(st->runningCue(list), show);
    for (const QList<QVariant> &l : states)
        QVERIFY(l.at(0).value<FocusCallCue::State>() != FocusCallCue::Armed);
    QCOMPARE(fcc->state(), FocusCallCue::Idle);

    QVERIFY(command(ShowControl::Stop));
    play(5);
    m_doc->setMode(Doc::Design);
    iom->setInputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine());
}

/* The changelog's safety promise, "a controller lost freezes everything it
   held", on the rig's output: the pad drops mid-move with the stick over
   and a D-pad arm repeating -- the universe's bytes stay where they were,
   in Focus mode and in a focus-call run a cue started. */
void FocusCalls_Test::aLostControllerFreezesTheRigOutput()
{
    IOPluginStub *stub = plugInStubPad(m_doc, { { 0, "lx" }, { 1, "ly" }, { 2, "rx" }, { 3, "ry" },
                                                { 10, "a" }, { 11, "b" }, { 16, "back" },
                                                { 17, "start" }, { 21, "dpad_right" } });
    QVERIFY2(stub != nullptr, "no I/O plugin stub in ../iopluginstub");
    InputOutputMap *iom = m_doc->inputOutputMap();
    ProgrammerController *pc = m_doc->programmer();
    auto value = [iom](quint32 ch, uchar v) { emit iom->inputValueChanged(0, ch, v, QString()); };
    auto drop = [iom, stub]() {
        iom->setInputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine());
        emit iom->inputDeviceDisconnected(0, stub->name(), QString("Stub pad"));
    };
    auto back = [iom, stub]() {
        iom->setInputPatch(0, stub->name(), QString(), 0);
        emit iom->inputDeviceConnected(0, stub->name(), QString("Stub pad"));
    };
    Universe *uni = iom->universes().at(0);
    // A frame: the functions' tick (a cue list playing), then the universe.
    auto frames = [this, uni](int n) {
        for (int i = 0; i < n; ++i)
        {
            m_doc->masterTimer()->timerTick();
            QCoreApplication::processEvents();
            uni->processFaders();
        }
    };
    auto bytes = [uni](Fixture *f) {
        QByteArray out;
        for (int c = 0; c < 5; ++c)
            out.append(char(uni->preGMValue(int(f->address()) + c)));
        return out;
    };

    Fixture *z = makeZoomSpot(m_doc, "Spot 5", 2.0f, 2.0f, 100);
    Fixture *w = makeZoomSpot(m_doc, "Spot 6", 4.0f, 2.0f, 110);
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    FocusSession *s = pc->focusSession();
    s->setBlinkTimed(false);
    FocusSession::Options o;
    o.fixtures = { z->id(), w->id() };
    o.order = FocusSession::AsGiven;
    o.first = targetRef(dsc->id());
    QVERIFY(s->start(o));
    QCOMPARE(s->currentFixture(), z->id());
    frames(2);
    const QByteArray atStart = bytes(z);

    // The left stick full over and the D-pad's right arm held (sharper):
    // past the repeat delay, the focus goes on stepping.
    value(0, 255);
    value(21, 255);
    joyTicks(pc, 30);                           // 0.6 s
    frames(2);
    double focusNow = 0.0;
    QVERIFY(s->focusLevel(z->id(), focusNow));
    QVERIFY2(focusNow > 0.53, qPrintable(QString::number(focusNow)));   // repeated, not just pressed
    QVERIFY(bytes(z).at(0) != atStart.at(0));   // pan moved on the rig
    QVERIFY(bytes(z).at(4) != atStart.at(4));   // and the focus channel

    // The pad drops: what is on the rig stays, however long.
    drop();
    frames(2);
    const QByteArray frozenZ = bytes(z), frozenW = bytes(w);
    const FocusSession::Offset frozen = s->liveOffset(z->id());
    double focusFrozen = 0.0;
    QVERIFY(s->focusLevel(z->id(), focusFrozen));
    joyTicks(pc, 100);                          // 2 s of ticks
    frames(5);
    QCOMPARE(bytes(z), frozenZ);
    QCOMPARE(bytes(w), frozenW);
    QCOMPARE(s->liveOffset(z->id()), frozen);
    double focusAfter = 0.0;
    QVERIFY(s->focusLevel(z->id(), focusAfter));
    QCOMPARE(focusAfter, focusFrozen);
    back();
    value(0, 128);
    value(21, 0);
    s->discardAll();
    s->stop();

    // The same in a focus-call run a cue started (Operate).
    FixtureGroup *g = makeGroup(m_doc, "Zoom spots", { z, w });
    FocusCall c1;
    c1.groups << g->id();
    c1.target = targetRef(dsc->id());
    pc->focusCalls()->setCalls({ c1 });
    QLCPalette *half = new QLCPalette(QLCPalette::Dimmer, m_doc);
    half->setName("Half");
    half->setValue(127);
    m_doc->addPalette(half);
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("Spring"), "Pre-show");
    const quint32 focus = st->addCue(list, -1, "Focus");
    ShowTree::Target t;
    t.groups << g->id();
    QVERIFY(st->setTargetLook(focus, t, st->lookForPalette(half->id())));
    QVERIFY(st->setCuesFocusCall({ focus }, true));
    m_doc->setMode(Doc::Operate);
    ShowControl::Request go;
    go.command = ShowControl::Go;
    go.chaser = st->cueList(list)->chaser;
    QVERIFY(m_doc->showControl()->execute(go));
    frames(5);
    FocusCallCue *fcc = pc->focusCallCue();
    QCOMPARE(fcc->state(), FocusCallCue::Armed);
    value(17, 255);                             // Start
    value(17, 0);
    QCOMPARE(fcc->state(), FocusCallCue::Running);
    frames(2);
    Fixture *cur = m_doc->fixture(s->currentFixture());
    QVERIFY(cur != nullptr);
    Fixture *other = cur == z ? w : z;
    const QByteArray runStart = bytes(cur);
    value(0, 255);
    value(21, 255);
    joyTicks(pc, 30);
    frames(2);
    QVERIFY(bytes(cur) != runStart);
    drop();
    frames(2);
    const QByteArray runFrozen = bytes(cur), otherFrozen = bytes(other);
    const FocusSession::Offset runOffset = s->liveOffset(cur->id());
    joyTicks(pc, 100);
    frames(5);
    QCOMPARE(bytes(cur), runFrozen);
    QCOMPARE(bytes(other), otherFrozen);
    QCOMPARE(s->liveOffset(cur->id()), runOffset);
    // The run is held, not ended; the list stays on its cue.
    QCOMPARE(fcc->state(), FocusCallCue::Running);
    QCOMPARE(st->runningCue(list), focus);
    back();
    value(0, 128);
    value(21, 0);
    s->discardAll();
    s->stop();
    ShowControl::Request stop;
    stop.command = ShowControl::Stop;
    stop.chaser = go.chaser;
    m_doc->showControl()->execute(stop);
    frames(5);
    m_doc->setMode(Doc::Design);
    iom->setInputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine());
}

/* The calls are the show's: through the whole show's save and load (Doc's
   own dispatch, not FocusCalls::loadXML alone) they come back in order,
   with their groups, fixtures, targets and notes. */
void FocusCalls_Test::callsComeBackWithTheWholeShow()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 4.0f, 2.0f);
    FixtureGroup *dh = makeGroup(m_doc, "DH Tspots", { a, b });
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    StageTarget *solo = makeTarget(m_doc, "Soloist DS", QVector3D(3.0f, 7.0f, 0.0f));
    FocusCall c1;
    c1.groups << dh->id();
    c1.target = targetRef(dsc->id());
    c1.note = "Check the shutter cut";
    FocusCall c2;
    c2.name = "AT spots -> Soloist";
    c2.fixtures << b->id();
    c2.target = targetRef(solo->id());
    FocusCall c3;
    c3.name = "Last";
    c3.fixtures << a->id();
    c3.target = targetRef(dsc->id());
    m_doc->programmer()->focusCalls()->setCalls({ c2, c1, c3 });
    const QByteArray xml = saveDoc(m_doc);

    Doc *other = new Doc(this);
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(other->loadXML(r, false));
    FocusCalls *back = other->programmer()->focusCalls();
    QCOMPARE(back->count(), 3);
    QVERIFY(back->call(0) == c2);
    QVERIFY(back->call(1) == c1);
    QVERIFY(back->call(2) == c3);
    QCOMPARE(back->call(1).groups, QList<quint32>({ dh->id() }));
    QCOMPARE(back->call(1).note, QString("Check the shutter cut"));
    QCOMPARE(back->call(0).fixtures, QList<quint32>({ b->id() }));
    QCOMPARE(back->call(0).target.id, solo->id());
    QVERIFY(other->fixtureGroup(dh->id()) != nullptr);
    // A second save of the loaded show writes the same calls.
    const QByteArray again = saveDoc(other);
    const auto callsOf = [](const QByteArray &x) {
        const int from = x.indexOf("<FocusCalls>");
        return from < 0 ? QByteArray() : x.mid(from, x.indexOf("</FocusCalls>") - from);
    };
    QVERIFY(!callsOf(xml).isEmpty());
    QCOMPARE(callsOf(again), callsOf(xml));
    delete other;
}

void FocusCalls_Test::holdingAOnThePadSaysAllSaved()
{
    QDir dir("../iopluginstub");
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub in ../iopluginstub");
    IOPluginStub *stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_channelRoles = { { 0, "lx" }, { 1, "ly" }, { 2, "rx" }, { 3, "ry" },
                             { 10, "a" }, { 11, "b" }, { 14, "lb" }, { 15, "rb" },
                             { 16, "back" }, { 17, "start" }, { 18, "rs" } };
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, stub->name(), QString(), 0));
    ProgrammerController *pc = m_doc->programmer();
    pc->autoBindFromProfile();
    auto value = [iom](quint32 ch, uchar v) { emit iom->inputValueChanged(0, ch, v, QString()); };
    auto held = [pc](int ticks) {
        for (int i = 0; i < ticks; ++i)
            pc->applyDesignJoystick();
    };

    QList<Fixture *> spots;
    for (int i = 0; i < 6; ++i)
        spots << makeSpot(m_doc, QString("Spot %1").arg(i + 1), 1.0f + i, 2.0f);
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    FocusSession *s = pc->focusSession();
    s->setBlinkTimed(false);
    FocusSession::Options o;
    for (Fixture *f : spots)
        o.fixtures << f->id();
    o.order = FocusSession::AsGiven;
    o.first = targetRef(dsc->id());
    QVERIFY(s->start(o));
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);
    for (int i = 0; i < 3; ++i)
    {
        s->nudge(0.5f, 0.0f);
        s->next();
    }
    QCOMPARE(s->unsavedFixtures().size(), 3);
    // A held a full second on the pad: every light saved, and said so.
    value(10, 255);
    held(80);
    value(10, 0);
    held(2);
    QVERIFY(!s->hasUnsaved());
    QStringList lines;
    for (const QList<QVariant> &l : said)
        lines << l.at(0).toString();
    QVERIFY2(lines.contains("Saved all, 6 lights."), qPrintable(lines.join(" | ")));
    s->stop();
    iom->setInputPatch(0, QString(), QString(), QLCIOPlugin::invalidLine());
}

void FocusCalls_Test::theVoiceSaysTheLightsName()
{
    Fixture *a = makeSpot(m_doc, "Spot 3", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "DS Wash L", 4.0f, 2.0f);
    FixtureGroup *g = makeGroup(m_doc, "Spots", { a, b });
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    ProgrammerController *pc = m_doc->programmer();
    FocusSession *s = pc->focusSession();
    s->setBlinkTimed(false);
    VenueCorrections *v = pc->venueCorrections();
    v->setShowFile(m_dir->filePath("Spring.qlcc"), false);
    QVERIFY(v->setActiveVenue(v->addVenue("Lincoln HS")));
    FocusCall c1;
    c1.groups << g->id();
    c1.target = targetRef(dsc->id());
    pc->focusCalls()->setCalls({ c1 });
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);
    auto last = [&said]() { return said.last().at(0).toString(); };
    FocusSession::Options o;
    o.calls = pc->focusCalls()->sessionCalls();
    o.order = FocusSession::AsGiven;

    struct Level
    {
        ControllerScheme::VoiceDetail detail;
        QString call, step2, saved, savedAll, reverted, revertedAll, light;
    };
    const QString at = m_doc->monitorProperties()->stagePositionText(dsc->position(), MonitorProperties::SpokenOffset);
    const QList<Level> levels = {
        { ControllerScheme::TerseVoice, "Call 1. F1 Spot 3.", "F2 DS Wash L.", "Saved.", "Saved all.",
          "Reverted.", "Reverted all.", "DS Wash L." },
        { ControllerScheme::StandardVoice, "Call 1, Spots to DSC. Fixture 1, Spot 3.", "Fixture 2, DS Wash L.",
          "Saved fixture 2 to Lincoln HS.", "Saved all to Lincoln HS, 2 lights.", "Fixture 2 reverted.",
          "Reverted all, 2 lights.", "DS Wash L. Call 1, Spots to DSC. Fixture 2." },
        { ControllerScheme::VerboseVoice, QString("Call 1 of 1, Spots to DSC, %1. Fixture 1 of 2, Spot 3.").arg(at),
          "Call 1 of 1, Spots to DSC. Fixture 2 of 2, DS Wash L.", "Saved DS Wash L to Lincoln HS.",
          "Saved all to Lincoln HS, 2 lights.", "DS Wash L back to its last save.",
          "2 lights back to their last save.", QString("DS Wash L. Call 1 of 1, Spots to DSC, %1. Fixture 2 of 2.").arg(at) },
    };
    for (const Level &l : levels)
    {
        const QByteArray tag = ControllerScheme::voiceDetailKey(l.detail).toUtf8();
        pc->setVoiceDetail(l.detail);
        QVERIFY(s->start(o));
        QCOMPARE(last(), l.call);
        s->next();
        QCOMPARE(last(), l.step2);
        s->nudge(0.5f, 0.0f);
        s->save();
        QCOMPARE(last(), l.saved);
        s->nudge(0.5f, 0.0f);
        s->revertLight();
        QCOMPARE(last(), l.reverted);
        s->previous();
        s->nudge(0.5f, 0.0f);
        s->next();
        s->nudge(0.5f, 0.0f);
        s->revertAllHeld();
        QCOMPARE(last(), l.revertedAll);
        s->saveAllHeld();
        QCOMPARE(last(), l.savedAll);
        // Say the light: always the name, first.
        s->whereAmI();
        QVERIFY2(last() == l.light, qPrintable(tag + ": " + last()));
        // Warnings are never shortened.
        s->nudge(0.5f, 0.0f);
        s->apply(FocusSession::Exit);
        QCOMPARE(last(), QString("DS Wash L not saved."));
        s->discardAll();
        s->stop();
        // Never "of 2" but at Verbose; never the sidecar's name.
        for (const QList<QVariant> &line : said)
        {
            const QString text = line.at(0).toString();
            QVERIFY2(!text.contains("venues.xml"), qPrintable(text));
            if (l.detail != ControllerScheme::VerboseVoice)
                QVERIFY2(!text.contains(" of 2"), qPrintable(text));
        }
        said.clear();
    }
    pc->setVoiceDetail(ControllerScheme::StandardVoice);
}

void FocusCalls_Test::renamingAVenueKeepsEverything()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    MonitorProperties *props = m_doc->monitorProperties();
    Truss *truss = props->addTruss();
    truss->setName("LX1");
    truss->setOrigin(QVector3D(0.0f, 3.0f, 6.0f));
    VenueCorrections *v = m_doc->programmer()->venueCorrections();
    v->setShowFile(m_dir->filePath("Spring.qlcc"), false);
    // Named with the date, as an unnamed new venue is, and worked in.
    const QString dated = v->addVenue();
    QVERIFY(dated.startsWith("Venue "));
    QVERIFY(v->setActiveVenue(dated));
    FixtureRigProps rp = props->fixtureRigProps(a->id());
    rp.panOffsetDeg = 1.5f;
    props->setFixtureRigProps(a->id(), rp);
    QVERIFY(v->record(a->id()));
    QVector3D o2 = truss->origin();
    o2.setZ(5.5f);
    truss->setOrigin(o2);
    QVERIFY(v->recordTrim(truss->id()));
    dsc->setPosition(QVector3D(6.0f, 6.5f, 0.0f));
    QVERIFY(v->recordTarget(dsc->id()));
    const VenueCorrections::Venue before = v->venue(dated);
    // A practice scrambled from it, too.
    const QString practice = v->startPractice({ a->id() }, 7, 2.0f, 4.0f);
    QVERIFY(!practice.isEmpty());
    QCOMPARE(v->venue(practice).practice.from, dated);

    QVERIFY(v->renameVenue(dated, "Lincoln HS"));
    QVERIFY(!v->hasVenue(dated));
    QCOMPARE(v->venueNames().count("Lincoln HS"), 1);
    QCOMPARE(v->venueNames().size(), 2);                       // nothing duplicated
    const VenueCorrections::Venue after = v->venue("Lincoln HS");
    QCOMPARE(after.fixtures.size(), before.fixtures.size());
    QCOMPARE(after.fixtures.value(a->id()).panOffsetDeg, 1.5f);
    QCOMPARE(after.trims.size(), before.trims.size());
    QCOMPARE(after.targets.size(), before.targets.size());
    QCOMPARE(v->venue(practice).practice.from, QString("Lincoln HS"));
    // The practice ends back on the renamed venue, its corrections on.
    QVERIFY(v->endPractice());
    QCOMPARE(v->activeVenue(), QString("Lincoln HS"));
    QCOMPARE(props->fixtureRigProps(a->id()).panOffsetDeg, 1.5f);
    QCOMPARE(truss->origin().z(), 5.5f);
    // One sidecar, with the new name and not the old.
    QFile side(v->filePath());
    QVERIFY(side.open(QIODevice::ReadOnly));
    const QByteArray xml = side.readAll();
    QVERIFY(xml.contains("Lincoln HS"));
    QVERIFY(!xml.contains(dated.toUtf8() + "\""));
    QCOMPARE(QDir(m_dir->path()).entryList({ "*.venues.xml" }).size(), 1);
}
