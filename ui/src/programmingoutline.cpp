/*
  Q Light Controller Plus
  programmingoutline.cpp

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

#include <QTreeWidgetItemIterator>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHeaderView>
#include <QScrollBar>
#include <QCollator>
#include <QMimeData>
#include <QDataStream>
#include <QSettings>
#include <QPainter>
#include <QPixmap>
#include <QKeyEvent>
#include <QTimer>
#include <QMenu>
#include <QIcon>
#include <algorithm>

#include "programmingoutline.h"
#include "focusmenu.h"
#include "itemicons.h"
#include "workbenchstack.h"
#include "scenegrouplooks.h"
#include "functionstreewidget.h"
#include "fixturegroupsource.h"
#include "showtreeview.h"
#include "showtreecontroller.h"
#include "lookstackcheck.h"
#include "speedmasters.h"
#include "fadetimespin.h"
#include "paletteops.h"
#include "qlceventpos.h"
#include "usedby.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "scenevalue.h"
#include "fixture.h"
#include "collection.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"

#define SETTINGS_BADGE_COLUMN QStringLiteral("programming/outline/badgeColumn")
#define SETTINGS_EXPANDED QStringLiteral("programming/outline/expanded")
#define SETTINGS_STACK_EXPANDED QStringLiteral("programming/workbench/expanded")
#define SETTINGS_STACK_BADGE_COLUMN QStringLiteral("programming/workbench/badgeColumn")

static const quint32 kInvalid = 0xFFFFFFFF;

ProgrammingOutline::ProgrammingOutline(Doc *doc, QWidget *parent)
    : QTreeWidget(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("programmingOutline"));
    setColumnCount(2);
    setHeaderLabels({ tr("Scenes"), QString() });
    setHeaderHidden(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::MoveAction);
    setAutoExpandDelay(600);           // hover a closed scene to open it mid-drag
    setUniformRowHeights(true);
    setAllColumnsShowFocus(true);
    setTextElideMode(Qt::ElideRight);
    // F2 (keyPressEvent) or a slow second click renames.
    setEditTriggers(QAbstractItemView::SelectedClicked);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setToolTip(QString());

    /* The names get the room; the badge column stays its width as the pane
       resizes, is dragged wider or narrower, and is remembered -- as the
       Show Tree's. */
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(QHeaderView::Interactive);
    header()->setMinimumSectionSize(24);
    m_badgeWidth = QSettings().value(SETTINGS_BADGE_COLUMN,
                                     fontMetrics().horizontalAdvance(QStringLiteral("⚠ 9  VC ×9  ▣ ⇄ ⏱")) + 12)
                       .toInt();
    connect(header(), &QHeaderView::sectionResized, this, [this](int section, int, int size) {
        if (m_fittingColumns)
            return;
        m_badgeWidth = qMax(24, section == 0 ? viewport()->width() - size : size);
        QSettings().setValue(m_mode == Stack ? SETTINGS_STACK_BADGE_COLUMN : SETTINGS_BADGE_COLUMN, m_badgeWidth);
        fitColumns();
    });

    m_rebuildTimer = new QTimer(this);
    m_rebuildTimer->setSingleShot(true);
    m_rebuildTimer->setInterval(120);
    connect(m_rebuildTimer, &QTimer::timeout, this, [this]() {
        // Never under an open rename: it would take the editor away.
        if (state() == QAbstractItemView::EditingState)
            m_rebuildTimer->start();
        else
            rebuild();
    });

    connect(this, &QTreeWidget::customContextMenuRequested, this, &ProgrammingOutline::slotContextMenu);
    // Stack mode: a layer that plays as it is opens on a double-click.
    connect(this, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *it, int) {
        if (m_mode != Stack || it == nullptr || it->parent() != nullptr)
            return;
        const Row r = rowOf(it);
        if (r.kind == FunctionRow)
            emit openRequested(r.id);
    });
    connect(this, &QTreeWidget::itemChanged, this, &ProgrammingOutline::slotItemChanged);
    connect(this, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem *it) {
        if (!m_building && m_filter.isEmpty())
        {
            m_expanded.insert(key(it), true);
            saveExpansion();
        }
    });
    connect(this, &QTreeWidget::itemCollapsed, this, [this](QTreeWidgetItem *it) {
        if (!m_building && m_filter.isEmpty())
        {
            m_expanded.insert(key(it), false);
            saveExpansion();
        }
    });

    // What the outline shows changes with the show.
    if (m_doc != nullptr)
    {
        /* One connect() per signal, never a loop over a braced list of
           &Doc::signal: on Windows (MinGW) such a list holds the import
           thunks' addresses, moc finds no signal there, and nothing is
           connected. */
        const auto rebuild = [this](quint32) { scheduleRebuild(); };
        connect(m_doc, &Doc::functionAdded, this, rebuild);
        connect(m_doc, &Doc::functionRemoved, this, rebuild);
        connect(m_doc, &Doc::functionChanged, this, rebuild);
        connect(m_doc, &Doc::fixtureGroupAdded, this, rebuild);
        connect(m_doc, &Doc::fixtureGroupRemoved, this, rebuild);
        connect(m_doc, &Doc::fixtureGroupChanged, this, rebuild);
        connect(m_doc, &Doc::fixtureChanged, this, rebuild);
        connect(m_doc, &Doc::fixtureRemoved, this, rebuild);
        connect(m_doc, &Doc::paletteAdded, this, rebuild);
        connect(m_doc, &Doc::paletteRemoved, this, rebuild);
        // Auto fixtures shown as their closest look, or not: their icons.
        connect(m_doc, &Doc::stageLooksChanged, this, [this]() { scheduleRebuild(); });
        connect(m_doc, &Doc::loaded, this, [this]() {
            m_expanded = QSettings().value(settingsKey()).toMap();
            scheduleRebuild();
        });
        connect(m_doc, &Doc::cleared, this, [this]() { scheduleRebuild(); });
        if (m_doc->paletteOps() != nullptr)
            connect(m_doc->paletteOps(), &PaletteOps::changed, this, [this]() { scheduleRebuild(); });
        m_expanded = QSettings().value(settingsKey()).toMap();
    }
    connect(this, &QTreeWidget::itemSelectionChanged, this, &ProgrammingOutline::trackSelectionOrder);
}

ProgrammingOutline::~ProgrammingOutline()
{
}

const char *ProgrammingOutline::rowMimeType()
{
    return "application/x-qlcconsole-outline-rows";
}

/****************************************************************************
 * Building
 ****************************************************************************/

QString ProgrammingOutline::settingsKey() const
{
    const QString where = m_doc != nullptr ? m_doc->workspacePath() : QString();
    const QString show = QString::number(qHash(where.isEmpty() ? QStringLiteral("(new)") : where));
    if (m_mode == Stack)
        return SETTINGS_STACK_EXPANDED + QLatin1Char('/') + show + QLatin1Char('/')
               + (m_stack != nullptr ? m_stack->key() : QStringLiteral("-"));
    /* Per show: by where it is kept (Doc knows the folder; one show per
       folder is the usual case). */
    return SETTINGS_EXPANDED + QLatin1Char('/') + show;
}

void ProgrammingOutline::saveExpansion()
{
    QSettings().setValue(settingsKey(), m_expanded);
}

QString ProgrammingOutline::key(const QTreeWidgetItem *item) const
{
    return item != nullptr ? item->data(NameCol, KeyRole).toString() : QString();
}

ProgrammingOutline::Row ProgrammingOutline::rowOf(const QTreeWidgetItem *item)
{
    Row r;
    if (item == nullptr)
        return r;
    r.kind = item->data(NameCol, KindRole).toInt();
    r.id = item->data(NameCol, IdRole).toUInt();
    r.scene = r.kind == SceneRow || r.kind == FunctionRow ? r.id : item->data(NameCol, SceneRole).toUInt();
    return r;
}

static QString rowKey(int kind, quint32 scene, quint32 id, const QString &path = QString())
{
    switch (kind)
    {
    case ProgrammingOutline::FolderRow:  return QStringLiteral("f:") + path;
    case ProgrammingOutline::SceneRow:   return QStringLiteral("s:%1").arg(id);
    case ProgrammingOutline::FunctionRow: return QStringLiteral("o:%1").arg(id);
    case ProgrammingOutline::GroupRow:   return QStringLiteral("g:%1:%2").arg(scene).arg(id);
    case ProgrammingOutline::FixtureRow: return QStringLiteral("x:%1:%2").arg(scene).arg(id);
    case ProgrammingOutline::PaletteRow: return QStringLiteral("p:%1:%2").arg(scene).arg(id);
    case ProgrammingOutline::CueLayerRow: return QStringLiteral("c:%1").arg(id);
    case ProgrammingOutline::AttributeRow: return QStringLiteral("a:%1:%2").arg(scene).arg(id);
    default:                             return QStringLiteral("h");
    }
}

QTreeWidgetItem *ProgrammingOutline::itemFor(const Row &row) const
{
    // W3: a cue's rows are keyed by their layer's target (stable while the
    // layers around them change); found by place here.
    if (row.kind == CueLayerRow || row.kind == AttributeRow)
    {
        for (QTreeWidgetItemIterator it(const_cast<ProgrammingOutline *>(this)); *it; ++it)
            if (rowOf(*it) == row)
                return *it;
        return nullptr;
    }
    const QString k = rowKey(row.kind, row.scene, row.id);
    for (QTreeWidgetItemIterator it(const_cast<ProgrammingOutline *>(this)); *it; ++it)
        if (key(*it) == k)
            return *it;
    return nullptr;
}

QTreeWidgetItem *ProgrammingOutline::sceneItem(quint32 sceneId) const
{
    Row r;
    r.kind = SceneRow;
    r.scene = r.id = sceneId;
    return itemFor(r);
}

bool ProgrammingOutline::defaultExpanded(const QTreeWidgetItem *item) const
{
    // Folders open, scenes closed (open what you work on), an effect shows
    // its own colour and dimmer. On the Workbench a layer is open: it is
    // what you loaded it for.
    const int kind = item->data(NameCol, KindRole).toInt();
    if (m_mode == Stack && (kind == SceneRow || kind == CueLayerRow))
        return true;
    return kind == FolderRow || kind == PaletteRow;
}

QTreeWidgetItem *ProgrammingOutline::folderItem(const QString &path,
                                                QHash<QString, QTreeWidgetItem *> &folders)
{
    if (path.isEmpty())
        return nullptr;
    if (QTreeWidgetItem *have = folders.value(path))
        return have;
    const int cut = path.lastIndexOf(QLatin1Char('/'));
    QTreeWidgetItem *parent = cut > 0 ? folderItem(path.left(cut), folders) : nullptr;
    QTreeWidgetItem *it = parent != nullptr ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(this);
    it->setText(NameCol, cut > 0 ? path.mid(cut + 1) : path);
    it->setIcon(NameCol, QIcon(QStringLiteral(":/folder.png")));
    it->setData(NameCol, KindRole, FolderRow);
    it->setData(NameCol, PathRole, path);
    it->setData(NameCol, KeyRole, rowKey(FolderRow, kInvalid, kInvalid, path));
    it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled | Qt::ItemIsEditable);
    folders.insert(path, it);
    return it;
}

static QString paletteName(const QLCPalette *p)
{
    if (p == nullptr)
        return QString();
    if (!p->name().isEmpty())
        return p->name();
    const QString type = QLCPalette::typeToString(p->type());
    if (p->type() == QLCPalette::Color)
        return QStringLiteral("%1 %2").arg(type, p->rgbValue().name());
    return type;
}

void ProgrammingOutline::decorateScene(QTreeWidgetItem *item, Scene *scene)
{
    /* Badges, the details on hover: ⚠ what won't light or doesn't do what it
       seems, VC ×n what fires it, ▣ raw channel values, ⇄ a transition,
       ⏱ a speed master. */
    QStringList badges, tips, problems;
    const QSet<QLCPalette::PaletteType> covered = SceneGroupLooks::coveredTypes(m_doc, scene);
    const QHash<quint32, QStringList> cannotReach = SceneGroupLooks::headsThatCannotReach(m_doc, scene);

    QSet<quint32> all;
    for (quint32 gid : scene->fixtureGroups())
        if (FixtureGroup *g = m_doc->fixtureGroup(gid))
        {
            QStringList missing;
            for (quint32 fid : g->fixtureList())
            {
                all.insert(fid);
                for (const QString &m : SceneGroupLooks::missingLooks(m_doc->fixture(fid), covered))
                    if (!missing.contains(m))
                        missing << m;
            }
            if (!missing.isEmpty())
                problems << tr("%1: no look for %2").arg(g->name(), missing.join(QStringLiteral(", ")));
        }
    for (quint32 fid : scene->fixtures())
    {
        all.insert(fid);
        Fixture *f = m_doc->fixture(fid);
        const QStringList missing = SceneGroupLooks::missingLooks(f, covered);
        // A fixed target with raw values for it covers itself.
        bool hasValues = false;
        for (const SceneValue &v : scene->values())
            hasValues = hasValues || v.fxi == fid;
        if (f != nullptr && !missing.isEmpty() && !hasValues)
            problems << tr("%1: no look for %2").arg(f->name(), missing.join(QStringLiteral(", ")));
    }
    if (!cannotReach.isEmpty())
        problems << tr("%n head(s) can't reach their aim target", "", cannotReach.size());
    int unpatched = 0;
    for (quint32 fid : all)
        if (Fixture *f = m_doc->fixture(fid))
            if (!f->isPatched())
                ++unpatched;
    if (unpatched > 0)
        problems << tr("%n fixture(s) not patched: they send no DMX", "", unpatched);
    for (const LookStackCheck::Issue &i : LookStackCheck::check(m_doc, scene))
        problems << i.text;

    if (!problems.isEmpty())
    {
        badges << tr("⚠ %1").arg(problems.size());
        tips << problems;
    }
    const QStringList vc = UsedBy::vcLines({ scene->id() });
    if (!vc.isEmpty())
    {
        badges << tr("VC ×%1").arg(vc.size());
        tips << tr("Fired by: %1").arg(vc.join(QStringLiteral(", ")));
    }
    if (!scene->values().isEmpty())
    {
        badges << QStringLiteral("▣");
        tips << tr("Sets %n raw channel value(s) directly", "", scene->values().size());
    }
    if (scene->transitionIn().isActive() || scene->transitionOut().isActive())
    {
        badges << QStringLiteral("⇄");
        tips << tr("Has a transition (how the look arrives or leaves across the rig)");
    }
    if (scene->speedMaster() != SpeedMasters::Inherit)
    {
        badges << QStringLiteral("⏱");
        tips << tr("Its effects keep time with a speed master");
    }
    item->setText(BadgeCol, badges.join(QStringLiteral("  ")));
    item->setToolTip(BadgeCol, tips.join(QLatin1Char('\n')));
    if (!problems.isEmpty())
        item->setForeground(BadgeCol, QColor(208, 138, 42));
    else
        item->setForeground(BadgeCol, palette().color(QPalette::Disabled, QPalette::Text));

    item->setToolTip(NameCol, (m_mode == Stack
                                   ? tr("Scene “%1”: %n fixture(s). Click to edit its timing; "
                                        "its targets and looks are the rows under it.", "", all.size())
                                   : tr("Scene “%1”: %n fixture(s). Click to load it in the Workbench; "
                                        "open it here to see its targets and looks.", "", all.size()))
                     .arg(scene->name())
                     + (tips.isEmpty() ? QString() : QStringLiteral("\n\n") + tips.join(QLatin1Char('\n'))));
}

void ProgrammingOutline::addSceneRows(QTreeWidgetItem *sceneItem, Scene *scene)
{
    const quint32 sid = scene->id();
    const QSet<QLCPalette::PaletteType> covered = SceneGroupLooks::coveredTypes(m_doc, scene);
    const QHash<quint32, QStringList> cannotReach = SceneGroupLooks::headsThatCannotReach(m_doc, scene);
    const QColor warn(208, 138, 42);
    const QColor grey = palette().color(QPalette::Disabled, QPalette::Text);
    QCollator coll;
    coll.setNumericMode(true);
    coll.setCaseSensitivity(Qt::CaseInsensitive);

    // Targets: groups (dynamic), then fixed fixtures.
    QList<FixtureGroup *> groups;
    for (quint32 gid : scene->fixtureGroups())
        if (FixtureGroup *g = m_doc->fixtureGroup(gid))
            groups << g;
    std::sort(groups.begin(), groups.end(), [&coll](FixtureGroup *a, FixtureGroup *b) {
        return coll.compare(a->name(), b->name()) < 0;
    });
    for (FixtureGroup *g : groups)
    {
        QTreeWidgetItem *it = new QTreeWidgetItem(sceneItem);
        it->setText(NameCol, g->name());
        it->setIcon(NameCol, ItemIcons::groupIcon(m_doc, g));
        it->setData(NameCol, KindRole, GroupRow);
        it->setData(NameCol, IdRole, g->id());
        it->setData(NameCol, SceneRole, sid);
        it->setData(NameCol, KeyRole, rowKey(GroupRow, sid, g->id()));
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled
                     | Qt::ItemIsDropEnabled);
        QStringList issues, missing;
        int shortOf = 0, unpatched = 0;
        for (quint32 fid : g->fixtureList())
        {
            Fixture *f = m_doc->fixture(fid);
            for (const QString &m : SceneGroupLooks::missingLooks(f, covered))
                if (!missing.contains(m))
                    missing << m;
            if (cannotReach.contains(fid))
                ++shortOf;
            if (f != nullptr && !f->isPatched())
                ++unpatched;
        }
        if (!missing.isEmpty())
            issues << tr("No look for %1").arg(missing.join(QStringLiteral(", ")));
        if (shortOf > 0)
            issues << tr("%n head(s) can't reach their aim target", "", shortOf);
        if (unpatched > 0)
            issues << tr("%n not patched: they send no DMX", "", unpatched);
        // How many it holds (the details on hover); ⚠ when something's wrong.
        it->setText(BadgeCol, issues.isEmpty() ? tr("%1 fx").arg(g->fixtureList().size())
                                               : tr("⚠ %1 fx").arg(g->fixtureList().size()));
        it->setForeground(BadgeCol, issues.isEmpty() ? grey : warn);
        const QString tip = tr("Group “%1”, a dynamic target: %n fixture(s). The scene's looks "
                               "follow its members.", "", g->fixtureList().size()).arg(g->name());
        it->setToolTip(NameCol, issues.isEmpty() ? tip : tip + QStringLiteral("\n\n") + issues.join(QLatin1Char('\n')));
        it->setToolTip(BadgeCol, issues.isEmpty() ? tr("%n fixture(s)", "", g->fixtureList().size())
                                                  : issues.join(QLatin1Char('\n')));
    }

    QList<Fixture *> fixed;
    for (quint32 fid : scene->fixtures())
        if (Fixture *f = m_doc->fixture(fid))
            fixed << f;
    std::sort(fixed.begin(), fixed.end(), [&coll](Fixture *a, Fixture *b) {
        return coll.compare(a->name(), b->name()) < 0;
    });
    for (Fixture *f : fixed)
    {
        QTreeWidgetItem *it = new QTreeWidgetItem(sceneItem);
        it->setText(NameCol, f->name());
        it->setIcon(NameCol, ItemIcons::fixtureIcon(f));
        it->setData(NameCol, KindRole, FixtureRow);
        it->setData(NameCol, IdRole, f->id());
        it->setData(NameCol, SceneRole, sid);
        it->setData(NameCol, KeyRole, rowKey(FixtureRow, sid, f->id()));
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled
                     | Qt::ItemIsDropEnabled);
        int values = 0;
        for (const SceneValue &v : scene->values())
            if (v.fxi == f->id())
                ++values;
        QStringList issues;
        const QStringList missing = SceneGroupLooks::missingLooks(f, covered);
        if (!missing.isEmpty() && values == 0)
            issues << tr("No look for %1").arg(missing.join(QStringLiteral(", ")));
        if (cannotReach.contains(f->id()))
            issues << tr("Can't reach: %1").arg(cannotReach.value(f->id()).join(QStringLiteral(", ")));
        if (!f->isPatched())
            issues << tr("Not patched: it sends no DMX");
        QStringList badge;
        if (!issues.isEmpty())
            badge << QStringLiteral("⚠");
        if (values > 0)
            badge << QStringLiteral("▣ %1").arg(values);
        it->setText(BadgeCol, badge.join(QLatin1Char(' ')));
        it->setForeground(BadgeCol, issues.isEmpty() ? grey : warn);
        QString tip = tr("Fixture “%1”, a fixed target.").arg(f->name());
        if (values > 0)
            tip += QLatin1Char(' ') + tr("The scene sets %n of its channels directly.", "", values);
        it->setToolTip(NameCol, issues.isEmpty() ? tip : tip + QStringLiteral("\n\n") + issues.join(QLatin1Char('\n')));
        it->setToolTip(BadgeCol, (issues + QStringList(values > 0 ? tr("%n raw value(s)", "", values) : QString()))
                                     .join(QLatin1Char('\n')).trimmed());
    }

    // Looks, in the scene's order; an effect's own colour/dimmer under it.
    QHash<quint32, QStringList> stack;
    for (const LookStackCheck::Issue &i : LookStackCheck::check(m_doc, scene))
        if (i.paletteId != QLCPalette::invalidId())
            stack[i.paletteId] << i.text;
    const QList<quint32> pals = scene->palettes();
    QTreeWidgetItem *effectItem = nullptr;
    QLCPalette *effect = nullptr;
    for (quint32 pid : pals)
    {
        QLCPalette *p = m_doc->palette(pid);
        const bool isEffect = p != nullptr && p->type() == QLCPalette::Effect;
        const bool nested = !isEffect && effectItem != nullptr
                            && QLCPalette::isEffectScoped(m_doc, pals, pid);
        QTreeWidgetItem *it = new QTreeWidgetItem(nested ? effectItem : sceneItem);
        it->setText(NameCol, p != nullptr ? paletteName(p) : tr("(missing palette %1)").arg(pid));
        if (p != nullptr)
            it->setIcon(NameCol, ItemIcons::paletteIcon(m_doc, p));
        it->setData(NameCol, KindRole, PaletteRow);
        it->setData(NameCol, IdRole, pid);
        it->setData(NameCol, SceneRole, sid);
        it->setData(NameCol, KeyRole, rowKey(PaletteRow, sid, pid));
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled
                     | Qt::ItemIsDropEnabled | (p != nullptr ? Qt::ItemIsEditable : Qt::NoItemFlags));

        QString where;
        if (p != nullptr)
        {
            QString path = p->path();
            if (path.startsWith(QStringLiteral("Palettes/")))
                path = path.mid(9);
            else if (path == QStringLiteral("Palettes"))
                path.clear();
            if (path.endsWith(QLatin1Char('/')))
                path.chop(1);
            where = path;
        }
        QString tip = isEffect ? tr("Effect “%1”. Looks nested under it are its own.").arg(it->text(NameCol))
                    : nested ? (QLCPalette::effectUsesLookColours(effect)
                                ? tr("“%1”: the effect above's own colour or level, not painted on the "
                                     "fixtures directly.").arg(it->text(NameCol))
                                : tr("“%1”: applied with the effect above, on the heads it drives.")
                                      .arg(it->text(NameCol)))
                             : tr("Look “%1”: lights the scene's targets directly.").arg(it->text(NameCol));
        if (p != nullptr)
            tip += QLatin1Char(' ') + tr("A %1 palette%2.").arg(QLCPalette::typeToString(p->type()),
                                     where.isEmpty() ? QString() : tr(" in %1").arg(where));
        tip += QLatin1Char(' ') + tr("Lower rows win where they share a channel.");

        QStringList badge, btips;
        if (stack.contains(pid))
        {
            badge << QStringLiteral("⚠");
            btips << stack.value(pid);
        }
        const int fin = scene->paletteFadeIn(pid), fout = scene->paletteFadeOut(pid);
        if ((fin >= 0 && !isEffect && !nested) || (fout >= 0 && !nested))
        {
            badge << QStringLiteral("◷");
            btips << tr("Its own fade: in %1, out %2")
                         .arg(FadeTimeSpin::text(fin, tr("scene")), FadeTimeSpin::text(fout, tr("scene")));
        }
        it->setText(BadgeCol, badge.join(QLatin1Char(' ')));
        it->setForeground(BadgeCol, stack.contains(pid) ? warn : grey);
        it->setToolTip(BadgeCol, btips.join(QLatin1Char('\n')));
        it->setToolTip(NameCol, btips.isEmpty() ? tip : tip + QStringLiteral("\n\n") + btips.join(QLatin1Char('\n')));
        if (isEffect)
        {
            effectItem = it;
            effect = p;
        }
    }
}

QString ProgrammingOutline::focusThisText() const
{
    // A Show Tree cue on the Workbench is a cue; anything from Build, a look.
    return m_stack != nullptr && m_stack->inherits("TreeCueStack") ? tr("Focus this cue\342\200\246")
                                                                   : tr("Focus this look\342\200\246");
}

void ProgrammingOutline::setStack(WorkbenchStack *stack)
{
    if (m_mode == Stack && m_stack == stack)
        return;
    if (m_mode == Navigator)
    {
        // The Workbench's rows: one per layer, and a layer moves as one.
        setObjectName(QStringLiteral("workbenchRows"));
        setHeaderLabels({ tr("Layers"), QString() });
        // Its badges say more ("also in 2  Collection ▣"): a column of its own.
        m_badgeWidth = QSettings().value(SETTINGS_STACK_BADGE_COLUMN,
                                         fontMetrics().horizontalAdvance(QStringLiteral("also in 99  Collection ▣  ⚠ 9"))
                                             + 12).toInt();
    }
    m_mode = Stack;
    m_stack = stack;
    m_expanded = QSettings().value(settingsKey()).toMap();
    if (m_stack != nullptr)
        connect(m_stack, &QObject::destroyed, this, [this, stack]() {
            if (m_stack == stack)
                m_stack = nullptr;
        });
    rebuild();
}

int ProgrammingOutline::layerOf(const QTreeWidgetItem *item) const
{
    if (m_mode != Stack || item == nullptr)
        return -1;
    while (item->parent() != nullptr)
        item = item->parent();
    const QVariant v = item->data(NameCol, LayerRole);
    return v.isValid() ? v.toInt() : -1;
}

QTreeWidgetItem *ProgrammingOutline::layerItem(int index) const
{
    for (int i = 0; i < topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *it = topLevelItem(i);
        const QVariant v = it->data(NameCol, LayerRole);
        if (v.isValid() && v.toInt() == index)
            return it;
    }
    return nullptr;
}

void ProgrammingOutline::rebuild()
{
    m_rebuildTimer->stop();
    m_dirty = false;
    m_building = true;

    QStringList selected;
    for (QTreeWidgetItem *it : selectedItems())
        selected << key(it);
    const QString current = key(currentItem());
    const int scroll = verticalScrollBar()->value();
    // Keys a finished edit asked for win over what was selected.
    const QStringList pending = property("pendingKeys").toStringList();
    setProperty("pendingKeys", QVariant());

    QSignalBlocker block(this);
    QSignalBlocker blockSel(selectionModel());
    clear();

    QList<Scene *> scenes;
    if (m_mode == Stack)
        rebuildStack();
    else
        rebuildNavigator(scenes);

    // As it was left: open rows, selection, scroll.
    for (QTreeWidgetItemIterator it(this); *it; ++it)
    {
        const QString k = key(*it);
        if ((*it)->childCount() > 0)
            (*it)->setExpanded(m_expanded.contains(k) ? m_expanded.value(k).toBool()
                                                      : defaultExpanded(*it));
    }
    const QStringList want = pending.isEmpty() ? selected : pending;
    QTreeWidgetItem *cur = nullptr;
    for (QTreeWidgetItemIterator it(this); *it; ++it)
    {
        const QString k = key(*it);
        if (want.contains(k))
        {
            (*it)->setSelected(true);
            for (QTreeWidgetItem *a = (*it)->parent(); a != nullptr; a = a->parent())
                if (!pending.isEmpty())
                    a->setExpanded(true);
            if (cur == nullptr || k == current)
                cur = *it;
        }
    }
    if (cur != nullptr)
        setCurrentItem(cur, NameCol, QItemSelectionModel::NoUpdate);
    verticalScrollBar()->setValue(scroll);
    if (!pending.isEmpty() && cur != nullptr)
        scrollToItem(cur);

    applyFilter();
    m_building = false;
    fitColumns();
}

void ProgrammingOutline::rebuildNavigator(QList<Scene *> &scenes)
{
    QCollator coll;
    coll.setNumericMode(true);
    coll.setCaseSensitivity(Qt::CaseInsensitive);

    /* Scenes, collections and chasers are the Scenes section's own: each
       makes its folder. Anything else (an EFX, a show...) filed in one of
       those folders shows there too, so nothing seems to vanish. */
    QList<Function *> others;
    QSet<QString> folderSet;
    auto addFolders = [&folderSet](QString p) {
        while (!p.isEmpty())
        {
            folderSet.insert(p);
            const int cut = p.lastIndexOf(QLatin1Char('/'));
            p = cut > 0 ? p.left(cut) : QString();
        }
    };
    if (m_doc != nullptr)
    {
        ShowTreeController *st = m_doc->showTree();
        for (Function *f : m_doc->functions())
        {
            if (f == nullptr || !f->isVisible() || (st != nullptr && st->isGenerated(f->id())))
                continue;
            if (Scene *s = qobject_cast<Scene *>(f))
            {
                scenes << s;
                addFolders(s->path(true));
            }
            else
            {
                others << f;
                if (f->type() == Function::CollectionType || f->type() == Function::ChaserType)
                    addFolders(f->path(true));
            }
        }
    }

    // Folders first (A-Z, numbers as numbers), then what is in each.
    QStringList folderList = folderSet.values();
    std::sort(folderList.begin(), folderList.end(), [&coll](const QString &a, const QString &b) {
        return coll.compare(a, b) < 0;
    });
    QHash<QString, QTreeWidgetItem *> folders;
    for (const QString &p : folderList)
        folderItem(p, folders);

    std::sort(scenes.begin(), scenes.end(), [&coll](Scene *a, Scene *b) {
        return coll.compare(a->name(), b->name()) < 0;
    });
    for (Scene *s : scenes)
    {
        QTreeWidgetItem *parent = folders.value(s->path(true));
        QTreeWidgetItem *it = parent != nullptr ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(this);
        it->setText(NameCol, s->name());
        it->setIcon(NameCol, QIcon(QStringLiteral(":/scene.png")));
        it->setData(NameCol, KindRole, SceneRow);
        it->setData(NameCol, IdRole, s->id());
        it->setData(NameCol, KeyRole, rowKey(SceneRow, s->id(), s->id()));
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled
                     | Qt::ItemIsDropEnabled | Qt::ItemIsEditable);
        decorateScene(it, s);
        addSceneRows(it, s);
    }

    /* Collections and chasers -- and the rest filed beside them -- are full
       rows: click one and the Workbench loads it (docs/design/WORKFLOW_DESIGN.md §3.3,
       reversing P-D12). */
    std::sort(others.begin(), others.end(), [&coll](Function *a, Function *b) {
        return coll.compare(a->name(), b->name()) < 0;
    });
    const QColor grey = palette().color(QPalette::Disabled, QPalette::Text);
    for (Function *f : others)
    {
        const bool own = f->type() == Function::CollectionType || f->type() == Function::ChaserType;
        QTreeWidgetItem *parent = folders.value(f->path(true));
        if (parent == nullptr && !own)
            continue;
        QTreeWidgetItem *it = parent != nullptr ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(this);
        it->setText(NameCol, f->name());
        it->setIcon(NameCol, f->getIcon());
        it->setData(NameCol, KindRole, FunctionRow);
        it->setData(NameCol, IdRole, f->id());
        it->setData(NameCol, KeyRole, rowKey(FunctionRow, f->id(), f->id()));
        // A chaser takes scenes dropped on it as steps (W2).
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled
                     | Qt::ItemIsEditable
                     | (f->type() == Function::ChaserType ? Qt::ItemIsDropEnabled : Qt::NoItemFlags));
        QString badge = Function::typeToString(f->type()).toLower();
        if (Collection *c = qobject_cast<Collection *>(f))
            badge = tr("collection · %1").arg(c->functions().size());
        else if (Chaser *ch = qobject_cast<Chaser *>(f))
            badge = tr("chaser · %1").arg(ch->stepsCount());
        const QStringList vc = UsedBy::vcLines({ f->id() });
        if (!vc.isEmpty())
            badge += QStringLiteral("  ") + tr("VC ×%1").arg(vc.size());
        it->setText(BadgeCol, badge);
        it->setForeground(BadgeCol, grey);
        QString tip = tr("A %1: click to load it in the Workbench.")
                          .arg(Function::typeToString(f->type()).toLower());
        if (!vc.isEmpty())
            tip += QLatin1Char('\n') + tr("Fired by: %1").arg(vc.join(QStringLiteral(", ")));
        it->setToolTip(NameCol, tip);
        it->setToolTip(BadgeCol, tip);
    }

    if (scenes.isEmpty() && topLevelItemCount() == 0)
    {
        QTreeWidgetItem *it = new QTreeWidgetItem(this);
        it->setText(NameCol, tr("No scenes yet: + New above to start one, then drag groups and palettes onto it."));
        it->setData(NameCol, KindRole, HintRow);
        it->setData(NameCol, KeyRole, rowKey(HintRow, kInvalid, kInvalid));
        it->setFlags(Qt::ItemIsEnabled);
        it->setForeground(NameCol, grey);
    }
}

void ProgrammingOutline::rebuildStack()
{
    const QColor grey = palette().color(QPalette::Disabled, QPalette::Text);
    auto hint = [this, grey](const QString &text) {
        QTreeWidgetItem *it = new QTreeWidgetItem(this);
        it->setText(NameCol, text);
        it->setData(NameCol, KindRole, HintRow);
        it->setData(NameCol, KeyRole, rowKey(HintRow, kInvalid, kInvalid));
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsDropEnabled);
        it->setForeground(NameCol, grey);
        it->setToolTip(NameCol, text);
    };
    if (m_stack == nullptr)
    {
        hint(tr("Nothing loaded: click a scene, collection or chaser on the left."));
        return;
    }
    const bool readOnly = !m_stack->readOnlyReason().isEmpty();
    const QList<WorkbenchStack::Layer> layers = m_stack->layers();
    int index = 0;
    for (const WorkbenchStack::Layer &l : layers)
    {
        if (l.kind == WorkbenchStack::Layer::CueLayer)
        {
            addCueLayerRows(l, index++);
            continue;
        }
        Function *f = m_doc->function(l.functionId);
        Scene *s = l.kind == WorkbenchStack::Layer::SceneLayer ? qobject_cast<Scene *>(f) : nullptr;
        QTreeWidgetItem *it = new QTreeWidgetItem(this);
        it->setText(NameCol, !l.label.isEmpty() ? l.label : f != nullptr ? f->name() : QString());
        it->setData(NameCol, IdRole, l.functionId);
        it->setData(NameCol, LayerRole, index);
        const Qt::ItemFlags edit = readOnly ? Qt::NoItemFlags
                                            : Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
        if (s != nullptr)
        {
            it->setIcon(NameCol, QIcon(QStringLiteral(":/scene.png")));
            it->setData(NameCol, KindRole, SceneRow);
            it->setData(NameCol, KeyRole, rowKey(SceneRow, s->id(), s->id()));
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | edit
                         | (readOnly ? Qt::NoItemFlags : Qt::ItemIsEditable));
            decorateScene(it, s);
            addSceneRows(it, s);
            if (readOnly)
                for (QTreeWidgetItemIterator c(it); *c; ++c)
                    (*c)->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        }
        else
        {
            if (f != nullptr)
                it->setIcon(NameCol, f->getIcon());
            it->setData(NameCol, KindRole, FunctionRow);
            // A chaser may play one function in two steps: the place tells them apart.
            it->setData(NameCol, KeyRole, QStringLiteral("o:%1:%2").arg(l.functionId).arg(index));
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | edit);
            it->setForeground(BadgeCol, grey);
        }
        if (!l.badge.isEmpty())
        {
            const QString had = it->text(BadgeCol);
            it->setText(BadgeCol, had.isEmpty() ? l.badge : l.badge + QStringLiteral("  ") + had);
        }
        if (!l.tip.isEmpty())
        {
            const QString had = it->toolTip(BadgeCol);
            it->setToolTip(BadgeCol, had.isEmpty() ? l.tip : l.tip + QLatin1Char('\n') + had);
            it->setToolTip(NameCol, it->toolTip(NameCol).isEmpty() ? l.tip
                                    : it->toolTip(NameCol) + QStringLiteral("\n\n") + l.tip);
        }
        if (l.tracked)
            it->setForeground(NameCol, grey);
        ++index;
    }
    if (layers.isEmpty())
        hint(m_stack->canAddLayers()
                 ? tr("No layers yet: + Scene, + EFX or + RGB Matrix below, or drag scenes here.")
                 : tr("Nothing in it."));
}

void ProgrammingOutline::addCueLayerRows(const WorkbenchStack::Layer &l, int index)
{
    /* W3: a Show Tree cue's layer, as the tree shows it -- one row per
       attribute set or carried in; what is carried in (a tracked layer, a
       value from an earlier cue) is grey and says where it comes from.
       Every edit goes through the back end (the tree's controller). */
    const QColor grey = palette().color(QPalette::Disabled, QPalette::Text);
    QTreeWidgetItem *it = new QTreeWidgetItem(this);
    it->setText(NameCol, l.label);
    it->setIcon(NameCol, ItemIcons::signatureIcon(m_doc, l.targetSignature));
    it->setData(NameCol, KindRole, CueLayerRow);
    it->setData(NameCol, IdRole, index);
    it->setData(NameCol, SceneRole, kInvalid);
    it->setData(NameCol, LayerRole, index);
    it->setData(NameCol, KeyRole, QStringLiteral("c:") + l.targetSignature);
    it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled);
    it->setText(BadgeCol, l.badge);
    it->setToolTip(NameCol, l.tip);
    it->setToolTip(BadgeCol, l.tip);
    it->setForeground(BadgeCol, grey);
    if (l.tracked)
    {
        it->setForeground(NameCol, grey);
        QFont f = it->font(NameCol);
        f.setItalic(true);
        it->setFont(NameCol, f);
    }
    int a = 0;
    for (const WorkbenchStack::Layer::Attribute &attr : l.attributes)
    {
        QTreeWidgetItem *row = new QTreeWidgetItem(it);
        row->setText(NameCol, QStringLiteral("%1  %2").arg(attr.label, attr.value));
        row->setText(BadgeCol, attr.origin);
        row->setData(NameCol, KindRole, AttributeRow);
        row->setData(NameCol, IdRole, a);
        row->setData(NameCol, SceneRole, index);
        row->setData(NameCol, KeyRole, QStringLiteral("a:%1:%2").arg(l.targetSignature, attr.key));
        row->setData(NameCol, PathRole, attr.key);
        // Press and drag moves it to another layer (⌥ copies), as a scene's
        // look rows; it doesn't rubber-band a selection. ⇧/⌘-click select.
        row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled
                      | (attr.inherited && attr.value.isEmpty() ? Qt::NoItemFlags : Qt::ItemIsDragEnabled));
        if (QLCPalette *p = m_doc->palette(attr.palette))
            row->setIcon(NameCol, ItemIcons::paletteIcon(m_doc, p));
        const QString tip = attr.origin.isEmpty() ? tr("%1: %2, set here").arg(attr.label, attr.value)
                                                  : tr("%1: %2 — %3").arg(attr.label, attr.value, attr.origin);
        row->setToolTip(NameCol, tip);
        row->setToolTip(BadgeCol, tip);
        row->setForeground(BadgeCol, grey);
        if (attr.inherited)
        {
            row->setForeground(NameCol, grey);
            QFont f = row->font(NameCol);
            f.setItalic(true);
            row->setFont(NameCol, f);
        }
        ++a;
    }
}

void ProgrammingOutline::scheduleRebuild()
{
    if (!isVisible())
    {
        m_dirty = true;
        return;
    }
    m_rebuildTimer->start();
}

void ProgrammingOutline::showEvent(QShowEvent *event)
{
    QTreeWidget::showEvent(event);
    if (m_dirty)
        rebuild();
    fitColumns();
}

void ProgrammingOutline::resizeEvent(QResizeEvent *event)
{
    QTreeWidget::resizeEvent(event);
    fitColumns();
}

void ProgrammingOutline::fitColumns()
{
    // The badges keep their width (dragged, remembered); the names take the
    // rest, and never less than 60%.
    const int w = viewport()->width();
    if (w <= 0)
        return;
    const int badge = qMin(m_badgeWidth, w * 2 / 5);
    m_fittingColumns = true;
    setColumnWidth(NameCol, w - badge);
    setColumnWidth(BadgeCol, badge);
    m_fittingColumns = false;
}

void ProgrammingOutline::setFilterText(const QString &text)
{
    m_filter = text.trimmed();
    applyFilter();
}

void ProgrammingOutline::applyFilter()
{
    const bool was = m_building;
    m_building = true;
    std::function<bool(QTreeWidgetItem *, bool)> visit = [&](QTreeWidgetItem *it, bool parentMatch) -> bool {
        const bool match = m_filter.isEmpty()
                           || it->text(NameCol).contains(m_filter, Qt::CaseInsensitive);
        bool child = false;
        for (int i = 0; i < it->childCount(); ++i)
            child = visit(it->child(i), parentMatch || match) || child;
        const bool show = match || child || parentMatch;
        it->setHidden(!show);
        if (!m_filter.isEmpty() && child)
            it->setExpanded(true);
        return match || child;
    };
    for (int i = 0; i < topLevelItemCount(); ++i)
        visit(topLevelItem(i), false);
    if (m_filter.isEmpty())
        for (QTreeWidgetItemIterator it(this); *it; ++it)
            if ((*it)->childCount() > 0)
            {
                const QString k = key(*it);
                (*it)->setExpanded(m_expanded.contains(k) ? m_expanded.value(k).toBool()
                                                          : defaultExpanded(*it));
            }
    m_building = was;
}

void ProgrammingOutline::expandEverything()
{
    for (QTreeWidgetItemIterator it(this); *it; ++it)
        if ((*it)->childCount() > 0)
            m_expanded.insert(key(*it), true);
    m_building = true;
    expandAll();
    m_building = false;
    saveExpansion();
}

void ProgrammingOutline::collapseEverything()
{
    for (QTreeWidgetItemIterator it(this); *it; ++it)
        if ((*it)->childCount() > 0)
            m_expanded.insert(key(*it), false);
    m_building = true;
    collapseAll();
    m_building = false;
    saveExpansion();
}

void ProgrammingOutline::setSubtreeExpanded(QTreeWidgetItem *item, bool expanded)
{
    if (item == nullptr)
        return;
    m_building = true;
    std::function<void(QTreeWidgetItem *)> walk = [&](QTreeWidgetItem *it) {
        if (it->childCount() == 0)
            return;
        m_expanded.insert(key(it), expanded);
        it->setExpanded(expanded);
        for (int i = 0; i < it->childCount(); ++i)
            walk(it->child(i));
    };
    walk(item);
    m_building = false;
    saveExpansion();
}

bool ProgrammingOutline::selectRow(const Row &row)
{
    if (m_dirty)
        rebuild();
    QTreeWidgetItem *it = itemFor(row);
    if (it == nullptr)
        return false;
    for (QTreeWidgetItem *a = it->parent(); a != nullptr; a = a->parent())
        a->setExpanded(true);
    clearSelection();
    setCurrentItem(it);
    it->setSelected(true);
    scrollToItem(it);
    return true;
}

QList<ProgrammingOutline::Row> ProgrammingOutline::selectedRows() const
{
    QList<Row> rows;
    for (QTreeWidgetItem *it : selectedItems())
    {
        const Row r = rowOf(it);
        if (r.kind != 0 && r.kind != HintRow && !rows.contains(r))
            rows << r;
    }
    return rows;
}

QString ProgrammingOutline::folderFor(const QTreeWidgetItem *item) const
{
    for (const QTreeWidgetItem *it = item; it != nullptr; it = it->parent())
    {
        const int kind = it->data(NameCol, KindRole).toInt();
        if (kind == FolderRow)
            return it->data(NameCol, PathRole).toString();
        if (kind == SceneRow || kind == FunctionRow)
            if (Function *f = m_doc->function(it->data(NameCol, IdRole).toUInt()))
                return f->path(true);
    }
    return QString();
}

void ProgrammingOutline::selectKeys(const QStringList &keys)
{
    setProperty("pendingKeys", keys);
}

void ProgrammingOutline::finishEdit(const QList<quint32> &sceneIds, const QStringList &keys)
{
    if (!keys.isEmpty())
        selectKeys(keys);
    // After the event that made it (a drop's drag is still unwinding).
    QTimer::singleShot(0, this, [this]() { rebuild(); emit itemSelectionChanged(); });
    emit scenesEdited(sceneIds);
}

/****************************************************************************
 * Edits
 ****************************************************************************/

QList<quint32> ProgrammingOutline::lookIdsOf(const QList<Row> &rows, quint32 sceneId) const
{
    Scene *scene = qobject_cast<Scene *>(m_doc->function(sceneId));
    QList<quint32> out;
    if (scene == nullptr)
        return out;
    QSet<quint32> picked;
    for (const Row &r : rows)
        if (r.kind == PaletteRow && r.scene == sceneId)
            picked.insert(r.id);
    const QList<quint32> order = scene->palettes();
    bool inPickedEffect = false;
    for (quint32 pid : order)
    {
        QLCPalette *p = m_doc->palette(pid);
        const bool isEffect = p != nullptr && p->type() == QLCPalette::Effect;
        if (isEffect)
            inPickedEffect = picked.contains(pid);
        if (picked.contains(pid)
            || (!isEffect && inPickedEffect && QLCPalette::isEffectScoped(m_doc, order, pid)))
            out << pid;
    }
    return out;
}

static void placeAdded(Doc *doc, Scene *dst, const QList<quint32> &added, quint32 atPalette)
{
    if (added.isEmpty())
        return;
    bool anyEffect = false;
    for (quint32 pid : added)
        if (QLCPalette *p = doc->palette(pid))
            anyEffect = anyEffect || p->type() == QLCPalette::Effect;
    QLCPalette *at = doc->palette(atPalette);
    if (at != nullptr && dst->palettes().contains(atPalette) && !added.contains(atPalette))
    {
        if (at->type() == QLCPalette::Effect && !anyEffect)
            SceneGroupLooks::nestLooks(doc, dst, added, atPalette);
        else
            SceneGroupLooks::placeLooks(doc, dst, added, dst->palettes().indexOf(atPalette));
    }
    else if (!anyEffect)
        SceneGroupLooks::placeLooks(doc, dst, added, -1);
    else
    {
        // An effect and its own go at the end, in their order, so they
        // nest as they did.
        QList<quint32> order = dst->palettes();
        for (quint32 pid : added)
            order.removeAll(pid);
        order << added;
        dst->reorderPalettes(order);
    }
}

bool ProgrammingOutline::moveRows(const QList<Row> &rows, quint32 toScene, quint32 atPalette, bool copy)
{
    Scene *dst = qobject_cast<Scene *>(m_doc->function(toScene));
    if (dst == nullptr || rows.isEmpty())
        return false;
    QList<quint32> scenes { toScene };
    for (const Row &r : rows)
        if (!scenes.contains(r.scene))
            scenes << r.scene;

    QStringList names;
    for (const Row &r : rows)
    {
        if (r.kind == GroupRow)
            if (FixtureGroup *g = m_doc->fixtureGroup(r.id))
                names << g->name();
        if (r.kind == FixtureRow)
            if (Fixture *f = m_doc->fixture(r.id))
                names << f->name();
        if (r.kind == PaletteRow)
            names << paletteName(m_doc->palette(r.id));
    }
    const QString what = names.size() == 1 ? names.first() : tr("%n row(s)", "", names.size());
    const bool reorderOnly = scenes.size() == 1;
    const QString label = reorderOnly ? tr("reorder looks in %1").arg(dst->name())
                          : copy ? tr("copy %1 to %2").arg(what, dst->name())
                                 : tr("move %1 to %2").arg(what, dst->name());

    QStringList keys;
    const bool ok = m_doc->paletteOps()->sceneStep(label, scenes, [&]() {
        bool changed = false;
        // Targets: into the destination; out of where they were (a move).
        for (const Row &r : rows)
        {
            Scene *src = qobject_cast<Scene *>(m_doc->function(r.scene));
            if (src == nullptr || src == dst)
                continue;
            if (r.kind == GroupRow)
            {
                changed = SceneGroupLooks::addGroupTarget(m_doc, dst, r.id) || changed;
                if (!copy)
                    changed = src->removeFixtureGroup(r.id) || changed;
                keys << rowKey(GroupRow, toScene, r.id);
            }
            else if (r.kind == FixtureRow)
            {
                if (!dst->fixtures().contains(r.id))
                {
                    dst->addFixture(r.id);
                    changed = true;
                }
                // Its raw values go with it.
                for (const SceneValue &v : src->values())
                    if (v.fxi == r.id)
                    {
                        dst->setValue(v);
                        if (!copy)
                            src->unsetValue(v.fxi, v.channel);
                    }
                if (!copy)
                    changed = src->removeFixture(r.id) || changed;
                keys << rowKey(FixtureRow, toScene, r.id);
            }
        }

        // Looks, scene by scene, each in its scene's order.
        QList<quint32> moved;
        for (quint32 sid : scenes)
        {
            const QList<quint32> pids = lookIdsOf(rows, sid);
            if (pids.isEmpty())
                continue;
            if (sid == toScene)
            {
                // Within the scene: a reorder.
                if (copy)
                    continue;
                const QList<quint32> before = dst->palettes();
                placeAdded(m_doc, dst, pids, atPalette);
                changed = changed || before != dst->palettes();
                for (quint32 pid : pids)
                    keys << rowKey(PaletteRow, toScene, pid);
                continue;
            }
            Scene *src = qobject_cast<Scene *>(m_doc->function(sid));
            if (src == nullptr)
                continue;
            for (quint32 pid : pids)
            {
                const int fin = src->paletteFadeIn(pid), fout = src->paletteFadeOut(pid);
                if (!dst->palettes().contains(pid))
                {
                    dst->addPalette(pid);
                    if (fin >= 0 || fout >= 0)
                        dst->setPaletteFade(pid, fin, fout);
                    moved << pid;
                    changed = true;
                }
                if (!copy)
                    changed = src->removePalette(pid) || changed;
                keys << rowKey(PaletteRow, toScene, pid);
            }
        }
        placeAdded(m_doc, dst, moved, atPalette);
        return changed;
    });
    if (ok)
        finishEdit(scenes, keys);
    return ok;
}

bool ProgrammingOutline::addFromSources(const QMimeData *mime, quint32 toScene, quint32 atPalette)
{
    Scene *dst = qobject_cast<Scene *>(m_doc->function(toScene));
    if (dst == nullptr || mime == nullptr)
        return false;
    auto ids = [mime](const char *type) {
        QList<quint32> out;
        if (!mime->hasFormat(type))
            return out;
        QByteArray data = mime->data(type);
        QDataStream in(&data, QIODevice::ReadOnly);
        while (!in.atEnd())
        {
            quint32 id = 0;
            in >> id;
            out << id;
        }
        return out;
    };
    const QList<quint32> pals = ids(FunctionsTreeWidget::paletteDragMimeType());
    // A Show Tree group set brings its member groups: a scene holds groups.
    QList<quint32> fixtures = ids(FixtureGroupSource::fixtureMimeType());
    QList<quint32> setFixtures;
    const QList<quint32> groups = FixtureGroupSource::droppedGroups(mime, m_doc, &setFixtures);
    for (quint32 fid : setFixtures)
        if (!fixtures.contains(fid))
            fixtures << fid;

    QStringList keys;
    bool lookAdded = false;
    const bool ok = m_doc->paletteOps()->sceneStep(tr("add to %1").arg(dst->name()), { toScene }, [&]() {
        bool changed = false;
        for (quint32 gid : groups)
            if (SceneGroupLooks::addGroupTarget(m_doc, dst, gid))
            {
                changed = true;
                keys << rowKey(GroupRow, toScene, gid);
            }
        for (quint32 fid : fixtures)
            if (m_doc->fixture(fid) != nullptr && !dst->fixtures().contains(fid))
            {
                dst->addFixture(fid);
                changed = true;
                keys << rowKey(FixtureRow, toScene, fid);
            }
        QList<quint32> added;
        for (quint32 pid : pals)
            if (m_doc->palette(pid) != nullptr && !dst->palettes().contains(pid))
            {
                dst->addPalette(pid);
                added << pid;
                keys << rowKey(PaletteRow, toScene, pid);
            }
        placeAdded(m_doc, dst, added, atPalette);
        lookAdded = !added.isEmpty();
        return changed || lookAdded;
    });
    if (!ok)
        return false;
    /* Looks applied: let them take over their channels (asks about the rest).
       Deferred: it may ask, and a modal question mustn't run inside a drop.
       Its own step, after the add. */
    if (lookAdded && !dst->values().isEmpty())
    {
        const quint32 sid = toScene;
        QTimer::singleShot(0, this, [this, sid]() {
            Scene *s = qobject_cast<Scene *>(m_doc->function(sid));
            if (s == nullptr)
                return;
            m_doc->paletteOps()->sceneStep(tr("let the looks take over %1").arg(s->name()), { sid }, [&]() {
                SceneGroupLooks::reconcileAfterPaletteApply(m_doc, s, this);
                return true;
            });
            emit scenesEdited({ sid });
        });
    }
    finishEdit({ toScene }, keys);
    return true;
}

bool ProgrammingOutline::removeRows(const QList<Row> &rows)
{
    QList<quint32> scenes;
    for (const Row &r : rows)
        if ((r.kind == GroupRow || r.kind == FixtureRow || r.kind == PaletteRow) && !scenes.contains(r.scene))
            scenes << r.scene;
    if (scenes.isEmpty())
        return false;
    Function *first = m_doc->function(scenes.first());
    const QString label = scenes.size() == 1 && first != nullptr
                          ? tr("remove from %1").arg(first->name()) : tr("remove from %n scene(s)", "", scenes.size());
    const bool ok = m_doc->paletteOps()->sceneStep(label, scenes, [&]() {
        bool changed = false;
        for (const Row &r : rows)
        {
            Scene *s = qobject_cast<Scene *>(m_doc->function(r.scene));
            if (s == nullptr)
                continue;
            if (r.kind == GroupRow)
                changed = s->removeFixtureGroup(r.id) || changed;
            else if (r.kind == FixtureRow)
            {
                // Its raw values too: they'd override the scene's looks.
                if (Fixture *f = m_doc->fixture(r.id))
                    for (quint32 ch = 0; ch < f->channels(); ++ch)
                        s->unsetValue(r.id, ch);
                changed = s->removeFixture(r.id) || changed;
            }
            else if (r.kind == PaletteRow)
                changed = s->removePalette(r.id) || changed;
        }
        return changed;
    });
    if (ok)
        finishEdit(scenes);
    return ok;
}

bool ProgrammingOutline::shiftLook(const Row &row, int delta)
{
    Scene *s = qobject_cast<Scene *>(m_doc->function(row.scene));
    if (s == nullptr || row.kind != PaletteRow)
        return false;
    QList<quint32> order = s->palettes();
    const int i = order.indexOf(row.id), j = i + delta;
    if (i < 0 || j < 0 || j >= order.size())
        return false;
    const bool ok = m_doc->paletteOps()->sceneStep(tr("reorder looks in %1").arg(s->name()), { row.scene }, [&]() {
        order.swapItemsAt(i, j);
        return s->reorderPalettes(order);
    });
    if (ok)
        finishEdit({ row.scene }, { rowKey(PaletteRow, row.scene, row.id) });
    return ok;
}

bool ProgrammingOutline::fileScenes(const QList<quint32> &sceneIds, const QString &folder)
{
    QList<quint32> moving;
    for (quint32 sid : sceneIds)
        if (Function *f = m_doc->function(sid))
            if (f->path(true) != folder)
                moving << sid;
    if (moving.isEmpty())
        return false;
    bool allScenes = true;
    for (quint32 sid : moving)
        allScenes = allScenes && qobject_cast<Scene *>(m_doc->function(sid)) != nullptr;
    const QString label = allScenes
        ? (folder.isEmpty() ? tr("unfile %n scene(s)", "", moving.size())
                            : tr("file %n scene(s) in %1", "", moving.size()).arg(folder))
        : (folder.isEmpty() ? tr("unfile %n function(s)", "", moving.size())
                            : tr("file %n function(s) in %1", "", moving.size()).arg(folder));
    const bool ok = m_doc->paletteOps()->functionStep(label, moving, [&](QList<quint32> &) {
        for (quint32 sid : moving)
            if (Function *f = m_doc->function(sid))
                f->setPath(folder);
        return true;
    });
    if (ok)
    {
        QStringList keys;
        for (quint32 sid : moving)
            keys << (qobject_cast<Scene *>(m_doc->function(sid)) != nullptr ? rowKey(SceneRow, sid, sid)
                                                                           : rowKey(FunctionRow, sid, sid));
        finishEdit(moving, keys);
    }
    return ok;
}

bool ProgrammingOutline::renameScene(quint32 sceneId, const QString &name)
{
    Function *f = m_doc->function(sceneId);
    const QString n = name.trimmed();
    if (f == nullptr || n.isEmpty() || n == f->name())
        return false;
    const bool ok = m_doc->paletteOps()->functionStep(tr("rename %1 to %2").arg(f->name(), n), { sceneId },
                                                     [&](QList<quint32> &) {
        f->setName(n);
        return true;
    });
    if (ok)
        finishEdit({ sceneId });
    return ok;
}

bool ProgrammingOutline::renameFolder(const QString &path, const QString &name)
{
    const QString n = name.trimmed();
    if (path.isEmpty() || n.isEmpty() || n.contains(QLatin1Char('/')))
        return false;
    const int cut = path.lastIndexOf(QLatin1Char('/'));
    const QString to = cut > 0 ? path.left(cut + 1) + n : n;
    if (to == path)
        return false;
    QList<quint32> scenes;
    QList<Function *> all;
    for (Function *f : m_doc->functions())
    {
        const QString p = f->path(true);
        if (p == path || p.startsWith(path + QLatin1Char('/')))
        {
            all << f;
            if (f->type() == Function::SceneType)
                scenes << f->id();
        }
    }
    // Everything filed there follows, on the history.
    QList<quint32> ids;
    for (Function *f : all)
        ids << f->id();
    m_doc->paletteOps()->functionStep(tr("rename folder %1 to %2").arg(path, n), ids, [&](QList<quint32> &) {
        for (Function *f : all)
            f->setPath(to + f->path(true).mid(path.length()));
        return true;
    });
    m_expanded.insert(rowKey(FolderRow, kInvalid, kInvalid, to), true);
    finishEdit(scenes, { rowKey(FolderRow, kInvalid, kInvalid, to) });
    return true;
}

bool ProgrammingOutline::removeLayers(const QList<int> &indexes)
{
    if (m_mode != Stack || m_stack == nullptr || indexes.isEmpty() || !m_stack->canRemoveLayers())
        return false;
    if (!m_stack->removeLayers(indexes))
        return false;
    QTimer::singleShot(0, this, [this]() { rebuild(); emit itemSelectionChanged(); });
    emit layersEdited();
    return true;
}

/****************************************************************************
 * Drag and drop
 ****************************************************************************/

QStringList ProgrammingOutline::mimeTypes() const
{
    return { QString::fromLatin1(rowMimeType()),
             QString::fromLatin1(WorkbenchStack::attributeMimeType()),
             QString::fromLatin1(FunctionsTreeWidget::paletteDragMimeType()),
             QString::fromLatin1(FixtureGroupSource::fixtureGroupMimeType()),
             QString::fromLatin1(FixtureGroupSource::fixtureMimeType()),
             QString::fromLatin1(ShowTreeView::setMimeType()) };
}

QMimeData *ProgrammingOutline::mimeData(const QList<QTreeWidgetItem *> &items) const
{
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    int n = 0;
    for (QTreeWidgetItem *it : items)
    {
        const Row r = rowOf(it);
        if (r.kind == SceneRow || r.kind == FunctionRow || r.kind == GroupRow || r.kind == FixtureRow
            || r.kind == PaletteRow)
        {
            out << qint32(r.kind) << r.scene << r.id;
            ++n;
        }
    }
    // A Show Tree cue's look rows (W3): which layer, which look.
    QByteArray attrs;
    QDataStream aout(&attrs, QIODevice::WriteOnly);
    int na = 0;
    for (QTreeWidgetItem *it : items)
        if (it->data(NameCol, KindRole).toInt() == AttributeRow)
        {
            aout << qint32(it->data(NameCol, SceneRole).toInt()) << it->data(NameCol, PathRole).toString();
            ++na;
        }
    if (n == 0 && na == 0)
        return nullptr;
    QMimeData *mime = new QMimeData;
    if (n > 0)
        mime->setData(QString::fromLatin1(rowMimeType()), data);
    if (na > 0)
        mime->setData(QString::fromLatin1(WorkbenchStack::attributeMimeType()), attrs);
    return mime;
}

static QList<ProgrammingOutline::Row> decodeRows(const QMimeData *mime)
{
    QList<ProgrammingOutline::Row> rows;
    QByteArray data = mime->data(QString::fromLatin1(ProgrammingOutline::rowMimeType()));
    QDataStream in(&data, QIODevice::ReadOnly);
    while (!in.atEnd())
    {
        qint32 kind = 0;
        ProgrammingOutline::Row r;
        in >> kind >> r.scene >> r.id;
        r.kind = kind;
        rows << r;
    }
    return rows;
}

static bool fromSources(const QMimeData *mime)
{
    return mime->hasFormat(FunctionsTreeWidget::paletteDragMimeType())
           || mime->hasFormat(FixtureGroupSource::fixtureGroupMimeType())
           || mime->hasFormat(FixtureGroupSource::fixtureMimeType())
           || FixtureGroupSource::hasGroupSets(mime);
}

QString ProgrammingOutline::dropPlan(const QMimeData *mime, const QPoint &pos, bool copy,
                                     quint32 *toScene, quint32 *atPalette, QString *folder) const
{
    if (toScene != nullptr)
        *toScene = kInvalid;
    if (atPalette != nullptr)
        *atPalette = kInvalid;
    if (mime == nullptr)
        return QString();
    if (m_mode == Stack && m_stack != nullptr && !m_stack->readOnlyReason().isEmpty())
        return QStringLiteral("!") + m_stack->readOnlyReason();
    // W3: a Show Tree cue takes every drop as the tree does.
    if (m_mode == Stack && m_stack != nullptr && m_stack->takesDrops())
        return m_stack->dropPlanOn(layerOf(itemAt(pos)), mime, copy);
    QTreeWidgetItem *it = itemAt(pos);
    const Row at = rowOf(it);
    Scene *dst = (at.kind == SceneRow || at.kind == GroupRow || at.kind == FixtureRow || at.kind == PaletteRow)
                 ? qobject_cast<Scene *>(m_doc->function(at.scene)) : nullptr;
    const quint32 atPal = at.kind == PaletteRow ? at.id : kInvalid;
    QLCPalette *atP = m_doc->palette(atPal);

    if (mime->hasFormat(rowMimeType()))
    {
        const QList<Row> rows = decodeRows(mime);
        QList<quint32> scenes;
        bool anyLook = false, anyTarget = false, allScenes = true;
        for (const Row &r : rows)
        {
            if (r.kind == SceneRow || r.kind == FunctionRow)
            {
                scenes << r.id;
                allScenes = allScenes && r.kind == SceneRow;
            }
            anyLook = anyLook || r.kind == PaletteRow;
            anyTarget = anyTarget || r.kind == GroupRow || r.kind == FixtureRow;
        }
        if (!scenes.isEmpty() && m_mode == Stack)
            return stackDropPlan(mime, pos, false, nullptr);
        if (!scenes.isEmpty())
        {
            // On a chaser: its steps (W2).
            if (at.kind == FunctionRow && m_doc->function(at.id) != nullptr
                && m_doc->function(at.id)->type() == Function::ChaserType)
                return chaserDropPlan(scenes, at.id);
            if (at.kind == FolderRow)
            {
                if (folder != nullptr)
                    *folder = it->data(NameCol, PathRole).toString();
                return allScenes
                    ? tr("File %n scene(s) in “%1”", "", scenes.size()).arg(it->data(NameCol, PathRole).toString())
                    : tr("File %n function(s) in “%1”", "", scenes.size()).arg(it->data(NameCol, PathRole).toString());
            }
            if (it == nullptr)
            {
                if (folder != nullptr)
                    *folder = QString();
                return allScenes ? tr("Take %n scene(s) out of their folder", "", scenes.size())
                                 : tr("Take %n function(s) out of their folder", "", scenes.size());
            }
            return QStringLiteral("!") + tr("A scene doesn't go in a scene: drop it on a folder, "
                                            "or on the Workbench to make it a layer of a collection.");
        }
        if (dst == nullptr)
            return QStringLiteral("!") + tr("Drop on a scene, or on a row in one.");
        bool same = true;
        for (const Row &r : rows)
            same = same && r.scene == dst->id();
        if (same)
        {
            if (copy || !anyLook)
                return QStringLiteral("!") + tr("Already in “%1”.").arg(dst->name());
            if (toScene != nullptr)
                *toScene = dst->id();
            if (atPalette != nullptr)
                *atPalette = atPal;
            if (atP != nullptr && atP->type() == QLCPalette::Effect)
                return tr("Make it the effect “%1”'s own").arg(paletteName(atP));
            return atP != nullptr ? tr("Reorder: put it above “%1” (lower rows win)").arg(paletteName(atP))
                                  : tr("Reorder: make it a base look");
        }
        if (toScene != nullptr)
            *toScene = dst->id();
        if (atPalette != nullptr)
            *atPalette = atPal;
        QString what = rows.size() == 1
            ? (rows.first().kind == PaletteRow ? paletteName(m_doc->palette(rows.first().id))
               : rows.first().kind == GroupRow && m_doc->fixtureGroup(rows.first().id) != nullptr
                 ? m_doc->fixtureGroup(rows.first().id)->name()
                 : m_doc->fixture(rows.first().id) != nullptr ? m_doc->fixture(rows.first().id)->name() : QString())
            : tr("%n row(s)", "", rows.size());
        Q_UNUSED(anyTarget)
        return copy ? tr("Copy %1 to “%2”").arg(what, dst->name())
                    : tr("Move %1 to “%2” (hold ⌥ to copy)").arg(what, dst->name());
    }

    if (fromSources(mime))
    {
        if (dst == nullptr)
            return QStringLiteral("!") + tr("Drop on a scene, or on a row in one.");
        if (toScene != nullptr)
            *toScene = dst->id();
        if (atPalette != nullptr)
            *atPalette = atPal;
        if (atP != nullptr && atP->type() == QLCPalette::Effect
            && mime->hasFormat(FunctionsTreeWidget::paletteDragMimeType()))
            return tr("Add to “%1”, as the effect “%2”'s own").arg(dst->name(), paletteName(atP));
        if (FixtureGroupSource::hasGroupSets(mime))
        {
            // Say what a set becomes here: its groups.
            QStringList sets, names;
            for (quint32 gid : FixtureGroupSource::droppedGroups(mime, m_doc, nullptr, &sets))
                if (const FixtureGroup *g = m_doc->fixtureGroup(gid))
                    names << g->name();
            if (names.isEmpty())
                return QStringLiteral("!") + tr("“%1” has no groups to add.").arg(sets.join(QStringLiteral(", ")));
            return tr("Add the groups of “%1” (%2) to “%3”")
                .arg(sets.join(QStringLiteral(", ")), names.join(QStringLiteral(", ")), dst->name());
        }
        return tr("Add to “%1”").arg(dst->name());
    }
    return QString();
}

QString ProgrammingOutline::stackDropPlan(const QMimeData *mime, const QPoint &pos, bool,
                                          int *layerAt) const
{
    if (layerAt != nullptr)
        *layerAt = -1;
    if (m_stack == nullptr)
        return QStringLiteral("!") + tr("Nothing is loaded: click a collection on the left first.");
    QList<quint32> ids;
    for (const Row &r : decodeRows(mime))
        if ((r.kind == SceneRow || r.kind == FunctionRow) && !ids.contains(r.id))
            ids << r.id;
    const int at = layerOf(itemAt(pos));
    if (layerAt != nullptr)
        *layerAt = at;
    QTreeWidgetItem *atItem = layerItem(at);
    const QString atName = atItem != nullptr ? atItem->text(NameCol) : QString();

    // Layers already here: a reorder.
    QList<quint32> layerIds;
    for (const WorkbenchStack::Layer &l : m_stack->layers())
        layerIds << l.functionId;
    bool allHere = true;
    for (quint32 id : ids)
        allHere = allHere && layerIds.contains(id);
    if (allHere)
    {
        if (!m_stack->canReorderLayers())
            return QStringLiteral("!") + tr("Its layers aren't reordered here.");
        if (ids.size() == 1 && at >= 0 && layerIds.indexOf(ids.first()) == at)
            return QStringLiteral("!") + tr("Already there.");
        return at >= 0 ? tr("Move it above “%1”").arg(atName) : tr("Move it to the bottom (it plays last)");
    }
    const QString plan = m_stack->addPlan(ids);
    if (plan.startsWith(QLatin1Char('!')) || at < 0)
        return plan;
    return tr("%1, above “%2”").arg(plan, atName);
}

void ProgrammingOutline::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(rowMimeType()) || fromSources(event->mimeData())
        || event->mimeData()->hasFormat(QString::fromLatin1(WorkbenchStack::attributeMimeType()))
        || (m_mode == Stack && m_stack != nullptr && m_stack->takesDrops()))
        event->acceptProposedAction();
    else
        event->ignore();
}

void ProgrammingOutline::dragMoveEvent(QDragMoveEvent *event)
{
    // Scrolls and opens rows on hover; what the drop does is decided here.
    QTreeWidget::dragMoveEvent(event);
    const bool copy = event->modifiers() & Qt::AltModifier;
    const QString plan = dropPlan(event->mimeData(), qlcEventPos(event), copy);
    emit hintChanged(plan.startsWith(QLatin1Char('!')) ? plan.mid(1) : plan);
    if (plan.isEmpty() || plan.startsWith(QLatin1Char('!')))
        event->ignore();
    else
    {
        event->setDropAction(copy ? Qt::CopyAction : Qt::MoveAction);
        event->accept();
    }
}

void ProgrammingOutline::dragLeaveEvent(QDragLeaveEvent *event)
{
    QTreeWidget::dragLeaveEvent(event);
    emit hintChanged(QString());
}

void ProgrammingOutline::dropEvent(QDropEvent *event)
{
    emit hintChanged(QString());
    const QMimeData *mime = event->mimeData();
    const bool copy = event->modifiers() & Qt::AltModifier;
    quint32 toScene = kInvalid, atPalette = kInvalid;
    QString folder;
    const QString plan = dropPlan(mime, qlcEventPos(event), copy, &toScene, &atPalette, &folder);
    if (plan.isEmpty() || plan.startsWith(QLatin1Char('!')))
    {
        event->ignore();
        return;
    }
    if (m_mode == Stack && m_stack != nullptr && m_stack->takesDrops())
    {
        // W3: the back end's (⌥: one layer per group, as in the tree).
        if (m_stack->dropOn(layerOf(itemAt(qlcEventPos(event))), mime, copy))
        {
            QTimer::singleShot(0, this, [this]() { rebuild(); emit itemSelectionChanged(); });
            emit layersEdited();
        }
        event->setDropAction(Qt::CopyAction);
        event->accept();
        return;
    }
    if (mime->hasFormat(rowMimeType()))
    {
        const QList<Row> rows = decodeRows(mime);
        QList<quint32> scenes;
        for (const Row &r : rows)
            if ((r.kind == SceneRow || r.kind == FunctionRow) && !scenes.contains(r.id))
                scenes << r.id;
        if (!scenes.isEmpty() && m_mode == Stack && m_stack != nullptr)
        {
            int at = -1;
            stackDropPlan(mime, qlcEventPos(event), false, &at);
            QList<quint32> layerIds;
            for (const WorkbenchStack::Layer &l : m_stack->layers())
                layerIds << l.functionId;
            bool allHere = true;
            for (quint32 id : scenes)
                allHere = allHere && layerIds.contains(id);
            bool changed = false;
            QStringList keys;
            if (allHere)
            {
                // In their order, each above the drop row (or at the bottom).
                for (quint32 id : scenes)
                {
                    QList<quint32> now;
                    for (const WorkbenchStack::Layer &l : m_stack->layers())
                        now << l.functionId;
                    const int from = now.indexOf(id);
                    int to = at < 0 ? int(now.size()) - 1 : at;
                    if (at >= 0 && from < at)
                        --to;
                    changed = m_stack->moveLayer(from, to) || changed;
                    if (at >= 0 && from > at)
                        ++at;
                }
            }
            else
                changed = m_stack->addLayers(scenes, at);
            if (changed)
            {
                for (quint32 id : scenes)
                    keys << (qobject_cast<Scene *>(m_doc->function(id)) != nullptr ? rowKey(SceneRow, id, id)
                                                                                  : QString());
                keys.removeAll(QString());
                if (!keys.isEmpty())
                    selectKeys(keys);
                QTimer::singleShot(0, this, [this]() { rebuild(); emit itemSelectionChanged(); });
                emit layersEdited();
            }
        }
        else if (!scenes.isEmpty())
        {
            const Row at = rowOf(itemAt(qlcEventPos(event)));
            if (at.kind == FunctionRow && m_doc->function(at.id) != nullptr
                && m_doc->function(at.id)->type() == Function::ChaserType)
            {
                // Appended as steps, in the order dragged (W2).
                if (FunctionStack::addSteps(m_doc, at.id, scenes))
                    finishEdit({}, { rowKey(FunctionRow, at.id, at.id) });
            }
            else
                fileScenes(scenes, folder);
        }
        else
            moveRows(rows, toScene, atPalette, copy);
    }
    else
        addFromSources(mime, toScene, atPalette);
    /* Never MoveAction back to the drag: the view would take the dragged
       rows out itself. The rebuild puts every row where it now is. */
    event->setDropAction(Qt::CopyAction);
    event->accept();
}

/****************************************************************************
 * Keys, mouse, menu
 ****************************************************************************/

void ProgrammingOutline::keyPressEvent(QKeyEvent *event)
{
    const Qt::KeyboardModifiers mods = event->modifiers();
    QTreeWidgetItem *cur = currentItem();
    switch (event->key())
    {
    case Qt::Key_F2:
        if (cur != nullptr && (cur->flags() & Qt::ItemIsEditable))
            editItem(cur, NameCol);
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (state() != QAbstractItemView::EditingState)
        {
            emit pageRequested();
            return;
        }
        break;
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
    {
        // W3: a cue's attribute row clears that value (it inherits again).
        if (m_mode == Stack && m_stack != nullptr && cur != nullptr
            && rowOf(cur).kind == AttributeRow)
        {
            if (m_stack->clearAttribute(int(rowOf(cur).scene), int(rowOf(cur).id)))
                emit layersEdited();
            return;
        }
        const QList<Row> rows = selectedRows();
        QList<quint32> scenes;
        QList<Row> parts;
        for (const Row &r : rows)
            (r.kind == SceneRow || r.kind == FunctionRow || r.kind == CueLayerRow ? (void)(scenes << r.id)
                                                                                  : (void)(parts << r));
        if (!parts.isEmpty())
            removeRows(parts);
        else if (m_mode == Stack)
        {
            // A layer comes out of what is loaded; the function stays.
            QList<int> layers;
            for (QTreeWidgetItem *it : selectedItems())
                if (it->parent() == nullptr && layerOf(it) >= 0)
                    layers << layerOf(it);
            removeLayers(layers);
        }
        else if (!scenes.isEmpty())
            emit deleteScenesRequested(scenes);
        return;
    }
    case Qt::Key_Right:
    case Qt::Key_Left:
    {
        const bool open = event->key() == Qt::Key_Right;
        // ⌘→ / ⌘← every row; ⌥→ / ⌥← the row and all under it.
        if (mods & Qt::ControlModifier)
        {
            open ? expandEverything() : collapseEverything();
            return;
        }
        if ((mods & Qt::AltModifier) && cur != nullptr)
        {
            setSubtreeExpanded(cur, open);
            return;
        }
        break;
    }
    default:
        break;
    }
    QTreeWidget::keyPressEvent(event);
}

void ProgrammingOutline::mousePressEvent(QMouseEvent *event)
{
    /* ⌥-click a row's triangle: it and everything under it open (or close),
       as Finder does. */
    if (event->modifiers() & Qt::AltModifier)
    {
        const QPoint pos = qlcEventPos(event);
        const QModelIndex idx = indexAt(pos);
        QTreeWidgetItem *it = itemFromIndex(idx);
        if (it != nullptr && it->childCount() > 0 && idx.column() == NameCol
            && pos.x() < visualRect(idx).x())
        {
            setSubtreeExpanded(it, !it->isExpanded());
            return;
        }
    }
    QTreeWidget::mousePressEvent(event);
}

void ProgrammingOutline::slotItemChanged(QTreeWidgetItem *item, int column)
{
    if (m_building || column != NameCol || item == nullptr)
        return;
    const Row r = rowOf(item);
    const QString text = item->text(NameCol);
    if (r.kind == SceneRow || r.kind == FunctionRow)
    {
        if (!renameScene(r.id, text))
            scheduleRebuild();
    }
    else if (r.kind == FolderRow)
    {
        if (!renameFolder(item->data(NameCol, PathRole).toString(), text))
            scheduleRebuild();
    }
    else if (r.kind == PaletteRow)
    {
        QLCPalette *p = m_doc->palette(r.id);
        if (p == nullptr || text.trimmed().isEmpty() || text.trimmed() == p->name()
            || !m_doc->paletteOps()->rename(r.id, text.trimmed()))
            scheduleRebuild();
        else
            finishEdit({ r.scene });
    }
}

void ProgrammingOutline::slotContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *clicked = itemAt(pos);
    // The selection, if the click is in it; else the clicked row alone.
    if (clicked != nullptr && !clicked->isSelected())
    {
        clearSelection();
        setCurrentItem(clicked);
        clicked->setSelected(true);
    }
    const Row at = rowOf(clicked);
    const QList<Row> rows = selectedRows();
    QList<quint32> scenes;
    QList<Row> parts;
    for (const Row &r : rows)
    {
        if (r.kind == SceneRow || r.kind == FunctionRow)
            scenes << r.id;
        else if (r.kind == GroupRow || r.kind == FixtureRow || r.kind == PaletteRow)
            parts << r;
    }

    // W3: a cue's layer or attribute row: the tree's own menu for it.
    if (m_mode == Stack && m_stack != nullptr && clicked != nullptr
        && (at.kind == CueLayerRow || at.kind == AttributeRow)
        && m_stack->showMenu(layerOf(clicked), at.kind == AttributeRow ? int(at.id) : -1,
                             viewport()->mapToGlobal(pos)))
        return;

    QMenu menu(this);
    menu.setToolTipsVisible(true);
    const QString folder = folderFor(clicked);
    const bool readOnly = m_mode == Stack && (m_stack == nullptr || !m_stack->readOnlyReason().isEmpty());

    if (m_mode == Stack && clicked != nullptr && clicked->parent() == nullptr && layerOf(clicked) >= 0)
    {
        // A layer: open it, take it out, move it.
        const int layer = layerOf(clicked);
        QList<int> layers;
        for (QTreeWidgetItem *it : selectedItems())
            if (it->parent() == nullptr && layerOf(it) >= 0)
                layers << layerOf(it);
        menu.addAction(tr("Open"), this, [this, at]() { emit openRequested(at.id); })
            ->setToolTip(tr("Load it in the Workbench on its own (↩ comes back here)"));
        if (!readOnly && layers.size() == 1 && m_stack->canMakeOwnCopy(layer))
            menu.addAction(tr("Make own copy"), this, [this, layer]() {
                if (m_stack->makeOwnCopy(layer) != kInvalid)
                {
                    QTimer::singleShot(0, this, [this, layer]() {
                        rebuild();
                        if (QTreeWidgetItem *it = layerItem(layer))
                        {
                            clearSelection();
                            setCurrentItem(it);
                            it->setSelected(true);
                        }
                    });
                    emit layersEdited();
                }
            })->setToolTip(tr("It is shared: split it off as a copy of its own here, so an edit changes only this one"));
        if (!readOnly && m_stack->canRemoveLayers())
            menu.addAction(layers.size() > 1 ? tr("Remove %n layer(s)", "", layers.size()) : tr("Remove the layer"),
                           this, [this, layers]() { removeLayers(layers); })
                ->setToolTip(tr("Take it out of what is loaded; the function itself stays"));
        if (!readOnly && m_stack->canReorderLayers() && layers.size() == 1)
        {
            menu.addAction(tr("Move layer up"), this, [this, layer]() {
                if (m_stack->moveLayer(layer, layer - 1))
                {
                    selectKeys({ key(layerItem(layer)) });
                    QTimer::singleShot(0, this, [this]() { rebuild(); emit itemSelectionChanged(); });
                    emit layersEdited();
                }
            });
            menu.addAction(tr("Move layer down"), this, [this, layer]() {
                if (m_stack->moveLayer(layer, layer + 1))
                {
                    selectKeys({ key(layerItem(layer)) });
                    QTimer::singleShot(0, this, [this]() { rebuild(); emit itemSelectionChanged(); });
                    emit layersEdited();
                }
            });
        }
        menu.addSeparator();
        if (at.kind == SceneRow && !readOnly)
            menu.addAction(tr("Rename"), this, [this, clicked]() { editItem(clicked, NameCol); });
        menu.addAction(tr("Open all under this"), this, [this, clicked]() { setSubtreeExpanded(clicked, true); });
        menu.addAction(tr("Close all under this"), this, [this, clicked]() { setSubtreeExpanded(clicked, false); });
        menu.exec(viewport()->mapToGlobal(pos));
        return;
    }
    if (readOnly)
        parts.clear();
    if (!parts.isEmpty())
    {
        menu.addAction(parts.size() == 1 ? tr("Remove from the scene") : tr("Remove %n from their scenes", "", parts.size()),
                       this, [this, parts]() { removeRows(parts); });
        if (parts.size() == 1 && at.kind == PaletteRow)
        {
            menu.addAction(tr("Move up"), this, [this, at]() { shiftLook(at, -1); });
            menu.addAction(tr("Move down"), this, [this, at]() { shiftLook(at, +1); });
            menu.addAction(tr("Rename palette"), this, [this, clicked]() { editItem(clicked, NameCol); });
            menu.addAction(tr("Show in Palettes"), this, [this, at]() { emit revealPaletteRequested(at.id); });
        }
        if (parts.size() == 1 && at.kind == GroupRow)
            menu.addAction(tr("Edit the group's layout"), this, [this, at]() { emit editGroupLayoutRequested(at.id); });
        // Focus this look (docs/design/FOCUS_REFINE_DESIGN.md 6B): this row from the stage.
        if (parts.size() == 1 && m_mode == Stack && (at.kind == GroupRow || at.kind == FixtureRow))
        {
            QAction *refine = menu.addAction(focusThisText(), this, [this, at]() {
                emit refineRequested(at.scene, at.kind == GroupRow ? at.id : 0xFFFFFFFF,
                                     at.kind == FixtureRow ? at.id : 0xFFFFFFFF);
            });
            refine->setObjectName(QStringLiteral("outlineRefine"));
            FocusMenu::setAvailable(refine, FocusMenu::focusCueBlocked(m_doc),
                                    tr("Move, zoom, focus and recolour this from the stage with the controller"));
        }
        menu.addSeparator();
    }
    if (at.kind == SceneRow && m_mode == Stack && clicked != nullptr && !readOnly)
    {
        QAction *refine = menu.addAction(focusThisText(), this, [this, at]() {
            emit refineRequested(at.id, 0xFFFFFFFF, 0xFFFFFFFF);
        });
        refine->setObjectName(QStringLiteral("outlineRefine"));
        FocusMenu::setAvailable(refine, FocusMenu::focusCueBlocked(m_doc),
                                tr("Move, zoom, focus and recolour this from the stage with the controller"));
    }
    if (!scenes.isEmpty() && m_mode == Navigator)
    {
        if (scenes.size() == 1)
            menu.addAction(tr("Rename"), this, [this, clicked]() { editItem(clicked, NameCol); });
        menu.addAction(scenes.size() == 1 ? tr("Duplicate") : tr("Duplicate %n", "", scenes.size()),
                       this, [this, scenes]() { emit duplicateRequested(scenes); });
        menu.addAction(scenes.size() == 1 ? tr("Delete…") : tr("Delete %n…", "", scenes.size()),
                       this, [this, scenes]() { emit deleteScenesRequested(scenes); });
        const QList<quint32> ordered = selectedFunctionsInOrder();
        menu.addAction(QIcon(QStringLiteral(":/chaser.png")), tr("Chaser from selection"), this,
                       [this, ordered, folder]() { emit chaserRequested(ordered, folder); })
            ->setToolTip(tr("A new chaser with these as its steps, in the order you selected them"));
        // A chaser: make it a Show Tree cue list in place (W5's Import).
        const Function *one = scenes.size() == 1 && m_doc != nullptr ? m_doc->function(scenes.first()) : nullptr;
        if (one != nullptr && one->type() == Function::ChaserType)
        {
            const quint32 chaserId = one->id();
            const QString why = m_doc->showTree()->whyNotSend(chaserId);
            QAction *send = menu.addAction(tr("Send to Show Tree…"), this,
                                           [this, chaserId]() { emit sendToShowTreeRequested(chaserId); });
            send->setObjectName(QStringLiteral("outlineSendToTree"));
            send->setEnabled(why.isEmpty());
            send->setToolTip(why.isEmpty() ? tr("Make this chaser a Show Tree cue list in place, checked to play "
                                                "the same; Restore original puts it back")
                                           : why);
        }
        menu.addSeparator();
    }
    if (at.kind == FolderRow && !clicked->data(NameCol, PathRole).toString().isEmpty())
    {
        const QString path = clicked->data(NameCol, PathRole).toString();
        menu.addAction(tr("Rename folder"), this, [this, clicked]() { editItem(clicked, NameCol); });
        menu.addAction(tr("Delete folder"), this, [this, path]() { emit removeFolderRequested(path); })
            ->setToolTip(tr("What is filed in it moves up a level; nothing is deleted"));
    }
    if (m_mode == Navigator)
    {
        menu.addAction(QIcon(QStringLiteral(":/scene.png")),
                       folder.isEmpty() ? tr("New scene") : tr("New scene in %1").arg(folder),
                       this, [this, folder]() { emit newSceneRequested(folder); });
        emit extendMenu(&menu);
        menu.addSeparator();
    }
    if (clicked != nullptr && clicked->childCount() > 0)
    {
        menu.addAction(tr("Open all under this"), this, [this, clicked]() { setSubtreeExpanded(clicked, true); });
        menu.addAction(tr("Close all under this"), this, [this, clicked]() { setSubtreeExpanded(clicked, false); });
    }
    menu.addAction(tr("Expand all"), this, &ProgrammingOutline::expandEverything);
    menu.addAction(tr("Collapse all"), this, &ProgrammingOutline::collapseEverything);
    menu.exec(viewport()->mapToGlobal(pos));
}

/****************************************************************************
 * W2: the selection's order, drops on a chaser
 ****************************************************************************/

void ProgrammingOutline::trackSelectionOrder()
{
    if (m_building)
        return;
    QStringList now;
    for (QTreeWidgetItemIterator it(this, QTreeWidgetItemIterator::Selected); *it; ++it)
        now << key(*it);
    // Gone from the selection: dropped; new to it: last, in the tree's order.
    QStringList kept;
    for (const QString &k : m_selectionOrder)
        if (now.contains(k))
            kept << k;
    for (const QString &k : now)
        if (!kept.contains(k))
            kept << k;
    m_selectionOrder = kept;
}

QList<quint32> ProgrammingOutline::selectedFunctionsInOrder() const
{
    QHash<QString, quint32> selected;
    QStringList treeOrder;
    for (QTreeWidgetItemIterator it(const_cast<ProgrammingOutline *>(this), QTreeWidgetItemIterator::Selected);
         *it; ++it)
    {
        const Row r = rowOf(*it);
        if (r.kind == SceneRow || r.kind == FunctionRow)
        {
            selected.insert(key(*it), r.id);
            treeOrder << key(*it);
        }
    }
    QList<quint32> out;
    for (const QString &k : m_selectionOrder)
        if (selected.contains(k) && !out.contains(selected.value(k)))
            out << selected.value(k);
    // Selected without a click we saw (a rebuild kept it): in the tree's order.
    for (const QString &k : treeOrder)
        if (!out.contains(selected.value(k)))
            out << selected.value(k);
    return out;
}

QString ProgrammingOutline::chaserDropPlan(const QList<quint32> &functionIds, quint32 chaserId) const
{
    return FunctionStack::stepsPlan(m_doc, chaserId, functionIds);
}
