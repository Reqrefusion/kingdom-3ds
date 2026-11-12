# Kingdom 3DS

Nintendo 3DS port of **Kingdom** by noio (music by Pez). The original ActionScript is translated line by line to C,
with stereoscopic 3D, a water reflection that continues on the lower screen, and 3DS controls.
Works on Old and New 3DS.

All media is taken from the [original repository](https://github.com/noio/kingdom) and embedded in the .3dsx.

## Play

1. Download **kingdom.3dsx** from [Releases](https://github.com/Reqrefusion/kingdom-3ds/releases) and copy it to `SD:/3ds/`.
2. Start it from the Homebrew Launcher.

Sound needs `SD:/3ds/dspfirm.cdc` (dumped from your own console). Saves and settings go to `SD:/3ds/kingdom-3ds/`.

| | |
| --- | --- |
| D-pad / Circle Pad | ride, drop a coin (down or A) |
| Title: A or touch / Y | new game / continue |
| START | menu (3D depth, water, music, screen size) |
| L + R | developer statistics |

## Build

```sh
python tools/build_release.py --sdk C:/devkitPro
```

Needs devkitPro (devkitARM, libctru, Citro2D, Citro3D), Python 3.9+, Pillow and ffmpeg. Details: [docs/BUILDING.md](docs/BUILDING.md).
Translating other Flash games the same way: [docs/PORTING_GUIDE.md](docs/PORTING_GUIDE.md).

## License

Kingdom and its assets are © 2013 Thomas "noio" van den Berg. This port is under the original license
([LICENSE](LICENSE)): non-commercial, not for mobile phones, license included in every copy. See [COPYRIGHT.md](COPYRIGHT.md).
