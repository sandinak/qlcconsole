/*
  Q Light Controller Plus
  editlive.h

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

#ifndef EDITLIVE_H
#define EDITLIVE_H

#include <QObject>
#include <QPointer>
#include <QHash>
#include <QList>
#include <QSet>
#include <functional>

#include "faderpickup.h"
#include "outputlayer.h"
#include "scene.h"
#include "qlcengine_export.h"

class SurfacePages;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Edit Live (Q337-Q339, docs/features/edit-live.md): in Operate, the
 * surface pages' knobs and faders trim what the running cue puts on the
 * selected fixtures, and Store writes only what was trimmed into the look
 * that sets it now -- the cue's own layer, or the earlier cue it tracks
 * from -- as one undo step.
 *
 * The trims are an OutputLayer (as Park, Highlight and Refine reach the
 * rig) at the Simple Desk's priority, Override: over the cue lists and the
 * override hits, under Highlight, Flash, Focus and Park; the Grand Master
 * and Blackout still apply. Only the channels a trim moved are on it, and
 * the show file is not touched until Store.
 *
 * GO is never held (Q338 a): when the running cue changes, the trims go at
 * once and are kept for leftBehindMs() as "left behind", which
 * storeLeftBehind() still stores into the cue they were made on.
 *
 * On only in Operate; it turns off (dropping the trims) on a switch to
 * Design, when the show is closed, and at the emergency stop. Who may
 * turn it on (the show PIN) is the UI's to ask. GUI thread.
 */
class QLC_ENGINE_EXPORT EditLive final : public QObject
{
    Q_OBJECT

public:
    EditLive(Doc *doc, SurfacePages *pages, QObject *parent = nullptr);
    ~EditLive() override;

    /** One trimmed setting: a fixture's attribute (an encoder bank slot),
     *  from and to (0-1), as the popup lists them. */
    struct Trim
    {
        quint32 fixture = 0;
        int bank = 0;
        int slot = 0;
        float from = 0.0f;
        float to = 0.0f;
    };

    bool isOn() const { return m_on; }
    /** On (Operate only: false and @p why otherwise) or off (drops the
     *  trims). */
    bool setOn(bool on, QString *why = nullptr);

    /** The cue the trims are on (invalid: none yet). */
    quint32 cue() const { return m_cue; }
    /** The cue running now in the show's current production (the first
     *  running list when none is current). */
    quint32 liveCue() const;
    /** "1.0.2 Chorus" */
    QString cueText(quint32 cueId) const;

    /** The pending settings, in the order they were first touched. */
    QList<Trim> trims() const;
    bool hasTrims() const { return !m_pending.isEmpty(); }
    /** "Spot 1 · Red · 40% → 62%" */
    QString trimText(const Trim &t) const;

    /** Whether Edit Live trims what @p slot of @p bank means (intensity,
     *  colour, position, zoom, focus, iris, frost; not wheels or strobe). */
    static bool canTrim(int bank, int slot);

    /** The fixtures a trim acts on: the programmer selection (a test
     *  stands in with @p probe; empty: the selection). */
    void setSelectionProbe(const std::function<QList<quint32>()> &probe) { m_selection = probe; }

    /*********************************************************************
     * Trimming (the board's pages, through SurfacePages)
     *********************************************************************/
public slots:
    /** A knob or encoder on a page turned. False (message()): refused. */
    bool adjust(int bank, int slot, int detents, bool fine);
    /** A fader on a page moved to @p value (0-255): it picks up first. */
    bool level(quint32 universe, quint32 channel, int bank, int slot, uchar value);

    /*********************************************************************
     * Store and Drop
     *********************************************************************/
public:
    /** Store the pending trims into the looks that set them in the running
     *  cue: one undo step, and a note (stored()). They stay on the rig
     *  until the cue changes (the running cue plays the stored look from
     *  its next GO). False (@p why): nothing stored. */
    bool store(QString *why = nullptr);
    /** The pending trims go at once (no fade). */
    void drop();

    /** After a GO: the trims that went with the old cue, for leftBehindMs(). */
    bool hasLeftBehind() const { return !m_left.values.isEmpty(); }
    quint32 leftBehindCue() const { return m_left.cue; }
    int leftBehindCount() const { return m_left.trims.size(); }
    /** "Store to Cue N": into the looks that set them in that cue. */
    bool storeLeftBehind(QString *why = nullptr);
    void forgetLeftBehind();
    static int leftBehindMs() { return 30000; }

    /** The last Store's note ("Stored Intensity into Warm in cue 1.0.1"). */
    QString lastNote() const { return m_note; }

public slots:
    /** The running cue may have changed (a GO, a stop): the trims drop. */
    void checkCue();

signals:
    void onChanged(bool on);
    /** The trims (or what is held) changed. */
    void changed();
    /** A refusal or a hint, for the footer ("select fixtures to trim"). */
    void message(const QString &text);
    /** A GO took the trims with the old cue @p cueId (@p count settings). */
    void leftBehind(quint32 cueId, int count);
    void stored(const QString &note);

private:
    /** What a Store writes: the trimmed values over what was there. */
    struct Snapshot
    {
        quint32 cue = 0xFFFFFFFF;
        QList<Trim> trims;
        QHash<quint64, uchar> values;   //!< (fixture << 32 | channel) -> trimmed
        QHash<quint64, uchar> base;     //!< what was there
    };

    QList<quint32> selection() const;
    bool begin(int bank, int slot, QList<quint32> *fixtures);
    void seed(const QList<quint32> &fixtures);
    void afterEdit(const QList<quint32> &fixtures, int bank, int slot);
    void push();
    Snapshot snapshot() const;
    bool storeSnapshot(const Snapshot &snap, QString *why);
    void clearAll();
    void connectSources();

private:
    Doc *m_doc;
    SurfacePages *m_pages;
    bool m_on = false;
    bool m_sourcesConnected = false;
    quint32 m_cue;
    OutputLayer m_layer;
    /** The values the trims start from (what the stage had), and where they
     *  are now: scratch scenes, never the show's. */
    QPointer<Scene> m_base;
    QPointer<Scene> m_trim;
    /** What the output had before any trim, per channel: what Drop and a
     *  stored trim (held until GO) are measured from. */
    QHash<quint64, uchar> m_original;
    QSet<quint32> m_seeded;
    /** (fixture, bank, slot) pending, in the order first touched. */
    QList<quint64> m_pending;
    QHash<quint64, FaderPickup> m_pickups;
    Snapshot m_left;
    QString m_note;
    std::function<QList<quint32>()> m_selection;
};

/** @} */

#endif
