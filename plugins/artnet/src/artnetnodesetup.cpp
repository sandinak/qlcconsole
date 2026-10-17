/*
  Q Light Controller Plus
  artnetnodesetup.cpp

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
#include <QRadioButton>
#include <QButtonGroup>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QTimer>

#include "artnetnodesetup.h"
#include "artnetplugin.h"

#define ANSWER_WAIT_MS 3000

ArtNetNodeSetup::ArtNetNodeSetup(ArtNetPlugin *plugin, const QString &address, QWidget *parent)
    : QDialog(parent)
    , m_plugin(plugin)
    , m_address(address)
    , m_reachable(false)
{
    setWindowTitle(tr("Art-Net node network settings"));
    setProperty("helpTopic", QStringLiteral("other-improvements.md#setting-up-a-new-art-net-node"));

    ArtNetNodeInfo info;
    int heardOn = -1;
    const bool known = m_plugin->findNode(address, &info, &heardOn);
    if (known)
    {
        m_mac = info.macAddress;
        m_nodeName = info.longName.isEmpty() ? info.shortName : info.longName;
    }

    const QHostAddress ip(address);
    const int reachLine = m_plugin->lineReaching(ip);
    m_reachable = reachLine >= 0;
    const QList<ArtNetIO> lines = m_plugin->getIOMapping();
    const int viaLine = m_reachable ? reachLine : heardOn;
    QString ifaceName;
    if (viaLine >= 0 && viaLine < lines.count())
    {
#if defined(Q_OS_WIN)
        ifaceName = lines.at(viaLine).iface.humanReadableName();
#else
        ifaceName = lines.at(viaLine).iface.name();
#endif
        m_viaText = QString("%1 (%2)").arg(lines.at(viaLine).iface.humanReadableName(),
                                           lines.at(viaLine).address.ip().toString());
    }

    QVBoxLayout *top = new QVBoxLayout(this);

    m_header = new QLabel(this);
    m_header->setObjectName("header");
    m_header->setTextFormat(Qt::PlainText);
    QStringList head;
    head << (m_nodeName.isEmpty() ? tr("Art-Net node") : m_nodeName);
    head << tr("Address now: %1").arg(address);
    if (m_mac.isEmpty() == false)
        head << tr("MAC: %1").arg(m_mac);
    if (heardOn >= 0 && heardOn < lines.count())
        head << tr("Heard on: %1").arg(lines.at(heardOn).iface.humanReadableName());
    m_header->setText(head.join("\n"));
    top->addWidget(m_header);

    m_reach = new QLabel(this);
    m_reach->setObjectName("reach");
    m_reach->setWordWrap(true);
    if (m_reachable)
    {
        m_reach->setText(tr("This computer reaches it directly through %1.").arg(m_viaText));
    }
    else
    {
        m_reach->setText(tr("<b>Not on this network: %1.</b><br>"
                            "The node answered, but %2 is on no network this computer "
                            "has an address on, so nothing can be sent to it directly. "
                            "Art-Net sets a node's address with a message sent to the "
                            "node's own address, and that message says nothing about "
                            "which node it is for -- broadcast, every node on the cable "
                            "would take the new address -- so it is never broadcast "
                            "from here.<br><br>"
                            "To set this node up: give this computer a temporary address "
                            "on the node's network (below, needs an administrator), press "
                            "<b>Rescan</b> in Connections, then open this again.")
                         .arg(ArtNetPlugin::attentionFor(ip, false).toHtmlEscaped(),
                              address.toHtmlEscaped()));
    }
    top->addWidget(m_reach);

    /* The temporary address. Shown, never run: it needs administrator rights
       and changes this computer's network, which is the operator's call. */
    m_aliasBox = new QGroupBox(tr("Temporary address for this computer"), this);
    m_aliasBox->setObjectName("aliasBox");
    {
        const QHostAddress mask = guessMask(ip);
        const QHostAddress alias = suggestAlias(ip, mask);
        QFormLayout *af = new QFormLayout(m_aliasBox);
        m_aliasAdd = new QLineEdit(aliasCommand(ifaceName, alias, mask, true), m_aliasBox);
        m_aliasAdd->setObjectName("aliasAdd");
        m_aliasAdd->setReadOnly(true);
        m_aliasRemove = new QLineEdit(aliasCommand(ifaceName, alias, mask, false), m_aliasBox);
        m_aliasRemove->setObjectName("aliasRemove");
        m_aliasRemove->setReadOnly(true);
        af->addRow(tr("Add:"), m_aliasAdd);
        af->addRow(tr("Remove afterwards:"), m_aliasRemove);
    }
    m_aliasBox->setVisible(m_reachable == false);
    top->addWidget(m_aliasBox);

    QGroupBox *settings = new QGroupBox(tr("New settings"), this);
    QFormLayout *form = new QFormLayout(settings);
    m_static = new QRadioButton(tr("Fixed address"), settings);
    m_static->setObjectName("modeStatic");
    m_dhcp = new QRadioButton(tr("DHCP"), settings);
    m_dhcp->setObjectName("modeDhcp");
    m_defaults = new QRadioButton(tr("Factory default address"), settings);
    m_defaults->setObjectName("modeDefaults");
    QButtonGroup *modes = new QButtonGroup(this);
    modes->addButton(m_static);
    modes->addButton(m_dhcp);
    modes->addButton(m_defaults);
    QHBoxLayout *modeRow = new QHBoxLayout;
    modeRow->addWidget(m_static);
    modeRow->addWidget(m_dhcp);
    modeRow->addWidget(m_defaults);
    form->addRow(tr("Mode:"), modeRow);

    m_ip = new QLineEdit(address, settings);
    m_ip->setObjectName("ip");
    m_mask = new QLineEdit(m_reachable ? lines.at(reachLine).address.netmask().toString()
                                       : guessMask(ip).toString(), settings);
    m_mask->setObjectName("mask");
    m_setGateway = new QCheckBox(tr("Set"), settings);
    m_setGateway->setObjectName("setGateway");
    m_gateway = new QLineEdit(settings);
    m_gateway->setObjectName("gateway");
    m_gateway->setPlaceholderText(tr("unchanged"));
    QHBoxLayout *gwRow = new QHBoxLayout;
    gwRow->addWidget(m_setGateway);
    gwRow->addWidget(m_gateway);
    form->addRow(tr("IP address:"), m_ip);
    form->addRow(tr("Subnet mask:"), m_mask);
    form->addRow(tr("Gateway:"), gwRow);
    top->addWidget(settings);

    /* Typing an address on one of this computer's networks takes that
       network's mask, until the mask is edited by hand. */
    connect(m_ip, &QLineEdit::textEdited, this, [this](const QString &text)
    {
        if (m_mask->property("edited").toBool())
            return;
        const int l = m_plugin->lineReaching(QHostAddress(text.trimmed()));
        if (l >= 0)
            m_mask->setText(m_plugin->getIOMapping().at(l).address.netmask().toString());
    });
    connect(m_mask, &QLineEdit::textEdited, this, [this]()
    { m_mask->setProperty("edited", true); });

    m_dhcp->setEnabled(known && info.dhcpCapable);
    if (known && info.dhcpCapable == false)
        m_dhcp->setToolTip(tr("This node does not report DHCP support."));
    if (known && info.dhcpConfigured)
        m_dhcp->setChecked(true);
    else
        m_static->setChecked(true);
    connect(m_static, &QRadioButton::toggled, this, &ArtNetNodeSetup::slotModeChanged);
    connect(m_dhcp, &QRadioButton::toggled, this, &ArtNetNodeSetup::slotModeChanged);
    connect(m_defaults, &QRadioButton::toggled, this, &ArtNetNodeSetup::slotModeChanged);
    connect(m_setGateway, &QCheckBox::toggled, this, &ArtNetNodeSetup::slotModeChanged);

    m_status = new QLabel(this);
    m_status->setObjectName("status");
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    top->addWidget(m_status);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_queryButton = buttons->addButton(tr("Ask the node"), QDialogButtonBox::ActionRole);
    m_queryButton->setObjectName("query");
    m_queryButton->setToolTip(tr("Ask the node for its current address, mask, gateway "
                                 "and DHCP setting. Changes nothing."));
    m_applyButton = buttons->addButton(tr("Apply to node…"), QDialogButtonBox::ActionRole);
    m_applyButton->setObjectName("apply");
    m_queryButton->setEnabled(m_reachable);
    m_applyButton->setEnabled(m_reachable);
    connect(m_queryButton, &QPushButton::clicked, this, [this]() { query(); });
    connect(m_applyButton, &QPushButton::clicked, this, [this]() { apply(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    top->addWidget(buttons);

    m_answerTimer = new QTimer(this);
    m_answerTimer->setSingleShot(true);
    m_answerTimer->setInterval(ANSWER_WAIT_MS);
    connect(m_answerTimer, &QTimer::timeout, this, &ArtNetNodeSetup::slotNoAnswer);
    connect(m_plugin, &ArtNetPlugin::ipProgReplied, this, &ArtNetNodeSetup::slotReply);

    m_confirm = [this](const QString &text)
    {
        return QMessageBox::question(this, tr("Change the node's network settings?"), text,
                                     QMessageBox::Yes | QMessageBox::Cancel,
                                     QMessageBox::Cancel) == QMessageBox::Yes;
    };

    slotModeChanged();
}

ArtNetNodeSetup::~ArtNetNodeSetup()
{
}

/*********************************************************************
 * Helpers
 *********************************************************************/

QHostAddress ArtNetNodeSetup::guessMask(const QHostAddress &ip)
{
    if (ip.isInSubnet(QHostAddress("2.0.0.0"), 8) || ip.isInSubnet(QHostAddress("10.0.0.0"), 8))
        return QHostAddress("255.0.0.0");
    if (ip.isInSubnet(QHostAddress("169.254.0.0"), 16))
        return QHostAddress("255.255.0.0");
    return QHostAddress("255.255.255.0");
}

QHostAddress ArtNetNodeSetup::suggestAlias(const QHostAddress &ip, const QHostAddress &mask)
{
    const quint32 m = mask.toIPv4Address();
    const quint32 net = ip.toIPv4Address() & m;
    const quint32 broadcast = net | ~m;
    quint32 alias = broadcast - 5;
    if (alias == ip.toIPv4Address())
        alias--;
    return QHostAddress(alias);
}

QString ArtNetNodeSetup::aliasCommand(const QString &iface, const QHostAddress &alias,
                                      const QHostAddress &mask, bool add)
{
    const QString ifn = iface.isEmpty() ? QStringLiteral("<interface>") : iface;
#if defined(Q_OS_MACOS)
    return add ? QString("sudo ifconfig %1 alias %2 netmask %3")
                     .arg(ifn, alias.toString(), mask.toString())
               : QString("sudo ifconfig %1 -alias %2").arg(ifn, alias.toString());
#elif defined(Q_OS_WIN)
    return add ? QString("netsh interface ipv4 add address \"%1\" %2 %3")
                     .arg(ifn, alias.toString(), mask.toString())
               : QString("netsh interface ipv4 delete address \"%1\" %2")
                     .arg(ifn, alias.toString());
#else
    int prefix = 0;
    for (quint32 m = mask.toIPv4Address(); m & 0x80000000u; m <<= 1)
        prefix++;
    return QString("sudo ip addr %1 %2/%3 dev %4")
              .arg(add ? "add" : "del", alias.toString()).arg(prefix).arg(ifn);
#endif
}

/** A dotted quad and nothing else. QHostAddress also takes "172.18.2" (as
 *  172.18.0.2) and similar shorthands, which is not what anybody typing an
 *  address into a node means. */
static QHostAddress dottedQuad(const QString &text)
{
    const QString t = text.trimmed();
    if (t.split(QLatin1Char('.')).count() != 4)
        return QHostAddress();
    QHostAddress a(t);
    return a.protocol() == QAbstractSocket::IPv4Protocol ? a : QHostAddress();
}

static bool contiguousMask(quint32 m)
{
    // 1s then 0s: inverting and adding one gives a single bit (or zero)
    const quint32 inv = ~m;
    return m != 0 && (inv & (inv + 1)) == 0;
}

QString ArtNetNodeSetup::validate(ArtNetIpProgRequest *request) const
{
    ArtNetIpProgRequest r;
    if (m_dhcp->isChecked())
        r.enableDhcp = true;
    else if (m_defaults->isChecked())
        r.resetToDefaults = true;
    else
    {
        const QHostAddress ip = dottedQuad(m_ip->text());
        const QHostAddress mask = dottedQuad(m_mask->text());
        if (ip.isNull())
            return tr("The IP address is not a valid IPv4 address.");
        if (mask.isNull() || contiguousMask(mask.toIPv4Address()) == false)
            return tr("The subnet mask is not valid (e.g. 255.255.255.0).");
        const quint32 v = ip.toIPv4Address(), m = mask.toIPv4Address();
        if (ip.isLoopback() || ip.isMulticast() || v == 0 || v == 0xffffffffu)
            return tr("%1 cannot be a node's address.").arg(ip.toString());
        if (m != 0xffffffffu && m != 0xfffffffeu
                && ((v & ~m) == 0 || (v & ~m) == ~m))
            return tr("%1 is the network or broadcast address of that subnet.")
                      .arg(ip.toString());

        /* Two devices on one address is the fault this is most likely to
           create, and both kinds are knowable here. */
        foreach (const ArtNetIO &io, m_plugin->getIOMapping())
            if (io.address.ip() == ip)
                return tr("%1 is this computer's own address.").arg(ip.toString());
        ArtNetNodeInfo other;
        if (ip.toString() != m_address && m_plugin->findNode(ip.toString(), &other))
            return tr("%1 is already used by another node (%2).")
                      .arg(ip.toString(),
                           other.longName.isEmpty() ? other.shortName : other.longName);

        r.setIp = true;
        r.ip = ip;
        r.setMask = true;
        r.mask = mask;
        if (m_setGateway->isChecked())
        {
            const QHostAddress gw = dottedQuad(m_gateway->text());
            if (gw.isNull())
                return tr("The gateway is not a valid IPv4 address.");
            r.setGateway = true;
            r.gateway = gw;
        }
    }
    if (request != NULL)
        *request = r;
    return QString();
}

QString ArtNetNodeSetup::describe(const ArtNetIpProgRequest &r) const
{
    if (r.enableDhcp)
        return tr("get its address from DHCP");
    if (r.resetToDefaults)
        return tr("go back to its factory default address");
    QString s = tr("%1 / %2").arg(r.ip.toString(), r.mask.toString());
    s += r.setGateway ? tr(", gateway %1").arg(r.gateway.toString())
                      : tr(", gateway unchanged");
    return s;
}

/*********************************************************************
 * Actions
 *********************************************************************/

void ArtNetNodeSetup::slotModeChanged()
{
    const bool fixed = m_static->isChecked();
    m_ip->setEnabled(fixed);
    m_mask->setEnabled(fixed);
    m_setGateway->setEnabled(fixed);
    m_gateway->setEnabled(fixed && m_setGateway->isChecked());
}

bool ArtNetNodeSetup::apply()
{
    if (m_reachable == false)
        return false;

    ArtNetIpProgRequest request;
    const QString problem = validate(&request);
    if (problem.isEmpty() == false)
    {
        m_status->setText(problem);
        return false;
    }

    QString text = tr("Change the network settings of %1?")
                       .arg(m_nodeName.isEmpty() ? m_address
                                                 : QString("%1 (%2)").arg(m_nodeName, m_address));
    if (m_mac.isEmpty() == false)
        text += "\n" + tr("MAC: %1").arg(m_mac);
    text += "\n\n" + tr("Now: %1").arg(m_address);
    text += "\n" + tr("New: %1").arg(describe(request));
    text += "\n\n" + tr("Sent once, to %1, through %2. The node takes it at once: "
                        "anything patched to %1 stops reaching it until the patch "
                        "is changed.").arg(m_address, m_viaText);
    if (request.setIp && m_plugin->lineReaching(request.ip) < 0)
        text += "\n\n" + tr("%1 is on no network this computer has an address on, "
                            "so the node will not be reachable from here afterwards.")
                             .arg(request.ip.toString());

    if (m_confirm(text) == false)
    {
        m_status->setText(tr("Nothing sent."));
        return false;
    }

    QString why;
    if (m_plugin->sendIpProg(m_address, request, &why) == false)
    {
        m_status->setText(why);
        return false;
    }
    m_expectFrom = request.setIp ? request.ip.toString() : QString();
    waitForAnswer(tr("Sent. Waiting for the node to answer…"));
    return true;
}

bool ArtNetNodeSetup::query()
{
    if (m_reachable == false)
        return false;
    QString why;
    if (m_plugin->sendIpProg(m_address, ArtNetIpProgRequest(), &why) == false)
    {
        m_status->setText(why);
        return false;
    }
    m_expectFrom.clear();
    waitForAnswer(tr("Asked. Waiting for the node to answer…"));
    return true;
}

void ArtNetNodeSetup::waitForAnswer(const QString &what)
{
    m_status->setText(what);
    m_answerTimer->start();
}

void ArtNetNodeSetup::slotReply(const QString &sender, const ArtNetIpProgReply &reply)
{
    if (sender != m_address && (m_expectFrom.isEmpty() || sender != m_expectFrom))
        return;

    m_answerTimer->stop();
    QString text = tr("The node answered: %1 / %2").arg(reply.ip.toString(), reply.mask.toString());
    if (reply.hasGateway)
        text += tr(", gateway %1").arg(reply.gateway.toString());
    text += reply.dhcpEnabled ? tr(", DHCP on.") : tr(", DHCP off.");

    if (reply.ip.isNull() == false && reply.ip.toString() != m_address)
    {
        text += " " + tr("It has moved from %1; press Rescan in Connections to see it there.")
                          .arg(m_address);
        m_address = reply.ip.toString();
        m_ip->setText(m_address);
        m_reachable = m_plugin->lineReaching(reply.ip) >= 0;
        m_queryButton->setEnabled(m_reachable);
        m_applyButton->setEnabled(m_reachable);
    }
    m_status->setText(text);
}

void ArtNetNodeSetup::slotNoAnswer()
{
    m_status->setText(tr("No answer within %1 s. The node may not support setting its "
                         "address over the network (many are set from a front panel or "
                         "a web page), or it has already moved: press Rescan in "
                         "Connections to look for it.").arg(ANSWER_WAIT_MS / 1000));
}
