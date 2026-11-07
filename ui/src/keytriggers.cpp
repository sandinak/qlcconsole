/*
  Q Light Controller Plus
  keytriggers.cpp

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

#include <QAbstractSpinBox>
#include <QKeySequenceEdit>
#include <QPlainTextEdit>
#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QShortcut>
#include <QDialog>
#include <QAction>

#include "virtualconsole/vcaudiotriggers.h"
#include "virtualconsole/virtualconsole.h"
#include "virtualconsole/vcspeeddial.h"
#include "virtualconsole/vccuerunner.h"
#include "virtualconsole/vccuelist.h"
#include "virtualconsole/vcbutton.h"
#include "virtualconsole/vcslider.h"
#include "virtualconsole/vcframe.h"
#include "virtualconsole/vcclock.h"
#include "keytriggers.h"
#include "controlmap.h"
#include "doc.h"

namespace
{
/** Where the keyboard is the editor's (class names, so nothing here needs
 *  their headers). */
const char *const kEditors[] = {
    "Workbench", "ProgrammingManager", "LookEditor", "SceneEditor", "EFXEditor", "ChaserEditor",
    "RGBMatrixEditor", "CollectionEditor", "ScriptEditor", "FunctionManager", "ShowManager",
    "FixtureManager", "SimpleDesk"
};

quint32 channelOf(const QKeySequence &seq)
{
    return seq.count() == 1 ? ControlMap::keyChannel(seq[0].key(), seq[0].keyboardModifiers()) : 0;
}

QString plainText(QString text)
{
    text.remove(QLatin1Char('&'));
    text.remove(QStringLiteral("…"));
    return text.trimmed();
}

/** A VC widget's keys (as VirtualConsole matches them: a press drops ⌘). */
QList<QKeySequence> vcKeys(VCWidget *w)
{
    QList<QKeySequence> keys;
    if (VCButton *b = qobject_cast<VCButton *>(w))
        keys << b->keySequence();
    else if (VCCueList *l = qobject_cast<VCCueList *>(w))
        keys << l->nextKeySequence() << l->previousKeySequence() << l->playbackKeySequence()
             << l->stopKeySequence();
    else if (VCFrame *f = qobject_cast<VCFrame *>(w))
        keys << f->enableKeySequence() << f->nextPageKeySequence() << f->previousPageKeySequence();
    else if (VCSlider *s = qobject_cast<VCSlider *>(w))
        keys << s->overrideResetKeySequence() << s->playbackFlashKeySequence();
    else if (VCSpeedDial *d = qobject_cast<VCSpeedDial *>(w))
        keys << d->tapKeySequence() << d->multKeySequence() << d->divKeySequence()
             << d->multDivResetKeySequence() << d->applyKeySequence();
    else if (VCClock *c = qobject_cast<VCClock *>(w))
        keys << c->playKeySequence() << c->resetKeySequence();
    else if (VCAudioTriggers *a = qobject_cast<VCAudioTriggers *>(w))
        keys << a->keySequence();
    else if (VCCueRunner *r = qobject_cast<VCCueRunner *>(w))
        for (int i = 0; i < int(VCCueRunner::ActionCount); ++i)
            keys << r->keySequence(VCCueRunner::Action(i));
    return keys;
}
}

KeyTriggers::KeyTriggers(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
    Q_ASSERT(doc != nullptr);
    setObjectName(QStringLiteral("keyTriggers"));
    connect(m_doc->controlMap(), &ControlMap::bindingsChanged, this, &KeyTriggers::update);
    update();
}

KeyTriggers::~KeyTriggers()
{
    if (m_watching && qApp != nullptr)
        qApp->removeEventFilter(this);
}

void KeyTriggers::update()
{
    const bool want = m_doc->controlMap()->hasKeyBindings();
    if (want == m_watching || qApp == nullptr)
        return;
    if (want)
        qApp->installEventFilter(this);
    else
        qApp->removeEventFilter(this);
    m_watching = want;
    if (!want)
        m_held.clear();
}

QString KeyTriggers::focusBlocks(QWidget *focus)
{
    if (QApplication::activePopupWidget() != nullptr)
        return tr("a menu is open");
    if (QApplication::activeModalWidget() != nullptr
        || qobject_cast<QDialog *>(QApplication::activeWindow()) != nullptr)
        return tr("a dialog is open");
    for (QWidget *w = focus; w != nullptr; w = w->parentWidget())
    {
        if (qobject_cast<QLineEdit *>(w) != nullptr || qobject_cast<QAbstractSpinBox *>(w) != nullptr
            || qobject_cast<QTextEdit *>(w) != nullptr || qobject_cast<QPlainTextEdit *>(w) != nullptr
            || qobject_cast<QKeySequenceEdit *>(w) != nullptr)
            return tr("a text field has the keyboard");
        if (QComboBox *c = qobject_cast<QComboBox *>(w))
            if (c->isEditable())
                return tr("a text field has the keyboard");
        if (qobject_cast<QDialog *>(w) != nullptr)
            return tr("a dialog is open");
        if (w->inherits("VirtualConsole"))
            return tr("the Virtual Console has the keyboard");
        for (const char *editor : kEditors)
            if (w->inherits(editor))
                return tr("an editor has the keyboard");
    }
    return QString();
}

QString KeyTriggers::refusal(quint32 channel, bool *dialogKey)
{
    if (dialogKey != nullptr)
        *dialogKey = false;
    if (channel == 0)
        return tr("Hold the modifiers and press a key.");
    const QKeyCombination combo = QKeyCombination::fromCombined(int(channel));
    const Qt::KeyboardModifiers mods = combo.keyboardModifiers();
    const QString name = ControlMap::keyName(channel);

    // The dialog's own keys (and every dialog's): they work the dialog.
    if (mods == Qt::NoModifier || mods == Qt::ShiftModifier)
    {
        switch (combo.key())
        {
            case Qt::Key_Escape: case Qt::Key_Tab: case Qt::Key_Backtab: case Qt::Key_Return:
            case Qt::Key_Enter: case Qt::Key_Space: case Qt::Key_Up: case Qt::Key_Down:
            case Qt::Key_Left: case Qt::Key_Right:
                if (dialogKey != nullptr)
                    *dialogKey = true;
                return tr("%1 works this dialog (and every list and dialog), so it can't be a trigger.")
                        .arg(name);
            default:
                break;
        }
    }
    // The edit keys (docs/features/shortcuts.md), handled by the lists
    // themselves rather than by a menu shortcut.
    static const QList<QKeyCombination> editKeys = {
        Qt::Key_Backspace, Qt::Key_Delete, Qt::Key_F1, Qt::Key_F2, Qt::SHIFT | Qt::Key_F1,
        Qt::Key_Colon, Qt::Key_Slash, Qt::SHIFT | Qt::Key_Colon,
        Qt::CTRL | Qt::Key_C, Qt::CTRL | Qt::Key_V, Qt::CTRL | Qt::Key_X, Qt::CTRL | Qt::Key_D,
        Qt::CTRL | Qt::Key_F, Qt::CTRL | Qt::Key_A, Qt::CTRL | Qt::Key_Z,
        Qt::CTRL | Qt::SHIFT | Qt::Key_Z, Qt::CTRL | Qt::Key_W, Qt::CTRL | Qt::Key_Slash
    };
    for (const QKeyCombination &k : editKeys)
        if (quint32(k.toCombined()) == channel)
            return tr("%1 is one of the app's edit keys (Delete, Rename, Copy, Help…), so it can't be "
                      "a trigger.").arg(name);

    // A menu shortcut, or any other registered one.
    const QWidgetList tops = QApplication::topLevelWidgets();
    for (QWidget *top : tops)
    {
        QList<QAction *> actions = top->actions();
        actions += top->findChildren<QAction *>();
        for (QAction *a : std::as_const(actions))
            for (const QKeySequence &seq : a->shortcuts())
                if (channelOf(seq) == channel)
                {
                    const QString what = plainText(a->text());
                    return what.isEmpty() ? tr("%1 is already a shortcut of the app.").arg(name)
                                          : tr("%1 is already the shortcut for %2.").arg(name, what);
                }
        for (QShortcut *s : top->findChildren<QShortcut *>())
            for (const QKeySequence &seq : s->keys())
                if (channelOf(seq) == channel)
                    return tr("%1 is already a shortcut of the app.").arg(name);
    }

    // A Virtual Console widget's key (a press there drops ⌘, as VC does).
    if (VirtualConsole *vc = VirtualConsole::instance())
    {
        const quint32 noCtrl = quint32(QKeyCombination(mods & ~Qt::ControlModifier, combo.key()).toCombined());
        for (VCWidget *w : vc->findChildren<VCWidget *>())
            for (const QKeySequence &seq : vcKeys(w))
            {
                const quint32 c = channelOf(seq);
                if (c != 0 && (c == channel || c == noCtrl))
                    return tr("%1 is already used by Virtual Console %2 \"%3\".")
                            .arg(name, VCWidget::typeToString(w->type()), w->caption());
            }
    }
    return QString();
}

bool KeyTriggers::eventFilter(QObject *watched, QEvent *event)
{
    const QEvent::Type type = event->type();
    if (type == QEvent::ApplicationStateChange)
    {
        // The keyboard went elsewhere: no release will come.
        if (QGuiApplication::applicationState() != Qt::ApplicationActive)
        {
            m_held.clear();
            m_doc->controlMap()->releaseKeys();
        }
        return false;
    }
    if (type != QEvent::KeyPress && type != QEvent::KeyRelease)
        return false;
    // Once per key: to the widget it is for (it is offered to each parent
    // in turn as it propagates).
    QWidget *target = QApplication::focusWidget();
    if (target == nullptr)
        target = QApplication::activeWindow();
    if (watched != target)
        return false;

    QKeyEvent *ke = static_cast<QKeyEvent *>(event);
    // The physical key (a Mac reports it as the virtual key), so the
    // release finds its press even when Shift was let go first.
    quint32 physical = quint32(ke->key());
    if (ke->nativeScanCode() != 0)
        physical = ke->nativeScanCode() | 0x80000000u;
    else if (ke->nativeVirtualKey() != 0)
        physical = ke->nativeVirtualKey() | 0x40000000u;
    ControlMap *cm = m_doc->controlMap();
    if (type == QEvent::KeyRelease)
    {
        if (ke->isAutoRepeat())
            return m_held.contains(physical);
        if (!m_held.contains(physical))
            return false;
        cm->handleKey(m_held.take(physical), false);
        return true;
    }

    const quint32 channel = ControlMap::keyChannel(ke->key(), ke->modifiers());
    if (ke->isAutoRepeat())
        return m_held.contains(physical) || (channel != 0 && cm->isKeyDown(channel));
    if (channel == 0 || !cm->isBound(ControlMap::KeyboardUniverse, channel))
        return false;
    if (!focusBlocks(QApplication::focusWidget()).isEmpty())
        return false;
    if (!cm->handleKey(channel, true))
        return false;
    m_held.insert(physical, channel);
    return true;
}
