

- Effects script that can fire up the LEDs based on position marked on stage

- in text mode can we create more traditional dropdown menus that match the MacOS model 
    - file operations in File
    - about and settings in "App" menu
    - monitor and full screen in View menu
    - Index under Help menu 
    - Dump, edit modes etc.. under Edit Menu
    - Blackout and Run mode can stay as operational icons

- why are there two blackouts

Distant Future
- per head tracking of targets and followspot

- in Run mode, make the follow-spot beam pin draggable with the mouse so
  the operator can drive the follow-spot by mouse (the equivalent of the joystick
  moving the stage target). Pin is currently a passive indicator; would become a
  draggable item wired into the stage-aim path (programmercontroller m_stageAimX/Y).

- 2D map "Views" follow-ups: Power-view legend (circuit→colour swatches); draw
  circuit lines from a source marker to its fixtures; DMX / Groups views.

- EffectScript that 
  - lets one define a pattern on the layout visually ( line/ square / circle ..etc ) 
  - assinged lights will then be distributed to point across that pattern 
  so a circle of 6 lights would be pointing to positions at 60 degree increments across the stage
  a line of 10 lights would all point to the line location at 10 intervals along it 
  etc.

NEW: 
Given the new features whats the best way to implement this workflow
 - we have preshow operations that must be hand cue'd .. ( preshow, announcer, shout, etc. ) 
 - we then have a click track that starts the time code and should sync lighting
 - end of song we have a break with spoken voice from stage and a few cues 
 - next song starts the light cue's again 
 - something breaks and the operator wants to immediatelty take over cuing 
etc.

would we need a cue's that can be hand cue'd between the time coded cues .. would that work? 
would we need another method ? 
do we need a way to discretely identify who "owns" the cuestack at any moment? 