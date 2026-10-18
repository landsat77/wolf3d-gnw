# Wolfenstein 3D for Game & Watch Retro-Go SD

id Software's **Wolfenstein 3D** (1992) running on the Nintendo Game & Watch
(Mario / Zelda) as a **Retro-Go SD homebrew** (`/homebrews/Wolf3D.bin`,
firmware v2.0.0-rc1 or later).

The game engine is **Wolf4SDL** (Moritz "Ripper" Kroll and the community,
from id Software's source code), from
[KS-Presto/Wolf4SDL](https://github.com/KS-Presto/Wolf4SDL), ported to the
console's 1 MB of RAM and 280 MHz Cortex-M7.

**The game data is not included.** You need the full version of Wolfenstein
3D, v1.4 (the `*.WL6` files: Steam, GOG, the Activision / id CD...).

## Installation

See [INSTALL.md](INSTALL.md) (English and French). In short:

1. Build the data pack from the `*.WL6` files (Python 3 only):

   ```
   python3 wf_pack.py Wolf3D.wf /path/to/wolf3d
   ```

   On Windows: `py wf_pack.py Wolf3D.wf "C:\Program Files (x86)\Steam\steamapps\common\Wolfenstein 3D\base"`

2. Copy both files to the SD card, side by side, with the same name:

   ```
   /homebrews/Wolf3D.bin
   /homebrews/Wolf3D.wf
   ```

3. Start "Wolfenstein 3D" from the Homebrew tab. The first start copies the
   pack to the external flash of the console (a minute or so).

### What wf_pack.py does

The pack holds the game data ready to be used in place from the external
flash: the pictures decompressed, the walls, sprites and sounds laid out like
the game loads them. The AdLib music and sound effects are synthesized by an
OPL chip emulator on the PC versions: too slow for the console. So
`wf_pack.py` runs the game's sequencer with an OPL emulator on your computer
(`wf_render`, shipped for Windows, macOS and Linux in `wf_render/` next to the
script) and stores the songs as samples, with their exact loops.

| Option | Pack size | Music |
|---|---|---|
| *(default)* | ~13 MB | IMA ADPCM, 11 kHz |
| `--music-codec pcm` | ~45 MB | 16 bit PCM, 11 kHz (no hiss) |
| `--no-music` | ~2.5 MB | none (no AdLib sound effects either) |

## Controls

| Game & Watch | In a level | In the menus |
|---|---|---|
| D-pad | Move / turn | Move |
| A | Fire | Validate (Enter, "Y") |
| B | Open doors, push walls | Back (Esc, "N") |
| B held + left / right | Strafe | — |
| TIME (SELECT on Zelda units) | Next weapon | — |
| GAME (START on Zelda units) | Game menu (Esc) | Back (Esc) |
| PAUSE/SET | Retro-Go menu | Retro-Go menu |

The Retro-Go menu adds savestates and, in *Options*: music, sound effects,
"Swap A/B" and "Always run" (on by default).

**Savestates** save the level being played, as the game's own saved games
do (they work in a level only). The game's own Load / Save works too: the
name of a saved game is "Episode N Floor M" (validate with A, or edit it
with the D-pad: up / down change the letter, left / right move).

## Building

Needs `arm-none-eabi-gcc` (hard-float, v10+), GNU Make, Python 3 + Pillow:

```
make            # → Wolf3D.bin
make docker     # same, inside the Retro-Go SD builder image
```

`wf_render` (tools/render) is cross-built with zig:

```
pip install ziglang
tools/build_renderers.sh    # → tools/bin/wf_render-<os>-<arch>
```

### Testing without hardware

`harness/` builds the same game and port code for Linux, headless, with a
virtual clock, scripted buttons, PPM screenshots and WAV output (and
`wf_render`):

```
make -C harness
python3 tools/wf_pack.py Wolf3D.wf /path/to/wolf3d --renderer harness/build/wf_render
harness/build/wf_host Wolf3D.wf -t 30000 -e 1000 -o /tmp/out -w /tmp/out/a.wav
harness/build/wf_host Wolf3D.wf -s "3000:A:1,3200:A:0,5000:A:1,5200:A:0" -S 6000 -o /tmp/out
```

`-q file -Q ms` writes a savestate, `-j file -J ms` loads one, `-L file`
restores one at start, `-H kb` sets the heap, `-r` always run, `-x` swap
A/B. `make -C harness SAN=1` builds it with AddressSanitizer and UBSan. The
pack is mapped read-only, like the flash.

## Layout

| Path | Role |
|------|------|
| `src/wolf/` | Wolf4SDL (KS-Presto/Wolf4SDL @ dc8b250). Game & Watch changes are under `#ifdef GNW`. |
| `src/port/sdl/`, `wf_sdl.c` | The small SDL 2: 8 bit surfaces, the screen, keys from the buttons, time. |
| `src/port/wf_sound.c`, `wf_music.c` | The sound manager (replaces id_sd.c): digitized sounds in place, pre-rendered AdLib music and sounds. |
| `src/port/wf_state.c` | Savestates. |
| `src/port/wf_file.c`, `wf_pack.c` | Files of the game: in the pack (flash) or on the SD card. |
| `src/port/wf_main.c` | Retro-Go side: entry point, display (LUT8), audio, buttons, pause menu. |
| `tools/wf_pack.py` | Builds the `.wf` pack. |
| `tools/render/` | `wf_render`: the AdLib music and sound effects to samples (OPL emulator of the DOSBox team, LGPL 2.1). |
| `harness/` | Headless test harness. |
| `sdk/` | Retro-Go SD core / homebrew SDK (from retro-go-sd-templates). |

See `CLAUDE.md` for the internals.

## License

GPLv2 (see `LICENSE`): Wolf4SDL's code from id Software is available under
the id license or the GPL, at your choice (`src/wolf/README.TXT`); this port
uses the GPL. The MAME OPL emulator of Wolf4SDL is not used. The OPL emulator of
`tools/render` is LGPL 2.1. The SDK under `sdk/` is MIT (`sdk/LICENSE.MIT`).
Wolfenstein 3D's data files belong to their owners and are not included.
