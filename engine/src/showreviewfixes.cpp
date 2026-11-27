/*
  Q Light Controller Plus - Console fork
  showreviewfixes.cpp

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

#include "showreview.h"
#include "fixture.h"
#include "doc.h"

/* How to fix each check (docs/features/show-review.md, "How to fix"): ONE
   place per check. A new check without a row here fails showreview_test's
   table test. %f is the finding's fixtures by name (or "the fixture").
   Kind: Fix (one-click, done by the Review panel, safe and unambiguous),
   Go There (the finding's jump opens the place), Manual (says why it can't
   be automatic). Anything that changes how the show behaves in a way a
   person should decide is never Fix; hazards are Go There at most. */

namespace {

struct Row
{
    ReviewFinding::FixKind kind;
    QString text;
};

const QHash<QString, Row> &rows()
{
    typedef ReviewFinding F;
    auto tr = [](const char *s) { return QCoreApplication::translate("ShowReview", s); };
    static const QHash<QString, Row> all = {
    { "PAT-01", { F::OneClick, tr("Move %f to the first free address that fits it in its universe (Fix does this), or patch it by hand in Fixtures.") } },
    { "PAT-02", { F::OneClick, tr("Move %f down to a free address that fits before channel 512 (Fix does this), or onto another universe.") } },
    { "PAT-03", { F::GoThere, tr("Patch %f to a free address in Fixtures, or delete it if the show doesn't use it.") } },
    { "PAT-04", { F::GoThere, tr("Re-patch %f off the channels the venue keeps, to a free address in Fixtures.") } },
    { "PAT-05", { F::GoThere, tr("Two venue files claim the same channels: change the address of one in Fixtures, or edit the venue's kept channels.") } },
    { "PAT-06", { F::GoThere, tr("Give this universe an output in Connections ▸ Devices, or move its fixtures to a universe that has one.") } },
    { "FIX-01", { F::GoThere, tr("Install the definition for %f (Fixture Editor, or copy its .qxf into the user fixtures folder), or open the show where it was built and save it as .qlcc so it carries the definition.") } },
    { "FIX-02", { F::GoThere, tr("Open the definition of %f in the Fixture Editor and fix what it lists, or pick a corrected definition for the fixture.") } },
    { "FIX-05", { F::GoThere, tr("Put %f on the plot: drop it on the stage in Rig Overview so focus calls can aim it.") } },
    { "FIX-07", { F::GoThere, tr("Enter lumens and watts on the definition of %f, Physical tab, so the plot and power checks stop guessing.") } },
    { "FIX-08", { F::GoThere, tr("Mark %f as working again in Fixtures, or take it out of the scenes and cues that still use it.") } },
    { "FIX-14", { F::GoThere, tr("Give each of %f its own name: Fix runs the footer's Number Them (Name 2, Name 3), or rename them in Fixtures.") } },
    { "FIX-15", { F::GoThere, tr("Raise %f above the floor in Rig Overview (its height), or move it out of the set piece it sits inside.") } },
    { "FIX-16", { F::GoThere, tr("Set %f's height on the plot, or tell the plot which deck it is on, so it isn't drawn on the floor row.") } },
    { "FIX-17", { F::GoThere, tr("Fit a picture to the gobo slots of %f in Gobo Wheels, or leave it: only the preview is plain.") } },
    { "FIX-18", { F::GoThere, tr("Set the beam angle and the pan/tilt range on the definition of %f, Physical tab, so the plot can draw its beam.") } },
    { "FIX-19", { F::GoThere, tr("Load the missing gobo into %f in Gobo Wheels, or choose another slot in the cues that use it.") } },
    { "FIX-20", { F::GoThere, tr("Check the by-hand setting on %f in Fixtures; clear it if the show should drive it normally.") } },
    { "FIX-21", { F::GoThere, tr("Treat %f as a laser: mark it as a laser in Fixtures and set its safety zones. This is a safety decision, so it is never done for you.") } },
    { "FIX-22", { F::GoThere, tr("Give the hazard cues a spotter's control: put a Hold or Go button for them on the board, where the spotter has it. Safety: only you can decide who holds it.") } },
    { "FIX-23", { F::GoThere, tr("Look at %f in Fixtures and confirm it is a hazard (fog, haze, pyro, a switch) with its safe state set. Safety: never changed for you.") } },
    { "FIX-24", { F::GoThere, tr("Take the effect off the hazard %f in its cue, or make it a plain on and off. Safety: never changed for you.") } },
    { "FX-02", { F::GoThere, tr("Point the RGB matrix at lights that mix colour, or change the look to a dimmer or chase.") } },
    { "FX-03", { F::GoThere, tr("Open the effect and give it something to move: more than one step, a size above 0, a speed above 0.") } },
    { "FX-07", { F::GoThere, tr("Choose a script for the effect look in its scene, or install the script it names on this computer.") } },
    { "PWR-01", { F::GoThere, tr("Move some of the lights on this circuit to another circuit, or raise the breaker size in Power; stay under 80 percent of the breaker.") } },
    { "PWR-03", { F::GoThere, tr("Put %f on a circuit in Power so the load adds up.") } },
    { "PWR-05", { F::GoThere, tr("Slow the effect or the strobe to 3 flashes a second or fewer, or tell the show it is meant (photosensitive audience: slower).") } },
    { "PWR-06", { F::GoThere, tr("Mark %f to follow Blackout in Fixtures, or accept it if it's a practical that must stay lit.") } },
    { "PWR-09", { F::GoThere, tr("Aim the laser or star field away from the audience, or add a no-light zone over their seats. Safety: never changed for you.") } },
    { "PWR-10", { F::GoThere, tr("Widen the laser pattern or give it motion so it isn't a tight static dot. Safety: never changed for you.") } },
    { "PWR-11", { F::GoThere, tr("Tape off the bottom of the beam (Apply This Cut puts the suggested amount in the fixture's Masking), or aim the light so it stays out of the zone.") } },
    { "PWR-12", { F::GoThere, tr("Show Me the layer that lights the cyc: aim it off, shutter or darken it in this cue, or say I Meant It if the backwash is on purpose.") } },
    { "PWR-13", { F::GoThere, tr("Aim the laser out of the zone, or cut its output in the cues that put it inside. Safety: never changed for you.") } },
    { "TEACH-01", { F::Manual, tr("Teaching mode is a choice for the room: turn it off in Settings when the class is over. It is never switched off for you.") } },
    { "FILE-01", { F::GoThere, tr("Copy the media into the show's folder and re-link it, or save the show as .qlcc so it carries them.") } },
    { "FILE-02", { F::GoThere, tr("Find the missing file and re-link it (the media list shows where the show looked), or remove the cue that plays it.") } },
    { "FILE-03", { F::GoThere, tr("Save the show as .qlcc so it carries its fixture definitions to the next computer.") } },
    { "NAME-01", { F::GoThere, tr("Rename it to the folder's SS-PP.II-Description pattern, as its siblings are.") } },
    { "NAME-02", { F::GoThere, tr("Rename the default or copy name to say what it is, in the folder's pattern.") } },
    { "PAL-01", { F::GoThere, tr("Two layers set the same attribute: remove one, or move them to different cues so only one decides.") } },
    { "PAL-02", { F::GoThere, tr("This look is covered by another and never shows: delete it, or raise it above the one hiding it.") } },
    { "PAL-03", { F::GoThere, tr("Change the palette to values %f can play, or take those fixtures out of the palette's targets.") } },
    { "PAL-05", { F::GoThere, tr("The palette points at nothing: choose fixtures or a group for it, or delete the palette.") } },
    { "CUE-01", { F::GoThere, tr("Add a block at the top of the next song so it doesn't track in the last song's look, or untrack the layers.") } },
    { "CUE-02", { F::GoThere, tr("Make the song's first cue a block (a full look), so it starts from a known state.") } },
    { "CUE-03", { F::GoThere, tr("This cue changes nothing: give it a look, or delete it.") } },
    { "CUE-04", { F::GoThere, tr("This cue is empty: add a look to it, or delete it.") } },
    { "CUE-05", { F::GoThere, tr("Set an auto-follow time on the cue, or turn auto-follow off so it waits for GO.") } },
    { "CUE-06", { F::GoThere, tr("Renumber one of the cues sharing the number, so each GO lands where you mean.") } },
    { "CUE-07", { F::GoThere, tr("Un-skip the cue, or delete it if it is never coming back.") } },
    { "CUE-08", { F::GoThere, tr("Fix the base cue as listed: it is the state every cue below tracks from.") } },
    { "CUE-11", { F::GoThere, tr("Break the include loop or re-point the dangling reference at a list that still exists.") } },
    { "CUE-12", { F::GoThere, tr("Play the reusable copy in the cue, not the original, or mark the original as used here on purpose.") } },
    { "CUE-13", { F::GoThere, tr("Put a Cue Runner or a button for this list on the board, or delete the list.") } },
    { "CUE-15", { F::GoThere, tr("Add a Sub GO to the board for this list, or take the Sub GO cue out.") } },
    { "CUE-17", { F::GoThere, tr("Make the press a plain hit: switch off tracking and repeat on it, so it lets go when released.") } },
    { "CUE-18", { F::GoThere, tr("Share the fade times with the reusable (Share Fades) so a change is made once.") } },
    { "VC-01", { F::GoThere, tr("Two hits drive the same place: give each its own place on the board, or let one win by order and mark it meant.") } },
    { "VC-02", { F::GoThere, tr("A narrow hit is overridden by a wider one: reorder them on the board, or remove the overridden one.") } },
    { "VC-03", { F::GoThere, tr("Give the hit a button, key or input so someone can fire it, or delete it.") } },
    { "VC-04", { F::GoThere, tr("Attach a function to the button in the board map, or delete the button.") } },
    { "VC-05", { F::GoThere, tr("Give one of the two buttons a different key in the board map.") } },
    { "VC-06", { F::GoThere, tr("Re-learn one of the two controls on a different input in the board map.") } },
    { "VC-07", { F::GoThere, tr("Patch an input for that universe in Connections ▸ Devices, or re-learn the control on an input that exists.") } },
    { "VC-08", { F::GoThere, tr("Point the Cue Runner at a cue list in the board map, or delete it.") } },
    { "VC-09", { F::GoThere, tr("Check the Cue List on the upgraded chaser still plays the steps you expect, in the board map.") } },
    { "CTL-01", { F::GoThere, tr("Plug the stop device back in, or learn a new emergency stop in Connections ▸ Control surfaces. Safety: never changed for you.") } },
    { "MOV-01", { F::GoThere, tr("Move the head in the dark (a blackout step first), or fade it; a head moving in light shows on the audience.") } },
    { "MOV-02", { F::GoThere, tr("Give the head time in black before the move: lengthen the dark step, or move after the light fades.") } },
    { "MOV-08", { F::GoThere, tr("Mark the head's position before the first cue (a pre-cue in black) so it doesn't swing in at GO.") } },
    { "MOV-13", { F::GoThere, tr("Light the dark part of the area with another light, or narrow the area.") } },
    { "MOV-14", { F::GoThere, tr("Centre the pan on stage: the head takes the turn that keeps the seam off the stage (it says what changes first).") } },
    { "BLK-01", { F::GoThere, tr("Change the blocking so the target is reachable, or move the target onto the stage.") } },
    { "VEN-01", { F::GoThere, tr("Give the focus call a target (a fixture, a group or a spot on the plot), or delete the call.") } },
    { "VEN-02", { F::GoThere, tr("Put the lights the focus call names on the plot, with their beam data, so it can aim them.") } },
    { "VEN-05", { F::GoThere, tr("The fixtures were swapped: run focus again for them.") } },
    { "VEN-09", { F::GoThere, tr("Find the venue file and re-link it in the show's venue settings, or choose another venue.") } },
    { "VEN-10", { F::GoThere, tr("Turn the practice venue off in the venue settings before the show.") } },
    { "VEN-11", { F::GoThere, tr("Re-point the standard cue at lights this venue has, or leave it if this venue doesn't use it.") } },
    };
    return all;
}

QString fixtureNames(Doc *doc, const QStringList &targets)
{
    QStringList names;
    int n = 0;
    for (const QString &t : targets)
    {
        if (!t.startsWith(QLatin1String("fx:")))
            continue;
        Fixture *fx = doc != nullptr ? doc->fixture(t.mid(3).toUInt()) : nullptr;
        const QString quoted = fx != nullptr ? QStringLiteral("'%1'").arg(fx->name()) : QString();
        if (!quoted.isEmpty() && names.contains(quoted))
            continue;           // two fixtures of one name: say it once
        ++n;
        if (fx != nullptr && names.count() < 3)
            names << quoted;
    }
    if (names.isEmpty())
        return QCoreApplication::translate("ShowReview", "the fixture");
    QString s = names.join(QStringLiteral(", "));
    if (n > names.count())
        s += QCoreApplication::translate("ShowReview", " and %n more", "", n - names.count());
    return s;
}

}

bool ShowReview::hasHowToFix(const QString &checkId)
{
    return rows().contains(checkId);
}

ReviewFinding::FixKind ShowReview::fixKind(const ReviewFinding &finding)
{
    const auto it = rows().constFind(finding.check);
    if (it == rows().constEnd())
        return ReviewFinding::Manual;
    if (it->kind == ReviewFinding::OneClick)
        return oneClickApplies(finding) ? ReviewFinding::OneClick
                                        : (finding.jump.isEmpty() ? ReviewFinding::Manual : ReviewFinding::GoThere);
    if (it->kind == ReviewFinding::GoThere && finding.jump.isEmpty() && finding.fixMask.isEmpty())
        return ReviewFinding::Manual;
    return it->kind;
}

QString ShowReview::howToFix(const ReviewFinding &finding, Doc *doc)
{
    const auto it = rows().constFind(finding.check);
    if (it == rows().constEnd())
        return QString();
    QString text = it->text;
    text.replace(QLatin1String("%f"), fixtureNames(doc, finding.targets));
    if (fixKind(finding) == ReviewFinding::Manual && it->kind != ReviewFinding::Manual)
        text += QCoreApplication::translate("ShowReview", " (Nothing to open for this one: it is done by hand.)");
    return text;
}

bool ShowReview::oneClickApplies(const ReviewFinding &finding)
{
    // PAT-01 / PAT-02 move one fixture: each needs fixture targets.
    if (finding.check != QLatin1String("PAT-01") && finding.check != QLatin1String("PAT-02"))
        return false;
    for (const QString &t : finding.targets)
        if (t.startsWith(QLatin1String("fx:")))
            return true;
    return false;
}

void ShowReview::annotate(Doc *doc, ReviewFinding &finding)
{
    finding.fixKind = fixKind(finding);
    finding.howToFix = howToFix(finding, doc);
}
