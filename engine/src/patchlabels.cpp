/*
  Q Light Controller Plus
  patchlabels.cpp

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

#include <QFontMetricsF>
#include <QCoreApplication>
#include <QPageLayout>
#include <QPdfWriter>
#include <QPageSize>
#include <QPainter>
#include <QStringList>
#include <QFile>
#include <QFont>

#include "patchlabels.h"

/*********************************************************************
 * The stocks
 *********************************************************************/

QList<PatchLabels::Sheet> PatchLabels::sheets()
{
    QList<Sheet> out;

    /* Avery 5160 (and its 8160, 5260, 5960 ... cousins): US Letter, three
       across and ten down, 2⅝" × 1", a ⅛" gutter between the columns,
       ½" top and 3⁄16" side margins. */
    Sheet avery;
    avery.key = QStringLiteral("avery5160");
    avery.name = QCoreApplication::translate("PatchLabels", "Avery 5160 (Letter, 30 a sheet, 2⅝ × 1 in)");
    avery.page = QSizeF(215.9, 279.4);
    avery.label = QSizeF(66.675, 25.4);
    avery.origin = QPointF(4.7625, 12.7);
    avery.pitch = QSizeF(69.85, 25.4);
    avery.columns = 3;
    avery.rows = 10;
    out << avery;

    /* A 62 mm continuous roll (Brother DK-22205 and the like), cut to
       29 mm: a label a page, the printer's driver cuts between them. */
    Sheet roll;
    roll.key = QStringLiteral("roll62");
    roll.name = QCoreApplication::translate("PatchLabels", "62 mm roll (one 62 × 29 mm label a page)");
    roll.page = QSizeF(62, 29);
    roll.label = QSizeF(62, 29);
    roll.origin = QPointF(0, 0);
    roll.pitch = QSizeF(62, 29);
    out << roll;

    return out;
}

QString PatchLabels::defaultSheetKey()
{
    return sheets().first().key;
}

PatchLabels::Sheet PatchLabels::sheet(const QString &key)
{
    const QList<Sheet> all = sheets();
    foreach (const Sheet &s, all)
        if (s.key == key)
            return s;
    return all.first();
}

QRectF PatchLabels::labelRect(const Sheet &sheet, int index, int *page)
{
    const int per = qMax(1, sheet.perPage());
    const int n = qMax(0, index);
    const int onPage = n % per;
    if (page != nullptr)
        *page = n / per;
    const int col = onPage % qMax(1, sheet.columns);
    const int row = onPage / qMax(1, sheet.columns);
    return QRectF(sheet.origin.x() + col * sheet.pitch.width(),
                  sheet.origin.y() + row * sheet.pitch.height(),
                  sheet.label.width(), sheet.label.height());
}

int PatchLabels::pageCount(const Sheet &sheet, int count)
{
    const int per = qMax(1, sheet.perPage());
    return count <= 0 ? 0 : (count + per - 1) / per;
}

/*********************************************************************
 * What a label says
 *********************************************************************/

PatchLabels::Label PatchLabels::label(const PatchList::Row &row)
{
    Label l;
    l.name = row.name;

    const QString at = QStringLiteral("U%1 · %2").arg(row.universe + 1).arg(row.firstChannel());
    if (row.clash)
        l.address = QStringLiteral("clash · %1").arg(at);
    else if (row.patched)
        l.address = at;
    else
        l.address = QStringLiteral("not patched");

    if (row.patched || row.clash)
        l.universe = row.universeName;

    QStringList detail;
    if (!row.mode.isEmpty())
        detail << row.mode;
    detail << QStringLiteral("%1 ch").arg(row.channels);
    l.detail = detail.join(QStringLiteral(" · "));

    if (row.network)
    {
        QString where;
        if (row.ip.isEmpty())
            where = QStringLiteral("(no IP)");
        else if (row.host.isEmpty() || row.host == row.ip)
            where = row.ip;
        else
            where = QStringLiteral("%1 (%2)").arg(row.host, row.ip);
        l.network = row.transport.isEmpty() ? where
                                            : QStringLiteral("%1 · %2").arg(row.transport, where);
    }
    l.mount = row.mountedOn;
    return l;
}

/*********************************************************************
 * The PDF
 *********************************************************************/

static QFont labelFont(bool bold, qreal px)
{
    QFont font(QStringLiteral("Helvetica"));
    font.setStyleHint(QFont::SansSerif);
    font.setBold(bold);
    font.setPixelSize(qMax(1, qRound(px)));
    return font;
}

/* The size @p text is set at to fit @p width: @p maxPx, shrunk down to
   @p minPx at most (it is elided past that). */
static qreal fitPx(QPainter &p, const QString &text, bool bold, qreal width, qreal maxPx, qreal minPx)
{
    qreal px = maxPx;
    while (px > minPx
           && QFontMetricsF(labelFont(bold, px), p.device()).horizontalAdvance(text) > width)
        px = qMax(minPx, px * 0.92);
    return px;
}

/* Draw @p text in @p box (device units) on one line, as large as @p maxPx
   allows: shrunk to fit down to @p minPx, then elided. */
static void drawLine(QPainter &p, const QRectF &box, const QString &text, bool bold,
                     qreal maxPx, qreal minPx)
{
    if (text.isEmpty() || box.width() <= 0)
        return;
    const QFont font = labelFont(bold, fitPx(p, text, bold, box.width(), maxPx, minPx));
    p.setFont(font);
    p.drawText(box, Qt::AlignLeft | Qt::AlignVCenter,
               QFontMetricsF(font, p.device()).elidedText(text, Qt::ElideRight, box.width()));
}

/* The address, large, and the universe's name beside it, smaller, on the
   same baseline, in what room is left. */
static void drawAddress(QPainter &p, const QRectF &box, const QString &address,
                        const QString &universe, qreal maxPx, qreal minPx)
{
    if (universe.isEmpty())
    {
        drawLine(p, box, address, true, maxPx, minPx);
        return;
    }
    /* The address keeps at least two thirds of the width for itself. */
    const QFont big = labelFont(true, fitPx(p, address, true, box.width() * 2 / 3, maxPx, minPx));
    const QFontMetricsF bfm(big, p.device());
    const QString addr = bfm.elidedText(address, Qt::ElideRight, box.width() * 2 / 3);
    const qreal baseline = box.center().y() + (bfm.ascent() - bfm.descent()) / 2;
    p.setFont(big);
    p.drawText(QPointF(box.left(), baseline), addr);

    const qreal gap = bfm.height() * 0.3;
    const qreal x = box.left() + bfm.horizontalAdvance(addr) + gap;
    const qreal room = box.right() - x;
    if (room <= 0)
        return;
    const qreal smallMax = qMax(minPx * 0.6, big.pixelSize() * 0.55);
    const QFont small = labelFont(false, fitPx(p, universe, false, room, smallMax, smallMax * 0.7));
    p.setFont(small);
    p.drawText(QPointF(x, baseline),
               QFontMetricsF(small, p.device()).elidedText(universe, Qt::ElideRight, room));
}

bool PatchLabels::writePdf(const QString &path, const QList<PatchList::Row> &rows,
                           const Sheet &sheet, const QString &title, QString *error)
{
    if (rows.isEmpty())
    {
        if (error != nullptr)
            *error = QCoreApplication::translate("PatchLabels", "There are no fixtures to label.");
        return false;
    }
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
    writer.setTitle(title);
    writer.setCreator(QStringLiteral("qlcconsole"));
    /* A page wider than it is tall (the roll) is a portrait size turned on
       its side: a PDF viewer then shows it the way it reads. */
    const bool wide = sheet.page.width() > sheet.page.height();
    const QSizeF portrait = wide ? sheet.page.transposed() : sheet.page;
    writer.setPageLayout(QPageLayout(QPageSize(portrait, QPageSize::Millimeter, sheet.name,
                                               QPageSize::ExactMatch),
                                     wide ? QPageLayout::Landscape : QPageLayout::Portrait,
                                     QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter));

    QPainter p;
    if (!p.begin(&writer))
    {
        if (error != nullptr)
            *error = QCoreApplication::translate("PatchLabels", "Could not draw the PDF.");
        return false;
    }
    p.setPen(Qt::black);
    const qreal dpm = writer.resolution() / 25.4;          // device units a millimetre

    int current = 0;
    for (int i = 0; i < rows.size(); ++i)
    {
        int page = 0;
        const QRectF mm = labelRect(sheet, i, &page);
        if (page != current)
        {
            writer.newPage();
            current = page;
        }

        /* Inside a printer's margin of error: 2 mm in from each edge. */
        const qreal pad = 2.0;
        const QRectF inner((mm.x() + pad) * dpm, (mm.y() + pad * 0.75) * dpm,
                           (mm.width() - 2 * pad) * dpm, (mm.height() - 1.5 * pad) * dpm);
        const Label l = label(rows.at(i));

        /* Name and address large, the rest small, in shares of the height:
           a network fixture's own line takes some from the large two. */
        const bool net = !l.network.isEmpty();
        const qreal big = net ? 0.28 : 0.31, addr = net ? 0.27 : 0.29;
        const qreal small = (1.0 - big - addr) / (net ? 3 : 2);
        qreal y = inner.top();
        auto box = [&](qreal share) {
            const QRectF r(inner.left(), y, inner.width(), inner.height() * share);
            y += r.height();
            return r;
        };
        QRectF r = box(big);
        drawLine(p, r, l.name, true, r.height() * 0.82, r.height() * 0.45);
        r = box(addr);
        drawAddress(p, r, l.address, l.universe, r.height() * 0.82, r.height() * 0.45);
        r = box(small);
        drawLine(p, r, l.detail, false, r.height() * 0.82, r.height() * 0.5);
        if (net)
        {
            r = box(small);
            drawLine(p, r, l.network, false, r.height() * 0.82, r.height() * 0.5);
        }
        r = box(small);
        drawLine(p, r, l.mount, false, r.height() * 0.82, r.height() * 0.5);
    }
    p.end();
    return true;
}
