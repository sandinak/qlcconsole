/*
  Q Light Controller Plus
  universeoutputview.cpp

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

#include <QHelpEvent>
#include <QVBoxLayout>
#include <QPainter>
#include <QToolTip>
#include <QLabel>
#include <QTimer>

#include "universeoutputview.h"
#include "inputoutputmap.h"
#include "qlcchannel.h"
#include "qlcfixturemode.h"
#include <QMouseEvent>
#include "universe.h"
#include "fixture.h"
#include "doc.h"

static const int MAX_COLS = 16;
static const int MIN_COLS = 4;
static const int CELL_W = 34;
static const int CELL_H = 34;          // number, a fixture's name, the value
static const int TOP = 34;          // under the header
static const int LEFT = 34;         // room for the row's first channel number
static const int LEGEND_ROW = 16;   // one line of the legend under the grid

UniverseOutputView::UniverseOutputView(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_universe(Universe::invalid())
{
    setObjectName("universeOutputView");
    setMouseTracking(true);
    m_header = new QLabel(this);
    // Within the grid's width: never past the view's own edge.
    m_header->setGeometry(6, 4, LEFT + m_cols * CELL_W - 4, 24);
    m_header->setStyleSheet("QLabel { color: #c8ccd6; font-weight: bold; }");

    /* Universes write up to the tick rate; the grid is repainted at most
       fifteen times a second, and only when something changed. */
    m_repaint = new QTimer(this);
    m_repaint->setInterval(66);
    connect(m_repaint, &QTimer::timeout, this, [this]() {
        if (m_dirty)
        {
            m_dirty = false;
            update();
        }
    });
    m_repaint->start();

    connect(m_doc->inputOutputMap(), &InputOutputMap::universeWritten,
            this, &UniverseOutputView::slotUniverseWritten);
    updateHeader();
}

QSize UniverseOutputView::sizeHint() const
{
    return QSize(widthFor(m_cols), TOP + rows() * CELL_H + 8 + legendHeight());
}

QSize UniverseOutputView::minimumSizeHint() const
{
    return QSize(widthFor(m_cols), TOP + 8 * CELL_H);
}

int UniverseOutputView::widthFor(int columns)
{
    return LEFT + columns * CELL_W + 8;
}

int UniverseOutputView::rows() const
{
    return (512 + m_cols - 1) / m_cols;
}

void UniverseOutputView::fitWidth(int width)
{
    /* As many channels a row as the width holds, up to sixteen: a narrow
       window gives the grid fewer columns, never the settings beside it. */
    const int cols = qBound(MIN_COLS, (width - LEFT - 8) / CELL_W, MAX_COLS);
    if (cols == m_cols)
        return;
    m_cols = cols;
    m_header->setGeometry(6, 4, LEFT + m_cols * CELL_W - 4, 24);
    showHeader(m_headerText);
    resize(sizeHint());
    updateGeometry();
    update();
}

void UniverseOutputView::setUniverse(quint32 universe)
{
    m_universe = universe;
    m_values = QByteArray(512, char(0));
    // What it is sending now, not only once it next changes.
    InputOutputMap *io = m_doc->inputOutputMap();
    if (universe != Universe::invalid() && universe < io->universesCount())
    {
        QList<Universe *> ua = io->claimUniverses();
        if (int(universe) < ua.size() && ua.at(int(universe)) != nullptr)
        {
            const QByteArray *post = ua.at(int(universe))->postGMValues();
            if (post != nullptr)
                m_values.replace(0, qMin(512, post->size()), post->left(512));
        }
        io->releaseUniverses(false);
    }
    updateHeader();
    update();
}

void UniverseOutputView::slotUniverseWritten(quint32 index, const QByteArray &data)
{
    if (index != m_universe)
        return;
    if (m_values.size() != 512)
        m_values = QByteArray(512, char(0));
    // Only the channels in use are written; the rest stay as they were (0).
    m_values.replace(0, qMin(512, data.size()), data.left(512));
    m_dirty = true;
}

void UniverseOutputView::updateHeader()
{
    InputOutputMap *io = m_doc->inputOutputMap();
    if (m_universe == Universe::invalid() || m_universe >= io->universesCount())
    {
        showHeader(tr("Select a universe to see what it sends"));
        return;
    }
    QString name = io->getUniverseNameByIndex(int(m_universe));
    if (name.isEmpty())
        name = tr("Universe %1").arg(m_universe + 1);
    showHeader(tr("%1 — sending").arg(name));
}

/* A universe's name can be longer than four columns are wide: the header
   then ends in "…" and its tooltip has the whole line. */
void UniverseOutputView::showHeader(const QString &text)
{
    m_headerText = text;
    m_hovered = -1;
    const QString shown = m_header->fontMetrics().elidedText(text, Qt::ElideRight,
                                                             m_header->width());
    m_header->setText(shown);
    m_header->setToolTip(shown == text ? QString() : text);
}

QRect UniverseOutputView::cellRect(int channel) const
{
    return QRect(LEFT + (channel % m_cols) * CELL_W, TOP + (channel / m_cols) * CELL_H, CELL_W - 2, CELL_H - 2);
}

int UniverseOutputView::channelAt(const QPoint &pos) const
{
    if (pos.x() < LEFT || pos.y() < TOP)
        return -1;
    const int c = (pos.x() - LEFT) / CELL_W, r = (pos.y() - TOP) / CELL_H;
    if (c >= m_cols)
        return -1;
    const int ch = r * m_cols + c;
    return (ch >= 0 && ch < 512) ? ch : -1;
}

/* Whose channel @p ch of @p universe is: the fixture and the channel's name. */
static Fixture *ownerOf(Doc *doc, quint32 universe, int ch, int *offset)
{
    foreach (Fixture *fx, doc->fixtures())
    {
        if (!fx->isPatched() || fx->universe() != universe)
            continue;
        const int a = int(fx->address());
        if (ch >= a && ch < a + int(fx->channels()))
        {
            if (offset != nullptr)
                *offset = ch - a;
            return fx;
        }
    }
    return nullptr;
}

/* What a channel is, from its definition's group and colour the way the
   Simple Desk reads them. */
UniverseOutputView::Kind UniverseOutputView::kindOf(const QLCChannel *qc)
{
    if (qc == nullptr)
        return Plain;
    const bool light = qc->group() == QLCChannel::Intensity || qc->group() == QLCChannel::Colour;
    if (light && qc->colour() != QLCChannel::NoColour)
    {
        switch (qc->colour())
        {
            case QLCChannel::Red: return Red;
            case QLCChannel::Green: return Green;
            case QLCChannel::Blue: return Blue;
            case QLCChannel::Amber: return Amber;
            case QLCChannel::White: return White;
            case QLCChannel::UV: return UV;
            case QLCChannel::Cyan: return Cyan;
            case QLCChannel::Magenta: return Magenta;
            case QLCChannel::Yellow: return Yellow;
            case QLCChannel::Lime: return Lime;
            case QLCChannel::Indigo: return Indigo;
            default: break;
        }
    }
    switch (qc->group())
    {
        case QLCChannel::Intensity: return Dimmer;
        case QLCChannel::Pan: return Pan;
        case QLCChannel::Tilt: return Tilt;
        case QLCChannel::Colour: return ColourWheel;
        case QLCChannel::Gobo: return Gobo;
        case QLCChannel::Shutter: return Shutter;
        case QLCChannel::Speed: return Speed;
        case QLCChannel::Prism: return PrismKind;
        case QLCChannel::Beam: return BeamKind;
        case QLCChannel::Effect: return EffectKind;
        default: return OtherKind;
    }
}

QColor UniverseOutputView::emitterColour(Kind kind)
{
    switch (kind)
    {
        case Red: return QColor(255, 40, 40);
        case Green: return QColor(40, 225, 70);
        case Blue: return QColor(70, 100, 255);
        case Amber: return QColor(255, 150, 0);
        case White: return QColor(240, 240, 240);
        case UV: return QColor(150, 40, 230);
        case Cyan: return QColor(0, 220, 230);
        case Magenta: return QColor(240, 40, 220);
        case Yellow: return QColor(250, 230, 30);
        case Lime: return QColor(170, 250, 50);
        case Indigo: return QColor(90, 40, 170);
        default: return QColor();
    }
}

/* The channel's block: a dull shade of its colour at 0 up to the bright one
   at 255; the dimmer a grey ramp. Pan, tilt and the rest are drawn as marks. */
QColor UniverseOutputView::fillFor(Kind kind, int v)
{
    const double t = v / 255.0;
    if (kind == Dimmer)
    {
        const int g = qRound(46 + t * 190);
        return QColor(g, g, g);
    }
    const QColor c = emitterColour(kind);
    if (!c.isValid())
        return QColor(44, 48, 58);
    const QColor dull(34, 36, 42);
    const double k = 0.22 + 0.78 * t;
    return QColor(qRound(dull.red() + (c.red() - dull.red()) * k),
                  qRound(dull.green() + (c.green() - dull.green()) * k),
                  qRound(dull.blue() + (c.blue() - dull.blue()) * k));
}

static const QColor kMark(120, 200, 255);

/* The mark of a channel that is not an emitter: an arrow for pan (left and
   right, placed by the value) and tilt (up and down), a small shape for the
   rest, brighter the higher the value. */
void UniverseOutputView::paintMark(QPainter &p, Kind kind, const QRect &cell, int v)
{
    const double t = v / 255.0;
    QColor c = kMark;
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    const QPoint mid = cell.center() + QPoint(0, 1);
    if (kind == Pan)
    {
        const int x = cell.left() + 8 + qRound(t * (cell.width() - 16));
        const int y = mid.y();
        p.setPen(QPen(c, 2));
        p.drawLine(x - 5, y, x + 5, y);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawPolygon(QPolygon({ QPoint(x - 8, y), QPoint(x - 4, y - 3), QPoint(x - 4, y + 3) }));
        p.drawPolygon(QPolygon({ QPoint(x + 8, y), QPoint(x + 4, y - 3), QPoint(x + 4, y + 3) }));
    }
    else if (kind == Tilt)
    {
        const int x = mid.x();
        const int y = cell.bottom() - 8 - qRound(t * (cell.height() - 16));
        p.setPen(QPen(c, 2));
        p.drawLine(x, y - 5, x, y + 5);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawPolygon(QPolygon({ QPoint(x, y - 8), QPoint(x - 3, y - 4), QPoint(x + 3, y - 4) }));
        p.drawPolygon(QPolygon({ QPoint(x, y + 8), QPoint(x - 3, y + 4), QPoint(x + 3, y + 4) }));
    }
    else
    {
        c.setAlpha(90 + qRound(165 * t));
        p.setPen(QPen(c, 1.5));
        p.setBrush(Qt::NoBrush);
        switch (kind)
        {
            case Gobo: p.drawEllipse(mid, 5, 5); break;
            case ColourWheel:
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(240, 180, 60, c.alpha()));
                p.drawPie(QRect(mid.x() - 5, mid.y() - 5, 10, 10), 0, 16 * 180);
                p.setBrush(QColor(80, 160, 240, c.alpha()));
                p.drawPie(QRect(mid.x() - 5, mid.y() - 5, 10, 10), 16 * 180, 16 * 180);
                break;
            case Shutter:
                p.drawLine(mid.x() - 5, mid.y() - 3, mid.x() + 5, mid.y() - 3);
                p.drawLine(mid.x() - 5, mid.y(), mid.x() + 5, mid.y());
                p.drawLine(mid.x() - 5, mid.y() + 3, mid.x() + 5, mid.y() + 3);
                break;
            case Speed:
                p.drawPolyline(QPolygon({ QPoint(mid.x() - 5, mid.y() - 4), QPoint(mid.x(), mid.y()),
                                          QPoint(mid.x() - 5, mid.y() + 4) }));
                p.drawPolyline(QPolygon({ QPoint(mid.x(), mid.y() - 4), QPoint(mid.x() + 5, mid.y()),
                                          QPoint(mid.x(), mid.y() + 4) }));
                break;
            case PrismKind:
                p.drawPolygon(QPolygon({ QPoint(mid.x(), mid.y() - 5), QPoint(mid.x() - 5, mid.y() + 4),
                                         QPoint(mid.x() + 5, mid.y() + 4) }));
                break;
            case BeamKind:
                p.drawPolygon(QPolygon({ QPoint(mid.x(), mid.y() - 5), QPoint(mid.x() + 5, mid.y()),
                                         QPoint(mid.x(), mid.y() + 5), QPoint(mid.x() - 5, mid.y()) }));
                break;
            case EffectKind:
                p.drawLine(mid.x() - 5, mid.y(), mid.x() + 5, mid.y());
                p.drawLine(mid.x(), mid.y() - 5, mid.x(), mid.y() + 5);
                break;
            default:
                p.drawLine(mid.x() - 4, mid.y(), mid.x() + 4, mid.y());
                break;
        }
    }
    p.restore();
}

/* What the colours mean: one small entry a kind, laid out in as many lines as
   the width needs. */
struct LegendEntry { UniverseOutputView::Kind kind; const char *text; };

static const LegendEntry *legendEntries(int *n)
{
    static const LegendEntry e[] = {
        { UniverseOutputView::Red, QT_TRANSLATE_NOOP("UniverseOutputView", "Red") },
        { UniverseOutputView::Green, QT_TRANSLATE_NOOP("UniverseOutputView", "Green") },
        { UniverseOutputView::Blue, QT_TRANSLATE_NOOP("UniverseOutputView", "Blue") },
        { UniverseOutputView::Amber, QT_TRANSLATE_NOOP("UniverseOutputView", "Amber") },
        { UniverseOutputView::White, QT_TRANSLATE_NOOP("UniverseOutputView", "White") },
        { UniverseOutputView::UV, QT_TRANSLATE_NOOP("UniverseOutputView", "UV") },
        { UniverseOutputView::Dimmer, QT_TRANSLATE_NOOP("UniverseOutputView", "Dimmer") },
        { UniverseOutputView::Pan, QT_TRANSLATE_NOOP("UniverseOutputView", "Pan") },
        { UniverseOutputView::Tilt, QT_TRANSLATE_NOOP("UniverseOutputView", "Tilt") },
        { UniverseOutputView::Gobo, QT_TRANSLATE_NOOP("UniverseOutputView", "Other") },
    };
    *n = int(sizeof(e) / sizeof(e[0]));
    return e;
}

int UniverseOutputView::legendHeight() const
{
    int n = 0;
    const LegendEntry *e = legendEntries(&n);
    const QFontMetrics fm(font());
    int x = LEFT, lines = 1;
    for (int i = 0; i < n; ++i)
    {
        const int w = 16 + fm.horizontalAdvance(tr(e[i].text)) + 12;
        if (x + w > widthFor(m_cols) - 8 && x > LEFT)
        {
            x = LEFT;
            ++lines;
        }
        x += w;
    }
    return lines * LEGEND_ROW + 6;
}

void UniverseOutputView::paintLegend(QPainter &p, int top)
{
    int n = 0;
    const LegendEntry *e = legendEntries(&n);
    QFont f = font();
    f.setPointSizeF(qMax(7.0, f.pointSizeF() - 2.0));
    p.setFont(f);
    const QFontMetrics fm(f);
    int x = LEFT, y = top;
    for (int i = 0; i < n; ++i)
    {
        const QString label = tr(e[i].text);
        const int w = 16 + fm.horizontalAdvance(label) + 12;
        if (x + w > widthFor(m_cols) - 8 && x > LEFT)
        {
            x = LEFT;
            y += LEGEND_ROW;
        }
        const QRect sw(x, y + 2, 12, 12);
        const Kind k = e[i].kind;
        if (k == Pan || k == Tilt || k == Gobo)
        {
            p.fillRect(sw, QColor(44, 48, 58));
            paintMark(p, k == Gobo ? OtherKind : k, sw, 128);
        }
        else
        {
            p.fillRect(sw, fillFor(k, 255));
        }
        p.setPen(QColor(150, 154, 166));
        p.drawText(QRect(x + 16, y, w - 16, LEGEND_ROW), Qt::AlignLeft | Qt::AlignVCenter, label);
        x += w;
    }
}

QString UniverseOutputView::describe(int ch) const
{
    if (ch < 0 || ch >= 512 || m_universe == Universe::invalid())
        return QString();
    int off = 0;
    if (Fixture *fx = ownerOf(m_doc, m_universe, ch, &off))
    {
        const int a = int(fx->address()) + 1;
        const int last = qMin(512, a + int(fx->channels()) - 1);
        const QLCChannel *qc = fx->channel(quint32(off));
        QString what = tr("ch %1").arg(off + 1);
        if (qc != nullptr && !qc->name().isEmpty())
            what += QStringLiteral(" ") + qc->name();
        QString text = tr("%1 · %2 (%3-%4) · %5").arg(ch + 1).arg(fx->name()).arg(a).arg(last).arg(what);
        // And the mode it is in, as the Simple Desk names it.
        if (fx->fixtureMode() != nullptr && !fx->fixtureMode()->name().isEmpty())
            text += tr(" · mode %1").arg(fx->fixtureMode()->name());
        return text;
    }
    // The free run it sits in: what someone patching is looking for.
    QVector<bool> used(512, false);
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (!fx->isPatched() || fx->universe() != m_universe)
            continue;
        for (quint32 i = 0; i < fx->channels() && fx->address() + i < 512; ++i)
            used[int(fx->address() + i)] = true;
    }
    int from = ch, to = ch;
    while (from > 0 && !used.at(from - 1))
        --from;
    while (to < 511 && !used.at(to + 1))
        ++to;
    const int n = to - from + 1;
    return tr("%1 · free %2-%3, %4").arg(ch + 1).arg(from + 1).arg(to + 1)
        .arg(n == 1 ? tr("1 channel") : tr("%1 channels").arg(n));
}

void UniverseOutputView::hoverChannel(int ch)
{
    if (ch == m_hovered)
        return;
    if (ch < 0)
    {
        showHeader(m_headerText);           // its own line back
        return;
    }
    m_hovered = ch;
    const QString text = describe(ch);
    m_header->setText(m_header->fontMetrics().elidedText(text, Qt::ElideRight, m_header->width()));
    m_header->setToolTip(QString());
}

void UniverseOutputView::mouseMoveEvent(QMouseEvent *e)
{
    /* At once, on the header line: no wait for a tooltip ("show the
       address of the moused over block"). */
    hoverChannel(m_universe == Universe::invalid() ? -1 : channelAt(e->pos()));
    QWidget::mouseMoveEvent(e);
}

void UniverseOutputView::leaveEvent(QEvent *e)
{
    hoverChannel(-1);
    QWidget::leaveEvent(e);
}

bool UniverseOutputView::event(QEvent *e)
{
    if (e->type() == QEvent::ToolTip)
    {
        QHelpEvent *he = static_cast<QHelpEvent *>(e);
        const int ch = channelAt(he->pos());
        if (ch < 0 || m_universe == Universe::invalid())
        {
            QToolTip::hideText();
            return true;
        }
        const int v = uchar(ch < m_values.size() ? m_values.at(ch) : char(0));
        const QString text = describe(ch) + QStringLiteral("\n")
                             + tr("Sending %1 (%2%)").arg(v).arg(qRound(v * 100.0 / 255.0));
        QToolTip::showText(he->globalPos(), text, this);
        return true;
    }
    return QWidget::event(e);
}

void UniverseOutputView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(24, 26, 31));
    if (m_universe == Universe::invalid())
        return;

    // Each fixture's span, shaded in turn so neighbours are told apart.
    QVector<int> owner(512, -1);
    QVector<QString> startName(512);
    QVector<const QLCChannel *> chan(512, nullptr);
    int nth = 0;
    QList<Fixture *> fixtures;
    foreach (Fixture *fx, m_doc->fixtures())
        if (fx->isPatched() && fx->universe() == m_universe)
            fixtures << fx;
    std::sort(fixtures.begin(), fixtures.end(), [](Fixture *a, Fixture *b) { return a->address() < b->address(); });
    foreach (Fixture *fx, fixtures)
    {
        const int a = int(fx->address());
        for (int i = 0; i < int(fx->channels()) && a + i < 512; ++i)
        {
            owner[a + i] = nth;
            chan[a + i] = fx->channel(quint32(i));
        }
        if (a < 512)
            startName[a] = fx->name();
        ++nth;
    }

    QFont tiny = font();
    tiny.setPointSizeF(qMax(6.5, tiny.pointSizeF() - 3.0));
    QFont val = font();
    val.setPointSizeF(qMax(7.5, val.pointSizeF() - 1.5));
    val.setBold(true);

    // Row numbers down the side.
    p.setFont(tiny);
    p.setPen(QColor(120, 124, 134));
    for (int r = 0; r < rows(); ++r)
        p.drawText(QRect(0, TOP + r * CELL_H, LEFT - 4, CELL_H), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(rowStart(r)));

    for (int ch = 0; ch < 512; ++ch)
    {
        const QRect cell = cellRect(ch);
        const int o = owner.at(ch);
        const int v = uchar(ch < m_values.size() ? m_values.at(ch) : char(0));
        const Kind kind = (o < 0) ? Plain : kindOf(chan.at(ch));
        QColor bg = (o < 0) ? QColor(34, 36, 42) : fillFor(kind, v);
        if (o >= 0 && kind == Plain)
            bg = (o % 2) ? QColor(40, 54, 74) : QColor(52, 46, 70);
        p.fillRect(cell, bg);
        if (o < 0 && v > 0)
        {
            // Not patched: a bar as high as the value, as before.
            const int h = qMax(1, qRound(double(cell.height()) * v / 255.0));
            p.fillRect(QRect(cell.left(), cell.bottom() - h + 1, cell.width(), h), QColor(70, 170, 110, 150));
        }
        if (o >= 0 && (kind == Pan || kind == Tilt || kind >= ColourWheel))
            paintMark(p, kind, cell, v);
        // A fixture's first channel: a mark, and its name over the cells it spans.
        if (!startName.at(ch).isEmpty())
        {
            p.fillRect(QRect(cell.left(), cell.top(), 2, cell.height()), QColor(255, 190, 60));
        }
        p.setFont(tiny);
        p.setPen(o >= 0 && bg.lightness() > 150 ? QColor(40, 44, 52) : QColor(130, 134, 146));
        p.drawText(cell.adjusted(2, 0, 0, 0), Qt::AlignLeft | Qt::AlignTop, QString::number(ch + 1));
        p.setFont(val);
        // On a bright block the number reads dark; elsewhere light.
        const bool bright = o >= 0 && bg.lightness() > 150;
        p.setPen(bright ? QColor(20, 22, 27) : (v > 0 ? QColor(235, 238, 244) : QColor(110, 114, 124)));
        p.drawText(cell.adjusted(0, 0, -2, 0), Qt::AlignRight | Qt::AlignBottom, QString::number(v));
    }
    // What the colours mean, under the grid.
    paintLegend(p, TOP + rows() * CELL_H + 6);
}
