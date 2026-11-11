# Kingdom → C transliteration (3DS)

Line-by-line C translation of the open-source Kingdom
(noio/kingdom @ `56d4e622461b72df8fa8f69d09fbd63dab7a1346`, AS3 + Flixel 2.55),
written so that every function can be compared statically with its AS3 original.

**Porting another Flash game with this system?** Read `PORTING_GUIDE.md` (how AS3 is translated, what to
watch for) and `AGENTS.md` (short contract for AI helpers). One entry point for everything:
`python tools/port.py doctor | check | swf-diff | abc | build-3ds | deploy | azahar | ruffle`.
Project settings live in `port.json`; reviewed/accepted differences in `tools/known_diffs.json`;
screenshots and captures go in `captures/`.

```
as3/      AS3 language semantics (Number, int/uint casts, >>/>>>, Array, vtables)
flash/    software Flash Player subset: BitmapData, Matrix, ColorTransform,
          filters (perlinNoise, DisplacementMap, ColorMatrix), Shape gradients,
          TextField, Sound/SoundChannel
flixel/   Flixel 2.55 classes, one file per .as (FlxObject.c ↔ FlxObject.as ...)
game/     the game, one file per .as (Player.c ↔ Player.as ...)
platform/ host/ = headless Linux/Windows runner, 3ds/ = citro2d + NDSP host
tools/    asset converter, static comparison, 3DS build and SD deploy
tests/    Flixel separation test against ../tests/flixel_parity.py
```

## Translation rules

* One `.c` per `.as`; method `Class.method` becomes `Class_method(self, ...)`,
  statements keep their original order, names and constants.
* Inheritance: `*_FIELDS` macros place the base fields first, so `self->x`
  works for every subclass. Overridable methods go through `As3VTable`
  (`CALL0(obj, update)`); `SUPER(Class_method)(self)` marks `super.method()`.
* AS3 semantics are explicit: `Number` is `double`, `int(x)`/`uint(x)` are
  `AS3_INT`/`AS3_UINT` (from `../source/engine/adapters/flash/numeric.h`),
  `>>`/`>>>` are `AS3_SAR`/`AS3_SHR`, field initialisers run before `super()`.
  Where C evaluates arguments in unspecified order and AS3 is left-to-right
  (several `FlxG.random()` in one call) temporaries keep the AS3 order.
* `FlxG.random()` is Flixel's own LCG (`FlxU.srand`), identical to the SWF.
  `Math.random()` uses a seeded xorshift (Flash's generator is not public).
* Anything that cannot be verified against Flash Player is marked
  `ADAPTATION:` in a comment. `grep -rn ADAPTATION` lists them all.

### Deliberately omitted (not reachable on 3DS)

Mochi leaderboard in `GameOverState`, `navigateToURL` in `MenuState`,
`PlayState.setWeatherFromInput`, the debug `Minimap`, `FlxG.watch`, the
S-key fullscreen toggle, and `Utils` helpers the game never calls.

## Carried over from the previous native prototype

| Prototype feature (../source) | Here |
|---|---|
| stereo scenery layers, 3D slider | `platform/common/stereo.c`: one draw pass feeds both eyes; depth from each object's own `scrollFactor` (Sky/SunMoon/Haze far, gameplay plane at the screen, fog in front). Left eye and game state are bit-identical with stereo on or off (checked on host). |
| Start pause menu, `settings.cfg` | `platform/common/session.c`: Resume / Music / 3D depth / Top screen / New game / Save and exit, drawn with the translated TextField |
| music on/off | `platform/3ds/media.c` silences `/assets/music/*` voices |
| top screen 1.5x | menu option "Top screen: fill (1.5x)" (crops 10 px per side); default shows the whole frame |
| progress kept across launches | the game's own `PlayState.savedProgress` (daybreak checkpoint) is written to `progress.sav`; the next launch starts `new PlayState(saved)`, the same path as the original crown-loss retry |
| New 3DS speedup, 128 KB stack, error screen when data is missing, `runtime.log` | `platform/3ds/main.c` |
| deploy keeps saves/settings/logs | `tools/deploy_sd.py` |
| `tests/check.py` behavioural oracles | `tests/run_native_oracles.py` + `tests/translit_scenarios.c` (below) |

Not carried over, on purpose: the touch "pay this target" button and coin
dropping before the first recruit (not in the original game), mid-day resume snapshots (the original only has the
daybreak checkpoint), GPU atlas rendering (would replace the translated Flash
renderer; revisit only if `runtime.log` shows it is needed).

## Windows host build and checks (no gcc needed)

```
python tools/build_host.py                      # kingdom_host.exe with the installed MSVC Build Tools
python tests/regress.py --exe ./kingdom_host.exe   # CPU = baseline pixel for pixel, GPU within 6/255
python tests/run_native_oracles.py              # oracles + direct source-derived checks (msvc is the Windows default)
```

`platform/host/main.c` writes PNGs itself unless `KINGDOM_HOST_LIBPNG` is
defined (the `host.mk` gcc build). The MSVC build reproduces the Linux/gcc
`tests/baseline` trace.csv exactly. `tests/run_native_oracles.py` also runs the
direct checks in `tests/translit_scenarios.c` (work dispatch, castle guards,
shops, bunnies, trolls; expectations written from the pinned ActionScript) and
prints them as PASS/FAIL lines.

3DS visual checks run in Azahar: `tools/azahar_autotest.ps1` (fixed seed,
scripted input, `cpu 0|1`, `progress ...`, `freeze N`) and
`tools/compare_shot.py` / `tools/phase_sweep.py` compare the GPU path with the
CPU compositor (the translation as written). See `CLAUDE.md`.

## Behavioural oracles from ../tests/check.py

```
python tests/run_native_oracles.py
```

builds `tests/translit_scenarios.c` (same scenarios and CSV columns as
`../tests/core_test.c`, driven through the real translated PlayState) and runs
the unchanged `compare_*` functions of `../tests/check.py` on them, twice:

* **Flixel-exact** - the game as translated.
* **one-callback** - test switch `FlxQuadTree_testOneCallbackPerPair`.

| oracle | Flixel-exact | one-callback |
|---|---|---|
| V006 player motion | PASS (max error 3e-12) | PASS |
| V007 food / rest | PASS (error 0) | PASS |
| V011 Troll x Citizen matrix + hack cooldown | FAIL (4 of 12 rows) | PASS |
| V011 Farmer then Poor | FAIL | PASS |
| V011 Farmer, Poor and Player | PASS | PASS |
| V014 tax AABB edges + order | PASS | PASS |

The V011 differences are not translation errors. Flixel 2.55's
`FlxQuadTree.addObject` puts an object that straddles a quadrant boundary into
every quadrant it touches, and `execute` notifies a pair once per shared leaf.
With Kingdom's world (3840x300 from -10,-10, 6 divisions -> 240 x 18.75 leaves)
a citizen standing on the ground (y 108..132) always straddles y = 121.25, so a
troll touching it triggers `trollHit` twice per frame. The oracles model one
callback per pair; with that switch on, every row matches. An independent
Python model of `addObject` gives the same 2 shared leaves.

The remaining oracles (bunny, troll reuse, night waves/patrol/fog, shop,
guard, work dispatch) compare native-only fields (native render coordinates,
native RNG state, native timers) and need a field mapping each; they are not
bound yet.

## Performance

All Flash compositing runs on the CPU (software BitmapData), like the shared
engine's own blitters. Host profile before tuning: 98 % of the frame in pixel
compositing (alpha `over` 30 %, full-screen ColorMatrixFilter 19 %, blend
modes 12 %, copyPixels 10 %); ~214k blended pixels per frame (4.6x the screen).

Done (output bit-identical, checked with the 1800-frame trace + PNGs):
* opaque-destination paths in `over`/`blend` (the camera buffer is opaque):
  no runtime divisions (ARM11 has no divide), lane layout as the engine's
  `lume_blend_pixel` -> host 7.8 s -> 4.8 s per 1800 frames;
* ColorMatrixFilter with per-coefficient product tables, rows split over the
  shared engine's job slot (`platform/common/parallel.c` on
  `engine/platform/jobs_backend.h`; New 3DS core 2, Old 3DS single core);
* 3DS build at -O3.

### GPU path (3DS default)

`flash/gpu.[ch]` records every write into the camera buffer as a display list
instead of rasterising it; `platform/3ds/gpu.c` plays it on Citro2D using the
previous prototype's proven patterns (../source/assets.c: RGBA8 nearest/clamp
textures, `t = 1 - y/h`, 512x256 VRAM render targets with C3D_FrameSplit, and
its Weather TEV stages / kg_weather_matrix for `Weather.ambientTransform`).
One texture per BitmapData, re-uploaded only when its `gen` changes; textures
are premultiplied so normal / multiply / screen are single blend equations.
Water's reflection is a flipped GPU copy of the scene (no ripple). Stereo uses
the recorded per-object depth. Game state is unchanged (trace identical);
pixels differ slightly from the CPU compositor. Host: `--gpu 1` records the
lists and draws them with the CPU reference player (`Gpu_playSoftware`).
CPU cost per frame on host: 4.3 ms (start) -> 2.7 ms (CPU tuning) -> 0.9 ms
(GPU path). If `Gpu3ds_init` fails the CPU compositor is used.

## Static comparison

```
python tools/side_by_side.py --source <original source>
```

writes `STATIC_COMPARISON.md`: every AS3 function next to its C translation,
statement by statement after normalisation (types, `self->`, casts).
Current result: **4125 / 4684 statements identical (88.1 %), 523 functions**
(the 162 AS3 functions without a counterpart are methods the game never calls —
43 unused ColorMatrix operations, unused FlxG/FlxTilemap/FlxU/FlxCamera helpers —
plus the omissions below; the list is at the end of STATIC_COMPARISON.md).
The remaining differences are mostly syntax the normaliser does not fold
(`for each`, array literals, string concatenation, vtable calls).

## Assets

The assets are not copied into the repo; they are generated from the original
source tree:

```
python tools/convert_assets.py --source ../work/kingdom-source/<...>/kingdom-56d4e622... [--audio]
```

* PNG → `translit_assets/assets/gfx/*.png.argb` (raw ARGB, width/height header)
* TTF → `translit_assets/fonts/{system,04b03}_{8,16,32}.font`
* `fields.oel` → `game/level_fields.c`, `WeatherPresets.as` → `game/WeatherPresets.c`
* `--audio` (needs ffmpeg) → `*.mp3.pcm`, PCM16 mono 22050 Hz (~36 MB).
  On 3DS, files above 512 KB (music, ambient loops) are streamed from the SD
  card; short effects are kept in linear memory.

`translit_assets.zip` is a ready-made copy of this output (graphics, fonts, audio).

## Host build (checks without a 3DS)

```
make -f host.mk             # needs gcc, libpng
./kingdom_host --frames 1800 --script input.txt --png-every 300 --out run --assets translit_assets --seed 0.5
```

Script lines: `<frame> key <NAME> down|up`, `<frame> mouse down|up`.
Output: `run/trace.csv` (one line per frame: state, day, gold, citizens …)
and PNG snapshots of the camera buffer. Same seed + script → identical trace.

Flixel separation test (4096 cases, compared with the repo's reference):

```
python tests/flixel_separate_check.py
```

## 3DS

Double-click `build_translit.bat`: unzips the assets, builds with devkitPro
and installs into `%APPDATA%\Azahar\sdmc`; everything is logged to `build.log`.
By hand:

```
python tools/convert_assets.py --source <original source> --audio
python tools/build_3ds.py --sdk C:/devkitPro       # → out/kingdom_translit.3dsx
python tools/deploy_sd.py --sd-root "%APPDATA%/Azahar/sdmc"   # or the SD card root
```

Data goes to `sdmc:/3ds/kingdom_translit/`. Top screen: the game scaled to
400 px; bottom screen: in PlayState the deep water reflection (previous prototype view, platform/3ds/gpu.c draw_deep), otherwise the game 1:1; touch is the mouse.
Controls: D-pad/Circle Pad left-right walk/run, D-pad down / A drop a coin
(A also clicks in menus), START opens the pause menu, 3D slider = stereo. Every 300 frames the game writes fps and
game-update time to `sdmc:/3ds/kingdom_translit/runtime.log`.

**Status:** builds with devkitPro (`build_translit.bat`) and runs in Azahar.
The stereo, pause menu, settings and progress code is shared with the host
runner and tested there (`--stereo N`, `--data DIR`); the 3DS-only parts were
syntax-checked against stub headers before each build.
Performance on Old 3DS is unknown: the game redraws large light/fog sprites
every frame exactly as the original does (≈4.3 ms/frame on x86).
