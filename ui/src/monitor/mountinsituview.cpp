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

/* The dimension line: drawn over the picture between the mount point and the
   fixture, where they really are on screen, with the formatted offset beside it. */
class MountInSituView::DimensionOverlay : public QWidget
{
public:
    explicit DimensionOverlay(QWidget *parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
    }
    QString text;
    StructureStudioView *view = nullptr;
    QVector3D fixturePos, mountPos;
    bool flush = true;

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (view == nullptr || text.isEmpty())
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
        const int tw = fm.horizontalAdvance(text) + 8;
        const QColor line(255, 214, 90);
        if (flush)
        {
            const QRectF box(width() - tw - 6, 6, tw, fm.height() + 2);
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
        const double dir = (y2 > y1) ? 1.0 : -1.0;
        p.drawLine(QPointF(x, y2), QPointF(x - 3, y2 - 5 * dir));
        p.drawLine(QPointF(x, y2), QPointF(x + 3, y2 - 5 * dir));
        // A leader from the dimension to the fixture's own height.
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
