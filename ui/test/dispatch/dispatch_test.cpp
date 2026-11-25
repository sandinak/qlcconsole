/*
  Q Light Controller Plus - Unit test
  dispatch_test.cpp

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

#include <QSignalSpy>
#include <QSettings>
#include <QDialog>
#include <QTimer>
#include <QtTest>
#include <QDir>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#include "mastertimer.h"
#undef private

#include "dispatch_test.h"
#include "grandmasterslider.h"
#include "functionparent.h"
#include "inputoutputmap.h"
#include "virtualconsole.h"
#include "ioplugincache.h"
#include "channelsgroup.h"
#include "iopluginstub.h"
#include "qlcinputsource.h"
#include "showfeatures.h"
#include "speedmasters.h"
#include "encoderbank.h"
#include "deskrecord.h"
#include "controlmap.h"
#include "qlcioplugin.h"
#include "vcbutton.h"
#include "vcslider.h"
#include "qlcfile.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

using D = DeskRecord;

namespace
{
const quint32 kCh = 42;
}

void Dispatch_Test::initTestCase()
{
    QVERIFY(m_settings.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
}

void Dispatch_Test::init()
{
    QSettings().remove(DeskRecord::settingsGroup());
    m_doc = new Doc(this);
    new VirtualConsole(nullptr, m_doc);
    QDir dir(QStringLiteral("../../../engine/test/iopluginstub"));
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub in ../../../engine/test/iopluginstub");
    m_stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    m_stub->m_nameOverride = QStringLiteral("MIDI");
    m_stub->m_lineNames = QStringList() << QStringLiteral("APC MINI");
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(1, m_stub->name(), QString(), 0));

    Fixture *fxi = new Fixture(m_doc);
    fxi->setName(QStringLiteral("Dim"));
    fxi->setAddress(0);
    fxi->setUniverse(0);
    fxi->setChannels(4);
    m_doc->addFixture(fxi);
}

void Dispatch_Test::cleanup()
{
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    tick();
    ShowFeatures::instance()->setOn(ShowFeatures::ControlSurfaces, true);
    delete VirtualConsole::instance();
    if (m_stub != nullptr)
    {
        m_stub->m_nameOverride.clear();
        m_stub->m_lineNames.clear();
    }
    delete m_doc;
    m_doc = nullptr;
    m_stub = nullptr;
    QSettings().remove(DeskRecord::settingsGroup());
}

void Dispatch_Test::send(quint32 channel, uchar value)
{
    emit m_doc->inputOutputMap()->inputValueChanged(1, channel, value);
    QCoreApplication::processEvents();
}

void Dispatch_Test::tick(int n)
{
    for (int i = 0; i < n; ++i)
        m_doc->masterTimer()->timerTick();
    QCoreApplication::processEvents();
}

void Dispatch_Test::deskOn(quint32 channel, bool on)
{
    D *desk = m_doc->deskRecord();
    D::Binding b;
    if (on)
    {
        b.identity = desk->identityOfUniverse(1);
        b.device = QStringLiteral("APC MINI");
        b.plugin = QStringLiteral("MIDI");
        b.channel = channel;
        b.control = QStringLiteral("Note 42");
    }
    desk->setBinding(D::Back, b);
    QCOMPARE(desk->isLive(D::Back), on);
    QCOMPARE(m_doc->inputOutputMap()->isStaticControl(1, channel), on);
}

void Dispatch_Test::staticControlSkipsShowListeners_data()
{
    QTest::addColumn<QString>("listener");
    QTest::addColumn<bool>("desk");
    for (const char *l : { "VC button", "VC slider", "GM input", "channel group", "speed master", "encoder bank" })
    {
        QTest::newRow(qPrintable(QString("%1, no desk").arg(l))) << QString(l) << false;
        QTest::newRow(qPrintable(QString("%1, the desk's control").arg(l))) << QString(l) << true;
    }
}

void Dispatch_Test::staticControlSkipsShowListeners()
{
    QFETCH(QString, listener);
    QFETCH(bool, desk);
    deskOn(kCh, desk);
    // The same control a step away is the show's either way.
    const bool fires = !desk;
    QWidget host;
    InputOutputMap *iom = m_doc->inputOutputMap();

    if (listener == "VC button")
    {
        m_doc->setMode(Doc::Operate);
        Scene *s = new Scene(m_doc);
        s->addFixture(m_doc->fixtures().first()->id());
        s->setValue(m_doc->fixtures().first()->id(), 0, 255);
        m_doc->addFunction(s);
        VCButton btn(&host, m_doc);
        btn.setFunction(s->id());
        btn.setInputSource(QSharedPointer<QLCInputSource>(new QLCInputSource(1, kCh)));
        send(kCh, 127);
        send(kCh, 0);
        tick();
        QCOMPARE(s->isRunning(), fires);
    }
    else if (listener == "VC slider")
    {
        m_doc->setMode(Doc::Operate);
        VCSlider slider(&host, m_doc);
        slider.setSliderMode(VCSlider::Submaster);
        slider.setInputSource(QSharedPointer<QLCInputSource>(new QLCInputSource(1, kCh)));
        const int before = slider.sliderValue();
        send(kCh, 3);
        send(kCh, 200);
        QCOMPARE(slider.sliderValue() != before, fires);
    }
    else if (listener == "GM input")
    {
        GrandMasterSlider gm(&host, iom);
        VirtualConsole::instance()->restoreGrandMasterInput(1, kCh);
        iom->setGrandMasterValue(255);
        send(kCh, 100);
        QCOMPARE(iom->grandMasterValue() == 100, fires);
    }
    else if (listener == "channel group")
    {
        m_doc->setMode(Doc::Design);    // a channel group hears input in Design
        ChannelsGroup *g = new ChannelsGroup(m_doc);
        g->setName(QStringLiteral("Group"));
        QVERIFY(m_doc->addChannelsGroup(g));
        g->setInputSource(QSharedPointer<QLCInputSource>(new QLCInputSource(1, kCh)));
        // by name: ChannelsGroup is not exported from the engine DLL (dllsignals_test)
        QSignalSpy heard(g, SIGNAL(valueChanged(quint32,uchar)));
        send(kCh, 90);
        QCOMPARE(heard.count() > 0, fires);
    }
    else if (listener == "speed master")
    {
        SpeedMasters *sm = m_doc->speedMasters();
        if (sm->count() < 1)
            sm->setCount(1);
        sm->setInput(0, SpeedMasters::PauseControl, 1, kCh);
        const bool was = sm->isPaused(0);
        send(kCh, 127);
        send(kCh, 0);
        QCOMPARE(sm->isPaused(0) != was, fires);
    }
    else if (listener == "encoder bank")
    {
        m_doc->setMode(Doc::Design);
        EncoderBank *enc = m_doc->controlMap()->encoderBank();
        EncoderBank::Device d;
        d.name = QStringLiteral("Test");
        d.universe = 1;
        d.encoders = 0;
        d.bankNext = qint32(kCh);
        enc->setDevice(d);
        enc->setEnabled(true);
        QSignalSpy bank(enc, &EncoderBank::bankChanged);
        send(kCh, 127);
        send(kCh, 0);
        QCOMPARE(bank.count() > 0, fires);
    }
    else
        QFAIL("no such listener");
}

void Dispatch_Test::stopFiresBehindAModalAndWithTheFeatureOff()
{
    D *desk = m_doc->deskRecord();
    D::Binding b;
    b.identity = desk->identityOfUniverse(1);
    b.device = QStringLiteral("APC MINI");
    b.plugin = QStringLiteral("MIDI");
    b.channel = 50;
    b.control = QStringLiteral("Note 50");
    QVERIFY(desk->setBinding(D::Stop, b));
    QSignalSpy fired(desk, &D::fired);

    // Control Surfaces off for the show: triggers put away, the desk's STOP not.
    ShowFeatures::instance()->setOn(ShowFeatures::ControlSurfaces, false);
    send(50, 127);
    send(50, 0);
    QCOMPARE(fired.count(), 1);

    // Behind a modal window, in Design and in Operate.
    for (Doc::Mode mode : { Doc::Design, Doc::Operate })
    {
        m_doc->setMode(mode);
        QDialog modal;
        modal.setModal(true);
        int during = -1;
        QTimer::singleShot(0, &modal, [&]() {
            send(50, 127);
            send(50, 0);
            during = int(fired.count());
            modal.accept();
        });
        modal.exec();
        QCOMPARE(during, mode == Doc::Design ? 2 : 3);
    }
    QCOMPARE(fired.last().first().toString(), QStringLiteral("ALL OFF"));
}

QTEST_MAIN(Dispatch_Test)
