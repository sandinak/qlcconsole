/*
  Q Light Controller Plus - qlcconsole
  connectioneditor.h

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

#ifndef CONNECTIONEDITOR_H
#define CONNECTIONEDITOR_H

#include <QDialog>
#include <QList>
#include <QString>

#include "universepatcher.h"

class QLCIOPlugin;
class QStackedWidget;
class QPushButton;
class QGroupBox;
class QComboBox;
class QLineEdit;
class QCheckBox;
class QSpinBox;
class QLabel;
class Doc;

/**
 * One window for a universe's connection: the universe, the protocol, the
 * interface, and below them that protocol's own settings -- Art-Net's node
 * and port address and transmit mode, sACN's universe, priority and
 * multicast or unicast, MIDI's output mode, a USB widget's device -- with
 * the universe's input, profile, feedback and passthrough under that.
 *
 * It stands in for the cascade the Connections tab used to ask (protocol?
 * interface? port? each its own little dialog) and for the dozen editing
 * entries on a universe's right-click: patching a universe and editing a
 * patched one open the same window, the second filled in.
 *
 * Nothing changes until OK, and then as ONE step of Undo Patch Change. A
 * plugin's own settings window (Configure…) and WLED's devices are still
 * their own windows, opened from here.
 *
 * Only the qlcconsole views use it; Classic QLC+'s Universes page keeps
 * QLC+'s own patch editor.
 */
class ConnectionEditor final : public QDialog
{
    Q_OBJECT

public:
    /** What the lower part shows for a protocol. */
    enum Kind
    {
        KindNone,       ///< not patched
        KindTargets,    ///< sends to a node and port (Art-Net)
        KindSacn,       ///< E1.31: universe, priority, multicast/unicast
        KindWled,       ///< WLED: its devices, in their own window
        KindMidi,       ///< MIDI: the output mode
        KindDevice      ///< the interface IS the device (USB widgets, ...)
    };

    /** Edit @p universe's output leg on @p pluginName / @p line, or, with no
     *  plugin, its first output leg (none: a new patch). */
    ConnectionEditor(Doc *doc, quint32 universe, const QString &pluginName = QString(),
                     quint32 line = 0, QWidget *parent = nullptr);
    ~ConnectionEditor();

    /** Open it over @p parent and make the change on OK. @p section: "input",
     *  "feedback" or "passthrough" to open with that part in view. True if
     *  anything changed. */
    static bool edit(Doc *doc, quint32 universe, const QString &pluginName, quint32 line,
                     QWidget *parent, const QString &section = QString());

    /** The lower part a plugin gets. */
    static Kind kindOf(const QLCIOPlugin *plugin);

    /** Make the change the form describes, as one undo step. False when it is
     *  refused (error() says why); nothing has changed then. */
    bool apply();
    QString error() const { return m_error; }

    /** Bring "input", "feedback" or "passthrough" into view and give it
     *  the focus. */
    void focusSection(const QString &section);

    /** The universe being edited. */
    quint32 universe() const { return m_universe; }

public slots:
    void accept() override;

private slots:
    void slotUniverseChanged();
    void slotProtocolChanged();
    void slotInterfaceChanged();
    void slotTargetChanged();
    void slotTargetTyped();
    void slotSacnMulticastToggled();
    void slotInputChanged();
    void slotFeedbackChanged();
    void slotConfigurePlugin();
    void slotWledDevices();

private:
    void buildForm();
    /** Fill everything from the universe as it is now. */
    void load();
    /** The plugin picked in the protocol list, or nullptr. */
    QLCIOPlugin *pickedPlugin() const;
    QString pickedPluginName() const;
    quint32 pickedLine() const;
    /** The lines of @p p as the interface list reads them (alias first). */
    QStringList lineLabels(QLCIOPlugin *p, bool output) const;
    /** Fill the lower part for the plugin and line picked. */
    void loadSettings();
    void loadTargets();
    /** The output leg being edited, or nullptr (none / pending). */
    class OutputPatch *editedPatch() const;
    QVariant origParam(const QString &key) const;
    void showSettingsFor(Kind kind);
    /** Refuse what can't be done; empty when it can. */
    QString validate(QString &address, quint32 &portAddress,
                     QString &fbAddress, quint32 &fbPort) const;

private:
    Doc *m_doc;
    UniversePatcher *m_patcher;
    quint32 m_universe;
    /** The leg as it was when opened: its plugin and line, its index among
     *  the universe's output patches (-1: none). */
    QString m_origPlugin;
    quint32 m_origLine = 0;
    int m_origIndex = -1;
    bool m_loading = false;
    QString m_error;

    QComboBox *m_universeCombo = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QComboBox *m_protocolCombo = nullptr;
    QComboBox *m_interfaceCombo = nullptr;
    QLabel *m_pendingNote = nullptr;
    QLabel *m_otherLegs = nullptr;

    QGroupBox *m_settingsBox = nullptr;
    QStackedWidget *m_settings = nullptr;
    QPushButton *m_configureButton = nullptr;

    // Art-Net (a node and port)
    QComboBox *m_targetCombo = nullptr;
    QLineEdit *m_targetIP = nullptr;
    QLineEdit *m_targetPort = nullptr;
    QComboBox *m_transmitMode = nullptr;
    QList<UniversePatcher::TargetPortChoice> m_ports;

    // sACN
    QCheckBox *m_sacnMulticast = nullptr;
    QLineEdit *m_sacnIP = nullptr;
    QSpinBox *m_sacnUniverse = nullptr;
    QSpinBox *m_sacnPriority = nullptr;
    QComboBox *m_sacnMode = nullptr;

    // MIDI
    QComboBox *m_midiMode = nullptr;

    // WLED
    QPushButton *m_wledButton = nullptr;
    QLabel *m_wledNote = nullptr;

    // A device (USB, ...)
    QLabel *m_deviceNote = nullptr;

    // The universe: input, profile, feedback, passthrough
    QGroupBox *m_inputBox = nullptr;
    QComboBox *m_inputCombo = nullptr;
    QComboBox *m_profileCombo = nullptr;
    QComboBox *m_midiChannel = nullptr;
    QSpinBox *m_artnetInUni = nullptr;
    QComboBox *m_feedbackCombo = nullptr;
    QLineEdit *m_feedbackIP = nullptr;
    QLineEdit *m_feedbackPort = nullptr;
    QCheckBox *m_passthrough = nullptr;
};

#endif
