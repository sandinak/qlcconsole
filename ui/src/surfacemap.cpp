/*
  Q Light Controller Plus - qlcconsole
  surfacemap.cpp

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

#include <QPainterPath>
#include <QFileDialog>
#include <QMessageBox>
#include <QComboBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QPainter>
#include <QToolTip>
#include <QLabel>
#include <QTimer>
#include <QFrame>

#include "surfacemap.h"
#include "controlowners.h"
#include "inputoutputmap.h"
#include "qlcinputprofile.h"
#include "showfeatures.h"
#include "deskrecord.h"
#include "controlmap.h"
#include "surfacepages.h"
#include "pageset.h"
#include "inputpatch.h"
#include "universe.h"
#include "doc.h"

namespace
{
/** How long a press or a move stays lit on the map (ms). */
const int kHighlightMs = 900;
const int kMargin = 8;

/* The board is drawn as the hardware is: dark, whatever the app's theme. */
const QColor kBoard(0x26, 0x28, 0x2b);
const QColor kSectionText(0xb8, 0xbc, 0xc2);
const QColor kFree(0x3a, 0x3d, 0x42);
const QColor kFreeText(0x9a, 0x9f, 0xa6);
const QColor kShow(0x1f, 0x7a, 0x8c);
const QColor kBoardOwn(0x55, 0x5d, 0x6b);
const QColor kPage(0x5b, 0x4a, 0x9e);       // a surface page's control (Design): violet, apart from GO's green
const QColor kHighlight(0xff, 0xff, 0xff);

QColor roleColour(const QString &role)
{
    if (role == QLatin1String("go"))
        return QColor(0x1e, 0x9e, 0x3e);
    if (role == QLatin1String("back"))
        return QColor(0x2f, 0x6f, 0xd6);
    if (role == QLatin1String("stop"))
        return QColor(0xd3, 0x2f, 0x2f);
    if (role == QLatin1String("gm"))
        return QColor(0xd9, 0x8a, 0x00);
    return QColor();
}

DeskRecord::Role roleOfKey(const QString &key, bool *ok)
{
    *ok = true;
    for (DeskRecord::Role r : DeskRecord::roles())
        if (DeskRecord::roleKey(r) == key)
            return r;
    *ok = false;
    return DeskRecord::Stop;
}

/** "GO", "Back", "STOP", "GM": what the map writes on a role's control. */
QString roleWord(const QString &key)
{
    if (key == QLatin1String("gm"))
        return SurfaceMapView::tr("GM");
    bool ok = false;
    const DeskRecord::Role r = roleOfKey(key, &ok);
    return ok ? DeskRecord::roleName(r) : QString();
}

/** One owner as the tooltip says it. */
QString ownerLine(Doc *doc, const ControlOwners::Owner &o)
{
    QString line = QStringLiteral("%1 (%2)").arg(o.thing, o.where);
    if (o.kind == ControlOwners::Trigger)
        line += QStringLiteral(" · ") + ControlOwners::placeName(doc, o.place);
    return line;
}

/** @p f made small enough (not under 8 px) that @p text's longest word
 *  fits @p width: a long word is never cut off by the cell's edge. */
QFont fitted(QFont f, const QString &text, double width)
{
    QString longest;
    for (const QString &w : text.split(QLatin1Char(' '), Qt::SkipEmptyParts))
        if (w.size() > longest.size())
            longest = w;
    while (f.pixelSize() > 8 && QFontMetricsF(f).horizontalAdvance(longest) > width)
        f.setPixelSize(f.pixelSize() - 1);
    return f;
}

bool isShowKind(ControlOwners::Kind k)
{
    return k != ControlOwners::Desk && k != ControlOwners::Pmj && k != ControlOwners::Pad
           && k != ControlOwners::Keyboard;
}
}

/*****************************************************************************
 * SurfaceMapView
 *****************************************************************************/

SurfaceMapView::SurfaceMapView(Doc *doc, quint32 universe, const SurfaceLayout &layout, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_universe(universe)
    , m_layout(layout)
    , m_fade(new QTimer(this))
{
    setObjectName(QStringLiteral("surfaceMapView%1").arg(universe));
    setAccessibleName(tr("Map of the %1").arg(layout.name()));
    setMouseTracking(true);
    QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
    for (int i = 0; i < m_layout.controls().size(); ++i)
        m_cells << Cell();
    m_clock.start();
    // A highlight fades by itself; the timer runs only while one is lit.
    m_fade->setInterval(150);
    connect(m_fade, &QTimer::timeout, this, [this]() {
        bool lit = false;
        for (const Cell &c : std::as_const(m_cells))
            lit = lit || (c.movedAt >= 0 && m_clock.elapsed() - c.movedAt < kHighlightMs);
        if (!lit)
            m_fade->stop();
        update();
    });
    // The page on now labels the board: a page switch, another set, the mode.
    if (SurfacePages *pages = m_doc->controlMap() != nullptr ? m_doc->controlMap()->surfacePages() : nullptr)
    {
        connect(pages, &SurfacePages::pageChanged, this, [this](quint32 u) {
            if (u == m_universe)
                refresh();
        });
        connect(pages, &SurfacePages::setsChanged, this, &SurfaceMapView::refresh);
    }
    refresh();
}

QColor SurfaceMapView::pageColour(quint32 channel) const
{
    const int i = cellIndex(channel);
    return i < 0 || m_cells.at(i).group != Page ? QColor() : m_cells.at(i).colour;
}

double SurfaceMapView::scale() const
{
    const QSizeF board = m_layout.size();
    if (board.width() <= 0)
        return kMinCell;
    const double s = (width() - 2 * kMargin) / board.width();
    return qBound(double(kMinCell), s, double(kMaxCell));
}

int SurfaceMapView::heightForWidth(int w) const
{
    const QSizeF board = m_layout.size();
    if (board.width() <= 0)
        return 0;
    const double s = qBound(double(kMinCell), (w - 2 * kMargin) / board.width(), double(kMaxCell));
    return int(board.height() * s) + 2 * kMargin;
}

QSize SurfaceMapView::sizeHint() const
{
    const QSizeF board = m_layout.size();
    const int cell = (kMinCell + kMaxCell) / 2;
    return QSize(int(board.width() * cell) + 2 * kMargin, int(board.height() * cell) + 2 * kMargin);
}

QSize SurfaceMapView::minimumSizeHint() const
{
    // Never squeezed below a legible cell: the page scrolls instead.
    const QSizeF board = m_layout.size();
    return QSize(int(board.width() * kMinCell) + 2 * kMargin, int(board.height() * kMinCell) + 2 * kMargin);
}

QPointF SurfaceMapView::origin() const
{
    // Wider than the board at the largest cell: centred.
    const double used = m_layout.size().width() * scale();
    return QPointF(qMax(double(kMargin), (width() - used) / 2.0), kMargin);
}

QRectF SurfaceMapView::toWidget(const QRectF &grid) const
{
    const double s = scale();
    const QPointF o = origin();
    return QRectF(o.x() + grid.x() * s, o.y() + grid.y() * s, grid.width() * s, grid.height() * s);
}

QRect SurfaceMapView::cellRect(quint32 channel) const
{
    const int i = cellIndex(channel);
    if (i < 0)
        return QRect();
    return toWidget(m_layout.controls().at(i).rect).toRect();
}

void SurfaceMapView::setPageLabels(const QHash<quint32, QString> &labels)
{
    m_pageLabels = labels;
    refresh();
}

void SurfaceMapView::refresh()
{
    if (m_doc == nullptr)
        return;
    const QList<SurfaceLayout::Control> controls = m_layout.controls();
    QList<quint32> channels;
    for (const SurfaceLayout::Control &c : controls)
        channels << c.channel << c.also;
    // One walk of the show for the whole board (ControlOwners::ownersOn()).
    const QHash<quint32, QList<ControlOwners::Owner>> owners =
        ControlOwners::ownersOn(m_doc, m_universe, channels);
    ControlMap *cm = m_doc->controlMap();
    DeskRecord *desk = m_doc->deskRecord();
    InputOutputMap *iom = m_doc->inputOutputMap();
    SurfacePages *pages = cm != nullptr ? cm->surfacePages() : nullptr;
    const QHash<quint32, QString> pageLabels = pages != nullptr ? pages->labels(m_universe)
                                                                : QHash<quint32, QString>();
    const QString pageName = pages != nullptr ? pages->pageName(m_universe) : QString();

    for (int i = 0; i < controls.size(); ++i)
    {
        const SurfaceLayout::Control &c = controls.at(i);
        Cell &cell = m_cells[i];
        cell.group = Free;
        cell.role.clear();
        cell.job.clear();
        cell.tip.clear();
        cell.dim = false;
        cell.colour = QColor();

        QList<ControlOwners::Owner> mine;
        QList<quint32> hears = QList<quint32>() << c.channel << c.also;
        for (quint32 ch : hears)
            for (const ControlOwners::Owner &o : owners.value(ch))
            {
                bool seen = false;
                for (const ControlOwners::Owner &m : std::as_const(mine))
                    seen = seen || m.sameAs(o);
                if (!seen)
                    mine << o;
            }

        // The board's own role on it (seed false: the PMJ's Go is the Run tab's GO).
        QString builtIn;
        for (const SurfaceLayout::Role &r : m_layout.roles())
            if (!r.seed && c.hears(r.channel))
                builtIn = r.role;

        QStringList lines;
        lines << QStringLiteral("<b>%1</b>").arg(c.face.toHtmlEscaped());
        if (c.kind == SurfaceLayout::Indicator)
            lines << tr("A light on the board: not a knob or a button.");
        const ControlOwners::Owner *deskOwner = nullptr;
        const ControlOwners::Owner *boardOwner = nullptr;
        QList<const ControlOwners::Owner *> show;
        for (const ControlOwners::Owner &o : std::as_const(mine))
        {
            if (o.kind == ControlOwners::Desk)
                deskOwner = &o;
            else if (o.kind == ControlOwners::Pmj)
                boardOwner = &o;
            else if (isShowKind(o.kind))
                show << &o;
        }

        if (deskOwner != nullptr)
        {
            cell.group = Desk;
            cell.role = DeskRecord::roleKey(DeskRecord::Role(deskOwner->index));
            cell.job = roleWord(cell.role);
            lines << tr("This computer's %1: in every show.").arg(DeskRecord::roleName(DeskRecord::Role(deskOwner->index)));
            if (cell.role == QLatin1String("gm") && desk != nullptr && desk->grandMasterWaiting())
                lines << desk->grandMasterHint().toHtmlEscaped();
        }
        else if (pages != nullptr && pages->owns(m_universe, c.channel))
        {
            // Design: the page on now has it (Q332 a); a show job waits for Operate.
            cell.group = Page;
            cell.job = pageLabels.value(c.channel);
            const PageSet::Binding *b = pages->bindingFor(m_universe, c.channel);
            if (b != nullptr)
                cell.colour = QColor(PageSet::paramColour(b->param));
            lines << tr("Page %1: %2.").arg(pageName.toHtmlEscaped(), cell.job.toHtmlEscaped());
            if (b == nullptr)
                lines << tr("A page key: press it for its page.");
            else if (c.isFader())
                lines << tr("Sets it on the open look or the selected fixtures; a fader picks the value up "
                            "first.");
            else
                lines << tr("Turns it on the open look or the selected fixtures.");
            if (!show.isEmpty())
                lines << tr("In Operate it is the show's again.");
        }
        else if (boardOwner != nullptr || !builtIn.isEmpty() || !c.job.isEmpty())
        {
            cell.group = Board;
            cell.role = builtIn;
            cell.job = !builtIn.isEmpty() ? roleWord(builtIn) : c.job;
            lines << (c.job.isEmpty() && builtIn.isEmpty()
                          ? tr("The %1's own control: no job yet.").arg(m_layout.name())
                          : tr("The %1's own: %2.").arg(m_layout.name(), cell.job));
        }
        else if (!show.isEmpty())
        {
            cell.group = Show;
            cell.job = show.first()->thing;
            if (show.size() > 1)
                cell.job = tr("%1 +%2").arg(cell.job).arg(show.size() - 1);
            // A hit or trigger armed only at a place that is not live now: dimmer.
            bool allAway = cm != nullptr;
            for (const ControlOwners::Owner *o : std::as_const(show))
                allAway = allAway && o->kind == ControlOwners::Trigger && !cm->isPlaceLive(o->place);
            cell.dim = allAway;
            lines << tr("This show:");
        }
        else
        {
            cell.job = m_pageLabels.value(c.channel, pageLabels.value(c.channel));
            if (!cell.job.isEmpty())
                lines << tr("Page %1 in Design: %2.").arg(pageName.toHtmlEscaped(), cell.job.toHtmlEscaped());
            lines << tr("Free: assign it from the thing (right-click a cue list, cue or hit ▸ "
                        "Assign to Control…).");
        }
        // Every show owner is named, the board's and the desk's too.
        for (const ControlOwners::Owner *o : std::as_const(show))
            lines << ownerLine(m_doc, *o).toHtmlEscaped();
        if (cell.group != Show && !show.isEmpty())
            lines << tr("Kept from this control: %1").arg(show.first()->thing.toHtmlEscaped());
        if (cell.dim)
            lines << tr("Not armed here now.");

        uchar sent = 0;
        if (iom != nullptr && iom->lastFeedback(m_universe, c.channel, &sent))
            cell.lamp = sent;
        if (cell.lamp >= 0 && c.lamp != SurfaceLayout::NoLamp)
            lines << tr("Light: last sent %1").arg(cell.lamp);
        cell.tip = lines.join(QStringLiteral("<br>"));
    }
    update();
}

SurfaceMapView::Group SurfaceMapView::group(quint32 channel) const
{
    const int i = cellIndex(channel);
    return i < 0 ? Free : m_cells.at(i).group;
}

QString SurfaceMapView::jobText(quint32 channel) const
{
    const int i = cellIndex(channel);
    return i < 0 ? QString() : m_cells.at(i).job;
}

QString SurfaceMapView::toolTipFor(quint32 channel) const
{
    const int i = cellIndex(channel);
    return i < 0 ? QString() : m_cells.at(i).tip;
}

QString SurfaceMapView::role(quint32 channel) const
{
    const int i = cellIndex(channel);
    return i < 0 ? QString() : m_cells.at(i).role;
}

bool SurfaceMapView::isDim(quint32 channel) const
{
    const int i = cellIndex(channel);
    return i >= 0 && m_cells.at(i).dim;
}

bool SurfaceMapView::isHighlighted(quint32 channel) const
{
    const int i = cellIndex(channel);
    return i >= 0 && m_cells.at(i).movedAt >= 0 && m_clock.elapsed() - m_cells.at(i).movedAt < kHighlightMs;
}

int SurfaceMapView::value(quint32 channel) const
{
    const int i = cellIndex(channel);
    return i < 0 ? -1 : m_cells.at(i).value;
}

int SurfaceMapView::lamp(quint32 channel) const
{
    const int i = cellIndex(channel);
    return i < 0 ? -1 : m_cells.at(i).lamp;
}

void SurfaceMapView::noteInput(quint32 universe, quint32 channel, uchar value)
{
    if (universe != m_universe)
        return;
    // A Virtual Console page's bits are not the control's.
    const int i = cellIndex(channel & 0xFFFF);
    if (i < 0)
        return;
    m_cells[i].value = value;
    m_cells[i].movedAt = m_clock.elapsed();
    if (!m_fade->isActive())
        m_fade->start();
    update(toWidget(m_layout.controls().at(i).rect).toAlignedRect().adjusted(-4, -4, 4, 4));
}

void SurfaceMapView::noteLamp(quint32 universe, quint32 channel, uchar value)
{
    if (universe != m_universe)
        return;
    const int i = cellIndex(channel);
    if (i < 0 || m_cells.at(i).lamp == value)
        return;
    m_cells[i].lamp = value;
    update(toWidget(m_layout.controls().at(i).rect).toAlignedRect().adjusted(-4, -4, 4, 4));
}

bool SurfaceMapView::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip)
    {
        QHelpEvent *he = static_cast<QHelpEvent *>(event);
        const QList<SurfaceLayout::Control> controls = m_layout.controls();
        for (int i = 0; i < controls.size(); ++i)
        {
            if (toWidget(controls.at(i).rect).contains(he->pos()))
            {
                QString tip = m_cells.at(i).tip;
                if (m_cells.at(i).value >= 0 && (controls.at(i).isFader() || controls.at(i).turns()))
                    tip += QStringLiteral("<br>") + tr("Now: %1 %").arg(qRound(m_cells.at(i).value * 100.0 / 255.0));
                QToolTip::showText(he->globalPos(), tip, this, toWidget(controls.at(i).rect).toRect());
                return true;
            }
        }
        QToolTip::hideText();
        event->ignore();
        return true;
    }
    return QWidget::event(event);
}

void SurfaceMapView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double s = scale();
    const QRectF board = toWidget(QRectF(QPointF(0, 0), m_layout.size())).adjusted(-kMargin / 2, -kMargin / 2,
                                                                                   kMargin / 2, kMargin / 2);
    p.setPen(Qt::NoPen);
    p.setBrush(kBoard);
    p.drawRoundedRect(board, 8, 8);

    // The panel's printed section names, above their controls.
    QFont sf = font();
    sf.setPixelSize(qMax(10, int(s * 0.26)));
    p.setFont(sf);
    p.setPen(kSectionText);
    for (const SurfaceLayout::Section &sec : m_layout.sections())
    {
        const QRectF r = toWidget(QRectF(sec.rect.x(), sec.rect.y() - 0.5, qMax(sec.rect.width(), 1.0), 0.5));
        p.drawText(r.adjusted(3, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter,
                   p.fontMetrics().elidedText(sec.label, Qt::ElideRight, int(r.width()) - 3));
    }
    for (int i = 0; i < m_cells.size(); ++i)
        paintCell(p, i);
}

void SurfaceMapView::paintCell(QPainter &p, int i)
{
    const SurfaceLayout::Control &c = m_layout.controls().at(i);
    const Cell &cell = m_cells.at(i);
    const double s = scale();
    const QRectF r = toWidget(c.rect).adjusted(2, 2, -2, -2);
    const bool lit = cell.movedAt >= 0 && m_clock.elapsed() - cell.movedAt < kHighlightMs;

    QColor fill;
    QColor text = Qt::white;
    switch (cell.group)
    {
        case Desk: fill = roleColour(cell.role); break;
        case Board: fill = cell.role.isEmpty() ? kBoardOwn : roleColour(cell.role); break;
        case Show: fill = kShow; break;
        case Free: fill = kFree; text = kFreeText; break;
        case Page:
            fill = cell.colour.isValid() ? cell.colour : kPage;
            // Words dark on a light colour (white, amber).
            if (fill.lightness() > 150)
                text = QColor(0x1a, 0x1a, 0x1a);
            break;
    }
    if (!fill.isValid())
        fill = kBoardOwn;
    if (cell.dim)
    {
        fill = fill.darker(170);
        text = text.darker(140);
    }

    if (c.kind == SurfaceLayout::Indicator)
    {
        /* A light only, never a knob: a small lamp in a dark bezel, lit as
           last sent, with a glow when it is on. A knob is a big disc with an
           arc; this is none of that. */
        const double d = qMin(r.width(), r.height()) * 0.55;
        const QRectF dot(r.center().x() - d / 2, r.center().y() - d / 2, d, d);
        const bool on = cell.lamp > 0;
        const QColor lamp(0xff, 0xb0, 0x20);
        if (on)
        {
            QRadialGradient glow(dot.center(), d * 1.1);
            glow.setColorAt(0, QColor(lamp.red(), lamp.green(), lamp.blue(), 150));
            glow.setColorAt(1, QColor(lamp.red(), lamp.green(), lamp.blue(), 0));
            p.setPen(Qt::NoPen);
            p.setBrush(glow);
            p.drawEllipse(dot.center(), d * 1.1, d * 1.1);
        }
        p.setPen(QPen(QColor(0x15, 0x15, 0x15), 2));
        p.setBrush(on ? lamp : QColor(0x3a, 0x3a, 0x3a));
        p.drawEllipse(dot);
        return;
    }

    QPen border(Qt::NoPen);
    if (lit)
        border = QPen(kHighlight, 3);
    else if (cell.group == Desk)
        border = QPen(fill.lighter(150), 2);
    else if (cell.group == Board)
        border = QPen(QColor(0x9a, 0xa3, 0xb0), 1, Qt::DashLine);    // the board's own: locked
    p.setPen(border);
    p.setBrush(fill);

    QRectF textRect = r.adjusted(3, 2, -3, -2);
    if (c.turns())
    {
        // A knob or an encoder: a disc, its value as an arc.
        const double d = qMin(r.width(), r.height()) * 0.62;
        const QRectF disc(r.center().x() - d / 2, r.top() + 2, d, d);
        p.drawEllipse(disc);
        if (cell.value >= 0)
        {
            p.setPen(QPen(kHighlight, 2));
            const int span = -int(cell.value / 255.0 * 270 * 16);
            p.drawArc(disc.adjusted(-2, -2, 2, 2), 225 * 16, span);
        }
        textRect = QRectF(r.left(), disc.bottom(), r.width(), r.bottom() - disc.bottom());
    }
    else if (c.isFader())
    {
        // A fader: its slot, and the cap where the board last put it.
        p.drawRoundedRect(r, 4, 4);
        const bool vertical = c.kind == SurfaceLayout::Fader;
        const double v = cell.value >= 0 ? cell.value / 255.0 : 0.0;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 110));
        QRectF slot = vertical ? QRectF(r.center().x() - 3, r.top() + s * 0.9, 6, r.height() - s * 1.5)
                               : QRectF(r.left() + 8, r.center().y() + 4, r.width() - 16, 5);
        p.drawRoundedRect(slot, 3, 3);
        p.setBrush(lit ? kHighlight : QColor(0xe0, 0xe0, 0xe0));
        if (vertical)
            p.drawRect(QRectF(r.left() + r.width() * 0.2, slot.bottom() - v * slot.height() - 4, r.width() * 0.6, 8));
        else
            p.drawRect(QRectF(slot.left() + v * slot.width() - 4, slot.top() - 6, 8, 17));
        textRect = vertical ? QRectF(r.left() + 2, r.top() + 2, r.width() - 4, s * 0.85)
                            : QRectF(r.left() + 2, r.top() + 1, r.width() - 4, r.height() / 2);
    }
    else
    {
        p.drawRoundedRect(r, 4, 4);
    }

    // The light: what the app last sent the board.
    if (c.lamp != SurfaceLayout::NoLamp && cell.lamp >= 0)
    {
        QColor led;
        if (m_layout.lampsOnOff())
            led = cell.lamp > 0 ? QColor(0xe5, 0x39, 0x35) : QColor(0x20, 0x20, 0x20);   // on, as the device's own colour
        else if (c.lamp == SurfaceLayout::Rgb && m_layout.hasLampColours())
            led = m_layout.lampColour(uchar(cell.lamp));
        else
            led = cell.lamp > 0 ? QColor::fromHsv(45, 200, qBound(90, 90 + cell.lamp * 165 / 255, 255))
                                : QColor(0x20, 0x20, 0x20);
        const double d = qMax(6.0, s * 0.16);
        p.setPen(QPen(QColor(0, 0, 0, 160), 1));
        p.setBrush(led);
        p.drawEllipse(QRectF(r.right() - d - 2, r.top() + 2, d, d));
    }

    // The words: the role's in the role's look; else the job, the face small.
    p.setPen(text);
    QFont f = font();
    const bool roleLook = !cell.role.isEmpty() && (cell.group == Desk || cell.group == Board);
    QString words = cell.job;
    if (c.isFader() && cell.value >= 0 && lit)
        words = tr("%1 %").arg(qRound(cell.value * 100.0 / 255.0));
    if (roleLook)
    {
        f.setBold(true);
        f.setPixelSize(qMax(11, int(qMin(textRect.height() * 0.5, s * 0.34))));
        f = fitted(f, words, textRect.width());
        p.setFont(f);
        p.drawText(textRect, Qt::AlignCenter, words);
        if (cell.group == Desk && cell.role == QLatin1String("gm") && m_doc->deskRecord() != nullptr
            && m_doc->deskRecord()->grandMasterWaiting())
        {
            // Q316: the pick-up, beside the fader.
            QFont h = font();
            h.setPixelSize(qMax(9, int(s * 0.2)));
            p.setFont(h);
            p.drawText(QRectF(r.left(), r.bottom() - s * 0.9, r.width(), s * 0.8), Qt::AlignCenter | Qt::TextWordWrap,
                       tr("Pick up"));
        }
        return;
    }
    const int px = qMax(9, int(s * 0.2));
    f.setPixelSize(px);
    const QString face = c.face;
    // A job that is the control's own name ("Looks" on the PMJ's Looks key) once.
    if (!c.turns() && words.compare(face, Qt::CaseInsensitive) == 0)
        words.clear();
    if (c.turns())
    {
        // Under a knob: one line, its job if it has one, else its name.
        const QString one = words.isEmpty() ? face : words;
        f.setBold(!words.isEmpty());
        f = fitted(f, one, textRect.width());
        p.setFont(f);
        p.drawText(textRect, Qt::AlignHCenter | Qt::AlignTop,
                   QFontMetricsF(f).elidedText(one, Qt::ElideRight, textRect.width()));
        return;
    }
    if (words.isEmpty())
    {
        f = fitted(f, face, textRect.width());
        p.setFont(f);
        p.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, face);
        return;
    }
    // Two lines: the face small at the top, the job under it.
    QFont small = f;
    small.setPixelSize(qMax(8, px - 2));
    p.setFont(small);
    const QFontMetricsF sm(small);
    QRectF top(textRect.left(), textRect.top(), textRect.width(), sm.height());
    p.drawText(top, Qt::AlignLeft | Qt::AlignTop, sm.elidedText(face, Qt::ElideRight, top.width()));
    f.setBold(true);
    f = fitted(f, words, textRect.width());
    p.setFont(f);
    const QRectF rest(textRect.left(), top.bottom(), textRect.width(), textRect.bottom() - top.bottom());
    const QFontMetricsF fm(f);
    // Elided to the lines that fit; the tooltip has it whole.
    const int lines = qMax(1, int(rest.height() / fm.height()));
    QString shown = words;
    if (fm.boundingRect(rest, Qt::TextWordWrap, words).height() > rest.height() + 0.5)
        shown = fm.elidedText(words, Qt::ElideRight, rest.width() * lines - fm.averageCharWidth() * 2);
    p.drawText(rest, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, shown);
}

/*****************************************************************************
 * SurfaceMaps
 *****************************************************************************/

SurfaceMaps::SurfaceMaps(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_box(new QVBoxLayout(this))
{
    setObjectName(QStringLiteral("surfaceMaps"));
    setProperty("helpTopic", QStringLiteral("control-surfaces.md#the-surface-map"));
    m_box->setContentsMargins(0, 0, 0, 0);

    auto queueRebuild = [this]() {
        if (m_rebuildQueued)
            return;
        m_rebuildQueued = true;
        QTimer::singleShot(0, this, &SurfaceMaps::rebuild);
    };
    InputOutputMap *iom = m_doc->inputOutputMap();
    connect(iom, &InputOutputMap::inputPatchChanged, this, queueRebuild);
    connect(iom, &InputOutputMap::profileChanged, this, queueRebuild);
    connect(iom, &InputOutputMap::universeAdded, this, queueRebuild);
    connect(iom, &InputOutputMap::universeRemoved, this, queueRebuild);
    connect(m_doc, &Doc::loaded, this, queueRebuild);
    // The desk's roles: where GO is on each board, and the Set Up… a board lacks.
    connect(m_doc->deskRecord(), &DeskRecord::changed, this, queueRebuild);
    connect(m_doc->deskRecord(), &DeskRecord::grandMasterWaitingChanged, this, &SurfaceMaps::refresh);
    ControlMap *cm = m_doc->controlMap();
    connect(cm, &ControlMap::bindingsChanged, this, &SurfaceMaps::refresh);
    // The pages row: the set, the page on now, the show's offer.
    connect(cm->surfacePages(), &SurfacePages::pageChanged, this, &SurfaceMaps::refreshPages);
    connect(cm->surfacePages(), &SurfacePages::setsChanged, this, &SurfaceMaps::refreshPages);
    // Who has a control follows the GO and the selection (Q296): while shown.
    connect(cm, &ControlMap::lampsChanged, this, [this]() {
        if (isVisible())
            refresh();
    });
    connect(ShowFeatures::instance(), &ShowFeatures::changed, this, &SurfaceMaps::refresh);
    // Live: the board's own input, and what the app sends its lights.
    connect(iom, &InputOutputMap::inputValueChanged, this,
            [this](quint32 u, quint32 ch, uchar v) {
                if (SurfaceMapView *mv = m_views.value(u))
                    mv->noteInput(u, ch, v);
            });
    connect(iom, &InputOutputMap::feedbackSent, this,
            [this](quint32 u, quint32 ch, uchar v) {
                if (SurfaceMapView *mv = m_views.value(u))
                    mv->noteLamp(u, ch, v);
            });
    rebuild();
}

QList<quint32> SurfaceMaps::surfaceUniverses(Doc *doc)
{
    QList<quint32> out;
    InputOutputMap *iom = doc != nullptr ? doc->inputOutputMap() : nullptr;
    for (quint32 u = 0; iom != nullptr && u < iom->universesCount(); ++u)
    {
        InputPatch *ip = iom->inputPatch(u);
        // By its profile: a port's name ("USB2.0-MIDI") says nothing of the board.
        if (ip != nullptr && SurfaceLayout::forProfile(ip->profileName()).isValid())
            out << u;
    }
    return out;
}

QString SurfaceMaps::roleLine(quint32 universe) const
{
    QLabel *l = m_roleLines.value(universe);
    return l != nullptr ? l->text() : QString();
}

QStringList SurfaceMaps::fallbackLines() const
{
    return m_fallback;
}

QWidget *SurfaceMaps::buildBlock(quint32 universe, const SurfaceLayout &layout)
{
    QWidget *block = new QWidget(this);
    block->setObjectName(QStringLiteral("surfaceMapBlock%1").arg(universe));
    QVBoxLayout *v = new QVBoxLayout(block);
    v->setContentsMargins(0, 6, 0, 6);

    QLabel *title = new QLabel(QStringLiteral("<b>%1</b> · %2")
                                   .arg(layout.name().toHtmlEscaped(),
                                        tr("Universe %1").arg(universe + 1).toHtmlEscaped()), block);
    title->setObjectName(QStringLiteral("surfaceMapTitle%1").arg(universe));
    title->setTextFormat(Qt::RichText);
    v->addWidget(title);

    // Where this computer's fixed jobs are on this board, in one line.
    DeskRecord *desk = m_doc->deskRecord();
    const QString here = desk->identityOfUniverse(universe);
    QStringList parts;
    QList<DeskRecord::Role> missing;
    const QList<SurfaceLayout::Control> controls = layout.controls();
    auto faceOf = [&layout, &controls](quint32 ch) {
        const int i = layout.indexOf(ch);
        return i < 0 ? QStringLiteral("#%1").arg(ch) : controls.at(i).face;
    };
    for (DeskRecord::Role r : { DeskRecord::Go, DeskRecord::Back, DeskRecord::Stop, DeskRecord::GrandMaster })
    {
        const QString word = roleWord(DeskRecord::roleKey(r));
        const DeskRecord::Binding b = desk->binding(r);
        const quint32 builtIn = layout.roleChannel(DeskRecord::roleKey(r));
        bool builtInOnly = false;
        for (const SurfaceLayout::Role &lr : layout.roles())
            if (lr.role == DeskRecord::roleKey(r) && !lr.seed)
                builtInOnly = true;
        if (b.isSet() && !here.isEmpty() && b.identity == here)
        {
            QString part = tr("%1 is %2").arg(word, faceOf(b.channel));
            if (!b.on)
                part = tr("%1 is %2 (off on this computer)").arg(word, faceOf(b.channel));
            parts << part;
        }
        else if (builtInOnly && builtIn != 0xFFFFFFFF)
        {
            parts << tr("%1 is %2").arg(word, faceOf(builtIn));
        }
        else if (b.isSet())
        {
            parts << tr("%1 is on %2").arg(word, b.device);
        }
        else
        {
            missing << r;
        }
    }
    /* This board's own defaults that went to another board's learned role:
       say they are free here (a board is set up once; Learn… moves one). */
    QStringList yielded;
    for (const SurfaceLayout::Role &lr : layout.roles())
    {
        bool ok = false;
        const DeskRecord::Role r = roleOfKey(lr.role, &ok);
        const DeskRecord::Binding b = desk->binding(r);
        if (ok && lr.seed && b.isSet() && b.identity != here)
        {
            const QString face = faceOf(lr.channel);
            yielded << (face.compare(roleWord(lr.role), Qt::CaseInsensitive) == 0
                            ? face : tr("%1 (%2)").arg(face, roleWord(lr.role)));
        }
    }
    QLabel *roles = new QLabel(parts.join(QStringLiteral(" · ")), block);
    roles->setObjectName(QStringLiteral("surfaceMapRoles%1").arg(universe));
    roles->setWordWrap(true);
    QFont bf = roles->font();
    bf.setBold(true);
    roles->setFont(bf);
    v->addWidget(roles);
    m_roleLines.insert(universe, roles);
    if (!yielded.isEmpty())
    {
        QLabel *free = new QLabel(tr("This board's own %1 stay free here: this computer has those roles "
                                     "on another board. Learn… above moves one here.")
                                      .arg(yielded.join(QStringLiteral(", "))), block);
        free->setObjectName(QStringLiteral("surfaceMapYielded%1").arg(universe));
        free->setWordWrap(true);
        v->addWidget(free);
    }

    for (DeskRecord::Role r : std::as_const(missing))
    {
        QHBoxLayout *h = new QHBoxLayout();
        QLabel *none = new QLabel(tr("No %1 on this computer yet.").arg(roleWord(DeskRecord::roleKey(r))), block);
        none->setObjectName(QStringLiteral("surfaceMapMissing%1_%2").arg(universe).arg(DeskRecord::roleKey(r)));
        QPushButton *btn = new QPushButton(tr("Set Up %1…").arg(roleWord(DeskRecord::roleKey(r))), block);
        btn->setObjectName(QStringLiteral("surfaceMapSetUp%1_%2").arg(universe).arg(DeskRecord::roleKey(r)));
        btn->setToolTip(tr("Learn this computer's %1: press the control on the board.").arg(DeskRecord::roleName(r)));
        connect(btn, &QPushButton::clicked, this, [this, r]() { emit deskLearnRequested(int(r)); });
        h->addWidget(none);
        h->addWidget(btn);
        h->addStretch(1);
        v->addLayout(h);
    }

    // The legend: the four looks.
    QHBoxLayout *legend = new QHBoxLayout();
    legend->setSpacing(12);
    struct Key { QColor colour; QString text; };
    const QList<Key> keys = {
        { roleColour(QStringLiteral("go")), tr("This computer's desk (GO, Back, STOP, GM)") },
        { kBoardOwn, tr("The board's own") },
        { kShow, tr("This show") },
        { kFree, tr("Free") },
    };
    QList<Key> shown = keys;
    if (m_doc->controlMap()->surfacePages()->pageSet(universe).isValid())
        shown.insert(1, Key{ kPage, tr("This page (Design)") });
    for (const Key &k : std::as_const(shown))
    {
        QLabel *swatch = new QLabel(block);
        swatch->setFixedSize(14, 14);
        swatch->setStyleSheet(QStringLiteral("background: %1; border-radius: 3px;").arg(k.colour.name()));
        QLabel *t = new QLabel(k.text, block);
        legend->addWidget(swatch);
        legend->addWidget(t);
    }
    legend->addStretch(1);
    v->addLayout(legend);
    if (layout.displayLines() > 0)
    {
        QLabel *screen = new QLabel(tr("Its screen (%1 lines of %2 characters) shows: %3.")
                                        .arg(layout.displayLines()).arg(layout.displayChars())
                                        .arg(layout.displayShows().join(QStringLiteral(" / "))), block);
        screen->setObjectName(QStringLiteral("surfaceMapScreen%1").arg(universe));
        screen->setWordWrap(true);
        v->addWidget(screen);
    }
    addPagesRow(v, block, universe);
    QLabel *hint = new QLabel(tr("Press or move a control on the board: it lights here. Hover a control for "
                                 "what it does and where."), block);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: gray;"));
    v->addWidget(hint);

    SurfaceMapView *mv = new SurfaceMapView(m_doc, universe, layout, block);
    v->addWidget(mv);
    m_views.insert(universe, mv);
    return block;
}

void SurfaceMaps::rebuild()
{
    m_rebuildQueued = false;
    for (QWidget *w : std::as_const(m_parts))
        w->deleteLater();
    m_parts.clear();
    m_views.clear();
    m_roleLines.clear();
    m_pageLines.clear();
    m_offers.clear();
    m_setBoxes.clear();
    m_blocks.clear();
    m_fallback.clear();
    while (QLayoutItem *it = m_box->takeAt(0))
        delete it;

    InputOutputMap *iom = m_doc->inputOutputMap();
    for (quint32 u = 0; u < iom->universesCount(); ++u)
    {
        InputPatch *ip = iom->inputPatch(u);
        if (ip == nullptr || ip->profileName().isEmpty())
            continue;
        const SurfaceLayout layout = SurfaceLayout::forProfile(ip->profileName());
        if (!layout.isValid())
        {
            // No drawing: the board's jobs stay in the rows below.
            m_fallback << tr("%1 on Universe %2: no drawing of this board yet. What its controls do is in "
                             "the rows below.").arg(ip->profileName()).arg(u + 1);
            continue;
        }
        QWidget *block = buildBlock(u, layout);
        m_box->addWidget(block);
        m_parts << block;
        m_blocks.insert(u, block);
    }
    if (!m_fallback.isEmpty())
    {
        QLabel *l = new QLabel(m_fallback.join(QStringLiteral("\n")), this);
        l->setObjectName(QStringLiteral("surfaceMapFallback"));
        l->setWordWrap(true);
        l->setStyleSheet(QStringLiteral("color: gray;"));
        m_box->addWidget(l);
        m_parts << l;
    }
    setVisible(!m_parts.isEmpty());
    refreshPages();
}

void SurfaceMaps::addPagesRow(QVBoxLayout *v, QWidget *block, quint32 universe)
{
    SurfacePages *pages = m_doc->controlMap()->surfacePages();
    const PageSet set = pages->pageSet(universe);
    if (!set.isValid())
        return;
    QHBoxLayout *h = new QHBoxLayout();
    QLabel *word = new QLabel(tr("Pages:"), block);
    QComboBox *box = new QComboBox(block);
    box->setObjectName(QStringLiteral("surfacePageSet%1").arg(universe));
    box->setToolTip(tr("The page set this board uses on this computer: labelled pages of jobs for its "
                       "controls, in Design."));
    box->setAccessibleName(tr("Page set"));
    m_setBoxes.insert(universe, box);
    QPushButton *exp = new QPushButton(tr("Export…"), block);
    exp->setObjectName(QStringLiteral("surfacePagesExport%1").arg(universe));
    exp->setToolTip(tr("Save this page set as a .pageset.json file, to share or to edit."));
    QPushButton *imp = new QPushButton(tr("Import…"), block);
    imp->setObjectName(QStringLiteral("surfacePagesImport%1").arg(universe));
    imp->setToolTip(tr("Add a .pageset.json file made for this board, and use it here."));
    h->addWidget(word);
    h->addWidget(box);
    h->addWidget(exp);
    h->addWidget(imp);
    h->addStretch(1);
    v->addLayout(h);

    QLabel *line = new QLabel(block);
    line->setObjectName(QStringLiteral("surfacePageLine%1").arg(universe));
    line->setWordWrap(true);
    v->addWidget(line);
    m_pageLines.insert(universe, line);

    // The show's own set, offered when this computer uses another (Q333 b).
    QWidget *offer = new QWidget(block);
    offer->setObjectName(QStringLiteral("surfacePageOffer%1").arg(universe));
    QHBoxLayout *oh = new QHBoxLayout(offer);
    oh->setContentsMargins(0, 0, 0, 0);
    QLabel *otext = new QLabel(offer);
    otext->setObjectName(QStringLiteral("surfacePageOfferText%1").arg(universe));
    otext->setWordWrap(true);
    QPushButton *use = new QPushButton(tr("Use It"), offer);
    use->setObjectName(QStringLiteral("surfacePageOfferUse%1").arg(universe));
    QPushButton *notNow = new QPushButton(tr("Not Now"), offer);
    notNow->setObjectName(QStringLiteral("surfacePageOfferNo%1").arg(universe));
    oh->addWidget(otext, 1);
    oh->addWidget(use);
    oh->addWidget(notNow);
    v->addWidget(offer);
    m_offers.insert(universe, offer);

    connect(box, &QComboBox::activated, this, [this, universe, box](int i) {
        m_doc->controlMap()->surfacePages()->chooseSet(universe, box->itemData(i).toString());
    });
    connect(use, &QPushButton::clicked, this,
            [this, universe]() { m_doc->controlMap()->surfacePages()->useShowSet(universe); });
    connect(notNow, &QPushButton::clicked, this,
            [this, universe]() { m_doc->controlMap()->surfacePages()->declineShowSet(universe); });
    connect(exp, &QPushButton::clicked, this, [this, universe, block]() {
        const PageSet s = m_doc->controlMap()->surfacePages()->pageSet(universe);
        const QString name = QStringLiteral("%1.%2.pageset.json")
                                 .arg(QString(s.profileName()).replace(QLatin1Char(' '), QLatin1Char('-')), s.name());
        const QString path = QFileDialog::getSaveFileName(block, tr("Export Page Set"),
                                                          QDir::home().absoluteFilePath(name),
                                                          tr("Page sets (*.pageset.json)"));
        if (path.isEmpty())
            return;
        QString why;
        if (!exportPageSet(universe, path, &why))
            QMessageBox::warning(block, tr("Export Page Set"), why);
    });
    connect(imp, &QPushButton::clicked, this, [this, universe, block]() {
        const QString path = QFileDialog::getOpenFileName(block, tr("Import Page Set"), QDir::homePath(),
                                                          tr("Page sets (*.pageset.json)"));
        if (path.isEmpty())
            return;
        QString why;
        if (!importPageSet(universe, path, &why))
            QMessageBox::warning(block, tr("Import Page Set"),
                                 tr("This page set can't be used on this board:\n%1").arg(why));
    });
}

QString SurfaceMaps::pageLine(quint32 universe) const
{
    const QLabel *l = m_pageLines.value(universe);
    return l != nullptr ? l->text() : QString();
}

QString SurfaceMaps::offerLine(quint32 universe) const
{
    const QWidget *w = m_offers.value(universe);
    if (w == nullptr || w->isHidden())
        return QString();
    const QLabel *l = w->findChild<QLabel *>(QStringLiteral("surfacePageOfferText%1").arg(universe));
    return l != nullptr ? l->text() : QString();
}

bool SurfaceMaps::exportPageSet(quint32 universe, const QString &path, QString *error) const
{
    const PageSet s = m_doc->controlMap()->surfacePages()->pageSet(universe);
    if (!s.isValid())
    {
        if (error != nullptr)
            *error = tr("This board has no page set.");
        return false;
    }
    return s.save(path, error);
}

bool SurfaceMaps::importPageSet(quint32 universe, const QString &path, QString *error)
{
    SurfacePages *pages = m_doc->controlMap()->surfacePages();
    const PageSet here = pages->pageSet(universe);
    const PageSet incoming = PageSet::fromFile(path, error);
    if (!incoming.isValid())
        return false;
    if (here.isValid() && incoming.profileName() != here.profileName())
    {
        if (error != nullptr)
            *error = tr("It is made for the %1, not this board.").arg(incoming.profileName());
        return false;
    }
    PageSet added;
    if (!PageSet::importFile(path, error, &added))
        return false;
    pages->chooseSet(universe, added.name());
    return true;
}

void SurfaceMaps::refreshPages()
{
    SurfacePages *pages = m_doc->controlMap()->surfacePages();
    for (auto it = m_pageLines.begin(); it != m_pageLines.end(); ++it)
    {
        const quint32 u = it.key();
        const PageSet set = pages->pageSet(u);
        if (QComboBox *box = m_setBoxes.value(u))
        {
            box->blockSignals(true);
            box->clear();
            for (const PageSet &s : PageSet::forProfile(set.profileName()))
                box->addItem(s.isBuiltIn() ? tr("%1 (built in)").arg(s.name()) : s.name(), s.name());
            box->setCurrentIndex(qMax(0, box->findData(set.name())));
            box->blockSignals(false);
        }
        QStringList keys;
        const SurfaceLayout layout = SurfaceLayout::forProfile(set.profileName());
        auto face = [&layout](quint32 ch) {
            const int i = layout.indexOf(ch);
            return i < 0 ? QString() : layout.controls().at(i).face;
        };
        for (const PageSet::Page &p : set.pages())
            if (p.key != 0xFFFFFFFF && !keys.contains(face(p.key)))
                keys << face(p.key);
        QString how;
        if (!keys.isEmpty())
            how = tr("%1 pick the page").arg(keys.join(QStringLiteral(", ")));
        if (set.nextKey() != 0xFFFFFFFF)
            how += (how.isEmpty() ? QString() : QStringLiteral("; ")) +
                   tr("%1 and %2 step").arg(face(set.previousKey()), face(set.nextKey()));
        if (set.stepper() != 0xFFFFFFFF)
            how += (how.isEmpty() ? QString() : QStringLiteral("; ")) + tr("turn %1 to step").arg(face(set.stepper()));
        QString text = tr("On now: <b>%1</b>").arg(pages->pageText(u).toHtmlEscaped());
        if (!how.isEmpty())
            text += QStringLiteral(" (%1)").arg(how.toHtmlEscaped());
        text += m_doc->mode() == Doc::Design
                    ? tr(". In Design a page edits the open look or the selected fixtures.")
                    : tr(". In Operate the controls are the show's: pages wait for Design.");
        it.value()->setTextFormat(Qt::RichText);
        it.value()->setText(text);
        // A hand-edited set the checker refused: said, not used.
        const QStringList refused = PageSet::refusedFor(set.profileName());
        for (const QString &r : refused)
            text += QStringLiteral("<br><span style=\"color:#c0392b\">%1</span>").arg(r.toHtmlEscaped());
        if (QWidget *offer = m_offers.value(u))
        {
            const QString name = pages->offeredSet(u);
            offer->setVisible(!name.isEmpty());
            if (QLabel *l = offer->findChild<QLabel *>(QStringLiteral("surfacePageOfferText%1").arg(u)))
                l->setText(tr("This show was built with the page set %1 on this board.").arg(name));
        }
    }
}

void SurfaceMaps::refresh()
{
    for (SurfaceMapView *mv : std::as_const(m_views))
        mv->refresh();
}

void SurfaceMaps::showMap(quint32 universe)
{
    if (m_rebuildQueued)
        rebuild();
    QWidget *block = m_blocks.value(universe);
    if (block == nullptr)
        return;
    for (QWidget *w = parentWidget(); w != nullptr; w = w->parentWidget())
        if (QScrollArea *scroll = qobject_cast<QScrollArea *>(w))
        {
            // Its top at the top: the name, where GO is, the legend, then the board.
            if (scroll->widget() != nullptr)
                scroll->verticalScrollBar()->setValue(block->mapTo(scroll->widget(), QPoint(0, 0)).y());
            break;
        }
    if (SurfaceMapView *mv = m_views.value(universe))
        mv->setFocus();
}
