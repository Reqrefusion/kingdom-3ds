# Building

## One command

```sh
python tools/build_release.py --sdk C:/devkitPro        # Windows
python3 tools/build_release.py --sdk /opt/devkitpro      # Linux
```

Needs devkitPro with devkitARM, libctru, Citro2D and Citro3D (install missing ones with `dkp-pacman -S 3ds-citro2d
3ds-citro3d`), Python 3.9+, Pillow (`pip install pillow`) and `ffmpeg` on `PATH`. Result: `release/kingdom.3dsx`
(about 42 MB, because the media are inside).

## What happens

1. `tools/fetch_assets.py` downloads `noio/kingdom` at the commit pinned in `port.json` (cached in `work/`).
   It refuses to continue unless the SHA-256 of all 73 files it uses matches `assets.sha256.json`.
2. `tools/convert_assets.py` converts them into `translit_assets/`:
   PNG to raw ARGB, MP3 to raw 16-bit mono 22.05 kHz PCM (ffmpeg), fonts to glyph sheets, the level and the
   weather presets to C data (`game/level_fields.c`, `game/WeatherPresets.c`; regenerating them must not change
   the committed files). The original `LICENSE.txt` is copied next to them.
3. `tools/build_3ds.py` compiles `flash/ flixel/ game/ platform/ vendor/` with devkitARM and packs `translit_assets/`
   into the .3dsx as RomFS: the game reads `romfs:/assets/...`. If a build has no RomFS the game falls back to the same
   files in `SD:/3ds/kingdom-3ds/`.

Saves, settings and the log are never inside the .3dsx: they are in `SD:/3ds/kingdom-3ds/`.

## Checks (PC)

```sh
python tools/port.py doctor        # tools and paths
python tools/port.py check --quick # parity boundary, C vs ActionScript, direct source-derived tests
```

The full `check` also builds a PC runner with MSVC (Windows), compares the CPU output with `tests/baseline`, and
compares the C with the ActionScript statement by statement. `swf-diff` compares against the shipped SWF when you
provide one in `port.json` (`swf`). See [PORTING_GUIDE.md](../PORTING_GUIDE.md).

## Emulator

`python tools/port.py deploy` copies the .3dsx to the Azahar SD folder set in `port.json`; `python tools/port.py
azahar "seed 0.5; freeze 600; progress D2 A7 X1000 B2 P0 F1 H2 W000011 C0 G4 S00" --out shot.png` runs a scripted
scene and takes a screenshot (it never touches another running Azahar).

## GitHub Actions

*Actions > Build 3DSX* builds the file on every push and uploads it as an artifact. Pushes to the default branch
update the `latest` prerelease; tags create named releases; *Run workflow* lets you name a release.
