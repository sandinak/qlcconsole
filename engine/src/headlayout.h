/*
  Q Light Controller Plus
  headlayout.h

  Where each head of a fixture really is, and what kind of head it is.

  A definition says how big its LED grid is (QLCPhysical::layoutSize(), e.g.
  15 x 5) but not which head sits in which cell, so everything that draws or
  groups heads fills the grid in head order. That is wrong for a fixture whose
  heads are numbered by kind: the Warmdance XL-450's 165-channel mode numbers
  its 45 RGB heads first and its 30 white ones after, while the bar itself is
  built RGB / W / RGB / W / RGB, a row of 15 each. A HeadLayout is that map --
  per manufacturer, model and mode -- kept in the operator's library and copied
  into a show that uses it (MonitorProperties), so the show still knows on
  another machine.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef HEADLAYOUT_H
#define HEADLAYOUT_H

#include <QStringList>
#include <QVector>
#include <QPoint>
#include <QSize>
#include <QDir>

class QLCFixtureMode;
class Fixture;

/** @addtogroup engine Engine
 * @{
 */

struct HeadLayout
{
    QString key;             ///< HeadLayout::keyFor(): manufacturer|model|mode
    QSize size;              ///< columns x rows
    QVector<QPoint> cells;   ///< per head index: its (column, row)

    bool isValid() const { return !key.isEmpty() && size.width() > 0 && size.height() > 0
                                  && !cells.isEmpty(); }
    /** The cell of @p head, or (-1,-1) when the map does not cover it. */
    QPoint cellOf(int head) const
    { return (head >= 0 && head < cells.size()) ? cells.at(head) : QPoint(-1, -1); }

    bool operator==(const HeadLayout &o) const
    { return key == o.key && size == o.size && cells == o.cells; }

    /** "Manufacturer|Model|Mode" for @p fx, or empty for one with no real
     *  definition (a generic dimmer has nothing to map). */
    static QString keyFor(const Fixture *fx);

    /** The grid in head order: the definition's layout size filled row by
     *  row, or a single row when it declares none. What everything assumed
     *  before there was a map. */
    static HeadLayout headOrder(const Fixture *fx);

    /** The grid with each ROW holding one kind of head (@p rowKinds, top to
     *  bottom, e.g. RGB, W, RGB, W, RGB): the heads of each kind, in head
     *  order, fill the rows of that kind left to right. Invalid when the rows
     *  cannot hold exactly the heads there are of each kind. */
    static HeadLayout fromRowPattern(const Fixture *fx, const QStringList &rowKinds);

    /** The kind of each row of this map, read from the heads in it (empty
     *  for a row whose heads are of mixed kinds). For seeding a row pattern. */
    QStringList rowKinds(const Fixture *fx) const;
};

namespace HeadKinds
{
    /** What a head IS, from its channels' colours: "RGB", "W", "RGBW",
     *  "RGBAW+UV"... in a fixed order; "I" for plain intensity with no colour;
     *  empty when it has neither. */
    QString kindOf(const QLCFixtureMode *mode, int head);
    /** The distinct kinds of @p fx's heads, in the order they first appear. */
    QStringList kindsOf(const Fixture *fx);
    /** True when @p fx has heads of more than one kind (RGB and W, say). */
    bool isMixed(const Fixture *fx);
}

namespace HeadLayoutLibrary
{
    /** The operator's own maps, one JSON file per model and mode:
     *  ~/Library/Application Support/qlcconsole/HeadLayouts on macOS. */
    QDir userDirectory();
    /** Point userDirectory() somewhere else (tests); empty restores it. */
    void setUserDirectoryOverride(const QString &path);

    /** The library's map for @p key, or an invalid one when it has none. */
    HeadLayout load(const QString &key);
    /** Keep @p layout in the library (replacing any for its key). */
    bool save(const HeadLayout &layout);
}

/** @} */

#endif
