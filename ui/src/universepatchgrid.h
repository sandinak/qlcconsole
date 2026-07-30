/*
  Q Light Controller Plus
  universepatchgrid.h

  Copyright (c) Massimo Callegari

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

#ifndef UNIVERSEPATCHGRID_H
#define UNIVERSEPATCHGRID_H

#include <QWidget>
#include <QList>
#include <QPair>
#include <QStringList>

class QTableWidget;
class QTableWidgetItem;
class QComboBox;
class QAction;
class InputOutputMap;
class Doc;

/** Spreadsheet-style overview of the whole universe patch, one row per universe.
 *
 *  Output-first: the Output device + its network target (IP / universe / mode)
 *  are always visible and inline-editable. Input, Feedback and Passthrough are
 *  collapsible column groups toggled from the toolbar (revealed automatically if
 *  the workspace already uses them). Rows/cells are colour-coded by protocol and
 *  direction. No modal configure dialog.
 *
 *  ArtNet input and output are independent (input listens on an input universe;
 *  output sends to a target IP + output universe), so each direction carries its
 *  own params. MIDI output and feedback share one port, so a MIDI output cell
 *  gets an Out/FB role toggle rather than a separate feedback slot. */
class UniversePatchGrid : public QWidget
{
    Q_OBJECT

public:
    UniversePatchGrid(Doc *doc, QWidget *parent = nullptr);

    /** Rebuild every row from the current InputOutputMap state. */
    void reload();

private slots:
    void onItemChanged(QTableWidgetItem *item);
    void onModeChanged(int row, const QString &mode);
    void onOutputDeviceChanged(int row);
    void onOutputRoleChanged(int row);
    void onInputChanged(int row);
    void onProfileChanged(int row);
    void onFeedbackChanged(int row);
    void onAddUniverse();
    void onRemoveUniverse();
    void onSelectionChanged();
    void onRescan();
    void toggleInputGroup(bool on);
    void toggleFeedbackGroup(bool on);
    void togglePassthroughGroup(bool on);

private:
    void populateRow(int row, int uniIndex);
    void setupColumnSizing();
    void refreshOptionCaches();
    void scheduleReload();
    bool isNetworkPlugin(const QString &pluginName) const;
    bool isMidiPlugin(const QString &pluginName) const;
    int  fixturesOnUniverse(int uniIndex) const;
    void setColumnsHidden(const QList<int> &cols, bool hidden);

    /** Apply @p prop=@p value to the output patch of every selected row that is a
     *  network patch (always including @p triggerRow). If @p autoIncrement the int
     *  value increments by one per row (ArtNet block patching). */
    void applyToSelection(const QString &prop, const QVariant &value,
                          bool autoIncrement, int triggerRow);

    /** Build an output/input/feedback device combo (in a cell widget). */
    QComboBox *buildDeviceCombo(bool inputs, bool excludeMidi,
                                const QString &curPlugin, int curLine);

private:
    Doc            *m_doc;
    InputOutputMap *m_ioMap;
    QTableWidget   *m_table;
    QAction        *m_removeAction = nullptr;
    QAction        *m_inputToggle = nullptr;
    QAction        *m_feedbackToggle = nullptr;
    QAction        *m_ptToggle = nullptr;
    bool            m_loading = false;
    bool            m_reloadScheduled = false;
    bool            m_autoRevealDone = false;

    // Option lists, rebuilt each reload (plugin → its line names).
    QList<QPair<QString, QStringList> > m_outPlugins;
    QList<QPair<QString, QStringList> > m_inPlugins;
    QStringList                         m_profiles;
};

#endif // UNIVERSEPATCHGRID_H
