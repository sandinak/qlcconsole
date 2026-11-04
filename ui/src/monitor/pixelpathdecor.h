/*
  Q Light Controller Plus
  pixelpathdecor.h

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

#ifndef PIXELPATHDECOR_H
#define PIXELPATHDECOR_H

#include <QtGlobal>

class MonitorGraphicsView;
class MonitorFixtureItem;
class Doc;

/** @addtogroup ui_monitor
 * @{
 */

/** The plot's drawing of a pixel path, a wand and a silk flame's outline on
 *  a fixture's item (docs/design/FOUNTAIN_WAND.md), in the plot's view
 *  (Top, Front or Side). */
namespace PixelPathDecor
{
    /** How many LEDs @p fid has to lay along its path: its heads, or the
     *  LEDs of a one-head device (a WLED effect). */
    int ledCount(Doc *doc, quint32 fid);

    /** Work out and set @p item's decor for where it is now. */
    void apply(const MonitorGraphicsView *view, Doc *doc, MonitorFixtureItem *item, quint32 fid);
}

/** @} */

#endif
