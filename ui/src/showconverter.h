/*
  Q Light Controller Plus
  showconverter.h

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

#ifndef SHOWCONVERTER_H
#define SHOWCONVERTER_H

#include <functional>
#include <QDialog>
#include <QList>

#include "showtreecontroller.h"

class QTextBrowser;
class QTreeWidget;
class QPushButton;
class QComboBox;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The show converter (WORKFLOW_DESIGN.md §3.5, W5): one row per chaser of
 * an old show, told apart by how it runs -- a cue list (waits for GO,
 * notes, cue numbers, show control, a VC Cue List) is offered as a Show
 * Tree cue list, ticked; an effect (strobes, chases, sweeps: it loops on
 * its own) stays a function and is not offered. Per row the import's plan
 * and its same-output check, made without changing the show. Convert runs
 * the ticked rows, each its own undo step: Import in place (the default)
 * or beside the chaser. Nothing happens until Convert is pressed.
 */
class ShowConverter final : public QDialog
{
    Q_OBJECT

public:
    ShowConverter(Doc *doc, QWidget *parent = nullptr);

    struct Row
    {
        quint32 chaser = 0xFFFFFFFF;
        ShowTreeController::ChaserClass cls;
        ShowTreeController::ImportReport plan;
        ShowTree::CueList list;
    };
    const QList<Row> &rows() const { return m_rows; }
    /** The chasers ticked to convert, in the table's order. */
    QList<quint32> chosen() const;
    void setChosen(quint32 chaserId, bool on);
    /** Import in place (true, the default) or beside the chaser. */
    bool inPlace() const;
    void setInPlace(bool on);
    /** Show row @a chaserId's plan below the table. */
    void select(quint32 chaserId);

    /** What Convert does, chaser by chaser (the view's import): the lines
     *  to show for each. */
    typedef std::function<QStringList(quint32 chaserId, bool inPlace)> Converter;
    void setConverter(const Converter &converter) { m_converter = converter; }
    /** Press Convert. The results, one or more lines a chaser. */
    QStringList convert();
    QPushButton *convertButton() const { return m_convertButton; }

    /** The table and the details as text (tests). */
    QString text() const;

    /** Whether a VC Cue List plays @a chaserId (a cue-list sign). */
    static bool playsInVcCueList(quint32 chaserId);
    /** The cue-list chasers of @a doc's show, as the converter tells them
     *  (for the banner), by name. */
    static QStringList cueListChasers(Doc *doc);

private:
    void build();
    void updateButtons();

    Doc *m_doc;
    QList<Row> m_rows;
    QTreeWidget *m_table = nullptr;
    QTextBrowser *m_details = nullptr;
    QComboBox *m_mode = nullptr;
    QPushButton *m_convertButton = nullptr;
    Converter m_converter;
    QStringList m_results;
};

/** @} */

#endif
