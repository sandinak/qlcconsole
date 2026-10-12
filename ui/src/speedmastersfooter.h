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

class QToolButton;
class QTimer;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The speed masters in the footer: a button for each master the show uses,
 * reading its tempo ("Pars 120") and lighting on its beat. A click opens the
 * Speed Masters window at that master.
 */
class SpeedMastersFooter final : public QWidget
{
    Q_OBJECT

public:
    explicit SpeedMastersFooter(Doc *doc, QWidget *parent = nullptr);

    QToolButton *button(int index) const;
    /** What a master's button reads: its name, cut to @p maxName, and its
     *  tempo. */
    static QString buttonText(const QString &name, double bpm, bool paused, int maxName = 10);

private:
    void applyCount();
    void tick();

    Doc *m_doc;
    QList<QToolButton *> m_buttons;
    QTimer *m_timer;
};

/** @} */

#endif
