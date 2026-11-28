/*
  Q Light Controller Plus
  plotsheet.cpp

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

#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QPagedPaintDevice>
#include <QFontMetricsF>
#include <QScrollBar>
#include <QPdfWriter>
#include <QPainter>
#include <QLocale>
#include <QFile>
#include <QHash>
#include <QMap>

#include <algorithm>
#include <climits>

#include "plotsheet.h"
#include "monitorgraphicsview.h"
#include "monitorfixtureitem.h"
#include "fixturevisualtraits.h"
#include "monitorproperties.h"
#include "measurementunits.h"
#include "qlcfixturedef.h"
#include "fixture.h"
#include "doc.h"

namespace
{
/* The page around the sheet, and the column the keys and title block
   share, in points. */
const double kMargin = 0.0;        // the device's own margins are the page's
const double kSideWidth = 198.0;
const double kGap = 10.0;

const QColor kInk(20, 20, 24);
const QColor kRule(120, 120, 128);
const QColor kSoft(96, 96, 104);

QFont sheetFont(int pixels, bool bold = false)
{
    QFont f;
    f.setPixelSize(pixels);         // the painter is in points: a pixel is a point
    f.setBold(bold);
    return f;
}

/* The owner overlay's colours (Monitor, ViewOwner): the tour package blue,
   the house rig amber, rentals violet. */
QColor ownerColour(RigOwner::Owner o)
{
    return o == RigOwner::House ? QColor(235, 170, 50)
         : o == RigOwner::Rental ? QColor(170, 110, 230)
                                 : QColor(70, 140, 230);
}

/* For the length of a paint: the plot looked at from the sheet's point of
   view, nothing selected, and put back afterwards -- the Studio is not
   changed by printing it. */
class ViewGuard
{
public:
    ViewGuard(MonitorGraphicsView *view, PlotSheet::View want) : m_view(view)
    {
        if (m_view == nullptr)
            return;
        m_pov = m_view->viewPOV();
        m_h = m_view->horizontalScrollBar()->value();
        m_v = m_view->verticalScrollBar()->value();
        const MonitorGraphicsView::ViewPOV pov =
            want == PlotSheet::FrontView ? MonitorGraphicsView::PovFront
          : want == PlotSheet::SideView  ? MonitorGraphicsView::PovSide
                                         : MonitorGraphicsView::PovTop;
        m_view->setViewPOV(pov);
        if (QGraphicsScene *s = m_view->scene())
        {
            m_selected = s->selectedItems();
            for (QGraphicsItem *it : std::as_const(m_selected))
                it->setSelected(false);
        }
    }
    ~ViewGuard()
    {
        if (m_view == nullptr)
            return;
        m_view->setViewPOV(m_pov);
        for (QGraphicsItem *it : std::as_const(m_selected))
            if (m_view->scene() != nullptr && m_view->scene()->items().contains(it))
                it->setSelected(true);
        m_view->horizontalScrollBar()->setValue(m_h);
        m_view->verticalScrollBar()->setValue(m_v);
    }
private:
    MonitorGraphicsView *m_view;
    MonitorGraphicsView::ViewPOV m_pov = MonitorGraphicsView::PovTop;
    QList<QGraphicsItem *> m_selected;
    int m_h = 0, m_v = 0;
};

QString typeOf(Fixture *fx)
{
    QLCFixtureDef *def = fx->fixtureDef();
    if (def == nullptr)
        return fx->typeString();
    const QString m = def->manufacturer().trimmed(), n = def->model().trimmed();
    if (m.isEmpty() || m.compare(QLatin1String("Generic"), Qt::CaseInsensitive) == 0)
    {
        // "Generic Generic" says nothing: the kind says what it is.
        if (n.isEmpty() || n.compare(QLatin1String("Generic"), Qt::CaseInsensitive) == 0)
            return QStringLiteral("Generic %1").arg(fx->typeString());
        return QStringLiteral("Generic %1").arg(n);
    }
    return n.isEmpty() ? m : QStringLiteral("%1 %2").arg(m, n);
}

/* Elide @p text to @p width on one line. */
QString fit(const QFont &f, const QString &text, double width)
{
    return QFontMetricsF(f).elidedText(text, Qt::ElideRight, width);
}
}

PlotSheet::PlotSheet(Doc *doc, const Context &context, const Options &options)
    : m_doc(doc)
    , m_context(context)
    , m_options(options)
{
    if (!canDraw(m_context, m_options.view))
        m_options.view = OverviewView;
    collect();
}

QString PlotSheet::viewName(View view)
{
    switch (view)
    {
        case FrontView:    return tr("Front elevation");
        case SideView:     return tr("Side elevation");
        case OverviewView: return tr("Rig Overview (angled)");
        case TopView:
        default:           return tr("Plan (top)");
    }
}

bool PlotSheet::canDraw(const Context &context, View view)
{
    // The angled view draws from the show alone; the others from the plot.
    return view == OverviewView || context.view != nullptr;
}

void PlotSheet::collect()
{
    MonitorGraphicsView *v = m_context.view;
    if (m_doc == nullptr)
        return;
    ViewGuard guard(v, m_options.view == OverviewView ? TopView : m_options.view);
    MonitorProperties *props = m_doc->monitorProperties();

    /* The key: one row per kind of fixture on the plot -- its definition and
       the symbol it is drawn with -- in the order of the most first. A
       fixture on a hidden layer is not drawn, so it is not counted; the
       sheet says how many that is. */
    QMap<QString, int> rowAt;
    QHash<quint32, int> perLayer;
    QHash<int, int> perOwner;
    const QHash<quint32, RigOwner::Owner> owners = RigOwner::effectiveAll(m_doc);
    // On the plot; without one (no Studio), every fixture placed in the show.
    QList<quint32> ids = v != nullptr ? v->fixtureItems().keys() : props->fixtureItemsID();
    std::sort(ids.begin(), ids.end());
    for (quint32 fid : std::as_const(ids))
    {
        Fixture *fx = m_doc->fixture(fid);
        if (fx == nullptr || (v != nullptr && v->fixtureItemForId(fid) == nullptr))
            continue;
        const quint32 layer = props->fixtureLayer(fid);
        if (!props->layer(layer).visible)
        {
            ++m_hidden;
            continue;
        }
        /* A light the plot shows as the count on its structure (it buries the
           truss, or Fixtures on the Rig is off) is still on the plot: it is
           counted, and the key says how many are drawn that way. */
        if (v != nullptr && !v->fixtureItemShouldBeVisible(fid))
        {
            if (props->fixtureRigProps(fid).primaryMount() == FixtureRigProps::NoMount)
                continue;
            ++m_asCount;
        }
        const Fixture::StageLook look = drawnStageLook(fx);
        const QString type = typeOf(fx);
        const QString lookName = look == Fixture::AutoLook ? QString() : Fixture::stageLookName(look);
        const QString key = type + QLatin1Char('\x1f') + lookName;
        if (!rowAt.contains(key))
        {
            rowAt.insert(key, m_symbols.size());
            KeyRow r;
            r.type = type;
            r.look = lookName;
            r.sampleFid = fid;
            m_symbols << r;
        }
        m_symbols[rowAt.value(key)].count++;
        perLayer[props->layer(layer).id]++;
        perOwner[int(owners.value(fid, RigOwner::Tour))]++;
    }
    std::stable_sort(m_symbols.begin(), m_symbols.end(), [](const KeyRow &a, const KeyRow &b) {
        if (a.count != b.count)
            return a.count > b.count;
        return a.type.localeAwareCompare(b.type) < 0;
    });

    for (const MonitorProperties::MonitorLayer &l : props->layers())
    {
        if (!perLayer.contains(l.id) && l.visible)
            continue;
        LayerRow r;
        r.name = l.name;
        r.fixtures = perLayer.value(l.id);
        r.visible = l.visible;
        if (r.fixtures > 0 || !r.visible)
            m_layers << r;
    }
    for (RigOwner::Owner o : RigOwner::owners())
    {
        if (perOwner.value(int(o)) == 0)
            continue;
        OwnerRow r;
        r.owner = o;
        r.count = perOwner.value(int(o));
        r.colour = ownerColour(o);
        m_owners << r;
    }

    /* What the drawing frames: the stage grid, and what sits off it (a FOH
       truss, a boom in the wings) as far as half a stage out. */
    if (v == nullptr)
        return;
    m_sceneUnitsPerMm = v->sceneUnitsPerMm();
    const QRectF grid = v->gridSceneRect();
    QRectF frame = grid;
    if (QGraphicsScene *s = v->scene())
    {
        const QRectF reach = grid.adjusted(-grid.width() / 2, -grid.height() / 2,
                                           grid.width() / 2, grid.height() / 2);
        for (QGraphicsItem *it : s->items())
        {
            if (it->parentItem() != nullptr || !it->isVisible())
                continue;
            const QRectF br = it->sceneBoundingRect();
            if (br.isEmpty() || !reach.contains(br))
                continue;
            frame |= br;
        }
    }
    const double pad = qMax(frame.width(), frame.height()) * 0.02;
    m_sceneRect = frame.adjusted(-pad, -pad, pad, pad);
}

QString PlotSheet::asCountNote() const
{
    return m_asCount == 1 ? tr("1 is drawn as the count on its structure")
                          : tr("%1 are drawn as the count on their structure").arg(m_asCount);
}

int PlotSheet::fixtureCount() const
{
    int n = 0;
    for (const KeyRow &r : m_symbols)
        n += r.count;
    return n;
}

QSizeF PlotSheet::pageSizePoints() const
{
    // Landscape, less 12 mm all round (as the patch list).
    const QSizeF p = QPageSize(m_options.page).size(QPageSize::Point);
    const double m = 12.0 / 25.4 * 72.0;
    return QSizeF(qMax(p.width(), p.height()) - 2 * m, qMin(p.width(), p.height()) - 2 * m);
}

PlotSheet::Layout PlotSheet::layout() const
{
    Layout l;
    const QSizeF page = pageSizePoints();
    l.side = QRectF(page.width() - kSideWidth - kMargin, kMargin, kSideWidth, page.height() - 2 * kMargin);
    l.drawing = QRectF(kMargin, kMargin, l.side.left() - kGap - kMargin, page.height() - 2 * kMargin);

    // Room under the plot for the scale bar.
    const QRectF room = l.drawing.adjusted(16, 16, -16, -40);
    if (m_options.view == OverviewView)
    {
        l.plot = room;
        return l;
    }
    if (m_sceneRect.isEmpty() || m_sceneUnitsPerMm <= 0)
        return l;
    const double k = qMin(room.width() / m_sceneRect.width(), room.height() / m_sceneRect.height());
    const QSizeF sz(m_sceneRect.width() * k, m_sceneRect.height() * k);
    l.plot = QRectF(room.center().x() - sz.width() / 2, room.top() + (room.height() - sz.height()) / 2,
                    sz.width(), sz.height());
    l.source = m_sceneRect;
    l.pointsPerMm = k * m_sceneUnitsPerMm;

    /* The bar: a round length in the person's unit, about a fifth of the
       frame -- whole feet, or metres in 1-2-5 steps. */
    const MeasurementUnits::LengthUnit unit = MeasurementUnits::preferredLength();
    const bool imperial = MeasurementUnits::isImperial(unit);
    const double want = l.drawing.width() / 5.0 / l.pointsPerMm;
    const double feet[] = { 1, 2, 5, 10, 20, 25, 50, 100, 200 };
    const double metres[] = { 0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, 100 };
    double best = imperial ? 304.8 : 100.0;
    if (imperial)
    {
        for (double f : feet)
            if (f * 304.8 <= want)
                best = f * 304.8;
    }
    else
    {
        for (double m : metres)
            if (m * 1000.0 <= want)
                best = m * 1000.0;
    }
    l.barMm = best;
    return l;
}

double PlotSheet::scaleBarMm() const
{
    return layout().barMm;
}

QString PlotSheet::scaleBarLabel() const
{
    const double mm = scaleBarMm();
    return mm > 0 ? MeasurementUnits::formatLength(mm) : QString();
}

QImage PlotSheet::overviewImage(const QSize &size) const
{
    StructureStudioView view(m_doc, StructureStudioView::StageKind, 0);
    view.setAttribute(Qt::WA_DontShowOnScreen);
    view.resize(size);
    if (m_context.haveCamera)
    {
        StructureStudioView::Camera c = m_context.camera;
        c.plane = StructureStudioView::Angled;
        view.setCamera(c);
    }
    else
    {
        view.setPlane(StructureStudioView::Angled);
    }
    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    view.render(&img);
    return img;
}

void PlotSheet::paintTo(QPagedPaintDevice *device) const
{
    QPainter p(device);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    // One unit = one point, from the top-left of the printable area.
    const QRectF area = device->pageLayout().paintRectPoints();
    // One scale both ways: a printer's paper may not be the sheet's.
    const double k = qMin(device->width() / area.width(), device->height() / area.height());
    p.scale(k, k);
    this->paint(&p);
}

bool PlotSheet::writePdf(const QString &path, QString *error) const
{
    {
        // QPdfWriter says nothing when it can't open the file; find out first.
        QFile probe(path);
        if (!probe.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            if (error != nullptr)
                *error = probe.errorString();
            return false;
        }
    }
    QPdfWriter writer(path);
    writer.setTitle(m_options.title);
    writer.setCreator(QStringLiteral("qlcconsole"));
    writer.setResolution(600);
    writer.setPageSize(QPageSize(m_options.page));
    writer.setPageOrientation(QPageLayout::Landscape);
    writer.setPageMargins(QMarginsF(12, 12, 12, 12), QPageLayout::Millimeter);
    paintTo(&writer);
    return true;
}

QImage PlotSheet::preview(int width) const
{
    const QSizeF page = pageSizePoints();
    const double pad = 12.0 / 25.4 * 72.0;
    const double k = width / (page.width() + 2 * pad);
    QImage img(QSize(width, int((page.height() + 2 * pad) * k)), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.scale(k, k);
    p.translate(pad, pad);
    paint(&p);
    return img;
}

void PlotSheet::paint(QPainter *p) const
{
    const Layout l = layout();
    p->save();
    paintDrawing(p, l);
    paintScaleBar(p, l);
    paintSide(p, l);
    p->restore();
}

void PlotSheet::paintDrawing(QPainter *p, const Layout &l) const
{
    MonitorGraphicsView *v = m_context.view;
    p->save();
    p->setPen(QPen(kInk, 1.0));
    p->setBrush(Qt::NoBrush);
    p->drawRect(l.drawing);

    // The plot on the Studio's own background, so it reads as it does there.
    const QRectF inner = l.drawing.adjusted(1, 1, -1, -1);
    p->fillRect(inner, v != nullptr ? v->backgroundBrush() : QBrush(QColor(11, 11, 11)));

    if (m_options.view == OverviewView)
    {
        const double k = 3.0;   // points to pixels: about 216 dpi on the page
        const QImage img = overviewImage(QSize(int(l.plot.width() * k), int(l.plot.height() * k)));
        p->drawImage(l.plot, img);
    }
    else if (v != nullptr && v->scene() != nullptr && !l.source.isEmpty())
    {
        ViewGuard guard(v, m_options.view);
        /* What keeps its size on screen whatever the zoom -- a structure's
           count badge, a network node's tag -- is drawn in DEVICE units:
           straight onto a 600 dpi PDF it shrank to a speck. For the length
           of the render each is scaled to half a point a unit (its size in
           the preview, about as on screen), then given its own back. */
        const double devPerPoint = qAbs(p->deviceTransform().m11()) + qAbs(p->deviceTransform().m12());
        const double grow = devPerPoint > 0 ? devPerPoint / 2.0 : 1.0;
        QList<QPair<QGraphicsItem *, QTransform> > fixedSize;
        if (qAbs(grow - 1.0) > 0.01)
        {
            for (QGraphicsItem *it : v->scene()->items())
                if (it->flags() & QGraphicsItem::ItemIgnoresTransformations)
                {
                    fixedSize << qMakePair(it, it->transform());
                    it->setTransform(it->transform() * QTransform::fromScale(grow, grow));
                }
        }
        p->save();
        p->setClipRect(inner);
        v->scene()->render(p, l.plot, l.source, Qt::KeepAspectRatio);
        p->restore();
        for (const QPair<QGraphicsItem *, QTransform> &f : std::as_const(fixedSize))
            f.first->setTransform(f.second);
    }
    else
    {
        p->setPen(QColor(200, 200, 200));
        p->setFont(sheetFont(12));
        p->drawText(inner, Qt::AlignCenter, tr("Nothing on the plot to draw."));
    }

    // Stage directions round the edge, as the plot has them.
    QString left, right, top, bottom;
    switch (m_options.view)
    {
        case FrontView:
            left = tr("stage right"); right = tr("stage left"); top = tr("up"); bottom = tr("floor");
            break;
        case SideView:
            left = tr("upstage"); right = tr("downstage"); top = tr("up"); bottom = tr("floor");
            break;
        case OverviewView:
            break;
        case TopView:
        default:
            left = tr("stage right"); right = tr("stage left");
            top = tr("upstage"); bottom = tr("downstage (audience)");
            break;
    }
    if (!top.isEmpty())
    {
        p->setPen(QColor(150, 180, 230));
        p->setFont(sheetFont(8));
        const QRectF a = inner.adjusted(4, 2, -4, -2);
        p->drawText(a, Qt::AlignHCenter | Qt::AlignTop, QStringLiteral("▲ ") + top);
        p->drawText(a, Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("◀ ") + left);
        p->drawText(a, Qt::AlignVCenter | Qt::AlignRight, right + QStringLiteral(" ▶"));
        p->drawText(a.adjusted(0, 0, 0, -26), Qt::AlignHCenter | Qt::AlignBottom,
                    QStringLiteral("▼ ") + bottom);
    }
    p->restore();
}

void PlotSheet::paintScaleBar(QPainter *p, const Layout &l) const
{
    p->save();
    p->setFont(sheetFont(8));
    const QRectF strip(l.drawing.left() + 12, l.drawing.bottom() - 26, l.drawing.width() - 24, 20);
    if (l.barMm <= 0 || l.pointsPerMm <= 0)
    {
        p->setPen(QColor(200, 200, 200));
        p->drawText(strip, Qt::AlignLeft | Qt::AlignVCenter, tr("Angled view: not to scale"));
        p->restore();
        return;
    }
    const double len = l.barMm * l.pointsPerMm;
    const QRectF bar(strip.left(), strip.top() + 2, len, 5);
    p->setPen(QPen(Qt::white, 0.6));
    for (int i = 0; i < 4; ++i)
    {
        const QRectF seg(bar.left() + i * len / 4, bar.top(), len / 4, bar.height());
        p->setBrush((i % 2) == 0 ? QBrush(Qt::white) : QBrush(QColor(11, 11, 11)));
        p->drawRect(seg);
    }
    p->setPen(Qt::white);
    const double ty = bar.bottom() + 1;
    p->drawText(QRectF(bar.left() - 20, ty, 40, 10), Qt::AlignHCenter | Qt::AlignTop, QStringLiteral("0"));
    p->drawText(QRectF(bar.center().x() - 40, ty, 80, 10), Qt::AlignHCenter | Qt::AlignTop,
                MeasurementUnits::formatLength(l.barMm / 2));
    p->drawText(QRectF(bar.right() - 40, ty, 80, 10), Qt::AlignHCenter | Qt::AlignTop,
                MeasurementUnits::formatLength(l.barMm));
    p->restore();
}

void PlotSheet::paintSymbol(QPainter *p, const QRectF &cell, quint32 fid) const
{
    MonitorGraphicsView *v = m_context.view;
    // Through QGraphicsItem: the item's own overrides are protected.
    QGraphicsItem *item = v != nullptr ? v->fixtureItemForId(fid) : nullptr;
    p->save();
    p->fillRect(cell, QColor(11, 11, 11));
    if (item != nullptr)
    {
        const QRectF br = item->boundingRect();
        if (br.width() > 0 && br.height() > 0)
        {
            const QRectF box = cell.adjusted(2, 2, -2, -2);
            const double k = qMin(box.width() / br.width(), box.height() / br.height());
            p->translate(box.center());
            p->scale(k, k);
            p->translate(-br.center());
            QStyleOptionGraphicsItem opt;
            opt.exposedRect = br;
            item->paint(p, &opt, nullptr);
        }
    }
    p->restore();
}

void PlotSheet::paintSide(QPainter *p, const Layout &l) const
{
    p->save();
    const QRectF col = l.side;
    double y = col.top();
    const QLocale locale;

    auto heading = [&](const QString &text) {
        p->setPen(kInk);
        const QFont f = sheetFont(10, true);
        p->setFont(f);
        p->drawText(QRectF(col.left(), y, col.width(), 14), Qt::AlignLeft | Qt::AlignVCenter, text);
        y += 15;
        p->setPen(QPen(kRule, 0.6));
        p->drawLine(QPointF(col.left(), y), QPointF(col.right(), y));
        y += 3;
    };

    /* The title block sits at the foot of the column, as on any drawing;
       the keys fill the space above it. */
    const double titleH = 150;
    const QRectF title(col.left(), col.bottom() - titleH, col.width(), titleH);
    const double keyBottom = title.top() - 8;

    if (m_options.symbolKey)
    {
        heading(tr("Symbol key"));
        const double rowH = 20;
        const QFont name = sheetFont(8, true), sub = sheetFont(7), count = sheetFont(10, true);
        const double reserve = m_options.layerKey
                ? 18 + 14 * (m_layers.size() + m_owners.size() + 2) : 0;
        const int room = qMax(1, int((keyBottom - reserve - y - 16) / rowH));
        const int shown = m_symbols.size() <= room ? m_symbols.size() : qMax(0, room - 1);
        for (int i = 0; i < shown; ++i)
        {
            const KeyRow &r = m_symbols.at(i);
            paintSymbol(p, QRectF(col.left(), y + 1, 18, 18), r.sampleFid);
            const QRectF text(col.left() + 24, y, col.width() - 24 - 34, rowH);
            p->setPen(kInk);
            p->setFont(name);
            p->drawText(text.adjusted(0, 1, 0, -rowH / 2), Qt::AlignLeft | Qt::AlignVCenter,
                        fit(name, r.type, text.width()));
            p->setPen(kSoft);
            p->setFont(sub);
            p->drawText(text.adjusted(0, rowH / 2 - 1, 0, 0), Qt::AlignLeft | Qt::AlignVCenter,
                        fit(sub, r.look.isEmpty() ? tr("drawn by kind") : r.look, text.width()));
            p->setPen(kInk);
            p->setFont(count);
            p->drawText(QRectF(col.right() - 34, y, 34, rowH), Qt::AlignRight | Qt::AlignVCenter,
                        locale.toString(r.count));
            y += rowH;
        }
        if (shown < m_symbols.size())
        {
            int rest = 0;
            for (int i = shown; i < m_symbols.size(); ++i)
                rest += m_symbols.at(i).count;
            p->setPen(kSoft);
            p->setFont(sub);
            p->drawText(QRectF(col.left() + 24, y, col.width() - 24, 14), Qt::AlignLeft | Qt::AlignVCenter,
                        tr("%1 more types: %2 fixtures").arg(m_symbols.size() - shown).arg(rest));
            y += 14;
        }
        if (m_asCount > 0)
        {
            p->setPen(kSoft);
            p->setFont(sub);
            p->drawText(QRectF(col.left(), y, col.width(), 14), Qt::AlignLeft | Qt::AlignVCenter,
                        fit(sub, asCountNote(), col.width()));
            y += 14;
        }
        p->setPen(QPen(kRule, 0.6));
        p->drawLine(QPointF(col.left(), y + 1), QPointF(col.right(), y + 1));
        p->setPen(kInk);
        p->setFont(sheetFont(9, true));
        p->drawText(QRectF(col.left(), y + 2, col.width() - 34, 14), Qt::AlignLeft | Qt::AlignVCenter,
                    tr("Total fixtures"));
        p->drawText(QRectF(col.right() - 34, y + 2, 34, 14), Qt::AlignRight | Qt::AlignVCenter,
                    locale.toString(fixtureCount()));
        y += 22;
    }

    if (m_options.layerKey && (!m_layers.isEmpty() || !m_owners.isEmpty()))
    {
        heading(tr("Layers and owners"));
        const QFont f = sheetFont(8), b = sheetFont(8, true);
        auto row = [&](const QString &label, const QString &value, const QColor &swatch, bool grey) {
            if (y + 13 > keyBottom)
                return;
            double x = col.left();
            if (swatch.isValid())
            {
                p->fillRect(QRectF(x, y + 3, 8, 8), swatch);
                x += 12;
            }
            p->setFont(f);
            p->setPen(grey ? kSoft : kInk);
            p->drawText(QRectF(x, y, col.right() - 60 - x, 13), Qt::AlignLeft | Qt::AlignVCenter,
                        fit(f, label, col.right() - 60 - x));
            p->setFont(b);
            p->drawText(QRectF(col.right() - 60, y, 60, 13), Qt::AlignRight | Qt::AlignVCenter, value);
            y += 13;
        };
        for (const LayerRow &r : m_layers)
            row(tr("Layer: %1").arg(r.name),
                r.visible ? locale.toString(r.fixtures) : tr("hidden"), QColor(), !r.visible);
        for (const OwnerRow &r : m_owners)
            row(RigOwner::displayName(r.owner), locale.toString(r.count),
                m_context.ownerColours ? r.colour : QColor(), false);
        if (m_hidden > 0)
            row(tr("Not drawn (on a hidden layer)"), locale.toString(m_hidden), QColor(), true);
    }

    // The title block.
    p->setPen(QPen(kInk, 1.0));
    p->setBrush(Qt::NoBrush);
    p->drawRect(title);
    double ty = title.top() + 6;
    p->setFont(sheetFont(14, true));
    const QString showTitle = m_options.title.isEmpty() ? tr("Lighting plot") : m_options.title;
    p->drawText(QRectF(title.left() + 6, ty, title.width() - 12, 34),
                Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, showTitle);
    ty += 38;
    p->setPen(QPen(kRule, 0.6));
    p->drawLine(QPointF(title.left(), ty), QPointF(title.right(), ty));
    ty += 3;
    const QFont lf = sheetFont(7), vf = sheetFont(8, true);
    auto field = [&](const QString &label, const QString &value) {
        p->setFont(lf);
        p->setPen(kSoft);
        p->drawText(QRectF(title.left() + 6, ty, 54, 13), Qt::AlignLeft | Qt::AlignVCenter, label);
        p->setFont(vf);
        p->setPen(kInk);
        p->drawText(QRectF(title.left() + 60, ty, title.width() - 66, 13), Qt::AlignLeft | Qt::AlignVCenter,
                    fit(vf, value.isEmpty() ? QStringLiteral("—") : value, title.width() - 66));
        ty += 13;
    };
    field(tr("Venue"), m_options.venue);
    field(tr("Drawn by"), m_options.drawnBy);
    field(tr("Date"), locale.toString(m_options.date, QLocale::ShortFormat));
    field(tr("Revision"), m_options.revision);
    field(tr("View"), viewName(m_options.view));
    field(tr("Scale"), l.barMm > 0 ? tr("fit to page: use the bar") : tr("not to scale"));
    field(tr("Sheet"), tr("1 of 1 · %1").arg(QPageSize(m_options.page).name()));
    p->setFont(sheetFont(6));
    p->setPen(kSoft);
    p->drawText(QRectF(title.left() + 6, title.bottom() - 11, title.width() - 12, 10),
                Qt::AlignRight | Qt::AlignVCenter, tr("drawn with qlcconsole"));
    p->restore();
}
