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
#include "fixturegroup.h"
#include "monitor.h"
#include "apputil.h"
#include "patchinventorydialog.h"
#include "doc.h"

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
        "from", "with", "as", "vs"
    };
    QString t = text;
    t.remove(QLatin1Char('&'));
    // Names and quotes are the user's own words, and a count is a number.
    t.remove(QRegularExpression(QStringLiteral("“[^”]*”|\"[^\"]*\"|\\([^)]*\\)")));
    const QStringList words = t.split(QRegularExpression(QStringLiteral("[\\s/]+")),
                                      Qt::SkipEmptyParts);
    for (int i = 0; i < words.size(); ++i)
    {
        const QString &w = words.at(i);
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

/****************************************************************************
 * The words
 ****************************************************************************/

QString ContextMenus::editHeadLayoutText()        { return ContextMenu::tr("Edit Head Layout…"); }
QString ContextMenus::fixturePropertiesText()     { return ContextMenu::tr("Fixture Properties…"); }
QString ContextMenus::locateText()                { return ContextMenu::tr("Locate"); }
QString ContextMenus::createGroupFromSelectionText() { return ContextMenu::tr("Create Fixture Group from Selection…"); }
QString ContextMenus::splitByHeadTypeText()       { return ContextMenu::tr("Split by Head Type…"); }
QString ContextMenus::layOutFromStageText()       { return ContextMenu::tr("Lay Out from the Stage…"); }

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
    if (fixtureIds.isEmpty() || Monitor::instance() == nullptr)
        return;
    Monitor::createAndShow(parent, doc);
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
    menu.add(ContextMenu::Kind, OutputRulesDialog::actionText(), [doc, groups, fixtures, parent, changed]() {
        if (OutputRulesDialog::edit(parent, doc, groups, fixtures) && changed)
            changed();
    }, QStringLiteral("outputRulesAction"))
        ->setToolTip(ContextMenu::tr("Power relays, house and work lights: ignore Blackout or the "
                                     "Grand Master, switch instead of fade, on when not in use"));
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

    /* ---- 3. More like it ---- */
    if (place.selectSimilar)
    {
        QAction *sim = menu.add(ContextMenu::Selection, selectAllSimilarText(place.similarCount),
                                place.selectSimilar, QStringLiteral("selectAllSimilar"));
        sim->setEnabled(place.similarCount != 1);       // 1: nothing like it but itself
        sim->setToolTip(ContextMenu::tr("Fixtures: every one of the same definition and mode. "
                                         "Fixture groups: every one made of the same fixtures, "
                                         "laid out the same way."));
    }

    /* ---- 4. A group of them; a folder for the groups ---- */
    if (editable && place.offerCreateGroup && (!fixtures.isEmpty() || groups.size() > 1))
    {
        const std::function<void()> create = place.createGroup;
        menu.add(ContextMenu::Organise, createGroupFromSelectionText(),
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
        }, QStringLiteral("ctxCreateGroupFromSelection"));
    }
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

    /* ---- 8. Delete, last. A group's fixtures stay. ---- */
    if (editable && place.offerDelete && !groups.isEmpty())
        menu.add(ContextMenu::Remove, deleteGroupsText(groups.size()), [doc, groups, parent, changed]() {
            if (deleteGroups(doc, groups, parent) && changed)
                changed();
        }, QStringLiteral("ctxDeleteGroup"));
}
