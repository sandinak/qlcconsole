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
 * A menu is built in sections, always in the same order: the thing's own
 * editor, what its kind does, selecting more like it, organising it, what
 * only this place offers, the list's view, clipboard and name, and taking
 * it out or deleting it last. Empty sections are left out; one separator
 * stands between two sections. And one builder per kind -- a fixture group,
 * a fixture -- gives the same items with the same words wherever the thing
 * is right-clicked; a place adds its own items to the Place section.
 */
class QLC_UI_EXPORT ContextMenu
{
    Q_DECLARE_TR_FUNCTIONS(ContextMenu)

public:
    enum Section
    {
        Open = 1,       ///< its own editor; first, the default action
        Kind,           ///< what this kind of thing does
        Selection,      ///< picking out more like it
        Organise,       ///< making something from it, filing it
        Place,          ///< what only this place offers
        View,           ///< the list itself (expand, collapse, group by)
        Edit,           ///< cut, copy, paste, duplicate, rename
        Remove          ///< out of a container, then delete -- always last
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
