/*
  Q Light Controller Plus
  contextmenus.h

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

#ifndef CONTEXTMENUS_H
#define CONTEXTMENUS_H

#include <QCoreApplication>
#include <QPointer>
#include <QString>
#include <QList>
#include <QMap>
#include <QMenu>
#include <functional>

#include "qlcui_export.h"

class QAction;
class QWidget;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * One layout for every right-click menu (docs/design/CONTEXT_MENUS.md).
 *
 * A menu is two blocks. On top, what only this place offers -- the
 * context: Set to ▸ on a look row, Remove from Truss on the plot, Pin in
 * the Library -- and the list's own view. Then one separator, and the
 * object's own kind menu, in a fixed order and the same wherever the object
 * is right-clicked: its editor (the default action), what its kind does,
 * selecting more like it, organising it, clipboard and name, and deleting
 * it last. Empty sections are left out; one separator stands between two
 * sections. One builder per kind (ContextMenus below) fills the kind block,
 * so a palette, a function, a fixture group or a fixture has the same items
 * with the same words in every place.
 */
class QLC_UI_EXPORT ContextMenu
{
    Q_DECLARE_TR_FUNCTIONS(ContextMenu)

public:
    /** Decided 2026-11-03 (user): place-specific items on top, then the
     *  kind menu. The two context sections come first; Open..Remove is the
     *  kind block. */
    enum Section
    {
        Place = 1,      ///< what only this place offers -- first
        View,           ///< the list itself (expand, collapse, group by)
        Open,           ///< its own editor; the kind block's first, the default action
        Kind,           ///< what this kind of thing does
        Selection,      ///< picking out more like it
        Organise,       ///< making something from it, filing it
        Edit,           ///< cut, copy, paste, duplicate, rename
        Remove          ///< deleting it -- always last
    };

    /** The dynamic property each top-level item carries: its Section */
    static const char *sectionProperty();

    explicit ContextMenu(QMenu *menu);
    /** Lays the sections out if finish() was not called */
    ~ContextMenu();

    QMenu *menu() const { return m_menu; }

    /** A new item in @p section, running @p fn when chosen */
    QAction *add(Section section, const QString &text,
                 const std::function<void()> &fn = std::function<void()>(),
                 const QString &objectName = QString());
    /** An existing (shared) action in @p section */
    QAction *add(Section section, QAction *action);
    /** A submenu in @p section */
    QMenu *addMenu(Section section, const QString &title, const QString &objectName = QString());

    /** Everything already in @p menu is this place's own: each item is
     *  filed in Place and the separators between them are dropped (a place
     *  whose own items are built straight into the QMenu, before the kind
     *  block goes under them). */
    static void fileAsPlace(QMenu *menu);

    /** Nothing added yet */
    bool isEmpty() const;
    /** Anything in @p section */
    bool has(Section section) const;

    /** Put the sections into the menu, in order, separated. Once. */
    void finish();

    /** Empty when @p menu follows the canon: every top-level item carries a
     *  section, the sections never go backwards, a separator stands exactly
     *  between two sections, and every label is Title Case with "…" (never
     *  "...") and no shortcut typed into it. Otherwise what is wrong. */
    static QStringList checkCanon(const QMenu *menu);
    /** Empty when @p text is Title Case by the canon's rule */
    static QString checkTitleCase(const QString &text);
    /** The kind block of @p menu: the object names (a submenu's own) of its
     *  top-level items in Open..Remove, in order -- what must be the same
     *  wherever one kind of object is right-clicked */
    static QStringList kindBlock(const QMenu *menu);
    /** The texts of @p menu's top-level items in Place and View */
    static QStringList contextItems(const QMenu *menu);

private:
    QPointer<QMenu> m_menu;
    QMap<int, QList<QAction *> > m_items;
    bool m_done;
};

/**
 * The shared builders, one per kind. Each fills the canon's common items for
 * its kind; what a place does differently comes in through its hooks, and
 * its own items go in after, in ContextMenu::Place.
 */
namespace ContextMenus
{
    /* ---- The words, one per action ---- */
    QLC_UI_EXPORT QString editHeadLayoutText();
    QLC_UI_EXPORT QString fixturePropertiesText();
    QLC_UI_EXPORT QString locateText();
    QLC_UI_EXPORT QString selectAllSimilarText(int count = 0);
    QLC_UI_EXPORT QString createGroupFromSelectionText();
    QLC_UI_EXPORT QString moveToFolderText(int groups = 1);
    QLC_UI_EXPORT QString renameText(bool dialog);
    QLC_UI_EXPORT QString deleteGroupsText(int count);
    QLC_UI_EXPORT QString removeFromSceneText(int count);
    QLC_UI_EXPORT QString splitByHeadTypeText();
    QLC_UI_EXPORT QString layOutFromStageText();
    QLC_UI_EXPORT QString usedByText();
    QLC_UI_EXPORT QString duplicateText();
    QLC_UI_EXPORT QString editPaletteText(bool effect);
    QLC_UI_EXPORT QString openFunctionText();
    QLC_UI_EXPORT QString deletePalettesText(int count);
    QLC_UI_EXPORT QString deleteFunctionsText(int count);

    /** Where fixtures and fixture groups were right-clicked, and what that
     *  place does its own way. Every hook may be left empty: the builder
     *  then does it itself (a dialog), or leaves the item out where noted. */
    struct QLC_UI_EXPORT RigPlace
    {
        QWidget *parent = nullptr;
        /** Design edits (rename, delete, move, layout, patch) are allowed here */
        bool editable = true;
        /** Edit Head Layout…: default, the head-layout editor in a dialog */
        std::function<void(quint32)> editLayout;
        /** Fixture Properties…: default, the Plot's Fixture Properties */
        std::function<void(const QList<quint32> &)> properties;
        /** Select All Similar: left out when empty */
        std::function<void()> selectSimilar;
        /** How many Select All Similar would select (0: no count shown;
         *  1: only itself, so the item is shown off) */
        int similarCount = 0;
        /** Patch…: default, the Patch dialog on the fixtures */
        std::function<void(const QList<quint32> &)> patch;
        /** Create Fixture Group from Selection…: default, from the fixtures,
         *  or from the groups as blocks */
        std::function<void()> createGroup;
        /** Rename a group inline in the list; empty: "Rename…", a dialog */
        std::function<void(quint32)> renameInPlace;
        /** Rename a fixture inline in the list; empty: "Rename…", a dialog */
        std::function<void(quint32)> renameFixtureInPlace;
        /** After the builder changed the doc */
        std::function<void()> changed;
        /** Offer Selection ▸ (SelectionMenu: a layer from them, Create
         *  Fixture Group from Selection… inside it) */
        bool offerSelectionMenu = true;
        bool offerPatch = true;
        bool offerCreateGroup = true;
        bool offerMoveToFolder = true;
        bool offerRename = true;
        bool offerDelete = true;
    };

    /** The canon's items for @p groupIds and @p fixtureIds (either may be
     *  empty) into @p menu */
    QLC_UI_EXPORT void rig(ContextMenu &menu, Doc *doc, const QList<quint32> &groupIds,
                           const QList<quint32> &fixtureIds, const RigPlace &place);
    inline void fixtureGroup(ContextMenu &menu, Doc *doc, const QList<quint32> &groupIds,
                             const RigPlace &place)
    { rig(menu, doc, groupIds, QList<quint32>(), place); }
    inline void fixture(ContextMenu &menu, Doc *doc, const QList<quint32> &fixtureIds,
                        const RigPlace &place)
    { rig(menu, doc, QList<quint32>(), fixtureIds, place); }

    /** Where palettes were right-clicked, and what that place does its own
     *  way. Every hook may be left empty: the builder then does it itself. */
    struct QLC_UI_EXPORT PalettePlace
    {
        QWidget *parent = nullptr;
        /** Design edits (move, duplicate, rename, delete) are allowed here */
        bool editable = true;
        /** Edit Palette / Edit Effect: default, in Build's Workbench
         *  (ProgrammingManager::openPalette) */
        std::function<void(quint32)> edit;
        /** Rename inline in the list; empty: "Rename…", a dialog */
        std::function<void(quint32)> renameInPlace;
        /** After Duplicate made @p copy; default: edit it */
        std::function<void(quint32 copy)> duplicated;
        /** A Used By ▸ entry chosen (PaletteOps::Use kind, owner, layer);
         *  default: the cue or look in the Show Tree, the scene in Build */
        std::function<void(int kind, quint32 owner, quint32 layer)> showUse;
        /** Move to Folder ▸ chose @p folder (empty: none); default
         *  PaletteOps::move */
        std::function<void(const QList<quint32> &, const QString &)> moveTo;
        /** Delete Palette…; default: ask (naming every use), then
         *  PaletteOps::remove */
        std::function<void(const QList<quint32> &)> remove;
        /** Make Reusable… on a palette the show does not manage (a cue's
         *  one-off value); left out when empty */
        std::function<void(quint32)> makeReusable;
        /** After the builder changed the doc */
        std::function<void()> changed;
    };
    /** The canon's palette menu for @p paletteIds (the first is the one
     *  clicked) into @p menu: Edit Palette | Runs ▸ (effects), Used By ▸ |
     *  Move to Folder ▸, Save as Bundle… | Duplicate, Rename | Delete
     *  Palette…. Only Edit and Used By for a palette the show does not
     *  manage (a cue's one-off value). */
    QLC_UI_EXPORT void palette(ContextMenu &menu, Doc *doc, const QList<quint32> &paletteIds,
                               const PalettePlace &place);

    /** Where functions (scenes, chasers, EFX, ...) were right-clicked. */
    struct QLC_UI_EXPORT FunctionPlace
    {
        QWidget *parent = nullptr;
        bool editable = true;
        /** Open in Workbench: default ProgrammingManager::openFunction */
        std::function<void(quint32)> open;
        /** Rename inline in the list; empty: "Rename…", a dialog */
        std::function<void(quint32)> renameInPlace;
        /** Duplicate; default: a copy each, one step on the shared history */
        std::function<void(const QList<quint32> &)> duplicate;
        /** Move to Folder ▸; default: their path, one step */
        std::function<void(const QList<quint32> &, const QString &)> moveTo;
        /** Delete…; default: ask, then delete them */
        std::function<void(const QList<quint32> &)> remove;
        std::function<void()> changed;
    };
    /** The canon's function menu for @p functionIds (the first is the one
     *  clicked): Open in Workbench | Learn Trigger…, Runs ▸, Send to Show
     *  Tree…, Used By ▸ | Move to Folder ▸ | Duplicate, Rename | Delete…. */
    QLC_UI_EXPORT void function(ContextMenu &menu, Doc *doc, const QList<quint32> &functionIds,
                                const FunctionPlace &place);

    /** The head-layout editor of @p groupId in a dialog */
    QLC_UI_EXPORT void editGroupLayout(Doc *doc, quint32 groupId, QWidget *parent);
    /** Flash @p fixtureIds on the rig (the Plot's Locate) */
    QLC_UI_EXPORT void locate(Doc *doc, const QList<quint32> &fixtureIds, QWidget *parent);
    /** Ask, then delete @p groupIds (their fixtures stay). True when deleted. */
    QLC_UI_EXPORT bool deleteGroups(Doc *doc, const QList<quint32> &groupIds, QWidget *parent);
    /** Rename @p groupId with a dialog. True when renamed. */
    QLC_UI_EXPORT bool renameGroup(Doc *doc, quint32 groupId, QWidget *parent);
}

/** @} */

#endif
