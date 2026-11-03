/*
  Q Light Controller Plus
  shownotespane.h

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

#ifndef SHOWNOTESPANE_H
#define SHOWNOTESPANE_H

#include <functional>
#include <QWidget>

#include "showtreenotes.h"

class StructureStudioView;
class ShowTreeController;
class QPlainTextEdit;
class QPushButton;
class QToolButton;
class QVBoxLayout;
class QLabel;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The notes pane beside the Show Tree (Q212): the notes of the selected cue,
 * layer, list, production or Library palette, newest first, each with its
 * date, who wrote it, an open/done tick and, when it has one, the snapshot of
 * the view it was written with (Show me puts the rig view back as it was).
 * A box at the bottom adds one; Snapshot the view adds one with the rig
 * view's camera, a small picture of it and the cue on stage. Every edit is a
 * step on the Show Tree's history (ShowTreeController::addNote...).
 */
class ShowNotesPane final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(ShowNotesPane)

public:
    ShowNotesPane(Doc *doc, QWidget *parent = nullptr);

    /** Follow item @a kind @a target (a layer: of cue @a layerCue). */
    void setItem(ShowTree::NoteKind kind, quint32 target, quint32 layerCue = 0xFFFFFFFF);
    /** Nothing that takes notes is selected. */
    void clearItem();
    bool hasItem() const { return m_target != 0xFFFFFFFF; }
    ShowTree::NoteKind itemKind() const { return m_kind; }
    quint32 itemTarget() const { return m_target; }

    /** Add a note with @a text (and, with @a snapshot, the view): what the
     *  Add and Snapshot buttons do. Invalid: nothing added. */
    quint32 addNote(const QString &text, bool snapshot = false);
    /** The view now: the rig view's camera and a small picture of it (the
     *  Rig Overview, opened when it isn't), and the cue on stage. */
    ShowTree::NoteView snapshotView();
    /** Show me: the rig view as note @a noteId saw it; in Design, its cue
     *  selected (so on the rig, as selecting it puts it); in Operate only
     *  the camera. False: the note has no snapshot. */
    bool showMe(quint32 noteId);

    /** The notes shown, newest first (tests). */
    QList<quint32> shownNotes() const { return m_shown; }
    /** The text box and buttons (tests, shots). */
    QPlainTextEdit *input() const { return m_input; }
    QLabel *sizeWarning() const { return m_sizeWarning; }

    /** Where the rig view comes from: by default the Rig Overview's
     *  (Monitor::stageOverviewView()). Tests give their own. */
    using ViewProvider = std::function<StructureStudioView *(bool open)>;
    static void setViewProvider(const ViewProvider &provider);
    static StructureStudioView *rigView(bool open);

    /** What selecting a cue does (ShowTreeView::selectNode). */
    std::function<void(quint32 cueId)> selectCue;

    /** Size of the thumbnails kept with a snapshot (their width, px). */
    static int imageWidth() { return 480; }

    /** "3 Nov 14:02" (with the year when it isn't this one); "cue note"
     *  for the undated entry from before notes. */
    static QString whenText(const QString &iso);

    QSize sizeHint() const override;

public slots:
    /** Show the item's notes as they are now. */
    void refresh();

signals:
    /** "All notes…" in the header. */
    void allNotesRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    ShowTreeController *controller() const;
    QWidget *entryWidget(const ShowTree::Note &note);
    void entryMenu(quint32 noteId, QToolButton *button);
    void submit(bool snapshot);

private:
    Doc *m_doc;
    ShowTree::NoteKind m_kind = ShowTree::NoteKind::Cue;
    quint32 m_target = 0xFFFFFFFF;
    quint32 m_layerCue = 0xFFFFFFFF;
    QLabel *m_title = nullptr;
    QToolButton *m_all = nullptr;
    QWidget *m_entriesHost = nullptr;
    QVBoxLayout *m_entries = nullptr;
    QLabel *m_empty = nullptr;
    QLabel *m_sizeWarning = nullptr;
    QPlainTextEdit *m_input = nullptr;
    QPushButton *m_add = nullptr;
    QPushButton *m_snapshot = nullptr;
    QList<quint32> m_shown;
};

/** @} */

#endif
