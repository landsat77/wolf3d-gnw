#!/bin/sh
# Build wf_render, the program tools/wf_pack.py runs to render the AdLib
# music and sound effects (tools/render: the game's sequencer + an OPL
# emulator), for Linux, Windows and macOS, with "zig cc" as cross compiler:
#   pip install ziglang
#   tools/build_renderers.sh            -> tools/bin/wf_render-<os>-<arch>
# TARGETS="triple:name ..." selects the targets.
set -e
cd "$(dirname "$0")/.."
OUT=${OUT:-tools/bin}
mkdir -p "$OUT"
ZIG="${ZIG:-python3 -m ziglang}"
TARGETS="${TARGETS:-x86_64-linux-musl:linux-x86_64 aarch64-linux-musl:linux-arm64 x86_64-windows-gnu:windows-x86_64.exe x86_64-macos:macos-x86_64 aarch64-macos:macos-arm64}"
for target in $TARGETS; do
    triple=${target%%:*}
    name=wf_render-${target#*:}
    $ZIG cc -target "$triple" -O2 -std=gnu11 -DGNW -w -s \
        tools/render/wf_render.c tools/render/opl.c -lm -o "$OUT/$name"
    rm -f "$OUT/${name%.exe}.pdb" "$OUT"/*.lib
    echo "built $OUT/$name"
done
