# Speed & Spread demo — what to try

Open it: `build/main/qlcconsole -o test-workspaces/speed-spread-demo.qxw`

The rig: **8 Cobra Wash movers** on the US Truss (6 m), **8 RGB pars**
hanging on the DS Truss (5 m), an Aim target **Centre** at centre stage
(1.5 m upstage of 0). Everything is in the Programming tab's tree under
**Speed and Spread demo**.

**Setup, once:** stay in **Design** mode, go to the **Programming** tab, and
open the **Rig Overview** (⇧⌘R) beside it — a top view (or a 45° one) shows
the par pools and the mover beams on the floor. Double-click a look to play
it; double-click the next to switch.

## Spread

| Look | Watch for | Then try (look editor → Spread row) |
|---|---|---|
| **01 wave stage left to right** | Cyan pars: one wave rolls across, left to right. | Direction **Reverse**; **Wings 2** (mirrored from the middle seam); **Blocks of 2** (pairs move together); Amount **2** (two waves). |
| **02 centre out** | Amber saw: the middle pars fire first, then out to both ends. | **Ends in** — the opposite. |
| **03 ripple out from the target** | Rainbow on all 16: colour rings spread out from Centre — the nearest heads lead. | Move the **Centre** target in the Lighting Studio: the ripple re-centres. |
| **04 sweep around the target** | Rainbow sweeps *round* the target, from downstage towards stage right. | **Reverse** sweeps the other way. |

The **preview** under the Spread row shows the look's heads where they stand,
coloured by phase — same colour, same moment — with a cross on the target.

## Stage Shape

| Look | Watch for | Then try |
|---|---|---|
| **05 line, left heads on the left** | The movers draw a 6 m line through Centre; the leftmost movers take its left end — no beams crossing. | Axis **In look order** (their list order, may cross); **Reverse** (crossed on purpose); Effect **plane: Wall** stands it up. |
| **06 circle, no crossed beams** | A 2 m circle slowly spinning; each mover takes the point on its own side of the target. | Axis **Stage left to right** — watch beams cross; **Centre out** / **Wings 2** pair beams onto one point. |

## Speed masters

Open **Masters** on the Programming toolbar. **Pars** (Speed 1) is at 60 BPM;
**Movers x2** (Speed 2) follows it at ×2.

| Look | Watch for | Then try |
|---|---|---|
| **07 pars on Speed 1** | A wave of one cycle a beat (rate reads "per beat"). | **Tap** Pars a few times: the wave locks to your taps. Ride its **Rate** fader. |
| **08 movers on Speed 2 (x2)** | Movers flash twice per Pars beat. | Change Speed 2's ratio to ×½; or make it **Its own tempo**. |
| **09 both together** (collection — open it, press **Test**) | Pars wave and mover flashes locked: two flashes per wave. | **Pause** Pars — both freeze (Speed 2 follows it). |
| **10 colour chase on Speed 1** (chaser — open it, press **Play**) | Red → green → blue → white, one step per Pars beat. | **Tap** faster/slower: the chase follows. **Pause** Pars: it holds its step. The chaser's **Speed** column puts one step on another master. |

If a later change breaks the file, the looks are simple to rebuild by hand
from the tables above.
