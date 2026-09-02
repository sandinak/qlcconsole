/*
  Q Light Controller Plus - qlcconsole
  startupwindow.h

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

#ifndef STARTUPWINDOW_H
#define STARTUPWINDOW_H

#include <QWidget>

class QVBoxLayout;
class QLabel;
class QPlainTextEdit;
class QProgressBar;

/**
 * A frameless splash-style window shown for the whole synchronous boot
 * sequence in App::init() -- the one place where every platform used to
 * show nothing (or, on macOS only, a bare indeterminate QProgressDialog
 * driven solely by plugin names). Replaces that entirely: app name/version,
 * a scrolling transcript of every load/build/validate step, and a
 * determinate progress bar so "still starting" is never ambiguous.
 *
 * The caller (App::startup()) knows the total number of top-level steps
 * up front, so the bar's range is exact rather than guessed. Two calls
 * drive it:
 *   - beginStep(): a new top-level phase -- bold line, advances the bar.
 *   - logDetail(): a sub-line under the current phase (e.g. one per loaded
 *     plugin) -- doesn't move the bar.
 *
 * App::init() runs before QApplication::exec() starts, so nothing repaints
 * on its own; both calls pump QApplication::processEvents() themselves.
 */
class StartupWindow : public QWidget
{
    Q_OBJECT

public:
    /** @param totalSteps The number of beginStep() calls to expect -- the
     *         progress bar's range is [0, totalSteps]. */
    explicit StartupWindow(int totalSteps, QWidget *parent = nullptr);

    void beginStep(const QString &text);
    void logDetail(const QString &text);

    /** Ask a yes/no question inline -- a button row appears in this same
     *  window instead of a separate QMessageBox, so a boot-time decision
     *  (e.g. autosave recovery) never pops a second window. Blocks the
     *  caller (a local QEventLoop, not the whole app) until answered, same
     *  synchronous contract as QMessageBox::question() had. Doesn't touch
     *  the progress bar -- callers decide themselves whether a question is
     *  one of their counted steps. */
    bool askYesNo(const QString &question);

private:
    QVBoxLayout *m_layout;
    QPlainTextEdit *m_log;
    QProgressBar *m_bar;
    int m_step;
};

#endif
