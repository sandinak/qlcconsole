/*
  Q Light Controller Plus
  show.h

  Copyright (c) Massimo Callegari

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

#ifndef SHOW_H
#define SHOW_H

#include <QMutex>
#include <QList>
#include <QSet>
#include <QMap>
#include <QColor>

#include "function.h"
#include "track.h"

class QXmlStreamReader;
class ShowRunner;

/** A timeline section marker: a labelled, coloured [start, end] region.
 *  Optionally links a MANUAL cue list (a Chaser) that covers this off-clock
 *  section, so the run-of-show knows which GO stack to fire here — the timecode
 *  ↔ manual-cue seam. cueListId == Function::invalidId() means "no link". */
struct ShowMarker
{
    quint32 end;
    QString label;
    QColor color;
    quint32 cueListId;
    ShowMarker(quint32 e = 0, const QString &l = QString(), const QColor &c = QColor(),
               quint32 cue = Function::invalidId())
        : end(e), label(l), color(c), cueListId(cue) {}
};

/** @addtogroup engine_functions Functions
 * @{
 */

class Show final : public Function
{
    Q_OBJECT
    Q_DISABLE_COPY(Show)

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    Show(Doc* doc);
    virtual ~Show();

    /** @reimp */
    QIcon getIcon() const override;

    /** @reimp */
    quint32 totalDuration() override;

    /*********************************************************************
     * Copying
     *********************************************************************/
public:
    /** @reimp */
    Function* createCopy(Doc* doc, bool addToDoc = true) override;

    /** Copy the contents for this function from another function */
    bool copyFrom(const Function* function) override;

    /*********************************************************************
     * Time division
     *********************************************************************/
public:
    enum TimeDivision
    {
        Time = 0,
        BPM_4_4,
        BPM_3_4,
        BPM_2_4,
        Invalid
    };
    Q_ENUM(TimeDivision)

    /** Set the show time division type (Time, BPM) */
    void setTimeDivision(Show::TimeDivision type, int BPM);

    Show::TimeDivision timeDivisionType();
    void setTimeDivisionType(Show::TimeDivision type);
    int beatsDivision();

    int timeDivisionBPM();
    void setTimeDivisionBPM(int BPM);

    static QString tempoToString(Show::TimeDivision type);
    static Show::TimeDivision stringToTempo(QString tempo);

private:
    TimeDivision m_timeDivisionType;
    int m_timeDivisionBPM;

    /*********************************************************************
     * Tracks
     *********************************************************************/
public:
    /**
     * Add a track to this show. If the track is already a
     * member of the show, this call fails.
     *
     * @param id The track to add
     * @return true if successful, otherwise false
     */
    bool addTrack(Track *track, quint32 id = Track::invalidId());

    /**
     * Remove a track from this show. If the track is not a
     * member of the show, this call fails.
     *
     * @param id The track to remove
     * @return true if successful, otherwise false
     */
    bool removeTrack(quint32 id);

    /** Get a track by id */
    Track *track(quint32 id) const;

    /** Get a reference to a Track from the provided Scene ID */
    Track *getTrackFromSceneID(quint32 id);

    /** Get a reference to a Track from the provided ShowFunction ID */
    Track *getTrackFromShowFunctionID(quint32 id);

    /** Get the number of tracks in the Show */
    int getTracksCount();

    /** Move a track ID up or down */
    void moveTrack(Track *track, int direction);

    /** Get a list of available tracks */
    QList <Track*> tracks() const;

    /** Set a track's intensity submaster (0..1) and, if the show is running,
     *  apply it live to that track's output. Persists on the Track. */
    void setTrackIntensity(quint32 trackId, qreal fraction);

private:
    /** Create a new track ID */
    quint32 createTrackId();

protected:
    /** Map of the available tracks coupled by ID */
    QMap <quint32,Track*> m_tracks;

    /** Guards m_tracks: the ShowRunner walks tracks() every frame on the timer
     *  thread while the UI adds/removes/moves on the main thread. Recursive so a
     *  locked accessor can call another. */
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    mutable QMutex m_tracksMutex;
#else
    mutable QRecursiveMutex m_tracksMutex;
#endif

    /** Latest assigned track ID */
    quint32 m_latestTrackId;

    /*********************************************************************
     * Show Functions
     *********************************************************************/
public:
    /** Get a unique ID for the creation of a new ShowFunction */
    quint32 getLatestShowFunctionId();

    /** Get a reference to a ShowFunction from the provided uinique ID */
    ShowFunction *showFunction(quint32 id);

protected:
    /** Latest assigned unique ShowFunction ID */
    quint32 m_latestShowFunctionID;

    /*********************************************************************
     * Markers (timeline section labels)
     *********************************************************************/
public:
    /** Add or update a section marker spanning [start, end] ms with a label
     *  and colour, keyed by its start time. An empty label removes it. An
     *  existing marker's linked cue list is PRESERVED across relabel/recolour
     *  edits (use setMarkerCueList to change the link). */
    void setMarker(quint32 start, quint32 end, const QString &label,
                   const QColor &color = QColor());

    /** Link (or clear, with invalidId) the manual cue list covering the marker
     *  at @p start. No-op if there is no marker there. */
    void setMarkerCueList(quint32 start, quint32 cueListId);

    /** The cue list linked to the marker at @p start, or invalidId if none. */
    quint32 markerCueList(quint32 start) const;

    /** Remove the marker whose start time is the given value, if any. */
    void removeMarker(quint32 start);

    /** All markers as start(ms) -> ShowMarker, sorted by start. */
    QMap<quint32, ShowMarker> markers() const;

private:
    QMap<quint32, ShowMarker> m_markers;

    /*********************************************************************
     * Save & Load
     *********************************************************************/
public:
    /** Save function's contents to an XML document */
    bool saveXML(QXmlStreamWriter *doc) const override;

    /** Load function's contents from an XML document */
    bool loadXML(QXmlStreamReader &root) override;

    /** @reimp */
    void postLoad() override;

public:
    /** @reimp */
    bool contains(quint32 functionId) const override;

    /** @reimp */
    QList<quint32> components() const override;

    /*********************************************************************
     * Running
     *********************************************************************/
public:
    /** @reimp */
    void preRun(MasterTimer* timer) override;

    /** @reimp */
    void setPause(bool enable) override;

    /** @reimp */
    void write(MasterTimer* timer, QList<Universe*> universes) override;

    /** @reimp */
    void postRun(MasterTimer* timer, QList<Universe*> universes) override;

    /*********************************************************************
     * Timecode follow (e.g. MIDI Time Code)
     *********************************************************************/
public:
    /** Enable/disable following an external absolute timecode. Persisted so
     *  a show remembers whether it is timecode-driven. */
    void setTimecodeFollow(bool enable);
    bool timecodeFollow() const;

    /** Push the current external absolute position (ms) to the runner. */
    void setExternalTime(quint32 ms);

    /** Suspend/resume timeline OUTPUT while keeping the playhead position
     *  (Operate-mode VC takeover). Runtime-only, not persisted. */
    void setTimelineSuspended(bool enable);
    bool isTimelineSuspended() const;

    /** Timecode value (ms) that maps to timeline position 0. Incoming timecode
     *  is offset by this, so a show Logic drives at SMPTE 01:00:00:00 lines up
     *  with a 0-based timeline. Persisted. */
    void setTimecodeOffset(quint32 ms);
    quint32 timecodeOffset() const;

protected slots:
    /** Called whenever one of this function's child functions stops */
    void slotChildStopped(quint32 fid);

signals:
    void timeChanged(quint32);
    void showFinished();

protected:
    ShowRunner *m_runner;
    /** Guards m_runner's lifetime: the UI thread (setTrackIntensity /
     *  adjustAttribute / setExternalTime / setPause / setTimecodeFollow /
     *  setTimelineSuspended) reads it while the timer thread creates/deletes it
     *  in preRun/postRun. write() shares the timer thread with pre/postRun so it
     *  needs no lock. mutable: const isTimelineSuspended() must lock it too. */
    mutable QMutex m_runnerMutex;
    /** Number of currently running children */
    QSet <quint32> m_runningChildren;
    /** Whether this show follows an external timecode (persisted) */
    bool m_timecodeFollow;
    /** Timecode-to-timeline offset in ms (persisted) */
    quint32 m_timecodeOffset;

    /*************************************************************************
     * Attributes
     *************************************************************************/
public:
    /** @reimp */
    int adjustAttribute(qreal fraction, int attributeId = 0) override;
};

/** @} */

#endif
