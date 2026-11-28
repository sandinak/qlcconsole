/*
  Q Light Controller Plus
  structurestudioview_lightmap.cpp

  The rig view's light map (Capture-study gap 3, board Q359): how much light
  reaches a person standing on the stage, at the show's subject height, as a
  heat map over the stage with the dark spots outlined.

  Cost. Off, it is one bool in paintEvent() and nothing else. On, the lights
  are read from the output on the GUI thread (a fixture's level, beam and
  direction: what the frame reads anyway) and the grid -- a few thousand
  points, each against every lit light and the line-of-sight model -- is
  worked out on a pool thread, only when what it reads has changed (the
  lights, the stage, the height or the level) and at most five times a
  second. A frame then draws one small image and an outline: well under a
  millisecond (lightMapPaintMs()).

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFontMetrics>
#include <QThreadPool>
#include <QPainter>
#include <QPointer>
#include <QRegion>
#include <QTimer>
#include <QtMath>
#include <cmath>

#include "structurestudioview.h"
#include "fixturevisualtraits.h"
#include "monitorproperties.h"
#include "measurementunits.h"
#include "qlcfixturemode.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "aimsolver.h"
#include "fixture.h"
#include "truss.h"
#include "doc.h"

namespace
{
/* Points no closer than this, the map no bigger than this: a 40 x 30 ft
   stage is about 2000 points 8" apart. */
const int kMaxCells = 2400;
const int kMinIntervalMs = 200;

quint64 mix(quint64 h, quint64 v)
{
    h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
}

quint64 bitsOf(float f)
{
    // Rounded so a value that wobbles in its last bits is the same map.
    return quint64(qint64(qRound64(double(f) * 1000.0)));
}

/* The heat map's colour for @p lux against the dark level: dark blue under
   it, then green, yellow and white as it climbs (by eights). */
QRgb heat(double lux, double dark)
{
    if (lux < dark)
    {
        const double k = dark > 0.0 ? qBound(0.0, lux / dark, 1.0) : 0.0;
        const int a = int(150 - 50 * k);
        return qPremultiply(qRgba(20, 30, int(90 + 40 * k), a));
    }
    const double t = qBound(0.0, std::log(lux / dark) / std::log(64.0), 1.0);   // 1x .. 64x
    struct Stop { double at; int r, g, b; };
    static const Stop stops[] = { { 0.0, 30, 150, 110 }, { 0.35, 120, 200, 60 },
                                  { 0.7, 245, 215, 60 }, { 1.0, 255, 250, 235 } };
    int i = 0;
    while (i < 2 && t > stops[i + 1].at)
        ++i;
    const Stop &a = stops[i], &b = stops[i + 1];
    const double u = (t - a.at) / (b.at - a.at);
    const int alpha = 130;
    return qPremultiply(qRgba(int(a.r + (b.r - a.r) * u), int(a.g + (b.g - a.g) * u),
                              int(a.b + (b.b - a.b) * u), alpha));
}
}

void StructureStudioView::setLightMap(bool on)
{
    if (m_lightMapOn == on)
        return;
    m_lightMapOn = on;
    if (!on)
    {
        if (m_lightCancel)
            m_lightCancel->store(true);
        m_lightPending = false;
        m_lightKey = 0;
        m_lightGrid = LightMap::Grid();
        m_lightImage = QImage();
        m_lightDark = QPainterPath();
        m_lightSpots = 0;
    }
    else
        requestLightMap();
    update();
}

void StructureStudioView::lightMapSettingsChanged()
{
    if (!m_lightMapOn)
        return;
    requestLightMap();
    update();
}

QVector<LightMap::Source> StructureStudioView::lightSources() const
{
    QVector<LightMap::Source> out;
    MonitorProperties *mp = m_doc != nullptr ? m_doc->monitorProperties() : nullptr;
    if (mp == nullptr)
        return out;
    for (Fixture *fx : m_doc->fixtures())
    {
        if (fx == nullptr || fx->fixtureMode() == nullptr || !mp->containsFixture(fx->id())
            || !fixtureShown(fx->id()))
            continue;
        const QLCPhysical &phys = fx->fixtureMode()->physical();
        if (!phys.emitsLight())
            continue;
        const FixtureVisualTraits traits = classifyFixture(fx);
        // A laser's pencils, a sky of stars, a lantern glowing on its hook: none lights a face.
        if (traits.laser || traits.stars || traits.omni || traits.flame
            || traits.kind == FixtureSilhouette::Lantern)
            continue;
        const QByteArray vals = fx->channelValues();
        QColor colour;
        uchar dim = 0;
        if (!fixtureLiveState(fx, vals, colour, dim) || dim <= 2)
            continue;
        const FixtureRigProps rp = mp->fixtureRigProps(fx->id());
        QVector3D dir;
        const bool mover = fx->channelNumber(QLCChannel::Pan, QLCChannel::MSB) != QLCChannel::invalid()
                           || fx->channelNumber(QLCChannel::Tilt, QLCChannel::MSB) != QLCChannel::invalid();
        if (mover ? !AimSolver::headDirection(fx, vals, 0, rp, dir) : !emitDirection(fx, rp, traits, dir))
            continue;
        if (dir.isNull())
            continue;
        // What its zoom has it at now; else its definition's lens.
        double deg = fixtureBeamAngle(fx, vals, traits);
        deg = deg <= 0.0 ? LightMap::beamDegrees(phys) : deg >= 170.0 ? 150.0 : deg;
        LightMap::Source s = LightMap::makeSource(fx->id(), fixtureCentre(fx->id()), dir, deg,
                                                  LightMap::fixtureLumens(fx), dim / 255.0,
                                                  double(traits.edgeSoftness));
        s.exclude = BeamOcclusion::ownMounts(mp, fx->id());
        out << s;
    }
    return out;
}

void StructureStudioView::requestLightMap()
{
    if (!m_lightMapOn)
        return;
    if (m_lightBusy)
    {
        m_lightPending = true;
        return;
    }
    const qint64 since = m_lightStarted.isValid() ? m_lightStarted.elapsed() : kMinIntervalMs;
    if (since < kMinIntervalMs)
    {
        if (m_lightThrottle == nullptr)
        {
            m_lightThrottle = new QTimer(this);
            m_lightThrottle->setSingleShot(true);
            connect(m_lightThrottle, &QTimer::timeout, this, [this]() { startLightMap(); });
        }
        if (!m_lightThrottle->isActive())
            m_lightThrottle->start(int(kMinIntervalMs - since));
        return;
    }
    startLightMap();
}

void StructureStudioView::startLightMap()
{
    if (!m_lightMapOn || m_lightBusy || m_doc == nullptr)
        return;
    m_lightPending = false;             // what it reads now is read here
    MonitorProperties *mp = m_doc->monitorProperties();
    const QVector<LightMap::Source> sources = lightSources();
    const BeamOcclusion::ModelPtr sight = mp->occlusionModel(m_doc);
    const QRectF area = LightMap::stageArea(mp);
    const double height = double(mp->lightMapHeight());
    const double dark = double(mp->lightMapDarkLux());

    // What it reads: the same as last time is the same map.
    quint64 key = 0x51ed2701;
    key = mix(key, sight.isNull() ? 0 : sight->signature());
    key = mix(key, bitsOf(float(area.left())));
    key = mix(key, bitsOf(float(area.top())));
    key = mix(key, bitsOf(float(area.width())));
    key = mix(key, bitsOf(float(area.height())));
    key = mix(key, bitsOf(float(height)));
    key = mix(key, bitsOf(float(dark)));
    for (const LightMap::Source &s : sources)
    {
        key = mix(key, s.fixture);
        key = mix(key, bitsOf(s.apex.x()) ^ (bitsOf(s.apex.y()) << 21) ^ (bitsOf(s.apex.z()) << 42));
        key = mix(key, bitsOf(s.dir.x()) ^ (bitsOf(s.dir.y()) << 21) ^ (bitsOf(s.dir.z()) << 42));
        key = mix(key, bitsOf(s.halfRad) ^ (bitsOf(s.candela * 0.01f) << 24));
    }
    if (key == m_lightKey && m_lightKey != 0)
        return;

    // The deck under each point is read here, on the GUI thread; the rest is pure.
    LightMap::Grid grid = LightMap::layout(mp, area, height, dark, kMaxCells);
    m_lightBusy = true;
    m_lightBusyKey = key;
    m_lightStarted.start();
    m_lightCancel = std::make_shared<std::atomic<bool>>(false);
    std::shared_ptr<std::atomic<bool>> cancel = m_lightCancel;
    QPointer<StructureStudioView> self(this);
    QThreadPool::globalInstance()->start([self, cancel, grid, sources, sight, key]() mutable {
        QElapsedTimer t;
        t.start();
        const bool done = LightMap::compute(grid, sources, sight, cancel.get());
        const double ms = t.nsecsElapsed() / 1e6;
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, cancel, grid, key, ms, done]() {
            if (self.isNull())
                return;
            if (!done || cancel->load())
            {
                self->m_lightBusy = false;
                if (self->m_lightMapOn)
                    self->requestLightMap();
                return;
            }
            self->lightMapDone(grid, key, ms);
        }, Qt::QueuedConnection);
    });
}

void StructureStudioView::lightMapDone(const LightMap::Grid &grid, quint64 key, double ms)
{
    m_lightBusy = false;
    m_lightComputeMs = ms;
    if (!m_lightMapOn)
        return;
    m_lightGrid = grid;
    m_lightKey = key;
    m_lightImage = QImage(grid.cols, grid.rows, QImage::Format_ARGB32_Premultiplied);
    QRegion dark;
    for (int r = 0; r < grid.rows; ++r)
    {
        QRgb *line = reinterpret_cast<QRgb *>(m_lightImage.scanLine(r));
        int runFrom = -1;
        for (int c = 0; c <= grid.cols; ++c)
        {
            const bool d = c < grid.cols && grid.dark(c, r);
            if (c < grid.cols)
                line[c] = heat(double(grid.lux.at(grid.index(c, r))), grid.darkLux);
            if (d && runFrom < 0)
                runFrom = c;
            else if (!d && runFrom >= 0)
            {
                dark += QRegion(runFrom, r, c - runFrom, 1);
                runFrom = -1;
            }
        }
    }
    QPainterPath path;
    path.addRegion(dark);
    m_lightDark = path.simplified();
    m_lightSpots = int(LightMap::darkSpots(grid).size());
    emit lightMapUpdated();
    update();
    if (m_lightPending)
        requestLightMap();
}

bool StructureStudioView::waitForLightMap(int ms)
{
    QElapsedTimer t;
    t.start();
    requestLightMap();
    while (t.elapsed() < ms)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        if (!m_lightBusy && !m_lightPending && (m_lightThrottle == nullptr || !m_lightThrottle->isActive()))
        {
            // Up to date with what it reads now?
            startLightMap();
            if (!m_lightBusy)
                return true;
        }
    }
    return false;
}

QString StructureStudioView::lightMapCaption() const
{
    if (!m_lightMapOn)
        return QString();
    if (m_plane != Top && m_plane != Angled)
        return tr("Light map: in the Top or Angled view");
    MonitorProperties *mp = m_doc->monitorProperties();
    if (!LightMap::stageArea(mp).isValid())
        return tr("Light map: the stage has no width or depth");
    if (!m_lightGrid.isValid())
        return tr("Light map: working it out…");
    const QString head = tr("Light at %1 · dark under %2")
                             .arg(MeasurementUnits::formatMetres(m_lightGrid.height),
                                  MeasurementUnits::formatIlluminance(m_lightGrid.darkLux));
    if (m_lightSpots == 0)
        return head + tr(" · no dark spots");
    return head + tr(" · %n dark spot(s)", nullptr, m_lightSpots);
}

void StructureStudioView::drawLightMap(QPainter &p) const
{
    QElapsedTimer t;
    t.start();
    const bool flat = (m_plane == Top || m_plane == Angled);
    if (flat && m_lightGrid.isValid() && !m_lightImage.isNull())
    {
        const LightMap::Grid &g = m_lightGrid;
        const float z = float(g.height);
        const QRectF a = g.area;
        const QPointF o = w2s(QVector3D(float(a.left()), float(a.top()), z));
        const QPointF ex = w2s(QVector3D(float(a.right()), float(a.top()), z)) - o;
        const QPointF ey = w2s(QVector3D(float(a.left()), float(a.bottom()), z)) - o;
        // A plane is drawn as a parallelogram in an orthographic view: one transform.
        const QTransform cells(ex.x() / g.cols, ex.y() / g.cols, ey.x() / g.rows, ey.y() / g.rows, o.x(), o.y());
        p.save();
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.setTransform(cells, true);
        p.drawImage(QRectF(0, 0, g.cols, g.rows), m_lightImage);
        p.restore();
        if (!m_lightDark.isEmpty())
        {
            p.save();
            QPen pen(QColor(255, 176, 50), 2.0, Qt::DashLine);
            pen.setCosmetic(true);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawPath(cells.map(m_lightDark));
            p.restore();
        }
    }

    // The legend, bottom left, over the picture: what it shows and what dark is.
    const QString caption = lightMapCaption();
    if (!caption.isEmpty())
    {
        p.save();
        QFont f = p.font();
        f.setPointSizeF(qMax(8.0, f.pointSizeF()));
        p.setFont(f);
        const QFontMetrics fm(f);
        const int rampW = (flat && m_lightGrid.isValid()) ? 90 : 0;
        const int w = fm.horizontalAdvance(caption) + 16 + (rampW > 0 ? rampW + 8 : 0);
        const int h = fm.height() + 10;
        const QRect box(8, height() - h - 30, qMin(w, width() - 16), h);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(20, 22, 28, 200));
        p.drawRoundedRect(box, 4, 4);
        int x = box.left() + 8;
        if (rampW > 0)
        {
            const double dark = m_lightGrid.darkLux;
            for (int i = 0; i < rampW; ++i)
            {
                // Half a dark level to 64 times it, by eights.
                const double lux = dark * 0.5 * std::pow(128.0, double(i) / (rampW - 1));
                p.fillRect(QRect(x + i, box.top() + 6, 1, h - 12), QColor::fromRgba(qUnpremultiply(heat(lux, dark))));
            }
            x += rampW + 8;
        }
        p.setPen(QColor(230, 232, 238));
        p.drawText(QRect(x, box.top(), box.right() - x, box.height()), Qt::AlignVCenter | Qt::AlignLeft, caption);
        p.restore();
    }
    m_lightPaintMs = t.nsecsElapsed() / 1e6;
}
