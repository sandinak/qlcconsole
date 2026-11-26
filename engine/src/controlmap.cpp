/*
  Q Light Controller Plus
  controlmap.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QKeySequence>
#include <QTimer>
#include <QDebug>
#include <climits>
#include <algorithm>

#include "outputrulekeeper.h"
#include "showtreecontroller.h"
#include "qlcinputprofile.h"
#include "qlcinputchannel.h"
#include "inputoutputmap.h"
#include "functionparent.h"
#include "showcontrol.h"
#include "mastertimer.h"
#include "controlmap.h"
#include "deskrecord.h"
#include "encoderbank.h"
#include "surfacepages.h"
#include "inputpatch.h"
#include "universe.h"
#include "qlcioplugin.h"
#include "function.h"
#include "chaser.h"
#include "doc.h"

#define KXMLTrigger QStringLiteral("Trigger")
#define KXMLUniverse QStringLiteral("Universe")
#define KXMLChannel QStringLiteral("Channel")
#define KXMLTarget QStringLiteral("Target")
#define KXMLID QStringLiteral("ID")
#define KXMLMode QStringLiteral("Mode")
#define KXMLKey QStringLiteral("Key")
#define KXMLRunList QStringLiteral("RunList")
#define KXMLLights QStringLiteral("Lights")
#define KXMLScope QStringLiteral("Scope")
#define KXMLScopeOf QStringLiteral("ScopeOf")

namespace
{
/* The MIDI plugin's channel numbering (plugins/midi/src/common/midiprotocol.h):
   each MIDI channel has 4096 input channels, CCs at 0, notes at 128. */
const quint32 kMidiChannelSpan = 4096;
const quint32 kNoteOffset = 128;
const quint32 kNoteAftertouch = 256;
const quint32 kProgramChange = 384;

QString noteName(quint32 note)
{
    static const char *names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return QStringLiteral("%1%2").arg(QLatin1String(names[note % 12])).arg(int(note / 12) - 1);
}
}

ControlMap::ControlMap(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
    Q_ASSERT(doc != nullptr);
    m_encoderBank = new EncoderBank(doc);
    /* A device coming back is heard always, not only while something is
       bound here: a VC button asks isTriggerDump() too (phase R). */
    if (InputOutputMap *iom = m_doc->inputOutputMap())
    {
        connect(iom, &InputOutputMap::inputDeviceConnected, this, &ControlMap::slotInputDeviceBack);
        // A controller's setup gave the board's lights back: ours again.
        connect(iom, &InputOutputMap::boardLampsReleased, this, &ControlMap::resendFeedback);
        connect(iom, &InputOutputMap::inputDeviceDisconnected, this,
                [this](quint32 universe) { m_backAt.remove(universe); });     // gone: no dump to wait for yet
    }
    m_encoderBank->setParent(this);
    m_surfacePages = new SurfacePages(doc);
    m_surfacePages->setParent(this);
    // The show's encoder bank on a board rests that board's pages.
    connect(m_encoderBank, &EncoderBank::deviceChanged, m_surfacePages, &SurfacePages::rescan);
    // A function deleted takes its binding with it.
    connect(m_doc, &Doc::functionRemoved, this, &ControlMap::slotFunctionRemoved);
    // Where a key may fire follows the mode and the show's cue lists.
    connect(m_doc, &Doc::modeChanged, this, &ControlMap::armingChanged);
    if (m_doc->showTree() != nullptr)
        connect(m_doc->showTree(), &ShowTreeController::hitArmingChanged, this, &ControlMap::armingChanged);
    // What may fire is what a light says (CS3): the same answer, shown.
    connect(this, &ControlMap::armingChanged, this, &ControlMap::slotLampsMayHaveChanged);
    /* A tree edit can change what a press would do (every cue of a list
       cut for the rehearsal, a cue deleted): the lights follow, through
       the same one-pass-per-turn queue (a show loading edits the tree
       thousands of times). */
    if (m_doc->showTree() != nullptr)
    {
        connect(m_doc->showTree(), &ShowTreeController::changed, this,
                &ControlMap::slotLampsMayHaveChanged);
        // A trigger's place deleted: widened, as a scoped hit is.
        connect(m_doc->showTree(), &ShowTreeController::changed, this, &ControlMap::slotTreeChanged);
    }
    // A show loaded: its tree and its triggers are both there now.
    connect(m_doc, &Doc::loaded, this, &ControlMap::fixPlaces);
}

ControlMap::~ControlMap()
{
}

/*****************************************************************************
 * Bindings
 *****************************************************************************/

const ControlMap::Binding *ControlMap::binding(const Target &target) const
{
    for (const Binding &b : m_bindings)
        if (b.target == target)
            return &b;
    return nullptr;
}

const ControlMap::Binding *ControlMap::bindingOn(quint32 universe, quint32 channel) const
{
    Binding b;
    if (!pick(key(universe, channel), &b, false))
        return nullptr;
    return binding(b.target);
}

QList<ControlMap::Binding> ControlMap::bindingsOn(quint32 universe, quint32 channel) const
{
    QList<Binding> out;
    for (int i : m_byControl.value(key(universe, channel)))
        out << m_bindings.at(i);
    return out;
}

const ControlMap::Binding *ControlMap::stopOn(quint64 k) const
{
    for (int i : m_byControl.value(k))
        if (m_bindings.at(i).target.type == EmergencyStopTarget)
            return &m_bindings.at(i);
    return nullptr;
}

void ControlMap::setBinding(const Binding &in, bool stepThrough)
{
    if (!in.target.isValid() || in.universe == 0xFFFFFFFF || in.channel == 0xFFFFFFFF)
        return;
    Binding b = in;
    // The emergency stop is everywhere, always (CONTROL_SCOPES §6).
    if (b.target.type == EmergencyStopTarget)
        b.place = Place();
    if (b.place.scope != Everywhere && b.place.scope != NewerPlace && b.place.id == 0xFFFFFFFF)
        b.place = Place();
    if (m_bindings.contains(b))
        return;         // already so
    // Set by hand: what fixPlaces() said about its old place is over.
    m_widened.remove(targetKey(b.target));
    QList<QPair<quint32, quint32>> left;        // controls a replaced binding leaves
    for (int i = m_bindings.count() - 1; i >= 0; --i)
    {
        const Binding &e = m_bindings.at(i);
        const bool sameControl = e.universe == b.universe && e.channel == b.channel;
        const bool stop = e.target.type == EmergencyStopTarget || b.target.type == EmergencyStopTarget;
        if (e.target == b.target || (sameControl && (stop || (!stepThrough && e.place == b.place))))
        {
            if (!sameControl)
                left << qMakePair(e.universe, e.channel);
            m_bindings.removeAt(i);
        }
    }
    if (b.target.type == EmergencyStopTarget)
        m_estopFlagged.clear();
    m_bindings.append(b);
    changed(true);
    // Its old control goes dark (or shows what is left there).
    for (const QPair<quint32, quint32> &c : std::as_const(left))
        sendControlLamp(c.first, c.second, true);
    sendControlLamp(b.universe, b.channel, true);
}

bool ControlMap::removeBinding(const Target &target)
{
    for (int i = 0; i < m_bindings.count(); ++i)
    {
        if (m_bindings.at(i).target != target)
            continue;
        const Binding gone = m_bindings.at(i);
        // A control held down keeps what it pressed: its release still lets go.
        if (target.type == EmergencyStopTarget)
            m_estopFlagged.clear();
        m_bindings.removeAt(i);
        m_widened.remove(targetKey(target));
        changed(true);
        // Dark, or what another binding on the control says now.
        sendControlLamp(gone.universe, gone.channel, true);
        return true;
    }
    return false;
}

bool ControlMap::setPlace(const Target &target, const Place &place)
{
    const Binding *b = binding(target);
    if (b == nullptr || target.type == EmergencyStopTarget)
        return false;
    if (b->place == place)
        return true;
    // A newer version's place is only ever kept, never chosen: what it
    // was cannot be written back from here.
    if (place.scope == NewerPlace)
        return false;
    // Another trigger on this control at that place: refused, never
    // replaced (Learn refuses it too, Q296/Q297). isPlaceTaken() says so.
    if (isPlaceTaken(*b, place))
        return false;
    Binding moved = *b;
    moved.place = place;
    setBinding(moved);
    return true;
}

bool ControlMap::isPlaceTaken(const Binding &b, const Place &place) const
{
    for (const Binding &e : bindingsOn(b.universe, b.channel))
        if (e.target != b.target && e.place == place)
            return true;
    return false;
}

bool ControlMap::setMode(const Target &target, Mode mode)
{
    for (Binding &b : m_bindings)
    {
        if (b.target != target)
            continue;
        if (b.mode != mode)
        {
            b.mode = mode;
            changed(true);
            // A Stop-only control on something idle does nothing: dark.
            sendLamps(false);
            emit lampsChanged();
        }
        return true;
    }
    return false;
}

void ControlMap::removeAll()
{
    if (m_bindings.isEmpty())
        return;
    for (const Binding &b : std::as_const(m_bindings))
        darkenLamp(b);
    m_bindings.clear();
    m_step.clear();
    m_estopFlagged.clear();
    m_widened.clear();
    // Controls held keep what they pressed: their releases still let go.
    changed(true);
}

void ControlMap::reset()
{
    m_encoderBank->reset();
    m_surfacePages->reset();
    m_blinking.clear();
    m_blinkLeft = 0;
    m_blinkRunning = false;
    /* The desk goes dark with the show (review finding 2). Without this a
       File > New left every learned control lit as the last show had it,
       and nothing was left to correct it with: the bindings and the record
       of what was sent both go here. A show loaded next sends its own
       lights (loadXML -> resendFeedback). */
    for (const Binding &b : std::as_const(m_bindings))
        darkenLamp(b);
    m_lampSent.clear();
    m_dimWhenReady = true;
    const bool had = !m_bindings.isEmpty() || m_listening || !m_down.isEmpty();
    m_bindings.clear();
    m_step.clear();
    m_down.clear();
    m_fired.clear();
    m_pressed.clear();
    m_swallow.clear();
    m_swallowAt.clear();
    m_swallowSeen.clear();
    m_estopHeld.clear();
    m_estopFlagged.clear();
    m_blockedHeld.clear();
    m_heldStarted.clear();
    m_listening = false;
    m_learnBind = false;
    m_learnButton = false;
    m_pending = false;
    const bool hadList = m_runTabList != 0xFFFFFFFF;
    m_runTabList = 0xFFFFFFFF;
    m_selected.clear();
    m_widened.clear();
    m_placeHomes.clear();
    if (had)
        changed(false);
    if (hadList)
        emit runTabListChanged(m_runTabList);
}

void ControlMap::changed(bool modified)
{
    rebuild();
    rememberPlaces();
    updateConnections();
    if (modified)
        m_doc->setModified();
    emit bindingsChanged();
}

void ControlMap::rebuild()
{
    m_byControl.clear();
    m_keyBindings = 0;
    for (int i = 0; i < m_bindings.count(); ++i)
    {
        m_byControl[key(m_bindings.at(i).universe, m_bindings.at(i).channel)] << i;
        if (isKey(m_bindings.at(i)))
            ++m_keyBindings;
    }
}

void ControlMap::updateConnections()
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    if (iom == nullptr)
        return;

    // Input: only while there is something to hear it for.
    // A control still held (its binding removed meanwhile) keeps its release coming.
    const bool wantInput = m_listening || !m_bindings.isEmpty() || !m_down.isEmpty();
    if (wantInput != m_inputConnected)
    {
        if (wantInput)
        {
            connect(iom, &InputOutputMap::inputValueChanged, this, &ControlMap::handleInput);
            connect(iom, &InputOutputMap::inputDeviceConnected, this, &ControlMap::slotInputDeviceChanged);
            connect(iom, &InputOutputMap::inputDeviceDisconnected, this, &ControlMap::slotInputDeviceChanged);
        }
        else
        {
            disconnect(iom, &InputOutputMap::inputValueChanged, this, &ControlMap::handleInput);
            disconnect(iom, &InputOutputMap::inputDeviceConnected, this, &ControlMap::slotInputDeviceChanged);
            disconnect(iom, &InputOutputMap::inputDeviceDisconnected, this, &ControlMap::slotInputDeviceChanged);
        }
        m_inputConnected = wantInput;
    }
    /* A device back (or its profile changed) gets every light again: the
       bindings' and the desk's (whose controls this map lights too). */
    const bool wantResend = wantInput || !m_deskLamps.isEmpty();
    if (wantResend != m_deviceResendConnected)
    {
        if (wantResend)
        {
            connect(iom, &InputOutputMap::inputDeviceConnected, this, &ControlMap::slotInputDeviceConnected);
            connect(iom, &InputOutputMap::profileChanged, this, &ControlMap::slotInputDeviceConnected);
        }
        else
        {
            disconnect(iom, &InputOutputMap::inputDeviceConnected, this, &ControlMap::slotInputDeviceConnected);
            disconnect(iom, &InputOutputMap::profileChanged, this, &ControlMap::slotInputDeviceConnected);
        }
        m_deviceResendConnected = wantResend;
    }

    // Feedback: what runs, from the timer thread (queued here).
    MasterTimer *timer = m_doc->masterTimer();
    const bool wantFeedback = hasLamps();
    if (timer != nullptr && wantFeedback != m_feedbackConnected)
    {
        if (wantFeedback)
        {
            connect(timer, &MasterTimer::functionStarted, this, &ControlMap::slotFunctionStarted,
                    Qt::QueuedConnection);
            connect(timer, &MasterTimer::functionStopped, this, &ControlMap::slotFunctionStopped,
                    Qt::QueuedConnection);
        }
        else
        {
            disconnect(timer, &MasterTimer::functionStarted, this, &ControlMap::slotFunctionStarted);
            disconnect(timer, &MasterTimer::functionStopped, this, &ControlMap::slotFunctionStopped);
        }
        m_feedbackConnected = wantFeedback;
    }

    /* The status sources behind the lights (CS3): a GO carried out, a list
       stopped, a command the guard held, the emergency stop struck or over.
       Each arrives on the thread the command came in on (a plugin's, the
       MasterTimer's), so each is queued: this class is GUI-thread only. */
    ShowControl *sc = m_doc->showControl();
    OutputRuleKeeper *rules = m_doc->outputRuleKeeper();
    if (wantFeedback != m_stateConnected && sc != nullptr && rules != nullptr)
    {
        if (wantFeedback)
        {
            connect(sc, &ShowControl::cueTaken, this, &ControlMap::slotLampsMayHaveChanged,
                    Qt::QueuedConnection);
            connect(sc, &ShowControl::cueListStopped, this, &ControlMap::slotLampsMayHaveChanged,
                    Qt::QueuedConnection);
            connect(sc, &ShowControl::commandHeld, this, &ControlMap::slotGuardHeld,
                    Qt::QueuedConnection);
            connect(rules, &OutputRuleKeeper::emergencyStopStruck, this,
                    &ControlMap::slotLampsMayHaveChanged, Qt::QueuedConnection);
            connect(rules, &OutputRuleKeeper::emergencyStopEnded, this,
                    &ControlMap::slotLampsMayHaveChanged, Qt::QueuedConnection);
        }
        else
        {
            disconnect(sc, &ShowControl::cueTaken, this, &ControlMap::slotLampsMayHaveChanged);
            disconnect(sc, &ShowControl::cueListStopped, this, &ControlMap::slotLampsMayHaveChanged);
            disconnect(sc, &ShowControl::commandHeld, this, &ControlMap::slotGuardHeld);
            disconnect(rules, &OutputRuleKeeper::emergencyStopStruck, this,
                       &ControlMap::slotLampsMayHaveChanged);
            disconnect(rules, &OutputRuleKeeper::emergencyStopEnded, this,
                       &ControlMap::slotLampsMayHaveChanged);
        }
        m_stateConnected = wantFeedback;
    }
}

/*****************************************************************************
 * Learning
 *****************************************************************************/

void ControlMap::listen()
{
    m_listening = true;
    m_learnBind = false;
    m_learnButton = false;
    m_pending = false;
    m_notButtons.clear();
    updateConnections();
}

void ControlMap::listenForButton()
{
    m_listening = true;
    m_learnBind = false;
    m_learnButton = true;
    m_pending = false;
    m_notButtons.clear();
    updateConnections();
}

bool ControlMap::isButtonControl(quint32 universe, quint32 channel) const
{
    if (universe == KeyboardUniverse)
        return false;
    InputOutputMap *iom = m_doc->inputOutputMap();
    InputPatch *ip = iom == nullptr ? nullptr : iom->inputPatch(universe);
    if (ip == nullptr)
        return true;
    if (ip->profile() != nullptr)
    {
        const QLCInputChannel *ich = ip->profile()->channel(channel);
        if (ich != nullptr)
        {
            switch (ich->type())
            {
                case QLCInputChannel::Slider:
                case QLCInputChannel::Knob:
                case QLCInputChannel::Encoder:
                    return false;
                default:
                    return true;
            }
        }
    }
    if (ip->pluginName() == QLatin1String("MIDI"))
    {
        // CCs and notes; a program change. Not aftertouch or pitch bend.
        const quint32 c = channel % kMidiChannelSpan;
        return c < kNoteAftertouch || (c >= kProgramChange && c < kProgramChange + 128);
    }
    return true;
}

bool ControlMap::learnButtonHeard(quint32 universe, quint32 channel, uchar value)
{
    const quint64 k = key(universe, channel);
    if (m_notButtons.contains(k) || !isButtonControl(universe, channel))
    {
        m_notButtons.insert(k);
        m_pending = false;
        emit refused(tr("%1 is a fader, knob or encoder. The emergency stop takes a button.")
                         .arg(controlName(universe, channel)));
        return false;
    }
    const bool fresh = !m_pending || m_pendingKey != k || m_pendingAt.elapsed() > m_confirmMs;
    if (fresh)
    {
        m_pending = true;
        m_pendingKey = k;
        m_pendingValue = value;
        m_pendingReleased = false;
        m_pendingAt.start();
        emit confirming(universe, channel);
        return false;
    }
    if (!m_pendingReleased)
    {
        // No release between two presses: a held button repeating says the
        // same value, a fader moving does not.
        if (value != m_pendingValue)
        {
            m_pending = false;
            m_notButtons.insert(k);
            emit refused(tr("%1 sends a range of values: it is a fader or knob, not a button. "
                            "The emergency stop takes a button.").arg(controlName(universe, channel)));
        }
        return false;
    }
    m_pending = false;
    return true;
}

void ControlMap::learn(const Target &target, Mode mode)
{
    m_listening = true;
    m_learnBind = true;
    m_learnButton = target.type == EmergencyStopTarget;
    m_pending = false;
    m_notButtons.clear();
    m_learning = Binding();
    m_learning.target = target;
    m_learning.mode = mode;
    updateConnections();
}

void ControlMap::stopListening()
{
    if (!m_listening)
        return;
    m_listening = false;
    m_learnBind = false;
    m_learnButton = false;
    m_pending = false;
    m_notButtons.clear();
    m_learnSeen.clear();
    updateConnections();
}

bool ControlMap::movesLikeFader(bool releasedSince, qint64 sinceLastMs, uchar last, uchar value, int windowMs)
{
    return !releasedSince && sinceLastMs < windowMs && value != last;
}

void ControlMap::observeLearn(quint64 k, uchar value)
{
    LearnSeen &s = m_learnSeen[k];
    const qint64 since = s.any ? s.at.elapsed() : 0;
    if (s.any)
    {
        // The stop's own rule: different values, no 0 between, under its window.
        if (value != 0 && movesLikeFader(s.last == 0, since, s.last, value, m_estopDebounceMs))
            s.fader = true;
        if (value == s.last)
            return;         // a held button repeating: not a step of anything
        // Latching: one "on" value and 0, alternating, a press apart.
        const bool onOff = value == 0 || s.on == 0 || value == s.on;
        if (!onOff)
            s.latchSteps = 0;
        else if (value == 0 && since < m_latchHoldMs)
        {
            /* A release inside a press's hold: momentary. A latching
               button's 0 is the next press, which comes later than a
               deliberate press is held (a Learn press held half a second
               is still a momentary button). */
            s.quickRelease = true;
            s.latchSteps = 0;
        }
        else if (since < m_latchGapMs)
            s.latchSteps = 0;
        else if (s.latchSteps > 0)
            ++s.latchSteps;
    }
    if (value != 0)
    {
        if (s.on == 0)
            s.on = value;
        if (s.latchSteps == 0 && value == s.on)
            s.latchSteps = 1;
    }
    s.any = true;
    s.last = value;
    s.at.start();
}

bool ControlMap::learnSawFader(quint32 universe, quint32 channel) const
{
    if (universe == KeyboardUniverse)
        return false;
    if (!isButtonControl(universe, channel))
        return true;
    const auto it = m_learnSeen.constFind(key(universe, channel));
    return it != m_learnSeen.constEnd() && it->fader;
}

bool ControlMap::learnSawLatching(quint32 universe, quint32 channel) const
{
    const auto it = m_learnSeen.constFind(key(universe, channel));
    return it != m_learnSeen.constEnd() && !it->fader && !it->quickRelease && it->latchSteps >= 3;
}

/*****************************************************************************
 * Names
 *****************************************************************************/

quint32 ControlMap::keyChannel(int key, Qt::KeyboardModifiers modifiers)
{
    switch (key)
    {
        case 0:
        case Qt::Key_unknown:
        case Qt::Key_Shift:
        case Qt::Key_Control:
        case Qt::Key_Meta:
        case Qt::Key_Alt:
        case Qt::Key_AltGr:
        case Qt::Key_CapsLock:
        case Qt::Key_NumLock:
        case Qt::Key_Super_L:
        case Qt::Key_Super_R:
        case Qt::Key_Hyper_L:
        case Qt::Key_Hyper_R:
            return 0;
        default:
            break;
    }
    const Qt::KeyboardModifiers mods = modifiers & (Qt::ShiftModifier | Qt::ControlModifier
                                                    | Qt::AltModifier | Qt::MetaModifier);
    return quint32(QKeyCombination(mods, Qt::Key(key)).toCombined());
}

QString ControlMap::keyName(quint32 channel)
{
    return QKeySequence(QKeyCombination::fromCombined(int(channel))).toString(QKeySequence::NativeText);
}

QString ControlMap::controlName(quint32 universe, quint32 channel) const
{
    if (universe == KeyboardUniverse)
        return tr("Key · %1").arg(keyName(channel));
    InputOutputMap *iom = m_doc->inputOutputMap();
    QString uni = iom == nullptr ? QString() : iom->getUniverseNameByIndex(universe);
    if (uni.isEmpty())
        uni = tr("Universe %1").arg(universe + 1);

    InputPatch *ip = iom == nullptr ? nullptr : iom->inputPatch(universe);
    QString what;
    if (ip != nullptr && ip->profile() != nullptr)
    {
        QLCInputChannel *ich = ip->profile()->channel(channel);
        if (ich != nullptr && !ich->name().isEmpty())
            what = ich->name();
    }
    if (what.isEmpty() && ip != nullptr && ip->plugin() != nullptr)
    {
        what = ip->plugin()->inputChannelName(ip->input(), channel);
        if (what.isEmpty() && ip->pluginName() == QLatin1String("MIDI"))
        {
            const quint32 midiCh = channel / kMidiChannelSpan + 1;
            const quint32 c = channel % kMidiChannelSpan;
            if (c < kNoteOffset)
                what = tr("CC %1, MIDI ch %2").arg(c).arg(midiCh);
            else if (c < kNoteAftertouch)
                what = tr("Note %1 (%2), MIDI ch %3").arg(c - kNoteOffset).arg(noteName(c - kNoteOffset)).arg(midiCh);
            else if (c >= kProgramChange && c < kProgramChange + 128)
                what = tr("Program %1, MIDI ch %2").arg(c - kProgramChange).arg(midiCh);
        }
        else if (what.isEmpty() && ip->pluginName() == QLatin1String("OSC"))
        {
            what = tr("OSC message %1").arg(channel + 1);
        }
    }
    if (what.isEmpty())
        what = tr("Channel %1").arg(channel + 1);
    return tr("%1 · %2").arg(uni, what);
}

bool ControlMap::targetExists(const Target &target) const
{
    switch (target.type)
    {
        case FunctionTarget:
            return m_doc->function(target.id) != nullptr;
        case CueListTarget:
        case CueListBackTarget:
            return m_doc->showTree()->cueList(target.id) != nullptr;
        case CueTarget:
        case HitTarget:
            return m_doc->showTree()->cue(target.id) != nullptr;
        case EmergencyStopTarget:
            return true;
    }
    return false;
}

QString ControlMap::targetName(const Target &target) const
{
    ShowTreeController *st = m_doc->showTree();
    switch (target.type)
    {
        case FunctionTarget:
        {
            Function *f = m_doc->function(target.id);
            if (f == nullptr)
                break;
            return tr("%1: %2").arg(Function::typeToString(f->type()), f->name());
        }
        case CueListTarget:
        {
            const ShowTree::CueList *l = st->cueList(target.id);
            if (l == nullptr)
                break;
            return tr("Cue list: %1").arg(l->name);
        }
        case CueListBackTarget:
        {
            const ShowTree::CueList *l = st->cueList(target.id);
            if (l == nullptr)
                break;
            return tr("Back: %1").arg(l->name);
        }
        case CueTarget:
        {
            const ShowTree::Cue *c = st->cue(target.id);
            const ShowTree::CueList *l = st->listOfCue(target.id);
            if (c == nullptr || l == nullptr)
                break;
            const QString number = st->cueNumber(*l, l->indexOf(target.id));
            return c->name.isEmpty() ? tr("Cue %1 of %2").arg(number, l->name)
                                     : tr("Cue %1 %2 of %3").arg(number, c->name, l->name);
        }
        case HitTarget:
        {
            const ShowTree::Cue *c = st->cue(target.id);
            if (c == nullptr)
                break;
            return tr("Hit: %1").arg(c->name);
        }
        case EmergencyStopTarget:
            return tr("Emergency stop (ALL OFF)");
    }
    return tr("(gone)");
}

QString ControlMap::modeName(Mode mode, TargetType type)
{
    const bool list = isListType(type);
    if (type == EmergencyStopTarget)
        return tr("ALL OFF (each press; never held)");
    if (type == CueListBackTarget)
        return mode == StopOnly ? tr("Stop the list") : tr("Back");
    switch (mode)
    {
        case Toggle:    return list ? tr("GO") : tr("Toggle");
        case Momentary: return list ? tr("GO while held") : tr("Momentary (on while held)");
        case StartOnly: return list ? tr("GO") : tr("Start only");
        case StopOnly:  return list ? tr("Stop the list") : tr("Stop only");
    }
    return QString();
}

QList<ControlMap::Mode> ControlMap::modesFor(TargetType type)
{
    switch (type)
    {
        case EmergencyStopTarget:
            return { StartOnly };
        case CueListBackTarget:
            // Back acts on the press; held, the release does nothing.
            return { Toggle, StopOnly };
        case CueListTarget:
        case CueTarget:
            return { Toggle, Momentary, StopOnly };
        case FunctionTarget:
        case HitTarget:
            break;
    }
    return { Toggle, Momentary, StartOnly, StopOnly };
}

QString ControlMap::typeToString(TargetType type)
{
    switch (type)
    {
        case FunctionTarget: return QStringLiteral("Function");
        case CueListTarget:  return QStringLiteral("CueList");
        case CueTarget:      return QStringLiteral("Cue");
        case HitTarget:      return QStringLiteral("Hit");
        case EmergencyStopTarget: return QStringLiteral("EmergencyStop");
        case CueListBackTarget: return QStringLiteral("CueListBack");
    }
    return QString();
}

bool ControlMap::stringToType(const QString &str, TargetType *type)
{
    static const TargetType all[] = { FunctionTarget, CueListTarget, CueTarget, HitTarget,
                                      EmergencyStopTarget, CueListBackTarget };
    for (TargetType t : all)
    {
        if (typeToString(t) == str)
        {
            *type = t;
            return true;
        }
    }
    return false;
}

QString ControlMap::modeToString(Mode mode)
{
    switch (mode)
    {
        case Toggle:    return QStringLiteral("Toggle");
        case Momentary: return QStringLiteral("Momentary");
        case StartOnly: return QStringLiteral("StartOnly");
        case StopOnly:  return QStringLiteral("StopOnly");
    }
    return QString();
}

bool ControlMap::stringToMode(const QString &str, Mode *mode)
{
    static const Mode all[] = { Toggle, Momentary, StartOnly, StopOnly };
    for (Mode m : all)
    {
        if (modeToString(m) == str)
        {
            *mode = m;
            return true;
        }
    }
    return false;
}

/*****************************************************************************
 * Input
 *****************************************************************************/

void ControlMap::handleInput(quint32 universe, quint32 channel, uchar value)
{
    const quint64 k = key(universe, channel);
    /* This computer's desk has the control (Q309, Q315): its STOP, GM, GO or
       Back. No trigger here is armed on it, the show's stop included (on the
       desk's STOP the desk stops the show itself; on another role it is
       blocked), and a Learn is told whose it is. A press held from before
       the desk took it still lets go. */
    DeskRecord::Role role = DeskRecord::Stop;
    if (universe != KeyboardUniverse && m_doc->deskRecord() != nullptr
        && m_doc->deskRecord()->isDeskControl(universe, channel, &role)
        && !(value == 0 && m_down.contains(k)))
    {
        if (m_listening && value > 0)
            emit refused(tr("%1 is this computer's %2 (Connections ▸ Control surfaces). Choose another control.")
                             .arg(controlName(universe, channel), DeskRecord::roleName(role)));
        return;
    }
    /* A surface page has it in Design (SurfacePages, Q332 a): the show's
       trigger on it waits for Operate. A Learn still hears it, so a key can
       be given its Operate job from Design. */
    if (!m_listening && universe != KeyboardUniverse && m_doc->inputOutputMap() != nullptr
        && m_doc->inputOutputMap()->isPageControl(universe, channel) && !(value == 0 && m_down.contains(k)))
        return;
    // Learning: what the control sends says whether it is a fader or a latch.
    if (m_listening && universe != KeyboardUniverse)
        observeLearn(k, value);
    if (value == 0)
    {
        // A release: the stop's next non-zero is a new press, and what was
        // said about a held press can be said again.
        auto h = m_estopHeld.find(k);
        if (h != m_estopHeld.end())
            h->released = true;
        m_blockedHeld.remove(k);
        // A real release arms a trigger after a replug (isTriggerDump()).
        if (!m_backAt.isEmpty())
            isTriggerDump(universe, channel, 0, QString());
    }

    // The emergency stop comes before anything else, learning included: a
    // learn that is left listening (a panel closed, a dialog open) must never
    // eat the press that stops the show. Only the press that taught it, still
    // held, is swallowed below.
    /* A device that came back sends its whole state (phase R): a stop that
       said 0 first is at rest, and a non-zero right after it is the dump,
       not a press -- ignored once, never more (isStateDump()). A first
       non-zero is a press (it fires, once). */
    if (universe != KeyboardUniverse && !m_backAt.isEmpty() && stopOn(k) != nullptr
        && isStateDump(k, value))
        return;

    if (value > 0 && universe != KeyboardUniverse)
    {
        const Binding *stop = stopOn(k);
        const bool isStop = stop != nullptr;
        // The press that taught the stop is swallowed until its release; a
        // release that was lost must not swallow the first real press for long.
        if (isStop && m_swallow.contains(k))
        {
            // The first value after it may be the held press repeating; a
            // second one, or a second of quiet, is a real press.
            if (m_swallowSeen.contains(k) || (m_swallowAt.contains(k) && m_swallowAt.value(k).elapsed() > 1000))
                endSwallow(k);
            else
                m_swallowSeen.insert(k);
        }
        if (isStop && !m_swallow.contains(k))
        {
            emergencyStop(*stop, value);
            return;
        }
    }

    /* This computer's desk is learning a role: its presses fire nothing of
       the show's (as this map's own Learn fires nothing). The emergency
       stop above still stops; releases still let go. */
    if (value > 0 && universe != KeyboardUniverse && m_doc->deskRecord() != nullptr
        && m_doc->deskRecord()->isLearning())
        return;
    // A controller's setup is listening to this board (Identify): the same.
    if (value > 0 && universe != KeyboardUniverse && m_doc->inputOutputMap()->isSetupHeld(universe))
        return;

    // A computer key is learned by the dialog itself, never from here.
    if (m_listening && m_learnButton && m_pending && value == 0 && k == m_pendingKey)
        m_pendingReleased = true;
    if (m_listening && value > 0 && universe != KeyboardUniverse)
    {
        if (stopOn(k) != nullptr)
        {
            // Its held learn press (swallowed above): not a new choice.
            emit refused(tr("%1 is the emergency stop. Choose another control.")
                             .arg(controlName(universe, channel)));
            return;
        }
        // The emergency stop learns a button, and a second press says so.
        if (m_learnButton && !learnButtonHeard(universe, channel, value))
            return;
        const bool bind = m_learnBind;
        m_listening = false;
        m_learnBind = false;
        m_learnButton = false;
        // The press that taught it is held: it doesn't fire what it was
        // bound to, and its release isn't a release of the binding.
        // A control still held from before (its press never let go) is let
        // go first, so the new binding's first press is a press.
        if (m_down.remove(k))
        {
            const Binding held = m_pressed.take(k);
            if (m_fired.remove(k) && held.target.isValid())
                release(held);
        }
        m_swallow.insert(k);
        m_swallowAt[k].start();
        m_swallowSeen.remove(k);
        if (bind)
        {
            Binding nb = m_learning;
            nb.universe = universe;
            nb.channel = channel;
            setBinding(nb);
        }
        else
        {
            updateConnections();
        }
        emit heard(universe, channel);
        return;
    }

    if (!m_swallow.isEmpty() && m_swallow.contains(k))
    {
        if (value == 0)
            endSwallow(k);
        return;
    }
    if (value == 0)
    {
        // A release lets go of what its press fired, as it was bound then.
        if (!m_down.remove(k))
            return;
        const Binding pressed = m_pressed.take(k);
        if (m_fired.remove(k) && pressed.target.isValid())
            release(pressed);
        if (m_bindings.isEmpty())
            updateConnections();        // nothing held, nothing bound: stop listening
        return;
    }
    if (!m_byControl.contains(k))
        return;
    // A press is the edge from 0: a fader or an aftertouch going on
    // sending values is still the one press.
    if (m_down.contains(k))
        return;
    /* After a replug the device dumps its state: a "pressed" with no
       release first is not the operator's, and an unwanted GO is worse
       than a missed one (phase R). The stop has its own rule above. The
       control counts as held: one really held across the replug and
       resending fires only after a release then a press. */
    if (!m_backAt.isEmpty() && stopOn(k) == nullptr
        && isTriggerDump(universe, channel, value, dumpName(k)))
    {
        m_down.insert(k);
        return;
    }
    m_down.insert(k);
    /* The narrowest armed place has the control (Q296); several there step
       through, one per press (Q303). None armed: one whose function runs
       may still let go of it (letting go is always allowed); else the
       press does nothing, and says where its triggers work. */
    Binding b;
    bool live = true;
    if (!pick(k, &b, true))
    {
        const Binding *running = runningOn(k);
        if (running == nullptr)
        {
            sayNotLive(k);
            return;
        }
        b = *running;
        live = false;
    }
    m_pressed.insert(k, b);
    if (press(b, live))
        m_fired.insert(k);
}

QString ControlMap::stopProblem() const
{
    const Binding *b = binding(emergencyStop());
    if (b == nullptr)
        return QString();
    if (isKey(*b))
        return tr("a computer key cannot be the stop");
    if (!isButtonControl(b->universe, b->channel))
        return tr("is a fader or knob");
    if (m_estopFlagged.contains(key(b->universe, b->channel)))
        return tr("moves like a fader");
    return QString();
}

void ControlMap::say(quint64 k, const QString &why)
{
    auto it = m_blockedHeld.find(k);
    if (it != m_blockedHeld.end() && it->elapsed() < 1000)
        return;
    m_blockedHeld[k].start();
    emit blocked(why);
}

void ControlMap::sayNotLive(quint64 k)
{
    const quint32 universe = quint32(k >> 32), channel = quint32(k & 0xFFFFFFFF);
    // A hit at a live place that may not fire: its own reason, not "not live here".
    if (const Binding *hit = refusedHitOn(k))
    {
        say(k, tr("Not fired: %1. %2").arg(targetName(hit->target), refusalFor(hit->target)));
        return;
    }
    say(k, tr("Not fired: %1 is not live here. %2")
               .arg(controlName(universe, channel), placesText(bindingsOn(universe, channel))));
}

void ControlMap::slotInputDeviceChanged(quint32 universe)
{
    // The controller went away or came back: whatever it held is let go (its
    // release never comes), and a first press waiting for its second is dropped.
    m_pending = false;
    for (const quint64 k : m_swallow.values())
        if (quint32(k >> 32) == universe)
            endSwallow(k);
    for (const quint64 k : m_estopHeld.keys())
        if (quint32(k >> 32) == universe)
            m_estopHeld.remove(k);
    for (const quint64 k : m_swallowAt.keys())
        if (quint32(k >> 32) == universe)
            m_swallowAt.remove(k);
    QList<quint32> held;
    for (quint64 k : std::as_const(m_down))
        if (quint32(k >> 32) == universe)
            held << quint32(k & 0xFFFFFFFF);
    m_releasingLost = true;
    for (quint32 ch : held)
        handleInput(universe, ch, 0);
    m_releasingLost = false;
}

void ControlMap::slotInputDeviceBack(quint32 universe)
{
    m_backAt[universe].start();
    for (const quint64 k : m_backHeard.values())
        if (quint32(k >> 32) == universe)
            m_backHeard.remove(k);
    for (const quint64 k : m_backAtRest.values())
        if (quint32(k >> 32) == universe)
            m_backAtRest.remove(k);
    for (const quint64 k : m_backReleased.values())
        if (quint32(k >> 32) == universe)
            m_backReleased.remove(k);
    for (const quint64 k : m_backSaid.values())
        if (quint32(k >> 32) == universe)
            m_backSaid.remove(k);
}

bool ControlMap::isTriggerDump(quint32 universe, quint32 channel, uchar value, const QString &what)
{
    if (universe == KeyboardUniverse || m_releasingLost)
        return false;
    auto at = m_backAt.find(universe);
    if (at == m_backAt.end())
        return false;
    if (at->elapsed() >= m_dumpWindowMs)
    {
        // The window is over: everything from here is the operator's.
        m_backAt.erase(at);
        return false;
    }
    const quint64 k = key(universe, channel);
    /* The first kDumpBurstMs is the device's burst, never a hand: OpenDeck
       dumps a key as 0, 127, 0, and its leading 0 is no release. Anything
       non-zero in it is the dump; a 0 in it arms nothing. */
    const bool burst = at->elapsed() < m_dumpBurstMs;
    if (value == 0)
    {
        if (!burst)
            m_backReleased.insert(k);   // a release: its next press is one
        return false;
    }
    if (!burst && m_backReleased.contains(k))
        return false;
    // "Pressed" with no release first: the device's dump. Ignored, said once.
    if (!m_backSaid.contains(k))
    {
        m_backSaid.insert(k);
        qDebug() << "[ControlMap]" << what << "value" << value << "ignored: the device's state dump after it came back";
        emit stateDumpIgnored(universe, channel, what);
    }
    return true;
}

QString ControlMap::dumpName(quint64 k) const
{
    const auto on = m_byControl.value(k);
    if (on.isEmpty())
        return QString();
    const Target &t = m_bindings.at(on.first()).target;
    ShowTreeController *st = m_doc->showTree();
    const ShowTree::CueList *l = st != nullptr ? st->cueList(t.id) : nullptr;
    if (t.type == CueListTarget && l != nullptr)
        return tr("GO of %1").arg(l->name);
    if (t.type == CueListBackTarget && l != nullptr)
        return tr("Back of %1").arg(l->name);
    return targetName(t);
}

bool ControlMap::isStateDump(quint64 k, uchar value)
{
    const quint32 universe = quint32(k >> 32);
    auto at = m_backAt.find(universe);
    if (at == m_backAt.end())
        return false;
    if (at->elapsed() >= m_dumpWindowMs)
    {
        // The window is over: everything from here is the operator's.
        m_backAt.erase(at);
        return false;
    }
    if (!m_backHeard.contains(k))
    {
        m_backHeard.insert(k);
        if (value == 0)
            m_backAtRest.insert(k);
        return false;
    }
    if (!m_backAtRest.contains(k))
        return false;
    /* At most ONE value is ignored per replug (the e-stop rule: a bound stop
       is never lost for more than one press). That one, or the next 0 (a
       real release), turns the guard off: the press after it fires. */
    m_backAtRest.remove(k);
    if (value == 0)
        return false;
    qDebug() << "[ControlMap] stop value" << value << "ignored: the device's state dump after it came back";
    emit stateDumpIgnored(universe, quint32(k & 0xFFFFFFFF), tr("STOP"));
    return true;
}

QString ControlMap::stopDeviceMissing() const
{
    const Binding *b = binding(emergencyStop());
    if (b == nullptr || isKey(*b))
        return QString();
    InputOutputMap *iom = m_doc->inputOutputMap();
    if (iom == nullptr)
        return QString();
    if (iom->inputPatch(b->universe) != nullptr)
        return QString();
    Universe *uni = iom->universe(b->universe);
    if (uni != nullptr)
        for (const Universe::OfflinePatch &rec : uni->offlinePatches())
            if (rec.tag == Universe::InputPatchTag && !rec.uid.isEmpty())
                return tr("its device '%1' is not connected").arg(rec.uid);
    return tr("universe %1 has no input patched").arg(b->universe + 1);
}

void ControlMap::emergencyStop(const Binding &b, uchar value)
{
    // The rule (Branson, 2026-11-19): a bound stop is never lost for more than
    // one press, and an extra ALL OFF is better than a missed one. Two
    // clauses. (a) A value above 0 that follows a 0 (or the first one after a
    // load or a replug) fires. (b) Otherwise it fires when a second has
    // passed since the last ALL OFF this control sent: a held button that
    // keeps resending, or a fader moving, stops the show about once a second.
    // Different values with no 0 inside a second are a run that moves like a
    // fader: flagged loudly, never disarmed.
    const quint64 k = key(b.universe, b.channel);
    if (!isButtonControl(b.universe, b.channel))
    {
        // The profile says fader or knob: said, once per hold. It still
        // fires (an extra ALL OFF beats a missed one), under the same rule.
        say(k, tr("The emergency stop is bound to %1, which is a fader or knob: learn it again on a "
                  "button. It still stops the show, but not on every move.")
                   .arg(controlName(b.universe, b.channel)));
    }
    bool press = true;
    bool differs = false;
    auto it = m_estopHeld.find(k);
    if (it == m_estopHeld.end())
    {
        StopHold h;
        h.value = value;
        h.last.start();
        it = m_estopHeld.insert(k, h);
    }
    else
    {
        differs = movesLikeFader(it->released, it->last.elapsed(), it->value, value, m_estopDebounceMs);
        press = it->released || !it->lastFired.isValid() || it->lastFired.elapsed() >= m_estopDebounceMs;
        it->value = value;
        it->last.start();
        it->released = false;
    }
    // Different values with no 0 between and under a second apart: a fader.
    if (differs && !m_estopFlagged.contains(k))
    {
        m_estopFlagged.insert(k);
        emit blocked(tr("The emergency stop is bound to %1, which moves like a fader: learn it again on "
                        "a button. It still stops the show, but not on every move.")
                         .arg(controlName(b.universe, b.channel)));
        emit stopStateChanged();
    }
    if (!press)
        return;
    // The one ALL OFF path (ShowControl::allOff()); never held.
    if (m_doc->showControl()->allOff(ShowControl::RouteControlMap))
    {
        // The second's wait starts only when ALL OFF ran: a refused one
        // must not hold back the next.
        it->lastFired.start();
        emit fired(tr("ALL OFF"));
    }
}

bool ControlMap::press(const Binding &b, bool placeLive)
{
    // A computer key was checked where it is pressed (keyMayFire()).
    const bool may = placeLive && (isKey(b) || surfaceMayFire(b.target));
    auto notFired = [this, &b, placeLive]() {
        say(key(b.universe, b.channel),
            placeLive ? tr("Not fired: %1. %2").arg(targetName(b.target), refusalFor(b.target))
                      : tr("Not fired: %1 works only %2.").arg(targetName(b.target), placeName(b.place)));
    };
    switch (b.target.type)
    {
        case FunctionTarget:
        case HitTarget:
        {
            const quint32 fid = b.target.type == FunctionTarget ? b.target.id : feedbackFunction(b);
            const QString what = targetName(b.target);
            Function *f = fid == 0xFFFFFFFF ? nullptr : m_doc->function(fid);
            // What is running can always be let go; only a start is held back.
            const bool running = f != nullptr && f->isRunning();
            switch (b.mode)
            {
                case Toggle:
                    if (may || running)
                        actOnFunction(fid, -1, what);
                    else
                        notFired();
                    break;
                case StartOnly:
                    if (may)
                        actOnFunction(fid, 1, what);
                    else
                        notFired();
                    break;
                case StopOnly:  actOnFunction(fid, 0, what); break;
                case Momentary:
                    if (!may)
                    {
                        notFired();
                    }
                    else if (f != nullptr && !f->isRunning())
                    {
                        m_heldStarted.insert(fid);
                        actOnFunction(fid, 1, what);
                    }
                    break;
            }
            return may;
        }
        case CueListTarget:
        case CueTarget:
        case CueListBackTarget:
            if (b.mode == StopOnly)
            {
                goList(b, true);
                return false;
            }
            if (!may)
            {
                notFired();
                return false;
            }
            goList(b, false);
            return true;
        case EmergencyStopTarget:
            break;
    }
    return false;
}

void ControlMap::release(const Binding &b)
{
    if (b.mode != Momentary)
        return;
    switch (b.target.type)
    {
        case FunctionTarget:
        case HitTarget:
        {
            const quint32 fid = b.target.type == FunctionTarget ? b.target.id : feedbackFunction(b);
            if (m_heldStarted.remove(fid))
                actOnFunction(fid, 0, targetName(b.target));
            break;
        }
        case CueListTarget:
        case CueTarget:
            goList(b, true);
            break;
        case CueListBackTarget:     // Back is the press's; the release lets go of nothing
        case EmergencyStopTarget:
            break;
    }
}

void ControlMap::actOnFunction(quint32 fid, int how, const QString &what)
{
    Function *f = fid == 0xFFFFFFFF ? nullptr : m_doc->function(fid);
    if (f == nullptr)
        return;
    const bool start = how < 0 ? !f->isRunning() : how > 0;
    if (start && !f->isRunning())
    {
        f->start(m_doc->masterTimer(), FunctionParent::controlMap());
        emit fired(tr("Started %1").arg(what));
    }
    else if (!start && f->isRunning())
    {
        f->stop(FunctionParent::controlMap());
        emit fired(tr("Stopped %1").arg(what));
    }
}

Chaser *ControlMap::targetChaser(const Binding &b, int *step, int *subCue) const
{
    if (step != nullptr)
        *step = -1;
    if (subCue != nullptr)
        *subCue = -1;
    ShowTreeController *st = m_doc->showTree();
    if (st == nullptr || !isListType(b.target.type))
        return nullptr;
    // A cue bound and then made a sub-cue (Make Sub-cues): GO plays its
    // cue's step at that sub-cue, as /go 4.2 does.
    quint32 parent = 0xFFFFFFFF;
    const bool sub = b.target.type == CueTarget && st->isSubCue(b.target.id, &parent);
    const ShowTree::CueList *list = b.target.type != CueTarget ? st->cueList(b.target.id)
                                                               : st->listOfCue(sub ? parent : b.target.id);
    if (list == nullptr || list->chaser == ShowTree::invalidId())
        return nullptr;
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(list->chaser));
    if (chaser == nullptr || b.target.type != CueTarget)
        return chaser;

    const ShowTree::Cue *c = st->cue(sub ? parent : b.target.id);
    // A cue with sub-cues plays their chaser as its step.
    const ShowTree::CueList *subs = c == nullptr ? nullptr : st->cueList(st->subCueList(c->id));
    const quint32 fid = subs != nullptr ? subs->chaser : c != nullptr ? c->collection : ShowTree::invalidId();
    int at = -1;
    for (int i = 0; c != nullptr && i < chaser->stepsCount(); ++i)
    {
        if (chaser->stepAt(i) != nullptr && chaser->stepAt(i)->fid == fid)
        {
            at = i;
            break;
        }
    }
    if (step != nullptr)
        *step = at;
    if (!sub)
        return chaser;
    // A sub-cue the list no longer holds: nothing to GO.
    const ShowTree::CueList *own = st->listOfCue(b.target.id);
    const int index = own == nullptr ? -1 : own->indexOf(b.target.id);
    if (index < 0)
        return nullptr;
    if (subCue != nullptr)
        *subCue = index;
    return chaser;
}

void ControlMap::goList(const Binding &b, bool stop)
{
    int step = -1, subCue = -1;
    Chaser *chaser = targetChaser(b, &step, &subCue);
    if (chaser == nullptr)
        return;

    ShowControl::Request r;
    r.chaser = chaser->id();
    const bool back = b.target.type == CueListBackTarget;
    r.command = stop ? ShowControl::Stop : back ? ShowControl::Back : ShowControl::Go;
    if (!stop && b.target.type == CueTarget)
    {
        if (step < 0)
            return;
        if (subCue >= 0 && subCue >= chaser->subCueCount(step))
            return;
        r.cue = ShowControl::cueName(chaser, step, subCue);
    }
    // The console's one run path (docs/design/RUN_COMMANDS.md).
    if (m_doc->showControl()->run(r, ShowControl::RouteControlMap))
    {
        if (back && !stop)
            emit fired(targetName(b.target));       // "Back: Act 1"
        else
            emit fired(tr("%1 %2").arg(stop ? tr("Stop") : tr("GO"),
                                       targetName(back ? cueList(b.target.id) : b.target)));
    }
}

/*****************************************************************************
 * Places
 *****************************************************************************/

QString ControlMap::placeScopeToString(PlaceScope scope)
{
    switch (scope)
    {
        case Everywhere:      return QStringLiteral("Global");
        case ProductionPlace: return QStringLiteral("Production");
        case ListPlace:       return QStringLiteral("List");
        case CuePlace:        return QStringLiteral("Cue");
        case NewerPlace:      break;      // saved as it was loaded
    }
    return QString();
}

ControlMap::PlaceScope ControlMap::stringToPlaceScope(const QString &str)
{
    if (str == QLatin1String("Global"))
        return Everywhere;
    if (str == QLatin1String("Production"))
        return ProductionPlace;
    if (str == QLatin1String("List"))
        return ListPlace;
    if (str == QLatin1String("Cue"))
        return CuePlace;
    /* A place a later build has (phase 4: a layer, an effect): never read
       as everywhere -- a trigger meant for one cue would fire all show. */
    return NewerPlace;
}

bool ControlMap::placeExists(const Place &place) const
{
    ShowTreeController *st = m_doc->showTree();
    switch (place.scope)
    {
        case Everywhere:
            return true;
        case ProductionPlace:
            return st != nullptr && st->production(place.id) != nullptr;
        case ListPlace:
            return st != nullptr && st->cueList(place.id) != nullptr;
        case CuePlace:
            return st != nullptr && st->cue(place.id) != nullptr;
        case NewerPlace:
            return false;
    }
    return false;
}

bool ControlMap::isPlaceLive(const Place &place) const
{
    if (place.scope == Everywhere)
        return true;
    if (place.scope == NewerPlace)
        return false;       // disarmed: this build cannot tell when it is live
    ShowTreeController *st = m_doc->showTree();
    if (st == nullptr)
        return false;
    switch (place.scope)
    {
        case ProductionPlace:
            if (st->production(place.id) == nullptr)
                return false;
            break;
        case ListPlace:
            if (st->cueList(place.id) == nullptr)
                return false;
            break;
        case CuePlace:
            if (st->cue(place.id) == nullptr)
                return false;
            break;
        case Everywhere:
        case NewerPlace:
            break;
    }
    // The hits' own rule: the one answer for "is this place live".
    if (st->isHitPlaceLive(ShowTree::HitScope(int(place.scope)), place.id))
        return true;
    if (m_doc->mode() == Doc::Operate)
        return false;
    // Design: also while it is what you are looking at (CONTROL_SCOPES §6.4).
    switch (place.scope)
    {
        case CuePlace:
            return m_selected.contains(cue(place.id));
        case ListPlace:
            return place.id == runList() || m_selected.contains(cueList(place.id));
        default:
            break;
    }
    return false;
}

bool ControlMap::holdsControl(const Binding &b) const
{
    if (b.target.type == EmergencyStopTarget || !isPlaceLive(b.place))
        return false;
    if (!deskBlockReason(b).isEmpty())
        return false;
    // A hit only while it may fire itself: learned at a place wider than
    // its own, it does not take the control from a wider trigger that would.
    return b.target.type != HitTarget || surfaceMayFire(b.target);
}

QString ControlMap::deskBlockReason(const Binding &b) const
{
    const DeskRecord *desk = m_doc->deskRecord();
    if (desk == nullptr || isKey(b))
        return QString();
    // Q309: on a control that is static here, the desk wins; kept, never armed.
    DeskRecord::Role role = DeskRecord::Stop;
    if (desk->isDeskControl(b.universe, b.channel, &role))
    {
        if (b.target.type == EmergencyStopTarget && role == DeskRecord::Stop)
            return tr("the same control as this computer's STOP, which stops the show itself");
        return tr("blocked on this computer: %1 is this computer's %2")
            .arg(controlName(b.universe, b.channel), DeskRecord::roleName(role));
    }
    /* Q314: the show's own GO of the Run tab's list is a fallback, armed
       only while this computer has no GO of its own. */
    if (b.target.type == CueListTarget && b.mode != StopOnly && b.place.scope == Everywhere
        && desk->isLive(DeskRecord::Go) && b.target.id == runList())
        return tr("not used on this computer: its desk has a GO (the show's GO is the fallback)");
    return QString();
}

QList<QPair<ControlMap::Binding, QString>> ControlMap::deskBlocked() const
{
    QList<QPair<Binding, QString>> out;
    for (const Binding &b : m_bindings)
    {
        const QString why = deskBlockReason(b);
        if (!why.isEmpty())
            out << qMakePair(b, why);
    }
    return out;
}

quint32 ControlMap::listOfCueId(quint32 cueId) const
{
    ShowTreeController *st = m_doc->showTree();
    if (st == nullptr)
        return 0xFFFFFFFF;
    // A sub-cue (a cue made one after it was bound): its cue's list.
    quint32 parent = 0xFFFFFFFF;
    const ShowTree::CueList *l = st->listOfCue(st->isSubCue(cueId, &parent) ? parent : cueId);
    return l == nullptr ? quint32(0xFFFFFFFF) : l->id;
}

bool ControlMap::isRunListPlace(const Place &place) const
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 run = runList();
    if (st == nullptr || run == 0xFFFFFFFF)
        return false;
    switch (place.scope)
    {
        case Everywhere:
            return true;
        case ProductionPlace:
        {
            quint32 prod = 0xFFFFFFFF;
            return st->cueList(run, &prod) != nullptr && prod == place.id;
        }
        case ListPlace:
            return place.id == run;
        case CuePlace:
            return listOfCueId(place.id) == run;
        case NewerPlace:
            return false;
    }
    return false;
}

bool ControlMap::isPlaceWithin(const Place &inner, const Place &outer) const
{
    if (outer.scope == Everywhere)
        return true;
    if (int(inner.scope) < int(outer.scope))
        return false;
    if (inner == outer)
        return true;
    ShowTreeController *st = m_doc->showTree();
    if (st == nullptr)
        return false;
    quint32 list = 0xFFFFFFFF;
    switch (inner.scope)
    {
        case CuePlace:
        {
            quint32 parent = 0xFFFFFFFF;
            if (outer.scope == CuePlace)
                return st->isSubCue(inner.id, &parent) && parent == outer.id;
            list = listOfCueId(inner.id);
            break;
        }
        case ListPlace:
            list = inner.id;
            break;
        default:
            return false;
    }
    if (list == 0xFFFFFFFF)
        return false;
    if (outer.scope == ListPlace)
        return list == outer.id;
    quint32 prod = 0xFFFFFFFF;
    return outer.scope == ProductionPlace && st->cueList(list, &prod) != nullptr && prod == outer.id;
}

QList<ControlMap::Binding> ControlMap::armedOn(quint32 universe, quint32 channel) const
{
    QList<Binding> live;
    int best = -1;
    for (int i : m_byControl.value(key(universe, channel)))
    {
        const Binding &b = m_bindings.at(i);
        if (!holdsControl(b))
            continue;
        const int rank = int(b.place.scope);
        if (rank > best)
        {
            live.clear();
            best = rank;
        }
        if (rank == best)
            live << b;
    }
    if (live.count() < 2)
        return live;
    /* Only those at one place step through (Q303). Two places of one scope
       live together (two lists' cues, two running lists): the run list's
       has the control, else the first in the table -- never each press
       the next, which nobody chose. */
    Place chosen = live.first().place;
    for (const Binding &b : std::as_const(live))
    {
        if (isRunListPlace(b.place))
        {
            chosen = b.place;
            break;
        }
    }
    QList<Binding> out;
    for (const Binding &b : std::as_const(live))
        if (b.place == chosen)
            out << b;
    return out;
}

const ControlMap::Binding *ControlMap::refusedHitOn(quint64 k) const
{
    for (int i : m_byControl.value(k))
    {
        const Binding &b = m_bindings.at(i);
        if (b.target.type == HitTarget && isPlaceLive(b.place) && !surfaceMayFire(b.target))
            return &b;
    }
    return nullptr;
}

bool ControlMap::isArmedOnControl(const Binding &b) const
{
    for (const Binding &a : armedOn(b.universe, b.channel))
        if (a.target == b.target)
            return true;
    return false;
}

bool ControlMap::pick(quint64 k, Binding *out, bool advance) const
{
    const QList<Binding> tier = armedOn(quint32(k >> 32), quint32(k & 0xFFFFFFFF));
    if (tier.isEmpty())
        return false;
    if (tier.count() == 1)
    {
        *out = tier.first();
        return true;
    }
    // Several at one place: each press the next (Q303, as a VC button's hits).
    QList<Target> targets;
    for (const Binding &b : tier)
        targets << b.target;
    auto it = m_step.find(k);
    if (it == m_step.end() || it->first != targets)
        it = m_step.insert(k, qMakePair(targets, 0));
    *out = tier.at(it->second % tier.count());
    if (advance)
        it->second = (it->second + 1) % tier.count();
    return true;
}

const ControlMap::Binding *ControlMap::runningOn(quint64 k) const
{
    const Binding *best = nullptr;
    for (int i : m_byControl.value(k))
    {
        const Binding &b = m_bindings.at(i);
        // The stop, and a trigger at a newer version's place: never fire.
        if (b.target.type == EmergencyStopTarget || b.place.scope == NewerPlace)
            continue;
        const quint32 fid = feedbackFunction(b);
        const Function *f = fid == 0xFFFFFFFF ? nullptr : m_doc->function(fid);
        if (f == nullptr || !f->isRunning())
            continue;
        if (best == nullptr || int(b.place.scope) > int(best->place.scope))
            best = &b;
    }
    return best;
}

QString ControlMap::placeName(const Place &place) const
{
    ShowTreeController *st = m_doc->showTree();
    switch (place.scope)
    {
        case Everywhere:
            return tr("everywhere");
        case ProductionPlace:
        {
            const ShowTree::Production *p = st == nullptr ? nullptr : st->production(place.id);
            return p == nullptr ? tr("in a production that is gone") : tr("in production %1").arg(p->name);
        }
        case ListPlace:
        {
            const ShowTree::CueList *l = st == nullptr ? nullptr : st->cueList(place.id);
            return l == nullptr ? tr("in a cue list that is gone") : tr("in cue list %1").arg(l->name);
        }
        case CuePlace:
        {
            const ShowTree::CueList *l = st == nullptr ? nullptr : st->listOfCue(place.id);
            if (l == nullptr)
                return tr("in a cue that is gone");
            return tr("in cue %1").arg(st->cueLabel(*l, l->indexOf(place.id)));
        }
        case NewerPlace:
            return tr("in a place from a newer version");
    }
    return QString();
}

QString ControlMap::placesText(const QList<Binding> &bindings) const
{
    // Grouped by place, widest first; a place's own in the given order.
    QList<Binding> sorted;
    for (const Binding &b : bindings)
        if (b.target.type != EmergencyStopTarget)
            sorted << b;
    std::stable_sort(sorted.begin(), sorted.end(), [](const Binding &a, const Binding &b) {
        return int(a.place.scope) < int(b.place.scope);
    });
    QStringList groups;
    for (int i = 0; i < sorted.count();)
    {
        QStringList names;
        const Place at = sorted.at(i).place;
        int j = i;
        for (; j < sorted.count() && sorted.at(j).place == at; ++j)
            names << targetName(sorted.at(j).target);
        // Same scope, another place (two cues): its own group.
        QString group = names.count() > 1
                ? tr("%1 %2 (each press the next)").arg(names.join(QStringLiteral(" / ")), placeName(at))
                : tr("%1 %2").arg(names.first(), placeName(at));
        groups << group;
        i = j;
    }
    if (groups.isEmpty())
        return QString();
    if (groups.count() == 1)
        return groups.first();
    // "A everywhere, except B in cue 2.3, C in cue 3.1"
    return tr("%1, except %2").arg(groups.first(), groups.mid(1).join(QStringLiteral(", ")));
}

ControlMap::Place ControlMap::placeOfHit(quint32 hitId) const
{
    ShowTreeController *st = m_doc->showTree();
    if (st == nullptr || st->cue(hitId) == nullptr)
        return Place();
    quint32 of = 0xFFFFFFFF;
    switch (st->hitScope(hitId, &of))
    {
        case ShowTree::HitScope::Global:
            return Place();
        case ShowTree::HitScope::Cue:
            return inCue(of);
        case ShowTree::HitScope::List:
            return inList(of);
        case ShowTree::HitScope::Production:
        {
            quint32 prod = 0xFFFFFFFF;
            if (const ShowTree::CueList *l = st->listOfCue(hitId))
                st->cueList(l->id, &prod);
            return prod == 0xFFFFFFFF ? Place() : inProduction(prod);
        }
    }
    return Place();
}

void ControlMap::slotTreeChanged()
{
    // A show loading edits the tree thousands of times, before its
    // triggers are in: Doc::loaded checks them once it is all there.
    if (m_doc->loadStatus() == Doc::Loading)
        return;
    fixPlaces();
}

void ControlMap::rememberPlaces()
{
    ShowTreeController *st = m_doc->showTree();
    if (st == nullptr)
        return;
    for (const Binding &b : std::as_const(m_bindings))
    {
        if (b.place.scope == Everywhere || b.place.scope == NewerPlace || !placeExists(b.place))
            continue;
        PlaceHome home;
        home.name = placeName(b.place);
        quint32 list = 0xFFFFFFFF;
        switch (b.place.scope)
        {
            case ProductionPlace:
                home.production = b.place.id;
                break;
            case ListPlace:
                list = b.place.id;
                break;
            case CuePlace:
                list = listOfCueId(b.place.id);
                break;
            case Everywhere:
            case NewerPlace:
                break;
        }
        if (list != 0xFFFFFFFF)
            st->cueList(list, &home.production);
        m_placeHomes.insert(placeKey(b.place), home);
    }
}

void ControlMap::fixPlaces()
{
    if (m_bindings.isEmpty())
        return;
    ShowTreeController *st = m_doc->showTree();
    bool moved = false;
    QList<QPair<quint32, quint32>> controls;
    auto usable = [this](const Binding &b, const Place &place) { return isPlaceUsable(b, place); };
    for (int i = 0; i < m_bindings.count(); ++i)
    {
        Binding &b = m_bindings[i];
        // Widened, and its place is back (an Undo of the delete): back there.
        auto w = m_widened.find(targetKey(b.target));
        if (w != m_widened.end() && b.place == w->to && usable(b, w->from) && !isPlaceTaken(b, w->from))
        {
            b.place = w->from;
            m_widened.erase(w);
            moved = true;
            controls << qMakePair(b.universe, b.channel);
            continue;
        }
        if (b.place.scope == Everywhere || b.place.scope == NewerPlace || usable(b, b.place))
            continue;
        /* Its cue, list or production is gone: as a scoped hit's (Q154,
           ShowTreeController::normalizeLooks), it moves up to its
           production, else everywhere -- never left at a place that can
           never be live, where it would never fire again. A place another
           trigger on the control has is passed over while a wider one is
           free (they would step through, which nobody chose). */
        const PlaceHome home = m_placeHomes.value(placeKey(b.place));
        Place to;
        if (b.target.type == HitTarget && targetExists(b.target))
            to = placeOfHit(b.target.id);       // the hit's own place now
        else if (b.place.scope != ProductionPlace && home.production != 0xFFFFFFFF && st != nullptr
                 && st->production(home.production) != nullptr)
            to = inProduction(home.production);
        // (A hit stays at its own place: wider, it would hold the control where it cannot fire.)
        if (b.target.type != HitTarget && to.scope != Everywhere && isPlaceTaken(b, to)
            && !isPlaceTaken(b, Place()))
            to = Place();
        /* Every place it could go has another trigger on its control: it
           is held where it is (a place that is gone is never live, so it
           never fires; it can still let go) rather than step through with
           that one, which nobody chose and Learn and setPlace() refuse.
           placeNote() says so; the next fix-up widens it once one is free. */
        if (isPlaceTaken(b, to))
            continue;
        Widened was;
        const auto had = m_widened.constFind(targetKey(b.target));
        if (had != m_widened.constEnd())
        {
            was.from = had->from;          // widened twice: where it was first
            was.fromName = had->fromName;
        }
        else
        {
            was.from = b.place;
            was.fromName = home.name;
        }
        was.to = to;
        m_widened.insert(targetKey(b.target), was);
        b.place = to;
        moved = true;
        controls << qMakePair(b.universe, b.channel);
    }
    if (!moved)
    {
        rememberPlaces();
        return;
    }
    m_step.clear();
    changed(true);
    for (const QPair<quint32, quint32> &c : std::as_const(controls))
        sendControlLamp(c.first, c.second, true);
}

bool ControlMap::isPlaceUsable(const Binding &b, const Place &place) const
{
    /* It is in the show, and a hit's is not apart from where the hit lives
       now (a hit fires only inside its own place: moved to another list, a
       trigger left at a cue of the old one would be armed where the hit
       can never fire). */
    if (!placeExists(place))
        return false;
    if (b.target.type != HitTarget || !targetExists(b.target))
        return true;
    const Place own = placeOfHit(b.target.id);
    return isPlaceWithin(place, own) || isPlaceWithin(own, place);
}

QString ControlMap::placeNote(const Target &target) const
{
    const Binding *b = binding(target);
    if (b == nullptr)
        return QString();
    if (b->place.scope == NewerPlace)
        return tr("%1: its place (%2) is from a newer version of qlcconsole, so it never fires here. "
                  "It is kept and saved as it was; choose a place in Works to use it here.")
                .arg(targetName(target), b->place.newerScope);
    // Held by fixPlaces(): its place went, and every place it could go has
    // another trigger on its control.
    if (b->place.scope != Everywhere && !isPlaceUsable(*b, b->place))
    {
        QString from = m_placeHomes.value(placeKey(b->place)).name;
        if (from.isEmpty())
            from = placeName(b->place);
        if (placeExists(b->place))
            return tr("%1 worked only %2, where the hit is not any more, and the hit's place already has "
                      "another trigger on this control: it never fires now. Choose a place in Works, or "
                      "remove it.").arg(targetName(target), from);
        return tr("%1 worked only %2, which was deleted, and every place it could move to already has "
                  "another trigger on this control: it never fires now. Choose a place in Works, or "
                  "remove it.").arg(targetName(target), from);
    }
    const auto w = m_widened.constFind(targetKey(target));
    if (w == m_widened.constEnd() || w->to != b->place)
        return QString();
    const QString from = w->fromName.isEmpty()
            ? (w->from.scope == CuePlace ? tr("in a cue") : w->from.scope == ListPlace ? tr("in a cue list")
                                                                                      : tr("in a production"))
            : w->fromName;
    if (placeExists(w->from))
        return tr("%1 worked only %2, where the hit is not any more: it works %3 now, where the hit is.")
                .arg(targetName(target), from, placeName(b->place));
    return tr("%1 worked only %2, which was deleted: it works %3 now.")
            .arg(targetName(target), from, placeName(b->place));
}

QStringList ControlMap::placeNotes() const
{
    QStringList out;
    for (const Binding &b : m_bindings)
    {
        const QString note = placeNote(b.target);
        if (!note.isEmpty())
            out << note;
    }
    return out;
}

QList<ControlMap::Place> ControlMap::placeChoices(const Place &where) const
{
    return placeChoices(where, Target());
}

QList<ControlMap::Place> ControlMap::placeChoices(const Place &where, const Target &target) const
{
    ShowTreeController *st = m_doc->showTree();
    QList<Place> out;
    auto add = [&out](const Place &p) {
        if (!out.contains(p))
            out << p;
    };
    // A place and the ones around it: a cue, its list, its production.
    auto around = [&](const Place &p) {
        if (st == nullptr || p.scope == Everywhere || p.scope == NewerPlace)
            return;
        quint32 list = 0xFFFFFFFF, prod = 0xFFFFFFFF;
        if (p.scope == CuePlace)
        {
            quint32 parent = 0xFFFFFFFF;
            const ShowTree::CueList *l = st->listOfCue(st->isSubCue(p.id, &parent) ? parent : p.id);
            if (l == nullptr || l->looks)
                return;
            add(p);
            list = l->id;
        }
        else if (p.scope == ListPlace)
        {
            list = p.id;
        }
        else
        {
            prod = p.id;
        }
        if (list != 0xFFFFFFFF)
        {
            if (st->cueList(list, &prod) == nullptr)
                return;
            add(inList(list));
        }
        if (prod != 0xFFFFFFFF && st->production(prod) != nullptr)
            add(inProduction(prod));
    };
    around(where);
    add(Place());
    // Where you are now: the Run tab's live cue, else what the Show Tree has selected.
    Place now;
    const quint32 run = runList();
    const quint32 live = st == nullptr || run == 0xFFFFFFFF ? 0xFFFFFFFF : st->runningCue(run);
    if (live != 0xFFFFFFFF)
        now = inCue(live);
    for (const Target &t : m_selected)
    {
        if (now.scope != Everywhere)
            break;
        if (t.type == CueTarget)
            now = inCue(t.id);
        else if (t.type == CueListTarget)
            now = inList(t.id);
    }
    if (now.scope == Everywhere && run != 0xFFFFFFFF)
        now = inList(run);
    // Narrowest first, then everywhere last.
    out.removeAll(Place());
    around(now);
    add(Place());
    if (target.type == HitTarget)
    {
        /* A hit fires only where its own place is live: a wider place would
           arm it where it cannot fire, dark, holding the control from a
           wider trigger that would. Its place and the ones inside it. */
        const Place own = placeOfHit(target.id);
        QList<Place> within;
        for (const Place &p : std::as_const(out))
            if (isPlaceWithin(p, own))
                within << p;
        if (!within.contains(own))
            within << own;
        return within;
    }
    return out;
}

/*****************************************************************************
 * Computer keys
 *****************************************************************************/

bool ControlMap::handleKey(quint32 channel, bool pressed)
{
    if (channel == 0)
        return false;
    const quint64 k = key(KeyboardUniverse, channel);
    if (!pressed)
    {
        // A release lets go of what its press fired, whatever the mode or
        // the arming is now: nothing stays held.
        m_blockedHeld.remove(k);
        if (!m_down.contains(k))
            return false;
        handleInput(KeyboardUniverse, channel, 0);
        return true;
    }
    if (m_down.contains(k))
        return true;    // held: one press
    if (!m_byControl.contains(k))
        return false;
    if (const Binding *stop = stopOn(k))
    {
        Q_UNUSED(stop)
        // A show file put the stop on a key: it does nothing, and says so.
        say(k, tr("The emergency stop is bound to a computer key, which cannot be the stop: a key works "
                  "only in Operate. Learn it on a MIDI or OSC button."));
        return false;
    }
    // The narrowest armed place has the key (Q296); none: it goes on to
    // whatever has the keyboard, and the footer says where it works.
    const Binding *b = bindingOn(KeyboardUniverse, channel);
    const Binding *running = b == nullptr ? runningOn(k) : nullptr;
    if (b == nullptr && running == nullptr)
    {
        sayNotLive(k);
        return false;
    }
    /* The key rule holds wherever its place is: in Design a scene or effect
       key, or one whose target is not armed, does nothing even to let go of
       what runs (in Operate a running one may still be let go, as on a
       surface). The key goes on to whatever has the keyboard; the footer
       says why it fired nothing. */
    const Binding *acting = b != nullptr ? b : running;
    if (!keyMayFire(acting->target) && (b != nullptr || m_doc->mode() != Doc::Operate))
    {
        say(k, tr("Not fired: %1. %2").arg(targetName(acting->target), refusalFor(acting->target)));
        return false;
    }
    handleInput(KeyboardUniverse, channel, UCHAR_MAX);
    return true;
}

void ControlMap::releaseKeys()
{
    QList<quint32> held;
    for (quint64 k : std::as_const(m_down))
        if (quint32(k >> 32) == KeyboardUniverse)
            held << quint32(k & 0xFFFFFFFF);
    for (quint32 ch : held)
        handleInput(KeyboardUniverse, ch, 0);
}

bool ControlMap::keyMayFire(const Target &target) const
{
    // One rule with a surface's (Q277); a key is also typing, so a scene or an
    // effect stays Operate-only on it.
    if (target.type == FunctionTarget)
        return m_doc->mode() == Doc::Operate;
    return surfaceMayFire(target);
}

QString ControlMap::refusalFor(const Target &target) const
{
    if (target.type == FunctionTarget)
        return tr("A key fires a scene or an effect in Operate.");
    if (target.type == HitTarget && m_doc->mode() == Doc::Operate)
        return tr("Its place is not live.");
    if (m_doc->mode() == Doc::Operate)
        return tr("Not now.");
    return tr("In Design a control fires only what is armed: the Run tab's list, or selected in the Show Tree.");
}

quint32 ControlMap::runList() const
{
    ShowTreeController *st = m_doc->showTree();
    if (st == nullptr)
        return 0xFFFFFFFF;
    if (m_runTabList != 0xFFFFFFFF && st->cueList(m_runTabList) != nullptr)
        return m_runTabList;
    // The Run tab's own choice: the current production's main cue list.
    ShowControl *sc = m_doc->showControl();
    const Chaser *ch = sc == nullptr ? nullptr : sc->cueList(QString());
    quint32 lid = 0xFFFFFFFF;
    if (ch != nullptr)
        st->productionOfChaser(ch->id(), &lid);
    return lid;
}

void ControlMap::setRunTabList(quint32 listId)
{
    if (listId == m_runTabList)
        return;
    m_runTabList = listId;
    // The show's: the pad and the Run tab play it next time too.
    m_doc->setModified();
    emit runTabListChanged(listId);
    emit armingChanged();
}

void ControlMap::setSelectedTargets(const QList<Target> &targets)
{
    if (targets == m_selected)
        return;
    m_selected = targets;
    emit armingChanged();
}

bool ControlMap::isArmed(const Target &target) const
{
    ShowTreeController *st = m_doc->showTree();
    if (st == nullptr || target.type == FunctionTarget || !target.isValid())
        return false;
    if (m_selected.contains(target))
        return true;
    const quint32 run = runList();
    auto listArmed = [&](quint32 listId) {
        return listId != 0xFFFFFFFF && (listId == run || m_selected.contains(cueList(listId)));
    };
    auto listOf = [this](quint32 cueId) { return listOfCueId(cueId); };
    switch (target.type)
    {
        case CueListTarget:
            return target.id == run;
        case CueListBackTarget:
            // Armed as the list's GO is: the run list, or the list selected.
            return target.id == run || m_selected.contains(cueList(target.id));
        case CueTarget:
            return listArmed(listOf(target.id));
        case HitTarget:
        {
            if (st->isHitArmed(target.id))
                return true;    // its place is live, as for a VC button
            quint32 place = 0xFFFFFFFF;
            switch (st->hitScope(target.id, &place))
            {
                case ShowTree::HitScope::Global:
                    return run != 0xFFFFFFFF;
                case ShowTree::HitScope::Production:
                {
                    quint32 hitProd = 0xFFFFFFFF, runProd = 0xFFFFFFFF;
                    st->cueList(listOf(target.id), &hitProd);
                    if (run == 0xFFFFFFFF || st->cueList(run, &runProd) == nullptr)
                        return false;
                    return hitProd != 0xFFFFFFFF && hitProd == runProd;
                }
                case ShowTree::HitScope::List:
                    return listArmed(place);
                case ShowTree::HitScope::Cue:
                    return m_selected.contains(cue(place)) || listArmed(listOf(place));
            }
            return false;
        }
        case FunctionTarget:
        case EmergencyStopTarget:
            break;
    }
    return false;
}

bool ControlMap::surfaceMayFire(const Target &target) const
{
    ShowTreeController *st = m_doc->showTree();
    const bool operate = m_doc->mode() == Doc::Operate;
    switch (target.type)
    {
        case FunctionTarget:
        case EmergencyStopTarget:
            return true;
        case CueListTarget:
        case CueTarget:
        case CueListBackTarget:
            return operate || isArmed(target);
        case HitTarget:
            // Its place is live: the narrowest-place rule a VC button and
            // the phone page follow. In Design, armed as a key is.
            return operate ? st != nullptr && st->isHitArmed(target.id) : isArmed(target);
    }
    return false;
}

/*****************************************************************************
 * Feedback
 *****************************************************************************/

quint32 ControlMap::feedbackFunction(const Binding &b) const
{
    ShowTreeController *st = m_doc->showTree();
    switch (b.target.type)
    {
        case FunctionTarget:
            return b.target.id;
        case CueListTarget:
        case CueListBackTarget:
        {
            const ShowTree::CueList *l = st->cueList(b.target.id);
            return l == nullptr ? 0xFFFFFFFF : l->chaser;
        }
        case CueTarget:
        case HitTarget:
        {
            const ShowTree::Cue *c = st->cue(b.target.id);
            return c == nullptr ? 0xFFFFFFFF : c->collection;
        }
        case EmergencyStopTarget:
            return 0xFFFFFFFF;
    }
    return 0xFFFFFFFF;
}

ControlMap::Lamp ControlMap::lamp(const Binding &b) const
{
    OutputRuleKeeper *rules = m_doc->outputRuleKeeper();
    const bool stopped = rules != nullptr && rules->emergencyStopped();
    /* The emergency stop has a light of its own: lit while the show is
       stopped, and ready at every other moment -- in either mode, whatever
       else is bound, however dark the stage is. It is never dark because
       something is not armed: it always works, so it always says so. */
    if (b.target.type == EmergencyStopTarget)
        return stopped ? LampStopped : LampStandby;
    if (!targetExists(b.target))
        return LampOff;         // the thing it fired is gone
    const quint32 fid = feedbackFunction(b);
    Function *f = fid == 0xFFFFFFFF ? nullptr : m_doc->function(fid);
    if (f != nullptr && f->isRunning())
        return LampRunning;
    if (b.mode == StopOnly)
        return LampOff;         // a press of it does nothing while nothing runs
    if (b.target.type == CueListBackTarget)
        return LampOff;         // Back on a list that is not running: no cue before
    if (stopped)
        return LampOff;         // after ALL OFF, dark until the next GO
    /* The desk's GO and Back are always armed: they play the Run tab's
       list in either mode (DeskRecord::goPressed()). */
    if (!b.desk)
    {
        // Its place is not live, or a narrower one has the control (Q296): a
        // press would not fire it.
        if (!isArmedOnControl(b))
            return LampOff;
        // The press rule itself (board Q269): a press that would be refused is
        // dark, so the desk says "not now" before anyone presses it.
        if (!surfaceMayFire(b.target))
            return LampOff;
    }
    if (b.target.type == CueListTarget)
    {
        /* A plain GO on the list: only while there is something left to
           play (the end of a single-shot list has nothing). A cue binding
           jumps to its cue, which works at any time, so it is not asked. */
        ShowControl *sc = m_doc->showControl();
        Chaser *chaser = targetChaser(b);
        if (sc == nullptr || chaser == nullptr || sc->standbyStep(chaser) < 0)
            return LampOff;
    }
    return LampStandby;
}

QString ControlMap::lampName(Lamp lamp)
{
    switch (lamp)
    {
        case LampOff:       return tr("Dark");
        case LampStandby:   return tr("Ready");
        case LampRunning:   return tr("Running");
        case LampHeld:      return tr("GO held");
        case LampStopped:   return tr("Stopped");
    }
    return QString();
}

uchar ControlMap::lampValue(Lamp lamp, bool dim)
{
    switch (lamp)
    {
        case LampOff:
            return 0;
        case LampStandby:
            /* Dim: 30 is MIDI velocity 15, the level the PMJ's own idle
               template lights its board at. A pad whose LEDs take a colour
               in the velocity shows nothing at a dim level, so with the
               dim light off a ready control stays dark (as before CS3). */
            return dim ? 30 : 0;
        case LampRunning:
        case LampHeld:
        case LampStopped:
            return UCHAR_MAX;
    }
    return 0;
}

bool ControlMap::idlesDim(const QString &profileName)
{
    return profileName.compare(QStringLiteral("PMJ Black 1"), Qt::CaseInsensitive) == 0;
}

bool ControlMap::profileLampRange(quint32 universe, quint32 channel, uchar *dim, uchar *lit) const
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    InputPatch *ip = iom == nullptr || universe == KeyboardUniverse ? nullptr : iom->inputPatch(universe);
    QLCInputProfile *profile = ip == nullptr ? nullptr : ip->profile();
    if (profile == nullptr || !idlesDim(profile->name()))
        return false;
    QLCInputChannel *ich = profile->channel(channel);
    if (ich == nullptr || ich->lowerValue() == 0)
        return false;
    // A MIDI profile's levels are velocities (0..127): the plugin halves
    // what it is sent (DMX2MIDI), so they go out doubled, 127 as 255.
    auto wire = [profile](uchar v) -> uchar {
        if (profile->type() != QLCInputProfile::MIDI)
            return v;
        return v >= 127 ? UCHAR_MAX : uchar(v * 2);
    };
    const uchar lo = wire(ich->lowerValue());
    const uchar hi = wire(ich->upperValue());
    if (hi <= lo)
        return false;   // no lit level above the dim one: not a range to light by
    if (dim != nullptr)
        *dim = lo;
    if (lit != nullptr)
        *lit = hi;
    return true;
}

uchar ControlMap::lampValueOn(const Binding &owner, Lamp lamp) const
{
    uchar dim = 0, lit = 0;
    if (!profileLampRange(owner.universe, owner.channel, &dim, &lit))
    {
        /* The emergency stop is never dimmed away (review finding 3): the
           switch is there for a pad whose lights are colours, and a panic
           button that cannot be found is worse than an odd colour. Its ready
           light is sent whatever the switch says. */
        const bool glow = m_dimWhenReady || owner.target.type == EmergencyStopTarget;
        return lampValue(lamp, glow);
    }
    /* The profile's own levels (the PMJ Black 1: 15 dim, 127 lit). Its
       board idles at the dim level, so "ready" at that level could not be
       told from a key that does nothing: a control lit at the profile's
       level is one a press would fire, now (a hit only while its place is
       live), or one whose thing runs; dim is everything else. The stop
       always works, so it is always lit (findable on a dark stage); after
       an ALL OFF it blinks lit / dim (stopBlinkStep()) until the next GO. */
    if (owner.target.type == EmergencyStopTarget)
        return lamp == LampStopped && !m_stopBlinkLit ? dim : lit;
    // A board that blinks by value blinks a running hit (board Q327).
    if (lamp == LampRunning && (owner.target.type == HitTarget || owner.target.type == FunctionTarget))
    {
        auto it = m_runningLamp.constFind(owner.universe);
        if (it != m_runningLamp.constEnd())
            return it.value();
    }
    return lamp == LampOff ? dim : lit;
}

void ControlMap::setRunningLampValue(quint32 universe, int value)
{
    const bool had = m_runningLamp.contains(universe);
    if (value < 0)
        m_runningLamp.remove(universe);
    else
        m_runningLamp.insert(universe, uchar(qMin(255, value)));
    if (had || value >= 0)
        sendLamps(false);
}

bool ControlMap::isBlinking(const Binding &b) const
{
    if (m_blinkLeft <= 0)
        return false;
    for (const Binding &e : m_blinking)
        if (e.universe == b.universe && e.channel == b.channel)
            return true;
    return false;
}

void ControlMap::setDimWhenReady(bool on)
{
    if (m_dimWhenReady == on)
        return;
    m_dimWhenReady = on;
    sendLamps(true);
    m_doc->setModified();
    emit lampsChanged();
}

void ControlMap::sendLampValue(const Binding &b, uchar value, bool force)
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    if (iom == nullptr || isKey(b))
        return;     // a computer key has no light
    const quint64 k = key(b.universe, b.channel);
    if (!force && m_lampSent.value(k, -1) == int(value))
        return;     // nothing changed here: a mode switch sends nothing
    m_lampSent.insert(k, int(value));
    // The profile's own parameters for the control (a MIDI note's channel
    // and message), as a VC button's feedback takes them.
    QVariant params;
    InputPatch *ip = iom->inputPatch(b.universe);
    if (ip != nullptr && ip->profile() != nullptr)
    {
        QLCInputChannel *ich = ip->profile()->channel(b.channel);
        if (ich != nullptr)
            params = ip->profile()->channelExtraParams(ich);
    }
    iom->sendFeedBack(b.universe, b.channel, value, params);
}

void ControlMap::sendLamp(const Binding &b, bool force)
{
    // One light per control: what the control does now (Q296).
    sendControlLamp(b.universe, b.channel, force);
}

void ControlMap::sendControlLamp(quint32 universe, quint32 channel, bool force)
{
    // A surface page's key in Design is the page's light (SurfacePages::sendLamps()).
    if (universe != KeyboardUniverse && m_doc->inputOutputMap() != nullptr
        && m_doc->inputOutputMap()->isPageControl(universe, channel))
        return;
    Binding at;
    at.universe = universe;
    at.channel = channel;
    const quint64 k = key(universe, channel);
    if (!m_byControl.contains(k) && deskLampOn(k) == nullptr)
    {
        darkenLamp(at);     // nothing is bound there any more
        return;
    }
    const Binding *owner = lampOwner(k);
    if (owner == nullptr)
    {
        // Nothing here would fire now (a hit whose place is not live): dark,
        // or the dim level of a profile that has one (profileLampRange()).
        uchar dim = 0;
        sendLampValue(at, profileLampRange(universe, channel, &dim, nullptr) ? dim : 0, force);
        return;
    }
    const Lamp now = lamp(*owner);
    if (owner->target.type == EmergencyStopTarget && now == LampStopped
        && profileLampRange(universe, channel, nullptr, nullptr))
    {
        // The stopped state is a blink there: lit is its everyday light.
        if (m_stopBlink == nullptr)
        {
            m_stopBlink = new QTimer(this);
            connect(m_stopBlink, &QTimer::timeout, this, &ControlMap::stopBlinkStep);
        }
        if (!m_stopBlink->isActive())
        {
            m_stopBlinkLit = true;
            m_stopBlink->start(m_blinkMs);
        }
    }
    sendLampValue(at, lampValueOn(*owner, now), force);
}

void ControlMap::sendLamps(bool force)
{
    refreshDeskTargets();
    QSet<quint64> done;
    // The desk's first: its controls are its own (Q309), whatever is bound there.
    const QList<Binding> all = m_deskLamps + m_bindings;
    for (const Binding &b : all)
    {
        const quint64 k = key(b.universe, b.channel);
        if (done.contains(k))
            continue;       // one light per control
        done.insert(k);
        if (isBlinking(b))
            continue;       // the blink has it until it is done
        sendControlLamp(b.universe, b.channel, force);
    }
}

void ControlMap::refreshDeskTargets()
{
    // The desk's GO and Back play the Run tab's list, whichever it is now.
    const quint32 list = m_deskLamps.isEmpty() ? 0xFFFFFFFF : runList();
    for (Binding &b : m_deskLamps)
        if (b.target.type != EmergencyStopTarget)
            b.target.id = list;
}

const ControlMap::Binding *ControlMap::deskLampOn(quint64 k) const
{
    for (const Binding &b : m_deskLamps)
        if (key(b.universe, b.channel) == k)
            return &b;
    return nullptr;
}

const ControlMap::Binding *ControlMap::lampOwner(quint64 k) const
{
    // This computer's desk has its controls (Q309): a show trigger there is blocked.
    if (const Binding *desk = deskLampOn(k))
        return desk;
    if (const Binding *stop = stopOn(k))
        return stop;
    Binding next;
    if (pick(k, &next, false))
        return binding(next.target);
    return runningOn(k);
}

ControlMap::Lamp ControlMap::controlLamp(quint32 universe, quint32 channel) const
{
    const Binding *owner = lampOwner(key(universe, channel));
    return owner == nullptr ? LampOff : lamp(*owner);
}

void ControlMap::darkenLamp(const Binding &b)
{
    // A control that fires nothing stops blinking too, rather than go on
    // for the rest of the hold's blink on a trigger that has gone.
    for (int i = m_blinking.count() - 1; i >= 0; --i)
        if (m_blinking.at(i).universe == b.universe && m_blinking.at(i).channel == b.channel)
            m_blinking.removeAt(i);
    if (m_blinking.isEmpty())
        m_blinkLeft = 0;    // the chain in flight ends at its next step
    sendLampValue(b, 0, true);
    m_lampSent.remove(key(b.universe, b.channel));
}

void ControlMap::resendFeedback()
{
    sendLamps(true);
}

void ControlMap::slotLampsMayHaveChanged()
{
    /* One pass per turn of the event loop, never one per event (review
       finding 1). A collection starting thirty scenes sends thirty
       functionStarted; each pass asks ShowControl::standbyStep() for every
       cue-list binding, and that takes the Chaser mutexes the ChaserRunner
       holds while it steps on the MasterTimer thread. Thirty passes would
       contend with the DMX tick for no gain: the lights are the same
       either way, so they are worked out once, after the batch. */
    if (!hasLamps() || m_lampsQueued)
        return;
    m_lampsQueued = true;
    QTimer::singleShot(0, this, [this]() {
        m_lampsQueued = false;
        if (!hasLamps())
            return;
        sendLamps(false);
        emit lampsChanged();
    });
}

void ControlMap::slotGuardHeld(const QString &text, quint32 chaserId)
{
    Q_UNUSED(text)
    /* The guard held a GO (or a Back, a Stop...) on this list: every
       control that GOes it blinks twice, so the surface says what the Run
       tab, the footer and the pad say (rule 2 of the double-GO guard). */
    refreshDeskTargets();
    QList<Binding> blinking;
    const QList<Binding> all = m_deskLamps + m_bindings;
    for (const Binding &b : all)
    {
        const Chaser *chaser = targetChaser(b);
        // Only where the control is this binding's now (Q296).
        const Binding *owner = lampOwner(key(b.universe, b.channel));
        if (chaser != nullptr && chaser->id() == chaserId && !isKey(b)
            && owner != nullptr && owner->target == b.target && owner->desk == b.desk)
            blinking.append(b);
    }
    if (blinking.isEmpty())
        return;
    // A hold on another list while one blinks: the first one's lights go
    // back to the truth rather than stay wherever its last step left them.
    const QList<Binding> was = m_blinking;
    m_blinking = blinking;
    for (const Binding &b : was)
        if (!isBlinking(b))
            sendLamp(b, true);
    /* One chain, however many holds arrive (review finding 4). The
       counter alone could not say whether a chain was in flight: it is 0
       for the whole of the last step's wait, so a hold landing there used
       to start a second chain, and the two shared the counter -- twice the
       rate, or one blink. */
    m_blinkLeft = 4;        // on, off, on, off: two blinks
    emit lampsChanged();    // the table says "GO held" while it blinks
    if (!m_blinkRunning)
    {
        m_blinkRunning = true;
        blinkStep();
    }
}

void ControlMap::blinkStep()
{
    if (m_blinkLeft <= 0)
    {
        // Done: back to what each light says now.
        m_blinkRunning = false;
        const QList<Binding> was = m_blinking;
        m_blinking.clear();
        for (const Binding &b : was)
            sendLamp(b, true);
        emit lampsChanged();
        return;
    }
    const bool on = (m_blinkLeft % 2) == 0;
    for (const Binding &b : std::as_const(m_blinking))
    {
        // Off is the profile's dim level on a desk that idles there.
        uchar dim = 0;
        const uchar off = profileLampRange(b.universe, b.channel, &dim, nullptr) ? dim : 0;
        sendLampValue(b, on ? lampValueOn(b, LampHeld) : off, true);
    }
    --m_blinkLeft;
    QTimer::singleShot(m_blinkMs, this, &ControlMap::blinkStep);
}

void ControlMap::stopBlinkStep()
{
    OutputRuleKeeper *rules = m_doc->outputRuleKeeper();
    const bool stopped = rules != nullptr && rules->emergencyStopped();
    m_stopBlinkLit = stopped ? !m_stopBlinkLit : true;
    bool any = false;
    const QList<Binding> all = m_deskLamps + m_bindings;
    for (const Binding &b : all)
    {
        if (b.target.type != EmergencyStopTarget || !profileLampRange(b.universe, b.channel, nullptr, nullptr))
            continue;
        if (!b.desk && deskLampOn(key(b.universe, b.channel)) != nullptr)
            continue;   // the desk has that control
        any = true;
        sendLampValue(b, lampValueOn(b, stopped ? LampStopped : lamp(b)), true);
    }
    if (!stopped || !any)
    {
        m_stopBlinkLit = true;
        m_stopBlink->stop();
    }
}

void ControlMap::slotFunctionStarted(quint32 id)
{
    Q_UNUSED(id)
    slotLampsMayHaveChanged();
}

void ControlMap::slotFunctionStopped(quint32 id)
{
    m_heldStarted.remove(id);
    slotLampsMayHaveChanged();
}

void ControlMap::slotFunctionRemoved(quint32 id)
{
    // Only a function binding goes: a cue's collection is rebaked under it.
    if (binding(function(id)) != nullptr)
        removeBinding(function(id));
}

void ControlMap::slotInputDeviceConnected()
{
    resendFeedback();
}

void ControlMap::slotDeskChanged()
{
    const DeskRecord *desk = m_doc->deskRecord();
    QList<Binding> lamps;
    if (desk != nullptr)
    {
        const quint32 list = runList();
        for (DeskRecord::Role role : { DeskRecord::Stop, DeskRecord::Go, DeskRecord::Back })
        {
            if (!desk->isLive(role))
                continue;   // not set, off here, or not reachable: no light of ours
            Target t = emergencyStop();
            if (role == DeskRecord::Go)
                t = cueList(list);
            else if (role == DeskRecord::Back)
                t = cueListBack(list);
            Binding b(desk->universeOf(role), desk->binding(role).channel, t);
            b.desk = true;
            lamps << b;
        }
    }
    /* A control the desk let go of: dark, unless the show binds it (its
       light is the show's again). */
    const QList<Binding> was = m_deskLamps;
    m_deskLamps = lamps;
    for (const Binding &b : was)
    {
        const quint64 k = key(b.universe, b.channel);
        if (deskLampOn(k) == nullptr)
            sendControlLamp(b.universe, b.channel, true);
    }
    updateConnections();
    for (const Binding &b : std::as_const(m_deskLamps))
        sendControlLamp(b.universe, b.channel, true);
    // Q314: the show's GO of the Run tab's list is armed again, or not.
    emit armingChanged();
}

/*****************************************************************************
 * Load & Save
 *****************************************************************************/

bool ControlMap::saveXML(QXmlStreamWriter *doc) const
{
    if (doc == nullptr)
        return false;
    if (m_bindings.isEmpty() && m_encoderBank->device().encoders < 0 && m_runTabList == 0xFFFFFFFF
        && m_dimWhenReady && !m_surfacePages->hasShowSets())
        return true;    // a show with none saves as before
    doc->writeStartElement(KXMLQLCControlMap);
    // The list the Run tab (and so the pad) plays, when it is not the main one.
    if (m_runTabList != 0xFFFFFFFF)
        doc->writeAttribute(KXMLRunList, QString::number(m_runTabList));
    // Only when the dim light is off: a show with it on saves as before.
    if (!m_dimWhenReady)
        doc->writeAttribute(KXMLLights, QStringLiteral("onoff"));
    for (const Binding &b : m_bindings)
    {
        doc->writeStartElement(KXMLTrigger);
        if (isKey(b))
        {
            // Key in place of Universe/Channel: a build without keys skips it.
            doc->writeAttribute(KXMLKey, QKeySequence(QKeyCombination::fromCombined(int(b.channel)))
                                             .toString(QKeySequence::PortableText));
        }
        else
        {
            doc->writeAttribute(KXMLUniverse, QString::number(b.universe));
            doc->writeAttribute(KXMLChannel, QString::number(b.channel));
        }
        doc->writeAttribute(KXMLTarget, typeToString(b.target.type));
        doc->writeAttribute(KXMLID, QString::number(b.target.id));
        if (b.mode != Toggle)
            doc->writeAttribute(KXMLMode, modeToString(b.mode));
        // Its place, as a hit's (Scope=, ScopeOf=); everywhere says nothing,
        // so an older build loads it as it always was.
        if (b.place.scope == NewerPlace)
        {
            // A newer version's place: back as it came, for that version.
            doc->writeAttribute(KXMLScope, b.place.newerScope);
            if (!b.place.newerOf.isNull())
                doc->writeAttribute(KXMLScopeOf, b.place.newerOf);
        }
        else if (b.place.scope != Everywhere)
        {
            doc->writeAttribute(KXMLScope, placeScopeToString(b.place.scope));
            doc->writeAttribute(KXMLScopeOf, QString::number(b.place.id));
        }
        doc->writeEndElement();
    }
    m_encoderBank->saveXML(doc);
    m_surfacePages->saveXML(doc);
    doc->writeEndElement();
    return true;
}

bool ControlMap::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCControlMap)
    {
        qWarning() << Q_FUNC_INFO << "Control Map node not found";
        return false;
    }
    m_bindings.clear();
    m_step.clear();
    m_down.clear();
    m_fired.clear();
    m_pressed.clear();
    m_estopHeld.clear();
    m_estopFlagged.clear();
    m_swallow.clear();
    m_swallowAt.clear();
    m_swallowSeen.clear();
    m_blockedHeld.clear();
    m_heldStarted.clear();
    m_blinking.clear();
    m_blinkLeft = 0;
    m_lampSent.clear();
    m_widened.clear();
    m_placeHomes.clear();
    m_encoderBank->reset();
    m_surfacePages->reset();
    // The light a ready control shows; a file that says nothing: dim.
    m_dimWhenReady = root.attributes().value(KXMLLights).toString() != QStringLiteral("onoff");
    bool okList = false;
    const quint32 runList = root.attributes().value(KXMLRunList).toUInt(&okList);
    const quint32 wasList = m_runTabList;
    m_runTabList = okList ? runList : 0xFFFFFFFF;
    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCEncoderBank)
        {
            m_encoderBank->loadXML(root);
            continue;
        }
        if (root.name() == KXMLQLCSurfacePageSet)
        {
            m_surfacePages->loadXML(root);
            continue;
        }
        if (root.name() != KXMLTrigger)
        {
            root.skipCurrentElement();
            continue;
        }
        const QXmlStreamAttributes attrs = root.attributes();
        Binding b;
        bool okU = false, okC = false, okI = false;
        if (attrs.hasAttribute(KXMLKey))
        {
            const QKeySequence seq = QKeySequence::fromString(attrs.value(KXMLKey).toString(),
                                                              QKeySequence::PortableText);
            b.universe = KeyboardUniverse;
            b.channel = seq.count() == 1 ? keyChannel(seq[0].key(), seq[0].keyboardModifiers()) : 0;
            okU = true;
            okC = b.channel != 0;
        }
        else
        {
            b.universe = attrs.value(KXMLUniverse).toUInt(&okU);
            b.channel = attrs.value(KXMLChannel).toUInt(&okC);
        }
        b.target.id = attrs.value(KXMLID).toUInt(&okI);
        const bool okT = stringToType(attrs.value(KXMLTarget).toString(), &b.target.type);
        if (okT && b.target.type == EmergencyStopTarget)
            okI = true;                 // the stop has no id: missing or bad is fine
        if (attrs.hasAttribute(KXMLMode) && !stringToMode(attrs.value(KXMLMode).toString(), &b.mode))
            b.mode = Toggle;
        // Its place (none in a show saved before places: everywhere).
        if (attrs.hasAttribute(KXMLScope))
        {
            bool okP = false;
            b.place.scope = stringToPlaceScope(attrs.value(KXMLScope).toString());
            b.place.id = attrs.value(KXMLScopeOf).toUInt(&okP);
            if (b.place.scope == Everywhere)
            {
                b.place = Place();
            }
            else if (b.place.scope == NewerPlace || !okP)
            {
                /* A place this build does not know, or cannot read: kept,
                   disarmed (never fires) and saved back as it was -- never
                   everywhere, which would fire a one-cue trigger all show. */
                b.place.scope = NewerPlace;
                b.place.id = 0xFFFFFFFF;
                b.place.newerScope = attrs.value(KXMLScope).toString();
                if (attrs.hasAttribute(KXMLScopeOf))
                    b.place.newerOf = attrs.value(KXMLScopeOf).toString();
            }
        }
        root.skipCurrentElement();
        if (b.target.type == EmergencyStopTarget)
        {
            b.target.id = 0;            // one stop: whatever id a file gave it
            b.place = Place();          // and always everywhere
        }
        // A target a newer build knows and this one doesn't: left out.
        if (!okU || !okC || !okI || !okT)
            continue;
        // One control per thing; one control may have several things (at
        // different places, or stepping through at one), never beside the stop.
        bool dup = false;
        for (const Binding &e : std::as_const(m_bindings))
            dup |= e.target == b.target
                   || (e.universe == b.universe && e.channel == b.channel
                       && (e.target.type == EmergencyStopTarget || b.target.type == EmergencyStopTarget));
        if (!dup)
            m_bindings.append(b);
    }
    changed(false);
    // Loaded on its own (not as part of a show): its places are checked now;
    // with a show, once the whole show is in (Doc::loaded).
    if (m_doc->loadStatus() != Doc::Loading)
        fixPlaces();
    resendFeedback();
    if (m_runTabList != wasList)
    {
        emit runTabListChanged(m_runTabList);
        emit armingChanged();
    }
    return true;
}
