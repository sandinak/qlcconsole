/*
  Q Light Controller Plus
  surfacesetup.h

  Copyright (C) Branson Matheson

  The OpenDeck check, align, identify and backup/restore follow
  qlcplus-midi-profiler's `opendeck align` / `identify` / `backup` /
  `restore` (Apache-2.0, by Branson Matheson); the PMJ Black 1's lamp
  pairing is the one his tool observed (maps/pmj-black-1.json).

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

#ifndef SURFACESETUP_H
#define SURFACESETUP_H

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QHash>
#include <QSet>

#include "qlcengine_export.h"
#include "opendecklink.h"

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * What a control surface needs set on the device itself to work with its
 * profile, as part of setting it up (docs/features/control-surfaces.md,
 * "Setting up a controller"). One mechanism for every known surface:
 *
 *  - its init: messages that put it in the mode its profile expects (the
 *    APC40 mkII's Ableton mode, the Launchpad Mini MK3's programmer mode),
 *    sent when its input is patched and again on every replug and wake;
 *  - a check: is the device set the way the profile needs it? (an OpenDeck
 *    board such as the PMJ Black 1: does each button's lamp listen on that
 *    button's note and channel?)
 *  - a fix (Align Lamps), with a full backup first and Undo.
 *
 * Nothing is ever written to a device's memory unless a person asked
 * (align(), undoAlign(), beginIdentify() are only called from a button in
 * the Set Up dialog), and never in Operate unless that person confirmed.
 * Owned by the Doc.
 */
class QLC_ENGINE_EXPORT SurfaceSetup final : public QObject
{
    Q_OBJECT

public:
    /** A lamp and the button it sits under: the button sends @a note on
     *  MIDI channel @a channel (1-16). */
    struct LampPair
    {
        int led = 0;
        int channel = 1;
        int note = 0;
    };

    /** A known surface, by its input profile's name. */
    struct Spec
    {
        QString profile;                ///< "PMJ Black 1"
        /** Sent on connect, replug and wake; each a complete message. */
        QList<QByteArray> init;
        QString initText;               ///< what the init does ("Ableton mode")
        bool openDeck = false;          ///< its config is read and written over SysEx
        QList<LampPair> pairing;        ///< shipped: which lamp is under which button
        /** Sent when the app lets go of it (it closes): the display back. */
        QList<QByteArray> release;
        /** A mode the board must be in that no message sets: checked by a
         *  pad press, which comes on this MIDI channel (1-16) in it. 0: none. */
        int modeChannel = 0;
        QString modeName;               ///< "Session"
        QString modeHint;               ///< "Press SESSION on the APC64"
    };

    /** A check's answer. */
    struct Check
    {
        enum State { NotChecked, NoAnswer, NoPairing, Aligned, Misplaced, NoInit, InitOnly,
                     WaitingForPress, InMode, WrongMode };
        State state = NotChecked;
        int lamps = 0;                  ///< lamps paired to a button
        int misplaced = 0;              ///< of those, not listening on their button
        QString text;                   ///< said in the dialog and the hub
    };

    explicit SurfaceSetup(Doc *doc);
    ~SurfaceSetup();

    /** Every known surface. */
    static const QList<Spec> &specs();
    static const Spec *spec(const QString &profileName);
    /** The surface patched on @p universe's input (null: none known). */
    const Spec *specOf(quint32 universe) const;
    /** Universes whose input is a known surface, in order. */
    QList<quint32> universes() const;
    /** "PMJ Black 1 (universe 3)". */
    QString title(quint32 universe) const;

    /* ---- init ---- */

    /** Send @p universe's surface its init now (false: none, or no way out).
     *  @p why is said in its board log ("on patch", "asked"). */
    bool sendInit(quint32 universe, const QString &why = QString());
    /** How long after a patch its init goes once more (ms; tests shorten it). */
    void setInitAgainDelay(int ms) { m_initAgainMs = ms; }

    /* ---- lamp test: any board with a feedback line ---- */
    /** Light each of @p universe's lamps in turn (the layout's lamps, else
     *  every button of its profile), then all of them, then give the lamps
     *  back to the show. False, with @p why, when it has no feedback line
     *  or no lamp. Each step is in the board log. */
    bool startLampTest(quint32 universe, QString *why = nullptr);
    void stopLampTest();
    bool lampTestRunning() const { return m_lampTestUniverse != UINT_MAX; }
    /** The channels a lamp test lights on @p universe, in order. */
    QList<quint32> lampChannels(quint32 universe) const;
    void setLampTestStepMs(int ms) { m_lampStepMs = ms; }

    /** Every board's log of MIDI in and out (SurfaceBoardLog). */
    class SurfaceBoardLog *boardLog() const { return m_boardLog; }
    /** Known boards set up when plugged in (SurfaceAutoPatch). */
    class SurfaceAutoPatch *autoPatch() const { return m_autoPatch; }
    /** Inits sent so far (a test counts them). */
    int initsSent() const { return m_initsSent; }

    /* ---- check ---- */

    /** Read the device and compare it with its profile (OpenDeck: its LED
     *  table against the lamp pairing). Reads only. */
    Check check(quint32 universe);
    /** The last check of @p universe (NotChecked until one is made). */
    Check lastCheck(quint32 universe) const;
    /** The pairing used for @p universe: one learned by Identify on this
     *  computer, else the one shipped with the profile. */
    QList<LampPair> pairing(quint32 universe) const;
    bool hasPairing(quint32 universe) const { return !pairing(universe).isEmpty(); }

    /* ---- a mode checked by a press (the APC64's Session mode) ---- */

    /** Ask for a pad press on @p universe's board to see its mode. Until
     *  it comes (or cancelModeCheck()) the board's presses do no job. */
    bool beginModeCheck(quint32 universe);
    void cancelModeCheck(quint32 universe);
    bool isWaitingForPress(quint32 universe) const { return m_modeWait.contains(universe); }

    /* ---- held presses ---- */

    /** While a setup step listens to @p universe's board for presses
     *  (Identify, a mode check), its presses do no job: no trigger, no
     *  overlay key, no desk GO or Back. The stops still stop. */
    void holdPresses(quint32 universe, bool on);

    /** The APC64's display text message for @p line (0-2). */
    static QByteArray apc64Text(int line, const QString &text);

    /* ---- writes: only from a button a person pressed ---- */

    /** Why a write may not happen now (empty: it may): in Operate a write
     *  needs @p confirmed. */
    QString writeRefusal(bool confirmed) const;

    /** Point every paired lamp at its own button. A full backup is taken
     *  first (lastBackup()); false with @p error when it was refused or failed. */
    bool align(quint32 universe, bool confirmed, QString *error = nullptr);
    /** The backup the last Align of this device took ("": none). Kept on this computer. */
    QString lastBackup(quint32 universe) const;
    /** Put back the last Align's backup (the state now is backed up first). */
    bool undoAlign(quint32 universe, bool confirmed, QString *error = nullptr);
    /** Put back the backup at @p path. */
    bool restore(quint32 universe, const QString &path, bool confirmed, QString *error = nullptr);

    /** Identify, for a board with no shipped pairing: backup, then lamp i
     *  answers note i on channel 1 so each can be flashed. */
    bool beginIdentify(quint32 universe, bool confirmed, QString *error = nullptr);
    /** Lamp @p led on or off (during Identify). */
    void flashLamp(quint32 universe, int led, bool on);
    /** The number of lamps the board has (read by beginIdentify()). */
    int lampCount(quint32 universe) const { return m_lampCount.value(universe); }
    /** The button QLC+ channel @p channel is, as a lamp pair for @p led
     *  (false: not a note). */
    static bool pairFromChannel(quint32 channel, int led, LampPair *pair);
    /** Identify done: its backup put back, @p pairs kept as this board's
     *  pairing on this computer (empty: nothing kept). */
    bool endIdentify(quint32 universe, const QList<LampPair> &pairs, QString *error = nullptr);

    /** Where backups go (tests point it at a temporary folder). */
    static QString backupDir();
    static void setBackupDir(const QString &dir);

    /** The link to @p universe's board, made on first use. */
    OpenDeckLink *link(quint32 universe);
    void setTimeout(int ms) { m_timeoutMs = ms; }

    /* ---- rescan ---- */

    /** Ask every hardware input plugin to look for devices again, and say
     *  what came of it: the names of new controllers, or (none) why the
     *  computer may not see one. */
    static QString rescanControllers(Doc *doc, QStringList *found = nullptr);

signals:
    /** A check, an align, an undo or an init changed what is known. */
    void changed(quint32 universe);
    /** The lamp test moved on: @p step says what is lit ("" when over). */
    void lampTestStepped(quint32 universe, const QString &step);

private slots:
    void slotSysEx(quint32 universe, const QByteArray &sysEx);
    void slotPatchChanged(quint32 universe);
    void slotWoke();
    void slotInput(quint32 universe, quint32 channel, uchar value);

private:
    bool sendTo(quint32 universe, const QByteArray &msg, const QString &why = QStringLiteral("setup"));
    QString deviceKey(quint32 universe) const;
    QString backup(quint32 universe, const QString &why, QString *error);

    Doc *m_doc;
    int m_timeoutMs = 1000;
    int m_initsSent = 0;
    QHash<quint32, OpenDeckLink *> m_links;
    QHash<quint32, Check> m_checks;
    QHash<quint32, int> m_lampCount;
    QHash<quint32, QString> m_identifyBackup;
    QSet<quint32> m_modeWait;
    QSet<quint32> m_holds;
    class SurfaceBoardLog *m_boardLog = nullptr;
    class SurfaceAutoPatch *m_autoPatch = nullptr;
    QSet<quint32> m_initAgain;
    int m_initAgainMs = 600;
    void lampTestStep();
    quint32 m_lampTestUniverse = UINT_MAX;
    QList<quint32> m_lampTestChannels;
    int m_lampTestIndex = 0;
    int m_lampStepMs = 250;
    class QTimer *m_lampTimer = nullptr;
};

/** @} */

#endif
