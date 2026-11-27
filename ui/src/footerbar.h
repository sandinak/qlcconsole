/*
  Q Light Controller Plus - qlcconsole
  footerbar.h

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

#ifndef FOOTERBAR_H
#define FOOTERBAR_H

#include <QStatusBar>
#include <QPointer>
#include <QList>
#include "qlcui_export.h"

class QLabel;

/**
 * The main window's footer: a QStatusBar whose message (showMessage())
 * is one label, drawn on its own background over the left-hand chips and
 * stopping short of the permanent ones on the right.
 *
 * QStatusBar draws a message straight onto the bar, across its whole
 * width: under the permanent chips (whose text then sat on top of it,
 * bench 2026-11-27: "Design mode. The footer does not go live…" over
 * another chip's words), and under any left-hand chip that was shown
 * again while the message stood. Here a message is one label at a time:
 * a new one replaces the last, and nothing else is drawn under or over it.
 */
class QLC_UI_EXPORT FooterBar final : public QStatusBar
{
    Q_OBJECT

public:
    explicit FooterBar(QWidget *parent = nullptr);

    /** The label the message is shown in (hidden while there is none). */
    QLabel *messageLabel() const { return m_label; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool event(QEvent *event) override;

private:
    void showText(const QString &text);
    void place();

    QLabel *m_label = nullptr;
    QString m_text;
    /** The chips still shown when the message came: the permanent ones. */
    QList<QPointer<QWidget>> m_permanent;
};

#endif
