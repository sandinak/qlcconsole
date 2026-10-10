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
#include "universe.h"
#include "fixture.h"
#include "doc.h"

static const int COLS = 16;
static const int CELL_W = 34;
static const int CELL_H = 34;          // number, a fixture's name, the value
static const int TOP = 34;          // under the header
static const int LEFT = 34;         // room for the row's first channel number

UniverseOutputView::UniverseOutputView(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_universe(Universe::invalid())
{
    setObjectName("universeOutputView");
    setMouseTracking(true);
    m_header = new QLabel(this);
    m_header->setGeometry(6, 4, 600, 24);
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
    return QSize(LEFT + COLS * CELL_W + 8, TOP + (512 / COLS) * CELL_H + 8);
}

QSize UniverseOutputView::minimumSizeHint() const
{
    return QSize(LEFT + COLS * CELL_W + 8, TOP + 8 * CELL_H);
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
        m_header->setText(tr("Select a universe to see what it sends"));
        return;
    }
    QString name = io->getUniverseNameByIndex(int(m_universe));
    if (name.isEmpty())
        name = tr("Universe %1").arg(m_universe + 1);
    m_header->setText(tr("%1 — sending").arg(name));
}

QRect UniverseOutputView::cellRect(int channel) const
{
    return QRect(LEFT + (channel % COLS) * CELL_W, TOP + (channel / COLS) * CELL_H, CELL_W - 2, CELL_H - 2);
}

int UniverseOutputView::channelAt(const QPoint &pos) const
{
    if (pos.x() < LEFT || pos.y() < TOP)
        return -1;
    const int c = (pos.x() - LEFT) / CELL_W, r = (pos.y() - TOP) / CELL_H;
    if (c >= COLS)
        return -1;
    const int ch = r * COLS + c;
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
        int off = 0;
        QString text = tr("Channel %1: %2 (%3%)").arg(ch + 1).arg(v).arg(qRound(v * 100.0 / 255.0));
        if (Fixture *fx = ownerOf(m_doc, m_universe, ch, &off))
        {
            const QLCChannel *qc = fx->channel(quint32(off));
            text += QStringLiteral("\n") + tr("%1, channel %2%3").arg(fx->name()).arg(off + 1)
                    .arg(qc != nullptr ? QStringLiteral(" — ") + qc->name() : QString());
        }
        else
            text += QStringLiteral("\n") + tr("no fixture patched here");
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
            owner[a + i] = nth;
        if (a < 512)
            startName[a] = fx->name();
        ++nth;
    }

    QFont small = font();
    small.setPointSizeF(qMax(6.5, small.pointSizeF() - 3.0));
    QFont val = font();
    val.setPointSizeF(qMax(7.5, val.pointSizeF() - 1.5));
    val.setBold(true);

    // Row numbers down the side.
    p.setFont(small);
    p.setPen(QColor(120, 124, 134));
    for (int r = 0; r < 512 / COLS; ++r)
        p.drawText(QRect(0, TOP + r * CELL_H, LEFT - 4, CELL_H), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(r * COLS + 1));

    for (int ch = 0; ch < 512; ++ch)
    {
        const QRect cell = cellRect(ch);
        const int o = owner.at(ch);
        QColor bg = (o < 0) ? QColor(34, 36, 42) : ((o % 2) ? QColor(40, 54, 74) : QColor(52, 46, 70));
        p.fillRect(cell, bg);
        const int v = uchar(ch < m_values.size() ? m_values.at(ch) : char(0));
        if (v > 0)
        {
            // A bar as high as the value.
            const int h = qMax(1, qRound(double(cell.height()) * v / 255.0));
            p.fillRect(QRect(cell.left(), cell.bottom() - h + 1, cell.width(), h), QColor(70, 170, 110, 150));
        }
        // A fixture's first channel: a mark, and its name over the cells it spans.
        if (!startName.at(ch).isEmpty())
        {
            p.fillRect(QRect(cell.left(), cell.top(), 2, cell.height()), QColor(255, 190, 60));
        }
        p.setFont(small);
        p.setPen(QColor(130, 134, 146));
        p.drawText(cell.adjusted(2, 0, 0, 0), Qt::AlignLeft | Qt::AlignTop, QString::number(ch + 1));
        p.setFont(val);
        p.setPen(v > 0 ? QColor(235, 238, 244) : QColor(90, 94, 104));
        p.drawText(cell.adjusted(0, 0, -2, 0), Qt::AlignRight | Qt::AlignBottom, QString::number(v));
    }
    // Names last, on top, where each fixture starts.
    p.setFont(small);
    for (int ch = 0; ch < 512; ++ch)
    {
        if (startName.at(ch).isEmpty())
            continue;
        const QRect cell = cellRect(ch);
        const QString name = p.fontMetrics().elidedText(startName.at(ch), Qt::ElideRight,
                                                        (COLS - ch % COLS) * CELL_W - 6);
        // Its own line, between the channel numbers and the values.
        const QRect tag(cell.left() + 3, cell.top() + 11, p.fontMetrics().horizontalAdvance(name) + 4,
                        p.fontMetrics().height());
        p.fillRect(tag, QColor(20, 22, 27, 190));
        p.setPen(QColor(255, 200, 90));
        p.drawText(tag.adjusted(2, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, name);
    }
}
