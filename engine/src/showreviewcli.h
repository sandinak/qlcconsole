/*
  Q Light Controller Plus - qlcconsole
  showreviewcli.h

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

#ifndef SHOWREVIEWCLI_H
#define SHOWREVIEWCLI_H

#include <QStringList>
#include <QString>

#include "qlcengine_export.h"

class QTextStream;

/**
 * qlcconsole --review show.qlcc (docs/design/SHOW_REVIEW.md section 5.4):
 * Show Review with no window, for CI and a teacher's folder of hand-ins.
 * Loads the show read-only -- no plugins, nothing sent, no device opened,
 * the file never written -- runs every check, prints the findings and
 * exits:
 *   0  nothing open at or above --min (default: warning)
 *   1  open warnings
 *   2  open blockers
 *   3  the show couldn't be read (or the arguments were wrong)
 * Accepted findings don't count; --include-accepted lists them with their
 * notes.
 */
namespace ShowReviewCli
{
    enum Exit { Clean = 0, Warnings = 1, Blockers = 2, Unreadable = 3 };

    /** Whether @a argv asks for a review (main() checks this before it
     *  makes the QApplication). */
    QLC_ENGINE_EXPORT bool wanted(int argc, char **argv);

    /** The whole command: makes its own (window-less) application, then run(). */
    QLC_ENGINE_EXPORT int main(int argc, char **argv);

    /** The command's work, with an application already made: @a args
     *  without the program name. Output to @a out, errors to @a err. Loads
     *  this computer's fixture definitions and scripts unless @a libraries
     *  is false (tests give a show that needs none). */
    QLC_ENGINE_EXPORT int run(const QStringList &args, QTextStream &out, QTextStream &err,
                              bool libraries = true);

    /** The usage text. */
    QLC_ENGINE_EXPORT QString usage();
}

#endif
