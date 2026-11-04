/*
  Q Light Controller Plus
  teachingglossary.cpp

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
#include <QHash>
#include <QVector>

#include "teachingglossary.h"

namespace
{
struct Entry
{
    const char *term;
    const char *text;
};

const Entry kEntries[] = {
    { "cue", QT_TRANSLATE_NOOP("TeachingGlossary", "A planned lighting change tied to a moment in the song. A list plays its cues in number order.") },
    { "cue list", QT_TRANSLATE_NOOP("TeachingGlossary", "The show's cues, numbered and in order. GO plays the next cue of the list.") },
    { "layer", QT_TRANSLATE_NOOP("TeachingGlossary", "One group of lights doing one job in a cue: a colour on the Backs, a level on the Fronts.") },
    { "track", QT_TRANSLATE_NOOP("TeachingGlossary", "A light holds its state into the following cues until a cue changes it. Grey text means tracked.") },
    { "tracking", QT_TRANSLATE_NOOP("TeachingGlossary", "A light holds its state into the following cues until a cue changes it. Grey text means tracked.") },
    { "tracked", QT_TRANSLATE_NOOP("TeachingGlossary", "Carried in from an earlier cue and unchanged here, so the cue does not set it again.") },
    { "release", QT_TRANSLATE_NOOP("TeachingGlossary", "Release: the layer plays in this cue, then lets go -- back to the base cue's value, else dark.") },
    { "untrack", QT_TRANSLATE_NOOP("TeachingGlossary", "Untrack: a layer carried in from an earlier cue never reaches this one -- the cue before releases it.") },
    { "block", QT_TRANSLATE_NOOP("TeachingGlossary", "A cue that starts from a dark stage, with nothing tracking into it (Mode B).") },
    { "cue-only", QT_TRANSLATE_NOOP("TeachingGlossary", "A cue that starts from the one before but whose changes do not pass on: a flash that is there and gone (Mode C).") },
    { "palette", QT_TRANSLATE_NOOP("TeachingGlossary", "A saved, named value (a colour, a level, a position) you drop on a layer. Change the palette and every cue using it changes.") },
    { "look", QT_TRANSLATE_NOOP("TeachingGlossary", "What a layer does, one value per attribute: colour, intensity, position.") },
    { "hit", QT_TRANSLATE_NOOP("TeachingGlossary", "A short, sharp change on a musical accent, fired by hand on top of whatever is running. It never tracks.") },
    { "fade", QT_TRANSLATE_NOOP("TeachingGlossary", "How long a change takes. Fade in is how long a cue takes to arrive; fade out how long its lights take to go.") },
    { "hold", QT_TRANSLATE_NOOP("TeachingGlossary", "How long a cue waits before the next one plays by itself. Infinite waits for GO.") },
    { "follow", QT_TRANSLATE_NOOP("TeachingGlossary", "A cue that plays by itself after its hold, without a GO.") },
    { "sub-cue", QT_TRANSLATE_NOOP("TeachingGlossary", "A cue inside a cue (2.1, 2.2) for stomps and stings within a section. The first plays with GO.") },
    { "base cue", QT_TRANSLATE_NOOP("TeachingGlossary", "The cue a list's released layers go back to.") },
    { "skip", QT_TRANSLATE_NOOP("TeachingGlossary", "GO passes over a skipped cue. It tracks as if it were not in the list.") },
    { "standby", QT_TRANSLATE_NOOP("TeachingGlossary", "The cue loaded to play on the next GO, shown in the Run tab as STANDBY.") },
    { "go", QT_TRANSLATE_NOOP("TeachingGlossary", "Plays the next cue. In the Run tab: the green button or Space.") },
    { "back", QT_TRANSLATE_NOOP("TeachingGlossary", "Plays the cue before the one on stage.") },
    { "design", QT_TRANSLATE_NOOP("TeachingGlossary", "Design is where you build: the selected cue shows on stage. Operate runs the show: only GO changes the stage.") },
    { "operate", QT_TRANSLATE_NOOP("TeachingGlossary", "Operate runs the show: only GO changes the stage. Design is where you build.") },
    { "group", QT_TRANSLATE_NOOP("TeachingGlossary", "A named set of lights that are always used together, like Steps or Trusses.") },
    { "target", QT_TRANSLATE_NOOP("TeachingGlossary", "A named place on stage that moving lights can aim at.") },
    { "sets", QT_TRANSLATE_NOOP("TeachingGlossary", "S: how many layers the cue sets itself.") },
    { "tracks in", QT_TRANSLATE_NOOP("TeachingGlossary", "T: how many layers track into the cue from earlier cues. On a grey row, ⤳ 3: the third cue since it was set.") },
    { "mode", QT_TRANSLATE_NOOP("TeachingGlossary", "M: B Block (starts from nothing), C Cue-only (its changes do not pass on), blank: it tracks.") },
    { "flags", QT_TRANSLATE_NOOP("TeachingGlossary", "F: base cue, skip, no move in black, and the like.") },
};

QString key(const QString &term)
{
    return term.trimmed().toLower().replace(QLatin1Char('_'), QLatin1Char(' '));
}
}

QString TeachingGlossary::definition(const QString &term)
{
    const QString k = key(term);
    for (const Entry &e : kEntries)
        if (k == QLatin1String(e.term))
            return QCoreApplication::translate("TeachingGlossary", e.text);
    return QString();
}

QString TeachingGlossary::line(const QString &term)
{
    const QString d = definition(term);
    if (d.isEmpty())
        return QString();
    QString name = key(term);
    name[0] = name.at(0).toUpper();
    return name + QStringLiteral(": ") + d;
}

QStringList TeachingGlossary::terms()
{
    QStringList out;
    for (const Entry &e : kEntries)
        out << QString::fromLatin1(e.term);
    return out;
}
