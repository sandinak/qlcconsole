/*
  Q Light Controller Plus
  cuesheetdialog.cpp

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

#include <QDialogButtonBox>
#include <QApplication>
#include <QTextBrowser>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QComboBox>
#include <QLabel>
#include <QFile>

#include "cuesheetdialog.h"
#include "app.h"
#include "doc.h"

CueSheetDialog::CueSheetDialog(Doc *doc, quint32 productionId, QWidget *parent)
    : QDialog(parent)
    , m_sheet(CueSheet::build(doc, productionId))
{
    foreach (QWidget *w, qApp->topLevelWidgets())
        if (App *app = qobject_cast<App *>(w))
            m_show = QFileInfo(app->fileName()).fileName();
    setObjectName(QStringLiteral("cueSheetDialog"));
    setWindowTitle(tr("Cue Sheet"));
    setProperty("helpTopic", QStringLiteral("show-tree.md#the-cue-sheet-and-the-tracking-sheet"));
    resize(1000, 600);

    QVBoxLayout *root = new QVBoxLayout(this);
    QHBoxLayout *bar = new QHBoxLayout;
    root->addLayout(bar);
    m_page = new QComboBox(this);
    m_page->setObjectName(QStringLiteral("cueSheetPage"));
    m_page->addItem(tr("Cue sheet"), int(CueSheetPage));
    m_page->addItem(tr("Tracking sheet"), int(TrackingPage));
    m_page->setToolTip(tr("Cue sheet: a row per cue in running order. Tracking sheet: a row per layer, "
                          "a column per cue; what a cue changes plain, what only tracks in grey."));
    bar->addWidget(new QLabel(tr("Sheet:"), this));
    bar->addWidget(m_page);
    bar->addStretch(1);
    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("cueSheetSummary"));
    bar->addWidget(m_summary);

    m_view = new QTextBrowser(this);
    m_view->setObjectName(QStringLiteral("cueSheetView"));
    root->addWidget(m_view, 1);

    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Close, this);
    struct Save { const char *name; QString text; QString tip; void (CueSheetDialog::*slot)(); };
    const Save saves[] = {
        { "cueSheetCsv", tr("Save CSV..."), tr("Save the sheet shown as a spreadsheet (CSV)"),
          &CueSheetDialog::slotSaveCsv },
        { "cueSheetXlsx", tr("Save XLSX..."), tr("Save the sheet shown as an Excel workbook "
                                                  "(Numbers and Google Sheets open it too)"),
          &CueSheetDialog::slotSaveXlsx },
        { "cueSheetPdf", tr("Save PDF..."), tr("Save the sheet shown as a printable PDF (landscape Letter)"),
          &CueSheetDialog::slotSavePdf },
    };
    for (const Save &s : saves)
    {
        QPushButton *b = btns->addButton(s.text, QDialogButtonBox::ActionRole);
        b->setObjectName(QString::fromLatin1(s.name));
        b->setToolTip(s.tip);
        connect(b, &QPushButton::clicked, this, s.slot);
    }
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(btns);

    connect(m_page, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CueSheetDialog::slotShow);
    slotShow();
}

CueSheetDialog::Sheet CueSheetDialog::page() const
{
    return Sheet(m_page->currentData().toInt());
}

void CueSheetDialog::setPage(Sheet page)
{
    m_page->setCurrentIndex(m_page->findData(int(page)));
}

QString CueSheetDialog::title() const
{
    QString t = page() == TrackingPage ? tr("Tracking sheet") : tr("Cue sheet");
    if (!m_sheet.productionName().isEmpty())
        t += QStringLiteral(" \u2014 ") + m_sheet.productionName();
    if (!m_show.isEmpty())
        t += QStringLiteral(" \u2014 ") + m_show;
    return t;
}

QString CueSheetDialog::shownHtml() const
{
    return page() == TrackingPage ? m_sheet.trackingHtml(title()) : m_sheet.cueHtml(title());
}

void CueSheetDialog::slotShow()
{
    m_view->setHtml(shownHtml());
    m_summary->setText(m_sheet.isEmpty() ? tr("No cue lists in this production")
                                         : tr("%n cue(s) in %1 list(s)", nullptr, m_sheet.cueCount())
                                               .arg(m_sheet.lists().count()));
}

QString CueSheetDialog::suggestedName(const QString &ext) const
{
    const QString show = QFileInfo(m_show).completeBaseName();
    const QString what = page() == TrackingPage ? QStringLiteral("tracking-sheet") : QStringLiteral("cue-sheet");
    return show.isEmpty() ? QStringLiteral("%1.%2").arg(what, ext)
                          : QStringLiteral("%1-%2.%3").arg(show, what, ext);
}

bool CueSheetDialog::saveCsv(const QString &path, QString *error) const
{
    const QString csv = page() == TrackingPage ? m_sheet.trackingCsv() : m_sheet.cueCsv();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(csv.toUtf8()) < 0)
    {
        if (error != nullptr)
            *error = file.errorString();
        return false;
    }
    return true;
}

bool CueSheetDialog::saveXlsx(const QString &path, QString *error) const
{
    return page() == TrackingPage ? m_sheet.writeTrackingXlsx(path, error) : m_sheet.writeCueXlsx(path, error);
}

bool CueSheetDialog::savePdf(const QString &path, QString *error) const
{
    return page() == TrackingPage ? m_sheet.writeTrackingPdf(path, title(), error)
                                  : m_sheet.writeCuePdf(path, title(), error);
}

void CueSheetDialog::slotSaveCsv()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Sheet"), suggestedName(QStringLiteral("csv")),
                                                      tr("CSV files (*.csv)"));
    QString error;
    if (!path.isEmpty() && !saveCsv(path, &error))
        QMessageBox::warning(this, tr("Save Sheet"), tr("Could not save %1:\n%2").arg(path, error));
}

void CueSheetDialog::slotSaveXlsx()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Sheet"), suggestedName(QStringLiteral("xlsx")),
                                                      tr("Excel workbooks (*.xlsx)"));
    QString error;
    if (!path.isEmpty() && !saveXlsx(path, &error))
        QMessageBox::warning(this, tr("Save Sheet"), tr("Could not save %1:\n%2").arg(path, error));
}

void CueSheetDialog::slotSavePdf()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Sheet"), suggestedName(QStringLiteral("pdf")),
                                                      tr("PDF files (*.pdf)"));
    QString error;
    if (!path.isEmpty() && !savePdf(path, &error))
        QMessageBox::warning(this, tr("Save Sheet"), tr("Could not save %1:\n%2").arg(path, error));
}
