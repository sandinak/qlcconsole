/*
  Q Light Controller Plus
  officeshow_test.cpp

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
#include <QTemporaryDir>
#include <QTabWidget>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QtTest>

#define private public
#define protected public
#include "app.h"
#include "showtreeview.h"
#undef protected
#undef private
#include "showtreecontroller.h"
#include "shutterautoopen.h"
#include "monitorproperties.h"
#include "qlcfixturedefcache.h"
#include "inputoutputmap.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "fixturegroup.h"
#include "stagetarget.h"
#include "showembedding.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "universe.h"
#include "fixture.h"
#include "stand.h"
#include "truss.h"
#include "doc.h"

#include "officeshow_test.h"

using namespace ShowTree;

namespace
{

const QString kProduction = QStringLiteral("Office Test");
const QString kList = QStringLiteral("Fixture check");

const QString kRing = QStringLiteral("Office Ring Mover");
const QString kSpot = QStringLiteral("Office Spot");
const QString kPar1 = QStringLiteral("Office Par 1");
const QString kPar2 = QStringLiteral("Office Par 2");
const QString k3Z = QStringLiteral("Office 3Z");

const QString kNote = QStringLiteral(
    "NOT PATCHED: universe U01-Office has no output yet. Before running, patch it "
    "(Inputs/Outputs) to the office Art-Net node on the rig network 172.18.0.0/20 "
    "-- its IP isn't known yet.");

/* The palettes, typed as a designer would. */
const QStringList kPaletteCommands = {
    "palette colour \"White\" #ffffff",
    "palette colour \"Red\" #ff0000",
    "palette colour \"Green\" #00ff00",
    "palette colour \"Blue\" #0000ff",
    "palette colour \"Amber\" #ffb000",
    "palette colour \"Magenta\" #ff00ff",
    // Black: a colour ring's "off" slot.
    "palette colour \"Black\" #000000",
    "palette dimmer \"Full\" 100",
    "palette dimmer \"Half\" 50",
    "palette dimmer \"Off\" 0",
    "palette shutter \"Shutter Open\"",
};

/* The cue list: one cue per check, each a few typed words. */
struct CueSpec { QString name; QStringList commands; };
const QList<CueSpec> kCues = {
    { "All White Full", { "All colour White dimmer Full", "Pars aim \"Test Area\"" } },
    { "Colour wash Red/Blue", { "Pars colour Red", "Movers colour Blue" } },
    { "Movers aim Test Area", { "Movers aim \"Test Area\"" } },
    { "Movers aim Presenter", { "Movers aim Presenter" } },
    { "Ring colour chase", { "Ring colour Red/Yellow-Chase" } },
    { "Blackout", { "All dimmer Off", "Ring colour Black" } },
};

void settle(int ms = 0)
{
    for (int i = 0; i < 3; ++i)
    {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
    }
    if (ms > 0)
        QTest::qWait(ms);
}

Fixture *fixtureNamed(Doc *doc, const QString &name)
{
    for (Fixture *f : doc->fixtures())
        if (f->name() == name)
            return f;
    return nullptr;
}

FixtureGroup *groupNamed(Doc *doc, const QString &name)
{
    for (FixtureGroup *g : doc->fixtureGroups())
        if (g->name() == name)
            return g;
    return nullptr;
}

QLCPalette *paletteNamed(Doc *doc, const QString &name)
{
    for (QLCPalette *p : doc->palettes())
        if (p->name() == name)
            return p;
    return nullptr;
}

/* A group of whole fixtures, one row, each fixture's heads side by side
   (@a onlyHead >= 0: just that head of each). */
void addGroup(Doc *doc, const QString &name, const QStringList &fixtures, int onlyHead = -1)
{
    FixtureGroup *g = new FixtureGroup(doc);
    g->setName(name);
    QList<GroupHead> heads;
    for (const QString &n : fixtures)
    {
        Fixture *f = fixtureNamed(doc, n);
        Q_ASSERT(f != nullptr);
        for (int h = 0; h < f->heads(); ++h)
            if (onlyHead < 0 || h == onlyHead)
                heads << GroupHead(f->id(), h);
    }
    g->setSize(QSize(heads.count(), 1));
    for (int i = 0; i < heads.count(); ++i)
        g->assignHead(QLCPoint(i, 0), heads.at(i));
    doc->addFixtureGroup(g);
}

/* What the rig sends now, after the universe's own steps (auto-open
   shutters among them): fixture @a name's channel named @a channel. */
int out(Doc *doc, const QString &name, const QString &channel)
{
    Fixture *fx = fixtureNamed(doc, name);
    if (fx == nullptr)
        return -1;
    for (quint32 c = 0; c < fx->channels(); ++c)
        if (fx->channel(c)->name() == channel)
        {
            QList<Universe *> us = doc->inputOutputMap()->claimUniverses();
            const QByteArray *post = us.at(int(fx->universe()))->postGMValues();
            const int a = int(fx->address() + c);
            const int v = a < post->size() ? uchar(post->at(a)) : 0;
            doc->inputOutputMap()->releaseUniverses(false);
            return v;
        }
    return -2;
}

/* Every channel of every fixture as sent now: "Office Spot: PAN=128 ...". */
QStringList dump(Doc *doc)
{
    QStringList lines;
    for (Fixture *fx : doc->fixtures())
    {
        QStringList vals;
        for (quint32 c = 0; c < fx->channels(); ++c)
            vals << QStringLiteral("%1=%2").arg(fx->channel(c)->name()).arg(out(doc, fx->name(), fx->channel(c)->name()));
        lines << QStringLiteral("  %1: %2").arg(fx->name(), vals.join(QStringLiteral(", ")));
    }
    return lines;
}

QStringList s_messages;

void collect(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg)
        s_messages << msg;
}

}

void OfficeShow_Test::initTestCase()
{
    m_app = new App();
    m_app->init();
    m_app->showNormal();
    m_app->resize(1280, 800);
}

void OfficeShow_Test::cleanupTestCase()
{
    settle();
    m_app->doc()->resetModified();
    delete m_app;
    m_app = nullptr;
}

bool OfficeShow_Test::open(const QString &path)
{
    m_app->doc()->resetModified();
    m_app->clearDocument();
    const bool ok = m_app->loadXML(path) == QFile::NoError;
    m_app->doc()->resetModified();
    settle(50);
    return ok;
}

ShowTreeView *OfficeShow_Test::showTree() const
{
    ShowTreeView *view = ShowTreeView::instance();
    QWidget *w = view;
    while (w != nullptr && m_app->m_tab->indexOf(w) < 0)
        w = w->parentWidget();
    if (w != nullptr)
        m_app->m_tab->setCurrentWidget(w);
    settle();
    return view;
}

void OfficeShow_Test::buildTheShow()
{
    const QString rig = qEnvironmentVariable("OFFICE_RIG");
    const QString outPath = qEnvironmentVariable("OFFICE_OUT");
    const QString userFixtures = qEnvironmentVariable("OFFICE_USER_FIXTURES");
    if (rig.isEmpty() || outPath.isEmpty() || userFixtures.isEmpty())
        QSKIP("set OFFICE_RIG, OFFICE_OUT and OFFICE_USER_FIXTURES");

    // The newest definitions: Branson's user folder first (his LM70, LED
    // SPOT and Focus Spot 3Z, as DanceHysteria embeds them), then the
    // two-head ring mover, then what the app ships. The first of a model
    // read is the one used.
    QLCFixtureDefCache *cache = m_app->doc()->fixtureDefCache();
    QVERIFY(cache->load(QDir(userFixtures)));
    QTemporaryDir ringDir;
    QVERIFY(QFile::copy(QStringLiteral(TEST_FIXTURES_DIR "/Branson-LED-Movinghead+Circle-2heads.qxf"),
                        ringDir.filePath("Branson-LED-Movinghead+Circle-2heads.qxf")));
    QVERIFY(cache->load(QDir(ringDir.path())));
    QVERIFY(cache->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures")));

    QVERIFY(open(rig));
    Doc *doc = m_app->doc();
    QCOMPARE(doc->fixtures().count(), 5);
    for (Fixture *fx : doc->fixtures())
    {
        QVERIFY2(!fx->definitionUnavailable() && fx->fixtureDef() != nullptr, qPrintable(fx->name()));
        QVERIFY2(fx->fixtureDef()->source() == QLCFixtureDef::Local, qPrintable(fx->name()));
        qInfo().noquote() << "[DEF]" << fx->name() << "<-" << fx->fixtureDef()->definitionSourceFile()
                          << "mode" << fx->fixtureMode()->name() << "heads" << fx->heads();
    }
    QCOMPARE(fixtureNamed(doc, kRing)->heads(), 2);

    // ---- the office stage (a guess at a small office test rig, in metres:
    // X stage left, Y downstage, Z up; world 0,0 is the floor's corner)
    MonitorProperties *mp = doc->monitorProperties();
    mp->setGridUnits(MonitorProperties::Feet);
    mp->setGridSize(QVector3D(16, 9, 14));      // a 16 x 14 ft floor, 9 ft ceiling

    // Two wind-up stands upstage, 3 m apart, tops at 2.2 m, a 12" box
    // truss across them: the whole rig hangs on it.
    const QVector3D centre = mp->stageCentre();
    const float standY = 0.90f, standH = 2.20f, span = 3.0f;
    Stand *sr = mp->addStand();
    sr->setName("SR Stand");
    sr->setOriginX(centre.x() - span / 2);
    sr->setOriginY(standY);
    sr->setHeight(standH);
    sr->setColor(QColor("#303034"));
    Stand *sl = mp->addStand();
    sl->setName("SL Stand");
    sl->setOriginX(centre.x() + span / 2);
    sl->setOriginY(standY);
    sl->setHeight(standH);
    sl->setColor(QColor("#303034"));
    Truss *truss = mp->addTruss();
    truss->setName("Office Truss");
    truss->setType(Truss::Horizontal);
    truss->setProfile(Truss::SquareTruss);
    truss->setWidth(0.29f);
    truss->setStandId(sr->id());
    truss->setEndStandId(sl->id());
    truss->setOrigin(sr->topPos());
    truss->setDirection(QPointF(1, 0));
    truss->setLength(span);
    mp->recomputeChildTrusses();
    QVERIFY(qAbs(truss->length() - span) < 0.01f);

    // Hung under the truss, all facing downstage at the test area: a par at
    // each end, the LED Spot SR, the ring mover in the middle, the 3Z SL.
    auto hang = [&](const QString &name, float along) {
        Fixture *f = fixtureNamed(doc, name);
        FixtureRigProps rp = mp->fixtureRigProps(f->id());
        rp.clearMounts();
        rp.trussId = truss->id();
        rp.trussOffset = along;
        rp.trussMountSide = FixtureRigProps::UnderHung;
        rp.mountingType = Truss::TopHung;
        rp.panZeroDir = 0.0f;
        mp->setFixtureRigProps(f->id(), rp);
        const QVector3D at = truss->positionAt(along);
        mp->setFixturePosition(f->id(), 0, 0, QVector3D(at.x() * 1000.0f, at.y() * 1000.0f, 0));
        if (truss->groupId() != 0)
            mp->setFixtureGroup(f->id(), truss->groupId());
    };
    hang(kPar1, 0.20f);
    hang(kSpot, 0.85f);
    hang(kRing, 1.50f);
    hang(k3Z, 2.15f);
    hang(kPar2, 2.80f);

    // Stage targets: the test area at centre, a presenter downstage of it
    // and a little stage right.
    StageTarget *area = mp->addStageTarget();
    area->setName("Test Area");
    area->setKind(StageTarget::Kind::Area);
    area->setPosition(QVector3D(centre.x(), centre.y(), 0));
    area->setFootprint(QSizeF(1.5, 1.2));
    area->setColor(QColor("#3cb4ff"));
    StageTarget *presenter = mp->addStageTarget();
    presenter->setName("Presenter");
    presenter->setKind(StageTarget::Kind::Person);
    presenter->setPosition(QVector3D(centre.x() - 0.6f, centre.y() + 0.8f, 0));
    presenter->setPersonHeight(1.75f);
    presenter->setColor(QColor("#ffb400"));
    mp->notifyStageTargetsChanged();

    // ---- groups
    addGroup(doc, "All", { kRing, kSpot, k3Z, kPar1, kPar2 });
    addGroup(doc, "Movers", { kRing, kSpot, k3Z });
    addGroup(doc, "Pars", { kPar1, kPar2 });
    addGroup(doc, "Spots", { kSpot, k3Z });
    // The ring mover's colour ring (its second head): what a ring look sets.
    addGroup(doc, "Ring", { kRing }, 1);

    // ---- palettes, typed; aims as a drop of the target makes them
    ShowTreeView *view = showTree();
    QVERIFY(view != nullptr);
    for (const QString &cmd : kPaletteCommands)
        QVERIFY2(view->runCommand(cmd), qPrintable(cmd + ": " + view->commandMessage()));
    for (StageTarget *t : mp->stageTargets())
    {
        QLCPalette *p = new QLCPalette(QLCPalette::Aim, doc);
        p->setName(QStringLiteral("Aim %1").arg(t->name()));
        p->setStageTargetId(t->id());
        QVERIFY(doc->addPalette(p));
    }

    // ---- the production and its cue list, typed
    QVERIFY2(view->runCommand(QStringLiteral("production \"%1\"").arg(kProduction)), qPrintable(view->commandMessage()));
    QVERIFY2(view->runCommand(QStringLiteral("list \"%1\"").arg(kList)), qPrintable(view->commandMessage()));
    for (const CueSpec &c : kCues)
    {
        QVERIFY2(view->runCommand(QStringLiteral("cue + \"%1\"").arg(c.name)), qPrintable(view->commandMessage()));
        for (const QString &cmd : c.commands)
            QVERIFY2(view->runCommand(cmd), qPrintable(cmd + ": " + view->commandMessage()));
    }

    ShowTreeController *st = doc->showTree();
    QCOMPARE(st->productions().count(), 1);
    QCOMPARE(st->productions().first().lists.count(), 1);
    const CueList &list = st->productions().first().lists.first();
    QCOMPARE(list.cues.count(), kCues.count());
    // The note: on the first cue, where the operator starts.
    Cue first = list.cues.first();
    first.note = kNote;
    QVERIFY(st->setCue(first));
    if (st->currentProduction() != st->productions().first().id)
        QVERIFY(view->runCommand(QStringLiteral("production \"%1\" current").arg(kProduction)));

    // Unpatched, on the office network, auto-open on.
    InputOutputMap *io = doc->inputOutputMap();
    QCOMPARE(io->universesCount(), quint32(1));
    QCOMPARE(io->outputPatchesCount(0), 0);
    QCOMPARE(io->rigNetworks(), QStringList() << "172.18.0.0/20");
    QVERIFY(doc->shutterAutoOpen()->isEnabled());

    QCOMPARE(m_app->saveXML(outPath), QFile::NoError);
    doc->resetModified();
}

void OfficeShow_Test::verifyTheShow()
{
    const QString path = qEnvironmentVariable("OFFICE_SHOW");
    if (path.isEmpty())
        QSKIP("set OFFICE_SHOW to the saved Office.qlcc");

    // This computer has the app's own definitions only: no user folder.
    QLCFixtureDefCache *cache = m_app->doc()->fixtureDefCache();
    QVERIFY(!QLCFixtureDefCache::userDefinitionDirectory().exists()
            || QLCFixtureDefCache::userDefinitionDirectory().entryList(QDir::Files).isEmpty());
    QVERIFY(cache->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures")));

    s_messages.clear();
    QtMessageHandler old = qInstallMessageHandler(collect);
    const bool loaded = open(path);
    settle(300);
    qInstallMessageHandler(old);
    QVERIFY(loaded);
    for (const QString &m : s_messages)
        qInfo().noquote() << "[LOAD WARNING]" << m.left(300);

    Doc *doc = m_app->doc();
    InputOutputMap *io = doc->inputOutputMap();
    QCOMPARE(io->universesCount(), quint32(1));
    QCOMPARE(io->getUniverseNameByID(0), QStringLiteral("U01-Office"));
    QCOMPARE(io->outputPatchesCount(0), 0);            // sends nowhere
    QVERIFY(io->inputPatch(0) == nullptr);
    QCOMPARE(io->rigNetworks(), QStringList() << "172.18.0.0/20");
    QVERIFY(doc->shutterAutoOpen()->isEnabled());

    // Every fixture with its own definition, out of the show file.
    // Out of the show file: the show's own copy (its working folder or the
    // show cache), not this computer's.
    const QString showCache = QLCFixtureDefCache::showCacheDirectory().absolutePath();
    auto fromShow = [&](const QString &src) {
        const QString work = doc->showEmbedding()->workingDirectory();
        return src.startsWith(showCache) || (!work.isEmpty() && src.startsWith(work));
    };
    const QHash<QString, QPair<int, QString>> expect = {
        { kRing, { 1, "LED Movinghead+Circle (2 heads)|12 Channel" } },
        { kSpot, { 13, "LED SPOT|10 Channel" } },
        { kPar1, { 25, "LM70|9 Channel" } },
        { kPar2, { 34, "LM70|9 Channel" } },
        { k3Z, { 43, "Focus Spot Three Z|18 Channel" } },
    };
    QCOMPARE(doc->fixtures().count(), expect.count());
    for (Fixture *fx : doc->fixtures())
    {
        QVERIFY2(expect.contains(fx->name()), qPrintable(fx->name()));
        QVERIFY2(!fx->definitionUnavailable() && fx->fixtureDef() != nullptr, qPrintable(fx->name()));
        const QString src = fx->fixtureDef()->definitionSourceFile();
        qInfo().noquote() << "[FIXTURE]" << fx->name() << "DMX" << fx->address() + 1
                          << fx->fixtureDef()->manufacturer() << fx->fixtureDef()->model()
                          << fx->fixtureMode()->name() << "<-" << src;
        // Embedded in the file, every one; in use, the show's copy -- or
        // this computer's when it is the same byte for byte (the 3Z's is).
        const QString key = QStringLiteral("fixture:%1|%2").arg(fx->fixtureDef()->manufacturer(),
                                                                fx->fixtureDef()->model());
        QVERIFY2(doc->showEmbedding()->hasItem(key), qPrintable(key));
        const ShowEmbedding::Item item = doc->showEmbedding()->item(key);
        qInfo().noquote() << "[EMBEDDED]" << key << "state" << int(item.state)
                          << (fromShow(src) ? "using the show's copy" : "identical to this computer's");
        QVERIFY2(fromShow(src) || item.state == ShowEmbedding::Identical,
                 qPrintable(fx->name() + " from " + src));
        QCOMPARE(int(fx->address()) + 1, expect.value(fx->name()).first);
        QCOMPARE(fx->fixtureDef()->model() + "|" + fx->fixtureMode()->name(), expect.value(fx->name()).second);
        QCOMPARE(fx->universe(), quint32(0));
    }

    for (const QString g : { "All", "Movers", "Pars", "Spots", "Ring" })
        QVERIFY2(groupNamed(doc, g) != nullptr, qPrintable(g));
    for (const QString &cmd : kPaletteCommands)
        QVERIFY2(paletteNamed(doc, cmd.section('"', 1, 1)) != nullptr, qPrintable(cmd));
    QVERIFY(paletteNamed(doc, "Aim Test Area") != nullptr);
    QVERIFY(paletteNamed(doc, "Aim Presenter") != nullptr);

    // The stage.
    MonitorProperties *mp = doc->monitorProperties();
    QCOMPARE(mp->stands().count(), 2);
    QCOMPARE(mp->trusses().count(), 1);
    QCOMPARE(mp->sceneries().count(), 0);              // no drape
    QCOMPARE(mp->stageTargets().count(), 2);
    for (Fixture *fx : doc->fixtures())
        qInfo().noquote() << "[RIG]" << fx->name() << mp->mountDescription(fx->id())
                          << "at" << mp->fixtureRigPosition(fx->id());
    for (StageTarget *t : mp->stageTargets())
        qInfo().noquote() << "[TARGET]" << t->name() << StageTarget::kindToString(t->kind()) << t->position();

    // The cues: each lights what it says.
    ShowTreeController *st = doc->showTree();
    QCOMPARE(st->productions().count(), 1);
    QCOMPARE(st->productions().first().name, kProduction);
    QCOMPARE(st->currentProduction(), st->productions().first().id);
    const CueList list = st->productions().first().lists.first();
    QCOMPARE(list.name, kList);
    QCOMPARE(list.cues.count(), kCues.count());
    QVERIFY(list.cues.first().note.contains("NOT PATCHED"));

    ShowTreeView *view = showTree();
    QHash<QString, QPair<int, int>> aimed;     // cue -> (ring pan, ring tilt)
    for (int i = 0; i < list.cues.count(); ++i)
    {
        const Cue &c = list.cues.at(i);
        QCOMPARE(c.name, kCues.at(i).name);
        QVERIFY(view->selectNode(c.id));
        settle(600);
        qInfo().noquote() << QStringLiteral("[CUE %1 %2]").arg(c.number, c.name);
        for (const QString &l : dump(doc))
            qInfo().noquote() << l;

        auto lit = [&](const QString &fx, const QString &ch) {
            const int v = out(doc, fx, ch);
            QVERIFY2(v > 0, qPrintable(QStringLiteral("%1: %2 %3 = %4").arg(c.name, fx, ch).arg(v)));
        };
        auto dark = [&](const QString &fx, const QString &ch) {
            const int v = out(doc, fx, ch);
            QVERIFY2(v == 0, qPrintable(QStringLiteral("%1: %2 %3 = %4").arg(c.name, fx, ch).arg(v)));
        };
        if (i <= 4)
        {
            lit(kRing, "Dimmer");
            lit(kSpot, "Dimmer");
            lit(k3Z, "Master Dimmer");
            lit(k3Z, "Strobe/Shutter");             // auto-opened
            lit(kPar1, "Intensity");
            lit(kPar2, "Intensity");
        }
        if (i == 0)
        {
            for (const QString &p : { kPar1, kPar2 })
                for (const QString ch : { "Red", "Green", "Blue", "Tilt" })
                    lit(p, ch);
        }
        if (i == 1)
        {
            for (const QString &p : { kPar1, kPar2 })
            {
                lit(p, "Red");
                dark(p, "Blue");
                dark(p, "Green");
            }
            // Blue on the wheels (30-39 on the ring mover's and the LED Spot's).
            QVERIFY(out(doc, kRing, "Color wheel") >= 30 && out(doc, kRing, "Color wheel") <= 39);
            QVERIFY(out(doc, kSpot, "Color") >= 30 && out(doc, kSpot, "Color") <= 39);
            lit(k3Z, "Color");
        }
        if (i == 2 || i == 3)
        {
            for (const QString &m : { kRing, kSpot })
                lit(m, m == kRing ? "Tilt" : "Tilt");
            lit(k3Z, "Tilt Movement");
            aimed.insert(c.name, qMakePair(out(doc, kRing, "Pan"), out(doc, kRing, "Tilt")));
        }
        if (i == 4)
        {
            const int ring = out(doc, kRing, "Circle Color");
            QVERIFY2(ring >= 110 && ring <= 124, qPrintable(QString::number(ring)));
        }
        if (i == 5)
        {
            dark(kRing, "Dimmer");
            dark(kSpot, "Dimmer");
            dark(k3Z, "Master Dimmer");
            dark(kPar1, "Intensity");
            dark(kPar2, "Intensity");
            QVERIFY(out(doc, kRing, "Circle Color") <= 4);  // the ring off
        }
    }
    // The two aims are two different places.
    QVERIFY(aimed.value("Movers aim Test Area") != aimed.value("Movers aim Presenter"));
    doc->resetModified();
}

int main(int argc, char **argv)
{
    // A home of its own: no user fixture folder (verifyTheShow's
    // definitions must come from the show), and nothing touches yours.
    QTemporaryDir home;
    qputenv("HOME", home.path().toUtf8());
    qputenv("QLC_NO_RECOVERY_PROMPT", "1");
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    OfficeShow_Test test;
    return QTest::qExec(&test, argc, argv);
}
