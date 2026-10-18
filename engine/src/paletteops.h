/*
  Q Light Controller Plus
  paletteops.h

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

#ifndef PALETTEOPS_H
#define PALETTEOPS_H

#include <QStringList>
#include <functional>
#include <QByteArray>
#include <QObject>
#include <QList>
#include <QHash>

#include "qlcengine_export.h"
#include "qlcpalette.h"

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * The everyday palette operations, one source of truth for every place
 * that offers them (PROGRAMMING_ROLE_DESIGN.md §5, P-D8): the Show Tree's
 * Library and command line, and the Programming tab's palette tree. Create
 * by type, rename, duplicate, file in a folder, folders of their own,
 * delete -- each one undo step on ONE palette undo stack (P-D14), which
 * either tab's Undo reaches -- and where a palette is used (P-D13): cues,
 * busking looks, reusable layers and cues, plain scenes, effect slots. The
 * VC widgets that fire those are added by the UI (it owns the VC).
 *
 * Folders are type-independent (as feat/palette-folders makes them): a
 * folder "01-Song" holds colours, dimmers and effects alike. A path stored
 * the older way, with the palette type as its first folder
 * ("Palettes/Color/01-Song/"), is read as the folder "01-Song"; anything
 * filed here is stored the new way ("Palettes/01-Song/").
 *
 * Owned by Doc (Doc::paletteOps()).
 */
class QLC_ENGINE_EXPORT PaletteOps final : public QObject
{
    Q_OBJECT

public:
    explicit PaletteOps(Doc *doc);
    ~PaletteOps();

    /************************************************************************
     * Folders
     ************************************************************************/
public:
    /** The folder palette @a p is filed in ("01-Song", "01-Song/Verse"; ""
     *  none), whichever way its path was stored. */
    static QString folderOf(const QLCPalette *p);
    static QString folderOfPath(const QString &path, QLCPalette::PaletteType type);
    /** The path a palette in @a folder is stored with ("Palettes/01-Song/";
     *  "" for none). */
    static QString pathForFolder(const QString &folder);
    /** A folder name made tidy: no empty parts, no spaces round them. */
    static QString cleanFolder(const QString &folder);

    /** Every folder in use (and the empty ones made with addFolder()),
     *  parents too, sorted. */
    QStringList folders() const;
    /** A folder with nothing in it yet (kept until the show closes, or
     *  something is filed there). One undo step. */
    bool addFolder(const QString &folder);
    /** Rename @a folder's last part: every palette in it and under it, of
     *  any type, follows. One undo step. */
    bool renameFolder(const QString &folder, const QString &name);
    /** Delete @a folder: what is in it moves up one level; nothing is
     *  deleted. One undo step. */
    bool removeFolder(const QString &folder);
    /** How many palettes are in @a folder and under it. */
    int folderCount(const QString &folder) const;

    /************************************************************************
     * Palettes: each one undo step
     ************************************************************************/
public:
    /** The value a new palette of @a p's type starts with, so it is valid
     *  and saves (white, full, centre...). */
    static void setStartValue(QLCPalette *p);
    /** "New Color", "New Color 2"... a name no palette of @a type has. */
    QString newName(QLCPalette::PaletteType type, const QString &base = QString()) const;

    /** A new palette of @a type stored at @a path (a raw path, as
     *  pathForFolder() makes or the Programming tab's tree gives), named
     *  @a name (empty: newName()), playing @a values (empty: the type's
     *  start value). Invalid: refused. */
    quint32 create(QLCPalette::PaletteType type, const QString &path, const QString &name = QString(),
                   const QVariantList &values = QVariantList());
    /** A copy, "<name> (copy)", in the same folder. */
    quint32 duplicate(quint32 paletteId);
    bool rename(quint32 paletteId, const QString &name);
    /** File @a paletteIds in @a folder ("" none). */
    bool move(const QList<quint32> &paletteIds, const QString &folder);
    /** Store @a paletteIds at the raw @a path (the Programming tab's "Move to
     *  folder…", which takes a path). */
    bool setPath(const QList<quint32> &paletteIds, const QString &path);
    /** Delete: scenes lose that look, effect slots their palette (as
     *  Doc::deletePalette); cues and looks of the Show Tree keep pointing at
     *  it ("(missing)") until it comes back. Undo brings it back with its
     *  id, in every scene and slot it was in. */
    bool remove(const QList<quint32> &paletteIds);

    /** Palettes the operations here leave alone (the Show Tree's one-off
     *  values, D21): they are the tree's, changed through it. */
    bool isManaged(quint32 paletteId) const;

    /************************************************************************
     * Where a palette is used (P-D13)
     ************************************************************************/
public:
    struct Use
    {
        enum Kind
        {
            Cue,            //!< a cue of a cue list (owner: the cue)
            Look,           //!< a busking look (owner: the look)
            Reusable,       //!< a reusable layer or cue (owner: the reusable)
            Scene,          //!< a plain scene's look (owner: the scene)
            EffectSlot      //!< an effect look's slot (owner: that palette)
        };
        Kind kind = Cue;
        quint32 owner = 0xFFFFFFFF;
        quint32 layer = 0xFFFFFFFF;     //!< a cue's / look's layer (invalid: the cue look)
        /** The function that plays it -- a cue's or look's Collection, the
         *  scene -- for the VC widgets firing it (invalid: none). */
        quint32 function = 0xFFFFFFFF;
        QString label;                  //!< "Cue 0.0.4 SHOUT in Full Show — Towers"
    };
    /** Every place @a paletteId is set, in tree order, then scenes, then
     *  slots. A generated scene is never listed: its cue (or look) is. */
    QList<Use> usage(quint32 paletteId) const;
    /** "6 cues, 2 looks, 1 scene" (empty: unused). */
    static QString usageSummary(const QList<Use> &uses);

    /************************************************************************
     * Undo: one palette stack, shared by both tabs (P-D14)
     ************************************************************************/
public:
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    QString undoText() const;
    QString redoText() const;
    bool undo();
    bool redo();
    void clearUndo();
    void clearRedo() { m_redo.clear(); }
    /** When the top step was taken (0: none), against every other stack
     *  stamped by nextUndoStamp(): the Show Tree's Undo undoes whichever
     *  was taken last. */
    quint64 undoStamp() const;
    quint64 redoStamp() const;
    /** A stamp for an undo step of any stack: later steps, larger stamps. */
    static quint64 nextUndoStamp();

    /** Forget the empty folders and the undo steps (a new show). */
    void reset();

signals:
    /** Palettes, folders or the undo stack changed. */
    void changed();

private:
    /** A palette as it was (or wasn't) at one end of a step. */
    struct State
    {
        quint32 id = 0xFFFFFFFF;
        bool exists = false;
        QByteArray xml;                         //!< the whole palette (to bring it back)
        QString name;
        QString path;
        /** Where it was used when it went: scene -> its place in the
         *  scene's looks; palette -> slots binding it. */
        QList<QPair<quint32, int>> scenes;
        QList<QPair<quint32, QString>> bindings;
    };
    struct Step
    {
        QString label;
        quint64 stamp = 0;
        QList<State> before;
        QList<State> after;
        QStringList foldersBefore;
        QStringList foldersAfter;
    };
    State capture(quint32 id) const;
    void apply(const QList<State> &states, const QStringList &folders);
    /** Run @a change as one undo step named @a label, recording the
     *  palettes @a ids (and any it makes, appended to @a ids). */
    bool step(const QString &label, QList<quint32> ids, const std::function<bool(QList<quint32> &)> &change);

    Doc *m_doc;
    QStringList m_emptyFolders;
    QList<Step> m_undo;
    QList<Step> m_redo;
    static const int kUndoLimit = 200;
};

/** @} */

#endif
