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

    QStringList detail;
    if (!row.mode.isEmpty())
        detail << row.mode;
    detail << QStringLiteral("%1 ch").arg(row.channels);
    if (row.network)
        detail << QStringLiteral("%1 %2").arg(row.transport,
                                             row.ip.isEmpty() ? QStringLiteral("(no IP)") : row.ip);
    l.detail = detail.join(QStringLiteral(" · "));
    l.mount = row.mountedOn;
    return l;
}

/*********************************************************************
 * The PDF
 *********************************************************************/

/* Draw @p text in @p box (device units) on one line, as large as @p maxPx
   allows: shrunk to fit down to @p minPx, then elided. */
static void drawLine(QPainter &p, const QRectF &box, const QString &text, bool bold,
                     qreal maxPx, qreal minPx)
{
    if (text.isEmpty() || box.width() <= 0)
        return;
    QFont font(QStringLiteral("Helvetica"));
    font.setStyleHint(QFont::SansSerif);
    font.setBold(bold);
    qreal px = maxPx;
    for (;;)
    {
        font.setPixelSize(qMax(1, qRound(px)));
        QFontMetricsF fm(font, p.device());
        if (fm.horizontalAdvance(text) <= box.width() || px <= minPx)
            break;
        px = qMax(minPx, px * 0.92);
    }
    QFontMetricsF fm(font, p.device());
    p.setFont(font);
    p.drawText(box, Qt::AlignLeft | Qt::AlignVCenter,
               fm.elidedText(text, Qt::ElideRight, box.width()));
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

        /* Name and address large, mode and mount small: shares of the height. */
        const qreal shares[] = { 0.31, 0.29, 0.20, 0.20 };
        const QString text[] = { l.name, l.address, l.detail, l.mount };
        const bool bold[] = { true, true, false, false };
        qreal y = inner.top();
        for (int line = 0; line < 4; ++line)
        {
            const qreal h = inner.height() * shares[line];
            const QRectF box(inner.left(), y, inner.width(), h);
            drawLine(p, box, text[line], bold[line], h * 0.82, h * 0.45);
            y += h;
        }
    }
    p.end();
    return true;
}
