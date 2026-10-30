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
#undef private
#undef protected

#include "vcslider_test.h"

void VCSlider_Test::init()
{
    m_doc = new Doc(this);
    new VirtualConsole(NULL, m_doc);
}

void VCSlider_Test::cleanup()
{
    delete VirtualConsole::instance();
    delete m_doc;
    m_doc = nullptr;
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

QTEST_MAIN(VCSlider_Test)
