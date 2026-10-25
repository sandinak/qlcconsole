# House rig and tour package

A touring show carries its own lights (the **tour package**) and uses
the lights each room already has (the **house rig**). qlcconsole keeps
the two apart: the tour package is saved in the show, and each venue's
house rig is saved with that venue. When you move the show to the next
room, apply that room's venue and its house rig comes in. The tour
package stays exactly as it was.

The venue is the one you pick in **Focus calls** (Lighting Studio ▸
Focus calls), kept in the file next to the show with its focus
corrections. See [Focus calls](focus-calls.md#venues-corrections-that-stay-with-the-room).

## Owner: Tour, House or Rental

Every fixture has an owner:

- **Tour**: it travels with the show. This is the default.
- **Rental**: hired for the run; it travels with the show too.
- **House**: the room's own.

Set it on a **fixture group** and every fixture in the group follows:
open the group in the Fixture Manager and pick **Owner** beside its
name (**Not set (Tour)** until you do). One fixture can say otherwise in
**Fixture Properties** (its **Owner** row; the first choice follows the
group and says what that gives: "From FOH wash: House"). A fixture in two
groups that disagree takes the first group's owner. A fixture nobody
says anything about is Tour.

## Where a house fixture is saved

With a venue applied, a fixture that becomes House goes into **that
venue's file**, not the show's: the fixture, its place on the plot and
its place in the show's groups. Add a fixture to a House group while
Lincoln HS is applied and it is Lincoln HS's. Set a house fixture back
to Tour and it comes on tour: the show keeps it from then on.

Applying another venue takes the old venue's house rig off and puts the
new one's on. **None** leaves only the tour package. Neither changes the
show file, so the show is not marked modified.

With **None** applied, House is only a label: the fixture is still saved
in the show. When you then apply a venue, its page offers **Move the
show's house fixtures in**. Nothing moves until you press it.

## Cues on whatever house rig the room has

Write cues against **groups** ("FOH wash", "Side L"), not single house
fixtures. The group belongs to the show; each venue fills it with its
own house fixtures. A Show Tree cue with a layer on FOH wash plays on
Lincoln HS's front-of-house pars at Lincoln HS and on the Grand Hall's
at the Grand Hall.

To fill a group for a venue, apply the venue and put its house fixtures
in the group, in the Fixture Manager or the group's head-layout grid, as
you would any fixture.

A value set on one house fixture by itself (a classic scene's channel,
an EFX fixture) belongs to that fixture only, and it does not carry to
another venue's rig. Use groups for house lights.

## Universes: slots, the room's patch, not ours

On the **Connections** tab each universe has a **Slot** beside its
name: **Tour** or **House**. It says which kind of fixture you mean to
put there ("U1 to U4 Tour, U5 and U6 House"). The clash check lists a
house fixture in a Tour slot and a tour fixture in a House slot.

**The room's patch.** Networks differ from room to room. With a venue
applied, change an output on the Connections tab (an Art-Net node's IP,
an sACN universe, a USB interface) and the change is that venue's: the
venue keeps the outputs of every universe as they are then. The show
file keeps the show's own outputs. Apply None and the show's go back
on; apply the venue again and its go on.

**Not ours.** A room's house desk often keeps some channels: the house
lights, or a whole universe. Mark them on the venue page. Nothing of
ours is sent there:

- **A whole universe**: nothing at all is sent to its outputs, Blackout
  and Blind included.
- **A range** in a universe we use (401-512, say): our frames carry zero
  there, whatever asks for them. DMX has no gaps, so a desk that shares
  the line needs a merge (HTP) at the node; marking the whole universe is
  the clean way.

A fixture can't be patched onto channels that are not ours: the Patch
dialog marks it **CONFLICT**.

## The venue page

**Focus calls ▸ Rig and universes…** opens the page of the venue chosen
beside it (with **None** or **New venue…** chosen, it asks you to choose
a venue first). Apply the venue first to change its house rig or its
patch; **Not ours** and the clash check work either way.

![The venue page: the house rig, each universe's slot, output and channels not ours, and the clash check](../images/venue-rig-page.png)

1. The house rig: the venue's house fixtures and the groups they fill.
2. Each universe: its slot, its output in this venue, the channels not ours.
3. Keep the outputs on the Connections tab as this venue's patch.
4. What clashes now.

- **House rig**: its house fixtures and the groups they fill, and **Move
  the show's house fixtures in** when there are any (while the venue is
  applied).
- **Universes**: each universe with its **Slot** (Tour or House; change
  it here or on the Connections toolbar), its **Output in this
  venue** (the venue's own, or the show's), and **Not ours** (type
  `all`, a range such as `401-512`, or several: `1-24, 500-512`).
- **Keep this patch for Lincoln HS** makes the outputs on the
  Connections tab now the venue's; **Use the show's patch** forgets them.
- **Clash check**: what clashes now.

## The clash check

**Apply now** (in Focus calls, beside the venue) first lists what
clashes with the show, with **Apply anyway** or **Cancel** (the default).
Starting the focus calls on a venue applies it without asking, so press
Apply now first in a new room:

- **Overlap**: two fixtures on the same addresses in a universe, at
  least one of them the room's: a house fixture and a tour or rental
  fixture, or two house fixtures.
- **Not ours**: a fixture on channels the venue keeps.
- **Slot**: a house fixture in a Tour slot, or a tour fixture in a House
  slot (only once the show has House slots).

A house fixture on a tour fixture's addresses goes in **unpatched**,
keeping the address it wants, so it never drives the tour fixture's
channels. Opening a show whose venue has clashes says so, and the Patch
dialog lists the venue's clashes under its table.

## Seeing who owns what

- **Overlay: Owner**, on the Lighting Studio plot's bar, colours the plot: Tour blue,
  House amber, Rental violet. The tooltip says where the owner comes
  from and whose file a house fixture is in.
- The **Inventory** panel shows **[House]** or **[Rental]** beside a
  fixture that is not Tour.
- The **patch list** (Fixture Manager ▸ **Patch List…**) marks each
  fixture that is not Tour, and its spreadsheet gains an **Owner**
  column.

## Old shows

Nothing changes in a show that never uses owners: every fixture and
group is Tour, every universe a Tour slot, and nothing extra is written.
A venue with no house rig, no patch and nothing marked not ours applies
exactly as before.
