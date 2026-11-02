# companion-module-qlcconsole

Run a qlcconsole show from a Stream Deck (or any Bitfocus Companion surface):
GO, Back, Stop, Sub GO, Hold, GO a cue, hits, group masters, the Grand Master
and Blackout, with the current and next cue and the hit and master state on
the keys. Dials (Stream Deck +) move masters or walk the cues.

User help: `companion/HELP.md` and docs/features/show-control.md, "Stream Deck
(Companion)". The app side is the Run socket of web access
(webaccess/src/webaccessrun.*); this module adds no protocol of its own.

Built on `@companion-module/base` 2.2 (Companion 4.3 to 5.1), after Bitfocus's
`companion-module-template-js`.

## Develop

```sh
npm install
npm test        # node --test against a mock of qlcconsole's socket
```

Load it in Companion with Settings, Developer modules path, pointing at the
folder that holds this one (`tools`), then add a connection of type qlcconsole.

## Layout

- `src/client.js`: the websocket, state, dial coalescing, the walk dial's arm.
- `src/actions.js`, `feedbacks.js`, `variables.js`, `presets.js`: Companion definitions.
- `src/main.js`: the `InstanceBase`.
- `companion/manifest.json`, `companion/HELP.md`.

## Not published

Nothing here is on npm or in Companion's module store, and no PR is open at Bitfocus.
Publishing would take: its own repo `bitfocus/companion-module-qlcconsole`
(Bitfocus requires one module per repo, named that way), the manifest's
repository and bugs URLs pointed at it, a licence they accept (MIT here),
`yarn` with `@companion-module/tools` (`yarn package` builds the tgz), a pass of
their review checklist (naming, HELP.md, no sample text in the manifest) and a
PR to `bitfocus/companion-bundled-modules` / the module list. It is Branson's call.
