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

    /** The LED grid @p mode's definition declares, when it holds EXACTLY
     *  this mode's heads; else an invalid size. A definition declares one
     *  grid for all its modes: the Oppsk wall washer's 8 x 4 is its 32-head
     *  64ch mode, and poured into its 16-head 48ch RGB mode it folded a
     *  one-metre bar into two rows ("fits" was the test, with room to spare).
     *  1 x 1 is the unset default, not a grid. */
    static QSize declaredGrid(const QLCFixtureMode *mode);

    /** The grid in head order: the definition's grid for this mode
     *  (declaredGrid()) filled row by row, or a single row when it has none.
     *  What everything assumed before there was a map. */
    static HeadLayout headOrder(const Fixture *fx);

    /** The grid with each ROW holding one kind of head (@p rowKinds, top to
     *  bottom, e.g. RGB, W, RGB, W, RGB): the heads of each kind, in head
     *  order, fill the rows of that kind left to right. Invalid when the rows
     *  cannot hold exactly the heads there are of each kind. */
    static HeadLayout fromRowPattern(const Fixture *fx, const QStringList &rowKinds);
    /** The same, on a grid @p cols wide (0: the model's own width, as
     *  headOrder() has it) -- for a strip laid out by hand in rows. */
    static HeadLayout fromRowPattern(const Fixture *fx, const QStringList &rowKinds, int cols);

    /** A row pattern as typed: "RGB, W, RGB, W, RGB", "RGB/W/RGB/W/RGB" or
     *  "rgb w rgb w rgb" -> RGB, W, RGB, W, RGB. Each word is matched
     *  without case to one of @p known (the model's kinds, kept as they are
     *  spelled there); "White", "Amber" and "Dimmer" are read as W, A and I.
     *  A word that matches none is kept as typed, for the caller to refuse. */
    static QStringList parseRowKinds(const QString &text, const QStringList &known = QStringList());

    /** Mirrored left to right (the fixture seen from behind). */
    HeadLayout flippedHorizontally() const;
    /** Mirrored top to bottom (the fixture hung upside down). */
    HeadLayout flippedVertically() const;
    /** The same heads on a grid of @p size: each keeps its cell where the
     *  grid still has it, the rest take the free cells in head order.
     *  Invalid when the grid has fewer cells than there are heads. */
    HeadLayout resized(const QSize &size) const;

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

    /** A head's TYPE, for making a group per type of head: its kind (above)
     *  plus "/PT" when it moves -- pan or tilt in the head, or in the
     *  fixture outside any head (a mover's one body moves all its heads).
     *  "PT" for a head that only moves; "Ring" for one whose only colour
     *  is a colour wheel/macro channel on a fixture that moves (a mover's
     *  colour ring), "Macro" for such a head on one that doesn't; "Other"
     *  for one with no colour, intensity or movement. Two heads are the same type when these match,
     *  whatever fixture model they are on. Empty only for no such head. */
    QString typeOf(const QLCFixtureMode *mode, int head);
    /** The distinct types of @p fx's heads, in the order they first appear. */
    QStringList typesOf(const Fixture *fx);
    /** A type said plainly, for a group's name: "RGB", "White", "Dimmer",
     *  "RGBW movers", "Movers" (a mover with only a dimmer, too), "Ring",
     *  "Colour macro"... */
    QString typeName(const QString &type);
}

namespace HeadLayoutLibrary
{
    /** The operator's own maps, one JSON file per model and mode:
     *  HeadLayouts in the per-platform user folder (~/Library/Application
     *  Support/qlcconsole on macOS, %USERPROFILE%\qlcconsole on Windows,
     *  ~/.qlcconsole on Linux). */
    QDir userDirectory();
    /** Point userDirectory() somewhere else (tests); empty restores it. */
    void setUserDirectoryOverride(const QString &path);

    /** The library's map for @p key, or an invalid one when it has none. */
    HeadLayout load(const QString &key);
    /** Keep @p layout in the library (replacing any for its key). */
    bool save(const HeadLayout &layout);
    /** The file name a map for @p key is kept under ("Make|Model|Mode"
     *  made safe for any file system, ".json"). */
    QString fileNameFor(const QString &key);
    /** @p layout as the library writes it. */
    QByteArray toJson(const HeadLayout &layout);
}

/** @} */

#endif
