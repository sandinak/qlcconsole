/*
  Q Light Controller Plus
  networkreportdialog.cpp

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
#include <QPlainTextEdit>
#include <QFontDatabase>
#include <QApplication>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QTextStream>
#include <QFile>

#include "networkreportdialog.h"
#include "monitorproperties.h"
#include "doc.h"

NetworkReportDialog::NetworkReportDialog(Doc *doc, QWidget *parent)
    : QDialog(parent)
    , m_report(NetworkReport::build(doc))
    , m_feet(doc->monitorProperties()->gridUnits() == MonitorProperties::Feet)
{
    setWindowTitle(tr("Network Report"));
    resize(640, 520);

    QVBoxLayout *root = new QVBoxLayout(this);
    m_text = new QPlainTextEdit(this);
    m_text->setReadOnly(true);
    m_text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_text->setPlainText(m_report.toText(m_feet));
    root->addWidget(m_text, 1);

    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_copyBtn = btns->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
    m_copyBtn->setToolTip(tr("Copy the summary to the clipboard"));
    m_saveBtn = btns->addButton(tr("Save CSV..."), QDialogButtonBox::ActionRole);
    m_saveBtn->setToolTip(tr("Save the ports and network fixtures as a spreadsheet"));
    root->addWidget(btns);

    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_copyBtn, &QPushButton::clicked, this, &NetworkReportDialog::slotCopy);
    connect(m_saveBtn, &QPushButton::clicked, this, &NetworkReportDialog::slotSaveCsv);
}

bool NetworkReportDialog::saveCsv(const QString &path, QString *error) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        if (error != NULL)
            *error = file.errorString();
        return false;
    }
    QTextStream out(&file);
    out << m_report.toCsv(m_feet);
    return true;
}

void NetworkReportDialog::slotCopy()
{
    QApplication::clipboard()->setText(m_text->toPlainText());
}

void NetworkReportDialog::slotSaveCsv()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Network Report"),
                                                      tr("network-report.csv"),
                                                      tr("CSV files (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!saveCsv(path, &error))
        QMessageBox::warning(this, tr("Save Network Report"),
                             tr("Could not save %1:\n%2").arg(path, error));
}
