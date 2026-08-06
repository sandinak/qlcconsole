/*
  Q Light Controller Plus
  scenegrouplooks.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QIcon>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDataStream>
#include <QMessageBox>
#include <QMenu>
#include <QSet>
#include <QMap>
#include <QTimer>
#include <QStyledItemDelegate>
#include <QDoubleSpinBox>

#include <algorithm>
#include <functional>

#include "scenegrouplooks.h"
#include "functionstreewidget.h"   // palette MIME type
#include "fixturegroupsource.h"    // fixture-group / fixture MIME types
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "qlccapability.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

// Target kinds, stored at Qt::UserRole + 1 on target items.
enum { TargetGroup = 0, TargetFixture = 1, TargetTypeFolder = 2 };
#define TARGET_KIND_ROLE (Qt::UserRole + 1)

// Looks tree columns. Column 0 carries the palette id at Qt::UserRole; the
// In/Out columns carry their fade override in ms at Qt::UserRole (-1 = "step",
// i.e. follow the chaser step / scene fade).
enum { LookColName = 0, LookColIn = 1, LookColOut = 2, LookColCount = 3 };
// Marks a fade cell whose fade doesn't apply, so the delegate keeps it read-only
// and it renders as "—": an Effect's Fade In (its fade-in is the scene envelope),
// and BOTH fades of a colour/dimmer nested under an effect (it feeds the effect's
// gradient — a value, not a painted look — so it has no fade of its own).
static const int LookFadeInertRole = Qt::UserRole + 3;

/** Human "1.5 s" / "step" text for a fade override in ms (-1 = step). */
static QString fadeCellText(int ms)
{
    if (ms < 0)
        return QObject::tr("step");
    return QString::number(ms / 1000.0, 'g', 3) + QObject::tr(" s");
}

/** Editable fade-time cell: a QDoubleSpinBox in seconds whose minimum reads
 *  "step" (fall back to the step/scene fade). Only the In/Out columns edit. */
class FadeSpinDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &index) const override
    {
        if (index.column() != LookColIn && index.column() != LookColOut)
            return nullptr;
        // Cells whose fade doesn't apply (effect Fade In, or a colour feeding an
        // effect) are read-only.
        if (index.data(LookFadeInertRole).toBool())
            return nullptr;
        QDoubleSpinBox *sp = new QDoubleSpinBox(parent);
        sp->setDecimals(2);
        sp->setSingleStep(0.1);
        sp->setRange(-0.1, 600.0);              // -0.1 == the "step" value
        sp->setSpecialValueText(QObject::tr("step"));
        sp->setSuffix(QObject::tr(" s"));
        return sp;
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        QDoubleSpinBox *sp = qobject_cast<QDoubleSpinBox*>(editor);
        if (sp == nullptr) return;
        const int ms = index.data(Qt::UserRole).toInt();
        sp->setValue(ms >= 0 ? ms / 1000.0 : sp->minimum());
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        QDoubleSpinBox *sp = qobject_cast<QDoubleSpinBox*>(editor);
        if (sp == nullptr) return;
        const int ms = sp->value() < 0 ? -1 : int(sp->value() * 1000.0 + 0.5);
        model->setData(index, ms, Qt::UserRole);
        model->setData(index, fadeCellText(ms), Qt::DisplayRole);
    }
};

// Human label + stable bucket key for grouping fixed fixtures by type.
static QString fixtureTypeLabel(const Fixture *f)
{
    if (f == NULL)
        return QString();
    if (f->fixtureDef() != NULL && f->fixtureMode() != NULL)
        return QString("%1 (%2)").arg(f->fixtureDef()->model())
                                 .arg(f->fixtureMode()->name());
    return QObject::tr("Generic %1-channel").arg(f->channels());
}

SceneGroupLooks::SceneGroupLooks(Scene *scene, Doc *doc, QWidget *parent,
                                 bool includeFixtureTargets)
    : QWidget(parent)
    , m_scene(scene)
    , m_doc(doc)
    , m_includeFixtureTargets(includeFixtureTargets)
{
    setAcceptDrops(true);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 6, 0, 0);

    QLabel *header = new QLabel(
        tr("<b>Looks</b> (palettes) are applied to the <b>fixtures in this scene</b>. "
           "Every look applies to every fixture.<br>"
           "Drag here to add: <b>palettes</b> &rarr; looks; "
           "<b>fixture groups</b> &rarr; scene fixtures (dynamic — follow membership); "
           "individual <b>fixtures</b> &rarr; scene fixtures (fixed)."), this);
    header->setWordWrap(true);
    root->addWidget(header);

    QHBoxLayout *cols = new QHBoxLayout();
    root->addLayout(cols);

    // --- Targets column (groups + optionally fixtures) ---
    QVBoxLayout *targetCol = new QVBoxLayout();
    m_targetsLabel = new QLabel(tr("Fixtures in Scene"), this);
    targetCol->addWidget(m_targetsLabel);
    m_targetList = new QTreeWidget(this);
    m_targetList->setHeaderHidden(true);
    m_targetList->setRootIsDecorated(true);
    m_targetList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_targetList->setContextMenuPolicy(Qt::CustomContextMenu);
    targetCol->addWidget(m_targetList);
    QHBoxLayout *targetBtns = new QHBoxLayout();
    m_removeTargetButton = new QPushButton(tr("Remove"), this);
    targetBtns->addWidget(m_removeTargetButton);
    targetBtns->addStretch();
    targetCol->addLayout(targetBtns);
    cols->addLayout(targetCol);

    // --- Looks column ---
    QVBoxLayout *lookCol = new QVBoxLayout();
    lookCol->addWidget(new QLabel(tr("Looks"), this));
    m_lookList = new QTreeWidget(this);
    m_lookList->setColumnCount(LookColCount);
    m_lookList->setHeaderLabels(QStringList() << tr("Look") << tr("Fade In") << tr("Fade Out"));
    if (QTreeWidgetItem *hdr = m_lookList->headerItem())
    {
        const QString fadeTip = tr("Per-look fade time (this look's channels only).\n"
                                   "0 = snap instantly; \"step\" = follow the chaser step / scene fade.\n"
                                   "Double-click a cell to edit; right-click a row to reset to \"step\".");
        hdr->setToolTip(LookColIn,  fadeTip);
        hdr->setToolTip(LookColOut, fadeTip);
    }
    // Tree view: an Effect look is a parent; Colour/Dimmer looks nested UNDER it
    // feed that effect (its gradient/master), while looks at the TOP level light
    // the fixtures directly (static base). Drag a colour onto an effect to feed
    // it, or out to the top level to make it a base.
    m_lookList->setRootIsDecorated(true);
    m_lookList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_lookList->setAllColumnsShowFocus(true);
    m_lookList->header()->setStretchLastSection(false);
    m_lookList->header()->setSectionResizeMode(LookColName, QHeaderView::Stretch);
    m_lookList->header()->setSectionResizeMode(LookColIn, QHeaderView::ResizeToContents);
    m_lookList->header()->setSectionResizeMode(LookColOut, QHeaderView::ResizeToContents);
    // Double-click an In/Out cell to give that look its own fade time.
    m_lookList->setItemDelegate(new FadeSpinDelegate(m_lookList));
    m_lookList->setEditTriggers(QAbstractItemView::DoubleClicked);
    // Order = precedence: looks lower in the list are applied last and win on
    // any channel they share (pan/tilt, colour, dimmer). Reorder by dragging
    // within the list or with the Up/Down buttons.
    m_lookList->setDragDropMode(QAbstractItemView::InternalMove);
    m_lookList->setDefaultDropAction(Qt::MoveAction);
    m_lookList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_lookList->setToolTip(
        tr("Colours/dimmers nested UNDER an Effect feed that effect (its gradient "
           "and master); looks at the top level light the fixtures directly.\n"
           "Drag a colour onto an effect to feed it, or out to the top level to "
           "make it a static base. Order still sets precedence among siblings.\n"
           "Double-click Fade In / Fade Out to set a per-look fade time. "
           "0 = snap instantly; \"step\" = follow the chaser step / scene fade "
           "(spin below 0, or right-click → Reset, to return to \"step\")."));
    lookCol->addWidget(m_lookList);
    QHBoxLayout *lookBtns = new QHBoxLayout();
    m_removeLookButton = new QPushButton(tr("Remove"), this);
    lookBtns->addWidget(m_removeLookButton);
    lookBtns->addStretch();
    m_moveLookUpButton = new QPushButton(QIcon(":/up.png"), QString(), this);
    m_moveLookUpButton->setToolTip(tr("Move look up (applied earlier — lower precedence)"));
    m_moveLookDownButton = new QPushButton(QIcon(":/down.png"), QString(), this);
    m_moveLookDownButton->setToolTip(tr("Move look down (applied later — higher precedence)"));
    lookBtns->addWidget(m_moveLookUpButton);
    lookBtns->addWidget(m_moveLookDownButton);
    lookCol->addLayout(lookBtns);
    cols->addLayout(lookCol);

    connect(m_removeTargetButton, SIGNAL(clicked()), this, SLOT(slotRemoveTarget()));
    connect(m_removeLookButton, SIGNAL(clicked()), this, SLOT(slotRemoveLook()));
    connect(m_moveLookUpButton, SIGNAL(clicked()), this, SLOT(slotMoveLookUp()));
    connect(m_moveLookDownButton, SIGNAL(clicked()), this, SLOT(slotMoveLookDown()));
    // Internal drag-drop reorder: QTreeWidget's internal move implements the drop
    // as take + insert, so the model emits rowsRemoved/rowsInserted, NOT rowsMoved
    // — a rowsMoved connection would silently never fire. Instead filter the
    // list's drop event and sync the order once the move settles. (See eventFilter.)
    m_lookList->viewport()->installEventFilter(this);
    connect(m_lookList, SIGNAL(itemSelectionChanged()),
            this, SLOT(slotLookSelectionChanged()));
    // Double-click the name column (re)loads the look into the editor even if it
    // was already selected; double-click In/Out edits that cell (via delegate).
    connect(m_lookList, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(slotLookDoubleClicked(QTreeWidgetItem*,int)));
    // A committed In/Out edit pushes the per-look fade to the scene.
    connect(m_lookList, SIGNAL(itemChanged(QTreeWidgetItem*,int)),
            this, SLOT(slotLookFadeEdited(QTreeWidgetItem*,int)));
    connect(m_lookList, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(slotLookContextMenu(QPoint)));
    connect(m_targetList, SIGNAL(itemSelectionChanged()),
            this, SLOT(slotTargetSelectionChanged()));
    connect(m_targetList, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(slotTargetContextMenu(QPoint)));

    reload();
}

SceneGroupLooks::~SceneGroupLooks()
{
}

QString SceneGroupLooks::lookLabel(quint32 paletteId) const
{
    QLCPalette *p = m_doc->palette(paletteId);
    if (p == NULL)
        return tr("(missing palette %1)").arg(paletteId);

    QString name = p->name();
    if (name.isEmpty())
    {
        const QString type = QLCPalette::typeToString(p->type());
        switch (p->type())
        {
        case QLCPalette::Color:
            name = QString("%1 %2").arg(type).arg(p->rgbValue().name()); break;
        case QLCPalette::PanTilt:
            name = QString("%1 P%2 / T%3")
                   .arg(type).arg(p->intValue1()).arg(p->intValue2()); break;
        case QLCPalette::Aim:
            name = type; break;
        default:
            name = QString("%1 %2").arg(type).arg(p->intValue1()); break;
        }
    }

    // Show the folder path too (e.g. "Shutters/Full"), stripping the
    // internal "Palettes/" category prefix.
    QString path = p->path();
    if (path.startsWith(QStringLiteral("Palettes/")))
        path = path.mid(9);
    else if (path == QStringLiteral("Palettes"))
        path.clear();
    if (path.endsWith('/'))
        path.chop(1);
    return path.isEmpty() ? name : path + "/" + name;
}

/** Returns a short "⚠ no look for: X, Y" string for capabilities the fixture
 *  has but the scene's current palette set does not cover.  Empty if all
 *  capabilities are covered (or the fixture has none of note). */
static QString capCoverageHint(Fixture *fxi, const QSet<QLCPalette::PaletteType> &covered)
{
    if (!fxi || !fxi->fixtureMode())
        return QString();

    bool hasRGB = false, hasDimmer = false, hasPT = false, needsShutter = false;
    for (quint32 c = 0; c < fxi->channels(); ++c)
    {
        QLCChannel *ch = fxi->fixtureMode()->channel(c);
        if (!ch) continue;
        switch (ch->group())
        {
            case QLCChannel::Pan:
            case QLCChannel::Tilt:
                hasPT = true; break;
            case QLCChannel::Intensity:
                if (ch->colour() == QLCChannel::Red   ||
                    ch->colour() == QLCChannel::Green ||
                    ch->colour() == QLCChannel::Blue)
                    hasRGB = true;
                else
                    hasDimmer = true;
                break;
            case QLCChannel::Colour:
                if (ch->colour() == QLCChannel::Red   ||
                    ch->colour() == QLCChannel::Green ||
                    ch->colour() == QLCChannel::Blue)
                    hasRGB = true;
                break;
            case QLCChannel::Shutter:
            {
                // Only flag shutter as "needs a look" when DMX value 0 defaults
                // to ShutterClose (beam blocked). Fixtures where 0 = ShutterOpen
                // or strobe-off (LED Movinghead-style) are fine without a look.
                QLCCapability *cap = ch->searchCapability(0);
                if (cap && cap->preset() == QLCCapability::ShutterClose)
                    needsShutter = true;
                break;
            }
            default: break;
        }
    }

    QStringList missing;
    if (hasRGB       && !covered.contains(QLCPalette::Color))  missing << QObject::tr("color");
    if (hasDimmer    && !covered.contains(QLCPalette::Dimmer)) missing << QObject::tr("dimmer");
    if (hasPT        && !covered.contains(QLCPalette::PanTilt) &&
                        !covered.contains(QLCPalette::Aim))    missing << QObject::tr("pan/tilt");
    if (needsShutter && !covered.contains(QLCPalette::Shutter))missing << QObject::tr("shutter");

    if (missing.isEmpty())
        return QString();
    return QObject::tr("  ⚠ no look for: %1").arg(missing.join(", "));
}


void SceneGroupLooks::reload()
{
    // Preserve the effective selection across the rebuild (a type folder is
    // resolved to its fixtures) so adding/removing a target doesn't drop the
    // selection — which would hide the console and make the splitter jump.
    QSet<quint32> selFix, selGrp;
    foreach (QTreeWidgetItem *it, m_targetList->selectedItems())
    {
        const int kind = it->data(0, TARGET_KIND_ROLE).toInt();
        if (kind == TargetFixture)
            selFix.insert(it->data(0, Qt::UserRole).toUInt());
        else if (kind == TargetGroup)
            selGrp.insert(it->data(0, Qt::UserRole).toUInt());
        else if (kind == TargetTypeFolder)
            for (int c = 0; c < it->childCount(); c++)
                selFix.insert(it->child(c)->data(0, Qt::UserRole).toUInt());
    }

    m_targetList->blockSignals(true);
    m_targetList->clear();

    // Which palette types does the current look cover?
    QSet<QLCPalette::PaletteType> coveredTypes;
    foreach (quint32 pid, m_scene->palettes())
    {
        QLCPalette *p = m_doc->palette(pid);
        if (p) coveredTypes.insert(p->type());
    }

    // Groups = dynamic targets (top level).
    QList<FixtureGroup*> groups;
    foreach (quint32 gid, m_scene->fixtureGroups())
    {
        FixtureGroup *g = m_doc->fixtureGroup(gid);
        if (g != NULL)
            groups.append(g);
    }
    std::sort(groups.begin(), groups.end(),
              [](FixtureGroup *a, FixtureGroup *b) {
                  return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
              });
    foreach (FixtureGroup *g, groups)
    {
        // Aggregate coverage hint across all fixtures in the group.
        QSet<QString> groupMissing;
        foreach (quint32 fid, g->fixtureList())
        {
            const QString hint = capCoverageHint(m_doc->fixture(fid), coveredTypes);
            if (!hint.isEmpty())
            {
                // Extract just the capability names from "  ⚠ no look for: x, y"
                QString caps = hint;
                const int colonIdx = caps.indexOf(':');
                if (colonIdx >= 0)
                    for (const QString &c : caps.mid(colonIdx + 1).trimmed().split(','))
                        groupMissing.insert(c.trimmed());
            }
        }
        QTreeWidgetItem *it = new QTreeWidgetItem(m_targetList);
        QString label = tr("%1  — group").arg(g->name());
        if (!groupMissing.isEmpty())
        {
            QStringList ml = groupMissing.values();
            std::sort(ml.begin(), ml.end());
            label += tr("  ⚠ no look for: %1").arg(ml.join(", "));
            it->setForeground(0, QColor(200, 130, 0));
        }
        it->setText(0, label);
        it->setData(0, Qt::UserRole, g->id());
        it->setData(0, TARGET_KIND_ROLE, TargetGroup);
    }

    // Fixtures = fixed targets, grouped under per-type folders.
    if (m_includeFixtureTargets)
    {
        QMap<QString, QList<Fixture*> > byType; // type label -> fixtures
        foreach (quint32 fid, m_scene->fixtures())
        {
            Fixture *f = m_doc->fixture(fid);
            if (f != NULL)
                byType[fixtureTypeLabel(f)].append(f);
        }

        QMapIterator<QString, QList<Fixture*> > tit(byType);
        while (tit.hasNext())
        {
            tit.next();
            QList<Fixture*> fxs = tit.value();
            std::sort(fxs.begin(), fxs.end(),
                      [](Fixture *a, Fixture *b) {
                          return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
                      });

            QTreeWidgetItem *folder = new QTreeWidgetItem(m_targetList);
            folder->setText(0, tr("%1  (%n)", "", fxs.count()).arg(tit.key()));
            folder->setData(0, TARGET_KIND_ROLE, TargetTypeFolder);
            folder->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            folder->setExpanded(true);
            foreach (Fixture *f, fxs)
            {
                QTreeWidgetItem *it = new QTreeWidgetItem(folder);
                const QString hint = capCoverageHint(f, coveredTypes);
                it->setText(0, hint.isEmpty() ? f->name() : f->name() + hint);
                if (!hint.isEmpty())
                    it->setForeground(0, QColor(200, 130, 0));
                it->setData(0, Qt::UserRole, f->id());
                it->setData(0, TARGET_KIND_ROLE, TargetFixture);
            }
        }
    }

    if (m_targetList->topLevelItemCount() == 0)
    {
        QTreeWidgetItem *it = new QTreeWidgetItem(m_targetList);
        it->setText(0, tr("(no fixtures — drag groups or fixtures here)"));
        it->setFlags(Qt::NoItemFlags);
    }

    // Restore the previous selection (re-select fixture children / groups),
    // then re-emit once so the bottom panel matches.
    QTreeWidgetItemIterator rit(m_targetList);
    while (*rit != NULL)
    {
        QTreeWidgetItem *it = *rit;
        const int kind = it->data(0, TARGET_KIND_ROLE).toInt();
        if (kind == TargetFixture && selFix.contains(it->data(0, Qt::UserRole).toUInt()))
            it->setSelected(true);
        else if (kind == TargetGroup && selGrp.contains(it->data(0, Qt::UserRole).toUInt()))
            it->setSelected(true);
        ++rit;
    }
    m_targetList->blockSignals(false);
    slotTargetSelectionChanged();

    // Total distinct fixtures across all targets (fixed + dynamic groups).
    QSet<quint32> allFixtures;
    foreach (quint32 fid, m_scene->fixtures())
        allFixtures.insert(fid);
    foreach (FixtureGroup *g, groups)
        foreach (quint32 fid, g->fixtureList())
            allFixtures.insert(fid);
    m_targetsLabel->setText(tr("Fixtures in Scene (count %n)", "", allFixtures.count()));

    // Looks = palettes — preserve the current selection so that moving an
    // effect slider (which triggers reload via slotLookEdited) doesn't
    // deselect the palette and clear the LookEditor.
    quint32 selPid = QLCPalette::invalidId();
    {
        const QList<QTreeWidgetItem*> prevSel = m_lookList->selectedItems();
        if (!prevSel.isEmpty())
            selPid = prevSel.first()->data(LookColName, Qt::UserRole).toUInt();
    }

    m_lookList->blockSignals(true);
    m_lookList->clear();
    // Build the tree from the ordered palette list: each Effect becomes a
    // top-level parent, and any Colour/Dimmer that FOLLOWS it (until the next
    // effect) is nested under it as a child that feeds it. Looks before the first
    // effect stay top-level — the static fixture base. The flat order is the
    // source of truth (see QLCPalette::isEffectScoped); the tree is derived from
    // it and re-derived after every reorder, so nesting == "after this effect".
    QTreeWidgetItem *currentEffectParent = NULL;
    foreach (quint32 pid, m_scene->palettes())
    {
        QLCPalette *p = m_doc->palette(pid);
        const bool isEffect = (p != NULL && p->type() == QLCPalette::Effect);

        // Effects (and any look before the first effect) are top-level; a
        // non-effect after an effect nests under it.
        QTreeWidgetItem *it = (isEffect || currentEffectParent == NULL)
                                  ? new QTreeWidgetItem(m_lookList)
                                  : new QTreeWidgetItem(currentEffectParent);
        it->setText(LookColName, lookLabel(pid));
        it->setData(LookColName, Qt::UserRole, pid);
        if (p != NULL)
            it->setIcon(LookColName, QIcon(p->iconResource()));

        const bool feedsEffect = (!isEffect && currentEffectParent != NULL);
        it->setToolTip(LookColName,
            isEffect    ? tr("Effect. Nest colours/dimmers under it to feed its gradient and master.")
            : feedsEffect ? tr("Feeds the effect above — not painted on the fixtures directly.")
                          : tr("Lights the fixtures directly (static base)."));

        // In/Out fade cells (ms at Qt::UserRole; -1 = "step").
        //  • Effect: Fade In is the scene envelope (inert); Fade Out is a real
        //    release time the runner honours (live).
        //  • Colour/Dimmer feeding an effect: BOTH inert — it's a value the effect
        //    reads, not a painted look, so it has no fade of its own.
        //  • Top-level base look: both live.
        const bool inInert  = isEffect || feedsEffect;
        const bool outInert = feedsEffect;
        const int inMs  = isEffect ? -1 : m_scene->paletteFadeIn(pid);
        const int outMs = m_scene->paletteFadeOut(pid);
        it->setData(LookColIn,  Qt::UserRole, inMs);
        it->setData(LookColOut, Qt::UserRole, outMs);
        it->setData(LookColIn,  LookFadeInertRole, inInert);
        it->setData(LookColOut, LookFadeInertRole, outInert);
        it->setText(LookColIn,  inInert  ? QStringLiteral("—") : fadeCellText(inMs));
        it->setText(LookColOut, outInert ? QStringLiteral("—") : fadeCellText(outMs));
        it->setTextAlignment(LookColIn,  Qt::AlignRight | Qt::AlignVCenter);
        it->setTextAlignment(LookColOut, Qt::AlignRight | Qt::AlignVCenter);
        if (isEffect)
            it->setToolTip(LookColOut,
                tr("Release fade: when this look stops, the effect fades out over "
                   "this time instead of snapping off. \"step\" = snap.\n"
                   "Per-key attack/decay comes from the effect's own parameters "
                   "(e.g. Glow / Release), not this column."));
        else if (feedsEffect)
            it->setToolTip(LookColName,
                tr("Feeds the effect above (its colour) — not painted on the "
                   "fixtures, so it has no fade of its own."));

        // Draggable + (In/Out) editable. Only EFFECT items accept drops, so
        // dropping a colour onto one nests it (feeds the effect); dropping between
        // rows at the top level makes it a base. Leaves never accept children.
        // Editable so a fade cell can be double-clicked (the delegate restricts an
        // effect to its Fade Out cell). Only Effect items accept drops (nesting).
        Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable
                        | Qt::ItemIsDragEnabled | Qt::ItemIsEditable;
        if (isEffect)
            f |= Qt::ItemIsDropEnabled;
        else
            f &= ~Qt::ItemIsDropEnabled;
        it->setFlags(f);

        if (isEffect)
            currentEffectParent = it;

        if (selPid != QLCPalette::invalidId() && pid == selPid)
            it->setSelected(true);
    }
    m_lookList->expandAll();
    m_lookList->blockSignals(false);
}

// Routed by MIME: palette -> look, fixture group -> dynamic target,
// fixture -> fixed target.
static bool hasAcceptedFormat(const QMimeData *mime)
{
    return mime->hasFormat(FunctionsTreeWidget::paletteDragMimeType())
        || mime->hasFormat(FixtureGroupSource::fixtureGroupMimeType())
        || mime->hasFormat(FixtureGroupSource::fixtureMimeType());
}

void SceneGroupLooks::dragEnterEvent(QDragEnterEvent *event)
{
    if (hasAcceptedFormat(event->mimeData()))
        event->acceptProposedAction();
}

void SceneGroupLooks::dragMoveEvent(QDragMoveEvent *event)
{
    if (hasAcceptedFormat(event->mimeData()))
        event->acceptProposedAction();
}

void SceneGroupLooks::dropEvent(QDropEvent *event)
{
    const QMimeData *mime = event->mimeData();
    if (hasAcceptedFormat(mime) == false)
        return;

    bool changed = false;
    bool palettesAdded = false;

    // If the drop landed on an Effect row in the Looks list, nest any dropped
    // colours/dimmers UNDER that effect (feed it) instead of appending them.
    quint32 dropEffectPid = QLCPalette::invalidId();
    {
        const QPoint lp = m_lookList->viewport()->mapFrom(this, event->pos());
        QTreeWidgetItem *it = m_lookList->itemAt(lp);
        while (it != NULL)
        {
            const quint32 pid = it->data(LookColName, Qt::UserRole).toUInt();
            QLCPalette *p = m_doc->palette(pid);
            if (p != NULL && p->type() == QLCPalette::Effect) { dropEffectPid = pid; break; }
            it = it->parent();
        }
    }
    QList<quint32> addedPalettePids;

    if (mime->hasFormat(FunctionsTreeWidget::paletteDragMimeType()))
    {
        QByteArray data = mime->data(FunctionsTreeWidget::paletteDragMimeType());
        QDataStream stream(&data, QIODevice::ReadOnly);
        while (stream.atEnd() == false)
        {
            quint32 pid = 0;
            stream >> pid;
            if (m_doc->palette(pid) != NULL
                && m_scene->palettes().contains(pid) == false)
            {
                m_scene->addPalette(pid);
                addedPalettePids << pid;
                changed = true;
                palettesAdded = true;
                QLCPalette *ap = m_doc->palette(pid);
                qDebug() << "[DROP] add palette/look" << pid
                            << (ap ? ap->name() : "?")
                            << "type=" << (ap ? (int)ap->type() : -1);
            }
        }
    }

    if (mime->hasFormat(FixtureGroupSource::fixtureGroupMimeType()))
    {
        QByteArray data = mime->data(FixtureGroupSource::fixtureGroupMimeType());
        QDataStream stream(&data, QIODevice::ReadOnly);
        while (stream.atEnd() == false)
        {
            quint32 gid = 0;
            stream >> gid;
            FixtureGroup *g = m_doc->fixtureGroup(gid);
            if (g != NULL && m_scene->fixtureGroups().contains(gid) == false)
            {
                m_scene->addFixtureGroup(gid);
                changed = true;

                qDebug() << "[DROP] add group" << gid << (g ? g->name() : "?")
                            << "members=" << g->fixtureList()
                            << "| scene fixtures BEFORE=" << m_scene->fixtures()
                            << "groups=" << m_scene->fixtureGroups();

                // The group's applied looks (non-effect palettes) will assert on
                // these fixtures at runtime. Only strip baked values those looks
                // actually replace — stripping a channel no palette covers would
                // leave the fixture dark (nothing else drives it).
                QSet<quint64> covered;
                foreach (quint32 pid, m_scene->palettes())
                {
                    QLCPalette *p = m_doc->palette(pid);
                    if (p == NULL || p->type() == QLCPalette::Effect)
                        continue;
                    const int cOff = QLCPalette::colorSetOffset(m_doc, m_scene->palettes(), pid);
                    foreach (const SceneValue &scv,
                             p->valuesFromFixtureGroups(m_doc, QList<quint32>() << gid, nullptr, cOff))
                        covered.insert((quint64(scv.fxi) << 32) | scv.channel);
                }

                foreach (quint32 fid, g->fixtureList())
                {
                    if (m_scene->fixtures().contains(fid) == false)
                        continue;

                    // Keep any baked value the group's looks don't cover, so the
                    // fixture stays lit; drop only the covered ones.
                    QList<SceneValue> keep;
                    foreach (const SceneValue &scv, m_scene->values())
                    {
                        if (scv.fxi != fid)
                            continue;
                        if (covered.contains((quint64(fid) << 32) | scv.channel))
                            m_scene->unsetValue(fid, scv.channel);
                        else
                            keep << scv;
                    }
                    // Only drop the fixed target if the group's looks fully cover
                    // it; otherwise keep it so its uncovered channels still light.
                    if (keep.isEmpty())
                        m_scene->removeFixture(fid);

                    qDebug() << "[DROP]   fixture" << fid
                                << "coveredByLooks=" << (keep.isEmpty())
                                << "keptBakedValues=" << keep.size();
                }

                qDebug() << "[DROP] scene fixtures AFTER=" << m_scene->fixtures()
                            << "groups=" << m_scene->fixtureGroups();
            }
        }
    }

    if (mime->hasFormat(FixtureGroupSource::fixtureMimeType()))
    {
        QByteArray data = mime->data(FixtureGroupSource::fixtureMimeType());
        QDataStream stream(&data, QIODevice::ReadOnly);
        while (stream.atEnd() == false)
        {
            quint32 fid = 0;
            stream >> fid;
            if (m_doc->fixture(fid) != NULL
                && m_scene->fixtures().contains(fid) == false)
            {
                m_scene->addFixture(fid);
                changed = true;
                qDebug() << "[DROP] add single fixture" << fid
                            << "| scene fixtures now=" << m_scene->fixtures();
            }
        }
    }

    // When a look was applied, let the palettes take over their channels.
    // Deferred so the modal prompt doesn't spin a nested event loop inside the
    // drop event (before it's been accepted / the drag source cleaned up).
    if (palettesAdded)
        QTimer::singleShot(0, this, [this]() { reconcileAfterPaletteApply(); });

    // Nest the just-dropped looks under the effect they were dropped on. This
    // reorders + reloads + emits, so skip the generic refresh below.
    const bool nested = (dropEffectPid != QLCPalette::invalidId() && !addedPalettePids.isEmpty());
    if (nested)
        moveLooksToTarget(addedPalettePids, dropEffectPid);
    else if (changed)
    {
        m_doc->setModified();
        reload();
        emit sceneModified();
    }
    event->acceptProposedAction();
}

void SceneGroupLooks::reconcileAfterPaletteApply()
{
    // Channels covered by every currently-applied palette (key = fxi<<32|ch).
    QSet<quint64> covered;
    foreach (quint32 pid, m_scene->palettes())
    {
        QLCPalette *p = m_doc->palette(pid);
        if (p == NULL)
            continue;
        const int cOff = QLCPalette::colorSetOffset(m_doc, m_scene->palettes(), pid);
        foreach (const SceneValue &scv, p->valuesFromFixtureGroups(m_doc, m_scene->fixtureGroups(), nullptr, cOff))
            covered.insert((quint64(scv.fxi) << 32) | scv.channel);
        foreach (const SceneValue &scv, p->valuesFromFixtures(m_doc, m_scene->fixtures(), nullptr, cOff))
            covered.insert((quint64(scv.fxi) << 32) | scv.channel);
    }

    // Strip baked values the palettes now control (palette paramount); gather
    // the rest (baked channels no applied look covers).
    QList<SceneValue> uncovered;
    foreach (const SceneValue &scv, m_scene->values())
    {
        if (covered.contains((quint64(scv.fxi) << 32) | scv.channel))
            m_scene->unsetValue(scv.fxi, scv.channel);
        else
            uncovered << scv;
    }

    if (uncovered.isEmpty() == false)
    {
        // Count the distinct fixtures involved so the total reads sensibly
        // (it's per-fixture channels × fixtures, not just "channels").
        QSet<quint32> fxis;
        foreach (const SceneValue &scv, uncovered)
            fxis.insert(scv.fxi);
        const int ret = QMessageBox::question(this, tr("Apply look"),
            tr("This scene still sets %1 channel value(s) across %2 fixture(s) "
               "that no applied look covers.\n"
               "Clear them so the applied looks fully control this scene?")
               .arg(uncovered.count()).arg(fxis.count()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret == QMessageBox::Yes)
            foreach (const SceneValue &scv, uncovered)
                m_scene->unsetValue(scv.fxi, scv.channel);
    }

    // Trigger a second runtime reset so the scene fader is rebuilt from the
    // NOW-reconciled value list (baked channels stripped above).  The first
    // resetRuntime fired synchronously in the dropEvent's sceneModified path,
    // before this deferred call ran.
    emit sceneModified();
}

void SceneGroupLooks::slotRemoveTarget()
{
    const QList<QTreeWidgetItem*> sel = m_targetList->selectedItems();
    if (sel.isEmpty())
        return;

    // Resolve the selection to fixture ids + group ids (a type folder removes
    // all its fixtures).
    QSet<quint32> fixIds, grpIds;
    foreach (QTreeWidgetItem *it, sel)
    {
        const int kind = it->data(0, TARGET_KIND_ROLE).toInt();
        if (kind == TargetFixture)
            fixIds.insert(it->data(0, Qt::UserRole).toUInt());
        else if (kind == TargetGroup)
            grpIds.insert(it->data(0, Qt::UserRole).toUInt());
        else if (kind == TargetTypeFolder)
            for (int c = 0; c < it->childCount(); c++)
                fixIds.insert(it->child(c)->data(0, Qt::UserRole).toUInt());
    }

    bool changed = false;
    foreach (quint32 id, fixIds)
    {
        // Also clear any baked static values for this fixture, otherwise they
        // are written AFTER (and override) the scene's palette looks.
        Fixture *f = m_doc->fixture(id);
        if (f != NULL)
            for (quint32 i = 0; i < f->channels(); i++)
                m_scene->unsetValue(id, i);
        changed = m_scene->removeFixture(id) || changed;
    }
    foreach (quint32 id, grpIds)
        changed = m_scene->removeFixtureGroup(id) || changed;

    if (changed)
    {
        m_doc->setModified();
        reload();
        emit sceneModified();
    }
}

void SceneGroupLooks::slotTargetContextMenu(const QPoint &pos)
{
    if (m_targetList->selectedItems().isEmpty())
        return;
    QMenu menu(m_targetList);
    QAction *removeAct = menu.addAction(tr("Remove"));
    if (menu.exec(m_targetList->viewport()->mapToGlobal(pos)) == removeAct)
        slotRemoveTarget();
}

void SceneGroupLooks::slotLookSelectionChanged()
{
    const QList<QTreeWidgetItem*> sel = m_lookList->selectedItems();
    emit lookSelected(sel.isEmpty() ? QLCPalette::invalidId()
                                    : sel.first()->data(LookColName, Qt::UserRole).toUInt());
}

void SceneGroupLooks::slotLookDoubleClicked(QTreeWidgetItem *item, int column)
{
    if (item == NULL)
        return;
    // Double-clicking In/Out opens the fade editor (delegate) — leave it be.
    if (column != LookColName)
        return;
    // Force-load this look into the editor (switches the bottom panel away
    // from the per-fixture channel editor).
    emit lookSelected(item->data(LookColName, Qt::UserRole).toUInt());
}

void SceneGroupLooks::slotLookFadeEdited(QTreeWidgetItem *item, int column)
{
    if (item == NULL || (column != LookColIn && column != LookColOut))
        return;
    const quint32 pid = item->data(LookColName, Qt::UserRole).toUInt();
    if (pid == QLCPalette::invalidId())
        return;
    // Keep the display text in step with the committed value.
    const int inMs  = item->data(LookColIn,  Qt::UserRole).toInt();
    const int outMs = item->data(LookColOut, Qt::UserRole).toInt();
    m_scene->setPaletteFade(pid, inMs, outMs);
    m_doc->setModified();
    emit sceneModified();
}

void SceneGroupLooks::moveLooksToTarget(const QList<quint32> &pids, quint32 effectPid)
{
    if (pids.isEmpty())
        return;

    QList<quint32> order = m_scene->palettes();
    // Pull the moved palettes out, preserving their current relative order.
    QList<quint32> block;
    foreach (quint32 pid, order)
        if (pids.contains(pid))
            block << pid;
    foreach (quint32 pid, block)
        order.removeAll(pid);

    int pos;
    if (effectPid == QLCPalette::invalidId())
    {
        // Base: insert before the first Effect (so isEffectScoped() is false).
        pos = order.size();
        for (int i = 0; i < order.size(); ++i)
        {
            QLCPalette *p = m_doc->palette(order.at(i));
            if (p != NULL && p->type() == QLCPalette::Effect) { pos = i; break; }
        }
    }
    else
    {
        // Feed effect: insert right after it (becomes its first child).
        pos = order.indexOf(effectPid) + 1;
        if (pos == 0)
            pos = order.size();   // effect vanished — fall back to the end
    }
    for (int i = 0; i < block.size(); ++i)
        order.insert(pos + i, block.at(i));

    if (m_scene->reorderPalettes(order))
    {
        m_doc->setModified();
        emit sceneModified();
        reload();
    }
}

void SceneGroupLooks::slotLookContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *clicked = m_lookList->itemAt(pos);
    QList<QTreeWidgetItem*> rows = m_lookList->selectedItems();
    if (clicked != NULL && rows.contains(clicked) == false)
        rows = QList<QTreeWidgetItem*>() << clicked;
    // Only looks that carry a fade (non-Effect, editable) can be reset / re-homed.
    rows.erase(std::remove_if(rows.begin(), rows.end(), [](QTreeWidgetItem *it) {
                   return (it->flags() & Qt::ItemIsEditable) == 0;
               }), rows.end());
    if (rows.isEmpty())
        return;

    // Collect the moved palette ids + whether any row is currently nested under
    // an effect (→ can become base) or at the top level (→ can feed an effect).
    QList<quint32> movePids;
    bool anyNested = false, anyBase = false;
    foreach (QTreeWidgetItem *it, rows)
    {
        movePids << it->data(LookColName, Qt::UserRole).toUInt();
        if (it->parent() != NULL) anyNested = true; else anyBase = true;
    }
    // The look's effects, in list order, for the "Feed effect ▸" submenu.
    QList<QPair<quint32,QString>> effects;
    foreach (quint32 pid, m_scene->palettes())
    {
        QLCPalette *p = m_doc->palette(pid);
        if (p != NULL && p->type() == QLCPalette::Effect)
            effects << qMakePair(pid, p->name());
    }

    QMenu menu(m_lookList);
    // --- Role: base (fixtures) vs feed an effect ---
    QAction *toBase = NULL;
    QHash<QAction*, quint32> feedActions;
    if (!effects.isEmpty())
    {
        if (anyNested)
            toBase = menu.addAction(tr("Move to fixtures (static base)"));
        if (anyBase || anyNested)
        {
            QMenu *feed = menu.addMenu(tr("Feed effect"));
            for (const auto &e : effects)
            {
                QAction *a = feed->addAction(e.second.isEmpty() ? tr("(effect)") : e.second);
                feedActions.insert(a, e.first);
            }
        }
        menu.addSeparator();
    }
    QAction *rin   = menu.addAction(tr("Reset Fade In to step"));
    QAction *rout  = menu.addAction(tr("Reset Fade Out to step"));
    QAction *rboth = menu.addAction(tr("Reset both to step"));
    QAction *chosen = menu.exec(m_lookList->viewport()->mapToGlobal(pos));
    if (chosen == NULL)
        return;

    if (chosen == toBase)
    {
        moveLooksToTarget(movePids, QLCPalette::invalidId());
        return;
    }
    if (feedActions.contains(chosen))
    {
        moveLooksToTarget(movePids, feedActions.value(chosen));
        return;
    }

    const bool resetIn  = (chosen == rin  || chosen == rboth);
    const bool resetOut = (chosen == rout || chosen == rboth);

    m_lookList->blockSignals(true);
    foreach (QTreeWidgetItem *it, rows)
    {
        const quint32 pid = it->data(LookColName, Qt::UserRole).toUInt();
        int inMs  = it->data(LookColIn,  Qt::UserRole).toInt();
        int outMs = it->data(LookColOut, Qt::UserRole).toInt();
        if (resetIn)  inMs  = -1;
        if (resetOut) outMs = -1;
        it->setData(LookColIn,  Qt::UserRole, inMs);
        it->setData(LookColOut, Qt::UserRole, outMs);
        it->setText(LookColIn,  fadeCellText(inMs));
        it->setText(LookColOut, fadeCellText(outMs));
        m_scene->setPaletteFade(pid, inMs, outMs);
    }
    m_lookList->blockSignals(false);
    slotLookSelectionChanged();  // refresh bottom look editor fade spinboxes

    m_doc->setModified();
    emit sceneModified();
}

void SceneGroupLooks::slotTargetSelectionChanged()
{
    // Selecting a type folder edits all its fixtures together. Preserve order
    // and de-duplicate (a folder + one of its children could both be selected).
    QList<quint32> fixtures;
    foreach (QTreeWidgetItem *it, m_targetList->selectedItems())
    {
        const int kind = it->data(0, TARGET_KIND_ROLE).toInt();
        if (kind == TargetFixture)
        {
            const quint32 id = it->data(0, Qt::UserRole).toUInt();
            if (fixtures.contains(id) == false)
                fixtures << id;
        }
        else if (kind == TargetTypeFolder)
        {
            for (int c = 0; c < it->childCount(); c++)
            {
                const quint32 id = it->child(c)->data(0, Qt::UserRole).toUInt();
                if (fixtures.contains(id) == false)
                    fixtures << id;
            }
        }
        else if (kind == TargetGroup)
        {
            const quint32 gid = it->data(0, Qt::UserRole).toUInt();
            FixtureGroup *g = m_doc->fixtureGroup(gid);
            qDebug() << "TargetGroup selected: gid=" << gid
                     << "group=" << (g ? g->name() : "(null)")
                     << "fixtures=" << (g ? g->fixtureList() : QList<quint32>());
            if (g != NULL)
            {
                foreach (quint32 fid, g->fixtureList())
                    if (fixtures.contains(fid) == false)
                        fixtures << fid;
            }
        }
    }
    qDebug() << "slotTargetSelectionChanged: emitting" << fixtures.size() << "fixtures" << fixtures;
    emit fixturesSelected(fixtures);
}

void SceneGroupLooks::slotRemoveLook()
{
    const QList<QTreeWidgetItem*> sel = m_lookList->selectedItems();
    if (sel.isEmpty())
        return;
    // Detach from the scene only; leave the palette in the Doc so any
    // other scene referencing it keeps working.
    foreach (QTreeWidgetItem *it, sel)
        m_scene->removePalette(it->data(LookColName, Qt::UserRole).toUInt());
    m_doc->setModified();
    reload();
    emit sceneModified();
}

void SceneGroupLooks::slotMoveLookUp()
{
    moveSelectedLooks(-1);
}

void SceneGroupLooks::slotMoveLookDown()
{
    moveSelectedLooks(+1);
}

void SceneGroupLooks::moveSelectedLooks(int delta)
{
    const QList<QTreeWidgetItem*> sel = m_lookList->selectedItems();
    if (sel.isEmpty() || delta == 0)
        return;

    // Move in the FLAT visual order (== the palette order), so a step can cross a
    // nesting boundary: Up on a colour at the top of an effect's children pops it
    // ABOVE the effect (→ becomes a base on reload); Down on a base colour pushes
    // it under the following effect. reorderPalettes → reload() re-derives the
    // tree, so nesting follows position automatically.
    QList<quint32> order = m_scene->palettes();
    QList<quint32> selPids;
    foreach (QTreeWidgetItem *it, sel)
        selPids << it->data(LookColName, Qt::UserRole).toUInt();

    QList<int> idx;
    foreach (quint32 pid, selPids)
    {
        const int i = order.indexOf(pid);
        if (i >= 0) idx << i;
    }
    if (idx.isEmpty())
        return;
    std::sort(idx.begin(), idx.end());
    // Block-edge guard: don't run off either end.
    if (idx.first() + delta < 0 || idx.last() + delta >= order.size())
        return;
    // Move each item; process the trailing edge first so indices stay valid.
    if (delta > 0)
        std::reverse(idx.begin(), idx.end());
    foreach (int i, idx)
        order.move(i, i + delta);

    if (m_scene->reorderPalettes(order))
    {
        m_doc->setModified();
        emit sceneModified();
        reload();
        // Re-select the moved looks (reload() rebuilt the tree).
        QTreeWidgetItemIterator sit(m_lookList);
        while (*sit)
        {
            if (selPids.contains((*sit)->data(LookColName, Qt::UserRole).toUInt()))
                (*sit)->setSelected(true);
            ++sit;
        }
    }
}

bool SceneGroupLooks::eventFilter(QObject *watched, QEvent *event)
{
    // The look list reorders by internal drag-drop. QTreeWidget performs the drop
    // as a take+insert (rowsRemoved + rowsInserted), so there is no rowsMoved to
    // hook — instead we watch the viewport for the drop itself. Let the view
    // process the drop normally (return false), then sync the resulting order.
    if (watched == m_lookList->viewport() && event->type() == QEvent::Drop)
        slotLooksReordered();
    return QWidget::eventFilter(watched, event);
}

void SceneGroupLooks::slotLooksReordered()
{
    // Fired after an internal drag-drop. Defer the sync so it runs once the drop
    // fully settles (items taken/inserted), not mid-operation.
    QTimer::singleShot(0, this, [this]() { applyLookOrderFromList(); });
}

void SceneGroupLooks::applyLookOrderFromList()
{
    // Depth-first walk: each top-level item, then its children in order. A child
    // (colour nested under an effect) thus lands right after its effect in the
    // flat order, which is exactly what marks it "effect-scoped" (isEffectScoped).
    // reorderPalettes → reload() re-derives the canonical tree from this order, so
    // any transient/invalid nesting (e.g. an effect dropped under an effect) is
    // flattened back automatically.
    QList<quint32> order;
    std::function<void(QTreeWidgetItem*)> walk = [&](QTreeWidgetItem *item) {
        order << item->data(LookColName, Qt::UserRole).toUInt();
        for (int c = 0; c < item->childCount(); ++c)
            walk(item->child(c));
    };
    for (int i = 0; i < m_lookList->topLevelItemCount(); ++i)
        walk(m_lookList->topLevelItem(i));

    if (m_scene->reorderPalettes(order))
    {
        m_doc->setModified();
        emit sceneModified();
        // Rebuild the tree from the new flat order so nesting is canonical (a
        // colour dropped between two effects re-homes under the effect above it).
        reload();
    }
}
