/*
  Q Light Controller Plus
  showcontrol.h

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

#ifndef SHOWCONTROL_H
#define SHOWCONTROL_H

#include <QByteArray>
#include "qlcengine_export.h"
#include <QObject>
#include <QString>
#include <QList>
#include <QHash>

class QXmlStreamReader;
class QXmlStreamWriter;
class Chaser;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLQLCShowControl QStringLiteral("ShowControl")

/**
 * Show control: cues fired by number from outside -- QLab, or anything that
 * speaks MIDI Show Control or OSC (SHOW_CONTROL_DESIGN.md).
 *
 * A cue list is a chaser; a cue is one of its steps, by its cue number
 * (Chaser::stepCue()). A command names a list by its number
 * (Chaser::cueListNumber()) or goes to the show's main cue list.
 *
 * The decoding is here, and pure: the MIDI plugin hands over SysEx as it
 * arrives (Doc wires it), the UI hands over OSC datagrams from its own
 * listener (the engine doesn't link Qt Network).
 */
class QLC_ENGINE_EXPORT ShowControl final : public QObject
{
    Q_OBJECT

public:
    explicit ShowControl(Doc *doc, QObject *parent = nullptr);

    /** SubGo / SubBack: through the sub-cues of the cue playing
     *  (SUBCUES_DESIGN.md) -- OSC /subgo and /subback; MSC has none. */
    enum Command { Go, Stop, Pause, Resume, Load, AllOff, Reset, Back, SubGo, SubBack };

    /** Where a command came from: OSC, MSC, or this console (a key, the
     *  footer, the Virtual Console). */
    enum Source { Local, Osc, Msc };
    Q_ENUM(Source)

    /** One command: what, which cue (empty: none) and which list (empty:
     *  the main one). */
    struct Request
    {
        Command command = Go;
        QString cue;
        QString list;
        /** A list by its chaser (a Cue Runner following one): wins over
         *  @a list. UINT_MAX: by @a list. */
        quint32 chaser = UINT_MAX;
    };

    static QString commandName(Command command);

    /*********************************************************************
     * Cue lists
     *********************************************************************/
public:
    /** The main cue list: the chaser a command with no list goes to. */
    quint32 mainCueList() const { return m_mainCueList; }
    void setMainCueList(quint32 chaserId);

    /** The cue list @a list names: the chaser with that cue-list number;
     *  empty, the main cue list -- or, with none set, the show's only
     *  chaser. Null when there's none. */
    Chaser *cueList(const QString &list) const;

    /*********************************************************************
     * Commands
     *********************************************************************/
public:
    /** Carries out @a request. False when it found nothing to act on (no
     *  such list or cue); commandHandled() says what happened either way. */
    bool execute(const Request &request);

    /** The cue GO with no cue goes to next, on a stopped list; -1: none.
     *  For a LOADed sub-cue, its parent step (loadedSubCue() says which). */
    int loadedStep(quint32 chaserId) const { return m_loaded.value(chaserId).step; }
    /** The sub-cue LOADed in loadedStep() (LOAD 4.2); -1 for a main cue,
     *  or none. */
    int loadedSubCue(quint32 chaserId) const { return m_loaded.value(chaserId).subCue; }

    /** The step a GO with no cue plays next on @a chaser -- what's on
     *  standby: the loaded cue; else, playing, the next not skipped; else
     *  the first not skipped. -1: none (the end of a single-shot list, or
     *  every step skipped). */
    int standbyStep(const Chaser *chaser) const;
    /** The sub-cue of standbyStep() on standby: a LOADed sub-cue; -1 when
     *  what's on standby is a main cue. */
    int standbySubCue(const Chaser *chaser) const;

    /** Cue @a step (sub-cue @a subCue, -1 for the cue itself) of
     *  @a chaser as a command names it: "4", "4.2". */
    static QString cueName(const Chaser *chaser, int step, int subCue);

signals:
    /** The main cue list was set (by the chaser editor, the Show Tree's
     *  current production), or let go of with its chaser. Not on load. */
    void mainCueListChanged(quint32 chaserId);

    /** A command arrived and was carried out (@a done), or not, with a
     *  line saying which and why: "GO cue 12.5 — Main", "GO cue 9: no such
     *  cue in Main". */
    void commandHandled(const QString &what, bool done);

    /** The same, with where the command came from -- and for MSC, the
     *  universe whose MIDI input it arrived on (UINT_MAX otherwise). */
    void commandFrom(ShowControl::Source source, quint32 universe,
                     const QString &what, bool done);

    /*********************************************************************
     * MIDI Show Control
     *********************************************************************/
public:
    /** The device ID MSC is answered on (0..111); the all-call 127 and
     *  the group IDs (112..126) are always answered. */
    int mscDeviceId() const { return m_mscDeviceId; }
    void setMscDeviceId(int id);

    bool mscEnabled() const { return m_mscEnabled; }
    void setMscEnabled(bool enable) { m_mscEnabled = enable; }

    /** The one input MSC is answered on, by the plugin's name for it
     *  ("MIDI", "IAC Driver Bus 1") -- the port belongs to the machine, so
     *  it's named, not numbered by universe. Empty: every patched input. */
    void setMscInput(const QString &plugin, const QString &input);
    QString mscInputPlugin() const { return m_mscInputPlugin; }
    QString mscInput() const { return m_mscInput; }

    /** The universes whose input patch is the chosen MSC input (none when
     *  it isn't patched in this show); with no choice, every universe with
     *  an input patched. */
    QList<quint32> mscUniverses() const;

    /** Whether MSC arriving on @a universe's input is answered. */
    bool acceptsMscFrom(quint32 universe) const;

    /** Decodes one MSC message (with or without its F0 ... F7) for device
     *  @a deviceId. False when it isn't MSC, isn't for us, or isn't a
     *  command we carry out. */
    static bool parseMsc(const QByteArray &sysEx, int deviceId, Request *out);

public slots:
    /** SysEx from a patched MIDI input: an MSC command is carried out. */
    void handleSysEx(quint32 universe, const QByteArray &sysEx);

    /*********************************************************************
     * OSC
     *********************************************************************/
public:
    /** The commands in one OSC datagram (a message, or a bundle of them).
     *  Addresses we don't know are left out. */
    static QList<Request> parseOsc(const QByteArray &datagram);

    /** Carries out the commands in @a datagram; the count carried out. */
    int handleOsc(const QByteArray &datagram);

    /*********************************************************************
     * Load & Save
     *********************************************************************/
public:
    void reset();
    bool loadXML(QXmlStreamReader &root);
    bool saveXML(QXmlStreamWriter *doc) const;

private slots:
    void slotFunctionRemoved(quint32 id);

private:
    QString listName(const Chaser *chaser) const;
    void go(Chaser *chaser, int step);
    void goSubCue(Chaser *chaser, int step, int subCue);

private:
    Doc *m_doc;
    quint32 m_mainCueList;
    struct Standby
    {
        int step = -1;
        int subCue = -1;                ///< a LOADed sub-cue of step; -1: none
    };
    QHash<quint32, Standby> m_loaded;   ///< chaser id -> cue on standby
    int m_mscDeviceId;
    bool m_mscEnabled;
    QString m_mscInputPlugin;           ///< empty: any input
    QString m_mscInput;
    Source m_source;                    ///< of the command being carried out
    quint32 m_sourceUniverse;           ///< MSC: the universe it came in on
};

/** @} */

#endif
