/*
  Q Light Controller Plus
  mountinsituview.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QVBoxLayout>
#include <QPainter>
#include <QLabel>
#include <QtMath>
#include <QPainterPath>
#include <cmath>
#include "aimsolver.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcphysical.h"
#include "fixture.h"
#include <QEvent>

#include "mountinsituview.h"
#include "structurestudioview.h"
#include "measurementunits.h"
#include "monitorproperties.h"
#include "stageplatform.h"
#include "truss.h"
#include "tower.h"
#include "pipe.h"
#include "doc.h"

/* The annotations, drawn over the picture where things really are on screen:
   the fixture as a side-on glyph (hung from its mount or standing on it, on the
   face it is on) with a short beam stub, and the height-offset dimension between
   the mount point and the fixture. */
class MountInSituView::DimensionOverlay : public QWidget
{
public:
    explicit DimensionOverlay(QWidget *parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
    }
    enum Shape { Can, Mover, Bar };
    QString text;
    QString faceTag;
    StructureStudioView *view = nullptr;
    QVector3D fixturePos, mountPos;
    bool flush = true;
    bool hung = true;
    bool behind = false;        ///< on the back face: drawn dashed, behind the structure
    bool edgeOn = false;        ///< on a side face: seen end-on, so as narrow as it is deep
    Shape shape = Can;
    double widthM = 0.2, heightM = 0.25, depthM = 0.2;
    QVector3D beam = QVector3D(0, 0, -1);

protected:
    void drawGlyph(QPainter &p, const QPointF &c, double pxPerM)
    {
        const double w = qMax(10.0, (edgeOn ? depthM : widthM) * pxPerM);
        const double h = qMax(12.0, heightM * pxPerM);
        const QColor ink(235, 238, 245), fill(52, 58, 70), lens(255, 230, 150);
        p.save();
        p.translate(c);
        if (!hung)
            p.scale(1.0, -1.0);         // standing: the mount is underneath
        QPen pen(ink, 1.6);
        if (behind)
            pen.setStyle(Qt::DashLine);
        p.setPen(pen);
        p.setBrush(fill);
        const double top = -h / 2.0;
        if (shape == Bar)
        {
            const double bh = qMax(8.0, h * 0.5);
            p.drawRoundedRect(QRectF(-w / 2.0, -bh / 2.0, w, bh), 2, 2);
            p.setPen(Qt::NoPen);
            p.setBrush(lens);
            for (int i = 0; i < 5; ++i)
                p.drawEllipse(QPointF(-w / 2.0 + w * (i + 0.5) / 5.0, bh / 2.0 - 2.5), 1.6, 1.6);
        }
        else
        {
            p.drawRect(QRectF(-w * 0.3, top - 4, w * 0.6, 4));          // the clamp plate
            if (shape == Mover)
            {
                p.setBrush(Qt::NoBrush);
                QPainterPath yoke;                                     // a U, open at the bottom
                yoke.moveTo(-w / 2.0, top + h * 0.75);
                yoke.lineTo(-w / 2.0, top);
                yoke.lineTo(w / 2.0, top);
                yoke.lineTo(w / 2.0, top + h * 0.75);
                p.drawPath(yoke);
                p.setBrush(fill);
                p.drawRoundedRect(QRectF(-w * 0.36, top + h * 0.2, w * 0.72, h * 0.8), 5, 5);
            }
            else
            {
                p.drawRoundedRect(QRectF(-w / 2.0, top, w, h), 3, 3);
            }
            p.setPen(QPen(lens, 3.0));
            p.drawLine(QPointF(-w * 0.3, h / 2.0 - 1.5), QPointF(w * 0.3, h / 2.0 - 1.5));   // the lens
        }
        p.restore();
    }

    void paintEvent(QPaintEvent *) override
    {
        if (view == nullptr)
            return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QFont f = font();
        f.setPointSizeF(qMax(7.0, f.pointSizeF() - 1.0));
        f.setBold(true);
        p.setFont(f);
        const QFontMetrics fm(f);
        const QPointF fs = view->worldToScreen(fixturePos);
        const QPointF ms = view->worldToScreen(mountPos);
        const double pxPerM = (view->worldToScreen(fixturePos + QVector3D(1, 0, 0)) - fs).x();
        const QColor line(255, 214, 90);

        // The beam: a cone and a stub about half a metre long; end-on when it
        // points into or out of the picture.
        const QPointF dir2(double(beam.x()), -double(beam.z()));
        const double len2 = std::hypot(dir2.x(), dir2.y());
        const double reach = 0.5 * pxPerM + 12.0;
        p.setPen(Qt::NoPen);
        if (len2 < 0.2)
        {
            p.setPen(QPen(line, 1.6));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(fs, 7, 7);
            p.setBrush(line);
            if (beam.y() > 0)                 // toward the audience: a dot
                p.drawEllipse(fs, 2.2, 2.2);
            else                              // away: a cross
            {
                p.drawLine(fs + QPointF(-4, -4), fs + QPointF(4, 4));
                p.drawLine(fs + QPointF(-4, 4), fs + QPointF(4, -4));
            }
        }
        else
        {
            const QPointF u(dir2.x() / len2, dir2.y() / len2);
            const QPointF nrm(-u.y(), u.x());
            const double start = qMax(heightM, widthM) * pxPerM * 0.5;
            const QPointF a = fs + u * start;
            const QPointF b = fs + u * (start + reach * len2);
            QPolygonF cone;
            cone << a << b + nrm * (reach * 0.22) << b - nrm * (reach * 0.22);
            p.setBrush(QColor(255, 220, 120, 70));
            p.drawPolygon(cone);
            p.setPen(QPen(line, 1.6));
            p.drawLine(a, b);
        }
        drawGlyph(p, fs, pxPerM);

        QRectF faceBox;
        if (!faceTag.isEmpty())
        {
            faceBox = QRectF(5, 5, fm.horizontalAdvance(faceTag) + 8, fm.height() + 2);
            p.fillRect(faceBox, QColor(0, 0, 0, 170));
            p.setPen(QColor(235, 238, 245));
            p.drawText(faceBox, Qt::AlignCenter, faceTag);
        }

        if (text.isEmpty())
            return;
        const int tw = fm.horizontalAdvance(text) + 8;
        if (flush)
        {
            // Beside the glyph, where the dimension would be.
            const QRectF box(qBound(4.0, fs.x() + 34.0, double(width() - tw - 4)),
                             fs.y() - fm.height() / 2.0, tw, fm.height() + 2);
            p.fillRect(box, QColor(0, 0, 0, 170));
            p.setPen(line);
            p.drawText(box, Qt::AlignCenter, text);
            return;
        }
        const double x = qBound(10.0, fs.x() + 34.0, double(width() - tw - 16));
        const double y1 = ms.y(), y2 = fs.y();
        p.setPen(QPen(line, 1.5));
        p.drawLine(QPointF(x, y1), QPointF(x, y2));
        p.drawLine(QPointF(x - 5, y1), QPointF(x + 5, y1));
        p.drawLine(QPointF(x - 5, y2), QPointF(x + 5, y2));
        const double d = (y2 > y1) ? 1.0 : -1.0;
        p.drawLine(QPointF(x, y2), QPointF(x - 3, y2 - 5 * d));
        p.drawLine(QPointF(x, y2), QPointF(x + 3, y2 - 5 * d));
        p.setPen(QPen(line, 1.0, Qt::DotLine));
        p.drawLine(QPointF(fs.x() + 12, y2), QPointF(x, y2));
        const QRectF box(x + 5, (y1 + y2) / 2.0 - fm.height() / 2.0 - 1, tw, fm.height() + 2);
        p.fillRect(box, QColor(0, 0, 0, 170));
        p.setPen(line);
        p.drawText(box, Qt::AlignCenter, text);
    }
};

namespace
{
enum { PlatformKind = StructureStudioView::PlatformKind };

QString faceName(FixtureRigProps::Face f)
{
    switch (f)
    {
    case FixtureRigProps::FaceTop:        return QObject::tr("Top");
    case FixtureRigProps::FaceBottom:     return QObject::tr("Bottom");
    case FixtureRigProps::FaceBack:       return QObject::tr("Back (upstage)");
    case FixtureRigProps::FaceStageLeft:  return QObject::tr("Stage left");
    case FixtureRigProps::FaceStageRight: return QObject::tr("Stage right");
    default:                              return QObject::tr("Front (downstage)");
    }
}
}

MountInSituView::MountInSituView(Doc *doc, quint32 fixtureId, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_fid(fixtureId)
{
    setObjectName(QStringLiteral("mountInSituView"));
    setMinimumSize(200, 120);
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(2);
    m_caption = new QLabel(this);
    m_caption->setObjectName(QStringLiteral("mountCaption"));
    m_caption->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    lay->addWidget(m_caption);
    refresh();
}

void MountInSituView::setFixtureId(quint32 fixtureId)
{
    m_fid = fixtureId;
    refresh();
}

QString MountInSituView::caption() const
{
    return m_caption->text();
}

QString MountInSituView::heightText() const
{
    return m_heightText;
}

bool MountInSituView::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == m_view && m_dim != nullptr && ev->type() == QEvent::Resize)
        m_dim->setGeometry(m_view->rect());
    return QWidget::eventFilter(obj, ev);
}

void MountInSituView::clearView()
{
    delete m_dim;
    m_dim = nullptr;
    delete m_view;
    m_view = nullptr;
    m_kind = -1;
    m_structId = 0;
    m_heightText.clear();
    m_full.clear();
}

void MountInSituView::refresh()
{
    MonitorProperties *props = m_doc->monitorProperties();
    const FixtureRigProps rp = props->fixtureRigProps(m_fid);

    // What it is on: pipe, tower, truss, then a riser or deck.
    int kind = -1;
    quint32 sid = 0;
    QString what, name;
    if (rp.onPipe() && props->pipe(rp.pipeId))
    {
        kind = StructureStudioView::PipeKind;
        sid = rp.pipeId;
        what = tr("pipe");
        name = props->pipe(sid)->name();
    }
    else if (rp.onTower() && props->tower(rp.towerId))
    {
        kind = StructureStudioView::TowerKind;
        sid = rp.towerId;
        what = tr("tower");
        name = props->tower(sid)->name();
    }
    else if (rp.trussId != Truss::invalidId() && props->truss(rp.trussId))
    {
        kind = StructureStudioView::TrussKind;
        sid = rp.trussId;
        what = tr("truss");
        name = props->truss(sid)->name();
    }
    else if (rp.onRiser() && props->platform(rp.riserPlatformId))
    {
        kind = PlatformKind;
        sid = rp.riserPlatformId;
        what = tr("riser");
        name = props->platform(sid)->name();
    }
    else if (rp.onDeck() && props->platform(rp.deckPlatformId))
    {
        kind = PlatformKind;
        sid = rp.deckPlatformId;
        what = tr("deck");
        name = props->platform(sid)->name();
    }

    if (kind < 0)
    {
        clearView();
        m_caption->setText(tr("Not on a structure ") + QChar(0x2014) + tr(" free on the plot"));
        setToolTip(QString());
        return;
    }

    if (m_view == nullptr || kind != m_kind || sid != m_structId)
    {
        clearView();
        m_kind = kind;
        m_structId = sid;
        m_view = new StructureStudioView(m_doc, StructureStudioView::Kind(kind), sid, this);
        m_view->setObjectName(QStringLiteral("mountStructureView"));
        m_view->setPlane(StructureStudioView::Front);
        m_view->setLocked(true);
        m_view->setLookOnly(true);
        m_view->setBeams(false);
        m_view->setDecorations(false);
        m_view->setMaxFrameRate(5);
        m_view->setFocusPolicy(Qt::NoFocus);
        m_view->setMinimumSize(40, 40);   // the picture gives way, the caption never does
        m_view->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        static_cast<QVBoxLayout *>(layout())->insertWidget(0, m_view, 1);
        m_dim = new DimensionOverlay(m_view);
        m_dim->view = m_view;
        m_dim->setGeometry(m_view->rect());
        m_view->installEventFilter(this);
        m_dim->show();
    }
    m_view->setHighlight({ m_fid });
    // Frame the mount around the fixture: a metre or so of structure either
    // side, the fixture big enough to read its face and which way up it is.
    const QVector3D at = props->fixtureRigPosition(m_fid);
    m_view->setFocus(at, 1.0);
    m_view->reload();

    // Height offset: a dimension line, in the person's unit.
    const double mm = double(rp.mountZOffset) * 1000.0;
    if (qAbs(mm) < 0.5)
        m_heightText = tr("flush with the %1").arg(what);
    else if (mm > 0)
        m_heightText = tr("%1 above the %2").arg(MeasurementUnits::formatLength(mm), what);
    else
        m_heightText = tr("%1 below the %2").arg(MeasurementUnits::formatLength(-mm), what);
    m_dim->fixturePos = at;
    m_dim->mountPos = at - QVector3D(0, 0, rp.mountZOffset);
    m_dim->flush = qAbs(mm) < 0.5;
    m_dim->hung = !AimSolver::floorMounted(rp);
    const FixtureRigProps::Face face = rp.face();
    m_dim->behind = (face == FixtureRigProps::FaceBack);
    m_dim->edgeOn = (face == FixtureRigProps::FaceStageLeft || face == FixtureRigProps::FaceStageRight);
    m_dim->faceTag = tr("face: %1").arg(faceName(face));
    m_dim->shape = MountInSituView::DimensionOverlay::Can;
    m_dim->widthM = 0.2; m_dim->heightM = 0.25; m_dim->depthM = 0.2;
    if (Fixture *fx = m_doc->fixture(m_fid))
    {
        if (fx->fixtureMode() != nullptr)
        {
            const QLCPhysical ph = fx->fixtureMode()->physical();
            if (ph.width() > 0)  m_dim->widthM = ph.width() / 1000.0;
            if (ph.height() > 0) m_dim->heightM = ph.height() / 1000.0;
            if (ph.depth() > 0)  m_dim->depthM = ph.depth() / 1000.0;
        }
        if (fx->fixtureDef() != nullptr)
        {
            const QLCFixtureDef::FixtureType t = fx->fixtureDef()->type();
            if (t == QLCFixtureDef::MovingHead || t == QLCFixtureDef::Scanner)
                m_dim->shape = MountInSituView::DimensionOverlay::Mover;
            else if (t == QLCFixtureDef::LEDBarBeams || t == QLCFixtureDef::LEDBarPixels)
                m_dim->shape = MountInSituView::DimensionOverlay::Bar;
        }
    }
    // Where it throws: the aimed direction of a fixed light, else its home
    // (straight down hung, straight up standing), the Aim and Beam dial's frame.
    QVector3D bd;
    if (!rp.fixedAimDirection(bd) && !AimSolver::beamDirection(rp, 0.0, 0.0, bd))
        bd = QVector3D(0, 0, m_dim->hung ? -1.0f : 1.0f);
    m_dim->beam = bd;
    m_dim->text = qAbs(mm) < 0.5 ? tr("flush") : MeasurementUnits::formatLength(qAbs(mm)) + (mm > 0 ? QString(QChar(0x20)) + QChar(0x2191) : QString(QChar(0x20)) + QChar(0x2193));
    m_dim->setToolTip(m_heightText);
    m_dim->update();

    // Orientation: hung below, upright on top, or centred on the run.
    QString orient;
    if (kind == StructureStudioView::TrussKind)
        orient = rp.trussMountSide == FixtureRigProps::TopMounted ? tr("upright on top")
               : rp.trussMountSide == FixtureRigProps::Centered   ? tr("centred on the run")
               : tr("hung below");
    else
        orient = rp.mountingType == Truss::FloorMounted ? tr("upright") : tr("hung");

    QString place;
    if (kind == StructureStudioView::TrussKind)
        place = tr("%1 along").arg(MeasurementUnits::formatMetres(double(rp.trussOffset)));
    else if (kind == StructureStudioView::TowerKind)
        place = rp.towerMountSide == FixtureRigProps::TowerTop ? tr("tower top")
              : rp.towerMountSide == FixtureRigProps::TowerBottom ? tr("tower base")
              : tr("shelf %1").arg(rp.towerShelf + 1);

    const QString sep = QString(QChar(0x20)) + QChar(0x00B7) + QChar(0x20);
    QStringList bits;
    bits << tr("On %1 %2").arg(what, name) << orient;
    QStringList more = bits;
    more << tr("face: %1").arg(faceName(rp.face()));
    if (!place.isEmpty())
        more << place;
    if (!qFuzzyIsNull(rp.studioAngle))
        more << tr("angle %1").arg(double(rp.studioAngle), 0, 'f', 0) + QChar(0x00B0);
    if (!qFuzzyIsNull(rp.studioTilt))
        more << tr("tilt %1").arg(double(rp.studioTilt), 0, 'f', 0) + QChar(0x00B0);
    more << m_heightText;
    m_full = bits.join(sep);
    m_caption->setText(m_caption->fontMetrics().elidedText(m_full, Qt::ElideRight, qMax(60, width() - 4)));
    setToolTip(more.join(QStringLiteral("\n")).replace(sep, QStringLiteral("\n")));
}

void MountInSituView::resizeEvent(QResizeEvent *ev)
{
    QWidget::resizeEvent(ev);
    if (!m_full.isEmpty() && m_view != nullptr)
        m_caption->setText(m_caption->fontMetrics().elidedText(m_full, Qt::ElideRight, qMax(60, width() - 4)));
}
