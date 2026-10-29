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
    setWindowTitle(tr("Fill with People"));
    setProperty("helpTopic", QStringLiteral("lighting-studio.md#people-on-the-risers"));
    m_feet = (props != nullptr && props->gridUnits() == MonitorProperties::Feet);

    QVBoxLayout *vl = new QVBoxLayout(this);
    QFormLayout *form = new QFormLayout;
    vl->addLayout(form);

    QString defName = tr("Performers");
    if (platformIds.size() == 1 && props != nullptr && props->platform(platformIds.first()) != nullptr
        && !props->platform(platformIds.first())->name().isEmpty())
        defName = tr("%1 people").arg(props->platform(platformIds.first())->name());
    m_name = new QLineEdit(defName, this);
    m_name->setObjectName(QStringLiteral("performersSetName"));
    m_name->setToolTip(tr("The set's name: one group in the Performers layer, "
                          "and each person is numbered in it"));
    form->addRow(tr("Set name:"), m_name);

    m_by = new QComboBox(this);
    m_by->setObjectName(QStringLiteral("performersBy"));
    m_by->addItem(tr("Space them"));
    m_by->addItem(tr("So many to a row"));
    form->addRow(tr("Place:"), m_by);

    m_spacing = new FeetInchesSpinBox(m_feet, this);
    m_spacing->setObjectName(QStringLiteral("performersSpacing"));
    const double toDisp = m_feet ? 3.28084 : 1.0;
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

    m_height = new QComboBox(this);
    m_height->setObjectName(QStringLiteral("performersHeight"));
    auto heightText = [this](const QString &who, float m) {
        if (!m_feet)
            return tr("%1 (%2 m)").arg(who).arg(double(m), 0, 'f', 2);
        const int in = qRound(double(m) * 39.3701);
        return tr("%1 (%2' %3\")").arg(who).arg(in / 12).arg(in % 12);
    };
    m_height->addItem(heightText(tr("Children"), PerformerFill::presetHeight(PerformerFill::Children)),
                      int(PerformerFill::Children));
    m_height->addItem(heightText(tr("Students"), PerformerFill::presetHeight(PerformerFill::Students)),
                      int(PerformerFill::Students));
    m_height->addItem(heightText(tr("Adults"), PerformerFill::presetHeight(PerformerFill::Adults)),
                      int(PerformerFill::Adults));
    m_height->setCurrentIndex(1);
    form->addRow(tr("Height:"), m_height);

    m_costume = new ColourSwatchButton(QColor(), tr("Costume colour"), this);
    m_costume->setObjectName(QStringLiteral("performersCostume"));
    m_costume->setResetColour(QColor());
    m_costume->setToolTip(tr("What they wear: the rig view lights the body in this colour, "
                             "so a wash that suits the costume shows it. None: a neutral body."));
    form->addRow(tr("Costume:"), m_costume);

    m_replace = new QCheckBox(tr("Replace the people already on them"), this);
    m_replace->setObjectName(QStringLiteral("performersReplace"));
    m_replace->setChecked(true);
    m_replace->setToolTip(tr("People in the Performers layer standing on these risers go first. "
                             "One an aim palette uses is kept."));
    form->addRow(QString(), m_replace);

    m_count = new QLabel(this);
    m_count->setObjectName(QStringLiteral("performersCount"));
    m_count->setWordWrap(true);
    vl->addWidget(m_count);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vl->addWidget(bb);

    auto byChanged = [this, form]() {
        const bool spaced = m_by->currentIndex() == 0;
        form->setRowVisible(m_spacing, spaced);
        form->setRowVisible(m_perRow, !spaced);
        m_stagger->setEnabled(spaced);
        refreshCount();
    };
    connect(m_by, QOverload<int>::of(&QComboBox::currentIndexChanged), this, byChanged);
    connect(m_spacing, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() { refreshCount(); });
    connect(m_perRow, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { refreshCount(); });
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
    o.height = PerformerFill::presetHeight(PerformerFill::HeightPreset(m_height->currentData().toInt()));
    o.costume = m_costume->colour();
    o.replace = m_replace->isChecked();
    return o;
}

int PerformersDialog::count() const
{
    int n = 0;
    const PerformerFill::Options o = options();
    foreach (quint32 pid, m_platformIds)
        n += PerformerFill::spots(m_props, m_props->platform(pid), o).size();
    return n;
}

void PerformersDialog::refreshCount()
{
    m_count->setText(tr("%n person(s)", "", count())
                     + QStringLiteral(" ")
                     + tr("on %n riser(s), facing the house, in the Performers layer.", "",
                          m_platformIds.size()));
}
