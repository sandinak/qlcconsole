/*
  Q Light Controller Plus - qlcconsole
  footericons.h

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

#ifndef FOOTERICONS_H
#define FOOTERICONS_H

#include <QIcon>
#include <QLabel>
#include <QList>
#include <QString>

#include "qlcui_export.h"

/** @addtogroup ui UI
 * @{
 */

/**
 * The footer's icons: one small monochrome glyph per kind of chip, so a
 * glance says WHAT a chip reports (power, time, venue, cue...) before its
 * value is read. Drawn in the text colour, so they follow light and dark.
 * The registry also feeds View ▸ Footer (which chips show, per computer).
 */
namespace FooterIcons
{
enum Glyph
{
    Ready, Pin, Cap, Book, Checklist, Network, Funnel, Warning, Download, Fader, Metronome,
    NextCue, Clock, Gauge, Bolt, Timeline, Lamp, Grid, Gamepad, Spot, Hold, Flask, Board,
    Pencil, Stage, Sliders, Disk
};

struct Entry
{
    QString id;          ///< stable key, saved in the settings
    QString label;       ///< the name in View ▸ Footer, the tooltip and the accessible name
    Glyph glyph;
    bool locked;         ///< always shown: a safety state, or the Student level the course needs
    int legacyChip;      ///< App::FooterChip it is also, or -1
};

/** Every chip, left to right as the footer has them. */
QLC_UI_EXPORT const QList<Entry> &entries();
QLC_UI_EXPORT const Entry *find(const QString &id);
QLC_UI_EXPORT QIcon icon(Glyph glyph, const QColor &colour, int size = 14);
}

/** The icon beside a chip, a footer item of its own: it stays when the chip
 *  is squeezed down to "icon only", and follows the chip's visibility. */
class QLC_UI_EXPORT FooterBadge final : public QLabel
{
    Q_OBJECT

public:
    FooterBadge(const QString &id, QWidget *chip, QWidget *parent);
    QWidget *chip() const { return m_chip; }
    const QString &id() const { return m_id; }
    /** The room the icon takes. */
    int iconWidth() const { return 18; }

protected:
    void changeEvent(QEvent *event) override;

private:
    void refresh();

    QString m_id;
    QWidget *m_chip;
};

/** @} */

#endif
