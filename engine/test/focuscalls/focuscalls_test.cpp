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
    // The D-pad: up is in, down is out; right tightens the focus (its value
    // up), left loosens it -- in every tool.
    QCOMPARE(verbFor(GamepadRole::DpadUp), ZoomNarrower);
    QCOMPARE(verbFor(GamepadRole::DpadDown), ZoomWider);
    QCOMPARE(verbFor(GamepadRole::DpadRight), FocusFarther);
    QCOMPARE(verbFor(GamepadRole::DpadLeft), FocusNearer);
    QCOMPARE(label(RefineTool, FocusFarther), QString("Focus tighter"));
    QCOMPARE(label(FocusCallsTool, ZoomNarrower), QString("Zoom in (narrower beam)"));
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
    QCOMPARE(said.last().at(0).toString(), QString("Fixture 1 of 1."));

    // Lights only: no voice, but Say the light still speaks (asked for).
    pc->cycleOperatorFeedback();
    QCOMPARE(pc->operatorFeedback(), LightsFeedback);
    QCOMPARE(said.last().at(0).toString(), QString("Feedback: lights only."));
    const int count = said.count();
    s->next();
    s->zero();
    QCOMPARE(said.count(), count);
    s->whereAmI();
    QCOMPARE(said.last().at(0).toString(), QString("Spot 3. Fixture 1 of 1."));
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
    // Say the light: the light (here the whole call, by its groups) first.
    s->apply(FocusSession::WhereAmI);
    QCOMPARE(said.last().at(0).toString(), QString("AT spots. Call 2 of 2, AT spots to Soloist DS. All 1, group."));
    s->setScope(FocusSession::FixtureScope);
    s->whereAmI();
    QCOMPARE(said.last().at(0).toString(), QString("C. Call 2 of 2, AT spots to Soloist DS. Fixture 1 of 1."));
    s->save();
    QCOMPARE(said.last().at(0).toString(), QString("C kept."));
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
    quint32 running = focus;                 // the list is on the focus cue
    fcc->setRunningCueProbe([&running](quint32) { return running; });
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
    QCOMPARE(said.last().at(0).toString(), QString("Focus calls ended."));
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
    QCOMPARE(said.last().at(0).toString(), QString("Focus calls ready. Press Start to begin."));
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
    QCOMPARE(said.last().at(0).toString(), QString("Call 1 of 1, DH Tspots to DSC. Fixture 1 of 1."));
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
    QCOMPARE(said.last().at(0).toString(), QString("Spot 7 kept."));
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
    QCOMPARE(said.last().at(0).toString(), QString("Spot 3 back to its last save."));
    QVERIFY(!s->isDirty(a->id()));
    s->nudge(0.5f, 0.0f);
    s->next();
    s->nudge(0.5f, 0.0f);
    s->saveAllHeld();
    QCOMPARE(said.last().at(0).toString(), QString("2 lights kept."));
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
    auto held = [pc](int ticks) {
        for (int i = 0; i < ticks; ++i)
            pc->applyDesignJoystick();
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
    QCOMPARE(said.last().at(0).toString(), QString("1 light kept."));
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
    QCOMPARE(said.last().at(0).toString(), QString("Spot 3 back to its last save."));

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
    QCOMPARE(why, QString("Focus mode has 1 light not saved: save it (A) or put it back (B) first."));
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
    QCOMPARE(why, QString("Refine has 1 change not kept: keep it (A) or go back to the cue (B) first."));
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

QTEST_GUILESS_MAIN(FocusCalls_Test)
