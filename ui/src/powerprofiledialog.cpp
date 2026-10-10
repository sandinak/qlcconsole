/*
  Q Light Controller Plus
  powerprofiledialog.cpp

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
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QSet>

#include "powerprofiledialog.h"
#include "qlcfixturemode.h"
#include "inputoutputmap.h"
#include "powerlimiter.h"
#include "genericfader.h"
#include "fadechannel.h"
#include "qlcchannel.h"
#include "universe.h"
#include "fixture.h"
#include "doc.h"

static const QList<int> kColourOrder = {
    int(QLCChannel::Red), int(QLCChannel::Green), int(QLCChannel::Blue), int(QLCChannel::White),
    int(QLCChannel::Amber), int(QLCChannel::UV), int(QLCChannel::Lime), int(QLCChannel::Indigo),
    int(QLCChannel::Cyan), int(QLCChannel::Magenta), int(QLCChannel::Yellow), int(QLCChannel::NoColour) };

/* The model for what a fixture emits, whatever its readings: built with a
   stand-in reading so its emitters are found. */
static PowerModel shapeOf(const Fixture *fx)
{
    PowerProfile any;
    any.fullAmps = 1.0;
    return PowerModel::build(fx, any);
}

static int emitterColour(const Fixture *fx, int offset)
{
    const QLCChannel *ch = (fx->fixtureMode() != nullptr && offset < fx->fixtureMode()->channels().size())
                           ? fx->fixtureMode()->channels().at(offset) : nullptr;
    return ch != nullptr ? int(ch->colour()) : int(QLCChannel::NoColour);
}

QList<int> powerColours(const Fixture *fx)
{
    QSet<int> present;
    if (fx != nullptr)
    {
        foreach (const PowerModel::Emitter &e, shapeOf(fx).emitters)
            present.insert(emitterColour(fx, e.offset));
    }
    QList<int> out;
    foreach (int c, kColourOrder)
        if (present.contains(c))
            out << c;
    return out;
}

QString powerColourName(int colour)
{
    if (colour == int(QLCChannel::NoColour))
        return QObject::tr("Lamp");
    return QLCChannel::colourToString(QLCChannel::PrimaryColour(colour));
}

static QDoubleSpinBox *ampsSpin(QWidget *parent, double value, const QString &none)
{
    QDoubleSpinBox *sp = new QDoubleSpinBox(parent);
    sp->setRange(0.0, 100.0);
    sp->setDecimals(2);
    sp->setSingleStep(0.1);
    sp->setSuffix(QStringLiteral(" A"));
    sp->setSpecialValueText(none);
    sp->setValue(value);
    return sp;
}

/*****************************************************************************
 * PowerProfileDialog
 *****************************************************************************/

PowerProfileDialog::PowerProfileDialog(Doc *doc, const QList<quint32> &fids, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_fids(fids)
{
    setWindowTitle(fids.size() > 1 ? tr("Power — %n fixtures", "", fids.size()) : tr("Power"));
    Fixture *first = fids.isEmpty() ? nullptr : doc->fixture(fids.first());
    const PowerProfile pp = (first != nullptr) ? first->powerProfile() : PowerProfile();

    QFormLayout *form = new QFormLayout;
    QLabel *intro = new QLabel(tr("Meter readings for the whole fixture, standby included. Leave a "
                                  "colour at \"not measured\" and it shares what is left of the "
                                  "everything-at-full reading."), this);
    intro->setWordWrap(true);

    // The colours any of them emit.
    QSet<int> present;
    foreach (quint32 fid, fids)
    {
        if (Fixture *fx = doc->fixture(fid))
        {
            foreach (int c, powerColours(fx))
                present.insert(c);
        }
    }
    foreach (int c, kColourOrder)
    {
        if (!present.contains(c))
            continue;
        QDoubleSpinBox *sp = ampsSpin(this, pp.colourAmps.value(c, 0.0), tr("not measured"));
        sp->setObjectName(QStringLiteral("powerColour_%1").arg(powerColourName(c)));
        m_colour.insert(c, sp);
        form->addRow(tr("%1 alone, full:").arg(powerColourName(c)), sp);
    }
    m_full = ampsSpin(this, pp.fullAmps, tr("not measured"));
    m_full->setObjectName("powerFull");
    m_half = ampsSpin(this, pp.halfAmps, tr("not measured"));
    m_half->setObjectName("powerHalf");
    m_half->setToolTip(tr("Everything at half: how the draw falls with level"));
    m_standby = ampsSpin(this, pp.standbyAmps, tr("none"));
    m_standby->setObjectName("powerStandby");
    m_max = ampsSpin(this, pp.maxAmps, tr("No limit"));
    m_max->setObjectName("powerMax");
    m_max->setToolTip(tr("The most it may draw: its output is scaled down -- every colour "
                         "together -- whenever what it is sent would draw more"));
    form->addRow(tr("Everything, full:"), m_full);
    form->addRow(tr("Everything, half:"), m_half);
    form->addRow(tr("Everything off:"), m_standby);
    form->addRow(tr("Maximum draw:"), m_max);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, [this]() { apply(); accept(); });
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    QVBoxLayout *vl = new QVBoxLayout(this);
    vl->addWidget(intro);
    vl->addLayout(form);
    if (fids.size() > 1)
        vl->addWidget(new QLabel(tr("Set on all %1 fixtures.").arg(fids.size()), this));
    vl->addWidget(bb);
}

PowerProfile PowerProfileDialog::profile() const
{
    PowerProfile pp;
    for (auto it = m_colour.constBegin(); it != m_colour.constEnd(); ++it)
        if (it.value()->value() > 0.0)
            pp.colourAmps.insert(it.key(), it.value()->value());
    pp.fullAmps = m_full->value();
    pp.halfAmps = m_half->value();
    pp.standbyAmps = m_standby->value();
    pp.maxAmps = m_max->value();
    return pp;
}

void PowerProfileDialog::apply()
{
    const PowerProfile pp = profile();
    foreach (quint32 fid, m_fids)
        if (Fixture *fx = m_doc->fixture(fid))
            fx->setPowerProfile(pp);
    m_doc->setModified();
}

/*****************************************************************************
 * PowerMeasureDialog
 *****************************************************************************/

PowerMeasureDialog::PowerMeasureDialog(Doc *doc, const QList<quint32> &fids, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_fids(fids)
    , m_fixture(fids.isEmpty() ? nullptr : doc->fixture(fids.first()))
    , m_universe(Universe::invalid())
{
    setWindowTitle(tr("Measure Power — %1").arg(m_fixture != nullptr ? m_fixture->name() : QString()));

    // The states to read.
    m_steps << Step{ tr("Everything off"), -1, 0, true };
    foreach (int c, powerColours(m_fixture))
        m_steps << Step{ tr("%1 alone, at full").arg(powerColourName(c)), c, 255 };
    m_steps << Step{ tr("Everything at half"), -1, 128 };
    m_steps << Step{ tr("Everything at full"), -1, 255 };
    m_readings.fill(0.0, m_steps.size());

    m_title = new QLabel(this);
    m_title->setStyleSheet("font-weight:bold;");
    m_what = new QLabel(this);
    m_what->setWordWrap(true);
    m_amps = ampsSpin(this, 0.0, QString());
    m_amps->setObjectName("measureAmps");
    connect(m_amps, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (m_step >= 0 && m_step < m_readings.size())
            m_readings[m_step] = v;
    });
    QLabel *note = new QLabel(tr("It drives this fixture's dimmers and colours over everything "
                                 "else; its other channels are left as they are, so stop what "
                                 "else is running on it first."), this);
    note->setWordWrap(true);
    note->setStyleSheet("color:#8a8e99;");

    m_back = new QPushButton(tr("Back"), this);
    m_next = new QPushButton(tr("Next"), this);
    m_next->setObjectName("measureNext");
    QPushButton *cancel = new QPushButton(tr("Cancel"), this);
    connect(m_back, &QPushButton::clicked, this, [this]() { showStep(m_step - 1); });
    connect(m_next, &QPushButton::clicked, this, [this]() {
        if (m_step + 1 < m_steps.size())
            showStep(m_step + 1);
        else
        {
            apply();
            accept();
        }
    });
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    m_all = new QCheckBox(tr("Use these readings for all %1 selected fixtures").arg(fids.size()), this);
    m_all->setObjectName("measureAll");
    m_all->setChecked(fids.size() > 1);
    m_all->setVisible(fids.size() > 1);

    QHBoxLayout *readRow = new QHBoxLayout;
    readRow->addWidget(new QLabel(tr("Meter reads:"), this));
    readRow->addWidget(m_amps);
    readRow->addStretch();
    QHBoxLayout *buttons = new QHBoxLayout;
    buttons->addWidget(cancel);
    buttons->addStretch();
    buttons->addWidget(m_back);
    buttons->addWidget(m_next);
    QVBoxLayout *vl = new QVBoxLayout(this);
    vl->addWidget(m_title);
    vl->addWidget(m_what);
    vl->addLayout(readRow);
    vl->addWidget(note);
    vl->addWidget(m_all);
    vl->addLayout(buttons);

    connect(this, &QDialog::finished, this, [this](int) { release(); });
    if (m_fixture != nullptr && m_doc->powerLimiter() != nullptr)
        m_doc->powerLimiter()->setMeasuring(m_fixture->id(), true);
    showStep(0);
}

PowerMeasureDialog::~PowerMeasureDialog()
{
    release();
}

void PowerMeasureDialog::showStep(int i)
{
    if (i < 0 || i >= m_steps.size())
        return;
    m_step = i;
    const Step &st = m_steps.at(i);
    m_title->setText(tr("Step %1 of %2: %3").arg(i + 1).arg(m_steps.size()).arg(st.what));
    m_what->setText(st.off ? tr("The fixture is dark. Read its standby draw.")
                           : tr("Wait for the reading to settle, then enter it."));
    m_amps->blockSignals(true);
    m_amps->setValue(m_readings.at(i));
    m_amps->blockSignals(false);
    m_back->setEnabled(i > 0);
    m_next->setText(i + 1 < m_steps.size() ? tr("Next") : tr("Finish"));
    drive(st);
}

void PowerMeasureDialog::drive(const Step &step)
{
    if (m_fixture == nullptr || !m_fixture->isPatched())
        return;
    const PowerModel shape = shapeOf(m_fixture);
    QSet<int> dimmers;
    foreach (const PowerModel::Emitter &e, shape.emitters)
        foreach (int d, e.dimmers)
            dimmers.insert(d);

    InputOutputMap *io = m_doc->inputOutputMap();
    QList<Universe *> ua = io->claimUniverses();
    const int u = int(m_fixture->universe());
    if (u >= 0 && u < ua.size() && ua.at(u) != nullptr)
    {
        if (m_fader.isNull())
        {
            m_fader = ua.at(u)->requestFader(Universe::SimpleDesk);
            m_universe = quint32(u);
        }
        auto set = [&](int offset, uchar value) {
            FadeChannel *fc = m_fader->getChannelFader(m_doc, ua.at(u), Fixture::invalidId(),
                                                       m_fixture->address() + quint32(offset));
            fc->setCurrent(value);
            fc->setTarget(value);
            fc->addFlag(FadeChannel::Override);
        };
        foreach (int d, dimmers)
            set(d, step.off ? 0 : 255);
        foreach (const PowerModel::Emitter &e, shape.emitters)
        {
            const bool lit = !step.off && (step.colour < 0 || emitterColour(m_fixture, e.offset) == step.colour);
            set(e.offset, lit ? step.level : 0);
        }
    }
    io->releaseUniverses(false);
}

void PowerMeasureDialog::release()
{
    if (!m_fader.isNull())
    {
        InputOutputMap *io = m_doc->inputOutputMap();
        QList<Universe *> ua = io->claimUniverses();
        if (m_universe < quint32(ua.size()) && ua.at(int(m_universe)) != nullptr)
            ua.at(int(m_universe))->dismissFader(m_fader);
        io->releaseUniverses(false);
        m_fader.clear();
    }
    if (m_fixture != nullptr && m_doc->powerLimiter() != nullptr
        && m_doc->powerLimiter()->isMeasuring(m_fixture->id()))
        m_doc->powerLimiter()->setMeasuring(m_fixture->id(), false);
}

PowerProfile PowerMeasureDialog::profile() const
{
    PowerProfile pp = (m_fixture != nullptr) ? m_fixture->powerProfile() : PowerProfile();
    pp.colourAmps.clear();
    for (int i = 0; i < m_steps.size(); ++i)
    {
        const Step &st = m_steps.at(i);
        const double r = m_readings.at(i);
        if (st.off)
            pp.standbyAmps = r;
        else if (st.colour >= 0)
        {
            if (r > 0.0)
                pp.colourAmps.insert(st.colour, r);
        }
        else if (st.level == 255)
            pp.fullAmps = r;
        else
            pp.halfAmps = r;
    }
    return pp;
}

void PowerMeasureDialog::apply()
{
    const PowerProfile measured = profile();
    QList<quint32> to;
    if (m_all->isChecked())
        to = m_fids;
    else if (m_fixture != nullptr)
        to << m_fixture->id();
    foreach (quint32 fid, to)
        if (Fixture *fx = m_doc->fixture(fid))
        {
            // Their own caps stay theirs.
            PowerProfile pp = measured;
            pp.maxAmps = fx->powerProfile().maxAmps;
            fx->setPowerProfile(pp);
        }
    m_doc->setModified();
}
