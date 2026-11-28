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
#include "fixture.h"
#include "fixturegroup.h"
#include "showtreecontroller.h"
#include "showcontrol.h"
#include "programmercontroller.h"
#include "speedmasters.h"
#include "effectscriptrunner.h"
#include "universe.h"
#include "chaser.h"
#include <QBuffer>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
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
    // A fader: the Grand Master first, then the fader jobs (Q355), never a key's.
    const QStringList faderJobs = keys(fader2);
    QCOMPARE(faderJobs.first(), QStringLiteral("gm"));
    QVERIFY(faderJobs.contains(QStringLiteral("size")));
    QVERIFY(!faderJobs.contains(QStringLiteral("go")));
    QCOMPARE(faderJobs.mid(faderJobs.size() - 2), QStringList({ QStringLiteral("other"), QStringLiteral("none") }));
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

namespace
{
/* A show for the board jobs (Q355): a dimmer in group "Front", the Run tab's
   list with three songs (Opener 0.0.x, Song 1.0.x, Encore 2.0.1). */
struct JobShow
{
    Fixture *dimmer = nullptr;
    FixtureGroup *front = nullptr;
    quint32 list = 0xFFFFFFFF;
    QList<quint32> cues;
};

JobShow jobShow(Doc *doc)
{
    JobShow s;
    s.dimmer = new Fixture(doc);
    s.dimmer->setName(QStringLiteral("Dim"));
    s.dimmer->setAddress(0);
    s.dimmer->setUniverse(0);
    s.dimmer->setChannels(1);
    doc->addFixture(s.dimmer);
    s.front = new FixtureGroup(doc);
    s.front->setName(QStringLiteral("Front"));
    doc->addFixtureGroup(s.front);
    s.front->assignFixture(s.dimmer->id());
    ShowTreeController *st = doc->showTree();
    const quint32 prod = st->addProduction(QStringLiteral("Show"));
    s.list = st->addCueList(prod, QStringLiteral("Main"));
    st->setNumbering(prod, ShowTree::Numbering::ShowPartIteration);
    s.cues << st->addSection(s.list, QStringLiteral("Opener"));    // 0.0.1
    s.cues << st->addCue(s.list, 0, QStringLiteral("Walk-in"));    // 0.0.2
    s.cues << st->addSection(s.list, QStringLiteral("Song"));      // 1.0.1
    s.cues << st->addCue(s.list, 2, QStringLiteral("Verse"));      // 1.0.2
    s.cues << st->addSection(s.list, QStringLiteral("Encore"));    // 2.0.1
    doc->controlMap()->setRunTabList(s.list);
    return s;
}

/* What the dimmer at @p address of universe 0 puts out, after a frame. */
int outAt(Doc *doc, int address)
{
    QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
    for (Universe *u : ua)
        u->processFaders();
    const QByteArray out = *ua.at(0)->postGMValues();
    doc->inputOutputMap()->releaseUniverses(false);
    return int(uchar(out.at(address)));
}

bool saidContains(const QSignalSpy &spy, const QString &text)
{
    for (const QList<QVariant> &args : spy)
        if (args.at(0).toString().contains(text))
            return true;
    return false;
}
}

void SurfaceMap_Test::theBoardJobsAreInThePicker()
{
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc, profile("Akai-APC40-mkII.qxi"));
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    const JobShow show = jobShow(m_doc);
    SurfaceMaps maps(m_doc);
    settle();
    auto job = [&maps](quint32 ch, const QString &key) {
        for (const SurfaceMaps::Job &j : maps.jobsFor(0, ch))
            if (j.key == key)
                return j;
        return SurfaceMaps::Job();
    };
    // A key: every button job, the N ones in their submenus; a fader its three.
    for (const QString &k : { QStringLiteral("subgo"), QStringLiteral("song-next"), QStringLiteral("song-prev"),
                              QStringLiteral("blind"), QStringLiteral("store"), QStringLiteral("blackout"),
                              QStringLiteral("clear"), QStringLiteral("fadestop:5"), QStringLiteral("tap:0"),
                              QStringLiteral("flash:%1").arg(show.front->id()),
                              QStringLiteral("cue:%1").arg(show.cues.at(2)) })
        QVERIFY2(!job(kClip11, k).key.isEmpty(), qPrintable(k));
    QCOMPARE(job(kClip11, QStringLiteral("flash:%1").arg(show.front->id())).group, QStringLiteral("Flash a Group"));
    QCOMPARE(job(kClip11, QStringLiteral("flash:%1").arg(show.front->id())).text, QStringLiteral("Front"));
    QVERIFY2(job(kClip11, QStringLiteral("cue:%1").arg(show.cues.at(2))).text.startsWith(QStringLiteral("Cue 1.0.1")),
             qPrintable(job(kClip11, QStringLiteral("cue:%1").arg(show.cues.at(2))).text));
    // The APC40's Shift is a Shift: the guarded ones are offered, and say so.
    QVERIFY(job(kClip11, QStringLiteral("blackout")).enabled);
    QCOMPARE(job(kClip11, QStringLiteral("blackout")).text, QStringLiteral("Blackout (Shift+)"));
    for (const QString &k : { QStringLiteral("level:%1").arg(show.front->id()), QStringLiteral("speed:0"),
                              QStringLiteral("size") })
        QVERIFY2(!job(4103, k).key.isEmpty(), qPrintable(k));
    QVERIFY(job(4103, QStringLiteral("blackout")).key.isEmpty());
    QVERIFY(job(kClip11, QStringLiteral("size")).key.isEmpty());

    // The menu: the N jobs are submenus; one given is ticked, and its submenu names it.
    QVERIFY(maps.applyJob(0, kClip11, QStringLiteral("flash:%1").arg(show.front->id())));
    QVERIFY(job(kClip11, QStringLiteral("flash:%1").arg(show.front->id())).checked);
    QCOMPARE(maps.view(0)->jobText(kClip11), QStringLiteral("Flash: Front"));
    std::unique_ptr<QMenu> menu(maps.jobMenu(0, kClip11));
    QVERIFY(menu != nullptr);
    QStringList texts;
    for (QAction *a : menu->actions())
        texts << a->text();
    QVERIFY2(texts.contains(QStringLiteral("Flash a Group: Front")), qPrintable(texts.join('|')));
    QVERIFY(texts.contains(QStringLiteral("Sub GO")));
    QVERIFY(texts.contains(QStringLiteral("Blackout (Shift+)")));

    // A fader job takes a fader, a key job a key: refused with why.
    QString why;
    QVERIFY(!maps.applyJob(0, kClip12, QStringLiteral("size"), &why));
    QVERIFY2(why.contains(QStringLiteral("fader")), qPrintable(why));
    QVERIFY(!maps.applyJob(0, 4103, QStringLiteral("blackout"), &why));
    // Shift itself is no Shift+ job's key.
    QVERIFY(!maps.applyJob(0, 226, QStringLiteral("blackout"), &why));
    QVERIFY2(why.contains(QStringLiteral("Shift")), qPrintable(why));

    // One job per control: a key's desk role goes, and Undo brings it back.
    DeskRecord *desk = m_doc->deskRecord();
    QCOMPARE(desk->binding(DeskRecord::Go).channel, kPlay);
    QVERIFY(maps.applyJob(0, kPlay, QStringLiteral("subgo")));
    QVERIFY(!desk->binding(DeskRecord::Go).isSet());
    QVERIFY(m_doc->controlMap()->binding(ControlMap::job(ControlMap::SubGoTarget)) != nullptr);
    QVERIFY(maps.undoLast());
    QCOMPARE(desk->binding(DeskRecord::Go).channel, kPlay);
    QVERIFY(m_doc->controlMap()->binding(ControlMap::job(ControlMap::SubGoTarget)) == nullptr);
    // A job moved to another control: Undo puts it back where it was.
    QVERIFY(maps.applyJob(0, kClip12, QStringLiteral("flash:%1").arg(show.front->id())));
    QCOMPARE(m_doc->controlMap()->binding(ControlMap::job(ControlMap::FlashGroupTarget, show.front->id()))->channel,
             kClip12);
    QVERIFY(maps.undoLast());
    QCOMPARE(m_doc->controlMap()->binding(ControlMap::job(ControlMap::FlashGroupTarget, show.front->id()))->channel,
             kClip11);
}

void SurfaceMap_Test::aBoardWithNoShiftHasNoShiftJobs()
{
    SimMidiDevice mini(m_doc, m_stub, QStringLiteral("APC mini mk2 Control"), 0, QStringLiteral("Akai APC Mini mk2"),
                       profile("Akai-APCMini-mk2.qxi"));
    QVERIFY2(mini.plug(), qPrintable(mini.lastError()));
    SurfaceMaps maps(m_doc);
    settle();
    SurfaceMapView *mv = maps.view(0);
    QVERIFY(mv != nullptr);
    QCOMPARE(m_doc->controlMap()->shiftChannelOf(0), 0xFFFFFFFFu);
    // Its first pad.
    quint32 pad = 0xFFFFFFFF;
    for (const SurfaceLayout::Control &c : mv->layout().controls())
        if ((c.kind == SurfaceLayout::Pad || c.kind == SurfaceLayout::Button) && pad == 0xFFFFFFFF
            && !m_doc->deskRecord()->isDeskControl(0, c.channel))
            pad = c.channel;
    QVERIFY(pad != 0xFFFFFFFF);
    bool listed = false;
    for (const SurfaceMaps::Job &j : maps.jobsFor(0, pad))
        if (j.key == QLatin1String("blackout") || j.key == QLatin1String("clear") || j.key.startsWith(QLatin1String("fadestop")))
        {
            listed = true;
            QVERIFY2(!j.enabled, qPrintable(j.key));
            QVERIFY2(j.text.contains(QStringLiteral("Shift")) || j.group.contains(QStringLiteral("Shift")), qPrintable(j.text));
        }
    QVERIFY(listed);
    QString why;
    QVERIFY(!maps.applyJob(0, pad, QStringLiteral("blackout"), &why));
    QVERIFY2(why.contains(QStringLiteral("two controls")), qPrintable(why));
    QVERIFY(m_doc->controlMap()->binding(ControlMap::job(ControlMap::BlackoutTarget)) == nullptr);
    // One loaded from a show anyway: dark, and a press is refused, saying why.
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, pad, ControlMap::job(ControlMap::BlackoutTarget), ControlMap::Toggle });
    settle();
    QCOMPARE(cm->lamp(cm->bindingsOn(0, pad).first()), ControlMap::LampOff);
    QSignalSpy blocked(cm, &ControlMap::blocked);
    m_doc->setMode(Doc::Operate);       // in Design the board's page may have the pad
    settle();
    QVERIFY(mini.tap(QStringLiteral("#%1").arg(pad)));
    settle();
    QVERIFY(!m_doc->inputOutputMap()->blackout());
    QVERIFY2(saidContains(blocked, QStringLiteral("no Shift key")), "the refusal was not said");
}

void SurfaceMap_Test::everyBoardJobFiresFromTheBoard()
{
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc, profile("Akai-APC40-mkII.qxi"));
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    const JobShow show = jobShow(m_doc);
    SurfaceMaps maps(m_doc);
    settle();
    ControlMap *cm = m_doc->controlMap();
    ShowControl *sc = m_doc->showControl();
    InputOutputMap *iom = m_doc->inputOutputMap();
    QSignalSpy blocked(cm, &ControlMap::blocked);
    QSignalSpy fired(cm, &ControlMap::fired);
    QSignalSpy handled(sc, &ShowControl::commandHandled);
    auto key = [](quint32 ch) { return QStringLiteral("#%1").arg(ch); };
    const QString shift = key(226);
    auto shiftTap = [&](quint32 ch) {
        QVERIFY(apc.press(shift));
        settle();
        QVERIFY(apc.tap(key(ch)));
        settle();
        QVERIFY(apc.release(shift));
        settle();
    };
    // The keys in a row of the clip grid (none is a desk or page key).
    const quint32 kBlackout = kClip11, kFade = kClip11 + 1, kClear = kClip11 + 2, kBlind = kClip11 + 3,
                  kFlash = kClip11 + 4, kNext = kClip11 + 5, kPrev = kClip11 + 6, kSubGo = kClip11 + 7,
                  kStore = 128 + 24, kTap = 128 + 25, kJump = 128 + 26;
    const QList<QPair<quint32, QString>> jobs = {
        { kBlackout, QStringLiteral("blackout") }, { kFade, QStringLiteral("fadestop:3") },
        { kClear, QStringLiteral("clear") }, { kBlind, QStringLiteral("blind") },
        { kFlash, QStringLiteral("flash:%1").arg(show.front->id()) }, { kNext, QStringLiteral("song-next") },
        { kPrev, QStringLiteral("song-prev") }, { kSubGo, QStringLiteral("subgo") },
        { kStore, QStringLiteral("store") }, { kTap, QStringLiteral("tap:0") },
        { kJump, QStringLiteral("cue:%1").arg(show.cues.at(4)) } };
    for (const auto &j : jobs)
    {
        QString why;
        QVERIFY2(maps.applyJob(0, j.first, j.second, &why), qPrintable(j.second + QStringLiteral(": ") + why));
    }
    settle();
    QCOMPARE(maps.view(0)->jobText(kBlackout), QStringLiteral("Shift+Blackout"));

    // Blackout: one press alone does nothing and says so; Shift held, it goes on, then off.
    QCOMPARE(cm->controlLamp(0, kBlackout), ControlMap::LampStandby);
    QVERIFY(apc.tap(key(kBlackout)));
    settle();
    QVERIFY2(!iom->blackout(), "a single press made a blackout");
    QVERIFY2(saidContains(blocked, QStringLiteral("hold Shift")), "the refusal was not said");
    // Shift pressed and let go first is no Shift.
    QVERIFY(apc.tap(shift));
    QVERIFY(apc.tap(key(kBlackout)));
    settle();
    QVERIFY(!iom->blackout());
    shiftTap(kBlackout);
    QVERIFY2(iom->blackout(), "Shift+key made no blackout");
    QTRY_COMPARE(cm->controlLamp(0, kBlackout), ControlMap::LampRunning);
    shiftTap(kBlackout);
    QVERIFY(!iom->blackout());

    // Clear the selection: alone nothing, with Shift the selection goes.
    m_doc->programmer()->setProgrammerSelection({ show.dimmer->id() });
    settle();
    QCOMPARE(cm->controlLamp(0, kClear), ControlMap::LampStandby);
    QVERIFY(apc.tap(key(kClear)));
    settle();
    QCOMPARE(m_doc->programmer()->programmerSelection().size(), 1);
    shiftTap(kClear);
    QVERIFY(m_doc->programmer()->programmerSelection().isEmpty());
    QTRY_COMPARE(cm->controlLamp(0, kClear), ControlMap::LampOff);

    // Fade and stop: alone nothing; with Shift, the toolbar's Fade and stop.
    fired.clear();
    QVERIFY(apc.tap(key(kFade)));
    settle();
    QVERIFY(!saidContains(fired, QStringLiteral("Fade and stop")));
    shiftTap(kFade);
    QVERIFY2(saidContains(fired, QStringLiteral("over 3 s")), "Shift+key did not fade and stop");

    // Blind: one press, on; again, off (no Shift: it never darkens a live show).
    QVERIFY(apc.tap(key(kBlind)));
    settle();
    QVERIFY(cm->blindIsOn());
    QTRY_COMPARE(cm->controlLamp(0, kBlind), ControlMap::LampRunning);
    QVERIFY(apc.tap(key(kBlind)));
    settle();
    QVERIFY(!cm->blindIsOn());

    // Flash a group: its dimmer at full while held, back when let go.
    QCOMPARE(outAt(m_doc, 0), 0);
    QVERIFY(apc.press(key(kFlash)));
    settle();
    QVERIFY(cm->isFlashing(show.front->id()));
    QCOMPARE(outAt(m_doc, 0), 255);
    QTRY_COMPARE(cm->controlLamp(0, kFlash), ControlMap::LampRunning);
    QVERIFY(apc.release(key(kFlash)));
    settle();
    QVERIFY(!cm->isFlashing(show.front->id()));
    QCOMPARE(outAt(m_doc, 0), 0);

    // Next and Previous song: the song's first cue on standby, never played.
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(m_doc->showTree()->cueList(show.list)->chaser));
    QVERIFY(chaser != nullptr);
    QCOMPARE(chaser->stepsCount(), 5);
    QCOMPARE(sc->standbyStep(chaser), 0);
    QVERIFY(apc.tap(key(kNext)));
    settle();
    QCOMPARE(sc->standbyStep(chaser), 2);
    QVERIFY(!chaser->isRunning());
    QVERIFY(apc.tap(key(kNext)));
    settle();
    QCOMPARE(sc->standbyStep(chaser), 4);
    QTRY_COMPARE(cm->controlLamp(0, kNext), ControlMap::LampOff);     // the last song: none after it
    QVERIFY(apc.tap(key(kPrev)));
    settle();
    QCOMPARE(sc->standbyStep(chaser), 2);

    // Sub GO: the one run path (here it says the list isn't playing).
    handled.clear();
    QVERIFY(apc.tap(key(kSubGo)));
    settle();
    QVERIFY2(saidContains(handled, QStringLiteral("SUB GO")), "Sub GO did not reach ShowControl");

    // Jump to cue: GO that cue.
    handled.clear();
    QVERIFY(apc.tap(key(kJump)));
    settle();
    QVERIFY2(saidContains(handled, QStringLiteral("GO")), "the jump did not reach ShowControl");

    // Edit Live's Store: Edit Live is off here, and it says so.
    blocked.clear();
    QVERIFY(apc.tap(key(kStore)));
    settle();
    QVERIFY2(saidContains(blocked, QStringLiteral("Edit Live")), "Store did not reach Edit Live");
    QCOMPARE(cm->controlLamp(0, kStore), ControlMap::LampOff);

    // Tap: three taps a quarter second apart are about 240 BPM.
    SpeedMasters *sm = m_doc->speedMasters();
    QVERIFY(qAbs(sm->bpm(0) - 120.0) < 1.0);
    for (int i = 0; i < 3; ++i)
    {
        QVERIFY(apc.tap(key(kTap), 20));
        settle();
        QTest::qWait(230);
    }
    QVERIFY2(sm->bpm(0) > 160.0, qPrintable(QString::number(sm->bpm(0))));

    // The faders, in Operate (in Design the Build page has them).
    m_doc->setMode(Doc::Operate);
    settle();
    const quint32 kLevel = 4103, kSpeed = 8199, kSize = 12295;
    QVERIFY(maps.applyJob(0, kLevel, QStringLiteral("level:%1").arg(show.front->id())));
    QVERIFY(maps.applyJob(0, kSpeed, QStringLiteral("speed:0")));
    QVERIFY(maps.applyJob(0, kSize, QStringLiteral("size")));
    QVERIFY(show.front->hasMaster());
    // A group's level: a master follows its fader at once.
    QVERIFY(apc.set(key(kLevel), 64));
    settle();
    QVERIFY2(qAbs(int(show.front->masterLevel()) - 128) <= 2, qPrintable(QString::number(show.front->masterLevel())));
    QVERIFY(apc.set(key(kLevel), 0));
    settle();
    QCOMPARE(int(show.front->masterLevel()), 0);
    // The effects' speed: picked up at the middle (as set), the top twice it.
    QVERIFY(apc.set(key(kSpeed), 64));
    settle();
    QVERIFY(apc.set(key(kSpeed), 127));
    settle();
    QVERIFY2(sm->rate(0) > 1.9, qPrintable(QString::number(sm->rate(0))));
    // The effects' size: picked up at the top (as made), half way is half.
    EffectScriptRunner *runner = m_doc->effectScriptRunner();
    QVERIFY(runner != nullptr);
    QVERIFY(apc.set(key(kSize), 127));
    settle();
    QVERIFY(apc.set(key(kSize), 64));
    settle();
    QVERIFY2(qAbs(runner->sizeMaster() - 0.5f) < 0.02f, qPrintable(QString::number(runner->sizeMaster())));
    // The rate moved elsewhere (the screen): the fader at the top waits to be
    // picked up, and says where to move it.
    sm->setRate(0, 1.0);
    blocked.clear();
    QVERIFY(apc.set(key(kSpeed), 120));
    settle();
    QVERIFY2(qAbs(sm->rate(0) - 1.0) < 0.01, "the speed jumped to a fader that was somewhere else");
    QVERIFY2(saidContains(blocked, QStringLiteral("50%")), "the pick-up was not said");
}

void SurfaceMap_Test::aFlashLeavesMachineryAlone()
{
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc, profile("Akai-APC40-mkII.qxi"));
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    const JobShow show = jobShow(m_doc);
    // In the group beside the dimmer: a kabuki drop, the house lights' relay,
    // a curtain and a flame (each a one-channel dimmer, as often patched).
    const QList<Fixture::Behaviour> kinds = { Fixture::OneShotBehaviour, Fixture::SwitchBehaviour,
                                              Fixture::MachineryBehaviour, Fixture::HazardBurstBehaviour };
    for (int i = 0; i < kinds.size(); ++i)
    {
        Fixture *f = new Fixture(m_doc);
        f->setName(QStringLiteral("Thing %1").arg(i));
        f->setAddress(quint32(i + 1));
        f->setUniverse(0);
        f->setChannels(1);
        m_doc->addFixture(f);
        f->setBehaviourSetting(kinds.at(i));
        show.front->assignFixture(f->id());
    }
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, kClip11, ControlMap::job(ControlMap::FlashGroupTarget, show.front->id()),
                     ControlMap::Momentary });
    m_doc->setMode(Doc::Operate);
    settle();
    QVERIFY(apc.press(QStringLiteral("#%1").arg(kClip11)));
    settle();
    QVERIFY(cm->isFlashing(show.front->id()));
    QCOMPARE(outAt(m_doc, 0), 255);         // the light flashes
    for (int i = 0; i < kinds.size(); ++i)
        QVERIFY2(outAt(m_doc, i + 1) == 0, qPrintable(QStringLiteral("a Flash drove behaviour %1").arg(int(kinds.at(i)))));
    QVERIFY(apc.release(QStringLiteral("#%1").arg(kClip11)));
    settle();
}

void SurfaceMap_Test::theEmergencyStopEndsAFlash()
{
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc, profile("Akai-APC40-mkII.qxi"));
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    const JobShow show = jobShow(m_doc);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, kClip11, ControlMap::job(ControlMap::FlashGroupTarget, show.front->id()),
                     ControlMap::Momentary });
    m_doc->setMode(Doc::Operate);
    settle();
    const QString flash = QStringLiteral("#%1").arg(kClip11);
    QVERIFY(apc.press(flash));
    settle();
    QCOMPARE(outAt(m_doc, 0), 255);
    // ALL OFF while the key is still held: everything is off, the Flash too.
    QVERIFY(m_doc->showControl()->allOff(ShowControl::RouteOther));
    settle();
    QCoreApplication::processEvents();
    QVERIFY2(!cm->isFlashing(show.front->id()), "the Flash outlived the emergency stop");
    QCOMPARE(outAt(m_doc, 0), 0);
    // Its release lets go of nothing; a new press flashes again.
    QVERIFY(apc.release(flash));
    settle();
    QCOMPARE(outAt(m_doc, 0), 0);
    QVERIFY(apc.press(flash));
    settle();
    QCOMPARE(outAt(m_doc, 0), 255);
    QVERIFY(apc.release(flash));
    settle();
}

void SurfaceMap_Test::aShiftHeldAcrossAReplugIsNoShift()
{
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc, profile("Akai-APC40-mkII.qxi"));
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    ControlMap *cm = m_doc->controlMap();
    cm->setDumpWindowMs(0);         // no dump to wait out: every press after the replug counts
    cm->setBinding({ 0, kClip11, ControlMap::job(ControlMap::BlackoutTarget), ControlMap::Toggle });
    settle();
    const QString shift = QStringLiteral("#226");
    QVERIFY(apc.press(shift));
    settle();
    QVERIFY(cm->isShiftHeld(0));
    // The cable pulled with Shift down: its release never comes.
    QVERIFY(apc.unplug());
    settle();
    QVERIFY(apc.replug());
    settle();
    QVERIFY2(!cm->isShiftHeld(0), "a Shift held as the board went away is still held");
    QVERIFY(apc.tap(QStringLiteral("#%1").arg(kClip11)));
    settle();
    QVERIFY2(!m_doc->inputOutputMap()->blackout(), "one press made a blackout after a replug");
}

void SurfaceMap_Test::theBoardJobsRoundTripThroughTheShowFile()
{
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc, profile("Akai-APC40-mkII.qxi"));
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    const JobShow show = jobShow(m_doc);
    SurfaceMaps maps(m_doc);
    settle();
    const QString g = QString::number(show.front->id());
    const QList<QPair<quint32, QString>> jobs = {
        { kClip11, QStringLiteral("blackout") }, { kClip11 + 1, QStringLiteral("fadestop:10") },
        { kClip11 + 2, QStringLiteral("clear") }, { kClip11 + 3, QStringLiteral("blind") },
        { kClip11 + 4, QStringLiteral("flash:") + g }, { kClip11 + 5, QStringLiteral("song-next") },
        { kClip11 + 6, QStringLiteral("song-prev") }, { kClip11 + 7, QStringLiteral("subgo") },
        { 128 + 24, QStringLiteral("store") }, { 128 + 25, QStringLiteral("tap:1") },
        { 128 + 26, QStringLiteral("cue:%1").arg(show.cues.at(2)) },
        { 4103, QStringLiteral("level:") + g }, { 8199, QStringLiteral("speed:2") }, { 12295, QStringLiteral("size") } };
    for (const auto &j : jobs)
        QVERIFY2(maps.applyJob(0, j.first, j.second), qPrintable(j.second));
    ControlMap *cm = m_doc->controlMap();
    const QList<ControlMap::Binding> before = cm->bindings();
    QCOMPARE(before.size(), jobs.size());

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buffer);
    w.writeStartDocument();
    QVERIFY(cm->saveXML(&w));
    w.writeEndDocument();
    buffer.close();
    // Every type by its own name in the file.
    for (const char *t : { "Blackout", "FadeStop", "ClearSelection", "Blind", "FlashGroup", "NextSong",
                           "PreviousSong", "SubGo", "EditLiveStore", "Tap", "GroupLevel", "EffectSpeed", "EffectSize" })
        QVERIFY2(buffer.data().contains(QByteArray("Target=\"") + t + "\""), t);

    cm->reset();
    QVERIFY(cm->bindings().isEmpty());
    QXmlStreamReader r(buffer.data());
    r.readNextStartElement();
    QVERIFY(cm->loadXML(r));
    QCOMPARE(cm->bindings(), before);
    settle();
    // And they are the board's jobs again.
    QCOMPARE(maps.view(0)->jobText(kClip11), QStringLiteral("Shift+Blackout"));
    QCOMPARE(cm->targetName(ControlMap::job(ControlMap::FadeStopTarget, 10)), QStringLiteral("Fade and stop (10 s)"));
}

void SurfaceMap_Test::jobPickerShots()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set");
    QDir().mkpath(dir);
    const QString prefix = qEnvironmentVariable("QLC_SHOT_PREFIX", QStringLiteral("after"));
    SimMidiDevice apc(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc, profile("Akai-APC40-mkII.qxi"));
    QVERIFY2(apc.plug(), qPrintable(apc.lastError()));
    const JobShow show = jobShow(m_doc);
    FixtureGroup *back = new FixtureGroup(m_doc);
    back->setName(QStringLiteral("Back"));
    m_doc->addFixtureGroup(back);
    settle();
    const QList<QSize> sizes = { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) };
    for (const QSize &size : sizes)
    {
        QScrollArea scroll;
        scroll.setWidgetResizable(true);
        ControlSurfacesPanel *hub = new ControlSurfacesPanel(m_doc);
        scroll.setWidget(hub);
        scroll.resize(size.width() - 240, size.height() - 120);
        scroll.show();
        settle();
        SurfaceMaps *maps = hub->surfaceMaps();
        if (size == sizes.first())
        {
            QVERIFY(maps->applyJob(0, kClip11, QStringLiteral("blackout")));
            QVERIFY(maps->applyJob(0, kClip11 + 1, QStringLiteral("flash:%1").arg(show.front->id())));
            QVERIFY(maps->applyJob(0, kClip11 + 2, QStringLiteral("song-next")));
            QVERIFY(maps->applyJob(0, kClip11 + 3, QStringLiteral("fadestop:5")));
            QVERIFY(maps->applyJob(0, 24583, QStringLiteral("level:%1").arg(show.front->id())));
        }
        maps->showMap(0);
        settle();
        const QString tag = QStringLiteral("%1x%2").arg(size.width()).arg(size.height());
        QVERIFY(scroll.grab().save(QStringLiteral("%1/%2-jobs-map-%3.png").arg(dir, prefix, tag)));
        // A key's picker, then its Flash a group submenu, then a fader's.
        std::unique_ptr<QMenu> menu(maps->jobMenu(0, kClip11 + 1));
        QVERIFY(menu != nullptr);
        menu->popup(scroll.mapToGlobal(QPoint(40, 40)));
        settle();
        QVERIFY(menu->grab().save(QStringLiteral("%1/%2-jobs-picker-key-%3.png").arg(dir, prefix, tag)));
        for (QAction *a : menu->actions())
            if (a->menu() != nullptr && a->text().startsWith(QStringLiteral("Flash a Group")))
            {
                a->menu()->popup(scroll.mapToGlobal(QPoint(300, 80)));
                settle();
                QVERIFY(a->menu()->grab().save(QStringLiteral("%1/%2-jobs-picker-flash-%3.png").arg(dir, prefix, tag)));
                a->menu()->close();
            }
        menu->close();
        std::unique_ptr<QMenu> fader(maps->jobMenu(0, 24583));
        QVERIFY(fader != nullptr);
        fader->popup(scroll.mapToGlobal(QPoint(40, 40)));
        settle();
        QVERIFY(fader->grab().save(QStringLiteral("%1/%2-jobs-picker-fader-%3.png").arg(dir, prefix, tag)));
        fader->close();
        while (QWidget *pop = QApplication::activePopupWidget())
            pop->close();
    }
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
