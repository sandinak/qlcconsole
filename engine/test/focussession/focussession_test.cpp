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
#include <QtMath>

#include <QtCore>
#define protected public                // Universe::processFaders()
#include "universe.h"
#undef protected
#define private public                  // MasterTimer::timerTick(), the joystick tick's clock
#include "mastertimer.h"
#include "programmercontroller.h"
#undef private
#include "focussession_test.h"
#include "qlccapability.h"
#include "genericfader.h"
#include "outputlayer.h"
#include "fadechannel.h"
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
#include "righeads.h"
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
    // Steady light to read: the selection's flash is focuscalls_test's.
    m_doc->programmer()->focusSession()->setFlashEnabled(false);
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

    // A par is a fixed light now (Q102): focused by where it lands.
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
    QVERIFY(s->start(optionsFor({ par->id() }, mark->id())));
    QVERIFY(s->isFixedLight(par->id()));
    s->stop();

    // Something with no light and no pan or tilt cannot.
    QLCFixtureDef *fdef = new QLCFixtureDef();
    fdef->setManufacturer("Test"); fdef->setModel("Fog");
    QLCFixtureMode *fmode = new QLCFixtureMode(fdef);
    QLCChannel *fog = new QLCChannel();
    fog->setName("Fog"); fog->setGroup(QLCChannel::Effect);
    fdef->addChannel(fog); fmode->insertChannel(fog, 0);
    fdef->addMode(fmode);
    Fixture *fogger = new Fixture(m_doc);
    fogger->setFixtureDefinition(fdef, fmode);
    fogger->setAddress(110);
    QVERIFY(m_doc->addFixture(fogger));
    m_doc->monitorProperties()->setFixturePosition(fogger->id(), 0, 0, QVector3D(3000, 2000, 5));
    why.clear();
    QVERIFY(!s->start(optionsFor({ fogger->id() }, mark->id()), &why));
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
    // On the rig too, where the solver aims from: the light moves on "Zeroed."
    QCOMPARE(m_doc->monitorProperties()->fixtureRigProps(a->id()).panOffsetDeg, 0.0f);
    QCOMPARE(m_doc->monitorProperties()->fixtureRigProps(a->id()).tiltOffsetDeg, 0.0f);
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

void FocusSession_Test::theUnsavedHeadsAreTold()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 4.0f, 2.0f);
    StageTarget *mark = makeTarget(m_doc, "Mark", QVector3D(5.0f, 6.0f, 0.0f));
    FocusSession *s = m_doc->programmer()->focusSession();
    QVERIFY(s->start(optionsFor({ a->id(), b->id() }, mark->id())));
    QSignalSpy told(s, &FocusSession::unsavedChanged);

    s->nudge(1.0f, 0.0f);
    QCOMPARE(told.count(), 1);
    QCOMPARE(s->unsavedFixtures(), QList<quint32>({ a->id() }));
    s->nudge(1.0f, 0.0f);                       // still just A
    QCOMPARE(told.count(), 1);
    s->next();
    s->nudge(0.0f, 1.0f);
    QCOMPARE(told.count(), 2);
    QCOMPARE(s->unsavedFixtures(), QList<quint32>({ a->id(), b->id() }));
    s->save();                                   // B kept
    QCOMPARE(told.count(), 3);
    QCOMPARE(s->unsavedFixtures(), QList<quint32>({ a->id() }));
    s->zero();                                   // B to zero: unsaved again
    QCOMPARE(told.count(), 4);
    s->revert();
    QCOMPARE(told.count(), 5);
    s->previous();
    s->revert();                                 // A back
    QCOMPARE(told.count(), 6);
    QVERIFY(!s->hasUnsaved());
    s->nudge(0.5f, 0.0f);
    QCOMPARE(told.count(), 7);
    s->saveAll();
    QCOMPARE(told.count(), 8);
    QVERIFY(s->undoSave());                      // as it was before: still none unsaved
    s->nudge(0.5f, 0.0f);
    const int before = told.count();
    s->discardAll();
    QCOMPARE(told.count(), before + 1);
    s->nudge(0.5f, 0.0f);
    s->stop();                                   // discards
    QVERIFY(!s->hasUnsaved());
    QCOMPARE(told.count(), before + 3);
}

void FocusSession_Test::aSaveBlinksTheLightTwice()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 4.0f, 2.0f);
    StageTarget *mark = makeTarget(m_doc, "Mark", QVector3D(5.0f, 6.0f, 0.0f));
    FocusSession *s = m_doc->programmer()->focusSession();
    s->setBlinkTimed(false);                     // stepped here, not by time
    FocusSession::Options o = optionsFor({ a->id(), b->id() }, mark->id());
    o.level = 0.6f;
    QVERIFY(s->start(o));
    const QPair<int, int> dimmer = qMakePair(0, int(a->address()) + 2);
    const int lit = qRound(0.6 * 255);
    QCOMPARE(int(s->pendingWrites().value(dimmer)), lit);
    QSignalSpy saved(s, &FocusSession::headSaved);

    // A revert does not blink.
    s->nudge(1.0f, 0.0f);
    s->revert();
    QVERIFY(!s->isBlinking());
    QCOMPARE(int(s->pendingWrites().value(dimmer)), lit);
    QCOMPARE(saved.count(), 0);

    // A save: dark, lit, dark, lit -- then done.
    s->nudge(1.0f, 0.0f);
    s->save();
    QCOMPARE(saved.count(), 1);
    QCOMPARE(saved.at(0).at(0).toUInt(), a->id());
    QVERIFY(s->isBlinking());
    QCOMPARE(s->blinkFixture(), a->id());
    QList<int> seen;
    seen << s->pendingWrites().value(dimmer);
    for (int i = 1; i < FocusSession::kBlinkPhases; ++i)
    {
        s->advanceBlink();
        seen << s->pendingWrites().value(dimmer);
    }
    QCOMPARE(seen, QList<int>({ 0, lit, 0, lit }));
    // Its pan and tilt are still sent through the dark: it is not moved.
    QVERIFY(s->pendingWrites().contains(qMakePair(0, int(a->address()))));
    s->advanceBlink();
    QVERIFY(!s->isBlinking());
    QCOMPARE(int(s->pendingWrites().value(dimmer)), lit);
    s->advanceBlink();                           // nothing to step
    QCOMPARE(int(s->pendingWrites().value(dimmer)), lit);

    // Moved on mid-blink: the saved light still finishes, the next is lit.
    s->save();
    s->next();
    QCOMPARE(int(s->pendingWrites().value(dimmer)), 0);
    QCOMPARE(int(s->pendingWrites().value(qMakePair(0, int(b->address()) + 2))), lit);
    s->advanceBlink();
    s->advanceBlink();
    QCOMPARE(int(s->pendingWrites().value(dimmer)), 0);     // the others: dark anyway
    s->advanceBlink();
    s->advanceBlink();
    QVERIFY(!s->isBlinking());

    // Off: a save keeps the head, and the rig does not blink.
    s->previous();
    s->setRigFeedback(false);
    s->nudge(1.0f, 0.0f);
    s->save();
    QCOMPARE(saved.count(), 3);
    QVERIFY(!s->isBlinking());
    QCOMPARE(int(s->pendingWrites().value(dimmer)), lit);
    s->setRigFeedback(true);

    // On its own timer, the blink ends by itself.
    s->setBlinkTimed(true);
    s->save();
    QVERIFY(s->isBlinking());
    QTRY_VERIFY_WITH_TIMEOUT(!s->isBlinking(), 3000);
    QCOMPARE(int(s->pendingWrites().value(dimmer)), lit);
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
    ua[0]->processFaders();                     // a frame
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

/* Focus mode takes over the heads it uses, over whatever runs (B1): its
   light survives the universe's frame -- which zeroes intensity and lays
   the running faders on -- and beats a cue that is brighter, on the head
   being focused and on the others it darkens. */
void FocusSession_Test::itsLightHoldsOverARunningCue()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 6.0f, 2.0f);
    StageTarget *mark = makeTarget(m_doc, "Mark", QVector3D(4.0f, 6.0f, 0.0f));
    Universe *uni = m_doc->inputOutputMap()->universes().at(0);

    // A running cue: both dimmers at 200.
    QSharedPointer<GenericFader> cue = uni->requestFader();
    for (Fixture *f : { a, b })
    {
        GenericFader::ChannelRef fc = cue->getChannelFader(m_doc, uni, f->id(), 2);
        fc->setStart(200);
        fc->setCurrent(200);
        fc->setTarget(200);
    }
    uni->processFaders();
    QCOMPARE(int(uni->preGMValue(int(a->address()) + 2)), 200);

    FocusSession *s = m_doc->programmer()->focusSession();
    FocusSession::Options o = optionsFor({ a->id(), b->id() }, mark->id());
    o.level = 0.6f;
    QVERIFY(s->start(o));
    for (int i = 0; i < 3; ++i)
    {
        m_doc->masterTimer()->timerTick();
        uni->processFaders();
    }
    QCOMPARE(int(uni->preGMValue(int(a->address()) + 2)), qRound(0.6 * 255));
    QCOMPARE(int(uni->preGMValue(int(b->address()) + 2)), 0);      // dark, not the cue's 200

    // Stopped, the cue has them back.
    s->stop();
    m_doc->masterTimer()->timerTick();
    uni->processFaders();
    QCOMPARE(int(uni->preGMValue(int(a->address()) + 2)), 200);
    QCOMPARE(int(uni->preGMValue(int(b->address()) + 2)), 200);
    uni->dismissFader(cue);
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
    // Y is the scope in every on-stage tool (ControllerScheme); the second
    // reference is a left-stick click.
    QCOMPARE(FocusSession::controlFor(GamepadRole::Y), FocusSession::CycleScope);
    QCOMPARE(FocusSession::controlFor(GamepadRole::LS), FocusSession::ToggleReference);
    QCOMPARE(FocusSession::controlFor(GamepadRole::Back), FocusSession::UndoSave);
    QCOMPARE(FocusSession::controlFor(GamepadRole::Start), FocusSession::Exit);
    // The D-pad is zoom (up, down) and focus (left, right): Q103.
    QCOMPARE(FocusSession::controlFor(GamepadRole::DpadUp), FocusSession::ZoomNarrower);
    QCOMPARE(FocusSession::controlFor(GamepadRole::DpadDown), FocusSession::ZoomWider);
    QCOMPARE(FocusSession::controlFor(GamepadRole::DpadRight), FocusSession::FocusFarther);
    QCOMPARE(FocusSession::controlFor(GamepadRole::LeftX), FocusSession::NoControl);

    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 4.0f, 2.0f);
    StageTarget *mark = makeTarget(m_doc, "Mark", QVector3D(5.0f, 6.0f, 0.0f));
    FocusSession *s = m_doc->programmer()->focusSession();
    QVERIFY(s->start(optionsFor({ a->id(), b->id() }, mark->id())));
    // The step controls still nudge (the keys use them); the D-pad doesn't.
    s->apply(FocusSession::StepPanUp);
    s->apply(FocusSession::StepTiltUp);
    s->apply(FocusSession::StepTiltUp);
    s->apply(FocusSession::controlFor(GamepadRole::DpadUp));   // zoom: this spot has none
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
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub in ../iopluginstub");
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

    s->nudge(0.1f, 0.0f);                      // (the D-pad is zoom and focus now)
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
    /* Each tick its nominal 20 ms (the tick counts a late one as up to
       0.1 s held), so the "finely" bound below holds on a loaded machine. */
    for (int i = 0; i < 10; ++i)
    {
        pc->m_joyTickClock.invalidate();
        pc->applyDesignJoystick();
    }
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
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub in ../iopluginstub");
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

void FocusSession_Test::aFixedLightIsFocusedByWhereItLands()
{
    // A dimmer par hung at 5 m, 4 m across, on the upstage line (y = 0).
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Focus Par");
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    QLCChannel *dim = new QLCChannel();
    dim->setName("Dimmer"); dim->setGroup(QLCChannel::Intensity);
    def->addChannel(dim); mode->insertChannel(dim, 0);
    def->addMode(mode);
    Fixture *par = new Fixture(m_doc);
    par->setName("FOH 1");
    par->setFixtureDefinition(def, mode);
    par->setAddress(20);
    QVERIFY(m_doc->addFixture(par));
    MonitorProperties *props = m_doc->monitorProperties();
    props->setFixturePosition(par->id(), 0, 0, QVector3D(4000, 0, 5));
    QCOMPARE(FocusSession::fixedLights(m_doc, { par->id() }), QList<quint32>({ par->id() }));
    QVERIFY(FocusSession::movingHeads(m_doc, { par->id() }).isEmpty());

    // No reference needed: it starts where it lands now -- straight down.
    FocusSession::Options o;
    o.fixtures = { par->id() };
    FocusSession *s = m_doc->programmer()->focusSession();
    QString why;
    QVERIFY2(s->start(o, &why), qPrintable(why));
    QVERIFY(!s->toggleReference());                 // a fixed light has no second reference
    QVector3D at;
    QVERIFY(s->referencePoint(par->id(), at));
    QVERIFY(qAbs(at.x() - 4.0f) < 1e-3f && qAbs(at.y()) < 1e-3f);
    QVERIFY(qAbs(at.z() - FocusSession::subjectHeight()) < 1e-3f);
    // Lit at its level, and nothing else.
    const QHash<QPair<int, int>, uchar> w = s->pendingWrites();
    QCOMPARE(w.value(qMakePair(0, 20)), uchar(qRound(0.8 * 255)));

    // Down on the stick or key: downstage. Four steps of 1° = 2 m.
    for (int i = 0; i < 4; ++i)
        s->nudge(0.0f, -1.0f);
    QVERIFY(s->referencePoint(par->id(), at));
    QVERIFY(qAbs(at.y() - 2.0f) < 1e-3f);
    FixtureRigProps rp = props->fixtureRigProps(par->id());
    QVERIFY(rp.fixedAim);
    QVERIFY(qAbs(rp.panZeroDir) < 0.05f);                       // toward the house
    const double down = qRadiansToDegrees(qAtan2(5.0 - FocusSession::subjectHeight(), 2.0));
    QVERIFY2(qAbs(rp.aimDown - down) < 0.1, qPrintable(QString::number(rp.aimDown)));
    QVERIFY(s->isDirty(par->id()));
    // Since the save, in the show's units: a show in feet hears feet and
    // inches, not metres.
    QCOMPARE(s->deltaText(par->id()), QString("2.00 m downstage"));
    props->setGridUnits(MonitorProperties::Feet);
    QCOMPARE(s->deltaText(par->id()), QString("6' 7\" downstage"));
    s->nudge(0.05f, 0.0f);                                      // 2.5 cm across
    QCOMPARE(s->deltaText(par->id()), QString("1\" stage left, 6' 7\" downstage"));
    s->nudge(-0.03f, 0.0f);                                     // 1 cm left of where it was
    QCOMPARE(s->deltaText(par->id()), QString::fromUtf8("\u00BD\" stage left, 6' 7\" downstage"));
    s->nudge(-0.02f, 0.0f);
    props->setGridUnits(MonitorProperties::Meters);
    QCOMPARE(s->deltaText(par->id()), QString("2.00 m downstage"));

    // Revert: the rig as it was before the session, not aimed.
    s->revert();
    QVERIFY(!props->fixtureRigProps(par->id()).fixedAim);
    QVERIFY(!s->isDirty(par->id()));

    // Right goes toward stage left (+X); save keeps it, past the session.
    s->nudge(2.0f, 0.0f);
    s->save();
    QVERIFY(s->isChecked(par->id()));
    QVERIFY(!s->isDirty(par->id()));
    s->stop();
    rp = props->fixtureRigProps(par->id());
    QVERIFY(rp.fixedAim);
    QVERIFY2(qAbs(rp.panZeroDir - 270.0f) < 0.05f, qPrintable(QString::number(rp.panZeroDir)));   // stage left
}

void FocusSession_Test::aFixedLightFacedBackLandsUpstage()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Sky Laser");
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    QLCChannel *dim = new QLCChannel();
    dim->setName("Dimmer"); dim->setGroup(QLCChannel::Intensity);
    def->addChannel(dim); mode->insertChannel(dim, 0);
    def->addMode(mode);
    Fixture *laser = new Fixture(m_doc);
    laser->setName("SR-LASER");
    laser->setFixtureDefinition(def, mode);
    laser->setAddress(40);
    QVERIFY(m_doc->addFixture(laser));
    MonitorProperties *props = m_doc->monitorProperties();
    props->setFixturePosition(laser->id(), 0, 0, QVector3D(4000, 0, 5));
    FixtureRigProps rp = props->fixtureRigProps(laser->id());
    props->setFixtureFace(rp, FixtureRigProps::FaceBack);
    props->setFixtureRigProps(laser->id(), rp);

    // The same throw the views draw: upstage, level.
    QVector3D dir;
    QVERIFY(RigHeads::fixedThrow(m_doc, laser->id(), dir));
    QVERIFY2(dir.y() < -0.99f, qPrintable(QString("%1 %2 %3").arg(dir.x()).arg(dir.y()).arg(dir.z())));

    FocusSession::Options o;
    o.fixtures = { laser->id() };
    FocusSession *s = m_doc->programmer()->focusSession();
    QString why;
    QVERIFY2(s->start(o, &why), qPrintable(why));
    QVector3D at;
    QVERIFY(s->referencePoint(laser->id(), at));
    // Level, so it never comes down to head height: 2 m out the way it
    // throws -- upstage (-Y) -- not under the truss or downstage.
    QVERIFY2(at.y() < -1.9f && qAbs(at.x() - 4.0f) < 1e-3f,
             qPrintable(QString("%1, %2").arg(at.x()).arg(at.y())));
    s->stop();

    // Tipped down in its yoke (aimed from the face): it lands upstage, at head height.
    rp = props->fixtureRigProps(laser->id());
    RigHeads::aimAtFace(rp);
    rp.aimDown = 45.0f;
    props->setFixtureRigProps(laser->id(), rp);
    QVERIFY2(s->start(o, &why), qPrintable(why));
    QVERIFY(s->referencePoint(laser->id(), at));
    const float drop = 5.0f - float(FocusSession::subjectHeight());
    QVERIFY2(qAbs(at.y() + drop) < 0.01f, qPrintable(QString::number(at.y())));
    s->stop();
}

void FocusSession_Test::theBeamIsSetWithinTheLensRange()
{
    // A fixed spot with a 15-35° lens zoomed by hand, and a wash with a zoom
    // channel over 10-40°.
    auto makeLight = [this](const QString &name, bool zoom, double lo, double hi, quint32 addr) {
        QLCFixtureDef *def = new QLCFixtureDef();
        def->setManufacturer("Test"); def->setModel(name);
        QLCFixtureMode *mode = new QLCFixtureMode(def);
        QLCChannel *dim = new QLCChannel();
        dim->setName("Dimmer"); dim->setGroup(QLCChannel::Intensity);
        def->addChannel(dim); mode->insertChannel(dim, 0);
        if (zoom)
        {
            QLCChannel *z = new QLCChannel();
            z->setName("Zoom"); z->setGroup(QLCChannel::Beam);
            z->setPreset(QLCChannel::BeamZoomSmallBig);
            def->addChannel(z); mode->insertChannel(z, 1);
            QLCChannel *f = new QLCChannel();
            f->setName("Focus"); f->setGroup(QLCChannel::Beam);
            f->setPreset(QLCChannel::BeamFocusNearFar);
            def->addChannel(f); mode->insertChannel(f, 2);
        }
        QLCPhysical ph;
        ph.setLensDegreesMin(lo);
        ph.setLensDegreesMax(hi);
        mode->setPhysical(ph);
        def->addMode(mode);
        Fixture *fx = new Fixture(m_doc);
        fx->setName(name);
        fx->setFixtureDefinition(def, mode);
        fx->setAddress(addr);
        m_doc->addFixture(fx);
        m_doc->monitorProperties()->setFixturePosition(fx->id(), 0, 0, QVector3D(4000, 0, 5));
        return fx;
    };
    Fixture *spot = makeLight("Spot", false, 15.0, 35.0, 30);
    Fixture *wash = makeLight("Wash", true, 10.0, 40.0, 40);
    MonitorProperties *props = m_doc->monitorProperties();
    QCOMPARE(RigHeads::beamAngle(spot, QByteArray(1, '\0')), 35.0);   // not set: its widest

    FocusSession::Options o;
    o.fixtures = { spot->id(), wash->id() };
    o.order = FocusSession::AsGiven;
    FocusSession *s = m_doc->programmer()->focusSession();
    QVERIFY(s->start(o));
    QCOMPARE(s->beamKind(spot->id()), FocusSession::LensSetByHand);
    QCOMPARE(s->beamKind(wash->id()), FocusSession::ZoomChannel);

    // By hand: set, held to its range, and part of what Save keeps.
    s->nudgeBeam(-5.0f);
    QCOMPARE(props->fixtureRigProps(spot->id()).lensDeg, 30.0f);
    QCOMPARE(RigHeads::beamAngle(spot, QByteArray(1, '\0')), 30.0);
    // The next nudge builds on the lens it is set to, not on its widest.
    s->nudgeBeam(-5.0f);
    QCOMPARE(props->fixtureRigProps(spot->id()).lensDeg, 25.0f);
    double deg = 0.0, lo = 0.0, hi = 0.0;
    QVERIFY(s->beamAngle(spot->id(), deg, lo, hi));
    QCOMPARE(deg, 25.0);
    s->nudgeBeam(-100.0f);
    QCOMPARE(props->fixtureRigProps(spot->id()).lensDeg, 15.0f);
    QVERIFY(s->isDirty(spot->id()));
    s->revert();
    QCOMPARE(props->fixtureRigProps(spot->id()).lensDeg, 0.0f);
    s->nudgeBeam(-10.0f);
    s->save();
    QVERIFY(!s->isDirty(spot->id()));

    // A zoom channel: sent while focusing, nothing saved.
    s->next();
    QCOMPARE(s->currentFixture(), wash->id());
    s->nudgeBeam(-10.0f);                                    // 40 -> 30
    QCOMPARE(s->pendingWrites().value(qMakePair(0, 41)), uchar(qRound((30.0 - 10.0) / 30.0 * 255.0)));
    QVERIFY(s->beamAngle(wash->id(), deg, lo, hi));         // the zoom asked for
    QCOMPARE(deg, 30.0);
    QVERIFY(!s->isDirty(wash->id()));
    QCOMPARE(props->fixtureRigProps(wash->id()).lensDeg, 0.0f);
    // Focus on the D-pad's left/right: its channel, from the middle.
    QVERIFY(!s->hasFocusControl(spot->id()));
    QVERIFY(s->hasFocusControl(wash->id()));
    s->apply(FocusSession::controlFor(GamepadRole::DpadRight));
    s->apply(FocusSession::controlFor(GamepadRole::DpadRight));
    double focus = 0.0;
    QVERIFY(s->focusLevel(wash->id(), focus));
    QCOMPARE(focus, 0.54);
    QCOMPARE(s->pendingWrites().value(qMakePair(0, 42)), uchar(qRound(0.54 * 255.0)));
    // Held, a D-pad arm repeats: once on the press, after a pause, then steadily.
    const FocusSession::Control tighter = FocusSession::controlFor(GamepadRole::DpadRight);
    s->press(tighter, true);
    QVERIFY(s->focusLevel(wash->id(), focus));
    QCOMPARE(focus, 0.56);
    s->tick(FocusSession::kRepeatDelay - 0.1);
    QVERIFY(s->focusLevel(wash->id(), focus));
    QCOMPARE(focus, 0.56);                                   // not yet
    s->tick(0.1);
    QVERIFY(s->focusLevel(wash->id(), focus));
    QCOMPARE(focus, 0.58);
    s->tick(FocusSession::kRepeatInterval * 2);
    QVERIFY(s->focusLevel(wash->id(), focus));
    QCOMPARE(focus, 0.62);
    s->press(tighter, false);                                // let go: it stops
    s->tick(1.0);
    QVERIFY(s->focusLevel(wash->id(), focus));
    QCOMPARE(focus, 0.62);
    // On the keyboard the controller is ignored: a held arm doesn't repeat.
    s->setInputDevice(FocusSession::KeyboardInput);
    s->press(tighter, true);
    QVERIFY(s->focusLevel(wash->id(), focus));
    QCOMPARE(focus, 0.64);                                   // the press itself
    s->tick(1.0);
    QVERIFY(s->focusLevel(wash->id(), focus));
    QCOMPARE(focus, 0.64);
    s->press(tighter, false);
    s->setInputDevice(FocusSession::ControllerInput);
    // All the way near is a level too: it is sent, not left where it was.
    s->nudgeFocus(-1.0f);
    QVERIFY(s->focusLevel(wash->id(), focus));
    QCOMPARE(focus, 0.0);
    QCOMPARE(s->pendingWrites().value(qMakePair(0, 42), uchar(99)), uchar(0));
    // A button that isn't on the D-pad doesn't repeat.
    QVERIFY(!FocusSession::repeats(FocusSession::controlFor(GamepadRole::RB)));
    s->stop();
    QCOMPARE(props->fixtureRigProps(spot->id()).lensDeg, 25.0f);   // the saved one stays
}

/* The changelog (Q140): "the focus look also takes gobo, prism and frost
   out" -- on the universe, over a running cue that has them in. */
void FocusSession_Test::theFocusLookTakesTheBeamOut()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Focus Gobo Spot");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("6ch");
    const QLCChannel::Group groups[] = { QLCChannel::Pan, QLCChannel::Tilt, QLCChannel::Intensity,
                                         QLCChannel::Gobo, QLCChannel::Prism, QLCChannel::Beam };
    const char *names[] = { "Pan", "Tilt", "Dimmer", "Gobo wheel", "Prism", "Frost" };
    QList<QLCChannel *> chans;
    QLCFixtureHead head;
    for (int k = 0; k < 6; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(names[k]);
        ch->setGroup(groups[k]);
        ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch);
        mode->insertChannel(ch, k);
        head.addChannel(k);
        chans << ch;
    }
    chans[3]->addCapability(new QLCCapability(0, 7, "Open"));
    chans[3]->addCapability(new QLCCapability(8, 15, "Gobo 1"));
    chans[3]->addCapability(new QLCCapability(16, 255, "Gobo 2"));
    chans[4]->addCapability(new QLCCapability(0, 9, "No prism"));
    chans[4]->addCapability(new QLCCapability(10, 255, "3-facet"));
    // The frost: a plain 0 (none) .. 255 (full), no capabilities.
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    mode->insertHead(-1, head);
    def->addMode(mode);
    Fixture *fx = new Fixture(m_doc);
    fx->setName("Gobo spot");
    fx->setFixtureDefinition(def, mode);
    fx->setUniverse(0);
    fx->setAddress(40);
    QVERIFY(m_doc->addFixture(fx));
    m_doc->monitorProperties()->setFixturePosition(fx->id(), 0, 0, QVector3D(2000.0f, 2000.0f, 5.0f));
    StageTarget *mark = makeTarget(m_doc, "Mark", QVector3D(4.0f, 6.0f, 0.0f));
    Universe *uni = m_doc->inputOutputMap()->universes().at(0);

    // A running cue with a gobo in, the prism in and full frost.
    QSharedPointer<GenericFader> cue = uni->requestFader();
    const QList<QPair<quint32, uchar>> cueHas = { { 3, 200 }, { 4, 150 }, { 5, 255 } };
    for (const QPair<quint32, uchar> &c : cueHas)
    {
        GenericFader::ChannelRef fc = cue->getChannelFader(m_doc, uni, fx->id(), c.first);
        fc->setStart(c.second);
        fc->setCurrent(c.second);
        fc->setTarget(c.second);
    }
    uni->processFaders();
    QCOMPARE(int(uni->preGMValue(40 + 3)), 200);

    FocusSession *s = m_doc->programmer()->focusSession();
    QVERIFY(s->start(optionsFor({ fx->id() }, mark->id())));
    for (int i = 0; i < 3; ++i)
    {
        m_doc->masterTimer()->timerTick();
        uni->processFaders();
    }
    // The bare beam: the gobo wheel open, no prism, no frost.
    QCOMPARE(int(uni->preGMValue(40 + 3)), 3);       // the middle of "Open" 0-7
    QCOMPARE(int(uni->preGMValue(40 + 4)), 4);       // the middle of "No prism" 0-9
    QCOMPARE(int(uni->preGMValue(40 + 5)), 0);
    QVERIFY(uni->preGMValue(40 + 2) > 0);            // and lit

    // Stopped, the cue has its beam back.
    s->stop();
    m_doc->masterTimer()->timerTick();
    uni->processFaders();
    for (const QPair<quint32, uchar> &c : cueHas)
        QCOMPARE(int(uni->preGMValue(40 + int(c.first))), int(c.second));
    uni->dismissFader(cue);
}

/* A fixed light's Save is undone like a moving head's (the changelog's
   Save/Undo for every light Focus mode works): back to the aim the save
   replaced, then to the light as it was -- not aimed, not checked. */
void FocusSession_Test::aFixedLightsSaveIsUndone()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Undo Par");
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    QLCChannel *dim = new QLCChannel();
    dim->setName("Dimmer"); dim->setGroup(QLCChannel::Intensity);
    def->addChannel(dim); mode->insertChannel(dim, 0);
    def->addMode(mode);
    Fixture *par = new Fixture(m_doc);
    par->setName("FOH 2");
    par->setFixtureDefinition(def, mode);
    par->setAddress(20);
    QVERIFY(m_doc->addFixture(par));
    MonitorProperties *props = m_doc->monitorProperties();
    props->setFixturePosition(par->id(), 0, 0, QVector3D(4000, 0, 5));

    FocusSession::Options o;
    o.fixtures = { par->id() };
    FocusSession *s = m_doc->programmer()->focusSession();
    QVERIFY(s->start(o));
    QVERIFY(s->isFixedLight(par->id()));
    QVERIFY(!s->canUndo());
    m_doc->resetModified();

    // First save: 2 m downstage.
    for (int i = 0; i < 4; ++i)
        s->nudge(0.0f, -1.0f);
    s->save();
    QVERIFY(m_doc->isModified());
    QVERIFY(s->isChecked(par->id()));
    const FixtureRigProps first = props->fixtureRigProps(par->id());
    QVERIFY(first.fixedAim);
    // Second save: 1 m toward stage left as well.
    s->nudge(2.0f, 0.0f);
    s->save();
    const FixtureRigProps second = props->fixtureRigProps(par->id());
    QVERIFY(qAbs(second.panZeroDir - first.panZeroDir) > 1.0f);
    QVector3D at;

    // Undo: the first save's aim, where it lands with it.
    QVERIFY(s->undoSave());
    FixtureRigProps rp = props->fixtureRigProps(par->id());
    QVERIFY(rp.fixedAim);
    QVERIFY2(qAbs(rp.panZeroDir - first.panZeroDir) < 0.01f, qPrintable(QString::number(rp.panZeroDir)));
    QVERIFY(qAbs(rp.aimDown - first.aimDown) < 0.01);
    QVERIFY(s->referencePoint(par->id(), at));
    QVERIFY2(qAbs(at.x() - 4.0f) < 1e-2f && qAbs(at.y() - 2.0f) < 1e-2f,
             qPrintable(QString("%1, %2").arg(at.x()).arg(at.y())));
    QVERIFY(!s->isDirty(par->id()));
    // Undo again: as it was before the session -- not aimed, not checked.
    QVERIFY(s->undoSave());
    QVERIFY(!props->fixtureRigProps(par->id()).fixedAim);
    QVERIFY(!s->isChecked(par->id()));
    QVERIFY(s->referencePoint(par->id(), at));
    QVERIFY(qAbs(at.y()) < 1e-2f);                   // straight down again
    QVERIFY(!s->undoSave());
    s->stop();
    QVERIFY(!props->fixtureRigProps(par->id()).fixedAim);
}

void FocusSession_Test::aLayerOutlivingItsDocNeverCallsIt()
{
    Doc *doc = new Doc(nullptr);
    QVERIFY(doc->inputOutputMap()->addUniverse());
    Fixture *fxi = new Fixture(doc);
    fxi->setChannels(2);
    QVERIFY(doc->addFixture(fxi));

    OutputLayer layer(doc, Universe::Focus, 0, QStringLiteral("probe"));
    QCOMPARE(layer.doc(), doc);
    layer.set({ OutputLayer::Level{ fxi->id(), 0, 200 } });
    QCOMPARE(layer.channelCounts().value(0), 1);

    delete doc;
    QVERIFY2(layer.doc() == nullptr, "the layer still points at a deleted Doc");
    layer.set({ OutputLayer::Level{ 0, 0, 10 } });   // nothing to write through
    layer.clear();
    QVERIFY(layer.channelCounts().isEmpty());
}

QTEST_MAIN(FocusSession_Test)

void FocusSession_Test::theViewsAreToldTheIntentNotTheNudge()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 6.0f, 2.0f);
    StageTarget *mark = makeTarget(m_doc, "Mark", QVector3D(5.0f, 6.0f, 0.0f));
    // A saved calibration already: the intent must not carry it either.
    FixtureRigProps rp = m_doc->monitorProperties()->fixtureRigProps(a->id());
    rp.panOffsetDeg = 3.0f;
    rp.tiltOffsetDeg = -1.5f;
    m_doc->monitorProperties()->setFixtureRigProps(a->id(), rp);

    FocusSession *s = m_doc->programmer()->focusSession();
    QVector3D dir;
    QVERIFY(!s->intendedDirection(a->id(), dir));          // not running
    FocusSession::Options o = optionsFor({ a->id(), b->id() }, mark->id());
    o.order = FocusSession::AsGiven;
    QVERIFY(s->start(o));
    QCOMPARE(s->currentFixture(), a->id());

    AimSolver::Frame frame;
    QVERIFY(AimSolver::frameFor(m_doc, a->id(), frame));
    const QVector3D want = (mark->position() - frame.position).normalized();
    auto offBy = [&](const char *when) {
        QVector3D d;
        if (!s->intendedDirection(a->id(), d))
        {
            qWarning() << when << ": no intended direction";
            return 180.0;
        }
        const double cosA = qBound(-1.0, double(QVector3D::dotProduct(d.normalized(), want)), 1.0);
        return qRadiansToDegrees(qAcos(cosA));
    };
    auto check = [&](const char *when) {
        const double deg = offBy(when);
        QVERIFY2(deg < 0.05, qPrintable(QString("%1: the drawn aim is %2 degrees off its mark")
                                        .arg(when).arg(deg)));
    };

    check("at the start");
    s->nudge(23.7f, -11.3f);                                 // a big, unsaved nudge
    QVERIFY(s->isDirty(a->id()));
    check("nudged");
    // Its gauges read the same intent: a pan and tilt the nudge is not in.
    double pan = 0.0, tilt = 0.0, pan0 = 0.0, tilt0 = 0.0;
    QVERIFY(s->intendedAngles(a->id(), pan, tilt));
    s->save();
    check("saved");
    QVERIFY(s->intendedAngles(a->id(), pan0, tilt0));
    QCOMPARE(pan0, pan);
    QCOMPARE(tilt0, tilt);
    s->nudge(-7.0f, 4.0f);
    check("nudged again");
    s->revert();
    check("reverted");
    s->zero();
    check("zeroed");
    s->revert();
    QVERIFY(s->undoSave());
    check("undone");
    s->nudge(5.0f, 5.0f);
    s->discardAll();
    check("discarded");

    // Only the head being focused.
    QVERIFY(!s->intendedDirection(b->id(), dir));
    s->next();
    QVERIFY(!s->intendedDirection(a->id(), dir));
    QVERIFY(s->intendedDirection(b->id(), dir));
    s->stop();
    QVERIFY(!s->intendedDirection(b->id(), dir));
}

void FocusSession_Test::aColourMixingHeadIsLitWhite()
{
    /* An LED head with no plain dimmer: pan, tilt, red, green, blue, cyan,
       and a prism rotation known only by its name. */
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Focus LED Spot");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("7ch");
    const QLCChannel::Group groups[] = { QLCChannel::Pan, QLCChannel::Tilt, QLCChannel::Intensity,
                                         QLCChannel::Intensity, QLCChannel::Intensity,
                                         QLCChannel::Intensity, QLCChannel::Prism };
    const QLCChannel::PrimaryColour colours[] = { QLCChannel::NoColour, QLCChannel::NoColour,
                                                  QLCChannel::Red, QLCChannel::Green, QLCChannel::Blue,
                                                  QLCChannel::Cyan, QLCChannel::NoColour };
    const char *names[] = { "Pan", "Tilt", "Red", "Green", "Blue", "Cyan", "Prism rotation" };
    QLCFixtureHead head;
    for (int k = 0; k < 7; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(names[k]);
        ch->setGroup(groups[k]);
        ch->setColour(colours[k]);
        ch->setControlByte(QLCChannel::MSB);
        if (groups[k] == QLCChannel::Prism)
        {
            ch->addCapability(new QLCCapability(0, 9, "Off"));
            ch->addCapability(new QLCCapability(10, 255, "Rotate"));
        }
        def->addChannel(ch);
        mode->insertChannel(ch, k);
        head.addChannel(k);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    mode->insertHead(-1, head);
    def->addMode(mode);
    Fixture *led = new Fixture(m_doc);
    led->setName("LED");
    led->setFixtureDefinition(def, mode);
    led->setAddress(50);
    QVERIFY(m_doc->addFixture(led));
    m_doc->monitorProperties()->setFixturePosition(led->id(), 0, 0, QVector3D(2000, 2000, 5));
    StageTarget *mark = makeTarget(m_doc, "Mark", QVector3D(5.0f, 6.0f, 0.0f));

    FocusSession *s = m_doc->programmer()->focusSession();
    QString why;
    QVERIFY2(s->start(optionsFor({ led->id() }, mark->id()), &why), qPrintable(why));
    const QHash<QPair<int, int>, uchar> w = s->pendingWrites();
    // White: every additive colour full, the subtractive one out.
    QCOMPARE(w.value(qMakePair(0, 52), uchar(1)), uchar(255));
    QCOMPARE(w.value(qMakePair(0, 53), uchar(1)), uchar(255));
    QCOMPARE(w.value(qMakePair(0, 54), uchar(1)), uchar(255));
    QCOMPARE(w.value(qMakePair(0, 55), uchar(1)), uchar(0));
    // A prism rotation is not in the beam's way: left alone, even with an "Off".
    QVERIFY(!w.contains(qMakePair(0, 56)));
}

void FocusSession_Test::aStaleStickDoesNotDriftAfterTheKeyboard()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    StageTarget *mark = makeTarget(m_doc, "Mark", QVector3D(5.0f, 6.0f, 0.0f));
    FocusSession *s = m_doc->programmer()->focusSession();
    QVERIFY(s->start(optionsFor({ a->id() }, mark->id())));
    s->setStickResponse(0.05f, 1.0f);

    // Both sticks pushed as the operator turns to the keyboard...
    s->setStick(FocusSession::Coarse, 1.0f, 0.5f);
    s->setStick(FocusSession::Fine, 1.0f, 1.0f);
    s->setInputDevice(FocusSession::KeyboardInput);
    s->tick(0.1);
    QCOMPARE(s->liveOffset(a->id()).pan, 0.0f);
    QCOMPARE(s->liveOffset(a->id()).tilt, 0.0f);
    // ...and back to the controller: neither stick says anything until it moves.
    s->setInputDevice(FocusSession::ControllerInput);
    s->tick(0.1);
    QCOMPARE(s->liveOffset(a->id()).pan, 0.0f);
    QCOMPARE(s->liveOffset(a->id()).tilt, 0.0f);
    s->setStick(FocusSession::Fine, 1.0f, 0.5f);
    s->tick(0.1);
    QVERIFY(s->liveOffset(a->id()).pan > 0.0f);
}

void FocusSession_Test::itsTimersNeverSpin()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    StageTarget *mark = makeTarget(m_doc, "Mark", QVector3D(5.0f, 6.0f, 0.0f));
    FocusSession *s = m_doc->programmer()->focusSession();
    s->setFlashEnabled(true);
    QVERIFY(s->start(optionsFor({ a->id() }, mark->id())));
    s->nudge(1.0f, 0.0f);
    QVERIFY(s->isBreathing());
    // The unsaved light breathes on a timer, not on every pass of the event loop.
    const QList<QTimer *> timers = s->findChildren<QTimer *>();
    QVERIFY(timers.size() >= 2);
    bool anyActive = false;
    for (const QTimer *t : timers)
    {
        QVERIFY2(t->interval() > 0, qPrintable(t->objectName()));
        anyActive = anyActive || t->isActive();
    }
    QVERIFY(anyActive);
    s->stop();
}
