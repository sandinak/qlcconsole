/*
  Q Light Controller Plus
  importsheet.h

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

#ifndef IMPORTSHEET_H
#define IMPORTSHEET_H

#include <QPointer>
#include <QDialog>
#include <QList>

#include "showtreecontroller.h"

class QTextBrowser;
class VCCueList;
class VCButton;
class VCSlider;
class QCheckBox;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * What Import chaser… made, as a sheet to read (W0): whether the list plays
 * the same as the chaser (the report's own check), what each step became
 * (cues, layers, includes, mixes), what plays as it is and why, and an
 * offer to fire the new list from whatever fires the chaser now (VC
 * buttons, VC Cue Lists and sliders, the main cue list and its cue-list
 * number) -- ticked, and part of the import's undo step.
 */
class ImportSheet final : public QDialog
{
    Q_OBJECT

public:
    ImportSheet(Doc *doc, const ShowTreeController::ImportReport &report, quint32 chaserId,
                QWidget *parent = nullptr);

    /** What fires a chaser now, and moving that to another (the list's). */
    struct Repoint
    {
        quint32 from = 0xFFFFFFFF;
        quint32 to = 0xFFFFFFFF;
        QList<QPointer<VCButton>> buttons;
        QList<QPointer<VCCueList>> cueLists;
        QList<QPointer<VCSlider>> sliders;
        bool mainCueList = false;
        QString cueListNumber;              //!< the chaser's, moved with it
        int count() const;
        /** One line each: "VC button “Go”", "the main cue list". */
        QStringList lines() const;
        /** To the list (@a forward) or back to the chaser. */
        void apply(Doc *doc, bool forward) const;
    };
    /** What fires @a from, to be moved to @a to. */
    static Repoint planRepoint(Doc *doc, quint32 from, quint32 to);

    /** The tick: fire the list from what fires the chaser. */
    bool repoint() const;
    QCheckBox *repointBox() const { return m_repoint; }
    /** Undo import was pressed. */
    bool undoAsked() const { return m_undo; }
    /** The sheet as text (tests). */
    QString text() const;

private:
    QString html(Doc *doc, const ShowTreeController::ImportReport &report, const QString &chaser) const;

    QTextBrowser *m_body = nullptr;
    QCheckBox *m_repoint = nullptr;
    bool m_undo = false;
};

/** @} */

#endif
