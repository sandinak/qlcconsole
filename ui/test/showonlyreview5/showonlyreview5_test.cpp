/*
  Q Light Controller Plus - Unit test
  showonlyreview5_test.cpp

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
#include <QWidget>

#include "../../../engine/test/common/stdprelude.h"
#define protected public
#define private public
#include "simpledeskengine.h"
#include "vcslider.h"
#include "audiobar.h"
#include "virtualconsole.h"
#include "iopluginstub.h"
#include "vcxypad.h"
#include "vcxypadarea.h"
#include "vcxypadfixture.h"
#include "vcaudiotriggers.h"
#include "vcclock.h"
#include "grouphead.h"
#include "vcbutton.h"
#include "vccuelist.h"
#include "mastertimer.h"
#undef private
#undef protected

#include "showonlyreview5_test.h"
#include "outputrulekeeper.h"
#include "qlcfixturedefcache.h"
#include "inputoutputmap.h"
#include "qlcfixturemode.h"
#include "ioplugincache.h"
#include "qlcfixturedef.h"
#include "showcontrol.h"
#include "mastertimer.h"
#include "genericfader.h"
#include "chaserstep.h"
#include "qlcchannel.h"
#include "universe.h"
#include "fixture.h"
#include "qlcfile.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"
#include "function.h"
#include "script.h"
#include "programmercontroller.h"
#include <QXmlStreamReader>
#include <QMouseEvent>
#include <QThread>
#include "cuestack.h"
#include "cue.h"
#include "collection.h"
#include "timecodesource.h"
#include "timecodecues.h"
#include "showtreecontroller.h"

enum { LaserAt = 0, Stage = 10, Flame = 14 };

void ShowOnlyReview5_Test::initTestCase()
{
    m_doc = new Doc(this);
    new VirtualConsole(NULL, m_doc);
    QDir dir(QStringLiteral("../../../engine/test/iopluginstub"));
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    m_stub = static_cast<IOPluginStub *>(
        m_doc->ioPluginCache()->plugin(QStringLiteral("I/O Plugin Stub")));
    QVERIFY2(m_stub != nullptr, "the I/O plugin stub did not load");
    QVERIFY(m_doc->inputOutputMap()->setOutputPatch(0, m_stub->name(), QString(), 0));
}

void ShowOnlyReview5_Test::cleanupTestCase()
{
    delete VirtualConsole::instance();
    delete m_doc;
}

void ShowOnlyReview5_Test::cleanup()
{
    for (Function *f : m_doc->functions())
        if (f->isRunning())
            f->stop(FunctionParent::master());
    tick(3);
    m_doc->setMode(Doc::Design);
    m_doc->clearContents();
    m_laserDef = nullptr;   // the cache keeps it; a new one per test is fine
    m_doc->outputRuleKeeper()->rebuild();
    QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
    for (Universe *u : ua)
    {
        for (const QSharedPointer<GenericFader> &f : u->faders())
            u->dismissFader(f);
        u->reset();
    }
    m_doc->inputOutputMap()->releaseUniverses(false);
    tick(2);
}

Fixture *ShowOnlyReview5_Test::addDimmer(const QString &name, int address, int rules)
{
    Fixture *f = new Fixture(m_doc);
    f->setName(name);
    f->setChannels(1);
    f->setAddress(quint32(address));
    f->setUniverse(0);
    f->setOutputRules(Fixture::OutputRules(rules));
    if (!m_doc->addFixture(f))
    {
        delete f;
        return nullptr;
    }
    return f;
}

Fixture *ShowOnlyReview5_Test::addLaser(const QString &name, int address)
{
    /* As the bundled Chauvet Scorpion Storm RGBY: its Control Mode channel
       (Maintenance, LTP) has Default="151" -- "DMX mode 1" -- and its
       colours are what light the beam. */
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer(QStringLiteral("Review5"));
    def->setModel(QStringLiteral("Laser %1").arg(name));
    def->setType(QLCFixtureDef::Laser);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName(QStringLiteral("DMX"));
    QLCChannel *ctl = new QLCChannel();
    ctl->setName(QStringLiteral("Control Mode"));
    ctl->setGroup(QLCChannel::Maintenance);
    ctl->setDefaultValue(151);
    def->addChannel(ctl);
    mode->insertChannel(ctl, 0);
    QLCChannel *red = new QLCChannel();
    red->setName(QStringLiteral("Red"));
    red->setGroup(QLCChannel::Intensity);
    red->setColour(QLCChannel::Red);
    def->addChannel(red);
    mode->insertChannel(red, 1);
    def->addMode(mode);
    if (!m_doc->fixtureDefCache()->addFixtureDef(def))
    {
        delete def;
        return nullptr;
    }
    m_laserDef = def;
    Fixture *f = new Fixture(m_doc);
    f->setName(name);
    f->setFixtureDefinition(def, def->modes().first());
    f->setUniverse(0);
    f->setAddress(quint32(address));
    f->setOutputRules(Fixture::ShowOnly);
    if (!m_doc->addFixture(f))
    {
        delete f;
        return nullptr;
    }
    return f;
}

void ShowOnlyReview5_Test::tick(int times)
{
    for (int i = 0; i < times; ++i)
    {
        m_doc->masterTimer()->timerTick();
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        for (Universe *u : ua)
            u->processFaders();
        m_doc->inputOutputMap()->releaseUniverses(false);
        QCoreApplication::processEvents();
    }
}

int ShowOnlyReview5_Test::sent(int address) const
{
    return address < m_stub->m_universe.size() ? int(uchar(m_stub->m_universe.at(address))) : -1;
}

void ShowOnlyReview5_Test::allOff()
{
    ShowControl::Request off;
    off.command = ShowControl::AllOff;
    QVERIFY(m_doc->showControl()->run(off, ShowControl::RoutePad));
}

void ShowOnlyReview5_Test::goUnrelated(int level)
{
    Scene *stage = new Scene(m_doc);
    stage->setValue(m_doc->fixtureForAddress(Stage), 0, uchar(level));
    m_doc->addFunction(stage);
    Chaser *list = new Chaser(m_doc);
    list->setName(QStringLiteral("Unrelated %1").arg(level));
    ChaserStep s(stage->id());
    s.hold = 100000;
    list->addStep(s);
    m_doc->addFunction(list);
    ShowControl::Request go;
    go.command = ShowControl::Go;
    go.chaser = list->id();
    QVERIFY(m_doc->showControl()->run(go, ShowControl::RouteRunTab));
}

/* C2 / the Simple Desk: the stop lets go of the Simple Desk's Show-only
   channels (SimpleDeskEngine::releaseShowOnlyChannels()), and its
   ResetChannel command then writes the channel's DEFINITION default into
   the stored value (Universe::setChannelDefaultValue) -- after the stop
   reset it to safe. A laser's safe value is 0 (FixtureRest::restValue());
   the stored 151 goes out at the next, unrelated, GO. */
void ShowOnlyReview5_Test::simpleDeskReleaseLeavesTheLaserAtSafe()
{
    QVERIFY(addLaser(QStringLiteral("Scorpion"), LaserAt) != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    {
        SimpleDeskEngine eng(m_doc);
        eng.setValue(LaserAt, 200);         // the desk puts the laser in DMX mode 2
        tick(3);
        QCOMPARE(sent(LaserAt), 200);       // Operate: the Simple Desk drives it

        allOff();
        tick(3);
        QCOMPARE(sent(LaserAt), 0);         // the e-stop: out, latched
        QVERIFY(!eng.hasChannel(LaserAt));  // and the desk let go of it

        goUnrelated(60);
        int laserMax = 0;
        for (int i = 0; i < 5; i++)
        {
            tick(1);
            laserMax = qMax(laserMax, sent(LaserAt));
        }
        QCOMPARE(sent(Stage), 60);          // the GO took
        // Nothing drives the laser: it stays at its safe value.
        QCOMPARE(laserMax, 0);
    }
}

/* The freeze (GenericFader::freezeForStop) is on the fader object; a VC
   slider's faders are dismissed in Design and made new in Operate
   (VCSlider::slotModeChanged sets m_levelValueChanged), so a slider left up
   through the stop is not frozen any more after Design -> Operate. */
void ShowOnlyReview5_Test::vcSliderLeftUpStaysHeldAcrossDesignAndOperate()
{
    Fixture *flame = addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly);
    QVERIFY(flame != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    QWidget w;
    VCSlider slider(&w, m_doc);
    slider.setSliderMode(VCSlider::Level);
    slider.addLevelChannel(flame->id(), 0);
    m_doc->setMode(Doc::Operate);
    slider.setSliderValue(255, false, true);    // pushed up in Operate
    tick(3);
    QCOMPARE(sent(Flame), 255);                 // a VC slider in Operate: the show

    allOff();
    tick(3);
    QCOMPARE(sent(Flame), 0);

    // Control: the fix holds a slider left up through a GO.
    goUnrelated(40);
    tick(4);
    QCOMPARE(sent(Stage), 40);
    QCOMPARE(sent(Flame), 0);

    // Again: the stop, then a trip to Design (to fix a cue) and back.
    allOff();
    tick(3);
    m_doc->setMode(Doc::Design);
    tick(2);
    m_doc->setMode(Doc::Operate);
    tick(2);
    QCOMPARE(sent(Flame), 0);                   // still latched
    goUnrelated(60);
    int flameMax = 0;
    for (int i = 0; i < 5; i++)
    {
        tick(1);
        flameMax = qMax(flameMax, sent(Flame));
    }
    QCOMPARE(sent(Stage), 60);
    // Nobody moved the slider since the stop: the flame must stay out.
    QCOMPARE(flameMax, 0);
}

/* unfreezeStop() runs on any m_levelValueChanged, not on a move: an
   external input sending the SAME value again (a motor fader's echo, an
   OSC surface re-syncing its page, an audio bar driving the slider)
   unfreezes it. */
void ShowOnlyReview5_Test::vcSliderLeftUpStaysHeldWhenItsInputEchoes()
{
    Fixture *flame = addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly);
    QVERIFY(flame != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    QWidget w;
    VCSlider slider(&w, m_doc);
    slider.setSliderMode(VCSlider::Level);
    slider.addLevelChannel(flame->id(), 0);
    m_doc->setMode(Doc::Operate);
    slider.setSliderValue(255, false, true);
    tick(3);
    QCOMPARE(sent(Flame), 255);

    allOff();
    tick(3);
    QCOMPARE(sent(Flame), 0);
    // The controller re-sends where its fader already is: not a move.
    slider.setSliderValue(255, false, true);
    tick(2);
    goUnrelated(60);
    int flameMax = 0;
    for (int i = 0; i < 5; i++)
    {
        tick(1);
        flameMax = qMax(flameMax, sent(Flame));
    }
    QCOMPARE(sent(Stage), 60);
    QCOMPARE(flameMax, 0);
}

/* An audio trigger's function bar armed before the stop: All Off stops
   its function, the music goes on, the next peak starts it again
   (AudioBar::checkFunctionThresholds, a fresh AutoVCWidget source in this
   session), and the unrelated GO lets the latch go -- the fogger fires on
   the beat. Its DMX bars, on the same widget, are frozen instead. */
void ShowOnlyReview5_Test::audioTriggerArmedBeforeTheStopStaysHeld()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    Scene *burst = new Scene(m_doc);
    burst->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(burst);

    AudioBar bar(AudioBar::FunctionBar, 0, 77);    // on VC audio triggers widget 77
    bar.attachFunction(burst);
    bar.setMinThreshold(20);
    bar.setMaxThreshold(200);
    // Switched on by the operator in Operate (R6-3: its own first start
    // arms nothing).
    Function::armAutoSource(bar.functionParent());
    bar.m_value = 230;                              // a peak
    bar.checkFunctionThresholds(m_doc);
    tick(3);
    QCOMPARE(sent(Flame), 255);                     // audio triggers in Operate: the show

    allOff();
    tick(3);
    QCOMPARE(sent(Flame), 0);
    bar.m_value = 230;                              // the music goes on
    bar.checkFunctionThresholds(m_doc);
    tick(3);
    QCOMPARE(sent(Flame), 0);                       // latched

    goUnrelated(60);
    int flameMax = 0;
    for (int i = 0; i < 5; i++)
    {
        tick(1);
        flameMax = qMax(flameMax, sent(Flame));
    }
    QCOMPARE(sent(Stage), 60);
    // Nobody touched the audio triggers since the stop.
    QCOMPARE(flameMax, 0);
}

/****************************************************************************
 * The e-stop rules (OutputRuleKeeper E1-E7), swept over the hand controls
 * and automatic widgets: set up in Operate, All Off, an unrelated GO --
 * held -- then its own operator action -- driven again.
 ****************************************************************************/

void ShowOnlyReview5_Test::sweepSimpleDeskChannel()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    SimpleDeskEngine eng(m_doc);
    eng.setValue(Flame, 200);
    tick(3);
    QCOMPARE(sent(Flame), 200);
    allOff();
    tick(3);
    QCOMPARE(sent(Flame), 0);
    goUnrelated(60);
    tick(4);
    QCOMPARE(sent(Stage), 60);
    QCOMPARE(sent(Flame), 0);
    eng.setValue(Flame, 180);           // a hand on it, after the stop
    tick(3);
    QCOMPARE(sent(Flame), 180);
}

void ShowOnlyReview5_Test::sweepVcSlider()
{
    Fixture *flame = addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly);
    QVERIFY(flame != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    QWidget w;
    VCSlider slider(&w, m_doc);
    slider.setSliderMode(VCSlider::Level);
    slider.addLevelChannel(flame->id(), 0);
    m_doc->setMode(Doc::Operate);
    slider.setSliderValue(255, false, true);
    tick(3);
    QCOMPARE(sent(Flame), 255);
    allOff();
    tick(3);
    QCOMPARE(sent(Flame), 0);
    goUnrelated(60);
    tick(4);
    QCOMPARE(sent(Flame), 0);
    slider.setSliderValue(200, false, true);    // moved after the stop
    tick(3);
    QCOMPARE(sent(Flame), 200);
}

void ShowOnlyReview5_Test::sweepXYPad()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer(QStringLiteral("Review5"));
    def->setModel(QStringLiteral("Mover"));
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName(QStringLiteral("2ch"));
    QLCChannel *pan = new QLCChannel();
    pan->setName(QStringLiteral("Pan"));
    pan->setGroup(QLCChannel::Pan);
    pan->setControlByte(QLCChannel::MSB);
    def->addChannel(pan);
    mode->insertChannel(pan, 0);
    QLCChannel *tilt = new QLCChannel();
    tilt->setName(QStringLiteral("Tilt"));
    tilt->setGroup(QLCChannel::Tilt);
    tilt->setControlByte(QLCChannel::MSB);
    def->addChannel(tilt);
    mode->insertChannel(tilt, 1);
    def->addMode(mode);
    mode->cacheHeads();
    QVERIFY(m_doc->fixtureDefCache()->addFixtureDef(def));
    Fixture *mover = new Fixture(m_doc);
    mover->setName(QStringLiteral("Mover"));
    mover->setFixtureDefinition(def, mode);
    mover->setUniverse(0);
    mover->setAddress(30);
    mover->setOutputRules(Fixture::ShowOnly);
    QVERIFY(m_doc->addFixture(mover));
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();

    QWidget w;
    VCXYPad pad(&w, m_doc);
    VCXYPadFixture fx(m_doc);
    fx.setHead(GroupHead(mover->id(), 0));
    pad.appendFixture(fx);
    m_doc->setMode(Doc::Operate);
    for (VCXYPadFixture &f : pad.m_fixtures)
        f.arm();
    auto tickPad = [this, &pad](int n) {
        for (int i = 0; i < n; i++)
        {
            QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
            pad.writeDMX(m_doc->masterTimer(), ua);
            m_doc->inputOutputMap()->releaseUniverses(false);
            tick(1);
        }
    };
    pad.m_area->setPosition(QPointF(200, 100));     // a hand on the pad
    tickPad(3);
    const int moved = sent(30);
    QVERIFY(moved > 0);
    allOff();
    tickPad(3);
    QCOMPARE(sent(30), 0);
    goUnrelated(60);
    tickPad(4);
    QCOMPARE(sent(30), 0);
    pad.m_area->setPosition(QPointF(120, 80));      // moved after the stop
    tickPad(3);
    QVERIFY(sent(30) > 0);
}

void ShowOnlyReview5_Test::sweepAudioFunctionBar()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    Scene *burst = new Scene(m_doc);
    burst->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(burst);
    AudioBar bar(AudioBar::FunctionBar, 0, 78);
    bar.attachFunction(burst);
    bar.setMinThreshold(20);
    bar.setMaxThreshold(200);
    Function::armAutoSource(bar.functionParent());  // switched on in Operate (R6-3)
    bar.m_value = 230;
    bar.checkFunctionThresholds(m_doc);
    tick(3);
    QCOMPARE(sent(Flame), 255);
    allOff();
    tick(3);
    goUnrelated(60);
    bar.m_value = 230;
    bar.checkFunctionThresholds(m_doc);
    tick(4);
    QCOMPARE(sent(Flame), 0);
    // Re-armed by the operator (the widget switched on in Operate does
    // this): the next peak is the show.
    Function::armAutoSource(bar.functionParent());
    bar.checkFunctionThresholds(m_doc);
    tick(3);
    QCOMPARE(sent(Flame), 255);
}

void ShowOnlyReview5_Test::sweepAudioDmxBar()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    QWidget w;
    VCAudioTriggers trig(&w, m_doc);
    trig.m_volumeBar->m_type = AudioBar::DMXBar;
    trig.m_volumeBar->m_absDmxChannels = { Flame };
    trig.m_volumeBar->m_value = 200;
    trig.m_button->blockSignals(true);
    trig.m_button->setChecked(true);
    trig.m_button->blockSignals(false);
    m_doc->setMode(Doc::Operate);
    // Switched on in Operate (slotEnableButtonToggled() stamps it so; the
    // capture itself is left out of a test).
    trig.m_liveEpoch.store(GenericFader::liveEpoch());
    auto tickBar = [this, &trig](int n) {
        for (int i = 0; i < n; i++)
        {
            QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
            trig.writeDMX(m_doc->masterTimer(), ua);
            m_doc->inputOutputMap()->releaseUniverses(false);
            tick(1);
        }
    };
    tickBar(3);
    QCOMPARE(sent(Flame), 200);
    allOff();
    tickBar(3);
    QCOMPARE(sent(Flame), 0);
    goUnrelated(60);
    tickBar(4);
    QCOMPARE(sent(Flame), 0);           // the music goes on: held
    trig.m_liveEpoch.store(GenericFader::liveEpoch());     // switched on again
    tickBar(3);
    QCOMPARE(sent(Flame), 200);
    m_doc->setMode(Doc::Design);        // as the VC leaves it before it goes
}

void ShowOnlyReview5_Test::sweepVcClockSchedule()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    Scene *fog = new Scene(m_doc);
    fog->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(fog);
    QWidget w;
    VCClock clock(&w, m_doc);
    // Started by the operator in Operate (R6-3: never its own first start).
    clock.playPauseTimer();
    // Its schedule starts the function (VCClock::slotUpdateTime()).
    fog->start(m_doc->masterTimer(), clock.functionParent());
    tick(3);
    QCOMPARE(sent(Flame), 255);
    allOff();
    tick(3);
    goUnrelated(60);
    fog->start(m_doc->masterTimer(), clock.functionParent());     // the schedule again
    tick(4);
    QCOMPARE(sent(Flame), 0);
    // Touched by the operator in Operate (VCClock::mousePressEvent()).
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(5, 5), QPointF(5, 5),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    clock.mousePressEvent(&press);
    fog->start(m_doc->masterTimer(), clock.functionParent());     // the next schedule
    tick(3);
    QCOMPARE(sent(Flame), 255);
}

/****************************************************************************
 * Sixth review (of 1391a70ca1)
 ****************************************************************************/

/* R6-1. A real VC audio triggers widget: its bars are made in its
   constructor, before the widget has an id (VCWidget::invalidId(), set
   later by loadXMLCommon()/setID()), so AudioBar::m_parentId is
   quint32(-1) and AudioBar::functionParent() is FunctionParent::master(),
   not the AutoVCWidget source the "armed in session" registry keys on.
   Every peak starts its function as a fresh Master source: after All Off
   the music goes on and the flame fires on the next peak. */
void ShowOnlyReview5_Test::r6AudioWidgetFunctionBarAfterStop()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    Scene *burst = new Scene(m_doc);
    burst->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(burst);
    QWidget w;
    VCAudioTriggers *at = new VCAudioTriggers(&w, m_doc);
    at->setID(42);                                  // as loadXMLCommon() does
    AudioBar *bar = at->m_spectrumBars.at(0);
    bar->setType(AudioBar::FunctionBar);
    bar->attachFunction(burst);
    bar->setMinThreshold(20);
    bar->setMaxThreshold(200);
    m_doc->setMode(Doc::Operate);
    // Switched on by the operator in Operate: what slotEnableButtonToggled() arms.
    Function::armAutoSource(FunctionParent(FunctionParent::AutoVCWidget, at->id()));
    Function::armAutoSource(bar->functionParent());
    bar->m_value = 230;
    bar->checkFunctionThresholds(m_doc);
    tick(3);
    QCOMPARE(sent(Flame), 255);

    allOff();
    tick(3);
    QCOMPARE(sent(Flame), 0);
    bar->m_value = 230;                             // the music goes on; nobody touches it
    bar->checkFunctionThresholds(m_doc);
    tick(3);
    QCOMPARE(sent(Flame), 0);
    delete at;
}

/* R6-2. An audio bar of type VC widget on a VC slider (Level, on the
   flame): AudioBar::checkWidgetFunctionality() calls
   VCSlider::setSliderValue(m_value, true, true) -- external, a new value
   on every beat -- which stamps the slider as "really moved". After All
   Off the music moves the slider and the flame follows. */
void ShowOnlyReview5_Test::r6AudioBarMovesVcSliderAfterStop()
{
    Fixture *flame = addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly);
    QVERIFY(flame != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    QWidget w;
    VCSlider slider(&w, m_doc);
    slider.setID(50);
    slider.setSliderMode(VCSlider::Level);
    slider.addLevelChannel(flame->id(), 0);
    m_doc->setMode(Doc::Operate);
    AudioBar bar(AudioBar::VCWidgetBar, 0, 77);
    bar.m_widgetID = slider.id();
    bar.m_widget = &slider;
    Function::armAutoSource(bar.functionParent());  // its widget switched on in Operate (R6-3)
    bar.m_value = 230;
    bar.checkWidgetFunctionality();
    tick(3);
    QCOMPARE(sent(Flame), 230);

    allOff();
    tick(3);
    QCOMPARE(sent(Flame), 0);
    bar.m_value = 240;                              // the next beat; nobody touched anything
    bar.checkWidgetFunctionality();
    tick(3);
    QCOMPARE(sent(Flame), 0);
}

/* R6-3. The same bar on a VC button: the peak presses it
   (VCButton::pressFunction()), a ManualVCWidget start in this session --
   the show. After All Off the next peak lights the flame. */
void ShowOnlyReview5_Test::r6AudioBarPressesVcButtonAfterStop()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    Scene *burst = new Scene(m_doc);
    burst->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(burst);
    QWidget w;
    VCButton button(&w, m_doc);
    button.setID(51);
    button.setFunction(burst->id());
    m_doc->setMode(Doc::Operate);
    AudioBar bar(AudioBar::VCWidgetBar, 0, 77);
    bar.setMinThreshold(20);
    bar.setMaxThreshold(200);
    bar.m_widgetID = button.id();
    bar.m_widget = &button;
    Function::armAutoSource(bar.functionParent());  // its widget switched on in Operate (R6-3)
    bar.m_value = 230;
    bar.checkWidgetFunctionality();
    tick(3);
    QCOMPARE(sent(Flame), 255);

    allOff();
    tick(3);
    QCOMPARE(sent(Flame), 0);
    QVERIFY(button.state() == VCButton::Inactive);
    bar.m_value = 10;                               // between beats
    bar.checkWidgetFunctionality();
    bar.m_value = 230;                              // the next beat
    bar.checkWidgetFunctionality();
    tick(3);
    QCOMPARE(sent(Flame), 0);
}

/* R6-4. A VC Clock schedule set up before the stop that had not fired yet:
   Function::autoSourceEpoch() makes its FIRST start arm it in the session
   it fires in -- the session All Off just started. E4 says an armed
   schedule from before the stop stays held. */
void ShowOnlyReview5_Test::r6VcClockFirstScheduleAfterStop()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    Scene *fog = new Scene(m_doc);
    fog->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(fog);
    QWidget w;
    VCClock clock(&w, m_doc);
    clock.setID(61);
    m_doc->setMode(Doc::Operate);
    tick(2);
    allOff();
    tick(3);
    QCOMPARE(sent(Flame), 0);
    // 21:00, the schedule's time (VCClock::slotUpdateTime()); nobody touched the clock.
    fog->start(m_doc->masterTimer(), clock.functionParent());
    tick(3);
    QCOMPARE(sent(Flame), 0);
}

/* R6-5. Input passthrough (Universe::setPassthrough): what comes in is
   HTP-merged into the output in updatePostGMValue()/applyPassthrough(),
   past every fader -- no gate, no stamp. In Operate a Show-only channel
   goes out at the input's value, and All Off's cut (cutShowOnly() then
   updatePostGMValue()) merges it straight back. */
void ShowOnlyReview5_Test::r6InputPassthroughThroughStop()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    {
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        ua[0]->setPassthrough(true);
        m_doc->inputOutputMap()->releaseUniverses(false);
    }
    m_doc->setMode(Doc::Operate);
    {
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        ua[0]->slotInputValueChanged(0, Flame, 255, QString());    // a house desk's channel
        m_doc->inputOutputMap()->releaseUniverses(false);
    }
    tick(3);
    allOff();
    tick(3);
    // E1: every Show-only channel at its safe value, at once.
    QCOMPARE(sent(Flame), 0);
}

/* R6-6. A VC playback slider: setPlaybackValue() arms a 5-tick change
   counter, and writeDMXPlayback() starts the function again whenever it
   is stopped while the counter runs. All Off within those ticks of the
   last move: the next tick restarts it as a fresh ManualVCWidget source
   in the new session -- the flame comes back with no hand after the stop. */
void ShowOnlyReview5_Test::r6PlaybackSliderRestartsAfterStop()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    Scene *burst = new Scene(m_doc);
    burst->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(burst);
    QWidget w;
    VCSlider slider(&w, m_doc);
    slider.setID(52);
    slider.setSliderMode(VCSlider::Playback);
    slider.setPlaybackFunction(burst->id());
    m_doc->setMode(Doc::Operate);
    slider.setSliderValue(255, false, true);
    tick(6);
    QCOMPARE(sent(Flame), 255);
    slider.setSliderValue(250, false, true);        // the last move
    allOff();                                       // in the same moment
    int flameMax = 0;
    for (int i = 0; i < 6; i++)
    {
        tick(1);
        flameMax = qMax(flameMax, sent(Flame));
    }
    QCOMPARE(flameMax, 0);
}

/* R6-7 (should drive). A GO in Operate on a Script that starts the fog
   cue and leaves it running (stoponexit false): the cue's source is
   Function(script) since this train (script.cpp handleStartFunction), so
   it follows the Script's gate -- and the Script's gate turns to held the
   moment the Script ends (no source left, E5). The cue keeps running and
   the fogger it was GOne for goes dark. */
void ShowOnlyReview5_Test::r6ScriptLaunchedCueDrives()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    Scene *fog = new Scene(m_doc);
    fog->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(fog);
    Script *script = new Script(m_doc);
    QVERIFY(script->setData(QStringLiteral("stoponexit:false\nstartfunction:%1\n").arg(fog->id())));
    m_doc->addFunction(script);
    m_doc->setMode(Doc::Operate);
    script->start(m_doc->masterTimer(), FunctionParent::master());     // the operator's GO
    tick(10);
    QVERIFY(fog->isRunning());
    QVERIFY(!script->isRunning());
    QCOMPARE(sent(Flame), 255);
}

/* R6-8. A show loaded while the console is in Operate (web access
   /loadProject -> App::slotLoadDocFromMemory(): clearDocument() and
   loadXML(), no mode change, so no new live session): its saved Park is
   set by OutputLayer::set() with this session's stamp -- the show --
   and the flame goes out at its parked level with no operator action. */
void ShowOnlyReview5_Test::r6ParkLoadedInOperateDrives()
{
    Fixture *flame = addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly);
    QVERIFY(flame != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    tick(2);
    const QString xml = QStringLiteral("<Park><PFixture ID=\"%1\"><PChannel Number=\"0\">255</PChannel>"
                                       "</PFixture></Park>").arg(flame->id());
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    QVERIFY(m_doc->programmer()->loadParkXML(r));  // as Doc::loadXML() does
    tick(3);
    QCOMPARE(sent(Flame), 0);
}

/****************************************************************************
 * The sixth review's writers, swept
 ****************************************************************************/

/* R6-1 and R6-3: a real audio triggers widget. Every bar is the widget's
   (never master()), the bars made later too; in Operate with nobody
   touching it a peak drives nothing; switched on from the web or an input
   (toggleEnableButton(), the button already on) it is the show; All Off
   holds it again. */
void ShowOnlyReview5_Test::sweepR6RealAudioWidgetArmsOnlyByTheOperator()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    Scene *burst = new Scene(m_doc);
    burst->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(burst);
    QWidget w;
    VCAudioTriggers *at = new VCAudioTriggers(&w, m_doc);
    at->setID(43);
    at->setSpectrumBarsNumber(at->m_spectrumBars.size() + 2);   // made after the id
    for (const AudioBar *b : at->getAudioBars())
    {
        QCOMPARE(b->functionParent().type(), quint32(FunctionParent::AutoVCWidget));
        QCOMPARE(b->functionParent().id(), quint32(43));
    }
    AudioBar *bar = at->m_spectrumBars.last();
    bar->setType(AudioBar::FunctionBar);
    bar->attachFunction(burst);
    bar->setMinThreshold(20);
    bar->setMaxThreshold(200);
    m_doc->setMode(Doc::Operate);                   // a show opened into Operate
    bar->m_value = 230;
    bar->checkFunctionThresholds(m_doc);
    tick(3);
    QCOMPARE(sent(Flame), 0);                       // never armed: held
    bar->m_value = 0;
    bar->checkFunctionThresholds(m_doc);
    tick(2);

    at->m_button->blockSignals(true);               // on already (no capture in a test)
    at->m_button->setChecked(true);
    at->m_button->blockSignals(false);
    at->toggleEnableButton(true);                   // the web's / an input's "on"
    bar->m_value = 230;
    bar->checkFunctionThresholds(m_doc);
    tick(3);
    QCOMPARE(sent(Flame), 255);

    allOff();
    tick(3);
    QCOMPARE(sent(Flame), 0);
    goUnrelated(60);
    bar->m_value = 230;
    bar->checkFunctionThresholds(m_doc);
    tick(4);
    QCOMPARE(sent(Stage), 60);
    QCOMPARE(sent(Flame), 0);
    at->m_button->blockSignals(true);
    at->m_button->setChecked(false);
    at->m_button->blockSignals(false);
    delete at;
}

/* R6-2: an audio bar stepping a VC cue list is automatic: what the list
   plays takes the audio widget's armed session, never a GO's -- held
   unarmed, the show armed, held after All Off, and no GO noted (the
   footer keeps "emergency stop"). */
void ShowOnlyReview5_Test::sweepR6AudioBarStepsAVcCueList()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    Scene *one = new Scene(m_doc);
    one->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(one);
    Scene *two = new Scene(m_doc);
    two->setValue(m_doc->fixtureForAddress(Flame), 0, 200);
    m_doc->addFunction(two);
    Chaser *list = new Chaser(m_doc);
    ChaserStep s1(one->id());
    s1.hold = 100000;
    list->addStep(s1);
    ChaserStep s2(two->id());
    s2.hold = 100000;
    list->addStep(s2);
    m_doc->addFunction(list);
    QWidget w;
    VCCueList cue(&w, m_doc);
    cue.setID(53);
    cue.setChaser(list->id());
    m_doc->setMode(Doc::Operate);
    AudioBar bar(AudioBar::VCWidgetBar, 0, 79);
    bar.setMinThreshold(20);
    bar.setMaxThreshold(200);
    bar.m_widgetID = cue.id();
    bar.m_widget = &cue;
    auto beat = [&bar]() {
        bar.m_value = 10;
        bar.checkWidgetFunctionality();
        bar.m_value = 230;
        bar.checkWidgetFunctionality();
    };
    beat();                                         // unarmed: starts the list...
    tick(4);
    QVERIFY(list->isRunning());
    QCOMPARE(sent(Flame), 0);                       // ...which drives nothing

    Function::armAutoSource(bar.functionParent());  // the audio widget switched on
    beat();                                         // the next step, armed
    tick(4);
    QVERIFY(sent(Flame) > 0);

    allOff();
    tick(3);
    QCOMPARE(sent(Flame), 0);
    beat();                                         // the music goes on
    tick(4);
    QCOMPARE(sent(Flame), 0);
    QVERIFY(m_doc->outputRuleKeeper()->emergencyStopped());    // no GO came
    goUnrelated(60);
    beat();
    tick(4);
    QCOMPARE(sent(Stage), 60);
    QCOMPARE(sent(Flame), 0);
}

/* R6-3: a VC Clock is armed by the operator working it -- from MIDI/OSC
   or the web (playPauseTimer(), resetTimer()) as much as a touch; never
   by its own first schedule. */
void ShowOnlyReview5_Test::sweepR6VcClockArmedByInputOrWeb()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    Scene *fog = new Scene(m_doc);
    fog->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(fog);
    QWidget w;
    VCClock clock(&w, m_doc);
    clock.setID(62);
    m_doc->setMode(Doc::Operate);
    fog->start(m_doc->masterTimer(), clock.functionParent());     // its first schedule
    tick(3);
    QCOMPARE(sent(Flame), 0);
    fog->stop(clock.functionParent());
    tick(3);

    clock.playPauseTimer();                         // the web's "S", a MIDI play
    fog->start(m_doc->masterTimer(), clock.functionParent());
    tick(3);
    QCOMPARE(sent(Flame), 255);

    allOff();
    tick(3);
    goUnrelated(60);
    fog->start(m_doc->masterTimer(), clock.functionParent());
    tick(4);
    QCOMPARE(sent(Stage), 60);
    QCOMPARE(sent(Flame), 0);
    fog->stop(clock.functionParent());
    tick(3);
    clock.resetTimer();                             // the web's "R", a MIDI reset
    fog->start(m_doc->masterTimer(), clock.functionParent());
    tick(3);
    QCOMPARE(sent(Flame), 255);
}

/* R6-4: input passthrough reaches an ordinary channel as before and never
   a Show-only one -- in Operate, after a GO on it, through All Off. */
void ShowOnlyReview5_Test::sweepR6PassthroughNeverDrives()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    {
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        ua[0]->setPassthrough(true);
        m_doc->inputOutputMap()->releaseUniverses(false);
    }
    m_doc->setMode(Doc::Operate);
    {
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        ua[0]->slotInputValueChanged(0, Flame, 255, QString());
        ua[0]->slotInputValueChanged(0, Stage, 120, QString());
        m_doc->inputOutputMap()->releaseUniverses(false);
    }
    tick(3);
    QCOMPARE(sent(Stage), 120);                     // an ordinary channel: as before
    QCOMPARE(sent(Flame), 0);

    // A GO on the flame drives it -- at its own level, not the input's.
    Scene *fire = new Scene(m_doc);
    fire->setValue(m_doc->fixtureForAddress(Flame), 0, 90);
    m_doc->addFunction(fire);
    Chaser *list = new Chaser(m_doc);
    ChaserStep st(fire->id());
    st.hold = 100000;
    list->addStep(st);
    m_doc->addFunction(list);
    ShowControl::Request go;
    go.command = ShowControl::Go;
    go.chaser = list->id();
    QVERIFY(m_doc->showControl()->run(go, ShowControl::RouteRunTab));
    tick(4);
    QCOMPARE(sent(Flame), 90);

    allOff();
    QCOMPARE(sent(Flame), 0);                       // the cut, at once
    tick(3);
    QCOMPARE(sent(Flame), 0);
    QCOMPARE(sent(Stage), 120);
}

/* R6-5: after the stop the playback slider stays held until it is really
   moved; moved, it drives. */
void ShowOnlyReview5_Test::sweepR6PlaybackSliderMovedAfterTheStop()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    Scene *burst = new Scene(m_doc);
    burst->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(burst);
    QWidget w;
    VCSlider slider(&w, m_doc);
    slider.setID(54);
    slider.setSliderMode(VCSlider::Playback);
    slider.setPlaybackFunction(burst->id());
    m_doc->setMode(Doc::Operate);
    slider.setSliderValue(255, false, true);
    tick(6);
    QCOMPARE(sent(Flame), 255);
    allOff();
    tick(6);
    QCOMPARE(sent(Flame), 0);
    goUnrelated(60);
    tick(4);
    QCOMPARE(sent(Stage), 60);
    QCOMPARE(sent(Flame), 0);
    slider.setSliderValue(200, false, true);        // moved after the stop
    tick(6);
    QVERIFY(sent(Flame) > 0);
}

/* R6-6: the cue a Script launched drives for the session, not past it:
   All Off holds it, and an unrelated GO does not wake it. */
void ShowOnlyReview5_Test::sweepR6ScriptLaunchedCueAfterTheStop()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    Scene *fog = new Scene(m_doc);
    fog->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(fog);
    Script *script = new Script(m_doc);
    QVERIFY(script->setData(QStringLiteral("stoponexit:false\nstartfunction:%1\n").arg(fog->id())));
    m_doc->addFunction(script);
    m_doc->setMode(Doc::Operate);
    script->start(m_doc->masterTimer(), FunctionParent::master());
    tick(10);
    QCOMPARE(sent(Flame), 255);
    allOff();
    tick(3);
    QCOMPARE(sent(Flame), 0);
    goUnrelated(60);
    tick(4);
    QCOMPARE(sent(Stage), 60);
    QCOMPARE(sent(Flame), 0);
    // A Script started in Design launches nothing that drives in Operate.
    m_doc->setMode(Doc::Design);
    tick(2);
    script->start(m_doc->masterTimer(), FunctionParent::master());
    tick(10);
    m_doc->setMode(Doc::Operate);
    tick(3);
    QVERIFY(fog->isRunning());
    QCOMPARE(sent(Flame), 0);
}

/* R6-7: a show loaded in Operate (the web's /loadProject: no mode change)
   starts a new session -- a hand control set before it is held. */
void ShowOnlyReview5_Test::sweepR6ShowLoadedInOperateStartsASession()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    SimpleDeskEngine eng(m_doc);
    eng.setValue(Flame, 200);
    tick(3);
    QCOMPARE(sent(Flame), 200);
    const quint32 before = GenericFader::liveEpoch();
    emit m_doc->loaded();                           // as Doc::loadXML() ends
    QVERIFY(GenericFader::liveEpoch() != before);
    tick(3);
    QCOMPARE(sent(Flame), 0);
    eng.setValue(Flame, 180);                       // a hand on it, after the load
    tick(3);
    QCOMPARE(sent(Flame), 180);
}

/* Re-patching: a Simple Desk channel moved in Operate for what was there
   is no stamp for a Show-only fixture patched onto it -- held until the
   fader is moved again. */
void ShowOnlyReview5_Test::sweepR6DeskStampIsForWhatIsPatched()
{
    Fixture *flame = addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly);
    QVERIFY(flame != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    SimpleDeskEngine eng(m_doc);
    enum { Free = 40 };
    eng.setValue(Free, 150);                        // nothing patched there
    eng.setValue(Stage, 70);
    tick(3);
    QCOMPARE(sent(Free), 150);
    flame->setAddress(Free);                        // the flame moved onto it, mid-show
    tick(3);
    QCOMPARE(sent(Free), 0);
    eng.setValue(Stage, 80);                        // another desk channel: no stamp for it
    tick(3);
    QCOMPARE(sent(Stage), 80);
    QCOMPARE(sent(Free), 0);
    eng.setValue(Free, 160);                        // moved for the flame
    tick(3);
    QCOMPARE(sent(Free), 160);
}

/****************************************************************************
 * Seventh review (of 66445810d0)
 ****************************************************************************/

Chaser *ShowOnlyReview5_Test::listOf(Function *f, const QString &name)
{
    Chaser *list = new Chaser(m_doc);
    list->setName(name);
    ChaserStep s(f->id());
    s.hold = 100000;
    list->addStep(s);
    m_doc->addFunction(list);
    return list;
}

void ShowOnlyReview5_Test::goList(Chaser *list)
{
    ShowControl::Request go;
    go.command = ShowControl::Go;
    go.chaser = list->id();
    QVERIFY(m_doc->showControl()->run(go, ShowControl::RouteRunTab));
}

Fixture *ShowOnlyReview5_Test::addFogger(const QString &name, int address)
{
    /* As the bundled Involight FM-series foggers: one channel, "Smoke",
       group Effect -- an LTP channel. */
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer(QStringLiteral("Review7"));
    def->setModel(QStringLiteral("Fogger %1").arg(name));
    def->setType(QLCFixtureDef::Smoke);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName(QStringLiteral("1ch"));
    QLCChannel *smoke = new QLCChannel();
    smoke->setName(QStringLiteral("Smoke"));
    smoke->setGroup(QLCChannel::Effect);
    def->addChannel(smoke);
    mode->insertChannel(smoke, 0);
    def->addMode(mode);
    if (!m_doc->fixtureDefCache()->addFixtureDef(def))
    {
        delete def;
        return nullptr;
    }
    Fixture *f = new Fixture(m_doc);
    f->setName(name);
    f->setFixtureDefinition(def, def->modes().first());
    f->setUniverse(0);
    f->setAddress(quint32(address));
    f->setOutputRules(Fixture::ShowOnly);
    if (!m_doc->addFixture(f))
    {
        delete f;
        return nullptr;
    }
    return f;
}

/* R7-1. Function::storeGateLocked() latches a parent's stamp whenever the
   parent's gate reads "the show". MasterTimer::refreshPreviewGates() walks
   the functions in list order, so a scene that sits BEFORE one of its
   parents in m_functionList (it was already running when the second list
   took it as a step) reads that parent's gate before the parent has been
   worked out for the new session: still false, so the scene latches the
   NEW session. A laser scene shared by two cue lists, both GO'd in
   Operate; a trip to Design and back; no GO since: the laser fires. */
void ShowOnlyReview5_Test::r7SharedSceneLatchesAcrossADesignTrip()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    Scene *fire = new Scene(m_doc);
    fire->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(fire);
    Chaser *a = listOf(fire, QStringLiteral("A"));
    Chaser *b = listOf(fire, QStringLiteral("B"));
    m_doc->setMode(Doc::Operate);
    goList(a);
    tick(3);
    goList(b);
    tick(3);
    QCOMPARE(fire->sources().size(), 2);
    QCOMPARE(sent(Flame), 255);             // two GOs in Operate: the show

    m_doc->setMode(Doc::Design);            // to fix a cue
    tick(3);
    QCOMPARE(sent(Flame), 0);
    auto dump = [&](const char *when) {
        QString src;
        for (const FunctionParent &p : fire->sources())
            src += QStringLiteral("%1:%2 ").arg(int(p.type())).arg(p.id());
        qDebug() << when << "epoch" << GenericFader::liveEpoch() << "fire gate" << fire->previewGate()->load()
                 << "a gate" << a->previewGate()->load() << "b gate" << b->previewGate()->load()
                 << "a" << a->id() << "b" << b->id() << "fire sources" << src
                 << "list" << m_doc->masterTimer()->m_functionList.indexOf(a)
                 << m_doc->masterTimer()->m_functionList.indexOf(fire)
                 << m_doc->masterTimer()->m_functionList.indexOf(b);
    };
    dump("design");
    m_doc->setMode(Doc::Operate);           // back; nobody presses GO
    dump("operate");
    int flameMax = 0;
    for (int i = 0; i < 5; i++)
    {
        tick(1);
        flameMax = qMax(flameMax, sent(Flame));
    }
    QVERIFY(a->previewGate()->load());      // both lists are held (left from Design)...
    QVERIFY(b->previewGate()->load());
    QCOMPARE(flameMax, 0);                  // ...so the flame must be too
}

/* R7-1, the same from Design only: a scene left on in Design, then a list
   that has it as a step started in Design. Switch to Operate: no GO. */
void ShowOnlyReview5_Test::r7DesignLeftoverLatchesAtOperate()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    Scene *fire = new Scene(m_doc);
    fire->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(fire);
    Chaser *list = listOf(fire, QStringLiteral("Design list"));
    fire->start(m_doc->masterTimer(), FunctionParent::master());   // left on in Design
    tick(2);
    list->start(m_doc->masterTimer(), FunctionParent::master());   // a list run in Design
    tick(3);
    QCOMPARE(fire->sources().size(), 2);
    QCOMPARE(sent(Flame), 0);               // Design: held
    m_doc->setMode(Doc::Operate);
    int flameMax = 0;
    for (int i = 0; i < 5; i++)
    {
        tick(1);
        flameMax = qMax(flameMax, sent(Flame));
    }
    QCOMPARE(flameMax, 0);                  // anything left from Design is held
}

/* R7-2. All Off from another thread (an OSC or MSC plugin): the cut is
   out at once and the function stop is queued to the main thread. In the
   gap, the latch of R7-1 makes a scene shared by two lists the show of the
   NEW session: the frame after the cut has the flame up again. */
void ShowOnlyReview5_Test::r7QueuedAllOffChildLatchesInTheGap()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    Scene *fire = new Scene(m_doc);
    fire->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(fire);
    Chaser *a = listOf(fire, QStringLiteral("A"));
    Chaser *b = listOf(fire, QStringLiteral("B"));
    m_doc->setMode(Doc::Operate);
    goList(a);
    tick(3);
    goList(b);
    tick(3);
    QCOMPARE(sent(Flame), 255);

    QThread *osc = QThread::create([this]() {
        ShowControl::Request off;
        off.command = ShowControl::AllOff;
        m_doc->showControl()->run(off, ShowControl::RouteOsc);
    });
    osc->start();
    QVERIFY(osc->wait(5000));
    delete osc;
    QCOMPARE(sent(Flame), 0);               // E1: the cut, at once
    // One frame before the main thread runs the queued stop (tick() only
    // processes events after the frame is sent).
    tick(1);
    const int gapFrame = sent(Flame);
    tick(3);
    QCOMPARE(sent(Flame), 0);
    QCOMPARE(gapFrame, 0);                  // nothing from before the stop comes back
}

/* R7-3. "A cue letting go of a Show-only fixture cuts it." A fogger whose
   fog channel is LTP (group Effect, as the bundled Involight FM foggers):
   the next GO lets go of its cue, the cue's function is stopped and held
   (E5) -- and the held fader's write is undone back to what the channel
   had, which on an LTP channel is the cue's own level. Nothing ever puts
   it to safe: the fogger keeps fogging for the rest of the show. */
void ShowOnlyReview5_Test::r7LtpChannelKeptWhenItsCueLetsGo()
{
    QVERIFY(addFogger(QStringLiteral("Fogger"), Flame) != nullptr);
    QVERIFY(addDimmer(QStringLiteral("Stage"), Stage, 0) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    Scene *fog = new Scene(m_doc);
    fog->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(fog);
    Scene *stage = new Scene(m_doc);
    stage->setValue(m_doc->fixtureForAddress(Stage), 0, 60);
    m_doc->addFunction(stage);
    Chaser *list = new Chaser(m_doc);
    ChaserStep s1(fog->id());
    s1.hold = 100000;
    list->addStep(s1);
    ChaserStep s2(stage->id());
    s2.hold = 100000;
    list->addStep(s2);
    m_doc->addFunction(list);
    m_doc->setMode(Doc::Operate);
    goList(list);
    tick(5);
    QCOMPARE(sent(Flame), 255);             // cue 1: fog, the show
    goList(list);                           // cue 2: the stage only
    tick(10);
    QCOMPARE(sent(Stage), 60);
    QVERIFY(!fog->isRunning());             // cue 1 let go of the fogger
    QCOMPARE(sent(Flame), 0);               // ...so it is cut
}

/* R7-4 (decision). Blind is listed as held. A GO taken in Blind, in
   Operate, is stamped as the show; Blind only masks the frame. Leaving
   Blind -- no GO -- fires the flame the GO in Blind played. */
void ShowOnlyReview5_Test::r7GoInBlindFiresWhenBlindEnds()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    Scene *fire = new Scene(m_doc);
    fire->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(fire);
    Chaser *list = listOf(fire, QStringLiteral("Rehearse"));
    m_doc->setMode(Doc::Operate);
    m_doc->inputOutputMap()->setOutputInhibited(true);     // Blind
    goList(list);                                           // checked on screen
    tick(3);
    QCOMPARE(sent(Flame), 0);
    m_doc->inputOutputMap()->setOutputInhibited(false);    // out of Blind, no GO
    const int atOnce = sent(Flame);
    tick(3);
    const int after = sent(Flame);
    QCOMPARE(atOnce, 0);
    QCOMPARE(after, 0);
}

/* R7-5 (should drive). A VC button in Flash mode, pressed by hand in
   Operate: Scene::flash() makes its fader in Scene::writeDMX(), a DMX
   source written outside any CreationScope -- no gate, no stamp, held. */
void ShowOnlyReview5_Test::r7VcButtonFlashIsHeld()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    Scene *burst = new Scene(m_doc);
    burst->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(burst);
    QWidget w;
    VCButton button(&w, m_doc);
    button.setID(61);
    button.setFunction(burst->id());
    button.setAction(VCButton::Flash);
    m_doc->setMode(Doc::Operate);
    button.pressFunction();                 // a hand on the flash button
    tick(3);
    QVERIFY(burst->flashing());
    const int flashed = sent(Flame);
    button.releaseFunction();
    tick(3);
    QCOMPARE(flashed, 255);
}

/* R7-6 (the noted limit). A Simple Desk cue stack stepped by hand in
   Operate plays on a free address; a Show-only fixture is patched onto
   that address mid-show. The cue stack's stamp is whole-fader (CueStack::
   write()), never dropped by Universe::clearShowOnlyStamps(): the flame
   fires at once. */
void ShowOnlyReview5_Test::r7CueStackStampSurvivesARepatch()
{
    Fixture *flame = addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly);
    QVERIFY(flame != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    enum { Free = 40 };
    SimpleDeskEngine eng(m_doc);
    CueStack *cs = eng.cueStack(0);
    QVERIFY(cs != nullptr);
    Cue cue(QStringLiteral("haze"));
    cue.setValue(Free, 150);
    cs->appendCue(cue);
    cs->nextCue();                          // stepped by hand: the show
    tick(10);
    QCOMPARE(sent(Free), 150);
    flame->setAddress(Free);                // the flame re-patched onto it, mid-show
    tick(5);
    const int repatched = sent(Free);
    cs->stop();
    tick(3);
    eng.clearContents();
    tick(2);
    QCOMPARE(repatched, 0);
}

/* R7-7. A Show Tree cue list set to follow timecode (saved with the show:
   FollowTimecode="1") GOes its cues as timecode reaches their times
   (TimecodeCues::slotTime() -> ShowControl::run(RouteTimecode)), with no
   StampScope: each GO is stamped as a hand's GO in the live session. The
   rule: "timecode, only if armed in Operate". Nothing armed it; Operate;
   the band's playback rolls timecode: the flame cue fires. */
void ShowOnlyReview5_Test::r7TimecodeCueListFiresUnarmed()
{
    QVERIFY(addDimmer(QStringLiteral("Flame"), Flame, Fixture::ShowOnly) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    Scene *fire = new Scene(m_doc);
    fire->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(fire);
    m_doc->timecodeSource();                    // makes TimecodeCues
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction(QStringLiteral("P"));
    const quint32 list = st->addCueList(prod, QStringLiteral("Song"));
    const quint32 cue = st->addCue(list, -1, QStringLiteral("Flame hit"));
    QVERIFY(st->setCueTimecode(cue, 1000));
    QVERIFY(st->setListFollowTimecode(list, true));   // as loaded with the show
    QCoreApplication::processEvents();
    Chaser *ch = qobject_cast<Chaser *>(m_doc->function(st->cueList(list)->chaser));
    QVERIFY(ch != nullptr);
    QVERIFY(ch->stepsCount() >= 1);
    Collection *coll = qobject_cast<Collection *>(m_doc->function(ch->steps().at(0).fid));
    QVERIFY(coll != nullptr);
    coll->addFunction(fire->id());              // the cue's look lights the flame

    m_doc->setMode(Doc::Operate);
    tick(2);
    m_doc->timecodeSource()->updateTimeCode(0, 0, 30);
    m_doc->timecodeSource()->updateTimeCode(0, 500, 30);
    m_doc->timecodeSource()->updateTimeCode(0, 1000, 30);   // the cue's time
    tick(5);
    QVERIFY(ch->isRunning());                   // timecode GOed it
    QVERIFY(coll->functions().contains(fire->id()));
    QVERIFY(fire->isRunning());
    QCOMPARE(sent(Flame), 0);                   // never armed in Operate: held
    st->setListFollowTimecode(list, false);
}

/* R7-8. A show loaded while in Operate (web /loadProject: no mode change):
   Doc::loaded starts the session at the end of Doc::loadXML(), and the
   Virtual Console is loaded AFTER it (App::loadXML()). VCXYPad::loadXML()
   ends with m_area->setPosition(saved), whose positionChanged() reaches
   slotPositionChanged() in Operate with a new point: stamped as a hand's
   move in the new session -- the pad drives a Show-only mover's pan and
   tilt with nobody touching it. */
void ShowOnlyReview5_Test::r7XYPadLoadedInOperateIsStamped()
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer(QStringLiteral("Review7"));
    def->setModel(QStringLiteral("Laser mover"));
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName(QStringLiteral("2ch"));
    QLCChannel *pan = new QLCChannel();
    pan->setName(QStringLiteral("Pan"));
    pan->setGroup(QLCChannel::Pan);
    pan->setControlByte(QLCChannel::MSB);
    def->addChannel(pan);
    mode->insertChannel(pan, 0);
    QLCChannel *tilt = new QLCChannel();
    tilt->setName(QStringLiteral("Tilt"));
    tilt->setGroup(QLCChannel::Tilt);
    tilt->setControlByte(QLCChannel::MSB);
    def->addChannel(tilt);
    mode->insertChannel(tilt, 1);
    def->addMode(mode);
    mode->cacheHeads();
    QVERIFY(m_doc->fixtureDefCache()->addFixtureDef(def));
    Fixture *mover = new Fixture(m_doc);
    mover->setName(QStringLiteral("Mover"));
    mover->setFixtureDefinition(def, mode);
    mover->setUniverse(0);
    mover->setAddress(30);
    mover->setOutputRules(Fixture::ShowOnly);
    QVERIFY(m_doc->addFixture(mover));
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    tick(2);

    emit m_doc->loaded();                       // the show loaded in Operate: a new session
    QWidget w;
    VCXYPad pad(&w, m_doc);                     // the VC loads after it
    VCXYPadFixture fx(m_doc);
    fx.setHead(GroupHead(mover->id(), 0));
    pad.appendFixture(fx);
    for (VCXYPadFixture &f : pad.m_fixtures)
        f.arm();
    pad.m_area->setPosition(QPointF(220, 40));  // as VCXYPad::loadXML() ends
    for (int i = 0; i < 3; i++)
    {
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        pad.writeDMX(m_doc->masterTimer(), ua);
        m_doc->inputOutputMap()->releaseUniverses(false);
        tick(1);
    }
    QCOMPARE(sent(30), 0);                      // nobody moved it: held at safe
}

/* R7-9. The cut (E1) resets the stored values; the functions' gates flip
   only when the MasterTimer refreshes them (or the stop reaches each
   function). The universe threads run processFaders() AFTER the
   MasterTimer's tick (Universe::run(), woken by tick()): a frame whose
   tick refreshed the gates before the stop is processed after the cut,
   with the gates still "the show" -- it writes the cue back. From the
   next tick the function is held, and holding means "undo to what the
   channel had": on an LTP channel, the value that frame just wrote. The
   stop then lets go, and LTP keeps it. All Off left the fogger on. (MSC
   All Off: the stop is queued; on the main thread the same window is the
   cut -> the per-function stop loop.) */
void ShowOnlyReview5_Test::r7AllOffRaceLeavesAnLtpChannelUp()
{
    QVERIFY(addFogger(QStringLiteral("Fogger"), Flame) != nullptr);
    m_doc->outputRuleKeeper()->rebuild();
    Scene *fog = new Scene(m_doc);
    fog->setValue(m_doc->fixtureForAddress(Flame), 0, 255);
    m_doc->addFunction(fog);
    Chaser *list = listOf(fog, QStringLiteral("Fog"));
    m_doc->setMode(Doc::Operate);
    goList(list);
    tick(5);
    QCOMPARE(sent(Flame), 255);

    // The MasterTimer has ticked (gates refreshed) and woken the universe
    // threads; MSC's All Off lands before they process that frame.
    QThread *msc = QThread::create([this]() {
        ShowControl::Request off;
        off.command = ShowControl::AllOff;
        m_doc->showControl()->run(off, ShowControl::RouteMsc);
    });
    msc->start();
    QVERIFY(msc->wait(5000));
    delete msc;
    QCOMPARE(sent(Flame), 0);               // the cut
    {
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        for (Universe *u : ua)
            u->processFaders();             // the woken universe thread's frame
        m_doc->inputOutputMap()->releaseUniverses(false);
    }
    const int raced = sent(Flame);
    tick(10);                               // the queued stop runs; ticks go on
    QVERIFY(!fog->isRunning());
    const int after = sent(Flame);
    qDebug() << "frame after the cut:" << raced << "ten frames later:" << after;
    QCOMPARE(after, 0);                     // All Off: out, and stays out
    QCOMPARE(raced, 0);
}

QTEST_MAIN(ShowOnlyReview5_Test)
