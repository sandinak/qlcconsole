---
title: Glossary
nav_order: 9
---

# Glossary

Every term the course uses, with the lesson that introduces it in
brackets.

## The stage

| Word | Means |
|---|---|
| **Upstage / downstage (US / DS)** | Toward the back / the front of the stage. (Old stages sloped, so the back was "up".) [0.2] |
| **Centre stage (CS)** | The middle of the stage; on our risers, the middle row. [0.2] |
| **Stage left / stage right (SL / SR)** | The singers' left / right as they face the audience. Stage right is the audience's left. [0.2] |
| **Risers / steps** | The platforms the choir stands on: three rows (US, CS, DS) of four units, numbered 1–4 from SR. Their lights are the **Steps** (strips on the fronts) and **StepTops** groups. [0.2] |
| **Cyc** (cyclorama, pronounced "sike") | The large white cloth across the back of the stage, lit to fill the background with colour. [0.2] |
| **Truss** | A metal frame that lights are rigged on. Our **Trusses** are four upright ones across the back: two SR, two SL (1 = outside). [0.2] |
| **AT (audience tower)** | A tower of lights in the house, one each side (AT1 SR, AT2 SL), lighting the singers from the front. [0.2] |
| **UST** | Upstage beam: the eight moving beams along the top riser. [0.2] |
| **RGB / W** | A pixel's red-green-blue colour LEDs / its white LEDs. *TrussC-RGB* is the colour pixels of the boxes lighting the **C**yc; *TrussW-RGB* those **W**ashing the stage. [1.1] |
| **House** | Where the audience sits. **Front of house (FOH)**: lights out there, pointing at the stage. [0.2] |
| **Rig** | All the lighting fixtures and everything they hang on. [0.2] |
| **Proscenium** | The frame around the stage opening, where the main drape hangs. [0.2] |
| **Stage manager (SM)** | Runs the show during performance and **calls** every cue. [1.5] |
| **Board operator (board op)** | Runs the lighting console, taking each cue when the SM calls it. [1.5] |

## Light

| Word | Means |
|---|---|
| **Intensity** | How bright a light is, from Full (100 %) to Out (0 %). [0.1] |
| **Colour** | The colour of the light: warm (amber, warm white) or cool (blue, lavender), saturated or pale. [0.1] |
| **Direction** | Where light comes from: front (shows faces), back (outlines), side (sculpts), below (drama). [0.1] |
| **Movement** | Light that changes while you watch: fades, chases, moving beams. [0.1] |
| **Visibility** | The first job of light: the audience can see the performers, especially their faces. [0.1] |
| **Focus (of the audience)** | Where the audience looks. The eye goes to the brightest part of the stage. [0.1] |
| **Mood** | The feeling of a moment. [0.1] |
| **Energy** | How big the music is at a given moment; the lighting follows it. [0.1] |
| **Warm / cool** | Warm colours (amber, warm white, red) read as friendly or hot; cool ones (blue, lavender, cyan) as calm, cold, night-time or eerie. [1.2] |
| **Saturated / pale** | Deep and strong (Red, Blue) / softer, with white mixed in (Lavender). [1.2] |
| **Contrast** | How different two moments are. A dark verse makes the chorus that follows feel bigger. [1.2] |
| **Front / back / side light** | Light from the audience side (shows faces) / from behind (outlines performers against the background) / from the side (sculpts bodies). [1.3] |
| **Snap** | A change with no fade (0 seconds), for a hit or a surprise. [1.4] |
| **Wash** | A soft, wide light that covers an area. [1.1] |
| **Spot** | A narrow, hard-edged light, often following one person. [1.3] |
| **Shutter** | A spot's built-in shutter blocks the beam: while it's closed, no light comes out, however high the level. Our AT-Spots need **Open** (Palettes → Shutter). [1.3] |
| **Position / pan / tilt** | Where a moving light points: it pans (turns left and right) and tilts (up and down). [1.3] |
| **Mover / moving light** | A fixture that can be pointed (panned and tilted) from the console. [1.3] |
| **Pixel box / pixel bar** | A fixture made of many small LEDs (pixels), each of which can be a different colour. [1.1] |
| **Uplight** | Light from below. Our riser tops uplight the singers. [1.1] |
| **Silhouette** | A performer seen as a dark shape against a lit background. [1.1] |
| **Button** | The final hit that ends a song, usually with a lighting cue on it. [1.1] |
| **Eye candy** | Light that's there to be looked at (beams, pixels) rather than to light the performers. [2.4] |

## The Show Tree

| Word | Means |
|---|---|
| **Production** | The whole show: every cue list in it. The **current** production is the one GO plays. [1.1] |
| **Cue list** | One song's cues, in order. [1.1] |
| **Cue** | A planned lighting change, tied to a moment in the song. Numbered 1.0.0, 1.0.1… and never renumbered. [1.1] |
| **Group** | A named set of lights that are always used together, like *Steps* or *TrussC-RGB*. [1.1] |
| **Layer** | One group (or several) doing one thing in a cue. [1.1] |
| **Look** | What a layer does, one value per attribute: colour, intensity, position… [1.1] |
| **Palette** | A saved, named value — a colour, a level, a position — that you drag onto a layer. Change the palette and every cue using it changes. [1.1] |
| **⋯ (more)** | The button at the end of each row of a layer's page: **Set to**, **Level…**, **Colour…** and more. [1.2] |
| **One-off value** | A level or colour you picked (Level…, Colour…) that isn't a palette: kept just for one layer. **Make reusable** turns it into a palette. [1.2] |
| **Cue look** | A palette dropped on the cue itself (its dashed box): it reaches every layer of the cue that doesn't set that attribute itself. [1.2] |
| **Undo / Redo** | ⌘Z reverses the last change; ⇧⌘Z reapplies it. [1.2] |
| **Library** | The panel on the left: groups, targets and palettes to drag. [1.1] |
| **Rig Overview** | qlcconsole's 3D view of the stage, showing what the lights are doing. It's our stage in class. [0.2] |
| **Tracking** | A light holds its state into the following cues until a cue changes it. A **tracked** value shows in grey: *tracked from 1.0.0*. [1.1] |
| **GO** | Plays the next cue. In the **Run** tab: the green button or Space. [1.1] |
| **Run tab** | The operator's screen: the cue list, NEXT, and big GO / Back / Stop buttons. [1.1] |
| **Design / Operate** | Design: you build, and the selected cue shows on the stage. Operate (the Run tab says **LIVE**): you run the show; only GO changes the stage. The button is at the top right. [1.5] |
| **Standby / LOAD** | The warning before a call. In the Run tab, clicking a cue LOADs it as **▷ STANDBY**, the cue the next GO plays. [1.5] |
| **Back / Stop** | Back: the cue before. Stop: stops the list; the stage goes dark. [1.5] |
| **Stage target** | A named place on stage (Soloist DS, Center) that moving lights can aim at. Library → All → Targets. [1.3] |
| **Aim** | Point moving lights at a stage target: drop the target on the layer. The Position row then says *Aim Center*. [1.3] |
| **Block cue** | A cue that starts from a dark stage, with nothing tracking into it (Mode: Block, **⊘ block** in the tree). [1.3] |
| **Fade** | How long a change takes. **Fade in**: how long a cue takes to arrive; **fade out**: how long its lights take to go when the next cue takes over. [1.4] |
| **Hold / follow** | How long a cue waits before the next one plays by itself (∞: waits for GO). A cue that plays by itself **follows**. [1.4] |
| **Release** | A layer that leaves when its cue ends. [2.1] |
| **Effect** | A pattern that moves through the lights: a chase, a sparkle, a rainbow. [2.4] |

## Hardware (Level 3)

| Word | Means |
|---|---|
| **DMX** | The signal that tells real lights what to do: 512 channels per **universe**. [3.1] |
| **Address** | Where a light's channels start in its universe. [3.1] |
| **Patch** | Which address each light is at. [3.1] |
| **Art-Net / sACN** | DMX sent over a network cable. [3.1] |
| **Fixture definition** | The file that tells the app what each channel of a light does. [3.2] |
