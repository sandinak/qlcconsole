/*
  Q Light Controller Plus
  speedmasterspanel.h

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

#ifndef SPEEDMASTERSPANEL_H
#define SPEEDMASTERSPANEL_H

#include <QWidget>

class QDoubleSpinBox;
class QPushButton;
class QComboBox;
class QLineEdit;
class QSlider;
class QLabel;
class QTimer;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

/**
 * The four speed masters (SpeedMasters), a row each: name, what it follows
 * (its own tempo, the main clock, another master) and at what ratio, BPM,
 * Tap, a rate fader, Pause, and a light that flashes on its beat. A window
 * of its own, opened from the Programming toolbar's Masters button.
 */
class SpeedMastersPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit SpeedMastersPanel(Doc *doc, QWidget *parent = nullptr);

    /** The one panel, made on first use and raised after; @p focus, a
     *  master's index, puts the cursor on its tempo. */
    static SpeedMastersPanel *open(Doc *doc, QWidget *parent, int focus = -1);

    /** Map a master's Tap, Rate and Pause to control-surface inputs. */
    void editInputs(int index);
    /** What a row's Inputs button reads: how many are mapped. */
    static QString inputsText(int mapped);

    /** The ratios offered for following ("×½", "×2"...). */
    static QList<double> ratios();

    /** The speed choices every editor offers, in order: @p inheritText
     *  (Inherit), Own speed, Main clock, the four masters by name. */
    static QList<int> choiceValues(Doc *doc);
    static QStringList choiceNames(Doc *doc, const QString &inheritText);
    /** Fill @p cb with them, @p current selected. */
    static void fillCombo(QComboBox *cb, Doc *doc, const QString &inheritText, int current);

    struct Row
    {
        QLineEdit *name = nullptr;
        QComboBox *source = nullptr;    //!< Own, Main clock, Speed N
        QComboBox *ratio = nullptr;
        QDoubleSpinBox *bpm = nullptr;
        QPushButton *tap = nullptr;
        QSlider *rate = nullptr;
        QLabel *rateText = nullptr;
        QPushButton *pause = nullptr;
        QLabel *light = nullptr;
        QPushButton *inputs = nullptr;
    };
    const Row &row(int index) const { return m_rows[index]; }

private:
    void refresh(int index);
    /** Show the rows the show uses and fill the "Follows" lists to match. */
    void applyCount();
    void applySource(int index);
    void tickLights();

    Doc *m_doc;
    Row m_rows[16];                 // SpeedMasters::MaxCount
    class QSpinBox *m_countSpin = nullptr;
    QTimer *m_lightTimer;
    bool m_refreshing = false;
};

/** @} */

#endif
