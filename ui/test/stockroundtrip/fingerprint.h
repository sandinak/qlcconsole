/*
  Q Light Controller Plus - Unit test
  fingerprint.h

  The show's core data as lines of text, compiled twice: against
  qlcconsole's engine (what was saved) and against stock QLC+ 4.14's
  (what it read back). Only API both engines have.

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

#ifndef STOCKROUNDTRIP_FINGERPRINT_H
#define STOCKROUNDTRIP_FINGERPRINT_H

#include <QStringList>
#include <algorithm>

#include "doc.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "channelsgroup.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcpalette.h"
#include "inputoutputmap.h"
#include "function.h"
#include "scene.h"
#include "chaser.h"
#include "chaserstep.h"
#include "collection.h"
#include "efx.h"
#include "rgbmatrix.h"
#include "rgbalgorithm.h"
#include "sequence.h"
#include "show.h"
#include "track.h"
#include "script.h"

namespace StockRoundTrip
{

inline QString ids(const QList<quint32> &list)
{
    QStringList out;
    for (quint32 id : list)
        out << QString::number(id);
    return out.join(QLatin1Char(','));
}

/* @p skip: functions not to list (made by qlcconsole for what QLC+ drops). */
inline QStringList fingerprint(Doc *doc, const QList<quint32> &skip = QList<quint32>())
{
    QStringList out;
    InputOutputMap *io = doc->inputOutputMap();
    for (quint32 i = 0; i < io->universesCount(); i++)
        out << QStringLiteral("universe %1 '%2'").arg(i).arg(io->getUniverseNameByIndex(int(i)));

    foreach (Fixture *fx, doc->fixtures())
    {
        QString def = QStringLiteral("-");
        if (fx->fixtureDef() != nullptr && fx->fixtureMode() != nullptr)
            def = fx->fixtureDef()->manufacturer() + QLatin1Char('/') + fx->fixtureDef()->model()
                  + QLatin1Char('/') + fx->fixtureMode()->name();
        out << QStringLiteral("fixture %1 '%2' %3 u%4 a%5 ch%6 heads%7")
                   .arg(fx->id()).arg(fx->name(), def).arg(fx->universe()).arg(fx->address())
                   .arg(fx->channels()).arg(fx->heads());
    }
    foreach (FixtureGroup *g, doc->fixtureGroups())
    {
        QStringList heads;
        const auto map = g->headsMap();
        for (auto it = map.constBegin(); it != map.constEnd(); ++it)
            heads << QStringLiteral("%1,%2=%3.%4").arg(it.key().x()).arg(it.key().y())
                         .arg(it.value().fxi).arg(it.value().head);
        out << QStringLiteral("group %1 '%2' %3x%4 [%5]").arg(g->id()).arg(g->name())
                   .arg(g->size().width()).arg(g->size().height()).arg(heads.join(QLatin1Char(' ')));
    }
    foreach (ChannelsGroup *g, doc->channelsGroups())
    {
        QStringList chans;
        foreach (SceneValue v, g->getChannels())
            chans << QStringLiteral("%1.%2").arg(v.fxi).arg(v.channel);
        out << QStringLiteral("channelsgroup %1 '%2' [%3]").arg(g->id()).arg(g->name(), chans.join(QLatin1Char(' ')));
    }
    foreach (QLCPalette *p, doc->palettes())
    {
        QStringList vals;
        foreach (QVariant v, p->values())
            vals << v.toString();
        out << QStringLiteral("palette %1 '%2' type%3 [%4]").arg(p->id()).arg(p->name())
                   .arg(int(p->type())).arg(vals.join(QLatin1Char(',')));
    }
    foreach (Function *f, doc->functions())
    {
        if (skip.contains(f->id()))
            continue;
        QString line = QStringLiteral("function %1 %2 '%3' path '%4'").arg(f->id())
                           .arg(Function::typeToString(f->type()), f->name(), f->path(true));
        if (Scene *s = qobject_cast<Scene *>(f))
        {
            QStringList vals;
            foreach (SceneValue v, s->values())
                vals << QStringLiteral("%1.%2=%3").arg(v.fxi).arg(v.channel).arg(v.value);
            std::sort(vals.begin(), vals.end());
            line += QStringLiteral(" values [%1]").arg(vals.join(QLatin1Char(' ')));
        }
        else if (Sequence *q = qobject_cast<Sequence *>(f))
            line += QStringLiteral(" scene %1 steps %2").arg(q->boundSceneID()).arg(q->stepsCount());
        else if (Chaser *c = qobject_cast<Chaser *>(f))
        {
            QStringList steps;
            foreach (ChaserStep st, c->steps())
                steps << QStringLiteral("%1/%2/%3/%4").arg(st.fid).arg(st.fadeIn).arg(st.hold).arg(st.fadeOut);
            line += QStringLiteral(" steps [%1]").arg(steps.join(QLatin1Char(' ')));
        }
        else if (Collection *c = qobject_cast<Collection *>(f))
            line += QStringLiteral(" members [%1]").arg(ids(c->functions()));
        else if (EFX *e = qobject_cast<EFX *>(f))
            line += QStringLiteral(" algorithm %1 fixtures %2 size %3")
                        .arg(EFX::algorithmToString(e->algorithm())).arg(e->fixtures().count()).arg(e->width());
        else if (RGBMatrix *m = qobject_cast<RGBMatrix *>(f))
            line += QStringLiteral(" group %1 algorithm '%2'").arg(m->fixtureGroup())
                        .arg(m->algorithm() != nullptr ? m->algorithm()->name() : QStringLiteral("-"));
        else if (Show *sh = qobject_cast<Show *>(f))
        {
            QStringList tracks;
            foreach (Track *t, sh->tracks())
                tracks << QStringLiteral("%1:%2").arg(t->name()).arg(t->showFunctions().count());
            line += QStringLiteral(" tracks [%1]").arg(tracks.join(QLatin1Char(' ')));
        }
        else if (Script *sc = qobject_cast<Script *>(f))
            line += QStringLiteral(" lines %1").arg(sc->dataLines().count());
        out << line;
    }
    return out;
}

}

#endif
