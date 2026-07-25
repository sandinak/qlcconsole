/*
  Q Light Controller Plus
  monitorlayerspanel.cpp

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

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QToolButton>
#include <QToolBar>
#include <QLabel>
#include <QMenu>
#include <QStyle>
#include <QKeyEvent>
#include <QDropEvent>
#include <QScrollBar>
#include <QSet>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>

// Draw a small rounded colour swatch icon (used for platforms).
static QIcon swatchIcon(const QColor &c)
{
    QPixmap pm(14, 14);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(c.isValid() ? c : QColor(120, 120, 120));
    p.setPen(QPen(QColor(0, 0, 0, 140), 1));
    p.drawRoundedRect(1, 1, 11, 11, 3, 3);
    return QIcon(pm);
}

// Draw a glyph icon in a colour (used for power sources).
static QIcon glyphIcon(const QString &glyph, const QColor &c)
{
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(c);
    QFont f = p.font();
    f.setPointSizeF(11.0);
    p.setFont(f);
    p.drawText(pm.rect(), Qt::AlignCenter, glyph);
    return QIcon(pm);
}

#include "monitorlayerspanel.h"
#include "monitorgraphicsview.h"
#include "monitorproperties.h"
#include "powerdistribution.h"
#include "stageplatform.h"
#include "stagetarget.h"
#include "truss.h"
#include "fixture.h"
#include "doc.h"

// Row-widget glyphs. Emoji render fine on the macOS Qt5 build this fork targets.
static const char *kEyeShown  = "\xF0\x9F\x91\x81";   // 👁
static const char *kEyeHidden = "\xE2\x80\x94";       // — (em dash)
static const char *kLockOn    = "\xF0\x9F\x94\x92";   // 🔒
static const char *kLockOff   = "\xF0\x9F\x94\x93";   // 🔓

// Tree-item data roles.
static const int NodeTypeRole = Qt::UserRole;       // MonitorLayersPanel::NodeType
static const int NodeIdRole   = Qt::UserRole + 1;   // layer / group / item id
static const int NodeKindRole = Qt::UserRole + 2;   // item kind (QString)

MonitorLayersPanel::MonitorLayersPanel(Doc *doc, MonitorGraphicsView *view, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_view(view)
{
    m_props = m_doc->monitorProperties();

    // Floor width so the splitter can never render the panel at zero px.
    setMinimumWidth(200);

    QVBoxLayout *vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(4);

    // Title row with an in-panel hide button.
    QHBoxLayout *titleRow = new QHBoxLayout();
    QLabel *title = new QLabel(tr("Layers"), this);
    QFont tf = title->font();
    tf.setBold(true);
    title->setFont(tf);
    titleRow->addWidget(title, 1);
    m_hideBtn = new QToolButton(this);
    m_hideBtn->setText("\xC3\x97"); // ×
    m_hideBtn->setAutoRaise(true);
    m_hideBtn->setToolTip(tr("Hide the Layers panel"));
    titleRow->addWidget(m_hideBtn);
    vbox->addLayout(titleRow);

    QToolBar *tb = new QToolBar(this);
    tb->setIconSize(QSize(16, 16));
    m_addBtn = new QToolButton(this);
    m_addBtn->setText("+");
    m_addBtn->setToolTip(tr("Add a new layer"));
    tb->addWidget(m_addBtn);

    m_removeBtn = new QToolButton(this);
    m_removeBtn->setText("\xE2\x80\x93"); // – en dash
    m_removeBtn->setToolTip(tr("Delete the selected layer (its items move to Default)"));
    tb->addWidget(m_removeBtn);

    tb->addSeparator();

    m_upBtn = new QToolButton(this);
    m_upBtn->setArrowType(Qt::UpArrow);
    m_upBtn->setToolTip(tr("Move layer up"));
    tb->addWidget(m_upBtn);

    m_downBtn = new QToolButton(this);
    m_downBtn->setArrowType(Qt::DownArrow);
    m_downBtn->setToolTip(tr("Move layer down"));
    tb->addWidget(m_downBtn);

    tb->addSeparator();

    m_moveHereBtn = new QToolButton(this);
    m_moveHereBtn->setText(tr("Move selection here"));
    m_moveHereBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_moveHereBtn->setToolTip(tr("Assign the canvas selection to the selected layer"));
    tb->addWidget(m_moveHereBtn);

    vbox->addWidget(tb);

    m_tree = new LayersTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(2);
    // Name column fills; the eye/lock column hugs its buttons. Object (group /
    // item) rows span the first column across BOTH columns (setFirstColumnSpanned
    // in reload) so their names get the full width — only LAYER rows reserve the
    // eye/lock column. Long/nested names scroll horizontally instead of eliding.
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->setTextElideMode(Qt::ElideNone);
    m_tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setDropIndicatorShown(true);
    m_tree->installEventFilter(this);   // Enter-to-rename only
    connect(m_tree, &LayersTreeWidget::itemsDropped,
            this, &MonitorLayersPanel::handleTreeDrop);
    // Suppress rebuilds during the drag; rebuild once, safely, after it ends.
    connect(m_tree, &LayersTreeWidget::dragStarted, this, [this]() { m_suppressReload = true; });
    connect(m_tree, &LayersTreeWidget::dragFinished, this, [this]() {
        m_suppressReload = false;
        reload();
    });
    vbox->addWidget(m_tree, 1);

    QLabel *hint = new QLabel(tr("Enter/F2 renames a layer or group. Multi-select "
                                 "items, then right-click to group or move."), this);
    hint->setWordWrap(true);
    QFont hf = hint->font();
    hf.setPointSizeF(hf.pointSizeF() - 1.0);
    hint->setFont(hf);
    hint->setEnabled(false);
    vbox->addWidget(hint);

    connect(m_hideBtn,     &QToolButton::clicked, this, &MonitorLayersPanel::closeRequested);
    connect(m_addBtn,      &QToolButton::clicked, this, &MonitorLayersPanel::slotAddLayer);
    connect(m_removeBtn,   &QToolButton::clicked, this, &MonitorLayersPanel::slotRemoveLayer);
    connect(m_upBtn,       &QToolButton::clicked, this, &MonitorLayersPanel::slotMoveUp);
    connect(m_downBtn,     &QToolButton::clicked, this, &MonitorLayersPanel::slotMoveDown);
    connect(m_moveHereBtn, &QToolButton::clicked, this, &MonitorLayersPanel::slotMoveSelectionHere);
    connect(m_tree, &QTreeWidget::itemClicked, this, &MonitorLayersPanel::slotItemClicked);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &MonitorLayersPanel::slotItemDoubleClicked);
    connect(m_tree, &QTreeWidget::itemChanged, this, &MonitorLayersPanel::slotItemChanged);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &MonitorLayersPanel::updateButtons);
    connect(m_tree, &QWidget::customContextMenuRequested, this, &MonitorLayersPanel::slotContextMenu);

    if (m_doc != nullptr)
        connect(m_doc, &Doc::modeChanged, this, &MonitorLayersPanel::updateEditableState);

    updateEditableState();   // sets m_editable and calls reload()
}

void MonitorLayersPanel::updateEditableState()
{
    m_editable = (m_doc == nullptr) || (m_doc->mode() == Doc::Design);

    m_addBtn->setEnabled(m_editable);
    m_removeBtn->setEnabled(m_editable);
    m_upBtn->setEnabled(m_editable);
    m_downBtn->setEnabled(m_editable);
    m_moveHereBtn->setEnabled(m_editable);
    // F2 starts inline rename; Enter is handled in eventFilter. Double-click is
    // NOT an edit trigger — it opens item editors (see slotItemDoubleClicked).
    m_tree->setEditTriggers(m_editable ? QAbstractItemView::EditKeyPressed
                                       : QAbstractItemView::NoEditTriggers);
    // Drag-to-reparent: enabled only while editing. We intercept the drop
    // ourselves (viewport event filter) rather than let the view move rows.
    m_tree->setDragEnabled(m_editable);
    m_tree->viewport()->setAcceptDrops(m_editable);
    m_tree->setDragDropMode(m_editable ? QAbstractItemView::InternalMove
                                       : QAbstractItemView::NoDragDrop);
    // Default the drag to a COPY at the source, so Qt's startDrag never runs its
    // own "remove the dragged rows" cleanup (which use-after-frees the tree we
    // rebuild ourselves). We do all model changes + the reload.
    m_tree->setDefaultDropAction(Qt::CopyAction);
    reload();   // re-apply per-item editable flags and eye/lock enabled state
}

QList<MonitorLayersPanel::ItemDesc> MonitorLayersPanel::gatherItems() const
{
    QList<ItemDesc> out;
    if (m_props == nullptr)
        return out;

    foreach (quint32 fid, m_props->fixtureItemsID())
    {
        Fixture *f = m_doc->fixture(fid);
        ItemDesc d;
        d.kind    = QStringLiteral("fixture");
        d.id      = fid;
        d.name    = (f != nullptr) ? f->name() : tr("Fixture %1").arg(fid);
        d.layerId = m_props->fixtureLayer(fid);
        d.groupId = m_props->fixtureGroup(fid);
        out << d;
    }
    foreach (Truss *t, m_props->trusses())
    {
        ItemDesc d;
        d.kind    = QStringLiteral("truss");
        d.id      = t->id();
        d.name    = t->name().isEmpty() ? tr("Truss %1").arg(t->id()) : t->name();
        d.layerId = t->layerId();
        d.groupId = t->groupId();
        out << d;
    }
    foreach (StagePlatform *p, m_props->platforms())
    {
        ItemDesc d;
        d.kind    = QStringLiteral("platform");
        d.id      = p->id();
        d.name    = p->name().isEmpty() ? tr("Platform %1").arg(p->id()) : p->name();
        d.layerId = p->layerId();
        d.groupId = p->groupId();
        d.color   = p->color();
        out << d;
    }
    // Stage targets are intentionally omitted: they're dynamic aim points that
    // move across levels during a show, so organising them into fixed layers /
    // groups doesn't fit. They live on the canvas (for the active scene) only.
    PowerDistribution *pd = m_doc->powerDistribution();
    for (int s = 0; s < pd->sources().size(); s++)
    {
        const PowerSource &src = pd->sources().at(s);
        ItemDesc d;
        d.kind    = QStringLiteral("power");
        d.id      = quint32(s);
        d.name    = src.name.isEmpty() ? tr("Power %1").arg(s) : src.name;
        d.layerId = src.layerId;
        d.groupId = src.groupId;
        out << d;
    }
    foreach (const MonitorProperties::MonitorImage &img, m_props->images())
    {
        ItemDesc d;
        d.kind    = QStringLiteral("image");
        d.id      = img.id;
        d.name    = img.name.isEmpty() ? tr("Image %1").arg(img.id) : img.name;
        d.layerId = img.layerId;
        d.groupId = img.groupId;
        out << d;
    }
    return out;
}

void MonitorLayersPanel::addItemLeaf(QTreeWidgetItem *parent, const ItemDesc &d)
{
    QTreeWidgetItem *node = new QTreeWidgetItem(parent);
    node->setText(0, d.name);
    node->setToolTip(0, d.name);
    node->setFirstColumnSpanned(true);   // full-width name (no eye/lock column)
    node->setData(0, NodeTypeRole, int(NodeItem));
    node->setData(0, NodeIdRole, d.id);
    node->setData(0, NodeKindRole, d.kind);
    // Item leaves are renamable inline (Enter/F2/double-click) — the rename is
    // applied to the underlying map item in slotItemChanged.
    if (m_editable)
        node->setFlags(node->flags() | Qt::ItemIsEditable);
    else
        node->setFlags(node->flags() & ~Qt::ItemIsEditable);
    if (d.kind == QStringLiteral("fixture"))
        node->setIcon(0, QIcon(":/fixture.png"));
    else if (d.kind == QStringLiteral("truss"))
        node->setIcon(0, QIcon(":/group.png"));
    else if (d.kind == QStringLiteral("platform"))
        node->setIcon(0, swatchIcon(d.color));                 // colour-coded riser
    else if (d.kind == QStringLiteral("power"))
        node->setIcon(0, glyphIcon(QStringLiteral("\xE2\x9A\xA1"), QColor(235, 185, 0))); // ⚡
    else if (d.kind == QStringLiteral("image"))
        node->setIcon(0, QIcon(":/image.png"));
    else
        node->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
}

void MonitorLayersPanel::buildGroupNode(QTreeWidgetItem *parent, quint32 groupId,
                                        const QList<ItemDesc> &items)
{
    const MonitorProperties::MonitorGroup g = m_props->group(groupId);

    // A group EXPLICITLY anchored (by ensureTruss/PlatformGroup) to a truss or
    // riser is presented AS that item — its name + icon, fixtures nested under
    // it. Manually-created groups have no anchor and render as a plain folder
    // (so renaming a platform never lets it "take over" a manual group).
    const QString anchorKind = g.anchorKind;
    const quint32 anchorId   = g.anchorId;
    QString anchorName;
    QColor  anchorColor;
    if (!anchorKind.isEmpty())
        foreach (const ItemDesc &d, items)
            if (d.groupId == groupId && d.kind == anchorKind && d.id == anchorId)
            { anchorName = d.name; anchorColor = d.color; break; }

    // Anchor only applies if the anchor item is actually present in the group.
    const bool anchored = !anchorKind.isEmpty() && !anchorName.isEmpty();

    QTreeWidgetItem *node = new QTreeWidgetItem(parent);
    if (anchored && anchorKind == QStringLiteral("truss"))
    {
        node->setText(0, anchorName);
        node->setIcon(0, QIcon(":/group.png"));         // truss glyph
    }
    else if (anchored && anchorKind == QStringLiteral("platform"))
    {
        node->setText(0, anchorName);
        node->setIcon(0, swatchIcon(anchorColor));      // riser colour swatch
    }
    else
    {
        node->setText(0, g.name);
        node->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    }
    node->setToolTip(0, node->text(0));
    node->setFirstColumnSpanned(true);   // full-width name (no eye/lock column)
    node->setData(0, NodeTypeRole, int(NodeGroup));
    node->setData(0, NodeIdRole, groupId);
    if (m_editable)
        node->setFlags(node->flags() | Qt::ItemIsEditable);
    else
        node->setFlags(node->flags() & ~Qt::ItemIsEditable);

    foreach (const MonitorProperties::MonitorGroup &c, m_props->childGroups(groupId))
        buildGroupNode(node, c.id, items);
    foreach (const ItemDesc &d, items)
    {
        if (d.groupId != groupId)
            continue;
        if (d.kind == anchorKind && d.id == anchorId)
            continue;   // the node itself represents this anchor item
        addItemLeaf(node, d);
    }

    node->setExpanded(true);
}

void MonitorLayersPanel::reload()
{
    if (m_props == nullptr)
        return;

    // Never rebuild the tree while a drag is in flight — Qt's nested drag loop
    // still references the rows we'd delete. The dragFinished handler reloads
    // once the drag has fully unwound.
    if (m_suppressReload)
        return;

    // Preserve the scroll position: without this, setCurrentItem(active) below
    // scrolls the tree back to the active layer after every reload — so moving
    // an item to a lower layer made that layer scroll out of view ("disappear").
    const int scroll = m_tree->verticalScrollBar()->value();

    m_reloading = true;
    m_tree->clear();

    const QList<ItemDesc> items = gatherItems();
    const quint32 activeId = m_props->activeLayerId();
    QTreeWidgetItem *activeNode = nullptr;

    foreach (const MonitorProperties::MonitorLayer &lyr, m_props->layers())
    {
        QTreeWidgetItem *layerNode = new QTreeWidgetItem(m_tree);
        layerNode->setText(0, lyr.name);
        layerNode->setData(0, NodeTypeRole, int(NodeLayer));
        layerNode->setData(0, NodeIdRole, lyr.id);
        layerNode->setSizeHint(0, QSize(0, 24));
        if (m_editable)
            layerNode->setFlags(layerNode->flags() | Qt::ItemIsEditable);
        else
            layerNode->setFlags(layerNode->flags() & ~Qt::ItemIsEditable);
        if (lyr.id == activeId)
        {
            QFont bf = layerNode->font(0);
            bf.setBold(true);
            layerNode->setFont(0, bf);
        }

        // Column 1: compact eye + lock toggles.
        QWidget *row = new QWidget(m_tree);
        QHBoxLayout *h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(0);

        QToolButton *eye = new QToolButton(row);
        eye->setCheckable(true);
        eye->setChecked(lyr.visible);
        eye->setText(lyr.visible ? kEyeShown : kEyeHidden);
        eye->setToolTip(tr("Show / hide this layer"));
        eye->setAutoRaise(true);
        eye->setEnabled(m_editable);
        eye->setFixedSize(18, 18);
        h->addWidget(eye);

        QToolButton *lock = new QToolButton(row);
        lock->setCheckable(true);
        lock->setChecked(lyr.locked);
        lock->setText(lyr.locked ? kLockOn : kLockOff);
        lock->setToolTip(tr("Lock / unlock this layer"));
        lock->setAutoRaise(true);
        lock->setEnabled(m_editable);
        lock->setFixedSize(18, 18);
        h->addWidget(lock);
        row->setLayout(h);
        m_tree->setItemWidget(layerNode, 1, row);

        const quint32 lid = lyr.id;
        connect(eye, &QToolButton::toggled, this, [this, lid](bool on) {
            if (m_reloading) return;
            toggleVisible(lid, on);
        });
        connect(lock, &QToolButton::toggled, this, [this, lid](bool on) {
            if (m_reloading) return;
            toggleLocked(lid, on);
        });

        foreach (const MonitorProperties::MonitorGroup &grp, m_props->groups())
            if (grp.parentGroupId == 0 && grp.layerId == lyr.id)
                buildGroupNode(layerNode, grp.id, items);
        foreach (const ItemDesc &d, items)
            if (d.groupId == 0 && d.layerId == lyr.id)
                addItemLeaf(layerNode, d);

        layerNode->setExpanded(true);
        if (lyr.id == activeId)
            activeNode = layerNode;
    }

    if (activeNode != nullptr)
        m_tree->setCurrentItem(activeNode);

    // Restore the scroll position (overriding the scrollTo that setCurrentItem
    // just did), so a reload doesn't jump the view around.
    m_tree->verticalScrollBar()->setValue(scroll);

    m_reloading = false;

    // A drop asked us to keep its destination layer visible — do it now that the
    // tree is rebuilt (this deliberately overrides the scroll restore above).
    if (m_focusLayerAfterReload >= 0)
    {
        selectLayerNode(quint32(m_focusLayerAfterReload));
        m_focusLayerAfterReload = -1;
    }

    updateButtons();
}

bool MonitorLayersPanel::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_tree && event->type() == QEvent::KeyPress)
    {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)
        {
            beginRenameCurrent();
            return true;
        }
    }
    // Drops are handled by LayersTreeWidget::dropEvent → itemsDropped → handleTreeDrop.
    return QWidget::eventFilter(obj, event);
}

void MonitorLayersPanel::handleTreeDrop(const QList<QTreeWidgetItem *> &dragged,
                                        QTreeWidgetItem *target)
{
    if (!m_editable || dragged.isEmpty() || m_view == nullptr)
        return;

    // Dragging a LAYER reorders it: drop it at the target's layer position.
    if (dragged.first()->data(0, NodeTypeRole).toInt() == NodeLayer)
    {
        const quint32 movedId = dragged.first()->data(0, NodeIdRole).toUInt();
        // Which layer did we drop onto (walk an item/group up to its layer)?
        QTreeWidgetItem *up = target;
        while (up != nullptr && up->data(0, NodeTypeRole).toInt() != NodeLayer)
            up = up->parent();
        const QList<MonitorProperties::MonitorLayer> ls = m_props->layers();
        int from = -1, to = -1;
        for (int i = 0; i < ls.size(); i++)
        {
            if (ls.at(i).id == movedId) from = i;
            if (up != nullptr && ls.at(i).id == up->data(0, NodeIdRole).toUInt()) to = i;
        }
        if (from >= 0 && to >= 0 && from != to)
            reorderLayer(from, to - from);
        return;
    }

    // Resolve the drop CONTAINER (a group, or a bare layer).
    bool intoGroup = false;
    quint32 containerId = 0;
    quint32 containerLayer = m_props->activeLayerId();
    if (target != nullptr)
    {
        const int t = target->data(0, NodeTypeRole).toInt();
        if (t == NodeGroup)
        {
            intoGroup = true;
            containerId = target->data(0, NodeIdRole).toUInt();
            containerLayer = m_props->group(containerId).layerId;
        }
        else if (t == NodeLayer)
        {
            containerLayer = target->data(0, NodeIdRole).toUInt();
        }
        else // NodeItem — drop into its container
        {
            QTreeWidgetItem *p = target->parent();
            if (p != nullptr && p->data(0, NodeTypeRole).toInt() == NodeGroup)
            {
                intoGroup = true;
                containerId = p->data(0, NodeIdRole).toUInt();
                containerLayer = m_props->group(containerId).layerId;
            }
            else
            {
                QTreeWidgetItem *up = target;
                while (up != nullptr && up->data(0, NodeTypeRole).toInt() != NodeLayer)
                    up = up->parent();
                if (up != nullptr)
                    containerLayer = up->data(0, NodeIdRole).toUInt();
            }
        }
    }

    // Split the drag into item leaves and group nodes.
    QList<QPair<QString, quint32> > leaves;
    QList<quint32> groups;
    foreach (QTreeWidgetItem *d, dragged)
    {
        const int type = d->data(0, NodeTypeRole).toInt();
        if (type == NodeItem)
            leaves << qMakePair(d->data(0, NodeKindRole).toString(), d->data(0, NodeIdRole).toUInt());
        else if (type == NodeGroup)
            groups << d->data(0, NodeIdRole).toUInt();
        // dragged layer nodes are ignored (use the up/down buttons to reorder)
    }

    if (!leaves.isEmpty())
    {
        // Dropping a fixture onto a TRUSS-anchored group binds it to that truss
        // (explicit attach, since 2D drop-to-bind was removed). Non-fixture
        // leaves just reparent into the group as usual.
        const MonitorProperties::MonitorGroup g =
            intoGroup ? m_props->group(containerId) : MonitorProperties::MonitorGroup();
        const bool trussAnchored =
            intoGroup && g.anchorKind == QStringLiteral("truss") && g.anchorId != 0;

        if (trussAnchored)
        {
            QList<QPair<QString, quint32> > nonFixtures;
            for (const QPair<QString, quint32> &lf : leaves)
            {
                if (lf.first == QStringLiteral("fixture"))
                    m_view->attachFixtureToTruss(lf.second, g.anchorId);
                else
                    nonFixtures << lf;
            }
            if (!nonFixtures.isEmpty())
                m_view->reparentToGroup(nonFixtures, containerId);
        }
        else if (intoGroup)
        {
            m_view->reparentToGroup(leaves, containerId);
        }
        else
        {
            m_view->reparentToLayer(leaves, containerLayer);
        }
    }
    foreach (quint32 g, groups)
    {
        if (intoGroup) m_view->reparentGroupToGroup(g, containerId);
        else           m_view->reparentGroupToLayer(g, containerLayer);
    }

    // Keep the destination layer in view once the (queued) reload runs.
    m_focusLayerAfterReload = int(containerLayer);

    // NOTE: no synchronous reload() here — that would delete the tree rows Qt's
    // drag machinery is still using (use-after-free / crash). The reparent calls
    // emit mapStructureChanged, which is wired as a QUEUED reload that runs once
    // this drop event has fully unwound.
}

void MonitorLayersPanel::beginRenameCurrent()
{
    if (!m_editable)
        return;
    QTreeWidgetItem *it = m_tree->currentItem();
    if (it == nullptr)
        return;
    // Layers, groups and item leaves are all renamable inline.
    m_tree->editItem(it, 0);
}

void MonitorLayersPanel::slotItemDoubleClicked(QTreeWidgetItem *item, int)
{
    if (item == nullptr)
        return;
    const int type = item->data(0, NodeTypeRole).toInt();
    if (type == NodeItem)
    {
        // Open the item's editor on the canvas.
        if (m_view)
            m_view->requestEditItem(item->data(0, NodeKindRole).toString(),
                                    item->data(0, NodeIdRole).toUInt());
    }
    else if (m_editable)
    {
        // Layer / group: double-click starts inline rename.
        m_tree->editItem(item, 0);
    }
}

void MonitorLayersPanel::slotItemChanged(QTreeWidgetItem *item, int column)
{
    if (m_reloading || item == nullptr || column != 0)
        return;
    const int type = item->data(0, NodeTypeRole).toInt();
    const quint32 id = item->data(0, NodeIdRole).toUInt();
    const QString name = item->text(0).trimmed();
    if (name.isEmpty())
        return;

    if (type == NodeLayer)
    {
        m_props->setLayerName(id, name);
    }
    else if (type == NodeGroup)
    {
        m_props->setGroupName(id, name);
        // An anchored group shows the anchor item's name — rename it too.
        const MonitorProperties::MonitorGroup g = m_props->group(id);
        if (!g.anchorKind.isEmpty())
            renameMapItem(g.anchorKind, g.anchorId, name);
    }
    else // NodeItem — rename the underlying map item
    {
        renameMapItem(item->data(0, NodeKindRole).toString(), id, name);
    }
    m_doc->setModified();
}

bool MonitorLayersPanel::kindLockable(const QString &kind) const
{
    return kind == QStringLiteral("truss") || kind == QStringLiteral("platform")
        || kind == QStringLiteral("image") || kind == QStringLiteral("power");
}

bool MonitorLayersPanel::objectLocked(const QString &kind, quint32 id) const
{
    if (kind == QStringLiteral("truss"))
        { Truss *t = m_props->truss(id); return t && t->locked(); }
    if (kind == QStringLiteral("platform"))
        { StagePlatform *p = m_props->platform(id); return p && p->locked(); }
    if (kind == QStringLiteral("image"))
        return m_props->image(id).locked;
    if (kind == QStringLiteral("power"))
    {
        PowerDistribution *pd = m_doc->powerDistribution();
        return int(id) < pd->sources().size() && pd->sources().at(int(id)).locked;
    }
    return false;
}

void MonitorLayersPanel::setObjectLocked(const QString &kind, quint32 id, bool locked)
{
    if (kind == QStringLiteral("truss"))
        { if (Truss *t = m_props->truss(id)) t->setLocked(locked); }
    else if (kind == QStringLiteral("platform"))
        { if (StagePlatform *p = m_props->platform(id)) p->setLocked(locked); }
    else if (kind == QStringLiteral("image"))
    {
        MonitorProperties::MonitorImage img = m_props->image(id);
        img.locked = locked;
        m_props->setImage(img);
    }
    else if (kind == QStringLiteral("power"))
    {
        PowerDistribution *pd = m_doc->powerDistribution();
        if (int(id) < pd->sources().size())
            pd->sources()[int(id)].locked = locked;
    }
    else
        return;
    m_doc->setModified();
    if (m_view) m_view->refreshItemLayerState();
}

void MonitorLayersPanel::renameMapItem(const QString &kind, quint32 id, const QString &name)
{
    if (kind == QStringLiteral("fixture"))
    {
        if (Fixture *f = m_doc->fixture(id)) f->setName(name);
        if (m_view) m_view->updateFixture(id);
    }
    else if (kind == QStringLiteral("truss"))
    {
        if (Truss *t = m_props->truss(id)) t->setName(name);
        if (m_view) m_view->updateTrusses();
    }
    else if (kind == QStringLiteral("platform"))
    {
        if (StagePlatform *p = m_props->platform(id)) p->setName(name);
        if (m_view) m_view->updatePlatforms();
    }
    else if (kind == QStringLiteral("power"))
    {
        PowerDistribution *pd = m_doc->powerDistribution();
        if (int(id) < pd->sources().size()) pd->sources()[int(id)].name = name;
        if (m_view) m_view->updatePowerSources();
    }
    else if (kind == QStringLiteral("image"))
    {
        if (m_props->hasImage(id))
        {
            MonitorProperties::MonitorImage img = m_props->image(id);
            img.name = name;
            m_props->setImage(img);
        }
        if (m_view) m_view->updateImages();
    }
}

void MonitorLayersPanel::collectLeaves(QTreeWidgetItem *node,
                                       QList<QPair<QString, quint32> > &out) const
{
    for (int i = 0; i < node->childCount(); i++)
    {
        QTreeWidgetItem *c = node->child(i);
        const int type = c->data(0, NodeTypeRole).toInt();
        if (type == NodeItem)
            out << qMakePair(c->data(0, NodeKindRole).toString(), c->data(0, NodeIdRole).toUInt());
        else if (type == NodeGroup)
            collectLeaves(c, out);
    }
}

QList<QPair<QString, quint32> > MonitorLayersPanel::selectedObjects() const
{
    QList<QPair<QString, quint32> > out;
    QSet<QString> seen;
    foreach (QTreeWidgetItem *it, m_tree->selectedItems())
    {
        QList<QPair<QString, quint32> > add;
        const int type = it->data(0, NodeTypeRole).toInt();
        if (type == NodeItem)
            add << qMakePair(it->data(0, NodeKindRole).toString(), it->data(0, NodeIdRole).toUInt());
        else if (type == NodeGroup)
            collectLeaves(it, add);
        // layer nodes contribute nothing on their own
        for (const QPair<QString, quint32> &p : add)
        {
            const QString key = p.first + QLatin1Char(':') + QString::number(p.second);
            if (!seen.contains(key)) { seen.insert(key); out << p; }
        }
    }
    return out;
}

quint32 MonitorLayersPanel::currentLayerId() const
{
    QTreeWidgetItem *it = m_tree->currentItem();
    while (it != nullptr && it->data(0, NodeTypeRole).toInt() != NodeLayer)
        it = it->parent();
    if (it == nullptr)
        return MonitorProperties::defaultLayerId;
    return it->data(0, NodeIdRole).toUInt();
}

bool MonitorLayersPanel::currentIsLayer() const
{
    QTreeWidgetItem *it = m_tree->currentItem();
    return it != nullptr && it->data(0, NodeTypeRole).toInt() == NodeLayer;
}

void MonitorLayersPanel::updateButtons()
{
    const bool isLayer = currentIsLayer();
    const quint32 id = currentLayerId();
    const bool isDefault = (id == MonitorProperties::defaultLayerId);

    m_removeBtn->setEnabled(m_editable && isLayer && !isDefault);
    m_moveHereBtn->setEnabled(m_editable && isLayer && m_view != nullptr && m_view->hasSelection());

    int row = -1, count = 0;
    if (isLayer)
    {
        row = m_tree->indexOfTopLevelItem(m_tree->currentItem());
        count = m_tree->topLevelItemCount();
    }
    m_upBtn->setEnabled(m_editable && row > 0);
    m_downBtn->setEnabled(m_editable && row >= 0 && row < count - 1);
    m_addBtn->setEnabled(m_editable);
}

void MonitorLayersPanel::slotItemClicked(QTreeWidgetItem *item, int)
{
    if (m_reloading || item == nullptr)
        return;

    const int type = item->data(0, NodeTypeRole).toInt();
    const quint32 id = item->data(0, NodeIdRole).toUInt();

    if (type == NodeLayer)
    {
        if (m_props->activeLayerId() != id)
        {
            m_props->setActiveLayerId(id);
            m_doc->setModified();
            reload();
        }
    }
    else if (type == NodeGroup)
    {
        if (m_view != nullptr)
            m_view->selectItemsInGroup(id);
    }
    else // NodeItem
    {
        if (m_view != nullptr)
            m_view->selectMapItem(item->data(0, NodeKindRole).toString(), id);
    }
}

void MonitorLayersPanel::slotContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    QMenu menu(this);

    if (item == nullptr)
    {
        // Blank space: offer to create a new layer.
        if (m_editable)
            menu.addAction(tr("New layer"), this, &MonitorLayersPanel::slotAddLayer);
        if (!menu.isEmpty())
            menu.exec(m_tree->viewport()->mapToGlobal(pos));
        return;
    }

    const int type = item->data(0, NodeTypeRole).toInt();
    const quint32 id = item->data(0, NodeIdRole).toUInt();

    // Selection-based Group / Move to layer (needs editing + real objects).
    const QList<QPair<QString, quint32> > objs = selectedObjects();
    if (m_editable && objs.size() >= 2)
    {
        menu.addAction(tr("Group selected"), this, [this, objs]() {
            m_view->selectMapItems(objs);
            m_view->groupSelectedItems();
        });
    }
    if (m_editable && !objs.isEmpty())
    {
        QMenu *moveMenu = menu.addMenu(tr("Move to layer"));
        foreach (const MonitorProperties::MonitorLayer &lyr, m_props->layers())
        {
            const quint32 lid = lyr.id;
            moveMenu->addAction(lyr.name, this, [this, objs, lid]() {
                m_view->selectMapItems(objs);
                m_view->setSelectedItemsLayer(lid);
                reload();
            });
        }
    }
    if (!menu.isEmpty())
        menu.addSeparator();

    if (type == NodeGroup)
    {
        // An anchored group (truss / platform) is presented AS that object —
        // offer Edit to open its editor directly (it's often covered on the
        // canvas by the fixtures riding on it).
        const MonitorProperties::MonitorGroup g = m_props->group(id);
        if (!g.anchorKind.isEmpty())
        {
            const QString ak = g.anchorKind; const quint32 aid = g.anchorId;
            menu.addAction(tr("Edit…"), this, [this, ak, aid]() {
                if (m_view) m_view->requestEditItem(ak, aid);
            });
        }
        menu.addAction(tr("Select on map"), this, [this, id]() {
            if (m_view) m_view->selectItemsInGroup(id);
        });
        if (m_editable)
        {
            // Position lock. For an anchored group (truss/platform) this locks
            // the ANCHOR object itself — same as the leaf node when the truss has
            // no fixtures, and same as the canvas right-click. For a plain folder
            // it locks the whole group.
            if (!g.anchorKind.isEmpty() && kindLockable(g.anchorKind))
            {
                const QString ak = g.anchorKind; const quint32 aid = g.anchorId;
                const bool lk = objectLocked(ak, aid);
                menu.addAction(lk ? tr("Unlock position") : tr("Lock position"),
                               this, [this, ak, aid, lk]() {
                    setObjectLocked(ak, aid, !lk);
                });
            }
            else
            {
                const bool locked = g.locked;
                menu.addAction(locked ? tr("Unlock position") : tr("Lock position"),
                               this, [this, id, locked]() {
                    m_props->setGroupLocked(id, !locked);
                    m_doc->setModified();
                    if (m_view) m_view->refreshItemLayerState();
                    reload();
                });
            }
            menu.addAction(tr("Rename group…"), this, [this, item]() {
                m_tree->editItem(item, 0);
            });
            menu.addAction(tr("Ungroup"), this, [this, id]() {
                if (m_view) m_view->ungroupById(id);
            });
        }
    }
    else if (type == NodeItem)
    {
        // Any object leaf (fixture / platform / image / power) → Edit + Select.
        const QString kind = item->data(0, NodeKindRole).toString();
        menu.addAction(tr("Edit…"), this, [this, kind, id]() {
            if (m_view) m_view->requestEditItem(kind, id);
        });
        menu.addAction(tr("Select on map"), this, [this, kind, id]() {
            if (m_view) m_view->selectMapItem(kind, id);
        });
        if (m_editable && kindLockable(kind))
        {
            const bool lk = objectLocked(kind, id);
            menu.addAction(lk ? tr("Unlock position") : tr("Lock position"),
                           this, [this, kind, id, lk]() {
                setObjectLocked(kind, id, !lk);
            });
        }
        if (m_editable)
            menu.addAction(tr("Rename…"), this, [this, item]() {
                m_tree->editItem(item, 0);
            });
    }
    else if (type == NodeLayer)
    {
        if (m_editable)
        {
            menu.addAction(tr("Rename layer…"), this, [this, item]() {
                m_tree->editItem(item, 0);
            });
            menu.addAction(tr("New layer"), this, &MonitorLayersPanel::slotAddLayer);
            if (id != MonitorProperties::defaultLayerId)
                menu.addAction(tr("Delete layer"), this, &MonitorLayersPanel::slotRemoveLayer);
        }
    }

    if (!menu.isEmpty())
        menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

void MonitorLayersPanel::slotAddLayer()
{
    if (!m_editable)
        return;
    const quint32 id = m_props->addLayer(tr("Layer %1").arg(m_props->layers().count()));
    m_props->setActiveLayerId(id);
    m_doc->setModified();
    reload();
}

void MonitorLayersPanel::slotRemoveLayer()
{
    if (!m_editable)
        return;
    const quint32 id = currentLayerId();
    if (id == MonitorProperties::defaultLayerId)
        return;
    if (m_view)
        m_view->reassignLayerItems(id, MonitorProperties::defaultLayerId);
    m_props->removeLayer(id);
    m_doc->setModified();
    reload();
}

void MonitorLayersPanel::slotMoveSelectionHere()
{
    if (m_view == nullptr || !m_editable)
        return;
    const int n = m_view->setSelectedItemsLayer(currentLayerId());
    if (n == 0)
        QMessageBox::information(this, tr("Move Selection"),
                                tr("No items are selected on the canvas."));
    reload();
}

void MonitorLayersPanel::selectLayerNode(quint32 layerId)
{
    for (int i = 0; i < m_tree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *n = m_tree->topLevelItem(i);
        if (n->data(0, NodeTypeRole).toInt() == NodeLayer
                && n->data(0, NodeIdRole).toUInt() == layerId)
        {
            m_tree->setCurrentItem(n);
            m_tree->scrollToItem(n);   // keep the moved layer in view
            return;
        }
    }
}

void MonitorLayersPanel::reorderLayer(int row, int delta)
{
    QList<MonitorProperties::MonitorLayer> ls = m_props->layers();   // sorted
    const int dst = row + delta;
    if (row < 0 || row >= ls.size() || dst < 0 || dst >= ls.size())
        return;

    const quint32 movedId = ls.at(row).id;
    const MonitorProperties::MonitorLayer tmp = ls[row];
    ls[row] = ls[dst];
    ls[dst] = tmp;
    // Renumber every layer 0..n-1 in the new visual order — no ties possible.
    for (int i = 0; i < ls.size(); i++)
        m_props->setLayerOrder(ls.at(i).id, i);
    m_doc->setModified();
    reload();
    selectLayerNode(movedId);
}

void MonitorLayersPanel::slotMoveUp()
{
    if (!m_editable || !currentIsLayer())
        return;
    reorderLayer(m_tree->indexOfTopLevelItem(m_tree->currentItem()), -1);
}

void MonitorLayersPanel::slotMoveDown()
{
    if (!m_editable || !currentIsLayer())
        return;
    reorderLayer(m_tree->indexOfTopLevelItem(m_tree->currentItem()), +1);
}

void MonitorLayersPanel::toggleVisible(quint32 layerId, bool visible)
{
    m_props->setLayerVisible(layerId, visible);
    m_doc->setModified();
    if (m_view)
        m_view->refreshItemLayerState();
    reload();
}

void MonitorLayersPanel::toggleLocked(quint32 layerId, bool locked)
{
    m_props->setLayerLocked(layerId, locked);
    m_doc->setModified();
    if (m_view)
        m_view->refreshItemLayerState();
    reload();
}
