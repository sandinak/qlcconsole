/*
  Q Light Controller Plus - qlcconsole
  connectioneditor.cpp

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

#include <QScrollArea>
#include <QDialogButtonBox>
#include <QStackedWidget>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QGroupBox>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QMap>
#include <QSet>
#include <QTimer>

#include "connectioneditor.h"
#include "wleddevicesdialog.h"
#include "ioplugincache.h"
#include "inputoutputmap.h"
#include "qlcioplugin.h"
#include "outputpatch.h"
#include "inputpatch.h"
#include "patchundo.h"
#include "universe.h"
#include "doc.h"
#include "apputil.h"

#define ROLE_PLUGIN (Qt::UserRole)
#define ROLE_LINE   (Qt::UserRole + 1)

/* A MIDI input's channel "every channel" (the plugin's MAX_MIDI_CHANNELS). */
static const int kMidiOmni = 16;

/* The target list's two entries that are not a port. */
#define TARGET_BROADCAST (-1)

ConnectionEditor::ConnectionEditor(Doc *doc, quint32 universe, const QString &pluginName,
                                   quint32 line, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_patcher(new UniversePatcher(doc, this))
    , m_universe(universe)
    , m_origPlugin(pluginName)
    , m_origLine(line)
{
    Q_ASSERT(doc != nullptr);
    setObjectName(QStringLiteral("connectionEditor"));
    setWindowTitle(tr("Edit Connection"));
    setProperty("helpTopic", QStringLiteral("other-improvements.md#editing-a-connection"));
    buildForm();
    load();
}

ConnectionEditor::~ConnectionEditor()
{
}

bool ConnectionEditor::edit(Doc *doc, quint32 universe, const QString &pluginName,
                            quint32 line, QWidget *parent, const QString &section)
{
    if (doc == nullptr || doc->inputOutputMap() == nullptr
            || doc->inputOutputMap()->universe(universe) == nullptr)
        return false;
    ConnectionEditor dlg(doc, universe, pluginName, line, parent);
    if (section.isEmpty() == false)
        dlg.focusSection(section);
    return dlg.exec() == QDialog::Accepted;
}

ConnectionEditor::Kind ConnectionEditor::kindOf(const QLCIOPlugin *plugin)
{
    if (plugin == nullptr)
        return KindNone;
    const QString name = plugin->name();
    if (name == QLatin1String("WLED"))
        return KindWled;
    if (plugin->supportsOutputTargets())
        return KindTargets;
    if (name == QLatin1String("E1.31"))
        return KindSacn;
    if (name.contains(QLatin1String("MIDI"), Qt::CaseInsensitive))
        return KindMidi;
    return KindDevice;
}

/*********************************************************************
 * The form
 *********************************************************************/

void ConnectionEditor::buildForm()
{
    /* The form scrolls up and down, OK and Cancel stay under it: it was
       taller than a laptop's screen (UX-34). */
    QWidget *body = new QWidget(this);
    QVBoxLayout *vl = new QVBoxLayout(body);
    vl->setContentsMargins(0, 0, 0, 0);

    // ---- Which universe, which protocol, which interface ----
    m_paneNote = new QLabel(this);
    m_paneNote->setObjectName(QStringLiteral("connPaneNote"));
    m_paneNote->setWordWrap(true);
    m_paneNote->setStyleSheet(QStringLiteral("QLabel { color: #d05a5a; }"));
    m_paneNote->hide();
    vl->addWidget(m_paneNote);

    QFormLayout *top = new QFormLayout;
    m_topForm = top;
    m_universeCombo = new QComboBox(this);
    m_universeCombo->setObjectName(QStringLiteral("connUniverse"));
    if (InputOutputMap *iom = m_doc->inputOutputMap())
    {
        foreach (Universe *u, iom->universes())
            if (u != nullptr)
                m_universeCombo->addItem(tr("%1: %2").arg(u->id() + 1).arg(u->name()), u->id());
    }
    top->addRow(tr("Universe:"), m_universeCombo);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setObjectName(QStringLiteral("connName"));
    top->addRow(tr("Name:"), m_nameEdit);

    m_protocolCombo = new QComboBox(this);
    m_protocolCombo->setObjectName(QStringLiteral("connProtocol"));
    m_protocolCombo->addItem(tr("Not patched"), QString());
    if (m_doc->ioPluginCache() != nullptr)
    {
        foreach (QLCIOPlugin *p, m_doc->ioPluginCache()->plugins())
            if (p != nullptr && (p->capabilities() & QLCIOPlugin::Output)
                    && p->outputs().isEmpty() == false)
                m_protocolCombo->addItem(p->name(), p->name());
    }
    top->addRow(tr("Protocol:"), m_protocolCombo);

    /* Which interface: for Art-Net and sACN it follows from where the
       universe is sent (the network that address is on, or the route the
       system takes to it), so it is shown, not asked -- "shouldn't need to
       set interface anymore". Choosing one by hand stays a click away. */
    m_interfaceRow = new QWidget(this);
    QHBoxLayout *ifl = new QHBoxLayout(m_interfaceRow);
    ifl->setContentsMargins(0, 0, 0, 0);
    m_interfaceAuto = new QLabel(m_interfaceRow);
    m_interfaceAuto->setObjectName(QStringLiteral("connInterfaceAuto"));
    m_interfaceAuto->setWordWrap(true);
    m_interfaceAuto->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ifl->addWidget(m_interfaceAuto, 1);
    m_interfaceCombo = new QComboBox(m_interfaceRow);
    m_interfaceCombo->setObjectName(QStringLiteral("connInterface"));
    ifl->addWidget(m_interfaceCombo, 1);
    m_interfaceChoose = new QPushButton(m_interfaceRow);
    m_interfaceChoose->setObjectName(QStringLiteral("connInterfaceChoose"));
    m_interfaceChoose->setAutoDefault(false);
    ifl->addWidget(m_interfaceChoose, 0, Qt::AlignTop);
    top->addRow(tr("Interface:"), m_interfaceRow);
    vl->addLayout(top);

    m_pendingNote = new QLabel(this);
    m_pendingNote->setObjectName(QStringLiteral("connPendingNote"));
    m_pendingNote->setWordWrap(true);
    m_pendingNote->setStyleSheet(QStringLiteral("QLabel { color: #a06000; }"));
    vl->addWidget(m_pendingNote);
    m_otherLegs = new QLabel(this);
    m_otherLegs->setObjectName(QStringLiteral("connOtherLegs"));
    m_otherLegs->setWordWrap(true);
    m_otherLegs->setStyleSheet(QStringLiteral("QLabel { color: #8a8e99; }"));
    vl->addWidget(m_otherLegs);

    // ---- The protocol's own settings, in place ----
    m_settingsBox = new QGroupBox(tr("Settings"), this);
    m_settingsBox->setObjectName(QStringLiteral("connSettingsBox"));
    QVBoxLayout *sv = new QVBoxLayout(m_settingsBox);
    m_settings = new QStackedWidget(m_settingsBox);
    m_settings->setObjectName(QStringLiteral("connSettings"));
    sv->addWidget(m_settings);

    // Not patched
    {
        QLabel *none = new QLabel(tr("Pick a protocol above to send this universe somewhere."));
        none->setObjectName(QStringLiteral("connPageNone"));
        none->setWordWrap(true);
        m_settings->addWidget(none);
    }
    // A node and port (Art-Net)
    {
        QWidget *page = new QWidget;
        page->setObjectName(QStringLiteral("connPageTargets"));
        QFormLayout *f = new QFormLayout(page);
        /* One control: type the node's address, or open the list for
           Broadcast and the nodes heard so far ("so I don't have to scroll
           all the available IPs to Another Address"). */
        m_targetCombo = new QComboBox(page);
        m_targetCombo->setObjectName(QStringLiteral("connTarget"));
        m_targetCombo->setEditable(true);
        m_targetCombo->setInsertPolicy(QComboBox::NoInsert);
        m_targetCombo->setCompleter(nullptr);
        m_targetCombo->setMinimumContentsLength(16);
        m_targetCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        m_targetIP = m_targetCombo->lineEdit();
        m_targetIP->setObjectName(QStringLiteral("connTargetIP"));
        /* Broadcast is the empty field, so typing an address starts
           clean -- there is no "Broadcast" to delete first. */
        m_targetIP->setPlaceholderText(tr("Broadcast — type a node's IP to send to it"));
        m_targetCombo->setToolTip(tr("Where this universe is sent: type a node's IP address, "
                                     "or open the list for Broadcast and the nodes heard on "
                                     "the network"));
        f->addRow(tr("Send To:"), m_targetCombo);
        m_targetNote = new QLabel(page);
        m_targetNote->setObjectName(QStringLiteral("connTargetNote"));
        m_targetNote->setWordWrap(true);
        f->addRow(QString(), m_targetNote);
        m_targetPort = new QLineEdit(page);
        m_targetPort->setObjectName(QStringLiteral("connTargetPort"));
        m_targetPort->setPlaceholderText(tr("A number (15) or 0:0:15"));
        m_targetPort->setToolTip(tr("The Art-Net universe on the node: a number from 0 to "
                                    "32767, or net:subnet:universe (net 0-127, subnet and "
                                    "universe 0-15). 15 is 0:0:15, 300 is 1:2:12."));
        f->addRow(tr("Universe on the Node:"), m_targetPort);
        m_transmitMode = new QComboBox(page);
        m_transmitMode->setObjectName(QStringLiteral("connTransmitMode"));
        m_transmitMode->addItems(QStringList() << QStringLiteral("Standard")
                                 << QStringLiteral("Full") << QStringLiteral("Partial"));
        m_transmitMode->setToolTip(tr("Standard sends on change plus a refresh; Full sends "
                                      "every frame; Partial only the channels in use"));
        f->addRow(tr("Transmit Mode:"), m_transmitMode);
        m_settings->addWidget(page);
    }
    // sACN
    {
        QWidget *page = new QWidget;
        page->setObjectName(QStringLiteral("connPageSacn"));
        QFormLayout *f = new QFormLayout(page);
        m_sacnUniverse = new QSpinBox(page);
        m_sacnUniverse->setObjectName(QStringLiteral("connSacnUniverse"));
        m_sacnUniverse->setRange(1, 63999);
        f->addRow(tr("sACN Universe:"), m_sacnUniverse);
        m_sacnMulticast = new QCheckBox(tr("Multicast"), page);
        m_sacnMulticast->setObjectName(QStringLiteral("connSacnMulticast"));
        f->addRow(QString(), m_sacnMulticast);
        m_sacnIP = new QLineEdit(page);
        m_sacnIP->setObjectName(QStringLiteral("connSacnIP"));
        m_sacnIP->setPlaceholderText(tr("Type the receiver's IP to unicast"));
        f->addRow(tr("Unicast To:"), m_sacnIP);
        m_sacnPriority = new QSpinBox(page);
        m_sacnPriority->setObjectName(QStringLiteral("connSacnPriority"));
        m_sacnPriority->setRange(0, 200);
        m_sacnPriority->setToolTip(tr("A receiver hearing this universe from two sources "
                                      "takes the higher priority (default 100)"));
        f->addRow(tr("Priority:"), m_sacnPriority);
        m_sacnMode = new QComboBox(page);
        m_sacnMode->setObjectName(QStringLiteral("connSacnMode"));
        m_sacnMode->addItems(QStringList() << QStringLiteral("Full") << QStringLiteral("Partial"));
        f->addRow(tr("Transmit Mode:"), m_sacnMode);
        m_settings->addWidget(page);
    }
    // WLED
    {
        QWidget *page = new QWidget;
        page->setObjectName(QStringLiteral("connPageWled"));
        QVBoxLayout *l = new QVBoxLayout(page);
        m_wledNote = new QLabel(page);
        m_wledNote->setWordWrap(true);
        l->addWidget(m_wledNote);
        m_wledButton = new QPushButton(UniversePatcher::retargetText(QStringLiteral("WLED")), page);
        m_wledButton->setObjectName(QStringLiteral("connWledDevices"));
        l->addWidget(m_wledButton, 0, Qt::AlignLeft);
        l->addStretch(1);
        m_settings->addWidget(page);
    }
    // MIDI
    {
        QWidget *page = new QWidget;
        page->setObjectName(QStringLiteral("connPageMidi"));
        QFormLayout *f = new QFormLayout(page);
        m_midiMode = new QComboBox(page);
        m_midiMode->setObjectName(QStringLiteral("connMidiMode"));
        m_midiMode->addItems(QStringList() << QStringLiteral("Control Change")
                             << QStringLiteral("Note Velocity") << QStringLiteral("Program Change"));
        f->addRow(tr("MIDI Output Mode:"), m_midiMode);
        m_settings->addWidget(page);
    }
    // A device: the interface is the widget
    {
        QWidget *page = new QWidget;
        page->setObjectName(QStringLiteral("connPageDevice"));
        QVBoxLayout *l = new QVBoxLayout(page);
        m_deviceNote = new QLabel(page);
        m_deviceNote->setObjectName(QStringLiteral("connDeviceNote"));
        m_deviceNote->setWordWrap(true);
        l->addWidget(m_deviceNote);
        l->addStretch(1);
        m_settings->addWidget(page);
    }
    /* The plugin's own settings window: per widget, per protocol, not per
       patch (a USB widget's output speed, Art-Net's interfaces). It stays a
       window of its own -- the plugins only have it as one. */
    m_configureButton = new QPushButton(m_settingsBox);
    m_configureButton->setObjectName(QStringLiteral("connConfigurePlugin"));
    m_configureButton->setToolTip(tr("Opens the protocol's own settings window"));
    sv->addWidget(m_configureButton, 0, Qt::AlignLeft);
    vl->addWidget(m_settingsBox);

    // ---- The universe: input, feedback, passthrough ----
    m_inputBox = new QGroupBox(tr("Input && Feedback"), this);
    m_inputBox->setObjectName(QStringLiteral("connInputBox"));
    QFormLayout *inf = new QFormLayout(m_inputBox);
    m_inputCombo = new QComboBox(m_inputBox);
    m_inputCombo->setObjectName(QStringLiteral("connInput"));
    m_inputCombo->addItem(tr("None"));
    m_feedbackCombo = new QComboBox(m_inputBox);
    m_feedbackCombo->setObjectName(QStringLiteral("connFeedback"));
    m_feedbackCombo->addItem(tr("None"));
    if (m_doc->ioPluginCache() != nullptr)
    {
        foreach (QLCIOPlugin *p, m_doc->ioPluginCache()->plugins())
        {
            if (p == nullptr)
                continue;
            if (p->capabilities() & QLCIOPlugin::Input)
            {
                const QStringList labels = lineLabels(p, false);
                for (int i = 0; i < labels.count(); i++)
                {
                    m_inputCombo->addItem(tr("%1: %2").arg(p->name(), labels.at(i)));
                    m_inputCombo->setItemData(m_inputCombo->count() - 1, p->name(), ROLE_PLUGIN);
                    m_inputCombo->setItemData(m_inputCombo->count() - 1, i, ROLE_LINE);
                }
            }
            /* Feedback is a capability of its own: a plugin that outputs
               DMX cannot necessarily send anything back. */
            if (p->capabilities() & QLCIOPlugin::Feedback)
            {
                const QStringList labels = lineLabels(p, true);
                for (int i = 0; i < labels.count(); i++)
                {
                    m_feedbackCombo->addItem(tr("%1: %2").arg(p->name(), labels.at(i)));
                    m_feedbackCombo->setItemData(m_feedbackCombo->count() - 1, p->name(), ROLE_PLUGIN);
                    m_feedbackCombo->setItemData(m_feedbackCombo->count() - 1, i, ROLE_LINE);
                }
            }
        }
    }
    inf->addRow(tr("Input:"), m_inputCombo);
    m_profileCombo = new QComboBox(m_inputBox);
    m_profileCombo->setObjectName(QStringLiteral("connProfile"));
    m_profileCombo->addItem(tr("None"), QString());
    if (m_doc->inputOutputMap() != nullptr)
    {
        QStringList profiles = m_doc->inputOutputMap()->profileNames();
        profiles.sort();
        foreach (const QString &pn, profiles)
            m_profileCombo->addItem(pn, pn);
    }
    inf->addRow(tr("Input Profile:"), m_profileCombo);
    m_midiChannel = new QComboBox(m_inputBox);
    m_midiChannel->setObjectName(QStringLiteral("connMidiChannel"));
    /* The MIDI plugin's own numbering: 0-15 are channels 1-16, and 16
       (MAX_MIDI_CHANNELS) is every channel -- an input's default. */
    m_midiChannel->addItem(tr("All channels"), kMidiOmni);
    for (int ch = 1; ch <= 16; ch++)
        m_midiChannel->addItem(tr("Channel %1").arg(ch), ch - 1);
    inf->addRow(tr("MIDI Input Channel:"), m_midiChannel);
    m_artnetInUni = new QSpinBox(m_inputBox);
    m_artnetInUni->setObjectName(QStringLiteral("connArtNetInputUniverse"));
    m_artnetInUni->setRange(0, 32767);
    m_artnetInUni->setToolTip(tr("Which Art-Net universe this input listens to"));
    inf->addRow(tr("Art-Net Input Universe:"), m_artnetInUni);
    inf->addRow(tr("Feedback To:"), m_feedbackCombo);
    m_feedbackIP = new QLineEdit(m_inputBox);
    m_feedbackIP->setObjectName(QStringLiteral("connFeedbackIP"));
    m_feedbackIP->setPlaceholderText(tr("Empty: broadcast"));
    inf->addRow(tr("Feedback Address:"), m_feedbackIP);
    m_feedbackPort = new QLineEdit(m_inputBox);
    m_feedbackPort->setObjectName(QStringLiteral("connFeedbackPort"));
    m_feedbackPort->setPlaceholderText(tr("A number (15) or 0:0:15"));
    inf->addRow(tr("Feedback Universe:"), m_feedbackPort);
    m_passthrough = new QCheckBox(tr("Passthrough"), m_inputBox);
    m_passthrough->setObjectName(QStringLiteral("connPassthrough"));
    m_passthrough->setToolTip(tr("Pass incoming values straight through to the output "
                                 "instead of into the engine"));
    inf->addRow(QString(), m_passthrough);
    vl->addWidget(m_inputBox);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttons = buttons;
    connect(buttons, &QDialogButtonBox::accepted, this, &ConnectionEditor::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &ConnectionEditor::reject);
    QVBoxLayout *dialogLayout = new QVBoxLayout(this);
    dialogLayout->addWidget(AppUtil::verticalScroller(body, this), 1);
    dialogLayout->addWidget(buttons);

    connect(m_universeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConnectionEditor::slotUniverseChanged);
    connect(m_protocolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConnectionEditor::slotProtocolChanged);
    connect(m_interfaceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConnectionEditor::slotInterfaceChanged);
    connect(m_targetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConnectionEditor::slotTargetChanged);
    connect(m_targetCombo, QOverload<int>::of(&QComboBox::activated),
            this, &ConnectionEditor::slotTargetChanged);
    connect(m_targetIP, &QLineEdit::textEdited, this, &ConnectionEditor::slotTargetTyped);
    connect(m_targetPort, &QLineEdit::editingFinished, this, &ConnectionEditor::slotPortCommitted);
    connect(m_feedbackPort, &QLineEdit::editingFinished, this, [this]() {
        quint32 pa = 0;
        if (UniversePatcher::parsePortAddress(m_feedbackPort->text(), pa))
            m_feedbackPort->setText(UniversePatcher::portAddressText(pa));
    });
    connect(m_interfaceChoose, &QPushButton::clicked, this, &ConnectionEditor::slotChooseInterface);
    connect(m_sacnMulticast, &QCheckBox::toggled, this, &ConnectionEditor::slotSacnMulticastToggled);
    connect(m_sacnIP, &QLineEdit::textEdited, this, &ConnectionEditor::slotSacnTyped);
    connect(m_inputCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConnectionEditor::slotInputChanged);
    connect(m_feedbackCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConnectionEditor::slotFeedbackChanged);
    connect(m_configureButton, &QPushButton::clicked, this, &ConnectionEditor::slotConfigurePlugin);
    connect(m_wledButton, &QPushButton::clicked, this, &ConnectionEditor::slotWledDevices);
}

QStringList ConnectionEditor::lineLabels(QLCIOPlugin *p, bool output) const
{
    QStringList labels;
    if (p == nullptr)
        return labels;
    /* As every line row in the Connections tree reads: the operator's name
       for it wins over the NIC's. */
    const QStringList lines = output ? p->outputs() : p->inputs();
    for (int i = 0; i < lines.count(); i++)
    {
        const QString ifname = p->lineDescription(quint32(i), output);
        const QString alias = m_doc->inputOutputMap()
            ? m_doc->inputOutputMap()->lineAlias(p->name(), lines.at(i)) : QString();
        const QString shown = alias.isEmpty() ? ifname : alias;
        labels << (shown.isEmpty() ? lines.at(i) : tr("%1 (%2)").arg(shown, lines.at(i)));
    }
    return labels;
}

/*********************************************************************
 * Filling it in
 *********************************************************************/

OutputPatch *ConnectionEditor::editedPatch() const
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    Universe *u = iom != nullptr ? iom->universe(m_universe) : nullptr;
    if (u == nullptr || m_origIndex < 0 || m_origIndex >= u->outputPatchesCount())
        return nullptr;
    return u->outputPatch(m_origIndex);
}

QVariant ConnectionEditor::origParam(const QString &key) const
{
    /* Only the patch's own values, and only while the protocol is the one it
       has: absent is the plugin's default, not zero. */
    OutputPatch *op = editedPatch();
    if (op == nullptr || pickedPluginName() != m_origPlugin)
        return QVariant();
    const QMap<QString, QVariant> pp = op->getPluginParameters();
    return pp.contains(key) ? pp.value(key) : QVariant();
}

void ConnectionEditor::load()
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    Universe *u = iom != nullptr ? iom->universe(m_universe) : nullptr;
    if (u == nullptr)
        return;
    m_loading = true;

    /* The leg: the one asked for; else (it moved to another interface, say)
       the first on that protocol; else the first that is really there. */
    int wanted = -1, samePlugin = -1, first = -1;
    for (int i = 0; i < u->outputPatchesCount(); i++)
    {
        OutputPatch *op = u->outputPatch(i);
        if (op == nullptr || op->plugin() == nullptr || op->isPending())
            continue;
        if (first < 0)
            first = i;
        if (op->plugin()->name() == m_origPlugin)
        {
            if (samePlugin < 0)
                samePlugin = i;
            if (wanted < 0 && op->output() == m_origLine)
                wanted = i;
        }
    }
    m_origIndex = wanted >= 0 ? wanted : samePlugin >= 0 ? samePlugin : first;
    QStringList others;
    for (int i = 0; i < u->outputPatchesCount(); i++)
    {
        OutputPatch *op = u->outputPatch(i);
        if (op == nullptr || op->plugin() == nullptr || op->isPending())
            continue;
        if (i == m_origIndex)
        {
            m_origPlugin = op->plugin()->name();
            m_origLine = op->output();
        }
        else
            others << m_patcher->outputLegText(op);
    }
    if (m_origIndex < 0)
        m_origPlugin.clear();
    m_otherLegs->setText(others.isEmpty() ? QString()
                         : tr("Also goes out: %1").arg(others.join(QStringLiteral(" · "))));
    m_otherLegs->setVisible(others.isEmpty() == false);

    QStringList pending;
    foreach (const Universe::OfflinePatch &p, u->offlinePatches())
        if (p.tag != Universe::InputPatchTag && p.tag != Universe::FeedbackPatchTag)
            pending << (p.uid.isEmpty() ? p.plugin : tr("%1 \"%2\"").arg(p.plugin, p.uid));
    for (int i = 0; i < u->outputPatchesCount(); i++)
        if (OutputPatch *op = u->outputPatch(i))
            if (op->isPending())
                pending << op->pluginName();
    m_pendingNote->setText(pending.isEmpty() ? QString()
        : tr("Patched to %1, which is not here now: a change here patches it again, on "
             "the interface shown.").arg(pending.join(QStringLiteral(", "))));
    m_pendingNote->setVisible(pending.isEmpty() == false);

    m_universeCombo->setCurrentIndex(qMax(0, m_universeCombo->findData(m_universe)));
    if (m_paneTitle != nullptr)
        m_paneTitle->setText(tr("Universe %1: %2").arg(m_universe + 1).arg(u->name()));
    m_nameEdit->setText(u->name());
    m_protocolCombo->setCurrentIndex(qMax(0, m_protocolCombo->findData(m_origPlugin)));
    m_loading = false;
    slotProtocolChanged();

    // The universe's input, profile, feedback and passthrough.
    m_loading = true;
    InputPatch *ip = u->inputPatch();
    int inIdx = 0;
    if (ip != nullptr && ip->plugin() != nullptr)
    {
        for (int i = 1; i < m_inputCombo->count(); i++)
            if (m_inputCombo->itemData(i, ROLE_PLUGIN).toString() == ip->plugin()->name()
                    && m_inputCombo->itemData(i, ROLE_LINE).toUInt() == ip->input())
                inIdx = i;
    }
    m_inputCombo->setCurrentIndex(inIdx);
    m_profileCombo->setCurrentIndex(qMax(0, m_profileCombo->findData(ip != nullptr ? ip->profileName()
                                                                                   : QString())));
    const QMap<QString, QVariant> inParams = ip != nullptr ? ip->getPluginParameters()
                                                           : QMap<QString, QVariant>();
    m_midiChannel->setCurrentIndex(qMax(0, m_midiChannel->findData(inParams.value(
                                       QStringLiteral("midichannel"), kMidiOmni).toInt())));
    m_artnetInUni->setValue(inParams.value(QStringLiteral("inputUni"), m_universe).toInt());

    OutputPatch *fb = u->feedbackPatch();
    int fbIdx = 0;
    if (fb != nullptr && fb->plugin() != nullptr)
    {
        for (int i = 1; i < m_feedbackCombo->count(); i++)
            if (m_feedbackCombo->itemData(i, ROLE_PLUGIN).toString() == fb->plugin()->name()
                    && m_feedbackCombo->itemData(i, ROLE_LINE).toUInt() == fb->output())
                fbIdx = i;
    }
    m_feedbackCombo->setCurrentIndex(fbIdx);
    const QMap<QString, QVariant> fbParams = fb != nullptr ? fb->getPluginParameters()
                                                           : QMap<QString, QVariant>();
    m_feedbackIP->setText(fbParams.value(QStringLiteral("outputIP")).toString());
    m_feedbackPort->setText(fbParams.contains(QStringLiteral("outputUni"))
        ? UniversePatcher::portAddressText(fbParams.value(QStringLiteral("outputUni")).toUInt())
        : QString());
    m_passthrough->setChecked(iom->getUniversePassthrough(int(m_universe)));
    m_loading = false;
    slotInputChanged();
    slotFeedbackChanged();
    markClean();
}

QLCIOPlugin *ConnectionEditor::pickedPlugin() const
{
    const QString name = pickedPluginName();
    if (name.isEmpty() || m_doc->ioPluginCache() == nullptr)
        return nullptr;
    return m_doc->ioPluginCache()->plugin(name);
}

QString ConnectionEditor::pickedPluginName() const
{
    return m_protocolCombo->currentData().toString();
}

quint32 ConnectionEditor::pickedLine() const
{
    return m_interfaceCombo->currentIndex() < 0 ? 0 : quint32(m_interfaceCombo->currentIndex());
}

void ConnectionEditor::showSettingsFor(Kind kind)
{
    const char *page = "connPageNone";
    switch (kind)
    {
        case KindTargets: page = "connPageTargets"; break;
        case KindSacn: page = "connPageSacn"; break;
        case KindWled: page = "connPageWled"; break;
        case KindMidi: page = "connPageMidi"; break;
        case KindDevice: page = "connPageDevice"; break;
        case KindNone: break;
    }
    for (int i = 0; i < m_settings->count(); i++)
        if (m_settings->widget(i)->objectName() == QLatin1String(page))
            m_settings->setCurrentIndex(i);
}

void ConnectionEditor::slotUniverseChanged()
{
    if (m_loading)
        return;
    const quint32 u = m_universeCombo->currentData().toUInt();
    if (u == m_universe)
        return;
    m_universe = u;
    m_origPlugin.clear();
    m_origLine = 0;
    load();
}

void ConnectionEditor::slotProtocolChanged()
{
    if (m_loading)
        return;
    QLCIOPlugin *p = pickedPlugin();
    const bool sameLeg = p != nullptr && editedPatch() != nullptr && p->name() == m_origPlugin;
    m_loading = true;
    m_interfaceCombo->clear();
    if (p != nullptr)
        m_interfaceCombo->addItems(lineLabels(p, true));
    int cur = sameLeg ? int(m_origLine) : 0;
    /* A new patch on a network protocol starts on the first interface that
       is not loopback; the destination moves it from there. */
    if (sameLeg == false && UniversePatcher::linesAreAddresses(p))
        cur = UniversePatcher::autoLine(p->outputs(), UniversePatcher::Destination(), -1).line;
    m_interfaceCombo->setCurrentIndex(qBound(-1, cur, m_interfaceCombo->count() - 1));
    m_interfaceCombo->setEnabled(m_interfaceCombo->count() > 1);
    m_autoInterface = true;
    m_loading = false;

    m_configureButton->setVisible(p != nullptr && p->canConfigure());
    if (p != nullptr)
        m_configureButton->setText(tr("Configure %1…").arg(p->name()));
    showSettingsFor(kindOf(p));
    m_settingsBox->setTitle(p != nullptr ? tr("%1 Settings").arg(p->name()) : tr("Settings"));
    loadSettings();

    /* A saved show keeps the interface it was patched on. Where the
       destination would pick that same one anyway it reads "automatic";
       where it was set to another one by hand, it stays chosen by hand. */
    if (sameLeg && UniversePatcher::linesAreAddresses(p))
    {
        const UniversePatcher::AutoLine a =
            UniversePatcher::autoLine(p->outputs(), destination(), int(m_origLine));
        m_autoInterface = (a.line == int(m_origLine));
        if (m_autoInterface == false && kindOf(p) == KindTargets)
            loadTargets();              // the nodes on that interface only
    }
    updateInterface();
    updateTargetNote();
    schedulePaneCommit();
}

void ConnectionEditor::slotInterfaceChanged()
{
    if (m_loading)
        return;
    /* Only what depends on the interface: the nodes heard on it. What was
       typed stays. */
    const Kind kind = kindOf(pickedPlugin());
    if (kind == KindTargets)
    {
        const QString typed = m_targetIP->text();
        const QString port = m_targetPort->text();
        loadTargets();
        m_loading = true;
        m_targetIP->setText(typed);
        m_targetPort->setText(port);
        m_loading = false;
        updateTargetNote();
    }
    else if (kind == KindWled)
        loadSettings();
    updateInterface();
    schedulePaneCommit();
}

void ConnectionEditor::slotChooseInterface()
{
    m_autoInterface = !m_autoInterface;
    if (kindOf(pickedPlugin()) == KindTargets)
    {
        const QString typed = m_targetIP->text();
        const QString port = m_targetPort->text();
        loadTargets();
        m_loading = true;
        m_targetIP->setText(typed);
        m_targetPort->setText(port);
        m_loading = false;
    }
    updateInterface();
    updateTargetNote();
    if (m_autoInterface == false)
        m_interfaceCombo->setFocus();
    schedulePaneCommit();
}

UniversePatcher::Destination ConnectionEditor::destination() const
{
    switch (kindOf(pickedPlugin()))
    {
        case KindTargets:
            return UniversePatcher::readDestination(m_targetIP->text());
        case KindSacn:
            if (m_sacnMulticast->isChecked())
                return UniversePatcher::Destination();
            return UniversePatcher::readDestination(m_sacnIP->text());
        default:
            return UniversePatcher::Destination();
    }
}

void ConnectionEditor::updateInterface()
{
    QLCIOPlugin *p = pickedPlugin();
    const Kind kind = kindOf(p);
    const bool network = (kind == KindTargets || kind == KindSacn)
                         && UniversePatcher::linesAreAddresses(p);
    if (network == false)
    {
        m_interfaceAuto->hide();
        m_interfaceChoose->hide();
        m_interfaceCombo->show();
        return;
    }
    m_interfaceChoose->show();
    if (m_autoInterface == false)
    {
        m_interfaceAuto->hide();
        m_interfaceCombo->show();
        m_interfaceChoose->setText(tr("Automatic"));
        m_interfaceChoose->setToolTip(tr("Let the address it is sent to pick the interface again"));
        return;
    }

    const UniversePatcher::AutoLine a =
        UniversePatcher::autoLine(p->outputs(), destination(), int(pickedLine()));
    if (a.line >= 0 && a.line != m_interfaceCombo->currentIndex())
    {
        m_loading = true;
        m_interfaceCombo->setCurrentIndex(a.line);
        m_loading = false;
    }
    m_interfaceAuto->setText(tr("%1 (automatic)\n%2").arg(a.via, a.why).trimmed());
    m_interfaceAuto->setStyleSheet(a.fallback ? QStringLiteral("QLabel { color: #a06000; }")
                                              : QString());
    m_interfaceAuto->setToolTip(tr("Picked from where this universe is sent: the interface on "
                                   "that address's network, or the one the system routes it "
                                   "through"));
    m_interfaceAuto->show();
    m_interfaceCombo->hide();
    m_interfaceChoose->setText(tr("Choose"));
    m_interfaceChoose->setToolTip(tr("Pick the interface by hand instead"));
}

void ConnectionEditor::updateTargetNote()
{
    if (kindOf(pickedPlugin()) != KindTargets)
        return;
    const UniversePatcher::Destination d = UniversePatcher::readDestination(m_targetIP->text());
    QString text;
    bool bad = false;
    switch (d.kind)
    {
        case UniversePatcher::Destination::Broadcast:
            text = d.network.isEmpty()
                ? tr("Broadcast to every node on the interface's network, as Art-Net "
                     "universe %1 (this universe's own number).")
                      .arg(UniversePatcher::portAddressText(m_universe))
                : tr("Broadcast on %1, as Art-Net universe %2 (this universe's own number).")
                      .arg(d.network, UniversePatcher::portAddressText(m_universe));
        break;
        case UniversePatcher::Destination::Unicast:
        {
            QString name;
            foreach (const UniversePatcher::TargetPortChoice &c, m_ports)
                if (c.address == d.address)
                    name = c.text.section(QStringLiteral(" — "), 0, 0);
            text = name.isEmpty() || name == d.address ? tr("Unicast to %1.").arg(d.address)
                                                       : tr("Unicast to %1.").arg(name);
        }
        break;
        case UniversePatcher::Destination::Partial:
            text = tr("An IP address is four numbers, like 172.18.2.50.");
        break;
        case UniversePatcher::Destination::Invalid:
            text = d.error;
            bad = true;
        break;
    }
    m_targetNote->setText(text);
    m_targetNote->setStyleSheet(bad ? QStringLiteral("QLabel { color: #d05a5a; }")
                                    : QStringLiteral("QLabel { color: #8a8e99; }"));
    const bool broadcast = d.kind == UniversePatcher::Destination::Broadcast;
    m_targetPort->setEnabled(broadcast == false);
    m_targetPort->setToolTip(broadcast
        ? tr("Broadcast goes out as this universe's own number; type a node's IP in Send To "
             "to choose the universe on the node")
        : tr("The Art-Net universe on the node: a number from 0 to 32767, or "
             "net:subnet:universe (net 0-127, subnet and universe 0-15). 15 is 0:0:15, "
             "300 is 1:2:12."));
}

void ConnectionEditor::loadSettings()
{
    QLCIOPlugin *p = pickedPlugin();
    const Kind kind = kindOf(p);
    m_loading = true;
    switch (kind)
    {
        case KindTargets:
        {
            const QString mode = origParam(QStringLiteral("transmitMode")).toString();
            m_transmitMode->setCurrentIndex(qMax(0, m_transmitMode->findText(mode)));
            m_loading = false;
            loadTargets();
            m_loading = true;
        }
        break;
        case KindSacn:
        {
            const QVariant mc = origParam(QStringLiteral("multicast"));
            m_sacnMulticast->setChecked(mc.isValid() ? mc.toInt() != 0 : true);
            m_sacnIP->setText(origParam(QStringLiteral("ucastIP")).toString());
            const QVariant uni = origParam(QStringLiteral("universe"));
            m_sacnUniverse->setValue(uni.isValid() ? uni.toInt() : int(m_universe) + 1);
            const QVariant pr = origParam(QStringLiteral("priority"));
            m_sacnPriority->setValue(pr.isValid() ? pr.toInt() : 100);
            const QString mode = origParam(QStringLiteral("transmitMode")).toString();
            m_sacnMode->setCurrentIndex(qMax(0, m_sacnMode->findText(mode)));
        }
        break;
        case KindMidi:
        {
            const QString mode = origParam(QStringLiteral("mode")).toString();
            m_midiMode->setCurrentIndex(qMax(0, m_midiMode->findText(mode)));
        }
        break;
        case KindWled:
        {
            const bool patched = editedPatch() != nullptr && m_origPlugin == p->name()
                                 && m_origLine == pickedLine();
            m_wledButton->setEnabled(patched);
            m_wledNote->setText(patched
                ? tr("A WLED universe goes to its devices -- the primary and any backup "
                     "lanterns mirroring it. They are set in their own window.")
                : tr("A WLED universe goes to its devices, set in their own window: it "
                     "opens when you press OK."));
        }
        break;
        case KindDevice:
            m_deviceNote->setText(tr("The interface above is the device: %1 sends this "
                                     "universe straight out of it. Its own settings, if it "
                                     "has any, are in Configure %1… below.")
                                  .arg(p != nullptr ? p->name() : QString()));
        break;
        case KindNone:
        break;
    }
    m_loading = false;
}

void ConnectionEditor::loadTargets()
{
    QLCIOPlugin *p = pickedPlugin();
    if (p == nullptr)
        return;
    m_loading = true;
    /* With the interface automatic, every node heard on any of them: the
       node picked then decides the interface, not the other way round. */
    QList<quint32> lines;
    if (m_autoInterface && UniversePatcher::linesAreAddresses(p))
    {
        for (int i = 0; i < p->outputs().count(); i++)
            lines << quint32(i);
    }
    else
        lines << pickedLine();
    m_ports.clear();
    m_portLines.clear();
    QSet<QString> seen;
    foreach (quint32 line, lines)
        foreach (const UniversePatcher::TargetPortChoice &c,
                 m_patcher->targetPortChoices(p->name(), line, m_universe))
        {
            const QString key = QStringLiteral("%1/%2").arg(c.address).arg(c.port);
            if (seen.contains(key))
                continue;
            seen.insert(key);
            m_ports << c;
            m_portLines << line;
        }
    m_targetCombo->clear();
    m_targetCombo->addItem(tr("Broadcast"), TARGET_BROADCAST);
    for (int i = 0; i < m_ports.count(); i++)
        m_targetCombo->addItem(m_ports.at(i).text, i);

    // Where it goes now, when it is this plugin.
    QString addr;
    quint32 pa = 0;
    const bool same = editedPatch() != nullptr && p->name() == m_origPlugin;
    if (same && m_patcher->patchTarget(m_universe, p->name(), m_origLine, addr, pa))
    {
        int pick = -1;
        for (int i = 0; i < m_ports.count(); i++)
            if (m_ports.at(i).address == addr && m_ports.at(i).port == pa)
            {
                pick = i + 1;
                m_targetCombo->setItemText(i + 1, tr("%1  (current)").arg(m_ports.at(i).text));
            }
        m_targetCombo->setCurrentIndex(pick);
        m_targetIP->setText(addr);
        m_targetPort->setText(UniversePatcher::portAddressText(pa));
    }
    else
    {
        m_targetCombo->setCurrentIndex(0);
        m_targetIP->clear();
        m_targetPort->clear();
    }
    m_loading = false;
    updateTargetNote();
}

void ConnectionEditor::slotTargetChanged()
{
    if (m_loading)
        return;
    const int which = m_targetCombo->currentData().toInt();
    if (m_targetCombo->currentIndex() < 0)
        return;
    m_loading = true;
    if (which >= 0 && which < m_ports.count())
    {
        m_targetIP->setText(m_ports.at(which).address);
        m_targetPort->setText(UniversePatcher::portAddressText(m_ports.at(which).port));
    }
    else if (which == TARGET_BROADCAST)
    {
        m_targetIP->clear();
        m_targetPort->clear();
    }
    m_loading = false;
    updateTargetNote();
    updateInterface();
    schedulePaneCommit();
}

void ConnectionEditor::slotTargetTyped()
{
    if (m_loading)
        return;
    /* Typed: Send To is that address now, whatever the list had picked --
       an IP is unicast, a broadcast address broadcast; the interface
       follows it. */
    updateTargetNote();
    updateInterface();
}

void ConnectionEditor::slotPortCommitted()
{
    if (m_loading)
        return;
    const QString txt = m_targetPort->text().trimmed();
    quint32 pa = 0;
    if (txt.isEmpty() == false && UniversePatcher::parsePortAddress(txt, pa))
        m_targetPort->setText(UniversePatcher::portAddressText(pa));   // 15 → 0:0:15
    else if (txt.isEmpty() == false)
    {
        m_targetNote->setText(UniversePatcher::portAddressError(txt));
        m_targetNote->setStyleSheet(QStringLiteral("QLabel { color: #d05a5a; }"));
    }
    schedulePaneCommit();
}

void ConnectionEditor::slotSacnMulticastToggled()
{
    if (m_loading == false && m_sacnMulticast->isChecked())
        m_sacnIP->clear();
    updateInterface();
    schedulePaneCommit();
}

void ConnectionEditor::slotSacnTyped()
{
    /* An address typed is unicast to it; emptied, multicast again. */
    m_loading = true;
    m_sacnMulticast->setChecked(m_sacnIP->text().trimmed().isEmpty());
    m_loading = false;
    updateInterface();
}

void ConnectionEditor::slotInputChanged()
{
    const QString plugin = m_inputCombo->currentData(ROLE_PLUGIN).toString();
    const bool any = plugin.isEmpty() == false;
    m_profileCombo->setEnabled(any);
    /* Rows that do not apply stay, greyed, so the window keeps its shape. */
    m_midiChannel->setEnabled(plugin.contains(QLatin1String("MIDI"), Qt::CaseInsensitive));
    m_artnetInUni->setEnabled(plugin == QLatin1String("ArtNet"));
}

void ConnectionEditor::slotFeedbackChanged()
{
    const QString plugin = m_feedbackCombo->currentData(ROLE_PLUGIN).toString();
    const bool targets = plugin.isEmpty() == false && m_patcher->supportsTargets(plugin);
    m_feedbackIP->setEnabled(targets);
    m_feedbackPort->setEnabled(targets);
}

void ConnectionEditor::slotConfigurePlugin()
{
    QLCIOPlugin *p = pickedPlugin();
    if (p == nullptr)
        return;
    const quint32 line = pickedLine();
    p->configure();
    /* Its interfaces and devices may be different now. */
    m_loading = true;
    m_interfaceCombo->clear();
    m_interfaceCombo->addItems(lineLabels(p, true));
    m_interfaceCombo->setCurrentIndex(qBound(-1, int(line), m_interfaceCombo->count() - 1));
    m_interfaceCombo->setEnabled(m_interfaceCombo->count() > 1);
    m_loading = false;
    loadSettings();
    updateInterface();
}

void ConnectionEditor::slotWledDevices()
{
    if (WledDevicesDialog::edit(m_doc, m_universe, m_origLine, this))
        loadSettings();
}

void ConnectionEditor::focusSection(const QString &section)
{
    if (section == QLatin1String("input"))
        m_inputCombo->setFocus();
    else if (section == QLatin1String("feedback"))
        m_feedbackCombo->setFocus();
    else if (section == QLatin1String("passthrough"))
        m_passthrough->setFocus();
}

/*********************************************************************
 * Making the change
 *********************************************************************/

QString ConnectionEditor::validate(QString &address, quint32 &portAddress,
                                   QString &fbAddress, quint32 &fbPort) const
{
    QLCIOPlugin *p = pickedPlugin();
    const Kind kind = kindOf(p);
    if (p != nullptr && m_interfaceCombo->count() == 0)
        return tr("%1 has no interface to send on.").arg(p->name());

    const UniversePatcher::Destination dest = destination();
    if (kind == KindTargets || kind == KindSacn)
    {
        if (dest.kind == UniversePatcher::Destination::Invalid)
            return dest.error;
        if (dest.kind == UniversePatcher::Destination::Partial)
            return tr("\"%1\" is not a whole IP address yet: it is four numbers, like "
                      "172.18.2.50.").arg(kind == KindTargets ? m_targetIP->text().trimmed()
                                                              : m_sacnIP->text().trimmed());
    }
    if (kind == KindTargets && dest.kind == UniversePatcher::Destination::Unicast)
    {
        address = dest.address;
        /* Nothing typed: this universe's own number, as the plugin sends
           when it is not told one. */
        const QString txt = m_targetPort->text().trimmed();
        portAddress = m_universe;
        if (txt.isEmpty() == false && UniversePatcher::parsePortAddress(txt, portAddress) == false)
            return UniversePatcher::portAddressError(txt);
        /* One node port, one universe (UniversePatcher::applyTarget()). */
        const quint32 owner = m_patcher->universeOnTarget(p->name(), pickedLine(), address,
                                                          portAddress, m_universe);
        if (owner != InputOutputMap::invalidUniverse())
            return tr("Universe %1 already sends to %2 %3.\n\nOne port carries one universe. "
                      "Move universe %1 first, or pick another port.")
                .arg(owner + 1).arg(address, UniversePatcher::portAddressText(portAddress));
    }
    if (kind == KindSacn && m_sacnMulticast->isChecked() == false
            && m_sacnIP->text().trimmed().isEmpty())
        return tr("Unicast to which receiver? Type its IP, or tick Multicast.");

    if (m_feedbackIP->isEnabled() && m_feedbackIP->text().trimmed().isEmpty() == false)
    {
        fbAddress = m_feedbackIP->text().trimmed();
        const QString txt = m_feedbackPort->text().trimmed();
        fbPort = m_universe;
        if (txt.isEmpty() == false && UniversePatcher::parsePortAddress(txt, fbPort) == false)
            return UniversePatcher::portAddressError(txt);
    }
    return QString();
}

bool ConnectionEditor::apply()
{
    m_error.clear();
    InputOutputMap *iom = m_doc->inputOutputMap();
    Universe *u = iom != nullptr ? iom->universe(m_universe) : nullptr;
    if (u == nullptr)
    {
        m_error = tr("That universe is gone.");
        return false;
    }

    QString address, fbAddress;
    quint32 portAddress = 0, fbPort = 0;
    m_error = validate(address, portAddress, fbAddress, fbPort);
    if (m_error.isEmpty() == false)
        return false;

    // One step for all of it.
    if (iom->patchUndo() != nullptr)
        iom->patchUndo()->capture(QList<quint32>() << m_universe,
                                  tr("edit the connection of universe %1").arg(m_universe + 1));

    // ---- The output leg ----
    QLCIOPlugin *p = pickedPlugin();
    const QString plugin = p != nullptr ? p->name() : QString();
    const quint32 line = pickedLine();
    if (p == nullptr)
    {
        if (m_origIndex >= 0)
            iom->setOutputPatch(m_universe, QString(), QString(), 0, false, m_origIndex);
    }
    else
    {
        /* A new patch replaces the universe's first (a pending one waiting
           for its interface included), as patching it always has. */
        const int index = m_origIndex >= 0 ? m_origIndex : 0;
        const bool samePlugin = m_origIndex >= 0 && plugin == m_origPlugin;
        if (samePlugin == false || line != m_origLine)
        {
            if (iom->setOutputPatch(m_universe, plugin, QString(), line, false, index) == false)
            {
                m_error = tr("Could not patch that universe to %1.").arg(plugin);
                return false;
            }
        }
        OutputPatch *op = iom->outputPatch(m_universe, index);
        if (op == nullptr || op->plugin() == nullptr || op->plugin()->name() != plugin)
        {
            m_error = tr("Could not patch that universe to %1.").arg(plugin);
            return false;
        }
        /* Another protocol's settings mean nothing to this one. */
        if (samePlugin == false)
        {
            foreach (const QString &key, op->getPluginParameters().keys())
                op->unSetPluginParameter(key);
        }

        switch (kindOf(p))
        {
            case KindTargets:
                if (address.isEmpty())
                {
                    op->unSetPluginParameter(QStringLiteral("outputIP"));
                    op->unSetPluginParameter(QStringLiteral("outputUni"));
                }
                else
                {
                    op->setPluginParameter(QStringLiteral("outputIP"), address);
                    op->setPluginParameter(QStringLiteral("outputUni"), portAddress);
                }
                op->setPluginParameter(QStringLiteral("transmitMode"), m_transmitMode->currentText());
            break;
            case KindSacn:
                op->setPluginParameter(QStringLiteral("multicast"), m_sacnMulticast->isChecked() ? 1 : 0);
                if (m_sacnMulticast->isChecked())
                    op->unSetPluginParameter(QStringLiteral("ucastIP"));
                else
                    op->setPluginParameter(QStringLiteral("ucastIP"), m_sacnIP->text().trimmed());
                op->setPluginParameter(QStringLiteral("universe"), m_sacnUniverse->value());
                op->setPluginParameter(QStringLiteral("priority"), m_sacnPriority->value());
                op->setPluginParameter(QStringLiteral("transmitMode"), m_sacnMode->currentText());
            break;
            case KindMidi:
                op->setPluginParameter(QStringLiteral("mode"), m_midiMode->currentText());
            break;
            case KindWled:
            case KindDevice:
            case KindNone:
            break;
        }
    }

    // ---- Input, profile ----
    const QString inPlugin = m_inputCombo->currentData(ROLE_PLUGIN).toString();
    const quint32 inLine = m_inputCombo->currentData(ROLE_LINE).toUInt();
    const QString profile = inPlugin.isEmpty() ? QString() : m_profileCombo->currentData().toString();
    InputPatch *ip = u->inputPatch();
    const bool inSame = (ip == nullptr || ip->plugin() == nullptr)
        ? inPlugin.isEmpty()
        : (ip->plugin()->name() == inPlugin && ip->input() == inLine);
    if (inSame == false)
        iom->setInputPatch(m_universe, inPlugin, QString(), inLine, profile);
    else if (inPlugin.isEmpty() == false && ip != nullptr && ip->profileName() != profile)
        iom->setInputProfile(m_universe, profile);
    ip = u->inputPatch();
    if (ip != nullptr && inPlugin.isEmpty() == false)
    {
        const QMap<QString, QVariant> pp = ip->getPluginParameters();
        if (m_midiChannel->isEnabled()
                && pp.value(QStringLiteral("midichannel"), kMidiOmni).toInt()
                   != m_midiChannel->currentData().toInt())
            ip->setPluginParameter(QStringLiteral("midichannel"), m_midiChannel->currentData().toInt());
        if (m_artnetInUni->isEnabled()
                && pp.value(QStringLiteral("inputUni"), m_universe).toInt() != m_artnetInUni->value())
            ip->setPluginParameter(QStringLiteral("inputUni"), m_artnetInUni->value());
    }

    // ---- Feedback ----
    const QString fbPlugin = m_feedbackCombo->currentData(ROLE_PLUGIN).toString();
    const quint32 fbLine = m_feedbackCombo->currentData(ROLE_LINE).toUInt();
    OutputPatch *fb = u->feedbackPatch();
    const bool fbSame = (fb == nullptr || fb->plugin() == nullptr)
        ? fbPlugin.isEmpty()
        : (fb->plugin()->name() == fbPlugin && fb->output() == fbLine);
    if (fbSame == false)
        iom->setOutputPatch(m_universe, fbPlugin, QString(), fbLine, true, 0);
    fb = u->feedbackPatch();
    if (fb != nullptr && fbPlugin.isEmpty() == false && m_feedbackIP->isEnabled())
    {
        if (fbAddress.isEmpty())
        {
            fb->unSetPluginParameter(QStringLiteral("outputIP"));
            fb->unSetPluginParameter(QStringLiteral("outputUni"));
        }
        else
        {
            fb->setPluginParameter(QStringLiteral("outputIP"), fbAddress);
            fb->setPluginParameter(QStringLiteral("outputUni"), fbPort);
        }
    }

    // ---- The universe itself ----
    if (iom->getUniversePassthrough(int(m_universe)) != m_passthrough->isChecked())
    {
        iom->setUniversePassthrough(int(m_universe), m_passthrough->isChecked());
        iom->saveDefaults();
    }
    const QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty() == false && name != u->name())
        iom->setUniverseName(int(m_universe), name);

    m_doc->setModified();
    /* The leg it is now, so a pane filled again finds it. */
    m_origPlugin = plugin;
    m_origLine = line;
    return true;
}

void ConnectionEditor::reject()
{
    if (m_pane)
    {
        load();                 // Esc in the pane: back to the patch as it is
        m_paneNote->hide();
        return;
    }
    QDialog::reject();
}

void ConnectionEditor::accept()
{
    if (m_pane)
    {
        slotCommit();
        return;
    }
    const bool wasWled = m_origPlugin == QLatin1String("WLED") && m_origIndex >= 0;
    if (apply() == false)
    {
        QMessageBox::warning(this, windowTitle(), m_error);
        return;
    }
    /* A universe newly sent to WLED: now it has a patch, its devices. */
    if (kindOf(pickedPlugin()) == KindWled && (wasWled == false || pickedLine() != m_origLine))
        WledDevicesDialog::edit(m_doc, m_universe, pickedLine(), parentWidget());
    QDialog::accept();
}

/*********************************************************************
 * As a pane
 *********************************************************************/

ConnectionEditor *ConnectionEditor::createPane(Doc *doc, QWidget *parent)
{
    ConnectionEditor *ed = new ConnectionEditor(doc, 0, QString(), 0, parent);
    ed->m_pane = true;
    ed->setWindowFlags(Qt::Widget);
    ed->setObjectName(QStringLiteral("connectionPane"));
    ed->m_buttons->hide();
    ed->m_topForm->setRowVisible(ed->m_universeCombo, false);
    /* Which universe, as a heading: the list on the left picks it. */
    ed->m_paneTitle = new QLabel(ed);
    ed->m_paneTitle->setObjectName(QStringLiteral("connPaneTitle"));
    QFont f = ed->m_paneTitle->font();
    f.setBold(true);
    ed->m_paneTitle->setFont(f);
    if (QWidget *body = ed->m_paneNote->parentWidget())
        if (QVBoxLayout *vl = qobject_cast<QVBoxLayout *>(body->layout()))
            vl->insertWidget(0, ed->m_paneTitle);

    /* Made as each is committed: a list picked, a field left. */
    auto commit = [ed]() { ed->schedulePaneCommit(); };
    foreach (QComboBox *c, QList<QComboBox *>() << ed->m_transmitMode << ed->m_sacnMode
             << ed->m_midiMode << ed->m_inputCombo << ed->m_profileCombo << ed->m_midiChannel
             << ed->m_feedbackCombo)
        connect(c, QOverload<int>::of(&QComboBox::currentIndexChanged), ed, commit);
    foreach (QLineEdit *e, QList<QLineEdit *>() << ed->m_targetIP << ed->m_sacnIP
             << ed->m_feedbackIP << ed->m_feedbackPort << ed->m_nameEdit)
        connect(e, &QLineEdit::editingFinished, ed, commit);
    foreach (QSpinBox *sp, QList<QSpinBox *>() << ed->m_sacnUniverse << ed->m_sacnPriority
             << ed->m_artnetInUni)
        connect(sp, &QSpinBox::editingFinished, ed, commit);
    connect(ed->m_passthrough, &QCheckBox::toggled, ed, commit);
    ed->markClean();
    return ed;
}

void ConnectionEditor::setUniverse(quint32 universe, const QString &pluginName, quint32 line)
{
    m_universe = universe;
    m_origPlugin = pluginName;
    m_origLine = line;
    m_paneNote->hide();
    load();
}

bool ConnectionEditor::commitPending()
{
    if (m_pane == false)
        return true;
    if (formState() == m_cleanState)
    {
        m_commitQueued = false;
        return true;
    }
    slotCommit();
    return m_paneNote->isHidden();
}

void ConnectionEditor::reloadIfClean()
{
    if (m_commitQueued || formState() != m_cleanState)
        return;
    load();
}

QString ConnectionEditor::formState() const
{
    QStringList v;
    v << pickedPluginName() << QString::number(m_interfaceCombo->currentIndex())
      << QString::number(m_autoInterface)
      << m_targetIP->text().trimmed() << m_targetPort->text().trimmed()
      << m_transmitMode->currentText()
      << QString::number(m_sacnMulticast->isChecked()) << m_sacnIP->text().trimmed()
      << QString::number(m_sacnUniverse->value()) << QString::number(m_sacnPriority->value())
      << m_sacnMode->currentText() << m_midiMode->currentText()
      << QString::number(m_inputCombo->currentIndex())
      << QString::number(m_profileCombo->currentIndex())
      << QString::number(m_midiChannel->currentIndex())
      << QString::number(m_artnetInUni->value())
      << QString::number(m_feedbackCombo->currentIndex())
      << m_feedbackIP->text().trimmed() << m_feedbackPort->text().trimmed()
      << QString::number(m_passthrough->isChecked()) << m_nameEdit->text().trimmed();
    return v.join(QChar(0x1f));
}

void ConnectionEditor::markClean()
{
    m_cleanState = formState();
}

void ConnectionEditor::schedulePaneCommit()
{
    if (m_pane == false || m_loading || m_commitQueued)
        return;
    m_commitQueued = true;
    QTimer::singleShot(0, this, &ConnectionEditor::slotCommit);
}

void ConnectionEditor::slotCommit()
{
    m_commitQueued = false;
    if (m_pane == false || formState() == m_cleanState)
        return;
    if (apply() == false)
    {
        m_paneNote->setText(m_error);
        m_paneNote->show();
        return;
    }
    m_paneNote->hide();
    load();
    emit changed();
}
