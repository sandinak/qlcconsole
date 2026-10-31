/*
  Q Light Controller Plus
  showcontrolsyncpage.cpp

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

#include <QTimer>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QRegularExpression>
#include <QScrollArea>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QSpinBox>
#include <QLabel>
#include <QLineEdit>

#include "showcontrolsyncpage.h"
#include "controlmappanel.h"
#include "showcontrollistener.h"
#include "timecodesource.h"
#include "inputoutputmap.h"
#include "showcontrol.h"
#include "showwebaccess.h"
#include "inputpatch.h"
#include "doc.h"

namespace
{
/** Hour:minute:second:frame at @a fps. */
QString timecodeText(quint32 ms, int fps)
{
    if (fps <= 0)
        fps = 30;
    const quint32 sec = ms / 1000;
    return QStringLiteral("%1:%2:%3:%4")
        .arg(sec / 3600, 2, 10, QChar('0'))
        .arg((sec % 3600) / 60, 2, 10, QChar('0'))
        .arg(sec % 60, 2, 10, QChar('0'))
        .arg(int((ms % 1000) * quint32(fps) / 1000), 2, 10, QChar('0'));
}

/** A universe's name, or "Universe N" when it has none. */
QString universeName(InputOutputMap *iom, quint32 index)
{
    const QString name = iom->getUniverseNameByIndex(index);
    return name.isEmpty() ? ShowControlSyncPage::tr("Universe %1").arg(index + 1) : name;
}

QLabel *noteLabel(const QString &text, QWidget *parent)
{
    QLabel *l = new QLabel(text, parent);
    l->setWordWrap(true);
    l->setStyleSheet(QStringLiteral("color: gray;"));
    return l;
}

/** The port at the end of a web access URL ("http://host:9999/"), or
 *  @a fallback; the host may be a placeholder, so no QUrl. */
int webUrlPort(const QString &url, int fallback)
{
    static const QRegularExpression re(QStringLiteral(":(\\d+)/?$"));
    const QRegularExpressionMatch m = re.match(url);
    return m.hasMatch() ? m.captured(1).toInt() : fallback;
}

/** True when a web access URL is this computer only (127.x, ::1). */
bool webUrlIsLocal(const QString &url)
{
    return url.startsWith(QLatin1String("http://127.")) || url.startsWith(QLatin1String("http://[::1]"));
}

const char *kGreen = "color: #27ae60;";
const char *kAmber = "color: #e67e22;";
const char *kRed   = "color: #c0392b;";
const char *kGrey  = "color: gray;";
}

ShowControlSyncPage::ShowControlSyncPage(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_updating(false)
{
    Q_ASSERT(doc != nullptr);
    setObjectName(QStringLiteral("connectionsShowControl"));
    setProperty("helpTopic", QStringLiteral("show-control.md"));

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);

    QWidget *content = new QWidget(scroll);
    QVBoxLayout *lay = new QVBoxLayout(content);
    lay->addWidget(noteLabel(tr("Everything outside that controls or clocks the show: cue "
                                "commands from QLab (or anything that sends OSC or MIDI Show "
                                "Control), MIDI timecode, web access and the controls that fire things (the Control "
                                "Map). Changes take effect at once."),
                             content));
    m_groups.insert(QStringLiteral("osc"), buildOscGroup());
    m_groups.insert(QStringLiteral("msc"), buildMscGroup());
    m_groups.insert(QStringLiteral("out"), buildOutGroup());
    m_groups.insert(QStringLiteral("mtc"), buildMtcGroup());
    m_groups.insert(QStringLiteral("web"), buildWebGroup());
    m_groups.insert(QStringLiteral("map"), buildMapGroup());
    for (const QString &key : { QStringLiteral("osc"), QStringLiteral("msc"), QStringLiteral("out"), QStringLiteral("mtc"),
                                QStringLiteral("web"), QStringLiteral("map") })
        lay->addWidget(m_groups.value(key));
    lay->addStretch();
    scroll->setWidget(content);

    InputOutputMap *iom = m_doc->inputOutputMap();
    /* Once for a run of them: a show's universes are added one at a time
       as it loads, and a rebuild over all of them for each was O(N^2) --
       minutes for a few thousand (F1). */
    connect(iom, &InputOutputMap::universeAdded, this, &ShowControlSyncPage::scheduleMidiInputsRefresh);
    connect(iom, &InputOutputMap::universeRemoved, this, &ShowControlSyncPage::scheduleMidiInputsRefresh);
    connect(iom, &InputOutputMap::pluginConfigurationChanged,
            this, &ShowControlSyncPage::refreshMidiInputs);

    TimecodeSource *tc = m_doc->timecodeSource();
    connect(tc, &TimecodeSource::timeChanged, this, &ShowControlSyncPage::slotTimeChanged);
    connect(tc, &TimecodeSource::runningChanged, this, &ShowControlSyncPage::refreshTimecode);
    connect(tc, &TimecodeSource::sourceUniverseChanged,
            this, &ShowControlSyncPage::refreshMidiInputs);

    ShowWebAccess *web = m_doc->webAccess();
    connect(web, &ShowWebAccess::settingsChanged, this, &ShowControlSyncPage::refreshWeb);
    connect(web, &ShowWebAccess::statusChanged, this, &ShowControlSyncPage::refreshWeb);

    setListener(nullptr);
    refreshMidiInputs();
    refreshWeb();
    refreshOut();
}

QGroupBox *ShowControlSyncPage::buildOscGroup()
{
    QGroupBox *box = new QGroupBox(tr("OSC in"), this);
    box->setObjectName(QStringLiteral("scOscGroup"));
    QFormLayout *form = new QFormLayout(box);

    QHBoxLayout *row = new QHBoxLayout();
    m_oscCheck = new QCheckBox(tr("Listen for OSC cue commands on port"), box);
    m_oscCheck->setObjectName(QStringLiteral("scOscEnabled"));
    m_oscCheck->setToolTip(tr("QLab's Network cue (or anything that sends OSC) can GO a cue: "
                              "/go, /cue/12/start, /subgo, /stop... Anyone on the network can, so it's off "
                              "until you turn it on."));
    m_oscPortSpin = new QSpinBox(box);
    m_oscPortSpin->setObjectName(QStringLiteral("scOscPort"));
    m_oscPortSpin->setRange(1, 65535);
    // Bind once the number is finished, not on every digit typed.
    m_oscPortSpin->setKeyboardTracking(false);
    m_oscPortSpin->setToolTip(tr("Not 53000 or 53001 if QLab runs on this computer: it uses those."));
    row->addWidget(m_oscCheck);
    row->addWidget(m_oscPortSpin);
    row->addStretch();
    form->addRow(row);

    QStringList addresses;
    for (const QHostAddress &a : QNetworkInterface::allAddresses())
        if (a.protocol() == QAbstractSocket::IPv4Protocol && !a.isLoopback())
            addresses << a.toString();
    QLabel *where = noteLabel(addresses.isEmpty()
        ? tr("Send to this computer as localhost (127.0.0.1).")
        : tr("Send to this computer at %1 (or 127.0.0.1 on this computer).")
              .arg(addresses.join(tr(" or "))), box);
    where->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(where);

    m_oscStatus = new QLabel(box);
    m_oscStatus->setObjectName(QStringLiteral("scOscStatus"));
    m_oscStatus->setWordWrap(true);
    form->addRow(tr("Status:"), m_oscStatus);

    m_oscLast = new QLabel(box);
    m_oscLast->setObjectName(QStringLiteral("scOscLast"));
    m_oscLast->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_oscLast->setWordWrap(true);
    form->addRow(tr("Last received:"), m_oscLast);

    connect(m_oscCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (!m_updating && m_listener)
            m_listener->setOscEnabled(on);
    });
    connect(m_oscPortSpin, &QSpinBox::valueChanged, this, [this](int port) {
        if (!m_updating && m_listener)
            m_listener->setOscPort(port);
    });
    return box;
}

QGroupBox *ShowControlSyncPage::buildMscGroup()
{
    QGroupBox *box = new QGroupBox(tr("MIDI Show Control in"), this);
    box->setObjectName(QStringLiteral("scMscGroup"));
    QFormLayout *form = new QFormLayout(box);

    QHBoxLayout *row = new QHBoxLayout();
    m_mscCheck = new QCheckBox(tr("Answer MIDI Show Control as device"), box);
    m_mscCheck->setObjectName(QStringLiteral("scMscEnabled"));
    m_mscCheck->setToolTip(tr("MSC GO, STOP, RESUME, LOAD, ALL_OFF, RESET and GO_OFF arriving on "
                              "a patched MIDI input, for lighting, moving lights or all types. "
                              "The all-call (127) is always answered."));
    m_mscDeviceSpin = new QSpinBox(box);
    m_mscDeviceSpin->setObjectName(QStringLiteral("scMscDevice"));
    m_mscDeviceSpin->setRange(0, 0x6F);
    m_mscDeviceSpin->setKeyboardTracking(false);
    m_mscDeviceSpin->setToolTip(tr("The device number QLab's MIDI cue sends to."));
    row->addWidget(m_mscCheck);
    row->addWidget(m_mscDeviceSpin);
    row->addStretch();
    form->addRow(row);

    m_mscInputCombo = new QComboBox(box);
    m_mscInputCombo->setObjectName(QStringLiteral("scMscInput"));
    m_mscInputCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_mscInputCombo->setToolTip(tr("Answer MSC from every MIDI input, or only from the port QLab "
                                   "sends on. The choice is this computer's: it names the port, "
                                   "not a universe, so it holds for every show."));
    connect(m_mscInputCombo, &QComboBox::activated,
            this, &ShowControlSyncPage::slotMscInputActivated);
    form->addRow(tr("Listen on:"), m_mscInputCombo);

    QHBoxLayout *inputs = new QHBoxLayout();
    m_mscInputs = new QLabel(box);
    m_mscInputs->setObjectName(QStringLiteral("scMscInputs"));
    m_mscInputs->setWordWrap(true);
    m_mscInputs->setToolTip(tr("Where MSC can come in: the universes with that MIDI input "
                               "patched (the same inputs MIDI timecode can come in on)."));
    m_patchMidiButton = new QPushButton(tr("Patch a MIDI input…"), box);
    m_patchMidiButton->setObjectName(QStringLiteral("scPatchMidi"));
    connect(m_patchMidiButton, &QPushButton::clicked,
            this, &ShowControlSyncPage::patchMidiInputRequested);
    inputs->addWidget(m_mscInputs, 1);
    inputs->addWidget(m_patchMidiButton, 0, Qt::AlignTop);
    form->addRow(QString(), inputs);

    m_mscLast = new QLabel(box);
    m_mscLast->setObjectName(QStringLiteral("scMscLast"));
    m_mscLast->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_mscLast->setWordWrap(true);
    form->addRow(tr("Last received:"), m_mscLast);

    connect(m_mscCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (!m_updating && m_listener)
            m_listener->setMscEnabled(on);
    });
    connect(m_mscDeviceSpin, &QSpinBox::valueChanged, this, [this](int id) {
        if (!m_updating && m_listener)
            m_listener->setMscDeviceId(id);
    });
    return box;
}

QGroupBox *ShowControlSyncPage::buildOutGroup()
{
    // The outgoing half: a GO sends its cue on to QLab and the like. Per
    // show (saved with it), through the OSC and MIDI plugins' outputs.
    QGroupBox *box = new QGroupBox(tr("QLab out (this show)"), this);
    box->setObjectName(QStringLiteral("scQLabOutGroup"));
    QFormLayout *form = new QFormLayout(box);

    form->addRow(noteLabel(tr("When a cue goes on stage from this console, send it on. Off until "
                              "you turn it on; a cue that came in by OSC or MSC is not sent back."),
                           box));

    QHBoxLayout *osc = new QHBoxLayout();
    m_outOscCheck = new QCheckBox(tr("Send OSC to"), box);
    m_outOscCheck->setObjectName(QStringLiteral("scOutOsc"));
    m_outHost = new QLineEdit(box);
    m_outHost->setObjectName(QStringLiteral("scOutHost"));
    m_outHost->setPlaceholderText(tr("host or IP"));
    m_outHost->setToolTip(tr("The computer QLab runs on: its address on the network, or 127.0.0.1 "
                             "on this computer."));
    m_outPort = new QSpinBox(box);
    m_outPort->setObjectName(QStringLiteral("scOutPort"));
    m_outPort->setRange(1, 65535);
    m_outPort->setKeyboardTracking(false);
    m_outPort->setValue(ShowControl::OutSettings().oscPort);
    m_outPort->setToolTip(tr("QLab listens on 53000."));
    osc->addWidget(m_outOscCheck);
    osc->addWidget(m_outHost, 1);
    osc->addWidget(m_outPort);
    form->addRow(osc);

    m_outPath = new QComboBox(box);
    m_outPath->setObjectName(QStringLiteral("scOutPath"));
    m_outPath->setEditable(true);
    m_outPath->addItem(ShowControl::OutSettings().oscPath);
    m_outPath->addItem(QStringLiteral("/go"));
    m_outPath->setToolTip(tr("The OSC address. {cue} is the cue number: /cue/{cue}/start starts "
                             "that cue in QLab; /go sends QLab's own GO."));
    form->addRow(tr("Address:"), m_outPath);

    m_outLine = new QComboBox(box);
    m_outLine->setObjectName(QStringLiteral("scOutLine"));
    m_outLine->setToolTip(tr("The network interface it leaves from (the OSC plugin's output)."));
    form->addRow(tr("From:"), m_outLine);

    QHBoxLayout *msc = new QHBoxLayout();
    m_outMscCheck = new QCheckBox(tr("Send MIDI Show Control GO to device"), box);
    m_outMscCheck->setObjectName(QStringLiteral("scOutMsc"));
    m_outDevice = new QSpinBox(box);
    m_outDevice->setObjectName(QStringLiteral("scOutDevice"));
    m_outDevice->setRange(0, 0x6F);
    m_outDevice->setKeyboardTracking(false);
    m_outDevice->setValue(ShowControl::OutSettings().mscDevice);
    m_outDevice->setToolTip(tr("The device number QLab's MIDI input answers to."));
    msc->addWidget(m_outMscCheck);
    msc->addWidget(m_outDevice);
    msc->addStretch();
    form->addRow(msc);

    m_outMidi = new QComboBox(box);
    m_outMidi->setObjectName(QStringLiteral("scOutMidi"));
    m_outMidi->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_outMidi->setToolTip(tr("The MIDI output the GO leaves on."));
    form->addRow(tr("On:"), m_outMidi);

    m_outCues = new QComboBox(box);
    m_outCues->setObjectName(QStringLiteral("scOutCues"));
    m_outCues->addItem(tr("Every cue"), int(ShowControl::OutAllCues));
    m_outCues->addItem(tr("Only cues with a cue number of their own"), int(ShowControl::OutNumberedCues));
    m_outCues->setToolTip(tr("A cue's number is the one set in its cue list (or by Renumber). "
                             "Cues left without one are not sent."));
    form->addRow(tr("Send:"), m_outCues);

    m_outLast = new QLabel(tr("Nothing sent yet."), box);
    m_outLast->setObjectName(QStringLiteral("scOutLast"));
    m_outLast->setWordWrap(true);
    form->addRow(tr("Last sent:"), m_outLast);

    connect(m_doc->showControl(), &ShowControl::sentOut, this, [this](const QString &what, bool ok) {
        m_outLast->setText(ok ? what : tr("%1 -- failed (output not available)").arg(what));
    });
    auto changed = [this]() { applyOut(); };
    connect(m_outOscCheck, &QCheckBox::toggled, this, changed);
    connect(m_outHost, &QLineEdit::editingFinished, this, changed);
    connect(m_outPort, &QSpinBox::valueChanged, this, changed);
    connect(m_outPath, &QComboBox::currentTextChanged, this, changed);
    connect(m_outLine, &QComboBox::activated, this, changed);
    connect(m_outMscCheck, &QCheckBox::toggled, this, changed);
    connect(m_outDevice, &QSpinBox::valueChanged, this, changed);
    connect(m_outMidi, &QComboBox::activated, this, changed);
    connect(m_outCues, &QComboBox::activated, this, changed);
    return box;
}

void ShowControlSyncPage::refreshOut()
{
    if (m_outOscCheck == nullptr)
        return;
    const ShowControl::OutSettings o = m_doc->showControl()->outSettings();
    const bool was = m_updating;
    m_updating = true;
    InputOutputMap *iom = m_doc->inputOutputMap();
    m_outLine->clear();
    m_outLine->addItem(tr("The interface on that network"), QString());
    for (const QString &n : iom->pluginOutputs(QStringLiteral("OSC")))
        m_outLine->addItem(n, n);
    if (!o.oscLine.isEmpty() && m_outLine->findData(o.oscLine) < 0)
        m_outLine->addItem(tr("%1 (not on this computer now)").arg(o.oscLine), o.oscLine);
    m_outLine->setCurrentIndex(qMax(0, m_outLine->findData(o.oscLine)));

    m_outMidi->clear();
    m_outMidi->addItem(tr("(none)"), QString());
    for (const QString &n : iom->pluginOutputs(QStringLiteral("MIDI")))
        m_outMidi->addItem(n, n);
    if (!o.mscOutput.isEmpty() && m_outMidi->findData(o.mscOutput) < 0)
        m_outMidi->addItem(tr("%1 (not on this computer now)").arg(o.mscOutput), o.mscOutput);
    m_outMidi->setCurrentIndex(qMax(0, m_outMidi->findData(o.mscOutput)));

    m_outOscCheck->setChecked(o.oscEnabled);
    m_outHost->setText(o.oscHost);
    m_outPort->setValue(o.oscPort);
    m_outPath->setCurrentText(o.oscPath);
    m_outMscCheck->setChecked(o.mscEnabled);
    m_outDevice->setValue(o.mscDevice);
    m_outCues->setCurrentIndex(qMax(0, m_outCues->findData(int(o.cues))));
    m_updating = was;
}

void ShowControlSyncPage::applyOut()
{
    if (m_updating)
        return;
    ShowControl::OutSettings o;
    o.oscEnabled = m_outOscCheck->isChecked();
    o.oscHost = m_outHost->text().trimmed();
    o.oscPort = m_outPort->value();
    o.oscPath = m_outPath->currentText().trimmed();
    o.oscLine = m_outLine->currentData().toString();
    o.mscEnabled = m_outMscCheck->isChecked();
    o.mscOutput = m_outMidi->currentData().toString();
    o.mscDevice = m_outDevice->value();
    o.cues = ShowControl::OutCues(m_outCues->currentData().toInt());
    if (o == m_doc->showControl()->outSettings())
        return;
    m_doc->showControl()->setOutSettings(o);
    m_doc->setModified();       // saved with the show
}

QGroupBox *ShowControlSyncPage::buildMtcGroup()
{
    QGroupBox *box = new QGroupBox(tr("MIDI timecode"), this);
    box->setObjectName(QStringLiteral("scMtcGroup"));
    QFormLayout *form = new QFormLayout(box);

    m_mtcSourceCombo = new QComboBox(box);
    m_mtcSourceCombo->setObjectName(QStringLiteral("scMtcSource"));
    m_mtcSourceCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_mtcSourceCombo->setToolTip(tr("Which universe's input the timecode is taken from: MIDI "
                                    "timecode on a MIDI input, Art-Net TimeCode on an Art-Net "
                                    "input. The same choice as the footer's MTC chip."));
    connect(m_mtcSourceCombo, &QComboBox::activated,
            this, &ShowControlSyncPage::slotMtcSourceActivated);
    form->addRow(tr("Source:"), m_mtcSourceCombo);

    m_mtcStatus = new QLabel(box);
    m_mtcStatus->setObjectName(QStringLiteral("scMtcStatus"));
    form->addRow(tr("Timecode:"), m_mtcStatus);

    m_mtcHealth = new QLabel(box);
    m_mtcHealth->setObjectName(QStringLiteral("scMtcHealth"));
    m_mtcHealth->setWordWrap(true);
    m_mtcHealth->setToolTip(tr("Measured on the incoming code: how fast it advances against "
                               "real time, how evenly it arrives (jitter), and how often."));
    form->addRow(tr("Sync health:"), m_mtcHealth);

    form->addRow(noteLabel(tr("A show follows it when Follow MTC is on in Shows; its offset "
                              "is set there too."), box));
    return box;
}

QGroupBox *ShowControlSyncPage::buildMapGroup()
{
    // MIDI and OSC controls bound straight to what they fire (no VC).
    QGroupBox *box = new QGroupBox(tr("Control Map (MIDI and OSC triggers)"), this);
    box->setObjectName(QStringLiteral("scMapGroup"));
    QVBoxLayout *lay = new QVBoxLayout(box);
    m_controlMap = new ControlMapPanel(m_doc, box);
    lay->addWidget(m_controlMap);
    connect(m_controlMap, &ControlMapPanel::summaryChanged, this, &ShowControlSyncPage::checkSummary);
    return box;
}

QGroupBox *ShowControlSyncPage::buildWebGroup()
{
    QGroupBox *box = new QGroupBox(tr("Web access"), this);
    box->setObjectName(QStringLiteral("scWebGroup"));
    QFormLayout *form = new QFormLayout(box);

    QHBoxLayout *row = new QHBoxLayout();
    m_webCheck = new QCheckBox(tr("Start web access with this show, on port"), box);
    m_webCheck->setObjectName(QStringLiteral("scWebEnabled"));
    m_webCheck->setToolTip(tr("Opening this show starts the web server: the Virtual Console, "
                              "Simple Desk and Configuration pages in a browser. Saved with "
                              "the show. Starting qlcconsole with -w overrides it."));
    m_webPortSpin = new QSpinBox(box);
    m_webPortSpin->setObjectName(QStringLiteral("scWebPort"));
    m_webPortSpin->setRange(1, 65535);
    m_webPortSpin->setKeyboardTracking(false);
    m_webPortSpin->setToolTip(tr("9999 unless something else on this computer uses it."));
    row->addWidget(m_webCheck);
    row->addWidget(m_webPortSpin);
    row->addStretch();
    form->addRow(row);

    m_webNetworkCheck = new QCheckBox(tr("Reachable from other devices on the network"), box);
    m_webNetworkCheck->setObjectName(QStringLiteral("scWebNetwork"));
    m_webNetworkCheck->setToolTip(tr("Off: a browser on this computer only. On: tablets and phones "
                                     "on any network this computer is on can open the page."));
    form->addRow(m_webNetworkCheck);

    m_webLoginCheck = new QCheckBox(tr("Users log in"), box);
    m_webLoginCheck->setObjectName(QStringLiteral("scWebLogin"));
    m_webLoginCheck->setToolTip(tr("As with -wa: everyone logs in with an account (Configuration ▸ "
                                   "Users on the web page; the first admin is made from this "
                                   "computer). Without it, other devices can look but change nothing."));
    form->addRow(m_webLoginCheck);

    m_webWhere = new QLabel(box);
    m_webWhere->setObjectName(QStringLiteral("scWebWhere"));
    m_webWhere->setWordWrap(true);
    m_webWhere->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(tr("Open:"), m_webWhere);

    m_webStatus = new QLabel(box);
    m_webStatus->setObjectName(QStringLiteral("scWebStatus"));
    m_webStatus->setWordWrap(true);
    form->addRow(tr("Status:"), m_webStatus);

    form->addRow(noteLabel(tr("Whoever can open the page can run the Virtual Console. Plain "
                              "http, no encryption: keep it on the show network, with logins "
                              "when it is reachable from the network."), box));

    ShowWebAccess *web = m_doc->webAccess();
    connect(m_webCheck, &QCheckBox::toggled, this, [this, web](bool on) {
        if (!m_updating)
            web->setEnabled(on);
    });
    connect(m_webPortSpin, &QSpinBox::valueChanged, this, [this, web](int port) {
        if (!m_updating)
            web->setPort(port);
    });
    connect(m_webNetworkCheck, &QCheckBox::toggled, this, [this, web](bool on) {
        if (!m_updating)
            web->setNetwork(on);
    });
    connect(m_webLoginCheck, &QCheckBox::toggled, this, [this, web](bool on) {
        if (!m_updating)
            web->setLogin(on);
    });
    return box;
}

void ShowControlSyncPage::refreshWeb()
{
    const ShowWebAccess *web = m_doc->webAccess();

    const bool was = m_updating;
    m_updating = true;
    m_webCheck->setChecked(web->isEnabled());
    m_webPortSpin->setValue(web->port());
    m_webNetworkCheck->setChecked(web->network());
    m_webLoginCheck->setChecked(web->login());
    m_updating = was;

    /* Where to point a browser: the addresses for the port it runs on, or
       will run on once on (-w's port and audience, when it ran from there). */
    const ShowWebAccess::State state = web->state();
    const bool running = (state == ShowWebAccess::Running || state == ShowWebAccess::CommandLine)
                         && !web->url().isEmpty();
    const int port = running ? webUrlPort(web->url(), web->port()) : web->port();
    const bool network = running ? !webUrlIsLocal(web->url()) : web->network();
    QStringList urls { QStringLiteral("http://127.0.0.1:%1/").arg(port) };
    if (network)
        for (const QHostAddress &a : QNetworkInterface::allAddresses())
            if (a.protocol() == QAbstractSocket::IPv4Protocol && !a.isLoopback())
                urls << QStringLiteral("http://%1:%2/").arg(a.toString()).arg(port);
    m_webWhere->setText(network ? urls.join(QStringLiteral("\n"))
                                : tr("%1 (this computer only)").arg(urls.first()));

    QString text;
    if (running)
    {
        text = network ? tr("Running on port %1, reachable from the network.").arg(port)
                       : tr("Running on port %1, this computer only.").arg(port);
        if (!web->message().isEmpty())
            text += QLatin1Char(' ') + web->message();
    }
    switch (running ? ShowWebAccess::Running : state)
    {
        case ShowWebAccess::Running:
            m_webStatus->setText(text);
            // Amber: reachable by others, worth knowing at a glance.
            m_webStatus->setStyleSheet(network ? kAmber : kGreen);
            break;
        case ShowWebAccess::Failed:
            m_webStatus->setText(web->message());
            m_webStatus->setStyleSheet(kRed);
            break;
        default:
            m_webStatus->setText(web->isEnabled() ? tr("Starting…") : tr("Off"));
            m_webStatus->setStyleSheet(kGrey);
            break;
    }
    m_webStatus->setProperty("webState", int(state));
    checkSummary();
}

void ShowControlSyncPage::setListener(ShowControlListener *listener)
{
    if (m_listener)
        disconnect(m_listener, nullptr, this, nullptr);
    m_listener = listener;

    const bool have = (listener != nullptr);
    for (QWidget *w : { static_cast<QWidget *>(m_oscCheck), static_cast<QWidget *>(m_oscPortSpin),
                        static_cast<QWidget *>(m_mscCheck), static_cast<QWidget *>(m_mscDeviceSpin),
                        static_cast<QWidget *>(m_mscInputCombo) })
        w->setEnabled(have);

    if (have)
    {
        connect(listener, &ShowControlListener::settingsChanged,
                this, &ShowControlSyncPage::slotListenerSettingsChanged);
        connect(listener, &ShowControlListener::statusChanged,
                this, &ShowControlSyncPage::slotOscStatusChanged);
        connect(listener, &ShowControlListener::lastOscChanged,
                this, &ShowControlSyncPage::slotLastOscChanged);
        connect(listener, &ShowControlListener::lastMscChanged,
                this, &ShowControlSyncPage::slotLastMscChanged);
    }
    slotListenerSettingsChanged();
    slotLastOscChanged(have ? listener->lastOsc() : QString());
    slotLastMscChanged(have ? listener->lastMsc() : QString());
}

void ShowControlSyncPage::slotListenerSettingsChanged()
{
    // The settings themselves, as saved -- the same whether or not the
    // listener exists yet.
    m_updating = true;
    m_oscCheck->setChecked(ShowControlListener::oscEnabled());
    m_oscPortSpin->setValue(ShowControlListener::oscPort());
    m_mscCheck->setChecked(ShowControlListener::mscEnabled());
    m_mscDeviceSpin->setValue(ShowControlListener::mscDeviceId());
    m_updating = false;
    m_mscDeviceSpin->setEnabled(m_listener && m_mscCheck->isChecked());
    m_mscInputCombo->setEnabled(m_listener && m_mscCheck->isChecked());
    slotOscStatusChanged();
    refreshMidiInputs();
}

void ShowControlSyncPage::slotOscStatusChanged()
{
    if (!m_listener)
    {
        m_oscStatus->setText(tr("Not started"));
        m_oscStatus->setStyleSheet(kGrey);
        checkSummary();
        return;
    }
    m_oscStatus->setText(m_listener->status());
    switch (m_listener->oscState())
    {
        case ShowControlListener::OscListening: m_oscStatus->setStyleSheet(kGreen); break;
        case ShowControlListener::OscError:     m_oscStatus->setStyleSheet(kRed); break;
        default:                                m_oscStatus->setStyleSheet(kGrey); break;
    }
    m_oscStatus->setProperty("oscState", int(m_listener->oscState()));
    checkSummary();
}

void ShowControlSyncPage::slotLastOscChanged(const QString &what)
{
    m_oscLast->setText(what.isEmpty() ? tr("Nothing received yet.") : what);
}

void ShowControlSyncPage::slotLastMscChanged(const QString &what)
{
    m_mscLast->setText(what.isEmpty() ? tr("Nothing received yet.") : what);
}

void ShowControlSyncPage::scheduleMidiInputsRefresh()
{
    if (m_midiRefreshPending)
        return;
    m_midiRefreshPending = true;
    QTimer::singleShot(0, this, [this]() {
        m_midiRefreshPending = false;
        refreshMidiInputs();
    });
}

void ShowControlSyncPage::refreshMidiInputs()
{
    refreshOut();
    InputOutputMap *iom = m_doc->inputOutputMap();
    TimecodeSource *tc = m_doc->timecodeSource();
    const qint32 cur = tc->sourceUniverse();

    QStringList midi;
    QStringList patchedMidiInputs;
    m_mtcSourceCombo->clear();
    m_mtcSourceCombo->addItem(tr("Any universe (auto-detect)"), -1);

    /* The same universes the footer chip's menu offers: those with a MIDI
       input (the only kind that carries timecode), plus the one picked and
       the one code last came in on, so neither drops out of sight. */
    for (quint32 i = 0; i < iom->universesCount(); i++)
    {
        const quint32 uni = iom->getUniverseID(i);
        InputPatch *ip = iom->inputPatch(uni);
        const bool isMidi = (ip != nullptr && ip->pluginName() == QLatin1String("MIDI"));
        const bool selected = (qint32(uni) == cur);
        const bool sawCode = (tc->lastUniverse() == qint32(uni));
        QString name = universeName(iom, i);
        if (isMidi)
        {
            name += QStringLiteral("  —  %1").arg(ip->inputName());
            midi << name;
            patchedMidiInputs << ip->inputName();
        }
        else if (ip != nullptr && ip->pluginName() == QLatin1String("ArtNet"))
        {
            // Art-Net TimeCode follows the same path as MTC
            name += tr("  —  Art-Net TimeCode (%1)").arg(ip->inputName());
        }
        else if (!selected && !sawCode)
        {
            continue;
        }
        else
        {
            name += tr("  —  no MIDI input");
        }
        m_mtcSourceCombo->addItem(name, qint32(uni));
    }
    m_mtcSourceCombo->setCurrentIndex(qMax(0, m_mtcSourceCombo->findData(cur)));

    /* MSC: every MIDI input, or the one chosen -- by name, from this
       computer's MIDI ports, the patched ones and the saved choice. */
    const QString wantPlugin = ShowControlListener::mscInputPlugin();
    const QString want = ShowControlListener::mscInputName();
    QStringList names = iom->pluginInputs(QStringLiteral("MIDI"));
    for (const QString &n : patchedMidiInputs)
        if (!names.contains(n))
            names << n;
    m_mscInputCombo->clear();
    m_mscInputCombo->addItem(tr("All MIDI inputs"), QStringList());
    for (const QString &n : names)
        m_mscInputCombo->addItem(n, QStringList{ QStringLiteral("MIDI"), n });
    if (!want.isEmpty())
    {
        int at = m_mscInputCombo->findData(QStringList{ wantPlugin, want });
        if (at < 0)
        {
            m_mscInputCombo->addItem(tr("%1 (not on this computer now)").arg(want),
                                     QStringList{ wantPlugin, want });
            at = m_mscInputCombo->count() - 1;
        }
        m_mscInputCombo->setCurrentIndex(at);
    }
    else
    {
        m_mscInputCombo->setCurrentIndex(0);
    }

    QStringList where;
    for (quint32 uni : m_doc->showControl()->mscUniverses())
        for (quint32 i = 0; i < iom->universesCount(); i++)
            if (iom->getUniverseID(i) == uni)
                where << universeName(iom, i);

    m_mscInputs->setProperty("mscInputPatched", want.isEmpty() || !where.isEmpty());
    if (want.isEmpty() && midi.isEmpty())
    {
        m_mscInputs->setText(tr("No MIDI input is patched, so neither MSC nor timecode can arrive."));
        m_mscInputs->setStyleSheet(kAmber);
    }
    else if (want.isEmpty())
    {
        m_mscInputs->setText(tr("Every patched MIDI input: %1").arg(midi.join(QStringLiteral("; "))));
        m_mscInputs->setStyleSheet(QString());
    }
    else if (where.isEmpty())
    {
        // Said plainly, and no quiet fall-back to every input.
        m_mscInputs->setText(tr("%1 isn't patched in this show, so MSC isn't answered. Patch it "
                                "as the input of a universe, or listen on all MIDI inputs.")
                             .arg(want));
        m_mscInputs->setStyleSheet(kRed);
    }
    else
    {
        m_mscInputs->setText(tr("Only %1, patched to %2.").arg(want, where.join(QStringLiteral(", "))));
        m_mscInputs->setStyleSheet(QString());
    }
    refreshTimecode();
}

void ShowControlSyncPage::slotMtcSourceActivated(int index)
{
    TimecodeSource *tc = m_doc->timecodeSource();
    const qint32 uni = m_mtcSourceCombo->itemData(index).toInt();
    if (uni == tc->sourceUniverse())
        return;
    tc->setSourceUniverse(uni);
    m_doc->setModified();       // saved with the show
}

void ShowControlSyncPage::slotMscInputActivated(int index)
{
    if (!m_listener)
        return;
    const QStringList id = m_mscInputCombo->itemData(index).toStringList();
    if (id.size() == 2)
        m_listener->setMscInput(id.at(0), id.at(1));
    else
        m_listener->setMscInput(QString(), QString());
}

void ShowControlSyncPage::slotTimeChanged()
{
    // Code arrives up to ~120 times a second; the readout doesn't need that.
    if (m_mtcThrottle.isValid() && m_mtcThrottle.elapsed() < 200)
        return;
    m_mtcThrottle.start();
    // A universe heard for the first time joins the source list.
    if (m_mtcSourceCombo->findData(m_doc->timecodeSource()->lastUniverse()) < 0)
        refreshMidiInputs();
    else
        refreshTimecode();
}

void ShowControlSyncPage::refreshTimecode()
{
    TimecodeSource *tc = m_doc->timecodeSource();
    const TimecodeSource::Health health = tc->health();
    m_mtcHealth->setProperty("health", int(health));

    if (health == TimecodeSource::NoSource)
    {
        m_mtcStatus->setText(tc->sourceUniverse() >= 0 ? tr("Waiting for the source to roll…")
                                                       : tr("None received yet"));
        m_mtcStatus->setStyleSheet(kGrey);
        m_mtcHealth->setText(tr("—"));
        m_mtcHealth->setStyleSheet(kGrey);
        checkSummary();
        return;
    }

    QString from;
    InputOutputMap *iom = m_doc->inputOutputMap();
    for (quint32 i = 0; i < iom->universesCount(); i++)
        if (qint32(iom->getUniverseID(i)) == tc->lastUniverse())
            from = universeName(iom, i);

    const QString code = timecodeText(tc->positionMs(), tc->fps());
    if (tc->isRunning())
    {
        m_mtcStatus->setText(tr("● %1 at %2 fps, from %3").arg(code).arg(tc->fps()).arg(from));
        m_mtcStatus->setStyleSheet(kGreen);
    }
    else
    {
        m_mtcStatus->setText(tr("❚❚ %1 (holding), from %2").arg(code, from));
        m_mtcStatus->setStyleSheet(kAmber);
    }

    QString verdict;
    const char *style = kGrey;
    switch (health)
    {
        case TimecodeSource::Healthy:  verdict = tr("✓ healthy"); style = kGreen; break;
        case TimecodeSource::Usable:   verdict = tr("~ usable"); style = kAmber; break;
        case TimecodeSource::Unstable: verdict = tr("⚠ unstable"); style = kRed; break;
        default:                       verdict = tr("held (no fresh code)"); style = kAmber; break;
    }
    if (tc->isRunning())
        verdict += tr(" — %1× real time · jitter %2 ms · ~%3 ms/update")
                       .arg(tc->rateEstimate(), 0, 'f', 2)
                       .arg(qRound(tc->jitterMs()))
                       .arg(qRound(tc->avgIntervalMs()));
    m_mtcHealth->setText(verdict);
    m_mtcHealth->setStyleSheet(style);
    checkSummary();
}

void ShowControlSyncPage::showSection(const QString &key)
{
    for (auto it = m_groups.cbegin(); it != m_groups.cend(); ++it)
        it.value()->setVisible(key.isEmpty() || it.key() == key
                               || (it.key() == QLatin1String("out")
                                   && (key == QLatin1String("osc") || key == QLatin1String("msc"))));
}

QList<QStringList> ShowControlSyncPage::summary() const
{
    QString osc = tr("Off");
    if (ShowControlListener::oscEnabled())
        osc = m_listener ? tr("Port %1 · %2").arg(ShowControlListener::oscPort()).arg(m_listener->status())
                         : tr("Port %1").arg(ShowControlListener::oscPort());

    QString msc = tr("Off");
    if (ShowControlListener::mscEnabled())
    {
        const QString want = ShowControlListener::mscInputName();
        msc = tr("Device %1 · %2").arg(ShowControlListener::mscDeviceId())
                  .arg(want.isEmpty() ? tr("all MIDI inputs") : want);
        if (!m_mscInputs->property("mscInputPatched").toBool())
            msc += tr(" (not patched)");
    }

    const ShowWebAccess *webAccess = m_doc->webAccess();
    QString web = tr("Off");
    switch (webAccess->state())
    {
        case ShowWebAccess::Running:
        case ShowWebAccess::CommandLine:
            web = tr("Port %1 · %2").arg(webUrlPort(webAccess->url(), webAccess->port()))
                      .arg(webUrlIsLocal(webAccess->url()) ? tr("this computer only") : tr("network"));
            if (webAccess->state() == ShowWebAccess::CommandLine)
                web += tr(" · from -w");
            break;
        case ShowWebAccess::Failed:
            web = tr("Port %1 · could not start").arg(webAccess->port());
            break;
        default:
            if (webAccess->isEnabled())
                web = tr("Port %1").arg(webAccess->port());
            break;
    }

    return { { QStringLiteral("osc"), tr("OSC in"), osc },
             { QStringLiteral("msc"), tr("MIDI Show Control in"), msc },
             { QStringLiteral("mtc"), tr("MIDI timecode"), m_mtcStatus->text() },
             { QStringLiteral("web"), tr("Web access"), web },
             { QStringLiteral("map"), tr("Control Map"), m_controlMap->summary() } };
}

void ShowControlSyncPage::checkSummary()
{
    const QList<QStringList> now = summary();
    if (now == m_lastSummary)
        return;
    m_lastSummary = now;
    emit summaryChanged();
}

void ShowControlSyncPage::showEvent(QShowEvent *ev)
{
    // Inputs are patched on the other Connections pages; catch up on them.
    refreshMidiInputs();
    QWidget::showEvent(ev);
}
