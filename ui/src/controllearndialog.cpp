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

#include "showfeatures.h"
#include "controlcollisionbox.h"
#include "controllearndialog.h"
#include "inputoutputmap.h"
#include "keytriggers.h"
#include "inputpatch.h"
#include "doc.h"

ControlLearnDialog::ControlLearnDialog(Doc *doc, const ControlMap::Target &target, QWidget *parent,
                                       const ControlMap::Place &where)
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
    if (target.type == ControlMap::CueListTarget && cm->binding(target) == nullptr
        && cm->binding(ControlMap::cueListBack(target.id)) != nullptr)
        m_target = ControlMap::cueListBack(target.id);   // only its Back is bound: open on it
    QLabel *what = new QLabel(tr("<b>%1</b>").arg(cm->targetName(m_target).toHtmlEscaped()), this);
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
    /* A cue list's window learns its GO or its Back (phase F): one target
       each, so both can be on one list; Back is chosen in When pressed. */
    const bool list = ControlMap::isListType(target.type);
    const bool estop = target.type == ControlMap::EmergencyStopTarget;
    if (estop)
        how->setText(tr("Press the button on your MIDI controller (or send the OSC message) that should be "
                        "the emergency stop, then release it and press it again to confirm. It is one "
                        "button: a fader, knob or encoder is refused, and so is a computer key. Every "
                        "press is ALL OFF, from any mode, at any moment."));
    const bool listGoOrBack = target.type == ControlMap::CueListTarget
                              || target.type == ControlMap::CueListBackTarget;
    if (listGoOrBack)
    {
        // GO, GO while held, Back, Stop the list: the GO's modes and the Back.
        for (ControlMap::Mode m : ControlMap::modesFor(ControlMap::CueListTarget))
        {
            if (m == ControlMap::StopOnly)
            {
                m_mode->addItem(ControlMap::modeName(ControlMap::Toggle, ControlMap::CueListBackTarget),
                                int(ControlMap::Toggle));
                m_mode->setItemData(m_mode->count() - 1, int(ControlMap::CueListBackTarget), kTypeRole);
            }
            m_mode->addItem(ControlMap::modeName(m, ControlMap::CueListTarget), int(m));
            /* Stop the list: the GO's or the Back's, the one the window is
               on. A Back set to stop the list (Control Map table) stays a
               Back that stops it, not a Back that steps back. */
            m_mode->setItemData(m_mode->count() - 1,
                                m == ControlMap::StopOnly ? kEitherListType : int(ControlMap::CueListTarget),
                                kTypeRole);
        }
    }
    else
    {
        for (ControlMap::Mode m : ControlMap::modesFor(target.type))
        {
            m_mode->addItem(ControlMap::modeName(m, target.type), int(m));
            m_mode->setItemData(m_mode->count() - 1, int(target.type), kTypeRole);
        }
    }
    m_mode->setToolTip(listGoOrBack ? tr("GO: each press is a GO. GO while held: GO on the press, the list "
                                         "stops on the release. Back: each press goes back one cue. Stop "
                                         "the list: a press stops it.")
                       : list ? tr("GO: each press is a GO. GO while held: GO on the press, the list stops "
                                   "on the release. Stop the list: a press stops it.")
                            : tr("Toggle: a press starts it, the next stops it. Momentary: on while "
                                 "held. Start only / Stop only: a press only starts, or only stops, it."));
    form->addRow(tr("When pressed"), m_mode);
    m_mode->setEnabled(!estop);
    setMode(ControlMap::Toggle);        // this target's first item (a Back opens on Back)
    if (listGoOrBack)
        connect(m_mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                &ControlLearnDialog::slotModeChanged);

    // Works: where it is armed (Q295). The stop is everywhere, always.
    m_place = new QComboBox(this);
    m_place->setObjectName(QStringLiteral("learnPlace"));
    m_place->setToolTip(tr("Where this trigger works: only while that cue is live, that cue list runs or "
                           "that production is current -- or everywhere. One control can fire other "
                           "things in other places: the narrowest live place has it."));
    const ControlMap::Binding *bound = cm->binding(m_target);
    QList<ControlMap::Place> places = estop ? QList<ControlMap::Place>{ ControlMap::Place() }
                                            : cm->placeChoices(where, m_target);
    if (bound != nullptr && !places.contains(bound->place))
        places.prepend(bound->place);
    for (const ControlMap::Place &p : std::as_const(places))
    {
        QString name = cm->placeName(p);
        if (!name.isEmpty())
            name[0] = name.at(0).toUpper();
        m_place->addItem(name, QVariantList{ int(p.scope), p.id });
    }
    // A GO on a list or a jump to a cue brings that place live: learned
    // there, it would work only once it is already live (never start the
    // list, never jump to the cue). Everywhere by default; the place is
    // offered. A hit or a scene works where it was learned.
    const bool bringsItLive = list;
    // A hit fires only inside its own place: learned outside it, at its own.
    ControlMap::Place learnAt = where;
    if (target.type == ControlMap::HitTarget && !cm->isPlaceWithin(where, cm->placeOfHit(target.id)))
        learnAt = cm->placeOfHit(target.id);
    setPlace(bound != nullptr ? bound->place : bringsItLive ? ControlMap::everywhere() : learnAt);
    m_place->setEnabled(!estop && places.count() > 1);
    form->addRow(tr("Works"), m_place);
    lay->addLayout(form);

    m_placeNote = new QLabel(this);
    m_placeNote->setObjectName(QStringLiteral("learnPlaceNote"));
    m_placeNote->setWordWrap(true);
    m_placeNote->setVisible(!estop);
    lay->addWidget(m_placeNote);

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

    m_collision = new ControlCollisionBox(this);
    lay->addWidget(m_collision);
    connect(m_collision, &ControlCollisionBox::moveRequested, this, &ControlLearnDialog::slotMoveHere);
    connect(m_collision, &ControlCollisionBox::stepThroughRequested, this, &ControlLearnDialog::slotStepThrough);
    // Another place: another answer to who has the control (Q296).
    connect(m_place, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        if (hasControl())
            checkOwners();
        updateText();
    });
    connect(m_collision, &ControlCollisionBox::cancelled, this, [this]() {
        /* Not taken: back to what this trigger had -- its control (none, or
           its own) and its place. Left at the place that was refused, OK
           would bind its own control there and replace the trigger that
           has it, without a word. */
        const ControlMap::Binding *b = m_doc->controlMap()->binding(m_target);
        m_universe = b != nullptr ? b->universe : 0xFFFFFFFF;
        m_channel = b != nullptr ? b->channel : 0xFFFFFFFF;
        m_stepThrough = false;
        if (b != nullptr)
        {
            m_place->blockSignals(true);
            setPlace(b->place);
            m_place->blockSignals(false);
        }
        m_stepThrough = keepsStepping();
        updateText();
    });

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    m_remove = buttons->addButton(tr("Remove trigger"), QDialogButtonBox::DestructiveRole);
    m_remove->setObjectName(QStringLiteral("learnRemove"));
    connect(buttons, &QDialogButtonBox::accepted, this, &ControlLearnDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &ControlLearnDialog::reject);
    connect(m_remove, &QPushButton::clicked, this, &ControlLearnDialog::slotRemove);
    lay->addWidget(buttons);

    // Bound already: that control and mode, until another is pressed.
    if (const ControlMap::Binding *b = cm->binding(m_target))
    {
        m_universe = b->universe;
        m_channel = b->channel;
        setMode(b->mode);
    }
    m_remove->setVisible(hasControl());
    if (const ControlMap::Binding *b = cm->binding(m_target))
    {
        for (const ControlMap::Binding &o : cm->bindingsOn(b->universe, b->channel))
            if (o.target != m_target && o.place == b->place)
                m_boundStepping = true;
        m_boundUniverse = b->universe;
        m_boundChannel = b->channel;
        m_boundPlace = b->place;
        m_stepThrough = m_boundStepping && m_target.type != ControlMap::EmergencyStopTarget;
        m_boundStepping = m_stepThrough;
    }

    // Nothing to hear it on: say where inputs are patched.
    bool anyInput = false;
    InputOutputMap *iom = m_doc->inputOutputMap();
    for (quint32 u = 0; iom != nullptr && u < iom->universesCount() && !anyInput; ++u)
        anyInput = iom->inputPatch(u) != nullptr;
    if (!anyInput)
        m_note->setText(tr("No MIDI or OSC input is patched yet: press a computer key, or patch your "
                           "controller in Connections ▸ Devices (or Inputs/Outputs), then learn."));

    connect(cm, &ControlMap::heard, this, &ControlLearnDialog::slotHeard);
    // A control that is the emergency stop is refused, in words, by any learn:
    // the same refusal as every other owner's, with no Move It Here.
    connect(cm, &ControlMap::refused, this, [this](const QString &why) {
        const ControlMap::Binding *stop = m_doc->controlMap()->binding(ControlMap::emergencyStop());
        if (m_target.type != ControlMap::EmergencyStopTarget && stop != nullptr)
        {
            m_collision->refuse(m_doc, stop->universe, stop->channel,
                                ControlOwners::others(m_doc, stop->universe, stop->channel, self()));
            m_ok->setEnabled(false);
            if (m_collision->isRefusing())
                return;
        }
        m_note->setText(why);
    });
    if (estop)
    {
        connect(cm, &ControlMap::confirming, this, [this](quint32 u, quint32 c) {
            m_note->setText(tr("%1: release it and press it again to confirm.")
                                .arg(m_doc->controlMap()->controlName(u, c)));
        });
    }
    estop ? cm->listenForButton() : cm->listen();
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

QAction *ControlLearnDialog::addTo(QMenu *menu, Doc *doc, const ControlMap::Target &target, QWidget *parent,
                                   const ControlMap::Place &where)
{
    if (menu == nullptr || doc == nullptr || !target.isValid())
        return nullptr;
    /* Control surfaces off for the show (Q293): not offered. The emergency
       stop is learned from its own button, never from here, and stays. */
    if (!ShowFeatures::isOn(ShowFeatures::ControlSurfaces)
        && target.type != ControlMap::EmergencyStopTarget)
        return nullptr;
    const ControlMap::Binding *b = doc->controlMap()->binding(target);
    // A cue list's one item learns its GO and its Back: either bound says so.
    if (b == nullptr && target.type == ControlMap::CueListTarget)
        b = doc->controlMap()->binding(ControlMap::cueListBack(target.id));
    QString text = tr("Learn Trigger…");
    if (b != nullptr)
    {
        QString control = doc->controlMap()->controlName(b->universe, b->channel);
        // A key in Design says whether it may fire now (docs: armed).
        if (ControlMap::isKey(*b) && doc->mode() == Doc::Design && doc->controlMap()->isArmed(target))
            control = tr("%1 · Armed").arg(control);
        // Learned at a place: said, so the menu shows where it works.
        if (b->place.scope != ControlMap::Everywhere)
            control = tr("%1 · %2").arg(control, doc->controlMap()->placeName(b->place));
        text = tr("Trigger: %1…").arg(control);
    }
    QAction *a = menu->addAction(text, parent, [doc, target, parent, where]() {
        ControlLearnDialog d(doc, target, parent, where);
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
    // Of this target's items (a list's GO and its Back share the box).
    auto find = [this](ControlMap::Mode m) {
        for (int i = 0; i < m_mode->count(); ++i)
        {
            const int type = m_mode->itemData(i, kTypeRole).toInt();
            if (m_mode->itemData(i).toInt() == int(m) && (type == int(m_target.type) || type == kEitherListType))
                return i;
        }
        return -1;
    };
    // StartOnly on a list is a GO, as Toggle is.
    int i = find(mode);
    if (i < 0)
        i = find(ControlMap::Toggle);
    if (i >= 0)
        m_mode->setCurrentIndex(i);
}

void ControlLearnDialog::slotModeChanged(int index)
{
    const int typeData = m_mode->itemData(index, kTypeRole).toInt();
    // Stop the list stops it from the GO or the Back alike: the window stays where it is.
    if (typeData == kEitherListType || typeData == int(m_target.type))
        return;
    const ControlMap::TargetType type = ControlMap::TargetType(typeData);
    /* GO <-> Back: another target on the same list. Its own binding, if it
       has one, is what the window shows now; else a control just heard is
       kept, and the other one's control is not (it already GOes, or goes
       back: one job per control). */
    ControlMap *cm = m_doc->controlMap();
    const ControlMap::Binding *was = cm->binding(m_target);
    const bool wasBound = was != nullptr && was->universe == m_universe && was->channel == m_channel;
    m_target = ControlMap::Target{ type, m_target.id };
    m_boundStepping = false;
    m_stepThrough = false;
    if (const ControlMap::Binding *b = cm->binding(m_target))
    {
        m_universe = b->universe;
        m_channel = b->channel;
        m_place->blockSignals(true);
        setPlace(b->place);
        m_place->blockSignals(false);
    }
    else if (wasBound)
    {
        m_universe = 0xFFFFFFFF;
        m_channel = 0xFFFFFFFF;
    }
    if (QLabel *what = findChild<QLabel *>(QStringLiteral("learnTarget")))
        what->setText(tr("<b>%1</b>").arg(cm->targetName(m_target).toHtmlEscaped()));
    m_remove->setVisible(cm->binding(m_target) != nullptr);
    m_collision->clear();
    if (hasControl())
        checkOwners();
    updateText();
}

bool ControlLearnDialog::refusesFader() const
{
    // A GO, a Back, a cue's GO or a hit fires on a press: a fader's every move would be one.
    return ControlMap::isListType(m_target.type) || m_target.type == ControlMap::HitTarget;
}

ControlMap::Target ControlLearnDialog::target() const
{
    return m_target;
}

ControlMap::Place ControlLearnDialog::placeAt(int index) const
{
    const QVariantList v = m_place->itemData(index).toList();
    if (v.count() != 2)
        return ControlMap::Place();
    const ControlMap::PlaceScope scope = ControlMap::PlaceScope(v.at(0).toInt());
    if (scope == ControlMap::NewerPlace)
    {
        // Only ever its own (a newer version's place), kept exactly as it was.
        const ControlMap::Binding *b = m_doc->controlMap()->binding(m_target);
        return b != nullptr && b->place.scope == ControlMap::NewerPlace ? b->place : ControlMap::Place();
    }
    return ControlMap::Place{ scope, v.at(1).toUInt(), QString(), QString() };
}

ControlMap::Place ControlLearnDialog::place() const
{
    return placeAt(m_place->currentIndex());
}

void ControlLearnDialog::setPlace(const ControlMap::Place &place)
{
    for (int i = 0; i < m_place->count(); ++i)
    {
        if (placeAt(i) == place)
        {
            m_place->setCurrentIndex(i);
            return;
        }
    }
}

QString ControlLearnDialog::placeText() const
{
    if (!hasControl() || m_target.type == ControlMap::EmergencyStopTarget)
        return QString();
    ControlMap *cm = m_doc->controlMap();
    // The control as it will be: the others on it, and this one here.
    QList<ControlMap::Binding> on;
    for (const ControlMap::Binding &b : cm->bindingsOn(m_universe, m_channel))
        if (b.target != m_target && (b.place != place() || m_stepThrough || m_collision->offersStepThrough()))
            on << b;
    ControlMap::Binding self;
    self.universe = m_universe;
    self.channel = m_channel;
    self.target = m_target;
    self.place = place();
    on << self;
    return tr("%1: %2.").arg(cm->controlName(m_universe, m_channel), cm->placesText(on));
}

void ControlLearnDialog::slotHeard(quint32 universe, quint32 channel)
{
    ControlMap *cm = m_doc->controlMap();
    m_keyNote->hide();
    m_note->clear();
    /* A GO or a hit on a fader (its profile says so, or it moved like one:
       the emergency stop's own rule): refused at Learn, saying why, and
       still listening for a button. */
    if (refusesFader() && cm->learnSawFader(universe, channel))
    {
        const ControlMap::Binding *b = cm->binding(m_target);
        m_universe = b != nullptr ? b->universe : 0xFFFFFFFF;
        m_channel = b != nullptr ? b->channel : 0xFFFFFFFF;
        m_collision->clear();
        updateText();
        m_note->setText(tr("%1 is a fader or knob (it sends a range of values): %2 fires on a press, so it "
                           "takes a button, pad or key. Press one.")
                            .arg(cm->controlName(universe, channel),
                                 m_target.type == ControlMap::HitTarget ? tr("a hit") : tr("a GO")));
        cm->listen();
        return;
    }
    m_universe = universe;
    m_channel = channel;
    updateText();
    checkOwners();
    /* A latching button sends on at one press and 0 at the next: only every
       other press reaches the show. That breaks a Toggle (the 0 between
       does nothing), so Toggle becomes Start only, once, and said. A mode
       chosen by hand is kept (Stop only still stops; Momentary follows the
       latch, on then off; a list's GO already is its press), said too. */
    if (cm->learnSawLatching(universe, channel) && m_target.type != ControlMap::EmergencyStopTarget)
    {
        const QString name = cm->controlName(universe, channel);
        if (!m_latchChose && mode() == ControlMap::Toggle
            && ControlMap::modesFor(m_target.type).contains(ControlMap::StartOnly))
        {
            m_latchChose = true;
            setMode(ControlMap::StartOnly);
            m_note->setText(tr("%1 looks like a latching button: it sends on at one press and off at the next, "
                               "so only every other press reaches the show. %2 is chosen; set the button to "
                               "momentary on the controller if it can be.")
                                .arg(name, ControlMap::modeName(mode(), m_target.type)));
        }
        else
        {
            m_note->setText(tr("%1 looks like a latching button: it sends on at one press and off at the next. "
                               "%2 is kept as chosen; set the button to momentary on the controller if it can be.")
                                .arg(name, ControlMap::modeName(mode(), m_target.type)));
        }
    }
    // Still listening: another press takes its place.
    if (m_target.type == ControlMap::EmergencyStopTarget)
        m_doc->controlMap()->listenForButton();
    else
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
    if (m_target.type == ControlMap::EmergencyStopTarget)
    {
        bool dialogKey = false;
        KeyTriggers::refusal(channel, &dialogKey);
        if (dialogKey)
            return false;   // Esc, Tab, Return: they work the dialog
        m_keyNote->setText(tr("A computer key cannot be the emergency stop: it fires only in Operate. "
                              "Use a button on a MIDI or OSC controller."));
        m_keyNote->show();
        return true;
    }
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
    checkOwners();
    return true;
}

ControlOwners::Owner ControlLearnDialog::self() const
{
    ControlOwners::Owner o;
    o.kind = m_target.type == ControlMap::EmergencyStopTarget ? ControlOwners::EmergencyStop
                                                              : ControlOwners::Trigger;
    o.target = m_target;
    o.place = m_target.type == ControlMap::EmergencyStopTarget ? ControlMap::Place() : place();
    return o;
}

bool ControlLearnDialog::keepsStepping() const
{
    return m_boundStepping && hasControl() && m_universe == m_boundUniverse
           && m_channel == m_boundChannel && place() == m_boundPlace;
}

void ControlLearnDialog::checkOwners()
{
    m_stepThrough = false;
    /* A trigger that already steps through with siblings at this control
       and place keeps doing so: reopened, its own OK is not "another job". */
    if (keepsStepping())
    {
        m_stepThrough = true;
        m_collision->clear();
        m_collision->setStepThrough(false);
        m_ok->setEnabled(true);
        return;
    }
    const QList<ControlOwners::Owner> others = ControlOwners::others(m_doc, m_universe, m_channel, self());
    m_collision->refuse(m_doc, m_universe, m_channel, others);
    /* Triggers at this same place on a button: they may step through, one
       per press (Q303). Not on a fader, and never beside the stop. */
    const bool button = m_universe == ControlMap::KeyboardUniverse
                        || m_doc->controlMap()->isButtonControl(m_universe, m_channel);
    m_collision->setStepThrough(m_target.type != ControlMap::EmergencyStopTarget && button
                                && ControlOwners::allTriggers(others));
    m_ok->setEnabled(hasControl() && !m_collision->isRefusing());
}

void ControlLearnDialog::slotStepThrough()
{
    if (!hasControl() || !m_collision->offersStepThrough())
        return;
    m_collision->clear();
    m_stepThrough = true;
    accept();
}

void ControlLearnDialog::slotMoveHere()
{
    if (!hasControl() || !m_collision->isRefusing())
        return;
    // Take it from there (it no longer reacts), then bind it here.
    if (!ControlOwners::releaseAll(m_doc, m_collision->owners()))
        return;
    m_collision->clear();
    accept();
}

void ControlLearnDialog::updateText()
{
    ControlMap *cm = m_doc->controlMap();
    m_ok->setEnabled(hasControl() && !m_collision->isRefusing());
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
        m_placeNote->clear();
        return;
    }
    m_heard->setText(tr("Control: <b>%1</b>").arg(cm->controlName(m_universe, m_channel).toHtmlEscaped()));
    m_placeNote->setText(placeText());
}

void ControlLearnDialog::accept()
{
    // A control with another job is never taken by OK: Move It Here or Cancel.
    if (!hasControl() || m_collision->isRefusing())
        return;
    /* Asked again here, whatever led to OK: a binding is never replaced
       without a word. Move It Here released the others already; Step
       Through Them keeps them beside this one. */
    if (keepsStepping())
        m_stepThrough = true;
    if (!m_stepThrough && !ControlOwners::others(m_doc, m_universe, m_channel, self()).isEmpty())
    {
        checkOwners();
        return;
    }
    ControlMap::Binding b;
    b.universe = m_universe;
    b.channel = m_channel;
    b.target = m_target;
    b.mode = mode();
    b.place = m_target.type == ControlMap::EmergencyStopTarget ? ControlMap::Place() : place();
    ControlMap *cm = m_doc->controlMap();
    cm->stopListening();
    const ControlMap::Binding *had = cm->binding(m_target);
    if (had != nullptr && had->universe == b.universe && had->channel == b.channel && had->place == b.place)
        cm->setMode(m_target, b.mode);
    else
        cm->setBinding(b, m_stepThrough);
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
