/*
  Q Light Controller Plus
  powerprofiledialog.h

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

#ifndef POWERPROFILEDIALOG_H
#define POWERPROFILEDIALOG_H

#include <QSharedPointer>
#include <QDialog>
#include <QMap>

#include "fixturepower.h"

class QDoubleSpinBox;
class GenericFader;
class QPushButton;
class QCheckBox;
class Fixture;
class QLabel;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/** The colours a fixture's draw is measured by: those it emits, in a fixed
 *  order (NoColour: a plain dimmer's lamp). */
QList<int> powerColours(const Fixture *fx);
/** A colour's name, as the meter readings are asked for ("Red", "Lamp"). */
QString powerColourName(int colour);

/** Of @p fids, the one whose readings stand for them: @p prefer when it is
 *  measured, else the first that is; nullptr when none is. */
Fixture *powerSource(Doc *doc, const QList<quint32> &fids, quint32 prefer);
/** Every other fixture in the show that is the same as @p fx: the same
 *  make, model and mode. */
QList<quint32> sameFixtures(Doc *doc, const Fixture *fx);
/** Give @p to @p from's readings, each keeping its own maximum. Returns how
 *  many changed. */
int copyPowerReadings(Doc *doc, const Fixture *from, const QList<quint32> &to);

/**
 * What a fixture draws, typed in from readings already taken: each colour
 * alone at full, everything at full, at half and off, and the most it may
 * draw. With several fixtures, it is set on all of them.
 */
class PowerProfileDialog final : public QDialog
{
    Q_OBJECT

public:
    PowerProfileDialog(Doc *doc, const QList<quint32> &fids, QWidget *parent = nullptr);

    /** The profile as the fields say. */
    PowerProfile profile() const;

    /** Set it on the fixtures (what OK does). */
    void apply();

private:
    Doc *m_doc;
    QList<quint32> m_fids;
    QMap<int, QDoubleSpinBox *> m_colour;
    QDoubleSpinBox *m_full;
    QDoubleSpinBox *m_half;
    QDoubleSpinBox *m_standby;
    QDoubleSpinBox *m_max;
};

/**
 * Measure a fixture's draw a state at a time ("how do you want to figure
 * out the levels"): it drives the fixture -- over anything else, and past
 * its cap -- off, each colour alone at full, everything at half and at
 * full, and asks for the meter's reading at each. Its other channels are
 * left alone, so stop what else is running on it first. The fixture is
 * given back when it is done or cancelled. The readings can be used for
 * the other fixtures selected too.
 */
class PowerMeasureDialog final : public QDialog
{
    Q_OBJECT

public:
    PowerMeasureDialog(Doc *doc, const QList<quint32> &fids, QWidget *parent = nullptr);
    ~PowerMeasureDialog();

    /** One state to read: what is lit, at what. */
    struct Step
    {
        QString what;
        int colour;            ///< the colour lit alone; -1: every emitter
        uchar level;           ///< at what
        bool off = false;      ///< nothing lit
    };
    QList<Step> steps() const { return m_steps; }
    int current() const { return m_step; }

    /** Go to step @p i, driving the fixture to it. */
    void showStep(int i);

    /** The profile the readings make (the cap kept as it was). */
    PowerProfile profile() const;

    /** Set it on the fixture -- or all of them, when ticked. */
    void apply();

private:
    void drive(const Step &step);
    void release();

    Doc *m_doc;
    QList<quint32> m_fids;
    Fixture *m_fixture;
    QList<Step> m_steps;
    QVector<double> m_readings;
    int m_step = -1;
    QLabel *m_title;
    QLabel *m_what;
    QDoubleSpinBox *m_amps;
    QPushButton *m_back;
    QPushButton *m_next;
    QCheckBox *m_all;
    QSharedPointer<GenericFader> m_fader;
    quint32 m_universe;
};

/** @} */

#endif
