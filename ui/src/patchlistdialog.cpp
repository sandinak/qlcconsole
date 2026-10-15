/*
  Q Light Controller Plus
  patchlistdialog.cpp

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
#include <QTableWidget>
#include <QApplication>
#include <QHeaderView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QClipboard>
#include <QFileInfo>
#include <QLabel>
#include <QFile>

#include <algorithm>

#include "patchlabelsdialog.h"
#include "patchlistdialog.h"
#include "app.h"
#include "doc.h"

/* The show's file name, for the heading and the suggested file names. */
static QString showFileName()
{
    foreach (QWidget *w, qApp->topLevelWidgets())
        if (App *app = qobject_cast<App *>(w))
            return QFileInfo(app->fileName()).fileName();
    return QString();
}

PatchListDialog::PatchListDialog(Doc *doc, QWidget *parent)
    : QDialog(parent)
    , m_list(PatchList::build(doc))
{
    const QString show = showFileName();
    m_title = show.isEmpty() ? tr("Patch list") : tr("Patch list — %1").arg(show);
    setWindowTitle(tr("Patch List"));
    setProperty("helpTopic", QStringLiteral("fixture-manager.md#the-patch-list"));
    resize(980, 560);

    QVBoxLayout *root = new QVBoxLayout(this);
    int patched = 0;
    foreach (const PatchList::Row &r, m_list.rows())
        if (r.patched)
            patched++;
    root->addWidget(new QLabel(tr("%1 fixtures, %2 patched, in universe and address order; "
                                  "the inventory last.").arg(m_list.rows().size()).arg(patched), this));

    const QStringList cols = { tr("Patch"), tr("Fixture"), tr("Make / model"), tr("Mode"),
                               tr("Ch"), tr("Connection"), tr("Goes to"), tr("Mounted on") };
    m_table = new QTableWidget(m_list.rows().size(), cols.size(), this);
    m_table->setHorizontalHeaderLabels(cols);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->verticalHeader()->hide();
    m_table->setWordWrap(false);
    int row = 0;
    foreach (const PatchList::Row &r, m_list.rows())
    {
        const QString make = r.makeModel();
        const QString conn = r.network
            ? tr("%1 %2").arg(r.transport, r.ip.isEmpty() ? tr("(no IP)") : r.ip)
            : tr("DMX");
        const QStringList cells = { r.patchText(), r.name, make, r.mode, QString::number(r.channels),
                                    conn, r.output, r.mountedOn };
        for (int c = 0; c < cells.size(); ++c)
            m_table->setItem(row, c, new QTableWidgetItem(cells.at(c)));
        row++;
    }
    m_table->resizeColumnsToContents();
    root->addWidget(m_table, 1);

    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Close, this);
    QPushButton *copyBtn = btns->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
    copyBtn->setToolTip(tr("Copy the list to the clipboard, ready to paste into a spreadsheet"));
    QPushButton *csvBtn = btns->addButton(tr("Save CSV..."), QDialogButtonBox::ActionRole);
    csvBtn->setObjectName(QStringLiteral("patchListCsv"));
    csvBtn->setToolTip(tr("Save every column as a spreadsheet"));
    QPushButton *pdfBtn = btns->addButton(tr("Save PDF..."), QDialogButtonBox::ActionRole);
    pdfBtn->setObjectName(QStringLiteral("patchListPdf"));
    pdfBtn->setToolTip(tr("Save a printable list, a table per universe"));
    QPushButton *labelsBtn = btns->addButton(tr("Labels..."), QDialogButtonBox::ActionRole);
    labelsBtn->setObjectName(QStringLiteral("patchListLabels"));
    labelsBtn->setToolTip(tr("Save a label per fixture (all, or the selected rows) as a PDF to print"));
    root->addWidget(btns);

    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(copyBtn, &QPushButton::clicked, this, &PatchListDialog::slotCopy);
    connect(csvBtn, &QPushButton::clicked, this, &PatchListDialog::slotSaveCsv);
    connect(pdfBtn, &QPushButton::clicked, this, &PatchListDialog::slotSavePdf);
    connect(labelsBtn, &QPushButton::clicked, this, &PatchListDialog::slotLabels);
}

bool PatchListDialog::saveCsv(const QString &path, QString *error) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        if (error != NULL)
            *error = file.errorString();
        return false;
    }
    QTextStream out(&file);
    out << m_list.toCsv();
    return true;
}

void PatchListDialog::slotCopy()
{
    QApplication::clipboard()->setText(m_list.toCsv());
}

/* "show-patch-list.csv" beside nothing in particular: the dialog remembers
   the folder itself. */
static QString suggestedName(const QString &ext)
{
    const QString show = QFileInfo(showFileName()).completeBaseName();
    return show.isEmpty() ? QStringLiteral("patch-list.%1").arg(ext)
                          : QStringLiteral("%1-patch-list.%2").arg(show, ext);
}

void PatchListDialog::slotSaveCsv()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Patch List"),
                                                      suggestedName(QStringLiteral("csv")),
                                                      tr("CSV files (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!saveCsv(path, &error))
        QMessageBox::warning(this, tr("Save Patch List"),
                             tr("Could not save %1:\n%2").arg(path, error));
}

void PatchListDialog::slotSavePdf()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Patch List"),
                                                      suggestedName(QStringLiteral("pdf")),
                                                      tr("PDF files (*.pdf)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_list.writePdf(path, m_title, &error))
        QMessageBox::warning(this, tr("Save Patch List"),
                             tr("Could not save %1:\n%2").arg(path, error));
}

QList<PatchList::Row> PatchListDialog::selectedRows() const
{
    QList<int> picked;
    foreach (const QModelIndex &idx, m_table->selectionModel()->selectedRows())
        picked << idx.row();
    std::sort(picked.begin(), picked.end());
    const QList<PatchList::Row> all = m_list.rows();
    QList<PatchList::Row> out;
    foreach (int r, picked)
        if (r >= 0 && r < all.size())
            out << all.at(r);
    return out;
}

void PatchListDialog::slotLabels()
{
    const QString show = showFileName();
    const QString title = show.isEmpty() ? tr("Fixture labels") : tr("Fixture labels — %1").arg(show);
    PatchLabelsDialog dlg(m_list.rows(), selectedRows(), title,
                          suggestedName(QStringLiteral("pdf")).replace(QStringLiteral("patch-list"),
                                                                       QStringLiteral("labels")),
                          this);
    dlg.exec();
}
