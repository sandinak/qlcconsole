/*
  Q Light Controller Plus - qlcconsole
  surfacekeylabels.cpp

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

#include <QFileDialog>
#include <QMessageBox>
#include <QPrintDialog>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QPrinter>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QPainter>
#include <QLabel>
#include <QDir>
#include <QFontMetricsF>
#include <QRegularExpression>

#include "surfacekeylabels.h"
#include "patchlabelsdialog.h"
#include "measurementunits.h"
#include "surfacepages.h"
#include "surfacemap.h"
#include "controlmap.h"
#include "pageset.h"
#include "doc.h"

namespace
{
/** @p text in @p rect, the font made smaller (not under @p minPx) until it fits, two lines at most. */
void drawFitted(QPainter &p, const QRectF &rect, const QString &text, QFont f, int flags, double minPx)
{
    double px = f.pixelSize() > 0 ? f.pixelSize() : 12;
    for (;;)
    {
        f.setPixelSize(qMax(1, int(px)));
        const QRectF need = QFontMetricsF(f).boundingRect(rect, flags | Qt::TextWordWrap, text);
        if ((need.height() <= rect.height() + 0.5 && need.width() <= rect.width() + 0.5) || px <= minPx)
            break;
        px -= 0.5;
    }
    p.setFont(f);
    p.drawText(rect, flags | Qt::TextWordWrap, QFontMetricsF(f).elidedText(text, Qt::ElideRight,
                                                                           int(rect.width() * 2)));
}

QString safeName(const QString &s)
{
    QString out = s;
    out.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")), QStringLiteral("_"));
    return out;
}
}

QList<SurfaceKeyLabels::Page> SurfaceKeyLabels::pages(const SurfaceMapView *view)
{
    QList<Page> out;
    if (view == nullptr)
        return out;
    const SurfaceLayout &layout = view->layout();
    const double unit = layout.unitMm();
    const PageSet set = view->pageSet();
    auto keysFor = [&](const QHash<quint32, QString> &pageLabels) {
        QList<Key> keys;
        for (const SurfaceLayout::Control &c : layout.controls())
        {
            if (c.kind != SurfaceLayout::Button && c.kind != SurfaceLayout::Pad)
                continue;
            Key k;
            k.channel = c.channel;
            k.face = c.face;
            k.text = pageLabels.contains(c.channel) ? pageLabels.value(c.channel) : view->jobTextNoPage(c.channel);
            k.mm = QSizeF(c.rect.width() * unit, c.rect.height() * unit);
            keys << k;
        }
        return keys;
    };
    if (set.isValid())
    {
        for (int i = 0; i < set.pages().size(); ++i)
            out << Page{ set.pages().at(i).name, keysFor(set.labels(i)) };
    }
    else
    {
        out << Page{ QObject::tr("Now"), keysFor(QHash<quint32, QString>()) };
    }
    return out;
}

bool SurfaceKeyLabels::writeSheet(const QString &path, const QString &title, const QList<Page> &pages, QString *error)
{
    bool known = false;
    for (const Page &p : pages)
        for (const Key &k : p.keys)
            known = known || (k.mm.width() > 0 && k.mm.height() > 0);
    if (!known)
    {
        if (error != nullptr)
            *error = QObject::tr("This board's key sizes are not known.");
        return false;
    }
    QPdfWriter pdf(path);
    pdf.setTitle(title);
    pdf.setPageSize(QPageSize(QPageSize::A4));
    pdf.setPageMargins(QMarginsF(10, 10, 10, 10), QPageLayout::Millimeter);
    pdf.setResolution(720);
    return paintSheet(&pdf, title, pages, error);
}

bool SurfaceKeyLabels::paintSheet(QPagedPaintDevice *device, const QString &title, const QList<Page> &pages,
                                  QString *error)
{
    QPainter p(device);
    if (!p.isActive())
    {
        if (error != nullptr)
            *error = QObject::tr("The labels can't be drawn.");
        return false;
    }
    const double mm = device->logicalDpiX() / 25.4;
    const QRectF page(0, 0, device->width(), device->height());
    const double gap = 2.5 * mm;
    bool first = true;
    for (const Page &pg : pages)
    {
        if (!first)
            device->newPage();
        first = false;
        QFont head;
        head.setPixelSize(int(4.2 * mm));
        head.setBold(true);
        p.setFont(head);
        p.setPen(Qt::black);
        const QString line = QObject::tr("%1 · %2: key labels, cut on the line").arg(title, pg.name);
        p.drawText(QRectF(0, 0, page.width(), 6 * mm), Qt::AlignLeft | Qt::AlignVCenter, line);
        double x = 0, y = 8 * mm, rowH = 0;
        for (const Key &k : pg.keys)
        {
            const double w = k.mm.width() * mm, h = k.mm.height() * mm;
            if (w <= 0 || h <= 0)
                continue;
            if (x + w > page.width() && x > 0)
            {
                x = 0;
                y += rowH + gap;
                rowH = 0;
            }
            if (y + h > page.height())
            {
                device->newPage();
                y = 0;
                rowH = 0;
                x = 0;
            }
            const QRectF r(x, y, w, h);
            p.setPen(QPen(QColor(0x66, 0x66, 0x66), 0.15 * mm, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRect(r);
            QFont f;
            f.setPixelSize(int(qMin(h * 0.42, 4.0 * mm)));
            f.setBold(!k.text.isEmpty());
            p.setPen(k.text.isEmpty() ? QColor(0x99, 0x99, 0x99) : Qt::black);
            drawFitted(p, r.adjusted(0.4 * mm, 0.3 * mm, -0.4 * mm, -0.3 * mm), k.text.isEmpty() ? k.face : k.text, f,
                       Qt::AlignCenter, 1.2 * mm);
            x += w + gap;
            rowH = qMax(rowH, h);
        }
    }
    p.end();
    return true;
}

QList<int> SurfaceKeyLabels::tapeWidths()
{
    return { 12, 18, 24 };
}

double SurfaceKeyLabels::printableMm(int tapeMm)
{
    return tapeMm >= 24 ? 18.0 : tapeMm >= 18 ? 12.0 : 9.0;
}

double SurfaceKeyLabels::tapeLengthMm(const Key &key)
{
    return qMax(8.0, key.mm.width());
}

QImage SurfaceKeyLabels::tapeLabel(const Key &key, int tapeMm)
{
    const double pxMm = kTapeDpi / 25.4;
    const int w = qMax(8, int(tapeLengthMm(key) * pxMm));
    const int h = qMax(8, int(printableMm(tapeMm) * pxMm));
    QImage img(w, h, QImage::Format_Mono);
    img.fill(1);
    img.setColor(0, qRgb(0, 0, 0));
    img.setColor(1, qRgb(255, 255, 255));
    QPainter p(&img);
    p.setRenderHint(QPainter::TextAntialiasing, false);
    p.setPen(QColor(Qt::black));
    QFont f;
    f.setPixelSize(int(h * 0.5));
    f.setBold(true);
    drawFitted(p, QRectF(1, 1, w - 2, h - 2), key.text.isEmpty() ? key.face : key.text, f, Qt::AlignCenter, 8);
    p.end();
    return img;
}

bool SurfaceKeyLabels::paintTape(QPagedPaintDevice *device, const QList<Key> &keys, int tapeMm, QString *error)
{
    if (keys.isEmpty())
    {
        if (error != nullptr)
            *error = QObject::tr("There is nothing to label.");
        return false;
    }
    QPainter p(device);
    if (!p.isActive())
    {
        if (error != nullptr)
            *error = QObject::tr("The labels can't be drawn.");
        return false;
    }
    const double dpmm = device->logicalDpiX() / 25.4;
    bool first = true;
    for (const Key &k : keys)
    {
        if (!first)
            device->newPage();
        first = false;
        const QImage img = tapeLabel(k, tapeMm);
        // The page is the tape's width by the longest label: each label from the top left.
        p.drawImage(QRectF(0, (tapeMm - printableMm(tapeMm)) / 2 * dpmm, tapeLengthMm(k) * dpmm,
                           printableMm(tapeMm) * dpmm), img);
    }
    p.end();
    return true;
}

static QPageLayout tapeLayout(const QList<SurfaceKeyLabels::Key> &keys, int tapeMm)
{
    double longest = 8;
    for (const SurfaceKeyLabels::Key &k : keys)
        longest = qMax(longest, SurfaceKeyLabels::tapeLengthMm(k));
    return QPageLayout(QPageSize(QSizeF(longest, tapeMm), QPageSize::Millimeter), QPageLayout::Landscape,
                       QMarginsF(0, 0, 0, 0));
}

void SurfaceKeyLabels::setUpPrinter(QPrinter *printer, const QList<Key> &keys, int tapeMm)
{
    const QList<PatchLabelsDialog::InstalledPrinter> installed = PatchLabelsDialog::installedPrinters();
    if (const PatchLabelsDialog::InstalledPrinter *b = PatchLabelsDialog::brotherPrinter(installed))
        printer->setPrinterName(b->name);
    printer->setDocName(QObject::tr("Key labels"));
    printer->setPageLayout(tapeLayout(keys, tapeMm));
    printer->setFullPage(true);
}

bool SurfaceKeyLabels::writeTapePdf(const QString &path, const QList<Key> &keys, int tapeMm, QString *error)
{
    QPdfWriter pdf(path);
    pdf.setTitle(QObject::tr("Key labels"));
    pdf.setPageLayout(tapeLayout(keys, tapeMm));
    pdf.setResolution(kTapeDpi * 4);
    return paintTape(&pdf, keys, tapeMm, error);
}

QStringList SurfaceKeyLabels::writeTapePngs(const QString &dir, const QList<Key> &keys, int tapeMm, QString *error)
{
    QStringList made;
    QDir().mkpath(dir);
    int n = 0;
    for (const Key &k : keys)
    {
        const QString path = QStringLiteral("%1/%2-%3-%4mm.png")
                                 .arg(dir).arg(++n, 3, 10, QLatin1Char('0')).arg(safeName(k.face)).arg(tapeMm);
        if (!tapeLabel(k, tapeMm).save(path))
        {
            if (error != nullptr)
                *error = QObject::tr("%1 can't be written.").arg(path);
            return made;
        }
        made << path;
    }
    return made;
}

/*****************************************************************************
 * The dialog
 *****************************************************************************/

SurfaceKeyLabelsDialog::SurfaceKeyLabelsDialog(const SurfaceMapView *view, QWidget *parent)
    : QDialog(parent)
    , m_pages(SurfaceKeyLabels::pages(view))
    , m_title(view != nullptr ? view->layout().name() : QString())
    , m_stock(new QComboBox(this))
    , m_page(new QComboBox(this))
    , m_sizes(new QLabel(this))
    , m_preview(new QLabel(this))
{
    setObjectName(QStringLiteral("surfaceKeyLabelsDialog"));
    setWindowTitle(tr("Print Key Labels"));
    QVBoxLayout *v = new QVBoxLayout(this);
    QHBoxLayout *h = new QHBoxLayout();
    h->addWidget(new QLabel(tr("Labels:"), this));
    m_stock->setObjectName(QStringLiteral("keyLabelStock"));
    m_stock->addItem(tr("A4 sheet, cut to the keys"), 0);
    for (int mm : SurfaceKeyLabels::tapeWidths())
        m_stock->addItem(tr("%1 mm TZe tape (Brother P-touch CUBE)").arg(mm), mm);     // units-ok: the tape's product size, as sold
    h->addWidget(m_stock, 1);
    v->addLayout(h);
    QHBoxLayout *ph = new QHBoxLayout();
    ph->addWidget(new QLabel(tr("Page:"), this));
    m_page->setObjectName(QStringLiteral("keyLabelPage"));
    m_page->addItem(tr("Every page"), -1);
    for (int i = 0; i < m_pages.size(); ++i)
        m_page->addItem(m_pages.at(i).name, i);
    ph->addWidget(m_page, 1);
    v->addLayout(ph);
    m_sizes->setObjectName(QStringLiteral("keyLabelSizes"));
    m_sizes->setWordWrap(true);
    v->addWidget(m_sizes);
    m_preview->setObjectName(QStringLiteral("keyLabelPreview"));
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumHeight(60);
    v->addWidget(m_preview);
    QDialogButtonBox *buttons = new QDialogButtonBox(this);
    QPushButton *pdf = buttons->addButton(tr("Save PDF…"), QDialogButtonBox::ActionRole);
    QPushButton *png = buttons->addButton(tr("Save PNGs…"), QDialogButtonBox::ActionRole);
    QPushButton *print = buttons->addButton(tr("Print…"), QDialogButtonBox::ActionRole);
    pdf->setObjectName(QStringLiteral("keyLabelSavePdf"));
    png->setObjectName(QStringLiteral("keyLabelSavePngs"));
    print->setObjectName(QStringLiteral("keyLabelPrint"));
    buttons->addButton(QDialogButtonBox::Close);
    v->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(pdf, &QPushButton::clicked, this, &SurfaceKeyLabelsDialog::slotSavePdf);
    connect(png, &QPushButton::clicked, this, &SurfaceKeyLabelsDialog::slotSavePngs);
    connect(print, &QPushButton::clicked, this, &SurfaceKeyLabelsDialog::slotPrint);
    connect(m_stock, &QComboBox::currentIndexChanged, this, &SurfaceKeyLabelsDialog::update);
    connect(m_page, &QComboBox::currentIndexChanged, this, &SurfaceKeyLabelsDialog::update);
    update();
}

int SurfaceKeyLabelsDialog::tapeMm() const
{
    return m_stock->currentData().toInt();
}

void SurfaceKeyLabelsDialog::setTapeMm(int mm)
{
    const int i = m_stock->findData(mm);
    if (i >= 0)
        m_stock->setCurrentIndex(i);
}

QList<SurfaceKeyLabels::Key> SurfaceKeyLabelsDialog::keys() const
{
    QList<SurfaceKeyLabels::Key> out;
    const int page = m_page->currentData().toInt();
    for (int i = 0; i < m_pages.size(); ++i)
        if (page < 0 || page == i)
            out << m_pages.at(i).keys;
    return out;
}

QString SurfaceKeyLabelsDialog::sizeLine() const
{
    const QList<SurfaceKeyLabels::Key> k = keys();
    if (k.isEmpty() || k.first().mm.width() <= 0)
        return tr("This board's key sizes are not known.");
    QSizeF smallest = k.first().mm, largest = k.first().mm;
    for (const SurfaceKeyLabels::Key &key : k)
    {
        smallest = QSizeF(qMin(smallest.width(), key.mm.width()), qMin(smallest.height(), key.mm.height()));
        largest = QSizeF(qMax(largest.width(), key.mm.width()), qMax(largest.height(), key.mm.height()));
    }
    auto one = [](const QSizeF &s) {
        return tr("%1 × %2").arg(MeasurementUnits::formatLength(s.width()), MeasurementUnits::formatLength(s.height()));
    };
    QString line = tr("%1 keys, from %2 to %3 (measured off the board's published size: check one with a rule).")
                       .arg(k.size()).arg(one(smallest), one(largest));
    if (tapeMm() > 0)
        line += QStringLiteral(" ") + tr("One label a key, %1 high, at %2 dpi.")
                    .arg(MeasurementUnits::formatLength(SurfaceKeyLabels::printableMm(tapeMm()))).arg(SurfaceKeyLabels::kTapeDpi);
    return line;
}

void SurfaceKeyLabelsDialog::update()
{
    m_sizes->setText(sizeLine());
    const QList<SurfaceKeyLabels::Key> k = keys();
    if (!k.isEmpty() && tapeMm() > 0)
    {
        // The first label with a job, as the tape would print it.
        SurfaceKeyLabels::Key sample = k.first();
        for (const SurfaceKeyLabels::Key &key : k)
            if (!key.text.isEmpty())
            {
                sample = key;
                break;
            }
        m_preview->setPixmap(QPixmap::fromImage(SurfaceKeyLabels::tapeLabel(sample, tapeMm()).convertToFormat(QImage::Format_RGB32)));
    }
    else
    {
        m_preview->clear();
    }
}

bool SurfaceKeyLabelsDialog::savePdf(const QString &path, QString *error) const
{
    if (tapeMm() > 0)
        return SurfaceKeyLabels::writeTapePdf(path, keys(), tapeMm(), error);
    QList<SurfaceKeyLabels::Page> chosen;
    const int page = m_page->currentData().toInt();
    for (int i = 0; i < m_pages.size(); ++i)
        if (page < 0 || page == i)
            chosen << m_pages.at(i);
    return SurfaceKeyLabels::writeSheet(path, m_title, chosen, error);
}

bool SurfaceKeyLabelsDialog::savePngs(const QString &dir, QStringList *made, QString *error) const
{
    const int mm = tapeMm() > 0 ? tapeMm() : 12;
    QString err;
    const QStringList files = SurfaceKeyLabels::writeTapePngs(dir, keys(), mm, &err);
    if (made != nullptr)
        *made = files;
    if (files.size() != keys().size())
    {
        if (error != nullptr)
            *error = err;
        return false;
    }
    return true;
}

void SurfaceKeyLabelsDialog::slotSavePdf()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Key Labels"),
                                                      QDir::home().absoluteFilePath(
                                                          QStringLiteral("%1-key-labels.pdf").arg(safeName(m_title))),
                                                      tr("PDF files (*.pdf)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!savePdf(path, &error))
        QMessageBox::warning(this, tr("Save Key Labels"), tr("Could not save %1:\n%2").arg(path, error));
}

void SurfaceKeyLabelsDialog::slotSavePngs()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Save Key Labels as Pictures"), QDir::homePath());
    if (dir.isEmpty())
        return;
    QString error;
    if (!savePngs(dir, nullptr, &error))
        QMessageBox::warning(this, tr("Save Key Labels"), tr("Could not save the pictures:\n%1").arg(error));
}

void SurfaceKeyLabelsDialog::slotPrint()
{
    QPrinter printer(QPrinter::HighResolution);
    if (tapeMm() > 0)
        SurfaceKeyLabels::setUpPrinter(&printer, keys(), tapeMm());
    else
        printer.setPageSize(QPageSize(QPageSize::A4));
    QPrintDialog dlg(&printer, this);
    dlg.setWindowTitle(tr("Print Key Labels"));
    if (dlg.exec() != QDialog::Accepted)
        return;
    QString error;
    bool ok = false;
    if (tapeMm() > 0)
    {
        ok = SurfaceKeyLabels::paintTape(&printer, keys(), tapeMm(), &error);
    }
    else
    {
        QList<SurfaceKeyLabels::Page> chosen;
        const int page = m_page->currentData().toInt();
        for (int i = 0; i < m_pages.size(); ++i)
            if (page < 0 || page == i)
                chosen << m_pages.at(i);
        ok = SurfaceKeyLabels::paintSheet(&printer, m_title, chosen, &error);
    }
    if (!ok)
        QMessageBox::warning(this, tr("Print Key Labels"), tr("Could not print the labels:\n%1").arg(error));
}
