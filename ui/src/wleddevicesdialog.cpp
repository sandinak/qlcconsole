/*
  Q Light Controller Plus
  wleddevicesdialog.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QRegularExpression>
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QHostAddress>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCompleter>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>
#include <QSet>

#include "wleddevicesdialog.h"
#include "inputoutputmap.h"
#include "qlcioplugin.h"
#include "outputpatch.h"
#include "patchundo.h"
#include "universe.h"
#include "fixture.h"
#include "doc.h"

#define KEY_IP       "outputIP"
#define KEY_ADDRESS  "wledAddress"
#define KEY_CHANNELS "wledChannels"

/* As the WLED output allows: "outputIP" .. "outputIP16". */
static const int kMaxDevices = 16;

namespace
{
    QString key(const char *base, int index)
    {
        return index == 0 ? QString::fromLatin1(base)
                          : QString::fromLatin1(base) + QString::number(index + 1);
    }

    int channelsFrom(const QVariant &v)
    {
        const int n = v.toInt();
        return n >= 15 ? 15 : n >= 4 ? 4 : 3;
    }

    /** Every device's address and channels spelled out: what it reads. */
    QList<WledDevicesDialog::Device> resolved(const QList<WledDevicesDialog::Device> &in)
    {
        QList<WledDevicesDialog::Device> out = in;
        if (out.isEmpty())
            return out;
        for (int i = 1; i < out.count(); i++)
        {
            if (out[i].address <= 0)
                out[i].address = out.first().address;
            if (out[i].channels <= 0)
                out[i].channels = out.first().channels;
        }
        return out;
    }

    /** Back to "the primary's" wherever a backup reads what the primary does. */
    QList<WledDevicesDialog::Device> normalised(const QList<WledDevicesDialog::Device> &in)
    {
        QList<WledDevicesDialog::Device> out = in;
        for (int i = 1; i < out.count(); i++)
        {
            if (out[i].address == out.first().address)
                out[i].address = 0;
            if (out[i].channels == out.first().channels)
                out[i].channels = 0;
        }
        return out;
    }
}

/*********************************************************************
 * Parameters
 *********************************************************************/

bool WledDevicesDialog::isDeviceKey(const QString &k)
{
    static const QRegularExpression re(QStringLiteral("^(outputIP|wledAddress|wledChannels)([2-9]|1[0-6])?$"));
    return re.match(k).hasMatch();
}

QList<WledDevicesDialog::Device> WledDevicesDialog::fromParameters(const QMap<QString, QVariant> &params)
{
    QList<Device> out;
    Device primary;
    primary.ip = params.value(KEY_IP).toString().trimmed();
    primary.address = qBound(1, params.value(KEY_ADDRESS, 1).toInt(), 512);
    primary.channels = channelsFrom(params.value(KEY_CHANNELS, 15));
    out << primary;
    for (int i = 1; i < kMaxDevices; i++)
    {
        const QString ip = params.value(key(KEY_IP, i)).toString().trimmed();
        if (ip.isEmpty())
            continue;               // numbered from 2, but a gap is no reason to drop the rest
        Device d;
        d.ip = ip;
        if (params.contains(key(KEY_ADDRESS, i)))
            d.address = qBound(1, params.value(key(KEY_ADDRESS, i)).toInt(), 512);
        if (params.contains(key(KEY_CHANNELS, i)))
            d.channels = channelsFrom(params.value(key(KEY_CHANNELS, i)));
        out << d;
    }
    return normalised(out);
}

QMap<QString, QVariant> WledDevicesDialog::toParameters(const QList<Device> &devices)
{
    QMap<QString, QVariant> out;
    const QList<Device> list = normalised(devices);
    for (int i = 0; i < list.count() && i < kMaxDevices; i++)
    {
        const Device &d = list.at(i);
        out[key(KEY_IP, i)] = d.ip;
        if (i == 0 || d.address > 0)
            out[key(KEY_ADDRESS, i)] = qMax(1, d.address);
        if (i == 0 || d.channels > 0)
            out[key(KEY_CHANNELS, i)] = d.channels > 0 ? d.channels : 15;
    }
    return out;
}

void WledDevicesDialog::apply(OutputPatch *op, const QList<Device> &devices)
{
    if (op == nullptr)
        return;
    const QMap<QString, QVariant> before = op->getPluginParameters();
    const QMap<QString, QVariant> after = toParameters(devices);

    foreach (const QString &k, before.keys())
        if (isDeviceKey(k) && !after.contains(k))
            op->unSetPluginParameter(k);
    for (auto it = after.constBegin(); it != after.constEnd(); ++it)
    {
        /* A show patched by hand may never have said the primary's address
           or channel count: the defaults are what it means, and writing
           them now would change a file nothing was changed in. */
        if (!before.contains(it.key())
            && ((it.key() == QLatin1String(KEY_ADDRESS) && it.value().toInt() == 1)
                || (it.key() == QLatin1String(KEY_CHANNELS) && it.value().toInt() == 15)))
            continue;
        if (before.contains(it.key()) && before.value(it.key()).toString() == it.value().toString())
            continue;
        op->setPluginParameter(it.key(), it.value());
    }
}

bool WledDevicesDialog::validAddress(const QString &ip)
{
    QString host = ip.trimmed();
    if (host.isEmpty())
        return false;
    // "host:port" -- but a bare IPv6 address is all colons.
    const int colon = host.lastIndexOf(QLatin1Char(':'));
    if (colon > 0 && host.count(QLatin1Char(':')) == 1)
    {
        bool ok = false;
        const int port = host.mid(colon + 1).toInt(&ok);
        if (!ok || port < 1 || port > 65535)
            return false;
        host = host.left(colon);
    }
    else if (host.startsWith(QLatin1Char('[')))
    {
        const int close = host.indexOf(QLatin1Char(']'));
        if (close < 0)
            return false;
        const QString rest = host.mid(close + 1);
        if (!rest.isEmpty())
        {
            bool ok = false;
            const int port = rest.startsWith(QLatin1Char(':')) ? rest.mid(1).toInt(&ok) : 0;
            if (!ok || port < 1 || port > 65535)
                return false;
        }
        host = host.mid(1, close - 1);
    }
    QHostAddress addr;
    if (addr.setAddress(host))
        return true;
    // Dotted digits that are not an address are a typo, not a host name.
    static const QRegularExpression digits(QStringLiteral("^[0-9.]+$"));
    if (digits.match(host).hasMatch())
        return false;
    static const QRegularExpression name(QStringLiteral(
        "^[A-Za-z0-9]([A-Za-z0-9-]{0,62}[A-Za-z0-9])?(\\.[A-Za-z0-9]([A-Za-z0-9-]{0,62}[A-Za-z0-9])?)*\\.?$"));
    return name.match(host).hasMatch();
}

QString WledDevicesDialog::problem(const QList<Device> &devices,
                                   const QHash<QString, quint32> &usedElsewhere)
{
    if (devices.isEmpty())
        return tr("Add the device this universe is sent to.");
    if (devices.count() > kMaxDevices)
        return tr("At most %1 devices per universe.").arg(kMaxDevices);
    QSet<QString> seen;
    for (int i = 0; i < devices.count(); i++)
    {
        const QString ip = devices.at(i).ip.trimmed();
        const QString which = i == 0 ? tr("The primary") : tr("Backup %1").arg(i);
        if (ip.isEmpty())
            return tr("%1 has no IP address: fill it in, or remove it.").arg(which);
        if (!validAddress(ip))
            return tr("%1: \"%2\" is not an IP address or host name.").arg(which, ip);
        if (seen.contains(ip.toLower()))
            return tr("%1 is listed twice.").arg(ip);
        seen.insert(ip.toLower());
        if (usedElsewhere.contains(ip))
            return tr("%1 is already sent universe %2: a device follows one universe.")
                       .arg(ip).arg(usedElsewhere.value(ip) + 1);
    }
    return QString();
}

/*********************************************************************
 * Editing a patch
 *********************************************************************/

bool WledDevicesDialog::edit(Doc *doc, quint32 universe, quint32 line, QWidget *parent)
{
    InputOutputMap *io = doc != nullptr ? doc->inputOutputMap() : nullptr;
    Universe *uni = io != nullptr ? io->universe(universe) : nullptr;
    if (uni == nullptr)
        return false;
    OutputPatch *op = nullptr;
    for (int i = 0; i < uni->outputPatchesCount() && op == nullptr; i++)
    {
        OutputPatch *p = uni->outputPatch(i);
        if (p != nullptr && p->pluginName() == QLatin1String("WLED")
            && (p->isPending() || p->output() == line))
            op = p;
    }
    if (op == nullptr)
        return false;

    /* Every other WLED universe's devices: one device, one universe -- two
       universes sending one lantern take turns at it. */
    QHash<QString, quint32> elsewhere;
    for (quint32 u = 0; u < io->universesCount(); u++)
    {
        Universe *other = io->universe(u);
        if (u == universe || other == nullptr)
            continue;
        for (int i = 0; i < other->outputPatchesCount(); i++)
        {
            OutputPatch *p = other->outputPatch(i);
            if (p == nullptr || p->pluginName() != QLatin1String("WLED"))
                continue;
            foreach (const Device &d, fromParameters(p->getPluginParameters()))
                if (!d.ip.isEmpty())
                    elsewhere.insert(d.ip, u);
        }
    }

    /* WLED has no discovery of its own; what the show already knows of are
       its WLED fixtures' addresses. */
    QStringList suggestions;
    foreach (Fixture *fx, doc->fixtures())
        if (fx != nullptr && fx->connection() == Fixture::Network
            && fx->transport() == Fixture::WledTransport && !fx->ipAddress().isEmpty()
            && !elsewhere.contains(fx->ipAddress()) && !suggestions.contains(fx->ipAddress()))
            suggestions << fx->ipAddress();
    suggestions.sort();

    const QList<Device> before = fromParameters(op->getPluginParameters());
    WledDevicesDialog dlg(before, suggestions, elsewhere, parent);
    dlg.setWindowTitle(tr("WLED devices — universe %1").arg(universe + 1));
    if (dlg.exec() != QDialog::Accepted || dlg.devices() == before)
        return false;

    if (io->patchUndo() != nullptr)
        io->patchUndo()->capture(QList<quint32>() << universe,
                                 tr("WLED devices of universe %1").arg(universe + 1));
    apply(op, dlg.devices());
    doc->setModified();
    return true;
}

/*********************************************************************
 * The dialog
 *********************************************************************/

WledDevicesDialog::WledDevicesDialog(const QList<Device> &devices, const QStringList &suggestions,
                                     const QHash<QString, quint32> &usedElsewhere, QWidget *parent)
    : QDialog(parent)
    , m_devices(normalised(devices))
    , m_suggestions(suggestions)
    , m_usedElsewhere(usedElsewhere)
{
    setWindowTitle(tr("WLED devices"));
    if (m_devices.isEmpty())
    {
        Device d;
        d.address = 1;
        d.channels = 15;
        m_devices << d;
    }

    QVBoxLayout *lay = new QVBoxLayout(this);
    QLabel *intro = new QLabel(tr("The universe's look goes to every device here, the same "
                                  "state to each: a <b>backup</b> lantern mirrors the "
                                  "<b>primary</b> without programming it twice. A device "
                                  "that does not answer never holds up the others."), this);
    intro->setWordWrap(true);
    lay->addWidget(intro);

    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels(QStringList() << tr("IP address") << tr("Address")
                                                     << tr("Channels"));
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setMinimumWidth(460);
    lay->addWidget(m_table);

    QHBoxLayout *row = new QHBoxLayout();
    m_add = new QPushButton(tr("Add Backup"), this);
    m_remove = new QPushButton(tr("Remove"), this);
    m_up = new QPushButton(tr("Move Up"), this);
    m_down = new QPushButton(tr("Move Down"), this);
    m_up->setToolTip(tr("To the top, it is the primary: the one older versions send to, "
                        "and whose address and channels the backups read unless they say otherwise."));
    row->addWidget(m_add);
    row->addWidget(m_remove);
    row->addStretch(1);
    row->addWidget(m_up);
    row->addWidget(m_down);
    lay->addLayout(row);

    m_problem = new QLabel(this);
    m_problem->setWordWrap(true);
    m_problem->setStyleSheet(QStringLiteral("color: #d9534f;"));
    lay->addWidget(m_problem);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    lay->addWidget(m_buttons);

    connect(m_add, &QPushButton::clicked, this, [this]() { addDevice(); });
    connect(m_remove, &QPushButton::clicked, this, [this]() { removeDevice(currentRow()); });
    connect(m_up, &QPushButton::clicked, this, [this]() { moveDevice(currentRow(), -1); });
    connect(m_down, &QPushButton::clicked, this, [this]() { moveDevice(currentRow(), 1); });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &WledDevicesDialog::updateState);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &WledDevicesDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &WledDevicesDialog::reject);

    rebuild();
}

int WledDevicesDialog::currentRow() const
{
    const QModelIndexList sel = m_table->selectionModel()->selectedRows();
    return sel.isEmpty() ? -1 : sel.first().row();
}

void WledDevicesDialog::rebuild()
{
    const int keep = currentRow();
    m_table->setRowCount(0);
    m_table->setRowCount(m_devices.count());
    QStringList rowNames;
    for (int i = 0; i < m_devices.count(); i++)
    {
        const Device &d = m_devices.at(i);
        rowNames << (i == 0 ? tr("Primary") : tr("Backup %1").arg(i));

        QLineEdit *ip = new QLineEdit(d.ip, m_table);
        ip->setPlaceholderText(tr("e.g. 172.18.2.51"));
        if (!m_suggestions.isEmpty())
        {
            QCompleter *c = new QCompleter(m_suggestions, ip);
            c->setCaseSensitivity(Qt::CaseInsensitive);
            ip->setCompleter(c);
        }
        connect(ip, &QLineEdit::textChanged, this, [this, i](const QString &t) {
            if (i < m_devices.count())
                m_devices[i].ip = t.trimmed();
            updateState();
        });
        m_table->setCellWidget(i, 0, ip);

        QSpinBox *addr = new QSpinBox(m_table);
        addr->setRange(i == 0 ? 1 : 0, 512);
        if (i > 0)
            addr->setSpecialValueText(tr("As primary"));
        addr->setValue(d.address);
        addr->setToolTip(tr("The first of its channels in the universe"));
        connect(addr, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, i](int v) {
            if (i < m_devices.count())
                m_devices[i].address = v;
            updateState();
        });
        m_table->setCellWidget(i, 1, addr);

        QComboBox *ch = new QComboBox(m_table);
        if (i > 0)
            ch->addItem(tr("As primary"), 0);
        ch->addItem(tr("15 (Effect)"), 15);
        ch->addItem(tr("4 (DRGB)"), 4);
        ch->addItem(tr("3 (RGB)"), 3);
        ch->setCurrentIndex(qMax(0, ch->findData(d.channels)));
        connect(ch, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, i, ch](int) {
            if (i < m_devices.count())
                m_devices[i].channels = ch->currentData().toInt();
            updateState();
        });
        m_table->setCellWidget(i, 2, ch);
    }
    m_table->setVerticalHeaderLabels(rowNames);
    if (keep >= 0 && keep < m_devices.count())
        m_table->selectRow(keep);
    updateState();
}

void WledDevicesDialog::updateState()
{
    const int row = currentRow();
    m_add->setEnabled(m_devices.count() < kMaxDevices);
    m_remove->setEnabled(row >= 0 && m_devices.count() > 1);
    m_up->setEnabled(row > 0);
    m_down->setEnabled(row >= 0 && row < m_devices.count() - 1);
    const QString why = problemText();
    m_problem->setText(why);
    m_problem->setVisible(!why.isEmpty());
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(why.isEmpty());
}

QString WledDevicesDialog::problemText() const
{
    return problem(m_devices, m_usedElsewhere);
}

void WledDevicesDialog::addDevice(const QString &ip)
{
    if (m_devices.count() >= kMaxDevices)
        return;
    Device d;
    d.ip = ip;
    m_devices << d;
    rebuild();
    m_table->selectRow(m_devices.count() - 1);
    if (QLineEdit *e = qobject_cast<QLineEdit *>(m_table->cellWidget(m_devices.count() - 1, 0)))
        e->setFocus();
}

void WledDevicesDialog::removeDevice(int row)
{
    if (row < 0 || row >= m_devices.count() || m_devices.count() <= 1)
        return;
    // The backups keep reading what they read, whoever is the primary now.
    QList<Device> all = resolved(m_devices);
    all.removeAt(row);
    m_devices = normalised(all);
    rebuild();
    m_table->selectRow(qMin(row, m_devices.count() - 1));
}

void WledDevicesDialog::moveDevice(int row, int delta)
{
    const int to = row + delta;
    if (row < 0 || row >= m_devices.count() || to < 0 || to >= m_devices.count())
        return;
    QList<Device> all = resolved(m_devices);
    all.move(row, to);
    m_devices = normalised(all);
    rebuild();
    m_table->selectRow(to);
}

void WledDevicesDialog::setDeviceIP(int row, const QString &ip)
{
    if (QLineEdit *e = qobject_cast<QLineEdit *>(m_table->cellWidget(row, 0)))
        e->setText(ip);
}

void WledDevicesDialog::accept()
{
    if (!problemText().isEmpty())
        return;
    for (Device &d : m_devices)
        d.ip = d.ip.trimmed();
    QDialog::accept();
}
