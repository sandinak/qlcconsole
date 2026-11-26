/*
  Q Light Controller Plus - Unit test
  surfacemap_test.cpp

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

#include <QApplication>
#include <QScrollArea>
#include <QScrollBar>
#include <QPushButton>
#include <QToolButton>
#include <QSignalSpy>
#include <QSettings>
#include <QLabel>
#include <QDir>
#include <QtTest>

#include "surfacemap_test.h"
#include "simmididevice.h"
#include "surfacescenarios.h"
#include "controlsurfacespanel.h"
#include "desksetuppanel.h"
#include "surfacechips.h"
#include "surfacemap.h"
#include "deskrecord.h"
#include "controlmap.h"
#include "ioplugincache.h"
#include "qlcioplugin.h"
#include "qlcfile.h"
#include "scene.h"
#include "doc.h"

namespace
{
const QString kApc = QStringLiteral("Akai APC40 mkII");
const QString kPmj = QStringLiteral("PMJ Black 1");
const QString kMaize = QStringLiteral("MaizeDMX Playback Controller");
const quint32 kPlay = 128 + 91, kRecord = 128 + 93, kStopAll = 128 + 81, kMasterFader = 14;
const quint32 kClip11 = 128 + 32;   // the top left pad
const quint32 kClip12 = 128 + 33;
const quint32 kFader1 = 7;

QString profile(const QString &file)
{
    return QStringLiteral(QLC_TEST_RESOURCE_ROOT) + QStringLiteral("inputprofiles/") + file;
}

void settle()
{
    for (int i = 0; i < 3; ++i)
    {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
    }
}

Scene *scene(Doc *doc, const QString &name)
{
    Scene *s = new Scene(doc);
    s->setName(name);
    doc->addFunction(s);
    return s;
}
}

void SurfaceMap_Test::initTestCase()
{
    // This computer's desk is kept in the settings: the test's own.
    QVERIFY(m_settings.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
}

void SurfaceMap_Test::init()
{
    QSettings().remove(DeskRecord::settingsGroup());
    m_doc = new Doc(this);
    QDir dir(QStringLiteral("../../../engine/test/iopluginstub"));
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub");
    m_stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    m_stub->m_feedback.clear();
    m_stub->m_feedbackParams.clear();
    m_stub->m_feedbackLines.clear();
    m_stub->m_lineNames.clear();
}

void SurfaceMap_Test::cleanup()
{
    m_stub->m_lineNames.clear();
    m_stub->m_linesAreHardware = false;
    m_stub->m_networkDmx = true;
    delete m_doc;
    m_doc = nullptr;
    m_stub = nullptr;
    QSettings().remove(DeskRecord::settingsGroup());
}

void SurfaceMap_Test::aPressLightsItsOwnControl()
{
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc, profile("Akai-APC40-mkII.qxi"));
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    SurfaceMaps maps(m_doc);
    settle();
    SurfaceMapView *mv = maps.view(0);
    QVERIFY2(mv != nullptr, "no map of the APC40");
    QVERIFY(!mv->isHighlighted(kClip11));

    QVERIFY(apc.press(QStringLiteral("#%1").arg(kClip11)));
    settle();
    QVERIFY2(mv->isHighlighted(kClip11), "the pad pressed is not lit on the map");
    QVERIFY2(!mv->isHighlighted(kClip12), "a pad not pressed is lit");
    QVERIFY(apc.release(QStringLiteral("#%1").arg(kClip11)));

    // A fader: lit, with its value (MIDI 64 of 127 is about half).
    QVERIFY(apc.set(QStringLiteral("#%1").arg(kFader1), 64));
    settle();
    QVERIFY(mv->isHighlighted(kFader1));
    QVERIFY2(qAbs(mv->value(kFader1) - 128) <= 2, qPrintable(QString::number(mv->value(kFader1))));
    // A device knob answers on the selected track's channel too: the same knob.
    QVERIFY(apc.set(QStringLiteral("#%1").arg((8 << 12) | 16), 100));
    settle();
    QVERIFY(mv->isHighlighted(16));
    // It fades.
    QTRY_VERIFY_WITH_TIMEOUT(!mv->isHighlighted(kClip11), 3000);
}

void SurfaceMap_Test::eachJobIsInItsGroup()
{
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc, profile("Akai-APC40-mkII.qxi"));
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    settle();
    ControlMap *cm = m_doc->controlMap();
    Scene *warm = scene(m_doc, QStringLiteral("Warm"));
    Scene *stab = scene(m_doc, QStringLiteral("Stab"));
    cm->setBinding({ 0, kClip11, ControlMap::function(warm->id()), ControlMap::Toggle });
    // Armed only in a cue that is not live now.
    cm->setBinding({ 0, kClip12, ControlMap::function(stab->id()), ControlMap::Toggle,
                     ControlMap::Place(ControlMap::CuePlace, 4242) });
    SurfaceMaps maps(m_doc);
    settle();
    SurfaceMapView *mv = maps.view(0);
    QVERIFY(mv != nullptr);

    // This computer's desk: the APC's defaults, seeded.
    QCOMPARE(mv->group(kPlay), SurfaceMapView::Desk);
    QCOMPARE(mv->role(kPlay), QStringLiteral("go"));
    QCOMPARE(mv->jobText(kPlay), QStringLiteral("GO"));
    QCOMPARE(mv->role(kRecord), QStringLiteral("back"));
    QCOMPARE(mv->role(kStopAll), QStringLiteral("stop"));
    QCOMPARE(mv->role(kMasterFader), QStringLiteral("gm"));
    QCOMPARE(mv->group(kMasterFader), SurfaceMapView::Desk);

    // The show's.
    QCOMPARE(mv->group(kClip11), SurfaceMapView::Show);
    QVERIFY2(mv->jobText(kClip11).contains(QStringLiteral("Warm")), qPrintable(mv->jobText(kClip11)));
    QVERIFY(!mv->isDim(kClip11));
    QCOMPARE(mv->group(kClip12), SurfaceMapView::Show);
    QVERIFY2(mv->isDim(kClip12), "a hit armed only in another cue is not dimmer");
    QVERIFY2(mv->toolTipFor(kClip12).contains(cm->placeName(ControlMap::Place(ControlMap::CuePlace, 4242)).toHtmlEscaped()),
             qPrintable(mv->toolTipFor(kClip12)));

    // Free.
    QCOMPARE(mv->group(128 + 34), SurfaceMapView::Free);
    QVERIFY(mv->jobText(128 + 34).isEmpty());

    // A trigger moved: the map follows.
    cm->removeBinding(ControlMap::function(warm->id()));
    settle();
    QCOMPARE(mv->group(kClip11), SurfaceMapView::Free);
}

void SurfaceMap_Test::theFixedJobsAreSaidAndALackingOneIsSetUp()
{
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc, profile("Akai-APC40-mkII.qxi"));
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    settle();
    ControlSurfacesPanel hub(m_doc);
    SurfaceMaps *maps = hub.surfaceMaps();
    QVERIFY(maps != nullptr);
    settle();
    const QString line = maps->roleLine(0);
    QVERIFY2(line.contains(QStringLiteral("GO is PLAY")) && line.contains(QStringLiteral("Back is RECORD"))
                 && line.contains(QStringLiteral("STOP is STOP ALL CLIPS")) && line.contains(QStringLiteral("GM is MASTER")),
             qPrintable(line));
    QVERIFY(maps->findChild<QPushButton *>(QStringLiteral("surfaceMapSetUp0_stop")) == nullptr);

    // STOP cleared: the map says it lacks one, and Set Up STOP… starts the desk's Learn.
    DeskRecord *desk = m_doc->deskRecord();
    QVERIFY(desk->clear(DeskRecord::Stop));
    settle();
    QPushButton *setUp = maps->findChild<QPushButton *>(QStringLiteral("surfaceMapSetUp0_stop"));
    QVERIFY2(setUp != nullptr, "no Set Up STOP… for a board with no STOP");
    QCOMPARE(setUp->text(), QStringLiteral("Set Up STOP…"));
    QSignalSpy asked(maps, &SurfaceMaps::deskLearnRequested);
    setUp->click();
    QCOMPARE(asked.count(), 1);
    QCOMPARE(asked.first().first().toInt(), int(DeskRecord::Stop));
    QVERIFY2(desk->isLearning() && desk->learningRole() == DeskRecord::Stop, "Set Up STOP… did not start the desk's Learn");
    desk->cancelLearn();
}

void SurfaceMap_Test::thePmjsOwnJobsAreTheBoards()
{
    SimMidiDevice pmj(m_doc, m_stub, kPmj, 0, kPmj, profile("PMJ-Black-1.qxi"));
    QVERIFY2(pmj.plug(), qPrintable(pmj.lastError()));
    SurfaceMaps maps(m_doc);
    settle();
    SurfaceMapView *mv = maps.view(0);
    QVERIFY(mv != nullptr);
    QCOMPARE(mv->group(32896), SurfaceMapView::Board);
    QCOMPARE(mv->role(32896), QStringLiteral("go"));
    QCOMPARE(mv->role(32907), QStringLiteral("back"));
    QCOMPARE(mv->role(32768), QStringLiteral("gm"));
    QCOMPARE(mv->jobText(32769), QStringLiteral("Level 1"));
    QCOMPARE(mv->jobText(32918), QStringLiteral("Blackout"));
    // The PMJ's own seeds nothing: its overlay does them.
    QVERIFY(!m_doc->deskRecord()->anySet());
    const QString line = maps.roleLine(0);
    QVERIFY2(line.contains(QStringLiteral("GO is Go")) && line.contains(QStringLiteral("GM is Master")), qPrintable(line));
    QVERIFY(maps.findChild<QPushButton *>(QStringLiteral("surfaceMapSetUp0_stop")) != nullptr);
}

void SurfaceMap_Test::aLampShowsWhatWasSent()
{
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc, profile("Akai-APC40-mkII.qxi"));
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    SurfaceMaps maps(m_doc);
    settle();
    SurfaceMapView *mv = maps.view(0);
    QVERIFY(mv != nullptr);
    // Through the one place every light goes out (InputOutputMap::sendFeedBack()).
    QVERIFY(m_doc->inputOutputMap()->sendFeedBack(0, kClip11, 42, QVariant()));
    settle();
    QCOMPARE(mv->lamp(kClip11), 42);
    QCOMPARE(apc.lamp(kClip11), 21);    // the board got velocity 21: the same light
    uchar sent = 0;
    QVERIFY(m_doc->inputOutputMap()->lastFeedback(0, kClip11, &sent));
    QCOMPARE(int(sent), 42);
    // A map built later reads what was sent before.
    SurfaceMaps later(m_doc);
    QCOMPARE(later.view(0)->lamp(kClip11), 42);
    QCOMPARE(later.view(0)->lamp(kClip12), -1);
}

void SurfaceMap_Test::aBoardWithNoLayoutFallsBackToTheRows()
{
    SimMidiDevice mini(m_doc, m_stub, QStringLiteral("APC MINI"), 0, QStringLiteral("Akai APC Mini"),
                       profile("Akai-APCMini.qxi"));
    QVERIFY2(mini.plug(), qPrintable(mini.lastError()));
    ControlSurfacesPanel hub(m_doc);
    settle();
    SurfaceMaps *maps = hub.surfaceMaps();
    QVERIFY(maps->views().isEmpty());
    QCOMPARE(maps->fallbackLines().size(), 1);
    QVERIFY2(maps->fallbackLines().first().contains(QStringLiteral("no drawing")), qPrintable(maps->fallbackLines().first()));
    QVERIFY(SurfaceMaps::surfaceUniverses(m_doc).isEmpty());
    // The rows are all still there.
    QVERIFY(!hub.status(QStringLiteral("map")).isEmpty());
    QVERIFY(hub.setUpButton(QStringLiteral("map")) != nullptr);
    SurfaceChips chips(m_doc);
    QVERIFY(chips.universes().isEmpty());
    QVERIFY(chips.isHidden());
}

void SurfaceMap_Test::theFooterChipOpensItsMap()
{
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc, profile("Akai-APC40-mkII.qxi"));
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    SimMidiDevice maize(m_doc, m_stub, QStringLiteral("USB2.0-MIDI Port 1"), 1, kMaize,
                        profile("MaizeDMX-Playback-Controller.qxi"));
    QVERIFY2(maize.plug(), qPrintable(maize.lastError()));
    SurfaceChips chips(m_doc);
    settle();
    QCOMPARE(chips.universes(), QList<quint32>({ 0, 1 }));
    QCOMPARE(chips.chip(0)->text(), QStringLiteral("APC40"));
    QCOMPARE(chips.chip(1)->text(), QStringLiteral("MaizeDMX"));
    QSignalSpy asked(&chips, &SurfaceChips::mapRequested);
    chips.chip(1)->click();
    QCOMPARE(asked.count(), 1);
    QCOMPARE(asked.first().first().toUInt(), 1u);

    // Its map, scrolled to in the page.
    QScrollArea scroll;
    ControlSurfacesPanel *hub = new ControlSurfacesPanel(m_doc);
    scroll.setWidget(hub);
    scroll.setWidgetResizable(true);
    scroll.resize(900, 500);
    scroll.show();
    settle();
    SurfaceMaps *maps = hub->surfaceMaps();
    QVERIFY(maps->view(1) != nullptr);
    maps->showMap(1);
    settle();
    QVERIFY2(scroll.verticalScrollBar()->value() > 0, "the page did not scroll to the map");
}

void SurfaceMap_Test::theMapScrollsRatherThanClips()
{
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc, profile("Akai-APC40-mkII.qxi"));
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    SurfaceMaps maps(m_doc);
    settle();
    SurfaceMapView *mv = maps.view(0);
    const QSize min = mv->minimumSizeHint();
    QVERIFY(min.width() >= int(mv->layout().size().width() * SurfaceMapView::kMinCell));
    // At its smallest every control is inside the widget.
    mv->resize(min.width(), mv->heightForWidth(min.width()));
    for (const SurfaceLayout::Control &c : mv->layout().controls())
        QVERIFY2(mv->rect().contains(mv->cellRect(c.channel)), qPrintable(c.face));
    // A narrow page scrolls sideways: the map is never squeezed below it.
    QScrollArea scroll;
    scroll.setWidgetResizable(true);
    scroll.setWidget(&maps);
    scroll.resize(400, 400);
    scroll.show();
    settle();
    QVERIFY(mv->width() >= min.width());
    QVERIFY(scroll.horizontalScrollBar()->maximum() > 0);
    scroll.takeWidget();
}

void SurfaceMap_Test::shots()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set");
    QDir().mkpath(dir);
    const QString prefix = qEnvironmentVariable("QLC_SHOT_PREFIX", QStringLiteral("after"));

    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc, profile("Akai-APC40-mkII.qxi"));
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    SimMidiDevice pmj(m_doc, m_stub, kPmj, 1, kPmj, profile("PMJ-Black-1.qxi"));
    QVERIFY2(pmj.plug(), qPrintable(pmj.lastError()));
    SimMidiDevice maize(m_doc, m_stub, QStringLiteral("USB2.0-MIDI Port 1"), 2, kMaize,
                        profile("MaizeDMX-Playback-Controller.qxi"));
    QVERIFY2(maize.plug(), qPrintable(maize.lastError()));
    SimMidiDevice apc64(m_doc, m_stub, QStringLiteral("APC64 DAW"), 3, QStringLiteral("Akai APC64"),
                        profile("Akai-APC64.qxi"));
    QVERIFY2(apc64.plug(), qPrintable(apc64.lastError()));
    settle();
    // A show with a few triggers, one only in a cue; a lit pad; a fader moved.
    ControlMap *cm = m_doc->controlMap();
    const char *names[] = { "Warm wash", "Cool wash", "Truss chase", "Blackout fade" };
    for (int i = 0; i < 4; ++i)
        cm->setBinding({ 0, quint32(kClip11 + i), ControlMap::function(scene(m_doc, names[i])->id()), ControlMap::Toggle });
    cm->setBinding({ 0, quint32(128 + 24), ControlMap::function(scene(m_doc, QStringLiteral("Stab"))->id()),
                     ControlMap::Toggle, ControlMap::Place(ControlMap::CuePlace, 4242) });
    cm->setBinding({ 2, quint32(128 + 0), ControlMap::function(scene(m_doc, QStringLiteral("Front"))->id()), ControlMap::Toggle });
    InputOutputMap *iom = m_doc->inputOutputMap();
    for (int i = 0; i < 4; ++i)
        iom->sendFeedBack(0, quint32(kClip11 + i), uchar(2 * (5 + 16 * i)), QVariant());
    iom->sendFeedBack(1, 32896, 254, QVariant());

    const QList<QSize> sizes = { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) };
    const QList<QPair<quint32, QString>> boards = { { 0, QStringLiteral("apc40") }, { 1, QStringLiteral("pmj") },
                                                    { 2, QStringLiteral("maizedmx") }, { 3, QStringLiteral("apc64") } };
    for (const QSize &size : sizes)
    {
        QScrollArea scroll;
        scroll.setWidgetResizable(true);
        ControlSurfacesPanel *hub = new ControlSurfacesPanel(m_doc);
        scroll.setWidget(hub);
        // The page sits beside Connections' list: about 240 px of the window.
        scroll.resize(size.width() - 240, size.height() - 120);
        scroll.show();
        settle();
        for (const auto &b : boards)
        {
            hub->surfaceMaps()->showMap(b.first);
            if (b.first == 0)
            {
                apc.set(QStringLiteral("#%1").arg(kFader1 | (2 << 12)), 90);
                apc.press(QStringLiteral("#%1").arg(kClip11 + 1));
            }
            if (b.first == 1)
                pmj.set(QStringLiteral("Ch 3"), 100);
            if (b.first == 2)
                maize.press(QStringLiteral("X3"));
            if (b.first == 3)
                apc64.set(QStringLiteral("Touch strip 2"), 70);
            settle();
            const QString file = QStringLiteral("%1/%2-surfacemap-%3-%4x%5.png")
                                     .arg(dir, prefix, b.second).arg(size.width()).arg(size.height());
            QVERIFY(scroll.grab().save(file));
            apc.release(QStringLiteral("#%1").arg(kClip11 + 1));
            maize.release(QStringLiteral("X3"));
        }
    }
}

QTEST_MAIN(SurfaceMap_Test)
