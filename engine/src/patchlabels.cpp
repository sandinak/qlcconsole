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

#include <QRegularExpression>
#include <QHash>
#include <QImage>

#include "patchlabels.h"
#include "rigowner.h"

/*********************************************************************
 * The stocks
 *********************************************************************/

/* A continuous tape @p mm wide, 60 mm a label until fitted(). TZe tape
   prints 18 mm of 24 and about 9 of 12: the rest is the margin. */
static void makeTape(PatchLabels::Sheet &s, int mm)
{
    const qreal w = mm;
    s.page = QSizeF(60, w);
    s.label = s.page;
    s.origin = QPointF(0, 0);
    s.pitch = s.page;
    s.columns = 1;
    s.rows = 1;
    s.tape = true;
    s.margin = mm >= 24 ? 3.0 : 1.5;
}

QList<PatchLabels::Sheet> PatchLabels::sheets()
{
    QList<Sheet> out;

    /* Continuous tape, the label printers' kind (Brother TZe, Dymo D1 ...):
       a label a page, the printer's driver cuts between them. */
    Sheet tape;
    tape.key = QStringLiteral("tape");
    tape.name = QCoreApplication::translate("PatchLabels", "Continuous tape (a label a page)");
    makeTape(tape, 24);
    out << tape;

    /* The Brother PT-P710BT: the same tape through Brother's own macOS
       driver, which takes the page size as the label. Picked as the printer
       when it is there. */
    Sheet brother;
    brother.key = QStringLiteral("ptp710bt");
    brother.name = QCoreApplication::translate("PatchLabels", "Brother PT-P710BT (TZe tape, Brother driver)");
    makeTape(brother, 24);
    brother.printer = QStringLiteral("PT-P710BT");
    out << brother;

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

QList<int> PatchLabels::tapeWidths()
{
    return { 12, 24 };
}

PatchLabels::Sheet PatchLabels::sheet(const QString &key, int tapeMm)
{
    const QList<Sheet> all = sheets();
    Sheet out = all.first();
    foreach (const Sheet &s, all)
    {
        if (s.key == key)
            out = s;
    }
    if (out.tape)
        makeTape(out, tapeWidths().contains(tapeMm) ? tapeMm : 24);
    return out;
}

/*********************************************************************
 * The template
 *********************************************************************/

QString PatchLabels::defaultTemplate()
{
    return QStringLiteral("{name} D{address}");
}

QStringList PatchLabels::templateFields()
{
    return { QStringLiteral("{name}"), QStringLiteral("{address}"), QStringLiteral("{universe}"),
             QStringLiteral("{mode}"), QStringLiteral("{location}"), QStringLiteral("{owner}"),
             QStringLiteral("{group}") };
}

QString PatchLabels::address(const PatchList::Row &row, bool multiUniverse)
{
    if (!row.patched && !row.clash)
        return QStringLiteral("—");
    if (multiUniverse)
        return QStringLiteral("%1.%2").arg(row.universe + 1).arg(row.firstChannel());
    return QString::number(row.firstChannel());
}

QString PatchLabels::render(const QString &text, const PatchList::Row &row, bool multiUniverse)
{
    const bool at = row.patched || row.clash;
    const QHash<QString, QString> fields = {
        { QStringLiteral("name"), row.name },
        { QStringLiteral("address"), address(row, multiUniverse) },
        { QStringLiteral("universe"), at ? QString::number(row.universe + 1) : QStringLiteral("—") },
        { QStringLiteral("mode"), row.modeText() },
        { QStringLiteral("location"), row.location },
        { QStringLiteral("owner"), RigOwner::displayName(row.ownerKind) },
        { QStringLiteral("group"), row.group },
    };
    /* One pass: a name with "{address}" in it stays as it is. */
    static const QRegularExpression field(QStringLiteral("\\{(\\w+)\\}"));
    const QString tmpl = text.isEmpty() ? defaultTemplate() : text;
    QString out;
    int from = 0;
    QRegularExpressionMatchIterator it = field.globalMatch(tmpl);
    while (it.hasNext())
    {
        const QRegularExpressionMatch m = it.next();
        out += tmpl.mid(from, m.capturedStart() - from);
        const QString key = m.captured(1).toLower();
        out += fields.contains(key) ? fields.value(key) : m.captured(0);
        from = m.capturedEnd();
    }
    out += tmpl.mid(from);
    return out.simplified();
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
        /* Its name, which the crew reads; the IP only when it has none
           (the patch list shows the IP on mouseover). */
        QString where = row.host;
        if (where.isEmpty())
            where = row.ip.isEmpty() ? QStringLiteral("(no IP)") : row.ip;
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

/* The size the font is set at for its line (ascent + descent) to fill
   @p height device units. */
static qreal fullHeightPx(QPaintDevice *device, qreal height)
{
    const QFontMetricsF fm(labelFont(true, 100), device);
    return height * 100.0 / qMax(1.0, fm.ascent() + fm.descent());
}

/* Tape: the line takes this share of the printed band, the rest air
   above and below so it reads as a label, not a stamp. */
static const qreal kTapeLine = 0.8;

PatchLabels::Sheet PatchLabels::fitted(const Sheet &sheet, const QList<PatchList::Row> &rows,
                                       const Options &options)
{
    if (!sheet.tape)
        return sheet;
    /* Measured in millimetres on a 254 dpi picture: 10 units a mm. */
    QImage probe(1, 1, QImage::Format_ARGB32);
    probe.setDotsPerMeterX(10000);
    probe.setDotsPerMeterY(10000);
    const qreal band = (sheet.page.height() - 2 * sheet.margin) * 10;
    const QFont font = labelFont(true, fullHeightPx(&probe, band * kTapeLine));
    const QFontMetricsF fm(font, &probe);
    qreal longest = 0;
    foreach (const PatchList::Row &r, rows)
        longest = qMax(longest, fm.horizontalAdvance(render(options.text, r, options.multiUniverse)) / 10);
    const qreal length = qBound(25.0, longest + 2 * sheet.margin + qMax(0.0, options.cutGap), 150.0);
    Sheet out = sheet;
    out.page.setWidth(length);
    out.label = out.page;
    out.pitch = out.page;
    return out;
}

QPageLayout PatchLabels::pageLayout(const Sheet &sheet)
{
    /* A page wider than it is tall (tape, the roll) is a portrait size
       turned on its side: a PDF viewer then shows it the way it reads. */
    const bool wide = sheet.page.width() > sheet.page.height();
    const QSizeF portrait = wide ? sheet.page.transposed() : sheet.page;
    return QPageLayout(QPageSize(portrait, QPageSize::Millimeter, sheet.name, QPageSize::ExactMatch),
                       wide ? QPageLayout::Landscape : QPageLayout::Portrait,
                       QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
}

/* One label in @p mm (millimetres), drawn at @p dpm device units a mm. */
static void drawLabel(QPainter &p, const QRectF &mm, qreal dpm, const PatchList::Row &row,
                      const PatchLabels::Sheet &sheet, const PatchLabels::Options &options)
{
    const QString text = PatchLabels::render(options.text, row, options.multiUniverse);
    p.setPen(Qt::black);

    if (sheet.tape)
    {
        /* One line, as tall as the tape prints; the cut gap at the ends. */
        const qreal end = qMax(0.0, options.cutGap) / 2 + sheet.margin;
        const QRectF band((mm.x() + end) * dpm, (mm.y() + sheet.margin) * dpm,
                          (mm.width() - 2 * end) * dpm, (mm.height() - 2 * sheet.margin) * dpm);
        const qreal full = fullHeightPx(p.device(), band.height() * kTapeLine);
        drawLine(p, band, text, true, full, full * 0.3);
        return;
    }

    /* Inside a printer's margin of error: 2 mm in from each edge. */
    const qreal pad = sheet.margin;
    const QRectF inner((mm.x() + pad) * dpm, (mm.y() + pad * 0.75) * dpm,
                       (mm.width() - 2 * pad) * dpm, (mm.height() - 1.5 * pad) * dpm);
    const PatchLabels::Label l = PatchLabels::label(row);

    /* The template's line large, the rest small, in shares of the height:
       a network fixture's own line takes some from the large one. */
    const bool net = !l.network.isEmpty();
    const qreal big = net ? 0.40 : 0.46;
    const qreal tiny = (1.0 - big) / (net ? 3 : 2);
    qreal y = inner.top();
    auto box = [&](qreal share) {
        const QRectF r(inner.left(), y, inner.width(), inner.height() * share);
        y += r.height();
        return r;
    };
    QRectF r = box(big);
    drawLine(p, r, text, true, r.height() * 0.82, r.height() * 0.4);
    r = box(tiny);
    const QString detail = l.universe.isEmpty() ? l.detail
                                                : QStringLiteral("%1 · %2").arg(l.detail, l.universe);
    drawLine(p, r, detail, false, r.height() * 0.82, r.height() * 0.5);
    if (net)
    {
        r = box(tiny);
        drawLine(p, r, l.network, false, r.height() * 0.82, r.height() * 0.5);
    }
    r = box(tiny);
    drawLine(p, r, l.mount, false, r.height() * 0.82, r.height() * 0.5);
}

bool PatchLabels::paint(QPagedPaintDevice *device, const QList<PatchList::Row> &rows,
                        const Sheet &sheet, const Options &options, QString *error)
{
    if (rows.isEmpty())
    {
        if (error != nullptr)
            *error = QCoreApplication::translate("PatchLabels", "There are no fixtures to label.");
        return false;
    }
    QPainter p;
    if (device == nullptr || !p.begin(device))
    {
        if (error != nullptr)
            *error = QCoreApplication::translate("PatchLabels", "Could not draw the labels.");
        return false;
    }
    const qreal dpm = device->logicalDpiX() / 25.4;        // device units a millimetre

    int current = 0;
    for (int i = 0; i < rows.size(); ++i)
    {
        int page = 0;
        const QRectF mm = labelRect(sheet, i, &page);
        if (page != current)
        {
            device->newPage();
            current = page;
        }
        drawLabel(p, mm, dpm, rows.at(i), sheet, options);
    }
    p.end();
    return true;
}

QImage PatchLabels::preview(const PatchList::Row &row, const Sheet &sheet, const Options &options,
                            qreal pxPerMm)
{
    const QSizeF mm = sheet.label;
    QImage img(qMax(1, qRound(mm.width() * pxPerMm)), qMax(1, qRound(mm.height() * pxPerMm)),
               QImage::Format_ARGB32_Premultiplied);
    img.setDotsPerMeterX(qRound(pxPerMm * 1000));
    img.setDotsPerMeterY(qRound(pxPerMm * 1000));
    img.fill(Qt::white);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    drawLabel(p, QRectF(QPointF(0, 0), mm), pxPerMm, row, sheet, options);
    p.setPen(QPen(QColor(160, 160, 160), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(0.5, 0.5, img.width() - 1, img.height() - 1));
    p.end();
    return img;
}

bool PatchLabels::writePdf(const QString &path, const QList<PatchList::Row> &rows,
                           const Sheet &sheet, const QString &title, QString *error)
{
    return writePdf(path, rows, sheet, title, Options(), error);
}

bool PatchLabels::writePdf(const QString &path, const QList<PatchList::Row> &rows,
                           const Sheet &sheet, const QString &title, const Options &options,
                           QString *error)
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

    const Sheet s = fitted(sheet, rows, options);
    QPdfWriter writer(path);
    writer.setTitle(title);
    writer.setCreator(QStringLiteral("qlcconsole"));
    writer.setPageLayout(pageLayout(s));
    return paint(&writer, rows, s, options, error);
}
