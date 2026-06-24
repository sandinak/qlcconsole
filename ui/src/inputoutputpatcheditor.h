/*
  Q Light Controller
  inputoutputpatcheditor.h

  Copyright (C) Massimo Callegari

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

#ifndef INPUTOUTPUTPATCHEDITOR_H
#define INPUTOUTPUTPATCHEDITOR_H

#include <QWidget>

#include "ui_inputoutputpatcheditor.h"

class QComboBox;
class QPushButton;
class InputOutputMap;
class AudioCapture;
class OutputPatch;
class InputPatch;
class Doc;

/** @addtogroup ui_io
 * @{
 */

class InputOutputPatchEditor final : public QWidget, public Ui_InputOutputPatchEditor
{
    Q_OBJECT
    Q_DISABLE_COPY(InputOutputPatchEditor)

    /********************************************************************
     * Initialization
     ********************************************************************/
public:
    InputOutputPatchEditor(QWidget* parent, quint32 universe, InputOutputMap* ioMap, Doc* doc);
    ~InputOutputPatchEditor();

signals:
    void mappingChanged();
    void audioInputDeviceChanged();

private:
    InputOutputMap* m_ioMap;
    Doc *m_doc;

    quint32 m_universe;

    QString m_currentInputPluginName;
    quint32 m_currentInput;
    QString m_currentOutputPluginName;
    quint32 m_currentOutput;
    QString m_currentProfileName;     // QLCInputProfile name (MIDI/OSC etc.)
    QString m_currentHidProfileName;  // HID device profile name
    QString m_currentFeedbackPluginName;
    quint32 m_currentFeedback;

    /************************************************************************
     * Mapping page
     ************************************************************************/
private:
    InputPatch* patch() const;
    QTreeWidgetItem* currentlyMappedItem() const;
    void setupMappingPage();
    void updateProfileColumnWidget(QTreeWidgetItem *item); // set/refresh profile cell for item
    void applyCurrentProfile();        // push current profile names to the patch
    bool isHidPlugin() const;          // true when m_currentInputPluginName is HID-style
    QString fullProfilePath(const QString& manufacturer, const QString& model) const;

    QTreeWidgetItem *itemLookup(QString pluginName, QString devName);
    void fillMappingTree();
    QTreeWidgetItem* pluginItem(const QString& pluginName);
    void showPluginMappingError();

private slots:
    void slotMapCurrentItemChanged(QTreeWidgetItem* item);
    void slotMapItemChanged(QTreeWidgetItem* item, int col);
    void slotConfigureInputClicked();
    void slotPluginConfigurationChanged(const QString& pluginName, bool success);
    void slotHotpluggingChanged(bool checked);

    /************************************************************************
     * Audio page
     ************************************************************************/
private:
    void initAudioTab();

private slots:
    void slotAudioDeviceItemChanged(QTreeWidgetItem* item, int col);
    void slotSampleRateIndexChanged(int index);
    void slotAudioChannelsChanged(int index);
    void slotAudioInputPreview(bool enable);
    void slotAudioUpdateLevel(double *spectrumBands, int size, double maxMagnitude, quint32 power);

private:
    AudioCapture *m_inputCapture;
};

/** @} */

#endif /* INPUTOUTPUTPATCHEDITOR_H */
