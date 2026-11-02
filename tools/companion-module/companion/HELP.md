## qlcconsole

Runs a qlcconsole show from a Stream Deck (or any Companion surface): GO, Back, Stop, Sub GO, Hold, GO a cue, hits, group masters, the Grand Master and Blackout, with the current and next cue on the keys.

Setup is on the qlcconsole side first: **Connections > Devices > Show control & sync > Web access**, tick **Start web access with this show** and **Reachable from other devices on the network**; in **File > Show Settings** tick **Allow GO from the phone Run page** (off in every show until you do). Put the computer's address and the port (9999) here. If the show has a PIN, enter it in the connection's settings.

Use it on the show network only: web access is plain http.

**Presets**: Run the show (GO showing the next cue, Back, Stop, Sub GO, Hold, Blackout, displays), Hits (32 keys, then "fire the armed hit"), and Dials for a Stream Deck + (one per group master, the Grand Master, and a "walk the cues" dial: turn to put a cue on standby, press to GO while armed).

**Variables**: `current_num`, `current_name`, `next_num`, `next_name`, `list`, `production`, `section`, `mode`, `remote`, `blackout`, `grand`, `master_N_name`, `master_N_level`, `hit_N_name`, `hit_N_on`, `hit_N_armed`.

Hits follow the show's rules: only an armed hit fires, the narrowest armed place wins, and only in Operate.
