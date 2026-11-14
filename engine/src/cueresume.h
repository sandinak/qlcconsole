/*
  Q Light Controller Plus
  cueresume.h

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

#ifndef CUERESUME_H
#define CUERESUME_H

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QList>
#include <QHash>
#include <QMap>

#include "qlcengine_export.h"
#include "showcontrol.h"

class QTimer;
class Chaser;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Resume after a crash (0.4.8, crash-resume-cue): which cue each cue list
 * was on, kept where a crash, a force-quit or a power cut leaves it, so
 * that opening the same show again can offer it back as standby.
 *
 * While the show runs in Operate, every GO and Back that is carried out
 * (ShowControl::cueTaken()) and every Stop, Reset and All Off
 * (ShowControl::cueListStopped()) rewrites a small file of its own -- the
 * show's path, and per list its cue -- once per command, never per frame,
 * after the command has acted. Never the show file itself.
 *
 * A clean close (another show opened, a new one, quitting) removes it. So
 * when the app starts and the file is there, the last session ended without
 * one; opening that same show arms the offer, which the Run tab shows in
 * Operate only. Load as Standby LOADs each cue through the one run path
 * (ShowControl::run(), RouteRunTab): nothing plays, the stage stays as it
 * is, and the next GO plays it. Nothing here ever plays a cue: the
 * operator's rule is that nothing moves the show without a GO.
 *
 * A cue is found again by its Show Tree id (a plain chaser: its number), so
 * a cue that was deleted is never offered, even when another now has its
 * number: the offer says it is no longer in the show.
 */
class QLC_ENGINE_EXPORT CueResume final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(CueResume)

public:
    /** Kept in @a path (empty: defaultPath()). What a previous session
     *  left there is read when a show is opened (showOpened()). */
    explicit CueResume(Doc *doc, const QString &path = QString(), QObject *parent = nullptr);
    ~CueResume();

    /** The one kept for @a doc (the App's; null: none). */
    static CueResume *of(const Doc *doc);

    /** The user folder's resume-cue.json, beside the untitled autosave. */
    static QString defaultPath();
    QString path() const { return m_path; }

    /** One list's cue. */
    struct Entry
    {
        quint32 chaser = UINT_MAX;      //!< the chaser that played it
        quint32 list = UINT_MAX;        //!< its Show Tree list (invalid: a plain chaser)
        quint32 cue = UINT_MAX;         //!< the Show Tree cue (or sub-cue); invalid: by number
        QString number;                 //!< "2.3.1" as show control names it
        QString listName;               //!< "Main"
    };

    /*********************************************************************
     * What the App tells it
     *********************************************************************/
public:
    /** The show's file now (opened, saved as): what a record is kept for. */
    void setShowFile(const QString &path);
    QString showFile() const { return m_showFile; }
    /** Show @a path has been opened (the file, or its recovered autosave):
     *  when the last session left a record for it, the offer is armed. */
    void showOpened(const QString &path);
    /** The show is closing cleanly (another opened, File ▸ New, quit):
     *  its record goes, and with it any offer. */
    void showClosed();

    /*********************************************************************
     * The offer
     *********************************************************************/
public:
    /** A record of the show open now, from a session that ended without a
     *  clean close, not yet loaded, dismissed or overtaken by a command. */
    bool hasOffer() const { return m_armed; }
    /** The Run tab shows it: hasOffer() and the show is in Operate. */
    bool offerShown() const;
    /** The lists' cues, as recorded. */
    QList<Entry> offer() const { return m_armed ? m_pending : QList<Entry>(); }
    /** When it was last written. */
    QDateTime offerTime() const { return m_pendingTime; }

    /** Where entry @a e's cue is now: its chaser, step and sub-cue (-1: the
     *  cue itself). False when the list or the cue is no longer there. */
    bool resolve(const Entry &e, Chaser **chaser, int *step, int *subCue) const;
    /** How many entries resolve() finds now. */
    int loadableCount() const;

    /** What the Run tab says: "The show was at cue 2.3.1 (Main) when it
     *  stopped. Load it?" -- and what is no longer there. Empty: no offer. */
    QString offerText() const;
    /** The tooltip: when, and what Load as Standby does. */
    QString offerDetail() const;

    /** Load as Standby: each cue that is still there is LOADed through
     *  ShowControl::run() (RouteRunTab) -- nothing plays. The offer goes;
     *  the record stays until the next GO rewrites it (a crash before then
     *  offers it again). The count LOADed. */
    int accept();
    /** Dismiss: the offer goes, and so does the record. */
    void dismiss();

    /*********************************************************************
     * What is written
     *********************************************************************/
public:
    /** The lists' cues recorded this session. */
    QList<Entry> current() const { return m_current.values(); }
    /** How many times the file has been written (or removed) this session:
     *  once per command, never per frame. For the tests. */
    int writes() const { return m_writes; }
    /** Write a pending change now (it is otherwise written as soon as the
     *  command that made it is done). */
    void flush();

signals:
    /** The offer came, went, or its show's mode changed. */
    void offerChanged();
    /** Load as Standby LOADed @a count cues (for the console's notice). */
    void loaded(int count);

private slots:
    void slotCueTaken(ShowControl::Command command, quint32 chaserId, int fromStep, int fromSub,
                      int step, int sub);
    void slotListStopped(quint32 chaserId);

private:
    static QHash<const Doc *, CueResume *> &registry();
    /** A command was carried out on the stage: the offer it overtakes goes. */
    void overtaken();
    void scheduleWrite();
    void write();
    void removeFile();
    void readFile();
    void setArmed(bool armed);
    QString cueWords(const Entry &e) const;

private:
    Doc *m_doc;
    QString m_path;
    QString m_showFile;
    /** What the last session left: its show and lists (read at start). */
    QString m_pendingShow;
    QList<Entry> m_pending;
    QDateTime m_pendingTime;
    bool m_armed = false;
    /** This session's lists, by chaser. */
    QMap<quint32, Entry> m_current;
    /** The show's main cue list when the last cue was taken: said first. */
    quint32 m_mainChaser = UINT_MAX;
    /** The file is this session's to remove on a clean close: written by
     *  it, or the record of the show it opened. */
    bool m_owns = false;
    QTimer *m_writeTimer;
    int m_writes = 0;
};

/** @} */

#endif
