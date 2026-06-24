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
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDataStream>
#include <QMessageBox>
#include <QMenu>
#include <QSet>
#include <QMap>
#include <QTimer>

#include <algorithm>

#include "scenegrouplooks.h"
#include "functionstreewidget.h"   // palette MIME type
#include "fixturegroupsource.h"    // fixture-group / fixture MIME types
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

// Target kinds, stored at Qt::UserRole + 1 on target items.
enum { TargetGroup = 0, TargetFixture = 1, TargetTypeFolder = 2 };
#define TARGET_KIND_ROLE (Qt::UserRole + 1)

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
        tr("<b>Looks</b> (palettes) are applied to this scene's <b>targets</b>. "
           "Every look applies to every target.<br>"
           "Drag here to add: <b>palettes</b> &rarr; looks; "
           "<b>fixture groups</b> &rarr; targets (dynamic — follow membership); "
           "<b>fixtures</b> &rarr; targets (fixed)."), this);
    header->setWordWrap(true);
    root->addWidget(header);

    QHBoxLayout *cols = new QHBoxLayout();
    root->addLayout(cols);

    // --- Targets column (groups + optionally fixtures) ---
    QVBoxLayout *targetCol = new QVBoxLayout();
    m_targetsLabel = new QLabel(tr("Targets"), this);
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
    m_lookList = new QListWidget(this);
    m_lookList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    lookCol->addWidget(m_lookList);
    QHBoxLayout *lookBtns = new QHBoxLayout();
    m_removeLookButton = new QPushButton(tr("Remove"), this);
    lookBtns->addWidget(m_removeLookButton);
    lookBtns->addStretch();
    lookCol->addLayout(lookBtns);
    cols->addLayout(lookCol);

    connect(m_removeTargetButton, SIGNAL(clicked()), this, SLOT(slotRemoveTarget()));
    connect(m_removeLookButton, SIGNAL(clicked()), this, SLOT(slotRemoveLook()));
    connect(m_lookList, SIGNAL(itemSelectionChanged()),
            this, SLOT(slotLookSelectionChanged()));
    // Double-click (re)loads the look into the editor even if it was already
    // selected (a plain click on an already-selected look fires no change).
    connect(m_lookList, SIGNAL(itemDoubleClicked(QListWidgetItem*)),
            this, SLOT(slotLookDoubleClicked(QListWidgetItem*)));
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
        QTreeWidgetItem *it = new QTreeWidgetItem(m_targetList);
        it->setText(0, tr("%1  — group (dynamic)").arg(g->name()));
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
                it->setText(0, f->name());
                it->setData(0, Qt::UserRole, f->id());
                it->setData(0, TARGET_KIND_ROLE, TargetFixture);
            }
        }
    }

    if (m_targetList->topLevelItemCount() == 0)
    {
        QTreeWidgetItem *it = new QTreeWidgetItem(m_targetList);
        it->setText(0, tr("(no targets — drag groups or fixtures here)"));
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
    m_targetsLabel->setText(tr("Targets (%n fixture(s))", "", allFixtures.count()));

    // Looks = palettes — preserve the current selection so that moving an
    // effect slider (which triggers reload via slotLookEdited) doesn't
    // deselect the palette and clear the LookEditor.
    quint32 selPid = QLCPalette::invalidId();
    {
        const QList<QListWidgetItem*> prevSel = m_lookList->selectedItems();
        if (!prevSel.isEmpty())
            selPid = prevSel.first()->data(Qt::UserRole).toUInt();
    }

    m_lookList->blockSignals(true);
    m_lookList->clear();
    foreach (quint32 pid, m_scene->palettes())
    {
        QListWidgetItem *it = new QListWidgetItem(lookLabel(pid), m_lookList);
        it->setData(Qt::UserRole, pid);
        QLCPalette *p = m_doc->palette(pid);
        if (p != NULL)
            it->setIcon(QIcon(p->iconResource()));
    }

    // Restore selection (setCurrentRow fires currentItemChanged after unblock)
    if (selPid != QLCPalette::invalidId())
    {
        for (int i = 0; i < m_lookList->count(); ++i)
        {
            if (m_lookList->item(i)->data(Qt::UserRole).toUInt() == selPid)
            {
                m_lookList->setCurrentRow(i);
                break;
            }
        }
    }
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
                changed = true;
                palettesAdded = true;
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

                // Replace any matching static (fixed) fixtures: the dynamic
                // group now covers them, and leaving them as fixed targets
                // would override the group's looks with their baked values.
                foreach (quint32 fid, g->fixtureList())
                {
                    if (m_scene->fixtures().contains(fid) == false)
                        continue;
                    Fixture *f = m_doc->fixture(fid);
                    if (f != NULL)
                        for (quint32 i = 0; i < f->channels(); i++)
                            m_scene->unsetValue(fid, i);
                    m_scene->removeFixture(fid);
                }
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
            }
        }
    }

    // When a look was applied, let the palettes take over their channels.
    // Deferred so the modal prompt doesn't spin a nested event loop inside the
    // drop event (before it's been accepted / the drag source cleaned up).
    if (palettesAdded)
        QTimer::singleShot(0, this, [this]() { reconcileAfterPaletteApply(); });

    if (changed)
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
        foreach (const SceneValue &scv, p->valuesFromFixtureGroups(m_doc, m_scene->fixtureGroups()))
            covered.insert((quint64(scv.fxi) << 32) | scv.channel);
        foreach (const SceneValue &scv, p->valuesFromFixtures(m_doc, m_scene->fixtures()))
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
    const QList<QListWidgetItem*> sel = m_lookList->selectedItems();
    emit lookSelected(sel.isEmpty() ? QLCPalette::invalidId()
                                    : sel.first()->data(Qt::UserRole).toUInt());
}

void SceneGroupLooks::slotLookDoubleClicked(QListWidgetItem *item)
{
    if (item == NULL)
        return;
    // Force-load this look into the editor (switches the bottom panel away
    // from the per-fixture channel editor).
    emit lookSelected(item->data(Qt::UserRole).toUInt());
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
    const QList<QListWidgetItem*> sel = m_lookList->selectedItems();
    if (sel.isEmpty())
        return;
    // Detach from the scene only; leave the palette in the Doc so any
    // other scene referencing it keeps working.
    foreach (QListWidgetItem *it, sel)
        m_scene->removePalette(it->data(Qt::UserRole).toUInt());
    m_doc->setModified();
    reload();
    emit sceneModified();
}
