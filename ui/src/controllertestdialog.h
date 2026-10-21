/*
  Q Light Controller Plus
  controllertestdialog.h

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

#ifndef CONTROLLERTESTDIALOG_H
#define CONTROLLERTESTDIALOG_H

#include <QDialog>
#include <QPointer>
#include <QHash>
#include <QList>

class FocusControlMap;
class QTreeWidgetItem;
class QLCIOPlugin;
class QTreeWidget;
class QPushButton;
class QLabel;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * Controller test: every raw input a game controller sends, as it arrives --
 * the device, how it is connected and read, the element (its raw number or
 * name), the value, and the channel, gamepad role, action and Focus mode
 * control it feeds. Press everything once, then Copy or Save the log: it is
 * what is needed to check (or fix) a controller's mapping without the
 * controller in hand.
 *
 * It reads through QLCIOPlugin's diagnostics (setInputDiagnostics()), from
 * every controller a plugin can see, patched or not. One window at a time.
 */
class ControllerTestDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ControllerTestDialog(Doc *doc, QWidget *parent = nullptr);
    ~ControllerTestDialog() override;

    /** Show the window, made if need be, and bring it to the front. */
    static ControllerTestDialog *open(Doc *doc, QWidget *parent);
    static ControllerTestDialog *instance() { return s_instance.data(); }

    /** The log, as Copy and Save give it. */
    QString logText() const;
    /** Inputs listed (one row each). */
    int rowCount() const;
    /** Plugins reporting to it. */
    int pluginCount() const { return m_plugins.size(); }

private:
    void addInput(QLCIOPlugin *plugin, quint32 input, const QString &device,
                  const QString &connection, const QString &element, int raw,
                  int channel, int value);
    /** What @p channel of @p plugin's @p input is for: its name, gamepad
     *  role, action and what it does in Focus mode. */
    QString mapsTo(QLCIOPlugin *plugin, quint32 input, int channel) const;
    void refreshStatus();
    void clear();
    void copyLog();
    void saveLog();

private:
    Doc *m_doc;
    QList<QPointer<QLCIOPlugin>> m_plugins;
    QLabel *m_statusLabel;
    QLabel *m_devicesLabel;
    QPushButton *m_askButton;
    QPushButton *m_settingsButton;
    QTreeWidget *m_tree;
    FocusControlMap *m_actions;       ///< never shown: what a control does in Focus mode
    QHash<QString, QTreeWidgetItem *> m_rows;
    QHash<QString, QString> m_connections;   ///< device -> how it is connected
    QStringList m_events;             ///< in order, for the log

    static QPointer<ControllerTestDialog> s_instance;
};

/** @} */

#endif
