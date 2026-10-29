/*
  Q Light Controller Plus
  controlmap.h

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

#ifndef CONTROLMAP_H
#define CONTROLMAP_H

#include <QMultiHash>
#include <QObject>
#include <QString>
#include <QList>
#include <QHash>
#include <QSet>

#include "qlcengine_export.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLQLCControlMap QStringLiteral("ControlMap")

/**
 * The Control Map: a MIDI note or CC, or an OSC message -- any control on
 * any input universe -- bound straight to something to fire, with no
 * Virtual Console widget in between (docs/dev/CONTROLMAP.md). A binding
 * fires a function (a scene, an effect, a chaser...), GOes a Show Tree cue
 * list, GOes one cue of a list, or fires a hit; how a press acts is its
 * mode. The control's light follows what it fires (input feedback, as a VC
 * button's does), where the universe has a feedback line.
 *
 * Saved with the show as <ControlMap>, only when there is a binding, so a
 * show without one saves as before; QLC+ 4 skips the element.
 *
 * GUI thread only: input arrives queued from the input patches, feedback
 * follows the MasterTimer's started/stopped signals queued here. With no
 * binding (and not learning) nothing is connected, so input costs nothing.
 */
class QLC_ENGINE_EXPORT ControlMap final : public QObject
{
    Q_OBJECT

public:
    explicit ControlMap(Doc *doc);
    ~ControlMap();

    /** What a binding fires. */
    enum TargetType
    {
        FunctionTarget, //!< a function, by its id
        CueListTarget,  //!< a Show Tree cue list, by its id: GO
        CueTarget,      //!< a Show Tree cue, by its id: GO that cue
        HitTarget       //!< a Show Tree hit (a Looks section's cue), by its id
    };

    /** How a press acts. On a cue list or a cue, Toggle and Start only GO,
     *  Momentary GOes on the press and stops the list on the release, and
     *  Stop only stops the list. */
    enum Mode
    {
        Toggle,     //!< a press starts it, the next press stops it
        Momentary,  //!< on while held
        StartOnly,  //!< a press starts it (never stops)
        StopOnly    //!< a press stops it (never starts)
    };

    struct Target
    {
        TargetType type = FunctionTarget;
        quint32 id = 0xFFFFFFFF;

        bool isValid() const { return id != 0xFFFFFFFF; }
        bool operator==(const Target &o) const { return type == o.type && id == o.id; }
        bool operator!=(const Target &o) const { return !(*this == o); }
    };

    struct Binding
    {
        quint32 universe = 0xFFFFFFFF;
        quint32 channel = 0xFFFFFFFF;
        Target target;
        Mode mode = Toggle;

        bool operator==(const Binding &o) const
        { return universe == o.universe && channel == o.channel && target == o.target && mode == o.mode; }
    };

    static Target function(quint32 id) { return Target{ FunctionTarget, id }; }
    static Target cueList(quint32 id) { return Target{ CueListTarget, id }; }
    static Target cue(quint32 id) { return Target{ CueTarget, id }; }
    static Target hit(quint32 id) { return Target{ HitTarget, id }; }

    /*********************************************************************
     * Bindings
     *********************************************************************/
public:
    QList<Binding> bindings() const { return m_bindings; }
    int count() const { return m_bindings.count(); }
    bool isEmpty() const { return m_bindings.isEmpty(); }

    /** The binding firing @a target (null: none). */
    const Binding *binding(const Target &target) const;
    /** The binding on a control (null: none). */
    const Binding *bindingOn(quint32 universe, quint32 channel) const;
    /** Whether a control fires something here. */
    bool isBound(quint32 universe, quint32 channel) const
    { return m_byControl.contains(key(universe, channel)); }

    /** Binds @a b: one control fires one thing and one thing has one
     *  control, so a binding on the same control or for the same target
     *  is replaced. Marks the show modified. */
    void setBinding(const Binding &b);
    /** Drops @a target's binding (its light goes off); false: none. */
    bool removeBinding(const Target &target);
    /** Changes how @a target's control acts; false: no binding. */
    bool setMode(const Target &target, Mode mode);
    /** Drops every binding (each light off). */
    void removeAll();

    /** Back to a new show's (no bindings, not learning); doesn't mark it
     *  modified or send feedback. */
    void reset();

    /*********************************************************************
     * Learning
     *********************************************************************/
public:
    /** Listens for the next press on any input universe (a value above 0:
     *  a release or a fader at zero doesn't count): heard() says which. */
    void listen();
    /** listen(), then bind what is heard to @a target with @a mode. */
    void learn(const Target &target, Mode mode = Toggle);
    void stopListening();
    bool isListening() const { return m_listening; }

    /*********************************************************************
     * Names
     *********************************************************************/
public:
    /** The control in words: its input profile's name for it, else a MIDI
     *  note or CC ("Note 60 (C4), MIDI ch 1"), else its channel number;
     *  with the universe's name. */
    QString controlName(quint32 universe, quint32 channel) const;
    /** What a target is ("Scene: Warm wash", "Cue list: Act 1",
     *  "Cue 4 of Act 1", "Hit: Stab"); "(gone)" when it isn't there. */
    QString targetName(const Target &target) const;
    /** Whether @a target is still in the show. */
    bool targetExists(const Target &target) const;
    /** The mode in words, as it acts on a target of @a type. */
    static QString modeName(Mode mode, TargetType type = FunctionTarget);

    static QString typeToString(TargetType type);
    static bool stringToType(const QString &str, TargetType *type);
    static QString modeToString(Mode mode);
    static bool stringToMode(const QString &str, Mode *mode);

    /*********************************************************************
     * Input and feedback
     *********************************************************************/
public slots:
    /** One input value (InputOutputMap::inputValueChanged). Public for
     *  tests: it is what a control does. */
    void handleInput(quint32 universe, quint32 channel, uchar value);

public:
    /** The function whose running state a binding's light shows (the
     *  function; a hit's or cue's collection; a cue list's chaser).
     *  Invalid: none. */
    quint32 feedbackFunction(const Binding &b) const;
    /** Sends every binding's light as it is now (a controller plugged
     *  back in, a show loaded). */
    void resendFeedback();

signals:
    /** A binding was added, removed or changed. */
    void bindingsChanged();
    /** listen(): the control pressed. */
    void heard(quint32 universe, quint32 channel);
    /** A control fired its target: what it did, in words. */
    void fired(const QString &what);

    /*********************************************************************
     * Load & Save
     *********************************************************************/
public:
    bool saveXML(QXmlStreamWriter *doc) const;
    bool loadXML(QXmlStreamReader &root);

private slots:
    void slotFunctionStarted(quint32 id);
    void slotFunctionStopped(quint32 id);
    void slotFunctionRemoved(quint32 id);
    void slotInputDeviceConnected();

private:
    static quint64 key(quint32 universe, quint32 channel)
    { return (quint64(universe) << 32) | channel; }
    /** Rebuilds the lookups and (dis)connects input and feedback. */
    void changed(bool modified);
    void rebuild();
    void updateConnections();
    void press(const Binding &b);
    void release(const Binding &b);
    /** Starts, stops or toggles (start: 1, stop: 0, toggle: -1) a function. */
    void actOnFunction(quint32 fid, int how, const QString &what);
    void goList(const Binding &b, bool stop);
    void sendFeedback(const Binding &b, bool on) const;
    void sendFeedbackFor(quint32 fid, bool on);

private:
    Doc *m_doc;
    QList<Binding> m_bindings;
    /** control -> index in m_bindings */
    QHash<quint64, int> m_byControl;
    /** controls held down (a press is an edge from 0) */
    QSet<quint64> m_down;
    /** the press a learn heard, until its release */
    QSet<quint64> m_swallow;
    /** Momentary functions a press started (the release stops only those) */
    QSet<quint32> m_heldStarted;
    bool m_listening = false;
    bool m_learnBind = false;
    Binding m_learning;
    bool m_inputConnected = false;
    bool m_feedbackConnected = false;
};

/** @} */

#endif
