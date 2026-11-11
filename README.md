# Kingdom 3DS

A Nintendo 3DS version of **Kingdom**, the open-source Flash game by noio (Thomas van den Berg, music by Pez).
The game's ActionScript 3 code is translated **line by line to C**, so its logic matches the original, and it adds
stereoscopic 3D, a water reflection that continues on the lower screen, and 3DS controls. It is built for Old and
New Nintendo 3DS (tested in the Azahar emulator; not yet reported on real hardware).

Everything the game needs (images, music, sounds, fonts, level data) is taken from the
[original repository](https://github.com/noio/kingdom) and **embedded in the .3dsx**. There is nothing else to
copy to the SD card.

## Play

1. Download **kingdom.3dsx** from [Releases](https://github.com/Reqrefusion/kingdom-3ds/releases).
2. Copy it to **SD:/3ds/kingdom.3dsx**.
3. Start **Kingdom** from the Homebrew Launcher.

Sound needs the DSP firmware dump of your own console at `SD:/3ds/dspfirm.cdc` (made with the DSP1 homebrew).
Without it the game runs silent and the lower screen says so.

Saves, settings and the log are stored in **SD:/3ds/kingdom-3ds/**. Keep that folder when you update; to update,
replace only the .3dsx.

## Controls

| | |
| --- | --- |
| D-pad or Circle Pad left / right | ride |
| D-pad down, Circle Pad down or A | drop a coin (give money) |
| Title screen: A or touch | new game |
| Title screen: Y | continue from the last daybreak |
| START | pause menu: 3D depth, 3D water, music, top screen size, new game |
| L + R (held) | show the developer statistics on the lower screen |
| 3D slider | stereoscopic depth of the top screen |

The lower screen continues the water reflection of the top screen, shows your coin pouch (top right) and a picture
of what your next coin goes to (the building you stand at, or the nearest beggar).

## Build

You need devkitPro (devkitARM, libctru, Citro2D, Citro3D), Python 3.9+, Pillow and `ffmpeg`.

```sh
python tools/build_release.py --sdk C:/devkitPro      # Linux: python3 ... --sdk /opt/devkitpro
```

or double-click **BUILD.cmd** on Windows. The script downloads the original repository at the pinned commit,
checks the SHA-256 of every file it uses (`assets.sha256.json`), converts the media, compiles the game and packs the
media into `release/kingdom.3dsx`. GitHub Actions does the same on every push. See [docs/BUILDING.md](docs/BUILDING.md).

## How it is built

* `game/`, `flixel/`: the game and the Flixel 2.55 engine, one C file per ActionScript file, same statement order.
* `flash/`, `as3/`: the part of Flash Player the game uses (bitmaps, filters, sound) and AS3 number semantics.
* `platform/`: the 3DS program (citro2d, audio, menu, saves, 3D, water, lower-screen picture) and a PC test runner.
* `tools/`: converters, build, and the comparison tools that check the C against the ActionScript **and against the
  shipped SWF's bytecode**. `python tools/port.py check` runs them.

[PORTING_GUIDE.md](PORTING_GUIDE.md) explains how a Flash game is translated this way and what to watch for;
[docs/TECHNICAL.md](docs/TECHNICAL.md) and [docs/DEVELOPMENT_NOTES.md](docs/DEVELOPMENT_NOTES.md) have the details.

## License

Kingdom, its code and its assets are © 2013 Thomas "noio" van den Berg and are used under the license of the
original repository, which applies to this port as well: **non-commercial use only**, no advertisements or
in-game purchases, not for phones and tablets, and the license must be included in every copy. It is in
[LICENSE](LICENSE) and inside the .3dsx as `romfs:/LICENSE.txt`. See [COPYRIGHT.md](COPYRIGHT.md).
