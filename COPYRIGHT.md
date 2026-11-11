# Copyright and license scope

**Kingdom** (game design, ActionScript code, artwork, music, sounds, level data) is © 2013 Thomas "noio" van den
Berg, music by Pez. It is published as open source at <https://github.com/noio/kingdom> under the license in
[LICENSE](LICENSE). That license grants the right to copy, modify, distribute and publish the code *and the
accompanying assets*, under these conditions:

* no commercial use (no sale, no advertisements, no in-game purchases);
* not for mobile devices such as iPhone, iPad, Windows Phone and Android phones (publication on mobile platforms in a
  modified form only with the copyright holder's permission);
* the license must be included in all copies.

This port is a modified copy of Kingdom (the code is a line-by-line C translation of the original ActionScript), so
the same license and conditions apply to everything in this repository that is derived from Kingdom: the files in
`game/`, `flixel/`, the converted media and the level data. The license text is copied into the .3dsx
(`romfs:/LICENSE.txt`) and the media are fetched from the original repository at build time, not stored here.
The Nintendo 3DS is a handheld game console, not one of the phone platforms named above; for any other use see the
license or ask the author.

## Other components

* **Flixel 2.55** (Adam Atomic) is the game engine of the original; `flixel/` is its C translation. Its license
  terms are those of the original repository's copy of Flixel (`org/flixel/` there).
* **Fonts** `04b03` and `nokiafc22` come with the original repository and keep their own terms (see the original).
* **`vendor/engine/`**: five small helper files (AS3 number coercion, a worker-thread slot) from the port author's own
  engine snapshot, included under this repository's terms.
* **devkitPro, libctru, Citro2D, Citro3D** are used to build and keep their licenses; none of their code is stored here.
* `as3/`, `flash/` (a re-implementation of the parts of Flash Player the game uses), `platform/` and `tools/` were
  written for this port.
