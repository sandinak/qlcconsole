/*
  Q Light Controller Plus
  teachingglossary.h

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

#ifndef TEACHINGGLOSSARY_H
#define TEACHINGGLOSSARY_H

#include <QString>
#include <QStringList>

#include "qlcengine_export.h"

/** @addtogroup engine Engine
 * @{
 */

/**
 * The one glossary behind Teaching mode's popovers (docs/features/teaching.md):
 * a one-line, crew-level definition for each term a student meets in the Show
 * Tree headers, the Run tab and the key dialogs. The wording is the course
 * glossary's (docs/course/GLOSSARY.md), shortened to one line. A Teaching
 * show shows it as a line added to the tooltip of the thing the term names;
 * a show that is not Teaching never asks for it.
 */
class QLC_ENGINE_EXPORT TeachingGlossary
{
public:
    /** The definition of @a term (any case, spaces or dashes), or empty. */
    static QString definition(const QString &term);
    /** The line a tooltip carries: "Layer: One group of lights ...", or
     *  empty when @a term is not in the glossary. */
    static QString line(const QString &term);
    /** Every term, in the order written. */
    static QStringList terms();
};

/** @} */

#endif
