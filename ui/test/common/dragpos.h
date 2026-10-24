/*
  Q Light Controller Plus
  dragpos.h

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

#ifndef DRAGPOS_H
#define DRAGPOS_H

#include <QtGlobal>
#include <QPointF>
#include <QPoint>

/* The position a test hands QDragEnterEvent / QDragMoveEvent. Qt 6.12 added
 * constructors taking QPointF and deprecated the QPoint ones (an error in
 * check-all's Werror leg); before 6.12 there is only QPoint, and QPointF
 * doesn't convert to it. */
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
inline QPointF dragPos(const QPoint &p) { return QPointF(p); }
#else
inline QPoint dragPos(const QPoint &p) { return p; }
#endif

#endif
