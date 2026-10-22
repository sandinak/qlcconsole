/*
  Q Light Controller Plus
  splitterhandles.h

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

#ifndef SPLITTERHANDLES_H
#define SPLITTERHANDLES_H

#include <QList>
#include <QString>

class QSplitter;

/** @addtogroup ui UI
 * @{
 */

/**
 * What every pane handle does: where it was left is remembered, and a
 * double-click on it puts it back where it started ("a handle between the
 * looks and fixtures and groups to resize as needed"). Shared, so each
 * splitter that wants that says one line instead of three lambdas.
 */
namespace SplitterHandles
{
    /** Restore @p splitter from QSettings @p key (if saved), save it there
     *  whenever a handle moves, and reset it on a double-click on a handle. */
    void remember(QSplitter *splitter, const QString &key);

    /** Reset @p splitter on a double-click on any of its handles; a non-empty
     *  @p key is forgotten too, so the next view starts from the default. */
    void resetOnDoubleClick(QSplitter *splitter, const QString &key = QString());

    /** The sizes the panes start from: a pane with no stretch gets what it
     *  asks for (its size hint), the rest is shared by stretch factor. */
    QList<int> defaultSizes(QSplitter *splitter);

    /** Put @p splitter's handles back to defaultSizes(); forget @p key. */
    void reset(QSplitter *splitter, const QString &key = QString());
}

/** @} */

#endif
