/*
  Q Light Controller Plus
  controllertestdialog.cpp

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
#include <QDesktopServices>
#include <QTreeWidgetItem>
#include <QStandardPaths>
#include <QApplication>
#include <QTreeWidget>
#include <QListWidget>
#include <QComboBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextStream>
#include <QClipboard>
#include <QDateTime>
#include <QSysInfo>
#include <QLabel>
#include <QTimer>
#include <QTime>
#include <QDir>
#include <QFile>
#include <QUrl>

#include "controllertestdialog.h"
#include "programmercontroller.h"
#include "controllerkeepalive.h"
#include "focuscontrolmap.h"
#include "padrun.h"
#include "ioplugincache.h"
#include "qlcioplugin.h"
#include "gamepadrole.h"
#include "inputdevicenotices.h"
#include "qlcconfig.h"
#include "doc.h"

QPointer<ControllerTestDialog> ControllerTestDialog::s_instance;

namespace
{
enum Column { ColDevice, ColConnection, ColElement, ColRaw, ColRange, ColChannel, ColValue,
              ColMapsTo, ColSeen, ColCount };
enum StickColumn { StickDevice, StickAxis, StickReads, StickRest, StickDeadzone, StickOutput,
                   StickColCount };

// A 0..255 stick reading as -1..1 (128 the middle), signed: "+0.12".
QString stickText(double norm)
{
    return QString::asprintf("%+.2f", norm);
}

double stickNorm(int value)
{
    return value >= 128 ? (value - 128) / 127.0 : (value - 128) / 128.0;
}

const int kMaxEvents = 5000;
}

ControllerTestDialog *ControllerTestDialog::open(Doc *doc, QWidget *parent)
{
    if (s_instance.isNull())
    {
        s_instance = new ControllerTestDialog(doc, parent);
        /* From a modal dialog (Focus mode's setup, Settings) the window would
           be shut out by it: be modal over it instead -- test, close, carry
           on. */
        if (QApplication::activeModalWidget() != nullptr)
            s_instance->setWindowModality(Qt::ApplicationModal);
    }
    s_instance->show();
    s_instance->raise();
    s_instance->activateWindow();
    return s_instance.data();
}

ControllerTestDialog::ControllerTestDialog(Doc *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName(QStringLiteral("controllerTest"));
    setWindowTitle(tr("Controller Test"));
    setProperty("helpTopic", QStringLiteral("lighting-studio.md#controller-test"));
    resize(980, 560);

    QVBoxLayout *vl = new QVBoxLayout(this);
    QLabel *intro = new QLabel(tr("Press every button once and move each stick and trigger all "
                                  "the way round. Every input the controller sends is listed as "
                                  "it arrives \342\200\224 what it is, its raw value, and what it "
                                  "does here. Then Copy or Save the log."), this);
    intro->setWordWrap(true);
    vl->addWidget(intro);

    m_devicesLabel = new QLabel(this);
    m_devicesLabel->setObjectName(QStringLiteral("controllerTestDevices"));
    m_devicesLabel->setWordWrap(true);
    m_devicesLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    vl->addWidget(m_devicesLabel);

    QHBoxLayout *status = new QHBoxLayout;
    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("controllerTestStatus"));
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("color: palette(mid);"));
    status->addWidget(m_statusLabel, 1);
    m_askButton = new QPushButton(tr("Ask macOS for Input Monitoring"), this);
    m_askButton->setObjectName(QStringLiteral("controllerTestAsk"));
    status->addWidget(m_askButton);
    m_settingsButton = new QPushButton(tr("Open Privacy Settings"), this);
    m_settingsButton->setObjectName(QStringLiteral("controllerTestPrivacy"));
    status->addWidget(m_settingsButton);
    vl->addLayout(status);
    connect(m_askButton, &QPushButton::clicked, this, [this]() {
        m_doc->programmer()->controllerKeepAlive()->requestInputMonitoring();
        refreshStatus();
    });
    connect(m_settingsButton, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral(
            "x-apple.systempreferences:com.apple.preference.security?Privacy_ListenEvent")));
    });

    /* Check Mapping: shown once a walk is started. */
    m_checkPanel = new QWidget(this);
    m_checkPanel->setObjectName(QStringLiteral("controllerCheckPanel"));
    QVBoxLayout *cl = new QVBoxLayout(m_checkPanel);
    cl->setContentsMargins(0, 0, 0, 0);
    m_checkDevice = new QComboBox(m_checkPanel);
    m_checkDevice->setObjectName(QStringLiteral("controllerCheckDevice"));
    cl->addWidget(m_checkDevice);
    m_checkPrompt = new QLabel(m_checkPanel);
    m_checkPrompt->setObjectName(QStringLiteral("controllerCheckPrompt"));
    m_checkPrompt->setWordWrap(true);
    QFont big = m_checkPrompt->font();
    big.setPointSizeF(big.pointSizeF() * 1.3);
    big.setBold(true);
    m_checkPrompt->setFont(big);
    cl->addWidget(m_checkPrompt);
    m_checkList = new QListWidget(m_checkPanel);
    m_checkList->setObjectName(QStringLiteral("controllerCheckList"));
    m_checkList->setFlow(QListView::TopToBottom);
    m_checkList->setWrapping(true);
    m_checkList->setResizeMode(QListView::Adjust);
    m_checkList->setSelectionMode(QAbstractItemView::NoSelection);
    m_checkList->setFocusPolicy(Qt::NoFocus);
    m_checkList->setUniformItemSizes(true);
    m_checkList->setFixedHeight(m_checkList->fontMetrics().height() * 6 + 14);
    cl->addWidget(m_checkList);
    QHBoxLayout *cb = new QHBoxLayout;
    m_checkSummary = new QLabel(m_checkPanel);
    m_checkSummary->setObjectName(QStringLiteral("controllerCheckSummary"));
    m_checkSummary->setWordWrap(true);
    cb->addWidget(m_checkSummary, 1);
    m_checkRetry = new QPushButton(tr("Retry"), m_checkPanel);
    m_checkRetry->setObjectName(QStringLiteral("controllerCheckRetry"));
    m_checkRetry->setToolTip(tr("Ask for this control again"));
    m_checkSkip = new QPushButton(tr("Skip"), m_checkPanel);
    m_checkSkip->setObjectName(QStringLiteral("controllerCheckSkip"));
    m_checkSkip->setToolTip(tr("This controller has no such control, or leave it for now"));
    m_checkStop = new QPushButton(tr("Stop"), m_checkPanel);
    m_checkStop->setObjectName(QStringLiteral("controllerCheckStop"));
    cb->addWidget(m_checkRetry);
    cb->addWidget(m_checkSkip);
    cb->addWidget(m_checkStop);
    cl->addLayout(cb);
    m_checkPanel->hide();
    vl->addWidget(m_checkPanel);
    connect(m_checkDevice, QOverload<int>::of(&QComboBox::activated), this,
            [this](int i) { beginCheck(i); });
    connect(m_checkRetry, &QPushButton::clicked, this, [this]() {
        if (m_check) { m_check->retry(); refreshCheck(); }
    });
    connect(m_checkSkip, &QPushButton::clicked, this, [this]() {
        if (m_check) { m_check->skip(); refreshCheck(); }
    });
    connect(m_checkStop, &QPushButton::clicked, this, [this]() {
        if (m_check && !m_check->finished())
        {
            m_check->stop();
            refreshCheck();
        }
        else
            m_checkPanel->hide();
    });

    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("controllerTestInputs"));
    m_tree->setRootIsDecorated(false);
    m_tree->setUniformRowHeights(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->setColumnCount(ColCount);
    m_tree->setHeaderLabels({ tr("Device"), tr("Connection"), tr("Element"), tr("Raw"),
                              tr("Raw seen"), tr("Channel"), tr("Value"), tr("Maps to"),
                              tr("Seen") });
    m_tree->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_tree->setColumnWidth(ColDevice, 150);
    m_tree->setColumnWidth(ColConnection, 170);
    m_tree->setColumnWidth(ColElement, 190);
    m_tree->setColumnWidth(ColRaw, 60);
    m_tree->setColumnWidth(ColRange, 110);
    m_tree->setColumnWidth(ColChannel, 60);
    m_tree->setColumnWidth(ColValue, 50);
    m_tree->setColumnWidth(ColMapsTo, 260);
    vl->addWidget(m_tree, 1);

    /* The sticks: where each rests (a worn stick does not come back to the
       middle) and what every tool gets once that is taken out. */
    QHBoxLayout *sticksHead = new QHBoxLayout;
    m_sticksNote = new QLabel(this);
    m_sticksNote->setObjectName(QStringLiteral("controllerTestSticksNote"));
    m_sticksNote->setWordWrap(true);
    m_sticksNote->setText(tr("Sticks: where each one rests is taken out of what it reads, so a "
                             "worn stick left alone moves nothing. Let go of the sticks and "
                             "press Centre Sticks (or hold View and click the left stick: "
                             "Create + L3 on a PlayStation pad) to learn it again."));
    sticksHead->addWidget(m_sticksNote, 1);
    m_centreButton = new QPushButton(tr("Centre Sticks"), this);
    m_centreButton->setObjectName(QStringLiteral("controllerTestCentre"));
    m_centreButton->setToolTip(tr("Learn where every controller's sticks rest, once they are let go"));
    sticksHead->addWidget(m_centreButton, 0, Qt::AlignTop);
    vl->addLayout(sticksHead);
    connect(m_centreButton, &QPushButton::clicked, this, [this]() { centreSticks(); });

    m_sticks = new QTreeWidget(this);
    m_sticks->setObjectName(QStringLiteral("controllerTestSticks"));
    m_sticks->setRootIsDecorated(false);
    m_sticks->setUniformRowHeights(true);
    m_sticks->setColumnCount(StickColCount);
    m_sticks->setHeaderLabels({ tr("Controller"), tr("Stick"), tr("Reads"), tr("Rests at"),
                                tr("Deadzone"), tr("Tools get") });
    // Names are read whole (two pads of a kind differ only in the address).
    m_sticks->header()->setSectionResizeMode(StickDevice, QHeaderView::ResizeToContents);
    m_sticks->header()->setSectionResizeMode(StickAxis, QHeaderView::ResizeToContents);
    m_sticks->setMaximumHeight(m_sticks->fontMetrics().height() * 8);
    vl->addWidget(m_sticks);
    m_sticksTimer = new QTimer(this);
    m_sticksTimer->setInterval(100);
    connect(m_sticksTimer, &QTimer::timeout, this, &ControllerTestDialog::refreshSticks);
    m_sticksTimer->start();

    m_actions = new FocusControlMap(this);
    m_actions->hide();

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_checkButton = bb->addButton(tr("Check Mapping"), QDialogButtonBox::ActionRole);
    m_checkButton->setObjectName(QStringLiteral("controllerTestCheck"));
    connect(m_checkButton, &QPushButton::clicked, this, [this]() { startCheck(); });
    QPushButton *clearBtn = bb->addButton(tr("Clear"), QDialogButtonBox::ResetRole);
    clearBtn->setObjectName(QStringLiteral("controllerTestClear"));
    QPushButton *copyBtn = bb->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
    copyBtn->setObjectName(QStringLiteral("controllerTestCopy"));
    QPushButton *saveBtn = bb->addButton(tr("Save\342\200\246"), QDialogButtonBox::ActionRole);
    saveBtn->setObjectName(QStringLiteral("controllerTestSave"));
    connect(clearBtn, &QPushButton::clicked, this, &ControllerTestDialog::clear);
    connect(copyBtn, &QPushButton::clicked, this, &ControllerTestDialog::copyLog);
    connect(saveBtn, &QPushButton::clicked, this, &ControllerTestDialog::saveLog);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vl->addWidget(bb);

    /* Every plugin that can report raw input does, for as long as this is
       open -- and runs its controllers to do it, patched or not. */
    if (m_doc->ioPluginCache() != nullptr)
    {
        for (QLCIOPlugin *plugin : m_doc->ioPluginCache()->plugins())
        {
            if (!plugin->setInputDiagnostics(true))
                continue;
            m_plugins.append(plugin);
            /* By name, not &QLCIOPlugin::signal: every plugin DLL builds its
               own copy of QLCIOPlugin, so on Windows that address is never
               the one the plugin's moc compares with, and nothing connects. */
            connect(plugin, SIGNAL(inputDiagnostic(quint32,QString,QString,QString,int,int,int)),
                    this, SLOT(slotInputDiagnostic(quint32,QString,QString,QString,int,int,int)));
            connect(plugin, SIGNAL(configurationChanged()), this, SLOT(refreshStatus()));
            connect(plugin, SIGNAL(inputSticksCentred(quint32,int)),
                    this, SLOT(slotSticksCentred(quint32,int)));
        }
    }
    connect(m_doc->programmer()->controllerKeepAlive(), &ControllerKeepAlive::heldChanged,
            this, &ControllerTestDialog::refreshStatus);
    refreshStatus();
    refreshSticks();
}

ControllerTestDialog::~ControllerTestDialog()
{
    for (const QPointer<QLCIOPlugin> &plugin : std::as_const(m_plugins))
        if (!plugin.isNull())
            plugin->setInputDiagnostics(false);
}

void ControllerTestDialog::slotInputDiagnostic(quint32 input, const QString &device,
                                               const QString &connection, const QString &element,
                                               int raw, int channel, int value)
{
    for (const QPointer<QLCIOPlugin> &plugin : std::as_const(m_plugins))
    {
        if (!plugin.isNull() && static_cast<QObject *>(plugin.data()) == sender())
        {
            addInput(plugin.data(), input, device, connection, element, raw, channel, value);
            feedCheck(plugin.data(), input, channel, value);
            return;
        }
    }
}

void ControllerTestDialog::refreshStatus()
{
    // The controllers, and how each is connected and read.
    QStringList devices;
    for (const QPointer<QLCIOPlugin> &plugin : std::as_const(m_plugins))
    {
        if (plugin.isNull())
            continue;
        const QStringList inputs = plugin->inputs();
        for (int i = 0; i < inputs.size(); ++i)
        {
            const QString info = plugin->inputConnectionInfo(quint32(i));
            if (info.isEmpty())
                continue;
            QString line = QStringLiteral("<b>%1</b> \u2014 %2").arg(inputs.at(i).toHtmlEscaped(),
                                                                         info.toHtmlEscaped());
            const QString problem = plugin->inputProblem(quint32(i));
            if (!problem.isEmpty())
                line += QStringLiteral("<br/><span style=\"color:#d08a1c\">%1</span>")
                        .arg(problem.toHtmlEscaped());
            devices << line;
            m_connections.insert(inputs.at(i), info);
        }
    }
    if (m_plugins.isEmpty())
        m_devicesLabel->setText(tr("No input plugin here can report raw controller input."));
    else if (devices.isEmpty())
        m_devicesLabel->setText(tr("No game controller is connected. Switch one on (hold its "
                                   "Xbox or PS button) or plug it in; it is listed here when it "
                                   "arrives."));
    else
        m_devicesLabel->setText(devices.join(QStringLiteral("<br/>")));

    ControllerKeepAlive *keep = m_doc->programmer()->controllerKeepAlive();
    QStringList status;
    status << (keep->isHeld() ? tr("Kept awake for the controller (no App Nap).")
                              : tr("Not being kept awake: Quick focus, Venue focus and the follow spot are off."));
    const ControllerKeepAlive::Permission perm = keep->inputMonitoring();
    switch (perm)
    {
        case ControllerKeepAlive::PermissionGranted:
            status << tr("Input Monitoring: allowed.");
            break;
        case ControllerKeepAlive::PermissionDenied:
            status << tr("Input Monitoring: not allowed.");
            break;
        case ControllerKeepAlive::PermissionUnknown:
            status << tr("Input Monitoring: not asked yet.");
            break;
        case ControllerKeepAlive::PermissionNotNeeded:
            break;
    }
    const QString hint = keep->permissionHint();
    if (!hint.isEmpty())
        status << hint;
    m_statusLabel->setText(status.join(QLatin1Char(' ')));
    const bool anyPad = !controllers().isEmpty();
    m_checkButton->setEnabled(anyPad);
    m_checkButton->setToolTip(anyPad ? tr("Walk a controller through every control, one at a time, "
                                          "and check each is read as the control it is")
                                     : tr("No game controller is connected"));
    m_askButton->setVisible(perm == ControllerKeepAlive::PermissionUnknown);
    m_settingsButton->setVisible(perm == ControllerKeepAlive::PermissionDenied);
}

QString ControllerTestDialog::mapsTo(QLCIOPlugin *plugin, quint32 input, int channel) const
{
    if (channel < 0)
        return tr("nothing \342\200\224 not in the controller's layout");
    const QString name = plugin->inputChannelName(input, quint32(channel));
    const QString roleKey = plugin->inputChannelRole(input, quint32(channel));
    const QString action = plugin->inputButtonAction(input, quint32(channel));
    const GamepadRole::Role role = GamepadRole::fromProfile(roleKey, name);

    QStringList parts;
    parts << (name.isEmpty() ? tr("channel %1 (no profile name)").arg(channel) : name);
    if (role != GamepadRole::None)
        parts << tr("role %1").arg(GamepadRole::key(role));
    if (!action.isEmpty())
        parts << tr("action %1").arg(action);
    m_actions->setFamily(PadNames::familyFrom(plugin->inputDeviceKind(input)));
    const QString focus = role != GamepadRole::None ? m_actions->actionText(int(role)) : QString();
    if (!focus.isEmpty())
        parts << tr("Focus: %1").arg(focus);
    // The Run role on (0.4): what the control does while running the show.
    if (role != GamepadRole::None && m_doc->programmer()->padRun()->isEnabled())
    {
        const PadRun::Action run = PadRun::actionFor(role);
        if (run != PadRun::NoAction)
            parts << tr("Run: %1").arg(PadNames::expand(PadRun::label(run),
                                                         PadNames::familyFrom(plugin->inputDeviceKind(input))));
        // Walking the show (Q192): the walking map's own controls.
        const PadRun::WalkAction walk = PadRun::walkActionFor(role);
        if (walk != PadRun::WalkNoAction)
            parts << tr("Walk: %1").arg(PadNames::expand(PadRun::label(walk),
                                                          PadNames::familyFrom(plugin->inputDeviceKind(input))));
    }
    return parts.join(QStringLiteral(" \u00B7 "));
}

void ControllerTestDialog::addInput(QLCIOPlugin *plugin, quint32 input, const QString &device,
                                    const QString &connection, const QString &element, int raw,
                                    int channel, int value)
{
    if (!connection.isEmpty() && !m_connections.contains(device))
    {
        m_connections.insert(device, connection);
        refreshStatus();
    }

    const QString key = QStringLiteral("%1|%2|%3|%4").arg(plugin->name()).arg(input).arg(device, element);
    QTreeWidgetItem *item = m_rows.value(key);
    if (item == nullptr)
    {
        item = new QTreeWidgetItem(m_tree);
        item->setText(ColDevice, device);
        item->setText(ColConnection, connection);
        item->setToolTip(ColConnection, connection);
        item->setText(ColElement, elementText(plugin, input, element, channel));
        item->setData(ColRange, Qt::UserRole, raw);       // min
        item->setData(ColRange, Qt::UserRole + 1, raw);   // max
        item->setData(ColSeen, Qt::UserRole, 0);
        const QString maps = mapsTo(plugin, input, channel);
        item->setText(ColMapsTo, maps);
        item->setToolTip(ColMapsTo, maps);
        m_rows.insert(key, item);
        m_tree->scrollToItem(item);
    }
    const int lo = qMin(item->data(ColRange, Qt::UserRole).toInt(), raw);
    const int hi = qMax(item->data(ColRange, Qt::UserRole + 1).toInt(), raw);
    const int seen = item->data(ColSeen, Qt::UserRole).toInt() + 1;
    item->setData(ColRange, Qt::UserRole, lo);
    item->setData(ColRange, Qt::UserRole + 1, hi);
    item->setData(ColSeen, Qt::UserRole, seen);
    item->setText(ColRaw, QString::number(raw));
    item->setText(ColRange, QStringLiteral("%1 \u2026 %2").arg(lo).arg(hi));
    item->setText(ColChannel, channel >= 0 ? QString::number(channel) : QStringLiteral("\u2014"));
    item->setText(ColValue, value >= 0 ? QString::number(value) : QStringLiteral("\u2014"));
    item->setText(ColSeen, QString::number(seen));

    if (m_events.size() < kMaxEvents)
        m_events << QStringLiteral("%1  %2  %3 = %4  -> ch %5 = %6")
                    .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz")), device, element)
                    .arg(raw)
                    .arg(channel >= 0 ? QString::number(channel) : QStringLiteral("-"))
                    .arg(value >= 0 ? QString::number(value) : QStringLiteral("-"));
}

QString ControllerTestDialog::elementText(QLCIOPlugin *plugin, quint32 input, const QString &element,
                                          int channel) const
{
    /* The element as the system names it ("buttonA"), and as the pad in
       hand does (Q170): "buttonA, Cross" on a PlayStation pad. */
    if (channel < 0)
        return element;
    const QString kind = plugin->inputDeviceKind(input);
    if (kind.isEmpty())
        return element;
    const GamepadRole::Role role = GamepadRole::fromProfile(plugin->inputChannelRole(input, quint32(channel)),
                                                            plugin->inputChannelName(input, quint32(channel)));
    const QString own = PadNames::name(role, PadNames::familyFrom(kind));
    if (own.isEmpty() || own.compare(element, Qt::CaseInsensitive) == 0)
        return element;
    return QStringLiteral("%1 \u00B7 %2").arg(element, own);
}

int ControllerTestDialog::rowCount() const
{
    return m_tree->topLevelItemCount();
}

QString ControllerTestDialog::logText() const
{
    QString out;
    QTextStream ts(&out);
    ts << "qlcconsole controller test -- " << APPNAME << " " << APPVERSION << " -- "
       << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
#if defined(Q_OS_MACOS)
    ts << "macOS " << QSysInfo::productVersion() << " (" << QSysInfo::currentCpuArchitecture() << ")\n";
#else
    ts << QSysInfo::prettyProductName() << "\n";
#endif
    ts << "\nDevices:\n";
    for (auto it = m_connections.constBegin(); it != m_connections.constEnd(); ++it)
        ts << "  " << it.key() << " -- " << it.value() << "\n";
    ts << "\nStatus: " << m_statusLabel->text() << "\n";

    const QStringList reports = m_checkReports + (m_check && !m_checkArchived
                                                  ? QStringList(checkReport()) : QStringList());
    for (const QString &r : reports)
        ts << "\nMapping check:\n" << r << "\n";

    ts << "\nInputs (one row per element):\n";
    QStringList header;
    for (int c = 0; c < ColCount; ++c)
        header << m_tree->headerItem()->text(c);
    ts << header.join(QLatin1Char('\t')) << "\n";
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
    {
        const QTreeWidgetItem *item = m_tree->topLevelItem(i);
        QStringList row;
        for (int c = 0; c < ColCount; ++c)
            row << item->text(c);
        ts << row.join(QLatin1Char('\t')) << "\n";
    }

    ts << "\nEvents, in order";
    if (m_events.size() >= kMaxEvents)
        ts << " (the first " << kMaxEvents << ")";
    ts << ":\n";
    for (const QString &e : m_events)
        ts << e << "\n";
    return out;
}

void ControllerTestDialog::clear()
{
    m_tree->clear();
    m_rows.clear();
    m_events.clear();
    m_checkReports.clear();
}

void ControllerTestDialog::copyLog()
{
    QApplication::clipboard()->setText(logText());
}

void ControllerTestDialog::saveLog()
{
    const QString suggested = QDir(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation))
        .filePath(QStringLiteral("controller-test-%1.txt")
                  .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))));
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Controller Test"), suggested,
                                                      tr("Text files (*.txt)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, tr("Save Controller Test"),
                             tr("Could not write %1: %2").arg(path, file.errorString()));
        return;
    }
    file.write(logText().toUtf8());
}

/*****************************************************************************
 * Sticks
 *****************************************************************************/

void ControllerTestDialog::refreshSticks()
{
    int row = 0;
    for (const QPointer<QLCIOPlugin> &plugin : std::as_const(m_plugins))
    {
        if (plugin.isNull())
            continue;
        const QStringList inputs = plugin->inputs();
        const int count = inputs.size();
        for (int i = 0; i < count; ++i)
        {
            const QVariantList axes = plugin->inputStickAxes(quint32(i));
            // A controller listed once it has said anything.
            bool read = false;
            for (const QVariant &v : axes)
                read = read || v.toMap().value(QStringLiteral("raw")).toInt() >= 0;
            if (!read)
                continue;
            for (const QVariant &v : axes)
            {
                const QVariantMap m = v.toMap();
                QTreeWidgetItem *item = m_sticks->topLevelItem(row);
                if (item == nullptr)
                    item = new QTreeWidgetItem(m_sticks);
                ++row;
                const int ch = m.value(QStringLiteral("channel")).toInt();
                const int raw = m.value(QStringLiteral("raw")).toInt();
                QString axis = plugin->inputChannelName(quint32(i), quint32(ch));
                if (axis.isEmpty())
                    axis = tr("Axis %1").arg(ch + 1);
                // The tools' deadzone is of the 0..1 reading: twice that of -1..1 travel.
                const double dz = 2.0 * m.value(QStringLiteral("deadzone")).toDouble();
                const double out = stickNorm(m.value(QStringLiteral("output")).toInt());
                // Its whole name: two pads of a kind are told apart by their address.
                item->setText(StickDevice, inputs.at(i).trimmed());
                item->setText(StickAxis, axis);
                item->setText(StickReads, raw < 0 ? QStringLiteral("\u2014") : stickText(stickNorm(raw)));
                item->setText(StickRest, m.value(QStringLiteral("learnt")).toBool()
                                         ? stickText(m.value(QStringLiteral("rest")).toDouble())
                                         : tr("not learnt yet"));
                item->setText(StickDeadzone, QStringLiteral("\u00B1") + QString::number(dz, 'f', 2));
                item->setText(StickOutput, raw < 0 ? QStringLiteral("\u2014")
                                          : qAbs(out) <= dz ? tr("%1 (still)").arg(stickText(out))
                                                            : stickText(out));
            }
        }
    }
    while (m_sticks->topLevelItemCount() > row)
        delete m_sticks->takeTopLevelItem(row);
    m_centreButton->setEnabled(row > 0);
    m_centreButton->setToolTip(row > 0 ? tr("Learn where every controller's sticks rest, once they are let go")
                                       : tr("No controller has sent a stick reading yet: move a stick"));
    m_sticks->setVisible(row > 0);
}

int ControllerTestDialog::stickRowCount() const
{
    return m_sticks->topLevelItemCount();
}

int ControllerTestDialog::centreSticks()
{
    int asked = 0;
    QStringList names;
    for (const QPointer<QLCIOPlugin> &plugin : std::as_const(m_plugins))
    {
        if (plugin.isNull())
            continue;
        const int count = plugin->inputs().size();
        for (int i = 0; i < count; ++i)
        {
            // The controllers listed: those that have said anything.
            bool read = false;
            for (const QVariant &v : plugin->inputStickAxes(quint32(i)))
                read = read || v.toMap().value(QStringLiteral("raw")).toInt() >= 0;
            if (!read || !plugin->centreInputSticks(quint32(i)))
                continue;
            ++asked;
            names << InputDeviceNotices::controllerName(plugin.data(), quint32(i));
        }
    }
    if (asked == 0)
        m_sticksNote->setText(tr("No controller has sent a stick reading yet: move a stick, then "
                                 "let go and press Centre Sticks."));
    else
        m_sticksNote->setText(tr("Centring the sticks of %1: let go of them for a moment.")
                              .arg(names.join(QStringLiteral(", "))));
    return asked;
}

void ControllerTestDialog::slotSticksCentred(quint32 input, int held)
{
    QLCIOPlugin *plugin = qobject_cast<QLCIOPlugin *>(sender());
    const QString name = InputDeviceNotices::controllerName(plugin, input);
    if (held > 0)
        m_sticksNote->setText(tr("%1: %n stick(s) were held off the middle and left as they were. "
                                 "Let go of the sticks and press Centre Sticks again.", "", held).arg(name));
    else
        m_sticksNote->setText(tr("%1's sticks are centred: where they rest now reads as the middle.").arg(name));
    refreshSticks();
}

/*****************************************************************************
 * Check Mapping
 *****************************************************************************/

QList<ControllerTestDialog::Controller> ControllerTestDialog::controllers() const
{
    QList<Controller> out;
    for (const QPointer<QLCIOPlugin> &plugin : std::as_const(m_plugins))
    {
        if (plugin.isNull())
            continue;
        const QStringList inputs = plugin->inputs();
        for (int i = 0; i < inputs.size(); ++i)
        {
            const QString info = plugin->inputConnectionInfo(quint32(i));
            if (info.isEmpty())
                continue;
            out.append({ plugin, quint32(i), inputs.at(i).trimmed(), info });
        }
    }
    return out;
}

bool ControllerTestDialog::startCheck(const QString &pluginName, const QString &device)
{
    const QList<Controller> list = controllers();
    if (list.isEmpty())
    {
        m_checkPanel->show();
        m_checkDevice->hide();
        m_checkList->hide();
        m_checkRetry->hide();
        m_checkSkip->hide();
        m_checkPrompt->setText(tr("No game controller is connected. Switch one on or plug it in, "
                                  "then press Check Mapping."));
        m_checkSummary->clear();
        m_checkStop->setText(tr("Close"));
        m_check.reset();
        return false;
    }
    int pick = 0;
    for (int i = 0; i < list.size(); ++i)
        if (!device.isEmpty() && list.at(i).name == device.trimmed()
            && (pluginName.isEmpty() || list.at(i).plugin->name() == pluginName))
        {
            pick = i;
            break;
        }
    m_checkControllers = list;
    m_checkDevice->clear();
    for (const Controller &c : list)
        m_checkDevice->addItem(c.name);
    m_checkDevice->setCurrentIndex(pick);
    beginCheck(pick);
    return true;
}

void ControllerTestDialog::beginCheck(int index)
{
    archiveCheck();
    if (index < 0 || index >= m_checkControllers.size())
        return;
    m_checkController = m_checkControllers.at(index);
    if (m_checkController.plugin.isNull())
        return;
    m_check.reset(new MappingCheck(PadNames::familyFrom(
        m_checkController.plugin->inputDeviceKind(m_checkController.input))));
    m_checkArchived = false;
    m_checkDevice->setVisible(m_checkControllers.size() > 1);
    m_checkList->show();
    m_checkRetry->show();
    m_checkSkip->show();
    m_checkList->clear();
    for (int i = 0; i < m_check->count(); ++i)
        m_checkList->addItem(new QListWidgetItem(m_checkList));
    m_checkPanel->show();
    refreshCheck();
}

void ControllerTestDialog::archiveCheck()
{
    if (m_check && !m_checkArchived)
        m_checkReports << checkReport();
    m_checkArchived = true;
}

QString ControllerTestDialog::checkReport() const
{
    if (!m_check)
        return QString();
    const Controller &c = m_checkController;
    return QStringLiteral("  Device: %1\n  Connection: %2\n  Layout: %3 (%4)\n%5")
        .arg(c.name, c.info, PadNames::kindKey(m_check->family()),
             PadNames::isPlayStation(m_check->family()) ? QStringLiteral("PlayStation names")
                                                        : QStringLiteral("Xbox names"),
             m_check->report());
}

void ControllerTestDialog::refreshCheck()
{
    if (!m_check)
        return;
    for (int i = 0; i < m_check->count() && i < m_checkList->count(); ++i)
    {
        QListWidgetItem *item = m_checkList->item(i);
        item->setText(m_check->line(i));
        QColor colour;
        switch (m_check->state(i))
        {
            case MappingCheck::Passed: colour = QColor(0x2e, 0x9e, 0x4f); break;
            case MappingCheck::Wrong: colour = QColor(0xd0, 0x3a, 0x3a); break;
            default: break;
        }
        if (colour.isValid())
            item->setForeground(colour);
        else
            item->setData(Qt::ForegroundRole, QVariant());
        QFont f = item->font();
        f.setBold(i == m_check->current());
        item->setFont(f);
    }
    const int cur = m_check->current();
    m_checkSummary->setStyleSheet(QString());
    if (m_check->finished())
    {
        archiveCheck();
        m_checkPrompt->setStyleSheet(QString());
        m_checkPrompt->setText(tr("Done: %1").arg(m_check->summary()));
        m_checkSummary->setText(tr("It is in the log: Copy or Save it to send."));
        m_checkRetry->setEnabled(false);
        m_checkSkip->setEnabled(false);
        m_checkStop->setText(tr("Close"));
        return;
    }
    const bool wrong = m_check->state(cur) == MappingCheck::Wrong;
    m_checkPrompt->setStyleSheet(wrong ? QStringLiteral("color: #d03a3a;") : QString());
    m_checkPrompt->setText(wrong ? tr("%1. Try again, or Skip.").arg(m_check->mismatchText(cur))
                                 : tr("%1 of %2: %3").arg(cur + 1).arg(m_check->count()).arg(m_check->prompt()));
    m_checkSummary->setText(tr("%1 matched so far.").arg(m_check->passedCount()));
    m_checkRetry->setEnabled(wrong);
    m_checkSkip->setEnabled(true);
    m_checkStop->setText(tr("Stop"));
    m_checkList->scrollToItem(m_checkList->item(cur));
}

void ControllerTestDialog::feedCheck(QLCIOPlugin *plugin, quint32 input, int channel, int value)
{
    if (!m_check || m_check->finished() || channel < 0 || value < 0
        || plugin != m_checkController.plugin.data() || input != m_checkController.input)
        return;
    const GamepadRole::Role role = GamepadRole::fromProfile(
        plugin->inputChannelRole(input, quint32(channel)),
        plugin->inputChannelName(input, quint32(channel)));
    if (m_check->feed(role, value) != MappingCheck::Ignored)
        refreshCheck();
}
