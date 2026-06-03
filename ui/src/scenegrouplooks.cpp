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
#include <QInputDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDataStream>

#include <algorithm>

#include "scenegrouplooks.h"
#include "functionstreewidget.h"
#include "paletteeditdialog.h"
#include "groupselection.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "scene.h"
#include "doc.h"

SceneGroupLooks::SceneGroupLooks(Scene *scene, Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_scene(scene)
    , m_doc(doc)
{
    setAcceptDrops(true);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 6, 0, 0);

    QLabel *header = new QLabel(
        tr("<b>Dynamic group looks</b> — palettes applied to fixture "
           "groups, following membership at run time. Every look applies "
           "to every target group; use separate scenes for different "
           "looks per group. Tip: drag palettes here from the Functions "
           "tree to add them as looks."), this);
    header->setWordWrap(true);
    root->addWidget(header);

    QHBoxLayout *cols = new QHBoxLayout();
    root->addLayout(cols);

    // --- Target groups column ---
    // Read-only summary of the scene's target groups; edited via a modal
    // multi-select dialog (Select groups…).
    QVBoxLayout *groupCol = new QVBoxLayout();
    groupCol->addWidget(new QLabel(tr("Target groups"), this));
    m_groupList = new QListWidget(this);
    m_groupList->setSelectionMode(QAbstractItemView::NoSelection);
    m_groupList->setFocusPolicy(Qt::NoFocus);
    groupCol->addWidget(m_groupList);
    m_selectGroupsButton = new QPushButton(tr("Select groups…"), this);
    groupCol->addWidget(m_selectGroupsButton);
    cols->addLayout(groupCol);

    // --- Looks column ---
    QVBoxLayout *lookCol = new QVBoxLayout();
    lookCol->addWidget(new QLabel(tr("Looks"), this));
    m_lookList = new QListWidget(this);
    m_lookList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    lookCol->addWidget(m_lookList);
    QHBoxLayout *lookBtns = new QHBoxLayout();
    m_addLookButton = new QPushButton(tr("Add look…"), this);
    m_removeLookButton = new QPushButton(tr("Remove"), this);
    lookBtns->addWidget(m_addLookButton);
    lookBtns->addWidget(m_removeLookButton);
    lookBtns->addStretch();
    lookCol->addLayout(lookBtns);
    cols->addLayout(lookCol);

    connect(m_selectGroupsButton, SIGNAL(clicked()),
            this, SLOT(slotSelectGroups()));
    connect(m_addLookButton, SIGNAL(clicked()), this, SLOT(slotAddLook()));
    connect(m_removeLookButton, SIGNAL(clicked()), this, SLOT(slotRemoveLook()));

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
    // List every fixture group with a checkbox; checked == targeted by
    // this scene. Block signals so repopulating doesn't fire toggles.
    // Summary of the scene's target groups, sorted by name. Editing is
    // done through the Select groups… dialog (slotSelectGroups).
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

    m_groupList->clear();
    if (groups.isEmpty())
    {
        QListWidgetItem *it = new QListWidgetItem(
            tr("(none — looks apply to the scene's own fixtures)"),
            m_groupList);
        it->setFlags(Qt::NoItemFlags);
    }
    else
    {
        foreach (FixtureGroup *g, groups)
            new QListWidgetItem(g->name(), m_groupList);
    }

    m_lookList->clear();
    foreach (quint32 pid, m_scene->palettes())
    {
        QListWidgetItem *it = new QListWidgetItem(lookLabel(pid), m_lookList);
        it->setData(Qt::UserRole, pid);
    }
}

void SceneGroupLooks::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(FunctionsTreeWidget::paletteDragMimeType()))
        event->acceptProposedAction();
}

void SceneGroupLooks::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat(FunctionsTreeWidget::paletteDragMimeType()))
        event->acceptProposedAction();
}

void SceneGroupLooks::dropEvent(QDropEvent *event)
{
    const QMimeData *mime = event->mimeData();
    if (mime->hasFormat(FunctionsTreeWidget::paletteDragMimeType()) == false)
        return;

    QByteArray data = mime->data(FunctionsTreeWidget::paletteDragMimeType());
    QDataStream stream(&data, QIODevice::ReadOnly);

    bool changed = false;
    while (stream.atEnd() == false)
    {
        quint32 pid = 0;
        stream >> pid;
        // Only attach palettes that exist and aren't already a look.
        if (m_doc->palette(pid) != NULL && m_scene->palettes().contains(pid) == false)
        {
            m_scene->addPalette(pid);
            changed = true;
        }
    }

    if (changed)
    {
        m_doc->setModified();
        reload();
    }
    event->acceptProposedAction();
}

void SceneGroupLooks::slotSelectGroups()
{
    GroupSelection dlg(m_doc, m_scene->fixtureGroups(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QList<quint32> chosen = dlg.selection();
    const QList<quint32> current = m_scene->fixtureGroups();

    // Apply the diff: drop unchecked, add newly checked.
    foreach (quint32 gid, current)
        if (chosen.contains(gid) == false)
            m_scene->removeFixtureGroup(gid);
    foreach (quint32 gid, chosen)
        if (current.contains(gid) == false)
            m_scene->addFixtureGroup(gid);

    m_doc->setModified();
    reload();
}

void SceneGroupLooks::slotAddLook()
{
    // Attach an existing managed palette, or create a new one. Build a
    // pick-list of palettes not already on the scene, plus a leading
    // "New palette…" entry.
    const QList<quint32> already = m_scene->palettes();
    QList<quint32> attachable;
    QStringList choices;
    choices << tr("New palette…");
    foreach (QLCPalette *p, m_doc->palettes())
    {
        if (p == NULL || already.contains(p->id()))
            continue;
        attachable << p->id();
        choices << QString("%1  (%2)")
                       .arg(p->name())
                       .arg(QLCPalette::typeToString(p->type()));
    }

    bool ok = false;
    const QString picked = QInputDialog::getItem(
        this, tr("Add look"),
        tr("Attach a palette (or create a new one):"),
        choices, 0, false, &ok);
    if (!ok || picked.isEmpty())
        return;

    const int idx = choices.indexOf(picked);
    if (idx <= 0)
    {
        // New palette → author it, add to the Doc, then attach.
        PaletteEditDialog dlg(this);
        if (dlg.exec() != QDialog::Accepted || dlg.result() == NULL)
            return;
        QLCPalette *p = dlg.result();
        if (!m_doc->addPalette(p))
        {
            delete p;
            return;
        }
        m_scene->addPalette(p->id());
    }
    else
    {
        // Attach the chosen existing palette (idx-1 into attachable).
        m_scene->addPalette(attachable.at(idx - 1));
    }
    m_doc->setModified();
    reload();
}

void SceneGroupLooks::slotRemoveLook()
{
    const QList<QListWidgetItem*> sel = m_lookList->selectedItems();
    if (sel.isEmpty())
        return;
    // Detach from the scene only; leave the palette in the Doc so any
    // other scene referencing it keeps working (orphans are harmless).
    foreach (QListWidgetItem *it, sel)
        m_scene->removePalette(it->data(Qt::UserRole).toUInt());
    m_doc->setModified();
    reload();
}
