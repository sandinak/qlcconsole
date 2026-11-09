/*
  Q Light Controller Plus
  fixtureglass.h

  The glass a unit actually carries. A tech loads a gobo into a slot ("the
  moon in AT-2 3's slot 3") and names it in Gobo Wheels…; that name belongs
  to THAT unit's slot, not to its fixture definition. Everything that names
  a gobo slot for a fixture -- the Look Editor, the channel console, Show
  Review, the Gobo Wheels dialog -- asks here, so the unit's own name shows
  wherever its slot does, and a Gobo look called "Moon" finds the slot each
  unit has it in (QLCPalette::resolveGobo()).

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef FIXTUREGLASS_H
#define FIXTUREGLASS_H

#include <QString>
#include <QStringList>

class QLCCapability;
class Fixture;
class Doc;

class FixtureGlass
{
public:
    /** The Doc @p fx belongs to (its parent), or nullptr. */
    static const Doc *docOf(const Fixture *fx);

    /** What the glass loaded in @p cap's slot of unit @p fid's channel
     *  @p channel is called; empty when nothing named is loaded there. */
    static QString loadedName(const Doc *doc, quint32 fid, quint32 channel, const QLCCapability *cap);
    /** The same, for the slot that DMX @p value falls in. */
    static QString loadedName(const Doc *doc, quint32 fid, quint32 channel, uchar value);

    /** What to call that slot on that unit: the loaded glass's name, else the
     *  definition's capability name. */
    static QString slotName(const Doc *doc, quint32 fid, quint32 channel, const QLCCapability *cap);
    static QString slotName(const Doc *doc, quint32 fid, quint32 channel, uchar value);

    /** The value (the slot's middle) of the first slot on @p fx's channel
     *  @p channel loaded with glass called @p name (QLCPalette::sameGoboName:
     *  case, spaces, '-' and '_' ignored); -1 when it carries none. */
    static int loadedValue(const Doc *doc, const Fixture *fx, quint32 channel, const QString &name);

    /** Every loaded glass name on @p fid, each once, in slot order. */
    static QStringList loadedNames(const Doc *doc, quint32 fid);
};

#endif
