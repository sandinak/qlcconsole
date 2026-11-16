# Pad traces

One `.jsonl` file per recorded pad session, played by `padtraces_test`
through the real HID plugin (`plugins/hid/test/padtraceplayer.h`) into Pad
Run. Format and recording: [docs/dev/PAD_TRACES.md](../../../docs/dev/PAD_TRACES.md).

**The three traces here are SYNTHETIC**, written by
[make_synthetic_traces.py](make_synthetic_traces.py) to copy what the bench
has shown, until Branson records the real pads:

- `clone_flip_to_xbox.jsonl`: a cabled DualSense clone arrives, LB+RB arm,
  LT+A GO, Create+Options held ~3 s, it leaves, three "Xbox 360 Controller
  for Windows" 045e:028e arrivals at one port within ~1 s, the first two
  leaving again.
- `twin_listing.jsonl`: one pad listed twice with the same identity.
- `trigger_rest.jsonl`: both triggers resting near 8 % and stick jitter
  around centre.

A real recording that replaces one keeps the file name its test loads, or
the test is pointed at the new name.
