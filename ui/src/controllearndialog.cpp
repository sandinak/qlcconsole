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
#include <QComboBox>
#include <QAction>
#include <QLabel>
#include <QMenu>

#include "controllearndialog.h"
#include "inputoutputmap.h"
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
                                "message) that should fire it. Press another one to change your mind."), this);
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
        m_note->setText(tr("No input is patched yet: patch your controller in Connections ▸ Devices "
                           "(or Inputs/Outputs), then learn."));

    connect(cm, &ControlMap::heard, this, &ControlLearnDialog::slotHeard);
    cm->listen();
    updateText();
}

ControlLearnDialog::~ControlLearnDialog()
{
    if (m_doc != nullptr && m_doc->controlMap() != nullptr)
        m_doc->controlMap()->stopListening();
}

QAction *ControlLearnDialog::addTo(QMenu *menu, Doc *doc, const ControlMap::Target &target, QWidget *parent)
{
    if (menu == nullptr || doc == nullptr || !target.isValid())
        return nullptr;
    const ControlMap::Binding *b = doc->controlMap()->binding(target);
    const QString text = b == nullptr ? tr("Learn trigger…")
                                      : tr("Trigger: %1…").arg(doc->controlMap()->controlName(b->universe, b->channel));
    QAction *a = menu->addAction(text, parent, [doc, target, parent]() {
        ControlLearnDialog d(doc, target, parent);
        d.exec();
    });
    a->setObjectName(QStringLiteral("learnTrigger"));
    a->setToolTip(tr("Fire this from a MIDI or OSC control: choose this, then press the key, pad or "
                     "button (Control Map)"));
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
    updateText();
    // Still listening: another press takes its place.
    m_doc->controlMap()->listen();
}

void ControlLearnDialog::updateText()
{
    ControlMap *cm = m_doc->controlMap();
    m_ok->setEnabled(hasControl());
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
