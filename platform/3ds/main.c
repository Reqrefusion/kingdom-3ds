/* Nintendo 3DS host for the translated game.
 *
 * Plays the role of Flash Player: delivers input events, calls
 * FlxGame.onEnterFrame with the elapsed milliseconds, and shows the camera
 * buffer (FlxG.camera.buffer, 288x160) on screen.
 *
 *   top screen    : the game, scaled to the full 400 px width (400x222)
 *   bottom screen : in PlayState the deep reflection of Water (gpu.c
 *                   draw_deep, from the previous prototype), else the
 *                   game at 1:1 (288x160); touch = mouse;
 *                   the pause menu while paused
 *
 * Controls: D-pad / Circle Pad left-right = LEFT/RIGHT, D-pad down / Circle
 * Pad down / A = DOWN (coin) and mouse click (menus), touch = mouse,
 * START = pause menu (Resume / Music / 3D depth / New game / Save and exit).
 * The 3D slider enables the stereoscopic top screen (platform/common/stereo.c).
 *
 * Carried over from the previous native prototype (../source/main.c):
 * New 3DS clock boost, 128 KB stack, a readable error screen when the data
 * folder is missing, stereoscopic top screen, Start pause menu, settings.cfg,
 * progress across launches (platform/common/session.c), runtime.log timing.
 */
#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <sys/stat.h>
#include "../../game/game.h"
#include "../common/session.h"
#include "../common/stereo.h"
#include "../common/parallel.h"
#include "gpu3ds.h"

/* saves, settings, log (and autotest.txt) live on the SD card; the game assets are inside the .3dsx (romfs:/) */
#ifndef KINGDOM_DATA_ROOT
#define KINGDOM_DATA_ROOT "sdmc:/3ds/kingdom_translit"
#endif
#define DATA_ROOT KINGDOM_DATA_ROOT
static char g_assetRoot[32] = "romfs:"; /* romfs:/ when the assets are embedded in the .3dsx, else DATA_ROOT */
static char g_fontRoot[48] = "romfs:/fonts";

u32 __stacksize__ = 128 * 1024;
extern int Gpu3ds_weatherMode;

extern Number FlxG_initialSeed;

typedef struct Screen {
	C3D_Tex tex;
	Tex3DS_SubTexture sub;
	C2D_Image image;
} Screen;

static void screen_init(Screen *s, int w, int h)
{
	C3D_TexInit(&s->tex, 512, 256, GPU_RGBA8);
	C3D_TexSetFilter(&s->tex, GPU_NEAREST, GPU_NEAREST);
	s->sub = (Tex3DS_SubTexture){(u16)w, (u16)h, 0.0f, 1.0f, w / 512.0f, 1.0f - h / 256.0f};
	s->image = (C2D_Image){&s->tex, &s->sub};
}

/* Copy an ARGB BitmapData into a 512x256 RGBA8 tiled texture. */
static void screen_upload(Screen *s, const BitmapData *bd)
{
	uint32_t *dst = (uint32_t *)s->tex.data;
	const int texW = 512;
	for (int y = 0; y < bd->height && y < 256; y++) {
		const uint32_t *src = &bd->pixels[y * bd->width];
		/* Memory row 0 is the top of the sub-texture (t = 1.0), so rows are
		   stored in image order. */
		uint32_t ty = (uint32_t)y;
		for (int x = 0; x < bd->width && x < texW; x++) {
			uint32_t argb = src[x];
			/* 8x8 Morton tiles */
			uint32_t tileX = (uint32_t)x >> 3, tileY = ty >> 3;
			uint32_t px = (uint32_t)x & 7, py = ty & 7;
			uint32_t morton = (px & 1) | ((py & 1) << 1) | ((px & 2) << 1) | ((py & 2) << 2) | ((px & 4) << 2) | ((py & 4) << 3);
			uint32_t offset = ((tileY * (texW >> 3) + tileX) << 6) + morton;
			uint32_t r = (argb >> 16) & 255, g = (argb >> 8) & 255, b = argb & 255;
			dst[offset] = (r << 24) | (g << 16) | (b << 8) | 0xFF;
		}
	}
	GSPGPU_FlushDataCache(s->tex.data, s->tex.size);
}

static bool data_present(void)
{
	char path[96];
	snprintf(path, sizeof path, "%s/assets/gfx/title.png.argb", g_assetRoot);
	FILE *f = fopen(path, "rb");
	if (f == NULL)
		return false;
	fclose(f);
	return true;
}

static void missing_data_screen(void)
{
	consoleInit(GFX_TOP, NULL);
	printf("Kingdom\n\nGame data not found.\n\nThis .3dsx has no embedded assets (romfs) and\n%s/\nhas none either.\n\n"
	       "Build it with tools/build_release.py (it embeds\nthe assets from the original Kingdom repo).\n\nSTART: exit\n", DATA_ROOT);
	while (aptMainLoop()) {
		hidScanInput();
		if (hidKeysDown() & KEY_START)
			break;
		gspWaitForVBlank();
	}
}

/* ---- bottom screen HUD (3DS-only extras, outside the game's parity) ---------------------
   top right: the coin pouch, big, with the coin count; right: a picture of what a coin would go
   to (the selected building, else a beggar within reach). Reads the game state, changes nothing. */
static int sack_frame(int c) /* Coinsack.show(): which pouch frame for how many coins */
{
	return c <= 0 ? 0 : c == 1 ? 1 : c <= 3 ? 2 : c <= 5 ? 3 : c <= 8 ? 4 : c <= 11 ? 5 : c <= 15 ? 6 : 7;
}

/* the bounding box of the non-transparent pixels of a sheet frame: the picture is scaled to what is drawn in it */
static void opaque_bounds(const BitmapData *bd, int sx, int sy, int fw, int fh, int *x0, int *y0, int *x1, int *y1)
{
	static const BitmapData *cbd;
	static int csx = -1, csy = -1, cfw, cfh, cx0, cy0, cx1, cy1;
	static uint32_t cgen;
	if (bd != cbd || sx != csx || sy != csy || fw != cfw || fh != cfh || bd->gen != cgen) {
		int ax = fw, ay = fh, bx = -1, by = -1;
		for (int y = 0; y < fh; y++)
			for (int x = 0; x < fw; x++) {
				int px = sx + x, py = sy + y;
				if (px < 0 || py < 0 || px >= bd->width || py >= bd->height)
					continue;
				if ((bd->pixels[py * bd->width + px] >> 24) > 8) {
					ax = x < ax ? x : ax, bx = x > bx ? x : bx;
					ay = y < ay ? y : ay, by = y > by ? y : by;
				}
			}
		if (bx < 0)
			ax = ay = 0, bx = fw - 1, by = fh - 1;
		cbd = bd, cgen = bd->gen, csx = sx, csy = sy, cfw = fw, cfh = fh, cx0 = ax, cy0 = ay, cx1 = bx, cy1 = by;
	}
	*x0 = cx0, *y0 = cy0, *x1 = cx1, *y1 = cy1;
}

static void draw_hud(PlayState *ps)
{
	Player *pl = (Player *)ps->player;
	int coins = pl->coins;
	/* the coin pouch, big, top right (no frame, no number: the pouch fills up with the coins) */
	const BitmapData *sheet = ((FlxSprite *)ps->sack)->_pixels;
	Gpu3ds_hudImage(sheet, (float)(sack_frame(coins) * 16), 0, 16, 16, 320.0f - 16 * 5 - 10, 8, 5.0f, 1.0f);
	/* what the next coin would go to: the selected building, else the nearest beggar in reach */
	FlxSprite *target = pl->selectedBuilding;
	if (target == NULL) {
		Number best = 96, px = pl->x + pl->width / 2;
		for (int32_t i = 0; i < ps->beggars->length; i++) {
			Citizen *c = As3Array_get(&ps->beggars->members, (uint32_t)i);
			if (c != NULL && c->alive && c->exists) {
				Number d = Math_abs((c->x + c->width / 2) - px);
				if (d < best) {
					best = d;
					target = (FlxSprite *)c;
				}
			}
		}
	}
	if (target != NULL && target->frameWidth > 0 && target->frameHeight > 0) {
		int ox0, oy0, ox1, oy1;
		opaque_bounds(target->_pixels, (int)target->_gpuSheetX, (int)target->_gpuSheetY, (int)target->frameWidth, (int)target->frameHeight, &ox0,
			&oy0, &ox1, &oy1);
		float bw = (float)(ox1 - ox0 + 1), bh = (float)(oy1 - oy0 + 1);
		float s = fminf(100.0f / bw, 104.0f / bh);
		s = floorf(s * 4.0f) / 4.0f;
		s = s < 0.5f ? 0.5f : (s > 6.0f ? 6.0f : s);
		/* centred on the right side of the lower screen, below the pouch */
		Gpu3ds_hudImage(target->_pixels, (float)(target->_gpuSheetX + ox0), (float)(target->_gpuSheetY + oy0), bw, bh, 258.0f - bw * s / 2,
			174.0f - bh * s / 2, s, 1.0f);
	}
}

int main(void)
{
	gfxInitDefault();
	gfxSet3D(true);
	osSetSpeedupEnable(true); /* New 3DS: 804 MHz + L2 cache */
	mkdir("sdmc:/3ds", 0777);
	mkdir(DATA_ROOT, 0777);
	/* the converted original assets are embedded in the .3dsx (romfs:/); an old-style build copied them to the SD
	   card instead, which still works as a fallback */
	if (R_FAILED(romfsInit())) {
		snprintf(g_assetRoot, sizeof g_assetRoot, "%s", DATA_ROOT);
		snprintf(g_fontRoot, sizeof g_fontRoot, "%s/fonts", DATA_ROOT);
	} else if (!data_present()) { /* romfs present but empty: try the SD card */
		snprintf(g_assetRoot, sizeof g_assetRoot, "%s", DATA_ROOT);
		snprintf(g_fontRoot, sizeof g_fontRoot, "%s/fonts", DATA_ROOT);
	}
	if (!data_present()) {
		missing_data_screen();
		gfxExit();
		return 1;
	}
	/* sizes as in the previous prototype: several render passes per frame */
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE * 2);
	C2D_Init(8192);
	C2D_Prepare();
	C3D_RenderTarget *top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	C3D_RenderTarget *topRight = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
	C3D_RenderTarget *bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
	Result ndspRes = ndspInit();
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);

	Screen game_screen, right_screen, menu_screen;
	screen_init(&game_screen, 288, 160);
	screen_init(&right_screen, 288, 160);
	screen_init(&menu_screen, 320, 240);
	/* Visual check against tests/baseline (host runner, CPU translation):
	   DATA_ROOT/autotest.txt holds "seed S", "freeze F", "cpu 0|1" and the host
	   script lines "<frame> key NAME down|up" / "<frame> mouse down|up". The
	   game then runs with the fixed Flixel step and scripted input, without
	   loading or writing progress, and stops advancing at frame F (the screen
	   keeps showing it). */
	static struct { uint32_t frame; bool mouse, down; char key[16]; } script[256];
	int scriptCount = 0, scriptNext = 0;
	bool autotest = false, forceCpu = false;
	uint32_t freezeAt = 0;
	Number autoSeed = 0.5;
	static char autoProgress[128]; /* "progress D1 A4 ...": start as new PlayState(saved) */
	FILE *af = fopen(DATA_ROOT "/autotest.txt", "r");
	if (af) {
		autotest = true;
		char line[128], kind[16], a[16], b[16];
		while (fgets(line, sizeof line, af)) {
			unsigned fr;
			double v;
			if (line[0] == '#')
				continue;
			if (strncmp(line, "progress ", 9) == 0) {
				snprintf(autoProgress, sizeof autoProgress, "%s", line + 9);
				autoProgress[strcspn(autoProgress, "\r\n")] = 0;
			} else if (sscanf(line, "seed %lf", &v) == 1)
				autoSeed = v;
			else if (sscanf(line, "freeze %u", &fr) == 1)
				freezeAt = fr;
			else if (sscanf(line, "cpu %u", &fr) == 1)
				forceCpu = fr != 0;
			else if (sscanf(line, "weathermode %u", &fr) == 1)
				Gpu3ds_weatherMode = (int)fr;
			else if (scriptCount < 256 && sscanf(line, "%u %15s %15s %15s", &fr, kind, a, b) >= 3) {
				script[scriptCount].frame = fr;
				script[scriptCount].mouse = strcmp(kind, "mouse") == 0;
				script[scriptCount].down = strcmp(script[scriptCount].mouse ? a : b, "down") == 0;
				snprintf(script[scriptCount].key, sizeof script[scriptCount].key, "%s", a);
				scriptCount++;
			}
		}
		fclose(af);
	}

	/* GPU path (flash/gpu.h + gpu.c); the CPU compositor stays as fallback */
	bool gpu = Gpu3ds_init() && !forceCpu;
	Gpu_recording = gpu;

	Embed_root = g_assetRoot;
	TextField_fontRoot = g_fontRoot;
	FlxG_initialSeed = autotest ? autoSeed : (Number)(osGetTime() % 1000003) / 1000003.0;
	if (!autotest)
		Math_random_seed((uint32_t)osGetTime());

	Session session;
	Session_init(&session, DATA_ROOT);
	/* shared engine worker (New 3DS core 2): splits the per-frame colour
	   matrix; Old 3DS runs everything on the main core */
	bool worker = Parallel_init();
	BitmapData *menuBitmap = BitmapData_new(320, 240, false, 0xFF000000);

	FILE *rlog = fopen(DATA_ROOT "/runtime.log", "w");
	if (rlog) {
		fprintf(rlog, "ndspInit result 0x%08lX (0 = ok)\n", (unsigned long)ndspRes);
		fflush(rlog);
	}
	bool isNew3ds = false;
	APT_CheckNew3DS(&isNew3ds);
	Gpu_waterSeg = isNew3ds ? 8 : 16; /* ripple detail vs. command count */
	if (rlog) {
		fprintf(rlog, "Kingdom translit; built " __DATE__ " " __TIME__ "; new3ds=%d; worker=%d; gpu=%d; music=%d\n",
		        isNew3ds, worker, gpu, session.settings.music);
		fflush(rlog);
	}

	FlxGame *game = king_new();
	FlxGame_onEnterFrame(game, autotest ? game->_step : 16); /* FlxGame creates its initial MenuState */
	if (autotest && autoProgress[0])
		FlxG_switchState((FlxState *)PlayState_new(autoProgress));
	if (autotest && rlog) {
		fprintf(rlog, "autotest: seed %g, %d events, freeze at %lu, cpu %d\n", autoSeed, scriptCount, (unsigned long)freezeAt,
		        forceCpu);
		fflush(rlog);
	}
	/* The original starts at MenuState: a click starts new PlayState() with the
	   tutorial (camera pans, hint texts). A saved checkpoint is offered with Y
	   (new PlayState(savedProgress), the crown-loss retry path, which skips
	   the tutorial as the original does). No automatic resume. */

	u64 last = osGetTime();
	bool stickDown = false;
	/* perf overlay on the bottom screen (above the game view) */
	extern uint32_t Gpu3ds_uploadedBytes;
	BitmapData *perfBitmap = BitmapData_new(320, 24, false, 0xFF000000);
	Screen perf_screen;
	screen_init(&perf_screen, 320, 24);
	char perfText[160] = "measuring...";
	u64 perfStart = osGetTime(), perfUpd = 0, perfPres = 0;
	uint32_t perfFrames = 0, perfUpload = 0;
	float perfGpu = 0;
	bool perfDirty = true, wasTitle = false, perfShown = true, lastDev = false;
	BitmapData *rightEye = NULL;
	bool stereoOn = false;
	uint32_t frames = autotest ? 1 : 0; /* autotest: the frame before the loop is the host runner's frame 0 */
	u64 updSum = 0, updMax = 0, frameStart = osGetTime();

	while (aptMainLoop() && !session.exitRequested) {
		hidScanInput();
		u32 held = hidKeysHeld(), pressed = hidKeysDown();
		bool devStats = (held & (KEY_L | KEY_R)) == (KEY_L | KEY_R); /* L+R: show the developer statistics */
		if (devStats != lastDev) {
			lastDev = devStats;
			perfDirty = true;
		}
		circlePosition circle;
		hidCircleRead(&circle);

		if (pressed & KEY_START) {
			session.paused = !session.paused;
			session.menu = MENU_RESUME;
		}
		bool onTitle = FlxG.state != NULL && as3_is((FlxBasic *)FlxG.state, &MenuState_class);
		if (onTitle != wasTitle)
			wasTitle = onTitle, perfDirty = true;
		if (!autotest && !session.paused && onTitle && (pressed & KEY_Y) && Session_resume(&session) && rlog) {
			fprintf(rlog, "resumed: %s\n", session.lastSaved);
			fflush(rlog);
		}
		u64 now = osGetTime();
		uint32_t elapsed = (uint32_t)(now - last);
		last = now;
		if (elapsed > 100)
			elapsed = 100;

		if (autotest) {
			elapsed = game->_step;
			session.paused = false;
		}
		bool frozen = autotest && freezeAt != 0 && frames > freezeAt;
		if (frozen) {
			/* autotest: keep showing frame `freezeAt` (the recorded list is replayed);
			   its state once, in the host runner's trace.csv columns */
			static bool logged;
			if (!logged && rlog) {
				logged = true;
				if (FlxG.state && as3_is((FlxBasic *)FlxG.state, &PlayState_class)) {
					PlayState *ps = (PlayState *)FlxG.state;
					fprintf(rlog, "trace %lu,%s,%.17g,%.6f,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%.4f\n", (unsigned long)freezeAt,
					        FlxG.state->klass->name, FlxG.globalSeed, ps->player->x, (long)((Player *)ps->player)->coins,
					        (long)ps->phase, (long)ps->day.value, (long)FlxGroup_countLiving(ps->beggars),
					        (long)FlxGroup_countLiving(ps->characters), (long)FlxGroup_countLiving(ps->trolls),
					        (long)FlxGroup_countLiving(ps->bunnies), FlxG.camera->scroll.x);
					/* autotest diagnostics (not in the host trace): wall x/stage/building/height still to build */
					fprintf(rlog, "walls");
					for (int32_t wi = 0; wi < ps->walls->length; wi++) {
						Wall *w = As3Array_get(&ps->walls->members, (uint32_t)wi);
						if (w)
							fprintf(rlog, " [x=%.0f s=%d b=%d h=%d]", w->x, (int)w->stage, (int)w->building, (int)w->heightToBuild);
					}
					fprintf(rlog, "\n");
				} else
					fprintf(rlog, "trace %lu,%s\n", (unsigned long)freezeAt, FlxG.state ? FlxG.state->klass->name : "-");
				fflush(rlog);
			}
		} else if (session.paused) {
			/* Flash Player paused: no frames, no input to the game */
			Session_menuInput(&session, pressed & KEY_DUP, pressed & KEY_DDOWN, pressed & KEY_DLEFT, pressed & KEY_DRIGHT,
			                  pressed & KEY_A);
			if (!session.paused) /* resumed / new game: release held keys */
				last = osGetTime();
		} else {
			bool left = (held & KEY_DLEFT) || circle.dx < -40;
			bool right = (held & KEY_DRIGHT) || circle.dx > 40;
			bool down = (held & (KEY_DDOWN | KEY_A)) || circle.dy < -70;
			FlxKeyboard_setRaw("LEFT", left);
			FlxKeyboard_setRaw("RIGHT", right);
			if (down != stickDown)
				FlxKeyboard_setRaw("DOWN", down);
			stickDown = down;

			/* mouse: touch on the bottom screen (game at 1:1, offset 16,40), or A */
			touchPosition touch;
			hidTouchRead(&touch);
			if (held & KEY_TOUCH)
				FlxMouse_setRaw(true, touch.px - 16, touch.py - 40);
			else
				FlxMouse_setRaw((held & KEY_A) != 0, -1, -1);
			if (autotest) { /* scripted input only, as the host runner */
				FlxKeyboard_setRaw("LEFT", false), FlxKeyboard_setRaw("RIGHT", false);
				FlxMouse_setRaw(false, 144, 80);
				static bool keyState[3], mouseState;
				while (scriptNext < scriptCount && script[scriptNext].frame <= frames) {
					if (script[scriptNext].mouse)
						mouseState = script[scriptNext].down;
					else {
						const char *k = script[scriptNext].key;
						int idx = !strcmp(k, "LEFT") ? 0 : !strcmp(k, "RIGHT") ? 1 : 2;
						keyState[idx] = script[scriptNext].down;
						if (idx == 2)
							FlxKeyboard_setRaw("DOWN", keyState[2]);
					}
					scriptNext++;
				}
				FlxKeyboard_setRaw("LEFT", keyState[0]);
				FlxKeyboard_setRaw("RIGHT", keyState[1]);
				FlxMouse_setRaw(mouseState, 144, 80);
			}

			/* 3D slider x depth setting -> right-eye shift (5 px at 100%) */
			float slider = osGet3DSliderState();
			int32_t shift = (int32_t)(slider * 5.0f * session.settings.stereo / 100.0f + 0.5f);
			u64 t0 = osGetTime();
			if (gpu) {
				Gpu_waterSlope = session.settings.water3d == 0 ? 0 : session.settings.water3d == 1 ? 2 : 4; /* menu: 3D water */
				Stereo_beginDepthOnly(shift);
				Gpu_beginFrame(FlxG.camera->buffer);
				stereoOn = shift > 0;
				/* PlayState.darkness composed on the GPU (prototype: KG_DARKNESS_APPLY) */
				Gpu3ds_water = NULL;
				if (FlxG.state && as3_is((FlxBasic *)FlxG.state, &PlayState_class))
					{
					Gpu3ds_water = ((PlayState *)FlxG.state)->water;
					FlxSprite_get_pixels(((PlayState *)FlxG.state)->darkness)->gpuSurface = true;
					/* Water.draw composed on the GPU (prototype: KG_WATER_*) */
					FlxSprite_get_pixels((FlxSprite *)((PlayState *)FlxG.state)->water)->gpuSurface = true;
					Gpu_stereoWater = FlxSprite_get_pixels((FlxSprite *)((PlayState *)FlxG.state)->water); /* stereo water depth */
				}
			} else
				rightEye = Stereo_begin(shift);
			FlxGame_onEnterFrame(game, elapsed);
			Stereo_end();
			u64 upd = osGetTime() - t0;
			perfUpd += upd;
			updSum += upd;
			if (upd > updMax)
				updMax = upd;
			if (!autotest)
				Session_afterFrame(&session);
			if (++frames % 300 == 0 && rlog != NULL) {
				u64 t = osGetTime();
				fprintf(rlog, "frames %lu  fps %.1f  game update avg %.1f ms max %llu ms  state %s\n", (unsigned long)frames,
				        300000.0 / (double)(t - frameStart), updSum / 300.0, (unsigned long long)updMax,
				        FlxG.state ? FlxG.state->klass->name : "-");
				fflush(rlog);
				updSum = updMax = 0;
				frameStart = t;
				if (gpu)
					Gpu3ds_uploadReport(rlog, 300);
				{
					extern uint32_t Media_playCalls, Media_playFailed, Media_loadFailed;
					extern int Media_lastFail;
					fprintf(rlog, "audio: play calls %lu failed %lu (last bad id %d) load failures %lu\n", (unsigned long)Media_playCalls,
					        (unsigned long)Media_playFailed, Media_lastFail, (unsigned long)Media_loadFailed);
					fflush(rlog);
				}
			}
		}

		float sx = session.settings.fill ? 1.5f : 400.0f / 288.0f;
		if (session.paused) {
			Session_drawMenu(&session, menuBitmap);
			screen_upload(&menu_screen, menuBitmap);
		}
		if (gpu) {
			C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
			if (stereoOn)
				C2D_TargetClear(topRight, C2D_Color32(0, 0, 0, 255));
			C2D_TargetClear(bottom, C2D_Color32(0, 0, 0, 255));
			const Weather *weather =
				FlxG.state && as3_is((FlxBasic *)FlxG.state, &PlayState_class) ? ((PlayState *)FlxG.state)->weather : NULL;
			u64 p0 = osGetTime();
			Gpu3ds_uploadedBytes = 0;
			Gpu3ds_present(&Gpu_list, weather, stereoOn, top, topRight, bottom, !session.paused, sx, 288, 160);
			perfPres += osGetTime() - p0;
			perfUpload += Gpu3ds_uploadedBytes;
			if (++perfFrames == 30) {
				u64 now2 = osGetTime();
				snprintf(perfText, sizeof perfText, "%s%.1f fps  game %.1fms  draw %.1fms  gpu %.1fms  up %luKB  cmds %d",
			         R_FAILED(ndspRes) ? "NO SOUND (dspfirm.cdc) " : "",
				         30000.0 / (double)(now2 - perfStart), perfUpd / 30.0, perfPres / 30.0, perfGpu,
				         (unsigned long)(perfUpload / 30 / 1024), Gpu_list.count);
				if (rlog) {
					fprintf(rlog, "%s\n", perfText);
					fflush(rlog);
				}
				perfStart = now2, perfUpd = perfPres = 0, perfFrames = 0, perfUpload = 0;
				perfDirty = true;
			}
			if (perfDirty) {
				TextField *tf = NULL;
				(void)tf;
				Rectangle all = Rectangle_new(0, 0, 320, 24);
				BitmapData_fillRect(perfBitmap, all, 0xFF000000);
				static TextField *perfField;
				if (!perfField) {
					perfField = TextField_new();
					perfField->width = 320;
					perfField->height = 24;
					TextField_setTextFormat(perfField, TextFormat_new("system", 8, 0xFFFF80, "left"));
				}
				char hint[96];
				/* the developer statistics are hidden unless L+R are held; the title hint and the "no sound" warning stay
				   (they tell the player something) */
				const char *shown = NULL;
				if (wasTitle) { /* title screen: how to start */
					const char *saved = Session_savedProgress(&session);
					if (saved != NULL)
						snprintf(hint, sizeof hint, "A / touch: new game   Y: continue (day %d)", atoi(saved + 1));
					else
						snprintf(hint, sizeof hint, "A / touch: new game");
					shown = hint;
				} else if (devStats) {
					shown = perfText;
				} else if (R_FAILED(ndspRes)) {
					shown = "NO SOUND (dspfirm.cdc missing)";
				}
				perfShown = shown != NULL;
				if (perfShown) {
					TextField_set_text(perfField, shown);
					BitmapData_drawTextField(perfBitmap, perfField, NULL, NULL);
					screen_upload(&perf_screen, perfBitmap);
				}
				perfDirty = false;
			}
			C3D_FrameSplit(0);
			C2D_SceneBegin(bottom);
			if (perfShown)
				C2D_DrawImageAt(perf_screen.image, 0, 4, 0.5f, NULL, 1.0f, 1.0f);
			if (!session.paused && FlxG.state && as3_is((FlxBasic *)FlxG.state, &PlayState_class))
				draw_hud((PlayState *)FlxG.state);
			if (session.paused) {
				C3D_FrameSplit(0);
				C2D_SceneBegin(bottom);
				C2D_DrawImageAt(menu_screen.image, 0, 0, 0.5f, NULL, 1.0f, 1.0f);
			}
		} else {
			screen_upload(&game_screen, FlxG.camera->buffer);
			if (rightEye != NULL)
				screen_upload(&right_screen, rightEye);
			C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
			C2D_SceneBegin(top);
			/* whole frame: 288 -> 400 wide (1.39x); fill: 1.5x, 160 -> 240 rows,
			   the frame centred so 10 source columns are cropped on each side */
			float ox = (400 - 288 * sx) / 2, oy = (240 - 160 * sx) / 2;
			C2D_DrawImageAt(game_screen.image, ox, oy, 0.5f, NULL, sx, sx);
			if (rightEye != NULL) {
				C2D_TargetClear(topRight, C2D_Color32(0, 0, 0, 255));
				C2D_SceneBegin(topRight);
				C2D_DrawImageAt(right_screen.image, ox, oy, 0.5f, NULL, sx, sx);
			}
			C2D_TargetClear(bottom, C2D_Color32(0, 0, 0, 255));
			C2D_SceneBegin(bottom);
			if (session.paused)
				C2D_DrawImageAt(menu_screen.image, 0, 0, 0.5f, NULL, 1.0f, 1.0f);
			else
				C2D_DrawImageAt(game_screen.image, 16, 40, 0.5f, NULL, 1.0f, 1.0f);
		}
		C3D_FrameEnd(0);
		perfGpu = C3D_GetDrawingTime();
	}

	Session_close(&session);
	Parallel_close();
	if (rlog != NULL)
		fclose(rlog);
	ndspExit();
	C3D_TexDelete(&game_screen.tex);
	C3D_TexDelete(&menu_screen.tex);
	C3D_TexDelete(&right_screen.tex);
	C2D_Fini();
	C3D_Fini();
	gfxExit();
	return 0;
}
