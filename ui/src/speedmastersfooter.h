/*
  Q Light Controller Plus
  speedmastersfooter.h

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

#ifndef SPEEDMASTERSFOOTER_H
#define SPEEDMASTERSFOOTER_H

#include <QWidget>

#include <QToolButton>
#include <QElapsedTimer>

class QTimer;
class Doc;

/** A master's footer button. Painted, not styled: a stylesheet change
 *  re-polishes the widget, and at a blink a beat that slowed the whole UI. */
class SpeedMasterButton final : public QToolButton
{
    Q_OBJECT

public:
    explicit SpeedMasterButton(QWidget *parent = nullptr);

    /** How the beat shows on the button. */
    enum BeatStyle { NoBeat, Blink, Bar };

    /** The beat: @p phase 0..1 through it, drawn in @p style (Blink: lit
     *  orange at its start; Bar: an orange bar filling the button's foot
     *  across each beat; NoBeat: neither). @p level 0..1 (the rate, 0..200 %)
     *  and @p levelAlpha 0..1: the green bar while the rate is being moved,
     *  fading out after. Repaints only when what's drawn changes. */
    void setState(BeatStyle style, double phase, double level, double levelAlpha);
    bool isFired() const { return m_fired; }
    /** The beat bar's width now; 0 when it isn't drawn. */
    int beatBarWidth() const { return m_barPx; }
    /** The green bar's width; 0 when it isn't showing. */
    int levelWidth() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_fired = false;
    int m_barPx = 0;        //!< the beat bar, in pixels
    double m_level = 0;     //!< 0..1; the width comes from the button's own at paint
    int m_levelAlpha = 0;   //!< 0..255, in steps
};

/** @addtogroup ui UI
 * @{
 */

/**
 * The speed masters in the footer: a button for each master the show uses
 * (one nothing is on stays out of the way),
 * reading its tempo ("Pars 120") and lighting on its beat. A click opens the
 * Speed Masters window at that master.
 */
class SpeedMastersFooter final : public QWidget
{
    Q_OBJECT

public:
    explicit SpeedMastersFooter(Doc *doc, QWidget *parent = nullptr);

    SpeedMasterButton *button(int index) const;
    /** What a master's button reads: its name, cut to @p maxName, and its
     *  tempo. */
    static QString buttonText(const QString &name, double bpm, bool paused, int maxName = 10);

    /** When the beat shows: always, or only on masters something playing is
     *  on (SpeedMasters::playingMask()). */
    enum BeatWhen { Always, WhilePlaying };
    /** The View menu's choices, kept in the settings. */
    static SpeedMasterButton::BeatStyle beatStyle();
    static void setBeatStyle(SpeedMasterButton::BeatStyle style);
    static BeatWhen beatWhen();
    static void setBeatWhen(BeatWhen when);

private:
    void applyCount();
    void tick();

    Doc *m_doc;
    QList<SpeedMasterButton *> m_buttons;
    QList<double> m_lastRate;
    QList<qint64> m_rateMovedAt;    //!< ms on m_clock; -1 never
    QElapsedTimer m_clock;
    QTimer *m_timer;
    quint32 m_used = 0;     //!< SpeedMasters::usedMask(), re-read twice a second
    quint32 m_playing = 0;  //!< SpeedMasters::playingMask(), re-read with it
    SpeedMasterButton::BeatStyle m_style = SpeedMasterButton::Blink;
    BeatWhen m_when = Always;
public:
    /** Take the View menu's choices now (else within half a second). */
    void reloadBeatOptions() { m_style = beatStyle(); m_when = beatWhen(); }
private:
    int m_ticks = 0;
};

/** @} */

#endif
