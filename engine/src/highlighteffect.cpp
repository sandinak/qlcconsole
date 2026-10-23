/*
  Q Light Controller Plus
  highlighteffect.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "highlighteffect.h"
#include "doc.h"
#include "fixture.h"
#include "qlcchannel.h"
#include "qlcfixturemode.h"
#include "fadechannel.h"

HighlightEffect::HighlightEffect(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
    , m_layer(doc, Universe::Highlight, FadeChannel::Override, QStringLiteral("Highlight"))
{
}

HighlightEffect::~HighlightEffect()
{
}

void HighlightEffect::setFixtures(const QList<quint32>& fixtureIds)
{
    QList<OutputLayer::Level> levels;
    for (const FixEntry &e : buildEntries(fixtureIds))
        for (const auto &w : e.writes)
            levels.append({ e.fixtureId, quint32(w.first), w.second });
    m_layer.set(levels);
}

QList<HighlightEffect::FixEntry>
HighlightEffect::buildEntries(const QList<quint32>& fixtureIds) const
{
    QList<FixEntry> result;
    for (quint32 fid : fixtureIds)
    {
        Fixture *fxi = m_doc->fixture(fid);
        if (!fxi || !fxi->fixtureMode() || !fxi->isPatched())
            continue;

        FixEntry entry;
        entry.fixtureId = fid;
        entry.universeId = (int)fxi->universe();

        quint32 nCh = fxi->channels();
        for (quint32 c = 0; c < nCh; c++)
        {
            QLCChannel *ch = fxi->fixtureMode()->channel(c);
            if (!ch)
                continue;

            // An over-addressed fixture (patched so its channel count runs
            // past 511; Fixture::setAddress only checks the start address):
            // nothing to write past the end.
            int absAddr = (int)fxi->address() + (int)c;
            if (absAddr >= UNIVERSE_SIZE)
                break;

            uchar value = 0;
            bool include = false;

            if (ch->group() == QLCChannel::Intensity)
            {
                value = 255;
                include = true;
            }
            else if (ch->group() == QLCChannel::Colour)
            {
                switch (ch->colour())
                {
                    case QLCChannel::Red:
                    case QLCChannel::Green:
                    case QLCChannel::Blue:
                    case QLCChannel::White:
                    case QLCChannel::Lime:
                    case QLCChannel::Indigo:
                        value = 255;
                        include = true;
                        break;
                    case QLCChannel::Cyan:
                    case QLCChannel::Magenta:
                    case QLCChannel::Yellow:
                    case QLCChannel::Amber:
                    case QLCChannel::UV:
                        value = 0;
                        include = true;
                        break;
                    default:
                        break;
                }
            }

            if (include)
                entry.writes.append(qMakePair(int(c), value));
        }

        if (!entry.writes.isEmpty())
            result.append(entry);
    }
    return result;
}
