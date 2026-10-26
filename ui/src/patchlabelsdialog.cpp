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
#include <QDoubleSpinBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QPrintDialog>
#include <QPrinterInfo>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QToolButton>
#include <QLineEdit>
#include <QComboBox>
#include <QSettings>
#include <QPrinter>
#include <QPixmap>
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
    m_sheet->setToolTip(tr("The Brother PT-P710BT preset prints through Brother's macOS driver "
                           "(P-touch), and picks that printer when it is there"));
    form->addRow(tr("Stock:"), m_sheet);

    m_tape = new QComboBox(this);
    m_tape->setObjectName(QStringLiteral("labelTape"));
    foreach (int mm, PatchLabels::tapeWidths())
        m_tape->addItem(tr("%1 mm (TZe)").arg(mm), mm);
    form->addRow(tr("Tape:"), m_tape);

    QHBoxLayout *text = new QHBoxLayout;
    m_template = new QLineEdit(this);
    m_template->setObjectName(QStringLiteral("labelTemplate"));
    m_template->setPlaceholderText(PatchLabels::defaultTemplate());
    m_template->setToolTip(tr("What each label says. Fields: %1. "
                              "{address} is the DMX start address, \"2.101\" when the show uses "
                              "more than one universe.")
                               .arg(PatchLabels::templateFields().join(QStringLiteral(" "))));
    text->addWidget(m_template, 1);
    QToolButton *reset = new QToolButton(this);
    reset->setObjectName(QStringLiteral("labelTemplateReset"));
    reset->setText(tr("Default"));
    reset->setToolTip(tr("Back to %1").arg(PatchLabels::defaultTemplate()));
    text->addWidget(reset);
    form->addRow(tr("Label says:"), text);
    QLabel *fields = new QLabel(tr("Fields: %1").arg(PatchLabels::templateFields().join(QStringLiteral(" "))), this);
    fields->setEnabled(false);
    form->addRow(QString(), fields);

    m_cutGap = new QDoubleSpinBox(this);
    m_cutGap->setObjectName(QStringLiteral("labelCutGap"));
    m_cutGap->setRange(0, 20);
    m_cutGap->setSingleStep(1);
    m_cutGap->setDecimals(1);
    m_cutGap->setSuffix(tr(" mm"));
    m_cutGap->setToolTip(tr("Blank tape between one label's text and the next, where it is cut"));
    form->addRow(tr("Cut gap:"), m_cutGap);

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

    m_preview = new QLabel(this);
    m_preview->setObjectName(QStringLiteral("labelPreview"));
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumHeight(90);
    root->addWidget(m_preview);

    m_count = new QLabel(this);
    m_count->setObjectName(QStringLiteral("labelCount"));
    root->addWidget(m_count);

    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    QPushButton *save = btns->addButton(tr("Save PDF..."), QDialogButtonBox::ActionRole);
    save->setObjectName(QStringLiteral("saveLabelsPdf"));
    save->setToolTip(tr("Save the labels as a PDF to print later (at 100%, actual size)"));
    QPushButton *printBtn = btns->addButton(tr("Print..."), QDialogButtonBox::AcceptRole);
    printBtn->setObjectName(QStringLiteral("printLabels"));
    printBtn->setToolTip(tr("Print the labels through the system's print dialog"));
    printBtn->setDefault(true);
    root->addWidget(btns);

    /* What was used last: what is in the printer. */
    QSettings settings;
    setSheetKey(settings.value(SETTINGS_PATCH_LABEL_SHEET, PatchLabels::defaultSheetKey()).toString());
    setTapeWidth(settings.value(SETTINGS_PATCH_LABEL_TAPE, 24).toInt());
    setLabelTemplate(settings.value(SETTINGS_PATCH_LABEL_TEMPLATE, PatchLabels::defaultTemplate()).toString());
    setCutGap(settings.value(SETTINGS_PATCH_LABEL_CUTGAP, 4.0).toDouble());
    slotUpdate();

    connect(m_sheet, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        QSettings().setValue(SETTINGS_PATCH_LABEL_SHEET, sheetKey());
        slotUpdate();
    });
    connect(m_tape, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        QSettings().setValue(SETTINGS_PATCH_LABEL_TAPE, tapeWidth());
        slotUpdate();
    });
    connect(m_template, &QLineEdit::textChanged, this, [this]() {
        QSettings().setValue(SETTINGS_PATCH_LABEL_TEMPLATE, labelTemplate());
        slotUpdate();
    });
    connect(reset, &QToolButton::clicked, this, [this]() {
        setLabelTemplate(PatchLabels::defaultTemplate());
    });
    connect(m_cutGap, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() {
        QSettings().setValue(SETTINGS_PATCH_LABEL_CUTGAP, cutGap());
        slotUpdate();
    });
    connect(m_allRadio, &QRadioButton::toggled, this, &PatchLabelsDialog::slotUpdate);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, this, &PatchLabelsDialog::slotSave);
    connect(printBtn, &QPushButton::clicked, this, &PatchLabelsDialog::slotPrint);
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

int PatchLabelsDialog::tapeWidth() const
{
    return m_tape->currentData().toInt();
}

void PatchLabelsDialog::setTapeWidth(int mm)
{
    const int idx = m_tape->findData(mm);
    m_tape->setCurrentIndex(idx >= 0 ? idx : m_tape->findData(24));
}

QString PatchLabelsDialog::labelTemplate() const
{
    const QString t = m_template->text().trimmed();
    return t.isEmpty() ? PatchLabels::defaultTemplate() : t;
}

void PatchLabelsDialog::setLabelTemplate(const QString &text)
{
    m_template->setText(text);
}

qreal PatchLabelsDialog::cutGap() const
{
    return m_cutGap->value();
}

void PatchLabelsDialog::setCutGap(qreal mm)
{
    m_cutGap->setValue(mm);
}

void PatchLabelsDialog::setMultiUniverse(bool multi)
{
    m_multiUniverse = multi;
    slotUpdate();
}

PatchLabels::Options PatchLabelsDialog::options() const
{
    PatchLabels::Options o;
    o.text = labelTemplate();
    o.multiUniverse = m_multiUniverse;
    o.cutGap = cutGap();
    return o;
}

PatchLabels::Sheet PatchLabelsDialog::sheet() const
{
    return PatchLabels::fitted(PatchLabels::sheet(sheetKey(), tapeWidth()), rows(), options());
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

void PatchLabelsDialog::slotUpdate()
{
    const PatchLabels::Sheet s = sheet();
    m_tape->setEnabled(s.tape);
    m_cutGap->setEnabled(s.tape);

    const QList<PatchList::Row> rs = rows();
    const int n = rs.size();
    const int pages = PatchLabels::pageCount(s, n);
    const QString labels = n == 1 ? tr("1 label") : tr("%1 labels").arg(n);
    if (s.tape)
        m_count->setText(tr("%1, each %2 mm of %3 mm tape").arg(labels)
                             .arg(qRound(s.page.width())).arg(qRound(s.page.height())));
    else if (s.perPage() > 1)
        m_count->setText(pages == 1 ? tr("%1, on 1 sheet").arg(labels)
                                    : tr("%1, on %2 sheets").arg(labels).arg(pages));
    else
        m_count->setText(labels);

    /* The first label as it prints, no wider than the dialog. */
    if (rs.isEmpty())
    {
        m_preview->setPixmap(QPixmap());
        m_preview->setText(tr("No fixtures to label"));
        return;
    }
    const qreal pxPerMm = qBound(2.0, 420.0 / qMax(1.0, s.label.width()), 8.0);
    const QImage img = PatchLabels::preview(rs.first(), s, options(), pxPerMm * devicePixelRatioF());
    QPixmap pm = QPixmap::fromImage(img);
    pm.setDevicePixelRatio(devicePixelRatioF());
    m_preview->setPixmap(pm);
    m_preview->setToolTip(PatchLabels::render(labelTemplate(), rs.first(), m_multiUniverse));
}

bool PatchLabelsDialog::savePdf(const QString &path, QString *error) const
{
    return PatchLabels::writePdf(path, rows(), PatchLabels::sheet(sheetKey(), tapeWidth()),
                                 m_title, options(), error);
}

void PatchLabelsDialog::setUpPrinter(QPrinter *printer) const
{
    const PatchLabels::Sheet s = sheet();
    /* The Brother preset's hook: its printer, when it is there. */
    if (!s.printer.isEmpty())
    {
        foreach (const QString &name, QPrinterInfo::availablePrinterNames())
        {
            if (name.contains(s.printer, Qt::CaseInsensitive))
            {
                printer->setPrinterName(name);
                break;
            }
        }
    }
    printer->setDocName(m_title);
    printer->setPageLayout(PatchLabels::pageLayout(s));
    printer->setFullPage(true);
}

bool PatchLabelsDialog::print(QPrinter *printer, QString *error) const
{
    return PatchLabels::paint(printer, rows(), sheet(), options(), error);
}

void PatchLabelsDialog::slotPrint()
{
    QPrinter printer(QPrinter::HighResolution);
    setUpPrinter(&printer);
    QPrintDialog dlg(&printer, this);
    dlg.setWindowTitle(tr("Print Labels"));
    if (dlg.exec() != QDialog::Accepted)
        return;
    QString error;
    if (!print(&printer, &error))
    {
        QMessageBox::warning(this, tr("Print Labels"), tr("Could not print the labels:\n%1").arg(error));
        return;
    }
    accept();
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
