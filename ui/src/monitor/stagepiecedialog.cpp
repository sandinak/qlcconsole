/*
  Q Light Controller Plus
  stagepiecedialog.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QCheckBox>
#include <QLabel>
#include <cmath>

#include "stagepiecedialog.h"
#include "feetinchesspinbox.h"
#include "monitorproperties.h"

typedef MonitorProperties MP;

StagePieceDialog::StagePieceDialog(MonitorProperties *props, int piece, QWidget *parent)
    : QDialog(parent)
    , m_props(props)
    , m_piece(piece)
    // The show's unit as it is now: entry and readouts alike (Q147).
    , m_feet(props->gridUnits() == MP::Feet)
{
    setObjectName(QStringLiteral("stagePieceDialog"));
    setProperty("helpTopic", QStringLiteral("lighting-studio.md#stage-marks-centre-line-curtain-line-number-line"));
    const MP::StagePiece sp = MP::StagePiece(piece);
    setWindowTitle(tr("%1 Properties").arg(MP::stagePieceName(sp)));
    setMinimumWidth(480);

    QVBoxLayout *vl = new QVBoxLayout(this);
    QFormLayout *form = new QFormLayout;
    vl->addLayout(form);

    m_visible = new QCheckBox(tr("Show on the plot and in the Rig Overview"), this);
    m_visible->setObjectName(QStringLiteral("stagePieceVisible"));
    form->addRow(tr("Shown:"), m_visible);
    m_locked = new QCheckBox(tr("Locked: not dragged on the plot"), this);
    m_locked->setObjectName(QStringLiteral("stagePieceLocked"));
    form->addRow(tr("Lock:"), m_locked);
    connect(m_visible, &QCheckBox::toggled, this, [this, sp](bool on) {
        if (m_syncing) return;
        m_props->setStagePieceVisible(sp, on);
        emit changed();
        sync();
    });
    connect(m_locked, &QCheckBox::toggled, this, [this, sp](bool on) {
        if (m_syncing) return;
        m_props->setStagePieceLocked(sp, on);
        emit changed();
    });

    switch (sp)
    {
        case MP::CentrePiece:
        {
            m_centreMark = new QCheckBox(tr("DSC mark at the front of the stage, labelled CL"), this);
            m_centreMark->setObjectName(QStringLiteral("centreMark"));
            m_centreLine = new QCheckBox(tr("The centre line, dashed, from the front to the back"), this);
            m_centreLine->setObjectName(QStringLiteral("centreLine"));
            form->addRow(tr("Mark:"), m_centreMark);
            form->addRow(tr("Line:"), m_centreLine);
            auto part = [this](MP::StageMark flag, bool on) {
                if (m_syncing) return;
                m_props->setStageMarkVisible(flag, on);
                emit changed();
                sync();
            };
            connect(m_centreMark, &QCheckBox::toggled, this,
                    [part](bool on) { part(MP::CentreMarkFlag, on); });
            connect(m_centreLine, &QCheckBox::toggled, this,
                    [part](bool on) { part(MP::CentreLineFlag, on); });
        }
        break;
        case MP::CurtainPiece:
            m_value = addDistance(form, tr("Upstage of the front:"), QStringLiteral("curtainLineY"),
                                  tr("How far upstage of the front of the deck the curtain line is. "
                                     "Negative is downstage of it. Auto: the most downstage main drape, "
                                     "else the proscenium, else 4 ft (1.2 m) upstage of the front."),
                                  &m_auto, &m_words);
            connect(m_auto, &QCheckBox::toggled, this, [this](bool on) {
                if (m_syncing) return;
                if (on) m_props->setCurtainLineAuto();
                else m_props->setCurtainLineY(m_props->curtainLineY());
                emit changed();
                sync();
            });
            connect(m_value, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
                if (m_syncing) return;
                m_props->setCurtainLineUpstage(toMetres(v));
                emit changed();
                sync();
            });
        break;
        case MP::NumberPiece:
            m_value = addDistance(form, tr("Downstage of the curtain line:"), QStringLiteral("numberLineY"),
                                  tr("How far downstage of the curtain line the number line is. Negative "
                                     "is upstage of it. Auto puts it at the front of the stage."),
                                  &m_auto, &m_words);
            connect(m_auto, &QCheckBox::toggled, this, [this](bool on) {
                if (m_syncing) return;
                if (on) m_props->setNumberLineAuto();
                else m_props->setNumberLineY(m_props->numberLineY());
                emit changed();
                sync();
            });
            connect(m_value, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
                if (m_syncing) return;
                m_props->setNumberLineDownstageOfCurtain(toMetres(v));
                emit changed();
                sync();
            });
        break;
        case MP::ApronPiece:
        {
            m_value = addDistance(form, tr("Depth:"), QStringLiteral("apronDepth"),
                                  tr("How deep the apron is, from the curtain line to its front. Auto "
                                     "reaches the front of the main deck. It is its own deck: no other "
                                     "platform is moved or resized."),
                                  &m_auto, &m_words);
            m_value->setMinimum(0.0);
            m_curve = new FeetInchesSpinBox(m_feet, this);
            m_curve->setObjectName(QStringLiteral("apronFrontCurve"));
            m_curve->setRange(0.0, toDisplay(50.0f));
            m_curve->setKeyboardTracking(false);
            m_curve->setToolTip(tr("How far the middle of the apron's front reaches past its corners: "
                                   "0 is a straight front"));
            form->addRow(tr("Front curve:"), m_curve);
            connect(m_auto, &QCheckBox::toggled, this, [this](bool on) {
                if (m_syncing) return;
                m_props->setApronAuto(on);
                emit changed();
                sync();
            });
            connect(m_value, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
                if (m_syncing) return;
                m_props->setApronDepth(qMax(0.0f, toMetres(v)));
                emit changed();
                sync();
            });
            connect(m_curve, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
                if (m_syncing) return;
                m_props->setApronFrontCurve(qMax(0.0f, toMetres(v)));
                emit changed();
                sync();
            });
        }
        break;
    }

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vl->addWidget(bb);
    sync();
}

double StagePieceDialog::toDisplay(float metres) const
{
    return double(metres) * (m_feet ? 1.0 / 0.3048 : 1.0);
}

float StagePieceDialog::toMetres(double shown) const
{
    return float(shown * (m_feet ? 0.3048 : 1.0));
}

FeetInchesSpinBox *StagePieceDialog::addDistance(QFormLayout *form, const QString &title,
                                                 const QString &name, const QString &tip,
                                                 QCheckBox **autoBox, QLabel **words)
{
    QWidget *row = new QWidget(this);
    QGridLayout *gl = new QGridLayout(row);
    gl->setContentsMargins(0, 0, 0, 0);
    gl->setVerticalSpacing(1);
    QCheckBox *a = new QCheckBox(tr("Auto"), row);
    a->setObjectName(name + QStringLiteral("Auto"));
    FeetInchesSpinBox *spin = new FeetInchesSpinBox(m_feet, row);
    spin->setObjectName(name);
    spin->setRange(toDisplay(-100.0f), toDisplay(300.0f));
    spin->setKeyboardTracking(false);       // one change when typed, not one a key
    spin->setToolTip(tip);
    QLabel *said = new QLabel(row);
    said->setObjectName(name + QStringLiteral("Words"));
    QFont small = said->font();
    small.setPointSizeF(small.pointSizeF() * 0.9);
    said->setFont(small);
    gl->addWidget(a, 0, 0);
    gl->addWidget(spin, 0, 1);
    gl->addWidget(said, 1, 1);
    gl->setColumnStretch(1, 1);
    form->addRow(title, row);
    *autoBox = a;
    *words = said;
    return spin;
}

void StagePieceDialog::sync()
{
    m_syncing = true;
    const MP::StagePiece sp = MP::StagePiece(m_piece);
    const MP::GridUnits units = m_feet ? MP::Feet : MP::Meters;
    m_visible->setChecked(m_props->stagePieceVisible(sp));
    m_locked->setChecked(m_props->stagePieceLocked(sp));
    switch (sp)
    {
        case MP::CentrePiece:
            m_centreMark->setChecked(m_props->stageMarkVisible(MP::CentreMarkFlag));
            m_centreLine->setChecked(m_props->stageMarkVisible(MP::CentreLineFlag));
        break;
        case MP::CurtainPiece:
        {
            m_auto->setChecked(m_props->curtainLineIsAuto());
            m_value->setValue(toDisplay(m_props->curtainLineUpstage()));
            m_value->setEnabled(!m_props->curtainLineIsAuto());
            const float up = m_props->curtainLineUpstage();
            m_words->setText(std::fabs(up) < 0.005f ? tr("at the front of the deck")
                             : (up > 0.0f ? tr("%1 US of the front of the deck")
                                          : tr("%1 DS of the front of the deck"))
                                   .arg(MP::formatStageDistance(up, units)));
        }
        break;
        case MP::NumberPiece:
            m_auto->setChecked(m_props->numberLineIsAuto());
            m_value->setValue(toDisplay(m_props->numberLineDownstageOfCurtain()));
            m_value->setEnabled(!m_props->numberLineIsAuto());
            m_words->setText(MP::formatCurtainLineOffset(m_props->numberLineDownstageOfCurtain(), units));
        break;
        case MP::ApronPiece:
        {
            m_auto->setChecked(m_props->apronAuto());
            m_value->setValue(toDisplay(m_props->apronDepth()));
            m_value->setEnabled(!m_props->apronAuto());
            m_curve->setValue(toDisplay(m_props->apronFrontCurve()));
            m_curve->setEnabled(!m_props->apronAuto());
            const float depth = m_props->apronDepth();
            m_words->setText(depth < 0.01f ? tr("No apron: the deck stops at the curtain line")
                                           : tr("Front %1 DS of the curtain line")
                                                 .arg(MP::formatStageDistance(depth, units)));
        }
        break;
    }
    m_syncing = false;
}
