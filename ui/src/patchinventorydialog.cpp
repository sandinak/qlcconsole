/*
  Q Light Controller Plus
  patchinventorydialog.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QSet>
#include <algorithm>

#include <QComboBox>
#include <QCheckBox>
#include <QStandardItemModel>
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "patchinventorydialog.h"
#include "monitor.h"
#include "universecombo.h"
#include "networktopology.h"
#include "inputoutputmap.h"
#include "fixture.h"
#include "doc.h"

#define COL_NAME    0
#define COL_MODEL   1
#define COL_MODE    2
#define COL_CH      3
#define COL_CONN    4
#define COL_UNI     5
#define COL_ADDR    6
#define COL_VIA     7
#define COL_STATUS  8

// Per-row proximity proposal, kept on the Via cell.
#define ROLE_NODE     (Qt::UserRole)
#define ROLE_PORT     (Qt::UserRole + 1)
#define ROLE_CLAIMS   (Qt::UserRole + 2)
#define ROLE_DIST     (Qt::UserRole + 3)
#define ROLE_PLANUNI  (Qt::UserRole + 4)

static bool isGenericDimmer(const Fixture *fx)
{
    return fx->fixtureDef() == NULL
        || (fx->fixtureDef()->manufacturer() == KXMLFixtureGeneric
            && fx->fixtureDef()->model() == KXMLFixtureGeneric);
}

PatchInventoryDialog::PatchInventoryDialog(Doc *doc, const QList<quint32> &fixtureIds, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_proximity(NULL)
    , m_placeUni(NULL)
    , m_placeAddr(NULL)
    , m_placeHow(NULL)
    , m_filling(false)
{
    setProperty("helpTopic", QStringLiteral("fixture-manager.md#patching-the-patch-dialog"));
    resize(960, 560);

    foreach (quint32 id, fixtureIds)
        if (m_doc->fixture(id) != NULL && !m_ids.contains(id))
            m_ids << id;
    if (m_ids.isEmpty())
    {
        foreach (Fixture *fx, m_doc->fixtures())
            if (fx != NULL && !fx->isPatched())
                m_ids << fx->id();
    }
    bool anyPatched = false;
    foreach (quint32 id, m_ids)
        anyPatched = anyPatched || m_doc->fixture(id)->isPatched();
    setWindowTitle(anyPatched ? tr("Patch / Re-address") : tr("Patch Inventory"));

    QVBoxLayout *root = new QVBoxLayout(this);
    QLabel *intro = new QLabel(tr("Proposed homes for the fixtures staged in the inventory. A DMX "
                                  "fixture on the plot goes to the nearest node port with room (the "
                                  "Via column); the rest are packed into the first free channels; "
                                  "each network fixture IP gets a universe of its own (unicast). Pick each "
                                  "fixture's mode here -- it sets the DMX footprint -- and edit any "
                                  "universe or address, then Apply. A fixture that a scene sets "
                                  "channel-by-channel keeps its mode. Fixtures already patched start "
                                  "where they are; Place moves the selected rows (or all of them) "
                                  "as a block, and Apply moves everything at once."), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    // Only worth offering when some node port has somewhere to be.
    bool anyAnchor = false;
    foreach (const NetworkNode &n, m_doc->networkTopology()->nodes())
        foreach (const NetworkPort &p, n.ports)
        {
            double x, y;
            anyAnchor = anyAnchor || NetworkTopology::proximityAnchor(n, p, x, y);
        }
    m_proximity = new QCheckBox(tr("Place by nearest node"), this);
    m_proximity->setChecked(anyAnchor);
    m_proximity->setEnabled(anyAnchor);
    m_proximity->setToolTip(anyAnchor
        ? tr("A DMX fixture on the plot goes to the nearest node port (its receiver, for "
             "wireless) with room; a free port on that node takes a new universe")
        : tr("Place network nodes on the plot (Lighting Studio) to patch by proximity"));
    root->addWidget(m_proximity);

    // Place: the selected rows (or all of them) at one universe and address.
    QHBoxLayout *placeRow = new QHBoxLayout;
    placeRow->addWidget(new QLabel(tr("Place selected at universe"), this));
    m_placeUni = new UniverseCombo(m_doc, 1, this);
    placeRow->addWidget(m_placeUni);
    placeRow->addWidget(new QLabel(tr("address"), this));
    m_placeAddr = new QSpinBox(this);
    m_placeAddr->setRange(1, 512);
    placeRow->addWidget(m_placeAddr);
    m_placeHow = new QComboBox(this);
    m_placeHow->addItem(tr("keeping their spacing"));
    m_placeHow->addItem(tr("packed together"));
    m_placeHow->addItem(tr("one universe each, counting up"));
    m_placeHow->setToolTip(tr("Keeping their spacing moves the patched fixtures as one block, "
                              "gaps and all. Packed puts every fixture in the first free channels "
                              "from there on, around whatever else is in the universe, and "
                              "carries on into the next universe when one fills. One universe "
                              "each gives the first row that universe, the next the one after, "
                              "and so on -- each at that address (a row of lanterns, a node's "
                              "ports)."));
    placeRow->addWidget(m_placeHow);
    QPushButton *place = new QPushButton(tr("Place"), this);
    place->setToolTip(tr("Move the selected rows -- every row when none is selected"));
    placeRow->addWidget(place);
    placeRow->addStretch();
    root->addLayout(placeRow);

    /* Like structures patched alike ("the lights on T1, 2, 3, 4 .. should all
       be addressed the same in the single U"): offered when these fixtures
       hang on more than one structure. */
    QStringList structures;
    foreach (quint32 id, m_ids)
    {
        const QString key = PatchPlanner::mountKey(m_doc, id);
        if (!key.isEmpty() && !structures.contains(key))
            structures << key;
    }
    if (structures.size() > 1)
    {
        QHBoxLayout *matchRow = new QHBoxLayout;
        matchRow->addWidget(new QLabel(tr("Patch the others like"), this));
        m_matchPattern = new QComboBox(this);
        m_matchPattern->setObjectName(QStringLiteral("matchPattern"));
        foreach (const QString &key, structures)
            m_matchPattern->addItem(PatchPlanner::structureName(m_doc, key), key);
        m_matchPattern->setToolTip(tr("Every fixture on the other structures takes the address of "
                                      "the one in its place on this one -- paired by where they "
                                      "hang -- in its own structure's universe"));
        matchRow->addWidget(m_matchPattern);
        QPushButton *match = new QPushButton(tr("Match"), this);
        match->setObjectName(QStringLiteral("matchButton"));
        matchRow->addWidget(match);
        matchRow->addStretch();
        root->addLayout(matchRow);
        connect(match, &QPushButton::clicked, this, [this]() {
            matchStructures(m_matchPattern->currentData().toString());
        });
    }

    m_table = new QTableWidget(0, 9, this);
    m_table->setHorizontalHeaderLabels({ tr("Fixture"), tr("Model"), tr("Mode"), tr("Ch"), tr("Connection"),
                                         tr("Universe"), tr("Address"), tr("Via"), tr("Status") });
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(COL_NAME, QHeaderView::Stretch);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    root->addWidget(m_table, 1);

    m_summary = new QLabel(this);
    root->addWidget(m_summary);
    m_matchProblems = new QLabel(this);
    m_matchProblems->setWordWrap(true);
    m_matchProblems->setStyleSheet(QStringLiteral("QLabel { color: #d08a2a; }"));
    m_matchProblems->hide();
    root->addWidget(m_matchProblems);

    QHBoxLayout *btns = new QHBoxLayout;
    QPushButton *replan = new QPushButton(tr("Re-plan"), this);
    replan->setToolTip(tr("Discard your edits and propose again"));
    btns->addWidget(replan);
    btns->addStretch();
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_apply = bb->addButton(tr("Apply"), QDialogButtonBox::AcceptRole);
    btns->addWidget(bb);
    root->addLayout(btns);

    connect(replan, &QPushButton::clicked, this, &PatchInventoryDialog::slotReplan);
    connect(m_proximity, &QCheckBox::toggled, this, &PatchInventoryDialog::slotReplan);
    connect(place, &QPushButton::clicked, this, &PatchInventoryDialog::slotPlace);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    fill(initialPlan());
}

QList<PatchPlanner::Assignment> PatchInventoryDialog::initialPlan(
    const QHash<quint32, quint32> &channelOverride) const
{
    // Patched fixtures first, in address order, where they are now; then the
    // inventory, as planned.
    QList<Fixture *> patched;
    QList<quint32> staged;
    foreach (quint32 id, m_ids)
    {
        Fixture *fx = m_doc->fixture(id);
        if (fx == NULL)
            continue;
        if (fx->isPatched())
            patched << fx;
        else
            staged << id;
    }
    std::sort(patched.begin(), patched.end(), [](Fixture *a, Fixture *b) {
        return a->universeAddress() < b->universeAddress();
    });

    QList<PatchPlanner::Assignment> out;
    foreach (Fixture *fx, patched)
    {
        PatchPlanner::Assignment a;
        a.fixtureId = fx->id();
        a.universe = fx->universe();
        a.address = fx->address();
        out << a;
    }
    foreach (PatchPlanner::Assignment a, planner().plan(staged, channelOverride))
    {
        // Say where a clash wanted to be, and whose channels those are.
        Fixture *fx = m_doc->fixture(a.fixtureId);
        if (fx != NULL && fx->addressClash())
        {
            Fixture *owner = m_doc->fixture(m_doc->fixtureForAddress(fx->universeAddress()));
            const QString clash = owner != NULL
                ? tr("the show file has it at universe %1, address %2, over %3")
                      .arg(fx->universe() + 1).arg(fx->address() + 1).arg(owner->name())
                : tr("the show file has it at universe %1, address %2, over another fixture")
                      .arg(fx->universe() + 1).arg(fx->address() + 1);
            a.note = a.note.isEmpty() ? clash : clash + QStringLiteral("; ") + a.note;
        }
        out << a;
    }
    return out;
}

void PatchInventoryDialog::slotPlace()
{
    QList<int> rows;
    foreach (const QModelIndex &idx, m_table->selectionModel()->selectedRows())
        rows << idx.row();
    std::sort(rows.begin(), rows.end());
    if (m_placeHow->currentIndex() == 2)
        placeCountingUp(m_placeUni->universe(), quint32(m_placeAddr->value() - 1), rows);
    else
        placeRows(m_placeUni->universe(), quint32(m_placeAddr->value() - 1),
                  m_placeHow->currentIndex() == 0, rows);
}

void PatchInventoryDialog::placeCountingUp(quint32 universe, quint32 address, const QList<int> &rows)
{
    QList<int> which = rows;
    if (which.isEmpty())
        for (int r = 0; r < m_table->rowCount(); r++)
            which << r;
    const quint32 existing = m_doc->inputOutputMap()->universesCount();
    QList<PatchPlanner::Assignment> plan;
    quint32 next = universe;
    foreach (int r, which)
    {
        QTableWidgetItem *name = m_table->item(r, COL_NAME);
        if (name == NULL)
            continue;
        PatchPlanner::Assignment a;
        a.fixtureId = name->data(Qt::UserRole).toUInt();
        a.universe = next++;
        a.address = address;
        a.newUniverse = (a.universe >= existing);
        plan << a;
    }
    setRowAddresses(plan);
}

void PatchInventoryDialog::placeRows(quint32 universe, quint32 address, bool keepSpacing,
                                     const QList<int> &rows)
{
    QList<int> which = rows;
    if (which.isEmpty())
        for (int r = 0; r < m_table->rowCount(); r++)
            which << r;
    QList<quint32> ids;
    foreach (int r, which)
        if (QTableWidgetItem *name = m_table->item(r, COL_NAME))
            ids << name->data(Qt::UserRole).toUInt();

    // The rows NOT being placed stay where the table has them, so the
    // planner must not hand their channels out.
    const QHash<quint32, quint32> overrides = channelOverrides();
    QHash<quint32, QPair<quint32, quint32> > elsewhere;
    foreach (const PatchPlanner::Assignment &a, currentPlan())
        if (!ids.contains(a.fixtureId))
            elsewhere.insert(a.fixtureId, qMakePair((a.universe << 9) | a.address,
                                                    overrides.value(a.fixtureId)));
    setRowAddresses(planner().place(ids, universe, address, keepSpacing, overrides, elsewhere));
}

void PatchInventoryDialog::setRowAddresses(const QList<PatchPlanner::Assignment> &plan)
{
    QHash<quint32, PatchPlanner::Assignment> byId;
    foreach (const PatchPlanner::Assignment &a, plan)
        byId.insert(a.fixtureId, a);

    m_filling = true;
    for (int r = 0; r < m_table->rowCount(); r++)
    {
        QTableWidgetItem *name = m_table->item(r, COL_NAME);
        if (name == NULL || !byId.contains(name->data(Qt::UserRole).toUInt()))
            continue;
        const PatchPlanner::Assignment a = byId.value(name->data(Qt::UserRole).toUInt());
        if (UniverseCombo *uni = qobject_cast<UniverseCombo *>(m_table->cellWidget(r, COL_UNI)))
            uni->setUniverse(a.universe);
        if (QSpinBox *addr = qobject_cast<QSpinBox *>(m_table->cellWidget(r, COL_ADDR)))
            addr->setValue(int(a.address) + 1);
        // Placed by hand: the nearest-node proposal no longer applies.
        if (QTableWidgetItem *via = m_table->item(r, COL_VIA))
            via->setData(ROLE_NODE, -1);
        if (QTableWidgetItem *st = m_table->item(r, COL_STATUS))
            st->setData(Qt::UserRole + 1, a.note);
    }
    m_filling = false;
    refreshConflicts();
}

void PatchInventoryDialog::slotReplan()
{
    // Re-plan around the modes as chosen: "propose again" should not throw away
    // the footprint decisions, only the address edits.
    QHash<quint32, QLCFixtureMode *> modes;
    QHash<quint32, quint32> dimmers;
    foreach (const PatchPlanner::Assignment &a, currentPlan())
    {
        if (a.mode != NULL)
            modes.insert(a.fixtureId, a.mode);
        if (a.dimmerChannels > 0)
            dimmers.insert(a.fixtureId, a.dimmerChannels);
    }
    fill(initialPlan(channelOverrides()), modes, dimmers);
}

PatchPlanner PatchInventoryDialog::planner() const
{
    PatchPlanner pp(m_doc);
    pp.setUseProximity(m_proximity != NULL && m_proximity->isChecked());
    return pp;
}

QString PatchInventoryDialog::viaText(int row, quint32 universe) const
{
    const NetworkTopology *nt = m_doc->networkTopology();
    QTableWidgetItem *via = m_table->item(row, COL_VIA);
    // As proposed: that port, how far, and whether it is being given the universe.
    if (via != NULL && via->data(ROLE_NODE).toInt() >= 0
        && via->data(ROLE_PLANUNI).toUInt() == universe)
    {
        const int ni = via->data(ROLE_NODE).toInt();
        if (ni < nt->nodes().size())
        {
            QString t = tr("%1 · port %2 · %3 m").arg(nt->nodes().at(ni).name)
                            .arg(via->data(ROLE_PORT).toInt() + 1)
                            .arg(via->data(ROLE_DIST).toDouble(), 0, 'f', 1);
            if (via->data(ROLE_CLAIMS).toBool())
                t += tr(" (assigns the port)");
            return t;
        }
    }
    // Edited by hand: whichever port already carries that universe, if any.
    int ni = -1, pi = -1;
    nt->portForUniverse(universe, ni, pi);
    if (ni >= 0)
        return tr("%1 · port %2").arg(nt->nodes().at(ni).name).arg(pi + 1);
    return QStringLiteral("\u2014");
}

quint32 PatchInventoryDialog::rowChannels(int row) const
{
    QTableWidgetItem *ch = m_table->item(row, COL_CH);
    return ch != NULL ? ch->data(Qt::UserRole).toUInt() : 0;
}

QHash<quint32, quint32> PatchInventoryDialog::channelOverrides() const
{
    QHash<quint32, quint32> out;
    for (int r = 0; r < m_table->rowCount(); r++)
        if (QTableWidgetItem *name = m_table->item(r, COL_NAME))
            out.insert(name->data(Qt::UserRole).toUInt(), rowChannels(r));
    return out;
}

void PatchInventoryDialog::slotModeChanged()
{
    if (m_filling)
        return;
    QObject *who = sender();
    for (int r = 0; r < m_table->rowCount(); r++)
    {
        if (m_table->cellWidget(r, COL_MODE) != who)
            continue;
        quint32 chans = rowChannels(r);
        if (QSpinBox *sp = qobject_cast<QSpinBox *>(who))
            chans = quint32(sp->value());
        else if (QComboBox *cb = qobject_cast<QComboBox *>(who))
            if (QLCFixtureMode *m = static_cast<QLCFixtureMode *>(cb->currentData(Qt::UserRole).value<void *>()))
                chans = quint32(m->channels().size());
        if (QTableWidgetItem *ch = m_table->item(r, COL_CH))
        {
            ch->setData(Qt::UserRole, chans);
            ch->setText(QString::number(chans));
        }
        break;
    }
    refreshConflicts();
}

void PatchInventoryDialog::fill(const QList<PatchPlanner::Assignment> &plan,
                                const QHash<quint32, QLCFixtureMode *> &modes,
                                const QHash<quint32, quint32> &dimmerChannels)
{
    m_filling = true;
    m_table->setRowCount(0);
    m_table->setRowCount(plan.size());
    // Each row's universe list offers the new ones the plan opens, and one more.
    const quint32 existing = m_doc->inputOutputMap()->universesCount();
    quint32 past = existing;
    foreach (const PatchPlanner::Assignment &a, plan)
        past = qMax(past, a.universe + 1);
    const int newOnes = int(past - existing) + 1;
    for (int r = 0; r < plan.size(); r++)
    {
        const PatchPlanner::Assignment &a = plan.at(r);
        Fixture *fx = m_doc->fixture(a.fixtureId);
        if (fx == NULL)
            continue;

        QTableWidgetItem *name = new QTableWidgetItem(fx->name());
        name->setData(Qt::UserRole, a.fixtureId);
        name->setToolTip(a.note);
        name->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_table->setItem(r, COL_NAME, name);

        QString model = fx->fixtureDef() ? fx->fixtureDef()->model() : tr("Generic");
        QTableWidgetItem *mi = new QTableWidgetItem(model);
        mi->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_table->setItem(r, COL_MODEL, mi);

        // Mode: it decides the DMX footprint, so it is chosen here, at patch
        // time. Modes a fixture cannot move to (a scene sets its channels
        // directly, or the head count differs) are shown but disabled, with why.
        quint32 chans = fx->channels();
        QString why;
        if (fx->isPatched())
        {
            /* Re-addressing, not re-patching: its mode is part of what is
               live, and is changed in its properties, where everything that
               depends on it is looked after. */
            QTableWidgetItem *mo = new QTableWidgetItem(fx->fixtureMode() ? fx->fixtureMode()->name()
                                                                          : QString());
            mo->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            mo->setToolTip(tr("Already patched: change its mode in the fixture's properties"));
            m_table->setItem(r, COL_MODE, mo);
        }
        else if (isGenericDimmer(fx))
        {
            QSpinBox *sp = new QSpinBox(m_table);
            sp->setRange(1, 512);
            sp->setSuffix(tr(" ch"));
            sp->setValue(int(dimmerChannels.value(a.fixtureId, fx->channels())));
            if (!m_doc->canChangeFixtureMode(a.fixtureId, NULL, &why))
            {
                sp->setEnabled(false);
                sp->setToolTip(tr("Fixed: %1").arg(why));
            }
            chans = quint32(sp->value());
            connect(sp, QOverload<int>::of(&QSpinBox::valueChanged), this, &PatchInventoryDialog::slotModeChanged);
            m_table->setCellWidget(r, COL_MODE, sp);
        }
        else if (fx->fixtureDef() != NULL && fx->fixtureDef()->modes().size() > 1)
        {
            QComboBox *cb = new QComboBox(m_table);
            QStandardItemModel *cm = new QStandardItemModel(cb);
            cb->setModel(cm);
            QLCFixtureMode *want = modes.value(a.fixtureId, fx->fixtureMode());
            int wantIdx = 0;
            foreach (QLCFixtureMode *m, fx->fixtureDef()->modes())
            {
                QStandardItem *it = new QStandardItem(QStringLiteral("%1 (%2 ch)")
                                                          .arg(m->name()).arg(m->channels().size()));
                it->setData(QVariant::fromValue<void *>(m), Qt::UserRole);
                if (m != fx->fixtureMode() && !m_doc->canChangeFixtureMode(a.fixtureId, m, &why))
                {
                    it->setEnabled(false);
                    it->setToolTip(why);
                }
                if (m == want)
                    wantIdx = cm->rowCount();
                cm->appendRow(it);
            }
            cb->setCurrentIndex(wantIdx);
            if (want != NULL)
                chans = quint32(want->channels().size());
            connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PatchInventoryDialog::slotModeChanged);
            m_table->setCellWidget(r, COL_MODE, cb);
        }
        else
        {
            QTableWidgetItem *mo = new QTableWidgetItem(fx->fixtureMode() ? fx->fixtureMode()->name() : QString());
            mo->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            m_table->setItem(r, COL_MODE, mo);
        }

        QTableWidgetItem *ch = new QTableWidgetItem(QString::number(chans));
        ch->setData(Qt::UserRole, chans);
        ch->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        ch->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(r, COL_CH, ch);

        QString conn = fx->connection() == Fixture::Network
                           ? (fx->ipAddress().isEmpty() ? tr("Network") : tr("Network %1").arg(fx->ipAddress()))
                           : tr("DMX");
        QTableWidgetItem *ci = new QTableWidgetItem(conn);
        ci->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_table->setItem(r, COL_CONN, ci);

        UniverseCombo *uni = new UniverseCombo(m_doc, newOnes, m_table);
        uni->setUniverse(a.universe);
        connect(uni, &UniverseCombo::universeChanged, this, &PatchInventoryDialog::slotCellEdited);
        m_table->setCellWidget(r, COL_UNI, uni);

        QSpinBox *addr = new QSpinBox(m_table);
        addr->setRange(1, 512);
        addr->setValue(int(a.address) + 1);
        connect(addr, QOverload<int>::of(&QSpinBox::valueChanged), this, &PatchInventoryDialog::slotCellEdited);
        m_table->setCellWidget(r, COL_ADDR, addr);

        QTableWidgetItem *via = new QTableWidgetItem;
        via->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        via->setData(ROLE_NODE, a.nodeIndex);
        via->setData(ROLE_PORT, a.portIndex);
        via->setData(ROLE_CLAIMS, a.claimsPort);
        via->setData(ROLE_DIST, a.distanceMetres);
        via->setData(ROLE_PLANUNI, a.universe);
        m_table->setItem(r, COL_VIA, via);

        QTableWidgetItem *st = new QTableWidgetItem;
        st->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        st->setData(Qt::UserRole + 1, a.note);
        st->setData(Qt::UserRole + 2, a.newUniverse);
        m_table->setItem(r, COL_STATUS, st);
    }
    m_filling = false;
    refreshConflicts();
}

QList<PatchPlanner::Assignment> PatchInventoryDialog::currentPlan() const
{
    QList<PatchPlanner::Assignment> out;
    const quint32 existing = m_doc->inputOutputMap()->universesCount();
    for (int r = 0; r < m_table->rowCount(); r++)
    {
        QTableWidgetItem *name = m_table->item(r, COL_NAME);
        UniverseCombo *uni = qobject_cast<UniverseCombo *>(m_table->cellWidget(r, COL_UNI));
        QSpinBox *addr = qobject_cast<QSpinBox *>(m_table->cellWidget(r, COL_ADDR));
        if (name == NULL || uni == NULL || addr == NULL)
            continue;
        PatchPlanner::Assignment a;
        a.fixtureId = name->data(Qt::UserRole).toUInt();
        a.universe = uni->universe();
        a.address = quint32(addr->value() - 1);
        a.newUniverse = (a.universe >= existing);
        if (QTableWidgetItem *st = m_table->item(r, COL_STATUS))
            a.note = st->data(Qt::UserRole + 1).toString();
        // The proposed port only stands while the row keeps the proposed universe.
        if (QTableWidgetItem *via = m_table->item(r, COL_VIA))
        {
            if (via->data(ROLE_NODE).toInt() >= 0 && via->data(ROLE_PLANUNI).toUInt() == a.universe)
            {
                a.nodeIndex = via->data(ROLE_NODE).toInt();
                a.portIndex = via->data(ROLE_PORT).toInt();
                a.claimsPort = via->data(ROLE_CLAIMS).toBool();
                a.distanceMetres = via->data(ROLE_DIST).toDouble();
            }
        }
        if (QComboBox *cb = qobject_cast<QComboBox *>(m_table->cellWidget(r, COL_MODE)))
            a.mode = static_cast<QLCFixtureMode *>(cb->currentData(Qt::UserRole).value<void *>());
        else if (QSpinBox *sp = qobject_cast<QSpinBox *>(m_table->cellWidget(r, COL_MODE)))
            a.dimmerChannels = quint32(sp->value());
        out << a;
    }
    return out;
}

void PatchInventoryDialog::slotCellEdited()
{
    if (!m_filling)
        refreshConflicts();
}

void PatchInventoryDialog::refreshConflicts()
{
    // A row collides if its channels overlap a patched fixture OR another row.
    QHash<quint32, quint32> taken;            // absolute channel -> owner (0 = the show)
    const QSet<quint32> mine(m_ids.begin(), m_ids.end());   // these are the ones moving
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == NULL || !fx->isPatched() || mine.contains(fx->id()))
            continue;
        for (quint32 i = 0; i < fx->channels(); i++)
            taken.insert(fx->universeAddress() + i, 0);
    }

    const QList<PatchPlanner::Assignment> plan = currentPlan();
    QSet<int> bad;
    for (int r = 0; r < plan.size(); r++)
    {
        Fixture *fx = m_doc->fixture(plan.at(r).fixtureId);
        if (fx == NULL)
            continue;
        const quint32 base = (plan.at(r).universe << 9) | plan.at(r).address;
        const quint32 nch = rowChannels(r);
        bool over = plan.at(r).address + nch > 512;
        for (quint32 i = 0; i < nch && !over; i++)
            if (taken.contains(base + i))
                over = true;
        for (quint32 i = 0; i < nch; i++)
            taken.insert(base + i, plan.at(r).fixtureId);
        if (over)
            bad.insert(r);
    }

    int newUnis = 0;
    int changes = 0;
    QSet<quint32> seenNew;
    for (int r = 0; r < plan.size(); r++)
    {
        if (QTableWidgetItem *via = m_table->item(r, COL_VIA))
            via->setText(viaText(r, plan.at(r).universe));
        QTableWidgetItem *st = m_table->item(r, COL_STATUS);
        if (st == NULL)
            continue;
        QString text;
        Fixture *fx = m_doc->fixture(plan.at(r).fixtureId);
        const bool moved = fx != NULL && fx->isPatched()
                           && (fx->universe() != plan.at(r).universe || fx->address() != plan.at(r).address);
        if (fx != NULL && (!fx->isPatched() || moved))
            changes++;
        if (bad.contains(r))
            text = tr("CONFLICT");
        else if (fx != NULL && fx->isPatched() && !moved)
            text = tr("unchanged");
        else if (moved)
            text = tr("moves from %1.%2").arg(fx->universe() + 1).arg(fx->address() + 1);
        else if (plan.at(r).newUniverse)
            text = tr("new universe");
        else
            text = tr("ok");
        if (moved && plan.at(r).newUniverse && !bad.contains(r))
            text += tr(" (new universe)");
        const QString note = st->data(Qt::UserRole + 1).toString();
        if (!note.isEmpty() && !bad.contains(r))
            text += tr(" -- %1").arg(note);
        st->setText(text);
        st->setForeground(bad.contains(r) ? QBrush(Qt::red) : QBrush());
        if (plan.at(r).newUniverse && !seenNew.contains(plan.at(r).universe))
        {
            seenNew.insert(plan.at(r).universe);
            newUnis++;
        }
    }
    m_summary->setText(tr("%1 fixture(s) to patch or move, %2 new universe(s), %3 conflict(s).")
                           .arg(changes).arg(newUnis).arg(bad.size()));
    m_apply->setEnabled(bad.isEmpty() && changes > 0);
}

void PatchInventoryDialog::accept()
{
    /* Undoable (⌘Z on the plot or the Fixtures tab), wherever this was
       opened from: one rig undo step for the whole patch. */
    Monitor *mon = Monitor::instance();
    const QByteArray before = mon != nullptr ? mon->rigSnapshot() : QByteArray();
    PatchPlanner pp(m_doc);
    QStringList problems;
    const int done = pp.apply(currentPlan(), &problems);
    if (done > 0 && mon != nullptr)
        mon->pushRigUndo(before);
    if (done == 0 && !problems.isEmpty())
    {
        // Nothing changed: stay open, so the plan can be fixed and tried again.
        QMessageBox::warning(this, windowTitle(), problems.join("\n"));
        refreshConflicts();
        return;
    }
    if (!problems.isEmpty())
    {
        QMessageBox::warning(this, windowTitle(),
            tr("Patched %1 fixture(s). Things to look at:\n\n%2").arg(done).arg(problems.join("\n")));
    }
    QDialog::accept();
}

int PatchInventoryDialog::matchStructures(const QString &pattern)
{
    QStringList others;
    foreach (quint32 id, m_ids)
    {
        const QString key = PatchPlanner::mountKey(m_doc, id);
        if (!key.isEmpty() && key != pattern && !others.contains(key))
            others << key;
    }
    QStringList problems;
    QList<PatchPlanner::Assignment> plan = planner().matchLike(pattern, others, &problems);
    // Only the fixtures in this view.
    for (int i = plan.size() - 1; i >= 0; --i)
        if (!m_ids.contains(plan.at(i).fixtureId))
            plan.removeAt(i);
    if (m_matchPattern != nullptr)
    {
        const int at = m_matchPattern->findData(pattern);
        if (at >= 0)
            m_matchPattern->setCurrentIndex(at);
    }
    setRowAddresses(plan);
    m_matchProblems->setText(problems.join(QStringLiteral("\n")));
    m_matchProblems->setVisible(!problems.isEmpty());
    return plan.size();
}
