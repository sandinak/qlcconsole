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
#include <functional>
#include <QMetaType>

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
 * speaks MIDI Show Control or OSC (docs/design/SHOW_CONTROL_DESIGN.md).
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
     *  (docs/design/SUBCUES_DESIGN.md) -- OSC /subgo and /subback; MSC has none. */
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

    /** The route a run command took to get here (docs/design/RUN_COMMANDS.md):
     *  every GO, Back, Sub GO, Stop... from anywhere enters through run()
     *  with its route, so the one place that guards it (App::runCommand())
     *  knows who asked. */
    enum Route
    {
        RouteKey,           //!< ⌘↩ and its family, Control ▸ GO (the App's actions)
        RouteRunTab,        //!< the Run tab's buttons and Space
        RouteVcRunner,      //!< a Cue Runner VC widget (click, its key or input)
        RoutePad,           //!< a controller profile's chaser step on a Show Tree list
        RouteVcCueList,     //!< a VC Cue List (and the web page driving it) or a VC button's next/previous step, on a Show Tree list
        RouteControlMap,    //!< a Control Map binding (GO / Stop a list or cue)
        RouteFocusCall,     //!< a focus-call cue going on with its list when done
        RouteOsc,           //!< OSC (QLab and others)
        RouteMsc,           //!< MIDI Show Control
        RouteOther          //!< anything else (a test, a script)
    };
    Q_ENUM(Route)
    static QString routeName(Route route);
    /** The console's own routes (a key, the Run tab, the pad): the App
     *  says what a GO held and offers Operate after one in Design. */
    static bool isConsoleRoute(Route route)
    {
        return route == RouteKey || route == RouteRunTab || route == RoutePad;
    }
    /** GO, Back, Sub GO and Sub Back put a cue on stage (RC8 holds the
     *  previews for them). */
    static bool takesStage(Command command)
    {
        return command == Go || command == Back || command == SubGo || command == SubBack;
    }

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
    /** THE entry point of a run command (docs/design/RUN_COMMANDS.md): what
     *  every route calls. With a run handler (the App's runCommand(): the
     *  guards, the preview hold, the console's notices), it goes there,
     *  which then calls execute(); with none (the engine alone, a test), or
     *  off this object's thread, straight to execute(). */
    bool run(const Request &request, Route route);

    /** The one place run commands are guarded, set by the App; empty: none.
     *  It must end in execute() for anything it lets through. */
    using RunHandler = std::function<bool(const Request &request, Route route)>;
    void setRunHandler(const RunHandler &handler) { m_runHandler = handler; }
    bool hasRunHandler() const { return bool(m_runHandler); }

    /** True when chaser @a chaserId plays as a Show Tree main or parallel
     *  cue list (not a Looks list, not sub-cues), so a stock QLC+ widget
     *  that drives it (the VC Cue List, a VC button's next/previous step)
     *  goes through run(). False for any other chaser, which those widgets
     *  keep stepping directly, as in QLC+ 4. */
    bool isTreeGoList(quint32 chaserId) const;

    /** Carries out @a request. False when it found nothing to act on (no
     *  such list or cue); commandHandled() says what happened either way.
     *  Only run() and its handler call this: a route calls run(). */
    bool execute(const Request &request);

    /** Would execute() put a cue on stage for @a request -- a GO, Back, Sub
     *  GO or Sub Back whose list (and named cue) is there? Exactly when
     *  execute() emits aboutToTakeStage(). */
    bool wouldTakeStage(const Request &request) const;

    /** The cue GO with no cue goes to next, on a stopped list; -1: none.
     *  For a LOADed sub-cue, its parent step (loadedSubCue() says which). */
    int loadedStep(quint32 chaserId) const { return m_loaded.value(chaserId).step; }
    /** The sub-cue LOADed in loadedStep() (LOAD 4.2); -1 for a main cue,
     *  or none. */
    int loadedSubCue(quint32 chaserId) const { return m_loaded.value(chaserId).subCue; }
    /** The LOADed cue of @a chaserId is now step @a step (its cue moved
     *  when the Show Tree reordered the list); -1 lets go of it. */
    void moveLoaded(quint32 chaserId, int step)
    {
        if (!m_loaded.contains(chaserId))
            return;
        if (step < 0)
            m_loaded.remove(chaserId);
        else
            m_loaded[chaserId].step = step;
    }

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

    /** A GO, Back, Sub GO or Sub Back is about to put a cue on stage, from
     *  anywhere (a key, the Run tab, OSC, MSC, the VC): emitted before it
     *  acts, on the thread the command arrived on. The Show Tree holds its
     *  Design previews on it (RC8, "GO holds previews"). */
    void aboutToTakeStage(ShowControl::Source source);

    /*********************************************************************
     * Show control out (QLab out)
     *********************************************************************/
public:
    /** Which GOs are sent on: every cue, or only those with a cue number
     *  of their own (decided: the "external cue" is the cue number a cue
     *  has been given -- no second number to keep (can change)). */
    enum OutCues { OutAllCues, OutNumberedCues };

    /** The per-show settings of what a GO sends on, saved with the show. */
    struct OutSettings
    {
        bool oscEnabled = false;
        QString oscHost;                          //!< where QLab (or anything) listens
        int oscPort = 53000;                      //!< QLab's own OSC port
        /** The OSC plugin line (an interface address) it leaves from;
         *  empty: the one on the host's subnet. */
        QString oscLine;
        /** The address, "{cue}" being the cue number: "/cue/{cue}/start",
         *  or "/go" to GO whatever QLab has next. */
        QString oscPath = QStringLiteral("/cue/{cue}/start");

        bool mscEnabled = false;
        QString mscOutput;                        //!< the MIDI plugin's name for the output
        int mscDevice = 1;                        //!< the MSC device ID sent to (0..111, 127)
        OutCues cues = OutAllCues;

        bool operator==(const OutSettings &o) const
        {
            return oscEnabled == o.oscEnabled && oscHost == o.oscHost && oscPort == o.oscPort
                   && oscLine == o.oscLine && oscPath == o.oscPath && mscEnabled == o.mscEnabled
                   && mscOutput == o.mscOutput && mscDevice == o.mscDevice && cues == o.cues;
        }
    };

    /** One message on its way out, ready to send. */
    struct OutMessage
    {
        enum Kind { Osc, Msc } kind = Osc;
        QString line;                             //!< OSC: the interface line; MSC: the MIDI output
        QString host;                             //!< OSC only
        int port = 0;                             //!< OSC only
        QByteArray data;                          //!< the OSC packet, or the SysEx F0 ... F7
    };

    const OutSettings &outSettings() const { return m_out; }
    void setOutSettings(const OutSettings &settings);

    /** The OSC packet for @a path with no arguments ("/cue/4/start"):
     *  the path and ",", each NUL-padded to a multiple of 4. */
    static QByteArray oscMessage(const QString &path);
    /** The MSC GO to @a deviceId for @a cue (lighting, all-call format
     *  01): F0 7F dev 02 01 01 cue F7. */
    static QByteArray mscGo(int deviceId, const QString &cue);

    /** Where an outgoing message goes; the default hands it to the OSC or
     *  MIDI plugin's output (no sockets of its own). A test swaps it. */
    using OutSender = std::function<bool(const OutMessage &message)>;
    void setOutSender(const OutSender &sender) { m_outSender = sender; }

    /** What a GO of @a cue (step @a step of @a chaser) sends, per the
     *  settings -- nothing for a cue left out by OutNumberedCues. */
    QList<OutMessage> outMessagesFor(const Chaser *chaser, int step, const QString &cue) const;

signals:
    /** A GO sent @a count messages on (for the page's "last sent" line). */
    void sentOut(const QString &what, bool ok);

private:
    void sendOut(const Chaser *chaser, int step, const QString &cue);
    bool sendViaPlugin(const OutMessage &message);

private:
    OutSettings m_out;
    OutSender m_outSender;

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
    RunHandler m_runHandler;            ///< the App's runCommand(); empty: none
    Source m_source;                    ///< of the command being carried out
    quint32 m_sourceUniverse;           ///< MSC: the universe it came in on
};

/** @} */

#endif
