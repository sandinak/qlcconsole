/*
  Q Light Controller Plus
  rigowner.cpp

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

#include <QCoreApplication>
#include <climits>

#include "fixturegroup.h"
#include "rigowner.h"
#include "fixture.h"
#include "doc.h"

QString RigOwner::xmlName(Owner owner)
{
    switch (owner)
    {
        case Tour:   return QStringLiteral("Tour");
        case House:  return QStringLiteral("House");
        case Rental: return QStringLiteral("Rental");
        case Unset:  break;
    }
    return QString();
}

RigOwner::Owner RigOwner::fromXmlName(const QString &text)
{
    const QString t = text.trimmed();
    if (t.compare(QLatin1String("Tour"), Qt::CaseInsensitive) == 0)
        return Tour;
    if (t.compare(QLatin1String("House"), Qt::CaseInsensitive) == 0)
        return House;
    if (t.compare(QLatin1String("Rental"), Qt::CaseInsensitive) == 0)
        return Rental;
    return Unset;
}

QString RigOwner::displayName(Owner owner)
{
    switch (owner)
    {
        case Tour:   return QCoreApplication::translate("RigOwner", "Tour");
        case House:  return QCoreApplication::translate("RigOwner", "House");
        case Rental: return QCoreApplication::translate("RigOwner", "Rental");
        case Unset:  break;
    }
    return QCoreApplication::translate("RigOwner", "Not set");
}

QList<RigOwner::Owner> RigOwner::owners()
{
    return { Tour, House, Rental };
}

RigOwner::Owner RigOwner::effective(const Doc *doc, quint32 fid, quint32 *fromGroup)
{
    if (fromGroup != nullptr)
        *fromGroup = UINT_MAX;
    const Fixture *fx = doc != nullptr ? doc->fixture(fid) : nullptr;
    if (fx == nullptr)
        return Tour;
    if (fx->owner() != Unset)
        return fx->owner();
    // fixtureGroups() is in id order: the first group that says wins.
    for (const FixtureGroup *grp : doc->fixtureGroups())
    {
        if (grp->owner() == Unset || !grp->fixtureList().contains(fid))
            continue;
        if (fromGroup != nullptr)
            *fromGroup = grp->id();
        return grp->owner();
    }
    return Tour;
}

QHash<quint32, RigOwner::Owner> RigOwner::effectiveAll(const Doc *doc)
{
    QHash<quint32, Owner> out;
    if (doc == nullptr)
        return out;
    QHash<quint32, Owner> byGroup;
    for (const FixtureGroup *grp : doc->fixtureGroups())
    {
        if (grp->owner() == Unset)
            continue;
        for (quint32 fid : grp->fixtureList())
            if (!byGroup.contains(fid))
                byGroup.insert(fid, grp->owner());
    }
    for (const Fixture *fx : doc->fixtures())
    {
        if (fx->owner() != Unset)
            out.insert(fx->id(), fx->owner());
        else
            out.insert(fx->id(), byGroup.value(fx->id(), Tour));
    }
    return out;
}
