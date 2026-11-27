#!/bin/bash
# Decode a generated 1 s WAV and MP3 with the libsndfile INSIDE a built .app
# (the same library, and codecs, the audio plugin loads): proves the bundled
# audio libraries load and that MP3 plays, without opening the app.
#
#   platforms/macos/audio-decode-check.sh /path/to/qlcconsole.app
#
# Needs the `lame` command line tool (brew install lame) to make the MP3, and
# a C compiler. Run by .github/workflows/build.yml (macOS job) and by
# platforms/macos/make-release.sh before signing.
set -euo pipefail
APP="${1:?usage: audio-decode-check.sh App.app}"
LIB="$APP/Contents/Frameworks/libsndfile.1.dylib"
[ -f "$LIB" ] || { echo "ERROR: no $LIB"; exit 1; }
command -v lame >/dev/null || { echo "ERROR: lame is needed to make the test MP3 (brew install lame)"; exit 1; }
W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT

python3 -I - "$W/tone.wav" <<'PY'
import math, struct, sys, wave
w = wave.open(sys.argv[1], "wb")
w.setnchannels(2); w.setsampwidth(2); w.setframerate(48000)
w.writeframes(b"".join(struct.pack("<hh", v, v) for v in
    (int(8000 * math.sin(2 * math.pi * 440 * i / 48000)) for i in range(48000))))
w.close()
PY
lame --silent -b 128 "$W/tone.wav" "$W/tone.mp3"

cat > "$W/dec.c" <<'C'
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct { long long frames; int samplerate, channels, format, sections, seekable; } INFO;
int main(int argc, char **argv)
{
    void *h = dlopen(argv[1], RTLD_NOW);
    if (!h) { printf("dlopen failed: %s\n", dlerror()); return 2; }
    void *(*op)(const char *, int, INFO *) = dlsym(h, "sf_open");
    long long (*rd)(void *, short *, long long) = dlsym(h, "sf_readf_short");
    int (*cl)(void *) = dlsym(h, "sf_close");
    const char *(*ver)(void) = dlsym(h, "sf_version_string");
    printf("%s\n", ver());
    int rc = 0;
    for (int i = 2; i < argc; ++i) {
        INFO in = { 0 };
        void *f = op(argv[i], 0x10, &in);
        if (!f) { printf("%s: cannot open\n", argv[i]); rc = 1; continue; }
        short *buf = malloc(sizeof(short) * 2 * 4096);
        long long n = 0, got;
        while ((got = rd(f, buf, 4096)) > 0) n += got;
        cl(f); free(buf);
        printf("%s: %lld frames, %d Hz, %d ch\n", argv[i], n, in.samplerate, in.channels);
        if (n < 40000 || n > 60000) { printf("  unexpected length\n"); rc = 1; }
    }
    return rc;
}
C
cc -o "$W/dec" "$W/dec.c"
"$W/dec" "$LIB" "$W/tone.wav" "$W/tone.mp3"
echo "OK: the app's libsndfile decodes WAV and MP3"
