/*
  Q Light Controller Plus
  pmsrehearsal_test.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QTemporaryDir>
#include <QTableWidget>
#include <QMessageBox>
#include <QInputDialog>
#include <QApplication>
#include <QListWidget>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QPointer>
#include <QLabel>
#include <QFile>
#include <QDir>
#include <QtTest>
#include <cmath>

#include "../../../engine/test/common/stdprelude.h"
/* The tick by hand, as the engine tests drive it. */
#define protected public
#define private public
#include "mastertimer.h"
#include "universe.h"
#undef private
#undef protected

#include "pmsrehearsal_test.h"
#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "venuefromshowdialog.h"
#include "monitorproperties.h"
#include "measurementunits.h"
#include "venuecorrections.h"
#include "roomoutputpicker.h"
#include "rolefilldialog.h"
#include "venuerigdialog.h"
#include "inputoutputmap.h"
#include "functionparent.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcfixturedef.h"
#include "qlcfixturedefcache.h"
#include "ioplugincache.h"
#include "showembedding.h"
#include "iopluginstub.h"
#include "fixturegroup.h"
#include "stageplatform.h"
#include "stagetarget.h"
#include "venueimport.h"
#include "qlcchannel.h"
#include "venuemenu.h"
#include "outputpatch.h"
#include "grouphead.h"
#include "houserole.h"
#include "rigowner.h"
#include "function.h"
#include "fixture.h"
#include "qlcfile.h"
#include "scenery.h"
#include "doc.h"
#include "../../../engine/test/common/pmslikeshow.h"

#define TESTPLUGINDIR "../../../engine/test/iopluginstub"

namespace
{
const QString kRoom = QStringLiteral("PMS Auditeria");

/* A made-up touring show shaped like Dance Hysteria where it matters here:
   a 50 ft grid, a cyc upstage (its upstage line), a main drape, tour
   lights on two universes sent by Art-Net, a third universe kept free for
   the room, a deck of platforms. Nothing in it is from a real file. */
QByteArray tourXml()
{
    QByteArray out;
    out += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE Workspace>\n"
           "<Workspace xmlns=\"http://www.qlcplus.org/Workspace\" CurrentWindow=\"Monitor\">\n"
           " <Creator><Name>qlcconsole</Name><Version>test</Version><Author>test</Author></Creator>\n"
           " <Engine>\n  <InputOutputMap>\n";
    for (int u = 0; u < 2; ++u)
        out += QStringLiteral("   <Universe Name=\"U%1-Tour\" ID=\"%2\">\n"
                              "    <Output Plugin=\"ArtNet\" UID=\"10.9.9.17\" Line=\"2\">\n"
                              "     <PluginParameters outputIP=\"10.9.9.6%2\"/>\n    </Output>\n   </Universe>\n")
                   .arg(u + 1).arg(u).toUtf8();
    out += "   <Universe Name=\"U3-Spare\" ID=\"2\"/>\n  </InputOutputMap>\n";
    for (int i = 0; i < 8; ++i)
        out += QStringLiteral("  <Fixture><Manufacturer>Generic</Manufacturer><Model>Generic</Model>"
                              "<Mode>1 Channel</Mode><ID>%1</ID><Name>Tour par %2</Name><Universe>%3</Universe>"
                              "<Address>%4</Address><Channels>1</Channels></Fixture>\n")
                   .arg(i).arg(i + 1).arg(i / 4).arg(i % 4).toUtf8();
    out += "  <FixtureGroup ID=\"0\"><Name>DS1f</Name><Size X=\"4\" Y=\"1\"/>";
    for (int i = 0; i < 4; ++i)
        out += QStringLiteral("<Head X=\"%1\" Y=\"0\" Fixture=\"%1\">0</Head>").arg(i).toUtf8();
    out += "</FixtureGroup>\n"
           "  <Monitor DisplayMode=\"1\">\n   <Grid Width=\"50\" Height=\"3\" Depth=\"36\" Units=\"1\"/>\n";
    for (int i = 0; i < 8; ++i)
        out += QStringLiteral("   <FxItem ID=\"%1\" XPos=\"%2\" YPos=\"%3\" ZPos=\"3\"/>\n")
                   .arg(i).arg(3000 + 1200 * (i % 4)).arg(i < 4 ? 4000 : 6000).toUtf8();
    out += "   <StagePlatform ID=\"0\" Name=\"USP\" OriginX=\"2.744\" OriginY=\"3.760\" Width=\"9.753\" "
           "Depth=\"3.657\" Height=\"0.396\"/>\n"
           "   <Scenery ID=\"0\" Name=\"Cyclorama 1\" Kind=\"0\" OriginX=\"7.620\" OriginY=\"1.355\" "
           "Width=\"11.192\" Height=\"6.002\"/>\n"
           "   <Scenery ID=\"1\" Name=\"Main drape\" Kind=\"2\" OriginX=\"7.620\" OriginY=\"7.925\" "
           "Width=\"16.764\" Height=\"6.002\" State=\"Open\" OpeningWidth=\"12.192\"/>\n"
           "  </Monitor>\n </Engine>\n</Workspace>\n";
    return out;
}

/* Its venues: an office venue, applied, with its own patch for U1. */
QByteArray tourVenuesXml(const QString &show)
{
    return QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                          "<QlcVenues Version=\"2\" Show=\"%1\" Active=\"Office\">\n"
                          " <Venue Name=\"Office\" Created=\"2026-10-30T14:22:48\" Modified=\"2026-10-30T14:22:48\">\n"
                          "  <Patch Universe=\"0\">\n"
                          "   <Output Plugin=\"ArtNet\" UID=\"10.9.9.17\" Line=\"2\">\n"
                          "    <PluginParameters outputIP=\"10.9.9.70\"/>\n   </Output>\n  </Patch>\n"
                          " </Venue>\n</QlcVenues>\n").arg(show).toUtf8();
}

/* A light the test can colour or aim, as engine/test/venuerig makes them. */
void testDef(Doc *doc, const QString &model, const QString &modeName,
             const QList<QPair<QLCChannel::Group, QLCChannel::PrimaryColour>> &channels)
{
    if (doc->fixtureDefCache()->fixtureDef(QStringLiteral("Test"), model) != nullptr)
        return;
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer(QStringLiteral("Test"));
    def->setModel(model);
    def->setType(QLCFixtureDef::ColorChanger);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName(modeName);
    QLCFixtureHead head;
    for (int k = 0; k < channels.size(); ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QStringLiteral("Ch %1").arg(k));
        ch->setGroup(channels.at(k).first);
        ch->setColour(channels.at(k).second);
        def->addChannel(ch);
        mode->insertChannel(ch, quint32(k));
        head.addChannel(quint32(k));
    }
    mode->insertHead(-1, head);
    def->addMode(mode);
    if (!doc->fixtureDefCache()->addFixtureDef(def))
        delete def;
}

bool writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    return f.open(QIODevice::WriteOnly | QIODevice::Truncate) && f.write(data) == data.size();
}

QByteArray readFile(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

/* The show opened as File > Open does (App::loadXML): its embedded
   definitions, the engine with its universes, then its venues file, which
   applies the venue it was left on. */
Doc *openShow(QObject *parent, const QString &path, QString *why)
{
    Doc *doc = new Doc(parent);
    QDir plugins(TESTPLUGINDIR);
    plugins.setFilter(QDir::Files);
    plugins.setNameFilters({ QStringLiteral("*%1").arg(KExtPlugin) });
    doc->ioPluginCache()->load(plugins);
    doc->fixtureDefCache()->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + QStringLiteral("fixtures")));
    testDef(doc, QStringLiteral("Focus Spot Three Z"), QStringLiteral("3ch"),
            { { QLCChannel::Pan, QLCChannel::NoColour }, { QLCChannel::Tilt, QLCChannel::NoColour },
              { QLCChannel::Intensity, QLCChannel::NoColour } });
    testDef(doc, QStringLiteral("ColorSource CYC"), QStringLiteral("3ch RGB"),
            { { QLCChannel::Intensity, QLCChannel::Red }, { QLCChannel::Intensity, QLCChannel::Green },
              { QLCChannel::Intensity, QLCChannel::Blue } });
    doc->setWorkspacePath(QFileInfo(path).absolutePath());
    if (doc->showEmbedding()->readWorkspace(path))
        doc->showEmbedding()->apply(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        *why = QStringLiteral("can't read %1").arg(path);
        return doc;
    }
    QXmlStreamReader r(&f);
    bool loaded = false;
    while (!r.atEnd() && !loaded)
        if (r.readNextStartElement() && r.name() == QLatin1String("Engine"))
            loaded = doc->loadXML(r);
    if (!loaded)
        *why = QStringLiteral("%1 did not load").arg(path);
    doc->programmer()->venueCorrections()->setShowFile(path, true);
    doc->resetModified();
    return doc;
}

/* Save as File > Save does (App::saveXML): the engine in a workspace,
   then the venues follow the file. */
bool saveShow(Doc *doc, const QString &path)
{
    QByteArray xml;
    {
        QXmlStreamWriter w(&xml);
        w.setAutoFormatting(true);
        w.writeStartDocument();
        w.writeDTD(QStringLiteral("<!DOCTYPE Workspace>"));
        w.writeStartElement(QStringLiteral("Workspace"));
        w.writeAttribute(QStringLiteral("xmlns"), QStringLiteral("http://www.qlcplus.org/Workspace"));
        doc->saveXML(&w);
        w.writeEndElement();
        w.writeEndDocument();
    }
    if (!writeFile(path, xml))
        return false;
    doc->programmer()->venueCorrections()->setShowFile(path, false);
    doc->resetModified();
    return true;
}

IOPluginStub *stubOf(Doc *doc)
{
    const QList<QLCIOPlugin *> plugins = doc->ioPluginCache()->plugins();
    return plugins.isEmpty() ? nullptr : static_cast<IOPluginStub *>(plugins.first());
}

void tick(Doc *doc, int n)
{
    for (int i = 0; i < n; ++i)
    {
        doc->masterTimer()->timerTick();
        QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
        for (Universe *u : ua)
            u->processFaders();
        doc->inputOutputMap()->releaseUniverses(false);
    }
}

/* What a universe sends (after the Grand Master and the output rules). */
QByteArray universeOut(Doc *doc, quint32 universe)
{
    QList<Universe *> ua = doc->inputOutputMap()->claimUniverses();
    const QByteArray out = int(universe) < ua.size() ? QByteArray(*ua.at(int(universe))->postGMValues())
                                                     : QByteArray();
    doc->inputOutputMap()->releaseUniverses(false);
    return out;
}

/* Every channel of @p ids on the room's universe: the lowest and the highest. */
QPair<int, int> levels(Doc *doc, const QList<quint32> &ids)
{
    int lo = 256, hi = -1;
    for (quint32 id : ids)
    {
        const Fixture *fx = doc->fixture(id);
        if (fx == nullptr || !fx->isPatched())
            continue;
        const QByteArray out = universeOut(doc, fx->universe());
        for (quint32 c = 0; c < fx->channels(); ++c)
        {
            const int at = int(fx->address() + c);
            const int v = at < out.size() ? uchar(out.at(at)) : 0;
            lo = qMin(lo, v);
            hi = qMax(hi, v);
        }
    }
    return qMakePair(lo, hi);
}

Function *standardHit(Doc *doc, const QString &key)
{
    ShowTreeController *st = doc->showTree();
    const quint32 cue = HouseRole::standardCue(doc, key);
    for (const ShowTreeController::ReusableUse &u : st->reusableUses(cue))
        if (st->isLooks(u.list))
            return doc->function(st->lookFunction(u.cue));
    return nullptr;
}

void stopAll(Doc *doc)
{
    for (Function *f : doc->functions())
        f->stop(FunctionParent::master());
    tick(doc, 300);
}

/* One element per item of the room file's <Monitor>: <FxItem> places
   (mm) and its scenery (metres), by id. */
struct RoomPlot
{
    QMap<quint32, QPointF> items;
    QMap<QString, QRectF> scenery;     //!< name -> x across (width), y its line
    QMap<QString, int> kinds;
};

RoomPlot readRoomPlot(const QByteArray &monitorXml)
{
    RoomPlot p;
    QXmlStreamReader r(monitorXml);
    while (!r.atEnd())
    {
        if (!r.readNextStartElement())
            continue;
        const QXmlStreamAttributes a = r.attributes();
        if (r.name() == QLatin1String("FxItem"))
            p.items.insert(a.value(QLatin1String("ID")).toUInt(),
                           QPointF(a.value(QLatin1String("XPos")).toDouble(), a.value(QLatin1String("YPos")).toDouble()));
        else if (r.name() == QLatin1String("Scenery"))
        {
            const QString name = a.value(QLatin1String("Name")).toString();
            const double x = a.value(QLatin1String("OriginX")).toDouble();
            const double w = a.hasAttribute(QLatin1String("OpeningWidth"))
                             ? a.value(QLatin1String("OpeningWidth")).toDouble()
                             : a.value(QLatin1String("Width")).toDouble();
            p.scenery.insert(name, QRectF(x - w / 2.0, a.value(QLatin1String("OriginY")).toDouble(), w, 0.0));
            p.kinds.insert(name, a.value(QLatin1String("Kind")).toInt());
        }
    }
    return p;
}

QString len(double mm)
{
    return MeasurementUnits::formatLength(mm);
}

/* The show as it stands, for "unchanged": every fixture's XML, every
   group's heads, every universe's output, the targets. */
QMap<QString, QByteArray> snapshot(Doc *doc)
{
    QMap<QString, QByteArray> out;
    for (Fixture *fx : doc->fixtures())
    {
        QByteArray xml;
        QXmlStreamWriter w(&xml);
        fx->saveXML(&w);
        const QVector3D at = doc->monitorProperties()->fixturePosition(fx->id(), 0, 0);
        xml += QStringLiteral(" @%1,%2,%3").arg(qRound(at.x())).arg(qRound(at.y())).arg(qRound(at.z())).toUtf8();
        out.insert(QStringLiteral("fixture %1").arg(fx->id()), xml);
    }
    for (FixtureGroup *g : doc->fixtureGroups())
    {
        if (HouseRole::roleOf(doc, g->id()) != HouseRole::None)
            continue;           // made by the first Fill Roles: new, not changed
        QByteArray heads;
        for (const GroupHead &h : g->headList())
            heads += QByteArray::number(h.fxi) + ':' + QByteArray::number(h.head) + ' ';
        out.insert(QStringLiteral("group %1 %2").arg(g->id()).arg(g->name()), heads);
    }
    InputOutputMap *iom = doc->inputOutputMap();
    for (quint32 u = 0; u < iom->universesCount(); ++u)
        out.insert(QStringLiteral("universe %1").arg(u + 1),
                   VenueRig::outputText(iom->universe(u)->outputPatchDescriptions()).toUtf8());
    for (const StageTarget *t : doc->monitorProperties()->stageTargets())
    {
        const QVector3D p = doc->monitorProperties()->targetPosition(t);
        out.insert(QStringLiteral("target %1").arg(t->name()),
                   QStringLiteral("%1,%2,%3").arg(qRound(p.x() * 1000)).arg(qRound(p.y() * 1000))
                       .arg(qRound(p.z() * 1000)).toUtf8());
    }
    return out;
}

QStringList differences(const QMap<QString, QByteArray> &a, const QMap<QString, QByteArray> &b)
{
    QStringList out;
    for (auto it = a.constBegin(); it != a.constEnd(); ++it)
        if (!b.contains(it.key()))
            out << QStringLiteral("gone: %1").arg(it.key());
        else if (b.value(it.key()) != it.value())
            out << QStringLiteral("changed: %1").arg(it.key());
    for (auto it = b.constBegin(); it != b.constEnd(); ++it)
        if (!a.contains(it.key()))
            out << QStringLiteral("new: %1").arg(it.key());
    return out;
}

QStringList listItems(const QListWidget *list)
{
    QStringList out;
    for (int i = 0; i < list->count(); ++i)
        out << list->item(i)->text();
    return out;
}
} // namespace

void PmsRehearsal_Test::initTestCase()
{
    qputenv("QLC_NO_VOICE", "1");
}

void PmsRehearsal_Test::rehearseTheVisit_data()
{
    QTest::addColumn<QString>("tour");
    QTest::addColumn<QString>("room");
    QTest::newRow("made-up tour and PMS-like room") << QString() << QString();
    const QString tour = qEnvironmentVariable("QLC_PMS_TOUR_SHOW");
    const QString room = qEnvironmentVariable("QLC_PMS_ROOM_SHOW");
    if (!tour.isEmpty() && !room.isEmpty())
        QTest::newRow("copies of the real files") << tour << room;
}

void PmsRehearsal_Test::rehearseTheVisit()
{
    QFETCH(QString, tour);
    QFETCH(QString, room);
    const bool real = !tour.isEmpty();
    QStringList report;
    QStringList rough;      // what works but would be rough on site

    // Copies only, in a folder of our own: the real files are only read.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString showPath = dir.filePath(real ? QFileInfo(tour).fileName() : QStringLiteral("Tour.qlcc"));
    const QString roomPath = dir.filePath(QStringLiteral("PMS Stage.qlcc"));
    if (real)
    {
        QVERIFY(QFile::copy(tour, showPath));
        const QString sidecar = VenueCorrections::sidecarPath(tour);
        if (QFileInfo::exists(sidecar))
            QVERIFY(QFile::copy(sidecar, VenueCorrections::sidecarPath(showPath)));
        QVERIFY(QFile::copy(room, roomPath));
    }
    else
    {
        QVERIFY(writeFile(showPath, tourXml()));
        QVERIFY(writeFile(VenueCorrections::sidecarPath(showPath), tourVenuesXml(QFileInfo(showPath).fileName())));
        QVERIFY(writeFile(roomPath, PmsLikeShow::xml(QStringLiteral("172.18.2.10"), true, true)));
    }
    const QByteArray roomBytes = readFile(roomPath);

    /* 1. The touring show, opened on the venue it was left on (the office). */
    QString why;
    /* Deleted on any early return too: a Doc left for the test object to
       delete outlives the application. */
    QScopedPointer<Doc> held(openShow(nullptr, showPath, &why));
    Doc *doc = held.data();
    QVERIFY2(why.isEmpty(), qPrintable(why));
    if (!real)
    {
        // The made-up show's production and one stage target.
        doc->showTree()->addProduction(QStringLiteral("Tour"));
        StageTarget *t = doc->monitorProperties()->addStageTarget();
        t->setName(QStringLiteral("CenterStage"));
        t->setPosition(QVector3D(7.62f, 5.486f, 1.4f));
        QVERIFY(saveShow(doc, showPath));
        held.reset(openShow(nullptr, showPath, &why));
        doc = held.data();
        QVERIFY2(why.isEmpty(), qPrintable(why));
    }
    VenueCorrections *v = doc->programmer()->venueCorrections();
    const QString office = v->activeVenue();
    QVERIFY2(!office.isEmpty(), "the show opened on no venue");
    QVERIFY(!v->hasVenue(kRoom));
    // Nothing here can reach a network: the only plugin is the stub.
    IOPluginStub *stub = stubOf(doc);
    QVERIFY(stub != nullptr);
    stub->init();
    QCOMPARE(doc->ioPluginCache()->plugins().size(), 1);
    QCOMPARE(stub->name(), QStringLiteral("I/O Plugin Stub"));
    const QMap<QString, QByteArray> before = snapshot(doc);
    const int tourFixtures = doc->fixtures().size();
    report << QStringLiteral("show: %1 fixtures, %2 groups, %3 universes; opened on venue \"%4\"")
                  .arg(tourFixtures).arg(doc->fixtureGroups().size())
                  .arg(doc->inputOutputMap()->universesCount()).arg(office);

    VenueImport::Source src;
    QVERIFY2(VenueImport::read(roomPath, src, &why), qPrintable(why));
    const RoomPlot roomPlot = readRoomPlot(src.monitorXml);
    report << QStringLiteral("room file: %1 fixtures, %2 groups, its output: %3")
                  .arg(src.fixtures.size()).arg(src.groups.size()).arg(VenueRig::outputText(src.fixtureOutputs()));

    /* 2. New Venue from a Show, as from the venue chip's menu. */
    QWidget window;
    window.resize(1440, 900);
    QPointer<QInputDialog> ask = VenueMenu::askNewVenue(doc, &window);
    QVERIFY(ask != nullptr);
    QPushButton *fromShow = ask->findChild<QPushButton *>(QStringLiteral("venueNewFromShow"));
    QVERIFY(fromShow != nullptr && fromShow->isEnabled());
    ask->setTextValue(kRoom);
    fromShow->click();
    QTRY_VERIFY(ask.isNull());
    QPointer<VenueFromShowDialog> d = window.findChild<VenueFromShowDialog *>();
    QVERIFY(d != nullptr);
    QCOMPARE(d->nameEdit()->text(), kRoom);
    QVERIFY(d->setFile(roomPath));

    // The universe the show keeps for the room: one with none of the show's lights.
    const quint32 roomU = d->universeBox()->currentData().toUInt();
    QVERIFY2(roomU != UINT_MAX, qPrintable(d->summaryLabel()->text()));
    QVERIFY(!d->universeConfirm()->isVisible());
    for (const Fixture *fx : doc->fixtures())
        QVERIFY2(fx->universe() != roomU, qPrintable(fx->name()));
    report << QStringLiteral("universe suggested for the room: %1").arg(d->universeBox()->currentText());

    // No USB DMX interface plugged in at home: Not yet, and the clash check says so.
    QCOMPARE(d->outputPicker()->kind(), RoomOutputPicker::NotYet);
    const QString noOutput = QStringLiteral("U%1: no output in this room yet").arg(roomU + 1);
    QVERIFY2(listItems(d->clashList()).filter(noOutput).size() == 1, qPrintable(listItems(d->clashList()).join('\n')));
    // What the file sent (the office's Art-Net) is offered, flagged, never chosen by itself.
    QComboBox *kind = d->outputPicker()->findChild<QComboBox *>(QStringLiteral("roomOutputKind"));
    QVERIFY(kind != nullptr);
    const int asFile = kind->findData(int(RoomOutputPicker::AsImported));
    QVERIFY(asFile >= 0);
    QVERIFY2(kind->itemText(asFile).contains(QStringLiteral("172.18.2.219")), qPrintable(kind->itemText(asFile)));
    kind->setCurrentIndex(asFile);
    QVERIFY2(listItems(d->clashList()).filter(QStringLiteral("still sends what PMS Stage.qlcc had")).size() == 1,
             qPrintable(listItems(d->clashList()).join('\n')));
    report << QStringLiteral("as the file had it: %1 (flagged)").arg(kind->itemText(asFile));
    kind->setCurrentIndex(kind->findData(int(RoomOutputPicker::NotYet)));
    QCOMPARE(d->outputPicker()->kind(), RoomOutputPicker::NotYet);

    // Q278 (a): the room's groups are held in the venue, matched later.
    GroupMatchTable *table = d->groupTable();
    QCOMPARE(table->rowCount(), int(src.groups.size()));
    QStringList suggested;
    for (int row = 0; row < table->rowCount(); ++row)
    {
        QComboBox *box = table->groupBox(row);
        QVERIFY(box != nullptr);
        if (box->currentData().toUInt() != UINT_MAX)
            suggested << QStringLiteral("%1 -> %2").arg(table->item(row, GroupMatchTable::NameCol)->text(),
                                                        box->currentText());
        box->setCurrentIndex(box->findData(uint(UINT_MAX)));
    }
    report << QStringLiteral("group names matched by the dialog: %1 (all held, Q278 a)")
                  .arg(suggested.isEmpty() ? QStringLiteral("none") : suggested.join(QStringLiteral("; ")));
    report << QStringLiteral("the dialog says: %1").arg(d->summaryLabel()->text().replace('\n', ' '));
    if (!d->makeButton()->isEnabled())
        QFAIL(qPrintable(d->makeButton()->text()));
    const QStringList dryClashes = listItems(d->clashList());
    report << QStringLiteral("clash check before it is made: %1").arg(dryClashes.join(QStringLiteral(" | ")));

    d->makeButton()->click();
    QTRY_VERIFY(d.isNull());
    QCOMPARE(v->activeVenue(), kRoom);
    QVERIFY2(!doc->isModified(), "making the venue changed the show");
    QPointer<QMessageBox> note = window.findChild<QMessageBox *>(QStringLiteral("vfsDone"));
    QVERIFY(note != nullptr);
    report << QStringLiteral("closing note: %1").arg(note->text().replace('\n', ' '));

    /* 3a. The room's fixtures: House, at their plot places (lined up), on its universe. */
    const VenueCorrections::Venue pms = v->venue(kRoom);
    QVERIFY(pms.rig.datum.isSet());
    const QPointF offset = VenueRig::offsetFor(pms.rig.datum, doc->monitorProperties());
    QCOMPARE(v->rig()->offset(), offset);
    QMap<QString, quint32> byName;
    for (quint32 id : v->rig()->venueFixtures())
        byName.insert(doc->fixture(id)->name(), id);
    QCOMPARE(byName.size(), int(src.fixtures.size()));
    QCOMPARE(doc->fixtures().size(), tourFixtures + int(src.fixtures.size()));
    QStringList placeErrors, offPlot;
    int placed = 0;
    const QVector3D grid = doc->monitorProperties()->gridSize()
                           * (doc->monitorProperties()->gridUnits() == MonitorProperties::Feet ? 304.8f : 1000.0f);
    for (const VenueRig::HouseFixture &hf : src.fixtures)
    {
        const quint32 id = byName.value(hf.name, Fixture::invalidId());
        const Fixture *fx = doc->fixture(id);
        QVERIFY2(fx != nullptr, qPrintable(hf.name));
        QCOMPARE(RigOwner::effective(doc, fx->id()), RigOwner::House);
        QCOMPARE(fx->universe(), roomU);
        QCOMPARE(fx->address(), hf.address);
        QCOMPARE(fx->isPatched(), hf.patched);
        const QPointF was = roomPlot.items.value(hf.id);
        if (was.isNull())
            continue;           // hung on the room's truss with no place of its own (the made-up file)
        const QVector3D at = doc->monitorProperties()->fixturePosition(id, 0, 0);
        const QPointF want = was + offset;
        ++placed;
        if (std::abs(at.x() - want.x()) > 2.0 || std::abs(at.y() - want.y()) > 2.0)
            placeErrors << QStringLiteral("%1 at %2,%3, not %4,%5").arg(hf.name).arg(at.x()).arg(at.y())
                               .arg(want.x()).arg(want.y());
        if (at.x() < 0 || at.y() < 0 || at.x() > grid.x() || at.y() > grid.z())
            offPlot << hf.name;
    }
    QVERIFY2(placeErrors.isEmpty(), qPrintable(placeErrors.join('\n')));
    QVERIFY(placed > 0);
    report << QStringLiteral("house fixtures: %1, all House on U%2, %3 checked at their plot places")
                  .arg(byName.size()).arg(roomU + 1).arg(placed);
    if (!offPlot.isEmpty())
        rough << QStringLiteral("off the show's plot once lined up (%1): %2").arg(offPlot.size())
                     .arg(offPlot.join(QStringLiteral(", ")));

    /* 3b. Every room group held, none of the show's groups changed. */
    QCOMPARE(pms.rig.room.groups.size(), src.groups.size());
    QVERIFY(pms.rig.fills.isEmpty());

    /* 3c. The clash check and the output: Not yet, amber; nothing to a network. */
    QCOMPARE(VenueRig::outputText(doc->inputOutputMap()->universe(roomU)->outputPatchDescriptions()),
             QStringLiteral("none"));
    const QList<VenueRig::Clash> clashes = v->clashes(kRoom);
    QStringList clashText;
    bool outputClash = false;
    for (const VenueRig::Clash &c : clashes)
    {
        clashText << QStringLiteral("%1: %2").arg(VenueRig::kindName(c.kind), c.text);
        outputClash = outputClash || (c.kind == VenueRig::Clash::Output && c.universe == roomU);
    }
    QVERIFY2(outputClash, qPrintable(clashText.join('\n')));
    report << QStringLiteral("clashes applied (%1): %2").arg(clashes.size()).arg(clashText.join(QStringLiteral(" | ")));
    VenueChip chip(doc, &window);
    chip.refresh();
    QVERIFY(chip.isAmber());
    QVERIFY2(chip.text().startsWith(QStringLiteral("Venue: %1").arg(kRoom)), qPrintable(chip.text()));
    report << QStringLiteral("chip: \"%1\" (amber)").arg(chip.text());
    QCOMPARE(doc->ioPluginCache()->plugins().size(), 1);

    /* 3d. Fill Roles from the note: suggested by words, model and place. */
    QPushButton *fillRoles = note->findChild<QPushButton *>(QStringLiteral("vfsFillRoles"));
    QVERIFY(fillRoles != nullptr);
    fillRoles->click();
    QTRY_VERIFY(note.isNull());
    QPointer<RoleFillDialog> roles = window.findChild<RoleFillDialog *>();
    QVERIFY(roles != nullptr);
    QMap<QString, QStringList> srcGroupsOf;     // fixture name -> its room groups
    QMap<quint32, QString> srcName;
    for (const VenueRig::HouseFixture &hf : src.fixtures)
        srcName.insert(hf.id, hf.name);
    for (const VenueImport::Group &g : src.groups)
        for (const VenueRig::FillHead &h : g.heads)
            srcGroupsOf[srcName.value(h.fixture)] << VenueImport::simplified(g.name);
    QStringList wrong;
    QMap<HouseRole::Role, QList<quint32>> want;
    for (const HouseRole::Suggestion &s : HouseRole::suggest(doc))
    {
        const QString name = doc->fixture(s.fixture)->name();
        const QStringList groups = srcGroupsOf.value(name);
        HouseRole::Role expect = HouseRole::None;
        if (groups.contains(QStringLiteral("house")))
            expect = HouseRole::HouseLights;
        else if (name.contains(QStringLiteral("Work")))
            expect = HouseRole::Work;
        else if (name.contains(QStringLiteral("Relay")) || name.contains(QStringLiteral("Hall")))
            expect = HouseRole::None;
        else if (groups.contains(QStringLiteral("foh")))
            expect = HouseRole::Front;
        else if (groups.contains(QStringLiteral("3rdelect")))
            expect = HouseRole::Cyc;
        else if (groups.contains(QStringLiteral("1stelect")) || groups.contains(QStringLiteral("2ndelect")))
            expect = HouseRole::Top;
        if (s.role != expect)
            wrong << QStringLiteral("%1: %2 (%3), expected %4").arg(name, HouseRole::groupName(s.role), s.why,
                                                                    HouseRole::groupName(expect));
        if (s.role != HouseRole::None)
            want[s.role] << s.fixture;
    }
    QVERIFY2(wrong.isEmpty(), qPrintable(wrong.join('\n')));
    QStringList roleRows;
    for (int row = 0; row < roles->table()->rowCount(); ++row)
        roleRows << QStringLiteral("%1 = %2 [%3]").arg(roles->table()->item(row, RoleFillDialog::RoleCol)->text(),
                                                      roles->table()->item(row, RoleFillDialog::LightsCol)->text(),
                                                      roles->table()->item(row, RoleFillDialog::WhyCol)->text());
    report << QStringLiteral("Fill Roles rows:") << roleRows;
    report << roles->notUsedLabel()->text();
    for (HouseRole::Role r : { HouseRole::HouseLights, HouseRole::Front, HouseRole::Top, HouseRole::Cyc,
                               HouseRole::Work })
        QVERIFY2(!want.value(r).isEmpty(), qPrintable(HouseRole::groupName(r)));
    roles->setUseExistingAnswer(0);
    QVERIFY(roles->fillButton()->isEnabled());
    roles->fillButton()->click();
    QVERIFY2(roles->statusLabel()->text().startsWith(QStringLiteral("Filled")), qPrintable(roles->statusLabel()->text()));
    report << roles->statusLabel()->text();
    roles->close();
    for (auto it = want.constBegin(); it != want.constEnd(); ++it)
    {
        const FixtureGroup *g = HouseRole::group(doc, it.key());
        QVERIFY(g != nullptr);
        QList<quint32> have = g->fixtureList();
        QList<quint32> exp = it.value();
        std::sort(have.begin(), have.end());
        std::sort(exp.begin(), exp.end());
        QCOMPARE(have, exp);
    }

    /* 3e. The six standard cues reach the room's lights through the roles. */
    const QList<quint32> house = want.value(HouseRole::HouseLights);
    const QList<quint32> stage = want.value(HouseRole::Front) + want.value(HouseRole::Top);
    const QList<quint32> cycs = want.value(HouseRole::Cyc);
    const QList<quint32> work = want.value(HouseRole::Work);
    QMap<quint32, QByteArray> tourOut;
    for (quint32 u = 0; u < doc->inputOutputMap()->universesCount(); ++u)
        if (u != roomU)
            tourOut.insert(u, universeOut(doc, u));
    for (const HouseRole::StandardCue &c : HouseRole::standardCues())
        QVERIFY2(standardHit(doc, c.key) != nullptr, qPrintable(c.name));
    auto fire = [doc](const QString &key) {
        standardHit(doc, key)->start(doc->masterTimer(), FunctionParent::master());
        tick(doc, 300);
    };
    fire(QStringLiteral("house-half"));
    const QPair<int, int> half = levels(doc, house);
    QVERIFY2(half.first >= 127 && half.second <= 255, qPrintable(QStringLiteral("%1-%2").arg(half.first).arg(half.second)));
    QCOMPARE(levels(doc, stage).second, 0);
    report << QStringLiteral("House to half: house lights at %1-%2 of 255").arg(half.first).arg(half.second);
    if (half.second < 255)
        rough << QStringLiteral("House to half sends %1 to the house circuits: no Switch rule on them, so a relay "
                                "there sees 50%% and its own threshold decides").arg(half.second);
    stopAll(doc);
    fire(QStringLiteral("house-full"));
    QCOMPARE(levels(doc, house).first, 255);
    fire(QStringLiteral("house-out"));
    QCOMPARE(levels(doc, house).second, 0);
    stopAll(doc);
    fire(QStringLiteral("stage-up"));
    QCOMPARE(levels(doc, stage), qMakePair(255, 255));
    QCOMPARE(levels(doc, house).second, 0);
    QCOMPARE(levels(doc, cycs).second, 0);
    fire(QStringLiteral("stage-out"));
    QCOMPARE(levels(doc, stage).second, 0);
    stopAll(doc);
    fire(QStringLiteral("work"));
    QCOMPARE(levels(doc, work), qMakePair(255, 255));
    QCOMPARE(levels(doc, stage).second, 0);
    // The tour's universes never moved.
    for (auto it = tourOut.constBegin(); it != tourOut.constEnd(); ++it)
        QVERIFY2(universeOut(doc, it.key()) == it.value(), qPrintable(QStringLiteral("U%1").arg(it.key() + 1)));
    stopAll(doc);
    report << QStringLiteral("standard cues: all six fire and reach the room's lights only");

    /* 4. The room lined up: its centre line on the show's, its drape on the show's upstage line. */
    const MonitorProperties *props = doc->monitorProperties();
    const VenueRig::Datum datum = v->venue(kRoom).rig.datum;
    QCOMPARE(qRound64(offset.x()), qRound64(double(props->stageCentre().x()) * 1000.0) - datum.centreX);
    QCOMPARE(qRound64(offset.y()), qRound64(double(props->upstageLineY()) * 1000.0) - datum.drape);
    QCOMPARE(datum.piece, QStringLiteral("Cyclorama 2"));
    report << VenueRig::datumText(datum, props);
    report << QStringLiteral("show: centre line %1 across, upstage line %2 (%3); room: centre line %4, drape %5 (%6)")
                  .arg(len(props->stageCentre().x() * 1000.0), len(props->upstageLineY() * 1000.0),
                       props->upstageLineSource(), len(double(datum.centreX)), len(double(datum.drape)), datum.piece);
    // The room's pieces in the show's frame.
    auto inShow = [&](const QString &name) {
        const QRectF r = roomPlot.scenery.value(name);
        return QRectF(r.x() * 1000.0 + offset.x(), r.y() * 1000.0 + offset.y(), r.width() * 1000.0, 0.0);
    };
    QString prosc, mainDrape;
    for (auto it = roomPlot.kinds.constBegin(); it != roomPlot.kinds.constEnd(); ++it)
        if (it.value() == Scenery::Proscenium)
            prosc = it.key();
        else if (it.value() == Scenery::MainDrape)
            mainDrape = it.key();
    QStringList targetNotes;
    const QString curtainPiece = !mainDrape.isEmpty() ? mainDrape : prosc;
    if (!curtainPiece.isEmpty())
    {
        const QRectF curtain = inShow(curtainPiece);
        const QRectF opening = inShow(prosc.isEmpty() ? curtainPiece : prosc);
        report << QStringLiteral("room's %1 lands %2 downstage of the show's upstage line; its opening %3 wide, "
                                 "centred %4 off the show's centre line")
                      .arg(curtainPiece, len(curtain.y() - props->upstageLineY() * 1000.0), len(opening.width()),
                           len(opening.center().x() - props->stageCentre().x() * 1000.0));
        // Every stage target of the show inside the room's opening and upstage of its curtain.
        for (const StageTarget *t : props->stageTargets())
        {
            const QVector3D p = props->targetPosition(t) * 1000.0f;
            QStringList bad;
            if (p.x() < opening.left() || p.x() > opening.right())
                bad << QStringLiteral("outside the room's opening");
            if (p.y() > curtain.y())
                bad << QStringLiteral("%1 downstage of the room's %2").arg(len(p.y() - curtain.y()), curtainPiece);
            if (p.y() < props->upstageLineY() * 1000.0)
                bad << QStringLiteral("upstage of the drape");
            if (!bad.isEmpty())
                targetNotes << QStringLiteral("%1: %2").arg(t->name(), bad.join(QStringLiteral(", ")));
        }
        // The show's deck against the room's curtain.
        double deckFront = -1.0;
        for (const StagePlatform *p : props->platforms())
            deckFront = qMax(deckFront, double(p->originY() + p->depth()) * 1000.0);
        if (deckFront > curtain.y())
            rough << QStringLiteral("the show's platforms reach %1 downstage of the room's %2")
                         .arg(len(deckFront - curtain.y()), curtainPiece);
    }
    report << QStringLiteral("stage targets: %1, %2 outside the room").arg(props->stageTargets().size())
                  .arg(targetNotes.size());
    report << targetNotes;
    if (!real)
        QVERIFY2(targetNotes.isEmpty(), qPrintable(targetNotes.join('\n')));
    else
        rough << targetNotes;

    /* 5. Saved and opened again: the venue, its rig, roles and cues survive. */
    QMap<QString, QPointF> placedAt;
    for (auto it = byName.constBegin(); it != byName.constEnd(); ++it)
    {
        const QVector3D at = doc->monitorProperties()->fixturePosition(it.value(), 0, 0);
        placedAt.insert(it.key(), QPointF(at.x(), at.y()));
    }
    QMap<HouseRole::Role, QStringList> roleNames;
    for (auto it = want.constBegin(); it != want.constEnd(); ++it)
        for (quint32 id : it.value())
            roleNames[it.key()] << doc->fixture(id)->name();
    const int clashCount = clashes.size();
    QVERIFY(saveShow(doc, showPath));
    const QByteArray savedShow = readFile(showPath);
    for (const VenueRig::HouseFixture &hf : src.fixtures)
        QVERIFY2(!savedShow.contains(QStringLiteral("<Name>%1</Name>").arg(hf.name).toUtf8()),
                 qPrintable(QStringLiteral("%1 went into the show file").arg(hf.name)));
    held.reset(openShow(nullptr, showPath, &why));
    doc = held.data();
    QVERIFY2(why.isEmpty(), qPrintable(why));
    v = doc->programmer()->venueCorrections();
    stub = stubOf(doc);
    stub->init();
    QCOMPARE(v->activeVenue(), kRoom);
    QCOMPARE(int(v->rig()->venueFixtures().size()), int(src.fixtures.size()));
    QCOMPARE(v->venue(kRoom).rig.datum, datum);
    QCOMPARE(v->rig()->offset(), offset);
    QCOMPARE(v->venue(kRoom).rig.room.groups.size(), src.groups.size());
    QStringList moved;
    for (quint32 id : v->rig()->venueFixtures())
    {
        const Fixture *fx = doc->fixture(id);
        const QVector3D at = doc->monitorProperties()->fixturePosition(id, 0, 0);
        const QPointF was = placedAt.value(fx->name());
        if (std::abs(at.x() - was.x()) > 2.0 || std::abs(at.y() - was.y()) > 2.0)
            moved << fx->name();
        QCOMPARE(RigOwner::effective(doc, fx->id()), RigOwner::House);
    }
    QVERIFY2(moved.isEmpty(), qPrintable(moved.join(QStringLiteral(", "))));
    for (auto it = roleNames.constBegin(); it != roleNames.constEnd(); ++it)
    {
        const FixtureGroup *g = HouseRole::group(doc, it.key());
        QVERIFY(g != nullptr);
        QStringList have;
        for (quint32 id : g->fixtureList())
            have << doc->fixture(id)->name();
        have.sort();
        QStringList exp = it.value();
        exp.sort();
        QCOMPARE(have, exp);
    }
    QCOMPARE(VenueRig::outputText(doc->inputOutputMap()->universe(roomU)->outputPatchDescriptions()),
             QStringLiteral("none"));
    QCOMPARE(v->clashes(kRoom).size(), clashCount);
    for (const HouseRole::StandardCue &c : HouseRole::standardCues())
        QVERIFY2(standardHit(doc, c.key) != nullptr, qPrintable(c.name));
    QStringList stageNames = roleNames.value(HouseRole::Front) + roleNames.value(HouseRole::Top);
    QList<quint32> stageIds;
    for (quint32 id : v->rig()->venueFixtures())
        if (stageNames.contains(doc->fixture(id)->name()))
            stageIds << id;
    QCOMPARE(stageIds.size(), stageNames.size());
    report << QStringLiteral("saved and opened again: venue, %1 house fixtures, places, roles, datum, cues kept")
                  .arg(v->rig()->venueFixtures().size());

    /* On site: the USB DMX interface plugged in and chosen on the venue page. */
    stub->m_nameOverride = QStringLiteral("DMX USB");
    {
        VenueRigDialog page(doc, kRoom, &window);
        RoomOutputPicker *pick = page.roomOutput();
        QVERIFY(pick != nullptr);
        pick->refreshInterfaces();
        QVERIFY(!pick->usbInterfaces().isEmpty());
        QComboBox *k = pick->findChild<QComboBox *>(QStringLiteral("roomOutputKind"));
        k->setCurrentIndex(k->findData(int(RoomOutputPicker::Usb)));
        QVERIFY(pick->isComplete());
        QVERIFY(page.useRoomOutputButton()->isEnabled());
        page.useRoomOutputButton()->click();
        QVERIFY2(listItems(page.clashList()).filter(QStringLiteral("Output")).isEmpty(),
                 qPrintable(listItems(page.clashList()).join('\n')));
    }
    QVERIFY2(VenueRig::outputText(doc->inputOutputMap()->universe(roomU)->outputPatchDescriptions())
                 .startsWith(QStringLiteral("DMX USB")),
             qPrintable(VenueRig::outputText(doc->inputOutputMap()->universe(roomU)->outputPatchDescriptions())));
    VenueChip chip2(doc, &window);
    chip2.refresh();
    report << QStringLiteral("USB chosen on site: chip \"%1\"%2").arg(chip2.text(),
                  chip2.isAmber() ? QStringLiteral(" (still amber)") : QString());
    if (chip2.isAmber())
        rough << QStringLiteral("with the USB interface chosen the chip is still amber: %1").arg(chip2.toolTip());
    // Stage up goes out the interface: the frame reaches the stub, nothing else.
    standardHit(doc, QStringLiteral("stage-up"))->start(doc->masterTimer(), FunctionParent::master());
    tick(doc, 300);
    QCOMPARE(levels(doc, stageIds), qMakePair(255, 255));
    const Fixture *first = doc->fixture(stageIds.first());
    const int line = doc->inputOutputMap()->universe(roomU)->outputPatch(0)->output();
    QVERIFY(stub->m_universe.size() > int(line * 512 + first->address()));
    QCOMPARE(uchar(stub->m_universe.at(int(line * 512 + first->address()))), uchar(255));
    stopAll(doc);
    QCOMPARE(doc->ioPluginCache()->plugins().size(), 1);

    /* 6. Back to the office: the show exactly as it was, the room's rig gone. */
    QVERIFY(VenueMenu::switchTo(doc, &window, office));
    QCOMPARE(v->activeVenue(), office);
    QVERIFY(v->rig()->venueFixtures().isEmpty());
    QCOMPARE(doc->fixtures().size(), tourFixtures);
    const QStringList diff = differences(before, snapshot(doc));
    QVERIFY2(diff.isEmpty(), qPrintable(diff.join('\n')));
    for (HouseRole::Role r : HouseRole::roles())
        if (const FixtureGroup *g = HouseRole::group(doc, r))
            QVERIFY2(g->fixtureList().isEmpty(), qPrintable(g->name()));
    report << QStringLiteral("back to %1: the show as it was; roles empty there").arg(office);
    QCOMPARE(readFile(roomPath), roomBytes);         // the room's file was only read

    const QString text = report.join('\n') + QStringLiteral("\nrough edges:\n") + rough.join('\n');
    qInfo().noquote() << text;
    const QString shots = qEnvironmentVariable("QLC_SHOT_DIR");
    if (!shots.isEmpty())
    {
        QDir().mkpath(shots);
        QVERIFY(writeFile(QStringLiteral("%1/pms-rehearsal-%2.txt").arg(shots, real ? "real" : "made-up"),
                          text.toUtf8() + '\n'));
    }
}

QTEST_MAIN(PmsRehearsal_Test)
