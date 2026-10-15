/*
  Q Light Controller Plus
  patchlabelsdialog.cpp

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
#include <QRadioButton>
#include <QButtonGroup>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QComboBox>
#include <QSettings>
#include <QLabel>

#include "patchlabelsdialog.h"
#include "patchlabels.h"

PatchLabelsDialog::PatchLabelsDialog(const QList<PatchList::Row> &all,
                                     const QList<PatchList::Row> &selected,
                                     const QString &title, const QString &suggestedFile,
                                     QWidget *parent)
    : QDialog(parent)
    , m_all(all)
    , m_selected(selected)
    , m_title(title)
    , m_suggestedFile(suggestedFile)
{
    setWindowTitle(tr("Fixture Labels"));
    setProperty("helpTopic", QStringLiteral("fixture-manager.md#labels"));

    QVBoxLayout *root = new QVBoxLayout(this);
    QFormLayout *form = new QFormLayout;
    root->addLayout(form);

    m_sheet = new QComboBox(this);
    m_sheet->setObjectName(QStringLiteral("labelSheet"));
    foreach (const PatchLabels::Sheet &s, PatchLabels::sheets())
        m_sheet->addItem(s.name, s.key);
    form->addRow(tr("Stock:"), m_sheet);

    m_allRadio = new QRadioButton(m_all.size() == 1 ? tr("The one fixture")
                                                    : tr("All %1 fixtures").arg(m_all.size()), this);
    m_allRadio->setObjectName(QStringLiteral("labelAll"));
    m_selectedRadio = new QRadioButton(m_selected.size() == 1
                                           ? tr("The selected row")
                                           : tr("The %1 selected rows").arg(m_selected.size()), this);
    m_selectedRadio->setObjectName(QStringLiteral("labelSelected"));
    m_selectedRadio->setEnabled(!m_selected.isEmpty());
    QButtonGroup *group = new QButtonGroup(this);
    group->addButton(m_allRadio);
    group->addButton(m_selectedRadio);
    QVBoxLayout *which = new QVBoxLayout;
    which->addWidget(m_allRadio);
    which->addWidget(m_selectedRadio);
    form->addRow(tr("Fixtures:"), which);
    // A selection says which ones; none means all of them.
    (m_selected.isEmpty() ? m_allRadio : m_selectedRadio)->setChecked(true);

    m_count = new QLabel(this);
    m_count->setObjectName(QStringLiteral("labelCount"));
    root->addWidget(m_count);

    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    QPushButton *save = btns->addButton(tr("Save Labels PDF..."), QDialogButtonBox::AcceptRole);
    save->setObjectName(QStringLiteral("saveLabelsPdf"));
    save->setToolTip(tr("Save the labels as a PDF to print (from Preview, at 100%)"));
    save->setDefault(true);
    root->addWidget(btns);

    /* The stock last used: what is in the printer. */
    setSheetKey(QSettings().value(SETTINGS_PATCH_LABEL_SHEET,
                                  PatchLabels::defaultSheetKey()).toString());
    slotUpdateCount();

    connect(m_sheet, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        QSettings().setValue(SETTINGS_PATCH_LABEL_SHEET, sheetKey());
        slotUpdateCount();
    });
    connect(m_allRadio, &QRadioButton::toggled, this, &PatchLabelsDialog::slotUpdateCount);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, this, &PatchLabelsDialog::slotSave);
}

QString PatchLabelsDialog::sheetKey() const
{
    return m_sheet->currentData().toString();
}

void PatchLabelsDialog::setSheetKey(const QString &key)
{
    int idx = m_sheet->findData(PatchLabels::sheet(key).key);
    m_sheet->setCurrentIndex(qMax(0, idx));
}

bool PatchLabelsDialog::selectedOnly() const
{
    return m_selectedRadio->isChecked();
}

void PatchLabelsDialog::setSelectedOnly(bool only)
{
    if (only && m_selectedRadio->isEnabled())
        m_selectedRadio->setChecked(true);
    else
        m_allRadio->setChecked(true);
}

QList<PatchList::Row> PatchLabelsDialog::rows() const
{
    return selectedOnly() ? m_selected : m_all;
}

void PatchLabelsDialog::slotUpdateCount()
{
    const PatchLabels::Sheet s = PatchLabels::sheet(sheetKey());
    const int n = rows().size();
    const int pages = PatchLabels::pageCount(s, n);
    const QString labels = n == 1 ? tr("1 label") : tr("%1 labels").arg(n);
    if (s.perPage() > 1)
        m_count->setText(pages == 1 ? tr("%1, on 1 sheet").arg(labels)
                                    : tr("%1, on %2 sheets").arg(labels).arg(pages));
    else
        m_count->setText(labels);
}

bool PatchLabelsDialog::savePdf(const QString &path, QString *error) const
{
    return PatchLabels::writePdf(path, rows(), PatchLabels::sheet(sheetKey()), m_title, error);
}

void PatchLabelsDialog::slotSave()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Labels"), m_suggestedFile,
                                                      tr("PDF files (*.pdf)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!savePdf(path, &error))
    {
        QMessageBox::warning(this, tr("Save Labels"), tr("Could not save %1:\n%2").arg(path, error));
        return;
    }
    accept();
}
