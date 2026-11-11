/* Host runner: plays the translated game headless with a scripted input
 * file, writes camera frames as PNG and a per-frame state trace.
 *
 *   ./kingdom_host --frames 3600 --script input.txt --png-every 600 --out out/
 *
 * Script lines:  <frame> key <NAME> down|up   or   <frame> mouse down|up
 */
#include "../../game/game.h"
#include "../common/session.h"
#include "../common/stereo.h"
#include "../common/parallel.h"
#include "../../flash/gpu.h"
#ifdef KINGDOM_HOST_LIBPNG
#include <png.h>
#endif
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

extern Number FlxG_initialSeed;

typedef struct Event {
	uint32_t frame;
	bool mouse;
	char key[16];
	bool down;
} Event;

static int load_script(const char *path, Event *events, int max)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return 0;
	int n = 0;
	char line[128];
	while (n < max && fgets(line, sizeof line, f)) {
		Event e = {0};
		char kind[16], a[16], b[16] = "";
		int got = sscanf(line, "%u %15s %15s %15s", &e.frame, kind, a, b);
		if (got < 3 || line[0] == '#')
			continue;
		if (strcmp(kind, "mouse") == 0) {
			e.mouse = true;
			e.down = strcmp(a, "down") == 0;
		} else {
			snprintf(e.key, sizeof e.key, "%s", a);
			e.down = strcmp(b, "down") == 0;
		}
		events[n++] = e;
	}
	fclose(f);
	return n;
}

#ifndef KINGDOM_HOST_LIBPNG
/* PNG writer without libpng (the Windows host build has none): RGB, 8 bit,
   zlib stored blocks. Pixel data is identical to the libpng writer's. */
static uint32_t png_crc(const uint8_t *p, size_t n, uint32_t crc)
{
	static uint32_t table[256];
	if (!table[1])
		for (uint32_t i = 0; i < 256; i++) {
			uint32_t c = i;
			for (int k = 0; k < 8; k++)
				c = (c >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(c & 1));
			table[i] = c;
		}
	crc = ~crc;
	while (n--)
		crc = table[(crc ^ *p++) & 255] ^ (crc >> 8);
	return ~crc;
}

static void png_chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len)
{
	uint8_t head[8] = {(uint8_t)(len >> 24), (uint8_t)(len >> 16), (uint8_t)(len >> 8), (uint8_t)len};
	memcpy(head + 4, type, 4);
	fwrite(head, 1, 8, f);
	if (len)
		fwrite(data, 1, len, f);
	uint32_t crc = png_crc((const uint8_t *)type, 4, 0);
	crc = png_crc(data, len, crc);
	uint8_t tail[4] = {(uint8_t)(crc >> 24), (uint8_t)(crc >> 16), (uint8_t)(crc >> 8), (uint8_t)crc};
	fwrite(tail, 1, 4, f);
}

static void write_png(const char *path, const BitmapData *bd)
{
	FILE *f = fopen(path, "wb");
	if (!f)
		return;
	size_t rowBytes = (size_t)bd->width * 3 + 1, rawLen = rowBytes * (size_t)bd->height;
	uint8_t *raw = malloc(rawLen);
	for (int32_t y = 0; y < bd->height; y++) {
		uint8_t *row = raw + (size_t)y * rowBytes;
		*row++ = 0; /* filter: none */
		for (int32_t x = 0; x < bd->width; x++) {
			uint32_t p = bd->pixels[y * bd->width + x];
			*row++ = (uint8_t)(p >> 16);
			*row++ = (uint8_t)(p >> 8);
			*row++ = (uint8_t)p;
		}
	}
	size_t blocks = (rawLen + 65534) / 65535, zLen = 2 + rawLen + blocks * 5 + 4;
	uint8_t *z = malloc(zLen), *q = z;
	*q++ = 0x78, *q++ = 0x01;
	uint32_t a = 1, b = 0;
	for (size_t i = 0; i < rawLen; i++)
		a = (a + raw[i]) % 65521, b = (b + a) % 65521;
	for (size_t off = 0; off < rawLen; off += 65535) {
		uint32_t n = rawLen - off > 65535 ? 65535 : (uint32_t)(rawLen - off);
		*q++ = off + n >= rawLen;
		*q++ = (uint8_t)n, *q++ = (uint8_t)(n >> 8), *q++ = (uint8_t)~n, *q++ = (uint8_t)(~n >> 8);
		memcpy(q, raw + off, n);
		q += n;
	}
	*q++ = (uint8_t)(b >> 8), *q++ = (uint8_t)b, *q++ = (uint8_t)(a >> 8), *q++ = (uint8_t)a;
	static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
	uint8_t ihdr[13] = {(uint8_t)(bd->width >> 24), (uint8_t)(bd->width >> 16), (uint8_t)(bd->width >> 8), (uint8_t)bd->width,
		(uint8_t)(bd->height >> 24), (uint8_t)(bd->height >> 16), (uint8_t)(bd->height >> 8), (uint8_t)bd->height, 8, 2, 0, 0, 0};
	fwrite(sig, 1, 8, f);
	png_chunk(f, "IHDR", ihdr, 13);
	png_chunk(f, "IDAT", z, (uint32_t)(q - z));
	png_chunk(f, "IEND", NULL, 0);
	free(raw);
	free(z);
	fclose(f);
}
#else
static void write_png(const char *path, const BitmapData *bd)
{
	FILE *f = fopen(path, "wb");
	if (!f)
		return;
	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info = png_create_info_struct(png);
	png_init_io(png, f);
	png_set_IHDR(png, info, (png_uint_32)bd->width, (png_uint_32)bd->height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info(png, info);
	uint8_t *row = malloc((size_t)bd->width * 3);
	for (int32_t y = 0; y < bd->height; y++) {
		for (int32_t x = 0; x < bd->width; x++) {
			uint32_t p = bd->pixels[y * bd->width + x];
			row[x * 3] = (uint8_t)(p >> 16);
			row[x * 3 + 1] = (uint8_t)(p >> 8);
			row[x * 3 + 2] = (uint8_t)p;
		}
		png_write_row(png, row);
	}
	png_write_end(png, NULL);
	png_destroy_write_struct(&png, &info);
	free(row);
	fclose(f);
}
#endif

static void host_weather_filter(BitmapData *b)
{
	if (FlxG.state && as3_is((FlxBasic *)FlxG.state, &PlayState_class))
		ColorMatrix_applyFilter(&((PlayState *)FlxG.state)->weather->ambientTransform, b);
}

int main(int argc, char **argv)
{
	uint32_t frames = 600, pngEvery = 0;
	int32_t stereo = 0;
	bool gpu = false;
	const char *script = NULL, *out = "out", *data = NULL;
	for (int i = 1; i + 1 < argc; i += 2) {
		if (!strcmp(argv[i], "--frames")) frames = (uint32_t)atoi(argv[i + 1]);
		else if (!strcmp(argv[i], "--script")) script = argv[i + 1];
		else if (!strcmp(argv[i], "--png-every")) pngEvery = (uint32_t)atoi(argv[i + 1]);
		else if (!strcmp(argv[i], "--out")) out = argv[i + 1];
		else if (!strcmp(argv[i], "--assets")) Embed_root = argv[i + 1];
		else if (!strcmp(argv[i], "--seed")) FlxG_initialSeed = atof(argv[i + 1]);
		else if (!strcmp(argv[i], "--gpu")) gpu = atoi(argv[i + 1]) != 0; /* record display lists, CPU reference player */
		else if (!strcmp(argv[i], "--water-slope")) Gpu_waterSlope = atoi(argv[i + 1]); /* 0 mirror, 2 soft, 4 tilt */
		else if (!strcmp(argv[i], "--stereo")) stereo = atoi(argv[i + 1]); /* right-eye max shift, px */
		else if (!strcmp(argv[i], "--data")) data = argv[i + 1]; /* session dir: progress.sav, settings.cfg */
	}
	mkdir(out, 0755);
	static Event events[4096];
	int eventCount = script ? load_script(script, events, 4096) : 0;
	static char fontRoot[512];
	snprintf(fontRoot, sizeof fontRoot, "%s/fonts", Embed_root);
	TextField_fontRoot = fontRoot;

	char path[512];
	snprintf(path, sizeof path, "%s/trace.csv", out);
	FILE *trace = fopen(path, "w");
	fprintf(trace, "frame,state,seed,player_x,coins,phase,day,beggars,characters,trolls,bunnies,camera_x\n");

	if (getenv("TRANSLIT_JOBS"))
		printf("engine worker: %s\n", Parallel_init() ? "on" : "off");
	FlxGame *game = king_new();
	static Session session;
	if (data) {
		Session_init(&session, data);
		FlxGame_onEnterFrame(game, game->_step); /* initial MenuState, as on 3DS */
		if (Session_resume(&session))
			printf("resumed: %s\n", session.lastSaved);
		/* pause menu snapshot for visual checks */
		BitmapData *menu = BitmapData_new(320, 240, false, 0xFF000000);
		Session_drawMenu(&session, menu);
		snprintf(path, sizeof path, "%s/pause_menu.png", out);
		write_png(path, menu);
	}
	int next = 0;
	for (uint32_t frame = 0; frame < frames; frame++) {
		while (next < eventCount && events[next].frame <= frame) {
			if (events[next].mouse)
				FlxMouse_setRaw(events[next].down, 144, 80);
			else
				FlxKeyboard_setRaw(events[next].key, events[next].down);
			next++;
		}
		BitmapData *rightEye = gpu ? NULL : Stereo_begin(stereo);
		if (gpu)
			Stereo_beginDepthOnly(stereo); /* GPU path: per-command depth (cmd->dx), right eye played from the list */
		if (gpu && FlxG.camera) {
			Gpu_recording = true;
			Gpu_beginFrame(FlxG.camera->buffer);
			/* PlayState.darkness is composed on the GPU (prototype: KG_DARKNESS_APPLY) */
			if (FlxG.state && as3_is((FlxBasic *)FlxG.state, &PlayState_class))
				{
					FlxSprite_get_pixels(((PlayState *)FlxG.state)->darkness)->gpuSurface = true;
					/* Water.draw composed on the GPU (prototype: KG_WATER_*) */
					FlxSprite_get_pixels((FlxSprite *)((PlayState *)FlxG.state)->water)->gpuSurface = true;
					Gpu_stereoWater = FlxSprite_get_pixels((FlxSprite *)((PlayState *)FlxG.state)->water); /* stereo water depth */
				}
		}
		FlxGame_onEnterFrame(game, game->_step);
		Stereo_end();
		Gpu_recording = false;
		if (gpu && getenv("TRANSLIT_UPLOADS")) {
			/* texture upload tally (what the 3DS backend re-uploads per frame) */
			enum { N = 512 };
			static const BitmapData *bd[N];
			static uint32_t lastGen[N], ups[N], w[N], h[N];
			static int used;
			for (int i = 0; i < Gpu_list.count; i++) {
				const GpuCmd *c = &Gpu_list.cmds[i];
				if (c->op != GPUCMD_IMAGE || c->src == NULL || c->src->gpuSurface)
					continue;
				int k = 0;
				while (k < used && bd[k] != c->src)
					k++;
				if (k == used && used < N) {
					bd[used] = c->src, lastGen[used] = c->src->gen - 1, w[used] = c->src->width, h[used] = c->src->height;
					used++;
				}
				if (k < N && lastGen[k] != c->src->gen) {
					lastGen[k] = c->src->gen;
					ups[k]++;
				}
			}
			if (frame + 1 == frames) {
				fprintf(stderr, "uploads over %u frames (distinct bitmaps %d):\n", frames, used);
				for (int k = 0; k < used; k++)
					if (ups[k] > frames / 20)
						fprintf(stderr, "  %p %ux%u uploads %u  (%.1f KB/frame)\n", (void *)bd[k], w[k], h[k], ups[k],
							ups[k] * w[k] * h[k] * 4.0 / 1024 / frames);
			}
		}
		if (gpu && getenv("TRANSLIT_DUMP") && frame == (uint32_t)atoi(getenv("TRANSLIT_DUMP"))) {
			/* debug: the recorded display list (op, blend, target, source size, source rect, matrix, tint) */
			for (int i = 0; i < Gpu_list.count; i++) {
				const GpuCmd *c = &Gpu_list.cmds[i];
				fprintf(stderr, "%d dx%d op%d b%d t%d src%dx%d [%g %g %g %g] m(%g %g %g %g | %g %g) tint(%g %g %g %g) fill(%g %g %g %g 0x%08X)\n", i, (int)c->dx, c->op,
					c->blend, c->target != NULL, c->src ? c->src->width : 0, c->src ? c->src->height : 0, c->sx, c->sy, c->sw, c->sh,
					c->a, c->b, c->c, c->d, c->tx, c->ty, c->tint[0], c->tint[1], c->tint[2], c->tint[3], c->fx, c->fy, c->fw, c->fh,
					(unsigned)c->color);
			}
		}
		static int maxCmds;
		if (gpu && Gpu_list.count > maxCmds)
			maxCmds = Gpu_list.count;
		if (data)
			Session_afterFrame(&session);
		const char *state = FlxG.state ? FlxG.state->klass->name : "-";
		if (FlxG.state && as3_is(FlxG.state, &PlayState_class)) {
			PlayState *ps = (PlayState *)FlxG.state;
			fprintf(trace, "%u,%s,%.17g,%.6f,%d,%d,%d,%d,%d,%d,%d,%.4f\n", frame, state, FlxG.globalSeed, ps->player->x,
				((Player *)ps->player)->coins, ps->phase, ps->day.value, FlxGroup_countLiving(ps->beggars),
				FlxGroup_countLiving(ps->characters), FlxGroup_countLiving(ps->trolls), FlxGroup_countLiving(ps->bunnies),
				FlxG.camera->scroll.x);
		} else
			fprintf(trace, "%u,%s,%.17g,,,,,,,,,\n", frame, state, FlxG.globalSeed);
		bool shot = pngEvery && frame % pngEvery == 0 && FlxG.camera;
		if (shot || (gpu && frame + 1 == frames && FlxG.camera)) {
			snprintf(path, sizeof path, "%s/frame_%05u.png", out, frame);
			if (gpu) {
				/* the 3DS applies Weather.ambientTransform in its present pass, at the
				   point PlayState.draw applied it (camera flash/fade come after) */
				Gpu_playSoftwareFiltered(&Gpu_list, FlxG.camera->buffer, false, host_weather_filter);
				printf("frame %u: %d commands (max %d)\n", frame, Gpu_list.count, maxCmds);
			}
			if (!shot)
				continue; /* GPU mode: last frame rendered for frame_last.png only */
			write_png(path, FlxG.camera->buffer);
			if (gpu && stereo > 0) {
				/* GPU path right eye: the recorded list played shifted per command, what the 3DS renders.
			   The eye starts magenta, so uncovered columns (holes) are visible and counted. */
			BitmapData *eye = BitmapData_new(FlxG.camera->buffer->width, FlxG.camera->buffer->height, FlxG.camera->buffer->transparent, 0xFFFF00FF);
			Gpu_playSoftwareFiltered(&Gpu_list, eye, true, host_weather_filter);
			{
				int holeCols = 0, holeLeft = 0, holeRight = 0;
				for (int32_t x = 0; x < eye->width; x++) {
					bool any = false;
					for (int32_t y = 0; y < eye->height && !any; y++)
						any = eye->pixels[y * eye->width + x] == 0xFFFF00FF;
					holeCols += any;
					if (any && x < eye->width / 2)
						holeLeft++;
					else if (any)
						holeRight++;
				}
				if (holeCols)
					fprintf(stderr, "frame %u: right eye has %d columns with uncovered pixels (%d left, %d right)\n", frame, holeCols, holeLeft, holeRight);
			}
				snprintf(path, sizeof path, "%s/frame_%05u_right.png", out, frame);
				write_png(path, eye);
			}
			if (rightEye) {
				snprintf(path, sizeof path, "%s/frame_%05u_right.png", out, frame);
				write_png(path, rightEye);
			}
		}
	}
	fclose(trace);
	if (FlxG.camera) {
		snprintf(path, sizeof path, "%s/frame_last.png", out);
		write_png(path, FlxG.camera->buffer);
	}
	if (data)
		Session_close(&session);
	printf("ran %u frames, final state %s\n", frames, FlxG.state ? FlxG.state->klass->name : "-");
	return 0;
}
