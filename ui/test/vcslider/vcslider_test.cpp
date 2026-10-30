/*
  Q Light Controller Plus - Unit test
  vcslider_test.cpp

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
#include <QComboBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QtTest>

#define protected public
#define private public
#include "vcsliderproperties.h"
#include "vcspeeddial.h"
#include "virtualconsole.h"
#include "speedmasters.h"
#include "speeddial.h"
#include "vcslider.h"
#include "doc.h"
#include "mastertimer.h"
#undef private

#include "effectscriptrunner.h"
#include "inputoutputmap.h"
#include "vceffectcontrol.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "universe.h"
#include "fixture.h"
#include "scene.h"
#include "efx.h"
#include "efxfixture.h"
#undef protected

#include "vcslider_test.h"

namespace
{
/* Its speed shows on the dimmer: speed / 4, so a live value is seen on
   the output. One with a list param, one with neither. */
const char *kScripts[][2] = {
    { "speedA.js",
      "(function() { var e = {}; e.apiVersion = 1; e.name = 'T Speed A';"
      " e.fixtureTypes = ['dimmer'];"
      " e.parameters = [ { name: 'speed', min: 0, max: 4, defaultValue: 1 },"
      "                  { name: 'direction', defaultValue: 0, values: ['Left', 'Right', 'Out'] } ];"
      " e.tick = function(fx, inp, pal, par) { return fx.map(function() { return { dimmer: par.speed / 4 }; }); };"
      " return e; })()" },
    { "speedB.js",
      "(function() { var e = {}; e.apiVersion = 1; e.name = 'T Speed B';"
      " e.fixtureTypes = ['dimmer'];"
      " e.parameters = [ { name: 'speed', min: 0, max: 2, defaultValue: 1 } ];"
      " e.tick = function(fx, inp, pal, par) { return fx.map(function() { return { dimmer: par.speed / 2 }; }); };"
      " return e; })()" },
};
}

void VCSlider_Test::init()
{
    m_doc = new Doc(this);
    new VirtualConsole(NULL, m_doc);
    // Queued, as across the app's MasterTimer thread: a direct stop here
    // would ask for the universes the tick that stops it holds.
    MasterTimer *mt = m_doc->masterTimer();
    EffectScriptRunner *runner = m_doc->effectScriptRunner();
    QObject::disconnect(mt, SIGNAL(functionStarted(quint32)), runner, SLOT(slotFunctionStarted(quint32)));
    QObject::disconnect(mt, SIGNAL(functionStopped(quint32)), runner, SLOT(slotFunctionStopped(quint32)));
    QObject::connect(mt, SIGNAL(functionStarted(quint32)), runner, SLOT(slotFunctionStarted(quint32)),
                     Qt::QueuedConnection);
    QObject::connect(mt, SIGNAL(functionStopped(quint32)), runner, SLOT(slotFunctionStopped(quint32)),
                     Qt::QueuedConnection);
    if (!QFile::exists(m_scripts.filePath("speedA.js")))
        for (const auto &sc : kScripts)
        {
            QFile f(m_scripts.filePath(QString::fromLatin1(sc[0])));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(sc[1]);
        }
}

void VCSlider_Test::cleanup()
{
    // What plays stops first, so the runner lets go of the universes.
    for (Function *f : m_doc->functions())
        if (f->isRunning())
            f->stop(FunctionParent::master());
    tick(3);
    delete VirtualConsole::instance();
    delete m_doc;
    m_doc = nullptr;
}

quint32 VCSlider_Test::effectPalette(const QString &script, const QString &name)
{
    QLCPalette *p = new QLCPalette(QLCPalette::Effect, m_doc);
    p->setScriptPath(m_scripts.filePath(script));
    p->setName(name);
    m_doc->addPalette(p);
    return p->id();
}

quint32 VCSlider_Test::play(quint32 palette, quint32 fixture)
{
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixture(fixture);
    scene->addPalette(palette);
    scene->start(m_doc->masterTimer(), FunctionParent::master());
    tick();
    return scene->id();
}

void VCSlider_Test::tick(int times)
{
    for (int i = 0; i < times; ++i)
    {
        m_doc->masterTimer()->timerTick();
        QTest::qWait(25);                   // the effect runner ticks on its own timer
    }
}

int VCSlider_Test::dimmerAt(int address)
{
    tick();
    QList<Universe *> unis = m_doc->inputOutputMap()->claimUniverses();
    for (Universe *u : unis)
        u->processFaders();
    const int v = uchar(unis.first()->preGMValues().at(address));
    m_doc->inputOutputMap()->releaseUniverses(false);
    return v;
}

/* Rate: the middle is 100 %, the top double, the bottom stopped -- as a
   controller's Rate fader; external input moves it the same way. */
void VCSlider_Test::speedMasterRate()
{
    QWidget w;
    SpeedMasters *sm = m_doc->speedMasters();
    QVERIFY(!sm->isUsed(1));
    VCSlider slider(&w, m_doc);
    slider.setSpeedMaster(1);
    slider.setSliderMode(VCSlider::SpeedMaster);
    QVERIFY(sm->isUsed(1));                         // a widget drives it: used
    QCOMPARE(slider.sliderValue(), 128);             // rate 1.0
    QCOMPARE(slider.topLabelText(), QString("100 %"));

    slider.setSliderValue(255, false);
    QCOMPARE(sm->rate(1), 2.0);
    QCOMPARE(sm->rate(0), 1.0);
    slider.setSliderValue(0, false);
    QCOMPARE(sm->rate(1), 0.0);
    slider.setSliderValue(128, false);               // near the middle: 100 %
    QCOMPARE(sm->rate(1), 1.0);

    // External input (MIDI/OSC): the same path, scaled from 0..255.
    slider.setSliderValue(255, true, true);
    QCOMPARE(sm->rate(1), 2.0);

    // Not a DMX source: nothing to do per tick.
    m_doc->setMode(Doc::Operate);
    QVERIFY(!m_doc->masterTimer()->m_dmxSourceList.contains(&slider));
    m_doc->setMode(Doc::Design);

    slider.setSliderMode(VCSlider::Level);
    QVERIFY(!sm->isUsed(1));
}

void VCSlider_Test::speedMasterTempo()
{
    QWidget w;
    SpeedMasters *sm = m_doc->speedMasters();
    VCSlider slider(&w, m_doc);
    slider.setSpeedControl(VCSlider::SpeedTempo);
    slider.setTempoRange(100, 200);
    slider.setSliderMode(VCSlider::SpeedMaster);
    QCOMPARE(slider.speedMaster(), 0);
    QCOMPARE(slider.sliderValue(), 51);              // 120 of 100..200
    QCOMPARE(slider.topLabelText(), QString("120 BPM"));

    slider.setSliderValue(255, false);
    QCOMPARE(sm->setBpm(0), 200.0);
    slider.setSliderValue(0, false);
    QCOMPARE(sm->setBpm(0), 100.0);
    QCOMPARE(slider.topLabelText(), QString("100 BPM"));

    // A master following another has no tempo of its own: n/a, untouched.
    QVERIFY(sm->setFollow(0, SpeedMasters::MainClock, 2.0));
    QCOMPARE(slider.topLabelText(), QString("n/a"));
    slider.setSliderValue(255, false);
    QCOMPARE(sm->setBpm(0), 100.0);
}

/* The master moved elsewhere (a tap, the window, a controller): the
   slider shows it. */
void VCSlider_Test::speedMasterFollowsTheMaster()
{
    QWidget w;
    SpeedMasters *sm = m_doc->speedMasters();
    VCSlider slider(&w, m_doc);
    slider.setSpeedMaster(2);
    slider.setSliderMode(VCSlider::SpeedMaster);
    sm->setRate(2, 0.5);
    QCOMPARE(slider.sliderValue(), 64);
    QCOMPARE(slider.topLabelText(), QString("50 %"));
    sm->setRate(1, 2.0);                             // another master: no change
    QCOMPARE(slider.sliderValue(), 64);
}

void VCSlider_Test::speedMasterSavesAndLoads()
{
    QWidget w;
    VCSlider slider(&w, m_doc);
    slider.setSpeedMaster(3);
    slider.setSpeedControl(VCSlider::SpeedTempo);
    slider.setTempoRange(80, 160);
    slider.setSliderMode(VCSlider::SpeedMaster);
    m_doc->speedMasters()->setBpm(3, 140);

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);
    xmlWriter.setAutoFormatting(true);              // as a show file is written
    xmlWriter.writeStartDocument();
    QVERIFY(slider.saveXML(&xmlWriter));
    xmlWriter.writeEndDocument();
    xmlWriter.setDevice(NULL);
    buffer.close();
    QVERIFY(buffer.buffer().contains("<SliderMode"));
    QVERIFY(buffer.buffer().contains(">SpeedMaster</SliderMode>"));
    QVERIFY(buffer.buffer().contains("<SpeedMaster Index=\"4\" Control=\"Tempo\" MinBPM=\"80\" MaxBPM=\"160\"/>"));

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();
    VCSlider loaded(&w, m_doc);
    QVERIFY(loaded.loadXML(xmlReader));
    QCOMPARE(loaded.sliderMode(), VCSlider::SpeedMaster);
    QCOMPARE(loaded.speedMaster(), 3);
    QCOMPARE(loaded.speedControl(), VCSlider::SpeedTempo);
    QCOMPARE(loaded.tempoMin(), 80);
    QCOMPARE(loaded.tempoMax(), 160);
    // Loading shows the master; it doesn't set it.
    QCOMPARE(m_doc->speedMasters()->setBpm(3), 140.0);
    QCOMPARE(loaded.topLabelText(), QString("140 BPM"));

    // A Level slider writes no SpeedMaster element: old shows as they were.
    VCSlider level(&w, m_doc);
    QBuffer b2;
    b2.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter w2(&b2);
    QVERIFY(level.saveXML(&w2));
    w2.setDevice(NULL);
    QVERIFY(!b2.buffer().contains("<SpeedMaster"));
}

void VCSlider_Test::speedMasterProperties()
{
    QWidget w;
    VCSlider slider(&w, m_doc);
    {
        VCSliderProperties props(&slider, m_doc);
        QVERIFY(props.m_switchToSpeedMasterModeButton != nullptr);
        props.slotModeSpeedMasterClicked();
        props.m_speedMasterCombo->setCurrentIndex(props.m_speedMasterCombo->findData(2));
        props.m_speedTempoRadio->setChecked(true);
        props.m_tempoMinSpin->setValue(90);
        props.m_tempoMaxSpin->setValue(150);
        props.accept();
    }
    QCOMPARE(slider.sliderMode(), VCSlider::SpeedMaster);
    QCOMPARE(slider.speedMaster(), 2);
    QCOMPARE(slider.speedControl(), VCSlider::SpeedTempo);
    QCOMPARE(slider.tempoMin(), 90);
    QCOMPARE(slider.tempoMax(), 150);

    // A Grand Master slider (the programmer frame's) stays one when its
    // properties are opened and closed.
    VCSlider gm(&w, m_doc);
    gm.setSliderMode(VCSlider::GrandMaster);
    {
        VCSliderProperties props(&gm, m_doc);
        props.accept();
    }
    QCOMPARE(gm.sliderMode(), VCSlider::GrandMaster);
}

/* A Speed Dial on a master: its time is one beat, a tap taps the master,
   and it shows the master's tempo set elsewhere. Saved with the show. */
void VCSlider_Test::speedDialSetsAMaster()
{
    QWidget w;
    SpeedMasters *sm = m_doc->speedMasters();
    VCSpeedDial dial(&w, m_doc);
    dial.setSpeedMaster(1);
    QVERIFY(sm->isUsed(1));
    QCOMPARE(dial.m_dial->value(), 500);             // 120 BPM

    m_doc->setMode(Doc::Operate);
    dial.m_dial->setValue(400, true);                // 150 BPM
    QCOMPARE(sm->setBpm(1), 150.0);
    sm->setBpm(1, 100);                              // set elsewhere
    QCOMPARE(dial.m_dial->value(), 600);
    m_doc->setMode(Doc::Design);

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);
    xmlWriter.writeStartDocument();
    QVERIFY(dial.saveXML(&xmlWriter));
    xmlWriter.writeEndDocument();
    xmlWriter.setDevice(NULL);
    buffer.close();
    QVERIFY(buffer.buffer().contains("<SpeedMaster Index=\"2\"/>"));

    sm->setBpm(1, 90);
    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();
    VCSpeedDial loaded(&w, m_doc);
    QVERIFY(loaded.loadXML(xmlReader));
    QCOMPARE(loaded.speedMaster(), 1);
    QCOMPARE(sm->setBpm(1), 90.0);                   // loading doesn't set it

    dial.setSpeedMaster(-1);
    loaded.setSpeedMaster(-1);
    QVERIFY(!sm->isUsed(1));
}

/* "The current effect": a fader on Speed moves effect A's, live, and when
   B starts it moves B's. The palette is left alone (a live override, gone
   when the effect stops) until Store. */
void VCSlider_Test::effectControlFollowsTheCurrentEffect()
{
    QWidget w;
    Fixture *fa = new Fixture(m_doc);
    fa->setChannels(1);
    fa->setAddress(0);
    m_doc->addFixture(fa);
    Fixture *fb = new Fixture(m_doc);
    fb->setChannels(1);
    fb->setAddress(1);
    m_doc->addFixture(fb);
    const quint32 a = effectPalette("speedA.js", "Wave A");
    const quint32 b = effectPalette("speedB.js", "Wave B");
    EffectScriptRunner *runner = m_doc->effectScriptRunner();

    m_doc->setMode(Doc::Operate);
    VCSlider slider(&w, m_doc);
    VCEffectControl ec;
    ec.target = VCEffectControl::TargetCurrent;
    ec.role = VCEffectControl::Speed;
    slider.setEffectControl(ec);
    slider.setSliderMode(VCSlider::EffectControl);
    QCOMPARE(slider.topLabelText(), QString("no effect"));
    QVERIFY(slider.m_storeButton != nullptr);

    // Not a DMX source: nothing per tick.
    QVERIFY(!m_doc->masterTimer()->m_dmxSourceList.contains(&slider));

    const quint32 sa = play(a, fa->id());
    QVERIFY(runner->isPlaying(a));
    QCOMPARE(slider.effectTarget().id, a);
    QCOMPARE(slider.bottomLabelText(), QString("Wave A · Speed"));
    QCOMPARE(slider.topLabelText(), QString("1.00"));
    QCOMPARE(slider.sliderValue(), 64);              // 1 of 0..4
    QCOMPARE(dimmerAt(0), 64);

    slider.setSliderValue(255, false);
    QCOMPARE(runner->paramValue(a, "speed"), 4.0);
    QCOMPARE(slider.topLabelText(), QString("4.00"));
    QVERIFY(!m_doc->palette(a)->effectParamValues().contains("speed"));   // the palette as it was
    QCOMPARE(dimmerAt(0), 255);

    // B starts: the same fader now moves B.
    play(b, fb->id());
    QCOMPARE(slider.effectTarget().id, b);
    QCOMPARE(slider.bottomLabelText(), QString("Wave B · Speed"));
    QCOMPARE(slider.sliderValue(), 128);             // 1 of 0..2
    slider.setSliderValue(0, true, true);            // external input, the same way
    QCOMPARE(runner->paramValue(b, "speed"), 0.0);
    QCOMPARE(runner->paramValue(a, "speed"), 4.0);   // A keeps its live value while it plays
    QCOMPARE(dimmerAt(1), 0);

    // Store keeps B's value in its palette.
    QVERIFY(slider.storeEffectControl());
    QCOMPARE(m_doc->palette(b)->effectParamValues().value("speed"), 0.0);
    QVERIFY(runner->paramOverrides(b).isEmpty());

    // A stops: its live value is gone; started again, it plays as stored.
    m_doc->function(sa)->stop(FunctionParent::master());
    tick();
    QVERIFY(!runner->isPlaying(a));
    QVERIFY(runner->paramOverrides(a).isEmpty());
    m_doc->function(sa)->start(m_doc->masterTimer(), FunctionParent::master());
    tick();
    QCOMPARE(runner->paramValue(a, "speed"), 1.0);
    QCOMPARE(slider.effectTarget().id, a);           // started last: current again

    m_doc->setMode(Doc::Design);
}

/* A palette, the effect on a group, the current one on a group; a role the
   effect hasn't reads n/a; a list param steps through its choices. */
void VCSlider_Test::effectControlTargets()
{
    QWidget w;
    Fixture *fa = new Fixture(m_doc);
    fa->setChannels(1);
    m_doc->addFixture(fa);
    Fixture *fb = new Fixture(m_doc);
    fb->setChannels(1);
    fb->setAddress(1);
    m_doc->addFixture(fb);
    FixtureGroup *ga = new FixtureGroup(m_doc);
    ga->setName("Towers");
    m_doc->addFixtureGroup(ga);
    ga->assignFixture(fa->id());
    const quint32 a = effectPalette("speedA.js", "Wave A");
    const quint32 b = effectPalette("speedB.js", "Wave B");
    EffectScriptRunner *runner = m_doc->effectScriptRunner();
    m_doc->setMode(Doc::Operate);

    VCSlider onGroup(&w, m_doc);
    VCEffectControl ec;
    ec.target = VCEffectControl::TargetGroup;
    ec.group = ga->id();
    onGroup.setEffectControl(ec);
    onGroup.setSliderMode(VCSlider::EffectControl);

    VCSlider onPalette(&w, m_doc);
    ec = VCEffectControl();
    ec.target = VCEffectControl::TargetPalette;
    ec.palette = b;
    onPalette.setEffectControl(ec);
    onPalette.setSliderMode(VCSlider::EffectControl);
    QCOMPARE(onPalette.topLabelText(), QString("not playing"));

    VCSlider direction(&w, m_doc);
    ec = VCEffectControl();
    ec.role = VCEffectControl::Direction;
    direction.setEffectControl(ec);
    direction.setSliderMode(VCSlider::EffectControl);

    VCSlider size(&w, m_doc);
    ec = VCEffectControl();
    ec.role = VCEffectControl::Size;
    size.setEffectControl(ec);
    size.setSliderMode(VCSlider::EffectControl);

    play(a, fa->id());
    play(b, fb->id());
    QCOMPARE(onGroup.effectTarget().id, a);          // B is newer, but not on Towers
    QCOMPARE(onPalette.effectTarget().id, b);
    QCOMPARE(onPalette.topLabelText(), QString("1.00"));
    QCOMPARE(size.effectTarget().id, b);
    QCOMPARE(size.topLabelText(), QString("n/a"));
    QVERIFY(!VCEffectControl::apply(m_doc, size.effectTarget(), VCEffectControl::Size, 200));

    // Selected in Build: current, though it started first.
    VCEffectTracker::instance(m_doc)->slotPaletteSelected(a);
    QCOMPARE(direction.effectTarget().id, a);
    QCOMPARE(direction.topLabelText(), QString("Left"));
    direction.setSliderValue(130, false);
    QCOMPARE(runner->paramValue(a, "direction"), 1.0);
    QCOMPARE(direction.topLabelText(), QString("Right"));
    direction.setSliderValue(255, false);
    QCOMPARE(direction.topLabelText(), QString("Out"));
    QCOMPARE(onGroup.effectTarget().id, a);
    // A group target goes by what started, not what was selected.
    QCOMPARE(size.effectTarget().id, a);

    // Colour 1 hue: a live value on any effect.
    VCEffectControl hue;
    hue.target = VCEffectControl::TargetPalette;
    hue.palette = b;
    hue.role = VCEffectControl::Hue;
    const VCEffectControl::Resolved r = hue.resolve(m_doc);
    QVERIFY(r.available);
    QVERIFY(VCEffectControl::apply(m_doc, r, VCEffectControl::Hue, 85));
    QCOMPARE(VCEffectControl::valueText(m_doc, r, VCEffectControl::Hue), QString("120°"));
    m_doc->setMode(Doc::Design);
}

void VCSlider_Test::effectControlSavesAndLoads()
{
    QWidget w;
    VCSlider slider(&w, m_doc);
    VCEffectControl ec;
    ec.target = VCEffectControl::TargetGroup;
    ec.group = 7;
    ec.role = VCEffectControl::Spread;
    slider.setEffectControl(ec);
    slider.setWidgetStyle(VCSlider::WKnob);
    slider.setSliderMode(VCSlider::EffectControl);

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);
    xmlWriter.setAutoFormatting(true);
    xmlWriter.writeStartDocument();
    QVERIFY(slider.saveXML(&xmlWriter));
    xmlWriter.writeEndDocument();
    xmlWriter.setDevice(NULL);
    buffer.close();
    QVERIFY(buffer.buffer().contains(">Effect</SliderMode>"));
    QVERIFY(buffer.buffer().contains("<EffectControl Target=\"Group\" Group=\"7\" Role=\"Spread\"/>"));

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();
    VCSlider loaded(&w, m_doc);
    QVERIFY(loaded.loadXML(xmlReader));
    QCOMPARE(loaded.sliderMode(), VCSlider::EffectControl);
    QVERIFY(loaded.effectControl() == ec);
    QCOMPARE(loaded.widgetStyle(), VCSlider::WKnob);

    // Another mode writes none: older shows as they were.
    VCSlider level(&w, m_doc);
    QBuffer b2;
    b2.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter w2(&b2);
    QVERIFY(level.saveXML(&w2));
    w2.setDevice(NULL);
    QVERIFY(!b2.buffer().contains("<EffectControl"));
}

void VCSlider_Test::effectControlProperties()
{
    QWidget w;
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName("Towers");
    m_doc->addFixtureGroup(g);
    const quint32 a = effectPalette("speedA.js", "Wave A");
    VCSlider slider(&w, m_doc);
    {
        VCSliderProperties props(&slider, m_doc);
        props.slotModeEffectClicked();
        props.m_effectPaletteRadio->setChecked(true);
        props.m_effectPaletteCombo->setCurrentIndex(props.m_effectPaletteCombo->findData(a));
        props.m_effectRoleCombo->setCurrentIndex(props.m_effectRoleCombo->findData(int(VCEffectControl::Intensity)));
        props.accept();
    }
    QCOMPARE(slider.sliderMode(), VCSlider::EffectControl);
    QCOMPARE(slider.effectControl().target, VCEffectControl::TargetPalette);
    QCOMPARE(slider.effectControl().palette, a);
    QCOMPARE(slider.effectControl().role, VCEffectControl::Intensity);
    {
        VCSliderProperties props(&slider, m_doc);
        QVERIFY(props.m_effectPaletteRadio->isChecked());
        props.m_effectCurrentRadio->setChecked(true);
        props.m_effectGroupCombo->setCurrentIndex(props.m_effectGroupCombo->findData(QVariant::fromValue(g->id())));
        props.accept();
    }
    QCOMPARE(slider.effectControl().target, VCEffectControl::TargetCurrent);
    QCOMPARE(slider.effectControl().group, g->id());
}

/* An EFX started is the current effect too: Speed scales its duration
   (the middle as it was), Direction turns it; both put back when it stops,
   unless stored. */
void VCSlider_Test::effectControlOnAnEfx()
{
    QWidget w;
    EFX *efx = new EFX(m_doc);
    efx->setName("Circle");
    efx->setDuration(2000);
    m_doc->addFunction(efx);
    Fixture *fx = new Fixture(m_doc);
    fx->setChannels(2);
    m_doc->addFixture(fx);
    EFXFixture *ef = new EFXFixture(efx);
    ef->setHead(GroupHead(fx->id(), 0));
    efx->addFixture(ef);
    m_doc->setMode(Doc::Operate);

    VCSlider speed(&w, m_doc);
    speed.setEffectControl(VCEffectControl());
    speed.setSliderMode(VCSlider::EffectControl);
    VCSlider direction(&w, m_doc);
    VCEffectControl dc;
    dc.role = VCEffectControl::Direction;
    direction.setEffectControl(dc);
    direction.setSliderMode(VCSlider::EffectControl);
    VCSlider rate(&w, m_doc);
    VCEffectControl rc;
    rc.role = VCEffectControl::Rate;
    rate.setEffectControl(rc);
    rate.setSliderMode(VCSlider::EffectControl);

    efx->start(m_doc->masterTimer(), FunctionParent::master());
    tick(2);
    QCOMPARE(speed.effectTarget().kind, VCEffectControl::Resolved::EFX);
    QCOMPARE(speed.bottomLabelText(), QString("Circle · Speed"));
    QCOMPARE(speed.sliderValue(), 128);
    QCOMPARE(speed.topLabelText(), QString("×1.00"));
    QCOMPARE(rate.topLabelText(), QString("n/a"));

    speed.setSliderValue(255, false);                // ×4 faster
    QCOMPARE(efx->duration(), 500u);
    direction.setSliderValue(255, false);
    QCOMPARE(efx->direction(), Function::Backward);
    QCOMPARE(direction.topLabelText(), QString("Backward"));

    efx->stop(FunctionParent::master());
    tick(2);
    QCOMPARE(efx->duration(), 2000u);                // put back
    QCOMPARE(efx->direction(), Function::Forward);
    QCOMPARE(speed.topLabelText(), QString("no effect"));

    // Stored: kept after it stops.
    efx->start(m_doc->masterTimer(), FunctionParent::master());
    tick(2);
    speed.setSliderValue(0, false);                  // ×¼
    QCOMPARE(efx->duration(), 8000u);
    QVERIFY(speed.storeEffectControl());
    efx->stop(FunctionParent::master());
    tick(2);
    QCOMPARE(efx->duration(), 8000u);
    m_doc->setMode(Doc::Design);
}

QTEST_MAIN(VCSlider_Test)
