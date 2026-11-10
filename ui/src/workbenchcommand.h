/*
  Q Light Controller Plus
  workbenchcommand.h

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

#ifndef WORKBENCHCOMMAND_H
#define WORKBENCHCOMMAND_H

#include <QStringList>
#include <QWidget>
#include <QPointer>
#include <functional>

#include "showtreecommand.h"

class OperationRegistry;
class QTextBrowser;
class QToolButton;
class QLineEdit;
class QMimeData;
class QLabel;
class Doc;
class AiCommandLine;

/** @addtogroup ui_functions
 * @{
 */

/**
 * The command bar in Build (docs/design/WORKFLOW_DESIGN.md W6): the Show
 * Tree's command line, through the same Operation Registry, with a Build
 * context -- the scene loaded in the Workbench (or the layer selected in
 * it) instead of a cue.
 *
 * A looks line (`Towers colour Blue dimmer 50`, `dimmer 50`, `effect
 * Sinelon`) is parsed by ShowTreeCommand with Context::build set: it
 * records the groups, fixtures and palettes the words meant and changes no
 * cue. This bar then adds them to the scene as a drop of them on the layer
 * would, which is one undo step on the shared history. `palette …`,
 * `undo`, `help`, the registry's questions and AI Assist's `? question` run
 * as they do in the tree; the words that need a cue (cue, list, record,
 * GO…) answer "Show Tree only".
 */
class BuildCommandBar : public QWidget
{
    Q_OBJECT

public:
    /** What the bar needs from Build. */
    struct Host
    {
        /** The scene the Workbench edits now (invalid: none). */
        std::function<quint32()> sceneId;
        /** Add @a mime's groups, fixtures and palettes to scene @a scene:
         *  the drop on it, one undo step. */
        std::function<bool(const QMimeData *mime, quint32 scene)> addToScene;
        /** The Library's Recent palettes, newest first. */
        std::function<QList<quint32>()> recentPalettes;
        /** A drop's palettes, groups and sets become Recent. */
        std::function<void(const QMimeData *mime)> noteRecent;
        /** The Workbench is lent to the Show Tree (nothing loaded here). */
        std::function<bool()> workbenchLent;
        /** `palette … edit`, a palette just made: show it in Build. */
        std::function<void(quint32 paletteId)> showPalette;
    };

    BuildCommandBar(Doc *doc, const Host &host, QWidget *parent = nullptr);
    ~BuildCommandBar();

    QLineEdit *line() const { return m_line; }
    OperationRegistry *operations() const { return m_operations; }
    /** The message under the line (the last result). */
    QString message() const;
    /** The help pane's text (rich text); empty when not shown. */
    QString helpHtml() const;
    bool isHelpShown() const;
    bool isSyntaxShown() const;
    void setSyntaxShown(bool shown);
    void focusLine();

    /** Run one line, as Return does. True: it worked (the line clears). */
    bool runLine(const QString &line);

    /** The mime a looks line's groups, fixtures, sets and palettes make --
     *  what a drop of them is. The caller owns it; null: nothing. */
    static QMimeData *mimeFor(const ShowTreeCommand::BuildEdit &edit);
    /** The one-line syntax shown under the bar. */
    static QString grammar();

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private:
    void setMessage(const QString &text, bool ok);

    Doc *m_doc;
    Host m_host;
    ShowTreeCommand *m_commander = nullptr;
    OperationRegistry *m_operations = nullptr;
    QLineEdit *m_line = nullptr;
    QLabel *m_message = nullptr;
    QLabel *m_syntax = nullptr;
    QToolButton *m_syntaxButton = nullptr;
    QTextBrowser *m_help = nullptr;
    /* AI Assist in the line (null without QLC_AI_ASSIST). Declared either
       way: QLC_AI_ASSIST is the ui library's own define, so a test built
       without it saw a smaller class, and the bar it made on its stack was
       written past -- glibc's "double free or corruption" in layoutsanity
       on Linux (gate 043j). */
    AiCommandLine *m_ai = nullptr;
    QStringList m_history;
    int m_historyAt = -1;
    /** "Did you mean": what was typed, and the fix put in its place (Esc
     *  puts it back). */
    QString m_suggestedFrom;
    QString m_suggestedLine;
};

/** @} */

#endif
