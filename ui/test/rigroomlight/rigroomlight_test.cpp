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
#include <QScopeGuard>
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

    // At Dim the warning is the warning (not the room's); in a blackout there is none (Q260).
    v.setAmbient(0.30);
    v.grab();
    bool warned = false;
    for (const QColor &c : fixtureEdges(v, dup->id()))
        warned = warned || redDominant(c);
    QVERIFY2(warned, "a real overlap lost its warning edge at Dim");
    v.setAmbient(0.0);
    v.grab();
    for (const QColor &c : fixtureEdges(v, dup->id()))
        QVERIFY2(!redDominant(c), qPrintable("a clash's red in a blackout: " + colourText(c)));
    QVERIFY(v.m_overlappingFixtures.contains(dup->id()));   // still a clash, for Show Review
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
    v.reload();
    v.setHighlight({ strip->id() });

    // At House: the selection's bright edge.
    v.setAmbient(0.62);
    v.grab();
    double brightest = 0.0;
    for (const QColor &c : fixtureEdges(v, strip->id()))
        brightest = qMax(brightest, luma(c));
    QVERIFY2(brightest > 150.0,
             qPrintable(QString("a selected fixture's edge at House: luma %1").arg(brightest)));

    /* In a Blackout: the selection has no edge, only the neutral faint one. */
    v.setAmbient(0.0);
    v.grab();
    brightest = 0.0;
    for (const QColor &c : fixtureEdges(v, strip->id()))
    {
        brightest = qMax(brightest, luma(c));
        QVERIFY2(qAbs(c.red() - c.green()) <= 12 && qAbs(c.green() - c.blue()) <= 12,
                 qPrintable("a coloured selection edge in a blackout: " + colourText(c)));
    }
    /* Q260: no selection edge at all in a Blackout -- the faint edge any
       unselected fixture has (blackoutEdgesAreFaintAndNeutral). */
    QVERIFY2(brightest <= 70.0,
             qPrintable(QString("a selected fixture still has an edge in a blackout: luma %1").arg(brightest)));

    /* Dragged, in a Blackout: an edge in the same selection grey, not a
       filled light body (it was the drag's amber, then a lit grey box). */
    v.setHighlight({});
    v.m_dragFid = strip->id();
    v.grab();
    {
        QImage canvas(64, 64, QImage::Format_ARGB32);
        QPainter p(&canvas);
        v.m_ops.clear();
        v.m_collecting = true;
        v.drawOneFixture(p, strip->id(), false);
        v.m_collecting = false;
        double edge = 0.0, face = 0.0;
        for (const auto &o : v.m_ops)
            if (o.kind == StructureStudioView::DrawOp::Poly && !o.light)
            {
                if (o.fill.isValid() && o.fill.alpha() > 0)
                    face = qMax(face, luma(o.fill));
                if (o.pen.isValid() && o.penWidth > 0.0)
                {
                    edge = qMax(edge, luma(o.pen));
                    QVERIFY2(qAbs(o.pen.red() - o.pen.green()) <= 12 && qAbs(o.pen.green() - o.pen.blue()) <= 12,
                             qPrintable("a coloured drag edge in a blackout: " + colourText(o.pen)));
                }
            }
        v.m_ops.clear();
        QVERIFY2(face <= 80.0, qPrintable(QString("a dragged body lit in a blackout: luma %1").arg(face)));
        QVERIFY2(edge >= 110.0, qPrintable(QString("a dragged fixture's edge lost in a blackout: luma %1").arg(edge)));
    }
    v.m_dragFid = Fixture::invalidId();
}

/* ---- The Blackout invariant, on the pixels ------------------------------
 *
 * "Room = Blackout, nothing playing": no fixture is putting out light, so
 * nothing on screen may be a colour. Measured as HSV on each pixel of the
 * frame the operator sees: saturation (max-min)/max above 0.35 AND value
 * max/255 above 0.25. Why those two:
 *   - 0.35: a neutral grey edge or label is under 0.1, a warm lamp-housing
 *     tint about 0.2; the selection's mint green (120,220,140) is 0.45, the
 *     overlap red 0.84, a cyan guide 1.0 -- every colour this view has drawn
 *     as an outline is well above it.
 *   - 0.25: below about 64 on the brightest channel a colour reads as black
 *     on this canvas (a red drape at blackout is a near-black red, which is
 *     what a dark room does to it); above it, it reads as a colour.
 * A clash's red is not drawn in a blackout (Q260); the allowance below for it
 * is now never used. */
namespace
{

struct ColourFinding
{
    int count = 0;
    QStringList where;      // the worst cells, each with a sample colour and what is there
};

bool saturatedLit(QRgb px)
{
    const int r = qRed(px), g = qGreen(px), b = qBlue(px);
    const int mx = qMax(r, qMax(g, b)), mn = qMin(r, qMin(g, b));
    if (mx <= 0)
        return false;
    const double v = mx / 255.0;
    const double s = double(mx - mn) / double(mx);
    return s > 0.35 && v > 0.25;
}

/* Where a clashing fixture draws itself (all its own primitives, its edges
   in the warning red among them), with a pixel or two for antialiasing. */
QRectF drawnBox(StructureStudioView &v, quint32 fid)
{
    QImage canvas(8, 8, QImage::Format_ARGB32);
    QPainter p(&canvas);
    v.m_ops.clear();
    v.m_collecting = true;
    v.drawOneFixture(p, fid, false);
    v.m_collecting = false;
    QRectF r;
    for (const auto &o : v.m_ops)
        if (!o.light && o.kind != StructureStudioView::DrawOp::Label && o.kind != StructureStudioView::DrawOp::Trace)
            for (const QPointF &q : o.poly)
                r = r.isNull() ? QRectF(q, QSizeF(0.01, 0.01)) : r.united(QRectF(q, QSizeF(0.01, 0.01)));
    v.m_ops.clear();
    // A warning edge is 1.1 px wide, drawn supersampled and smoothed down.
    return r.adjusted(-8, -8, 8, 8);
}

QRectF screenBox(StructureStudioView &v, quint32 fid)
{
    QVector3D k[8];
    v.fixtureBoxCorners(fid, classifyFixture(v.m_doc->fixture(fid)), k);
    QRectF r;
    for (const QVector3D &q : k)
    {
        const QPointF s = v.w2s(q);
        r = r.isNull() ? QRectF(s, QSizeF(0.01, 0.01)) : r.united(QRectF(s, QSizeF(0.01, 0.01)));
    }
    return r.adjusted(-4, -4, 4, 4);
}

/* Every saturated pixel of @p img, apart from a clash's red inside its own
   fixture's box, grouped in 40-pixel cells: the busiest twelve, each with a
   sample colour and the fixtures whose centre is in it. */
ColourFinding findColour(StructureStudioView &v, const QImage &img, QImage *mask = nullptr)
{
    QList<QRectF> clashes;
    QStringList clashText;
    for (quint32 fid : std::as_const(v.m_overlappingFixtures))
        if (v.m_doc->fixture(fid) != nullptr)
        {
            clashes << (v.m_plane == StructureStudioView::Angled ? drawnBox(v, fid) : screenBox(v, fid));
            const QRectF &r = clashes.last();
            clashText << QStringLiteral("%1 [%2,%3 %4x%5]").arg(v.m_doc->fixture(fid)->name())
                             .arg(int(r.x())).arg(int(r.y())).arg(int(r.width())).arg(int(r.height()));
        }
    QList<QRectF> marks;
    for (quint32 fid : std::as_const(v.m_unsaved))
        if (v.m_doc->fixture(fid) != nullptr)
        {
            const QPointF c = v.w2s(v.fixtureCentre(fid));
            marks << QRectF(c.x() - 22, c.y() - 22, 44, 44);
        }
    foreach (Scenery *sc, v.m_doc->monitorProperties()->sceneries())
        if (sc != nullptr && v.sceneryWarned(sc->id()))
        {
            QImage canvas(8, 8, QImage::Format_ARGB32);
            QPainter cp(&canvas);
            v.m_ops.clear();
            v.m_collecting = true;
            v.drawOneStructure(cp, StructureStudioView::SceneryKind, sc->id());
            v.m_collecting = false;
            QRectF r;
            for (const auto &o : v.m_ops)
                for (const QPointF &q : o.poly)
                    r = r.isNull() ? QRectF(q, QSizeF(0.01, 0.01)) : r.united(QRectF(q, QSizeF(0.01, 0.01)));
            v.m_ops.clear();
            if (!r.isNull())
                marks << r.adjusted(-3, -3, 3, 3);
        }
    const int cell = 40;
    QMap<QPair<int, int>, QPair<int, QRgb> > cells;
    ColourFinding f;
    for (int y = 0; y < img.height(); ++y)
    {
        const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x)
        {
            const QRgb px = line[x];
            if (!saturatedLit(px))
                continue;
            /* The warning's red, and its antialiased edge over the dark
               (a darker red): red's hue, inside a clashing fixture. */
            const int hue = QColor(px).hsvHue();
            /* A status mark's amber (an unsaved focus change's badge, a Show
               Review warning on scenery) is information, kept in a Blackout
               as the clash red is: allowed where that mark is. */
            if (hue >= 25 && hue <= 60)
            {
                bool inMark = false;
                for (const QRectF &r : std::as_const(marks))
                    inMark = inMark || r.contains(QPointF(x, y));
                if (inMark)
                    continue;
            }
            if (hue >= 0 && (hue <= 30 || hue >= 340))
            {
                bool inClash = false;
                for (const QRectF &r : std::as_const(clashes))
                    inClash = inClash || r.contains(QPointF(x, y));
                if (inClash)
                    continue;
            }
            ++f.count;
            if (mask != nullptr)
                mask->setPixel(x, y, qRgb(255, 0, 255));
            auto &c = cells[qMakePair(x / cell, y / cell)];
            if (c.first++ == 0)
                c.second = px;
        }
    }
    QList<QPair<int, QPair<int, int> > > order;
    for (auto it = cells.cbegin(); it != cells.cend(); ++it)
        order << qMakePair(it.value().first, it.key());
    std::sort(order.begin(), order.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
    for (int i = 0; i < order.size() && i < 12; ++i)
    {
        const QPair<int, int> key = order.at(i).second;
        const QRect r(key.first * cell, key.second * cell, cell, cell);
        QStringList names;
        for (quint32 fid : v.mountedFixtures())
            if (r.contains(v.w2s(v.fixtureCentre(fid)).toPoint()) && names.size() < 4)
                names << v.m_doc->fixture(fid)->name();
        f.where << QStringLiteral("x %1-%2 y %3-%4: %5 px, e.g. %6%7")
                       .arg(r.left()).arg(r.right()).arg(r.top()).arg(r.bottom())
                       .arg(order.at(i).first).arg(colourText(QColor(cells.value(key).second)))
                       .arg(names.isEmpty() ? QString() : QStringLiteral(" near ") + names.join(QStringLiteral(", ")));
    }
    if (f.count > 0)
        f.where << QStringLiteral("clashes allowed: ") + clashText.join(QStringLiteral(", "))
                 + QStringLiteral("; status marks allowed: %1").arg(marks.size());
    if (mask != nullptr)
    {
        QPainter mp(mask);
        mp.setPen(QPen(QColor(0, 255, 0), 1.0));
        for (const QRectF &r : std::as_const(clashes))
            mp.drawRect(r);
    }
    return f;
}

enum StageViewKind { ViewTop, ViewFront, ViewSide, ViewBack, ViewOverview, ViewAbove };

void setStageView(StructureStudioView &v, int which)
{
    v.setStandOnCentreStage(false);
    switch (which)
    {
    case ViewTop:   v.setPlane(StructureStudioView::Top); break;
    case ViewFront: v.setPlane(StructureStudioView::Front); break;
    case ViewSide:  v.setPlane(StructureStudioView::Side); break;
    case ViewBack:  v.setPlane(StructureStudioView::Back); break;
    case ViewOverview:
        // As the Rig Overview opens: 45°, stood at centre stage.
        v.setPlane(StructureStudioView::Angled);
        v.setStandOnCentreStage(true);
        break;
    default:
        v.setPlane(StructureStudioView::Angled);
        v.setAngledView(20.0, 30.0);
        break;
    }
}

}

void RigRoomLight_Test::loadStageShow()
{
    if (m_stageShowLoaded)
        return;
    if (m_app == nullptr)
    {
        m_app = new App();
        m_app->init();
    }
    QLCFixtureDefCache *cache = m_app->doc()->fixtureDefCache();
    cache->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures"));
    m_app->clearDocument();
    QCOMPARE(m_app->loadXML(QStringLiteral(RIGROOM_SRC_DIR "/stage-show.qlcc")), QFile::NoError);
    m_app->doc()->resetModified();
    QVERIFY2(m_app->doc()->fixtures().size() > 150,
             qPrintable(QString("the stage show has %1 fixtures").arg(m_app->doc()->fixtures().size())));
    /* The main drape open, as the stage is looked at: its curtain fixture
       at zero (nothing playing) would close it under Live, and the rig is
       behind it. */
    foreach (Scenery *sc, m_app->doc()->monitorProperties()->sceneries())
        if (sc != nullptr && sc->hasState())
        {
            sc->setDriverFixture(Scenery::invalidId());
            sc->setState(Scenery::Open);
        }
    settle(100);
    m_stageShowLoaded = true;
}

void RigRoomLight_Test::blackoutDrawsNoColour_data()
{
    QTest::addColumn<int>("view");
    QTest::addColumn<bool>("selectAll");
    const struct { const char *name; int view; } views[] = {
        { "top", ViewTop }, { "front", ViewFront }, { "side", ViewSide }, { "back", ViewBack },
        { "45 overview", ViewOverview }, { "45 above", ViewAbove } };
    for (const auto &v : views)
    {
        QTest::newRow(qPrintable(QStringLiteral("%1, nothing selected").arg(v.name))) << v.view << false;
        QTest::newRow(qPrintable(QStringLiteral("%1, whole rig selected").arg(v.name))) << v.view << true;
    }
}

void RigRoomLight_Test::blackoutDrawsNoColour()
{
    QFETCH(int, view);
    QFETCH(bool, selectAll);
    loadStageShow();
    Doc *doc = m_app->doc();
    for (Fixture *fx : doc->fixtures())
        fx->setOwnChannelValues(QByteArray(int(fx->channels()), 0));   // nothing playing
    // Heads where the output puts them, not on their way: one frame is one picture.
    const bool moveSpeed = StructureStudioView::moveSpeed();
    StructureStudioView::setMoveSpeed(false);
    auto restoreMoveSpeed = qScopeGuard([moveSpeed]() { StructureStudioView::setMoveSpeed(moveSpeed); });

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.setLocked(true);
    v.setLiveValues(true);
    v.setBeams(true);
    v.setAmbient(0.0);
    setStageView(v, view);
    QList<quint32> all;
    if (selectAll)
        for (Fixture *fx : doc->fixtures())
            all << fx->id();
    v.setHighlight(all);
    v.resize(1280, 800);
    v.reload();
    settle(30);
    v.m_overlapAge.invalidate();
    const QImage img = v.grab().toImage().convertToFormat(QImage::Format_ARGB32);

    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    const QString tag = QString(QTest::currentDataTag()).replace(QLatin1Char(' '), QLatin1Char('-'))
                            .remove(QLatin1Char(',')).replace(QStringLiteral("°"), QString());
    if (!dir.isEmpty())
        img.save(QDir(dir).filePath(QStringLiteral("%1-blackout-%2.png")
                                        .arg(qEnvironmentVariable("QLC_SHOT_STATE", QStringLiteral("after")), tag)));

    QImage mask = img.convertToFormat(QImage::Format_RGB32);
    const ColourFinding f = findColour(v, img, &mask);
    if (!dir.isEmpty() && f.count > 0)
    {
        const QString base = QDir(dir).filePath(QStringLiteral("%1-blackout-%2")
            .arg(qEnvironmentVariable("QLC_SHOT_STATE", QStringLiteral("after")), tag));
        mask.save(base + QStringLiteral("-colour.png"));
        QFile txt(base + QStringLiteral("-colour.txt"));
        if (txt.open(QIODevice::WriteOnly | QIODevice::Text))
            txt.write(f.where.join(QLatin1Char('\n')).toUtf8());
    }
    /* Review shots of the Rig Overview as it opens (45°, centre stage) and
       of the Front elevation, at the three review sizes. */
    if (!dir.isEmpty() && (view == ViewOverview || view == ViewFront))
    {
        const QSize sizes[] = { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) };
        for (const QSize &sz : sizes)
        {
            v.resize(sz);
            v.reload();
            settle(30);
            v.grab().toImage().save(QDir(dir).filePath(QStringLiteral("%1-rigoverview-blackout-%2-%3x%4.png")
                .arg(qEnvironmentVariable("QLC_SHOT_STATE", QStringLiteral("after")), tag)
                .arg(sz.width()).arg(sz.height())));
        }
    }
    QVERIFY2(f.count == 0,
             qPrintable(QStringLiteral("%1 saturated pixels in a blackout (%2 clash fixtures allowed), at:\n  %3")
                            .arg(f.count).arg(v.m_overlappingFixtures.size()).arg(f.where.join(QStringLiteral("\n  ")))));
}

void RigRoomLight_Test::aLitFixtureIsInColourInABlackout()
{
    loadStageShow();
    Doc *doc = m_app->doc();
    for (Fixture *fx : doc->fixtures())
        fx->setOwnChannelValues(QByteArray(int(fx->channels()), 0));

    // The front step's lowest strip, on its face, at full red: its dimmers and its red.
    Fixture *lit = nullptr;
    for (Fixture *fx : doc->fixtures())
        if (fx->name() == QLatin1String("DS1 #1") && fx->isPatched() && fx->heads() > 8)
        {
            lit = fx;
            break;
        }
    QVERIFY2(lit != nullptr, "no patched step strip in the stage show");
    QByteArray vals(int(lit->channels()), 0);
    for (quint32 ch = 0; ch < lit->channels(); ++ch)
    {
        const QLCChannel *c = lit->channel(ch);
        if (c != nullptr && c->group() == QLCChannel::Intensity
            && (c->colour() == QLCChannel::Red || c->colour() == QLCChannel::NoColour))
            vals[int(ch)] = char(255);
    }
    QVERIFY(lit->setOwnChannelValues(vals));

    for (int view : { int(ViewOverview), int(ViewFront) })
    {
        StructureStudioView v(doc, StructureStudioView::StageKind, 0);
        v.setLocked(true);
        v.setLiveValues(true);
        v.setBeams(true);
        v.setAmbient(0.0);
        setStageView(v, view);
        v.setHighlight({});
        v.resize(1280, 800);
        v.reload();
        settle(30);
        const QImage img = v.grab().toImage().convertToFormat(QImage::Format_ARGB32);
        const QRectF box = screenBox(v, lit->id()).adjusted(-6, -6, 6, 6);
        int red = 0;
        for (int y = qMax(0, int(box.top())); y < qMin(img.height(), int(box.bottom())); ++y)
            for (int x = qMax(0, int(box.left())); x < qMin(img.width(), int(box.right())); ++x)
            {
                const QRgb px = img.pixel(x, y);
                if (saturatedLit(px) && redDominant(QColor(px)))
                    ++red;
            }
        QVERIFY2(red >= 20, qPrintable(QStringLiteral("view %1: %2 lit red pixels on %3 (box %4,%5 %6x%7)")
                                       .arg(view).arg(red).arg(lit->name())
                                       .arg(box.x()).arg(box.y()).arg(box.width()).arg(box.height())));
    }
    lit->setOwnChannelValues(QByteArray(int(lit->channels()), 0));
}

void RigRoomLight_Test::selectedNamesNeverOverlap()
{
    loadStageShow();
    Doc *doc = m_app->doc();
    for (Fixture *fx : doc->fixtures())
        fx->setOwnChannelValues(QByteArray(int(fx->channels()), 0));
    for (int view : { int(ViewOverview), int(ViewAbove), int(ViewFront), int(ViewTop) })
    {
        StructureStudioView v(doc, StructureStudioView::StageKind, 0);
        v.setLocked(true);
        v.setLiveValues(true);
        v.setAmbient(0.0);
        setStageView(v, view);
        QList<quint32> all;
        for (Fixture *fx : doc->fixtures())
            all << fx->id();
        v.setHighlight(all);
        v.resize(1280, 800);
        v.reload();
        v.grab();
        const QList<QRectF> names = v.drawnNameLabels();
        QVERIFY2(!names.isEmpty(), qPrintable(QStringLiteral("view %1: no names drawn").arg(view)));
        int clashes = 0;
        QString first;
        for (int i = 0; i < names.size(); ++i)
            for (int j = i + 1; j < names.size(); ++j)
                if (names.at(i).intersects(names.at(j)))
                {
                    if (clashes++ == 0)
                        first = QStringLiteral("(%1,%2 %3x%4) and (%5,%6)").arg(names.at(i).x()).arg(names.at(i).y())
                                    .arg(names.at(i).width()).arg(names.at(i).height())
                                    .arg(names.at(j).x()).arg(names.at(j).y());
                }
        QVERIFY2(clashes == 0, qPrintable(QStringLiteral("view %1: %2 of %3 names overlap, first %4")
                                          .arg(view).arg(clashes).arg(names.size()).arg(first)));

        /* Against what the view wanted to write: everything written was
           wanted, and a name left out was left out only because one that is
           written is where it would go -- not lost for nothing. */
        const QList<QRectF> wanted = v.wantedNameLabels();
        QVERIFY2(wanted.size() >= names.size(),
                 qPrintable(QStringLiteral("view %1: %2 written, %3 wanted").arg(view).arg(names.size()).arg(wanted.size())));
        for (const QRectF &n : names)
            QVERIFY2(wanted.contains(n), qPrintable(QStringLiteral("view %1: a name written that was not wanted").arg(view)));
        int lost = 0;
        for (const QRectF &w : wanted)
        {
            if (names.contains(w))
                continue;
            bool covered = false;
            for (const QRectF &n : names)
                if (n.intersects(w))
                {
                    covered = true;
                    break;
                }
            lost += covered ? 0 : 1;
        }
        QVERIFY2(lost == 0, qPrintable(QStringLiteral("view %1: %2 names left out with nothing in their place")
                                       .arg(view).arg(lost)));
    }
}

void RigRoomLight_Test::aboveDimTheClashAndTheSelectionAreAsBefore()
{
    loadStageShow();
    Doc *doc = m_app->doc();
    for (Fixture *fx : doc->fixtures())
        fx->setOwnChannelValues(QByteArray(int(fx->channels()), 0));
    const bool moveSpeed = StructureStudioView::moveSpeed();
    StructureStudioView::setMoveSpeed(false);
    auto restoreMoveSpeed = qScopeGuard([moveSpeed]() { StructureStudioView::setMoveSpeed(moveSpeed); });

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.setLocked(true);
    v.setLiveValues(true);
    setStageView(v, ViewOverview);
    v.resize(1280, 800);
    v.reload();
    v.grab();

    // The show's clashing movers (the towers' heads).
    QList<quint32> movers;
    for (quint32 fid : std::as_const(v.m_overlappingFixtures))
        if (classifyFixture(doc->fixture(fid)).kind == FixtureSilhouette::Mover)
            movers << fid;
    QVERIFY2(!movers.isEmpty(), "no clashing mover in the stage show");
    const QColor warning(255, 70, 40);
    const QColor mint(120, 220, 140);

    QImage canvas(8, 8, QImage::Format_ARGB32);
    QPainter p(&canvas);
    auto opsOf = [&](quint32 fid) {
        v.m_ops.clear();
        v.m_collecting = true;
        v.drawOneFixture(p, fid, false);
        v.m_collecting = false;
        QVector<StructureStudioView::DrawOp> ops = v.m_ops;
        v.m_ops.clear();
        return ops;
    };

    const struct { double level; const char *name; } rooms[] = {
        { 1.0, "Work light" }, { 0.62, "House" }, { 0.30, "Dim" }, { 0.0, "Blackout" } };
    for (const auto &room : rooms)
    {
        v.setAmbient(room.level);
        v.setHighlight({});
        v.grab();
        /* A clash's red from Dim up -- body edges and the aim stub, the
           line that points out of it; none in a Blackout (Q260). */
        for (quint32 fid : std::as_const(movers))
        {
            int edges = 0, stubs = 0;
            for (const auto &o : opsOf(fid))
            {
                if (o.kind == StructureStudioView::DrawOp::Poly && o.pen == warning && o.penWidth > 0.0)
                    ++edges;
                if (o.kind == StructureStudioView::DrawOp::Line && o.pen == warning)
                    ++stubs;
            }
            const bool blackout = room.level < 0.30;
            QVERIFY2(blackout ? (edges == 0 && stubs == 0) : (edges > 0 && stubs > 0),
                     qPrintable(QStringLiteral("%1, %2: %3 red edges, %4 red stubs")
                                    .arg(room.name, doc->fixture(fid)->name()).arg(edges).arg(stubs)));
        }

        // The selection: the mint at Dim and above, as it always was; none in a Blackout.
        quint32 strip = Fixture::invalidId();
        for (Fixture *fx : doc->fixtures())
            if (fx->name() == QLatin1String("DS1 #1"))
                strip = fx->id();
        QVERIFY(strip != Fixture::invalidId());
        v.setHighlight({ strip });
        v.grab();
        bool sawMint = false;
        for (const auto &o : opsOf(strip))
            if (o.kind == StructureStudioView::DrawOp::Poly && !o.light)
            {
                const QColor f = o.fill;
                /* drawSolidBox() shades each face (lighter/darker) from the
                   one base: mint's own hue and saturation on some face. */
                sawMint = sawMint || (f.isValid() && qAbs(f.hsvHue() - mint.hsvHue()) <= 2
                                      && qAbs(f.hsvSaturation() - mint.hsvSaturation()) <= 6);
            }
        if (room.level >= 0.30)
            QVERIFY2(sawMint, qPrintable(QStringLiteral("%1: a selected strip is not in the mint").arg(room.name)));
        else
            QVERIFY2(!sawMint, "Blackout: a selected strip is still filled in the mint");
    }
}

/* Q260, on the pixels: Rig Overview, Room = Blackout, a clashing pair and a
   strip selected. No clash red and no selection edge around them; at Dim and
   above both are drawn. The selection edge is measured as the light-grey
   pixels selecting adds inside the fixture's own box (the frame with the same
   fixtures unselected is the reference), the clash red as red-dominant pixels there. */
void RigRoomLight_Test::aBlackoutHasNoClashRedAndNoSelectionEdge()
{
    loadStageShow();
    Doc *doc = m_app->doc();
    for (Fixture *fx : doc->fixtures())
        fx->setOwnChannelValues(QByteArray(int(fx->channels()), 0));
    const bool moveSpeed = StructureStudioView::moveSpeed();
    StructureStudioView::setMoveSpeed(false);
    auto restoreMoveSpeed = qScopeGuard([moveSpeed]() { StructureStudioView::setMoveSpeed(moveSpeed); });

    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.setLocked(true);
    v.setLiveValues(true);
    setStageView(v, ViewOverview);
    v.resize(1280, 800);
    v.reload();
    v.grab();

    QList<quint32> pair;
    for (quint32 fid : std::as_const(v.m_overlappingFixtures))
        if (classifyFixture(doc->fixture(fid)).kind == FixtureSilhouette::Mover)
            pair << fid;
    QVERIFY2(pair.size() >= 2, "no clashing pair of movers in the stage show");
    quint32 strip = Fixture::invalidId();
    for (Fixture *fx : doc->fixtures())
        if (fx->name() == QLatin1String("DS1 #1") && !v.m_overlappingFixtures.contains(fx->id()))
            strip = fx->id();
    QVERIFY(strip != Fixture::invalidId());
    QList<quint32> selected = pair;
    selected << strip;

    auto frame = [&](double level, const QList<quint32> &sel, const QSize &size) {
        v.resize(size);
        v.setAmbient(level);
        v.setHighlight(sel);
        v.reload();
        settle(30);
        v.m_overlapAge.invalidate();
        v.grab();
        return v.grab().toImage().convertToFormat(QImage::Format_ARGB32);
    };
    auto inBox = [&](const QImage &img, const QRectF &box, auto &&visit) {
        for (int y = qMax(0, int(box.top())); y <= qMin(img.height() - 1, int(box.bottom())); ++y)
            for (int x = qMax(0, int(box.left())); x <= qMin(img.width() - 1, int(box.right())); ++x)
                visit(x, y);
    };
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    const QString state = qEnvironmentVariable("QLC_SHOT_STATE", QStringLiteral("after"));

    const QSize sizes[] = { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) };
    for (const QSize &sz : sizes)
    {
        const struct { double level; const char *name; } rooms[] = { { 0.0, "blackout" }, { 0.30, "dim" } };
        for (const auto &room : rooms)
        {
            const QImage none = frame(room.level, {}, sz);
            const QImage sel = frame(room.level, selected, sz);
            if (!dir.isEmpty())
                sel.save(QDir(dir).filePath(QStringLiteral("%1-q260-%2-%3x%4.png")
                                                .arg(state, QString::fromLatin1(room.name)).arg(sz.width()).arg(sz.height())));
            const QString where = QStringLiteral("%1 %2x%3").arg(QString::fromLatin1(room.name)).arg(sz.width()).arg(sz.height());

            int red = 0;
            for (quint32 fid : std::as_const(pair))
                inBox(sel, drawnBox(v, fid), [&](int x, int y) { red += redDominant(sel.pixelColor(x, y)) ? 1 : 0; });
            /* Light-grey pixels (an edge line: unselected edges and unlit
               faces are under luma 100 in a Blackout) that selecting adds. */
            int edge = 0;
            inBox(sel, drawnBox(v, strip), [&](int x, int y) {
                if (luma(sel.pixelColor(x, y)) >= 100.0 && luma(none.pixelColor(x, y)) < 100.0)
                    ++edge;
            });
            if (room.level < 0.30)
            {
                QVERIFY2(red == 0, qPrintable(QStringLiteral("%1: %2 clash-red pixels on a clashing pair").arg(where).arg(red)));
                QVERIFY2(edge <= 12, qPrintable(QStringLiteral("%1: %2 selection-edge pixels round a selected strip").arg(where).arg(edge)));
            }
            else
            {
                QVERIFY2(red >= 20, qPrintable(QStringLiteral("%1: only %2 clash-red pixels at Dim").arg(where).arg(red)));
                QVERIFY2(edge >= 30, qPrintable(QStringLiteral("%1: only %2 selection-edge pixels at Dim").arg(where).arg(edge)));
            }
        }
    }
    // Show Review still lists them: the clash is a fact of the rig, not of the room.
    QVERIFY(v.m_overlappingFixtures.contains(pair.first()));
}

void RigRoomLight_Test::statusMarksKeepTheirColourInABlackout()
{
    loadStageShow();
    Doc *doc = m_app->doc();
    quint32 fid = Fixture::invalidId();
    for (Fixture *fx : doc->fixtures())
        if (fx->name() == QLatin1String("DS1 #1"))
            fid = fx->id();
    QVERIFY(fid != Fixture::invalidId());
    StructureStudioView v(doc, StructureStudioView::StageKind, 0);
    v.setLocked(true);
    v.setLiveValues(true);
    v.setAmbient(0.0);
    setStageView(v, ViewOverview);
    v.resize(1280, 800);
    v.reload();
    // A focus change not saved yet: its badge is the amber it always was.
    v.setUnsavedMarks({ fid });
    const QImage img = v.grab().toImage().convertToFormat(QImage::Format_ARGB32);
    const QPointF c = v.w2s(v.fixtureCentre(fid));
    int amber = 0;
    for (int y = int(c.y()) - 24; y <= int(c.y()) + 24; ++y)
        for (int x = int(c.x()) - 24; x <= int(c.x()) + 24; ++x)
            if (img.rect().contains(x, y))
            {
                const QColor q = img.pixelColor(x, y);
                if (qAbs(q.red() - 240) < 30 && qAbs(q.green() - 200) < 30 && q.blue() < 90)
                    ++amber;
            }
    QVERIFY2(amber >= 20, qPrintable(QStringLiteral("the unsaved badge in a blackout: %1 amber px").arg(amber)));
    // And the invariant still holds: the badge is a status mark, allowed as a clash's red is.
    const ColourFinding f = findColour(v, img);
    QVERIFY2(f.count == 0, qPrintable(QStringLiteral("%1 saturated px apart from the marks:\n  %2")
                                      .arg(f.count).arg(f.where.join(QStringLiteral("\n  ")))));
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
