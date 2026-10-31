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
#include <QDebug>
#include <climits>

#include "showtreecontroller.h"
#include "qlcinputprofile.h"
#include "qlcinputchannel.h"
#include "inputoutputmap.h"
#include "functionparent.h"
#include "showcontrol.h"
#include "mastertimer.h"
#include "controlmap.h"
#include "inputpatch.h"
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
    // A function deleted takes its binding with it.
    connect(m_doc, &Doc::functionRemoved, this, &ControlMap::slotFunctionRemoved);
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
    const int i = m_byControl.value(key(universe, channel), -1);
    return i < 0 ? nullptr : &m_bindings.at(i);
}

void ControlMap::setBinding(const Binding &b)
{
    if (!b.target.isValid() || b.universe == 0xFFFFFFFF || b.channel == 0xFFFFFFFF)
        return;
    for (int i = m_bindings.count() - 1; i >= 0; --i)
    {
        const Binding &e = m_bindings.at(i);
        if (e == b)
            return;     // already so
        if (e.target == b.target || (e.universe == b.universe && e.channel == b.channel))
        {
            // Its old control goes dark, unless the new one is the same.
            if (e.universe != b.universe || e.channel != b.channel)
                sendFeedback(e, false);
            m_bindings.removeAt(i);
        }
    }
    m_bindings.append(b);
    changed(true);
    const quint32 fid = feedbackFunction(b);
    Function *f = fid == 0xFFFFFFFF ? nullptr : m_doc->function(fid);
    sendFeedback(b, f != nullptr && f->isRunning());
}

bool ControlMap::removeBinding(const Target &target)
{
    for (int i = 0; i < m_bindings.count(); ++i)
    {
        if (m_bindings.at(i).target != target)
            continue;
        sendFeedback(m_bindings.at(i), false);
        m_down.remove(key(m_bindings.at(i).universe, m_bindings.at(i).channel));
        m_bindings.removeAt(i);
        changed(true);
        return true;
    }
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
        sendFeedback(b, false);
    m_bindings.clear();
    m_down.clear();
    m_heldStarted.clear();
    changed(true);
}

void ControlMap::reset()
{
    const bool had = !m_bindings.isEmpty() || m_listening;
    m_bindings.clear();
    m_down.clear();
    m_swallow.clear();
    m_heldStarted.clear();
    m_listening = false;
    m_learnBind = false;
    if (had)
        changed(false);
}

void ControlMap::changed(bool modified)
{
    rebuild();
    updateConnections();
    if (modified)
        m_doc->setModified();
    emit bindingsChanged();
}

void ControlMap::rebuild()
{
    m_byControl.clear();
    for (int i = 0; i < m_bindings.count(); ++i)
        m_byControl.insert(key(m_bindings.at(i).universe, m_bindings.at(i).channel), i);
}

void ControlMap::updateConnections()
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    if (iom == nullptr)
        return;

    // Input: only while there is something to hear it for.
    const bool wantInput = m_listening || !m_bindings.isEmpty();
    if (wantInput != m_inputConnected)
    {
        if (wantInput)
        {
            connect(iom, &InputOutputMap::inputValueChanged, this, &ControlMap::handleInput);
            connect(iom, &InputOutputMap::inputDeviceConnected, this, &ControlMap::slotInputDeviceConnected);
            connect(iom, &InputOutputMap::profileChanged, this, &ControlMap::slotInputDeviceConnected);
        }
        else
        {
            disconnect(iom, &InputOutputMap::inputValueChanged, this, &ControlMap::handleInput);
            disconnect(iom, &InputOutputMap::inputDeviceConnected, this, &ControlMap::slotInputDeviceConnected);
            disconnect(iom, &InputOutputMap::profileChanged, this, &ControlMap::slotInputDeviceConnected);
        }
        m_inputConnected = wantInput;
    }

    // Feedback: what runs, from the timer thread (queued here).
    MasterTimer *timer = m_doc->masterTimer();
    const bool wantFeedback = !m_bindings.isEmpty();
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
}

/*****************************************************************************
 * Learning
 *****************************************************************************/

void ControlMap::listen()
{
    m_listening = true;
    m_learnBind = false;
    updateConnections();
}

void ControlMap::learn(const Target &target, Mode mode)
{
    m_listening = true;
    m_learnBind = true;
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
    updateConnections();
}

/*****************************************************************************
 * Names
 *****************************************************************************/

QString ControlMap::controlName(quint32 universe, quint32 channel) const
{
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
            return m_doc->showTree()->cueList(target.id) != nullptr;
        case CueTarget:
        case HitTarget:
            return m_doc->showTree()->cue(target.id) != nullptr;
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
    }
    return tr("(gone)");
}

QString ControlMap::modeName(Mode mode, TargetType type)
{
    const bool list = type == CueListTarget || type == CueTarget;
    switch (mode)
    {
        case Toggle:    return list ? tr("GO") : tr("Toggle");
        case Momentary: return list ? tr("GO while held") : tr("Momentary (on while held)");
        case StartOnly: return list ? tr("GO") : tr("Start only");
        case StopOnly:  return list ? tr("Stop the list") : tr("Stop only");
    }
    return QString();
}

QString ControlMap::typeToString(TargetType type)
{
    switch (type)
    {
        case FunctionTarget: return QStringLiteral("Function");
        case CueListTarget:  return QStringLiteral("CueList");
        case CueTarget:      return QStringLiteral("Cue");
        case HitTarget:      return QStringLiteral("Hit");
    }
    return QString();
}

bool ControlMap::stringToType(const QString &str, TargetType *type)
{
    static const TargetType all[] = { FunctionTarget, CueListTarget, CueTarget, HitTarget };
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
    if (m_listening && value > 0)
    {
        const bool bind = m_learnBind;
        m_listening = false;
        m_learnBind = false;
        // The press that taught it is held: it doesn't fire what it was
        // bound to, and its release isn't a release of the binding.
        m_swallow.insert(key(universe, channel));
        if (bind)
        {
            Binding b = m_learning;
            b.universe = universe;
            b.channel = channel;
            setBinding(b);
        }
        else
        {
            updateConnections();
        }
        emit heard(universe, channel);
        return;
    }

    const quint64 k = key(universe, channel);
    if (!m_swallow.isEmpty() && m_swallow.contains(k))
    {
        if (value == 0)
            m_swallow.remove(k);
        return;
    }
    const int i = m_byControl.value(k, -1);
    if (i < 0)
    {
        if (value == 0)
            m_down.remove(k);
        return;
    }
    const Binding b = m_bindings.at(i);
    if (value > 0)
    {
        // A press is the edge from 0: a fader or an aftertouch going on
        // sending values is still the one press.
        if (m_down.contains(k))
            return;
        m_down.insert(k);
        press(b);
    }
    else
    {
        if (!m_down.remove(k))
            return;
        release(b);
    }
}

void ControlMap::press(const Binding &b)
{
    switch (b.target.type)
    {
        case FunctionTarget:
        case HitTarget:
        {
            const quint32 fid = b.target.type == FunctionTarget ? b.target.id : feedbackFunction(b);
            const QString what = targetName(b.target);
            switch (b.mode)
            {
                case Toggle:    actOnFunction(fid, -1, what); break;
                case StartOnly: actOnFunction(fid, 1, what); break;
                case StopOnly:  actOnFunction(fid, 0, what); break;
                case Momentary:
                {
                    Function *f = m_doc->function(fid);
                    if (f != nullptr && !f->isRunning())
                    {
                        m_heldStarted.insert(fid);
                        actOnFunction(fid, 1, what);
                    }
                    break;
                }
            }
            break;
        }
        case CueListTarget:
        case CueTarget:
            goList(b, b.mode == StopOnly);
            break;
    }
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

void ControlMap::goList(const Binding &b, bool stop)
{
    ShowTreeController *st = m_doc->showTree();
    const ShowTree::CueList *list = b.target.type == CueListTarget ? st->cueList(b.target.id)
                                                                   : st->listOfCue(b.target.id);
    if (list == nullptr || list->chaser == ShowTree::invalidId())
        return;
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(list->chaser));
    if (chaser == nullptr)
        return;

    ShowControl::Request r;
    r.chaser = chaser->id();
    r.command = stop ? ShowControl::Stop : ShowControl::Go;
    if (!stop && b.target.type == CueTarget)
    {
        const ShowTree::Cue *c = st->cue(b.target.id);
        int step = -1;
        for (int i = 0; c != nullptr && i < chaser->stepsCount(); ++i)
        {
            if (chaser->stepAt(i) != nullptr && chaser->stepAt(i)->fid == c->collection)
            {
                step = i;
                break;
            }
        }
        if (step < 0)
            return;
        r.cue = ShowControl::cueName(chaser, step, -1);
    }
    // The console's one run path (docs/design/RUN_COMMANDS.md).
    if (m_doc->showControl()->run(r, ShowControl::RouteControlMap))
        emit fired(tr("%1 %2").arg(stop ? tr("Stop") : tr("GO"), targetName(b.target)));
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
    }
    return 0xFFFFFFFF;
}

void ControlMap::sendFeedback(const Binding &b, bool on) const
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    if (iom == nullptr)
        return;
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
    iom->sendFeedBack(b.universe, b.channel, on ? UCHAR_MAX : 0, params);
}

void ControlMap::sendFeedbackFor(quint32 fid, bool on)
{
    for (const Binding &b : std::as_const(m_bindings))
        if (feedbackFunction(b) == fid)
            sendFeedback(b, on);
}

void ControlMap::resendFeedback()
{
    for (const Binding &b : std::as_const(m_bindings))
    {
        const quint32 fid = feedbackFunction(b);
        Function *f = fid == 0xFFFFFFFF ? nullptr : m_doc->function(fid);
        sendFeedback(b, f != nullptr && f->isRunning());
    }
}

void ControlMap::slotFunctionStarted(quint32 id)
{
    sendFeedbackFor(id, true);
}

void ControlMap::slotFunctionStopped(quint32 id)
{
    m_heldStarted.remove(id);
    sendFeedbackFor(id, false);
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

/*****************************************************************************
 * Load & Save
 *****************************************************************************/

bool ControlMap::saveXML(QXmlStreamWriter *doc) const
{
    if (doc == nullptr)
        return false;
    if (m_bindings.isEmpty())
        return true;    // a show with none saves as before
    doc->writeStartElement(KXMLQLCControlMap);
    for (const Binding &b : m_bindings)
    {
        doc->writeStartElement(KXMLTrigger);
        doc->writeAttribute(KXMLUniverse, QString::number(b.universe));
        doc->writeAttribute(KXMLChannel, QString::number(b.channel));
        doc->writeAttribute(KXMLTarget, typeToString(b.target.type));
        doc->writeAttribute(KXMLID, QString::number(b.target.id));
        if (b.mode != Toggle)
            doc->writeAttribute(KXMLMode, modeToString(b.mode));
        doc->writeEndElement();
    }
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
    m_down.clear();
    m_heldStarted.clear();
    while (root.readNextStartElement())
    {
        if (root.name() != KXMLTrigger)
        {
            root.skipCurrentElement();
            continue;
        }
        const QXmlStreamAttributes attrs = root.attributes();
        Binding b;
        bool okU = false, okC = false, okI = false;
        b.universe = attrs.value(KXMLUniverse).toUInt(&okU);
        b.channel = attrs.value(KXMLChannel).toUInt(&okC);
        b.target.id = attrs.value(KXMLID).toUInt(&okI);
        const bool okT = stringToType(attrs.value(KXMLTarget).toString(), &b.target.type);
        if (attrs.hasAttribute(KXMLMode) && !stringToMode(attrs.value(KXMLMode).toString(), &b.mode))
            b.mode = Toggle;
        root.skipCurrentElement();
        // A target a newer build knows and this one doesn't: left out.
        if (!okU || !okC || !okI || !okT)
            continue;
        bool dup = false;
        for (const Binding &e : std::as_const(m_bindings))
            dup |= e.target == b.target || (e.universe == b.universe && e.channel == b.channel);
        if (!dup)
            m_bindings.append(b);
    }
    changed(false);
    resendFeedback();
    return true;
}
