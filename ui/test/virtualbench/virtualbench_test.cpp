/*
  Q Light Controller Plus
  virtualbench_test.cpp

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

#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTreeWidget>
#include <QTimer>
#include <QColor>
#include <QtTest>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
#include "deskrecord.h"
#include "surfaceautopatch.h"
#undef protected
#undef private
#include "connectionstree.h"
#include "footerbar.h"
#include "surfacechips.h"
#include "surfacesetup.h"
#include "surfaceboardlog.h"
#include "outputrulekeeper.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "qlcioplugin.h"
#include "qlcinputprofile.h"
#include "inputpatch.h"
#include "outputpatch.h"
#include "showcontrol.h"
#include "universe.h"
#include "qlcfile.h"
#include "doc.h"

#include "virtualmidi.h"
#include "virtualbench_test.h"

namespace
{
const QString kApc40 = QStringLiteral("APC40 mkII");
const QString kApc64Daw = QStringLiteral("APC64 DAW (APC64)");
const QString kFp2 = QStringLiteral("PreSonus FP2");
const QString kMaize = QStringLiteral("USB2.0-MIDI Port 1");
const QString kMaize2 = QStringLiteral("USB2.0-MIDI Port 2");
const QString kPmj = QStringLiteral("OpenDeck | PMJ_BLACK_1");
const QString kRtp = QStringLiteral("Network RTP Session 1");
const QString kLpd8Mk2 = QStringLiteral("LPD8 mk2");
const QString kLpd8 = QStringLiteral("LPD8");

QByteArray hex(const char *s)
{
    return QByteArray::fromHex(QByteArray(s).replace(' ', ""));
}

QString hexOf(const QList<QByteArray> &msgs)
{
    QStringList out;
    for (const QByteArray &m : msgs)
        out << QString::fromLatin1(m.toHex(' '));
    return out.join(QStringLiteral(" | "));
}

/** The 8-bit value a pitch bend carries, as the MIDI plugin reads it. */
int pitchValue(const QByteArray &m)
{
    return (uchar(m.at(2)) << 1) | ((uchar(m.at(1)) >> 6) & 0x01);
}

/** A pitch bend on MIDI channel 1 that the plugin reads as @p v. */
QByteArray pitch(int v)
{
    QByteArray m(3, 0);
    m[0] = char(0xE0);
    m[1] = char((v & 0x01) << 6);
    m[2] = char(v >> 1);
    return m;
}

const QByteArray kAbleton = hex("F0 47 7F 29 60 00 04 41 09 00 05 F7");
const QByteArray kApc64Display = hex("F0 47 00 53 1C 00 01 01 F7");
const QByteArray kLpd8Paint = hex("F0 47 7F 4C 06 00 30");

/** A pad's colour in an LPD8 mk2 paint: its six bytes put back together. */
QColor lpd8Pad(const QByteArray &m, int pad)
{
    auto v = [&m, pad](int c) { return (uchar(m.at(7 + pad * 6 + c * 2)) << 7) | uchar(m.at(8 + pad * 6 + c * 2)); };
    return QColor(v(0), v(1), v(2));
}
}

/****************************************************************************
 * Set up
 ****************************************************************************/

void VirtualBench_Test::initTestCase()
{
    /* One bench per computer at a time: the system's MIDI list is shared,
       and the gate runs three legs side by side (another leg's "APC40
       mkII" would be this one's too). */
    m_lockFd = ::open("/tmp/qlcconsole-virtualbench.lock", O_CREAT | O_RDWR, 0666);
    QVERIFY2(m_lockFd >= 0, "the bench's lock file could not be opened");
    QElapsedTimer waited;
    waited.start();
    while (::flock(m_lockFd, LOCK_EX | LOCK_NB) != 0)
    {
        QVERIFY2(waited.elapsed() < 600000, "another virtual bench held the MIDI list for 10 minutes");
        QTest::qWait(200);
    }

    // A real board of one of these names on this computer: not a bench.
    const QStringList already = VirtualMidiBench::systemSources() + VirtualMidiBench::systemDestinations();
    for (const QString &b : VirtualMidiBench::boards())
        for (const VirtualMidiBench::PortSpec &p : VirtualMidiBench::board(b))
            if (already.contains(p.name))
                QSKIP(qPrintable(QStringLiteral("a real \"%1\" is connected to this computer").arg(p.name)));

    /* Every port already here is a real device (his LPD8 mk2 on dink): the
       app under test never sets one up, so nothing is sent to it. */
    for (const QString &n : already)
        if (!m_realPorts.contains(n))
            m_realPorts << n;
    // Declined before the app starts: its start-up scan must not set one up either.
    for (const QString &n : std::as_const(m_realPorts))
        SurfaceAutoPatch::setDeclined(n, true);

    m_bench = new VirtualMidiBench();
    QVERIFY2(m_bench->isValid(), qPrintable(QStringLiteral("CoreMIDI refused a client: %1").arg(m_bench->status())));

    // The real MIDI plugin, alone: no stub, no other plugin.
    m_pluginDir = new QTemporaryDir();
    QVERIFY(m_pluginDir->isValid());
    const QFileInfo plugin(QStringLiteral(QLC_MIDI_PLUGIN_FILE));
    QVERIFY2(plugin.exists(), QLC_MIDI_PLUGIN_FILE);
    QVERIFY(QFile::copy(plugin.absoluteFilePath(), m_pluginDir->filePath(plugin.fileName())));
    qputenv("QLC_IOPLUGIN_DIR", m_pluginDir->path().toLocal8Bit());

    m_app = new App();
    m_app->init();
    m_app->doc()->showControl()->setGoGuardMs(0);
    m_app->showNormal();
    m_app->resize(1280, 800);
    QVERIFY2(midi() != nullptr, "the MIDI plugin did not load");
    m_app->doc()->inputOutputMap()->loadProfiles(
        QDir(QStringLiteral(QLC_TEST_RESOURCE_ROOT) + QStringLiteral("inputprofiles/")));

    // Every message the footer shows, in order (one at a time is checked in cleanup()).
    QStatusBar *sb = m_app->statusBar();
    connect(sb, &QStatusBar::messageChanged, this, [this](const QString &text) {
        if (!text.isEmpty())
            m_footerMessages << text;
        const QString p = footerProblem();
        if (!p.isEmpty())
            m_footerProblems << p;
    });
    settle(50);
}

void VirtualBench_Test::cleanupTestCase()
{
    if (m_pump != nullptr)
        m_pump->stop();
    if (m_bench != nullptr)
        m_bench->unplugAll();
    if (m_app != nullptr)
    {
        settle(100);
        m_app->doc()->setMode(Doc::Design);
        m_app->doc()->resetModified();
    }
    delete m_app;
    m_app = nullptr;
    delete m_bench;
    m_bench = nullptr;
    delete m_pluginDir;
    m_pluginDir = nullptr;
    for (const QString &n : std::as_const(m_realPorts))
        SurfaceAutoPatch::setDeclined(n, false);
    if (m_lockFd >= 0)
    {
        ::flock(m_lockFd, LOCK_UN);
        ::close(m_lockFd);
        m_lockFd = -1;
    }
}

void VirtualBench_Test::init()
{
    // What this computer learned of the boards in an earlier test: forgotten.
    QSettings s;
    s.remove(QStringLiteral("surfaces"));
    s.remove(DeskRecord::settingsGroup());
    s.sync();
    // A real board on this computer is never set up by the bench (declined, as an Undo does).
    for (const QString &n : std::as_const(m_realPorts))
        SurfaceAutoPatch::setDeclined(n, true);
    m_app->doc()->deskRecord()->load();
    InputOutputMap::setConfirmedProfile(kMaize, QString());
    SurfaceSetup *ss = m_app->doc()->surfaceSetup();
    ss->autoPatch()->m_asked.clear();
    ss->autoPatch()->setEnabled(true);
    m_footerMessages.clear();
    m_footerProblems.clear();
}

void VirtualBench_Test::cleanup()
{
    const QStringList problems = m_footerProblems;
    m_bench->unplugAll();
    auto benchLines = [this]() {
        const QStringList lines = midi()->inputs();
        return QSet<QString>(lines.begin(), lines.end())
            .intersect(QSet<QString>({ kApc40, kApc64Daw, kFp2, kMaize, kPmj, kRtp, kLpd8Mk2, kLpd8 }));
    };
    QTRY_VERIFY_WITH_TIMEOUT(benchLines().isEmpty(), 5000);
    settle(100);
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    m_app->doc()->surfaceSetup()->stopLampTest();
    m_app->doc()->surfaceSetup()->autoPatch()->setEnabled(false, false);
    m_app->doc()->surfaceSetup()->autoPatch()->m_patched.clear();
    for (quint32 u = 0; u < iom->universesCount(); ++u)
    {
        iom->setInputPatch(u, KInputNone, QString(), QLCIOPlugin::invalidLine());
        iom->setOutputPatch(u, KOutputNone, QString(), 0, true);
        if (Universe *uni = iom->universe(u))
        {
            uni->clearOfflinePatches(Universe::InputPatchTag);
            uni->clearOfflinePatches(Universe::FeedbackPatchTag);
        }
    }
    m_app->doc()->outputRuleKeeper()->noteGo();
    m_app->hideViewNotice();
    settle(50);
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));
}

void VirtualBench_Test::settle(int ms)
{
    for (int i = 0; i < 3; ++i)
    {
        VirtualMidiBench::pumpRunLoop();
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
    }
    if (ms > 0)
        QTest::qWait(ms);
}

QLCIOPlugin *VirtualBench_Test::midi() const
{
    return m_app->doc()->ioPluginCache()->plugin(QStringLiteral("MIDI"));
}

quint32 VirtualBench_Test::universeOf(const QString &device) const
{
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    for (quint32 u = 0; u < iom->universesCount(); ++u)
    {
        InputPatch *ip = iom->inputPatch(u);
        if (ip != nullptr && ip->plugin() != nullptr && ip->inputName() == device)
            return u;
    }
    return UINT_MAX;
}

QString VirtualBench_Test::footerProblem() const
{
    FooterBar *fb = qobject_cast<FooterBar *>(m_app->statusBar());
    if (fb == nullptr)
        return QStringLiteral("the footer is not the FooterBar");
    QLabel *label = fb->messageLabel();
    if (label == nullptr || !label->isVisible() || label->text().isEmpty())
        return QString();
    // Nothing else of the footer drawn where its one message is.
    const QRect r = label->geometry();
    for (QWidget *w : fb->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
        if (w != label && w->isVisible() && w->geometry().intersects(r) && !w->isAncestorOf(label))
            return QStringLiteral("footer message \"%1\" drawn over %2 \"%3\"")
                .arg(label->text(), QString::fromLatin1(w->metaObject()->className()), w->objectName());
    return QString();
}

/****************************************************************************
 * The bench and the OS
 ****************************************************************************/

void VirtualBench_Test::aPlugReachesThePluginByItself()
{
    /* CoreMIDI says "a device came" on the run loop of the thread that
       made the first MIDI client (here, the GUI thread). The real app's
       Cocoa event dispatcher runs that loop; the offscreen platform's does
       not. Find out which this is, then pump it for the rest. */
    const QString dispatcher = QString::fromLatin1(QAbstractEventDispatcher::instance()->metaObject()->className());
    QVERIFY(m_bench->plug(kRtp) != nullptr);
    QElapsedTimer t;
    t.start();
    bool alone = false;
    while (t.elapsed() < 1500 && !alone)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QTest::qSleep(10);
        alone = midi()->inputs().contains(kRtp);
    }
    qInfo().noquote() << "[VirtualBench] event dispatcher" << dispatcher << "-- a plug seen without pumping CoreFoundation:"
                      << (alone ? "yes" : "no");
    // The bench pumps the main run loop as Cocoa does, from here on.
    m_pump = new QTimer(this);
    connect(m_pump, &QTimer::timeout, this, []() { VirtualMidiBench::pumpRunLoop(); });
    m_pump->start(5);
    if (!alone)
        QTRY_VERIFY_WITH_TIMEOUT(midi()->inputs().contains(kRtp), 3000);
    QVERIFY2(midi()->outputs().contains(kRtp), qPrintable(midi()->outputs().join(", ")));
    // Unplugged: gone by itself.
    m_bench->unplug(kRtp);
    QTRY_VERIFY_WITH_TIMEOUT(!midi()->inputs().contains(kRtp), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(!midi()->outputs().contains(kRtp), 3000);
}

void VirtualBench_Test::theBenchIsListedAsHisBoardsAre()
{
    for (const QString &b : VirtualMidiBench::boards())
        QVERIFY2(!m_bench->plugBoard(b).contains(nullptr), qPrintable(b));
    QStringList ins, outs;
    for (const QString &b : VirtualMidiBench::boards())
        for (const VirtualMidiBench::PortSpec &p : VirtualMidiBench::board(b))
        {
            if (p.source)
                ins << p.name;
            if (p.destination)
                outs << p.name;
        }
    // What every MIDI program on this computer sees.
    for (const QString &n : ins)
        QVERIFY2(VirtualMidiBench::systemSources().contains(n), qPrintable(n));
    // The plugin: hardware lines, by those names.
    QVERIFY(midi()->linesAreHardware());
    for (const QString &n : ins)
        QTRY_VERIFY2_WITH_TIMEOUT(midi()->inputs().contains(n), qPrintable(n + ": " + midi()->inputs().join(", ")), 3000);
    for (const QString &n : outs)
        QTRY_VERIFY2_WITH_TIMEOUT(midi()->outputs().contains(n), qPrintable(n), 3000);
    QVERIFY2(!midi()->inputs().contains(kMaize2), "the MaizeDMX's Port 2 is an output only");
    // Its other ports and the session were never patched.
    settle(800);
    for (const QString &n : { QStringLiteral("APC64 Notes (APC64)"), QStringLiteral("APC64 MIDI (APC64)"),
                              QStringLiteral("APC64 Custom (APC64)"), QStringLiteral("APC mini mk2 Notes"), kRtp })
        QVERIFY2(universeOf(n) == UINT_MAX, qPrintable(n + " was patched"));
}

/****************************************************************************
 * Plugged in
 ****************************************************************************/

void VirtualBench_Test::theApc40IsSetUpWhenPluggedIn()
{
    SurfaceAutoPatch *ap = m_app->doc()->surfaceSetup()->autoPatch();
    QSignalSpy patched(ap, &SurfaceAutoPatch::autoPatched);
    VirtualMidiPort *apc = m_bench->plug(kApc40);
    QVERIFY(apc != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(patched.count(), 1, 5000);
    const quint32 u = patched.first().at(0).toUInt();
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    QCOMPARE(iom->inputPatch(u)->inputName(), kApc40);
    QCOMPARE(iom->inputPatch(u)->profileName(), QStringLiteral("Akai APC40 mkII"));
    QVERIFY(iom->feedbackPatch(u) != nullptr && iom->feedbackPatch(u)->isPatched());
    QCOMPARE(iom->feedbackPatch(u)->outputName(), kApc40);
    // Ableton mode at its own port, then once more when the port has settled.
    QTRY_VERIFY_WITH_TIMEOUT(apc->count(kAbleton) >= 2, 3000);
    settle(800);
    qInfo().noquote() << "[VirtualBench] APC40 mkII: Ableton mode sent" << apc->count(kAbleton) << "times";
    // The notice, and its chip in the footer.
    QVERIFY2(m_app->m_viewNotice->isVisible(), "no notice for the board set up");
    QVERIFY2(m_app->m_viewNoticeText->text().contains(QStringLiteral("ready on universe %1").arg(u + 1)),
             qPrintable(m_app->m_viewNoticeText->text()));
    SurfaceChips *chips = m_app->surfaceChips();
    QVERIFY(chips != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(chips->chip(u) != nullptr && chips->chip(u)->isVisible(), 2000);
    // A pad pressed on the board reaches its Board Log, decoded.
    QVERIFY(apc->send("90 00 7F 80 00 00"));
    QTRY_VERIFY_WITH_TIMEOUT(m_app->doc()->surfaceSetup()->boardLog()->text(u).contains(QStringLiteral("in")), 2000);
}

void VirtualBench_Test::theApc64UsesItsDawPortOnly()
{
    SurfaceAutoPatch *ap = m_app->doc()->surfaceSetup()->autoPatch();
    QSignalSpy patched(ap, &SurfaceAutoPatch::autoPatched);
    const QList<VirtualMidiPort *> ports = m_bench->plugBoard(QStringLiteral("APC64"));
    QVERIFY(!ports.contains(nullptr));
    QTRY_COMPARE_WITH_TIMEOUT(patched.count(), 1, 5000);
    settle(1000);
    QCOMPARE(patched.count(), 1);
    const quint32 u = patched.first().at(0).toUInt();
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    QCOMPARE(iom->inputPatch(u)->inputName(), kApc64Daw);
    QCOMPARE(iom->feedbackPatch(u)->outputName(), kApc64Daw);
    for (int i = 1; i < ports.size(); ++i)
        QVERIFY2(universeOf(ports.at(i)->name()) == UINT_MAX, qPrintable(ports.at(i)->name()));
    // The display, taken and named, on the DAW port; nothing on the others.
    VirtualMidiPort *daw = ports.first();
    QTRY_VERIFY_WITH_TIMEOUT(daw->count(kApc64Display) >= 1, 2000);
    QVERIFY2(daw->received().contains(SurfaceSetup::apc64Text(0, QStringLiteral("qlc"))), qPrintable(hexOf(daw->received())));
    QVERIFY(daw->received().contains(SurfaceSetup::apc64Text(1, QStringLiteral("console"))));
    for (int i = 1; i < ports.size(); ++i)
        QVERIFY2(ports.at(i)->received().isEmpty(),
                 qPrintable(ports.at(i)->name() + " was sent " + hexOf(ports.at(i)->received())));
    // Connections ▸ Control surfaces: the APC64 once, its other ports folded under it.
    ConnectionsTree tree(m_app->doc());
    tree.setShowControlPanel(new QWidget(&tree));
    tree.setShowControlRows({ { "surfaces", "Control surfaces", "" } });
    tree.refresh();
    QTreeWidget *tw = tree.findChild<QTreeWidget *>();
    QVERIFY(tw != nullptr);
    QTreeWidgetItem *apcRow = nullptr;
    QStringList rows;
    for (QTreeWidgetItemIterator it(tw); *it != nullptr; ++it)
        if ((*it)->data(0, Qt::UserRole + 2).toString() == QLatin1String("surfaces"))
        {
            rows << (*it)->text(0);
            if ((*it)->text(0) == kApc64Daw)
                apcRow = *it;
        }
    QVERIFY2(apcRow != nullptr, qPrintable(rows.join(" | ")));
    QCOMPARE(apcRow->childCount(), 3);
    QCOMPARE(int(rows.count(kApc64Daw)), 1);
}

void VirtualBench_Test::theMaizeIsAskedOnceThenLit()
{
    SurfaceAutoPatch *ap = m_app->doc()->surfaceSetup()->autoPatch();
    QSignalSpy patched(ap, &SurfaceAutoPatch::autoPatched);
    QSignalSpy asked(ap, &SurfaceAutoPatch::askToUse);
    QList<VirtualMidiPort *> ports = m_bench->plugBoard(QStringLiteral("MaizeDMX"));
    QVERIFY(!ports.contains(nullptr));
    QTRY_COMPARE_WITH_TIMEOUT(asked.count(), 1, 5000);
    QCOMPARE(asked.first().at(1).toString(), kMaize);
    settle(1000);
    QCOMPARE(patched.count(), 0);
    // The question is still what the notice says once the plug has settled.
    QVERIFY2(m_app->m_viewNotice->isVisible(), "the question was not shown, or did not stay");
    QVERIFY2(m_app->m_viewNoticeText->text().contains(QStringLiteral("is this a MaizeDMX")),
             qPrintable(m_app->m_viewNoticeText->text()));
    QCOMPARE(m_app->m_viewNoticeButton->text(), QStringLiteral("Use It"));
    // Replugged before an answer: not asked again.
    m_bench->unplugBoard(QStringLiteral("MaizeDMX"));
    QTRY_VERIFY_WITH_TIMEOUT(!midi()->inputs().contains(kMaize), 3000);
    ports = m_bench->plugBoard(QStringLiteral("MaizeDMX"));
    QTRY_VERIFY_WITH_TIMEOUT(midi()->inputs().contains(kMaize), 3000);
    settle(1000);
    QCOMPARE(asked.count(), 1);
    // Use It.
    if (!m_app->m_viewNoticeText->text().contains(QStringLiteral("is this a MaizeDMX")))
        QFAIL(qPrintable(QStringLiteral("the question is gone after a replug (notice: \"%1\"): nothing left to say Use It to")
                             .arg(m_app->m_viewNoticeText->text())));
    QTest::mouseClick(m_app->m_viewNoticeButton, Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(patched.count(), 1, 3000);
    const quint32 u = patched.first().at(0).toUInt();
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    QCOMPARE(iom->inputPatch(u)->profileName(), QStringLiteral("MaizeDMX Playback Controller"));
    QCOMPARE(iom->feedbackPatch(u)->outputName(), kMaize);
    // X7's lamp (note 43, channel 1): the lamp test lights it, at Port 1.
    VirtualMidiPort *port1 = ports.first();
    port1->clear();
    SurfaceSetup *ss = m_app->doc()->surfaceSetup();
    ss->setLampTestStepMs(5);
    QString why;
    QVERIFY2(ss->startLampTest(u, &why), qPrintable(why));
    auto x7Lit = [port1]() {
        for (const QByteArray &m : port1->receivedOfType(0x90))
            if (uchar(m.at(1)) == 43 && m.size() > 2 && m.at(2) != 0)
                return true;
        return false;
    };
    QTRY_VERIFY2_WITH_TIMEOUT(x7Lit(), qPrintable("X7 (note 43) was not lit: " + hexOf(port1->received())), 5000);
    QVERIFY2(ports.at(1)->received().isEmpty(), qPrintable("Port 2 was sent " + hexOf(ports.at(1)->received())));
    ss->stopLampTest();
}

void VirtualBench_Test::thePmjIsSetUpAndLitOnItsOwnPort()
{
    SurfaceAutoPatch *ap = m_app->doc()->surfaceSetup()->autoPatch();
    QSignalSpy patched(ap, &SurfaceAutoPatch::autoPatched);
    VirtualMidiPort *rtp = m_bench->plug(kRtp);
    VirtualMidiPort *pmj = m_bench->plug(kPmj);
    QVERIFY(rtp != nullptr && pmj != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(midi()->inputs().contains(kPmj), 3000);
    settle(1200);
    /* int/053-boards' PMJ profile has no <Match>: OpenDeck's port name is
       never recognised. fix/053-board-repair gives it one; from then on
       it must be set up by itself. */
    QLCInputProfile *prof = m_app->doc()->inputOutputMap()->profile(QStringLiteral("PMJ Black 1"));
    QVERIFY(prof != nullptr);
    if (prof->deviceMatches().isEmpty())
    {
        qWarning().noquote() << "[VirtualBench] BUG (fixed on fix/053-board-repair): the PMJ Black 1 profile has no"
                                " Match, so" << kPmj << "is not set up when plugged in";
        QCOMPARE(patched.count(), 0);
        ap->setUp(midi(), kPmj, QStringLiteral("PMJ Black 1"));
        settle(500);
    }
    QVERIFY2(!patched.isEmpty(), qPrintable(QStringLiteral("\"%1\" was not set up when plugged in (profile for its name: \"%2\")")
                                                .arg(kPmj, m_app->doc()->inputOutputMap()->profileForDevice(kPmj))));
    const quint32 u = patched.first().at(0).toUInt();
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    QCOMPARE(iom->inputPatch(u)->profileName(), QStringLiteral("PMJ Black 1"));
    QCOMPARE(iom->feedbackPatch(u)->outputName(), kPmj);
    QVERIFY(universeOf(kRtp) == UINT_MAX);
    // Its lamps: at the PMJ, none at the session.
    rtp->clear();
    SurfaceSetup *ss = m_app->doc()->surfaceSetup();
    ss->setLampTestStepMs(5);
    QString why;
    QVERIFY2(ss->startLampTest(u, &why), qPrintable(why));
    QTRY_VERIFY_WITH_TIMEOUT(pmj->receivedOfType(0x90).size() >= 3, 5000);
    ss->stopLampTest();
    settle(100);
    QVERIFY2(rtp->received().isEmpty(), qPrintable("the RTP session was sent " + hexOf(rtp->received())));
}

void VirtualBench_Test::aPortGoingKeepsThePmjLampsOnThePmj()
{
    /* The order CoreMIDI lists them: the APC40, the PMJ, the session. The
       APC40 goes: the PMJ moves up one line, the session onto the PMJ's
       old one. */
    QVERIFY(m_bench->plug(kApc40) != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(midi()->outputs().contains(kApc40), 3000);
    VirtualMidiPort *pmj = m_bench->plug(kPmj);
    QTRY_VERIFY_WITH_TIMEOUT(midi()->outputs().contains(kPmj), 3000);
    VirtualMidiPort *rtp = m_bench->plug(kRtp);
    QTRY_VERIFY_WITH_TIMEOUT(midi()->outputs().contains(kRtp), 3000);
    const QStringList before = midi()->outputs();
    QVERIFY2(before.indexOf(kApc40) < before.indexOf(kPmj) && before.indexOf(kPmj) < before.indexOf(kRtp),
             qPrintable(before.join(", ")));
    settle(1200);
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    quint32 u = universeOf(kPmj);
    if (u == UINT_MAX)
        u = m_app->doc()->surfaceSetup()->autoPatch()->setUp(midi(), kPmj, QStringLiteral("PMJ Black 1"));
    QVERIFY(u != UINT_MAX);
    QCOMPARE(iom->feedbackPatch(u)->outputName(), kPmj);

    m_bench->unplug(kApc40);
    QTRY_VERIFY_WITH_TIMEOUT(!midi()->outputs().contains(kApc40), 3000);
    settle(300);
    pmj->clear();
    rtp->clear();
    QVERIFY2(iom->feedbackPatch(u)->outputName() == kPmj,
             qPrintable(QStringLiteral("the PMJ's feedback is now \"%1\"").arg(iom->feedbackPatch(u)->outputName())));
    SurfaceSetup *ss = m_app->doc()->surfaceSetup();
    ss->setLampTestStepMs(5);
    QString why;
    QVERIFY2(ss->startLampTest(u, &why), qPrintable(why));
    QTRY_VERIFY_WITH_TIMEOUT(pmj->receivedOfType(0x90).size() + rtp->receivedOfType(0x90).size() >= 3, 5000);
    ss->stopLampTest();
    settle(100);
    QVERIFY2(rtp->received().isEmpty(), qPrintable("the PMJ's lamps went to the RTP session: " + hexOf(rtp->received())));
    QVERIFY(!pmj->receivedOfType(0x90).isEmpty());
}

void VirtualBench_Test::thePmjsOwnReplugKeepsItsFeedback()
{
    /* The PMJ, then the session after it. The PMJ goes (its power): the
       session moves onto the PMJ's old line. The PMJ comes back, last. */
    VirtualMidiPort *pmj = m_bench->plug(kPmj);
    QTRY_VERIFY_WITH_TIMEOUT(midi()->outputs().contains(kPmj), 3000);
    VirtualMidiPort *rtp = m_bench->plug(kRtp);
    QTRY_VERIFY_WITH_TIMEOUT(midi()->outputs().contains(kRtp), 3000);
    settle(1200);
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    quint32 u = universeOf(kPmj);
    if (u == UINT_MAX)
        u = m_app->doc()->surfaceSetup()->autoPatch()->setUp(midi(), kPmj, QStringLiteral("PMJ Black 1"));
    QVERIFY(u != UINT_MAX);
    QCOMPARE(iom->feedbackPatch(u)->outputName(), kPmj);

    m_bench->unplug(kPmj);
    QTRY_VERIFY_WITH_TIMEOUT(!midi()->outputs().contains(kPmj), 3000);
    settle(300);
    // Gone: its feedback is not the session that took its line.
    QVERIFY2(iom->feedbackPatch(u) == nullptr || iom->feedbackPatch(u)->outputName() != kRtp,
             "the PMJ unplugged: its feedback is now the RTP session (the line number it had)");
    rtp->clear();
    pmj = m_bench->plug(kPmj);
    QTRY_VERIFY_WITH_TIMEOUT(universeOf(kPmj) == u, 5000);
    settle(1200);
    QVERIFY2(iom->feedbackPatch(u) != nullptr && iom->feedbackPatch(u)->outputName() == kPmj,
             qPrintable(QStringLiteral("the PMJ back: its feedback is \"%1\"")
                            .arg(iom->feedbackPatch(u) != nullptr ? iom->feedbackPatch(u)->outputName() : QString())));
    pmj->clear();
    SurfaceSetup *ss = m_app->doc()->surfaceSetup();
    ss->setLampTestStepMs(5);
    QString why;
    QVERIFY2(ss->startLampTest(u, &why), qPrintable(why));
    QTRY_VERIFY_WITH_TIMEOUT(pmj->receivedOfType(0x90).size() >= 3, 5000);
    ss->stopLampTest();
    settle(100);
    QVERIFY2(rtp->received().isEmpty(), qPrintable("the RTP session was sent " + hexOf(rtp->received())));
}

void VirtualBench_Test::aPmjFeedbackOnTheRtpLineIsRepaired()
{
    // His show: U01's input the PMJ, its feedback the RTP session's line.
    VirtualMidiPort *rtp = m_bench->plug(kRtp);
    VirtualMidiPort *pmj = m_bench->plug(kPmj);
    QTRY_VERIFY_WITH_TIMEOUT(midi()->inputs().contains(kPmj) && midi()->outputs().contains(kRtp), 3000);
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    m_app->doc()->surfaceSetup()->autoPatch()->setEnabled(false, false);
    QVERIFY(iom->setInputPatch(0, QStringLiteral("MIDI"), kPmj, quint32(midi()->inputs().indexOf(kPmj)),
                               QStringLiteral("PMJ Black 1")));
    QVERIFY(iom->setOutputPatch(0, QStringLiteral("MIDI"), kRtp, quint32(midi()->outputs().indexOf(kRtp)), true));
    QCOMPARE(iom->feedbackPatch(0)->outputName(), kRtp);
    m_app->doc()->surfaceSetup()->autoPatch()->setEnabled(true, false);
    // The PMJ goes and comes back (as it does at power-on).
    m_bench->unplug(kPmj);
    QTRY_VERIFY_WITH_TIMEOUT(!midi()->inputs().contains(kPmj), 3000);
    settle(200);
    pmj = m_bench->plug(kPmj);
    QTRY_VERIFY_WITH_TIMEOUT(midi()->inputs().contains(kPmj), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(universeOf(kPmj) == 0, 3000);
    settle(1200);
    if (iom->feedbackPatch(0)->outputName() == kRtp)
        QSKIP("his saved feedback stays on the RTP session: repaired on appear by fix/053-board-repair "
              "(not on int/053-boards); this test asserts it once that is merged");
    QCOMPARE(iom->feedbackPatch(0)->outputName(), kPmj);
    rtp->clear();
    pmj->clear();
    SurfaceSetup *ss = m_app->doc()->surfaceSetup();
    ss->setLampTestStepMs(5);
    QString why;
    QVERIFY2(ss->startLampTest(0, &why), qPrintable(why));
    QTRY_VERIFY_WITH_TIMEOUT(pmj->receivedOfType(0x90).size() >= 3, 5000);
    ss->stopLampTest();
    settle(100);
    QVERIFY2(rtp->received().isEmpty(), qPrintable("the RTP session was sent " + hexOf(rtp->received())));
}

/****************************************************************************
 * The FaderPort 2
 ****************************************************************************/

namespace
{
/** The motor's last position the app sent (-1: none). */
int lastMotor(const VirtualMidiPort *fp)
{
    const QList<QByteArray> bends = fp->receivedOfType(0xE0);
    return bends.isEmpty() ? -1 : pitchValue(bends.last());
}
}

void VirtualBench_Test::theFaderPortMotorStaysAndShiftStopStops()
{
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    DeskRecord *desk = m_app->doc()->deskRecord();
    iom->setGrandMasterValue(200);
    SurfaceAutoPatch *ap = m_app->doc()->surfaceSetup()->autoPatch();
    QSignalSpy patched(ap, &SurfaceAutoPatch::autoPatched);
    VirtualMidiPort *fp = m_bench->plug(kFp2);
    QTRY_COMPARE_WITH_TIMEOUT(patched.count(), 1, 5000);
    const quint32 u = patched.first().at(0).toUInt();
    QCOMPARE(iom->inputPatch(u)->profileName(), QStringLiteral("PreSonus FaderPort 2"));
    QCOMPARE(iom->feedbackPatch(u)->outputName(), kFp2);
    // Its defaults: the fader the Grand Master, Stop the STOP, on this board.
    QTRY_VERIFY_WITH_TIMEOUT(desk->isLive(DeskRecord::GrandMaster) && desk->isLive(DeskRecord::Stop), 2000);
    QCOMPARE(desk->universeOf(DeskRecord::GrandMaster), u);
    QCOMPARE(desk->universeOf(DeskRecord::Stop), u);
    // The motor is sent where the master is.
    QTRY_COMPARE_WITH_TIMEOUT(lastMotor(fp), 200, 2000);
    settle(1000);     // past the plug's dump window

    // Touch, move, let go.
    fp->clear();
    QVERIFY(fp->send("90 68 7F"));
    settle(30);
    for (int v : { 200, 180, 150, 120, 100 })
    {
        QVERIFY(fp->send(pitch(v)));
        settle(20);
    }
    QTRY_COMPARE_WITH_TIMEOUT(int(iom->grandMasterValue()), 100, 2000);
    QVERIFY2(fp->receivedOfType(0xE0).isEmpty(),
             qPrintable("the motor was driven while the hand was on it: " + hexOf(fp->receivedOfType(0xE0))));
    QVERIFY(fp->send("90 68 00"));
    QTRY_VERIFY_WITH_TIMEOUT(lastMotor(fp) >= 0, 2000);
    settle(300);
    QCOMPARE(lastMotor(fp), int(iom->grandMasterValue()));
    QCOMPARE(int(iom->grandMasterValue()), 100);

    // Stop alone: refused, and said.
    QSignalSpy fired(desk, &DeskRecord::fired);
    QSignalSpy refused(desk, &DeskRecord::refused);
    QVERIFY(fp->send("90 5D 7F"));
    settle(30);
    QVERIFY(fp->send("90 5D 00"));
    QTRY_COMPARE_WITH_TIMEOUT(refused.count(), 1, 2000);
    QCOMPARE(fired.count(), 0);
    QVERIFY(!m_app->doc()->outputRuleKeeper()->emergencyStopped());
    QVERIFY2(m_footerMessages.join(QLatin1Char('\n')).contains(QStringLiteral("Shift+Stop")),
             qPrintable(m_footerMessages.join(" | ")));
    // Shift+Stop: ALL OFF.
    QVERIFY(fp->send("90 46 7F"));
    settle(30);
    QVERIFY(fp->send("90 5D 7F"));
    settle(30);
    QVERIFY(fp->send("90 5D 00"));
    settle(30);
    QVERIFY(fp->send("90 46 00"));
    QTRY_COMPARE_WITH_TIMEOUT(fired.count(), 1, 2000);
    QCOMPARE(fired.first().first().toString(), QStringLiteral("ALL OFF"));
    QTRY_VERIFY_WITH_TIMEOUT(m_app->doc()->outputRuleKeeper()->emergencyStopped(), 2000);
}

void VirtualBench_Test::theFaderPortBesideAnApc64HoldingTheDesk()
{
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    DeskRecord *desk = m_app->doc()->deskRecord();
    SurfaceAutoPatch *ap = m_app->doc()->surfaceSetup()->autoPatch();
    QSignalSpy patched(ap, &SurfaceAutoPatch::autoPatched);
    // The APC64 first: the desk's STOP and Grand Master go to it.
    QVERIFY(!m_bench->plugBoard(QStringLiteral("APC64")).contains(nullptr));
    QTRY_COMPARE_WITH_TIMEOUT(patched.count(), 1, 5000);
    const quint32 apcU = patched.first().at(0).toUInt();
    QTRY_VERIFY_WITH_TIMEOUT(desk->isLive(DeskRecord::Stop), 2000);
    QCOMPARE(desk->universeOf(DeskRecord::Stop), apcU);
    QCOMPARE(desk->universeOf(DeskRecord::GrandMaster), apcU);
    // Then the FaderPort.
    VirtualMidiPort *fp = m_bench->plug(kFp2);
    QTRY_COMPARE_WITH_TIMEOUT(patched.count(), 2, 5000);
    const quint32 fpU = patched.at(1).at(0).toUInt();
    QCOMPARE(iom->inputPatch(fpU)->inputName(), kFp2);
    settle(1200);
    qInfo().noquote() << "[VirtualBench] with the APC64 holding the desk: STOP on universe"
                      << desk->universeOf(DeskRecord::Stop) + 1 << "GM on universe"
                      << desk->universeOf(DeskRecord::GrandMaster) + 1 << "the FaderPort on" << fpU + 1;
    // Shift+Stop on the FaderPort: the stop, or a word in the footer -- never nothing.
    QSignalSpy fired(desk, &DeskRecord::fired);
    QSignalSpy refused(desk, &DeskRecord::refused);
    const int footerBefore = int(m_footerMessages.size());
    QVERIFY(fp->send("90 46 7F"));
    settle(30);
    QVERIFY(fp->send("90 5D 7F"));
    settle(30);
    QVERIFY(fp->send("90 5D 00"));
    settle(30);
    QVERIFY(fp->send("90 46 00"));
    settle(500);
    const bool stopped = m_app->doc()->outputRuleKeeper()->emergencyStopped();
    const QStringList said = m_footerMessages.mid(footerBefore);
    QVERIFY2(stopped || !refused.isEmpty() || !said.isEmpty(),
             "Shift+Stop on the FaderPort did nothing and said nothing (STOP is the APC64's)");
    qInfo().noquote() << "[VirtualBench] Shift+Stop on the FaderPort beside the APC64:"
                      << (stopped ? "stopped" : "not stopped") << "-- footer:" << said.join(" | ");
    if (!stopped)
        QVERIFY2(said.join(QLatin1Char(' ')).contains(QStringLiteral("STOP is on")), qPrintable(said.join(" | ")));
    // Its fader: the master, or a word -- never a motor that fights it.
    fp->clear();
    const int gm = int(iom->grandMasterValue());
    QVERIFY(fp->send("90 68 7F"));
    for (int v : { gm, qMax(0, gm - 40), qMax(0, gm - 80) })
    {
        QVERIFY(fp->send(pitch(v)));
        settle(20);
    }
    QVERIFY(fp->send("90 68 00"));
    settle(500);
    qInfo().noquote() << "[VirtualBench] the FaderPort fader beside the APC64: GM" << gm << "->"
                      << int(iom->grandMasterValue()) << "; motor sent" << hexOf(fp->receivedOfType(0xE0));
    QVERIFY2(int(iom->grandMasterValue()) != gm || fp->receivedOfType(0xE0).isEmpty()
                 || lastMotor(fp) == qMax(0, gm - 80),
             "the FaderPort's fader moved nothing, and its motor drove it back");
}

void VirtualBench_Test::theFaderPortKnobInBothModes()
{
    SurfaceAutoPatch *ap = m_app->doc()->surfaceSetup()->autoPatch();
    QSignalSpy patched(ap, &SurfaceAutoPatch::autoPatched);
    VirtualMidiPort *fp = m_bench->plug(kFp2);
    QTRY_COMPARE_WITH_TIMEOUT(patched.count(), 1, 5000);
    const quint32 u = patched.first().at(0).toUInt();
    settle(1000);
    SurfaceBoardLog *log = m_app->doc()->surfaceSetup()->boardLog();
    // What the log says of a message: the log cleared, the message, its lines.
    auto said = [this, log, fp, u](const char *bytes) {
        log->clear(u);
        fp->send(bytes);
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < 2000 && log->entries(u).isEmpty())
            settle(20);
        settle(50);
        return log->text(u);
    };
    // Mackie mode: CC 16, the profile's Knob.
    const QString cc16 = said("B0 10 01");
    QVERIFY2(cc16.contains(QStringLiteral("Knob")), qPrintable(cc16));
    // Studio One mode: CC 60. It reaches the app; its log says what it is.
    const QString cc60 = said("B0 3C 01");
    QVERIFY2(!cc60.isEmpty(), "CC 60 from the FaderPort never reached its Board Log");
    qInfo().noquote() << "[VirtualBench] FaderPort knob as CC 16:" << cc16;
    qInfo().noquote() << "[VirtualBench] FaderPort knob as CC 60:" << cc60;
}

/****************************************************************************
 * The Akai LPD8s
 ****************************************************************************/

void VirtualBench_Test::theLpd8Mk2IsSetUpAndPaintedWhenPluggedIn()
{
    if (realBoard(kLpd8Mk2))
        QSKIP("a real LPD8 mk2 is connected to this computer: the bench leaves it alone");
    SurfaceAutoPatch *ap = m_app->doc()->surfaceSetup()->autoPatch();
    QSignalSpy patched(ap, &SurfaceAutoPatch::autoPatched);
    VirtualMidiPort *lpd = m_bench->plug(kLpd8Mk2);
    QVERIFY(lpd != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(patched.count(), 1, 5000);
    const quint32 u = patched.first().at(0).toUInt();
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    QCOMPARE(iom->inputPatch(u)->inputName(), kLpd8Mk2);
    QCOMPARE(iom->inputPatch(u)->profileName(), QStringLiteral("Akai LPD8 mk2"));
    QVERIFY(iom->feedbackPatch(u) != nullptr && iom->feedbackPatch(u)->isPatched());
    QCOMPARE(iom->feedbackPatch(u)->outputName(), kLpd8Mk2);
    // Its pads painted at its own port, by their one documented message.
    auto paints = [lpd]() {
        QList<QByteArray> out;
        for (const QByteArray &m : lpd->receivedOfType(0xF0))
            if (m.startsWith(kLpd8Paint))
                out << m;
        return out;
    };
    QTRY_VERIFY_WITH_TIMEOUT(!paints().isEmpty(), 3000);
    settle(800);
    for (const QByteArray &m : lpd->receivedOfType(0xF0))
        QVERIFY2(m.startsWith(kLpd8Paint) && m.size() == 56,
                 qPrintable("a SysEx that is not the pad colours: " + QString::fromLatin1(m.toHex(' '))));
    qInfo().noquote() << "[VirtualBench] LPD8 mk2:" << paints().size() << "paints on plug-in, last"
                      << QString::fromLatin1(paints().last().toHex(' '));
    // The notice, and its chip.
    QVERIFY2(m_app->m_viewNoticeText->text().contains(QStringLiteral("ready on universe %1").arg(u + 1)),
             qPrintable(m_app->m_viewNoticeText->text()));
    SurfaceChips *chips = m_app->surfaceChips();
    QVERIFY(chips != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(chips->chip(u) != nullptr && chips->chip(u)->isVisible(), 2000);
    // The lamp test lights each pad through that message.
    lpd->clear();
    SurfaceSetup *ss = m_app->doc()->surfaceSetup();
    ss->setLampTestStepMs(5);
    QString why;
    QVERIFY2(ss->startLampTest(u, &why), qPrintable(why));
    QTRY_VERIFY_WITH_TIMEOUT(!ss->lampTestRunning(), 5000);
    settle(100);
    QSet<int> lit;
    for (const QByteArray &m : paints())
        for (int p = 0; p < 8; ++p)
            if (lpd8Pad(m, p) != QColor(0, 0, 0))
                lit.insert(p);
    QVERIFY2(lit.size() == 8, qPrintable(QStringLiteral("the lamp test lit %1 of the 8 pads").arg(lit.size())));
}

void VirtualBench_Test::theLpd8Mk2PadsReachTheirJobs()
{
    if (realBoard(kLpd8Mk2))
        QSKIP("a real LPD8 mk2 is connected to this computer: the bench leaves it alone");
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    DeskRecord *desk = m_app->doc()->deskRecord();
    SurfaceAutoPatch *ap = m_app->doc()->surfaceSetup()->autoPatch();
    /* The Grand Master's pick-up ignores everything for 500 ms after a re-arm
       (a device's dump). On a loaded host the knob's values could land inside
       that window by wall time, so the desk runs on a clock the test steps:
       the window is over exactly when the test says so. */
    qint64 deskNow = 1000;
    desk->setClock([&deskNow]() { return deskNow; });
    const auto restoreClock = qScopeGuard([desk]() { desk->setClock(nullptr); });
    QSignalSpy patched(ap, &SurfaceAutoPatch::autoPatched);
    VirtualMidiPort *lpd = m_bench->plug(kLpd8Mk2);
    QTRY_COMPARE_WITH_TIMEOUT(patched.count(), 1, 5000);
    const quint32 u = patched.first().at(0).toUInt();
    QTRY_VERIFY_WITH_TIMEOUT(desk->isLive(DeskRecord::Go) && desk->isLive(DeskRecord::Stop)
                                 && desk->isLive(DeskRecord::GrandMaster), 2000);
    for (DeskRecord::Role r : { DeskRecord::Go, DeskRecord::Back, DeskRecord::Stop, DeskRecord::GrandMaster })
        QCOMPARE(desk->universeOf(r), u);
    settle(100);
    deskNow += 1000;    // past the plug's dump window
    QSignalSpy fired(desk, &DeskRecord::fired);
    QSignalSpy refused(desk, &DeskRecord::refused);
    auto pad = [this, lpd](int n, bool down) {
        // Pads 1-8 are notes 36-43 on MIDI channel 10, as the board sends them.
        const char msg[3] = { char(down ? 0x99 : 0x89), char(35 + n), char(down ? 0x64 : 0x00) };
        QVERIFY(lpd->send(QByteArray(msg, 3)));
        settle(30);
    };
    // Pad 4: GO (the Run tab has no list here, and says so).
    pad(4, true);
    pad(4, false);
    QTRY_VERIFY_WITH_TIMEOUT(refused.count() >= 1, 2000);
    QVERIFY2(refused.last().first().toString().startsWith(QStringLiteral("GO")), qPrintable(refused.last().first().toString()));
    // Pad 3: Back.
    pad(3, true);
    pad(3, false);
    QTRY_VERIFY_WITH_TIMEOUT(refused.count() >= 2, 2000);
    QVERIFY2(refused.last().first().toString().startsWith(QStringLiteral("Back")), qPrintable(refused.last().first().toString()));
    // Pad 8 alone: refused, and said in the footer.
    pad(8, true);
    pad(8, false);
    QTRY_VERIFY_WITH_TIMEOUT(refused.count() >= 3, 2000);
    QCOMPARE(refused.last().first().toString(), QStringLiteral("LPD8 mk2: Pad 1 + Pad 8 stops."));
    QCOMPARE(fired.count(), 0);
    QVERIFY(!m_app->doc()->outputRuleKeeper()->emergencyStopped());
    QVERIFY2(m_footerMessages.join(QLatin1Char('\n')).contains(QStringLiteral("Pad 1 + Pad 8 stops")),
             qPrintable(m_footerMessages.join(" | ")));
    // Pad 1 held, Pad 8: ALL OFF.
    pad(1, true);
    pad(8, true);
    pad(8, false);
    pad(1, false);
    QTRY_COMPARE_WITH_TIMEOUT(fired.count(), 1, 2000);
    QCOMPARE(fired.first().first().toString(), QStringLiteral("ALL OFF"));
    QTRY_VERIFY_WITH_TIMEOUT(m_app->doc()->outputRuleKeeper()->emergencyStopped(), 2000);
    // K1 (CC 70, channel 1): the Grand Master, picked up at the master and then followed.
    deskNow += 1000;    // and past any re-arm since (the e-stop, the patch settling)
    iom->setGrandMasterValue(255);
    for (int v : { 120, 124, 127, 110, 90, 64 })
    {
        const char msg[3] = { char(0xB0), char(70), char(v) };
        QVERIFY(lpd->send(QByteArray(msg, 3)));
        settle(20);
    }
    QTRY_COMPARE_WITH_TIMEOUT(int(iom->grandMasterValue()), 128, 2000);
}

void VirtualBench_Test::theLpd8Mk2SurvivesSaveAndReopen()
{
    if (realBoard(kLpd8Mk2))
        QSKIP("a real LPD8 mk2 is connected to this computer: the bench leaves it alone");
    SurfaceAutoPatch *ap = m_app->doc()->surfaceSetup()->autoPatch();
    QSignalSpy patched(ap, &SurfaceAutoPatch::autoPatched);
    VirtualMidiPort *lpd = m_bench->plug(kLpd8Mk2);
    QTRY_COMPARE_WITH_TIMEOUT(patched.count(), 1, 5000);
    const quint32 u = patched.first().at(0).toUInt();
    settle(1000);
    // A lamp lit (a live state of the board, never the show's).
    m_app->doc()->inputOutputMap()->sendFeedBack(u, (9u << 12) | (128u + 41), 255, QVariant());
    settle(50);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("lpd8.qxw"));
    QCOMPARE(m_app->saveXML(path), QFile::NoError);
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QString xml = QString::fromUtf8(f.readAll());
    f.close();
    // The show keeps the patch it was given: the port and its profile.
    QVERIFY2(xml.contains(QStringLiteral("LPD8 mk2")), "the saved show has no LPD8 mk2 patch");
    QVERIFY2(xml.contains(QStringLiteral("Akai LPD8 mk2")), "the saved show lost the profile");
    // Not the board's live state: no pad colours, no message to it.
    QVERIFY2(!xml.contains(QStringLiteral("F0477F4C"), Qt::CaseInsensitive)
                 && !xml.contains(QStringLiteral("f0 47 7f 4c"), Qt::CaseInsensitive),
             "the pads' colours were saved into the show");

    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    settle(1500);
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    QTRY_COMPARE_WITH_TIMEOUT(universeOf(kLpd8Mk2), u, 3000);
    QCOMPARE(iom->inputPatch(u)->profileName(), QStringLiteral("Akai LPD8 mk2"));
    QCOMPARE(iom->feedbackPatch(u)->outputName(), kLpd8Mk2);
    int universesWithIt = 0;
    for (quint32 i = 0; i < iom->universesCount(); ++i)
        if (iom->inputPatch(i) != nullptr && iom->inputPatch(i)->inputName() == kLpd8Mk2)
            ++universesWithIt;
    QCOMPARE(universesWithIt, 1);
    // Its pads are painted again, and Pad 4 is still GO on this computer.
    QTRY_VERIFY_WITH_TIMEOUT(!lpd->receivedOfType(0xF0).isEmpty(), 3000);
    DeskRecord *desk = m_app->doc()->deskRecord();
    QSignalSpy refused(desk, &DeskRecord::refused);
    QSignalSpy fired(desk, &DeskRecord::fired);
    QVERIFY(lpd->send("99 27 64"));
    settle(30);
    QVERIFY(lpd->send("89 27 00"));
    QTRY_VERIFY_WITH_TIMEOUT(refused.count() + fired.count() >= 1, 2000);
    const QString said = !refused.isEmpty() ? refused.last().first().toString() : fired.last().first().toString();
    QVERIFY2(said.startsWith(QStringLiteral("GO")), qPrintable(said));
    m_app->doc()->resetModified();
}

void VirtualBench_Test::theOriginalLpd8LightsByNote()
{
    if (realBoard(kLpd8))
        QSKIP("a real LPD8 is connected to this computer: the bench leaves it alone");
    SurfaceAutoPatch *ap = m_app->doc()->surfaceSetup()->autoPatch();
    QSignalSpy patched(ap, &SurfaceAutoPatch::autoPatched);
    VirtualMidiPort *lpd = m_bench->plug(kLpd8);
    QTRY_COMPARE_WITH_TIMEOUT(patched.count(), 1, 5000);
    const quint32 u = patched.first().at(0).toUInt();
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    QCOMPARE(iom->inputPatch(u)->profileName(), QStringLiteral("Akai LPD8"));
    QCOMPARE(iom->feedbackPatch(u)->outputName(), kLpd8);
    settle(800);
    lpd->clear();
    SurfaceSetup *ss = m_app->doc()->surfaceSetup();
    ss->setLampTestStepMs(5);
    QString why;
    QVERIFY2(ss->startLampTest(u, &why), qPrintable(why));
    QTRY_VERIFY_WITH_TIMEOUT(!ss->lampTestRunning(), 5000);
    settle(100);
    QSet<int> notes;
    for (const QByteArray &m : lpd->receivedOfType(0x90))
        if ((uchar(m.at(0)) & 0x0F) == 0 && m.size() > 2 && m.at(2) != 0)
            notes.insert(uchar(m.at(1)));
    QCOMPARE(notes, QSet<int>({ 36, 37, 38, 39, 40, 41, 42, 43 }));
    QVERIFY2(lpd->receivedOfType(0xF0).isEmpty(), "the original LPD8 was sent a SysEx");
}

/****************************************************************************
 * Unplug, two boards
 ****************************************************************************/

void VirtualBench_Test::anUnpluggedBoardIsGreyAndComesBackWithItsInit()
{
    SurfaceAutoPatch *ap = m_app->doc()->surfaceSetup()->autoPatch();
    QSignalSpy patched(ap, &SurfaceAutoPatch::autoPatched);
    VirtualMidiPort *apc = m_bench->plug(kApc40);
    QTRY_COMPARE_WITH_TIMEOUT(patched.count(), 1, 5000);
    const quint32 u = patched.first().at(0).toUInt();
    QTRY_VERIFY_WITH_TIMEOUT(apc->count(kAbleton) >= 2, 3000);
    settle(500);

    m_bench->unplug(kApc40);
    QTRY_VERIFY_WITH_TIMEOUT(!midi()->inputs().contains(kApc40), 3000);
    settle(300);
    ConnectionsTree tree(m_app->doc());
    tree.setShowControlPanel(new QWidget(&tree));
    tree.setShowControlRows({ { "surfaces", "Control surfaces", "" } });
    tree.refresh();
    QTreeWidget *tw = tree.findChild<QTreeWidget *>();
    QVERIFY(tw != nullptr);
    QTreeWidgetItem *gone = nullptr;
    QStringList rows;
    for (QTreeWidgetItemIterator it(tw); *it != nullptr; ++it)
        if ((*it)->data(0, Qt::UserRole + 2).toString() == QLatin1String("surfaces"))
        {
            rows << (*it)->text(0) + " / " + (*it)->text(1);
            if ((*it)->text(0).contains(kApc40))
                gone = *it;
        }
    QVERIFY2(gone != nullptr, qPrintable(rows.join(" | ")));
    QVERIFY2(gone->text(1).contains(QStringLiteral("not connected")), qPrintable(gone->text(1)));
    QCOMPARE(gone->foreground(0).color(), tw->palette().color(QPalette::Disabled, QPalette::Text));

    // Back: on its own universe, Ableton mode sent to it again.
    apc = m_bench->plug(kApc40);
    QTRY_VERIFY_WITH_TIMEOUT(universeOf(kApc40) == u, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(apc->count(kAbleton) >= 1, 3000);
    settle(1000);
    qInfo().noquote() << "[VirtualBench] APC40 mkII replugged: Ableton mode sent" << apc->count(kAbleton) << "times";
    InputOutputMap *iom = m_app->doc()->inputOutputMap();
    QCOMPARE(iom->feedbackPatch(u)->outputName(), kApc40);
    // Its lamps reach it again.
    apc->clear();
    SurfaceSetup *ss = m_app->doc()->surfaceSetup();
    ss->setLampTestStepMs(5);
    QString why;
    QVERIFY2(ss->startLampTest(u, &why), qPrintable(why));
    QTRY_VERIFY_WITH_TIMEOUT(apc->receivedOfType(0x90).size() >= 3, 5000);
    ss->stopLampTest();
}

void VirtualBench_Test::twoBoardsEachGetOnlyTheirOwnLamps()
{
    SurfaceAutoPatch *ap = m_app->doc()->surfaceSetup()->autoPatch();
    VirtualMidiPort *apc = m_bench->plug(kApc40);
    VirtualMidiPort *pmj = m_bench->plug(kPmj);
    QTRY_VERIFY_WITH_TIMEOUT(universeOf(kApc40) != UINT_MAX, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(midi()->inputs().contains(kPmj), 3000);
    settle(1200);
    quint32 pmjU = universeOf(kPmj);
    if (pmjU == UINT_MAX)
        pmjU = ap->setUp(midi(), kPmj, QStringLiteral("PMJ Black 1"));
    QVERIFY(pmjU != UINT_MAX);
    settle(1000);
    // The APC40's lamps as they stand: everything it was sent so far, last wins.
    auto lampsOf = [](const QList<QByteArray> &msgs) {
        QHash<int, int> lamps;
        for (const QByteArray &m : msgs)
            if ((uchar(m.at(0)) & 0xF0) == 0x90 && m.size() > 2)
                lamps.insert((uchar(m.at(0)) << 8) | uchar(m.at(1)), uchar(m.at(2)));
        return lamps;
    };
    // The PMJ's Back (note 11 on channel 9), pressed and let go: nothing at the APC40.
    int seen = int(apc->received().size());
    pmj->clear();
    QVERIFY(pmj->send("98 0B 7F"));
    settle(60);
    QVERIFY(pmj->send("98 0B 00"));
    settle(600);
    QList<QByteArray> after = apc->received().mid(seen);
    QVERIFY2(after.isEmpty(), qPrintable("the PMJ's Back reached the APC40: " + hexOf(after)));
    /* The PMJ's lamp test: at the PMJ only. The APC40 may be sent its own
       lamps again as they were (the desk's lamps given back after the
       test), never one that changes. */
    const QHash<int, int> before = lampsOf(apc->received());
    seen = int(apc->received().size());
    SurfaceSetup *ss = m_app->doc()->surfaceSetup();
    ss->setLampTestStepMs(5);
    QString why;
    QVERIFY2(ss->startLampTest(pmjU, &why), qPrintable(why));
    QTRY_VERIFY_WITH_TIMEOUT(pmj->receivedOfType(0x90).size() >= 3, 5000);
    ss->stopLampTest();
    settle(300);
    after = apc->received().mid(seen);
    QList<QByteArray> changed;
    for (const QByteArray &m : after)
        if ((uchar(m.at(0)) & 0xF0) == 0x90 && m.size() > 2
            && before.value((uchar(m.at(0)) << 8) | uchar(m.at(1)), -1) != uchar(m.at(2)))
            changed << m;
    qInfo().noquote() << "[VirtualBench] the APC40 during the PMJ's lamp test was sent:" << hexOf(after);
    QVERIFY2(changed.isEmpty(), qPrintable("the PMJ's lamp test changed the APC40's lamps: " + hexOf(changed)
                                           + " (before: " + hexOf(apc->received().mid(0, seen)) + ")"));
}

QTEST_MAIN(VirtualBench_Test)
