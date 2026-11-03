/*
  Q Light Controller Plus
  formationeditor.h

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

#ifndef FORMATIONEDITOR_H
#define FORMATIONEDITOR_H

#include <QWidget>
#include <QPointer>
#include <QList>
#include <QHash>
#include "qlcui_export.h"

class Doc;
class QLabel;
class QSlider;
class QCheckBox;
class QComboBox;
class QListWidget;
class QToolButton;
class QDoubleSpinBox;
class LinePlanView;
class LineSpreadPreview;
class LinePlanWindow;

/** @addtogroup ui UI
 * @{
 */

/**
 * The Look Editor's page for a Formation palette (Q197, docs/features/
 * show-tree.md "Line aims"): its Spread, Grouping and Cross, each set or
 * left to come from elsewhere, and a PREVIEW of it -- the order strip and
 * the plan view -- on a line and the lights picked here.
 *
 * The preview pick (the line, the groups, fixtures one by one) is only for
 * looking: it is kept in this editor for the session, per palette, never
 * in the palette, the show file or the undo history, and never marks the
 * show modified. Nothing plays on stage: it is drawn, like Blind.
 */
class QLC_UI_EXPORT FormationEditor final : public QWidget
{
    Q_OBJECT

public:
    explicit FormationEditor(Doc *doc, QWidget *parent = nullptr);
    ~FormationEditor() override;

    /** Edit Formation palette @p paletteId (anything else: nothing shown). */
    void setPalette(quint32 paletteId);
    quint32 paletteId() const { return m_paletteId; }

    /** What the preview shows: a line target and the lights on it. */
    struct Pick
    {
        quint32 line = 0xFFFFFFFF;
        QList<quint32> groups;      //!< fixture groups, as one formation
        QList<quint32> fixtures;    //!< fixtures picked one by one
        /** Why these lights, when the editor chose them rather than the
         *  user ("Showing AT-W-ALL -- aimed at Line 1 in cue 1.1.1 Verse"). */
        QString note;
        bool isEmpty() const { return groups.isEmpty() && fixtures.isEmpty(); }
    };
    /** The preview of the palette being edited. */
    Pick pick() const { return m_pick; }
    /** Preview @p pick instead (the page's picker; tests). Not an edit. */
    void setPick(const Pick &pick);

    /** The preview a palette starts with: the first line target, and the
     *  groups the show most often aims at it (a layer's, or a cue-wide
     *  aim's layers' together), with a note naming them and the first cue
     *  that does; none: no groups and no note. */
    static Pick defaultPick(Doc *doc);
    /** Show no lights (the "Show none" link of a chosen pick). Not an edit. */
    void clearPick();

    /** The views (tests). */
    LineSpreadPreview *strip() const { return m_strip; }
    LinePlanView *plan() const { return m_plan; }
    QLabel *hint() const { return m_hint; }
    /** The pop-out window, when open (tests). */
    LinePlanWindow *planWindow() const;
    /** Open the strip and plan in a window of their own, fed by this
     *  page's preview. */
    void openPlanWindow();

    /** The controls (tests). */
    QCheckBox *spreadSet() const { return m_spreadSet; }
    QDoubleSpinBox *spreadSpin() const { return m_spreadSpin; }
    QCheckBox *groupSet() const { return m_groupSet; }
    QComboBox *groupCombo() const { return m_groupCombo; }
    QCheckBox *crossSet() const { return m_crossSet; }
    QComboBox *crossCombo() const { return m_crossCombo; }
    QComboBox *lineCombo() const { return m_lineCombo; }
    QListWidget *pickList() const { return m_pickList; }

signals:
    /** The palette's settings changed (one undo step each). */
    void edited(quint32 paletteId);

private slots:
    void slotSettingChanged();
    void slotPickChanged();
    void slotDocCleared();

private:
    /** Fill the controls from the palette. */
    void syncControls();
    /** Fill the picker (lines, groups, fixtures) for m_pick. */
    void syncPicker();
    /** Draw the strip and the plan for m_pick and the palette. */
    void updatePreview();
    /** Write one setting through the Show Tree (undo step + bake). */
    void commit(const QString &key, const QString &value);

private:
    Doc *m_doc;
    quint32 m_paletteId = 0xFFFFFFFF;
    bool m_loading = false;
    Pick m_pick;
    /** The session's preview per palette (never saved). */
    QHash<quint32, Pick> m_picks;

    QLabel *m_summary;
    QCheckBox *m_spreadSet;
    QSlider *m_spreadSlider;
    QDoubleSpinBox *m_spreadSpin;
    QCheckBox *m_groupSet;
    QComboBox *m_groupCombo;
    QCheckBox *m_crossSet;
    QComboBox *m_crossCombo;

    QComboBox *m_lineCombo;
    QListWidget *m_pickList;
    QLabel *m_hint;
    LineSpreadPreview *m_strip;
    LinePlanView *m_plan;
    QPointer<LinePlanWindow> m_planWindow;
};

/** @} */

#endif
