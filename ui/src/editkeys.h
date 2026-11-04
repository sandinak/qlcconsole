/*
  Q Light Controller Plus
  editkeys.h

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

#ifndef EDITKEYS_H
#define EDITKEYS_H

#include <QObject>
#include <functional>

class QKeyEvent;
class QWidget;

/** @addtogroup ui UI
 * @{
 */

/**
 * The same edit keys in every list (WF1-A, docs/features/shortcuts.md
 * "Edit keys"): ⌫ / Delete, F2, ⌘C / ⌘V, ⌘D, ⌘F and Esc, each a verb the
 * list gives (an empty one leaves the list's own handling alone).
 *
 * Keys a window owns too (⌘D is Control ▸ Dump DMX) are claimed only while
 * the list has the focus and something is selected (a ShortcutOverride), so
 * the window's shortcut still works everywhere else.
 *
 * Esc backs out first (the verb @c backOut: a Workbench or Look Editor open
 * over the page); with nothing to back out of it clears the selection.
 */
class EditKeys final : public QObject
{
    Q_OBJECT

public:
    struct Verbs
    {
        /** Something to act on. Empty: the list's (QAbstractItemView)
         *  selection. */
        std::function<bool()> hasSelection;
        std::function<void()> remove;      ///< ⌫ / Delete (asks first where it deletes for good)
        std::function<void()> rename;      ///< F2
        std::function<void()> copy;        ///< ⌘C
        std::function<void()> paste;       ///< ⌘V
        std::function<void()> duplicate;   ///< ⌘D
        std::function<void()> find;        ///< ⌘F
        /** Esc: true when it backed out of something (then the selection
         *  stays). */
        std::function<bool()> backOut;
        /** Esc with nothing to back out of. Empty: the list's clearSelection(). */
        std::function<void()> clearSelection;
    };

    /** Give @p target the edit keys @p verbs names; the filter lives as long
     *  as the target. */
    static EditKeys *install(QWidget *target, const Verbs &verbs);

    /** ⌘D (Ctrl+D) and nothing else held. */
    static bool isDuplicateKey(const QKeyEvent *ke);

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private:
    EditKeys(QWidget *target, const Verbs &verbs);

    enum Verb { None, Remove, Rename, Copy, Paste, Duplicate, Find, Escape };
    Verb verbFor(const QKeyEvent *ke) const;
    bool hasSelection() const;
    bool editing() const;

    QWidget *m_target;
    Verbs m_verbs;
};

/** @} */

#endif
