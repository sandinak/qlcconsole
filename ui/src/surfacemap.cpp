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
#include <cmath>
#include <QFileDialog>
#include <QMessageBox>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QCollator>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <QLabel>
#include <QTimer>
#include <QFrame>

#include "surfacemap.h"
#include "surfacekeylabels.h"
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
#include "showsettingsdialog.h"
#include "showoperate.h"
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
            // A board that asks for Shift: said on the control itself.
            if (desk != nullptr && cell.role == QLatin1String("stop") && desk->shiftGuardsStop(m_universe))
            {
                cell.job = tr("Shift+%1").arg(cell.job);
                lines << tr("Held with Shift it stops everything (ALL OFF). Alone it does nothing: the "
                            "footer says so. Connections ▸ Control surfaces can turn this off.");
            }
            else if (desk != nullptr && cell.role == QLatin1String("go") && desk->shiftGuardsRelease(m_universe))
                lines << tr("After a stop, Shift+%1 releases it: alone it does not.").arg(c.face.toHtmlEscaped());
            if (cell.role == QLatin1String("gm") && desk != nullptr && desk->grandMasterWaiting())
                lines << desk->grandMasterHint().toHtmlEscaped();
        }
        else if (desk != nullptr && c.channel == m_layout.shiftChannel() && desk->hasShift(m_universe))
        {
            cell.group = Board;
            cell.job = tr("Shift");
            lines << tr("The %1's Shift key: held with STOP it stops everything, held with GO it "
                        "releases a stop.").arg(m_layout.name().toHtmlEscaped());
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
    emit refreshed();
}

QList<SurfaceMapView::Row> SurfaceMapView::rows() const
{
    QList<Row> out;
    SurfacePages *pages = m_doc->controlMap() != nullptr ? m_doc->controlMap()->surfacePages() : nullptr;
    DeskRecord *desk = m_doc->deskRecord();
    const QList<SurfaceLayout::Control> controls = m_layout.controls();
    for (int i = 0; i < controls.size(); ++i)
    {
        const SurfaceLayout::Control &c = controls.at(i);
        const Cell &cell = m_cells.at(i);
        Row r;
        r.channel = c.channel;
        r.control = c.face;
        r.job = cell.job;
        r.group = cell.group;
        if (pages != nullptr && pages->pageSet(m_universe).isValid())
        {
            if (cell.group == Page && pages->bindingFor(m_universe, c.channel) != nullptr)
                r.page = pages->pageName(m_universe);
            else if (!pages->keyJob(m_universe, c.channel).isEmpty())
                r.page = tr("Every page");
        }
        if (c.lamp != SurfaceLayout::NoLamp)
            r.lamp = cell.lamp < 0 ? QString() : cell.lamp > 0 ? tr("On") : tr("Off");
        if (c.isFader() && (cell.group == Page || (cell.role == QLatin1String("gm") && desk != nullptr)))
            r.picksUp = tr("Yes");
        out << r;
    }
    return out;
}

PageSet SurfaceMapView::pageSet() const
{
    SurfacePages *pages = m_doc->controlMap() != nullptr ? m_doc->controlMap()->surfacePages() : nullptr;
    return pages != nullptr ? pages->pageSet(m_universe) : PageSet();
}

QString SurfaceMapView::jobTextNoPage(quint32 channel) const
{
    const int i = cellIndex(channel);
    return i < 0 || m_cells.at(i).group == Page ? QString() : m_cells.at(i).job;
}

void SurfaceMapView::setSelected(qint64 channel)
{
    if (channel >= 0 && cellIndex(quint32(channel)) < 0)
        channel = -1;
    if (channel >= 0)
        channel = m_layout.controls().at(cellIndex(quint32(channel))).channel;
    if (channel == m_selected)
        return;
    m_selected = channel;
    update();
    emit selectionChanged(m_selected);
}

qint64 SurfaceMapView::controlAt(const QPoint &pos) const
{
    const QList<SurfaceLayout::Control> controls = m_layout.controls();
    for (int i = 0; i < controls.size(); ++i)
        if (toWidget(controls.at(i).rect).contains(pos))
            return controls.at(i).channel;
    return -1;
}

void SurfaceMapView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        const qint64 ch = controlAt(event->position().toPoint());
        if (ch >= 0)
        {
            setSelected(ch);
            emit controlClicked(quint32(ch), event->globalPosition().toPoint());
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

SurfaceMapView::Group SurfaceMapView::group(quint32 channel) const
{
    const int i = cellIndex(channel);
    return i < 0 ? Free : m_cells.at(i).group;
}

QString SurfaceMapView::wordsOf(int i) const
{
    return i < 0 || i >= m_cells.size() ? QString() : m_cells.at(i).job;
}

QString SurfaceMapView::valueTextOf(int i) const
{
    if (i < 0 || i >= m_cells.size() || m_cells.at(i).value < 0)
        return QString();
    return tr("%1 %").arg(qRound(m_cells.at(i).value * 100.0 / 255.0));
}

QString SurfaceMapView::drawnWords(quint32 channel) const
{
    const int i = cellIndex(channel);
    if (i < 0)
        return QString();
    const QString w = wordsOf(i);
    return w.isEmpty() ? m_layout.controls().at(i).face : w;
}

QString SurfaceMapView::valueText(quint32 channel) const
{
    const int i = cellIndex(channel);
    return i >= 0 && m_layout.controls().at(i).isFader() ? valueTextOf(i) : QString();
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

    // The panel's printed groups: an outline round them, the name on its top edge.
    QFont sf = font();
    sf.setPixelSize(qMax(9, int(s * 0.17)));
    sf.setBold(true);
    sf.setCapitalization(QFont::AllUppercase);
    sf.setLetterSpacing(QFont::PercentageSpacing, 104);
    for (const SurfaceLayout::Section &sec : m_layout.sections())
    {
        const QRectF r = toWidget(sec.rect);
        p.setPen(QPen(QColor(0xff, 0xff, 0xff, 46), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r, 6, 6);
        if (sec.label.isEmpty())
            continue;
        p.setFont(sf);
        if (p.fontMetrics().horizontalAdvance(sec.label) > r.width() - 14)
        {
            // Narrower than its name: the name wraps inside the outline, the way a panel prints it.
            p.setPen(kSectionText);
            QFont small = sf;
            small.setPixelSize(qMax(8, int(s * 0.12)));
            small.setLetterSpacing(QFont::PercentageSpacing, 100);
            p.setFont(small);
            p.drawText(r.adjusted(2, 3, -2, 0), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, sec.label);
            continue;
        }
        const QString text = p.fontMetrics().elidedText(sec.label, Qt::ElideRight, int(r.width()) - 14);
        const QRectF chip(r.left() + 6, r.top() - p.fontMetrics().height() / 2.0,
                          p.fontMetrics().horizontalAdvance(text) + 8, p.fontMetrics().height());
        p.setPen(Qt::NoPen);
        p.setBrush(kBoard);
        p.drawRect(chip);
        p.setPen(kSectionText);
        p.drawText(chip, Qt::AlignCenter, text);
    }
    for (int i = 0; i < m_cells.size(); ++i)
        paintCell(p, i);
    // The control the table has selected.
    const int sel = m_selected >= 0 ? cellIndex(quint32(m_selected)) : -1;
    if (sel >= 0)
    {
        p.setPen(QPen(QColor(0xff, 0xd3, 0x4d), 3));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(toWidget(m_layout.controls().at(sel).rect).adjusted(0, 0, 0, 0), 5, 5);
    }
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
        // A knob or an encoder: a round cap filling its place, its value as an arc round it.
        const double d = qMin(r.width(), r.height()) - 4;
        const QRectF disc(r.center().x() - d / 2, r.center().y() - d / 2, d, d);
        p.drawEllipse(disc);
        if (cell.value >= 0)
        {
            p.setPen(QPen(kHighlight, 2));
            const int span = -int(cell.value / 255.0 * 270 * 16);
            p.drawArc(disc.adjusted(-2, -2, 2, 2), 225 * 16, span);
            const double ang = (225 - cell.value / 255.0 * 270) * M_PI / 180.0;
            p.drawLine(disc.center(), disc.center() + QPointF(std::cos(ang), -std::sin(ang)) * d * 0.42);
        }
        textRect = disc.adjusted(d * 0.12, d * 0.12, -d * 0.12, -d * 0.12);
        if (d < 34)
            textRect = QRectF();
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
    else if (c.shape == SurfaceLayout::Round)
    {
        const double d = qMin(r.width(), r.height());
        const QRectF disc(r.center().x() - d / 2, r.center().y() - d / 2, d, d);
        p.drawEllipse(disc);
        textRect = d < 30 ? QRectF() : disc.adjusted(d * 0.1, d * 0.1, -d * 0.1, -d * 0.1);
    }
    else
    {
        const double rad = qMin(4.0, qMin(r.width(), r.height()) / 3);
        p.drawRoundedRect(r, rad, rad);
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
        const double d = qMin(qMax(5.0, s * 0.16), r.height() * 0.45);
        p.setPen(QPen(QColor(0, 0, 0, 160), 1));
        p.setBrush(led);
        p.drawEllipse(QRectF(r.right() - d - 2, r.top() + 2, d, d));
    }

    // The words: the role's in the role's look; else the job, the face small.
    p.setPen(text);
    QFont f = font();
    const bool roleLook = !cell.role.isEmpty() && (cell.group == Desk || cell.group == Board);
    // The control's words stay (bench 2026-11-27: a moved fader lost its name
    // to its percentage); the value is its own small number, under the slot.
    const QString words = wordsOf(i);
    const QString valueWords = valueTextOf(i);
    if (!valueWords.isEmpty() && c.isFader()
        && !(cell.group == Desk && cell.role == QLatin1String("gm") && m_doc->deskRecord() != nullptr
             && m_doc->deskRecord()->grandMasterWaiting()))
    {
        QFont vf = font();
        vf.setPixelSize(qMax(9, int(s * 0.2)));
        p.setFont(vf);
        const bool vertical = c.kind == SurfaceLayout::Fader;
        const QRectF at = vertical ? QRectF(r.left(), r.bottom() - s * 0.55, r.width(), s * 0.5)
                                   : QRectF(r.left() + 2, r.bottom() - s * 0.4, r.width() - 4, s * 0.38);
        p.drawText(at, Qt::AlignCenter, valueWords);
        p.setPen(text);
    }
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
    QString jobWords = words;
    if (!c.turns() && jobWords.compare(face, Qt::CaseInsensitive) == 0)
        jobWords.clear();
    if (c.turns())
    {
        // Under a knob: one line, its job if it has one, else its name.
        QString one = jobWords.isEmpty() ? face : jobWords;
        // A small cap with a long name: its number is enough ("Knob 3" is "3").
        if (QFontMetricsF(f).horizontalAdvance(one) > textRect.width() && jobWords.isEmpty()
            && one.section(QLatin1Char(' '), -1).toInt() > 0)
            one = one.section(QLatin1Char(' '), -1);
        f.setBold(!jobWords.isEmpty());
        f = fitted(f, one, textRect.width());
        p.setFont(f);
        p.drawText(textRect, Qt::AlignCenter,
                   QFontMetricsF(f).elidedText(one, Qt::ElideRight, textRect.width()));
        return;
    }
    if (textRect.isEmpty())
        return;     // too small to write on: the tooltip and the table say it
    if (textRect.height() < px * 2.4 && !jobWords.isEmpty())
    {
        // A thin key: one line, the job.
        f.setBold(true);
        f = fitted(f, jobWords, textRect.width());
        p.setFont(f);
        p.drawText(textRect, Qt::AlignCenter, QFontMetricsF(f).elidedText(jobWords, Qt::ElideRight, textRect.width()));
        return;
    }
    if (jobWords.isEmpty())
    {
        f = fitted(f, face, textRect.width());
        p.setFont(f);
        if (textRect.height() < px * 2.4)
        {
            // A thin key: one line, cut with an ellipsis, never through a letter.
            p.drawText(textRect, Qt::AlignCenter, QFontMetricsF(f).elidedText(face, Qt::ElideRight, textRect.width()));
            return;
        }
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
    f = fitted(f, jobWords, textRect.width());
    p.setFont(f);
    const QRectF rest(textRect.left(), top.bottom(), textRect.width(), textRect.bottom() - top.bottom());
    const QFontMetricsF fm(f);
    // Elided to the lines that fit; the tooltip has it whole.
    const int lines = qMax(1, int(rest.height() / fm.height()));
    QString shown = jobWords;
    if (fm.boundingRect(rest, Qt::TextWordWrap, jobWords).height() > rest.height() + 0.5)
        shown = fm.elidedText(jobWords, Qt::ElideRight, rest.width() * lines - fm.averageCharWidth() * 2);
    p.drawText(rest, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, shown);
}


/*****************************************************************************
 * SurfaceControlTable
 *****************************************************************************/

namespace
{
/** A cell that sorts by what a person reads: "Clip 1-2" before "Clip 1-10". */
class SortItem final : public QTableWidgetItem
{
public:
    using QTableWidgetItem::QTableWidgetItem;
    bool operator<(const QTableWidgetItem &other) const override
    {
        static QCollator collator = []() {
            QCollator c;
            c.setNumericMode(true);
            c.setCaseSensitivity(Qt::CaseInsensitive);
            return c;
        }();
        // The layout's own order breaks a tie, so a sort is stable.
        const int cmp = collator.compare(text(), other.text());
        return cmp != 0 ? cmp < 0 : data(Qt::UserRole + 1).toInt() < other.data(Qt::UserRole + 1).toInt();
    }
};
}

QStringList SurfaceControlTable::columns()
{
    return { tr("Control"), tr("Page"), tr("Job"), tr("Lamp"), tr("Picks up") };
}

SurfaceControlTable::SurfaceControlTable(SurfaceMapView *view, QWidget *parent)
    : QWidget(parent)
    , m_view(view)
    , m_table(new QTableWidget(this))
    , m_filter(new QLineEdit(this))
{
    setObjectName(QStringLiteral("surfaceTable%1").arg(view->universe()));
    QVBoxLayout *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(4);
    m_filter->setObjectName(QStringLiteral("surfaceTableFilter%1").arg(view->universe()));
    m_filter->setPlaceholderText(tr("Filter the controls (a name, a job, a page)"));
    m_filter->setClearButtonEnabled(true);
    m_filter->setAccessibleName(tr("Filter the controls"));
    v->addWidget(m_filter);

    m_table->setObjectName(QStringLiteral("surfaceTableView%1").arg(view->universe()));
    m_table->setColumnCount(columns().size());
    m_table->setHorizontalHeaderLabels(columns());
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);      // the job has the room
    m_table->setColumnWidth(0, 170);
    m_table->setColumnWidth(1, 110);
    m_table->setColumnWidth(3, 70);
    m_table->setColumnWidth(4, 90);
    m_table->horizontalHeader()->setSortIndicatorShown(true);
    m_table->setMinimumHeight(120);
    m_table->setMaximumHeight(220);
    m_table->setAccessibleName(tr("What each control does"));
    v->addWidget(m_table);

    connect(m_filter, &QLineEdit::textChanged, this, [this](const QString &t) { setFilter(t); });
    // A person's sort is kept when the rows are read again.
    connect(m_table->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this, [this]() {
        if (!m_syncing)
            m_sorted = m_table->horizontalHeader()->sortIndicatorSection() >= 0;
    });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &SurfaceControlTable::rowSelected);
    connect(m_view, &SurfaceMapView::refreshed, this, &SurfaceControlTable::rebuild);
    connect(m_view, &SurfaceMapView::selectionChanged, this, &SurfaceControlTable::selectionFromMap);
    // The lights change faster than the jobs: read them once a second, while shown.
    QTimer *t = new QTimer(this);
    t->setInterval(1000);
    connect(t, &QTimer::timeout, this, [this]() { if (isVisible()) rebuild(); });
    t->start();
    rebuild();
}

void SurfaceControlTable::rebuild()
{
    const QList<SurfaceMapView::Row> rows = m_view->rows();
    QHeaderView *h = m_table->horizontalHeader();
    const int sortCol = h->sortIndicatorSection();
    const Qt::SortOrder order = h->sortIndicatorOrder();
    const bool sorted = m_sorted;
    m_syncing = true;
    m_table->setSortingEnabled(false);
    m_table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i)
    {
        const SurfaceMapView::Row &r = rows.at(i);
        const QStringList cells = { r.control, r.page, r.job.isEmpty() ? tr("Free") : r.job, r.lamp, r.picksUp };
        for (int c = 0; c < cells.size(); ++c)
        {
            QTableWidgetItem *it = m_table->item(i, c);
            if (it == nullptr)
            {
                it = new SortItem();
                m_table->setItem(i, c, it);
            }
            it->setText(cells.at(c));
            it->setData(Qt::UserRole, r.channel);
            it->setData(Qt::UserRole + 1, i);
            if (c == 2 && r.job.isEmpty())
                it->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
            else
                it->setForeground(palette().color(QPalette::Text));
        }
    }
    m_table->setSortingEnabled(true);
    if (sorted)
        m_table->sortByColumn(sortCol, order);
    else
        m_table->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    m_syncing = false;
    applyFilter();
    selectionFromMap(m_view->selected());
}

void SurfaceControlTable::applyFilter()
{
    const QString needle = m_filterText.trimmed();
    for (int r = 0; r < m_table->rowCount(); ++r)
    {
        bool match = needle.isEmpty();
        for (int c = 0; c < m_table->columnCount() && !match; ++c)
            match = m_table->item(r, c) != nullptr
                    && m_table->item(r, c)->text().contains(needle, Qt::CaseInsensitive);
        m_table->setRowHidden(r, !match);
    }
}

int SurfaceControlTable::visibleRows() const
{
    int n = 0;
    for (int r = 0; r < m_table->rowCount(); ++r)
        n += m_table->isRowHidden(r) ? 0 : 1;
    return n;
}

int SurfaceControlTable::rowOfChannel(quint32 channel) const
{
    for (int r = 0; r < m_table->rowCount(); ++r)
        if (m_table->item(r, 0) != nullptr && m_table->item(r, 0)->data(Qt::UserRole).toUInt() == channel)
            return m_table->isRowHidden(r) ? -1 : r;
    return -1;
}

QString SurfaceControlTable::text(int row, int col) const
{
    const QTableWidgetItem *it = m_table->item(row, col);
    return it != nullptr ? it->text() : QString();
}

void SurfaceControlTable::rowSelected()
{
    if (m_syncing)
        return;
    const QList<QTableWidgetItem *> sel = m_table->selectedItems();
    if (sel.isEmpty())
        return;
    m_syncing = true;
    m_view->setSelected(sel.first()->data(Qt::UserRole).toUInt());
    m_syncing = false;
}

void SurfaceControlTable::selectionFromMap(qint64 channel)
{
    if (m_syncing)
        return;
    m_syncing = true;
    if (channel < 0)
    {
        m_table->clearSelection();
    }
    else
    {
        const int row = rowOfChannel(quint32(channel));
        if (row >= 0)
        {
            m_table->selectRow(row);
            m_table->scrollToItem(m_table->item(row, 0));
        }
        else
        {
            m_table->clearSelection();
        }
    }
    m_syncing = false;
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
    // A guard changed (here, or a show loaded): the boxes and the jobs follow.
    connect(cm, &ControlMap::shiftGuardsChanged, this, [this]() {
        for (auto it = m_guardBoxes.constBegin(); it != m_guardBoxes.constEnd(); ++it)
        {
            const QSignalBlocker b1(it.value().first), b2(it.value().second);
            it.value().first->setChecked(m_doc->deskRecord()->shiftGuardsStop(it.key()));
            it.value().second->setChecked(m_doc->deskRecord()->shiftGuardsRelease(it.key()));
        }
        refresh();
    });
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

    addOptionsRow(v, block, universe, layout);

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
    QLabel *hint = new QLabel(tr("Press or move a control on the board: it lights here. Click a control, or "
                                 "its row, to select it; clicking the drawing offers the control a job."), block);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: gray;"));
    v->addWidget(hint);

    SurfaceMapView *mv = new SurfaceMapView(m_doc, universe, layout, block);
    QPushButton *labels = new QPushButton(tr("Print Key Labels…"), block);
    labels->setObjectName(QStringLiteral("surfaceKeyLabels%1").arg(universe));
    labels->setToolTip(tr("A sheet of cut-out labels at the board's key sizes, or tape labels for a Brother "
                          "P-touch CUBE (TZe tape in its three widths), with what each key does on each page."));
    connect(labels, &QPushButton::clicked, this, [this, mv, block]() {
        SurfaceKeyLabelsDialog dlg(mv, block);
        dlg.exec();
    });
    QHBoxLayout *tools = new QHBoxLayout();
    tools->addWidget(labels);
    tools->addStretch(1);
    v->addLayout(tools);
    SurfaceControlTable *table = new SurfaceControlTable(mv, block);
    v->addWidget(table);
    v->addWidget(mv);
    m_views.insert(universe, mv);
    m_tables.insert(universe, table);
    connect(mv, &SurfaceMapView::controlClicked, this,
            [this, universe](quint32 ch, const QPoint &pos) { showJobMenu(universe, ch, pos); });
    return block;
}

void SurfaceMaps::rebuild()
{
    m_rebuildQueued = false;
    for (QWidget *w : std::as_const(m_parts))
        w->deleteLater();
    m_parts.clear();
    m_views.clear();
    m_tables.clear();
    m_roleLines.clear();
    m_pageLines.clear();
    m_offers.clear();
    m_setBoxes.clear();
    m_guardBoxes.clear();
    m_allButtons.clear();
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
    applyFilter();
    refreshPages();
}

/* The board's options, in place (a footer chip's click lands here): its
   controller setup, a lamp test where it has one, and the Shift guards of a
   board that has a Shift key. The guards are the show's (saved with it). */
void SurfaceMaps::addOptionsRow(QVBoxLayout *v, QWidget *block, quint32 universe, const SurfaceLayout &layout)
{
    DeskRecord *desk = m_doc->deskRecord();
    ControlMap *cm = m_doc->controlMap();
    QHBoxLayout *h = new QHBoxLayout();
    QPushButton *setUp = new QPushButton(tr("Set Up…"), block);
    setUp->setObjectName(QStringLiteral("surfaceMapOptSetUp%1").arg(universe));
    setUp->setToolTip(tr("This controller's own setup: what it needs set on the device itself."));
    connect(setUp, &QPushButton::clicked, this, [this, universe]() { emit controllerSetupRequested(universe); });
    h->addWidget(setUp);
    // Every board's lamps can be tested (the PMJ's own test steps its levels).
    {
        QPushButton *test = new QPushButton(tr("Lamp Test…"), block);
        test->setObjectName(QStringLiteral("surfaceMapOptLampTest%1").arg(universe));
        test->setToolTip(tr("Light this board's lamps one by one, with what was sent in its log."));
        connect(test, &QPushButton::clicked, this, [this, universe]() { emit lampTestRequested(universe); });
        h->addWidget(test);
        QPushButton *log = new QPushButton(tr("Board Log…"), block);
        log->setObjectName(QStringLiteral("surfaceMapOptBoardLog%1").arg(universe));
        log->setToolTip(tr("What this board sent and was sent, to copy into a report."));
        connect(log, &QPushButton::clicked, this, [this, universe]() { emit boardLogRequested(universe); });
        h->addWidget(log);
    }
    if (desk != nullptr && desk->hasShift(universe))
    {
        QCheckBox *stop = new QCheckBox(tr("Stop needs Shift"), block);
        stop->setObjectName(QStringLiteral("surfaceMapShiftStop%1").arg(universe));
        stop->setToolTip(tr("STOP on this board does nothing alone: Shift+STOP stops everything. The "
                            "keyboard's and the footer's stop stay one press. Saved with this show."));
        stop->setChecked(desk->shiftGuardsStop(universe));
        connect(stop, &QCheckBox::toggled, this, [cm](bool on) {
            cm->setShiftGuardStop(on ? ControlMap::GuardOn : ControlMap::GuardOff);
        });
        QCheckBox *rel = new QCheckBox(tr("Releasing a stop needs Shift"), block);
        rel->setObjectName(QStringLiteral("surfaceMapShiftRelease%1").arg(universe));
        rel->setToolTip(tr("After a stop, GO on this board needs Shift too, so a stray press does not "
                           "start the show again. Saved with this show."));
        rel->setChecked(desk->shiftGuardsRelease(universe));
        connect(rel, &QCheckBox::toggled, this, [cm](bool on) {
            cm->setShiftGuardRelease(on ? ControlMap::GuardOn : ControlMap::GuardOff);
        });
        h->addWidget(stop);
        h->addWidget(rel);
        m_guardBoxes.insert(universe, qMakePair(stop, rel));
    }
    QPushButton *all = new QPushButton(tr("All Boards"), block);
    all->setObjectName(QStringLiteral("surfaceMapAllBoards%1").arg(universe));
    all->setToolTip(tr("Show the maps of every patched board again."));
    all->setVisible(false);
    connect(all, &QPushButton::clicked, this, &SurfaceMaps::showAll);
    h->addStretch(1);
    h->addWidget(all);
    m_allButtons.insert(universe, all);
    v->addLayout(h);
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

/*****************************************************************************
 * Clicking a control: a job for it (Q355)
 *****************************************************************************/

namespace
{
SurfaceMaps::PinAsker g_pinAsker = nullptr;

bool askPin(QWidget *parent, const QString &why, Doc *doc)
{
    if (g_pinAsker != nullptr)
        return g_pinAsker(parent, why);
    return ShowSettingsDialog::askForPin(parent, doc->operateSettings()->pinHash(), why);
}
}

void SurfaceMaps::setPinAsker(PinAsker asker)
{
    g_pinAsker = asker;
}

bool SurfaceMaps::unlocked(const QString &why)
{
    // Operate: a job is the running show's, so it asks for the PIN when the show has one.
    if (!m_doc->operateSettings()->hasPin() || m_doc->mode() != Doc::Operate)
        return true;
    return askPin(this, why, m_doc);
}

QList<SurfaceMaps::Job> SurfaceMaps::jobsFor(quint32 universe, quint32 channel) const
{
    QList<Job> out;
    SurfaceMapView *mv = m_views.value(universe);
    if (mv == nullptr)
        return out;
    const int i = mv->layout().indexOf(channel);
    if (i < 0)
        return out;
    const SurfaceLayout::Control &c = mv->layout().controls().at(i);
    if (c.kind == SurfaceLayout::Indicator)
        return out;
    DeskRecord *desk = m_doc->deskRecord();
    SurfacePages *pages = m_doc->controlMap()->surfacePages();
    const QString here = desk->identityOfUniverse(universe);
    auto isRole = [&](DeskRecord::Role r) {
        const DeskRecord::Binding b = desk->binding(r);
        return b.isSet() && b.identity == here && c.hears(b.channel);
    };
    auto add = [&out](const QString &key, const QString &text, bool checked = false, bool enabled = true) {
        Job j;
        j.key = key;
        j.text = text;
        j.checked = checked;
        j.enabled = enabled;
        out << j;
    };
    const bool key = c.kind == SurfaceLayout::Button || c.kind == SurfaceLayout::Pad;
    if (key)
    {
        add(QStringLiteral("go"), tr("GO"), isRole(DeskRecord::Go));
        add(QStringLiteral("back"), tr("Back"), isRole(DeskRecord::Back));
        add(QStringLiteral("stop"), tr("STOP (all off)"), isRole(DeskRecord::Stop));
        const PageSet set = pages->pageSet(universe);
        if (set.isValid())
        {
            add(QStringLiteral("page-prev"), tr("Page ◀ (previous page)"), set.previousKey() == c.channel);
            add(QStringLiteral("page-next"), tr("Page ▶ (next page)"), set.nextKey() == c.channel);
            for (int p = 0; p < set.pages().size(); ++p)
                add(QStringLiteral("page:%1").arg(p), tr("Page: %1").arg(set.pages().at(p).name),
                    set.pages().at(p).key == c.channel);
        }
    }
    else if (c.isFader())
    {
        add(QStringLiteral("gm"), tr("Grand Master"), isRole(DeskRecord::GrandMaster));
    }
    add(QStringLiteral("other"), tr("Other… (Assign to Control)"));
    // No job: what this control has of the desk, the pages or the Control Map.
    bool has = false;
    for (const Job &j : std::as_const(out))
        has = has || j.checked;
    has = has || !m_doc->controlMap()->bindingsOn(universe, channel).isEmpty();
    add(QStringLiteral("none"), tr("No job"), false, has);
    return out;
}

QMenu *SurfaceMaps::jobMenu(quint32 universe, quint32 channel)
{
    SurfaceMapView *mv = m_views.value(universe);
    const QList<Job> jobs = jobsFor(universe, channel);
    if (mv == nullptr || jobs.isEmpty())
        return nullptr;
    QMenu *menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("surfaceJobMenu%1").arg(universe));
    const int i = mv->layout().indexOf(channel);
    const SurfaceLayout::Control &c = mv->layout().controls().at(i);
    const QString now = mv->jobText(channel);
    QAction *title = menu->addAction(now.isEmpty() ? tr("%1 · Free").arg(c.face) : tr("%1 · %2").arg(c.face, now));
    title->setEnabled(false);
    menu->addSeparator();
    auto addJob = [&](const Job &j) {
        QAction *a = menu->addAction(j.text);
        a->setData(j.key);
        a->setCheckable(j.key != QLatin1String("other") && j.key != QLatin1String("none"));
        a->setChecked(j.checked);
        a->setEnabled(j.enabled);
        connect(a, &QAction::triggered, this, [this, universe, channel, key = j.key]() {
            QString why;
            if (!applyJob(universe, channel, key, &why) && !why.isEmpty())
                QToolTip::showText(QCursor::pos(), why, this);
        });
    };
    bool sep = false;
    for (const Job &j : jobs)
    {
        if (!sep && (j.key == QLatin1String("other")))
        {
            menu->addSeparator();
            sep = true;
        }
        addJob(j);
    }
    if (c.turns())
    {
        QAction *note = new QAction(tr("A knob turns what the page says."), menu);
        note->setEnabled(false);
        menu->insertAction(menu->actions().at(2), note);
    }
    if (!m_undo.text.isEmpty())
    {
        menu->addSeparator();
        QAction *u = menu->addAction(tr("Undo: %1").arg(m_undo.text));
        u->setObjectName(QStringLiteral("surfaceJobUndo"));
        connect(u, &QAction::triggered, this, [this]() { undoLast(); });
    }
    return menu;
}

void SurfaceMaps::showJobMenu(quint32 universe, quint32 channel, const QPoint &globalPos)
{
    QMenu *menu = jobMenu(universe, channel);
    if (menu == nullptr)
        return;
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(globalPos);
}

bool SurfaceMaps::undoLast()
{
    if (!m_undo.undo)
        return false;
    if (!unlocked(tr("Enter the show's PIN to change a control's job.")))
        return false;
    const std::function<void()> undo = m_undo.undo;
    m_undo = Undo();
    undo();
    refresh();
    return true;
}

bool SurfaceMaps::applyJob(quint32 universe, quint32 channel, const QString &key, QString *why)
{
    auto refuse = [why](const QString &text) {
        if (why != nullptr)
            *why = text;
        return false;
    };
    SurfaceMapView *mv = m_views.value(universe);
    const int idx = mv != nullptr ? mv->layout().indexOf(channel) : -1;
    if (idx < 0)
        return refuse(tr("That control is not on this board."));
    const SurfaceLayout::Control c = mv->layout().controls().at(idx);
    DeskRecord *desk = m_doc->deskRecord();
    ControlMap *cm = m_doc->controlMap();
    SurfacePages *pages = cm->surfacePages();

    if (key == QLatin1String("other"))
    {
        // The thing is assigned from where it is; the control is told which one to press.
        QMessageBox *box = new QMessageBox(QMessageBox::Information, tr("Assign to Control"),
                                           tr("Right-click the cue list, cue, hit, scene or effect, choose "
                                              "Assign to Control…, and press %1 on the board.").arg(c.face), QMessageBox::Ok, this);
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->setObjectName(QStringLiteral("surfaceJobOther"));
        box->open();
        return true;
    }
    if (!unlocked(tr("Enter the show's PIN to change a control's job.")))
        return refuse(tr("The show's PIN is needed to change a job in Operate."));

    // One step of Undo: the desk's roles, the board's page keys and the control's triggers, as they were.
    QList<DeskRecord::Binding> roles;
    for (DeskRecord::Role r : DeskRecord::roles())
        roles << desk->binding(r);
    const SurfacePages::KeyState keys = pages->keyState(universe);
    const QList<ControlMap::Binding> triggers = cm->bindingsOn(universe, channel);
    QString text;
    bool ok = false;
    QString reason;

    auto assign = [&](DeskRecord::Role r) {
        // The PIN was asked above, for this change: the desk takes it once.
        desk->setUnlocked(true);
        ok = desk->assignRole(r, universe, c.channel, &reason);
        desk->relock();
        // The show's trigger on a desk control is kept, not armed (Q309); the page key leaves.
        if (ok)
            pages->clearKey(universe, c.channel);
        text = tr("%1 is %2").arg(c.face, DeskRecord::roleName(r));
    };
    if (key == QLatin1String("go"))
        assign(DeskRecord::Go);
    else if (key == QLatin1String("back"))
        assign(DeskRecord::Back);
    else if (key == QLatin1String("stop"))
        assign(DeskRecord::Stop);
    else if (key == QLatin1String("gm"))
        assign(DeskRecord::GrandMaster);
    else if ((key == QLatin1String("page-prev") || key == QLatin1String("page-next") || key.startsWith(QLatin1String("page:")))
             && desk->isDeskControl(universe, c.channel))
    {
        return refuse(tr("%1 is one of this computer's desk controls: give it \"No job\" first.").arg(c.face));
    }
    else if (key == QLatin1String("page-prev") || key == QLatin1String("page-next"))
    {
        ok = pages->setStepKey(universe, key == QLatin1String("page-next") ? 1 : -1, c.channel);
        if (!ok)
            reason = tr("This board has no pages, or %1 is a desk control.").arg(c.face);
        text = tr("%1 is %2").arg(c.face, key == QLatin1String("page-next") ? tr("Page ▶") : tr("Page ◀"));
    }
    else if (key.startsWith(QLatin1String("page:")))
    {
        const int p = key.mid(5).toInt();
        ok = pages->setPageKey(universe, p, c.channel);
        if (!ok)
            reason = tr("This board has no pages, or %1 is a desk control.").arg(c.face);
        text = tr("%1 brings page %2").arg(c.face, p >= 0 && p < pages->pageSet(universe).pages().size()
                                                       ? pages->pageSet(universe).pages().at(p).name : QString());
    }
    else if (key == QLatin1String("none"))
    {
        ok = true;
        desk->setUnlocked(true);
        for (DeskRecord::Role r : DeskRecord::roles())
        {
            const DeskRecord::Binding b = desk->binding(r);
            if (b.isSet() && b.identity == desk->identityOfUniverse(universe) && c.hears(b.channel))
                desk->clear(r);
        }
        desk->relock();
        pages->clearKey(universe, c.channel);
        for (const ControlMap::Binding &b : triggers)
            cm->removeBinding(b.target);
        text = tr("%1: no job").arg(c.face);
    }
    else
    {
        return refuse(tr("Unknown job."));
    }
    if (!ok)
        return refuse(reason);

    m_undo.text = text;
    m_undo.undo = [this, universe, roles, keys, triggers]() {
        DeskRecord *d = m_doc->deskRecord();
        d->setUnlocked(true);
        int n = 0;
        for (DeskRecord::Role r : DeskRecord::roles())
        {
            d->setUnlocked(true);
            d->setBinding(r, roles.at(n++));
        }
        d->relock();
        m_doc->controlMap()->surfacePages()->setKeyState(universe, keys);
        for (const ControlMap::Binding &b : triggers)
            m_doc->controlMap()->setBinding(b, true);
    };
    refresh();
    refreshPages();
    return true;
}

void SurfaceMaps::refresh()
{
    for (SurfaceMapView *mv : std::as_const(m_views))
        mv->refresh();
}

void SurfaceMaps::showOnly(quint32 universe)
{
    if (m_rebuildQueued)
        rebuild();
    if (!m_blocks.contains(universe))
        return;
    m_only = universe;
    applyFilter();
    showMap(universe);
}

void SurfaceMaps::showAll()
{
    m_only = 0xFFFFFFFF;
    applyFilter();
}

bool SurfaceMaps::isBoardShown(quint32 universe) const
{
    const QWidget *b = m_blocks.value(universe);
    return b != nullptr && !b->isHidden();
}

void SurfaceMaps::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    // Leaving the page ends "this board alone": Connections lists them all.
    if (m_only != 0xFFFFFFFF)
        showAll();
}

void SurfaceMaps::applyFilter()
{
    const bool one = m_only != 0xFFFFFFFF && m_blocks.contains(m_only);
    for (auto it = m_blocks.constBegin(); it != m_blocks.constEnd(); ++it)
        it.value()->setVisible(!one || it.key() == m_only);
    for (QPushButton *b : std::as_const(m_allButtons))
        b->setVisible(one && m_blocks.size() > 1);
    if (QWidget *f = findChild<QWidget *>(QStringLiteral("surfaceMapFallback")))
        f->setVisible(!one);
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
