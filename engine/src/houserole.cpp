/*
  Q Light Controller Plus
  houserole.cpp

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
#include <QRegularExpression>
#include <QColor>
#include <algorithm>

#include "houserole.h"
#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "monitorproperties.h"
#include "measurementunits.h"
#include "venuecorrections.h"
#include "qlcfixturedef.h"
#include "fixturegroup.h"
#include "eosimporter.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "venuerig.h"
#include "fixture.h"
#include "doc.h"

using namespace HouseRole;

namespace
{
QString tr(const char *text)
{
    return QCoreApplication::translate("HouseRole", text);
}

VenueCorrections *venuesOf(const Doc *doc)
{
    return doc != nullptr && doc->programmer() != nullptr ? doc->programmer()->venueCorrections() : nullptr;
}

/* The stage words (section 3.5, rule 1), after the house ones. */
Role stageWord(const QString &label, QString *word)
{
    static const QRegularExpression nonLetters(QStringLiteral("[^a-z]+"));
    const QStringList words = label.toLower().split(nonLetters, Qt::SkipEmptyParts);
    for (int i = 0; i < words.count(); ++i)
    {
        const QString &w = words.at(i);
        const QString next = words.value(i + 1);
        Role r = None;
        if (w == QLatin1String("foh") || w == QLatin1String("front") || w == QLatin1String("balcony")
            || (w == QLatin1String("box") && next == QLatin1String("boom")))
            r = Front;
        else if (w == QLatin1String("top") || w == QLatin1String("down") || w == QLatin1String("downlight"))
            r = Top;
        else if (w == QLatin1String("side") || w == QLatin1String("boom") || w == QLatin1String("booms")
                 || w == QLatin1String("shin") || w == QLatin1String("shins") || w == QLatin1String("ladder"))
            r = Side;
        else if (w == QLatin1String("back") || w == QLatin1String("backlight"))
            r = Back;
        else if (w == QLatin1String("cyc") || w == QLatin1String("cyclorama"))
            r = Cyc;
        if (r != None)
        {
            if (word != nullptr)
                *word = w == QLatin1String("foh") ? QStringLiteral("FOH") : w.at(0).toUpper() + w.mid(1);
            return r;
        }
    }
    return None;
}

/* The house words (EosCircuit::classify): a role, or none with a reason;
   false when the label says nothing of the kind. */
bool houseWord(const QString &label, bool singleChannel, Role &role, QString &why, const QString &where)
{
    static const QRegularExpression aisle(QStringLiteral("\\b(aisle|step)s?\\b"),
                                          QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression spare(QStringLiteral("\\bspares?\\b"), QRegularExpression::CaseInsensitiveOption);
    if (aisle.match(label).hasMatch())
    {
        role = Aisle;
        why = tr("\"Aisle\" in %1").arg(where);
        return true;
    }
    switch (EosCircuit::classify(label, singleChannel))
    {
    case EosCircuit::House:
        role = HouseLights;
        why = tr("\"House\" in %1").arg(where);
        return true;
    case EosCircuit::Work:
        role = Work;
        why = tr("\"Work\" in %1").arg(where);
        return true;
    case EosCircuit::Backstage:
        role = None;
        why = tr("a running light (%1)").arg(where);
        return true;
    case EosCircuit::Relay:
        role = None;
        why = tr("a power relay (%1)").arg(where);
        return true;
    case EosCircuit::Normal:
        break;
    }
    if (spare.match(label).hasMatch())
    {
        role = None;
        why = tr("a spare (%1)").arg(where);
        return true;
    }
    return false;
}

bool isMover(const Fixture *fx)
{
    bool pan = false, tilt = false;
    for (quint32 c = 0; c < fx->channels(); ++c)
    {
        const QLCChannel *ch = fx->channel(c);
        if (ch == nullptr)
            continue;
        pan = pan || ch->group() == QLCChannel::Pan;
        tilt = tilt || ch->group() == QLCChannel::Tilt;
    }
    return pan && tilt;
}

bool onNotOurs(const Fixture *fx, const QList<VenueRig::NotOurs> &notOurs)
{
    const int first = int(fx->address());
    const int last = first + int(fx->channels()) - 1;
    for (const VenueRig::NotOurs &n : notOurs)
    {
        if (n.universe != fx->universe())
            continue;
        if (n.whole() || (first <= n.to && last >= n.from))
            return true;
    }
    return false;
}

QString paletteFolder()
{
    return QStringLiteral("Palettes/%1/").arg(folder());
}
} // namespace

/****************************************************************************
 * The roles
 ****************************************************************************/

QList<Role> HouseRole::roles()
{
    return { HouseLights, Aisle, Front, Top, Side, Back, Cyc, Work };
}

QString HouseRole::xmlName(Role role)
{
    switch (role)
    {
    case HouseLights: return QStringLiteral("House");
    case Aisle:       return QStringLiteral("Aisle");
    case Front:       return QStringLiteral("Front");
    case Top:         return QStringLiteral("Top");
    case Side:        return QStringLiteral("Side");
    case Back:        return QStringLiteral("Back");
    case Cyc:         return QStringLiteral("Cyc");
    case Work:        return QStringLiteral("Work");
    case None:        break;
    }
    return QString();
}

Role HouseRole::fromXmlName(const QString &text)
{
    const QString t = text.trimmed();
    for (Role r : roles())
        if (t.compare(xmlName(r), Qt::CaseInsensitive) == 0)
            return r;
    return None;
}

QString HouseRole::groupName(Role role)
{
    switch (role)
    {
    case HouseLights: return tr("House Lights");
    case Aisle:       return tr("Aisle Lights");
    case Front:       return tr("Front Wash");
    case Top:         return tr("Top Wash");
    case Side:        return tr("Side Light");
    case Back:        return tr("Back Light");
    case Cyc:         return tr("Cyc");
    case Work:        return tr("Work Lights");
    case None:        break;
    }
    return QString();
}

QString HouseRole::folder()
{
    return tr("Roles");
}

FixtureGroup *HouseRole::group(const Doc *doc, Role role)
{
    if (doc == nullptr || role == None)
        return nullptr;
    for (FixtureGroup *g : doc->fixtureGroups())
        if (g->role() == role)
            return g;
    return nullptr;
}

bool HouseRole::hasRoles(const Doc *doc)
{
    if (doc == nullptr)
        return false;
    for (const FixtureGroup *g : doc->fixtureGroups())
        if (g->role() != None)
            return true;
    return false;
}

Role HouseRole::roleOf(const Doc *doc, quint32 groupId)
{
    const FixtureGroup *g = doc != nullptr ? doc->fixtureGroup(groupId) : nullptr;
    return g != nullptr ? g->role() : None;
}

/****************************************************************************
 * Suggesting fills
 ****************************************************************************/

QList<Suggestion> HouseRole::suggest(const Doc *doc)
{
    QList<Suggestion> out;
    VenueCorrections *vc = venuesOf(doc);
    if (vc == nullptr || !vc->rig()->isApplied())
        return out;
    const VenueCorrections::Venue v = vc->venue(vc->activeVenue());
    QList<quint32> ids = vc->rig()->venueFixtures().values();
    std::sort(ids.begin(), ids.end());
    const MonitorProperties *props = doc->monitorProperties();
    const float curtain = props != nullptr ? props->curtainLineY() : 0.0f;
    static const double kFrontBeyond = 914.4;   // 3 ft downstage of the curtain line
    static const double kOverhead = 3048.0;     // 10 ft up

    for (quint32 id : ids)
    {
        const Fixture *fx = doc->fixture(id);
        if (fx == nullptr)
            continue;
        Suggestion s;
        s.fixture = id;

        // Never suggested: not ours, unpatched, or already in a role by hand.
        if (onNotOurs(fx, v.rig.notOurs))
        {
            s.why = tr("on channels the room keeps");
            out << s;
            continue;
        }
        if (!fx->isPatched())
        {
            s.why = tr("unpatched (a clash)");
            out << s;
            continue;
        }
        bool inRole = false;
        for (const FixtureGroup *g : doc->fixtureGroups())
            if (g->role() != None && g->fixtureList().contains(id))
            {
                s.role = g->role();
                s.why = tr("in %1 already").arg(g->name());
                inRole = true;
                break;
            }
        if (inRole)
        {
            out << s;
            continue;
        }

        // 1. Words: its name, else its groups' (the house words first).
        QList<QPair<QString, QString>> labels;     // the label, where it is
        labels << qMakePair(fx->name(), tr("its name"));
        for (const FixtureGroup *g : doc->fixtureGroups())
            if (g->role() == None && g->fixtureList().contains(id))
                labels << qMakePair(g->name(), tr("its group %1").arg(g->name()));
        for (const VenueRig::Fill &g : v.rig.room.groups)
            for (const VenueRig::FillHead &h : g.heads)
                if (h.fixture == id)
                {
                    labels << qMakePair(g.groupName, tr("its group %1").arg(g.groupName));
                    break;
                }
        bool decided = false;
        for (const auto &l : labels)
            if (houseWord(l.first, fx->channels() == 1, s.role, s.why, l.second))
            {
                decided = true;
                break;
            }
        for (int i = 0; !decided && i < labels.count(); ++i)
        {
            QString word;
            const Role r = stageWord(labels.at(i).first, &word);
            if (r != None)
            {
                s.role = r;
                s.why = tr("\"%1\" in %2").arg(word, labels.at(i).second);
                decided = true;
            }
        }
        if (decided)
        {
            out << s;
            continue;
        }

        // 2. A moving light: by hand.
        if (isMover(fx))
        {
            s.why = tr("a moving light: add it by hand");
            out << s;
            continue;
        }
        // 3. A cyc light, by its model.
        const QLCFixtureDef *def = fx->fixtureDef();
        if (def != nullptr && def->model().contains(QLatin1String("CYC"), Qt::CaseInsensitive))
        {
            s.role = Cyc;
            s.why = tr("a cyc light (%1)").arg(def->model());
            out << s;
            continue;
        }
        // 4. Where it hangs, in the show's frame (the room lined up).
        if (props != nullptr && (props->containsFixture(id) || props->hasFixtureRigProps(id)))
        {
            const QVector3D at = props->fixtureRigPosition(id);
            const double downstage = (double(at.y()) - double(curtain)) * 1000.0;
            const double up = double(at.z()) * 1000.0;
            if (downstage > kFrontBeyond)
            {
                s.role = Front;
                s.why = tr("%1 downstage of the curtain line").arg(MeasurementUnits::formatLength(downstage));
            }
            else if (up > kOverhead)
            {
                s.role = Top;
                s.why = tr("over the stage, %1 up").arg(MeasurementUnits::formatLength(up));
            }
            else
            {
                s.role = Side;
                s.why = tr("%1 up, beside the stage").arg(MeasurementUnits::formatLength(up));
            }
            out << s;
            continue;
        }
        s.why = tr("nothing says what it lights");
        out << s;
    }
    return out;
}

QList<Role> HouseRole::nameClashes(const Doc *doc)
{
    QList<Role> out;
    if (doc == nullptr)
        return out;
    for (Role r : roles())
    {
        if (group(doc, r) != nullptr)
            continue;
        for (const FixtureGroup *g : doc->fixtureGroups())
            if (g->role() == None && g->name().compare(groupName(r), Qt::CaseInsensitive) == 0)
            {
                out << r;
                break;
            }
    }
    return out;
}

/****************************************************************************
 * The standard cues
 ****************************************************************************/

QList<StandardCue> HouseRole::standardCues()
{
    return {
        { tr("House up"), { { HouseLights, 255 }, { Aisle, 255 } }, 5000 },
        { tr("House to half"), { { HouseLights, 128 } }, 5000 },
        { tr("House out"), { { HouseLights, 0 } }, 5000 },
        { tr("Stage up"), { { Front, 255 }, { Top, 255 }, { Side, 255 }, { Back, 255 } }, 3000 },
        { tr("Work lights"), { { Work, 255 } }, 0 },
        { tr("Work lights off"), { { Work, 0 } }, 0 },
    };
}

QString HouseRole::hitFolder()
{
    return tr("House");
}

namespace
{
/* A Dimmer palette at @p level, or Colour Open White (@p level < 0): an
   existing one with exactly that value, else a new one in Roles. */
quint32 rolePalette(Doc *doc, int level, int *made)
{
    const bool white = level < 0;
    for (QLCPalette *p : doc->palettes())
    {
        if (p->name().isEmpty())
            continue;
        if (white && p->type() == QLCPalette::Color && p->rgbValue() == QColor(Qt::white)
            && (!p->wauvValue().isValid() || p->wauvValue() == QColor(255, 0, 0)))
            return p->id();
        if (!white && p->type() == QLCPalette::Dimmer && p->values().size() == 1 && p->value().toInt() == level)
            return p->id();
    }
    QLCPalette *p = new QLCPalette(white ? QLCPalette::Color : QLCPalette::Dimmer, doc);
    if (white)
    {
        p->setName(tr("Open White"));
        p->setValue(QStringLiteral("#ffffff"));
    }
    else
    {
        p->setName(level >= 255 ? tr("Full") : level <= 0 ? tr("Out") : tr("%1%").arg(qRound(level * 100.0 / 255.0)));
        p->setValue(level);
    }
    p->setPath(paletteFolder());
    if (!doc->addPalette(p))
    {
        delete p;
        return QLCPalette::invalidId();
    }
    ++*made;
    return p->id();
}

const ShowTree::ReusableCue *reusableNamed(const ShowTreeController *st, const QString &name)
{
    for (const ShowTree::ReusableCue &r : st->reusableCues())
        if (r.name.compare(name, Qt::CaseInsensitive) == 0)
            return &r;
    return nullptr;
}
} // namespace

Made HouseRole::ensure(Doc *doc, const QSet<Role> &useExisting)
{
    Made made;
    if (doc == nullptr)
        return made;

    // The role groups.
    for (Role r : roles())
    {
        if (group(doc, r) != nullptr)
            continue;
        FixtureGroup *existing = nullptr;
        for (FixtureGroup *g : doc->fixtureGroups())
            if (g->role() == None && g->name().compare(groupName(r), Qt::CaseInsensitive) == 0)
                existing = g;
        if (existing != nullptr && useExisting.contains(r))
        {
            existing->setRole(r);
            ++made.groups;
            continue;
        }
        FixtureGroup *g = new FixtureGroup(doc);
        g->setName(existing != nullptr ? tr("%1 (role)").arg(groupName(r)) : groupName(r));
        g->setPath(folder());
        g->setRole(r);
        if (!doc->addFixtureGroup(g))
        {
            delete g;
            continue;
        }
        ++made.groups;
    }

    // The palettes: Dimmer at each level the cues use, and Open White.
    QMap<int, quint32> dimmers;
    for (const StandardCue &c : standardCues())
        for (const Layer &l : c.layers)
            if (!dimmers.contains(l.level))
                dimmers.insert(l.level, rolePalette(doc, l.level, &made.palettes));
    const quint32 white = rolePalette(doc, -1, &made.palettes);

    // The standard cues and their Global hits: one undo step in the tree.
    ShowTreeController *st = doc->showTree();
    if (st == nullptr)
        return made;
    st->beginBatch(tr("Fill Roles"));
    for (const StandardCue &c : standardCues())
    {
        quint32 rid = ShowTree::invalidId();
        if (const ShowTree::ReusableCue *have = reusableNamed(st, c.name))
            rid = have->id;
        else
        {
            ShowTree::ReusableCue def;
            def.name = c.name;
            def.fadeIn = c.fade;
            def.fadeOut = c.fade;
            for (const Layer &l : c.layers)
            {
                const FixtureGroup *g = group(doc, l.role);
                if (g == nullptr)
                    continue;
                ShowTree::Layer layer;
                layer.target.groups << g->id();
                layer.setLook(st->lookForPalette(dimmers.value(l.level)));
                layer.setLook(st->lookForPalette(white));
                def.layers << layer;
            }
            rid = st->addReusableCue(def);
            if (rid == ShowTree::invalidId())
                continue;
            ++made.cues;
        }

        // Its hit, unless it has one (anywhere) already.
        bool hasHit = false;
        for (const ShowTreeController::ReusableUse &u : st->reusableUses(rid))
            hasHit = hasHit || st->isLooks(u.list);
        if (hasHit)
            continue;
        quint32 prod = st->currentProduction();
        if (st->production(prod) == nullptr)
            prod = st->productions().isEmpty() ? ShowTree::invalidId() : st->productions().first().id;
        if (prod == ShowTree::invalidId())
            continue;       // no production yet: the next Fill Roles makes them
        const quint32 section = st->addLooksSection(prod);
        const ShowTree::CueList *hits = st->cueList(section);
        if (hits == nullptr)
            continue;
        const quint32 hit = st->insertReusableCue(section, int(hits->cues.count()) - 1, rid);
        if (hit == ShowTree::invalidId())
            continue;
        st->setLookFolder({ hit }, hitFolder());       // filed: a production hit...
        st->setHitScope({ hit }, ShowTree::HitScope::Global, prod);    // ...then everywhere
        st->setHitsOverride({ hit }, true);
        ++made.hits;
    }
    st->endBatch(true);
    return made;
}

bool HouseRole::fillRoles(Doc *doc, const QString &venue, const QMap<Role, QList<quint32>> &rows,
                          const QSet<Role> &useExisting, Made *made, QString *why)
{
    VenueCorrections *vc = venuesOf(doc);
    if (vc == nullptr || venue.isEmpty() || vc->activeVenue() != venue || !vc->rig()->isApplied())
    {
        if (why != nullptr)
            *why = tr("Apply %1 first: its lights fill the roles while it is applied.").arg(venue);
        return false;
    }
    const Made m = ensure(doc, useExisting);
    if (made != nullptr)
        *made = m;

    const QSet<quint32> house = vc->rig()->venueFixtures();
    QHash<quint32, Role> wanted;
    for (auto it = rows.constBegin(); it != rows.constEnd(); ++it)
        for (quint32 fid : it.value())
            if (house.contains(fid) && doc->fixture(fid) != nullptr)
                wanted.insert(fid, it.key());
    for (auto it = rows.constBegin(); it != rows.constEnd(); ++it)
    {
        FixtureGroup *g = group(doc, it.key());
        if (g == nullptr)
            continue;
        // This venue's lights in it become exactly the row's; tour heads stay.
        for (quint32 fid : g->fixtureList())
            if (house.contains(fid) && wanted.value(fid, None) != it.key())
                g->resignFixture(fid);
        for (quint32 fid : it.value())
            if (wanted.value(fid, None) == it.key() && !g->fixtureList().contains(fid))
                g->assignFixture(fid);
    }
    // A light moved to another row leaves the role it was in.
    for (Role r : roles())
    {
        if (rows.contains(r))
            continue;
        if (FixtureGroup *g = group(doc, r))
            for (quint32 fid : g->fixtureList())
                if (wanted.contains(fid))
                    g->resignFixture(fid);
    }
    return vc->recordRig(why);
}

/****************************************************************************
 * Saying so on screen
 ****************************************************************************/

QString HouseRole::emptyText(const Doc *doc, const QList<quint32> &groups)
{
    if (doc == nullptr || groups.isEmpty())
        return QString();
    bool house = false;
    for (quint32 gid : groups)
    {
        const FixtureGroup *g = doc->fixtureGroup(gid);
        if (g == nullptr || g->role() == None || !g->fixtureList().isEmpty())
            return QString();
        house = house || g->role() == HouseLights;
    }
    VenueCorrections *vc = venuesOf(doc);
    const QString venue = vc != nullptr ? vc->activeVenue() : QString();
    if (venue.isEmpty())
        return tr("none in this show");
    if (house && !vc->venue(venue).rig.notOurs.isEmpty())
        return tr("none in %1: call the house desk").arg(venue);
    return tr("none in %1").arg(venue);
}
