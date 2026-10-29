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
class QTreeWidgetItem;
class QTabWidget;
class QPushButton;
class QComboBox;
class QLabel;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The show converter (docs/design/WORKFLOW_DESIGN.md §3.5, W5): one row per chaser of
 * an old show, told apart by how it runs -- a cue list (waits for GO,
 * notes, cue numbers, show control, a VC Cue List) is offered as a Show
 * Tree cue list, ticked; an effect (strobes, chases, sweeps: it loops on
 * its own) stays a function and is not offered. Per row the import's plan
 * and its same-output check, made without changing the show. Convert runs
 * the ticked rows, each its own undo step: Import in place (the default)
 * or beside the chaser. Nothing happens until Convert is pressed.
 *
 * Upgrade mode (File ▸ Upgrade this show…, §3.7) is the same table as a
 * front end over all of W5: the show's collections listed too, each with
 * what becomes of it; chasers already upgraded in place, greyed with the
 * reason (ShowTreeController::whyNotSend) and Restore original… on their
 * row; a line saying when nothing is left to upgrade, and how many chasers
 * are the Show Tree's own (not listed); a second page that offers Capture into palettes for the raw
 * scenes the lists will play. Upgrade applies the lot as one undo step.
 */
class ShowConverter final : public QDialog
{
    Q_OBJECT

public:
    enum Mode
    {
        Convert,    //!< Import chaser… ▸ Convert an old show…
        Upgrade     //!< File ▸ Upgrade this show…
    };
    ShowConverter(Doc *doc, QWidget *parent = nullptr, Mode mode = Convert);
    Mode mode() const { return m_kind; }

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

    /** The table and the details as text (tests); Upgrade: both pages. */
    QString text() const;

    /** Upgrade: what Apply does with the ticked chasers (@a inPlace) and
     *  the ticked raw scenes, as one undo step. The lines to show. */
    typedef std::function<QStringList(const QList<quint32> &chasers, bool inPlace,
                                      const QList<quint32> &scenes)> Upgrader;
    void setUpgrader(const Upgrader &upgrader) { m_upgrader = upgrader; }
    /** Upgrade: Restore original for an in-place list (asks; true: done). */
    typedef std::function<bool(quint32 listId)> Restorer;
    void setRestorer(const Restorer &restorer)
    {
        m_restorer = restorer;
        updateButtons();
    }

    struct CaptureRow
    {
        quint32 scene = 0xFFFFFFFF;
        ShowTreeController::CapturePlan plan;
        bool inTree = false;            //!< a ▣ row of the tree plays it now
        QList<quint32> chasers;         //!< cue-list chasers whose lists would
    };
    /** Upgrade: the raw scenes the tree plays, or would once converted. */
    const QList<CaptureRow> &captureRows() const { return m_captureRows; }
    /** The scenes ticked to capture into palettes. */
    QList<quint32> chosenScenes() const;
    void setCaptureChosen(quint32 sceneId, bool on);
    /** Upgrade: the list rows made in place, with Restore original. */
    QList<quint32> convertedLists() const;
    /** Press Restore original… on in-place list @a listId's row. */
    bool restore(quint32 listId);
    /** Upgrade: the Restore original… button on list @a listId's row. */
    QPushButton *restoreButton(quint32 listId) const;
    /** Upgrade: the line under the table (nothing left to upgrade, the
     *  Show Tree's own chasers); empty when there's nothing to say. */
    QString note() const;
    QTabWidget *pages() const { return m_pages; }

    /** A capture plan as lines: each layer with its looks, what stays raw. */
    static QStringList captureLines(Doc *doc, const ShowTreeController::CapturePlan &plan);

    /** Whether a VC Cue List plays @a chaserId (a cue-list sign). */
    static bool playsInVcCueList(quint32 chaserId);
    /** The cue-list chasers of @a doc's show, as the converter tells them
     *  (for the banner), by name. */
    static QStringList cueListChasers(Doc *doc);

private:
    void build();
    void buildUpgradeRows();
    void buildCaptureRows();
    void updateButtons();
    void updatePlanTexts();
    void showItem(QTreeWidgetItem *item);
    QString resultsHtml() const;

    Doc *m_doc;
    Mode m_kind;
    QList<Row> m_rows;
    QTreeWidget *m_table = nullptr;
    QTextBrowser *m_details = nullptr;
    QComboBox *m_mode = nullptr;
    QPushButton *m_convertButton = nullptr;
    Converter m_converter;
    QStringList m_results;
    QTabWidget *m_pages = nullptr;
    QTreeWidget *m_captureTable = nullptr;
    QLabel *m_note = nullptr;
    QList<CaptureRow> m_captureRows;
    Upgrader m_upgrader;
    Restorer m_restorer;
};

/** @} */

#endif
