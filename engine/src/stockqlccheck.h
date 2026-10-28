/*
  Q Light Controller Plus
  stockqlccheck.h

  What a saved .qxw holds that stock QLC+ 4 (4.14) reads past: the blocks
  only qlcconsole writes. QLC+ 4 skips an element it does not know, so
  these open fine there and are gone once it saves. Read from the file as
  written, by element name, against what QLC+ 4.14's loaders read
  (App::loadXML, Doc::loadXML, MonitorProperties::loadXML, VCFrame).

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

#ifndef STOCKQLCCHECK_H
#define STOCKQLCCHECK_H

#include <QStringList>

class QIODevice;

/** @addtogroup engine Engine
 * @{
 */

namespace StockQlcCheck
{
    /** What in the workspace XML on @p device QLC+ 4 would drop, as short
     *  phrases for a sentence ("the Show Tree (cue lists, cues and hits)"),
     *  each once, in the order met. Empty: QLC+ 4 reads all of it. */
    QStringList losses(QIODevice *device);
    /** losses() of the file at @p path (empty when it can't be read). */
    QStringList lossesInFile(const QString &path);
}

/** @} */

#endif
