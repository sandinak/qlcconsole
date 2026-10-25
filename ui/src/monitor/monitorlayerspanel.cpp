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
#include <QTreeWidgetItemIterator>
#include <QHeaderView>
#include <QToolButton>
#include <QInputDialog>
#include <QLineEdit>
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
#include <QTimer>
#include <QStyledItemDelegate>

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
#include "truss.h"
#include "doc.h"
#include "powerdistribution.h"
#include "networktopology.h"
#include "stageplatform.h"
#include "pipe.h"
#include "stand.h"
#include "tower.h"
#include "fixturegroup.h"
#include "createfixturegroup.h"
#include "stagegroupdialog.h"
#include "headtypesdialog.h"
#include <QLineEdit>
#include <QtMath>
#include "stagetarget.h"
#include "truss.h"
#include "fixture.h"
#include "doc.h"
#include "itemicons.h"

// Row-widget glyphs. Emoji render fine on the macOS Qt5 build this fork targets.
static const char *kEyeShown  = "\xF0\x9F\x91\x81";   // 👁
static const char *kEyeHidden = "\xE2\x80\x94";       // — (em dash)
static const char *kLockOn    = "\xF0\x9F\x94\x92";   // 🔒
static const char *kLockOff   = "\xF0\x9F\x94\x93";   // 🔓

// Tree-item data roles.
static const int NodeTypeRole = Qt::UserRole;       // MonitorLayersPanel::NodeType
static const int NodeIdRole   = Qt::UserRole + 1;   // layer / group / item id
static const int NodeKindRole = Qt::UserRole + 2;   // item kind (QString)
static const int NodeBadgeRole = Qt::UserRole + 3;  // trailing "what kind of grouping" badge

/* A folder in this tree can be any of four unrelated things -- the fixtures on
   a truss, the fixtures on a riser, a studio frame, or a plain folder -- and
   until now they all looked alike. Worse, ":/group.png" was set BOTH on a
   truss-anchored group ("truss glyph") and on an engine FixtureGroup ("the
   app's Fixture Group icon"), so the two most easily confused kinds wore the
   same picture.

   The badge says which, in words, on the row. It is appended to the display
   text rather than painted by a delegate because this tree already works that
   way (fixture rows carry a lighting badge), and it is remembered in
   NodeBadgeRole so an inline rename can strip exactly what was added instead
   of guessing at a separator. */
static void setNodeLabel(QTreeWidgetItem *node, const QString &name, const QString &badge)
{
    node->setData(0, NodeBadgeRole, badge);
    node->setText(0, badge.isEmpty() ? name : name + badge);
}

/* A row's name without what this list adds to it: the "  · on riser" badge
   (exactly what setNodeLabel() appended) and a fixture's "  💡 <group>"
   lighting badge. */
static QString bareNodeName(QString text, QString badge)
{
    text = text.trimmed();
    badge = badge.trimmed();
    if (!badge.isEmpty() && text.endsWith(badge))
        text = text.left(text.size() - badge.size()).trimmed();
    const int bulb = text.indexOf(QStringLiteral("\xF0\x9F\x92\xA1"));
    if (bulb >= 0)
        text = text.left(bulb).trimmed();
    return text;
}

/* Inline rename starts from the bare name: the editor showed "US2f  · fixture
   group", badge and all, to be deleted by hand. */
class LayersNameDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        QLineEdit *le = qobject_cast<QLineEdit *>(editor);
        if (le == nullptr)
        {
            QStyledItemDelegate::setEditorData(editor, index);
            return;
        }
        le->setText(bareNodeName(index.data(Qt::DisplayRole).toString(),
                                 index.data(NodeBadgeRole).toString()));
        le->selectAll();
    }
};

MonitorLayersPanel::MonitorLayersPanel(Doc *doc, MonitorGraphicsView *view, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_view(view)
{
    setProperty("helpTopic", QStringLiteral("other-improvements.md#plot-layers-panel"));
    m_props = m_doc->monitorProperties();

    // Floor width so the splitter can never render the panel at zero px.
    setMinimumWidth(200);

    QVBoxLayout *vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(4);

    // Title row with an in-panel hide button.
    QHBoxLayout *titleRow = new QHBoxLayout();
    QLabel *title = new QLabel(tr("Plot layers"), this);
    QFont tf = title->font();
    tf.setBold(true);
    title->setFont(tf);
    titleRow->addWidget(title);

    // Expand-all / collapse-all, right of the "Layers" label.
    QToolButton *expandBtn = new QToolButton(this);
    expandBtn->setText("\xE2\x8A\x9E");   // ⊞
    expandBtn->setAutoRaise(true);
    expandBtn->setToolTip(tr("Expand all"));
    connect(expandBtn, &QToolButton::clicked, this, [this]() { m_tree->expandAll(); });
    titleRow->addWidget(expandBtn);

    QToolButton *collapseBtn = new QToolButton(this);
    collapseBtn->setText("\xE2\x8A\x9F");   // ⊟
    collapseBtn->setAutoRaise(true);
    collapseBtn->setToolTip(tr("Collapse all"));
    connect(collapseBtn, &QToolButton::clicked, this, [this]() { m_tree->collapseAll(); });
    titleRow->addWidget(collapseBtn);

    titleRow->addStretch(1);
    m_hideBtn = new QToolButton(this);
    m_hideBtn->setText("\xC3\x97"); // ×
    m_hideBtn->setAutoRaise(true);
    m_hideBtn->setToolTip(tr("Hide the Plot layers panel"));
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

    // Search / filter box — find things in a deep tree by name.
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search the tree…"));
    m_search->setClearButtonEnabled(true);
    connect(m_search, &QLineEdit::textChanged, this, &MonitorLayersPanel::filterTree);
    vbox->addWidget(m_search);

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
    m_tree->viewport()->installEventFilter(this);   // the selection a double-click starts from
    LayersNameDelegate *names = new LayersNameDelegate(m_tree);
    m_tree->setItemDelegate(names);
    // The reload held back while a name was typed in, now it is in.
    connect(names, &QAbstractItemDelegate::closeEditor, this, [this]() {
        if (!m_reloadAfterEdit)
            return;
        m_reloadAfterEdit = false;
        QTimer::singleShot(0, this, &MonitorLayersPanel::reload);
    });
    connect(m_tree, &LayersTreeWidget::itemsDropped,
            this, &MonitorLayersPanel::handleTreeDrop);
    // Suppress rebuilds during the drag; rebuild once, safely, after it ends.
    connect(m_tree, &LayersTreeWidget::dragStarted, this, [this]() { m_suppressReload = true; });
    connect(m_tree, &LayersTreeWidget::dragFinished, this, [this]() {
        m_suppressReload = false;
        reload();
    });
    vbox->addWidget(m_tree, 1);

    QLabel *hint = new QLabel(tr("Enter/F2 renames a layer or folder. Multi-select "
                                 "items, then right-click to make a folder or move."), this);
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
    /* Selecting here selects on the canvas, which is what the tree is for --
       and it is the only way to get hold of an item the projection has buried.
       In Side view every fixture on a left-right truss lands on the same point
       (X is not on screen), so they stack and only the top one can be clicked;
       picking it by name here reaches the rest. */
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, [this]() {
        /* Not while the tree is being rebuilt: clearing it "deselects" every
           row, and passing that on emptied the plot's selection whenever a
           fixture changed -- setting Max draw on several fixtures deselected
           them all ("it deselected the fixtures"). */
        if (m_syncingSelection || m_reloading || m_view == nullptr)
            return;
        m_syncingSelection = true;
        m_view->selectMapItems(selectedObjects());
        m_syncingSelection = false;
    });
    connect(m_tree, &QWidget::customContextMenuRequested, this, &MonitorLayersPanel::slotContextMenu);

    if (m_doc != nullptr)
        connect(m_doc, &Doc::modeChanged, this, &MonitorLayersPanel::updateEditableState);

    /* Selecting on the plot selects here too: the list shows where the thing
       you clicked lives (its layer, its structure, its folder). */
    if (m_view != nullptr)
        connect(m_view, &MonitorGraphicsView::mapSelectionChanged, this, [this]() {
            followMapSelection(m_view->selectedMapObjects());
        });

    /* A fixture renamed anywhere shows its new name here. Deferred: a rename
       made IN this list arrives while its row is still being edited, and a
       rebuild there would pull the row out from under the edit. */
    if (m_doc != nullptr)
        connect(m_doc, &Doc::fixtureChanged, this, [this](quint32) {
            if (m_reloadQueued)
                return;
            m_reloadQueued = true;
            QTimer::singleShot(0, this, [this]() {
                m_reloadQueued = false;
                reload();
            });
        });

    /* Auto fixtures shown as their closest look, or not: the fixture and
       group icons. Coalesced like a rename. */
    if (m_doc != nullptr)
        connect(m_doc, &Doc::stageLooksChanged, this, [this]() {
            if (m_reloadQueued)
                return;
            m_reloadQueued = true;
            QTimer::singleShot(0, this, [this]() {
                m_reloadQueued = false;
                reload();
            });
        });

    /* A layer shown or hidden somewhere else -- the Rig Overview's Layers
       menu -- shows here too. Deferred and coalesced like a rename: a toggle
       made HERE also lands here, from inside its own button's signal. */
    if (m_view != nullptr)
        connect(m_view, &MonitorGraphicsView::layerStateChanged, this, [this]() {
            if (m_reloadQueued)
                return;
            m_reloadQueued = true;
            QTimer::singleShot(0, this, [this]() {
                m_reloadQueued = false;
                reload();
            });
        });

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
        if (m_props->isApronPlatform(p->id()))
            continue;           // the apron's own deck: listed as the apron, below
        ItemDesc d;
        d.kind    = QStringLiteral("platform");
        d.id      = p->id();
        d.name    = p->name().isEmpty() ? tr("Platform %1").arg(p->id()) : p->name();
        d.layerId = p->layerId();
        d.groupId = p->groupId();
        d.color   = p->color();
        out << d;
    }
    foreach (Pipe *b, m_props->pipes())
    {
        ItemDesc d;
        d.kind    = QStringLiteral("pipe");
        d.id      = b->id();
        d.name    = b->name().isEmpty() ? tr("Boom %1").arg(b->id()) : b->name();
        d.layerId = b->layerId();
        d.groupId = b->groupId();
        out << d;
    }
    foreach (Stand *s, m_props->stands())
    {
        ItemDesc d;
        d.kind    = QStringLiteral("stand");
        d.id      = s->id();
        d.name    = s->name().isEmpty() ? tr("Stand %1").arg(s->id()) : s->name();
        d.layerId = s->layerId();
        d.groupId = s->groupId();
        out << d;
    }
    foreach (Scenery *sc, m_props->sceneries())
    {
        ItemDesc d;
        d.kind    = QStringLiteral("scenery");
        d.id      = sc->id();
        d.name    = sc->name().isEmpty() ? Scenery::kindName(sc->kind()) : sc->name();
        d.layerId = sc->layerId();
        d.groupId = sc->groupId();
        d.color   = sc->color();
        out << d;
    }
    /* Hangers sit with what they hang off: its layer and its group. */
    foreach (Hanger *h, m_props->hangers())
    {
        ItemDesc d;
        d.kind    = QStringLiteral("hanger");
        d.id      = h->id();
        d.name    = h->name().isEmpty() ? tr("Hanger %1").arg(h->id() + 1) : h->name();
        d.layerId = h->layerId();
        d.groupId = 0;
        if (h->parentKind() == Hanger::OnTruss)
        {
            if (Truss *t = m_props->truss(h->parentId())) { d.layerId = t->layerId(); d.groupId = t->groupId(); }
        }
        else if (Tower *tw = m_props->tower(h->parentId()))
        {
            d.layerId = tw->layerId();
            d.groupId = tw->groupId();
        }
        out << d;
    }
    foreach (Tower *t, m_props->towers())
    {
        ItemDesc d;
        d.kind    = QStringLiteral("tower");
        d.id      = t->id();
        d.name    = t->name().isEmpty() ? tr("Tower %1").arg(t->id()) : t->name();
        d.layerId = t->layerId();
        d.groupId = t->groupId();
        out << d;
    }
    /* Stage targets. They were omitted here on the grounds that they are
       dynamic aim points, but that only describes them once a scene is USING
       one. At the point of creation a target is placed in the studio like any
       other piece of geometry -- it has a position, a name and a layer, and
       slotAddTarget() has always assigned it one -- so leaving it out of this
       list meant a newly created target appeared nowhere at all and could not
       be renamed, re-layered or found. Being aimed at is shown on the canvas
       (glow + light-path lines), which is where that changes moment to moment. */
    foreach (StageTarget *t, m_props->stageTargets())
    {
        ItemDesc d;
        d.kind    = QStringLiteral("target");
        d.id      = t->id();
        d.name    = t->name().isEmpty() ? tr("Target %1").arg(t->id() + 1) : t->name();
        d.layerId = t->layerId();
        d.groupId = t->groupId();
        out << d;
    }
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
    NetworkTopology *nt = m_doc->networkTopology();
    for (int n = 0; n < nt->nodes().size(); n++)
    {
        const NetworkNode &node = nt->nodes().at(n);
        ItemDesc d;
        d.kind    = QStringLiteral("network");
        d.id      = quint32(n);
        d.name    = node.name.isEmpty() ? tr("Network %1").arg(n) : node.name;
        d.layerId = node.layerId;
        d.groupId = node.groupId;
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
    /* The stage pieces (Q147), on their own Stage layer: always listed, so
       a hidden one can be found and shown. */
    if (m_props->hasStageLayer())
    {
        foreach (MonitorProperties::StagePiece piece, MonitorProperties::stagePieces())
        {
            ItemDesc d;
            d.kind    = QStringLiteral("stage");
            d.id      = quint32(piece);
            d.name    = MonitorProperties::stagePieceName(piece);
            d.layerId = m_props->stageLayerId();
            d.groupId = 0;
            out << d;
        }
    }
    return out;
}

void MonitorLayersPanel::setSubtreeExpanded(QTreeWidgetItem *node, bool expanded)
{
    if (node == nullptr)
        return;
    node->setExpanded(expanded);
    for (int i = 0; i < node->childCount(); ++i)
        setSubtreeExpanded(node->child(i), expanded);
}

static bool applyTreeFilter(QTreeWidgetItem *node, const QString &t)
{
    bool childVisible = false;
    for (int i = 0; i < node->childCount(); ++i)
        childVisible = applyTreeFilter(node->child(i), t) || childVisible;
    const bool selfMatch = t.isEmpty() || node->text(0).contains(t, Qt::CaseInsensitive);
    const bool visible = selfMatch || childVisible;
    node->setHidden(!visible);
    if (!t.isEmpty() && visible)
        node->setExpanded(true);   // reveal matches inside collapsed branches
    return visible;
}

void MonitorLayersPanel::filterTree(const QString &text)
{
    if (m_tree == nullptr)
        return;
    const QString t = text.trimmed();
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        applyTreeFilter(m_tree->topLevelItem(i), t);
}

QTreeWidgetItem *MonitorLayersPanel::addFixtureGroupNode(QTreeWidgetItem *host, FixtureGroup *fg)
{
    QTreeWidgetItem *fgNode = new QTreeWidgetItem(host);
    setNodeLabel(fgNode, fg->name(), QObject::tr("  \xc2\xb7 fixture group"));
    fgNode->setIcon(0, ItemIcons::groupIcon(m_doc, fg));   // as in every other list
    fgNode->setFirstColumnSpanned(true);
    // Draggable too: dropped on a layer or a folder, its fixtures go there.
    // Renamed in place like every other row (select it, Enter).
    fgNode->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled
                     | (m_editable ? Qt::ItemIsDragEnabled | Qt::ItemIsEditable : Qt::NoItemFlags));
    fgNode->setData(0, NodeTypeRole, int(NodeFixtureGroup));
    fgNode->setData(0, NodeIdRole, fg->id());
    fgNode->setToolTip(0, tr("Fixture Group “%1” — double-click to edit the head "
                             "layout; drop fixtures here to add them").arg(fg->name()));
    m_listedFixtureGroups.insert(fg->id());
    return fgNode;
}

void MonitorLayersPanel::addLocalFixtureGroups(QTreeWidgetItem *host,
        const QSet<quint32> &fixtureSet, QHash<quint32, QList<QTreeWidgetItem *> > &map)
{
    if (host == nullptr || fixtureSet.isEmpty())
        return;
    foreach (FixtureGroup *fg, m_doc->fixtureGroups())
    {
        if (fg == nullptr)
            continue;
        const QList<quint32> members = fg->fixtureList();
        if (members.isEmpty())
            continue;
        bool allInside = true;
        foreach (quint32 fid, members)
            if (!fixtureSet.contains(fid)) { allInside = false; break; }
        if (!allInside)
            continue;   // spans a wider scope → not local here

        QTreeWidgetItem *fgNode = addFixtureGroupNode(host, fg);
        /* Under EVERY group it is in, not only the first: a fixture split by
           head type is in its RGB group and its W group alike, and listing it
           once left the others looking empty ("it puts the fixture in that
           group but not the other groups"). */
        foreach (quint32 fid, members)
            if (!map.value(fid).contains(fgNode))
                map[fid] << fgNode;
    }
}

void MonitorLayersPanel::createFixtureGroupFrom(const QList<quint32> &fixtureIds)
{
    if (fixtureIds.isEmpty())
        return;
    // Shared creator (grid heuristic + physical-order arrangement), consistent
    // with the Fixture Manager and Programming tab.
    if (CreateFixtureGroup::createFromFixtures(m_doc, fixtureIds, this) != NULL)
        reload();
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
    // A fixture wears the icon it has in every other list.
    if (d.kind == QStringLiteral("fixture"))
        node->setIcon(0, ItemIcons::fixtureIcon(m_doc->fixture(d.id)));
    else
        node->setIcon(0, kindIcon(d.kind, d.color));
    if (d.kind == QStringLiteral("stage"))
        addStagePieceToggles(node, MonitorProperties::StagePiece(d.id));
}

/* A stage piece's row: its own eye and lock, beside the name, as a layer
   has -- the piece is shown and locked by itself as well as with the Stage
   layer. Not renamed, nor dragged to another layer. */
void MonitorLayersPanel::addStagePieceToggles(QTreeWidgetItem *node, MonitorProperties::StagePiece piece)
{
    node->setFirstColumnSpanned(false);
    node->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    const bool shown = m_props->stagePieceVisible(piece);
    const bool locked = m_props->stagePieceLocked(piece);
    if (!shown)
        node->setForeground(0, QBrush(QColor(140, 140, 146)));
    node->setToolTip(0, tr("%1: double-click for its properties").arg(MonitorProperties::stagePieceName(piece)));

    QWidget *row = new QWidget(m_tree);
    row->setObjectName(QStringLiteral("stagePieceToggles%1").arg(int(piece)));
    QHBoxLayout *h = new QHBoxLayout(row);
    h->setContentsMargins(2, 1, 2, 1);
    h->setSpacing(3);
    auto style = [](QToolButton *b, bool on, const QString &glyph, const char *onBg, const char *offBg) {
        b->setText(glyph);
        QFont f = b->font(); f.setBold(true); b->setFont(f);
        b->setFixedSize(20, 20);
        b->setStyleSheet(QString(
            "QToolButton{border:none;border-radius:4px;background:%1;}"
            "QToolButton:hover{border:1px solid rgba(255,255,255,0.4);}"
            "QToolButton:disabled{background:rgba(90,90,94,0.25);}")
            .arg(QLatin1String(on ? onBg : offBg)));
    };
    QToolButton *eye = new QToolButton(row);
    eye->setObjectName(QStringLiteral("stagePieceEye"));
    eye->setCheckable(true);
    eye->setChecked(shown);
    eye->setToolTip(shown ? tr("Shown — click to hide") : tr("Hidden — click to show"));
    style(eye, shown, QString::fromUtf8(shown ? kEyeShown : kEyeHidden), "rgb(42,120,60)", "rgb(120,46,46)");
    h->addWidget(eye);
    QToolButton *lock = new QToolButton(row);
    lock->setObjectName(QStringLiteral("stagePieceLock"));
    lock->setCheckable(true);
    lock->setChecked(locked);
    lock->setEnabled(m_editable);
    lock->setToolTip(locked ? tr("Locked — click to unlock") : tr("Unlocked — click to lock"));
    style(lock, locked, QString::fromUtf8(locked ? kLockOn : kLockOff), "rgb(168,120,30)", "rgb(64,64,68)");
    h->addWidget(lock);
    h->addStretch(1);
    m_tree->setItemWidget(node, 1, row);

    auto changed = [this]() {
        m_doc->setModified();
        if (m_view != nullptr)
        {
            m_view->updateStageMarks();
            emit m_view->stagePiecesChanged();
        }
        QTimer::singleShot(0, this, &MonitorLayersPanel::reload);   // not under the button
    };
    connect(eye, &QToolButton::toggled, this, [this, piece, changed](bool on) {
        if (m_reloading) return;
        m_props->setStagePieceVisible(piece, on);
        changed();
    });
    connect(lock, &QToolButton::toggled, this, [this, piece, changed](bool on) {
        if (m_reloading) return;
        m_props->setStagePieceLocked(piece, on);
        changed();
    });
}

QIcon MonitorLayersPanel::kindIcon(const QString &kind, const QColor &color) const
{
    if (kind == QStringLiteral("fixture"))  return QIcon(":/fixture.png");
    // NOT :/group.png -- that is the Fixture Group icon, and a truss wearing it
    // made the two kinds indistinguishable in the tree.
    if (kind == QStringLiteral("truss"))    return glyphIcon(QStringLiteral("\xF0\x9F\x8F\x97"), QColor(150, 200, 235)); // ð truss
    if (kind == QStringLiteral("platform")) return swatchIcon(color);           // colour-coded riser
    if (kind == QStringLiteral("pipe"))     return glyphIcon(QStringLiteral("\xF0\x9F\x8E\xA4"), QColor(150, 200, 235)); // 🎤 boom
    if (kind == QStringLiteral("stand"))    return glyphIcon(QStringLiteral("\xF0\x9F\x93\x8D"), QColor(150, 200, 235)); // 📍 stand
    if (kind == QStringLiteral("tower"))    return glyphIcon(QStringLiteral("\xF0\x9F\x8F\xAF"), QColor(150, 200, 235)); // 🏯 tower
    if (kind == QStringLiteral("hanger"))   return glyphIcon(QStringLiteral("\xF0\x9F\xAA\x9D"), QColor(150, 200, 235)); // 🪝 hanger
    if (kind == QStringLiteral("power"))    return glyphIcon(QStringLiteral("\xE2\x9A\xA1"), QColor(235, 185, 0));       // ⚡
    if (kind == QStringLiteral("network"))  return glyphIcon(QStringLiteral("\xF0\x9F\x8C\x90"), QColor(70, 140, 220));  // 🌐 network node
    if (kind == QStringLiteral("target"))   return glyphIcon(QStringLiteral("\xF0\x9F\x8E\xAF"), QColor(255, 180, 0));    // 🎯 aim target
    if (kind == QStringLiteral("image"))    return QIcon(":/image.png");
    if (kind == QStringLiteral("stage"))    return glyphIcon(QStringLiteral("\xF0\x9F\x93\x90"), QColor(240, 210, 110)); // 📐 stage piece
    if (kind == QStringLiteral("scenery"))  return swatchIcon(color.isValid() ? color : QColor(226, 228, 236)); // the cloth
    return style()->standardIcon(QStyle::SP_FileIcon);
}

void MonitorLayersPanel::collectCollapsedNodes(QTreeWidgetItem *item, QSet<QString> &out) const
{
    const int type = item->data(0, NodeTypeRole).toInt();
    if ((type == NodeLayer || type == NodeGroup) && item->isExpanded() == false)
        out.insert(QString("%1:%2").arg(type).arg(item->data(0, NodeIdRole).toUInt()));
    /* A fixture group's row is built CLOSED, the opposite of a layer or a
       folder, so what has to survive a rebuild is that it was OPEN. It was
       not recorded at all: any rebuild -- and reload() has ~20 triggers --
       snapped every fixture group shut again ("I can't click on any side
       arrow to open any of the groups"). Marked so it cannot be mistaken
       for a closed folder. */
    if (type == NodeFixtureGroup && item->isExpanded())
        out.insert(QString("open:%1:%2").arg(type).arg(item->data(0, NodeIdRole).toUInt()));

    for (int i = 0; i < item->childCount(); i++)
        collectCollapsedNodes(item->child(i), out);
}

bool MonitorLayersPanel::wasNodeCollapsed(NodeType type, quint32 id) const
{
    return m_collapsedKeys.contains(QString("%1:%2").arg(int(type)).arg(id));
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
    {
        foreach (const ItemDesc &d, items)
            if (d.groupId == groupId && d.kind == anchorKind && d.id == anchorId)
            { anchorName = d.name; anchorColor = d.color; break; }
    }

    // Anchor only applies if the anchor item is actually present in the group.
    const bool anchored = !anchorKind.isEmpty() && !anchorName.isEmpty();

    QTreeWidgetItem *node = new QTreeWidgetItem(parent);
    if (anchored && anchorKind == QStringLiteral("truss"))
    {
        setNodeLabel(node, anchorName, tr("  \xc2\xb7 on truss"));
        node->setIcon(0, kindIcon(anchorKind, anchorColor));
    }
    else if (anchored && anchorKind == QStringLiteral("platform"))
    {
        setNodeLabel(node, anchorName, tr("  \xc2\xb7 on riser"));
        node->setIcon(0, swatchIcon(anchorColor));      // riser colour swatch
    }
    else if (anchored && anchorKind == QStringLiteral("tower"))
    {
        setNodeLabel(node, anchorName, tr("  \xc2\xb7 on tower"));
        node->setIcon(0, kindIcon(anchorKind, anchorColor));
    }
    else if (anchored && anchorKind == QStringLiteral("pipe"))
    {
        // A standing, vertical pipe is a boom; anything else a pipe.
        const Pipe *pp = m_props->pipe(anchorId);
        const bool boom = pp != nullptr && pp->isVertical();
        setNodeLabel(node, anchorName, boom ? tr("  \xc2\xb7 on boom") : tr("  \xc2\xb7 on pipe"));
        node->setIcon(0, kindIcon(anchorKind, anchorColor));
    }
    else if (g.hasFrame)
    {
        /* The only group kind with real editing behaviour -- a rigid body with
           its own origin and rotation, whose members hold group-local offsets --
           and until now the only one with no visual identity at all. */
        setNodeLabel(node, g.name, tr("  \xc2\xb7 frame"));
        node->setIcon(0, glyphIcon(QStringLiteral("\xF0\x9F\x93\xA6"), QColor(200, 175, 235))); // ð¦ rigid body
    }
    else
    {
        setNodeLabel(node, g.name, QString());   // a plain folder needs no badge
        // A manual group whose direct members are all ONE kind wears that kind's
        // icon (a group of trusses reads as trusses, not a bare folder); mixed or
        // empty groups keep the folder.
        QString commonKind;
        QColor  commonColor;
        bool uniform = true, any = false;
        foreach (const ItemDesc &d, items)
        {
            if (d.groupId != groupId)
                continue;
            any = true;
            if (commonKind.isEmpty()) { commonKind = d.kind; commonColor = d.color; }
            else if (commonKind != d.kind) { uniform = false; break; }
        }
        if (any && uniform && commonKind == QStringLiteral("fixture"))
        {
            // Fixtures: as a group of them is in every other list.
            QList<quint32> fids;
            foreach (const ItemDesc &d, items)
                if (d.groupId == groupId)
                    fids << d.id;
            node->setIcon(0, ItemIcons::targetsIcon(m_doc, QList<quint32>(), fids));
        }
        else if (any && uniform && !commonKind.isEmpty())
            node->setIcon(0, kindIcon(commonKind, commonColor));
        else
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

    // This group's direct members, and the set of fixture ids among them.
    QList<ItemDesc> myItems;
    QSet<quint32>   myFixtures;
    foreach (const ItemDesc &d, items)
    {
        if (d.groupId != groupId)
            continue;
        if (d.kind == anchorKind && d.id == anchorId)
            continue;   // the node itself represents this anchor item
        myItems << d;
        if (d.kind == QStringLiteral("fixture"))
            myFixtures.insert(d.id);
    }

    /* A bar (a child Truss hung on this one) becomes its own container here,
       the same way the truss itself does one level up -- "if I add a bar to
       a truss .. that becomes it's own container for lights like the truss
       does". Without this a bar was just another flat leaf beside its own
       fixtures: T1-B1 sat next to Bar 1 as siblings instead of under it. */
    QHash<quint32, QTreeWidgetItem *> barNode;    // bar truss id → its container
    QHash<quint32, quint32> fixtureBar;           // fid → the bar it's mounted on
    foreach (const ItemDesc &d, myItems)
    {
        if (d.kind != QStringLiteral("truss"))
            continue;
        Truss *bt = m_props->truss(d.id);
        if (bt == nullptr || bt->isChildBar() == false)
            continue;
        QTreeWidgetItem *bn = new QTreeWidgetItem(node);
        setNodeLabel(bn, d.name, tr("  \xc2\xb7 bar"));
        bn->setIcon(0, kindIcon(QStringLiteral("truss"), QColor()));
        bn->setToolTip(0, bn->text(0));
        bn->setFirstColumnSpanned(true);
        bn->setData(0, NodeTypeRole, int(NodeItem));
        bn->setData(0, NodeIdRole, d.id);
        bn->setData(0, NodeKindRole, d.kind);
        bn->setExpanded(!wasNodeCollapsed(NodeGroup, d.id));
        barNode.insert(d.id, bn);
    }
    if (!barNode.isEmpty())
    {
        foreach (quint32 fid, myFixtures)
        {
            const quint32 tid = m_props->fixtureRigProps(fid).trussId;
            if (barNode.contains(tid))
                fixtureBar.insert(fid, tid);
        }
    }

    /* A tower or pipe in a manual group (a "Towers" folder holding SL and SR,
       a "Booms" folder) is a container for what is mounted on it, the way a
       bar is on its truss -- otherwise its fixtures sit in the folder beside
       it, not under it. (One whose group is its own is the group node itself,
       above.) */
    typedef QPair<QString, quint32> HostKey;
    QHash<HostKey, QTreeWidgetItem *> hostNode;   // (kind, id) → its container
    QHash<quint32, HostKey> fixtureHost;          // fid → the tower/pipe it's on
    foreach (quint32 fid, myFixtures)
    {
        const FixtureRigProps rp = m_props->fixtureRigProps(fid);
        HostKey key;
        if (rp.onTower())
            key = HostKey(QStringLiteral("tower"), rp.towerId);
        else if (rp.pipeId != Pipe::invalidId())
            key = HostKey(QStringLiteral("pipe"), rp.pipeId);
        else
            continue;
        if (!(anchorKind == key.first && anchorId == key.second))
            fixtureHost.insert(fid, key);
    }
    foreach (const ItemDesc &d, myItems)
    {
        const HostKey key(d.kind, d.id);
        if ((d.kind != QStringLiteral("tower") && d.kind != QStringLiteral("pipe"))
            || fixtureHost.key(key, UINT_MAX) == UINT_MAX)
            continue;
        QTreeWidgetItem *tn = new QTreeWidgetItem(node);
        setNodeLabel(tn, d.name, QString());
        tn->setIcon(0, kindIcon(d.kind, d.color));
        tn->setToolTip(0, tn->text(0));
        tn->setFirstColumnSpanned(true);
        tn->setData(0, NodeTypeRole, int(NodeItem));
        tn->setData(0, NodeIdRole, d.id);
        tn->setData(0, NodeKindRole, d.kind);
        tn->setExpanded(true);
        hostNode.insert(key, tn);
    }
    // A structure that is not in this group (the fixture was grouped by
    // hand): the fixture stays a plain member here.
    for (auto it = fixtureHost.begin(); it != fixtureHost.end(); )
        it = hostNode.contains(it.value()) ? std::next(it) : fixtureHost.erase(it);

    // LOCAL Fixture Groups whose members are all within this group's fixtures show
    // as 💡 nodes, with those fixtures nested under them -- scoped per bar (so an
    // FG entirely on one bar nests under it) plus once more for whatever is left
    // directly on this truss.
    QHash<quint32, QList<QTreeWidgetItem *> > fixtureFGNode;   // fid → the FG nodes it is listed under
    QSet<quint32> onNoBar = myFixtures;
    foreach (quint32 barId, barNode.keys())
    {
        QSet<quint32> onThisBar;
        foreach (quint32 fid, fixtureBar.keys(barId)) { onThisBar.insert(fid); onNoBar.remove(fid); }
        addLocalFixtureGroups(barNode.value(barId), onThisBar, fixtureFGNode);
    }
    foreach (const HostKey &key, hostNode.keys())
    {
        QSet<quint32> onThisHost;
        foreach (quint32 fid, fixtureHost.keys(key)) { onThisHost.insert(fid); onNoBar.remove(fid); }
        addLocalFixtureGroups(hostNode.value(key), onThisHost, fixtureFGNode);
    }
    addLocalFixtureGroups(node, onNoBar, fixtureFGNode);

    // Members: a bar's own row is replaced by the container built above; a
    // fixture nests under its bar, then under a local FG if it belongs to
    // one (FG is the more specific choice, so it wins); everything else
    // (loose fixtures, non-fixture objects) stays directly under this group.
    foreach (const ItemDesc &d, myItems)
    {
        if (d.kind == QStringLiteral("truss") && barNode.contains(d.id))
            continue;       // this bar is a container now, not a leaf
        if (hostNode.contains(HostKey(d.kind, d.id)))
            continue;       // likewise a tower or pipe carrying fixtures
        QTreeWidgetItem *host = node;
        if (d.kind == QStringLiteral("fixture") && fixtureBar.contains(d.id))
            host = barNode.value(fixtureBar.value(d.id));
        if (d.kind == QStringLiteral("fixture") && fixtureHost.contains(d.id))
            host = hostNode.value(fixtureHost.value(d.id));
        if (d.kind == QStringLiteral("fixture") && fixtureFGNode.contains(d.id))
        {
            foreach (QTreeWidgetItem *fgNode, fixtureFGNode.value(d.id))
                addItemLeaf(fgNode, d);
            continue;
        }
        addItemLeaf(host, d);
    }

    node->setExpanded(!wasNodeCollapsed(NodeGroup, groupId));
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

    /* Nor while a name is being typed in: clearing the tree closes the editor
       mid-rebuild and the new name is thrown away. Enter commits a beat later
       (the delegate's commit is queued), so a reload already queued -- a
       fixture renamed a moment before -- landed first and the rename was lost.
       Once the editor closes, reload. */
    if (m_tree->isEditing())
    {
        m_reloadAfterEdit = true;
        return;
    }

    // Preserve the scroll position: without this, setCurrentItem(active) below
    // scrolls the tree back to the active layer after every reload — so moving
    // an item to a lower layer made that layer scroll out of view ("disappear").
    const int scroll = m_tree->verticalScrollBar()->value();

    // Capture which layer/group nodes are currently collapsed BEFORE clear()
    // discards them, so the rebuild below can restore that instead of
    // hard-expanding everything -- reload() runs on ~20 different triggers
    // (toggling a layer's visibility/lock, renaming, drag-drop, add/remove
    // layer, ...), every one of which used to silently reopen every folder
    // the user had closed.
    m_collapsedKeys.clear();
    for (int i = 0; i < m_tree->topLevelItemCount(); i++)
        collectCollapsedNodes(m_tree->topLevelItem(i), m_collapsedKeys);

    // The stage pieces' layer, so they can be found here (made once).
    m_props->ensureStageLayer();

    m_reloading = true;
    m_tree->clear();
    m_listedFixtureGroups.clear();

    const QList<ItemDesc> items = gatherItems();
    const quint32 activeId = m_props->activeLayerId();
    QTreeWidgetItem *activeNode = nullptr;
    QHash<quint32, QTreeWidgetItem *> layerNodes;

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
        h->setContentsMargins(2, 1, 2, 1);
        h->setSpacing(3);

        // Emoji ignore CSS colour, so signal on/off with a state-coded
        // BACKGROUND (green = visible, red = hidden; amber = locked, grey =
        // unlocked) plus a bold glyph — clearly distinct at a glance.
        auto styleToggle = [](QToolButton *b, bool on, const QString &glyph,
                              const char *onBg, const char *offBg) {
            b->setText(glyph);
            QFont f = b->font(); f.setBold(true); b->setFont(f);
            b->setStyleSheet(QString(
                "QToolButton{border:none;border-radius:4px;background:%1;}"
                "QToolButton:hover{border:1px solid rgba(255,255,255,0.4);}"
                "QToolButton:disabled{background:rgba(90,90,94,0.25);}")
                .arg(QLatin1String(on ? onBg : offBg)));
        };

        // Collapse / expand just this layer's subtree (neutral, not a state
        // toggle) — leftmost so it reads as the row's disclosure control.
        QToolButton *coll = new QToolButton(row);
        coll->setFixedSize(20, 20);
        coll->setToolTip(tr("Collapse / expand this layer"));
        {
            QFont f = coll->font(); f.setBold(true); coll->setFont(f);
            coll->setStyleSheet(
                "QToolButton{border:none;border-radius:4px;background:rgba(90,90,94,0.35);}"
                "QToolButton:hover{border:1px solid rgba(255,255,255,0.4);}");
        }
        // ▾ / ▸: narrow enough for the 20 px button in any font (⊟ / ⊞ came
        // out as "…" on a Mac whose font drew them wider).
        coll->setText(QStringLiteral("\xE2\x96\xBE"));   // ▾ (layers build expanded)
        h->addWidget(coll);
        connect(coll, &QToolButton::clicked, this, [layerNode, coll]() {
            const bool nowExpanded = !layerNode->isExpanded();
            setSubtreeExpanded(layerNode, nowExpanded);
            coll->setText(nowExpanded ? QStringLiteral("\xE2\x96\xBE")
                                      : QStringLiteral("\xE2\x96\xB8"));
        });

        QToolButton *eye = new QToolButton(row);
        eye->setCheckable(true);
        eye->setChecked(lyr.visible);
        eye->setToolTip(lyr.visible ? tr("Visible — click to hide")
                                    : tr("Hidden — click to show"));
        /* Showing, hiding and labels are how you LOOK at the rig, wanted in
           a show as much as while building it: any mode. Locking -- and
           everything that changes the rig -- stays Design only. */
        eye->setFixedSize(20, 20);
        styleToggle(eye, lyr.visible, lyr.visible ? kEyeShown : kEyeHidden,
                    "rgb(42,120,60)", "rgb(120,46,46)");
        h->addWidget(eye);

        QToolButton *lbl = new QToolButton(row);
        lbl->setCheckable(true);
        lbl->setChecked(lyr.labels);
        lbl->setToolTip(lyr.labels ? tr("Labels shown — click to hide")
                                   : tr("Labels hidden — click to show"));
        lbl->setFixedSize(20, 20);
        // "A" = labels on (blue), dimmed when off.
        styleToggle(lbl, lyr.labels, QStringLiteral("A"),
                    "rgb(50,96,150)", "rgb(64,64,68)");
        h->addWidget(lbl);

        QToolButton *lock = new QToolButton(row);
        lock->setCheckable(true);
        lock->setChecked(lyr.locked);
        lock->setToolTip(lyr.locked ? tr("Locked — click to unlock")
                                    : tr("Unlocked — click to lock"));
        lock->setEnabled(m_editable);
        lock->setFixedSize(20, 20);
        styleToggle(lock, lyr.locked, lyr.locked ? kLockOn : kLockOff,
                    "rgb(168,120,30)", "rgb(64,64,68)");
        h->addWidget(lock);
        row->setLayout(h);
        m_tree->setItemWidget(layerNode, 1, row);

        const quint32 lid = lyr.id;
        connect(eye, &QToolButton::toggled, this, [this, lid](bool on) {
            if (m_reloading) return;
            toggleVisible(lid, on);
        });
        connect(lbl, &QToolButton::toggled, this, [this, lid](bool on) {
            if (m_reloading) return;
            m_props->setLayerLabels(lid, on);
            if (m_view) m_view->refreshFixtureLabels();
            m_doc->setModified();
            reload();
        });
        connect(lock, &QToolButton::toggled, this, [this, lid](bool on) {
            if (m_reloading) return;
            toggleLocked(lid, on);
        });

        foreach (const MonitorProperties::MonitorGroup &grp, m_props->groups())
            if (grp.parentGroupId == 0 && grp.layerId == lyr.id)
                buildGroupNode(layerNode, grp.id, items);

        // Loose (un-foldered) fixtures on this layer, and any Fixture Group local
        // to them — so a group made from loose fixtures (e.g. UST-1..8) is visible.
        QSet<quint32> looseFix;
        foreach (const ItemDesc &d, items)
            if (d.groupId == 0 && d.layerId == lyr.id && d.kind == QStringLiteral("fixture"))
                looseFix.insert(d.id);
        QHash<quint32, QList<QTreeWidgetItem *> > looseFGNode;
        addLocalFixtureGroups(layerNode, looseFix, looseFGNode);

        foreach (const ItemDesc &d, items)
            if (d.groupId == 0 && d.layerId == lyr.id)
            {
                if (d.kind == QStringLiteral("fixture") && looseFGNode.contains(d.id))
                {
                    foreach (QTreeWidgetItem *fgNode, looseFGNode.value(d.id))
                        addItemLeaf(fgNode, d);
                    continue;
                }
                addItemLeaf(layerNode, d);
            }
        layerNodes.insert(lyr.id, layerNode);

        layerNode->setExpanded(!wasNodeCollapsed(NodeLayer, lyr.id));
        if (lyr.id == activeId)
            activeNode = layerNode;
    }

    /* Fixture Groups local to no one structure -- a group made across T-1 to
       T-4 -- were in no row at all. Under their layer when all their fixtures
       are on it, else under "Fixture Groups" at the end; their fixtures
       listed under them as well as where they hang. */
    {
        QHash<quint32, ItemDesc> fixtureDesc;
        foreach (const ItemDesc &d, items)
            if (d.kind == QStringLiteral("fixture"))
                fixtureDesc.insert(d.id, d);
        QTreeWidgetItem *across = nullptr;
        foreach (FixtureGroup *fg, m_doc->fixtureGroups())
        {
            if (fg == nullptr || m_listedFixtureGroups.contains(fg->id()))
                continue;
            QList<ItemDesc> members;
            QSet<quint32> layers;
            foreach (quint32 fid, fg->fixtureList())
                if (fixtureDesc.contains(fid))
                {
                    members << fixtureDesc.value(fid);
                    layers.insert(fixtureDesc.value(fid).layerId);
                }
            if (members.isEmpty())
                continue;           // nothing of it on the plot
            QTreeWidgetItem *host = (layers.size() == 1) ? layerNodes.value(*layers.constBegin()) : nullptr;
            if (host == nullptr)
            {
                if (across == nullptr)
                {
                    across = new QTreeWidgetItem(m_tree);
                    across->setText(0, tr("Fixture Groups"));
                    across->setToolTip(0, tr("Fixture groups whose fixtures are on more than one layer"));
                    across->setFirstColumnSpanned(true);
                    across->setFlags(Qt::ItemIsEnabled);
                    // Not a layer: no type would read as 0, which IS a layer.
                    across->setData(0, NodeTypeRole, -1);
                    across->setExpanded(true);
                }
                host = across;
            }
            QTreeWidgetItem *fgNode = addFixtureGroupNode(host, fg);
            foreach (const ItemDesc &d, members)
                addItemLeaf(fgNode, d);
        }
    }

    if (activeNode != nullptr)
        m_tree->setCurrentItem(activeNode);

    // Reopen the fixture groups that were open (see collectCollapsedNodes()).
    for (QTreeWidgetItemIterator it(m_tree); *it != nullptr; ++it)
    {
        const int type = (*it)->data(0, NodeTypeRole).toInt();
        if (type == NodeFixtureGroup
            && m_collapsedKeys.contains(QString("open:%1:%2").arg(type)
                                            .arg((*it)->data(0, NodeIdRole).toUInt())))
            (*it)->setExpanded(true);
    }

    // What is selected on the plot is selected here again after the rebuild.
    if (m_view != nullptr)
        followMapSelection(m_view->selectedMapObjects());

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

    // Re-apply the active search filter to the freshly-rebuilt rows.
    if (m_search != nullptr && !m_search->text().trimmed().isEmpty())
        filterTree(m_search->text());
}

bool MonitorLayersPanel::eventFilter(QObject *obj, QEvent *event)
{
    /* A double-click's first click leaves only its own row selected; what
       was selected before it is what the double-click means to edit. */
    if (obj == m_tree->viewport() && event->type() == QEvent::MouseButtonPress)
        m_pressSelection = selectedObjects();
    if (obj == m_tree && event->type() == QEvent::KeyPress)
    {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        /* Enter starts a rename -- not the one that ENDS a rename: the
           editor's Return comes on up to the tree, and the edit it is
           committing must be left to finish. */
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) && !m_tree->isEditing())
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

    // Dropping fixtures ONTO a Fixture Group node adds them to that lighting group
    // (the direct way to put US #1/#2 back into "US1").
    {
        QTreeWidgetItem *fgTarget = target;
        while (fgTarget != nullptr && fgTarget->data(0, NodeTypeRole).toInt() != NodeFixtureGroup)
            fgTarget = fgTarget->parent();
        if (fgTarget != nullptr)
        {
            const quint32 fgId = fgTarget->data(0, NodeIdRole).toUInt();
            QList<quint32> fids;
            foreach (QTreeWidgetItem *d, dragged)
                if (d->data(0, NodeTypeRole).toInt() == NodeItem
                    && d->data(0, NodeKindRole).toString() == QStringLiteral("fixture"))
                    fids << d->data(0, NodeIdRole).toUInt();
            // Defer: assignFixture reloads the tree, deleting the rows Qt's drag
            // machinery is still unwinding.
            QTimer::singleShot(0, this, [this, fgId, fids]() {
                FixtureGroup *fg = m_doc->fixtureGroup(fgId);
                if (fg == nullptr || fids.isEmpty())
                    return;
                foreach (quint32 fid, fids)
                    fg->assignFixture(fid);
                m_doc->setModified();
                reload();
            });
            return;
        }
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

    // A bare (not-yet-grouped) truss/pipe/tower ITEM is also a valid attach
    // target directly -- the group-anchor check below only ever fires once a
    // truss already HAS an anchored group (i.e. already has a fixture on it,
    // see ensureTrussGroup()), so a fresh structural feature with nothing on
    // it yet needed this too: dropping a fixture on it did nothing but move
    // it to that layer, not attach it, until this was added.
    QString directAttachKind;
    quint32 directAttachId = 0;
    if (target != nullptr && target->data(0, NodeTypeRole).toInt() == NodeItem)
    {
        const QString k = target->data(0, NodeKindRole).toString();
        if (k == QStringLiteral("truss") || k == QStringLiteral("pipe")
            || k == QStringLiteral("tower"))
        {
            directAttachKind = k;
            directAttachId = target->data(0, NodeIdRole).toUInt();
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
        else if (type == NodeFixtureGroup)
        {
            // A fixture group goes where it is dropped as its fixtures do.
            for (const QPair<QString, quint32> &m : fixtureGroupMembers(d->data(0, NodeIdRole).toUInt()))
                if (!leaves.contains(m))
                    leaves << m;
        }
        // dragged layer nodes are ignored (use the up/down buttons to reorder)
    }

    if (!leaves.isEmpty())
    {
        // Dropping fixture(s) directly onto a truss/pipe/tower ITEM, or onto a
        // truss-, tower- or pipe-anchored GROUP (an already-populated one),
        // attaches them -- explicit attach, since 2D drop-to-bind was removed.
        // A riser's group is mounted through the Face Editor; the direct-item
        // path above covers a not-yet-grouped structure. Non-fixture leaves
        // just reparent as usual.
        const MonitorProperties::MonitorGroup g =
            intoGroup ? m_props->group(containerId) : MonitorProperties::MonitorGroup();
        QString attachKind = directAttachKind;
        quint32 attachId = directAttachId;
        if (attachKind.isEmpty() && intoGroup && g.anchorId != 0
            && (g.anchorKind == QStringLiteral("truss") || g.anchorKind == QStringLiteral("tower")
                || g.anchorKind == QStringLiteral("pipe")))
        {
            attachKind = g.anchorKind;
            attachId = g.anchorId;
        }

        if (!attachKind.isEmpty())
        {
            QList<QPair<QString, quint32> > nonFixtures;
            for (const QPair<QString, quint32> &lf : leaves)
            {
                if (lf.first != QStringLiteral("fixture"))
                { nonFixtures << lf; continue; }
                if (attachKind == QStringLiteral("truss"))
                    m_view->attachFixtureToTruss(lf.second, attachId);
                else if (attachKind == QStringLiteral("pipe"))
                    m_view->attachFixtureToPipe(lf.second, attachId);
                else if (attachKind == QStringLiteral("tower"))
                    m_view->attachFixtureToTower(lf.second, attachId);
            }
            if (!nonFixtures.isEmpty())
            {
                if (intoGroup) m_view->reparentToGroup(nonFixtures, containerId);
                else           m_view->reparentToLayer(nonFixtures, containerLayer);
            }
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
    if (type == NodeFixtureGroup)
    {
        // Open the head-layout mapping editor (host wires this to the same dialog
        // the Programming tab uses).
        emit editFixtureGroupRequested(item->data(0, NodeIdRole).toUInt());
    }
    else if (type == NodeItem)
    {
        // Open the item's editor on the canvas: for every fixture selected
        // when this one was among them.
        const QPair<QString, quint32> me(item->data(0, NodeKindRole).toString(),
                                         item->data(0, NodeIdRole).toUInt());
        if (m_view && me.first == QLatin1String("fixture") && m_pressSelection.size() > 1
                && m_pressSelection.contains(me))
        {
            m_view->selectMapItems(m_pressSelection);
            emit m_view->fixtureDoubleClicked(me.second);
        }
        else if (m_view)
            m_view->requestEditItem(me.first, me.second);
    }
    else if (type == NodeGroup && m_view)
    {
        /* Same rule as the right-click menu, and it has to be the same or the
           two gestures disagree: an auto-group for a truss or riser has no
           studio frame, so the Fixture Studio has nothing in it to change.
           Double-click goes to the STRUCTURE it stands for -- which is what a
           row wearing a truss glyph and reading "on truss" promises.

           Only the menu was gated at first, and double-click is the gesture
           people actually use: "whyyyyy when I click on T-1 .. which has a
           truss glyph .. am I NOT getting the truss editor". */
        const quint32 gid = item->data(0, NodeIdRole).toUInt();
        const MonitorProperties::MonitorGroup g = m_props->group(gid);

        /* An ANCHORED row is the structure. It takes the structure's name, wears
           its glyph and reads "on truss" / "on riser", so activating it has to
           open the structure -- whatever else the group happens to carry.
           hasFrame is NOT the test: an auto-group may well have a frame (T-1 and
           T-2 in stage-structures-demo do), and gating on it sent exactly the
           rows that look most like a truss to the Fixture Studio instead. The
           frame, where there is one, is reachable from the right-click menu as
           "Fixture layout...". */
        if (!g.anchorKind.isEmpty())
        {
            m_view->requestEditItem(g.anchorKind, g.anchorId);
            return;
        }
        // Unanchored: a frame, or a plain folder promoted in place.
        m_view->openStudioGroupForGroup(gid);
        reload();
    }
    else if (m_editable)
    {
        // Layer: double-click starts inline rename.
        m_tree->editItem(item, 0);
    }
}

void MonitorLayersPanel::slotItemChanged(QTreeWidgetItem *item, int column)
{
    if (m_reloading || item == nullptr || column != 0)
        return;
    const int type = item->data(0, NodeTypeRole).toInt();
    const quint32 id = item->data(0, NodeIdRole).toUInt();
    /* Strip the badges before they are baked into the name -- an edit that
       kept them, or one made before the editor started from the bare name. */
    const QString name = bareNodeName(item->text(0), item->data(0, NodeBadgeRole).toString());
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
    else if (type == NodeFixtureGroup)
    {
        FixtureGroup *fg = m_doc->fixtureGroup(id);
        if (fg == nullptr || fg->name() == name)
            return;
        fg->setName(name);
        /* It is listed under every structure holding its fixtures: every row
           shows the new name. Not now -- the edited row is still in use. */
        if (!m_reloadQueued)
        {
            m_reloadQueued = true;
            QTimer::singleShot(0, this, [this]() {
                m_reloadQueued = false;
                reload();
            });
        }
    }
    else // NodeItem — rename the underlying map item
    {
        /* Refused (another fixture has that name): the row goes back to the
           name it has. Not now -- the edited row is still in use. */
        if (!renameMapItem(item->data(0, NodeKindRole).toString(), id, name))
        {
            if (!m_reloadQueued)
            {
                m_reloadQueued = true;
                QTimer::singleShot(0, this, [this]() {
                    m_reloadQueued = false;
                    reload();
                });
            }
            return;
        }
    }
    m_doc->setModified();
}

bool MonitorLayersPanel::kindLockable(const QString &kind) const
{
    return kind == QStringLiteral("truss") || kind == QStringLiteral("platform")
        || kind == QStringLiteral("image") || kind == QStringLiteral("power")
        || kind == QStringLiteral("network")
        || kind == QStringLiteral("pipe")  || kind == QStringLiteral("stand")
        || kind == QStringLiteral("tower") || kind == QStringLiteral("target")
        || kind == QStringLiteral("scenery");
}

bool MonitorLayersPanel::objectLocked(const QString &kind, quint32 id) const
{
    // An object is locked exactly when its layer is (MonitorGraphicsView).
    return m_view != nullptr && m_view->itemLocked(kind, id);
}

void MonitorLayersPanel::setObjectLocked(const QString &kind, quint32 id, bool locked)
{
    // Locks its whole layer: there is no per-object lock.
    if (m_view != nullptr)
        m_view->setItemLocked(kind, id, locked);
}

bool MonitorLayersPanel::renameMapItem(const QString &kind, quint32 id, const QString &name)
{
    return m_view == nullptr || m_view->renameItem(kind, id, name);
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

QList<QPair<QString, quint32> > MonitorLayersPanel::fixtureGroupMembers(quint32 fgId) const
{
    /* A fixture group is not a thing on the plot -- its FIXTURES are. So it
       moves to a layer, or anywhere else, as they do ("I can't move USTs
       fixture group to a different layer .. drag/drop or right click move to
       layer"); a row that stood for nothing left Move to Layer with nothing
       to move. Only the members that are on the plot: a layer is a plot
       thing. */
    QList<QPair<QString, quint32> > out;
    FixtureGroup *fg = m_doc->fixtureGroup(fgId);
    if (fg == nullptr)
        return out;
    const QList<quint32> onPlot = m_props->fixtureItemsID();
    foreach (quint32 fid, fg->fixtureList())
        if (onPlot.contains(fid))
            out << qMakePair(QStringLiteral("fixture"), fid);
    return out;
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
        else if (type == NodeFixtureGroup)
            add << fixtureGroupMembers(it->data(0, NodeIdRole).toUInt());
        // layer nodes contribute nothing on their own
        for (const QPair<QString, quint32> &p : add)
        {
            const QString key = p.first + QLatin1Char(':') + QString::number(p.second);
            if (!seen.contains(key)) { seen.insert(key); out << p; }
        }
    }
    return out;
}

void MonitorLayersPanel::removeLeavesFromGroup(const QList<QPair<QString, quint32> > &targets)
{
    if (targets.isEmpty() || m_view == nullptr)
        return;
    m_view->removeFromFolder(targets);
    reload();
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
        // New things are never put on the stage pieces' layer.
        if (m_props->activeLayerId() != id && id != m_props->stageLayerId())
        {
            m_props->setActiveLayerId(id);
            m_doc->setModified();
            reload();
        }
    }
    else if (m_view != nullptr)
    {
        /* The plot follows the list here -- so the list must not then follow
           the plot back. An item the plot cannot select (a fixture stacked out
           of sight inside a tower in plan view: exactly what this list is for
           reaching) came back as "nothing selected", and the row the click
           had just selected was cleared again ("they select and deselect
           immediately .. T1-W which I need to rename"). */
        m_syncingSelection = true;
        if (type == NodeGroup)
            m_view->selectItemsInGroup(id);
        else if (type == NodeFixtureGroup)
            /* Its fixtures, as the selection already picked them. Taken as an
               item, it had no kind, selected nothing and cleared the plot a
               moment after the row lit it ("they lighted for a second and
               then went back out"). */
            m_view->selectMapItems(selectedObjects());
        else // NodeItem
            m_view->selectMapItem(item->data(0, NodeKindRole).toString(), id);
        m_syncingSelection = false;
    }
}

void MonitorLayersPanel::slotContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    buildContextMenu(&menu, m_tree->itemAt(pos));
    if (!menu.isEmpty())
        menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

/* A row that IS an item -- a fixture, a truss, a riser shown as its group --
   gets exactly the menu that item has on the plot, built by the plot, down to
   the order; only "Show on Map" stands where the plot says "Show in Layers".
   Rows that are not one item (a plain folder, a layer, a fixture group) have
   their own few actions, then the same organising section for the selection. */
void MonitorLayersPanel::buildContextMenu(QMenu *out, QTreeWidgetItem *item)
{
    QMenu &menu = *out;
    if (item == nullptr)
    {
        // Blank space: offer to create a new layer.
        if (m_editable)
            menu.addAction(tr("New Layer"), this, &MonitorLayersPanel::slotAddLayer);
        return;
    }
    if (m_view == nullptr)
        return;

    const int type = item->data(0, NodeTypeRole).toInt();
    const quint32 id = item->data(0, NodeIdRole).toUInt();

    MonitorGraphicsView::MenuPlace place;
    place.inList = true;
    place.selection = selectedObjects();
    place.renameInPlace = [this, item]() { m_tree->editItem(item, 0); };

    if (type == NodeItem)
    {
        m_view->buildItemMenu(&menu, item->data(0, NodeKindRole).toString(), id, place);
        return;
    }

    if (type == NodeGroup)
    {
        const MonitorProperties::MonitorGroup g = m_props->group(id);
        if (!g.anchorKind.isEmpty())
        {
            // A truss or riser carrying fixtures: it is still that truss or riser.
            m_view->buildItemMenu(&menu, g.anchorKind, g.anchorId, place);
            return;
        }

        // A plain folder.
        if (m_editable)
            menu.addAction(tr("Edit Layout…"), this, [this, id]() {
                if (m_view) m_view->openStudioGroupForGroup(id);
                reload();
            });
        menu.addAction(tr("Show on Map"), this, [this, id]() {
            if (m_view) m_view->selectItemsInGroup(id);
        });
        const QList<quint32> folderFixtures = m_view->fixturesInGroup(id);
        if (!folderFixtures.isEmpty())
        {
            QAction *fp = menu.addAction(tr("Show Light Footprints"), this,
                                         [this, folderFixtures]() {
                if (m_view)
                    m_view->setFootprintsShown(folderFixtures,
                                               !m_view->footprintsShown(folderFixtures));
            });
            fp->setCheckable(true);
            fp->setChecked(m_view->footprintsShown(folderFixtures));
        }
        if (m_editable)
        {
            const bool locked = g.locked;
            menu.addAction(locked ? tr("Unlock Folder Position") : tr("Lock Folder Position"),
                           this, [this, id, locked]() {
                m_props->setGroupLocked(id, !locked);
                m_doc->setModified();
                if (m_view) m_view->refreshItemLayerState();
                reload();
            });
            menu.addAction(g.hasFrame ? tr("Rename…") : tr("Rename Folder…"),
                           this, [this, item]() {
                m_tree->editItem(item, 0);
            });
            menu.addAction(g.hasFrame ? tr("Ungroup") : tr("Dissolve Folder"),
                           this, [this, id]() {
                if (m_view) m_view->ungroupById(id);
            });
        }
    }
    else if (type == NodeFixtureGroup && m_doc->fixtureGroup(id) != nullptr)
    {
        /* The others like it: the step fronts are four groups of eight of the
           same bar, laid out the same -- select them all at once. When none
           match the layout exactly, the groups of the same fixtures in any
           layout. Selecting is not an edit: offered in any mode. */
        FixtureGroup *fg = m_doc->fixtureGroup(id);
        const QList<FixtureGroup *> same = CreateFixtureGroup::similarGroups(m_doc, fg, true);
        const QList<FixtureGroup *> sameFixtures = CreateFixtureGroup::similarGroups(m_doc, fg, false);
        auto idsOf = [](const QList<FixtureGroup *> &gs) {
            QList<quint32> ids;
            foreach (FixtureGroup *g, gs) ids << g->id();
            return ids;
        };
        QAction *sim = menu.addAction(same.size() > 1 ? tr("Select All Similar (%1)").arg(same.size())
                                                      : tr("Select All Similar"),
                                      this, [this, ids = idsOf(same)]() { selectFixtureGroups(ids); });
        sim->setEnabled(same.size() > 1);
        sim->setToolTip(tr("Fixture groups of the same fixture models, laid out the same way"));
        if (sameFixtures.size() > same.size())
            menu.addAction(tr("Select All with the Same Fixtures (%1)").arg(sameFixtures.size()),
                           this, [this, ids = idsOf(sameFixtures)]() { selectFixtureGroups(ids); });
        menu.addSeparator();
    }
    if (type == NodeFixtureGroup && m_editable)
    {
        FixtureGroup *fg = m_doc->fixtureGroup(id);
        /* Listed under a truss, tower or boom: it can be taken out of it
           ("I can't right click the 3z fixture group and have an option to
           remove from truss .. which it clearly shouldn't be there"). */
        for (QTreeWidgetItem *up = item->parent(); up != nullptr && fg != nullptr; up = up->parent())
        {
            if (up->data(0, NodeTypeRole).toInt() != NodeGroup)
                continue;
            const MonitorProperties::MonitorGroup host = m_props->group(up->data(0, NodeIdRole).toUInt());
            if (host.anchorKind != QLatin1String("truss") && host.anchorKind != QLatin1String("tower")
                && host.anchorKind != QLatin1String("pipe"))
                continue;
            const QString kind = host.anchorKind;
            const quint32 anchor = host.anchorId;
            const QList<quint32> members = fg->fixtureList();
            menu.addAction(tr("Remove “%1” from “%2”").arg(fg->name(), up->text(0).section(QStringLiteral("  "), 0, 0)),
                           this, [this, members, kind, anchor]() {
                if (m_view) m_view->takeOutOfStructure(members, kind, anchor);
                reload();
            });
            break;
        }

        /* Several fixture groups: one group of them, laid out as they stand on
           stage ("create fixture group from those groups that uses the
           relative layout sizing from the studio"). */
        QList<quint32> groups;
        foreach (QTreeWidgetItem *sel, m_tree->selectedItems())
            if (sel->data(0, NodeTypeRole).toInt() == NodeFixtureGroup)
                groups << sel->data(0, NodeIdRole).toUInt();
        if (!groups.contains(id))
            groups = { id };
        if (groups.size() > 1)
            menu.addAction(tr("Create Fixture Group from %1 Groups…").arg(groups.size()),
                           this, [this, groups]() {
                if (CreateFixtureGroup::createFromGroups(m_doc, groups, this) != nullptr)
                    reload();
            });

        /* Its layout made again from where its fixtures stand -- a group
           filled in head order (US2f: eight 64-head bars poured into a 23 x
           23 square) is put right without making a new one. */
        if (fg != nullptr && StageGroupDialog::anyOnStage(m_doc, fg->fixtureList()))
            menu.addAction(tr("Lay Out from the Stage…"), this, [this, id]() {
                FixtureGroup *g = m_doc->fixtureGroup(id);
                if (g == nullptr)
                    return;
                StageGroupDialog dlg(m_doc, g->fixtureList(), g->name(), this);
                dlg.setEditing(g);
                if (dlg.exec() == QDialog::Accepted)
                    reload();
            });

        // Mixed heads (RGB and white rows): a group per kind of head.
        if (fg != nullptr && HeadTypesDialog::anyMixed(m_doc, fg->fixtureList()))
        {
            const QList<quint32> members = fg->fixtureList();
            const QString base = fg->name();
            menu.addAction(tr("Split “%1” by Head Type…").arg(base), this, [this, members, base]() {
                HeadTypesDialog dlg(m_doc, members, base, this);
                if (dlg.exec() == QDialog::Accepted)
                    reload();
            });
        }

        /* The group itself can go -- "UST" was a duplicate of "USTs", the
           same eight fixtures, with no way to get rid of it from here. The
           fixtures stay; only the group (its head layout) is removed. */
        if (fg != nullptr)
        {
            menu.addSeparator();
            const QString name = fg->name();
            menu.addAction(tr("Delete Fixture Group “%1”…").arg(name), this, [this, id, name]() {
                if (QMessageBox::question(this, tr("Delete Fixture Group"),
                        tr("Delete the fixture group “%1”?\n\nIts fixtures are not deleted -- "
                           "only the group and its head layout. Anything set up to use this "
                           "group will no longer find it.").arg(name),
                        QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
                    return;
                m_doc->deleteFixtureGroup(id);
                m_doc->setModified();
                reload();
            });
        }
    }
    else if (type == NodeLayer && m_editable)
    {
        menu.addAction(tr("Rename Layer…"), this, [this, item]() {
            m_tree->editItem(item, 0);
        });
        menu.addAction(tr("New Layer"), this, &MonitorLayersPanel::slotAddLayer);
        if (id != MonitorProperties::defaultLayerId)
            menu.addAction(tr("Delete Layer"), this, &MonitorLayersPanel::slotRemoveLayer);
    }

    m_view->populateOrganiseMenu(&menu, place.selection);
}

bool MonitorLayersPanel::revealItem(const QString &kind, quint32 id)
{
    QTreeWidgetItemIterator it(m_tree);
    for (; *it != nullptr; ++it)
    {
        QTreeWidgetItem *row = *it;
        const int type = row->data(0, NodeTypeRole).toInt();
        const quint32 rid = row->data(0, NodeIdRole).toUInt();
        bool match = (type == NodeItem && rid == id
                      && row->data(0, NodeKindRole).toString() == kind);
        if (!match && type == NodeGroup)
        {
            const MonitorProperties::MonitorGroup g = m_props->group(rid);
            match = (g.anchorKind == kind && g.anchorId == id);
        }
        if (!match)
            continue;
        for (QTreeWidgetItem *up = row->parent(); up != nullptr; up = up->parent())
            up->setExpanded(true);
        m_tree->clearSelection();
        row->setSelected(true);
        m_tree->setCurrentItem(row);
        m_tree->scrollToItem(row, QAbstractItemView::PositionAtCenter);
        return true;
    }
    return false;
}

void MonitorLayersPanel::selectFixtureGroups(const QList<quint32> &ids)
{
    QSet<quint32> want(ids.begin(), ids.end());
    m_syncingSelection = true;          // one hand-off to the plot, at the end
    m_tree->clearSelection();
    QTreeWidgetItem *first = nullptr;
    for (QTreeWidgetItemIterator it(m_tree); *it != nullptr; ++it)
    {
        QTreeWidgetItem *row = *it;
        if (row->data(0, NodeTypeRole).toInt() != NodeFixtureGroup)
            continue;
        const quint32 fgId = row->data(0, NodeIdRole).toUInt();
        if (!want.remove(fgId))
            continue;                   // not wanted, or already selected elsewhere
        for (QTreeWidgetItem *up = row->parent(); up != nullptr; up = up->parent())
            up->setExpanded(true);
        row->setSelected(true);
        if (first == nullptr)
            first = row;
    }
    if (first != nullptr)
    {
        m_tree->setCurrentItem(first, 0, QItemSelectionModel::NoUpdate);
        m_tree->scrollToItem(first);
    }
    if (m_view != nullptr)
        m_view->selectMapItems(selectedObjects());
    m_syncingSelection = false;
    updateButtons();
}

void MonitorLayersPanel::followMapSelection(const QList<QPair<QString, quint32> > &objects)
{
    if (m_syncingSelection)
        return;                         // the plot is following US
    QSet<QString> want;
    for (const QPair<QString, quint32> &o : objects)
        want.insert(o.first + QLatin1Char(':') + QString::number(o.second));

    m_syncingSelection = true;
    m_tree->clearSelection();
    QTreeWidgetItem *first = nullptr;
    for (QTreeWidgetItemIterator it(m_tree); *it != nullptr; ++it)
    {
        QTreeWidgetItem *row = *it;
        const int type = row->data(0, NodeTypeRole).toInt();
        QString key;
        if (type == NodeItem)
            key = row->data(0, NodeKindRole).toString() + QLatin1Char(':')
                  + QString::number(row->data(0, NodeIdRole).toUInt());
        else if (type == NodeGroup)
        {
            // A structure shown AS its group ("T-1 · on truss") is that row.
            const MonitorProperties::MonitorGroup g = m_props->group(row->data(0, NodeIdRole).toUInt());
            if (!g.anchorKind.isEmpty())
                key = g.anchorKind + QLatin1Char(':') + QString::number(g.anchorId);
        }
        if (key.isEmpty() || !want.contains(key))
            continue;
        for (QTreeWidgetItem *up = row->parent(); up != nullptr; up = up->parent())
            up->setExpanded(true);
        row->setSelected(true);
        if (first == nullptr)
            first = row;
    }
    if (first != nullptr)
        m_tree->scrollToItem(first);
    m_syncingSelection = false;
    updateButtons();
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
    // Default and the stage pieces' layer stay.
    if (id == MonitorProperties::defaultLayerId || id == m_props->stageLayerId())
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
