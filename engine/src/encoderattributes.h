/*
  Q Light Controller Plus
  encoderattributes.h

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

#ifndef ENCODERATTRIBUTES_H
#define ENCODERATTRIBUTES_H

#include <QList>

#include "qlcengine_export.h"

class Scene;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * What an encoder bank's slot (EncoderBank: Colour ▸ Hue, Position ▸ Tilt...)
 * means on a selection of fixtures: which of each fixture's channels it is,
 * and the edit of a scene's values it makes. The Workbench's channel console
 * path (the fixtures and groups of a scene, a layer of the Show Tree): the
 * Look Editor edits a look through its own controls instead.
 *
 * A turn moves every selected fixture by the same amount from where it is,
 * so a mixed selection keeps its differences (a console's relative
 * behaviour). Values clamp at the ends (hue wraps). Channels a look of the
 * scene drives are left alone -- the look wins there, as it does on the
 * output -- and adjust() says so by returning false when nothing could be
 * written.
 */
namespace EncoderAttributes
{
    /** The normalised step of one detent (coarse, or @a fine). */
    QLC_ENGINE_EXPORT float step(int bank, int slot, bool fine);

    /** Whether any of @a fixtures has what @a slot of @a bank means. */
    QLC_ENGINE_EXPORT bool available(Doc *doc, const QList<quint32> &fixtures, int bank, int slot);

    /** Where @a slot is, 0-1, read from @a scene on the first fixture that
     *  has it (a hue is its place round the wheel). False: no fixture has
     *  it. */
    QLC_ENGINE_EXPORT bool read(Doc *doc, Scene *scene, const QList<quint32> &fixtures,
                                int bank, int slot, float *norm);

    /** Turn @a slot of @a bank by @a detents (signed; @a fine: the small
     *  step) on every fixture of @a fixtures, in @a scene's values. True:
     *  something was written (or already at its end); false: no fixture
     *  has it, or a look drives it. */
    QLC_ENGINE_EXPORT bool adjust(Doc *doc, Scene *scene, const QList<quint32> &fixtures,
                                  int bank, int slot, int detents, bool fine);

    /** @a slot back to its default (a tap on the encoder). */
    QLC_ENGINE_EXPORT bool home(Doc *doc, Scene *scene, const QList<quint32> &fixtures,
                                int bank, int slot);

    /** Whether a look of @a scene drives channel @a channel of @a fixture
     *  (an edit of its raw value would show nothing). */
    QLC_ENGINE_EXPORT bool drivenByLook(Doc *doc, Scene *scene, quint32 fixture, quint32 channel);
}

/** @} */

#endif
