/*
  Q Light Controller Plus
  controllearndialog.cpp

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

#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QAction>
#include <QLabel>
#include <QMenu>

#include "controllearndialog.h"
#include "inputoutputmap.h"
#include "keytriggers.h"
#include "inputpatch.h"
#include "doc.h"

ControlLearnDialog::ControlLearnDialog(Doc *doc, const ControlMap::Target &target, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_target(target)
{
    Q_ASSERT(doc != nullptr);
    ControlMap *cm = m_doc->controlMap();
    setObjectName(QStringLiteral("controlLearnDialog"));
    setProperty("helpTopic", QStringLiteral("other-improvements.md#control-map-midi-and-osc-triggers"));
    setWindowTitle(tr("Learn trigger"));

    QVBoxLayout *lay = new QVBoxLayout(this);
    QLabel *what = new QLabel(tr("<b>%1</b>").arg(cm->targetName(target).toHtmlEscaped()), this);
    what->setObjectName(QStringLiteral("learnTarget"));
    lay->addWidget(what);
    QLabel *how = new QLabel(tr("Press the key, pad or button on your MIDI controller (or send the OSC "
                                "message), or a key on the computer keyboard, that should fire it. "
                                "Press another one to change your mind."), this);
    how->setWordWrap(true);
    lay->addWidget(how);

    m_heard = new QLabel(this);
    m_heard->setObjectName(QStringLiteral("learnHeard"));
    m_heard->setWordWrap(true);
    lay->addWidget(m_heard);

    QFormLayout *form = new QFormLayout();
    m_mode = new QComboBox(this);
    m_mode->setObjectName(QStringLiteral("learnMode"));
    const bool list = target.type == ControlMap::CueListTarget || target.type == ControlMap::CueTarget;
    const QList<ControlMap::Mode> modes = list
        ? QList<ControlMap::Mode>{ ControlMap::Toggle, ControlMap::Momentary, ControlMap::StopOnly }
        : QList<ControlMap::Mode>{ ControlMap::Toggle, ControlMap::Momentary, ControlMap::StartOnly,
                                   ControlMap::StopOnly };
    for (ControlMap::Mode m : modes)
        m_mode->addItem(ControlMap::modeName(m, target.type), int(m));
    m_mode->setToolTip(list ? tr("GO: each press is a GO. GO while held: GO on the press, the list stops "
                                 "on the release. Stop the list: a press stops it.")
                            : tr("Toggle: a press starts it, the next stops it. Momentary: on while "
                                 "held. Start only / Stop only: a press only starts, or only stops, it."));
    form->addRow(tr("When pressed"), m_mode);
    lay->addLayout(form);

    m_note = new QLabel(this);
    m_note->setObjectName(QStringLiteral("learnNote"));
    m_note->setWordWrap(true);
    m_note->setStyleSheet(QStringLiteral("color: gray;"));
    lay->addWidget(m_note);

    m_keyNote = new QLabel(this);
    m_keyNote->setObjectName(QStringLiteral("learnKeyNote"));
    m_keyNote->setWordWrap(true);
    m_keyNote->setStyleSheet(QStringLiteral("color: gray;"));
    m_keyNote->hide();
    lay->addWidget(m_keyNote);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    m_remove = buttons->addButton(tr("Remove trigger"), QDialogButtonBox::DestructiveRole);
    m_remove->setObjectName(QStringLiteral("learnRemove"));
    connect(buttons, &QDialogButtonBox::accepted, this, &ControlLearnDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &ControlLearnDialog::reject);
    connect(m_remove, &QPushButton::clicked, this, &ControlLearnDialog::slotRemove);
    lay->addWidget(buttons);

    // Bound already: that control and mode, until another is pressed.
    if (const ControlMap::Binding *b = cm->binding(target))
    {
        m_universe = b->universe;
        m_channel = b->channel;
        setMode(b->mode);
    }
    m_remove->setVisible(hasControl());

    // Nothing to hear it on: say where inputs are patched.
    bool anyInput = false;
    InputOutputMap *iom = m_doc->inputOutputMap();
    for (quint32 u = 0; iom != nullptr && u < iom->universesCount() && !anyInput; ++u)
        anyInput = iom->inputPatch(u) != nullptr;
    if (!anyInput)
        m_note->setText(tr("No MIDI or OSC input is patched yet: press a computer key, or patch your "
                           "controller in Connections ▸ Devices (or Inputs/Outputs), then learn."));

    connect(cm, &ControlMap::heard, this, &ControlLearnDialog::slotHeard);
    cm->listen();
    // Computer keys: every key pressed in the dialog, before its buttons
    // and the mode box see it.
    qApp->installEventFilter(this);
    updateText();
}

ControlLearnDialog::~ControlLearnDialog()
{
    qApp->removeEventFilter(this);
    if (m_doc != nullptr && m_doc->controlMap() != nullptr)
        m_doc->controlMap()->stopListening();
}

QAction *ControlLearnDialog::addTo(QMenu *menu, Doc *doc, const ControlMap::Target &target, QWidget *parent)
{
    if (menu == nullptr || doc == nullptr || !target.isValid())
        return nullptr;
    const ControlMap::Binding *b = doc->controlMap()->binding(target);
    QString text = tr("Learn Trigger…");
    if (b != nullptr)
    {
        QString control = doc->controlMap()->controlName(b->universe, b->channel);
        // A key in Design says whether it may fire now (docs: armed).
        if (ControlMap::isKey(*b) && doc->mode() == Doc::Design && doc->controlMap()->isArmed(target))
            control = tr("%1 · Armed").arg(control);
        text = tr("Trigger: %1…").arg(control);
    }
    QAction *a = menu->addAction(text, parent, [doc, target, parent]() {
        ControlLearnDialog d(doc, target, parent);
        d.exec();
    });
    a->setObjectName(QStringLiteral("learnTrigger"));
    a->setToolTip(tr("Fire this from a MIDI or OSC control or a computer key: choose this, then press "
                     "the key, pad or button (Control Map)"));
    return a;
}

ControlMap::Mode ControlLearnDialog::mode() const
{
    return ControlMap::Mode(m_mode->currentData().toInt());
}

void ControlLearnDialog::setMode(ControlMap::Mode mode)
{
    // StartOnly on a list is a GO, as Toggle is.
    int i = m_mode->findData(int(mode));
    if (i < 0)
        i = m_mode->findData(int(ControlMap::Toggle));
    m_mode->setCurrentIndex(i);
}

void ControlLearnDialog::slotHeard(quint32 universe, quint32 channel)
{
    m_universe = universe;
    m_channel = channel;
    m_keyNote->hide();
    updateText();
    // Still listening: another press takes its place.
    m_doc->controlMap()->listen();
}

bool ControlLearnDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::KeyPress)
        return false;
    QWidget *w = qobject_cast<QWidget *>(watched);
    QWidget *target = QApplication::focusWidget() != nullptr ? QApplication::focusWidget() : this;
    if (w == nullptr || w != target || w->window() != this)
        return false;
    QKeyEvent *ke = static_cast<QKeyEvent *>(event);
    if (ke->isAutoRepeat())
        return ControlMap::keyChannel(ke->key(), ke->modifiers()) == m_channel;
    return learnKey(ke->key(), ke->modifiers());
}

bool ControlLearnDialog::learnKey(int key, Qt::KeyboardModifiers modifiers)
{
    const quint32 channel = ControlMap::keyChannel(key, modifiers);
    if (channel == 0)
        return false;       // a modifier on its own: wait for the key
    bool dialogKey = false;
    const QString why = KeyTriggers::refusal(channel, &dialogKey);
    if (!why.isEmpty())
    {
        m_keyNote->setText(why);
        m_keyNote->show();
        return !dialogKey;
    }
    m_universe = ControlMap::KeyboardUniverse;
    m_channel = channel;
    updateText();
    return true;
}

void ControlLearnDialog::updateText()
{
    ControlMap *cm = m_doc->controlMap();
    m_ok->setEnabled(hasControl());
    if (m_universe == ControlMap::KeyboardUniverse)
    {
        m_keyNote->setText(m_target.type == ControlMap::FunctionTarget
            ? tr("A key fires in Operate, when no text field, editor or dialog has the keyboard.")
            : tr("A key fires in Operate, and in Design while this is armed: in the Run tab's cue "
                 "list, or selected in the Show Tree. Never while a text field, editor or dialog "
                 "has the keyboard."));
        m_keyNote->show();
    }
    if (!hasControl())
    {
        m_heard->setText(tr("Listening…"));
        return;
    }
    QString text = tr("Control: <b>%1</b>").arg(cm->controlName(m_universe, m_channel).toHtmlEscaped());
    const ControlMap::Binding *other = cm->bindingOn(m_universe, m_channel);
    if (other != nullptr && other->target != m_target)
        text += QStringLiteral("<br>") + tr("It fires %1 now; OK moves it here.")
                    .arg(cm->targetName(other->target).toHtmlEscaped());
    m_heard->setText(text);
}

void ControlLearnDialog::accept()
{
    if (!hasControl())
        return;
    ControlMap::Binding b;
    b.universe = m_universe;
    b.channel = m_channel;
    b.target = m_target;
    b.mode = mode();
    ControlMap *cm = m_doc->controlMap();
    cm->stopListening();
    const ControlMap::Binding *had = cm->binding(m_target);
    if (had != nullptr && had->universe == b.universe && had->channel == b.channel)
        cm->setMode(m_target, b.mode);
    else
        cm->setBinding(b);
    QDialog::accept();
}

void ControlLearnDialog::done(int r)
{
    m_doc->controlMap()->stopListening();
    QDialog::done(r);
}

void ControlLearnDialog::slotRemove()
{
    m_doc->controlMap()->stopListening();
    m_doc->controlMap()->removeBinding(m_target);
    QDialog::reject();
}
