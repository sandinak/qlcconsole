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
#include <QSet>
#include <QSharedPointer>

#include "qlcengine_export.h"
#include "qlcpalette.h"

class Doc;
class Scene;
class Function;

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
    /** Delete, and clear every use (open 26): scenes lose that look,
     *  effect slots their palette (as Doc::deletePalette), and the Show
     *  Tree's cues, layers, looks and reusables lose it too -- a look left
     *  without a palette goes; the tree bakes once. Undo brings it back
     *  with its id, in every scene, slot and look it was in, in its place;
     *  redo deletes again. */
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
    /** The palettes the scenes and functions held by this history's steps
     *  play (either side, undo and redo): a Show Tree one-off a scene step
     *  took out of a scene stays while an undo or redo can put it back. */
    QSet<quint32> palettesInHistory() const;

    /************************************************************************
     * Scene steps (PROGRAMMING_REDESIGN.md P1): the Programming tab's scene
     * edits -- targets and looks added, moved, removed, reordered, renamed,
     * timing, raw values -- on this same stack, so they undo AND redo with
     * everything else, from either tab.
     ************************************************************************/
public:
    /** Scene @a sceneId as it is now, detached (owned by nobody but the
     *  pointer); null if there is no such scene. */
    QSharedPointer<Scene> sceneSnapshot(quint32 sceneId) const;
    /** One step for scene edits already made: @a before holds each touched
     *  scene as it was (sceneSnapshot()), the after side is taken now. A
     *  scene that didn't change is dropped; nothing changed, no step (false).
     *  A non-empty @a mergeKey equal to the top step's, within 1.5 s, extends
     *  that step instead (a fader drag is one step). */
    bool recordSceneStep(const QString &label, const QList<QSharedPointer<Scene>> &before,
                         const QString &mergeKey = QString());
    /** Run @a change over scenes @a sceneIds as one step (recordSceneStep). */
    bool sceneStep(const QString &label, const QList<quint32> &sceneIds,
                   const std::function<bool()> &change);

    /************************************************************************
     * Value steps: the Look Editor's edits to a palette's value -- and the
     * name that follows it (PaletteNaming) -- as one step.
     ************************************************************************/
public:
    /** Run @a change over palette @a paletteId's value (and name) as one
     *  step. Undo puts back the value, wheel slot, gobo name and values and
     *  the name it had. A non-empty @a mergeKey equal to the top step's,
     *  within 1.5 s, extends that step (a picker drag is one step). A
     *  palette the tree owns (isManaged() false) is changed with no step.
     *  Nothing changed: no step (false). */
    bool valueStep(const QString &label, quint32 paletteId, const std::function<void()> &change,
                   const QString &mergeKey = QString());

    /************************************************************************
     * Function steps (WORKFLOW_DESIGN.md W1): the Workbench's edits to a
     * collection's members -- added, removed, reordered, a new EFX or RGB
     * Matrix made on a layer -- on this same stack.
     ************************************************************************/
public:
    /** Run @a change over functions @a functionIds (any type) as one step.
     *  Each is kept whole on either side; a function @a change makes is
     *  appended to @a made, so Undo deletes it and Redo brings it back with
     *  its id. Nothing changed: no step (false). */
    bool functionStep(const QString &label, const QList<quint32> &functionIds,
                      const std::function<bool(QList<quint32> &made)> &change);
    /** What the last step changed outside the functions it keeps (a VC
     *  button's or cue list's function, the main cue list): @a outside runs
     *  after that step is undone (true) or redone (false), so a delete's
     *  Undo puts back what pointed at the functions it brings back. */
    void attachToLastStep(const std::function<void(bool undone)> &outside);

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
        /** A value step: apply() puts these back too (other steps leave the
         *  value as it is, edited since or not). */
        bool value = false;
        QVariantList values;
        QString colourMacro;
        QString goboName;
        QMap<QString, int> goboValues;
    };
    struct Step
    {
        QString label;
        quint64 stamp = 0;
        QList<State> before;
        QList<State> after;
        QStringList foldersBefore;
        QStringList foldersAfter;
        /** A delete that cleared the tree's uses: the tree on either side
         *  (ShowTreeController::outsideUndoState()); empty: untouched. */
        QByteArray treeBefore;
        QByteArray treeAfter;
        /** A scene step: each touched scene on either side (the snapshot's
         *  own id is invalid, so the scene's id is kept beside it). */
        QList<QPair<quint32, QSharedPointer<Scene>>> scenesBefore;
        QList<QPair<quint32, QSharedPointer<Scene>>> scenesAfter;
        QString mergeKey;
        qint64 mergeAt = 0;     //!< when it was last extended (ms since epoch)
        /** A function step: each touched function on either side (a null
         *  snapshot: it didn't exist). */
        bool functions = false;
        QList<QPair<quint32, QSharedPointer<Function>>> functionsBefore;
        QList<QPair<quint32, QSharedPointer<Function>>> functionsAfter;
        /** attachToLastStep(): run after the step is undone (true) or
         *  redone (false). */
        std::function<void(bool undone)> outside;
    };
    /** Function @a id as it is now, detached; null if there is none. */
    QSharedPointer<Function> functionSnapshot(quint32 id) const;
    /** Put the functions back as @a states hold them: made, changed back,
     *  deleted. */
    void applyFunctions(const QList<QPair<quint32, QSharedPointer<Function>>> &states);
    /** Put the scenes back as @a states hold them. */
    void applyScenes(const QList<QPair<quint32, QSharedPointer<Scene>>> &states,
                     const QList<QPair<quint32, QSharedPointer<Scene>>> &other);
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
