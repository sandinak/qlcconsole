/*
  Q Light Controller Plus
  cuesheet.h

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

#ifndef CUESHEET_H
#define CUESHEET_H

#include <QStringList>
#include <QByteArray>
#include <QString>
#include <QList>

#include "qlcengine_export.h"

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * The stage manager's paper, derived from the Show Tree on demand (nothing
 * here is stored):
 *
 *  - the cue sheet: a row per cue in the order the Run plays them
 *    (production, cue list, song section, cue) with its number, name,
 *    fades, how it goes on, notes, and the hits armed at it;
 *  - the tracking sheet: a row per layer (the groups and fixtures it
 *    lights), a column per cue; a cell holds the values the layer plays in
 *    that cue, those the cue changes plainly and those only carried in
 *    from an earlier cue marked as tracked. It reads
 *    ShowTree::resolve() through ShowTreeController::resolved(), the same
 *    cascade the Run plays, not a resolver of its own.
 *
 * Both go out as CSV, XLSX and a PDF made from toHtml()-style pages, as the
 * patch list does.
 */
class QLC_ENGINE_EXPORT CueSheet
{
public:
    /** One row of the cue sheet. */
    struct Row
    {
        enum Kind { Section, Cue, SubCue };
        Kind kind = Cue;
        quint32 cueId = 0xFFFFFFFF;     //!< invalid for a Section
        QString section;                //!< "1 — Everybody Scream"; the section it is in
        QString number;                 //!< "1.0.2", a sub-cue's "4.1"; a Section's title is in @a name
        QString name;
        QString fadeIn;                 //!< "2s"
        QString fadeOut;
        QString follow;                 //!< "GO", "auto after 4s", a sub-cue's "Sub GO"
        QString note;
        QString hits;                   //!< the hits under this cue, ", "-separated
        bool    skipped = false;        //!< left out of GO (Cue::skip), or its song is tonight
        bool    needsWork = false;      //!< tagged Needs work (Cue::needsWork), from the stage or the cue page
    };

    /** A cue list of the production, in running order. */
    struct ListBlock
    {
        quint32 listId = 0xFFFFFFFF;
        QString name;
        /** The hits armed all the time here: this list's, the
         *  production's and the global ones. */
        QStringList hitsThroughout;
        QList<Row> rows;
    };

    /** One value of a tracking cell. */
    struct Value
    {
        QString text;                   //!< "Red", "Intensity 45%", "gobo Breakup", "out"
        bool changed = false;           //!< set in this cue; false: tracked in from before
    };

    struct Cell
    {
        QList<Value> values;
        bool isEmpty() const { return values.isEmpty(); }
        /** Everything in it is tracked, nothing changes here. */
        bool allTracked() const;
    };

    struct TrackingColumn
    {
        quint32 cueId = 0xFFFFFFFF;
        QString number;
        QString name;
        bool skipped = false;
    };

    struct TrackingRow
    {
        QString label;                  //!< the layer's name or its groups / fixtures; indented per depth
        QList<Cell> cells;              //!< one per column
    };

    /** A cue list's tracking sheet. */
    struct TrackingBlock
    {
        quint32 listId = 0xFFFFFFFF;
        QString name;
        QList<TrackingColumn> columns;  //!< the cues, in running order
        QList<TrackingRow> rows;
    };

    /** The sheets of every playable cue list (no Looks section, no
     *  sub-cue list) of production @a productionId. */
    static CueSheet build(Doc *doc, quint32 productionId);

    QString productionName() const { return m_production; }
    const QList<ListBlock> &lists() const { return m_lists; }
    const QList<TrackingBlock> &tracking() const { return m_tracking; }
    bool isEmpty() const { return m_lists.isEmpty(); }
    int cueCount() const;

    /** The column titles of the cue sheet. */
    static QStringList header();
    /** The cue sheet as a table: the header, then a line per cue and
     *  sub-cue (the cue list and section in columns of their own, so it
     *  sorts and filters). */
    QList<QStringList> cueTable() const;
    /** The tracking sheets stacked: per list a title line, a header (the
     *  cue numbers), a line per layer, a blank line. A tracked value reads
     *  "(Red)", one this cue changes plain. */
    QList<QStringList> trackingTable() const;

    QString cueCsv() const;
    QString trackingCsv() const;
    QByteArray cueXlsx() const;
    QByteArray trackingXlsx() const;
    bool writeCueXlsx(const QString &path, QString *error = nullptr) const;
    bool writeTrackingXlsx(const QString &path, QString *error = nullptr) const;

    /** The printable pages (what the PDF is made from): a table per list
     *  with a heading per song; the tracking sheet's cells show the values
     *  a cue changes plain and the tracked ones grey, in brackets. */
    QString cueHtml(const QString &title) const;
    QString trackingHtml(const QString &title, int cuesPerPage = 8) const;
    /** Write the PDF to @p path (landscape Letter). False, with @p error, if
     *  the file can't be written. */
    bool writeCuePdf(const QString &path, const QString &title, QString *error = nullptr) const;
    bool writeTrackingPdf(const QString &path, const QString &title, QString *error = nullptr) const;

    /** A tracking cell as text: "Red · Intensity 45%", tracked ones in
     *  brackets, "(Red) · Intensity 45%". */
    static QString cellText(const Cell &cell);

private:
    static bool writeFile(const QString &path, const QByteArray &data, QString *error);
    static bool writePdf(const QString &path, const QString &title, const QString &html, QString *error);

    QString m_production;
    QList<ListBlock> m_lists;
    QList<TrackingBlock> m_tracking;
};

/** @} */

#endif
