#!/usr/bin/env python3
"""Build the game data pack of the Wolfenstein 3D homebrew.

    python3 wf_pack.py Wolf3D.wf /path/to/wolf3d

The folder holds the data files of the full version of Wolfenstein 3D
(v1.4, *.WL6: Steam, GOG, CD...): AUDIOHED, AUDIOT, GAMEMAPS, MAPHEAD,
VGADICT, VGAHEAD, VGAGRAPH, VSWAP. The pack holds them ready to be used in
place from the external flash of the console:

  - vgagraph.wl6: the pictures, fonts and texts, decompressed (Huffman) and
    "deplaned" like Wolf4SDL does it when it starts (no copy in RAM);
  - vswap.wl6: the walls, sprites and sounds, aligned like Wolf4SDL loads
    them (PM_Startup);
  - gamemaps / maphead / audiohed / audiot: as they are;
  - musicNN.mus / adlibNN.mus: the AdLib songs and sound effects rendered to
    samples by wf_render (wf_render/ next to this script): the console is too
    small to run an OPL emulator while the game plays.

Pack format: src/port/wf_pack.c. Only Python 3 is needed (no extra module).
"""
import argparse
import os
import platform
import shutil
import struct
import subprocess
import sys
import tempfile

MAGIC = b"GWWF01\0\0"
ENTRY_NAME_LEN = 24
NAMES = ["audiohed", "audiot", "gamemaps", "maphead", "vgadict", "vgahead", "vgagraph", "vswap"]

# Wolfenstein 3D v1.4 full version (gfxv_wl6.h, audiowl6.h of Wolf4SDL)
WL6 = {
    "NUMCHUNKS": 149, "NUMPICS": 132, "STRUCTPIC": 0, "STARTPICS": 3,
    "STARTTILE8": 135, "NUMTILE8": 35, "STARTEXTERNS": 136, "NUMSOUNDS": 87,
}


def find_files(folder, ext):
    found = {}
    for entry in os.listdir(folder):
        path = os.path.join(folder, entry)
        base, dot, e = entry.lower().rpartition(".")
        if os.path.isfile(path) and dot and e == ext and base in NAMES:
            found[base] = path
    return found


def read(path):
    with open(path, "rb") as fh:
        return fh.read()


# ---- VGAGRAPH: Huffman, deplaning (id_ca.c / id_vl.c of Wolf4SDL) ----
def huff_expand(src, length, nodes):
    """CAL_HuffExpand: nodes[i] = (bit0, bit1), head node 254."""
    out = bytearray(length)
    head = nodes[254]
    node = head
    written = 0
    pos = 0
    n = len(src)
    while written < length and pos < n:
        val = src[pos]
        pos += 1
        for bit in range(8):
            nv = node[1] if (val >> bit) & 1 else node[0]
            if nv < 256:
                out[written] = nv
                written += 1
                node = head
                if written >= length:
                    break
            else:
                node = nodes[nv - 256]
    return bytes(out)


def deplane(data, width, height):
    """VL_DePlaneVGA: four planes of width/4 columns -> linear pixels."""
    size = width * height
    pw = width >> 2
    out = bytearray(size)
    src = 0
    for plane in range(4):
        for y in range(height):
            row = y * width + plane
            out[row:row + width:4] = data[src:src + pw]
            src += pw
    return bytes(out)


def build_vgagraph(head, dict_data, graph, k):
    count = k["NUMCHUNKS"]
    starts = []
    for i in range(len(head) // 3):
        v = head[3 * i] | (head[3 * i + 1] << 8) | (head[3 * i + 2] << 16)
        starts.append(-1 if v == 0xFFFFFF else v)
    if len(starts) != count + 1:
        sys.exit("error: VGAHEAD has %d chunks instead of %d: these are not the data files of "
                 "Wolfenstein 3D v1.4 (full version, *.WL6)" % (len(starts) - 1, count))
    nodes = [struct.unpack_from("<HH", dict_data, 4 * i) for i in range(255)]

    def compressed(chunk):
        pos = starts[chunk]
        nxt = chunk + 1
        while starts[nxt] == -1:
            nxt += 1
        return graph[pos:starts[nxt]]

    chunks = [None] * count
    # the picture sizes (STRUCTPIC)
    src = compressed(k["STRUCTPIC"])
    pictable_raw = huff_expand(src[4:], k["NUMPICS"] * 4, nodes)
    chunks[k["STRUCTPIC"]] = pictable_raw
    pictable = [struct.unpack_from("<hh", pictable_raw, 4 * i) for i in range(k["NUMPICS"])]
    for chunk in range(k["STRUCTPIC"] + 1, count):
        if starts[chunk] < 0:
            continue
        src = compressed(chunk)
        if k["STARTTILE8"] <= chunk < k["STARTEXTERNS"]:
            # tile 8s are all in one chunk (no other tiles in WL6)
            expanded = 64 * k["NUMTILE8"]
            data = huff_expand(src, expanded, nodes)
        else:
            expanded = struct.unpack_from("<i", src, 0)[0]
            data = huff_expand(src[4:], expanded, nodes)
        if k["STARTPICS"] <= chunk < k["STARTEXTERNS"]:
            if chunk == k["STARTTILE8"]:
                data = b"".join(deplane(data[64 * i:64 * i + 64], 8, 8) for i in range(k["NUMTILE8"]))
            else:
                w, h = pictable[chunk - k["STARTPICS"]]
                if w & 3 or w * h > len(data):
                    sys.exit("error: picture %d: bad size %dx%d" % (chunk, w, h))
                data = deplane(data[:w * h], w, h) + data[w * h:]
        chunks[chunk] = data
    # "GWVG", u32 count, u32 offset[count], u32 size[count], data (4 aligned)
    header = 8 + 8 * count
    offsets, sizes, blob = [], [], bytearray()
    pos = header
    for data in chunks:
        if data is None:
            offsets.append(0)
            sizes.append(0)
            continue
        offsets.append(pos)
        sizes.append(len(data))
        blob += data
        pad = (-len(blob)) % 4
        blob += b"\0" * pad
        pos = header + len(blob)
    out = b"GWVG" + struct.pack("<I", count) + struct.pack("<%dI" % count, *offsets) \
        + struct.pack("<%dI" % count, *sizes) + bytes(blob)
    return out


# ---- VSWAP: pages aligned like PM_Startup (id_pm.c) ----
def build_vswap(vswap):
    chunks, sprite_start, sound_start = struct.unpack_from("<HHH", vswap, 0)
    offsets = list(struct.unpack_from("<%dI" % chunks, vswap, 6))
    lengths = list(struct.unpack_from("<%dH" % chunks, vswap, 6 + 4 * chunks))
    filesize = len(vswap)
    offsets.append(filesize)
    data = bytearray()
    page_pos = []
    sound_info_padded = 0
    for i in range(chunks):
        if (sprite_start <= i < sound_start) or i == chunks - 1:
            if len(data) & 1:
                data.append(0)
                if i == chunks - 1:
                    sound_info_padded = 1
        page_pos.append(len(data))
        if not offsets[i]:
            continue  # sparse page
        if not offsets[i + 1]:
            size = lengths[i]
        else:
            size = offsets[i + 1] - offsets[i]
        data += vswap[offsets[i]:offsets[i] + size]
    page_pos.append(len(data))
    # "GWPM", u16 chunks, sprite start, sound start, sound info padded,
    # u32 page[chunks + 1] (from the data), data (4 aligned)
    header = 4 + 8 + 4 * (chunks + 1)
    pad = (-header) % 4
    out = b"GWPM" + struct.pack("<HHHH", chunks, sprite_start, sound_start, sound_info_padded) \
        + struct.pack("<%dI" % (chunks + 1), *page_pos) + b"\0" * pad + bytes(data)
    return out


# ---- the pack ----
def write_pack(out_path, files, verbose=True):
    header_size = 16 + len(files) * (ENTRY_NAME_LEN + 8)
    offset = (header_size + 3) & ~3
    entries = []
    for name, blob in files:
        entries.append((name, offset, len(blob)))
        offset = (offset + len(blob) + 3) & ~3
    total = offset
    out = bytearray(total)
    out[0:8] = MAGIC
    struct.pack_into("<II", out, 8, len(files), total)
    for i, (name, off, size) in enumerate(entries):
        nb = name.encode("ascii")[:ENTRY_NAME_LEN - 1]
        struct.pack_into("<%dsII" % ENTRY_NAME_LEN, out, 16 + i * (ENTRY_NAME_LEN + 8), nb, off, size)
    for (name, blob), (_, off, size) in zip(files, entries):
        out[off:off + size] = blob
    with open(out_path, "wb") as fh:
        fh.write(out)
    if verbose:
        print("wrote %s: %d files, %d bytes" % (out_path, len(files), total))
    return total


def default_renderer():
    """wf_render-<os>-<arch> in wf_render/ next to this script (release zip)
    or in tools/bin/ (tools/build_renderers.sh), else the one built by
    'make -C harness'."""
    here = os.path.dirname(os.path.abspath(__file__))
    system = {"Linux": "linux", "Windows": "windows", "Darwin": "macos"}.get(platform.system())
    machine = platform.machine().lower()
    arch = "arm64" if machine in ("arm64", "aarch64") else "x86_64"
    candidates = []
    if system:
        names = ["wf_render-%s-%s%s" % (system, arch, ".exe" if system == "windows" else "")]
        if system == "windows" and arch == "arm64":  # x86_64 emulation
            names.append("wf_render-windows-x86_64.exe")
        for n in names:
            candidates += [os.path.join(here, "wf_render", n), os.path.join(here, "bin", n),
                           os.path.join(here, n)]
    candidates += [os.path.join(here, "..", "harness", "build", "wf_render")]
    for p in candidates:
        if os.path.isfile(p):
            prepare_executable(p)
            return p
    return None


def prepare_executable(path):
    """Make a renderer from a zip file runnable (exec bit, macOS quarantine)."""
    if os.name != "nt" and not os.access(path, os.X_OK):
        try:
            os.chmod(path, os.stat(path).st_mode | 0o111)
        except OSError:
            pass
    if platform.system() == "Darwin":
        subprocess.run(["xattr", "-d", "com.apple.quarantine", path],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def render_audio(paths, renderer, codec, rate, sounds, verbose=True):
    tmp = tempfile.mkdtemp(prefix="wf_pack_")
    try:
        args = [renderer, paths["audiohed"], paths["audiot"], tmp, "--sounds", str(sounds),
                "--rate", str(rate)] + (["--pcm"] if codec == "pcm" else [])
        r = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           universal_newlines=True)
        if r.returncode != 0:
            sys.stderr.write(r.stdout[-4000:])
            sys.exit("error: rendering the music failed (%s)" % renderer)
        result = []
        size = 0
        for name in sorted(os.listdir(tmp)):
            if name.endswith(".mus"):
                data = read(os.path.join(tmp, name))
                result.append((name, data))
                size += len(data)
        if verbose:
            songs = sum(1 for n, _ in result if n.startswith("music"))
            print("music: %d songs, %d AdLib sounds, %s %d Hz, %d bytes" %
                  (songs, len(result) - songs, codec.upper(), rate, size))
        return result
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("output", help="pack to write (same name as the .bin, extension .wf)")
    ap.add_argument("folder", help="folder of Wolfenstein 3D (the *.WL6 files)")
    ap.add_argument("--music-rate", type=int, choices=(11025, 22050), default=11025,
                    help="rate of the pre-rendered music (default 11025)")
    ap.add_argument("--music-codec", choices=("pcm", "adpcm"), default="adpcm",
                    help="adpcm (default, 4 times smaller) or pcm (16 bit, no hiss)")
    ap.add_argument("--no-music", action="store_true", help="no music and no AdLib sounds (smaller pack)")
    ap.add_argument("--renderer", help="wf_render program (wf_render/ next to this script by default)")
    ap.add_argument("-q", "--quiet", action="store_true")
    args = ap.parse_args()
    if not os.path.isdir(args.folder):
        sys.exit("error: %s is not a folder" % args.folder)
    paths = find_files(args.folder, "wl6")
    if len(paths) < len(NAMES):
        sub = os.path.join(args.folder, "base")  # Steam: <game>/base
        if os.path.isdir(sub):
            paths = find_files(sub, "wl6")
    missing = [n.upper() + ".WL6" for n in NAMES if n not in paths]
    if missing:
        if find_files(args.folder, "wl1"):
            sys.exit("error: this is the shareware version (*.WL1): the homebrew needs the full version (*.WL6)")
        sys.exit("error: missing game data: %s (is this the folder of Wolfenstein 3D?)" % ", ".join(missing))
    k = WL6
    verbose = not args.quiet
    vgagraph = build_vgagraph(read(paths["vgahead"]), read(paths["vgadict"]), read(paths["vgagraph"]), k)
    vswap = build_vswap(read(paths["vswap"]))
    items = [
        ("audiohed.wl6", read(paths["audiohed"])),
        ("audiot.wl6", read(paths["audiot"])),
        ("gamemaps.wl6", read(paths["gamemaps"])),
        ("maphead.wl6", read(paths["maphead"])),
        ("vgagraph.wl6", vgagraph),
        ("vswap.wl6", vswap),
    ]
    if verbose:
        print("graphics: %d bytes, pages: %d bytes" % (len(vgagraph), len(vswap)))
    if not args.no_music:
        renderer = args.renderer or default_renderer()
        if not renderer or not os.path.isfile(renderer):
            sys.exit("error: wf_render not found (wf_render/ next to this script, 'make -C harness' or --renderer)")
        items += render_audio(paths, renderer, args.music_codec, args.music_rate, k["NUMSOUNDS"], verbose)
    write_pack(args.output, sorted(items), verbose=verbose)


if __name__ == "__main__":
    main()
