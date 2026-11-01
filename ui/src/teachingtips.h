/*
  Q Light Controller Plus
  teachingtips.h

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

#ifndef TEACHINGTIPS_H
#define TEACHINGTIPS_H

#include <QStringList>
#include <QString>

#include "qlcui_export.h"

class QWidget;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * Teaching mode's glossary popovers (docs/features/teaching.md): in a
 * Teaching show, the tooltip of a thing a student meets (a Show Tree column
 * header, a Run tab button, a dialog's setting) ends with a one-line
 * definition of the terms it names, from the one glossary
 * (TeachingGlossary). Hovering shows it; Shift+F1 (What's This) shows the
 * same on a touch screen. A show that is not Teaching has the plain tooltip
 * and nothing is looked up.
 */
namespace TeachingTips
{
/** @a baseTip with the definition of each of @a terms on a line of its
 *  own, when @a doc is a Teaching show; else @a baseTip. */
QLC_UI_EXPORT QString tip(const Doc *doc, const QString &baseTip, const QStringList &terms);

/** Give @a widget the tooltip (and What's This) @a baseTip + the terms,
 *  now and whenever Teaching is turned on or off. */
QLC_UI_EXPORT void attach(Doc *doc, QWidget *widget, const QString &baseTip, const QStringList &terms);
}

/** @} */

#endif
