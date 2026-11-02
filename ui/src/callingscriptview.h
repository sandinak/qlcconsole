/*
  Q Light Controller Plus
  callingscriptview.h

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

#ifndef CALLINGSCRIPTVIEW_H
#define CALLINGSCRIPTVIEW_H

#include <QWidget>
#include <QString>
#include <functional>

#include "qlcui_export.h"

#include "callingscript.h"

class QPlainTextEdit;
class QPushButton;
class QCheckBox;
class QComboBox;
class QTimer;
class QLabel;
class Doc;

/** @addtogroup ui_showtree
 * @{
 */

/**
 * The calling script view (0.5): the stage manager's script or lyrics with
 * the production's cues in the margin -- standby and GO, number, name, note
 * -- placed by dragging a cue from the Show Tree onto a word, or selecting
 * text and choosing "Cue here". It is a view of the production's cue lists
 * (the script and its anchors are saved with the Show Tree), a window of
 * its own opened from the Show Tree's menu, not a tab. In Operate it
 * follows the running cue: the line being called is shown, the next
 * standby marked. GO goes through the one run path (CueRunner::runCommand
 * -> ShowControl::run). Print / PDF is the cue sheet's writer.
 */
class QLC_UI_EXPORT CallingScriptView : public QWidget
{
    Q_OBJECT

public:
    CallingScriptView(Doc *doc, quint32 productionId, QWidget *parent = nullptr);

    quint32 productionId() const { return m_production; }

    /** The cue the Show Tree has selected, offered to "Cue here". */
    void setCueSource(const std::function<quint32()> &source) { m_cueSource = source; }

    /** The script as laid out now. */
    const CallingScriptSheet &sheet() const { return m_sheet; }
    QPlainTextEdit *textEdit() const;

    /** Rebuild from the Show Tree and re-mark the running cue and standby. */
    void refresh();

    /** Import a .txt or .md file as the script. */
    bool importFile(const QString &path, QString *error = nullptr);
    /** Place cue @a cueId at the text selection (or the cursor); GO or standby. */
    bool cueHere(quint32 cueId, bool go = true);
    /** Place cue @a cueId at character @a offset (what a drop does). */
    bool dropCue(quint32 cueId, int offset, bool go = true);
    /** Take cue @a cueId's markers off. */
    bool removeCue(quint32 cueId);
    /** The script text being edited is the production's now. */
    void commitEdit();
    bool isEditing() const;
    void setEditing(bool editing);
    /** GO by the one run path (the main list's chaser). */
    bool go();
    /** The script as a PDF. */
    bool savePdf(const QString &path, QString *error = nullptr) const;

    /** The cue "Cue here" would place (the combo's). */
    quint32 chosenCue() const;
    void chooseCue(quint32 cueId);

    bool follows() const;
    void setFollows(bool follow);
    /** The lines marked as now (the running cue's GO) and as next (standby);
     *  -1: none. */
    int nowLine() const { return m_nowLine; }
    int standbyLine() const { return m_standbyLine; }
    quint32 nowCue() const { return m_nowCue; }
    quint32 standbyCue() const { return m_standbyCue; }
    /** The first line in view, and whether line @a line is in view. */
    int firstVisibleLine() const;
    bool isLineVisible(int line) const;
    /** The width of the margin the markers are drawn in. */
    int marginWidth() const;
    /** The markers of line @a line as drawn ("GO 1.0.2 Walk-in"). */
    QStringList marginText(int line) const;

signals:
    /** The user placed or removed a cue, or changed the text. */
    void scriptChanged();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void slotImport();
    void slotPdf();
    void slotCueHere();
    void slotStandbyHere();
    void slotEditToggled(bool on);
    void slotTick();

private:
    void rebuild();
    void updateMarks(bool scroll);
    void fillCues();
    void updateButtons();
    int placeOffset() const;
    class Chaser *mainChaser() const;
    quint32 mainList() const;

    Doc *m_doc;
    quint32 m_production;
    std::function<quint32()> m_cueSource;
    CallingScriptSheet m_sheet;
    class CallingScriptEdit *m_text;
    QPushButton *m_import;
    QPushButton *m_edit;
    QPushButton *m_here;
    QPushButton *m_standby;
    QPushButton *m_pdf;
    QPushButton *m_go;
    QCheckBox *m_follow;
    QComboBox *m_cue;
    QLabel *m_summary;
    QTimer *m_timer;
    int m_nowLine = -1;
    int m_standbyLine = -1;
    quint32 m_nowCue = 0xFFFFFFFF;
    quint32 m_standbyCue = 0xFFFFFFFF;
    bool m_rebuilding = false;
    bool m_forceScroll = false;
};

/** @} */

#endif
