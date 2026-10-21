/*
  Q Light Controller Plus - Unit test
  focussession_test.cpp

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

#include <QtTest>
#include <QSignalSpy>

#include "focussession_test.h"
#include "iopluginstub.h"
#include "qlcioplugin.h"
#include "qlcfile.h"
#include "programmercontroller.h"
#include "controllerkeepalive.h"
#include "monitorproperties.h"
#include "inputoutputmap.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "focussession.h"
#include "gamepadrole.h"
#include "qlcphysical.h"
#include "hidprofile.h"
#include "stagetarget.h"
#include "qlcchannel.h"
#include "aimsolver.h"
#include "universe.h"
#include "fixture.h"
#include "truss.h"
#include "doc.h"

namespace
{
/* A 540/270 spot: pan, tilt, dimmer. Placed free at (x, y) metres, hung at
   5 m. */
Fixture *makeSpot(Doc *doc, const QString &name, float x, float y)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Focus Spot " + name);
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

FocusSession::Options optionsFor(const QList<quint32> &fids, quint32 targetId)
{
    FocusSession::Options o;
    o.fixtures = fids;
    o.first.kind = FocusSession::Reference::Target;
    o.first.id = targetId;
    return o;
}

/* What the solver would send for @p fid at @p point, as a pan/tilt DMX pair. */
QPair<uchar, uchar> solvedDmx(Doc *doc, quint32 fid, const QVector3D &point)
{
    AimSolver::Solution sol;
    if (!AimSolver::reach(doc, fid, point, sol))
        return qMakePair(uchar(0), uchar(0));
    return qMakePair(uchar(qBound(0.0, sol.panDeg / 540.0 * 65535.0, 65535.0) / 256),
                     uchar(qBound(0.0, sol.tiltDeg / 270.0 * 65535.0, 65535.0) / 256));
}
}

void FocusSession_Test::init()
{
    m_doc = new Doc(this);
}

void FocusSession_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

void FocusSession_Test::stepsThroughTheStageInOrder()
{
    // Upstage row (y = 2) and a downstage row (y = 6, one head a little off).
    Fixture *usR = makeSpot(m_doc, "US right", 1.0f, 2.0f);
    Fixture *usL = makeSpot(m_doc, "US left", 7.0f, 2.0f);
    Fixture *dsL = makeSpot(m_doc, "DS left", 6.0f, 6.0f);
    Fixture *dsR = makeSpot(m_doc, "DS right", 2.0f, 6.4f);
    const QList<quint32> given = { usL->id(), dsL->id(), usR->id(), dsR->id() };

    // Downstage first; within a row house left (stage right, -X) first.
    const QList<quint32> want = { dsR->id(), dsL->id(), usR->id(), usL->id() };
    QCOMPARE(FocusSession::stageOrder(m_doc, given), want);

    StageTarget *mark = makeTarget(m_doc, "Centre", QVector3D(4.0f, 4.0f, 0.0f));
    FocusSession *s = m_doc->programmer()->focusSession();
    QVERIFY(s->start(optionsFor(given, mark->id())));
    QCOMPARE(s->order(), want);
    QCOMPARE(s->currentFixture(), dsR->id());
    s->stop();

    FocusSession::Options o = optionsFor(given, mark->id());
    o.order = FocusSession::AsGiven;
    QVERIFY(s->start(o));
    QCOMPARE(s->order(), given);
    s->stop();
}

void FocusSession_Test::nextAndPreviousWrap()
{
    Fixture *a = makeSpot(m_doc, "A", 1.0f, 3.0f);
    Fixture *b = makeSpot(m_doc, "B", 3.0f, 3.0f);
    Fixture *c = makeSpot(m_doc, "C", 5.0f, 3.0f);
    StageTarget *mark = makeTarget(m_doc, "Mark", QVector3D(3.0f, 5.0f, 0.0f));
    FocusSession *s = m_doc->programmer()->focusSession();
    QSignalSpy current(s, &FocusSession::currentChanged);
    QVERIFY(s->start(optionsFor({ a->id(), b->id(), c->id() }, mark->id())));
    QCOMPARE(s->currentFixture(), a->id());
    s->previous();
    QCOMPARE(s->currentFixture(), c->id());
    s->next();
    QCOMPARE(s->currentFixture(), a->id());
    s->next();
    QCOMPARE(s->currentFixture(), b->id());
    QCOMPARE(current.count(), 4);   // start + three steps
}

void FocusSession_Test::refusesWhatItCannotFocus()
{
    Fixture *a = makeSpot(m_doc, "A", 1.0f, 3.0f);
    StageTarget *mark = makeTarget(m_doc, "Mark", QVector3D(3.0f, 5.0f, 0.0f));
    FocusSession *s = m_doc->programmer()->focusSession();
    QString why;

    FocusSession::Options none;
    none.fixtures = { a->id() };
    QVERIFY(!s->start(none, &why));
    QVERIFY(!why.isEmpty());

    // A dimmer cannot be focused.
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Par");
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    QLCChannel *dim = new QLCChannel();
    dim->setName("Dimmer"); dim->setGroup(QLCChannel::Intensity);
    def->addChannel(dim); mode->insertChannel(dim, 0);
    def->addMode(mode);
    Fixture *par = new Fixture(m_doc);
    par->setFixtureDefinition(def, mode);
    par->setAddress(100);
    QVERIFY(m_doc->addFixture(par));
    m_doc->monitorProperties()->setFixturePosition(par->id(), 0, 0, QVector3D(2000, 2000, 5));
    why.clear();
    QVERIFY(!s->start(optionsFor({ par->id() }, mark->id()), &why));
    QVERIFY(!why.isEmpty());

    m_doc->setMode(Doc::Operate);
    why.clear();
    QVERIFY(!s->start(optionsFor({ a->id() }, mark->id()), &why));
    QVERIFY2(why.contains("Design"), qPrintable(why));
    QVERIFY(!s->isActive());
    m_doc->setMode(Doc::Design);
    QVERIFY(s->start(optionsFor({ a->id() }, mark->id())));
}

void FocusSession_Test::aNudgeMovesTheLightNotTheShow()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    StageTarget *mark = makeTarget(m_doc, "Mark", QVector3D(5.0f, 6.0f, 0.0f));
    FocusSession *s = m_doc->programmer()->focusSession();
    QVERIFY(s->start(optionsFor({ a->id() }, mark->id())));
    m_doc->resetModified();

    const QHash<QPair<int, int>, uchar> before = s->pendingWrites();
    const QPair<int, int> panAddr(0, int(a->address()));
    QVERIFY(before.contains(panAddr));

    QSignalSpy changed(s, &FocusSession::offsetChanged);
    s->nudge(4.0f, -2.5f);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(s->liveOffset(a->id()).pan, 4.0f);
    QCOMPARE(s->liveOffset(a->id()).tilt, -2.5f);
    QVERIFY(s->isDirty(a->id()));
    // In the rig, where the solver and the views read it...
    const FixtureRigProps rp = m_doc->monitorProperties()->fixtureRigProps(a->id());
    QCOMPARE(rp.panOffsetDeg, 4.0f);
    QCOMPARE(rp.tiltOffsetDeg, -2.5f);
    // ...so the DMX moved, and is what the solver says with the new offsets.
    const QHash<QPair<int, int>, uchar> after = s->pendingWrites();
    QVERIFY(after.value(panAddr) != before.value(panAddr));
    QVector3D point;
    QVERIFY(s->referencePoint(a->id(), point));
    const QPair<uchar, uchar> want = solvedDmx(m_doc, a->id(), point);
    QCOMPARE(after.value(panAddr), want.first);
    QCOMPARE(after.value(qMakePair(0, int(a->address()) + 1)), want.second);
    // And the show is not modified by a nudge.
    QVERIFY(!m_doc->isModified());
}

void FocusSession_Test::saveRevertZeroUndo()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    StageTarget *mark = makeTarget(m_doc, "Mark", QVector3D(5.0f, 6.0f, 0.0f));
    FixtureRigProps rp = m_doc->monitorProperties()->fixtureRigProps(a->id());
    rp.panOffsetDeg = 1.0f;
    m_doc->monitorProperties()->setFixtureRigProps(a->id(), rp);

    FocusSession *s = m_doc->programmer()->focusSession();
    QVERIFY(s->start(optionsFor({ a->id() }, mark->id())));
    QCOMPARE(s->savedOffset(a->id()).pan, 1.0f);
    m_doc->resetModified();

    s->nudge(2.0f, 1.0f);
    s->save();
    QVERIFY(m_doc->isModified());
    QVERIFY(!s->isDirty(a->id()));
    QVERIFY(s->isChecked(a->id()));
    QCOMPARE(s->checkedCount(), 1);
    QCOMPARE(s->savedOffset(a->id()).pan, 3.0f);
    QVERIFY(s->canUndo());

    s->nudge(0.5f, 0.0f);
    s->revert();
    QCOMPARE(s->liveOffset(a->id()).pan, 3.0f);
    QCOMPARE(m_doc->monitorProperties()->fixtureRigProps(a->id()).panOffsetDeg, 3.0f);

    s->zero();
    QCOMPARE(s->liveOffset(a->id()).pan, 0.0f);
    QCOMPARE(s->liveOffset(a->id()).tilt, 0.0f);
    QVERIFY(s->isDirty(a->id()));          // zero is unsaved until A
    QCOMPARE(s->savedOffset(a->id()).pan, 3.0f);

    // Undo the save: back to the offsets the show had before it.
    QVERIFY(s->undoSave());
    QCOMPARE(s->savedOffset(a->id()).pan, 1.0f);
    QCOMPARE(s->liveOffset(a->id()).pan, 1.0f);
    QCOMPARE(m_doc->monitorProperties()->fixtureRigProps(a->id()).panOffsetDeg, 1.0f);
    QVERIFY(!s->isChecked(a->id()));
    QVERIFY(!s->undoSave());
}

void FocusSession_Test::exitKeepsOrDiscards()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 4.0f, 2.0f);
    StageTarget *mark = makeTarget(m_doc, "Mark", QVector3D(5.0f, 6.0f, 0.0f));
    FocusSession *s = m_doc->programmer()->focusSession();
    MonitorProperties *props = m_doc->monitorProperties();

    QVERIFY(s->start(optionsFor({ a->id(), b->id() }, mark->id())));
    s->nudge(2.0f, 0.0f);
    s->next();
    s->nudge(0.0f, 3.0f);
    QVERIFY(s->hasUnsaved());
    QCOMPARE(s->unsavedFixtures().size(), 2);
    s->saveAll();
    QVERIFY(!s->hasUnsaved());
    QCOMPARE(s->checkedCount(), 2);
    s->stop();
    QCOMPARE(props->fixtureRigProps(a->id()).panOffsetDeg, 2.0f);
    QCOMPARE(props->fixtureRigProps(b->id()).tiltOffsetDeg, 3.0f);

    // Again, and leave without saving: the rig goes back.
    QVERIFY(s->start(optionsFor({ a->id(), b->id() }, mark->id())));
    QSignalSpy exitAsked(s, &FocusSession::exitRequested);
    s->nudge(5.0f, 5.0f);
    s->apply(FocusSession::Exit);
    QCOMPARE(exitAsked.count(), 1);
    QVERIFY(s->isActive());               // Exit only asks
    s->stop();
    QVERIFY(!s->isActive());
    QCOMPARE(props->fixtureRigProps(a->id()).panOffsetDeg, 2.0f);
    QCOMPARE(props->fixtureRigProps(a->id()).tiltOffsetDeg, 0.0f);
    QVERIFY(s->pendingWrites().isEmpty());
}

void FocusSession_Test::theFocusedHeadIsAimedAndLit()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 6.0f, 2.0f);
    StageTarget *mark = makeTarget(m_doc, "Mark", QVector3D(4.0f, 6.0f, 0.0f));
    FocusSession *s = m_doc->programmer()->focusSession();
    FocusSession::Options o = optionsFor({ a->id(), b->id() }, mark->id());
    o.level = 0.6f;
    QVERIFY(s->start(o));
    QCOMPARE(s->currentFixture(), a->id());

    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    QVERIFY(!ua.isEmpty());
    s->writeDMX(m_doc->masterTimer(), ua);
    QVector3D point;
    QVERIFY(s->referencePoint(a->id(), point));
    const QPair<uchar, uchar> want = solvedDmx(m_doc, a->id(), point);
    QCOMPARE(ua[0]->preGMValue(int(a->address())), want.first);
    QCOMPARE(ua[0]->preGMValue(int(a->address()) + 1), want.second);
    QCOMPARE(int(ua[0]->preGMValue(int(a->address()) + 2)), qRound(0.6 * 255));
    QCOMPARE(int(ua[0]->preGMValue(int(b->address()) + 2)), 0);   // the other: dark
    m_doc->inputOutputMap()->releaseUniverses(false);

    // Its pan and tilt are not written for the other head at all.
    QVERIFY(!s->pendingWrites().contains(qMakePair(0, int(b->address()))));

    s->next();
    QCOMPARE(int(s->pendingWrites().value(qMakePair(0, int(a->address()) + 2))), 0);
    QCOMPARE(int(s->pendingWrites().value(qMakePair(0, int(b->address()) + 2))), qRound(0.6 * 255));
}

void FocusSession_Test::secondReferenceToggles()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    StageTarget *one = makeTarget(m_doc, "Main Drape", QVector3D(4.0f, 1.0f, 0.0f));
    StageTarget *two = makeTarget(m_doc, "DSC", QVector3D(4.0f, 7.0f, 0.0f));
    FocusSession *s = m_doc->programmer()->focusSession();

    // Without a second reference, Y does nothing.
    QVERIFY(s->start(optionsFor({ a->id() }, one->id())));
    QVERIFY(!s->toggleReference());
    s->stop();

    FocusSession::Options o = optionsFor({ a->id() }, one->id());
    o.second.kind = FocusSession::Reference::Target;
    o.second.id = two->id();
    QVERIFY(s->start(o));
    QCOMPARE(s->referenceName(), QString("Main Drape"));
    const uchar firstTilt = s->pendingWrites().value(qMakePair(0, int(a->address()) + 1));
    QVERIFY(s->toggleReference());
    QVERIFY(s->usingSecondReference());
    QCOMPARE(s->referenceName(), QString("DSC"));
    QVERIFY(s->pendingWrites().value(qMakePair(0, int(a->address()) + 1)) != firstTilt);
    s->apply(FocusSession::ToggleReference);
    QVERIFY(!s->usingSecondReference());
}

void FocusSession_Test::aLineTargetSpreadsTheHeads()
{
    Fixture *a = makeSpot(m_doc, "A", 1.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 7.0f, 2.0f);
    StageTarget *line = makeTarget(m_doc, "Line", QVector3D(4.0f, 5.0f, 0.0f));
    line->setKind(StageTarget::Kind::Line);
    line->setLinePoints({ QVector3D(-3.0f, 0.0f, 0.0f), QVector3D(3.0f, 0.0f, 0.0f) });
    FocusSession *s = m_doc->programmer()->focusSession();
    QVERIFY(s->start(optionsFor({ a->id(), b->id() }, line->id())));
    QVector3D pa, pb;
    QVERIFY(s->referencePoint(a->id(), pa));
    QVERIFY(s->referencePoint(b->id(), pb));
    QVERIFY2((pa - pb).length() > 1.0f,
             qPrintable(QString("two heads on a 6 m line share a point: (%1,%2) (%3,%4)")
                        .arg(double(pa.x())).arg(double(pa.y())).arg(double(pb.x())).arg(double(pb.y()))));
    // Each takes the end on its own side.
    QVERIFY(pa.x() < pb.x());
}

void FocusSession_Test::theStickFeelsLikeTheFollowSpot()
{
    // followspot.js axisVelocity(): nothing in the deadzone, linear past it.
    QCOMPARE(FocusSession::stickVelocity(0.5f, 0.05f), 0.0f);
    QCOMPARE(FocusSession::stickVelocity(0.53f, 0.05f), 0.0f);
    QCOMPARE(FocusSession::stickVelocity(1.0f, 0.05f), 1.0f);
    QCOMPARE(FocusSession::stickVelocity(0.0f, 0.05f), -1.0f);
    QVERIFY(qAbs(FocusSession::stickVelocity(0.775f, 0.05f) - 0.5f) < 1e-4f);

    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    StageTarget *mark = makeTarget(m_doc, "Mark", QVector3D(5.0f, 6.0f, 0.0f));
    FocusSession *s = m_doc->programmer()->focusSession();
    QVERIFY(s->start(optionsFor({ a->id() }, mark->id())));
    s->setStickResponse(0.05f, 1.0f);

    // Left stick full right for a second: coarseRate degrees of pan.
    s->setStick(FocusSession::Coarse, 1.0f, 0.5f);
    for (int i = 0; i < 50; ++i)
        s->tick(0.02);
    QVERIFY2(qAbs(s->liveOffset(a->id()).pan - FocusSession::coarseRate()) < 0.05f,
             qPrintable(QString("a second of full stick gave %1 degrees")
                        .arg(double(s->liveOffset(a->id()).pan))));
    QCOMPARE(s->liveOffset(a->id()).tilt, 0.0f);

    // Released: holds.
    s->setStick(FocusSession::Coarse, 0.5f, 0.5f);
    const float held = s->liveOffset(a->id()).pan;
    s->tick(0.02);
    QCOMPARE(s->liveOffset(a->id()).pan, held);

    // Right stick pushed UP, half a second: fine, and up is more tilt.
    s->setStick(FocusSession::Fine, 0.5f, 0.0f);
    for (int i = 0; i < 25; ++i)
        s->tick(0.02);
    QVERIFY2(qAbs(s->liveOffset(a->id()).tilt - 0.5f * FocusSession::fineRate()) < 0.03f,
             qPrintable(QString("half a second of full fine stick up gave %1 degrees of tilt")
                        .arg(double(s->liveOffset(a->id()).tilt))));
    s->setStick(FocusSession::Fine, 0.5f, 0.5f);

    /* One path: the programmer's own joystick tick (the follow spot's)
       drives the session while it is on, instead of an aim target. */
    s->setStick(FocusSession::Coarse, 0.0f, 0.5f);
    const float before = s->liveOffset(a->id()).pan;
    m_doc->programmer()->applyDesignJoystick();
    QVERIFY(s->liveOffset(a->id()).pan < before);
}

void FocusSession_Test::buttonsByRole()
{
    QCOMPARE(FocusSession::controlFor(GamepadRole::RB), FocusSession::Next);
    QCOMPARE(FocusSession::controlFor(GamepadRole::LB), FocusSession::Previous);
    QCOMPARE(FocusSession::controlFor(GamepadRole::A), FocusSession::Save);
    QCOMPARE(FocusSession::controlFor(GamepadRole::B), FocusSession::Revert);
    QCOMPARE(FocusSession::controlFor(GamepadRole::X), FocusSession::Zero);
    QCOMPARE(FocusSession::controlFor(GamepadRole::Y), FocusSession::ToggleReference);
    QCOMPARE(FocusSession::controlFor(GamepadRole::Back), FocusSession::UndoSave);
    QCOMPARE(FocusSession::controlFor(GamepadRole::Start), FocusSession::Exit);
    QCOMPARE(FocusSession::controlFor(GamepadRole::DpadUp), FocusSession::StepTiltUp);
    QCOMPARE(FocusSession::controlFor(GamepadRole::DpadRight), FocusSession::StepPanUp);
    QCOMPARE(FocusSession::controlFor(GamepadRole::LeftX), FocusSession::NoControl);

    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 4.0f, 2.0f);
    StageTarget *mark = makeTarget(m_doc, "Mark", QVector3D(5.0f, 6.0f, 0.0f));
    FocusSession *s = m_doc->programmer()->focusSession();
    QVERIFY(s->start(optionsFor({ a->id(), b->id() }, mark->id())));
    s->apply(FocusSession::controlFor(GamepadRole::DpadRight));
    s->apply(FocusSession::controlFor(GamepadRole::DpadUp));
    s->apply(FocusSession::controlFor(GamepadRole::DpadUp));
    QCOMPARE(s->liveOffset(a->id()).pan, 0.1f);
    QCOMPARE(s->liveOffset(a->id()).tilt, 0.2f);
    s->apply(FocusSession::controlFor(GamepadRole::A));
    QVERIFY(s->isChecked(a->id()));
    s->apply(FocusSession::controlFor(GamepadRole::RB));
    QCOMPARE(s->currentFixture(), b->id());
    s->apply(FocusSession::controlFor(GamepadRole::Back));     // undo the save on A
    QVERIFY(!s->isChecked(a->id()));
    QCOMPARE(s->liveOffset(a->id()).pan, 0.0f);
}

void FocusSession_Test::gamepadRolesParse()
{
    QCOMPARE(GamepadRole::fromProfile("lb", "whatever"), GamepadRole::LB);
    QCOMPARE(GamepadRole::fromProfile("DPAD_UP", QString()), GamepadRole::DpadUp);
    QCOMPARE(GamepadRole::fromProfile("rx", QString()), GamepadRole::RightX);
    // An explicit role the tool does not know is not guessed from the name.
    QCOMPARE(GamepadRole::fromProfile("paddle1", "A"), GamepadRole::None);
    // No role: the usual names.
    QCOMPARE(GamepadRole::fromProfile(QString(), "A"), GamepadRole::A);
    QCOMPARE(GamepadRole::fromProfile(QString(), "Left Bumper"), GamepadRole::LB);
    QCOMPARE(GamepadRole::fromProfile(QString(), "RB"), GamepadRole::RB);
    QCOMPARE(GamepadRole::fromProfile(QString(), "View"), GamepadRole::Back);
    QCOMPARE(GamepadRole::fromProfile(QString(), "Menu"), GamepadRole::Start);
    QCOMPARE(GamepadRole::fromProfile(QString(), "D-pad Left"), GamepadRole::DpadLeft);
    QCOMPARE(GamepadRole::fromProfile(QString(), "Right Stick Y"), GamepadRole::RightY);
    QCOMPARE(GamepadRole::fromProfile(QString(), "Slider 3"), GamepadRole::None);
    QVERIFY(GamepadRole::isAxis(GamepadRole::RightX));
    QVERIFY(!GamepadRole::isAxis(GamepadRole::A));
    QCOMPARE(GamepadRole::key(GamepadRole::DpadRight), QString("dpad_right"));
}

void FocusSession_Test::theXboxProfileMapsFocusMode()
{
    const QString path = QString(QLC_TEST_RESOURCE_ROOT) + "hidprofiles/xbox.qxhid";
    HIDProfile *p = HIDProfile::load(path);
    QVERIFY2(p != nullptr, qPrintable(path));

    QHash<GamepadRole::Role, int> buttons;
    for (int i = 0; i < 32; ++i)
        if (!p->buttonName(i).isEmpty())
            buttons.insert(GamepadRole::fromProfile(p->buttonRole(i), p->buttonName(i)), i);
    QHash<GamepadRole::Role, int> axes;
    for (int i = 0; i < 16; ++i)
        if (!p->axisName(i).isEmpty())
            axes.insert(GamepadRole::fromProfile(p->axisRole(i), p->axisName(i)), i);

    const GamepadRole::Role needed[] = { GamepadRole::A, GamepadRole::B, GamepadRole::X,
        GamepadRole::Y, GamepadRole::LB, GamepadRole::RB, GamepadRole::Back, GamepadRole::Start,
        GamepadRole::DpadUp, GamepadRole::DpadDown, GamepadRole::DpadLeft, GamepadRole::DpadRight };
    for (GamepadRole::Role r : needed)
        QVERIFY2(buttons.contains(r), qPrintable("no button has role " + GamepadRole::key(r)));
    QCOMPARE(buttons.value(GamepadRole::LB), 4);
    QCOMPARE(buttons.value(GamepadRole::RB), 5);
    QCOMPARE(buttons.value(GamepadRole::DpadUp), 11);
    QVERIFY(axes.contains(GamepadRole::RightX) && axes.contains(GamepadRole::RightY));
    // The left stick is the follow spot's: the pan/tilt slots are on it.
    QCOMPARE(p->axisForSlot("pan"), axes.value(GamepadRole::LeftX));
    QCOMPARE(p->axisForSlot("tilt"), axes.value(GamepadRole::LeftY));

    // A role written by the editor survives a save and reload.
    const QString copy = QDir::temp().filePath("focussession_test_xbox.qxhid");
    QVERIFY(p->saveTo(copy));
    HIDProfile *q = HIDProfile::load(copy);
    QVERIFY(q != nullptr);
    QCOMPARE(q->buttonRole(4), QString("lb"));
    QCOMPARE(q->axisRole(3), QString("rx"));
    QFile::remove(copy);
    delete q;
    delete p;
}

void FocusSession_Test::theControllerReachesItThroughTheProgrammer()
{
    QDir dir("../iopluginstub");
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY(m_doc->ioPluginCache()->plugins().size() > 0);
    IOPluginStub *stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    /* A gamepad as its profile describes it: axes 0..3, then buttons. No
       pan/tilt slots here, so the left stick is found by its role. */
    stub->m_channelRoles = { { 0, "lx" }, { 1, "ly" }, { 2, "rx" }, { 3, "ry" },
                             { 10, "a" }, { 14, "lb" }, { 15, "rb" },
                             { 16, "back" }, { 17, "start" }, { 21, "dpad_right" } };
    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, stub->name(), QString(), 0));
    ProgrammerController *pc = m_doc->programmer();
    pc->autoBindFromProfile();       // connects the controller, as on load

    auto press = [iom](quint32 ch) {
        emit iom->inputValueChanged(0, ch, 255, QString());
        emit iom->inputValueChanged(0, ch, 0, QString());
    };

    // View + Menu, held together, asks for Focus mode.
    QSignalSpy asked(pc, &ProgrammerController::focusModeRequested);
    emit iom->inputValueChanged(0, 16, 255, QString());
    emit iom->inputValueChanged(0, 17, 255, QString());
    emit iom->inputValueChanged(0, 17, 0, QString());
    emit iom->inputValueChanged(0, 16, 0, QString());
    QCOMPARE(asked.count(), 1);

    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 4.0f, 2.0f);
    StageTarget *mark = makeTarget(m_doc, "Mark", QVector3D(5.0f, 6.0f, 0.0f));
    FocusSession *s = pc->focusSession();
    QVERIFY(s->start(optionsFor({ a->id(), b->id() }, mark->id())));

    QSignalSpy actions(pc, &ProgrammerController::buttonActionTriggered);
    QSignalSpy joystick(pc, &ProgrammerController::joystickUpdated);

    press(21);                                 // D-pad right: +step pan
    QCOMPARE(s->liveOffset(a->id()).pan, 0.1f);
    press(10);                                 // A: save
    QVERIFY(s->isChecked(a->id()));
    press(15);                                 // RB: next
    QCOMPARE(s->currentFixture(), b->id());

    // The left stick, by role, through the programmer's joystick tick.
    emit iom->inputValueChanged(0, 0, 255, QString());
    pc->applyDesignJoystick();
    QVERIFY(s->liveOffset(b->id()).pan > 0.0f);
    emit iom->inputValueChanged(0, 0, 128, QString());
    // The right stick, finely.
    emit iom->inputValueChanged(0, 3, 0, QString());
    const float tiltBefore = s->liveOffset(b->id()).tilt;
    for (int i = 0; i < 10; ++i)
        pc->applyDesignJoystick();
    QVERIFY(s->liveOffset(b->id()).tilt > tiltBefore);
    QVERIFY(s->liveOffset(b->id()).tilt - tiltBefore < 0.25f);

    // Nothing else heard any of it.
    QCOMPARE(actions.count(), 0);
    QCOMPARE(joystick.count(), 0);

    // Menu (Start) asks to leave.
    QSignalSpy exitAsked(s, &FocusSession::exitRequested);
    press(17);
    QCOMPARE(exitAsked.count(), 1);

    // What the panel draws: the controller found by its roles, and its input.
    const ProgrammerController::FocusController pad = pc->focusController();
    QVERIFY(pad.present);
    QCOMPARE(pad.universe, 0u);
    QVERIFY(pad.controls.contains(GamepadRole::RB) && pad.controls.contains(GamepadRole::RightY));
    QVERIFY(!pad.controls.contains(GamepadRole::Y));
    QSignalSpy shown(pc, &ProgrammerController::focusControllerInput);
    press(15);
    QCOMPARE(shown.count(), 2);
    QCOMPARE(shown.at(0).at(0).toInt(), int(GamepadRole::RB));
    QCOMPARE(shown.at(0).at(1).toFloat(), 1.0f);

    // On the keyboard the controller does nothing at all -- and still
    // reaches nothing else.
    s->setInputDevice(FocusSession::KeyboardInput);
    const quint32 at = s->currentFixture();
    const FocusSession::Offset before = s->liveOffset(at);
    shown.clear();
    press(21);
    press(15);
    press(17);
    emit iom->inputValueChanged(0, 0, 255, QString());
    for (int i = 0; i < 10; ++i)
        pc->applyDesignJoystick();
    QCOMPARE(s->currentFixture(), at);
    QVERIFY(s->liveOffset(at) == before);
    QCOMPARE(exitAsked.count(), 1);
    QCOMPARE(shown.count(), 0);
    QCOMPARE(actions.count(), 0);
    emit iom->inputValueChanged(0, 0, 128, QString());
    s->setInputDevice(FocusSession::ControllerInput);
    s->stop();
}

void FocusSession_Test::performEndsIt()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    StageTarget *mark = makeTarget(m_doc, "Mark", QVector3D(5.0f, 6.0f, 0.0f));
    FocusSession *s = m_doc->programmer()->focusSession();
    QVERIFY(s->start(optionsFor({ a->id() }, mark->id())));
    s->nudge(1.0f, 0.0f);
    s->save();
    s->nudge(1.0f, 0.0f);
    m_doc->setMode(Doc::Operate);
    QVERIFY(!s->isActive());
    QCOMPARE(m_doc->monitorProperties()->fixtureRigProps(a->id()).panOffsetDeg, 1.0f);
    m_doc->setMode(Doc::Design);
}

namespace
{
class CountingBackend final : public ControllerKeepAlive::Backend
{
public:
    CountingBackend(int *begun, int *ended) : m_begun(begun), m_ended(ended) {}
    void beginActivity(const QString &) override { ++*m_begun; }
    void endActivity() override { ++*m_ended; }
    ControllerKeepAlive::Permission inputMonitoring() const override
    { return ControllerKeepAlive::PermissionGranted; }

private:
    int *m_begun;
    int *m_ended;
};
}

void FocusSession_Test::theSessionKeepsTheControllerAwake()
{
    int begun = 0, ended = 0;
    ControllerKeepAlive *keep = m_doc->programmer()->controllerKeepAlive();
    QVERIFY(keep != nullptr);
    keep->setBackend(new CountingBackend(&begun, &ended));

    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    StageTarget *t = makeTarget(m_doc, "Centre", QVector3D(3.0f, 4.0f, 0.0f));
    FocusSession *s = m_doc->programmer()->focusSession();

    QVERIFY(!keep->isHeld());
    QVERIFY(s->start(optionsFor({ a->id() }, t->id())));
    QVERIFY(keep->isHeld());
    QCOMPARE(begun, 1);
    // No controller patched: paused, and said why.
    QVERIFY(keep->isPaused());
    QVERIFY2(keep->pauseReason().contains("no controller"), qPrintable(keep->pauseReason()));
    s->stop();
    QVERIFY(!keep->isHeld());
    QCOMPARE(ended, 1);
    QVERIFY(!keep->isPaused());

    // A controller the plugin cannot read: the plugin's reason is the one said.
    QDir dir("../iopluginstub");
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    IOPluginStub *stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    stub->m_channelRoles = { { 0, "lx" }, { 1, "ly" }, { 10, "a" }, { 14, "lb" }, { 15, "rb" } };
    stub->m_inputProblems = { { 0, "macOS is not letting qlcconsole read the controller." } };
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, stub->name(), QString(), 0));
    QVERIFY(s->start(optionsFor({ a->id() }, t->id())));
    QVERIFY(keep->isHeld());
    QCOMPARE(keep->pauseReason(), QString("macOS is not letting qlcconsole read the controller."));
    // Readable again: the next look (the programmer asks every 2 s) clears it.
    stub->m_inputProblems.clear();
    emit m_doc->programmer()->focusControllerChanged();
    QVERIFY(!keep->isPaused());
    s->stop();
    QCOMPARE(begun, 2);
    QCOMPARE(ended, 2);
    stub->m_channelRoles.clear();
}

QTEST_MAIN(FocusSession_Test)
