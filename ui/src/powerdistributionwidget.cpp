/*
  Q Light Controller Plus
  powerdistributionwidget.cpp

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

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTreeWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QSplitter>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLabel>
#include <QMenu>
#include <QTimer>
#include <QSettings>
#include <QBrush>
#include <QColor>
#include <QSet>
#include <QMimeData>
#include <QDataStream>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QStyledItemDelegate>
#include <QComboBox>
#include <algorithm>

#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>

#include "powerdistributionwidget.h"
#include "powerdistribution.h"
#include "venue.h"
#include "inputoutputmap.h"
#include "universe.h"
#include "fixture.h"
#include "function.h"
#include "scene.h"
#include "doc.h"

#define KDefaultVoltageKey    "power/defaultVoltage"
#define KDefaultDerateKey     "power/deratePercent"
#define KDefaultBreakerKey    "power/defaultBreakerAmps"
#define KDefaultDemandKey     "power/demandPercent"
#define KPowerFactorKey       "power/powerFactor"

// Item data roles on the sources/circuits tree
enum { RoleType = Qt::UserRole, RoleSource, RoleCircuit, RoleFixtureId,
       RoleConnector, RoleSourceType };
enum { TypeSource = 0, TypeCircuit = 1, TypeFixture = 2 };

// Sources/circuits tree columns
enum { ColName = 0, ColType = 1, ColVolts = 2, ColAmps = 3, ColDemand = 4,
       ColConnector = 5, ColLoad = 6, ColMax = 7 };
enum { ColumnCount = 8 };

static const QColor kOverColor(192, 57, 43);
static const QColor kWarnColor(211, 130, 30);
// Subtle lightening so circuit (and wall-socket) rows read as a band under the
// darker source rows. Alpha overlay works in both light and dark themes.
static const QColor kCircuitBg(255, 255, 255, 20);

/** Human-readable source-type name for the Type column / dropdown. */
static QString sourceTypeName(int t)
{
    switch (t)
    {
        case PowerSource::WallSocket:  return QObject::tr("Wall socket");
        case PowerSource::BreakoutBox: return QObject::tr("Breakout box");
        case PowerSource::Battery:     return QObject::tr("Battery / UPS");
        default:                       return QObject::tr("Distro");
    }
}

// Same drag payload the Fixture Manager's tree produces: a QDataStream of the
// dragged fixtures' quint32 ids (see fixturetreewidget.cpp).
static const char *FIXTURE_DRAG_MIME_TYPE = "application/x-qlcplus-fixtures";

/** Resolve the circuit a tree item belongs to (the circuit row itself, or the
 *  circuit a nested fixture row hangs under). Returns false for source rows. */
static bool circuitIndicesForItem(QTreeWidgetItem *it, int &s, int &c)
{
    if (it == NULL)
        return false;
    const int type = it->data(0, RoleType).toInt();
    if (type == TypeCircuit)
    {
        s = it->data(0, RoleSource).toInt();
        c = it->data(0, RoleCircuit).toInt();
        return true;
    }
    // A wall-socket source is itself the target (its implicit circuit index is
    // stored in RoleCircuit); other source rows are not drop targets.
    if (type == TypeSource && it->data(0, RoleCircuit).isValid())
    {
        s = it->data(0, RoleSource).toInt();
        c = it->data(0, RoleCircuit).toInt();
        return true;
    }
    if (type == TypeFixture)
    {
        QTreeWidgetItem *p = it->parent();
        if (p != NULL && circuitIndicesForItem(p, s, c))
            return true;
    }
    return false;
}

/**
 * Combobox editor for the Connector column: pick the receptacle a circuit
 * provides from common stage-power types, or "(auto)" to fall back to the
 * voltage/amps-based suggestion. Stores the chosen type (empty == auto) in the
 * item's RoleConnector data; the widget copies it into the model on commit.
 */
class ConnectorDelegate : public QStyledItemDelegate
{
public:
    explicit ConnectorDelegate(QObject *parent = NULL) : QStyledItemDelegate(parent) {}

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &index) const override
    {
        // Circuits (output receptacle) and sources (input feed) both carry a
        // connector; fixture rows do not.
        const int rt = index.sibling(index.row(), 0).data(RoleType).toInt();
        if (rt != TypeCircuit && rt != TypeSource)
            return NULL;

        QComboBox *cb = new QComboBox(parent);
        cb->setAutoFillBackground(true); // paint over the cell text underneath
        cb->setEditable(true);           // allow a type not in the list
        cb->addItem(tr("(auto)"), QString());
        static const char *types[] = {
            "Edison 5-15", "Edison 5-20", "L5-20", "L5-30", "L6-20", "L6-30",
            "L14-30", "L21-30", "14-50", "CS6364", "CS6365", "Cam-Lok",
            "powerCON", "powerCON TRUE1", "Socapex 19-pin", "Bare ends", NULL };
        for (int i = 0; types[i] != NULL; i++)
            cb->addItem(QString::fromLatin1(types[i]), QString::fromLatin1(types[i]));

        ConnectorDelegate *self = const_cast<ConnectorDelegate *>(this);
        connect(cb, QOverload<int>::of(&QComboBox::activated), self,
                [self, cb](int) { emit self->commitData(cb); emit self->closeEditor(cb); });
        return cb;
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        QComboBox *cb = qobject_cast<QComboBox *>(editor);
        if (cb == NULL)
            return;
        const QString ov = index.data(RoleConnector).toString();
        const int i = cb->findData(ov);
        if (i >= 0)
            cb->setCurrentIndex(i);
        else if (ov.isEmpty())
            cb->setCurrentIndex(0);
        else
            cb->setEditText(ov);
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        QComboBox *cb = qobject_cast<QComboBox *>(editor);
        if (cb == NULL)
            return;
        // Index 0 is "(auto)"; anything else is the typed/selected connector.
        const QString val = (cb->currentIndex() == 0) ? QString() : cb->currentText().trimmed();
        model->setData(index, val, RoleConnector);
    }
};

/**
 * Dropdown for the source Type column: how the source is wired (distro, wall
 * socket, breakout box, battery/UPS). Stores the SourceType int in RoleSourceType.
 */
class SourceTypeDelegate : public QStyledItemDelegate
{
public:
    explicit SourceTypeDelegate(QObject *parent = NULL) : QStyledItemDelegate(parent) {}

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &index) const override
    {
        if (index.sibling(index.row(), 0).data(RoleType).toInt() != TypeSource)
            return NULL;   // only sources have a type
        QComboBox *cb = new QComboBox(parent);
        cb->setAutoFillBackground(true); // paint over the cell text underneath
        const int types[] = { PowerSource::Distro, PowerSource::WallSocket,
                              PowerSource::BreakoutBox, PowerSource::Battery };
        for (int i = 0; i < 4; i++)
            cb->addItem(sourceTypeName(types[i]), types[i]);
        SourceTypeDelegate *self = const_cast<SourceTypeDelegate *>(this);
        connect(cb, QOverload<int>::of(&QComboBox::activated), self,
                [self, cb](int) { emit self->commitData(cb); emit self->closeEditor(cb); });
        return cb;
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        QComboBox *cb = qobject_cast<QComboBox *>(editor);
        if (cb == NULL)
            return;
        const int i = cb->findData(index.data(RoleSourceType).toInt());
        cb->setCurrentIndex(i >= 0 ? i : 0);
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        QComboBox *cb = qobject_cast<QComboBox *>(editor);
        if (cb != NULL)
            model->setData(index, cb->currentData().toInt(), RoleSourceType);
    }
};

PowerCircuitTree::PowerCircuitTree(QWidget *parent)
    : QTreeWidget(parent)
{
    // Accepts fixtures dragged in from the Fixture Manager tree, and lets its own
    // assigned-fixture rows be dragged onto another circuit to move them.
    setDragDropMode(QAbstractItemView::DragDrop);
    setDragEnabled(true);
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
    setDropIndicatorShown(true);
    // We apply the "move" ourselves (reassign); don't let the view delete rows.
    setDefaultDropAction(Qt::CopyAction);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
QMimeData *PowerCircuitTree::mimeData(const QList<QTreeWidgetItem *> &items) const
#else
QMimeData *PowerCircuitTree::mimeData(const QList<QTreeWidgetItem *> items) const
#endif
{
    QByteArray fxData;
    QDataStream stream(&fxData, QIODevice::WriteOnly);
    int count = 0;
    foreach (QTreeWidgetItem *it, items)
    {
        if (it->data(0, RoleType).toInt() != TypeFixture)
            continue;
        stream << it->data(0, RoleFixtureId).toUInt();
        count++;
    }
    if (count == 0)
        return QTreeWidget::mimeData(items);

    QMimeData *mime = new QMimeData();
    mime->setData(FIXTURE_DRAG_MIME_TYPE, fxData);
    return mime;
}

void PowerCircuitTree::keyPressEvent(QKeyEvent *event)
{
    // Enter/Return/F2 → inline rename of the current row's NAME column (sources
    // and circuits are ItemIsEditable; slotTreeItemChanged persists it).
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter
         || event->key() == Qt::Key_F2)
        && state() != QAbstractItemView::EditingState)
    {
        QTreeWidgetItem *it = currentItem();
        if (it != NULL && (it->flags() & Qt::ItemIsEditable)
            && selectedItems().size() <= 1)
        {
            editItem(it, ColName);   // ColName == 0
            event->accept();
            return;
        }
    }
    QTreeWidget::keyPressEvent(event);
}

void PowerCircuitTree::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(FIXTURE_DRAG_MIME_TYPE))
        event->acceptProposedAction();
    else
        QTreeWidget::dragEnterEvent(event);
}

void PowerCircuitTree::dragMoveEvent(QDragMoveEvent *event)
{
    int s, c;
    if (event->mimeData()->hasFormat(FIXTURE_DRAG_MIME_TYPE)
            && circuitIndicesForItem(itemAt(event->pos()), s, c))
        event->acceptProposedAction();
    else
        event->ignore();
}

void PowerCircuitTree::dropEvent(QDropEvent *event)
{
    int s, c;
    if (event->mimeData()->hasFormat(FIXTURE_DRAG_MIME_TYPE) == false
            || circuitIndicesForItem(itemAt(event->pos()), s, c) == false)
    {
        event->ignore();
        return;
    }

    QByteArray data = event->mimeData()->data(FIXTURE_DRAG_MIME_TYPE);
    QDataStream stream(&data, QIODevice::ReadOnly);
    QList<quint32> ids;
    while (stream.atEnd() == false)
    {
        quint32 id;
        stream >> id;
        ids.append(id);
    }
    if (ids.isEmpty() == false)
        emit fixturesDroppedOnCircuit(s, c, ids);
    event->acceptProposedAction();
}

/** Rough US connector suggestion from voltage + breaker rating, so the user
 *  knows what plug a circuit needs. Best-effort hint, not exhaustive. */
static QString suggestedConnector(double volts, double amps)
{
    const int v = int(volts + 0.5);
    const int a = int(amps + 0.5);
    if (v >= 100 && v <= 130) // 120 V leg
    {
        if (a <= 15) return QStringLiteral("Edison 5-15");
        if (a <= 20) return QStringLiteral("Edison 5-20 / L5-20");
        if (a <= 30) return QStringLiteral("L5-30");
        if (a <= 50) return QStringLiteral("CS6364 / 14-50");
    }
    else if (v >= 200 && v <= 250) // 208/240 V
    {
        if (a <= 20) return QStringLiteral("L6-20");
        if (a <= 30) return QStringLiteral("L6-30 / L14-30");
        if (a <= 50) return QStringLiteral("CS6365 / 14-50");
    }
    return QString();
}

PowerDistributionWidget::PowerDistributionWidget(Doc *doc, bool withFixtureList, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_withFixtureList(withFixtureList)
    , m_fixtureList(NULL)
    , m_assignBtn(NULL)
    , m_unassignBtn(NULL)
{
    QVBoxLayout *top = new QVBoxLayout(this);
    top->setContentsMargins(0, 0, 0, 0);

    // --- Top row: workspace-wide diversity/demand default ---
    QHBoxLayout *demandRow = new QHBoxLayout();
    QLabel *demandLabel = new QLabel(tr("Default demand factor:"), this);
    m_demandSpin = new QSpinBox(this);
    m_demandSpin->setRange(10, 100);
    m_demandSpin->setSuffix(tr(" %"));
    m_demandSpin->setValue(defaultDemandPercent());
    m_demandSpin->setToolTip(tr("Share of a circuit's connected (all-on) load to "
                                "plan around, i.e. how much is lit at once:\n"
                                "  100%% = assume every fixture can be on together "
                                "(design load = max load; no diversity)\n"
                                "  70%%  = plan for 70%% of the connected load\n"
                                "Design load = max load x this factor. Circuits can "
                                "override it in the Demand column."));
    // Live hint so the meaning of the current value is obvious at a glance.
    m_demandHint = new QLabel(this);
    m_demandHint->setStyleSheet(QStringLiteral("color: gray;"));
    demandRow->addWidget(demandLabel);
    demandRow->addWidget(m_demandSpin);
    demandRow->addWidget(m_demandHint);
    demandRow->addStretch(1);
    top->addLayout(demandRow);
    updateDemandHint();

    // --- Left: sources / circuits ---
    QWidget *left = new QWidget(this);
    QVBoxLayout *lv = new QVBoxLayout(left);
    lv->setContentsMargins(0, 0, 0, 0);
    lv->addWidget(new QLabel(tr("Power sources & circuits"), left));

    m_tree = new PowerCircuitTree(left);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setColumnCount(ColumnCount);
    m_tree->setHeaderLabels(QStringList()
                            << tr("Name") << tr("Type") << tr("Voltage") << tr("Rated A")
                            << tr("Demand") << tr("Connector")
                            << tr("Design load") << tr("Max load"));
    m_tree->headerItem()->setToolTip(ColConnector,
        tr("On a source: the feed INTO it (power for the source). "
           "On a circuit: the receptacle it provides (power from the source)."));
    m_tree->headerItem()->setToolTip(ColLoad,
        tr("Design load = max load x demand factor, vs the derated breaker limit. "
           "▲ marks the amount over the limit."));
    m_tree->headerItem()->setToolTip(ColMax,
        tr("Max load = every fixture on the circuit at full intensity (worst case)."));
    // Name takes the slack; the numeric/connector columns hug their contents so
    // the widest, most-read column is the circuit name.
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    for (int col = ColType; col <= ColMax; col++)
        m_tree->header()->setSectionResizeMode(col, QHeaderView::ResizeToContents);
    m_tree->setRootIsDecorated(true);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    // Dropdowns: source wiring Type, and the connector (source feed / circuit receptacle).
    m_tree->setItemDelegateForColumn(ColType, new SourceTypeDelegate(m_tree));
    m_tree->setItemDelegateForColumn(ColConnector, new ConnectorDelegate(m_tree));
    lv->addWidget(m_tree, 1);

    QHBoxLayout *lb = new QHBoxLayout();
    m_addSourceBtn = new QPushButton(tr("Add source"), left);
    m_addCircuitBtn = new QPushButton(tr("Add circuit"), left);
    m_removeBtn = new QPushButton(tr("Remove"), left);
    m_autoBtn = new QPushButton(tr("Auto-assign…"), left);
    m_autoBtn->setToolTip(tr("Distribute all fixtures across circuits, heaviest "
                             "first, packing each up to its derated limit."));
    lb->addWidget(m_addSourceBtn);
    lb->addWidget(m_addCircuitBtn);
    lb->addWidget(m_removeBtn);
    lb->addWidget(m_autoBtn);
    lb->addStretch(1);
    m_sceneCheckBtn = new QPushButton(tr("Power Check…"), left);
    m_sceneCheckBtn->setToolTip(tr("Report every scene and collection's load on "
                                   "each circuit, flagging overloads."));
    lb->addWidget(m_sceneCheckBtn);
    lv->addLayout(lb);

    // Venue import/export — circuit infrastructure portable across shows (no
    // fixture assignments). Kept here while Power is the only venue section.
    QHBoxLayout *vb = new QHBoxLayout();
    m_importBtn = new QPushButton(tr("Import Venue…"), left);
    m_exportBtn = new QPushButton(tr("Export Venue…"), left);
    vb->addStretch(1);
    vb->addWidget(m_importBtn);
    vb->addWidget(m_exportBtn);
    lv->addLayout(vb);

    // UPS / battery properties for the selected source. A non-zero VA rating
    // turns a source into a UPS: its load is checked against the VA capacity and
    // runtime is estimated from the datasheet point (minutes @ watts).
    m_sourceBox = new QGroupBox(tr("Source properties"), left);
    QFormLayout *sf = new QFormLayout(m_sourceBox);

    // Source wiring type — same choice as the Type column, editable here too.
    m_typeCombo = new QComboBox(m_sourceBox);
    {
        const int types[] = { PowerSource::Distro, PowerSource::WallSocket,
                              PowerSource::BreakoutBox, PowerSource::Battery };
        for (int i = 0; i < 4; i++)
            m_typeCombo->addItem(sourceTypeName(types[i]), types[i]);
    }
    sf->addRow(tr("Type:"), m_typeCombo);

    m_vaSpin = new QDoubleSpinBox(m_sourceBox);
    m_vaSpin->setRange(0, 1000000);
    m_vaSpin->setDecimals(0);
    m_vaSpin->setSuffix(tr(" VA"));
    m_vaSpin->setSpecialValueText(tr("— mains (no UPS) —")); // shown at 0
    sf->addRow(tr("VA rating:"), m_vaSpin);

    QWidget *runRow = new QWidget(m_sourceBox);
    QHBoxLayout *rl = new QHBoxLayout(runRow);
    rl->setContentsMargins(0, 0, 0, 0);
    m_runMinSpin = new QDoubleSpinBox(runRow);
    m_runMinSpin->setRange(0, 100000);
    m_runMinSpin->setDecimals(0);
    m_runMinSpin->setSuffix(tr(" min"));
    m_runWattSpin = new QDoubleSpinBox(runRow);
    m_runWattSpin->setRange(0, 1000000);
    m_runWattSpin->setDecimals(0);
    m_runWattSpin->setSuffix(tr(" W"));
    rl->addWidget(m_runMinSpin);
    rl->addWidget(new QLabel(tr("at"), runRow));
    rl->addWidget(m_runWattSpin);
    rl->addStretch(1);
    sf->addRow(tr("Rated runtime:"), runRow);

    m_sourceSummary = new QLabel(m_sourceBox);
    m_sourceSummary->setWordWrap(true);
    sf->addRow(m_sourceSummary);

    lv->addWidget(m_sourceBox);

    if (m_withFixtureList)
    {
        // Standalone dialog: no fixture tree nearby, so provide our own list +
        // Assign buttons alongside the sources/circuits tree.
        QSplitter *split = new QSplitter(Qt::Horizontal, this);
        split->addWidget(left);

        QWidget *right = new QWidget(this);
        QVBoxLayout *rv = new QVBoxLayout(right);
        rv->setContentsMargins(0, 0, 0, 0);
        rv->addWidget(new QLabel(tr("Fixtures"), right));

        m_fixtureList = new QTreeWidget(right);
        m_fixtureList->setColumnCount(3);
        m_fixtureList->setHeaderLabels(QStringList()
                                       << tr("Fixture") << tr("Circuit") << tr("Watts"));
        m_fixtureList->setRootIsDecorated(false);
        m_fixtureList->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_fixtureList->header()->setStretchLastSection(true);
        m_fixtureList->setContextMenuPolicy(Qt::CustomContextMenu);
        rv->addWidget(m_fixtureList, 1);

        QHBoxLayout *rb = new QHBoxLayout();
        m_assignBtn = new QPushButton(tr("Assign to selected circuit"), right);
        m_unassignBtn = new QPushButton(tr("Unassign"), right);
        rb->addWidget(m_assignBtn);
        rb->addWidget(m_unassignBtn);
        rb->addStretch(1);
        rv->addLayout(rb);
        split->addWidget(right);

        split->setStretchFactor(0, 5);
        split->setStretchFactor(1, 3);
        top->addWidget(split, 1);
    }
    else
    {
        // Embedded next to a fixture tree (Fixture Manager): assignment is driven
        // from that tree's context menu, and each circuit already lists its
        // fixtures, so no separate list is needed here.
        top->addWidget(left, 1);
    }

    // --- Footer: running totals across the whole rig ---
    m_footer = new QLabel(this);
    m_footer->setWordWrap(true);
    m_footer->setTextFormat(Qt::RichText);
    top->addWidget(m_footer);

    connect(m_demandSpin, SIGNAL(valueChanged(int)), this, SLOT(slotDefaultDemandChanged(int)));
    connect(m_addSourceBtn, &QPushButton::clicked, this, &PowerDistributionWidget::slotAddSource);
    connect(m_addCircuitBtn, &QPushButton::clicked, this, &PowerDistributionWidget::slotAddCircuit);
    connect(m_removeBtn, &QPushButton::clicked, this, &PowerDistributionWidget::slotRemoveSelected);
    connect(m_tree, &QTreeWidget::itemSelectionChanged,
            this, &PowerDistributionWidget::slotTreeSelectionChanged);
    connect(m_tree, &QTreeWidget::itemChanged,
            this, &PowerDistributionWidget::slotTreeItemChanged);
    connect(m_tree, &QWidget::customContextMenuRequested,
            this, &PowerDistributionWidget::slotTreeContextMenu);
    connect(m_tree, &PowerCircuitTree::fixturesDroppedOnCircuit,
            this, &PowerDistributionWidget::slotFixturesDroppedOnCircuit);
    if (m_withFixtureList)
    {
        connect(m_assignBtn, &QPushButton::clicked, this, &PowerDistributionWidget::slotAssign);
        connect(m_unassignBtn, &QPushButton::clicked, this, &PowerDistributionWidget::slotUnassign);
        connect(m_fixtureList, &QWidget::customContextMenuRequested,
                this, &PowerDistributionWidget::slotFixtureContextMenu);
    }
    connect(m_autoBtn, &QPushButton::clicked, this, &PowerDistributionWidget::slotAutoAssign);
    connect(m_sceneCheckBtn, &QPushButton::clicked, this, &PowerDistributionWidget::slotSceneCheck);
    connect(m_importBtn, &QPushButton::clicked, this, &PowerDistributionWidget::slotImportVenue);
    connect(m_exportBtn, &QPushButton::clicked, this, &PowerDistributionWidget::slotExportVenue);
    connect(m_typeCombo, SIGNAL(activated(int)), this, SLOT(slotSourceTypeChanged()));
    connect(m_vaSpin, SIGNAL(valueChanged(double)), this, SLOT(slotSourceUpsChanged()));
    connect(m_runMinSpin, SIGNAL(valueChanged(double)), this, SLOT(slotSourceUpsChanged()));
    connect(m_runWattSpin, SIGNAL(valueChanged(double)), this, SLOT(slotSourceUpsChanged()));

    // Coalesce bursts of fixture edits (patching a group hits many signals) into
    // a single rebuild on the next event-loop pass.
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(150);
    connect(m_refreshTimer, &QTimer::timeout, this, &PowerDistributionWidget::refresh);
    connect(m_doc, SIGNAL(fixtureAdded(quint32)), this, SLOT(slotFixturesChanged()));
    connect(m_doc, SIGNAL(fixtureRemoved(quint32)), this, SLOT(slotFixturesChanged()));
    connect(m_doc, SIGNAL(fixtureChanged(quint32)), this, SLOT(slotFixturesChanged()));

    computeFixtureWatts();
    rebuildTree();
    rebuildFixtures();
    updateFooter();
    slotTreeSelectionChanged();
}

PowerDistributionWidget::~PowerDistributionWidget()
{
}

int PowerDistributionWidget::defaultDemandPercent() const
{
    QSettings settings;
    int v = settings.value(KDefaultDemandKey, 100).toInt();
    return (v >= 10 && v <= 100) ? v : 100;
}

void PowerDistributionWidget::showEvent(QShowEvent *ev)
{
    QWidget::showEvent(ev);
    refresh();
}

void PowerDistributionWidget::slotFixturesChanged()
{
    if (!m_refreshTimer->isActive())
        m_refreshTimer->start();
}

void PowerDistributionWidget::refresh()
{
    computeFixtureWatts();
    rebuildTree();
    rebuildFixtures();
    updateFooter();
    slotTreeSelectionChanged();
}

void PowerDistributionWidget::computeFixtureWatts()
{
    m_fixtureWatts.clear();
    QList<Universe*> universes = m_doc->inputOutputMap()->claimUniverses();
    PowerEstimator::PreGMReader reader =
        [&universes](quint32 uni, quint32 addr) -> uchar
        {
            if (uni >= quint32(universes.size()))
                return 0;
            return universes[uni]->preGMValue(int(addr));
        };
    PowerEstimator::estimateWatts(m_doc, reader, &m_fixtureWatts);
    m_doc->inputOutputMap()->releaseUniverses(false);

    // Full-rated draw per fixture (everything at full) — for the worst-case
    // "would this trip if everything went full" check. No universe read needed.
    m_fixtureFullWatts.clear();
    foreach (Fixture *fx, m_doc->fixtures())
        if (fx != NULL)
            m_fixtureFullWatts.insert(fx->id(), PowerEstimator::fixtureFullWatts(fx));
}

double PowerDistributionWidget::circuitAmpsFrom(int sourceIdx, int circuitIdx,
                                                const QHash<quint32, double> &watts) const
{
    PowerDistribution *pd = m_doc->powerDistribution();
    const QList<PowerSource> &srcs = pd->sources();
    if (sourceIdx < 0 || sourceIdx >= srcs.size())
        return 0.0;
    const PowerSource &src = srcs.at(sourceIdx);
    if (circuitIdx < 0 || circuitIdx >= src.circuits.size())
        return 0.0;

    QSettings settings;
    const double dflt = settings.value(KDefaultVoltageKey, 120.0).toDouble();
    const double srcV = (src.voltage > 0.0) ? src.voltage : dflt;
    const PowerCircuit &cir = src.circuits.at(circuitIdx);
    const double v = cir.effectiveVoltage(srcV);

    double w = 0.0;
    foreach (quint32 fxId, cir.fixtures)
        w += watts.value(fxId, 0.0);
    return w / v;
}

double PowerDistributionWidget::circuitAmps(int sourceIdx, int circuitIdx) const
{
    return circuitAmpsFrom(sourceIdx, circuitIdx, m_fixtureWatts);
}

double PowerDistributionWidget::circuitFullAmps(int sourceIdx, int circuitIdx) const
{
    return circuitAmpsFrom(sourceIdx, circuitIdx, m_fixtureFullWatts);
}

double PowerDistributionWidget::circuitDesignAmps(int sourceIdx, int circuitIdx) const
{
    PowerDistribution *pd = m_doc->powerDistribution();
    const QList<PowerSource> &srcs = pd->sources();
    if (sourceIdx < 0 || sourceIdx >= srcs.size())
        return 0.0;
    const PowerSource &src = srcs.at(sourceIdx);
    if (circuitIdx < 0 || circuitIdx >= src.circuits.size())
        return 0.0;
    const int demand = src.circuits.at(circuitIdx).effectiveDemandPercent(defaultDemandPercent());
    return circuitFullAmps(sourceIdx, circuitIdx) * demand / 100.0;
}

double PowerDistributionWidget::sourceWatts(int sourceIdx) const
{
    PowerDistribution *pd = m_doc->powerDistribution();
    if (sourceIdx < 0 || sourceIdx >= pd->sources().size())
        return 0.0;
    double w = 0.0;
    foreach (const PowerCircuit &cir, pd->sources().at(sourceIdx).circuits)
        foreach (quint32 fxId, cir.fixtures)
            w += m_fixtureWatts.value(fxId, 0.0);
    return w;
}

void PowerDistributionWidget::refreshSourcePanel()
{
    QTreeWidgetItem *it = m_tree->currentItem();
    int s = -1;
    if (it != NULL && it->data(0, RoleType).toInt() == TypeSource)
        s = it->data(0, RoleSource).toInt();
    else if (it != NULL && it->data(0, RoleType).toInt() == TypeCircuit)
        s = it->data(0, RoleSource).toInt(); // editing the circuit's parent source

    m_panelSource = s;
    PowerDistribution *pd = m_doc->powerDistribution();
    const bool valid = (s >= 0 && s < pd->sources().size());
    m_sourceBox->setEnabled(valid);

    m_panelUpdating = true;
    if (valid)
    {
        const PowerSource &src = pd->sources().at(s);
        m_sourceBox->setTitle(tr("Source — %1").arg(src.name));

        const int ti = m_typeCombo->findData(src.type);
        m_typeCombo->setCurrentIndex(ti >= 0 ? ti : 0);

        // VA / runtime only apply to a battery/UPS source.
        const bool battery = (src.type == PowerSource::Battery);
        m_vaSpin->setEnabled(battery);
        m_runMinSpin->setEnabled(battery);
        m_runWattSpin->setEnabled(battery);
        m_vaSpin->setValue(src.vaRating);
        m_runMinSpin->setValue(src.runtimeMinutes);
        m_runWattSpin->setValue(src.runtimeWatts);

        const double w = sourceWatts(s);
        QSettings settings;
        const double pf = settings.value(KPowerFactorKey, 0.9).toDouble();
        const double va = (pf > 0.0) ? w / pf : w;
        QString txt = tr("Load: %1 kW / %2 kVA (PF %3)")
                .arg(w / 1000.0, 0, 'f', 2).arg(va / 1000.0, 0, 'f', 2)
                .arg(pf, 0, 'f', 2);
        if (src.isUPS())
        {
            const double rt = src.estimateRuntimeMinutes(w);
            if (va > src.vaRating)
                txt += tr("  ⚠ exceeds %1 VA").arg(src.vaRating, 0, 'f', 0);
            txt += rt >= 0.0 ? tr("\nEstimated runtime: ~%1 min").arg(rt, 0, 'f', 0)
                             : tr("\nEstimated runtime: set a rated-runtime point below");
        }
        m_sourceSummary->setText(txt);
    }
    else
    {
        m_sourceBox->setTitle(tr("Source properties"));
        m_typeCombo->setCurrentIndex(0);
        m_vaSpin->setValue(0);
        m_runMinSpin->setValue(0);
        m_runWattSpin->setValue(0);
        m_sourceSummary->setText(tr("Select a source to edit its type and limits."));
    }
    m_panelUpdating = false;
}

void PowerDistributionWidget::slotSourceTypeChanged()
{
    if (m_panelUpdating)
        return;
    PowerDistribution *pd = m_doc->powerDistribution();
    if (m_panelSource < 0 || m_panelSource >= pd->sources().size())
        return;
    const int s = m_panelSource;

    pd->sources()[s].type = m_typeCombo->currentData().toInt();
    applySourceType(s);
    m_doc->setModified();
    refresh();

    // refresh() rebuilt the tree and cleared the selection; reselect this source
    // so the panel stays on it.
    for (int i = 0; i < m_tree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *si = m_tree->topLevelItem(i);
        if (si->data(0, RoleSource).toInt() == s)
        {
            m_tree->setCurrentItem(si);
            break;
        }
    }
}

void PowerDistributionWidget::slotSourceUpsChanged()
{
    if (m_panelUpdating)
        return;
    PowerDistribution *pd = m_doc->powerDistribution();
    if (m_panelSource < 0 || m_panelSource >= pd->sources().size())
        return;

    pd->sources()[m_panelSource].vaRating = m_vaSpin->value();
    pd->sources()[m_panelSource].runtimeMinutes = m_runMinSpin->value();
    pd->sources()[m_panelSource].runtimeWatts = m_runWattSpin->value();
    m_doc->setModified();

    // Refresh the source row's summary column + the panel's live readout.
    m_tree->blockSignals(true);
    for (int i = 0; i < m_tree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *si = m_tree->topLevelItem(i);
        if (si->data(0, RoleSource).toInt() == m_panelSource)
        {
            updateSourceRow(si, m_panelSource);
            break;
        }
    }
    m_tree->blockSignals(false);
    refreshSourcePanel();
}

void PowerDistributionWidget::updateDemandHint()
{
    const int p = m_demandSpin->value();
    m_demandHint->setText(p >= 100
        ? tr("= all fixtures on at once (no diversity)")
        : tr("= plan for %1%% of connected load").arg(p));
}

void PowerDistributionWidget::slotDefaultDemandChanged(int percent)
{
    QSettings settings;
    settings.setValue(KDefaultDemandKey, percent);
    updateDemandHint();
    // Ripples into every circuit that inherits the default; rebuild to re-render.
    rebuildTree();
    updateFooter();
}

void PowerDistributionWidget::renderCircuitLoadCells(QTreeWidgetItem *it, int s, int c,
                                                     bool sourceRow)
{
    PowerDistribution *pd = m_doc->powerDistribution();
    const PowerSource &src = pd->sources().at(s);
    const PowerCircuit &cir = src.circuits.at(c);
    const double v = cir.effectiveVoltage(src.voltage);
    const int demand = cir.effectiveDemandPercent(defaultDemandPercent());

    // Voltage — italic when a circuit inherits it from its source (not for the
    // wall-socket source row, whose voltage is the source's own).
    it->setText(ColVolts, QString::number(v, 'f', 0));
    QFont vf = it->font(ColVolts);
    vf.setItalic(!sourceRow && cir.voltage <= 0.0);
    it->setFont(ColVolts, vf);
    it->setToolTip(ColVolts, (!sourceRow && cir.voltage <= 0.0)
                   ? tr("Inherited from source (edit to override; clear to re-inherit)")
                   : QString());

    it->setText(ColAmps, QString::number(cir.ratedAmps, 'f', 0));

    it->setText(ColDemand, tr("%1%").arg(demand));
    QFont df = it->font(ColDemand);
    df.setItalic(!sourceRow && cir.demandPercent <= 0);
    it->setFont(ColDemand, df);
    it->setToolTip(ColDemand, tr("Design load = max load x this factor."));

    const double limit = cir.deratedLimit();
    const double full = circuitFullAmps(s, c);
    const double design = full * demand / 100.0;
    const double live = circuitAmps(s, c);

    const bool over = design > limit + 1e-6;
    QString loadTxt = tr("%1 / %2 A").arg(design, 0, 'f', 1).arg(limit, 0, 'f', 1);
    if (over)
        loadTxt += tr("  ▲ %1 A").arg(design - limit, 0, 'f', 1);
    it->setText(ColLoad, loadTxt);
    it->setToolTip(ColLoad, tr("Design load %1 A (max %2 A x %3%% demand) vs %4 A "
                               "derated limit (%5 A x %6%%). Live now: %7 A.")
                   .arg(design, 0, 'f', 1).arg(full, 0, 'f', 1).arg(demand)
                   .arg(limit, 0, 'f', 1).arg(cir.ratedAmps, 0, 'f', 0)
                   .arg(cir.deratePercent).arg(live, 0, 'f', 1));

    const bool fullOver = full > limit + 1e-6;
    it->setText(ColMax, tr("%1 A%2").arg(full, 0, 'f', 1)
                .arg(fullOver ? tr("  ⚠ TRIPS") : QString()));
    it->setToolTip(ColMax, tr("Load if every fixture on this circuit goes to full "
                              "intensity. ⚠ = exceeds the %1 A derated limit.")
                   .arg(limit, 0, 'f', 1));

    const int rightCols[] = { ColVolts, ColAmps, ColDemand, ColLoad, ColMax };
    for (int rc : rightCols)
        it->setTextAlignment(rc, Qt::AlignRight | Qt::AlignVCenter);

    // Red row = design load overloads the breaker; amber Max = trips only at full.
    const QBrush red(kOverColor);
    const QBrush normal = sourceRow ? QBrush() : QBrush(kCircuitBg);
    for (int col = 0; col < m_tree->columnCount(); col++)
    {
        it->setForeground(col, over ? QBrush(Qt::white) : QBrush());
        it->setBackground(col, over ? red : normal);
    }
    if (fullOver && !over)
        it->setForeground(ColMax, QBrush(kWarnColor));
}

void PowerDistributionWidget::populateCircuitItem(QTreeWidgetItem *ci, int s, int c)
{
    PowerDistribution *pd = m_doc->powerDistribution();
    const PowerSource &src = pd->sources().at(s);
    const PowerCircuit &cir = src.circuits.at(c);
    const double v = cir.effectiveVoltage(src.voltage);

    ci->setText(ColName, cir.name);

    // Connector = the receptacle this circuit provides (power FROM the source).
    const QString conn = cir.connector.isEmpty()
            ? suggestedConnector(v, cir.ratedAmps) : cir.connector;
    ci->setText(ColConnector, conn);
    ci->setData(ColConnector, RoleConnector, cir.connector);
    QFont nf = ci->font(ColConnector);
    nf.setItalic(cir.connector.isEmpty());
    ci->setFont(ColConnector, nf);
    ci->setToolTip(ColConnector, cir.connector.isEmpty()
                   ? tr("Auto-suggested from voltage & amps. Double-click to set the "
                        "receptacle this circuit provides (power from the source).")
                   : tr("Receptacle this circuit provides (power from the source). "
                        "Double-click to change; pick “(auto)” to re-suggest."));

    renderCircuitLoadCells(ci, s, c, false);

    ci->setData(0, RoleType, TypeCircuit);
    ci->setData(0, RoleSource, s);
    ci->setData(0, RoleCircuit, c);
    ci->setFlags(ci->flags() | Qt::ItemIsEditable);
}

void PowerDistributionWidget::addFixtureRow(QTreeWidgetItem *parent, quint32 fxId)
{
    Fixture *fx = m_doc->fixture(fxId);
    QTreeWidgetItem *fi = new QTreeWidgetItem(parent);
    fi->setText(ColName, fx ? fx->name() : tr("(missing fixture %1)").arg(fxId));
    fi->setText(ColMax, tr("%1 W").arg(m_fixtureFullWatts.value(fxId, 0.0), 0, 'f', 0));
    fi->setTextAlignment(ColMax, Qt::AlignRight | Qt::AlignVCenter);
    fi->setData(0, RoleType, TypeFixture);
    fi->setData(0, RoleFixtureId, fxId);
    fi->setForeground(ColName, QBrush(Qt::gray));
    // Share the circuit's light tint so a circuit and its fixtures read as one
    // block — clear divisions for seeing/where to drag & drop.
    const QBrush bg(kCircuitBg);
    for (int col = 0; col < m_tree->columnCount(); col++)
        fi->setBackground(col, bg);
}

void PowerDistributionWidget::ensureWallSocketCircuit(int s)
{
    PowerDistribution *pd = m_doc->powerDistribution();
    if (s < 0 || s >= pd->sources().size())
        return;
    if (pd->sources()[s].circuits.isEmpty())
    {
        QSettings settings;
        PowerCircuit cir;
        cir.name = pd->sources()[s].name;
        cir.ratedAmps = settings.value(KDefaultBreakerKey, 20.0).toDouble();
        cir.deratePercent = settings.value(KDefaultDerateKey, 80).toInt();
        pd->sources()[s].circuits.append(cir);
    }
}

void PowerDistributionWidget::applySourceType(int s)
{
    PowerDistribution *pd = m_doc->powerDistribution();
    if (s < 0 || s >= pd->sources().size())
        return;
    PowerSource &src = pd->sources()[s];
    if (src.type == PowerSource::WallSocket)
    {
        // A wall socket is a single outlet: collapse to one circuit, folding any
        // existing circuits' fixtures into it.
        ensureWallSocketCircuit(s);
        for (int c = src.circuits.size() - 1; c >= 1; c--)
        {
            src.circuits[0].fixtures += src.circuits.at(c).fixtures;
            src.circuits.removeAt(c);
        }
    }
}

void PowerDistributionWidget::fillSourceRow(QTreeWidgetItem *si, int s)
{
    PowerDistribution *pd = m_doc->powerDistribution();
    const PowerSource &src = pd->sources().at(s);

    si->setText(ColName, src.name);
    si->setText(ColType, sourceTypeName(src.type));
    si->setData(ColType, RoleSourceType, src.type);
    si->setData(0, RoleType, TypeSource);
    si->setData(0, RoleSource, s);
    si->setFlags(si->flags() | Qt::ItemIsEditable);

    // Feed connector = the power INTO the source (input). Italic placeholder
    // when unset; UPS/battery is indicated by the Type column, not here.
    si->setText(ColConnector, src.connector.isEmpty() ? tr("set feed…") : src.connector);
    si->setData(ColConnector, RoleConnector, src.connector);
    QFont cf = si->font(ColConnector);
    cf.setItalic(src.connector.isEmpty());
    si->setFont(ColConnector, cf);
    si->setToolTip(ColConnector, tr("Feed into this source (the power FOR the "
                                    "source). Double-click to set."));

    // A source with 0 or 1 circuits is shown "flat": fixtures attach to the
    // source row directly (its single/implicit circuit), no child circuit row.
    // With 2+ circuits it becomes a distro and lists them below. This lets a
    // wall socket or single-outlet UPS take fixtures directly, while still
    // allowing circuits to be added.
    const int nc = src.circuits.size();
    if (nc == 1)
    {
        si->setData(0, RoleCircuit, 0);          // the row is the assign target
        renderCircuitLoadCells(si, s, 0, true);  // its single circuit's load
        foreach (quint32 fxId, pd->sources().at(s).circuits.at(0).fixtures)
            addFixtureRow(si, fxId);
    }
    else
    {
        // No circuit yet (still a direct target — assigning creates one), or a
        // multi-circuit distro (drop onto a specific circuit below).
        si->setData(0, RoleCircuit, nc == 0 ? QVariant(0) : QVariant());
        si->setText(ColVolts, QString::number(src.voltage, 'f', 0));
        si->setTextAlignment(ColVolts, Qt::AlignRight | Qt::AlignVCenter);
        si->setText(ColAmps, QString());
        si->setText(ColDemand, QString());
        updateSourceRow(si, s);                  // aggregate load in ColLoad
    }
    si->setExpanded(true);
}

void PowerDistributionWidget::updateSourceRow(QTreeWidgetItem *si, int s)
{
    PowerDistribution *pd = m_doc->powerDistribution();
    if (s < 0 || s >= pd->sources().size())
        return;
    const PowerSource &src = pd->sources().at(s);
    const double w = sourceWatts(s);

    QSettings settings;
    const double pf = settings.value(KPowerFactorKey, 0.9).toDouble();
    const double va = (pf > 0.0) ? w / pf : w;

    bool over = false;
    if (src.isUPS())
    {
        const double rt = src.estimateRuntimeMinutes(w);
        QString load = tr("%1 / %2 kVA").arg(va / 1000.0, 0, 'f', 2)
                       .arg(src.vaRating / 1000.0, 0, 'f', 2);
        if (rt >= 0.0)
            load += tr(" · ~%1 min").arg(rt, 0, 'f', 0);
        si->setText(ColLoad, load);
        over = (va > src.vaRating);
    }
    else
    {
        si->setText(ColLoad, tr("Σ %1 kW").arg(w / 1000.0, 0, 'f', 2));
    }
    si->setTextAlignment(ColLoad, Qt::AlignRight | Qt::AlignVCenter);

    const QBrush red(kOverColor);
    for (int col = 0; col < m_tree->columnCount(); col++)
    {
        si->setForeground(col, over ? QBrush(Qt::white) : QBrush());
        si->setBackground(col, over ? red : QBrush());
    }
}

void PowerDistributionWidget::rebuildTree()
{
    m_tree->blockSignals(true);
    m_tree->clear();

    PowerDistribution *pd = m_doc->powerDistribution();
    const QList<PowerSource> &srcs = pd->sources();
    for (int s = 0; s < srcs.size(); s++)
    {
        QTreeWidgetItem *si = new QTreeWidgetItem(m_tree);
        fillSourceRow(si, s);

        // Flat (0-1 circuit) sources render their fixtures on the source row
        // itself (done in fillSourceRow); a multi-circuit distro lists each
        // circuit with its fixtures below.
        if (srcs.at(s).circuits.size() >= 2)
        {
            for (int c = 0; c < srcs.at(s).circuits.size(); c++)
            {
                QTreeWidgetItem *ci = new QTreeWidgetItem(si);
                populateCircuitItem(ci, s, c);
                foreach (quint32 fxId, srcs.at(s).circuits.at(c).fixtures)
                    addFixtureRow(ci, fxId);
                ci->setExpanded(true);
            }
        }
    }
    m_tree->blockSignals(false);
}

void PowerDistributionWidget::rebuildFixtures()
{
    if (m_fixtureList == NULL)
        return;
    m_fixtureList->clear();
    PowerDistribution *pd = m_doc->powerDistribution();

    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == NULL)
            continue;
        QTreeWidgetItem *it = new QTreeWidgetItem(m_fixtureList);
        it->setText(0, fx->name());
        it->setData(0, Qt::UserRole, fx->id());

        int s = -1, c = -1;
        pd->circuitOf(fx->id(), s, c);
        if (s >= 0 && c >= 0 && s < pd->sources().size()
                && c < pd->sources().at(s).circuits.size())
        {
            it->setText(1, tr("%1 / %2")
                        .arg(pd->sources().at(s).name)
                        .arg(pd->sources().at(s).circuits.at(c).name));
        }
        else
        {
            it->setText(1, tr("(unassigned)"));
            it->setForeground(1, QBrush(Qt::gray));
        }
        it->setText(2, QString::number(m_fixtureWatts.value(fx->id(), 0.0), 'f', 0));
    }
    m_fixtureList->resizeColumnToContents(0);
}

void PowerDistributionWidget::updateFooter()
{
    PowerDistribution *pd = m_doc->powerDistribution();
    const QList<PowerSource> &srcs = pd->sources();

    double designW = 0.0, maxW = 0.0, liveW = 0.0, worstOver = 0.0;
    int overCircuits = 0;
    for (int s = 0; s < srcs.size(); s++)
    {
        for (int c = 0; c < srcs.at(s).circuits.size(); c++)
        {
            const int demand = srcs.at(s).circuits.at(c)
                    .effectiveDemandPercent(defaultDemandPercent());
            double cFull = 0.0, cLive = 0.0;
            foreach (quint32 fid, srcs.at(s).circuits.at(c).fixtures)
            {
                cFull += m_fixtureFullWatts.value(fid, 0.0);
                cLive += m_fixtureWatts.value(fid, 0.0);
            }
            maxW += cFull;
            designW += cFull * demand / 100.0;
            liveW += cLive;

            const double over = circuitDesignAmps(s, c)
                    - srcs.at(s).circuits.at(c).deratedLimit();
            if (over > 1e-6)
            {
                overCircuits++;
                worstOver = qMax(worstOver, over);
            }
        }
    }

    // Fixtures not yet on any circuit — their load isn't accounted for above.
    int unassigned = 0;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == NULL)
            continue;
        int s = -1, c = -1;
        pd->circuitOf(fx->id(), s, c);
        if (s < 0 || c < 0)
            unassigned++;
    }

    QStringList parts;
    parts << tr("<b>Rig load</b> — Design %1 kW · Max %2 kW · Live %3 kW")
             .arg(designW / 1000.0, 0, 'f', 2)
             .arg(maxW / 1000.0, 0, 'f', 2)
             .arg(liveW / 1000.0, 0, 'f', 2);

    if (overCircuits > 0)
        parts << tr("<span style='color:#c0392b'><b>⚠ %n circuit(s) over</b> "
                    "(worst +%1 A)</span>", "", overCircuits)
                 .arg(worstOver, 0, 'f', 1);
    else if (!srcs.isEmpty())
        parts << tr("<span style='color:#27ae60'>✓ within limits</span>");

    if (unassigned > 0)
        parts << tr("%n fixture(s) unassigned", "", unassigned);

    m_footer->setText(parts.join(tr("&nbsp;&nbsp;|&nbsp;&nbsp;")));
}

void PowerDistributionWidget::slotTreeSelectionChanged()
{
    QTreeWidgetItem *it = m_tree->currentItem();
    const bool isSource = it && it->data(0, RoleType).toInt() == TypeSource;
    const bool isCircuit = it && it->data(0, RoleType).toInt() == TypeCircuit;
    // A wall socket has a single implicit circuit, so it can't take more.
    PowerDistribution *pd = m_doc->powerDistribution();
    bool wall = false;
    if ((isSource || isCircuit) && it)
    {
        const int s = it->data(0, RoleSource).toInt();
        if (s >= 0 && s < pd->sources().size())
            wall = pd->sources().at(s).isWallSocket();
    }
    // Add-circuit needs a source (or circuit) selected; assign needs a circuit.
    m_addCircuitBtn->setEnabled((isSource || isCircuit) && !wall);
    m_removeBtn->setEnabled(isSource || isCircuit);
    if (m_assignBtn != NULL)
        m_assignBtn->setEnabled(isCircuit);
    refreshSourcePanel();
}

void PowerDistributionWidget::slotTreeItemChanged(QTreeWidgetItem *item, int column)
{
    if (item == NULL)
        return;
    PowerDistribution *pd = m_doc->powerDistribution();
    const int type = item->data(0, RoleType).toInt();
    const int s = item->data(0, RoleSource).toInt();

    // NOTE: refresh the affected rows IN PLACE — never rebuildTree() here. The
    // tree is still committing this edit, and clear()ing it would delete the
    // item mid-commit and drop the typed value (that was the "Rated A
    // disappears" bug). rebuildFixtures()/updateFooter() are safe: other widgets.
    m_tree->blockSignals(true);

    if (type == TypeSource)
    {
        if (s < 0 || s >= pd->sources().size()) { m_tree->blockSignals(false); return; }
        PowerSource &src = pd->sources()[s];
        const bool wall = src.isWallSocket();
        bool structural = false;

        if (column == ColName)
            src.name = item->text(ColName);
        else if (column == ColType)
        {
            src.type = item->data(ColType, RoleSourceType).toInt();
            applySourceType(s);
            structural = true;   // circuit layout / row shape changed
        }
        else if (column == ColConnector)
            src.connector = item->data(ColConnector, RoleConnector).toString();
        else if (column == ColVolts)
        {
            const double v = item->text(ColVolts).toDouble();
            if (v > 0.0)
                src.voltage = v;
        }
        else if (column == ColAmps && wall)
        {
            const double a = item->text(ColAmps).toDouble();
            if (a > 0.0 && src.circuits.isEmpty() == false)
                src.circuits[0].ratedAmps = a;
        }
        else if (column == ColDemand && wall)
        {
            QString t = item->text(ColDemand);
            t.remove(QLatin1Char('%'));
            int d = qBound(0, t.trimmed().toInt(), 100);
            if (src.circuits.isEmpty() == false)
                src.circuits[0].demandPercent = d;
        }

        Q_UNUSED(structural);
        m_tree->blockSignals(false);
        m_doc->setModified();
        // Re-render off the event loop: a full rebuild here would delete the item
        // mid-commit (the "value disappears" bug). Deferred, the edit has landed.
        // Reselect the source afterwards so the bottom panel stays on it.
        QTimer::singleShot(0, this, [this, s]() {
            refresh();
            for (int i = 0; i < m_tree->topLevelItemCount(); i++)
            {
                QTreeWidgetItem *si = m_tree->topLevelItem(i);
                if (si->data(0, RoleSource).toInt() == s)
                {
                    m_tree->setCurrentItem(si);
                    break;
                }
            }
        });
        return;
    }
    else if (type == TypeCircuit)
    {
        const int c = item->data(0, RoleCircuit).toInt();
        if (s < 0 || s >= pd->sources().size()
                || c < 0 || c >= pd->sources()[s].circuits.size())
        { m_tree->blockSignals(false); return; }

        if (column == ColName)
            pd->sources()[s].circuits[c].name = item->text(ColName);
        else if (column == ColVolts)
            // Explicit per-circuit override; 0/blank reverts to inheriting.
            pd->sources()[s].circuits[c].voltage = item->text(ColVolts).toDouble();
        else if (column == ColAmps)
        {
            const double a = item->text(ColAmps).toDouble();
            if (a > 0.0)
                pd->sources()[s].circuits[c].ratedAmps = a;
        }
        else if (column == ColDemand)
        {
            // Accept "70" or "70%"; 0/blank reverts to the workspace default.
            QString t = item->text(ColDemand);
            t.remove(QLatin1Char('%'));
            int d = t.trimmed().toInt();
            if (d < 0) d = 0;
            if (d > 100) d = 100;
            pd->sources()[s].circuits[c].demandPercent = d;
        }
        else if (column == ColConnector)
            // Committed by the dropdown delegate into RoleConnector ("" = auto).
            pd->sources()[s].circuits[c].connector =
                    item->data(ColConnector, RoleConnector).toString();
        populateCircuitItem(item, s, c);   // re-render this row from the model
    }

    m_tree->blockSignals(false);
    m_doc->setModified();
    rebuildFixtures();   // assignment column may reference an edited circuit name
    updateFooter();
}

void PowerDistributionWidget::slotAddSource()
{
    QSettings settings;
    PowerDistribution *pd = m_doc->powerDistribution();
    PowerSource src;
    src.name = tr("Source %1").arg(pd->sources().size() + 1);
    src.voltage = settings.value(KDefaultVoltageKey, 120.0).toDouble();
    pd->sources().append(src);
    m_doc->setModified();
    rebuildTree();
    updateFooter();
}

void PowerDistributionWidget::slotAddCircuit()
{
    QTreeWidgetItem *it = m_tree->currentItem();
    if (it == NULL)
        return;
    const int s = it->data(0, RoleSource).toInt();
    PowerDistribution *pd = m_doc->powerDistribution();
    if (s < 0 || s >= pd->sources().size())
        return;

    QSettings settings;
    PowerCircuit cir;
    cir.name = tr("Circuit %1").arg(pd->sources()[s].circuits.size() + 1);
    cir.ratedAmps = settings.value(KDefaultBreakerKey, 20.0).toDouble();
    cir.deratePercent = settings.value(KDefaultDerateKey, 80).toInt();
    pd->sources()[s].circuits.append(cir);
    m_doc->setModified();
    rebuildTree();
    updateFooter();
}

void PowerDistributionWidget::slotRemoveSelected()
{
    removeSelectedTreeItems();
}

void PowerDistributionWidget::removeSelectedTreeItems()
{
    PowerDistribution *pd = m_doc->powerDistribution();

    // Gather selected sources and circuits (ignore fixture rows — those unassign
    // via their own menu entry).
    QSet<int> sourcesToRemove;
    QList<QPair<int, int> > circuitsToRemove;
    foreach (QTreeWidgetItem *it, m_tree->selectedItems())
    {
        const int type = it->data(0, RoleType).toInt();
        if (type == TypeSource)
            sourcesToRemove.insert(it->data(0, RoleSource).toInt());
        else if (type == TypeCircuit)
            circuitsToRemove.append(qMakePair(it->data(0, RoleSource).toInt(),
                                              it->data(0, RoleCircuit).toInt()));
    }
    if (sourcesToRemove.isEmpty() && circuitsToRemove.isEmpty())
        return;

    // Circuits whose source is being removed anyway are redundant.
    QList<QPair<int, int> > circuits;
    foreach (const auto &sc, circuitsToRemove)
        if (sourcesToRemove.contains(sc.first) == false)
            circuits.append(sc);

    // Tally what the removal will affect, for the warning.
    int circuitCount = circuits.size();
    int fixtureCount = 0;
    bool sourceWithCircuits = false;
    foreach (const auto &sc, circuits)
        if (sc.first < pd->sources().size() && sc.second < pd->sources().at(sc.first).circuits.size())
            fixtureCount += pd->sources().at(sc.first).circuits.at(sc.second).fixtures.size();
    foreach (int s, sourcesToRemove)
    {
        if (s < 0 || s >= pd->sources().size())
            continue;
        const PowerSource &src = pd->sources().at(s);
        if (src.circuits.isEmpty() == false)
            sourceWithCircuits = true;
        circuitCount += src.circuits.size();
        foreach (const PowerCircuit &cir, src.circuits)
            fixtureCount += cir.fixtures.size();
    }

    const int itemCount = sourcesToRemove.size() + circuits.size();
    const bool bulk = (itemCount > 1) || (fixtureCount > 0) || sourceWithCircuits;
    if (bulk)
    {
        QStringList bits;
        if (sourcesToRemove.isEmpty() == false)
            bits << tr("%n source(s)", "", sourcesToRemove.size());
        if (circuits.isEmpty() == false)
            bits << tr("%n circuit(s)", "", circuits.size());
        QString msg = tr("Remove %1?").arg(bits.join(tr(" and ")));
        if (fixtureCount > 0)
            msg += tr("\n\nThis clears %n fixture assignment(s).", "", fixtureCount);
        if (QMessageBox::warning(this, tr("Remove power items"), msg,
                                 QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok)
            return;
    }

    // Remove circuits first (descending, so earlier indices stay valid), then
    // sources (descending). Source indices are unaffected by circuit removals.
    std::sort(circuits.begin(), circuits.end(),
              [](const QPair<int, int> &a, const QPair<int, int> &b)
              { return a.first != b.first ? a.first > b.first : a.second > b.second; });
    foreach (const auto &sc, circuits)
        if (sc.first >= 0 && sc.first < pd->sources().size()
                && sc.second >= 0 && sc.second < pd->sources()[sc.first].circuits.size())
            pd->sources()[sc.first].circuits.removeAt(sc.second);

    QList<int> srcs = sourcesToRemove.values();
    std::sort(srcs.begin(), srcs.end(), [](int a, int b) { return a > b; });
    foreach (int s, srcs)
        if (s >= 0 && s < pd->sources().size())
            pd->sources().removeAt(s);

    m_doc->setModified();
    refresh();
}

void PowerDistributionWidget::assignSelectionTo(int s, int c)
{
    PowerDistribution *pd = m_doc->powerDistribution();
    const QList<QTreeWidgetItem*> sel = m_fixtureList->selectedItems();
    if (sel.isEmpty())
        return;

    foreach (QTreeWidgetItem *fit, sel)
    {
        const quint32 fxId = fit->data(0, Qt::UserRole).toUInt();
        if (c < 0)
            pd->unassignFixture(fxId);
        else
            pd->assignFixture(fxId, s, c);
    }

    m_doc->setModified();
    rebuildTree();
    rebuildFixtures();
    updateFooter();
}

void PowerDistributionWidget::slotAssign()
{
    QTreeWidgetItem *cit = m_tree->currentItem();
    if (cit == NULL || cit->data(0, RoleType).toInt() != TypeCircuit)
        return;
    assignSelectionTo(cit->data(0, RoleSource).toInt(),
                      cit->data(0, RoleCircuit).toInt());
}

void PowerDistributionWidget::slotUnassign()
{
    assignSelectionTo(-1, -1);
}

void PowerDistributionWidget::slotFixtureContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *clicked = m_fixtureList->itemAt(pos);
    // Right-clicking a row that isn't part of the current selection acts on that
    // row alone; right-clicking within a selection keeps the whole selection.
    if (clicked != NULL && !clicked->isSelected())
    {
        m_fixtureList->clearSelection();
        clicked->setSelected(true);
        m_fixtureList->setCurrentItem(clicked);
    }
    const int count = m_fixtureList->selectedItems().size();
    if (count == 0)
        return;

    QMenu menu(this);
    const QString scope = (count == 1)
            ? m_fixtureList->selectedItems().first()->text(0)
            : tr("%1 fixtures").arg(count);

    PowerDistribution *pd = m_doc->powerDistribution();
    const QList<PowerSource> &srcs = pd->sources();

    QMenu *assign = menu.addMenu(tr("Add %1 to circuit").arg(scope));
    bool haveCircuit = false;
    for (int s = 0; s < srcs.size(); s++)
    {
        for (int c = 0; c < srcs.at(s).circuits.size(); c++)
        {
            haveCircuit = true;
            QAction *a = assign->addAction(tr("%1 / %2")
                    .arg(srcs.at(s).name).arg(srcs.at(s).circuits.at(c).name));
            connect(a, &QAction::triggered, this, [this, s, c]() { assignSelectionTo(s, c); });
        }
    }
    if (!haveCircuit)
    {
        QAction *none = assign->addAction(tr("(no circuits yet — add one first)"));
        none->setEnabled(false);
    }

    menu.addSeparator();
    QAction *un = menu.addAction(tr("Unassign %1").arg(scope));
    connect(un, &QAction::triggered, this, [this]() { assignSelectionTo(-1, -1); });

    menu.exec(m_fixtureList->viewport()->mapToGlobal(pos));
}

void PowerDistributionWidget::slotTreeContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *it = m_tree->itemAt(pos);
    const int type = (it != NULL) ? it->data(0, RoleType).toInt() : -1;

    // A fixture row nested under a circuit: unassign the whole fixture selection.
    if (type == TypeFixture)
    {
        // Right-clicking a row outside the selection acts on that row alone;
        // within a selection keeps the whole set.
        if (it->isSelected() == false)
        {
            m_tree->clearSelection();
            it->setSelected(true);
        }
        QList<quint32> ids;
        foreach (QTreeWidgetItem *sel, m_tree->selectedItems())
            if (sel->data(0, RoleType).toInt() == TypeFixture)
                ids.append(sel->data(0, RoleFixtureId).toUInt());
        if (ids.isEmpty())
            ids.append(it->data(0, RoleFixtureId).toUInt());

        QMenu menu(this);
        QAction *un = menu.addAction(ids.size() > 1
            ? tr("Unassign %1 fixtures").arg(ids.size())
            : tr("Unassign %1").arg(it->text(ColName)));
        if (menu.exec(m_tree->viewport()->mapToGlobal(pos)) == un)
        {
            PowerDistribution *pd = m_doc->powerDistribution();
            foreach (quint32 id, ids)
                pd->unassignFixture(id);
            m_doc->setModified();
            refresh();
        }
        return;
    }

    // Right-clicking a source/circuit that isn't part of the current selection
    // targets just that row; right-clicking within a selection keeps it (so a
    // multi-selection can be removed at once).
    if (it != NULL && it->isSelected() == false)
    {
        m_tree->clearSelection();
        it->setSelected(true);
    }
    if (it != NULL)
        m_tree->setCurrentItem(it); // add-circuit targets the current row's source

    PowerDistribution *pd = m_doc->powerDistribution();
    // The source this row belongs to (a source itself, or a circuit's parent).
    int srcIdx = -1;
    if (it != NULL && (type == TypeSource || type == TypeCircuit))
        srcIdx = it->data(0, RoleSource).toInt();
    const bool srcValid = (srcIdx >= 0 && srcIdx < pd->sources().size());
    const int srcType = srcValid ? pd->sources().at(srcIdx).type : -1;
    const bool wall = (srcType == PowerSource::WallSocket);

    QMenu menu(this);
    QAction *addSource = menu.addAction(tr("Add source"));

    // A wall socket has no separate circuits; every other source type can gain one.
    QAction *addCircuit = NULL;
    if (type == TypeCircuit || (type == TypeSource && !wall))
        addCircuit = menu.addAction(tr("Add circuit"));

    // Breakout presets — turn a single feed into a fixed set of circuits.
    QMenu *presetMenu = NULL;
    if (type == TypeSource && srcType == PowerSource::BreakoutBox)
        presetMenu = menu.addMenu(tr("Add breakout preset"));
    static const struct { const char *label; int n; double volts, amps; const char *conn; } kPresets[] = {
        { "L14-30 → 2 × 120 V / 20 A", 2, 120, 20, "Edison 5-20" },
        { "L21-30 → 3 × 120 V / 20 A", 3, 120, 20, "Edison 5-20" },
        { "Quad box → 4 × 120 V / 15 A", 4, 120, 15, "Edison 5-15" },
        { "L14-30 → 2 × 120 V / 15 A", 2, 120, 15, "Edison 5-15" },
    };
    QList<QAction *> presetActions;
    if (presetMenu != NULL)
        for (const auto &p : kPresets)
            presetActions.append(presetMenu->addAction(tr(p.label)));

    QAction *removeSel = NULL;
    if (type == TypeSource || type == TypeCircuit)
    {
        int selCount = 0;
        foreach (QTreeWidgetItem *s, m_tree->selectedItems())
        {
            const int t = s->data(0, RoleType).toInt();
            if (t == TypeSource || t == TypeCircuit)
                selCount++;
        }
        menu.addSeparator();
        removeSel = menu.addAction(selCount > 1
            ? tr("Remove %1 items").arg(selCount) : tr("Remove"));
    }

    QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
    if (chosen == NULL)
        return;
    if (chosen == addSource)
        slotAddSource();
    else if (chosen == addCircuit)
        slotAddCircuit();
    else if (chosen == removeSel)
        removeSelectedTreeItems();
    else if (presetActions.contains(chosen) && srcValid)
    {
        const auto &p = kPresets[presetActions.indexOf(chosen)];
        QSettings settings;
        const int derate = settings.value(KDefaultDerateKey, 80).toInt();
        for (int i = 0; i < p.n; i++)
        {
            PowerCircuit cir;
            cir.name = tr("Circuit %1").arg(pd->sources()[srcIdx].circuits.size() + 1);
            cir.voltage = p.volts;
            cir.ratedAmps = p.amps;
            cir.deratePercent = derate;
            cir.connector = QString::fromLatin1(p.conn);
            pd->sources()[srcIdx].circuits.append(cir);
        }
        m_doc->setModified();
        refresh();
    }
}

void PowerDistributionWidget::slotFixturesDroppedOnCircuit(int s, int c,
                                                           const QList<quint32> &ids)
{
    PowerDistribution *pd = m_doc->powerDistribution();
    // c == 0 on a circuit-less source is valid: assignFixture creates the first
    // circuit. Otherwise the circuit index must exist.
    if (s < 0 || s >= pd->sources().size() || c < 0
            || (c >= pd->sources()[s].circuits.size()
                && !(c == 0 && pd->sources()[s].circuits.isEmpty())))
        return;
    foreach (quint32 id, ids)
        pd->assignFixture(id, s, c);
    m_doc->setModified();
    refresh();
}

void PowerDistributionWidget::slotAutoAssign()
{
    // Greedy bin-pack: heaviest fixtures first into the first circuit with
    // headroom under its derated limit; spawn new circuits (seeded from the
    // saved defaults) on a single auto source when everything is full.
    QSettings settings;
    const double defV = settings.value(KDefaultVoltageKey, 120.0).toDouble();
    const double defBreaker = settings.value(KDefaultBreakerKey, 20.0).toDouble();
    const int defDerate = settings.value(KDefaultDerateKey, 80).toInt();

    PowerDistribution *pd = m_doc->powerDistribution();

    // Sort fixtures by watts descending.
    QList<QPair<double, quint32> > fixturesByLoad;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == NULL)
            continue;
        pd->unassignFixture(fx->id());
        fixturesByLoad.append(qMakePair(m_fixtureWatts.value(fx->id(), 0.0), fx->id()));
    }
    std::sort(fixturesByLoad.begin(), fixturesByLoad.end(),
              [](const QPair<double, quint32> &a, const QPair<double, quint32> &b)
              { return a.first > b.first; });

    // Ensure there's at least one source to pack into.
    if (pd->sources().isEmpty())
    {
        PowerSource src;
        src.name = tr("Auto source");
        src.voltage = defV;
        pd->sources().append(src);
    }

    // Running load (amps) per existing circuit, addressed as "s:c".
    QHash<QString, double> loadAmps;
    for (int s = 0; s < pd->sources().size(); s++)
        for (int c = 0; c < pd->sources()[s].circuits.size(); c++)
            loadAmps.insert(QString("%1:%2").arg(s).arg(c), 0.0);

    for (int i = 0; i < fixturesByLoad.size(); i++)
    {
        const double watts = fixturesByLoad[i].first;
        const quint32 fxId = fixturesByLoad[i].second;
        bool placed = false;

        for (int s = 0; s < pd->sources().size() && !placed; s++)
        {
            const double srcV = (pd->sources()[s].voltage > 0.0)
                                ? pd->sources()[s].voltage : defV;
            for (int c = 0; c < pd->sources()[s].circuits.size() && !placed; c++)
            {
                const QString key = QString("%1:%2").arg(s).arg(c);
                const PowerCircuit &cir = pd->sources()[s].circuits.at(c);
                const double amps = watts / cir.effectiveVoltage(srcV);
                if (loadAmps[key] + amps <= cir.deratedLimit())
                {
                    pd->assignFixture(fxId, s, c);
                    loadAmps[key] += amps;
                    placed = true;
                }
            }
        }

        if (!placed)
        {
            // No circuit has headroom — add a new one on the first source.
            const int s = 0;
            const double srcV = (pd->sources()[s].voltage > 0.0)
                                ? pd->sources()[s].voltage : defV;
            PowerCircuit cir;
            cir.name = tr("Circuit %1").arg(pd->sources()[s].circuits.size() + 1);
            cir.ratedAmps = defBreaker;
            cir.deratePercent = defDerate;
            pd->sources()[s].circuits.append(cir);
            const int c = pd->sources()[s].circuits.size() - 1;
            pd->assignFixture(fxId, s, c);
            loadAmps.insert(QString("%1:%2").arg(s).arg(c),
                            watts / pd->sources()[s].circuits[c].effectiveVoltage(srcV));
        }
    }

    m_doc->setModified();
    rebuildTree();
    rebuildFixtures();
    updateFooter();
}

void PowerDistributionWidget::slotSceneCheck()
{
    PowerDistribution *pd = m_doc->powerDistribution();
    QSettings settings;
    const double dflt = settings.value(KDefaultVoltageKey, 120.0).toDouble();
    const QBrush red(kOverColor);

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Power Check — scenes & collections"));
    dlg.resize(620, 460);
    QVBoxLayout *vl = new QVBoxLayout(&dlg);
    QLabel *summary = new QLabel(&dlg);
    vl->addWidget(summary);

    QTreeWidget *rt = new QTreeWidget(&dlg);
    rt->setColumnCount(3);
    rt->setHeaderLabels(QStringList() << tr("Scene / Collection")
                        << tr("Type") << tr("Load (circuit: amps / limit)"));
    vl->addWidget(rt, 1);

    int funcs = 0, overloadFuncs = 0;
    const bool haveCircuits = !pd->sources().isEmpty();

    foreach (Function *fn, m_doc->functions())
    {
        const bool isScene = (fn->type() == Function::SceneType);
        const bool isColl  = (fn->type() == Function::CollectionType);
        if (!isScene && !isColl)
            continue;
        funcs++;

        const QHash<quint32, double> w = PowerEstimator::functionFixtureWatts(m_doc, fn);

        // Per-circuit load for this function + this function's total watts.
        double totalW = 0.0;
        bool funcOver = false;
        QList<QTreeWidgetItem*> circuitRows;
        for (int s = 0; s < pd->sources().size(); s++)
        {
            const PowerSource &src = pd->sources().at(s);
            const double srcV = (src.voltage > 0.0) ? src.voltage : dflt;
            for (int c = 0; c < src.circuits.size(); c++)
            {
                const PowerCircuit &cir = src.circuits.at(c);
                double cw = 0.0;
                foreach (quint32 fid, cir.fixtures)
                    cw += w.value(fid, 0.0);
                if (cw <= 0.0)
                    continue;                      // skip circuits this look doesn't load
                totalW += cw;
                const double amps = cw / cir.effectiveVoltage(srcV);
                const double limit = cir.deratedLimit();
                const bool over = amps > limit;
                funcOver = funcOver || over;

                QTreeWidgetItem *crow = new QTreeWidgetItem();
                crow->setText(0, QString("%1 / %2").arg(src.name).arg(cir.name));
                crow->setText(2, tr("%1 / %2 A%3").arg(amps, 0, 'f', 1)
                              .arg(limit, 0, 'f', 1).arg(over ? tr("  ⚠") : QString()));
                if (over)
                    for (int col = 0; col < 3; col++)
                    { crow->setForeground(col, Qt::white); crow->setBackground(col, red); }
                circuitRows.append(crow);
            }
        }
        if (funcOver)
            overloadFuncs++;

        QTreeWidgetItem *tw = new QTreeWidgetItem(rt);
        tw->setText(0, fn->name());
        tw->setText(1, isColl ? tr("Collection") : tr("Scene"));
        tw->setText(2, tr("%1 kW%2").arg(totalW / 1000.0, 0, 'f', 2)
                     .arg(funcOver ? tr("  ⚠ OVERLOAD") : QString()));
        tw->addChildren(circuitRows);
        if (funcOver)
        {
            tw->setForeground(2, red);
            tw->setExpanded(true);
        }
    }

    if (!haveCircuits)
        summary->setText(tr("No circuits defined yet — add sources/circuits and "
                            "assign fixtures to see per-circuit load. "
                            "(%1 scenes/collections, total kW shown.)").arg(funcs));
    else if (overloadFuncs == 0)
        summary->setText(tr("✓ No overloads. %1 scenes/collections checked.").arg(funcs));
    else
        summary->setText(tr("⚠ %1 of %2 scenes/collections overload a circuit:")
                         .arg(overloadFuncs).arg(funcs));

    rt->resizeColumnToContents(0);
    rt->resizeColumnToContents(1);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);
    vl->addWidget(bb);
    dlg.exec();
}

void PowerDistributionWidget::slotExportVenue()
{
    QSettings settings;
    QString dir = settings.value(QStringLiteral("venue/lastdir")).toString();
    QString path = QFileDialog::getSaveFileName(this, tr("Export Venue"),
                       dir, tr("Venue files (*.venue)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QStringLiteral(".venue"), Qt::CaseInsensitive))
        path += QStringLiteral(".venue");
    settings.setValue(QStringLiteral("venue/lastdir"), QFileInfo(path).absolutePath());

    if (Venue::exportToFile(m_doc, path, Venue::AllSections))
        QMessageBox::information(this, tr("Export Venue"),
            tr("Venue exported to:\n%1\n\n(Circuit infrastructure only — fixture "
               "assignments stay with the show.)").arg(path));
    else
        QMessageBox::warning(this, tr("Export Venue"),
            tr("Could not write the venue file."));
}

void PowerDistributionWidget::slotImportVenue()
{
    QSettings settings;
    QString dir = settings.value(QStringLiteral("venue/lastdir")).toString();
    QString path = QFileDialog::getOpenFileName(this, tr("Import Venue"),
                       dir, tr("Venue files (*.venue)"));
    if (path.isEmpty())
        return;
    settings.setValue(QStringLiteral("venue/lastdir"), QFileInfo(path).absolutePath());

    const QStringList sections = Venue::sectionsInFile(path);
    if (sections.isEmpty())
    {
        QMessageBox::warning(this, tr("Import Venue"),
            tr("Not a valid venue file (no recognizable sections)."));
        return;
    }

    if (QMessageBox::question(this, tr("Import Venue"),
            tr("Import these venue sections, replacing the current "
               "power distribution?\n\n• %1\n\nFixture assignments will be "
               "cleared — reassign your rig after importing.").arg(sections.join("\n• ")),
            QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok)
        return;

    if (Venue::importFromFile(m_doc, path))
        refresh();
    else
        QMessageBox::warning(this, tr("Import Venue"),
            tr("Could not read the venue file."));
}
