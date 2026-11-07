/*
  Q Light Controller Plus
  keytriggers.h

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

#ifndef KEYTRIGGERS_H
#define KEYTRIGGERS_H

#include <QObject>
#include <QHash>

#include "qlcui_export.h"

class QWidget;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * Computer keys for the Control Map (docs/features/other-improvements.md,
 * "Keys on the computer keyboard"): watches the app's key presses while a
 * key is bound (no binding, no filter: nothing costs anything) and hands a
 * key to ControlMap::handleKey(), which fires it in Operate, or in Design
 * while its target is armed.
 *
 * A key goes to its trigger only when nothing else is using the keyboard:
 * no dialog, menu or popup open; the keyboard focus not in a text field,
 * a spin box, an editable list, the command line, an editor (Build, the
 * Workbench, the Look Editor, a function editor, the timeline, the Fixture
 * Manager, the Simple Desk) or the Virtual Console (which has its own keys,
 * as in QLC+). Auto-repeat is not a press; a release always reaches the key
 * it lets go of, wherever the focus went. A fired key is used up: the
 * widget with the focus doesn't also see it.
 */
class QLC_UI_EXPORT KeyTriggers final : public QObject
{
    Q_OBJECT

public:
    KeyTriggers(Doc *doc, QObject *parent = nullptr);
    ~KeyTriggers();

    /** Whether the filter is installed now (a key is bound). */
    bool isWatching() const { return m_watching; }

    /** Why a key can't fire with the focus on @a focus (empty: it can). */
    static QString focusBlocks(QWidget *focus);

    /** Why key @a channel (ControlMap::keyChannel()) can't be learned
     *  (empty: it can): the dialog itself needs it (@a dialogKey set: Esc,
     *  Tab, Return, Space, the arrows), the app uses it (a menu shortcut, an
     *  edit key, Help), or a Virtual Console widget has it. */
    static QString refusal(quint32 channel, bool *dialogKey = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void update();

private:
    Doc *m_doc;
    bool m_watching = false;
    /** physical key pressed -> the key channel its press fired */
    QHash<quint32, quint32> m_held;
};

/** @} */

#endif
