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

#include <QComboBox>
#include <QStandardItemModel>
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "patchinventorydialog.h"
#include "fixture.h"
#include "doc.h"

#define COL_NAME    0
#define COL_MODEL   1
#define COL_MODE    2
#define COL_CH      3
#define COL_CONN    4
#define COL_UNI     5
#define COL_ADDR    6
#define COL_STATUS  7

static bool isGenericDimmer(const Fixture *fx)
{
    return fx->fixtureDef() == NULL
        || (fx->fixtureDef()->manufacturer() == KXMLFixtureGeneric
            && fx->fixtureDef()->model() == KXMLFixtureGeneric);
}

PatchInventoryDialog::PatchInventoryDialog(Doc *doc, const QList<quint32> &fixtureIds, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_filling(false)
{
    setWindowTitle(tr("Patch Inventory"));
    resize(900, 520);

    foreach (quint32 id, fixtureIds)
        m_ids << id;
    if (m_ids.isEmpty())
    {
        foreach (Fixture *fx, m_doc->fixtures())
            if (fx != NULL && !fx->isPatched())
                m_ids << fx->id();
    }

    QVBoxLayout *root = new QVBoxLayout(this);
    QLabel *intro = new QLabel(tr("Proposed homes for the fixtures staged in the inventory. DMX "
                                  "fixtures are packed into the first free channels; each network "
                                  "fixture IP gets a universe of its own (unicast). Pick each "
                                  "fixture's mode here -- it sets the DMX footprint -- and edit any "
                                  "universe or address, then Apply. A fixture that a scene sets "
                                  "channel-by-channel keeps its mode."), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    m_table = new QTableWidget(0, 8, this);
    m_table->setHorizontalHeaderLabels({ tr("Fixture"), tr("Model"), tr("Mode"), tr("Ch"), tr("Connection"),
                                         tr("Universe"), tr("Address"), tr("Status") });
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(COL_NAME, QHeaderView::Stretch);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    root->addWidget(m_table, 1);

    m_summary = new QLabel(this);
    root->addWidget(m_summary);

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
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    fill(PatchPlanner(m_doc).plan(m_ids));
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
    fill(PatchPlanner(m_doc).plan(m_ids, channelOverrides()), modes, dimmers);
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
    for (int r = 0; r < plan.size(); r++)
    {
        const PatchPlanner::Assignment &a = plan.at(r);
        Fixture *fx = m_doc->fixture(a.fixtureId);
        if (fx == NULL)
            continue;

        QTableWidgetItem *name = new QTableWidgetItem(fx->name());
        name->setData(Qt::UserRole, a.fixtureId);
        name->setToolTip(a.note);
        name->setFlags(Qt::ItemIsEnabled);
        m_table->setItem(r, COL_NAME, name);

        QString model = fx->fixtureDef() ? fx->fixtureDef()->model() : tr("Generic");
        QTableWidgetItem *mi = new QTableWidgetItem(model);
        mi->setFlags(Qt::ItemIsEnabled);
        m_table->setItem(r, COL_MODEL, mi);

        // Mode: it decides the DMX footprint, so it is chosen here, at patch
        // time. Modes a fixture cannot move to (a scene sets its channels
        // directly, or the head count differs) are shown but disabled, with why.
        quint32 chans = fx->channels();
        QString why;
        if (isGenericDimmer(fx))
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
            mo->setFlags(Qt::ItemIsEnabled);
            m_table->setItem(r, COL_MODE, mo);
        }

        QTableWidgetItem *ch = new QTableWidgetItem(QString::number(chans));
        ch->setData(Qt::UserRole, chans);
        ch->setFlags(Qt::ItemIsEnabled);
        ch->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(r, COL_CH, ch);

        QString conn = fx->connection() == Fixture::Network
                           ? (fx->ipAddress().isEmpty() ? tr("Network") : tr("Network %1").arg(fx->ipAddress()))
                           : tr("DMX");
        QTableWidgetItem *ci = new QTableWidgetItem(conn);
        ci->setFlags(Qt::ItemIsEnabled);
        m_table->setItem(r, COL_CONN, ci);

        QSpinBox *uni = new QSpinBox(m_table);
        uni->setRange(1, 999);
        uni->setValue(int(a.universe) + 1);
        connect(uni, QOverload<int>::of(&QSpinBox::valueChanged), this, &PatchInventoryDialog::slotCellEdited);
        m_table->setCellWidget(r, COL_UNI, uni);

        QSpinBox *addr = new QSpinBox(m_table);
        addr->setRange(1, 512);
        addr->setValue(int(a.address) + 1);
        connect(addr, QOverload<int>::of(&QSpinBox::valueChanged), this, &PatchInventoryDialog::slotCellEdited);
        m_table->setCellWidget(r, COL_ADDR, addr);

        QTableWidgetItem *st = new QTableWidgetItem;
        st->setFlags(Qt::ItemIsEnabled);
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
        QSpinBox *uni = qobject_cast<QSpinBox *>(m_table->cellWidget(r, COL_UNI));
        QSpinBox *addr = qobject_cast<QSpinBox *>(m_table->cellWidget(r, COL_ADDR));
        if (name == NULL || uni == NULL || addr == NULL)
            continue;
        PatchPlanner::Assignment a;
        a.fixtureId = name->data(Qt::UserRole).toUInt();
        a.universe = quint32(uni->value() - 1);
        a.address = quint32(addr->value() - 1);
        a.newUniverse = (a.universe >= existing);
        if (QTableWidgetItem *st = m_table->item(r, COL_STATUS))
            a.note = st->data(Qt::UserRole + 1).toString();
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
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (fx == NULL || !fx->isPatched())
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
    QSet<quint32> seenNew;
    for (int r = 0; r < plan.size(); r++)
    {
        QTableWidgetItem *st = m_table->item(r, COL_STATUS);
        if (st == NULL)
            continue;
        QString text;
        if (bad.contains(r))
            text = tr("CONFLICT");
        else if (plan.at(r).newUniverse)
            text = tr("new universe");
        else
            text = tr("ok");
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
    m_summary->setText(tr("%1 fixture(s) to patch, %2 new universe(s), %3 conflict(s).")
                           .arg(plan.size()).arg(newUnis).arg(bad.size()));
    m_apply->setEnabled(bad.isEmpty() && !plan.isEmpty());
}

void PatchInventoryDialog::accept()
{
    PatchPlanner pp(m_doc);
    QStringList problems;
    const int done = pp.apply(currentPlan(), &problems);
    if (!problems.isEmpty())
    {
        QMessageBox::warning(this, tr("Patch Inventory"),
            tr("Patched %1 fixture(s). Things to look at:\n\n%2").arg(done).arg(problems.join("\n")));
    }
    QDialog::accept();
}
