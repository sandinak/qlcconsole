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
#include <QTime>
#include <QDir>
#include <QFile>
#include <QUrl>

#include "controllertestdialog.h"
#include "programmercontroller.h"
#include "controllerkeepalive.h"
#include "focuscontrolmap.h"
#include "ioplugincache.h"
#include "qlcioplugin.h"
#include "gamepadrole.h"
#include "qlcconfig.h"
#include "doc.h"

QPointer<ControllerTestDialog> ControllerTestDialog::s_instance;

namespace
{
enum Column { ColDevice, ColConnection, ColElement, ColRaw, ColRange, ColChannel, ColValue,
              ColMapsTo, ColSeen, ColCount };

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

    m_actions = new FocusControlMap(this);
    m_actions->hide();

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
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
            connect(plugin, &QLCIOPlugin::inputDiagnostic, this,
                    [this, plugin](quint32 input, const QString &device, const QString &connection,
                                   const QString &element, int raw, int channel, int value) {
                addInput(plugin, input, device, connection, element, raw, channel, value);
            });
            connect(plugin, &QLCIOPlugin::configurationChanged, this,
                    &ControllerTestDialog::refreshStatus);
        }
    }
    connect(m_doc->programmer()->controllerKeepAlive(), &ControllerKeepAlive::heldChanged,
            this, &ControllerTestDialog::refreshStatus);
    refreshStatus();
}

ControllerTestDialog::~ControllerTestDialog()
{
    for (const QPointer<QLCIOPlugin> &plugin : std::as_const(m_plugins))
        if (!plugin.isNull())
            plugin->setInputDiagnostics(false);
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
        m_devicesLabel->setText(tr("No game controller is connected. Switch one on (hold the "
                                   "Xbox button) or plug it in; it is listed here when it "
                                   "arrives."));
    else
        m_devicesLabel->setText(devices.join(QStringLiteral("<br/>")));

    ControllerKeepAlive *keep = m_doc->programmer()->controllerKeepAlive();
    QStringList status;
    status << (keep->isHeld() ? tr("Kept awake for the controller (no App Nap).")
                              : tr("Not being kept awake: Focus mode and the follow spot are off."));
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
    const QString focus = role != GamepadRole::None ? m_actions->actionText(int(role)) : QString();
    if (!focus.isEmpty())
        parts << tr("Focus: %1").arg(focus);
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
        item->setText(ColElement, element);
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
