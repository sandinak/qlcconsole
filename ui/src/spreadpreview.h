/*
  Q Light Controller Plus
  spreadpreview.h

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

#ifndef SPREADPREVIEW_H
#define SPREADPREVIEW_H

#include <QWidget>
#include <memory>

#include "effectinstance.h"

class Doc;

/** @addtogroup ui_functions
 * @{
 */

/**
 * The look editor's picture of a spread (SPEED_SPREAD_DESIGN.md, S3): the
 * look's heads on the floor plan (or their head-layout cells when nothing is
 * placed), each coloured by its phase -- the same hue at the same place in
 * the cycle -- and the target marked when the spread runs out from or round
 * it. update() after changing the settings.
 */
class SpreadPreview final : public QWidget
{
    Q_OBJECT

public:
    SpreadPreview(Doc *doc, quint32 sceneId, quint32 paletteId, QWidget *parent = nullptr);
    ~SpreadPreview();

    QSize sizeHint() const override { return QSize(260, 110); }

    /** Read the spread again and repaint. */
    void refresh();
    /** What it shows (tests). */
    const QList<EffectInstance::SpreadDot> &dots() const { return m_dots; }
    bool showsCentre() const { return m_hasCentre; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    std::unique_ptr<EffectInstance> m_instance;
    QList<EffectInstance::SpreadDot> m_dots;
    QPointF m_centre;
    bool m_hasCentre = false;
};

/** @} */

#endif
