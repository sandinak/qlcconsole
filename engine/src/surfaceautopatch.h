/*
  Q Light Controller Plus - qlcconsole
  surfaceautopatch.h

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

#ifndef SURFACEAUTOPATCH_H
#define SURFACEAUTOPATCH_H

#include <QObject>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include "qlcengine_export.h"

class Doc;
class QLCIOPlugin;

/** @addtogroup engine Engine
 * @{
 */

/**
 * A known control surface is set up when it is plugged in (bench
 * 2026-11-27: "not found on plug-in", every board patched by hand).
 *
 * A MIDI or HID input line that appears and whose name fits a surface
 * profile (InputOutputMap::profileForDevice(): case aside, a trailing
 * "(Device)" and port words aside; of a board with several ports only the
 * one its profile names) is patched: its input with that profile, its
 * feedback to the same device's output, on the universe it last had on
 * this computer when that one is free, else the first free one (a new one
 * when none is). Its init follows (SurfaceSetup). A line whose name many
 * boxes share (the MaizeDMX's "USB2.0-MIDI") is never patched by itself:
 * askToUse() offers it.
 *
 * Nothing happens to a device a universe already has or waits for (the
 * show's own patch follows it), nor to one whose auto-patch was undone.
 * Off until the app turns it on (setEnabled()): a test's devices stay as
 * the test patches them.
 */
class QLC_ENGINE_EXPORT SurfaceAutoPatch final : public QObject
{
    Q_OBJECT

public:
    explicit SurfaceAutoPatch(Doc *doc, QObject *parent = nullptr);

    bool isEnabled() const { return m_enabled; }
    /** On: devices plugged in from now on are set up; with @p scanNow, the
     *  ones already here too. */
    void setEnabled(bool on, bool scanNow = true);

    /** How long after a device appears it is set up (ms): the show's own
     *  patch, or a person patching it, goes first. */
    void setDelay(int ms) { m_delayMs = ms; }

    /** Set up every known board that is here and no universe has. */
    void scanNow();

    /** Set up @p device of @p plugin now, as if it had just appeared
     *  (@p profile: this one; empty: the one its name fits). Returns the
     *  universe, or UINT_MAX when nothing was done (@p why says why). */
    quint32 setUp(QLCIOPlugin *plugin, const QString &device, const QString &profile = QString(),
                  QString *why = nullptr);

    /** Set up the game pad @p device (line @p line of @p plugin): its input
     *  patched (the pad's roles come from its HID profile, chosen by its
     *  USB ids; light bar and rumble go back on its own line), on the
     *  universe it had last on this computer when free, else the first
     *  free one. padPatched() says it. UINT_MAX: nothing done. */
    quint32 setUpPad(QLCIOPlugin *plugin, const QString &device, quint32 line);

    /** Undo the auto-patch of @p universe: its input and feedback let go,
     *  and its device is not set up by itself again on this computer
     *  until it is patched by hand. */
    bool undo(quint32 universe);

    /** A person's yes to askToUse(): the device is that board on this
     *  computer from now on, and is set up now. */
    quint32 useAs(QLCIOPlugin *plugin, const QString &device, const QString &profile);

    /** @p device was declined (an undo) on this computer. */
    static bool isDeclined(const QString &device);
    static void setDeclined(const QString &device, bool declined);

    /** The universes set up by this object this session (for undo). */
    QList<quint32> patchedUniverses() const { return m_patched.keys(); }

    /**
     * A recognised board already patched (it came back, the show loaded,
     * the app started): what is wrong with its universe is put right
     * (Branson, build 4910: U01 had the PMJ as input and its feedback on
     * "Network RTP Session 1", left by the APC40 patched there before).
     *  - its feedback, when it is none, or points at another known board,
     *    a network session or a port that is gone: the board's own output.
     *    A feedback set to another port that is here and is not a board is
     *    a choice and is left;
     *  - its profile, when it is none, not installed or another board's:
     *    the board's own;
     *  - then its init goes out.
     * Logged in the Board Log; repaired() says it (with Undo: undoRepair()).
     * @return what was repaired, in words (empty: nothing).
     */
    QString repair(quint32 universe);
    /** repair() for a game pad's universe: an input profile of a board's
     *  (a pad needs none) is cleared, a feedback patch on another board, a
     *  network session or a gone port is let go (its light bar and rumble
     *  go back on its own input line). */
    QString repairPad(quint32 universe);
    /** Undo the last repair() of @p universe (PatchUndo), and leave that
     *  universe's patch as it is on this computer from now on. */
    bool undoRepair(quint32 universe);

    /** @p universe's feedback, paired to @p output of its input's plugin
     *  (empty: let go); false when that is no such line. */
    bool setFeedback(quint32 universe, const QString &output);
    /** Choose @p profile for @p universe's board: it is patched again with
     *  it, and its init goes out. */
    bool setProfile(quint32 universe, const QString &profile);

signals:
    /** @p device was set up on @p universe with @p profile: "APC40 mkII
     *  ready on universe 5" (with Undo). */
    void autoPatched(quint32 universe, const QString &profile, const QString &device);
    /** @p universe's board (@p profile) had its patch put right: @p what
     *  ("feedback repaired to its own port"). */
    void repaired(quint32 universe, const QString &profile, const QString &what);
    /** The game pad @p device was set up on @p universe: "DUALSHOCK 4
     *  ready as the pad" (with Undo: undo()). @p name: its short name. */
    void padPatched(quint32 universe, const QString &name, const QString &device);
    /** @p device may be the board of @p profile (a shared port name): ask. */
    void askToUse(const QString &plugin, const QString &device, const QString &profile);
    /** A device of @p plugin came or went (the Control surfaces list follows). */
    void devicesChanged();

private:
    void slotPluginChanged(const QString &pluginName);
    /** @p universe's controller slot set: a pad set up by itself on
     *  another universe meanwhile is let go and the slot takes it. */
    void slotInputSlotChanged(quint32 universe);
    void consider(QLCIOPlugin *plugin, const QString &device);
    bool known(QLCIOPlugin *plugin, const QString &device) const;
    quint32 freeUniverse(const QString &device);
    void snapshot();

    Doc *m_doc;
    bool m_enabled = false;
    int m_delayMs = 400;
    QHash<QString, QSet<QString>> m_present;     ///< plugin -> its input lines
    QHash<quint32, QString> m_patched;           ///< universe -> device, this session
    QHash<quint32, QString> m_padStandIns;       ///< universe -> pad set up by itself (setUpPad())
    bool m_settingUpPad = false;                 ///< our own setInputPatch() in progress
    QSet<QString> m_asked;                       ///< asked this session
    QHash<quint32, QString> m_repairUndo;        ///< universe -> its repair's undo summary
};

/** @} */

#endif
