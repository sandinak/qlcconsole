/*
  Q Light Controller Plus
  editkeys.cpp

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

#include <QAbstractItemView>
#include <QApplication>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QKeySequence>

#include "editkeys.h"

EditKeys::EditKeys(QWidget *target, const Verbs &verbs)
    : QObject(target)
    , m_target(target)
    , m_verbs(verbs)
{
    target->installEventFilter(this);
}

EditKeys *EditKeys::install(QWidget *target, const Verbs &verbs)
{
    if (target == nullptr)
        return nullptr;
    return new EditKeys(target, verbs);
}

bool EditKeys::isDuplicateKey(const QKeyEvent *ke)
{
    return ke->key() == Qt::Key_D
        && (ke->modifiers() & ~Qt::KeypadModifier) == Qt::ControlModifier;
}

EditKeys::Verb EditKeys::verbFor(const QKeyEvent *ke) const
{
    const Qt::KeyboardModifiers mods = ke->modifiers() & ~Qt::KeypadModifier;
    if ((ke->key() == Qt::Key_Delete || ke->key() == Qt::Key_Backspace) && mods == Qt::NoModifier)
        return m_verbs.remove ? Remove : None;
    if (ke->key() == Qt::Key_F2 && mods == Qt::NoModifier)
        return m_verbs.rename ? Rename : None;
    if (ke->key() == Qt::Key_Escape && mods == Qt::NoModifier)
        return Escape;
    if (isDuplicateKey(ke))
        return m_verbs.duplicate ? Duplicate : None;
    if (ke->matches(QKeySequence::Copy))
        return m_verbs.copy ? Copy : None;
    if (ke->matches(QKeySequence::Paste))
        return m_verbs.paste ? Paste : None;
    if (ke->matches(QKeySequence::Find))
        return m_verbs.find ? Find : None;
    return None;
}

bool EditKeys::hasSelection() const
{
    if (m_verbs.hasSelection)
        return m_verbs.hasSelection();
    if (auto *view = qobject_cast<QAbstractItemView *>(m_target))
        return view->selectionModel() != nullptr && view->selectionModel()->hasSelection();
    return false;
}

bool EditKeys::editing() const
{
    /* An inline editor has the focus inside the list: its keys that come on
       up (F2, Esc, a Backspace at the start) are its own, not the list's. */
    QWidget *focus = QApplication::focusWidget();
    return focus != nullptr && focus != m_target && m_target->isAncestorOf(focus);
}

bool EditKeys::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj != m_target || (ev->type() != QEvent::ShortcutOverride && ev->type() != QEvent::KeyPress))
        return false;
    // An inline rename keeps its own keys.
    if (editing())
        return false;

    QKeyEvent *ke = static_cast<QKeyEvent *>(ev);
    const Verb verb = verbFor(ke);
    if (verb == None)
        return false;
    // What acts on the selection waits for one; paste and find do not.
    const bool needsSelection = verb == Remove || verb == Rename || verb == Copy || verb == Duplicate;
    if (needsSelection && !hasSelection())
        return false;

    if (ev->type() == QEvent::ShortcutOverride)
    {
        /* The key is the list's while it has the focus: the window's own
           shortcut on it (⌘D, Dump DMX) waits. Esc is never claimed here --
           an editor's back-out is a key press like any other. */
        if (verb == Escape)
            return false;
        ev->accept();
        return true;
    }

    switch (verb)
    {
    case Remove:    m_verbs.remove();    break;
    case Rename:    m_verbs.rename();    break;
    case Copy:      m_verbs.copy();      break;
    case Paste:     m_verbs.paste();     break;
    case Duplicate: m_verbs.duplicate(); break;
    case Find:      m_verbs.find();      break;
    case Escape:
        // Back out first; nothing to back out of: the selection goes.
        if (m_verbs.backOut && m_verbs.backOut())
            break;
        if (!hasSelection())
            return false;
        if (m_verbs.clearSelection)
            m_verbs.clearSelection();
        else if (auto *view = qobject_cast<QAbstractItemView *>(m_target))
            view->clearSelection();
        break;
    case None:
        return false;
    }
    ev->accept();
    return true;
}
