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
#include <QStatusBar>
#include <QCheckBox>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
#undef protected
#undef private
#include "iopluginstub.h"

#include "surfacemap_test.h"
#include "simmididevice.h"
#include "surfacescenarios.h"
#include "controlsurfacespanel.h"
#include "desksetuppanel.h"
#include "surfacechips.h"
#include "surfacemap.h"
#include "deskrecord.h"
#include "controlmap.h"
#include "surfacepages.h"
#include "showoperate.h"
#include "inputoutputmap.h"
#include "surfacelayout.h"
#include "surfacesetup.h"
#include "surfaceautopatch.h"
#include "surfaceboardlog.h"
#include "boardlogdialog.h"
#include "footerbar.h"
#include <QMainWindow>
#include "connectionstree.h"
#include "inputpatch.h"
#include "outputpatch.h"
#include <QTreeWidget>
#include <QPlainTextEdit>
#include <QClipboard>
#include <QMenu>
#include <QTableWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QCollator>
#include <QImage>
#include <QFile>
#include <memory>
#include <QContextMenuEvent>
#include "surfacekeylabels.h"
#include "measurementunits.h"
#include "ioplugincache.h"
#include "qlcioplugin.h"
#include "qlcfile.h"
#include "scene.h"
#include "doc.h"
#include <QTemporaryDir>
#include <QRegularExpression>

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

int g_pinAsked = 0;
bool g_pinAnswer = false;
bool pinAsker(QWidget *, const QString &)
{
    ++g_pinAsked;
    return g_pinAnswer;
}

const SurfaceLayout::Control &ctl(const SurfaceLayout &l, quint32 ch)
{
    return l.controls().at(l.indexOf(ch));
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
    if (m_stub != nullptr)
    {
        m_stub->m_lineNames.clear();
        m_stub->m_linesAreHardware = false;
        m_stub->m_networkDmx = true;
    }
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
    // Each with the surface page on now (Design).
    QCOMPARE(chips.chip(0)->text(), QStringLiteral("APC40 · Move"));
    QCOMPARE(chips.chip(1)->text(), QStringLiteral("MaizeDMX · Colour"));
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

void SurfaceMap_Test::theFaderPort2IsDrawn()
{
    SimMidiDevice fp(m_doc, m_stub, QStringLiteral("PreSonus FP2"), 0, QStringLiteral("PreSonus FaderPort 2"),
                     profile("PreSonus-FaderPort-2.qxi"));
    QVERIFY2(fp.plug(), qPrintable(fp.lastError()));
    SurfaceMaps maps(m_doc);
    settle();
    SurfaceMapView *mv = maps.view(0);
    QVERIFY2(mv != nullptr, "no map of the FaderPort 2");
    const quint32 kFader = 513, kKnob = 16, kPlayKey = 128 + 94, kRew = 128 + 91, kStopKey = 128 + 93;
    QCOMPARE(mv->layout().controls().size(), 22);     // the knob and its push are one control
    QVERIFY(mv->layout().controls().at(mv->layout().indexOf(kFader)).isFader());
    QVERIFY(mv->layout().controls().at(mv->layout().indexOf(kKnob)).turns());
    QCOMPARE(mv->layout().roleOn(kPlayKey), QStringLiteral("go"));
    QCOMPARE(mv->layout().roleOn(kRew), QStringLiteral("back"));
    QCOMPARE(mv->layout().roleOn(kStopKey), QStringLiteral("stop"));
    QCOMPARE(mv->layout().roleOn(kFader), QStringLiteral("gm"));
    QVERIFY(!mv->isHighlighted(kPlayKey));
    QVERIFY(fp.press(QStringLiteral("Play")));
    settle();
    QVERIFY2(mv->isHighlighted(kPlayKey), "Play is not lit on the map");
    QVERIFY(!mv->isHighlighted(kStopKey));
    QVERIFY(fp.release(QStringLiteral("Play")));
}

/* The stub plugin is one instance per process: an App's cache takes it over,
   so the test's own Doc lets go of it first (cleanup() copes with none). */
void SurfaceMap_Test::dropDoc()
{
    delete m_doc;
    m_doc = nullptr;
    m_stub = nullptr;
}

namespace
{
IOPluginStub *stubOf(Doc *doc)
{
    for (QLCIOPlugin *p : doc->ioPluginCache()->plugins())
        if (IOPluginStub *s = dynamic_cast<IOPluginStub *>(p))
            return s;
    return nullptr;
}
}

void SurfaceMap_Test::theFaderPortChipIsInTheFooterAtSmallSizes()
{
    dropDoc();
    App app;
    app.init();
    Doc *doc = app.doc();
    QDir dir(QStringLiteral("../../../engine/test/iopluginstub"));
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    doc->ioPluginCache()->load(dir);
    IOPluginStub *stub = stubOf(doc);
    QVERIFY2(stub != nullptr, "no I/O plugin stub");
    stub->m_lineNames.clear();
    SimMidiDevice fp(doc, stub, QStringLiteral("PreSonus FP2"), 0, QStringLiteral("PreSonus FaderPort 2"),
                     profile("PreSonus-FaderPort-2.qxi"));
    QVERIFY2(fp.plug(), qPrintable(fp.lastError()));
    doc->setMode(Doc::Operate);     // a show running
    for (const QSize &size : { QSize(1024, 640), QSize(1440, 900) })
    {
        app.resize(size);
        app.show();
        settle();
        QTest::qWait(200);
        settle();
        SurfaceChips *chips = app.m_statusSurfaceChips;
        QVERIFY(chips != nullptr);
        QVERIFY2(chips->isVisibleTo(app.statusBar()), qPrintable(QStringLiteral("no chip at %1").arg(size.width())));
        QToolButton *chip = chips->chip(0);
        QVERIFY(chip != nullptr);
        QCOMPARE(chip->text(), QStringLiteral("FaderPort"));
        const QRect where(chip->mapTo(app.statusBar(), QPoint(0, 0)), chip->size());
        QVERIFY2(chip->width() > 20 && app.statusBar()->rect().contains(where),
                 qPrintable(QStringLiteral("chip squeezed or clipped at %1: %2 wide")
                                .arg(size.width()).arg(chip->width())));
    }
}

void SurfaceMap_Test::aBareStopSaysShiftStopInTheFooter()
{
    dropDoc();
    App app;
    app.init();
    Doc *doc = app.doc();
    QDir dir(QStringLiteral("../../../engine/test/iopluginstub"));
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    doc->ioPluginCache()->load(dir);
    IOPluginStub *stub = stubOf(doc);
    QVERIFY(stub != nullptr);
    stub->m_lineNames.clear();
    SimMidiDevice fp(doc, stub, QStringLiteral("PreSonus FP2"), 0, QStringLiteral("PreSonus FaderPort 2"),
                     profile("PreSonus-FaderPort-2.qxi"));
    QVERIFY2(fp.plug(), qPrintable(fp.lastError()));
    app.show();
    settle();
    QVERIFY(doc->deskRecord()->isLive(DeskRecord::Stop));
    QSignalSpy fired(doc->deskRecord(), &DeskRecord::fired);
    QVERIFY(fp.press(QStringLiteral("Stop")));
    QVERIFY(fp.release(QStringLiteral("Stop")));
    settle();
    QCOMPARE(fired.count(), 0);
    QCOMPARE(app.statusBar()->currentMessage(), QStringLiteral("FaderPort: Shift+Stop stops."));
    QVERIFY(fp.press(QStringLiteral("Shift")));
    QVERIFY(fp.press(QStringLiteral("Stop")));
    QVERIFY(fp.release(QStringLiteral("Stop")));
    QVERIFY(fp.release(QStringLiteral("Shift")));
    settle();
    QCOMPARE(fired.count(), 1);
}

void SurfaceMap_Test::aFooterClickNeverGoesLive()
{
    dropDoc();
    App app;
    app.init();
    Doc *doc = app.doc();
    QDir dir(QStringLiteral("../../../engine/test/iopluginstub"));
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    doc->ioPluginCache()->load(dir);
    IOPluginStub *stub = stubOf(doc);
    QVERIFY(stub != nullptr);
    stub->m_lineNames.clear();
    SimMidiDevice fp(doc, stub, QStringLiteral("PreSonus FP2"), 0, QStringLiteral("PreSonus FaderPort 2"),
                     profile("PreSonus-FaderPort-2.qxi"));
    QVERIFY2(fp.plug(), qPrintable(fp.lastError()));
    app.resize(1440, 900);
    app.show();
    settle();
    QCOMPARE(doc->mode(), Doc::Design);
    // The Design/Operate chip does not go live.
    QVERIFY(app.m_statusModeChipLabel->isVisibleTo(app.statusBar()));
    QTest::mouseClick(app.m_statusModeChipLabel, Qt::LeftButton);
    settle();
    QCOMPARE(doc->mode(), Doc::Design);
    QVERIFY2(app.statusBar()->currentMessage().contains("Operate"), qPrintable(app.statusBar()->currentMessage()));
    // Nor does a board's chip (it opens a map).
    for (quint32 u : app.m_statusSurfaceChips->universes())
    {
        app.m_statusSurfaceChips->chip(u)->click();
        settle();
        QCOMPARE(doc->mode(), Doc::Design);
    }
}

void SurfaceMap_Test::aSurfaceChipReadsAsABoard()
{
    SimMidiDevice fp(m_doc, m_stub, QStringLiteral("PreSonus FP2"), 0, QStringLiteral("PreSonus FaderPort 2"),
                     profile("PreSonus-FaderPort-2.qxi"));
    QVERIFY2(fp.plug(), qPrintable(fp.lastError()));
    SurfaceChips chips(m_doc);
    settle();
    QToolButton *chip = chips.chip(0);
    QVERIFY(chip != nullptr);
    QVERIFY2(!chip->icon().isNull(), "no controller glyph on the chip");
    QCOMPARE(chip->toolButtonStyle(), Qt::ToolButtonTextBesideIcon);
    QCOMPARE(chip->text(), QStringLiteral("FaderPort"));
    QVERIFY(chip->toolTip().contains("click for its map"));
}

void SurfaceMap_Test::aChipOpensOnlyItsBoard()
{
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc, profile("Akai-APC40-mkII.qxi"));
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    SimMidiDevice fp(m_doc, m_stub, QStringLiteral("PreSonus FP2"), 1, QStringLiteral("PreSonus FaderPort 2"),
                     profile("PreSonus-FaderPort-2.qxi"));
    QVERIFY2(fp.plug(), qPrintable(fp.lastError()));
    SurfaceMaps maps(m_doc);
    maps.show();
    settle();
    // Connections lists every board.
    QVERIFY(maps.isBoardShown(0) && maps.isBoardShown(1));
    maps.showOnly(1);
    settle();
    QVERIFY2(!maps.isBoardShown(0), "another board's map is stacked above");
    QVERIFY(maps.isBoardShown(1));
    QPushButton *all = maps.findChild<QPushButton *>(QStringLiteral("surfaceMapAllBoards1"));
    QVERIFY(all != nullptr && !all->isHidden());
    all->click();
    QVERIFY(maps.isBoardShown(0) && maps.isBoardShown(1));
    // Leaving the page ends it.
    maps.showOnly(0);
    QVERIFY(!maps.isBoardShown(1));
    maps.hide();
    settle();
    QVERIFY(maps.isBoardShown(0) && maps.isBoardShown(1));
}

void SurfaceMap_Test::theFaderPortMapSaysTheShiftCombosAndHasItsOptions()
{
    SimMidiDevice fp(m_doc, m_stub, QStringLiteral("PreSonus FP2"), 0, QStringLiteral("PreSonus FaderPort 2"),
                     profile("PreSonus-FaderPort-2.qxi"));
    QVERIFY2(fp.plug(), qPrintable(fp.lastError()));
    SurfaceMaps maps(m_doc);
    settle();
    SurfaceMapView *mv = maps.view(0);
    QVERIFY(mv != nullptr);
    const quint32 kStopKey = 221, kPlayKey = 222, kShiftKey = 198;
    QCOMPARE(mv->jobText(kStopKey), QStringLiteral("Shift+STOP"));
    QVERIFY(mv->toolTipFor(kStopKey).contains("Shift"));
    QCOMPARE(mv->jobText(kPlayKey), QStringLiteral("GO"));
    QVERIFY2(mv->toolTipFor(kPlayKey).contains("After a stop, Shift+PLAY releases it"), qPrintable(mv->toolTipFor(kPlayKey)));
    QCOMPARE(mv->jobText(kShiftKey), QStringLiteral("Shift"));
    // Every control has its words: a job, or free.
    for (const SurfaceLayout::Control &c : mv->layout().controls())
        QVERIFY2(!mv->toolTipFor(c.channel).isEmpty(), qPrintable(c.face));

    // The options, in the map's header.
    QCheckBox *stop = maps.findChild<QCheckBox *>(QStringLiteral("surfaceMapShiftStop0"));
    QCheckBox *rel = maps.findChild<QCheckBox *>(QStringLiteral("surfaceMapShiftRelease0"));
    QPushButton *setUp = maps.findChild<QPushButton *>(QStringLiteral("surfaceMapOptSetUp0"));
    QVERIFY(stop != nullptr && rel != nullptr && setUp != nullptr);
    QCOMPARE(stop->text(), QStringLiteral("Stop needs Shift"));
    QCOMPARE(rel->text(), QStringLiteral("Releasing a stop needs Shift"));
    QVERIFY(stop->isChecked() && rel->isChecked());
    // Every board has a lamp test and its Board Log (2026-11-27; the PMJ alone had one).
    QVERIFY(maps.findChild<QPushButton *>(QStringLiteral("surfaceMapOptLampTest0")) != nullptr);
    QVERIFY(maps.findChild<QPushButton *>(QStringLiteral("surfaceMapOptBoardLog0")) != nullptr);
    QSignalSpy setUpAsked(&maps, &SurfaceMaps::controllerSetupRequested);
    setUp->click();
    QCOMPARE(setUpAsked.count(), 1);
    // Unticking is the show's word, kept; the map's words follow.
    stop->setChecked(false);
    settle();
    QCOMPARE(m_doc->controlMap()->shiftGuardStopSetting(), int(ControlMap::GuardOff));
    QVERIFY(!m_doc->deskRecord()->shiftGuardsStop(0));
    QCOMPARE(mv->jobText(kStopKey), QStringLiteral("STOP"));
    // A board with no Shift key has no such boxes (the APC).
}

void SurfaceMap_Test::theMaizeLedsAreLightsNotKnobs()
{
    SimMidiDevice maize(m_doc, m_stub, QStringLiteral("USB2.0-MIDI Port 1"), 0, kMaize,
                        profile("MaizeDMX-Playback-Controller.qxi"));
    QVERIFY2(maize.plug(), qPrintable(maize.lastError()));
    SurfaceMaps maps(m_doc);
    settle();
    SurfaceMapView *mv = maps.view(0);
    QVERIFY(mv != nullptr);
    const quint32 kLed1 = 173, kX7 = 171, kX9 = 172;
    const SurfaceLayout::Control &led = mv->layout().controls().at(mv->layout().indexOf(kLed1));
    QCOMPARE(led.kind, SurfaceLayout::Indicator);
    QVERIFY(!led.turns());
    QVERIFY2(mv->toolTipFor(kLed1).contains(QStringLiteral("A light on the board: not a knob or a button.")),
             qPrintable(mv->toolTipFor(kLed1)));
    // The page keys say their page; the one on is the Colour page.
    QVERIFY2(mv->toolTipFor(kX9).contains(QStringLiteral("Move")), qPrintable(mv->toolTipFor(kX9)));
    QVERIFY2(mv->toolTipFor(kX7).contains(QStringLiteral("Colour")), qPrintable(mv->toolTipFor(kX7)));
    // Lit as last sent.
    QVERIFY(m_doc->inputOutputMap()->sendFeedBack(0, kLed1, 255, QVariant()));
    settle();
    QVERIFY(mv->lamp(kLed1) > 0);
}

void SurfaceMap_Test::theApcMiniMk2IsDrawnAndAnOlderCopyIsSaid()
{
    SimMidiDevice mini(m_doc, m_stub, QStringLiteral("APC mini mk2 Control"), 0, QStringLiteral("Akai APC Mini mk2"),
                       profile("Akai-APCMini-mk2.qxi"));
    QVERIFY2(mini.plug(), qPrintable(mini.lastError()));
    SurfaceChips chips(m_doc);
    ControlSurfacesPanel hub(m_doc);
    settle();
    QCOMPARE(chips.universes(), QList<quint32>({ 0 }));
    QCOMPARE(chips.chip(0)->text(), QStringLiteral("APC mini \u00B7 Colour"));
    SurfaceMapView *mv = hub.surfaceMaps()->view(0);
    QVERIFY(mv != nullptr);
    QCOMPARE(mv->layout().roleOn(240), QStringLiteral("go"));
    QCOMPARE(mv->layout().roleOn(56), QStringLiteral("gm"));
    QVERIFY2(!hub.status(QStringLiteral("setup")).contains(QStringLiteral("older")), "no older copy yet");

    // A copy of the APC40 mkII profile without its track-channel knobs, in the user's folder.
    QFile built(profile("Akai-APC40-mkII.qxi"));
    QVERIFY(built.open(QIODevice::ReadOnly));
    QString text = QString::fromUtf8(built.readAll());
    text.remove(QRegularExpression(QStringLiteral("<Channel Number=\"4117\">.*?</Channel>\\r?\\n"),
                                   QRegularExpression::DotMatchesEverythingOption));
    QTemporaryDir user;
    QVERIFY(user.isValid());
    QFile out(user.filePath(QStringLiteral("Akai-APC40-mkII.qxi")));
    QVERIFY(out.open(QIODevice::WriteOnly));
    out.write(text.toUtf8());
    out.close();
    QDir ud(user.path());
    ud.setFilter(QDir::Files);
    ud.setNameFilters(QStringList() << QStringLiteral("*.qxi"));
    QDir sd(QStringLiteral(QLC_TEST_RESOURCE_ROOT) + QStringLiteral("inputprofiles"));
    sd.setFilter(QDir::Files);
    sd.setNameFilters(QStringList() << QStringLiteral("*.qxi"));
    m_doc->inputOutputMap()->loadProfiles(ud);
    m_doc->inputOutputMap()->loadProfiles(sd);
    hub.refresh();
    const QString say = hub.status(QStringLiteral("setup"));
    QVERIFY2(say.contains(QStringLiteral("Your copy of the Akai APC40 mkII profile"))
                 && say.contains(QStringLiteral("older than the one built in")), qPrintable(say));
}

void SurfaceMap_Test::eachBoardIsDrawnLikeTheRealOne()
{
    // The PMJ Black 1: Master, Go and Back are on the left, under a 3 x 4 grid of keys.
    const SurfaceLayout pmj = SurfaceLayout::forProfile(kPmj);
    QVERIFY(pmj.isValid());
    const quint32 gm = 32768, go = 32896, back = 32907, strip1 = 32769;
    QVERIFY(ctl(pmj, gm).rect.right() <= ctl(pmj, strip1).rect.left());
    QVERIFY(ctl(pmj, go).rect.right() <= ctl(pmj, strip1).rect.left());
    QVERIFY(ctl(pmj, back).rect.right() <= ctl(pmj, strip1).rect.left());
    QVERIFY(ctl(pmj, go).rect.bottom() <= ctl(pmj, back).rect.top());      // the red above the white
    const QList<quint32> grid = { 32946, 32947, 32948, 32949, 32931, 32944, 32945, 32919, 32920, 32918, 32932, 32933 };
    QSet<int> xs, ys;
    double gridBottom = 0;
    for (quint32 ch : grid)
    {
        xs << qRound(ctl(pmj, ch).rect.x() * 100);
        ys << qRound(ctl(pmj, ch).rect.y() * 100);
        gridBottom = qMax(gridBottom, ctl(pmj, ch).rect.bottom());
        QVERIFY2(ctl(pmj, ch).rect.right() <= ctl(pmj, strip1).rect.left(), "a grid key right of the strips");
    }
    QCOMPARE(xs.size(), 3);
    QCOMPARE(ys.size(), 4);
    QVERIFY(ctl(pmj, gm).rect.top() >= gridBottom);
    QVERIFY(ctl(pmj, go).rect.top() >= gridBottom);
    // Ten faders of one length, four round encoders above them, the two rows of keys under them.
    for (int i = 0; i < 10; ++i)
    {
        const SurfaceLayout::Control &f = ctl(pmj, quint32(32769 + i));
        QVERIFY(f.isFader());
        QCOMPARE(f.rect.height(), ctl(pmj, strip1).rect.height());
        QVERIFY(ctl(pmj, quint32(32921 + i)).rect.bottom() <= f.rect.top());         // Load above
        QVERIFY(ctl(pmj, quint32(32908 + i)).rect.top() >= f.rect.bottom());         // Up below
        QVERIFY(ctl(pmj, quint32(32897 + i)).rect.top() >= ctl(pmj, quint32(32908 + i)).rect.bottom());
    }
    for (quint32 ch = 32779; ch <= 32782; ++ch)
    {
        QCOMPARE(ctl(pmj, ch).shape, SurfaceLayout::Round);
        QVERIFY(ctl(pmj, ch).turns());
        QVERIFY(ctl(pmj, ch).rect.bottom() <= ctl(pmj, 32921).rect.top());
    }
    QCOMPARE(ctl(pmj, go).rect.top(), ctl(pmj, 32908).rect.top());          // on the rows of the keys

    // The FaderPort 2: one long fader; its knob is round and hears its push too; Play the biggest cap.
    const SurfaceLayout fp = SurfaceLayout::forProfile(QStringLiteral("PreSonus FaderPort 2"));
    QVERIFY(ctl(fp, 513).rect.height() > ctl(fp, 513).rect.width() * 5);
    QCOMPARE(ctl(fp, 16).shape, SurfaceLayout::Round);
    QCOMPARE(fp.indexOf(160), fp.indexOf(16));
    QCOMPARE(ctl(fp, 222).shape, SurfaceLayout::Round);
    QVERIFY(ctl(fp, 222).rect.width() > ctl(fp, 221).rect.width());
    QVERIFY(ctl(fp, 513).rect.right() < ctl(fp, 144).rect.left());         // the fader on the left

    // The APC40 mkII: the clip grid 8 x 5 of wide pads, eight track and eight device knobs.
    const SurfaceLayout apc = SurfaceLayout::forProfile(kApc);
    QSet<int> cols, rows;
    for (quint32 ch = 128; ch <= 167; ++ch)
    {
        const SurfaceLayout::Control &c = ctl(apc, ch);
        QCOMPARE(c.kind, SurfaceLayout::Pad);
        QVERIFY2(c.rect.width() > c.rect.height() * 2, "a clip pad is wide");
        cols << qRound(c.rect.x() * 100);
        rows << qRound(c.rect.y() * 100);
    }
    QCOMPARE(cols.size(), 8);
    QCOMPARE(rows.size(), 5);
    int knobs = 0;
    for (const SurfaceLayout::Control &c : apc.controls())
        if (c.turns())
        {
            ++knobs;
            QCOMPARE(c.shape, SurfaceLayout::Round);
            QCOMPARE(c.rect.width(), c.rect.height());
        }
    QCOMPARE(knobs, 18);        // 8 track, 8 device, tempo, cue level
    QVERIFY(ctl(apc, 7).rect.height() > ctl(apc, 7).rect.width() * 2.5);
    QVERIFY(ctl(apc, 15).kind == SurfaceLayout::CrossFader);

    // The APC64: 8 x 8 square pads, the touch strips 1-4 left and 5-8 right of them.
    const SurfaceLayout a64 = SurfaceLayout::forProfile(QStringLiteral("Akai APC64"));
    int pads = 0;
    double padLeft = 1e9, padRight = 0;
    for (const SurfaceLayout::Control &c : a64.controls())
        if (c.kind == SurfaceLayout::Pad)
        {
            ++pads;
            QVERIFY(qAbs(c.rect.width() - c.rect.height()) < 0.12);
            padLeft = qMin(padLeft, c.rect.left());
            padRight = qMax(padRight, c.rect.right());
        }
    QCOMPARE(pads, 64);
    for (quint32 ch : { 513u, 4609u, 8705u, 12801u })
        QVERIFY(ctl(a64, ch).rect.right() <= padLeft);
    for (quint32 ch : { 16897u, 20993u, 25089u, 29185u })
        QVERIFY(ctl(a64, ch).rect.left() >= padRight);
    QCOMPARE(ctl(a64, 90).shape, SurfaceLayout::Round);

    // A few printed groups on each, outlined by the app.
    for (const SurfaceLayout &l : { pmj, fp, apc, a64 })
        QVERIFY(l.sections().size() >= 2);
}

void SurfaceMap_Test::theControlTableAndTheDrawingSelectEachOther()
{
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc, profile("Akai-APC40-mkII.qxi"));
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, quint32(kClip11), ControlMap::function(scene(m_doc, QStringLiteral("Warm wash"))->id()), ControlMap::Toggle });
    SurfaceMaps maps(m_doc);
    maps.resize(1200, 900);
    maps.show();
    settle();
    SurfaceMapView *mv = maps.view(0);
    SurfaceControlTable *t = maps.table(0);
    QVERIFY(mv != nullptr && t != nullptr);
    QCOMPARE(SurfaceControlTable::columns(),
             QStringList({ QStringLiteral("Control"), QStringLiteral("Page"), QStringLiteral("Job"),
                           QStringLiteral("Lamp"), QStringLiteral("Picks up") }));
    QCOMPARE(t->table()->columnCount(), 5);
    QCOMPARE(t->table()->rowCount(), int(mv->layout().controls().size()));
    // A row says the control, its job and where a fader picks up.
    const int row = t->rowOfChannel(kClip11);
    QVERIFY(row >= 0);
    QCOMPARE(t->text(row, 0), QStringLiteral("Clip 1-1"));
    QVERIFY2(t->text(row, 2).contains(QStringLiteral("Warm wash")), qPrintable(t->text(row, 2)));
    QCOMPARE(t->text(t->rowOfChannel(kMasterFader), 4), QStringLiteral("Yes"));   // the Grand Master picks up
    QCOMPARE(t->text(t->rowOfChannel(kClip12), 2), QStringLiteral("Free"));

    // Filter: only what matches, anywhere in the row.
    const int all = t->visibleRows();
    t->filterBox()->setText(QStringLiteral("Warm"));
    settle();
    QCOMPARE(t->visibleRows(), 1);
    t->filterBox()->setText(QStringLiteral("clip 1-"));
    QCOMPARE(t->visibleRows(), 8);
    t->filterBox()->clear();
    QCOMPARE(t->visibleRows(), all);

    // Sort by the Control column, numbers as numbers ("Clip 1-2" before "Clip 1-10").
    t->table()->sortByColumn(0, Qt::DescendingOrder);
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);     // as the table's own sort
    for (int r = 1; r < t->table()->rowCount(); ++r)
        QVERIFY2(collator.compare(t->text(r - 1, 0), t->text(r, 0)) >= 0, qPrintable(t->text(r, 0)));
    t->table()->sortByColumn(2, Qt::AscendingOrder);
    QVERIFY(t->table()->horizontalHeader()->sortIndicatorSection() == 2);

    // A row selected is the control selected on the drawing, and back.
    QSignalSpy sel(mv, &SurfaceMapView::selectionChanged);
    t->table()->selectRow(t->rowOfChannel(kClip12));
    settle();
    QCOMPARE(mv->selected(), qint64(kClip12));
    QCOMPARE(sel.count(), 1);
    QTest::mouseClick(mv, Qt::LeftButton, Qt::NoModifier, mv->cellRect(kClip11).center());
    settle();
    QCOMPARE(mv->selected(), qint64(kClip11));
    QCOMPARE(t->table()->selectedItems().first()->data(Qt::UserRole).toUInt(), kClip11);
    QCOMPARE(t->text(t->table()->currentRow(), 0), QStringLiteral("Clip 1-1"));
    // Still selected after the jobs are read again (a sorted table is rebuilt).
    mv->refresh();
    settle();
    QCOMPARE(t->table()->selectedItems().first()->data(Qt::UserRole).toUInt(), kClip11);
    while (QWidget *pop = QApplication::activePopupWidget())
        pop->close();
}

void SurfaceMap_Test::aClickGivesAControlAJob()
{
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc, profile("Akai-APC40-mkII.qxi"));
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    SurfaceMaps maps(m_doc);
    settle();
    DeskRecord *desk = m_doc->deskRecord();
    SurfacePages *pages = m_doc->controlMap()->surfacePages();
    QVERIFY(desk->isLive(DeskRecord::Go));
    const quint32 tap = 227, nudge = 228, knob1 = 48, fader2 = 4103;

    auto keys = [&maps](quint32 ch) {
        QStringList k;
        for (const SurfaceMaps::Job &j : maps.jobsFor(0, ch))
            k << j.key;
        return k;
    };
    // By kind: a key has the desk's jobs and the pages; a fader the Grand Master; a knob the page's.
    const QStringList keyJobs = keys(tap);
    for (const char *k : { "go", "back", "stop", "page-prev", "page-next", "page:0", "other", "none" })
        QVERIFY2(keyJobs.contains(QLatin1String(k)), k);
    QVERIFY(!keyJobs.contains(QStringLiteral("gm")));
    QCOMPARE(keys(fader2), QStringList({ QStringLiteral("gm"), QStringLiteral("other"), QStringLiteral("none") }));
    QCOMPARE(keys(knob1), QStringList({ QStringLiteral("other"), QStringLiteral("none") }));
    // The jobs now are ticked: PLAY is GO.
    bool playIsGo = false;
    for (const SurfaceMaps::Job &j : maps.jobsFor(0, kPlay))
        playIsGo = playIsGo || (j.key == QLatin1String("go") && j.checked);
    QVERIFY(playIsGo);
    QMenu *menu = maps.jobMenu(0, tap);
    QVERIFY(menu != nullptr);
    QStringList texts;
    for (QAction *a : menu->actions())
        texts << a->text();
    QVERIFY2(texts.contains(QStringLiteral("GO")), qPrintable(texts.join('|')));
    QVERIFY(texts.contains(QStringLiteral("Other… (Assign to Control)")));
    QVERIFY(texts.contains(QStringLiteral("No job")));
    delete menu;

    // GO moves to Tap Tempo, PLAY lets go of it; Undo puts it back.
    QVERIFY(maps.applyJob(0, tap, QStringLiteral("go")));
    QCOMPARE(desk->binding(DeskRecord::Go).channel, tap);
    QCOMPARE(maps.view(0)->role(tap), QStringLiteral("go"));
    QCOMPARE(maps.view(0)->jobText(tap), QStringLiteral("GO"));
    QVERIFY(maps.view(0)->role(kPlay).isEmpty());
    QVERIFY(!maps.undoText().isEmpty());
    QVERIFY(maps.undoLast());
    QCOMPARE(desk->binding(DeskRecord::Go).channel, kPlay);
    QVERIFY(maps.undoText().isEmpty());

    // The Grand Master takes a fader, not a key.
    QString why;
    QVERIFY(!maps.applyJob(0, tap, QStringLiteral("gm"), &why));
    QVERIFY2(!why.isEmpty(), "no reason given");
    QVERIFY(maps.applyJob(0, fader2, QStringLiteral("gm")));
    QCOMPARE(desk->binding(DeskRecord::GrandMaster).channel, fader2);
    QVERIFY(maps.undoLast());
    QCOMPARE(desk->binding(DeskRecord::GrandMaster).channel, kMasterFader);

    // No job: what the control had is gone; Undo gives it back.
    QVERIFY(maps.applyJob(0, kPlay, QStringLiteral("none")));
    QVERIFY(!desk->binding(DeskRecord::Go).isSet());
    QVERIFY(maps.view(0)->role(kPlay).isEmpty());
    QVERIFY(maps.undoLast());
    QCOMPARE(desk->binding(DeskRecord::Go).channel, kPlay);

    // A desk control is not a page key until it is let go.
    QVERIFY(!maps.applyJob(0, kPlay, QStringLiteral("page-next"), &why));
    QVERIFY2(why.contains(QStringLiteral("No job")), qPrintable(why));
    QVERIFY(pages->keyJob(0, kPlay).isEmpty());

    // In Operate, with a PIN, a job asks for it; a wrong one changes nothing.
    QVERIFY(m_doc->operateSettings()->setPin(QStringLiteral("1234")));
    m_doc->setMode(Doc::Operate);
    SurfaceMaps::setPinAsker(&pinAsker);
    g_pinAsked = 0;
    g_pinAnswer = false;
    QVERIFY(!maps.applyJob(0, tap, QStringLiteral("go"), &why));
    QCOMPARE(g_pinAsked, 1);
    QCOMPARE(desk->binding(DeskRecord::Go).channel, kPlay);
    g_pinAnswer = true;
    QVERIFY(maps.applyJob(0, tap, QStringLiteral("go")));
    QCOMPARE(g_pinAsked, 2);
    QCOMPARE(desk->binding(DeskRecord::Go).channel, tap);
    QVERIFY(desk->isLocked());      // one PIN, one change
    // In Design the show's PIN is not asked.
    m_doc->setMode(Doc::Design);
    g_pinAsked = 0;
    QVERIFY(maps.applyJob(0, nudge, QStringLiteral("back")));
    QCOMPARE(g_pinAsked, 0);
    SurfaceMaps::setPinAsker(nullptr);
    while (QWidget *pop = QApplication::activePopupWidget())
        pop->close();
}

void SurfaceMap_Test::pagesHaveKeysTheMapChooses()
{
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc, profile("Akai-APC40-mkII.qxi"));
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    SurfaceMaps maps(m_doc);
    SurfaceChips chips(m_doc);
    chips.show();
    settle();
    SurfacePages *pages = m_doc->controlMap()->surfacePages();
    const quint32 tap = 227, nudge = 228;
    const int count = int(pages->pageSet(0).pages().size());
    QVERIFY(count >= 2);
    QCOMPARE(pages->page(0), 0);

    // Page ▶ and ◀ on two keys the board's set did not use.
    QVERIFY(maps.applyJob(0, tap, QStringLiteral("page-next")));
    QVERIFY(maps.applyJob(0, nudge, QStringLiteral("page-prev")));
    QCOMPARE(pages->stepKey(0, 1), tap);
    QCOMPARE(pages->stepKey(0, -1), nudge);
    QCOMPARE(maps.view(0)->jobText(tap), QStringLiteral("Next page"));
    QCOMPARE(maps.view(0)->jobText(nudge), QStringLiteral("Previous page"));
    QCOMPARE(maps.view(0)->group(tap), SurfaceMapView::Page);
    // Lit while there is a page that way.
    uchar sent = 0;
    QVERIFY(m_doc->inputOutputMap()->lastFeedback(0, tap, &sent));
    QVERIFY(sent > 0);
    // They step the pages, round.
    QVERIFY(apc.tap(QStringLiteral("#%1").arg(tap)));
    settle();
    QCOMPARE(pages->page(0), 1);
    QVERIFY(apc.tap(QStringLiteral("#%1").arg(nudge)));
    settle();
    QCOMPARE(pages->page(0), 0);
    QVERIFY(apc.tap(QStringLiteral("#%1").arg(nudge)));
    settle();
    QCOMPARE(pages->page(0), count - 1);

    // A key of its own for a page.
    QVERIFY(maps.applyJob(0, 229, QStringLiteral("page:0")));
    QCOMPARE(pages->pageKey(0, 0), 229u);
    QVERIFY(apc.tap(QStringLiteral("#229")));
    settle();
    QCOMPARE(pages->page(0), 0);
    // A key has one job: Next page moved off Tap Tempo onto it.
    QVERIFY(maps.applyJob(0, 229, QStringLiteral("page-next")));
    QCOMPARE(pages->pageKey(0, 0), 0xFFFFFFFFu);
    QCOMPARE(pages->stepKey(0, 1), 229u);
    QVERIFY(maps.applyJob(0, 229, QStringLiteral("none")));
    QCOMPARE(pages->stepKey(0, 1), 0xFFFFFFFFu);
    QVERIFY(maps.undoLast());
    QCOMPARE(pages->stepKey(0, 1), 229u);

    // Kept for this computer: another read of the boards finds the same keys.
    pages->rescan();
    QCOMPARE(pages->stepKey(0, -1), nudge);

    // The chip says the page, in Operate too, and its page name picks one.
    QCOMPARE(chips.chip(0)->text(), QStringLiteral("APC40 · %1").arg(pages->pageName(0)));
    m_doc->setMode(Doc::Operate);
    settle();
    QVERIFY(chips.chip(0)->text().contains(pages->pageName(0)));
    m_doc->setMode(Doc::Design);
    settle();
    // A page change lights the chip for a moment, and the text is the new page (no lamps, no screen needed).
    pages->setPage(0, 1);
    settle();
    QVERIFY(chips.isFlashing(0));
    QCOMPARE(chips.chip(0)->text(), QStringLiteral("APC40 · %1").arg(pages->pageName(0)));
    QVERIFY(chips.chip(0)->styleSheet().contains(QStringLiteral("5b4a9e")));
    QTRY_VERIFY_WITH_TIMEOUT(!chips.isFlashing(0), 2500);
    QMenu *menu = chips.pageMenu(0);
    QVERIFY(menu != nullptr);
    QCOMPARE(menu->actions().size(), count);
    menu->actions().last()->trigger();
    QCOMPARE(pages->page(0), count - 1);
    delete menu;
    QSignalSpy asked(&chips, &SurfaceChips::mapRequested);
    QToolButton *chip = chips.chip(0);
    QTest::mouseClick(chip, Qt::LeftButton, Qt::NoModifier, QPoint(chip->width() - 10, chip->height() / 2));
    QCOMPARE(asked.count(), 0);     // on the page name: the menu, not the map
    while (QWidget *pop = QApplication::activePopupWidget())
        pop->close();
    QTest::mouseClick(chip, Qt::LeftButton, Qt::NoModifier, QPoint(4, chip->height() / 2));
    QCOMPARE(asked.count(), 1);
}

void SurfaceMap_Test::keyLabelsAtTheRealKeySizes()
{
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc, profile("Akai-APC40-mkII.qxi"));
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    SurfaceMaps maps(m_doc);
    settle();
    SurfaceMapView *mv = maps.view(0);
    const SurfaceLayout &layout = mv->layout();
    QVERIFY(layout.unitMm() > 0);
    const QList<SurfaceKeyLabels::Page> pages = SurfaceKeyLabels::pages(mv);
    QCOMPARE(pages.size(), int(m_doc->controlMap()->surfacePages()->pageSet(0).pages().size()));
    QVERIFY(!pages.isEmpty());
    // Only keys and pads, at the drawing's size in millimetres.
    int keys = 0;
    for (const SurfaceLayout::Control &c : layout.controls())
        if (c.kind == SurfaceLayout::Button || c.kind == SurfaceLayout::Pad)
            ++keys;
    QCOMPARE(pages.first().keys.size(), keys);
    for (const SurfaceKeyLabels::Key &k : pages.first().keys)
    {
        const SurfaceLayout::Control &c = ctl(layout, k.channel);
        QCOMPARE(k.mm.width(), c.rect.width() * layout.unitMm());
        QVERIFY(k.mm.width() > 5 && k.mm.height() > 5);
    }
    // A page says its own jobs: the page key of Beam is on the Beam page.
    const PageSet set = m_doc->controlMap()->surfacePages()->pageSet(0);
    for (int p = 0; p < pages.size(); ++p)
        for (const SurfaceKeyLabels::Key &k : pages.at(p).keys)
            if (set.labels(p).contains(k.channel))
                QCOMPARE(k.text, set.labels(p).value(k.channel));
    // The desk's PLAY says GO on every page.
    for (const SurfaceKeyLabels::Page &pg : pages)
        for (const SurfaceKeyLabels::Key &k : pg.keys)
            if (k.channel == kPlay)
                QCOMPARE(k.text, QStringLiteral("GO"));

    // The sheet is a PDF.
    QTemporaryDir out;
    const QString pdf = out.filePath(QStringLiteral("sheet.pdf"));
    QString error;
    QVERIFY2(SurfaceKeyLabels::writeSheet(pdf, QStringLiteral("APC40 mkII"), pages, &error), qPrintable(error));
    QFile f(pdf);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray head = f.read(8);
    QVERIFY2(head.startsWith("%PDF"), head.constData());
    QVERIFY(f.size() > 2000);
    f.close();

    // Tape: 180 dpi, the tape's printable width high, the key's length long.
    const SurfaceKeyLabels::Key key = pages.first().keys.first();
    for (int mm : SurfaceKeyLabels::tapeWidths())
    {
        const QImage img = SurfaceKeyLabels::tapeLabel(key, mm);
        QCOMPARE(img.height(), int(SurfaceKeyLabels::printableMm(mm) * 180 / 25.4));
        QVERIFY(img.width() >= int(key.mm.width() * 180 / 25.4) - 1);
    }
    QCOMPARE(SurfaceKeyLabels::printableMm(12), 9.0);
    QCOMPARE(SurfaceKeyLabels::printableMm(18), 12.0);
    QCOMPARE(SurfaceKeyLabels::printableMm(24), 18.0);

    // The dialog: the sheet or each tape, the page, the sizes in the units chosen.
    SurfaceKeyLabelsDialog dlg(mv);
    QCOMPARE(dlg.tapeMm(), 0);
    QCOMPARE(dlg.keys().size(), keys * int(pages.size()));
    MeasurementUnits::setPreferredLength(MeasurementUnits::Millimetres);
    QVERIFY2(dlg.sizeLine().contains(QStringLiteral("mm")), qPrintable(dlg.sizeLine()));
    dlg.setTapeMm(12);
    MeasurementUnits::setPreferredLength(MeasurementUnits::Feet);
    dlg.setTapeMm(24);
    dlg.setTapeMm(12);
    QVERIFY2(!dlg.sizeLine().contains(QStringLiteral("mm")), qPrintable(dlg.sizeLine()));   // feet and inches, as Settings says
    QVERIFY(dlg.sizeLine().contains(QStringLiteral("dpi")));
    QStringList made;
    QVERIFY2(dlg.savePngs(out.filePath(QStringLiteral("png")), &made, &error), qPrintable(error));
    QCOMPARE(made.size(), dlg.keys().size());
    QVERIFY(QImage(made.first()).height() == int(SurfaceKeyLabels::printableMm(12) * 180 / 25.4));
    const QString tapePdf = out.filePath(QStringLiteral("tape.pdf"));
    QVERIFY2(dlg.savePdf(tapePdf, &error), qPrintable(error));
    QVERIFY(QFile(tapePdf).size() > 2000);
    MeasurementUnits::setPreferredLength(MeasurementUnits::Feet);
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

namespace
{
QAction *menuAction(QMenu *menu, const QString &name)
{
    for (QAction *a : menu->findChildren<QAction *>())
        if (a->objectName() == name)
            return a;
    for (QAction *a : menu->actions())
        if (a->objectName() == name)
            return a;
    return nullptr;
}
}

void SurfaceMap_Test::theChipMenuDoesWhatTheBoardNeeds()
{
    m_stub->m_hasFeedback = true;
    m_stub->m_acceptShowControl = true;
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc, profile("Akai-APC40-mkII.qxi"));
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    SurfaceChips chips(m_doc);
    settle();
    QToolButton *mark = chips.menuButton(0);
    QVERIFY(mark != nullptr);
    QCOMPARE(mark->text(), QStringLiteral("▾"));
    // The chip's own text is as it was; a click on it still opens the map.
    QCOMPARE(chips.chip(0)->text(), QStringLiteral("APC40 · Move"));
    QSignalSpy map(&chips, &SurfaceChips::mapRequested);
    chips.chip(0)->click();
    QCOMPARE(map.count(), 1);

    std::unique_ptr<QMenu> menu(chips.boardMenu(0));
    QVERIFY(menu != nullptr);
    QStringList titles;
    for (QAction *a : menu->actions())
        if (!a->isSeparator())
            titles << a->text();
    QCOMPARE(titles, QStringList({ "Map", "Send Init Again", "Profile", "Feedback", "Lamp Test",
                                   QString::fromUtf8("Board Log…"), QString::fromUtf8("Set Up…") }));
    // Send Init Again: its Ableton mode, out of its port.
    m_stub->m_showControlSent.clear();
    menuAction(menu.get(), "chipMenuInit")->trigger();
    QCOMPARE(m_stub->m_showControlSent.size(), 1);
    QCOMPARE(QString::fromLatin1(m_stub->m_showControlSent.first().second.toHex(' ')),
             QString("f0 47 7f 29 60 00 04 41 09 00 05 f7"));
    // Profile ▸: the one in use ticked.
    QMenu *prof = menu->findChild<QMenu *>("chipMenuProfile");
    QVERIFY(prof != nullptr);
    QAction *ticked = nullptr;
    for (QAction *a : prof->actions())
        if (a->isChecked())
            ticked = a;
    QVERIFY(ticked != nullptr);
    QCOMPARE(ticked->text(), kApc);
    // Feedback ▸: paired to its own port; None lets it go, Pair pairs it again.
    QMenu *fb = menu->findChild<QMenu *>("chipMenuFeedback");
    QVERIFY(fb != nullptr);
    QCOMPARE(fb->actions().first()->text(), QStringLiteral("Paired to APC40 mkII"));
    menuAction(menu.get(), "chipMenuFeedbackNone")->trigger();
    QVERIFY(m_doc->inputOutputMap()->feedbackPatch(0) == nullptr
            || !m_doc->inputOutputMap()->feedbackPatch(0)->isPatched());
    std::unique_ptr<QMenu> again(chips.boardMenu(0));
    QAction *lamp = menuAction(again.get(), "chipMenuLampTest");
    QVERIFY(lamp != nullptr && !lamp->isEnabled());         // no feedback line: no lamps to test
    QVERIFY2(lamp->toolTip().contains("Pair first"), qPrintable(lamp->toolTip()));
    m_stub->m_showControlSent.clear();
    menuAction(again.get(), "chipMenuPair")->trigger();
    QCOMPARE(m_doc->inputOutputMap()->feedbackPatch(0)->outputName(), QStringLiteral("APC40 mkII"));
    QVERIFY(!m_stub->m_showControlSent.isEmpty());          // its init again, now it has its line
    // Board Log… and Lamp Test ask for their windows.
    QSignalSpy log(&chips, &SurfaceChips::boardLogRequested);
    QSignalSpy test(&chips, &SurfaceChips::lampTestRequested);
    QSignalSpy setUp(&chips, &SurfaceChips::setUpRequested);
    std::unique_ptr<QMenu> third(chips.boardMenu(0));
    menuAction(third.get(), "chipMenuBoardLog")->trigger();
    menuAction(third.get(), "chipMenuLampTest")->trigger();
    menuAction(third.get(), "chipMenuSetUp")->trigger();
    QCOMPARE(log.count(), 1);
    QCOMPARE(test.count(), 1);
    QCOMPARE(setUp.count(), 1);
    // A right-click on the chip opens the same menu.
    QContextMenuEvent ctx(QContextMenuEvent::Mouse, QPoint(4, 4), chips.chip(0)->mapToGlobal(QPoint(4, 4)));
    QApplication::sendEvent(chips.chip(0), &ctx);
    QVERIFY(chips.lastMenu() != nullptr);
    QCOMPARE(chips.lastMenu()->objectName(), QStringLiteral("footerSurfaceMenu0"));
    chips.lastMenu()->close();
    m_stub->m_hasFeedback = false;
    m_stub->m_acceptShowControl = false;
}

void SurfaceMap_Test::aMovedFaderKeepsItsName()
{
    SimMidiDevice mini(m_doc, m_stub, QStringLiteral("APC mini mk2 Control"), 0, QStringLiteral("Akai APC Mini mk2"),
                       profile("Akai-APCMini-mk2.qxi"));
    QVERIFY2(mini.plug(), qPrintable(mini.lastError()));
    ControlSurfacesPanel hub(m_doc);
    settle();
    SurfaceMapView *mv = hub.surfaceMaps()->view(0);
    QVERIFY(mv != nullptr);
    // A fader with no desk role: its words on the map (its page job, else its name).
    quint32 fader = 0xFFFFFFFF;
    for (const SurfaceLayout::Control &c : mv->layout().controls())
        if (c.isFader() && mv->role(c.channel).isEmpty())
        {
            fader = c.channel;
            break;
        }
    QVERIFY(fader != 0xFFFFFFFF);
    const QString before = mv->drawnWords(fader);
    QVERIFY(!before.isEmpty());
    mv->noteInput(0, fader, 128);
    settle();
    QCOMPARE(mv->drawnWords(fader), before);         // not "50 %"
    QCOMPARE(mv->valueText(fader), QStringLiteral("50 %"));
}

void SurfaceMap_Test::everyPluggedInDeviceIsUnderControlSurfaces()
{
    m_doc->inputOutputMap()->loadProfiles(QDir(QStringLiteral(QLC_TEST_RESOURCE_ROOT) + QStringLiteral("inputprofiles/")));
    InputOutputMap::setNotSurface(QStringLiteral("Mystery Keys"), false);
    SurfaceAutoPatch::setDeclined(QStringLiteral("APC64 DAW (APC64)"), false);
    m_stub->m_linesAreHardware = true;
    m_stub->m_networkDmx = false;
    m_stub->m_hasFeedback = true;
    m_stub->m_lineNames = QStringList({ "APC64 DAW (APC64)", "APC64 Notes (APC64)", "APC64 MIDI (APC64)",
                                        "APC64 Custom (APC64)", "Mystery Keys" });
    emit m_stub->configurationChanged();
    ConnectionsTree tree(m_doc);
    tree.setShowControlPanel(new QWidget(&tree));
    tree.setShowControlRows({ { "surfaces", "Control surfaces", "" } });
    tree.refresh();
    QTreeWidget *tw = tree.findChild<QTreeWidget *>();
    QVERIFY(tw != nullptr);
    auto surfaces = [tw]() -> QTreeWidgetItem * {
        QTreeWidgetItemIterator it(tw);
        for (; *it != nullptr; ++it)
            if ((*it)->data(0, Qt::UserRole + 2).toString() == QLatin1String("surfaces")
                && (*it)->data(0, Qt::UserRole + 1).toInt() == 10)
                return *it;
        return nullptr;
    };
    auto names = [](QTreeWidgetItem *row) {
        QStringList out;
        for (int i = 0; i < row->childCount(); ++i)
            out << row->child(i)->text(0);
        return out;
    };
    // Before any patch: the APC64 once (its other ports under it), the unknown box.
    QTreeWidgetItem *row = surfaces();
    QVERIFY(row != nullptr);
    QCOMPARE(names(row), QStringList({ "APC64 DAW (APC64): Akai APC64, not set up", "Mystery Keys: not set up" }));
    QTreeWidgetItem *apc = row->child(0);
    QCOMPARE(names(apc), QStringList({ "APC64 Notes (APC64)", "APC64 MIDI (APC64)", "APC64 Custom (APC64)" }));
    QCOMPARE(apc->child(0)->text(1), QStringLiteral("part of Akai APC64 (not used by qlcconsole)"));
    // The hub does not nag about the other ports.
    ControlSurfacesPanel hub(m_doc);
    QCOMPARE(hub.status("devices"), QStringLiteral("APC64 DAW (APC64): not set up\nMystery Keys: not set up"));

    // "Not a Control Surface": gone, on this computer.
    std::unique_ptr<QMenu> mystery(tree.surfaceDeviceMenu(row->child(1)));
    QVERIFY(mystery != nullptr);
    menuAction(mystery.get(), "surfaceDeviceNot")->trigger();
    tree.refresh();
    row = surfaces();
    QCOMPARE(names(row), QStringList({ "APC64 DAW (APC64): Akai APC64, not set up" }));
    QVERIFY(InputOutputMap::isNotSurface(QStringLiteral("Mystery Keys")));

    // One click on the APC64: "Use as Akai APC64" patches it.
    std::unique_ptr<QMenu> use(tree.surfaceDeviceMenu(row->child(0)));
    QAction *useApc = menuAction(use.get(), "surfaceDeviceUse");
    QVERIFY(useApc != nullptr);
    QCOMPARE(useApc->text(), QStringLiteral("Use as Akai APC64"));
    useApc->trigger();
    tree.refresh();
    row = surfaces();
    QCOMPARE(row->childCount(), 1);
    QCOMPARE(row->child(0)->data(0, Qt::UserRole + 1).toInt(), 12);
    QVERIFY2(row->child(0)->text(1).contains("Akai APC64") && row->child(0)->text(1).contains("feedback yes")
             && row->child(0)->text(1).contains("init: Its display"), qPrintable(row->child(0)->text(1)));
    QCOMPARE(row->child(0)->childCount(), 3);

    // Unplugged: greyed, "not connected".
    m_stub->m_lineNames = QStringList() << QStringLiteral("(no device)");
    emit m_stub->configurationChanged();
    settle();
    tree.refresh();
    row = surfaces();
    QTreeWidgetItem *gone = nullptr;
    for (int i = 0; i < row->childCount(); ++i)
        if (row->child(i)->text(0).contains("APC64 DAW (APC64)"))
            gone = row->child(i);
    QVERIFY2(gone != nullptr, qPrintable(names(row).join(" | ")));
    QVERIFY2(gone->text(1).contains("not connected"), qPrintable(gone->text(1)));
    QCOMPARE(gone->foreground(0).color(), tw->palette().color(QPalette::Disabled, QPalette::Text));
    InputOutputMap::setNotSurface(QStringLiteral("Mystery Keys"), false);
    m_stub->m_hasFeedback = false;
}

void SurfaceMap_Test::theBoardLogDialogCopiesTheLog()
{
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc, profile("Akai-APC40-mkII.qxi"));
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    m_doc->surfaceSetup()->boardLog()->note(0, QStringLiteral("hello from the bench"));
    BoardLogDialog d(m_doc, 0);
    d.show();
    settle();
    QCOMPARE(d.windowTitle(), QStringLiteral("Board Log: Akai APC40 mkII"));
    QPlainTextEdit *text = d.findChild<QPlainTextEdit *>("boardLogText");
    QVERIFY(text != nullptr);
    QVERIFY(text->toPlainText().contains("hello from the bench"));
    QVERIFY(text->toPlainText().contains("Input: APC40 mkII"));
    d.findChild<QPushButton *>("boardLogCopy")->click();
    QCOMPARE(QApplication::clipboard()->text(), d.logText());
    // No feedback line: the lamp test says why.
    m_doc->inputOutputMap()->setOutputPatch(0, KOutputNone, QString(), 0, true);
    d.findChild<QPushButton *>("boardLogLampTest")->click();
    QVERIFY2(d.findChild<QLabel *>("boardLogStep")->text().contains("no feedback line"),
             qPrintable(d.findChild<QLabel *>("boardLogStep")->text()));
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
    SimMidiDevice fp2(m_doc, m_stub, QStringLiteral("PreSonus FP2"), 4, QStringLiteral("PreSonus FaderPort 2"),
                      profile("PreSonus-FaderPort-2.qxi"));
    QVERIFY2(fp2.plug(), qPrintable(fp2.lastError()));
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
    SimMidiDevice mini(m_doc, m_stub, QStringLiteral("APC mini mk2 Control"), 5, QStringLiteral("Akai APC Mini mk2"),
                       profile("Akai-APCMini-mk2.qxi"));
    QVERIFY2(mini.plug(), qPrintable(mini.lastError()));
    settle();
    const QList<QPair<quint32, QString>> boards = { { 5, QStringLiteral("apcmini-mk2") }, { 0, QStringLiteral("apc40") }, { 1, QStringLiteral("pmj") },
                                                    { 2, QStringLiteral("maizedmx") }, { 3, QStringLiteral("apc64") }, { 4, QStringLiteral("faderport2") } };
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
            if (b.first == 4)
                fp2.set(QStringLiteral("Fader"), 80);
            settle();
            const QString file = QStringLiteral("%1/%2-surfacemap-%3-%4x%5.png")
                                     .arg(dir, prefix, b.second).arg(size.width()).arg(size.height());
            QVERIFY(scroll.grab().save(file));
            // The whole drawing, whatever the window shows of it.
            if (SurfaceMapView *whole = hub->surfaceMaps()->view(b.first))
                QVERIFY(whole->grab().save(QStringLiteral("%1/%2-board-%3-%4x%5.png")
                                               .arg(dir, prefix, b.second).arg(size.width()).arg(size.height())));
            // And the table above it.
            if (SurfaceControlTable *t = hub->surfaceMaps()->table(b.first))
                QVERIFY(t->grab().save(QStringLiteral("%1/%2-table-%3-%4x%5.png")
                                           .arg(dir, prefix, b.second).arg(size.width()).arg(size.height())));
            apc.release(QStringLiteral("#%1").arg(kClip11 + 1));
            maize.release(QStringLiteral("X3"));
        }
    }
}

void SurfaceMap_Test::boardShots()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set");
    QDir().mkpath(dir);
    m_doc->inputOutputMap()->loadProfiles(QDir(QStringLiteral(QLC_TEST_RESOURCE_ROOT) + QStringLiteral("inputprofiles/")));
    m_stub->m_hasFeedback = true;
    m_stub->m_acceptShowControl = true;
    SimMidiDevice fp2(m_doc, m_stub, QStringLiteral("PreSonus FP2"), 0, QStringLiteral("PreSonus FaderPort 2"),
                      profile("PreSonus-FaderPort-2.qxi"));
    QVERIFY2(fp2.plug(), qPrintable(fp2.lastError()));
    // What a fader move and a Shift+Stop look like in the log.
    fp2.raw(QByteArray::fromHex("90687f"));
    for (int v = 0x30; v < 0x48; v += 4)
        fp2.raw(QByteArray::fromHex(QByteArray("e000") + QByteArray::number(v, 16)));
    fp2.raw(QByteArray::fromHex("906800"));
    fp2.raw(QByteArray::fromHex("90467f"));
    fp2.raw(QByteArray::fromHex("905d7f"));
    fp2.raw(QByteArray::fromHex("905d00"));
    fp2.raw(QByteArray::fromHex("904600"));
    m_doc->inputOutputMap()->sendFeedBack(0, 513, 140, QVariant());
    // Plugged in, not set up: the APC64's ports and a keyboard.
    m_stub->m_lineNames << QStringLiteral("APC64 DAW (APC64)") << QStringLiteral("APC64 Notes (APC64)")
                        << QStringLiteral("APC64 MIDI (APC64)") << QStringLiteral("APC64 Custom (APC64)")
                        << QStringLiteral("Launchkey MK4 49 MIDI Out") << QStringLiteral("USB2.0-MIDI Port 1");
    emit m_stub->configurationChanged();
    settle();
    for (const QSize &size : { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) })
    {
        const QString tag = QStringLiteral("%1x%2").arg(size.width()).arg(size.height());
        {
            BoardLogDialog d(m_doc, 0);
            d.resize(qMin(size.width() - 100, 820 + (size.width() - 1024) / 3), qMin(size.height() - 100, 520));
            d.show();
            QVERIFY(QTest::qWaitForWindowExposed(&d));
            settle();
            QVERIFY(d.grab().save(dir + QStringLiteral("/board-log-%1.png").arg(tag)));
        }
        {
            ConnectionsTree tree(m_doc);
            tree.setShowControlPanel(new QWidget(&tree));
            tree.setShowControlRows({ { "surfaces", "Control surfaces", "" } });
            tree.resize(size);
            tree.show();
            QVERIFY(QTest::qWaitForWindowExposed(&tree));
            tree.refresh();
            if (QTreeWidget *tw = tree.findChild<QTreeWidget *>())
                tw->expandAll();
            settle();
            QVERIFY(tree.grab().save(dir + QStringLiteral("/control-surfaces-devices-%1.png").arg(tag)));
        }
        {
            QMainWindow mw;
            FooterBar *bar = new FooterBar(&mw);
            mw.setStatusBar(bar);
            mw.setCentralWidget(new QWidget(&mw));
            bar->addWidget(new QLabel(QStringLiteral("Show Status: ready"), bar));
            SurfaceChips *chips = new SurfaceChips(m_doc, bar);
            bar->addPermanentWidget(chips);
            bar->addPermanentWidget(new QLabel(QStringLiteral("DESIGN"), bar));
            mw.resize(size);
            mw.show();
            QVERIFY(QTest::qWaitForWindowExposed(&mw));
            bar->showMessage(QStringLiteral("Design mode. The footer does not go live: use Control ▸ Operate (⌘F12)."));
            bar->showMessage(QStringLiteral("FaderPort: Shift+Stop stops."));
            settle();
            QVERIFY(bar->grab().save(dir + QStringLiteral("/footer-one-message-%1.png").arg(tag)));
            std::unique_ptr<QMenu> menu(chips->boardMenu(0));
            QVERIFY(menu != nullptr);
            menu->popup(chips->mapToGlobal(QPoint(0, 0)));
            settle();
            QVERIFY(menu->grab().save(dir + QStringLiteral("/board-menu-%1.png").arg(tag)));
            menu->close();
        }
    }
    m_stub->m_hasFeedback = false;
    m_stub->m_acceptShowControl = false;
}

QTEST_MAIN(SurfaceMap_Test)
