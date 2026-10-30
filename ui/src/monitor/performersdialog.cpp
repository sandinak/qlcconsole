/*
  Q Light Controller Plus
  performersdialog.cpp

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
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>

#include "performersdialog.h"
#include "feetinchesspinbox.h"
#include "colourswatchbutton.h"
#include "monitorproperties.h"
#include "stageplatform.h"

PerformersDialog::PerformersDialog(MonitorProperties *props, const QList<quint32> &platformIds,
                                   QWidget *parent)
    : QDialog(parent)
    , m_props(props)
    , m_platformIds(platformIds)
{
    setObjectName(QStringLiteral("performersDialog"));
    setWindowTitle(fills() ? tr("Fill with People") : tr("Performer Line"));
    setProperty("helpTopic", QStringLiteral("lighting-studio.md#people-on-the-stage"));
    m_feet = (props != nullptr && props->gridUnits() == MonitorProperties::Feet);
    const double toDisp = m_feet ? 3.28084 : 1.0;

    QVBoxLayout *vl = new QVBoxLayout(this);
    QFormLayout *form = new QFormLayout;
    vl->addLayout(form);

    QString defName = tr("Performers");
    if (platformIds.size() == 1 && props != nullptr && props->platform(platformIds.first()) != nullptr
        && !props->platform(platformIds.first())->name().isEmpty())
        defName = tr("%1 people").arg(props->platform(platformIds.first())->name());
    m_name = new QLineEdit(defName, this);
    m_name->setObjectName(QStringLiteral("performersSetName"));
    m_name->setToolTip(fills() ? tr("The set's name: one group in the Performers layer, "
                                    "a line for each row in it")
                               : tr("The line's name"));
    form->addRow(fills() ? tr("Set name:") : tr("Name:"), m_name);

    m_by = new QComboBox(this);
    m_by->setObjectName(QStringLiteral("performersBy"));
    m_by->addItem(tr("Space them"));
    m_by->addItem(tr("So many to a row"));
    form->addRow(tr("Place:"), m_by);

    m_spacing = new FeetInchesSpinBox(m_feet, this);
    m_spacing->setObjectName(QStringLiteral("performersSpacing"));
    m_spacing->setRange(0.3 * toDisp, 3.0 * toDisp);
    m_spacing->setSingleStep(m_feet ? 1.0 / 12.0 : 0.05);
    m_spacing->setValue(0.6 * toDisp);
    m_spacing->setToolTip(tr("Shoulder to shoulder is about 0.5 m; a choir stands about 0.6 m apart"));
    form->addRow(tr("Apart:"), m_spacing);

    m_perRow = new QSpinBox(this);
    m_perRow->setObjectName(QStringLiteral("performersPerRow"));
    m_perRow->setRange(1, 60);
    m_perRow->setValue(6);
    form->addRow(tr("To a row:"), m_perRow);

    m_stagger = new QCheckBox(tr("Stagger the rows (every face in a window)"), this);
    m_stagger->setObjectName(QStringLiteral("performersStagger"));
    m_stagger->setChecked(true);
    form->addRow(QString(), m_stagger);

    m_lineCount = new QSpinBox(this);
    m_lineCount->setObjectName(QStringLiteral("performersCountOnLine"));
    m_lineCount->setRange(1, 100);
    m_lineCount->setValue(8);
    m_lineCount->setToolTip(tr("Spread evenly along the line: one on each end, the rest between"));
    form->addRow(tr("People:"), m_lineCount);

    m_heightPreset = new QComboBox(this);
    m_heightPreset->setObjectName(QStringLiteral("performersHeightPreset"));
    m_heightPreset->addItem(tr("Children"), int(PerformerFill::Children));
    m_heightPreset->addItem(tr("Students"), int(PerformerFill::Students));
    m_heightPreset->addItem(tr("Adults"), int(PerformerFill::Adults));
    m_heightPreset->setCurrentIndex(1);
    m_height = new FeetInchesSpinBox(m_feet, this);
    m_height->setObjectName(QStringLiteral("performersHeight"));
    m_height->setRange(0.8 * toDisp, 2.2 * toDisp);
    m_height->setSingleStep(m_feet ? 1.0 / 12.0 : 0.01);
    m_height->setValue(double(PerformerFill::presetHeight(PerformerFill::Students)) * toDisp);
    m_height->setToolTip(tr("How tall they are on average"));
    QHBoxLayout *heightRow = new QHBoxLayout;
    heightRow->addWidget(m_heightPreset);
    heightRow->addWidget(m_height);
    form->addRow(tr("Height:"), heightRow);
    connect(m_heightPreset, QOverload<int>::of(&QComboBox::activated), this, [this, toDisp](int) {
        m_height->setValue(double(PerformerFill::presetHeight(
            PerformerFill::HeightPreset(m_heightPreset->currentData().toInt()))) * toDisp);
    });

    m_spread = new FeetInchesSpinBox(m_feet, this);
    m_spread->setObjectName(QStringLiteral("performersSpread"));
    m_spread->setRange(0.0, 0.3 * toDisp);
    m_spread->setSingleStep(m_feet ? 1.0 / 12.0 : 0.01);
    m_spread->setValue(0.08 * toDisp);
    m_spread->setToolTip(tr("Some are taller than others: each person is the height above, "
                            "give or take this much, and keeps it (Shuffle in the line's "
                            "dialog picks again)"));
    form->addRow(tr("Give or take:"), m_spread);

    m_costume = new ColourSwatchButton(QColor(), tr("Costume colour"), this);
    m_costume->setObjectName(QStringLiteral("performersCostume"));
    m_costume->setResetColour(QColor());
    m_costume->setToolTip(tr("What they wear: the rig view lights the body in this colour, "
                             "so a wash that suits the costume shows it. None: a neutral body."));
    form->addRow(tr("Costume:"), m_costume);

    m_replace = new QCheckBox(tr("Replace the people already on them"), this);
    m_replace->setObjectName(QStringLiteral("performersReplace"));
    m_replace->setChecked(true);
    m_replace->setToolTip(tr("Performer lines and people in the Performers layer standing on these "
                             "risers go first. One an aim palette uses is kept."));
    form->addRow(QString(), m_replace);

    m_count = new QLabel(this);
    m_count->setObjectName(QStringLiteral("performersCount"));
    m_count->setWordWrap(true);
    vl->addWidget(m_count);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vl->addWidget(bb);

    // Only the rows that mean something for filling risers, or for a line.
    form->setRowVisible(m_by, fills());
    form->setRowVisible(m_stagger, fills());
    form->setRowVisible(m_replace, fills());
    form->setRowVisible(m_lineCount, !fills());
    auto byChanged = [this, form]() {
        const bool spaced = m_by->currentIndex() == 0;
        form->setRowVisible(m_spacing, fills() && spaced);
        form->setRowVisible(m_perRow, fills() && !spaced);
        m_stagger->setEnabled(spaced);
        refreshCount();
    };
    connect(m_by, QOverload<int>::of(&QComboBox::currentIndexChanged), this, byChanged);
    connect(m_spacing, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() { refreshCount(); });
    connect(m_perRow, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { refreshCount(); });
    connect(m_lineCount, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { refreshCount(); });
    connect(m_stagger, &QCheckBox::toggled, this, [this]() { refreshCount(); });
    byChanged();
}

PerformerFill::Options PerformersDialog::options() const
{
    PerformerFill::Options o;
    o.setName = m_name->text();
    const double fromDisp = m_feet ? (1.0 / 3.28084) : 1.0;
    o.spacing = float(m_spacing->value() * fromDisp);
    o.perRow = (m_by->currentIndex() == 1) ? m_perRow->value() : 0;
    o.stagger = m_stagger->isChecked();
    o.height = float(m_height->value() * fromDisp);
    o.spread = float(m_spread->value() * fromDisp);
    o.costume = m_costume->colour();
    o.replace = m_replace->isChecked();
    return o;
}

int PerformersDialog::lineCount() const
{
    return m_lineCount->value();
}

int PerformersDialog::count() const
{
    if (!fills())
        return m_lineCount->value();
    int n = 0;
    const PerformerFill::Options o = options();
    foreach (quint32 pid, m_platformIds)
        foreach (const PerformerFill::Row &r, PerformerFill::rows(m_props, m_props->platform(pid), o))
            n += r.count;
    return n;
}

void PerformersDialog::refreshCount()
{
    if (!fills())
    {
        m_count->setText(tr("%n person(s) spread evenly along the line, each standing on whatever "
                            "is under them.", "", count()));
        return;
    }
    int rowsMade = 0;
    const PerformerFill::Options o = options();
    foreach (quint32 pid, m_platformIds)
        rowsMade += PerformerFill::rows(m_props, m_props->platform(pid), o).size();
    m_count->setText(tr("%n person(s)", "", count())
                     + QStringLiteral(" ")
                     + tr("in %n performer line(s), one to a row, facing the house, in the "
                          "Performers layer.", "", rowsMade));
}
