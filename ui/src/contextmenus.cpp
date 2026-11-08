/*
  Q Light Controller Plus
  contextmenus.cpp

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

#include <QDialogButtonBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QDialog>
#include <QAction>
#include <QCursor>
#include <QRegularExpression>
#include <QSet>

#include "contextmenus.h"
#include "createfixturegroup.h"
#include "fixturegroupeditor.h"
#include "outputrulesdialog.h"
#include "headtypesdialog.h"
#include "stagegroupdialog.h"
#include "fixturemanager.h"
#include "whyonpanel.h"
#include "selectionmenu.h"
#include "fixturegroup.h"
#include "fixture.h"
#include "monitor.h"
#include "apputil.h"
#include "patchinventorydialog.h"
#include "ledlayouteditor.h"
#include "gobowheelsdialog.h"
#include "structurestudioview.h"
#include "programmingmanager.h"
#include "controllearndialog.h"
#include "effectrunsmenu.h"
#include "showtreeview.h"
#include "paletteops.h"
#include "qlcpalette.h"
#include "collection.h"
#include "usedby.h"
#include "chaser.h"
#include "app.h"
#include "doc.h"

#include <QApplication>

/****************************************************************************
 * ContextMenu: the sections
 ****************************************************************************/

const char *ContextMenu::sectionProperty()
{
    return "ctxSection";
}

ContextMenu::ContextMenu(QMenu *menu)
    : m_menu(menu)
    , m_done(false)
{
}

ContextMenu::~ContextMenu()
{
    finish();
}

QAction *ContextMenu::add(Section section, const QString &text, const std::function<void()> &fn,
                          const QString &objectName)
{
    if (m_menu.isNull())
        return nullptr;
    QAction *a = new QAction(text, m_menu);
    if (!objectName.isEmpty())
        a->setObjectName(objectName);
    if (fn)
        QObject::connect(a, &QAction::triggered, m_menu, [fn]() { fn(); });
    return add(section, a);
}

QAction *ContextMenu::add(Section section, QAction *action)
{
    if (action == nullptr || m_done)
        return action;
    action->setProperty(sectionProperty(), int(section));
    m_items[int(section)] << action;
    return action;
}

QMenu *ContextMenu::addMenu(Section section, const QString &title, const QString &objectName)
{
    if (m_menu.isNull())
        return nullptr;
    QMenu *sub = new QMenu(title, m_menu);
    if (!objectName.isEmpty())
        sub->setObjectName(objectName);
    add(section, sub->menuAction());
    return sub;
}

void ContextMenu::fileAsPlace(QMenu *menu)
{
    if (menu == nullptr)
        return;
    for (QAction *a : menu->actions())
    {
        if (a->isSeparator())
            menu->removeAction(a);
        else if (!a->property(sectionProperty()).isValid())
            a->setProperty(sectionProperty(), int(Place));
    }
}

bool ContextMenu::isEmpty() const
{
    for (auto it = m_items.constBegin(); it != m_items.constEnd(); ++it)
        if (!it.value().isEmpty())
            return false;
    return true;
}

bool ContextMenu::has(Section section) const
{
    return !m_items.value(int(section)).isEmpty();
}

void ContextMenu::finish()
{
    if (m_done || m_menu.isNull())
        return;
    m_done = true;
    // Whatever the place put in the menu directly comes first (a place not
    // yet on the builder); the sections follow it.
    bool any = !m_menu->actions().isEmpty();
    for (auto it = m_items.constBegin(); it != m_items.constEnd(); ++it)
    {
        if (it.value().isEmpty())
            continue;
        if (any)
            m_menu->addSeparator();
        for (QAction *a : it.value())
            m_menu->addAction(a);
        any = true;
    }
}

QString ContextMenu::checkTitleCase(const QString &text)
{
    static const QSet<QString> minor = {
        "a", "an", "the", "and", "or", "of", "to", "in", "on", "at", "by", "for",
        "from", "with", "as", "vs", "into"
    };
    QString t = text;
    t.remove(QLatin1Char('&'));
    // Names and quotes are the user's own words, and a count is a number.
    t.remove(QRegularExpression(QStringLiteral("“[^”]*”|\"[^\"]*\"|\\([^)]*\\)")));
    const QStringList words = t.split(QRegularExpression(QStringLiteral("[\\s/]+")),
                                      Qt::SkipEmptyParts);
    for (int i = 0; i < words.size(); ++i)
    {
        // "to…", "Setup:" -- the word without what trails it
        QString w = words.at(i);
        while (!w.isEmpty() && !w.at(w.size() - 1).isLetterOrNumber())
            w.chop(1);
        if (w.isEmpty())
            continue;
        const QChar c = w.at(0);
        if (!c.isLetter())
            continue;                        // "+ Cue", "…", "3"
        if (i > 0 && minor.contains(w.toLower()))
            continue;
        if (c.isLower())
            return QStringLiteral("not Title Case: \"%1\" in \"%2\"").arg(w, text);
    }
    return QString();
}

QStringList ContextMenu::checkCanon(const QMenu *menu)
{
    QStringList problems;
    if (menu == nullptr)
        return problems;
    const QList<QAction *> acts = menu->actions();
    int last = 0;
    bool sepPending = false;
    bool first = true;
    for (int i = 0; i < acts.size(); ++i)
    {
        QAction *a = acts.at(i);
        if (a->isSeparator())
        {
            if (first)
                problems << QStringLiteral("starts with a separator");
            if (sepPending)
                problems << QStringLiteral("two separators in a row");
            sepPending = true;
            continue;
        }
        const QVariant sv = a->property(sectionProperty());
        if (!sv.isValid())
        {
            problems << QStringLiteral("\"%1\" is in no section").arg(a->text());
            continue;
        }
        const int s = sv.toInt();
        if (s < last)
            problems << QStringLiteral("\"%1\" (section %2) after section %3").arg(a->text()).arg(s).arg(last);
        if (!first && s != last && !sepPending)
            problems << QStringLiteral("no separator before \"%1\"").arg(a->text());
        if (s == last && sepPending)
            problems << QStringLiteral("a separator inside section %1, before \"%2\"").arg(s).arg(a->text());
        const QString text = a->text();
        if (text.contains(QLatin1String("...")))
            problems << QStringLiteral("\"...\" instead of \"…\" in \"%1\"").arg(text);
        if (text.contains(QRegularExpression(QStringLiteral("\\((Ctrl|Shift|Alt|⌘)"))))
            problems << QStringLiteral("a shortcut typed into \"%1\"").arg(text);
        // "Why is this on?" is the panel's name, a question kept as asked.
        const QString tc = text == WhyOnPanel::actionText() ? QString() : checkTitleCase(text);
        if (!tc.isEmpty())
            problems << tc;
        last = s;
        sepPending = false;
        first = false;
    }
    if (sepPending && !first)
        problems << QStringLiteral("ends with a separator");
    return problems;
}

QStringList ContextMenu::kindBlock(const QMenu *menu)
{
    QStringList out;
    if (menu == nullptr)
        return out;
    for (QAction *a : menu->actions())
    {
        const QVariant sv = a->property(sectionProperty());
        if (a->isSeparator() || !sv.isValid() || sv.toInt() < Open)
            continue;
        out << (a->menu() != nullptr ? a->menu()->objectName() : a->objectName());
    }
    return out;
}

QStringList ContextMenu::contextItems(const QMenu *menu)
{
    QStringList out;
    if (menu == nullptr)
        return out;
    for (QAction *a : menu->actions())
    {
        const QVariant sv = a->property(sectionProperty());
        if (!a->isSeparator() && sv.isValid() && sv.toInt() < Open)
            out << a->text();
    }
    return out;
}

/****************************************************************************
 * The words
 ****************************************************************************/

QString ContextMenus::editHeadLayoutText()        { return ContextMenu::tr("Edit Head Layout…"); }
QString ContextMenus::fixturePropertiesText()     { return ContextMenu::tr("Fixture Properties…"); }
QString ContextMenus::locateText()                { return ContextMenu::tr("Locate"); }
QString ContextMenus::createGroupFromSelectionText() { return ContextMenu::tr("Create Fixture Group from Selection…"); }
QString ContextMenus::splitByHeadTypeText()       { return ContextMenu::tr("Split by Head Type…"); }
QString ContextMenus::layOutFromStageText()       { return ContextMenu::tr("Lay Out from the Stage…"); }
QString ContextMenus::usedByText()                { return ContextMenu::tr("Used By"); }
QString ContextMenus::duplicateText()             { return ContextMenu::tr("Duplicate"); }
QString ContextMenus::openFunctionText()          { return ContextMenu::tr("Open in Workbench"); }

QString ContextMenus::editPaletteText(bool effect)
{
    // It opens in the Workbench, not a dialog: no "…".
    return effect ? ContextMenu::tr("Edit Effect") : ContextMenu::tr("Edit Palette");
}

QString ContextMenus::deletePalettesText(int count)
{
    return count > 1 ? ContextMenu::tr("Delete %1 Palettes…").arg(count)
                     : ContextMenu::tr("Delete Palette…");
}

QString ContextMenus::deleteFunctionsText(int count)
{
    return count > 1 ? ContextMenu::tr("Delete %1 Functions…").arg(count)
                     : ContextMenu::tr("Delete…");
}

QString ContextMenus::selectAllSimilarText(int count)
{
    return count > 1 ? ContextMenu::tr("Select All Similar (%1)").arg(count)
                     : ContextMenu::tr("Select All Similar");
}

QString ContextMenus::moveToFolderText(int groups)
{
    return groups > 1 ? ContextMenu::tr("Move %1 Fixture Groups to Folder").arg(groups)
                      : ContextMenu::tr("Move to Folder");
}

QString ContextMenus::renameText(bool dialog)
{
    return dialog ? ContextMenu::tr("Rename…") : ContextMenu::tr("Rename");
}

QString ContextMenus::deleteGroupsText(int count)
{
    return count > 1 ? ContextMenu::tr("Delete %1 Fixture Groups…").arg(count)
                     : ContextMenu::tr("Delete Fixture Group…");
}

QString ContextMenus::removeFromSceneText(int count)
{
    return count > 1 ? ContextMenu::tr("Remove %1 from Their Scenes").arg(count)
                     : ContextMenu::tr("Remove from Scene");
}

/****************************************************************************
 * The shared actions
 ****************************************************************************/

void ContextMenus::editGroupLayout(Doc *doc, quint32 groupId, QWidget *parent)
{
    FixtureGroup *g = doc != nullptr ? doc->fixtureGroup(groupId) : nullptr;
    if (g == nullptr)
        return;
    QDialog dlg(parent);
    dlg.setWindowTitle(ContextMenu::tr("Head Layout — %1").arg(g->name()));
    QVBoxLayout *vl = new QVBoxLayout(&dlg);
    // The same grid editor the Fixtures tab uses -- it edits the group live.
    vl->addWidget(new FixtureGroupEditor(g, doc, &dlg), 1);
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    QObject::connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);
    vl->addWidget(bb);
    dlg.resize(720, 520);
    dlg.exec();
    doc->setModified();
}

void ContextMenus::locate(Doc *doc, const QList<quint32> &fixtureIds, QWidget *parent)
{
    Q_UNUSED(doc)
    Q_UNUSED(parent)
    if (fixtureIds.isEmpty() || Monitor::instance() == nullptr)
        return;
    /* It flashes on the rig from where it was asked: the person stays on
       their page. Going to the Lighting Studio tab hid the Show Tree, whose
       preview stops when it is left -- so the cue being built (a WLED's
       held effect in its new colour) went dark under the flashes and never
       came back (UX-72, F-11). The flashing needs no tab: the Monitor is
       always there. */
    Monitor::instance()->locateFixtures(fixtureIds);
}

bool ContextMenus::deleteGroups(Doc *doc, const QList<quint32> &groupIds, QWidget *parent)
{
    if (doc == nullptr || groupIds.isEmpty())
        return false;
    QString what;
    if (groupIds.size() == 1)
    {
        FixtureGroup *g = doc->fixtureGroup(groupIds.first());
        if (g == nullptr)
            return false;
        what = ContextMenu::tr("Delete the fixture group “%1”?").arg(g->name());
    }
    else
        what = ContextMenu::tr("Delete %1 fixture groups?").arg(groupIds.size());
    if (QMessageBox::question(parent, ContextMenu::tr("Delete Fixture Group"),
            what + QStringLiteral("\n\n")
            + ContextMenu::tr("Their fixtures are not deleted -- only the groups and their head "
                              "layouts. Anything set up to use them will no longer find them."),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return false;
    for (quint32 id : groupIds)
        doc->deleteFixtureGroup(id);
    doc->setModified();
    return true;
}

bool ContextMenus::renameGroup(Doc *doc, quint32 groupId, QWidget *parent)
{
    FixtureGroup *g = doc != nullptr ? doc->fixtureGroup(groupId) : nullptr;
    if (g == nullptr)
        return false;
    bool ok = false;
    const QString name = QInputDialog::getText(parent, ContextMenu::tr("Rename Fixture Group"),
                                               ContextMenu::tr("Name:"), QLineEdit::Normal,
                                               g->name(), &ok).trimmed();
    if (!ok || name.isEmpty() || name == g->name())
        return false;
    g->setName(name);
    doc->setModified();
    return true;
}

/****************************************************************************
 * Fixtures and fixture groups
 ****************************************************************************/

void ContextMenus::rig(ContextMenu &menu, Doc *doc, const QList<quint32> &groupIds,
                       const QList<quint32> &fixtureIds, const RigPlace &place)
{
    if (doc == nullptr)
        return;
    QList<quint32> groups;
    for (quint32 id : groupIds)
        if (doc->fixtureGroup(id) != nullptr && !groups.contains(id))
            groups << id;
    QList<quint32> fixtures;
    for (quint32 id : fixtureIds)
        if (doc->fixture(id) != nullptr && !fixtures.contains(id))
            fixtures << id;
    if (groups.isEmpty() && fixtures.isEmpty())
        return;

    QWidget *parent = place.parent;
    const std::function<void()> changed = place.changed;
    const bool editable = place.editable;
    // One group and nothing else; fixtures and no group.
    FixtureGroup *one = (groups.size() == 1 && fixtures.isEmpty()) ? doc->fixtureGroup(groups.first())
                                                                  : nullptr;
    const bool onlyFixtures = groups.isEmpty();
    // Every fixture concerned: the ones picked and the groups' members.
    QList<quint32> members = fixtures;
    for (quint32 gid : groups)
        for (quint32 fid : doc->fixtureGroup(gid)->fixtureList())
            if (!members.contains(fid))
                members << fid;

    /* ---- 1. Its own editor ---- */
    if (one != nullptr && editable)
    {
        const quint32 gid = one->id();
        const std::function<void(quint32)> edit = place.editLayout;
        menu.add(ContextMenu::Open, editHeadLayoutText(), [doc, gid, edit, parent]() {
            if (edit)
                edit(gid);
            else
                editGroupLayout(doc, gid, parent);
        }, QStringLiteral("ctxEditHeadLayout"));
    }
    else if (onlyFixtures && (place.properties || Monitor::instance() != nullptr))
    {
        const std::function<void(const QList<quint32> &)> props = place.properties;
        menu.add(ContextMenu::Open, fixturePropertiesText(), [fixtures, props]() {
            if (props)
                props(fixtures);
            else if (Monitor *mon = Monitor::instance())
            {
                if (fixtures.size() > 1)
                    mon->editFixturesProperties(fixtures);
                else
                    mon->editFixtureProperties(fixtures.first());
            }
        }, QStringLiteral("ctxFixtureProperties"));
    }

    /* ---- 2. What it does: find it, ask about it, address it, its rules ---- */
    if (!members.isEmpty() && Monitor::instance() != nullptr)
        menu.add(ContextMenu::Kind, locateText(), [doc, members, parent]() {
            locate(doc, members, parent);
        }, QStringLiteral("ctxLocate"))
            ->setToolTip(ContextMenu::tr("Flash it on the rig to find it"));
    /* Why is this on: about the light, or a group's first light. */
    quint32 whyFid = 0xFFFFFFFF;
    if (onlyFixtures && fixtures.size() == 1)
        whyFid = fixtures.first();
    else if (one != nullptr && !one->fixtureList().isEmpty())
        whyFid = one->fixtureList().first();
    if (doc->fixture(whyFid) != nullptr)
    {
        const quint32 fid = whyFid;
        menu.add(ContextMenu::Kind, WhyOnPanel::actionText(), [doc, fid, parent]() {
            WhyOnPanel::showFor(doc, fid, 0xFFFFFFFF, parent, QCursor::pos());
        }, QStringLiteral("whyOnAction"));
    }
    if (onlyFixtures && place.offerPatch)
    {
        const std::function<void(const QList<quint32> &)> patch = place.patch;
        QAction *pa = menu.add(ContextMenu::Kind, AppUtil::patchActionText(fixtures.size()),
                               [doc, fixtures, parent, changed, patch]() {
            if (patch)
            {
                patch(fixtures);
                return;
            }
            PatchInventoryDialog dlg(doc, fixtures, parent);
            if (dlg.exec() == QDialog::Accepted && changed)
                changed();
        }, QStringLiteral("ctxPatch"));
        pa->setEnabled(editable && AppUtil::patchAllowed(doc, fixtures));
        pa->setToolTip(ContextMenu::tr("Give a universe and address, or move to another -- as a block "
                                       "keeping their spacing, or packed -- reviewed before it is applied"));
    }
    /* One fixture: where each of its heads is, and its own glass. */
    if (onlyFixtures && fixtures.size() == 1)
    {
        const quint32 fid = fixtures.first();
        if (LedLayoutDialog::canEdit(doc, fid))
            menu.add(ContextMenu::Kind, ContextMenu::tr("LED Layout…"), [doc, fid, parent]() {
                LedLayoutDialog::edit(doc, fid, parent);
            }, QStringLiteral("ledLayoutAction"));
        if (GoboWheelsDialog::hasGoboWheels(doc->fixture(fid)))
            menu.add(ContextMenu::Kind, ContextMenu::tr("Gobo Wheels…"), [doc, fid, parent, changed]() {
                GoboWheelsDialog dlg(doc, fid, parent);
                if (dlg.exec() != QDialog::Accepted)
                    return;
                // Every open rig view draws the new glass.
                foreach (QWidget *w, QApplication::allWidgets())
                    if (StructureStudioView *sv = qobject_cast<StructureStudioView *>(w))
                        sv->update();
                if (changed)
                    changed();
            }, QStringLiteral("ctxGoboWheels"));
    }
    menu.add(ContextMenu::Kind, OutputRulesDialog::actionText(), [doc, groups, fixtures, parent, changed]() {
        if (OutputRulesDialog::edit(parent, doc, groups, fixtures) && changed)
            changed();
    }, QStringLiteral("outputRulesAction"))
        ->setToolTip(ContextMenu::tr("Power relays, house and work lights: ignore Blackout or the "
                                     "Grand Master, switch instead of fade, on when not in use"));
    /* Fixtures of more than one kind of head (RGB and white rows): a
       fixture group per kind. */
    if (onlyFixtures && editable && HeadTypesDialog::anyMixed(doc, fixtures))
        menu.add(ContextMenu::Kind, splitByHeadTypeText(), [doc, fixtures, parent, changed]() {
            HeadTypesDialog dlg(doc, fixtures, HeadTypesDialog::suggestName(doc, fixtures), parent);
            if (dlg.exec() == QDialog::Accepted && changed)
                changed();
        }, QStringLiteral("ctxSplitByHeadType"));
    if (one != nullptr && editable)
    {
        const quint32 gid = one->id();
        if (HeadTypesDialog::anyMixed(doc, members))
            menu.add(ContextMenu::Kind, splitByHeadTypeText(), [doc, gid, parent, changed]() {
                FixtureGroup *g = doc->fixtureGroup(gid);
                if (g == nullptr)
                    return;
                HeadTypesDialog dlg(doc, g->fixtureList(), g->name(), parent);
                if (dlg.exec() == QDialog::Accepted && changed)
                    changed();
            }, QStringLiteral("ctxSplitByHeadType"));
        if (StageGroupDialog::anyOnStage(doc, members))
            menu.add(ContextMenu::Kind, layOutFromStageText(), [doc, gid, parent, changed]() {
                FixtureGroup *g = doc->fixtureGroup(gid);
                if (g == nullptr)
                    return;
                StageGroupDialog dlg(doc, g->fixtureList(), g->name(), parent);
                dlg.setEditing(g);
                if (dlg.exec() == QDialog::Accepted && changed)
                    changed();
            }, QStringLiteral("ctxLayOutFromStage"));
    }

    /* ---- 4's Create Fixture Group: made here, filed in Selection ▸ ---- */
    QAction *createAct = nullptr;
    if (editable && place.offerCreateGroup && (!fixtures.isEmpty() || groups.size() > 1))
    {
        const std::function<void()> create = place.createGroup;
        createAct = new QAction(createGroupFromSelectionText(), menu.menu());
        createAct->setObjectName(QStringLiteral("ctxCreateGroupFromSelection"));
        QObject::connect(createAct, &QAction::triggered, menu.menu(),
                         [doc, groups, fixtures, create, parent, changed]() {
            if (create)
            {
                create();
                return;
            }
            FixtureGroup *made = groups.isEmpty()
                ? CreateFixtureGroup::createFromFixtures(doc, fixtures, parent)
                : CreateFixtureGroup::createFromGroups(doc, groups, parent);
            if (made != nullptr && changed)
                changed();
        });
    }

    /* ---- 3. Selection ▸ (Q201): a layer from what is picked, and a group
       of it; then more like it ---- */
    if (place.offerSelectionMenu && !members.isEmpty() && menu.menu() != nullptr)
    {
        QMenu *staging = new QMenu(menu.menu());
        QList<QAction *> extra;
        if (createAct != nullptr)
            extra << createAct;
        QMenu *sub = SelectionMenu::addTo(staging, doc, members, extra);
        sub->setObjectName(QStringLiteral("selectionMenu"));
        menu.add(ContextMenu::Selection, sub->menuAction());
        createAct = nullptr;            // it is in Selection ▸
    }
    if (place.selectSimilar)
    {
        QAction *sim = menu.add(ContextMenu::Selection, selectAllSimilarText(place.similarCount),
                                place.selectSimilar, QStringLiteral("selectAllSimilar"));
        sim->setEnabled(place.similarCount != 1);       // 1: nothing like it but itself
        sim->setToolTip(ContextMenu::tr("Fixtures: every one of the same definition and mode. "
                                         "Fixture groups: every one made of the same fixtures, "
                                         "laid out the same way."));
    }

    /* ---- 4. A group of them (when there is no Selection ▸); a folder for
       the groups ---- */
    if (createAct != nullptr)
        menu.add(ContextMenu::Organise, createAct);
    if (editable && place.offerMoveToFolder && !groups.isEmpty())
    {
        QMenu *sub = menu.addMenu(ContextMenu::Organise, moveToFolderText(groups.size()),
                                  QStringLiteral("ctxMoveToFolder"));
        sub->setIcon(QIcon(":/folder.png"));
        FixtureManager::fillMoveToFolderMenu(sub, doc, groups);
        QObject::connect(sub, &QMenu::triggered, sub, [doc, groups, parent, changed](QAction *a) {
            if (FixtureManager::applyMoveToFolder(parent, doc, groups, a) && changed)
                changed();
        });
    }

    /* ---- 7. Its name ---- */
    if (one != nullptr && editable && place.offerRename)
    {
        const quint32 gid = one->id();
        const std::function<void(quint32)> inPlace = place.renameInPlace;
        menu.add(ContextMenu::Edit, renameText(!inPlace), [doc, gid, inPlace, parent, changed]() {
            if (inPlace)
                inPlace(gid);
            else if (renameGroup(doc, gid, parent) && changed)
                changed();
        }, QStringLiteral("ctxRename"));
    }

    if (onlyFixtures && fixtures.size() == 1 && editable && place.offerRename)
    {
        const quint32 fid = fixtures.first();
        const std::function<void(quint32)> inPlace = place.renameFixtureInPlace;
        menu.add(ContextMenu::Edit, renameText(!inPlace), [doc, fid, inPlace, parent, changed]() {
            if (inPlace)
            {
                inPlace(fid);
                return;
            }
            Fixture *fxi = doc->fixture(fid);
            if (fxi == nullptr)
                return;
            bool ok = false;
            const QString name = QInputDialog::getText(parent, ContextMenu::tr("Rename Fixture"),
                                                       ContextMenu::tr("Name:"), QLineEdit::Normal,
                                                       fxi->name(), &ok).trimmed();
            if (!ok || name.isEmpty() || name == fxi->name()
                || AppUtil::refuseTakenFixtureName(parent, doc, fid, name))
                return;
            fxi->setName(name);
            doc->setModified();
            if (changed)
                changed();
        }, QStringLiteral("ctxRename"));
    }

    /* ---- 8. Delete, last. A group's fixtures stay. ---- */
    if (editable && place.offerDelete && !groups.isEmpty())
        menu.add(ContextMenu::Remove, deleteGroupsText(groups.size()), [doc, groups, parent, changed]() {
            if (deleteGroups(doc, groups, parent) && changed)
                changed();
        }, QStringLiteral("ctxDeleteGroup"));
}

/****************************************************************************
 * Palettes
 ****************************************************************************/

/* Where a Used By ▸ entry leads when the place has no way of its own: a cue
   or a look in the Show Tree, a scene in Build, an effect's slot to that
   effect. */
static void showPaletteUse(Doc *doc, int kind, quint32 owner, quint32 layer)
{
    if (kind == PaletteOps::Use::Cue || kind == PaletteOps::Use::Look)
    {
        ShowTreeView *tree = ShowTreeView::instance();
        if (tree == nullptr)
            return;
        foreach (QWidget *w, QApplication::topLevelWidgets())
            if (App *app = qobject_cast<App *>(w))
                app->showContext(App::tr("Show Tree"));
        if (layer != ShowTree::invalidId())
            tree->selectNode(layer, owner);
        else
            tree->selectNode(owner);
    }
    else if (kind == PaletteOps::Use::Scene)
        ProgrammingManager::openFunction(owner);
    else if (kind == PaletteOps::Use::EffectSlot && doc->palette(owner) != nullptr)
        ProgrammingManager::openPalette(owner);
}

void ContextMenus::palette(ContextMenu &menu, Doc *doc, const QList<quint32> &paletteIds,
                           const PalettePlace &place)
{
    if (doc == nullptr)
        return;
    QList<quint32> pids;
    for (quint32 id : paletteIds)
        if (doc->palette(id) != nullptr && !pids.contains(id))
            pids << id;
    if (pids.isEmpty())
        return;
    const quint32 pid = pids.first();
    QLCPalette *pal = doc->palette(pid);
    PaletteOps *ops = doc->paletteOps();
    QWidget *parent = place.parent;
    const std::function<void()> changed = place.changed;
    const bool effect = pal->type() == QLCPalette::Effect;
    const bool managed = ops->isManaged(pid);
    const bool editable = place.editable && managed;

    /* ---- 1. Its own editor: the Look Editor or the effect designer, in
       the Workbench ---- */
    {
        const std::function<void(quint32)> edit = place.edit;
        QAction *a = menu.add(ContextMenu::Open, editPaletteText(effect), [pid, edit]() {
            if (edit)
                edit(pid);
            else
                ProgrammingManager::openPalette(pid);
        }, QStringLiteral("ctxEditPalette"));
        a->setToolTip(effect ? ContextMenu::tr("In the effect designer, in the Workbench")
                             : ContextMenu::tr("In the Look Editor, in the Workbench"));
        menu.menu()->setDefaultAction(a);
    }

    /* ---- 2. How an effect runs; where it is used ---- */
    // A cue's one-off effect runs its own way too: not only a managed one.
    if (effect && place.editable && menu.menu() != nullptr)
    {
        QMenu *staging = new QMenu(menu.menu());
        if (QMenu *runs = EffectRunsMenu::add(staging, doc, pids, parent))
            menu.add(ContextMenu::Kind, runs->menuAction());
    }
    {
        QMenu *used = menu.addMenu(ContextMenu::Kind, usedByText(), QStringLiteral("ctxUsedBy"));
        used->setToolTipsVisible(true);
        const std::function<void(int, quint32, quint32)> show = place.showUse;
        QSet<quint32> fns;
        for (const PaletteOps::Use &u : ops->usage(pid))
        {
            QAction *a = used->addAction(u.label);
            const int k = int(u.kind);
            const quint32 owner = u.owner, layer = u.layer;
            QObject::connect(a, &QAction::triggered, used, [doc, show, k, owner, layer]() {
                if (show)
                    show(k, owner, layer);
                else
                    showPaletteUse(doc, k, owner, layer);
            });
            if (u.function != 0xFFFFFFFF)
                fns.insert(u.function);
        }
        for (const QString &vc : UsedBy::vcLines(fns))
            used->addAction(vc)->setEnabled(false);
        if (used->isEmpty())
            used->addAction(ContextMenu::tr("(not used)"))->setEnabled(false);
    }

    /* ---- 4. A one-off value named; filed in a folder; a bundle of them ---- */
    if (!managed && place.editable && place.makeReusable && pids.size() == 1)
    {
        const std::function<void(quint32)> make = place.makeReusable;
        menu.add(ContextMenu::Organise, ContextMenu::tr("Make Reusable…"), [make, pid]() { make(pid); },
                 QStringLiteral("ctxMakeReusable"))
            ->setToolTip(ContextMenu::tr("A named palette in the Library, for any cue"));
    }
    if (editable)
    {
        QMenu *move = menu.addMenu(ContextMenu::Organise, pids.size() > 1
                                       ? ContextMenu::tr("Move %1 Palettes to Folder").arg(pids.size())
                                       : ContextMenu::tr("Move to Folder"),
                                   QStringLiteral("ctxPaletteMoveToFolder"));
        move->setIcon(QIcon(":/folder.png"));
        const std::function<void(const QList<quint32> &, const QString &)> moveTo = place.moveTo;
        auto to = [doc, pids, moveTo, changed](const QString &folder) {
            if (moveTo)
                moveTo(pids, folder);
            else if (doc->paletteOps()->move(pids, folder) && changed)
                changed();
        };
        const QString current = PaletteOps::folderOf(pal);
        QAction *none = move->addAction(ContextMenu::tr("(no folder)"), move, [to]() { to(QString()); });
        none->setEnabled(!current.isEmpty() || pids.size() > 1);
        for (const QString &f : ops->folders())
        {
            QAction *a = move->addAction(f, move, [to, f]() { to(f); });
            a->setEnabled(f != current || pids.size() > 1);
        }
        move->addSeparator();
        move->addAction(ContextMenu::tr("New Folder…"), move, [to, parent]() {
            bool ok = false;
            const QString n = QInputDialog::getText(parent, ContextMenu::tr("Move to a New Folder"),
                                                    ContextMenu::tr("Folder name:"), QLineEdit::Normal,
                                                    QString(), &ok).trimmed();
            if (ok && !n.isEmpty())
                to(n);
        });
        if (ProgrammingManager::instance() != nullptr)
            menu.add(ContextMenu::Organise, pids.size() > 1
                         ? ContextMenu::tr("Save %1 Palettes as Bundle…").arg(pids.size())
                         : ContextMenu::tr("Save as Bundle…"),
                     [pids]() {
                if (ProgrammingManager *pm = ProgrammingManager::instance())
                    pm->saveBundleFromPalettes(pids);
            }, QStringLiteral("ctxSaveBundle"));
    }

    /* ---- 7. A copy; its name ---- */
    if (editable && pids.size() > 1)
    {
        // Several selected: each copied, one undo step (as Delete takes them all).
        menu.add(ContextMenu::Edit, ContextMenu::tr("Duplicate %1 Palettes").arg(pids.size()),
                 [doc, pids, changed]() {
            if (!doc->paletteOps()->duplicate(pids).isEmpty() && changed)
                changed();
        }, QStringLiteral("ctxDuplicate"));
    }
    if (editable && pids.size() == 1)
    {
        const std::function<void(quint32)> edit = place.edit;
        const std::function<void(quint32)> duplicated = place.duplicated;
        menu.add(ContextMenu::Edit, duplicateText(), [doc, pid, edit, duplicated, changed]() {
            const quint32 copy = doc->paletteOps()->duplicate(pid);
            if (doc->palette(copy) == nullptr)
                return;
            if (changed)
                changed();
            if (duplicated)
                duplicated(copy);
            else if (edit)
                edit(copy);
            else
                ProgrammingManager::openPalette(copy);
        }, QStringLiteral("ctxDuplicate"));
        const std::function<void(quint32)> inPlace = place.renameInPlace;
        menu.add(ContextMenu::Edit, renameText(!inPlace), [doc, pid, inPlace, parent, changed]() {
            if (inPlace)
            {
                inPlace(pid);
                return;
            }
            QLCPalette *p = doc->palette(pid);
            if (p == nullptr)
                return;
            bool ok = false;
            const QString n = QInputDialog::getText(parent, ContextMenu::tr("Rename Palette"),
                                                    ContextMenu::tr("Name:"), QLineEdit::Normal,
                                                    p->name(), &ok).trimmed();
            if (ok && !n.isEmpty() && n != p->name() && doc->paletteOps()->rename(pid, n) && changed)
                changed();
        }, QStringLiteral("ctxRename"));
    }

    /* ---- 8. Delete, last: it asks, naming every use ---- */
    if (editable)
    {
        const std::function<void(const QList<quint32> &)> remove = place.remove;
        menu.add(ContextMenu::Remove, deletePalettesText(pids.size()), [doc, pids, remove, parent, changed]() {
            if (remove)
            {
                remove(pids);
                return;
            }
            const QString q = UsedBy::deletePalettesQuestion(doc, pids);
            if (q.isEmpty() || QMessageBox::question(parent, pids.size() > 1 ? ContextMenu::tr("Delete Palettes")
                                                                             : ContextMenu::tr("Delete Palette"),
                                                     q, QMessageBox::Yes | QMessageBox::No,
                                                     QMessageBox::No) != QMessageBox::Yes)
                return;
            if (doc->paletteOps()->remove(pids) && changed)
                changed();
        }, QStringLiteral("ctxDeletePalette"));
    }
}

/****************************************************************************
 * Functions
 ****************************************************************************/

void ContextMenus::function(ContextMenu &menu, Doc *doc, const QList<quint32> &functionIds,
                            const FunctionPlace &place)
{
    if (doc == nullptr)
        return;
    QList<quint32> fids;
    for (quint32 id : functionIds)
        if (doc->function(id) != nullptr && !fids.contains(id))
            fids << id;
    if (fids.isEmpty())
        return;
    const quint32 fid = fids.first();
    Function *f = doc->function(fid);
    QWidget *parent = place.parent;
    const std::function<void()> changed = place.changed;
    const bool editable = place.editable && doc->mode() == Doc::Design;

    /* ---- 1. Its editor, in the Workbench ---- */
    {
        const std::function<void(quint32)> open = place.open;
        QAction *a = menu.add(ContextMenu::Open, openFunctionText(), [fid, open]() {
            if (open)
                open(fid);
            else
                ProgrammingManager::openFunction(fid);
        }, QStringLiteral("ctxOpenFunction"));
        a->setToolTip(ContextMenu::tr("Load it in the Workbench, in its own editor"));
        menu.menu()->setDefaultAction(a);
    }

    /* ---- 2. Fired from a control; how it runs; to the Show Tree; who uses it ---- */
    if (fids.size() == 1 && menu.menu() != nullptr)
    {
        QMenu *staging = new QMenu(menu.menu());
        // Its own name (learnTrigger), the same on every menu that offers it.
        if (QAction *learn = ControlLearnDialog::addTo(staging, doc, ControlMap::function(fid), parent))
            menu.add(ContextMenu::Kind, learn);
        if (editable)
            if (QMenu *runs = EffectRunsMenu::addForFunction(staging, doc, f))
                menu.add(ContextMenu::Kind, runs->menuAction());
    }
    if (fids.size() == 1 && f->type() == Function::ChaserType && ProgrammingManager::instance() != nullptr
        && doc->showTree() != nullptr)
    {
        const QString why = doc->showTree()->whyNotSend(fid);
        QAction *send = menu.add(ContextMenu::Kind, ContextMenu::tr("Send to Show Tree…"), [fid]() {
            if (ProgrammingManager *pm = ProgrammingManager::instance())
                pm->sendToShowTree(fid);
        }, QStringLiteral("ctxSendToShowTree"));
        send->setEnabled(editable && why.isEmpty());
        send->setToolTip(why.isEmpty() ? ContextMenu::tr("Make this chaser a Show Tree cue list in place, "
                                                         "checked to play the same; Restore original puts it back")
                                       : why);
    }
    if (fids.size() == 1)
    {
        QMenu *used = menu.addMenu(ContextMenu::Kind, usedByText(), QStringLiteral("ctxUsedBy"));
        for (Function *other : doc->functions())
        {
            bool uses = false;
            if (Chaser *c = qobject_cast<Chaser *>(other))
            {
                for (const ChaserStep &st : c->steps())
                    uses = uses || st.fid == fid;
            }
            else if (Collection *col = qobject_cast<Collection *>(other))
                uses = col->functions().contains(fid);
            if (!uses || other->id() == fid)
                continue;
            const quint32 oid = other->id();
            used->addAction(other->name(), used, [oid]() { ProgrammingManager::openFunction(oid); });
        }
        for (const QString &vc : UsedBy::vcLines({ fid }))
            used->addAction(vc)->setEnabled(false);
        if (used->isEmpty())
            used->addAction(ContextMenu::tr("(not used)"))->setEnabled(false);
    }

    /* ---- 4. Filed in a folder ---- */
    if (editable)
    {
        QMenu *move = menu.addMenu(ContextMenu::Organise, fids.size() > 1
                                       ? ContextMenu::tr("Move %1 Functions to Folder").arg(fids.size())
                                       : ContextMenu::tr("Move to Folder"),
                                   QStringLiteral("ctxFunctionMoveToFolder"));
        move->setIcon(QIcon(":/folder.png"));
        const std::function<void(const QList<quint32> &, const QString &)> moveTo = place.moveTo;
        auto to = [doc, fids, moveTo, changed](const QString &folder) {
            if (moveTo)
            {
                moveTo(fids, folder);
                return;
            }
            doc->paletteOps()->functionStep(ContextMenu::tr("move to folder %1").arg(folder), fids,
                                            [&](QList<quint32> &) {
                for (quint32 id : fids)
                    if (Function *fn = doc->function(id))
                        fn->setPath(folder);
                return true;
            });
            if (changed)
                changed();
        };
        // The folders this type is filed in.
        QStringList folders;
        for (Function *other : doc->functions())
            if (other->type() == f->type() && !other->path(true).isEmpty()
                && !folders.contains(other->path(true)) && other->isVisible())
                folders << other->path(true);
        folders.sort(Qt::CaseInsensitive);
        const QString current = f->path(true);
        QAction *none = move->addAction(ContextMenu::tr("(no folder)"), move, [to]() { to(QString()); });
        none->setEnabled(!current.isEmpty() || fids.size() > 1);
        for (const QString &fo : folders)
            move->addAction(fo, move, [to, fo]() { to(fo); })->setEnabled(fo != current || fids.size() > 1);
        move->addSeparator();
        move->addAction(ContextMenu::tr("New Folder…"), move, [to, parent]() {
            bool ok = false;
            const QString n = QInputDialog::getText(parent, ContextMenu::tr("Move to a New Folder"),
                                                    ContextMenu::tr("Folder name:"), QLineEdit::Normal,
                                                    QString(), &ok).trimmed();
            if (ok && !n.isEmpty())
                to(n);
        });
    }

    /* ---- 7. Copies; its name ---- */
    if (editable)
    {
        const std::function<void(const QList<quint32> &)> dup = place.duplicate;
        menu.add(ContextMenu::Edit, fids.size() > 1 ? ContextMenu::tr("Duplicate %1").arg(fids.size())
                                                    : duplicateText(),
                 [doc, fids, dup, changed]() {
            if (dup)
            {
                dup(fids);
                return;
            }
            if (ProgrammingManager *pm = ProgrammingManager::instance())
            {
                pm->duplicateFunctions(fids);
                if (changed)
                    changed();
                return;
            }
            // One step on the shared history, as Build's Duplicate.
            doc->paletteOps()->functionStep(ContextMenu::tr("Duplicate"), QList<quint32>(),
                                            [&](QList<quint32> &made) {
                for (quint32 id : fids)
                    doc->duplicateFunction(id, &made);   // all its own (UX-69)
                return !made.isEmpty();
            });
            if (changed)
                changed();
        }, QStringLiteral("ctxDuplicate"));
        if (fids.size() == 1)
        {
            const std::function<void(quint32)> inPlace = place.renameInPlace;
            menu.add(ContextMenu::Edit, renameText(!inPlace), [doc, fid, inPlace, parent, changed]() {
                if (inPlace)
                {
                    inPlace(fid);
                    return;
                }
                Function *fn = doc->function(fid);
                if (fn == nullptr)
                    return;
                bool ok = false;
                const QString n = QInputDialog::getText(parent, ContextMenu::tr("Rename"), ContextMenu::tr("Name:"),
                                                        QLineEdit::Normal, fn->name(), &ok).trimmed();
                if (!ok || n.isEmpty() || n == fn->name())
                    return;
                doc->paletteOps()->functionStep(ContextMenu::tr("Rename"), { fid }, [&](QList<quint32> &) {
                    fn->setName(n);
                    return true;
                });
                if (changed)
                    changed();
            }, QStringLiteral("ctxRename"));
        }
    }

    /* ---- 8. Delete, last ---- */
    if (editable)
    {
        const std::function<void(const QList<quint32> &)> remove = place.remove;
        menu.add(ContextMenu::Remove, deleteFunctionsText(fids.size()), [doc, fids, remove, parent, changed]() {
            if (remove)
            {
                remove(fids);
                return;
            }
            // Build's own: it asks naming every use, one undo step.
            if (ProgrammingManager *pm = ProgrammingManager::instance())
            {
                pm->deleteFunctions(fids);
                if (changed)
                    changed();
                return;
            }
            QStringList names;
            for (quint32 id : fids)
                if (Function *fn = doc->function(id))
                    names << fn->name();
            if (QMessageBox::question(parent, ContextMenu::tr("Delete"),
                                      ContextMenu::tr("Delete %1?").arg(names.join(QStringLiteral(", "))),
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
                return;
            for (quint32 id : fids)
                doc->deleteFunction(id);
            doc->setModified();
            if (changed)
                changed();
        }, QStringLiteral("ctxDeleteFunction"));
    }
}
