/*
  Q Light Controller Plus
  lookstackcheck.h

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

#ifndef LOOKSTACKCHECK_H
#define LOOKSTACKCHECK_H

#include <QString>
#include <QList>

class Scene;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Reads a look's stack by the stacking rules (docs/features/effects.md,
 * "How a look stacks") and says where something in it won't do what it
 * looks like it does: a look the one below it always covers, an effect that
 * matches none of the targets, a dimmer that only reaches some heads...
 */
namespace LookStackCheck
{
    struct Issue
    {
        enum Level { Note, Warning };
        quint32 paletteId;      //!< the look entry it is about; invalid: the look as a whole
        Level level;
        QString text;
    };

    QList<Issue> check(Doc *doc, const Scene *scene);
}

/** @} */

#endif
