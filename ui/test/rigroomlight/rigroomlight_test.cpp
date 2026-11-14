/*
  Q Light Controller Plus
  rigroomlight_test.cpp

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
#include <QTextStream>
#include <QPainter>
#include <QImage>
#include <QFile>
#include <QDir>
#include <QMap>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define protected public
#define private public
#include "app.h"
#include "structurestudioview.h"
#undef private
#undef protected
#include "fixturevisualtraits.h"
#include "monitorproperties.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlcfixturehead.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "stageplatform.h"
#include "scenery.h"
#include "fixture.h"
#include "truss.h"
#include "tower.h"
#include "doc.h"

#include "rigroomlight_test.h"

namespace
{

void settle(int ms = 20)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
}

/* Red enough to read as a red line: red well above both green and blue. */
bool redDominant(const QColor &c)
{
    return c.isValid() && c.alpha() > 0 && c.red() >= 90
           && c.red() > c.green() + 50 && c.red() > c.blue() + 50;
}

QString colourText(const QColor &c)
{
    return QStringLiteral("(%1,%2,%3,a%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
}

double luma(const QColor &c)
{
    return c.red() * 0.30 + c.green() * 0.59 + c.blue() * 0.11;
}

/* A strip of LED tape: @p heads RGB pixels along @p widthMm, as the show's
   "Step Row 64 Heads" is (2134 x 25 x 2 mm). */
QLCFixtureDef *stripDef(int heads, int widthMm, int heightMm, int depthMm)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel(QStringLiteral("Strip %1").arg(widthMm));
    def->setType(QLCFixtureDef::LEDBarPixels);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("RGB");
    const QLCChannel::PrimaryColour pc[] = { QLCChannel::Red, QLCChannel::Green, QLCChannel::Blue };
    for (int h = 0; h < heads; ++h)
    {
        QLCFixtureHead head;
        for (int c = 0; c < 3; ++c)
        {
            QLCChannel *ch = new QLCChannel();
            ch->setName(QStringLiteral("H%1 %2").arg(h).arg(c));
            ch->setGroup(QLCChannel::Intensity);
            ch->setColour(pc[c]);
            def->addChannel(ch);
            mode->insertChannel(ch, quint32(h * 3 + c));
            head.addChannel(quint32(h * 3 + c));
        }
        mode->insertHead(-1, head);
    }
    QLCPhysical ph;
    ph.setWidth(widthMm); ph.setHeight(heightMm); ph.setDepth(depthMm);
    mode->setPhysical(ph);
    def->addMode(mode);
    return def;
}

Fixture *addStrip(Doc *doc, QLCFixtureDef *def, const QString &name, quint32 address)
{
    Fixture *fx = new Fixture(doc);
    fx->setName(name);
    fx->setFixtureDefinition(def, def->modes().first());
    fx->setUniverse(0);
    fx->setAddress(address);
    if (!doc->addFixture(fx))
    {
        delete fx;
        return nullptr;
    }
    doc->monitorProperties()->setFixturePosition(fx->id(), 0, 0, QVector3D(0, 0, 0));
    return fx;
}

/* Every edge (pen) one object draws in the 45° view at the view's room level. */
QList<QColor> structureEdges(StructureStudioView &v, StructureStudioView::Kind kind, quint32 id)
{
    QImage canvas(64, 64, QImage::Format_ARGB32);
    QPainter p(&canvas);
    v.m_ops.clear();
    v.m_collecting = true;
    v.drawOneStructure(p, kind, id);
    v.m_collecting = false;
    QList<QColor> out;
    for (const auto &o : v.m_ops)
        if (o.pen.isValid() && o.pen.alpha() > 0 && o.penWidth > 0.0 && !o.light)
            out << o.pen;
    v.m_ops.clear();
    return out;
}

QList<QColor> fixtureEdges(StructureStudioView &v, quint32 fid)
{
    QImage canvas(64, 64, QImage::Format_ARGB32);
    QPainter p(&canvas);
    v.m_ops.clear();
    v.m_collecting = true;
    v.drawOneFixture(p, fid, false);
    v.m_collecting = false;
    QList<QColor> out;
    for (const auto &o : v.m_ops)
        if (o.kind == StructureStudioView::DrawOp::Poly && o.pen.isValid() && o.pen.alpha() > 0
            && o.penWidth > 0.0 && !o.light)
            out << o.pen;
    v.m_ops.clear();
    return out;
}

}

void RigRoomLight_Test::cleanupTestCase()
{
    if (m_app != nullptr)
    {
        m_app->doc()->resetModified();
        delete m_app;
        m_app = nullptr;
    }
}

void RigRoomLight_Test::blackoutEdgesAreFaintAndNeutral()
{
    QScopedPointer<Doc> docOwner(new Doc(this));
    Doc *doc = docOwner.data();
    MonitorProperties *props = doc->monitorProperties();

    // A riser in stage black (no finish set), and one painted the plot's red.
    StagePlatform *black = props->addPlatform();
    black->setName("USP1"); black->setOriginX(1.0f); black->setOriginY(1.0f);
    black->setWidth(2.438f); black->setDepth(1.219f); black->setHeight(0.61f);
    black->setColor(QColor(0xd2, 0x13, 0x2a));          // its plot colour
    StagePlatform *red = props->addPlatform();
    red->setName("Red deck"); red->setOriginX(4.0f); red->setOriginY(1.0f);
    red->setWidth(2.0f); red->setDepth(1.0f); red->setHeight(0.4f);
    red->setColor(QColor(0xd2, 0x13, 0x2a));
    red->setFinish(QColor(0xd2, 0x13, 0x2a));           // painted red for real

    // A strip of tape along the black riser's front.
    QLCFixtureDef *def = stripDef(8, 2134, 25, 2);
    Fixture *strip = addStrip(doc, def, "US1 #1", 0);
    QVERIFY(strip != nullptr);
    FixtureRigProps rp;
    rp.riserPlatformId = black->id();
    rp.riserFace = 0;
    rp.riserU = 1.219f;
    rp.riserV = 0.3f;
    props->setFixtureRigProps(strip->id(), rp);

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(800, 500);
    v.setPlane(StructureStudioView::Angled);
    v.setLiveValues(true);                              // nothing playing: all at zero
    v.reload();

    auto check = [&](double room, double *maxLuma, double *minLuma) {
        v.setAmbient(room);
        v.grab();                                       // a frame: its snapshot and warnings
        QList<QColor> edges;
        edges << structureEdges(v, StructureStudioView::PlatformKind, black->id())
              << structureEdges(v, StructureStudioView::PlatformKind, red->id())
              << fixtureEdges(v, strip->id());
        QVERIFY(!edges.isEmpty());
        *maxLuma = 0.0;
        *minLuma = 255.0;
        for (const QColor &c : edges)
        {
            *maxLuma = qMax(*maxLuma, luma(c));
            *minLuma = qMin(*minLuma, luma(c));
            if (room == 0.0)
            {
                QVERIFY2(!redDominant(c), qPrintable("a red edge in a blackout: " + colourText(c)));
                QVERIFY2(qAbs(c.red() - c.blue()) <= 12 && qAbs(c.red() - c.green()) <= 12,
                         qPrintable("a coloured edge in a blackout: " + colourText(c)));
            }
        }
    };
    double darkMax = 0.0, darkMin = 0.0, houseMax = 0.0, houseMin = 0.0;
    check(0.0, &darkMax, &darkMin);
    check(0.62, &houseMax, &houseMin);

    // Faint in a blackout, but above the canvas (luma about 26), so the shape stays.
    QVERIFY2(darkMax <= 70.0, qPrintable(QString("blackout edges as bright as luma %1").arg(darkMax)));
    QVERIFY2(darkMin >= 40.0, qPrintable(QString("a blackout edge lost on the canvas: luma %1").arg(darkMin)));
    // And the room's light brings them up.
    QVERIFY2(houseMax > darkMax + 25.0,
             qPrintable(QString("House edges (luma %1) no brighter than blackout (%2)")
                        .arg(houseMax).arg(darkMax)));

    doc->deleteFixture(strip->id());
}

void RigRoomLight_Test::stackedStripsAreNotAnOverlap()
{
    QScopedPointer<Doc> docOwner(new Doc(this));
    Doc *doc = docOwner.data();
    MonitorProperties *props = doc->monitorProperties();
    StagePlatform *box = props->addPlatform();
    box->setName("US-1"); box->setOriginX(1.0f); box->setOriginY(1.0f);
    box->setWidth(2.438f); box->setDepth(0.204f); box->setHeight(0.204f);

    /* Eight strips up the box's face, 25.5 mm apart, each 25 mm tall: the
       show's steps, each row touching the next. */
    QLCFixtureDef *def = stripDef(8, 2134, 25, 2);
    QList<quint32> rows;
    for (int i = 0; i < 8; ++i)
    {
        Fixture *fx = addStrip(doc, def, QStringLiteral("US1 #%1").arg(i + 1), quint32(i * 24));
        QVERIFY(fx != nullptr);
        FixtureRigProps rp;
        rp.riserPlatformId = box->id();
        rp.riserFace = 0;
        rp.riserU = 1.219f;
        rp.riserV = 0.191f - 0.0255f * i;
        props->setFixtureRigProps(fx->id(), rp);
        rows << fx->id();
    }

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(600, 400);
    v.setPlane(StructureStudioView::Angled);
    v.reload();
    v.m_overlapAge.invalidate();
    v.rebuildOverlapWarnings();
    QVERIFY2(v.m_overlappingFixtures.isEmpty(),
             qPrintable(QString("%1 stacked strips flagged as overlapping")
                        .arg(v.m_overlappingFixtures.size())));

    // A ninth rigged exactly on the first: that IS the same space.
    Fixture *dup = addStrip(doc, def, "US1 #1 again", 200);
    QVERIFY(dup != nullptr);
    props->setFixtureRigProps(dup->id(), props->fixtureRigProps(rows.first()));
    v.m_overlapAge.invalidate();
    v.rebuildOverlapWarnings();
    QVERIFY(v.m_overlappingFixtures.contains(dup->id()));
    QVERIFY(v.m_overlappingFixtures.contains(rows.first()));
    QVERIFY(!v.m_overlappingFixtures.contains(rows.last()));

    // And in a blackout the warning is still the warning: it is not the room's.
    v.setAmbient(0.0);
    v.grab();
    bool warned = false;
    for (const QColor &c : fixtureEdges(v, dup->id()))
        warned = warned || redDominant(c);
    QVERIFY2(warned, "a real overlap lost its warning edge");
}

void RigRoomLight_Test::uprightBarsSideBySideAreNotAnOverlap()
{
    QScopedPointer<Doc> docOwner(new Doc(this));
    Doc *doc = docOwner.data();
    MonitorProperties *props = doc->monitorProperties();
    Truss *t = props->addTruss();
    t->setName("FART-1");
    t->setType(Truss::Horizontal);
    t->setOrigin(QVector3D(3.453f, 3.622f, 1.524f));
    t->setDirection(QPointF(1.0, 0.0));
    t->setLength(2.438f);
    t->setWidth(0.152f);

    // The show's bar row: 1.5 m bars, 75 mm across, stood on end 152 mm apart.
    QLCFixtureDef *def = stripDef(4, 1500, 75, 228);
    QList<quint32> bars;
    for (int i = 0; i < 6; ++i)
    {
        Fixture *fx = addStrip(doc, def, QStringLiteral("FART-SR-%1").arg(i + 1), quint32(i * 12));
        QVERIFY(fx != nullptr);
        FixtureRigProps rp;
        rp.trussId = t->id();
        rp.trussOffset = 0.076f + 0.152f * i;
        rp.mountingType = Truss::TopHung;
        rp.trussMountSide = 2;
        rp.trussCross = 0.076f;
        rp.studioMount = 1;
        rp.studioFaceChosen = true;
        rp.studioAngle = 90.0f;
        props->setFixtureRigProps(fx->id(), rp);
        bars << fx->id();
    }

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(600, 400);
    v.setPlane(StructureStudioView::Angled);
    v.reload();

    // They do stand on end: the body is taller than it is wide along the truss.
    QVector3D k[8];
    v.fixtureBoxCorners(bars.first(), classifyFixture(doc->fixture(bars.first())), k);
    float zLo = k[0].z(), zHi = k[0].z(), xLo = k[0].x(), xHi = k[0].x();
    for (const QVector3D &q : k)
    {
        zLo = qMin(zLo, q.z()); zHi = qMax(zHi, q.z());
        xLo = qMin(xLo, q.x()); xHi = qMax(xHi, q.x());
    }
    QVERIFY2(zHi - zLo > 1.0f && xHi - xLo < 0.3f,
             qPrintable(QString("not upright: %1 m tall, %2 m along the truss")
                        .arg(zHi - zLo).arg(xHi - xLo)));

    v.m_overlapAge.invalidate();
    v.rebuildOverlapWarnings();
    QVERIFY2(v.m_overlappingFixtures.isEmpty(),
             qPrintable(QString("%1 upright bars flagged as overlapping")
                        .arg(v.m_overlappingFixtures.size())));
}

void RigRoomLight_Test::aSelectedFixtureKeepsItsEdge()
{
    QScopedPointer<Doc> docOwner(new Doc(this));
    Doc *doc = docOwner.data();
    MonitorProperties *props = doc->monitorProperties();
    StagePlatform *deck = props->addPlatform();
    deck->setName("Deck"); deck->setOriginX(1.0f); deck->setOriginY(1.0f);
    deck->setWidth(2.438f); deck->setDepth(1.219f); deck->setHeight(0.61f);
    QLCFixtureDef *def = stripDef(8, 2134, 25, 2);
    Fixture *strip = addStrip(doc, def, "Picked", 0);
    QVERIFY(strip != nullptr);
    FixtureRigProps rp;
    rp.riserPlatformId = deck->id();
    rp.riserFace = 0;
    rp.riserU = 1.219f;
    rp.riserV = 0.3f;
    props->setFixtureRigProps(strip->id(), rp);

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.resize(600, 400);
    v.setPlane(StructureStudioView::Angled);
    v.setLiveValues(true);
    v.setAmbient(0.0);
    v.reload();
    v.setHighlight({ strip->id() });
    v.grab();
    double brightest = 0.0;
    for (const QColor &c : fixtureEdges(v, strip->id()))
        brightest = qMax(brightest, luma(c));
    QVERIFY2(brightest > 150.0,
             qPrintable(QString("a selected fixture's edge sank with the room: luma %1").arg(brightest)));
}

void RigRoomLight_Test::userShowShots()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    const QString show = QStringLiteral(RIGROOM_SRC_DIR "/repro/show.qlcc");
    if (dir.isEmpty() || !QFile::exists(show))
        QSKIP("QLC_SHOT_DIR not set, or no repro/show.qlcc");
    const QString state = qEnvironmentVariable("QLC_SHOT_STATE", QStringLiteral("after"));
    if (m_app == nullptr)
    {
        m_app = new App();
        m_app->init();
    }

    QLCFixtureDefCache *cache = m_app->doc()->fixtureDefCache();
    cache->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures"));
    m_app->clearDocument();
    QCOMPARE(m_app->loadXML(show), QFile::NoError);
    m_app->doc()->resetModified();
    settle(200);

    Doc *doc = m_app->doc();
    MonitorProperties *props = doc->monitorProperties();
    /* The drape as the user looks at it: open. Its curtain fixture is at
       zero with nothing playing, which closes it under Live; the file's own
       state (Open) is what we want here. */
    foreach (Scenery *sc, props->sceneries())
        if (sc != nullptr && sc->hasState())
        {
            sc->setDriverFixture(Scenery::invalidId());
            sc->setState(Scenery::Open);
        }

    StructureStudioView view(doc, StructureStudioView::StageKind, 0);
    view.setPlane(StructureStudioView::Angled);
    view.setStandOnCentreStage(true);
    view.setLocked(true);
    view.setLiveValues(true);
    view.setBeams(true);
    view.reload();

    QFile diag(QDir(dir).filePath(QStringLiteral("%1-rig45-diagnostics.txt").arg(state)));
    QVERIFY(diag.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&diag);

    const struct { const char *name; double level; } rooms[] = {
        { "blackout", 0.0 }, { "house", 0.62 } };
    const QSize sizes[] = { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) };
    for (const auto &room : rooms)
    {
        view.setAmbient(room.level);
        for (const QSize &sz : sizes)
        {
            view.resize(sz);
            view.reload();
            settle(50);
            const QImage img = view.grab().toImage();
            QVERIFY(img.save(QDir(dir).filePath(QStringLiteral("%1-rig45-%2-%3x%4.png")
                        .arg(state, QLatin1String(room.name)).arg(sz.width()).arg(sz.height()))));
        }

        /* Every object's own primitives, at this room level: which draw a
           red-dominant edge (pen) or face (fill), and in what colour. */
        out << "== room " << room.name << " (ambient " << room.level << ")\n";
        QImage canvas(64, 64, QImage::Format_ARGB32);
        QPainter p(&canvas);
        typedef QPair<StructureStudioView::Kind, quint32> KindId;
        auto report = [&](const QString &what) {
            QMap<QString, int> pens, fills;
            for (const auto &o : view.m_ops)
            {
                if (redDominant(o.pen) && o.penWidth > 0.0)
                    pens[colourText(o.pen)]++;
                if (redDominant(o.fill) && !o.light)
                    fills[colourText(o.fill)]++;
            }
            if (pens.isEmpty() && fills.isEmpty())
                return;
            out << what << ":";
            for (auto it = pens.cbegin(); it != pens.cend(); ++it)
                out << " pen" << it.key() << "x" << it.value();
            for (auto it = fills.cbegin(); it != fills.cend(); ++it)
                out << " fill" << it.key() << "x" << it.value();
            out << "\n";
        };
        foreach (const KindId &ki, view.everyStructure())
        {
            view.m_ops.clear();
            view.m_collecting = true;
            view.drawOneStructure(p, ki.first, ki.second);
            view.m_collecting = false;
            QString name;
            if (ki.first == StructureStudioView::PlatformKind && props->platform(ki.second))
                name = QStringLiteral("platform ") + props->platform(ki.second)->name();
            else if (ki.first == StructureStudioView::SceneryKind && props->scenery(ki.second))
                name = QStringLiteral("scenery ") + props->scenery(ki.second)->name();
            else if (ki.first == StructureStudioView::TrussKind && props->truss(ki.second))
                name = QStringLiteral("truss ") + props->truss(ki.second)->name();
            else if (ki.first == StructureStudioView::TowerKind && props->tower(ki.second))
                name = QStringLiteral("tower ") + props->tower(ki.second)->name();
            else
                name = QStringLiteral("kind %1 id %2").arg(int(ki.first)).arg(ki.second);
            report(name);
        }
        foreach (quint32 fid, view.mountedFixtures())
        {
            view.m_ops.clear();
            view.m_collecting = true;
            view.drawOneFixture(p, fid, false);
            view.m_collecting = false;
            Fixture *fx = doc->fixture(fid);
            report(QStringLiteral("fixture %1%2").arg(fx ? fx->name() : QString::number(fid),
                   view.m_overlappingFixtures.contains(fid) ? QStringLiteral(" [overlap]") : QString()));
        }
        view.m_ops.clear();
        view.m_collecting = true;
        view.drawSocks(p);
        view.drawTowerFaces(p);
        view.drawStageFigures(p);
        view.m_collecting = false;
        report(QStringLiteral("socks/tower faces/figures"));
        view.m_ops.clear();
    }
    out << "overlapping fixtures: " << view.m_overlappingFixtures.size() << "\n";
    // Which pairs, and their bodies' boxes (m), for telling a real one from a false one.
    auto boxOf = [&](quint32 fid, QVector3D &lo, QVector3D &hi) {
        QVector3D k[8];
        view.fixtureBoxCorners(fid, classifyFixture(doc->fixture(fid)), k);
        lo = hi = k[0];
        for (const QVector3D &q : k)
        {
            lo = QVector3D(qMin(lo.x(), q.x()), qMin(lo.y(), q.y()), qMin(lo.z(), q.z()));
            hi = QVector3D(qMax(hi.x(), q.x()), qMax(hi.y(), q.y()), qMax(hi.z(), q.z()));
        }
    };
    auto boxText = [](const QVector3D &lo, const QVector3D &hi) {
        return QStringLiteral("[%1..%2, %3..%4, %5..%6]").arg(lo.x(), 0, 'f', 3).arg(hi.x(), 0, 'f', 3)
            .arg(lo.y(), 0, 'f', 3).arg(hi.y(), 0, 'f', 3).arg(lo.z(), 0, 'f', 3).arg(hi.z(), 0, 'f', 3);
    };
    const QList<quint32> flagged = view.m_overlappingFixtures.values();
    for (int i = 0; i < flagged.size(); ++i)
        for (int j = i + 1; j < flagged.size(); ++j)
        {
            QVector3D alo, ahi, blo, bhi;
            boxOf(flagged.at(i), alo, ahi);
            boxOf(flagged.at(j), blo, bhi);
            bool hit = true;
            for (int a = 0; a < 3; ++a)
                hit = hit && alo[a] + 0.01f < bhi[a] && blo[a] + 0.01f < ahi[a];
            if (hit)
                out << "pair " << doc->fixture(flagged.at(i))->name() << " " << boxText(alo, ahi)
                    << " x " << doc->fixture(flagged.at(j))->name() << " " << boxText(blo, bhi) << "\n";
        }
    diag.close();
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    RigRoomLight_Test test;
    return QTest::qExec(&test, argc, argv);
}
