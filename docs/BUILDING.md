# Building

```sh
python tools/build_release.py --sdk C:/devkitPro        # Linux: python3 ... --sdk /opt/devkitpro
```

Needs devkitPro (devkitARM, libctru, Citro2D, Citro3D: `dkp-pacman -S 3ds-citro2d 3ds-citro3d`), Python 3.9+,
Pillow and `ffmpeg`. Result: `release/kingdom.3dsx` (about 42 MB, media inside).

The script downloads `noio/kingdom` at the commit pinned in `port.json`, checks the SHA-256 of every file it uses
(`assets.sha256.json`), converts the media, builds the game and packs the media into the .3dsx (RomFS).

Checks on PC: `python tools/port.py check --quick`.
