/*
  Q Light Controller Plus
  scenegrouplooks.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDataStream>

#include <algorithm>

#include "scenegrouplooks.h"
#include "functionstreewidget.h"   // palette MIME type
#include "fixturegroupsource.h"    // fixture-group / fixture MIME types
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

// Target kinds, stored at Qt::UserRole + 1 on target items.
enum { TargetGroup = 0, TargetFixture = 1 };
#define TARGET_KIND_ROLE (Qt::UserRole + 1)

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
    targetCol->addWidget(new QLabel(tr("Targets"), this));
    m_targetList = new QListWidget(this);
    m_targetList->setSelectionMode(QAbstractItemView::ExtendedSelection);
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

    if (!p->name().isEmpty())
        return p->name();

    const QString type = QLCPalette::typeToString(p->type());
    switch (p->type())
    {
    case QLCPalette::Color:
        return QString("%1 %2").arg(type).arg(p->rgbValue().name());
    case QLCPalette::PanTilt:
        return QString("%1 P%2 / T%3")
            .arg(type).arg(p->intValue1()).arg(p->intValue2());
    default:
        return QString("%1 %2").arg(type).arg(p->intValue1());
    }
}

void SceneGroupLooks::reload()
{
    m_targetList->clear();

    // Groups = dynamic targets.
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
        QListWidgetItem *it = new QListWidgetItem(
            tr("%1  — group (dynamic)").arg(g->name()), m_targetList);
        it->setData(Qt::UserRole, g->id());
        it->setData(TARGET_KIND_ROLE, TargetGroup);
    }

    // Fixtures = fixed targets (only when this host shows them).
    if (m_includeFixtureTargets)
    {
        QList<Fixture*> fixtures;
        foreach (quint32 fid, m_scene->fixtures())
        {
            Fixture *f = m_doc->fixture(fid);
            if (f != NULL)
                fixtures.append(f);
        }
        std::sort(fixtures.begin(), fixtures.end(),
                  [](Fixture *a, Fixture *b) {
                      return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
                  });
        foreach (Fixture *f, fixtures)
        {
            QListWidgetItem *it = new QListWidgetItem(
                tr("%1  — fixture (fixed)").arg(f->name()), m_targetList);
            it->setData(Qt::UserRole, f->id());
            it->setData(TARGET_KIND_ROLE, TargetFixture);
        }
    }

    if (m_targetList->count() == 0)
    {
        QListWidgetItem *it = new QListWidgetItem(
            tr("(no targets — drag groups or fixtures here)"), m_targetList);
        it->setFlags(Qt::NoItemFlags);
    }

    // Looks = palettes.
    m_lookList->clear();
    foreach (quint32 pid, m_scene->palettes())
    {
        QListWidgetItem *it = new QListWidgetItem(lookLabel(pid), m_lookList);
        it->setData(Qt::UserRole, pid);
    }
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
            if (m_doc->fixtureGroup(gid) != NULL
                && m_scene->fixtureGroups().contains(gid) == false)
            {
                m_scene->addFixtureGroup(gid);
                changed = true;
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

    if (changed)
    {
        m_doc->setModified();
        reload();
        emit sceneModified();
    }
    event->acceptProposedAction();
}

void SceneGroupLooks::slotRemoveTarget()
{
    const QList<QListWidgetItem*> sel = m_targetList->selectedItems();
    if (sel.isEmpty())
        return;
    bool changed = false;
    foreach (QListWidgetItem *it, sel)
    {
        const quint32 id = it->data(Qt::UserRole).toUInt();
        if (it->data(TARGET_KIND_ROLE).toInt() == TargetFixture)
            changed = m_scene->removeFixture(id) || changed;
        else
            changed = m_scene->removeFixtureGroup(id) || changed;
    }
    if (changed)
    {
        m_doc->setModified();
        reload();
        emit sceneModified();
    }
}

void SceneGroupLooks::slotLookSelectionChanged()
{
    const QList<QListWidgetItem*> sel = m_lookList->selectedItems();
    emit lookSelected(sel.isEmpty() ? QLCPalette::invalidId()
                                    : sel.first()->data(Qt::UserRole).toUInt());
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
