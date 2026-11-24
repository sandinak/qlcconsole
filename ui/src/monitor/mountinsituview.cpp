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

/* The dimension line: drawn over the picture, down its right edge, from the
   mount to the fixture, with the formatted offset beside it. */
class MountInSituView::DimensionOverlay : public QWidget
{
public:
    explicit DimensionOverlay(QWidget *parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
    }
    QString text;

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (text.isEmpty())
            return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QColor ink = palette().color(QPalette::WindowText);
        QFont f = font();
        f.setPointSizeF(qMax(7.0, f.pointSizeF() - 1.0));
        p.setFont(f);
        const QFontMetrics fm(f);
        const int w = fm.horizontalAdvance(text) + 8;
        const QRect box(width() - w - 14, 4, w, fm.height() + 2);
        p.fillRect(box, QColor(0, 0, 0, 150));
        p.setPen(QColor(255, 255, 255));
        p.drawText(box, Qt::AlignCenter, text);
        // The line: ticks top and bottom, arrow heads inward.
        const int x = width() - 7;
        const int y1 = box.bottom() + 4, y2 = height() - 8;
        if (y2 - y1 < 12)
            return;
        p.setPen(QPen(ink, 1.4));
        p.drawLine(x, y1, x, y2);
        p.drawLine(x - 4, y1, x + 4, y1);
        p.drawLine(x - 4, y2, x + 4, y2);
        p.drawLine(x, y1, x - 3, y1 + 5);
        p.drawLine(x, y1, x + 3, y1 + 5);
        p.drawLine(x, y2, x - 3, y2 - 5);
        p.drawLine(x, y2, x + 3, y2 - 5);
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
    lay->addStretch(1);
    m_caption = new QLabel(this);
    m_caption->setObjectName(QStringLiteral("mountCaption"));
    m_caption->setWordWrap(true);
    m_caption->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
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
        m_view->setMaxFrameRate(5);
        m_view->setFocusPolicy(Qt::NoFocus);
        m_view->setMinimumSize(40, 40);   // the picture gives way, the caption never does
        m_view->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        static_cast<QVBoxLayout *>(layout())->insertWidget(0, m_view, 1);
        m_dim = new DimensionOverlay(m_view);
        m_dim->setGeometry(m_view->rect());
        m_view->installEventFilter(this);
        m_dim->show();
    }
    m_view->setHighlight({ m_fid });
    m_view->reload();

    // Height offset: a dimension line, in the person's unit.
    const double mm = double(rp.mountZOffset) * 1000.0;
    if (qAbs(mm) < 0.5)
        m_heightText = tr("flush with the %1").arg(what);
    else if (mm > 0)
        m_heightText = tr("%1 above the %2").arg(MeasurementUnits::formatLength(mm), what);
    else
        m_heightText = tr("%1 below the %2").arg(MeasurementUnits::formatLength(-mm), what);
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

    QStringList bits;
    bits << tr("On %1 %2").arg(what, name) << orient << tr("face: %1").arg(faceName(rp.face()));
    if (!place.isEmpty())
        bits << place;
    if (!qFuzzyIsNull(rp.studioAngle))
        bits << tr("angle %1").arg(double(rp.studioAngle), 0, 'f', 0) + QChar(0x00B0);
    if (!qFuzzyIsNull(rp.studioTilt))
        bits << tr("tilt %1").arg(double(rp.studioTilt), 0, 'f', 0) + QChar(0x00B0);
    m_caption->setText(bits.join(QString(QChar(0x20)) + QChar(0x00B7) + QChar(0x20)));
}
