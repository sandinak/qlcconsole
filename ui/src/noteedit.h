/*
  Q Light Controller Plus
  noteedit.h

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

#ifndef NOTEEDIT_H
#define NOTEEDIT_H

#include <QStyledItemDelegate>
#include <QTextEdit>

/** @addtogroup ui
 * @{
 */

/**
 * A note field (a chaser step's, a Show Tree cue's) that opens up while it
 * is being typed in.
 *
 * Not being edited it is one line: the note elided, all of it in the
 * tooltip. Being edited it word-wraps and grows to fit what is typed, up to
 * maxLines() lines. Return commits (editingFinished()), Shift+Return or
 * Alt+Return starts a new line, Esc puts back the note as it was. Leaving
 * the field commits too, as a QLineEdit does.
 *
 * In a layout it grows downwards; as an item view's editor (NoteDelegate) it
 * floats over the rows below it (on the view's window, so a short list does
 * not clip it), and wider than its cell where there is room.
 */
class NoteEdit final : public QTextEdit
{
    Q_OBJECT

public:
    explicit NoteEdit(QWidget *parent = nullptr);

    /** The note, as typed (new lines kept). */
    QString text() const;
    /** Show @a text, without committing it. */
    void setText(const QString &text);

    /** What the field is for, shown in the tooltip under the note. */
    void setHelpTip(const QString &tip);

    /** The most lines it grows to before it scrolls (default 6). */
    int maxLines() const { return m_maxLines; }
    void setMaxLines(int lines);

    /** True while it is open for typing (it has focus). */
    bool isExpanded() const { return m_expanded; }

    /** As an item view's editor: float over @a cell, in its parent. */
    void setAnchorRect(const QRect &cell);

    /** The text as one line: new lines shown as " ⏎ ". */
    static QString flattened(const QString &text);
    /** The tooltip for a note: all of it, wrapped, then @a help. */
    static QString tipFor(const QString &note, const QString &help = QString());

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    /** Return pressed, or the field left with the note changed or not. */
    void editingFinished();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void setExpanded(bool on);
    void fit();
    int heightFor(int lines) const;
    void updateTip();

    QString m_original;
    QString m_help;
    QRect m_anchor;
    int m_maxLines = 6;
    bool m_expanded = false;
    bool m_floating = false;
    bool m_finished = false;
    bool m_fitting = false;
};

/**
 * The note column of a step list: a cell shows the note on one line, elided
 * (new lines as " ⏎ "), all of it in the tooltip; editing it opens a
 * NoteEdit over the cell. Commits through the model as a line edit would.
 */
class NoteDelegate final : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit NoteDelegate(QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option,
                   const QModelIndex &index) override;

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;
};

/** @} */

#endif
