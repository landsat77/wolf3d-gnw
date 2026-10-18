# Changelog

This file follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). Release tags must
match a section heading exactly (for example `v1.0.0`): CI uses the matching
section as the GitHub Release notes.

## [Unreleased]

## [v1.0.0] - 2026-10-18

### Added

- Wolfenstein 3D (full version v1.4, `*.WL6`) as a Retro-Go SD GWHB homebrew,
  built on Wolf4SDL and the retro-go-sd-templates SDK (firmware ABI v2,
  Retro-Go SD v2.0.0-rc1 or later), on a small SDL 2 written for the
  console.
- Game data from a `.wf` pack next to the `.bin`, used in place from the
  external flash (`tools/wf_pack.py`: pictures decompressed, pages laid out
  like the game loads them), with the AdLib music and sound effects
  pre-rendered on the PC (`wf_render` for Windows, macOS and Linux).
- Savestates of the level being played (the game's saved games), resume
  from the launcher.
- Pause menu options: music, sound effects, swap A/B, always run.
- Names of saved games without a keyboard ("Episode N Floor M", editable
  with the D-pad).
