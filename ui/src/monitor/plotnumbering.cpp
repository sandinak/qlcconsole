/*
  Q Light Controller Plus
  plotnumbering.cpp

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

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSpinBox>
#include <QSet>
#include <QMap>
#include <algorithm>

#include "plotnumbering.h"
#include "fixturevisualtraits.h"
#include "monitorproperties.h"
#include "fixture.h"
#include "truss.h"

QList<quint32> PlotNumbering::ordered(Doc *doc, const QList<quint32> &fids, Order order)
{
    struct Key { quint32 fid; double x, y; quint32 truss; double along; };
    QList<Key> keys;
    MonitorProperties *props = doc != nullptr ? doc->monitorProperties() : nullptr;
    for (quint32 fid : fids)
    {
        if (doc == nullptr || doc->fixture(fid) == nullptr)
            continue;
        const QVector3D c = fixtureWorldCentre(doc, fid);
        Key k{ fid, double(c.x()), double(c.y()), Truss::invalidId(), 0.0 };
        if (props != nullptr)
        {
            const FixtureRigProps rp = props->fixtureRigProps(fid);
            k.truss = rp.trussId;
            k.along = double(rp.trussOffset);
        }
        keys << k;
    }
    // +X is stage left and +Y downstage: SR to SL is rising X, US to DS rising Y.
    std::stable_sort(keys.begin(), keys.end(), [order](const Key &a, const Key &b) {
        auto cmp = [](double p, double q) { return qAbs(p - q) < 1e-4 ? 0 : (p < q ? -1 : 1); };
        int r = 0;
        if (order == AlongTruss)
        {
            // Truss by truss (the first of them first), each along its own length;
            // what hangs on none comes last, left to right.
            const bool at = a.truss != Truss::invalidId(), bt = b.truss != Truss::invalidId();
            if (at != bt)
                return at;
            if (at && a.truss != b.truss)
                return a.truss < b.truss;
            r = at ? cmp(a.along, b.along) : cmp(a.x, b.x);
            if (r == 0)
                r = cmp(a.y, b.y);
        }
        else if (order == UpstageToDownstage)
        {
            r = cmp(a.y, b.y);
            if (r == 0)
                r = cmp(a.x, b.x);
        }
        else
        {
            r = cmp(a.x, b.x);
            if (r == 0)
                r = cmp(a.y, b.y);
        }
        if (r != 0)
            return r < 0;
        return a.fid < b.fid;
    });
    QList<quint32> out;
    for (const Key &k : keys)
        out << k.fid;
    return out;
}

QList<PlotNumbering::Row> PlotNumbering::plan(Doc *doc, const QList<quint32> &fids, Order order,
                                              bool setUnit, int firstUnit, bool setChannel,
                                              int firstChannel, bool patch)
{
    QList<Row> rows;
    const QList<quint32> list = ordered(doc, fids, order);
    int unit = firstUnit, channel = firstChannel;
    for (quint32 fid : list)
    {
        Row r;
        r.fixture = fid;
        if (setUnit)
            r.unit = unit++;
        if (setChannel)
            r.channel = channel++;
        rows << r;
    }
    if (!patch || doc == nullptr)
        return rows;

    // What the others hold: universe -> taken channels.
    const QSet<quint32> mine(list.begin(), list.end());
    QMap<quint32, QSet<quint32> > taken;
    quint32 startUni = 0, startAddr = 0;
    bool haveStart = false;
    for (Fixture *fx : doc->fixtures())
    {
        if (fx == nullptr || !fx->isPatched())
            continue;
        if (mine.contains(fx->id()))
        {
            if (!haveStart || fx->universe() < startUni
                || (fx->universe() == startUni && fx->address() < startAddr))
            {
                startUni = fx->universe();
                startAddr = fx->address();
                haveStart = true;
            }
            continue;
        }
        for (quint32 c = 0; c < fx->channels(); ++c)
            taken[fx->universe()].insert(fx->address() + c);
    }
    quint32 uni = startUni, cursor = startAddr;
    for (Row &r : rows)
    {
        const Fixture *fx = doc->fixture(r.fixture);
        const quint32 n = fx != nullptr ? fx->channels() : 0;
        if (n == 0)
            continue;
        bool placed = false;
        for (quint32 u = uni; u < uni + 4 && !placed; ++u)
        {
            for (quint32 a = (u == uni ? cursor : 0); a + n <= 512 && !placed; ++a)
            {
                bool free = true;
                for (quint32 c = 0; c < n && free; ++c)
                    free = !taken[u].contains(a + c);
                if (!free)
                    continue;
                r.patch = true;
                r.universe = u;
                r.address = a;
                for (quint32 c = 0; c < n; ++c)
                    taken[u].insert(a + c);
                uni = u;
                cursor = a + n;
                placed = true;
            }
        }
    }
    return rows;
}

bool PlotNumbering::apply(Doc *doc, const QList<Row> &rows, QString *why)
{
    if (doc == nullptr)
        return false;
    QList<Doc::FixturePatch> patches;
    for (const Row &r : rows)
        if (r.patch)
            patches << Doc::FixturePatch{ r.fixture, r.universe, r.address };
    if (!patches.isEmpty() && !doc->patchFixtures(patches, why))
        return false;
    MonitorProperties *props = doc->monitorProperties();
    for (const Row &r : rows)
    {
        if (r.unit > 0)
            props->setFixtureUnit(r.fixture, r.unit);
        if (r.channel > 0)
            props->setFixtureChannel(r.fixture, r.channel);
    }
    doc->setModified();
    return true;
}

NumberInPlotOrderDialog::NumberInPlotOrderDialog(Doc *doc, const QList<quint32> &fids, QWidget *parent)
    : QDialog(parent), m_doc(doc), m_fids(fids)
{
    setObjectName(QStringLiteral("numberInPlotOrder"));
    setWindowTitle(tr("Number in Plot Order"));
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel(tr("Number the %1 selected fixtures by where they stand on the plot.")
                                  .arg(fids.size()), this));
    QFormLayout *form = new QFormLayout;
    m_order = new QComboBox(this);
    m_order->setObjectName(QStringLiteral("numberOrder"));
    m_order->addItem(tr("Stage right to stage left"), int(PlotNumbering::StageRightToLeft));
    m_order->addItem(tr("Upstage to downstage"), int(PlotNumbering::UpstageToDownstage));
    m_order->addItem(tr("Along the truss"), int(PlotNumbering::AlongTruss));
    form->addRow(tr("Order:"), m_order);
    m_unitOn = new QCheckBox(tr("Unit # from"), this);
    m_unitOn->setObjectName(QStringLiteral("numberUnitOn"));
    m_unitOn->setChecked(true);
    m_unit = new QSpinBox(this);
    m_unit->setObjectName(QStringLiteral("numberUnitFrom"));
    m_unit->setRange(1, 99999);
    form->addRow(m_unitOn, m_unit);
    m_channelOn = new QCheckBox(tr("Channel from"), this);
    m_channelOn->setObjectName(QStringLiteral("numberChannelOn"));
    m_channel = new QSpinBox(this);
    m_channel->setObjectName(QStringLiteral("numberChannelFrom"));
    m_channel->setRange(1, 99999);
    form->addRow(m_channelOn, m_channel);
    lay->addLayout(form);
    m_patchOn = new QCheckBox(tr("Patch them in that order, into the next free addresses"), this);
    m_patchOn->setObjectName(QStringLiteral("numberPatchOn"));
    lay->addWidget(m_patchOn);

    m_table = new QTableWidget(0, 4, this);
    m_table->setObjectName(QStringLiteral("numberPreview"));
    m_table->setHorizontalHeaderLabels({ tr("Fixture"), tr("Unit #"), tr("Channel"), tr("Address") });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setMinimumHeight(160);
    lay->addWidget(m_table, 1);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(box);

    connect(m_order, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NumberInPlotOrderDialog::refresh);
    connect(m_unitOn, &QCheckBox::toggled, this, &NumberInPlotOrderDialog::refresh);
    connect(m_channelOn, &QCheckBox::toggled, this, &NumberInPlotOrderDialog::refresh);
    connect(m_patchOn, &QCheckBox::toggled, this, &NumberInPlotOrderDialog::refresh);
    connect(m_unit, QOverload<int>::of(&QSpinBox::valueChanged), this, &NumberInPlotOrderDialog::refresh);
    connect(m_channel, QOverload<int>::of(&QSpinBox::valueChanged), this, &NumberInPlotOrderDialog::refresh);
    resize(520, 480);
    refresh();
}

QList<PlotNumbering::Row> NumberInPlotOrderDialog::rows() const
{
    return PlotNumbering::plan(m_doc, m_fids, PlotNumbering::Order(m_order->currentData().toInt()),
                               m_unitOn->isChecked(), m_unit->value(),
                               m_channelOn->isChecked(), m_channel->value(), m_patchOn->isChecked());
}

void NumberInPlotOrderDialog::refresh()
{
    const QList<PlotNumbering::Row> list = rows();
    m_table->setRowCount(list.size());
    for (int i = 0; i < list.size(); ++i)
    {
        const PlotNumbering::Row &r = list.at(i);
        const Fixture *fx = m_doc->fixture(r.fixture);
        auto cell = [&](int col, const QString &text) {
            m_table->setItem(i, col, new QTableWidgetItem(text));
        };
        cell(0, fx != nullptr ? fx->name() : QString());
        cell(1, r.unit > 0 ? QString::number(r.unit) : QString());
        cell(2, r.channel > 0 ? QString::number(r.channel) : QString());
        cell(3, !m_patchOn->isChecked() ? QString()
                : r.patch ? tr("%1.%2").arg(r.universe + 1).arg(r.address + 1)
                          : tr("no room"));
    }
}
