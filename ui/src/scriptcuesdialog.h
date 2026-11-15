/*
  Q Light Controller Plus
  scriptcuesdialog.h

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

#ifndef SCRIPTCUESDIALOG_H
#define SCRIPTCUESDIALOG_H

#include <QDialog>
#include <QHash>
#include <QSet>

#include "qlcui_export.h"

#include "showtreecontroller.h"

class QTreeWidgetItem;
class QTreeWidget;
class QPushButton;
class QLabel;
class QTimer;
class Doc;

/** @addtogroup ui_showtree
 * @{
 */

/**
 * Cues from the script (0.4.9, board Q237): the calling script's SCENE and
 * LIGHTING lines as proposed cues, song by song, each with the number it
 * would get (ShowTreeController::proposeScriptCues), its name and its note.
 * Untick a line (or a whole song) and the rest are numbered again. Lines
 * that already have a cue are listed, greyed, with the cue. Nothing is made
 * until Make: then every ticked line is a cue in one undo step
 * (makeScriptCues). Cancel makes nothing.
 */
class QLC_UI_EXPORT ScriptCuesDialog : public QDialog
{
    Q_OBJECT

public:
    ScriptCuesDialog(Doc *doc, quint32 productionId, quint32 listId, QWidget *parent = nullptr);

    QTreeWidget *tree() const { return m_tree; }
    /** What would be made, as shown now. */
    const QList<ShowTreeController::ScriptCue> &proposals() const { return m_plan; }
    /** Tick or untick the cue proposed for script line @a line. */
    void setLineTicked(int line, bool ticked);
    bool isLineTicked(int line) const;
    /** The lines unticked. */
    QSet<int> leftOut() const;
    /** How many cues Make would make. */
    int toMake() const;
    /** After Make: the cues made, and what was said. */
    const QList<quint32> &made() const { return m_made; }
    const QString &message() const { return m_message; }

public slots:
    void accept() override;

private:
    void fill();
    void renumber();
    void updateSummary();

    Doc *m_doc;
    quint32 m_production;
    quint32 m_list;
    QList<ShowTreeController::ScriptCue> m_plan;
    QHash<int, QTreeWidgetItem *> m_rows;      //!< script line -> its row
    QTreeWidget *m_tree;
    QLabel *m_intro;
    QLabel *m_summary;
    QPushButton *m_make;
    QTimer *m_renumber;
    QList<quint32> m_made;
    QString m_message;
    bool m_filling = false;
};

/** @} */

#endif
