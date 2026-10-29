/*
  Q Light Controller Plus
  venuecorrections.h

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

#ifndef VENUECORRECTIONS_H
#define VENUECORRECTIONS_H

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QSet>
#include <QList>
#include <QMap>
#include <QVector3D>

#include "qlcengine_export.h"
#include "truss.h"
#include "venuerig.h"

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Per-venue focus corrections (board Q105, docs/design/FOCUS_REFINE_DESIGN.md section 5).
 *
 * Focusing a show in a new room corrects its calibration -- a head's pan and
 * tilt offsets, a fixed light's aim, a hand-zoomed lens -- and those
 * corrections belong to the ROOM, not the show. They are kept here, by venue,
 * in a sidecar file next to the show ("Spring Show.qlcc" ->
 * "Spring Show.venues.xml"), written the moment one is saved so a restart
 * keeps them. The show file never takes them: while a venue is applied its
 * corrections are in the rig (the views and the aim solver read them there)
 * and the show's own calibration is held as each fixture's calibration base
 * (MonitorProperties::setCalibrationBase()), which is what saving the show
 * writes.
 *
 * One venue is active at a time, or none ("the show as built": Focus mode
 * then edits the show's own calibration, as before). The active venue is
 * remembered in the sidecar and applied again when the show is opened.
 */
class QLC_ENGINE_EXPORT VenueCorrections final : public QObject
{
    Q_OBJECT

public:
    explicit VenueCorrections(Doc *doc, QObject *parent = nullptr);
    ~VenueCorrections() override;

    /** Board Q134: a light looked at in a venue -- saved with A in Focus
     *  mode or the focus calls -- by whom and when, and whether it was
     *  marked dead (Q140: it needs a lamp or a repair, not a focus). For
     *  the focus report. */
    struct Check
    {
        QDateTime when;
        QString by;
        bool dead = false;
    };

    /** Scramble practice (docs/design/FOCUS_REFINE_DESIGN.md section 13): a practice
     *  venue's lights were put off by a known amount; this is where they
     *  are really right, and how the student did. */
    struct Practice
    {
        bool on = false;
        /** The venue it was scrambled from (empty: the show's own), put
         *  back when the practice ends. */
        QString from;
        /** fixture id -> its right calibration (what it was scrambled from) */
        QMap<quint32, RigCalibration> truth;
        QDateTime started;
        QDateTime lastSave;
        int saves = 0;
        /** Elements inside <Practice> this build doesn't know, kept. */
        QList<QByteArray> unknown;
    };

    /** One venue's corrections. */
    struct Venue
    {
        QString name;
        QDateTime created;
        QDateTime modified;
        /** fixture id -> its calibration in this venue */
        QMap<quint32, RigCalibration> fixtures;
        /** fixture id -> its name when corrected (for a reader of the file) */
        QMap<quint32, QString> names;
        /** fixture id -> what it was when corrected (board Q133): the
         *  definition's manufacturer and model, and the mode. Empty in a
         *  file from before they were kept. */
        QMap<quint32, QString> makers, models, modes;
        /** Board Q128: truss id -> this venue's trim (its height, metres),
         *  and stage target id -> this venue's position (metres). */
        QMap<quint32, float> trims;
        QMap<quint32, QVector3D> targets;
        /** Board Q137/Q138 (docs/design/VENUES.md): the room's house rig, its patch,
         *  what is not ours. Empty: the venue behaves as before. */
        VenueRig::Data rig;
        /** fixture id -> when it was checked here (Q134), dead or not. */
        QMap<quint32, Check> checks;
        Practice practice;
        /** Elements inside <Venue> this build doesn't know (a newer
         *  build's), kept whole and written back (docs/design/VENUES.md section 7). */
        QList<QByteArray> unknown;
    };

    /*********************************************************************
     * The sidecar file
     *********************************************************************/
public:
    /** The sidecar for @p showFile: the same folder and base name,
     *  ".venues.xml". Empty for an empty path. */
    static QString sidecarPath(const QString &showFile);

    /** The show was opened from, or saved as, @p showFile. Opened
     *  (@p opened true): forget any venue and read the show's sidecar, if it
     *  has one, applying its active venue. Saved under a new name: the
     *  venues go with it (written next to the new name). */
    void setShowFile(const QString &showFile, bool opened);
    QString showFile() const { return m_showFile; }
    /** Where the corrections are written; empty with no show file. */
    QString filePath() const { return sidecarPath(m_showFile); }

    /** Read the sidecar (replacing what is held). False, with @p why, on a
     *  file that cannot be read; a missing file is no venues, and true. */
    bool load(QString *why = nullptr);
    /** Write the sidecar now (atomically). False, with @p why, when there is
     *  no show file yet or it cannot be written. */
    bool save(QString *why = nullptr);

    /** Gap 8, the file travels: write these venues as the sidecar of
     *  @p showFile too (Collect for Transfer's copy of the show). */
    bool writeSidecarFor(const QString &showFile, QString *why = nullptr) const;
    /** A show's name without a version or copy ending: "Dance-0.6",
     *  "Dance copy 2", "Dance (3)" -> "Dance". */
    static QString showStem(const QString &showFile);
    /** Opened with no sidecar of its own: venue files beside it written
     *  for the same show under another name (an earlier versioned Save As,
     *  a Finder copy) -- their Show= has the same stem. Newest first. */
    static QStringList importCandidates(const QString &showFile);
    /** Take @p sidecar's venues as this show's (written beside it), the
     *  venue it names applied. */
    bool importFrom(const QString &sidecar, QString *why = nullptr);

    /** The XML, without a file (tests, and save()). */
    QByteArray toXml() const;
    bool fromXml(const QByteArray &xml, QString *why = nullptr);

    /*********************************************************************
     * Venues
     *********************************************************************/
public:
    QStringList venueNames() const;
    bool hasVenue(const QString &name) const;
    Venue venue(const QString &name) const;

    /** A name for a venue nobody named: "Venue 2026-10-23 19:04". */
    static QString defaultVenueName(const QDateTime &when = QDateTime::currentDateTime());

    /** Add a venue called @p name (empty: defaultVenueName(), made unique)
     *  and return the name it got; an existing name is returned as is. */
    QString addVenue(const QString &name = QString());
    /** Board Q140: a new venue starting from @p from's corrections, trims
     *  and targets (not its checks: nothing is checked in a new room). The
     *  name it got, as addVenue(); empty when @p from is not a venue. */
    QString addVenueFrom(const QString &name, const QString &from);
    /** The real venue (not a practice one) changed most recently, else
     *  empty: what a new venue starts from on a tour. */
    QString lastVenue() const;
    bool renameVenue(const QString &from, const QString &to);
    /** Remove a venue (un-applying it first if it is active). */
    bool removeVenue(const QString &name);

    /** The venue applied now; empty for none. */
    QString activeVenue() const { return m_active; }
    bool isActive() const { return !m_active.isEmpty(); }
    /** Apply @p name (empty: none) -- the one before is taken off first, so
     *  the rig is the show's calibration with this venue's on top -- and
     *  remember it in the sidecar. */
    bool setActiveVenue(const QString &name);

    /** When @p name's corrections last changed (else when it was made). */
    QDateTime lastChanged(const QString &name) const;
    /** How old an applied venue may be before opening the show asks
     *  "Still at <venue>?" (board Q129): about a day. */
    static constexpr int staleAfterHours() { return 20; }
    /** The venue applied was last changed more than staleAfterHours()
     *  before @p now and nobody has said it is still the room since the
     *  show was opened: ask. */
    bool needsConfirm(const QDateTime &now = QDateTime::currentDateTime()) const;
    /** Someone said the applied venue is still the room. */
    void confirmVenue() { m_confirmed = true; emit confirmedChanged(); }
    bool isConfirmed() const { return m_confirmed; }

    /*********************************************************************
     * Corrections (in the active venue)
     *********************************************************************/
public:
    /** Hold @p fid's calibration as the show's own, if it is not held yet,
     *  so whatever happens to it in the rig from now on stays out of the
     *  show file. Focus mode calls this on every head it starts on. */
    void ensureBase(quint32 fid);
    /** The show's own calibration for @p fid (the base, else the rig's). */
    RigCalibration baseOf(quint32 fid) const;

    /** Take @p fid's calibration as it is in the rig now as the active
     *  venue's correction -- or drop the correction when it is the show's
     *  own again -- and write the sidecar. False with no active venue, or
     *  when it cannot be written (@p why). */
    bool record(quint32 fid, QString *why = nullptr);
    /** Drop @p fid's correction: back to the show's calibration. A fixture
     *  no longer in the show (orphaned) is just forgotten. */
    bool clearCorrection(quint32 fid);

    bool hasCorrection(quint32 fid) const;
    RigCalibration correction(quint32 fid) const;
    int correctionCount() const;

    /*********************************************************************
     * Trims and targets (board Q128): a room's truss heights and stage
     * marks differ too. With a venue applied, moving a target or trimming
     * a truss (anywhere: the Lighting Studio, the pad's target page) goes
     * into the venue's file; the show keeps its own.
     *********************************************************************/
public:
    /** Take target @p id's position as it is now as the venue's (or drop
     *  it when it is the show's again) and write the sidecar. */
    bool recordTarget(quint32 id, QString *why = nullptr);
    /** The same for truss @p id's trim (its origin's height). */
    bool recordTrim(quint32 id, QString *why = nullptr);
    bool hasTargetOverride(quint32 id) const;
    bool hasTrimOverride(quint32 id) const;
    /** Put target @p id back where the show has it. */
    bool clearTarget(quint32 id);
    bool clearTrim(quint32 id);
    /** Compare every truss and target with what the venue (or the show)
     *  says and record what was moved meanwhile. */
    void syncStructure();

    /** Board Q133: lights whose correction was skipped when the venue was
     *  applied because the fixture is no longer what was focused (another
     *  definition or mode): they need focusing again here. */
    QList<quint32> needsFocus() const { return m_needsFocus; }
    bool needsFocus(quint32 fid) const { return m_needsFocus.contains(fid); }
    /** Corrections in the applied venue for fixtures no longer in the show:
     *  kept (the fixture may come back), listed. */
    QList<quint32> orphaned() const;
    /** @p fid is not the fixture venue @p name focused (another definition
     *  or mode now): swapped, in the focus report (Q134). */
    bool isSwapped(const QString &name, quint32 fid) const;
    /** "Spot 3 (Robe Robin 600, 16 bit)" -- how the venue remembers @p fid. */
    QString correctedAs(quint32 fid) const;

    /*********************************************************************
     * Checks and dead lights (board Q134, Q140)
     *********************************************************************/
public:
    /** Who is focusing ("Sam"), written with every check from now on. */
    void setCheckedBy(const QString &name) { m_checkedBy = name.trimmed(); }
    QString checkedBy() const { return m_checkedBy; }
    /** @p fid was saved in the active venue now (by checkedBy()): kept for
     *  the report, the sidecar written. A practice venue also counts the
     *  save. False with no venue applied. */
    bool markChecked(const QList<quint32> &fids);
    bool isChecked(quint32 fid) const;
    Check check(quint32 fid) const;
    /** Board Q140: mark @p fid dead in the active venue (or not). False
     *  with no venue applied. */
    bool setDead(quint32 fid, bool dead);
    bool isDead(quint32 fid) const;
    QList<quint32> deadLights() const;

    /*********************************************************************
     * Scramble practice (docs/design/FOCUS_REFINE_DESIGN.md section 13)
     *********************************************************************/
public:
    /** Make and apply a practice venue: each of @p fids (the moving heads
     *  among them) put off its right calibration -- the active venue's,
     *  else the show's -- by a random pan and tilt of between @p minDeg and
     *  @p maxDeg each way (@p seed makes it repeatable). The venue that was
     *  applied is not changed, and comes back when endPractice() is called.
     *  The practice venue's name, or empty when none of @p fids can be
     *  scrambled or there is no show file. */
    QString startPractice(const QList<quint32> &fids, quint32 seed, float minDeg = 2.0f,
                          float maxDeg = 8.0f, QString *why = nullptr);
    /** The applied venue is a practice one. */
    bool isPracticeActive() const;
    static bool isPractice(const Venue &v) { return v.practice.on; }
    /** A practice venue applied: @p fid's right calibration (the views draw
     *  where a scrambled light really lands from it). */
    bool practiceTruth(quint32 fid, RigCalibration &truth) const;
    /** How a practice went: for each scrambled light, how far its saved
     *  calibration is from the right one (degrees, pan and tilt together),
     *  the time from the start to the last save, and the saves. */
    struct Score
    {
        int lights = 0;
        int checked = 0;          ///< lights saved at least once
        int withinOne = 0;        ///< lights within 1 degree
        double meanError = 0.0;   ///< degrees
        double worstError = 0.0;
        double seconds = 0.0;
        int saves = 0;
        QMap<quint32, double> errors;
    };
    Score practiceScore(const QString &name) const;
    /** The error left on @p fid in practice venue @p name, degrees; -1 when
     *  it was not scrambled. */
    double practiceError(const QString &name, quint32 fid) const;
    /** Put back the venue the practice was scrambled from (its results
     *  stay in the practice venue, for the report). */
    bool endPractice();

    /*********************************************************************
     * What the venue owns (board Q137/Q138, docs/design/VENUES.md): its house rig, the
     * patch of every slot in this room, the channels that are not ours.
     *********************************************************************/
public:
    VenueRig *rig() const { return m_rig; }
    /** @p id is a house fixture of the venue applied: the show file does
     *  not take it. */
    bool isVenueFixture(quint32 id) const { return m_rig->isVenueFixture(id); }
    /** A house fixture of some venue uses @p id: a new show fixture skips it. */
    bool isReservedFixtureId(quint32 id) const { return m_rig->isReservedId(id); }
    /** @p address (zero-based) in @p universe is kept by the venue applied. */
    bool isNotOurs(quint32 universe, quint32 address) const;

    /** The house rig as it is now into the venue applied, written when it
     *  changed. True when nothing needed writing or it was written. */
    bool recordRig(QString *why = nullptr);
    /** Take the show's House fixtures (set House with no venue applied)
     *  into the venue applied; how many moved. */
    int moveShowHouseIn();

    /** The output patch on the rig now becomes the venue applied's, for
     *  every slot (a patch changed while it is applied does this too). */
    bool keepPatch(QString *why = nullptr);
    /** The venue applied forgets its patch: the show's own goes back on. */
    bool forgetPatch(QString *why = nullptr);
    bool hasPatch(const QString &name) const { return !venue(name).rig.patch.isEmpty(); }

    /** What @p name keeps for the room; applied at once if it is on. */
    bool setNotOurs(const QString &name, const QList<VenueRig::NotOurs> &notOurs, QString *why = nullptr);
    QList<VenueRig::NotOurs> notOurs(const QString &name) const { return venue(name).rig.notOurs; }

    /** What going live with @p name would clash on (docs/design/VENUES.md section 6):
     *  a dry run unless it is the venue applied. */
    QList<VenueRig::Clash> clashes(const QString &name) const;

    /** The doc was cleared: forget everything without touching the rig. */
    void reset();

    /** Fixtures whose calibration is moving under a tool that records it
     *  itself on a save (Focus mode's heads while it runs): a live,
     *  unsaved nudge is not a correction. Any other change to a fixture's
     *  calibration while a venue is applied -- the fixture dialog, say --
     *  is recorded into the venue at once (board Q111). */
    void setHeld(const QList<quint32> &fids);

signals:
    void activeVenueChanged(const QString &name);
    void venuesChanged();
    /** confirmVenue(): "Still at <venue>?" answered (the venue chip). */
    void confirmedChanged();
    void correctionsChanged();
    /** Something the operator should read once, about the venue applied
     *  (a file from before definitions were kept). */
    void note(const QString &text);
    /** The house rig, the patch or what is not ours changed. */
    void houseRigChanged();

private:
    void apply();
    void unapply();
    void writeRig(quint32 fid, const RigCalibration &c);
    void onRigPropsChanged(quint32 fid);

private:
    Doc *m_doc;
    QString m_showFile;
    QList<Venue> m_venues;
    /** Elements between the venues this build doesn't know, kept. */
    QList<QByteArray> m_unknown;
    QString m_active;
    bool m_writing = false;
    /** The applied venue was chosen, or said to be still the room, since
     *  the show was opened. */
    bool m_confirmed = false;
    QSet<quint32> m_held;
    QList<quint32> m_needsFocus;
    QString m_checkedBy;
    /** What @p fid is now: its definition and mode. */
    void identify(quint32 fid, QString &maker, QString &model, QString &mode) const;
    Venue *activeVenueData();
    void updateReserved();
    /** @p fid's calibration is kept in its house-rig entry, not as a
     *  correction: a house fixture, unless a practice venue is applied. */
    bool houseByRig(quint32 fid) const;

    VenueRig *m_rig = nullptr;
    class QTimer *m_rigTimer = nullptr;
};

/** @} */

#endif
