# Definition Updates

Fixture definitions get fixed: a channel saved as fine that is really
coarse, a default that should be 128, a mode that was missing. When a newer
version of a definition exists, the old copies don't change by themselves
— in your fixture folder, in the old QLC+ folder, or inside a saved show.
qlcconsole finds them and offers to bring them up to date, never silently,
always with a way back.

## Out-of-date definitions in your folder

**Fixtures → Definition Updates…** lists the definitions in your fixture
folder that are **older than the current version of the same fixture**.
The current version is the newest of:

- the one that comes with qlcconsole (included), and
- every copy of the same make and model in your folder — including "X 2.qxf"
  sync copies, and links into the old QLC+ folder.

The footer says so too: **"N of your fixture definitions are older than
the current version"** — a note when the open show doesn't use them, a
warning when it does — with **Review…**.

The list has two parts: **Used by this show** (the open show's fixtures use
that file) and **Not used by this show**. Each row shows the file, your
version, the current one, and why it is listed. Pick one to see **what an
update changes**:

- modes added, removed or changed — a mode whose channels no longer do the
  same thing, in the same order, is marked **[layout]**;
- channels added, removed or changed — "Pan, was Pan (fine); default 0 →
  128";
- for a definition the show uses, whether the show's fixtures keep their
  modes and channels ("updating is safe"), and if not, which fixtures and
  functions are affected.

The choices:

- **Update from Current** — writes the current version over the selected
  files.
- **Update All** — every out-of-date file listed, in one go.
- **Keep Mine** — stop offering an update for that file while it stays as
  it is. Change the file and it is checked again.
- **Ignore This Version** — don't offer this current version again; a newer
  one will be offered.
- **Undo Last Update** — puts back the files the last update replaced.

### How "older" is decided

A copy is only called old when it really is an earlier version of the same
definition — never just because it differs:

1. **The same bytes** — nothing to do.
2. **Someone else's** — a different author in the file's Creator: a
   customised copy, not an old one.
3. **It has something the current one doesn't** — a channel or a mode of
   its own: made on purpose, so customised.
4. Otherwise **the version that saved it** decides: 4.12.6 is older than
   4.14.2, which is older than 4.14.2+qlcconsole.1393. A qlcconsole build
   and a later QLC+ release (4.14.4) are different lines, so neither counts
   as newer by its number.
5. At the same version (or when the numbers can't tell), between your own
   files **the one changed last is current**. A shipped file's date is when
   the app was installed, so it never decides — that case counts as
   customised.

Customised copies are never listed as out of date. **Also list customised
copies** shows them too, with the same choices, for when you know better.

### What keeps it safe

- **A backup of every file replaced.** Each update makes a dated folder in
  your fixture folder's `.backup` (for example
  `Fixtures/.backup/2026-10-21_153000/`) holding the old files exactly as
  they were, and a list of what was done. **Undo Last Update** uses it, and
  puts back the files' dates too. A file changed since the update is left
  alone, and says so.
- **Links into the old QLC+ folder.** Many definitions in your folder are
  links into the QLC+ folder — which may be a synced folder (Synology,
  Dropbox…), so a change there reaches every computer that syncs it. Choose
  under **Linked files**:
  - **Update the original the link points to** (the default) — the synced
    file is fixed, for every computer. The report says which file, and that
    it syncs.
  - **Replace the link with a file of its own** — only this computer
    changes; the original is left as it was. Undo makes it a link again.
- **Copies left behind in the QLC+ folder.** A link edited in qlcconsole
  becomes a file of its own, and the synced original stays old for every
  other computer still linking to it. Those originals are listed too ("in
  the QLC+ folder, not linked"), and updating one fixes it there.
- **Included files are never written.** A link that leads into the app's
  own definitions is replaced with a file of its own instead.
- **The open show follows.** A definition the show uses is switched in
  place: each fixture keeps its mode by name, its address and its values.
  A mode that is gone, or has more or fewer channels, would need
  re-patching, so that update is **refused**, naming the fixtures. A mode
  whose channels are as many but one does something else (Pan fine → Pan
  coarse) is done only after you confirm, with the fixtures and the
  functions that use them listed.
- **Links that lead nowhere** (their target is missing — a sync folder not
  mounted, say) can't be checked; the dialog counts them.

## Old definitions inside a show

A **.qlcc** carries its own copy of each definition it uses (see
[Opening a show on another computer](fixture-manager.md#opening-a-show-on-another-computer)).
When the show's copy differs from this computer's, the show's is used and
the footer warns **"N items in this show differ from this computer's"**.

**File → Show Definitions…** lists everything the show carries — the same,
different, or only in the show — at any time; the footer's **Review…**
lists just the differences not yet dealt with. For each difference:

- **Which looks newer.** The version that saved each copy first, as above;
  then the times. "Updated on this computer since the show was saved" means
  the show's copy is the old one.
- **What changes**, channel by channel and mode by mode, from the show's
  copy to this computer's.
- **Safe to switch?** Compatible when every mode the show's fixtures use is
  in this computer's copy with the same channels in the same order.
  Otherwise the fixtures and the functions affected are named.

The choices:

- **Use This Computer's** — the update path: the show switches to this
  computer's copy now, each fixture to its mode of the same name, and the
  next save carries the new copy in the show. A changed layout asks first,
  listing what it touches; a missing mode, or fixtures that would grow into
  the next one, is refused. **Use This Computer's for All Compatible** does
  every safe one.
- **Undo** — goes back to the show's copies the last Use This Computer's
  switched from, in one step.
- **Keep the Show's** — stop warning about it (remembered in the show).
- **Install the Show's** — the other way round, for when the show's copy
  is newer ("This computer has an older copy"): it goes into your fixture
  folder (the old file to the Trash), and every show opened here uses it.

An update in your folder reaches an open show too: when the show's copy was
the newer one and your folder catches up, the difference simply goes away.
