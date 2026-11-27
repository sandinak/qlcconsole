/*
  Q Light Controller Plus - qlcconsole
  surfacechips.h

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

#ifndef SURFACECHIPS_H
#define SURFACECHIPS_H

#include <QWidget>
#include <QMap>

#include "qlcui_export.h"

class QToolButton;
class QMenu;
class QHBoxLayout;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The footer's chips for the patched surfaces (Q311): one per universe
 * whose input is a surface with a drawn map (SurfaceMaps::
 * surfaceUniverses()), named as its type is ("APC40", "PMJ"); a click
 * opens its map in Connections ▸ Control surfaces. A board with surface
 * pages says the page on now ("APC40 · Move"). A chip as the footer's
 * others are (DeskChip: a flat QToolButton, bold, 6 px padding). Hidden
 * while there is none.
 */
class QLC_UI_EXPORT SurfaceChips final : public QWidget
{
    Q_OBJECT

public:
    explicit SurfaceChips(Doc *doc, QWidget *parent = nullptr);

    /** The chip of @p universe; null when it has none. */
    QToolButton *chip(quint32 universe) const { return m_chips.value(universe); }
    QList<quint32> universes() const { return m_chips.keys(); }
    /** Each chip's height (the footer's line). */
    void setChipHeight(int h);
    /** The menu that picks a page of @p universe's board (the page name on
     *  the chip, Q356); null when the board has no pages. The caller owns it. */
    QMenu *pageMenu(quint32 universe);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

public slots:
    void rebuild();

signals:
    /** Open @p universe's map. */
    void mapRequested(quint32 universe);

private:
    Doc *m_doc;
    QHBoxLayout *m_box;
    QMap<quint32, QToolButton *> m_chips;
    /** Whether widget point @p x of @p universe's chip is on the page name. */
    bool onPageName(quint32 universe, int x) const;
    int m_height = -1;
    bool m_queued = false;
};

/** @} */

#endif
