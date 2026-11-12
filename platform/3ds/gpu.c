/* GPU playback of the camera display list (flash/gpu.h) on Citro2D.
 *
 * Built on the previous native prototype's renderer (../source/assets.c):
 * RGBA8 textures with nearest filtering and clamp, t = 1 - y/height,
 * 512x256 VRAM render targets switched with C3D_FrameSplit + C2D_SceneBegin
 * (no C2D_TargetClear inside a frame), and its Weather TEV stages
 * (weather_stages / kg_weather_matrix) for Weather.ambientTransform.
 *
 * Textures are premultiplied so normal / multiply / screen are single blend
 * equations: normal (1, 1-As), multiply (Cd, 1-As), screen (1-Cd, 1).
 */
#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <math.h>
#include "gpu3ds.h"

/* One BitmapData = one or more texture pages: the GPU limit is 1024 px per
   side, while Flixel's reversed sheets (loadGraphic(..., Reverse=true)) are
   twice the image width (king 1536, troll 1728, trollbig 2304, castle 1920). */
#define PAGE 1024
#define MAX_PAGES 8
typedef struct Page {
	C3D_Tex tex;
	int pw, ph;
} Page;
typedef struct Tex {
	int w, h, cols, rows;
	bool narrow; /* every row one colour (Sky/Haze gradients): 8 px wide, stretched */
	Page page[MAX_PAGES];
} Tex;

#define SURFACE_SLOTS 2 /* PlayState.darkness, Water */
static C3D_Tex sceneTex[2], copyTex, surfaceTex[SURFACE_SLOTS];
static C3D_RenderTarget *sceneTarget[2], *copyTarget, *surfaceTarget[SURFACE_SLOTS];
/* The weather matrix is applied where PlayState.draw applies it. Normally that
   is the end of the scene and the present pass does it; when commands follow
   (camera flash/fade) the scene is filtered into filteredTex first and those
   commands are drawn on top, unfiltered, as in the original. */
static C3D_Tex filteredTex[2];
static C3D_RenderTarget *filteredTarget[2];
static bool sceneFiltered[2];
typedef struct KgWeatherMatrix KgWeatherMatrix;
static void weather_stages(const KgWeatherMatrix *m);
static void reset_weather_stages(void);
static const BitmapData *slotSurface[SURFACE_SLOTS]; /* composed this pass */
static bool slotDirty[SURFACE_SLOTS]; /* got new commands since composed */
static int currentBlend = -1;
/* lower-screen reflection (previous prototype's KG_WATER_* view): the
   translated Water's own commands replayed into a deeper surface */
#define DEEP_ROWS 256 /* the whole texture: the lower screen starts below the top screen's last row and shows 240 / scale rows */
static C3D_Tex deepTex;
static C3D_RenderTarget *deepTarget;
static bool deepValid;
Water *Gpu3ds_water;
uint32_t Gpu3ds_uploadedBytes; /* texture bytes uploaded this frame (perf overlay) */

/* upload diagnostics: which bitmaps are re-uploaded (runtime.log) */
#define UPSTAT_N 24
static struct { const BitmapData *bd; int w, h; bool sparse, narrow; uint32_t count; } upStat[UPSTAT_N];
static void upstat_add(const BitmapData *bd)
{
	int i;
	for (i = 0; i < UPSTAT_N && upStat[i].bd != NULL && upStat[i].bd != bd; i++)
		;
	if (i == UPSTAT_N)
		return;
	upStat[i].bd = bd, upStat[i].w = bd->width, upStat[i].h = bd->height;
	/* copy the flags now: the bitmap may be freed before the report is printed */
	upStat[i].sparse = bd->gpuSparse >= 0;
	upStat[i].narrow = bd->gpu && ((Tex *)bd->gpu)->narrow;
	upStat[i].count++;
}
void Gpu3ds_uploadReport(FILE *f, uint32_t frames)
{
	fprintf(f, "uploads per %lu frames:", (unsigned long)frames);
	for (int i = 0; i < UPSTAT_N && upStat[i].bd != NULL; i++)
		if (upStat[i].count > 1)
			fprintf(f, " %dx%d%s%s=%lu", upStat[i].w, upStat[i].h, upStat[i].sparse ? "s" : "",
			        upStat[i].narrow ? "n" : "",
			        (unsigned long)upStat[i].count);
	fputc('\n', f);
	memset(upStat, 0, sizeof upStat);
}

static int pow2(int v)
{
	int p = 8;
	while (p < v)
		p <<= 1;
	return p;
}

static void free_tex(Tex *t)
{
	for (int i = 0; i < t->cols * t->rows; i++)
		C3D_TexDelete(&t->page[i].tex);
	free(t);
}

/* ARGB (straight alpha) -> RGBA8 premultiplied texel */
static inline uint32_t tex_px(uint32_t p, bool transparent)
{
	uint32_t a = transparent ? p >> 24 : 255;
	uint32_t rr = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
	if (a == 0)
		return 0;
	if (a != 255) {
		/* (v*a + 127) / 255 without a divide (ARM11 has none); exact for
		   all 8-bit v, a */
		uint32_t t;
		t = rr * a + 127, rr = (t + (t >> 8) + 1) >> 8;
		t = g * a + 127, g = (t + (t >> 8) + 1) >> 8;
		t = b * a + 127, b = (t + (t >> 8) + 1) >> 8;
	}
	return rr << 24 | g << 16 | b << 8 | a;
}

static const uint8_t mortonX[8] = {0, 1, 4, 5, 16, 17, 20, 21};

/* Rows of a single colour each (the vertical gradients of Sky and Haze,
   redrawn whenever the weather moves): upload one 8 px column instead of
   288 and stretch it. Same texels, ~36x less upload. */
#define NARROW 8
static bool rows_uniform(const BitmapData *bd)
{
	if (bd->width < 64 || bd->width > PAGE || bd->height > PAGE)
		return false;
	for (int y = 0; y < bd->height; y++) {
		const uint32_t *row = &bd->pixels[y * bd->width];
		uint32_t p = row[0];
		for (int x = 1; x < bd->width; x++)
			if (row[x] != p)
				return false;
	}
	return true;
}

static Tex *texture(BitmapData *bd)
{
	Tex *t = bd->gpu;
	bool narrow = (t == NULL || bd->gpuGen != bd->gen) ? rows_uniform(bd) : t->narrow;
	if (t != NULL && (t->w != bd->width || t->h != bd->height || t->narrow != narrow)) {
		free_tex(t);
		t = bd->gpu = NULL;
	}
	if (t == NULL) {
		int cols = (bd->width + PAGE - 1) / PAGE, rows = (bd->height + PAGE - 1) / PAGE;
		if (cols < 1 || rows < 1 || cols * rows > MAX_PAGES)
			return NULL;
		t = calloc(1, sizeof *t);
		if (!t)
			return NULL;
		t->w = bd->width, t->h = bd->height;
		for (int r = 0; r < rows; r++)
			for (int c = 0; c < cols; c++) {
				Page *pg = &t->page[r * cols + c];
				int w = bd->width - c * PAGE, h = bd->height - r * PAGE;
				pg->pw = narrow ? NARROW : pow2(w < PAGE ? w : PAGE);
				pg->ph = pow2(h < PAGE ? h : PAGE);
				if (!C3D_TexInit(&pg->tex, (u16)pg->pw, (u16)pg->ph, GPU_RGBA8)) {
					for (int i = 0; i < r * cols + c; i++)
						C3D_TexDelete(&t->page[i].tex);
					free(t);
					return NULL;
				}
				C3D_TexSetFilter(&pg->tex, GPU_NEAREST, GPU_NEAREST);
				C3D_TexSetWrap(&pg->tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
				memset(pg->tex.data, 0, pg->tex.size);
			}
		t->cols = cols, t->rows = rows, t->narrow = narrow;
		bd->gpu = t;
		bd->gpuGen = bd->gen - 1;
	}
	if (bd->gpuGen != bd->gen && t->narrow) {
		Page *pg = &t->page[0];
		uint32_t *dst = (uint32_t *)pg->tex.data;
		for (int y = 0; y < bd->height; y++) {
			uint32_t py = (uint32_t)y & 7;
			uint32_t mortonY = ((py & 1) << 1) | ((py & 2) << 2) | ((py & 4) << 3);
			uint32_t v = tex_px(bd->pixels[y * bd->width], bd->transparent);
			uint32_t *tile = dst + (((uint32_t)y >> 3) << 6) + mortonY; /* pw = 8: one tile per 8 rows */
			for (int lx = 0; lx < NARROW; lx++)
				tile[mortonX[lx]] = v;
		}
		C3D_TexFlush(&pg->tex);
		bd->gpuGen = bd->gen;
		Gpu3ds_uploadedBytes += (uint32_t)(NARROW * bd->height * 4);
		upstat_add(bd);
	} else if (bd->gpuGen != bd->gen) {
		/* 8x8 Morton tiles, memory row 0 = top (t = 1), premultiplied */
		for (int y = 0; y < bd->height; y++) {
			const uint32_t *src = &bd->pixels[y * bd->width];
			int r = y / PAGE, ly = y - r * PAGE;
			uint32_t py = (uint32_t)ly & 7;
			uint32_t mortonY = ((py & 1) << 1) | ((py & 2) << 2) | ((py & 4) << 3);
			for (int c = 0; c < t->cols; c++) {
				Page *pg = &t->page[r * t->cols + c];
				uint32_t *dst = (uint32_t *)pg->tex.data;
				uint32_t tileRow = ((uint32_t)ly >> 3) * (uint32_t)(pg->pw >> 3);
				int x0 = c * PAGE, x1 = x0 + PAGE < bd->width ? x0 + PAGE : bd->width;
				uint32_t lastP = src[x0], lastV = tex_px(lastP, bd->transparent);
				for (int x = x0; x < x1; x++) {
					uint32_t p = src[x];
					if (p != lastP) /* rows of equal pixels (Sky, Haze, fills) convert once */
						lastP = p, lastV = tex_px(p, bd->transparent);
					uint32_t lx = (uint32_t)(x - x0);
					dst[((tileRow + (lx >> 3)) << 6) + mortonY + mortonX[lx & 7]] = lastV;
				}
			}
		}
		for (int i = 0; i < t->cols * t->rows; i++)
			C3D_TexFlush(&t->page[i].tex);
		bd->gpuGen = bd->gen;
		Gpu3ds_uploadedBytes += (uint32_t)(bd->width * bd->height * 4);
		upstat_add(bd);
	}
	return t;
}

static void blend(int mode)
{
	if (mode == currentBlend)
		return;
	C2D_Flush();
	/* Alpha is source-over (as + ad(1-as)) for every mode, as flash.c's W3C
	   blend: SCREEN beams stamped into the transparent PlayState.darkness
	   must raise its alpha, otherwise the premultiplied colour exceeds the
	   alpha and the later MULTIPLY brightens the scene (night was washed out). */
	if (mode == GPUBLEND_MULTIPLY)
		C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_DST_COLOR, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
	else if (mode == GPUBLEND_SCREEN)
		C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE_MINUS_DST_COLOR, GPU_ONE, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
	else
		C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
	currentBlend = mode;
}

static u32 premul(uint32_t argb)
{
	uint32_t a = argb >> 24;
	float f = a / 255.0f;
	return C2D_Color32((u8)(((argb >> 16) & 255) * f + 0.5f), (u8)(((argb >> 8) & 255) * f + 0.5f), (u8)((argb & 255) * f + 0.5f),
		(u8)a);
}

/* textured quad: rectangle (sx,sy,sw,sh) of a texture of size pw x ph mapped
   to the screen by the affine matrix (a b c d tx ty), shifted by dx */
static void quad_at(C3D_Tex *tex, int pw, int ph, float ox, float oy, const GpuCmd *c, float dx, bool narrow)
{
	if (c->sw <= 0 || c->sh <= 0)
		return;
	float u = c->sx - ox, v = c->sy - oy;
	Tex3DS_SubTexture sub = {(u16)c->sw, (u16)c->sh, u / pw, 1.0f - v / ph, (u + c->sw) / pw, 1.0f - (v + c->sh) / ph};
	if (narrow) /* one colour per row: sample the middle column */
		sub.left = sub.right = 0.5f / pw;
	C2D_Image image = {tex, &sub};
	float scaleX, scaleY, angle;
	if (c->b == 0 && c->c == 0) { /* axis-aligned: no trig */
		scaleX = c->a, scaleY = c->d, angle = 0;
	} else {
		scaleX = sqrtf(c->a * c->a + c->b * c->b);
		if (scaleX == 0)
			return;
		angle = atan2f(c->b, c->a);
		scaleY = (c->a * c->d - c->b * c->c) / scaleX;
	}
	float x = c->a * c->sx + c->c * c->sy + c->tx + dx, y = c->b * c->sx + c->d * c->sy + c->ty;
	/* Citro2D mirrors a negative-size quad in place (it spans pos .. pos+|size|);
	   the matrix mirrors around pos (pos+size .. pos): move the anchor. Water's
	   scale(1, -1) camera-buffer copy depends on it. */
	if (angle == 0) {
		if (scaleX < 0)
			x += c->sw * scaleX;
		if (scaleY < 0)
			y += c->sh * scaleY;
	}
	C2D_DrawParams params = {{x, y, c->sw * scaleX, c->sh * scaleY}, {0, 0}, 0.5f, angle};
	float ta = c->tint[3];
	C2D_ImageTint tint;
	C2D_PlainImageTint(&tint, C2D_Color32f(c->tint[0] * ta, c->tint[1] * ta, c->tint[2] * ta, ta), 1.0f);
	C2D_DrawImage(image, &params, &tint);
}

static void quad(C3D_Tex *tex, int pw, int ph, const GpuCmd *c, float dx)
{
	quad_at(tex, pw, ph, 0, 0, c, dx, false);
}

/* a source rectangle may span several pages: one quad per page piece */
static void quad_tex(const Tex *t, const GpuCmd *c, float dx)
{
	if (t->cols == 1 && t->rows == 1) {
		quad_at((C3D_Tex *)&t->page[0].tex, t->page[0].pw, t->page[0].ph, 0, 0, c, dx, t->narrow);
		return;
	}
	for (int r = 0; r < t->rows; r++)
		for (int k = 0; k < t->cols; k++) {
			float x0 = fmaxf(c->sx, (float)(k * PAGE)), x1 = fminf(c->sx + c->sw, (float)((k + 1) * PAGE));
			float y0 = fmaxf(c->sy, (float)(r * PAGE)), y1 = fminf(c->sy + c->sh, (float)((r + 1) * PAGE));
			if (x1 <= x0 || y1 <= y0)
				continue;
			GpuCmd piece = *c;
			piece.sx = x0, piece.sw = x1 - x0, piece.sy = y0, piece.sh = y1 - y0;
			const Page *pg = &t->page[r * t->cols + k];
			quad_at((C3D_Tex *)&pg->tex, pg->pw, pg->ph, (float)(k * PAGE), (float)(r * PAGE), &piece, dx, false);
		}
}

static void release_texture(BitmapData *bd)
{
	if (bd->gpu)
		free_tex(bd->gpu);
	bd->gpu = NULL;
}

bool Gpu3ds_init(void)
{
	for (int i = 0; i < 2; i++) {
		if (!C3D_TexInitVRAM(&sceneTex[i], 512, 256, GPU_RGBA8))
			return false;
		C3D_TexSetFilter(&sceneTex[i], GPU_NEAREST, GPU_NEAREST);
		C3D_TexSetWrap(&sceneTex[i], GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
		sceneTarget[i] = C3D_RenderTargetCreateFromTex(&sceneTex[i], GPU_TEXFACE_2D, 0, -1);
	}
	if (!C3D_TexInitVRAM(&copyTex, 512, 256, GPU_RGBA8))
		return false;
	C3D_TexSetFilter(&copyTex, GPU_NEAREST, GPU_NEAREST);
	C3D_TexSetWrap(&copyTex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
	copyTarget = C3D_RenderTargetCreateFromTex(&copyTex, GPU_TEXFACE_2D, 0, -1);
	/* PlayState.darkness is composed here, as the prototype's KG_DARKNESS_APPLY */
	for (int i = 0; i < SURFACE_SLOTS; i++) {
		if (!C3D_TexInitVRAM(&surfaceTex[i], 512, 256, GPU_RGBA8))
			return false;
		C3D_TexSetFilter(&surfaceTex[i], GPU_NEAREST, GPU_NEAREST);
		C3D_TexSetWrap(&surfaceTex[i], GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
		surfaceTarget[i] = C3D_RenderTargetCreateFromTex(&surfaceTex[i], GPU_TEXFACE_2D, 0, -1);
		if (!surfaceTarget[i])
			return false;
	}
	/* optional: without them the weather filter is applied at present (camera
	   flash/fade then get filtered too) */
	for (int i = 0; i < 2; i++)
		if (C3D_TexInitVRAM(&filteredTex[i], 512, 256, GPU_RGBA8)) {
			C3D_TexSetFilter(&filteredTex[i], GPU_NEAREST, GPU_NEAREST);
			C3D_TexSetWrap(&filteredTex[i], GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
			filteredTarget[i] = C3D_RenderTargetCreateFromTex(&filteredTex[i], GPU_TEXFACE_2D, 0, -1);
		}
	/* optional: without it the lower screen shows the game at 1:1 */
	if (C3D_TexInitVRAM(&deepTex, 512, 256, GPU_RGBA8)) {
		C3D_TexSetFilter(&deepTex, GPU_NEAREST, GPU_NEAREST);
		C3D_TexSetWrap(&deepTex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
		deepTarget = C3D_RenderTargetCreateFromTex(&deepTex, GPU_TEXFACE_2D, 0, -1);
	}
	Gpu_releaseTexture = release_texture;
	return sceneTarget[0] && sceneTarget[1] && copyTarget;
}

static void whole_frame(GpuCmd *c, int w, int h, float scale, float x, float y)
{
	memset(c, 0, sizeof *c);
	c->sw = (float)w, c->sh = (float)h, c->a = c->d = scale, c->tx = x, c->ty = y;
	c->tint[0] = c->tint[1] = c->tint[2] = c->tint[3] = 1;
}

static void draw_cmd(const GpuCmd *c, float dx)
{
	if (c->op == GPUCMD_FILL) {
		blend(GPUBLEND_NORMAL);
		C2D_DrawRectSolid(c->fx + dx, c->fy, 0.5f, c->fw, c->fh, premul(c->color));
		return;
	}
	Tex *t = texture(c->src);
	if (t == NULL)
		return;
	blend(c->blend);
	quad_tex(t, c, dx);
}

/* Replays Water's surface commands (base fill, gradientOverlay, flipped
   camera buffer, reflected light stamps) into a surface DEEP_ROWS tall, for
   the lower screen. ADAPTATION (3DS view, from the previous prototype): the
   original water band is 44 rows; below them the base keeps the band's last
   gradient value (0x66 black) and the reflection continues to the top of
   the scene. Surface coordinates, as Water's own bitmap (x = screen x + 4). */
static void compose_deep(const GpuList *list, int upto, const BitmapData *surface, int eye)
{
	C2D_Flush();
	C3D_FrameSplit(0);
	C2D_SceneBegin(deepTarget);
	currentBlend = -1;
	C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
	C2D_DrawRectSolid(0, 0, 0.5f, 512, 256, C2D_Color32(0, 0, 0, 0));
	C2D_Flush();
	bool deepened = false;
	for (int i = 0; i < upto; i++) {
		const GpuCmd *c = &list->cmds[i];
		if (c->target != surface)
			continue;
		if (c->op == GPUCMD_FILL) {
			float fh = c->fh;
			if (c->fy <= 0 && c->fy + c->fh >= surface->height) /* the whole band: the whole deep surface */
				fh = 256;
			C2D_Flush();
			currentBlend = -1;
			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
			C2D_DrawRectSolid(c->fx, c->fy, 0.5f, c->fw, fh, premul(c->color));
			C2D_Flush();
			continue;
		}
		if (c->op == GPUCMD_REFLECT) {
			if (!deepened) { /* gradient below the band, before the reflection */
				blend(GPUBLEND_NORMAL);
				C2D_DrawRectSolid(0, (float)surface->height, 0.5f, 512, 256, C2D_Color32(0, 0, 0, 0x66));
				deepened = true;
			}
			blend(GPUBLEND_NORMAL);
			quad(&sceneTex[eye], 512, 256, c, 0);
			if (!c->cont && c->b == 0 && c->c == 0 && c->d < 0 && c->sy == 0 && c->ty > 0 && c->ty < 256) {
				/* A real reflection goes on upwards, through the sky above the top of the picture. Repeat the
				   top scene row (the last reflected row) downwards over the rest of the deep surface, with the
				   reflection's own tint, so the sky continues instead of a flat slab. Source row 0 of the
				   mirrored scene ends at deep row ty; the stretched row covers [ty, 256). */
				GpuCmd sky = *c;
				sky.sh = 1;
				sky.d = 256.0f - c->ty;
				sky.ty = c->ty;
				quad(&sceneTex[eye], 512, 256, &sky, 0);
			}
			continue;
		}
		draw_cmd(c, 0);
	}
	C2D_Flush();
	deepValid = true;
}

/* composes a gpuSurface (all its commands recorded before index `upto`)
   into a surface slot, then returns to the eye's scene target. REFLECT
   commands inside it sample the scene so far (prototype: KG_WATER_COMPOSE). */
static int compose_surface(const GpuList *list, int upto, const BitmapData *surface, int eye)
{
	int slot = 0;
	while (slot < SURFACE_SLOTS - 1 && slotSurface[slot] != NULL && slotSurface[slot] != surface)
		slot++;
	C2D_Flush();
	C3D_FrameSplit(0);
	C2D_SceneBegin(surfaceTarget[slot]);
	currentBlend = -1;
	/* a surface starts transparent; its own fills replace it */
	C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
	C2D_DrawRectSolid(0, 0, 0.5f, 512, 256, C2D_Color32(0, 0, 0, 0));
	C2D_Flush();
	for (int i = 0; i < upto; i++) {
		const GpuCmd *c = &list->cmds[i];
		if (c->target != surface)
			continue;
		if (c->op == GPUCMD_FILL) {
			C2D_Flush();
			currentBlend = -1;
			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO); /* fillRect replaces */
			C2D_DrawRectSolid(c->fx, c->fy, 0.5f, c->fw, c->fh, premul(c->color));
			C2D_Flush();
			continue;
		}
		if (c->op == GPUCMD_REFLECT) {
			blend(GPUBLEND_NORMAL);
			quad(&sceneTex[eye], 512, 256, c, 0);
			continue;
		}
		draw_cmd(c, 0);
	}
	C2D_Flush();
	slotSurface[slot] = surface;
	slotDirty[slot] = false;
	if (eye == 0 && deepTarget != NULL && Gpu3ds_water != NULL && surface == Gpu3ds_water->_pixels)
		compose_deep(list, upto, surface, eye);
	C3D_FrameSplit(0);
	C2D_SceneBegin(sceneTarget[eye]);
	currentBlend = -1;
	return slot;
}

static int surface_slot(const GpuList *list, int i, const BitmapData *surface, int eye)
{
	for (int s = 0; s < SURFACE_SLOTS; s++)
		if (slotSurface[s] == surface) /* recompose only if it got new commands */
			return slotDirty[s] ? compose_surface(list, i, surface, eye) : s;
	return compose_surface(list, i, surface, eye);
}

/* plays the list into the scene texture of one eye */
static void play(const GpuList *list, int eye, int w, int h, const KgWeatherMatrix *filter)
{
	int split = filter != NULL && filteredTarget[eye] != NULL && list->filterAt >= 0 && list->filterAt < list->count
		? list->filterAt : -1;
	sceneFiltered[eye] = false;
	C3D_FrameSplit(0);
	C2D_SceneBegin(sceneTarget[eye]);
	currentBlend = -1;
	C2D_SetTintMode(C2D_TintMult);
	blend(GPUBLEND_NORMAL);
	C2D_DrawRectSolid(0, 0, 0.5f, 512, 256, C2D_Color32(0, 0, 0, 255));
	for (int k = 0; k < SURFACE_SLOTS; k++)
		slotSurface[k] = NULL;
	if (eye == 0)
		deepValid = false;
	for (int i = 0; i < list->count; i++) {
		const GpuCmd *c = &list->cmds[i];
		if (i == split) { /* the weather filter point: filter the scene so far, draw the rest on the result */
			GpuCmd whole;
			whole_frame(&whole, w, h, 1, 0, 0);
			C2D_Flush();
			C3D_FrameSplit(0);
			C2D_SceneBegin(filteredTarget[eye]);
			currentBlend = -1;
			blend(GPUBLEND_NORMAL);
			quad(&sceneTex[eye], 512, 256, &whole, 0);
			weather_stages(filter);
			C2D_Flush();
			reset_weather_stages();
			C2D_Prepare();
			C2D_SetTintMode(C2D_TintMult);
			currentBlend = -1;
			sceneFiltered[eye] = true;
		}
		if (c->target != NULL) { /* surface commands are played by compose_surface */
			for (int k = 0; k < SURFACE_SLOTS; k++)
				if (slotSurface[k] == c->target)
					slotDirty[k] = true;
			continue;
		}
		float dx = eye ? c->dx : 0;
		if (c->op == GPUCMD_IMAGE && c->src->gpuSurface) {
			int slot = surface_slot(list, i, c->src, eye);
			blend(c->blend);
			quad(&surfaceTex[slot], 512, 256, c, dx);
		} else if (c->op == GPUCMD_REFLECT) {
			if (!c->cont) { /* snapshot the scene so far */
				GpuCmd whole;
				whole_frame(&whole, w, h, 1, 0, 0);
				C2D_Flush();
				C3D_FrameSplit(0);
				C2D_SceneBegin(copyTarget);
				currentBlend = -1;
				blend(GPUBLEND_NORMAL);
				quad(&sceneTex[eye], 512, 256, &whole, 0);
				C2D_Flush();
				C3D_FrameSplit(0);
				C2D_SceneBegin(sceneTarget[eye]);
				currentBlend = -1;
			}
			blend(GPUBLEND_NORMAL);
			quad(&copyTex, 512, 256, c, dx);
		} else
			draw_cmd(c, dx);
	}
	C2D_Flush();
}

/* ---- Weather.ambientTransform on the GPU --------------------------------
 * From ../source/weather_matrix.[ch] and ../source/assets.c (weather_stages),
 * fed with the translated Weather's own fields. Darkness is 0 here: in the
 * translation it is the separate multiply-blended darkness sprite. */
typedef struct { uint32_t ambient, darkness_color; float darkness, ambient_amount, saturation, contrast; } WeatherIn;
typedef enum {
 KG_WEATHER_CONTRAST_ADD,
 KG_WEATHER_CONTRAST_OFFSET_THEN_MULTIPLY
} KgWeatherContrastMode;

struct KgWeatherMatrix {
 float luma_weights[3];
 float luma_scale[3];
 float pixel_scale[3];
 float contrast_k;
 float contrast_bias;
 float contrast_offset;
 float contrast_multiplier;
 float contrast_stage_scale;
 KgWeatherContrastMode contrast_mode;
};

static float clampf(float value,float low,float high){return fminf(high,fmaxf(low,value));}

static void kg_weather_matrix(const WeatherIn *weather,KgWeatherMatrix *matrix){
 static const float luma[3]={0.212671f,0.71516f,0.072169f};
 const float ambient[3]={
  ((weather->ambient>>16)&255)/255.0f,
  ((weather->ambient>>8)&255)/255.0f,
  (weather->ambient&255)/255.0f
 };
 const float shade[3]={
  ((weather->darkness_color>>16)&255)/255.0f,
  ((weather->darkness_color>>8)&255)/255.0f,
  (weather->darkness_color&255)/255.0f
 };
 float darkness=floorf(clampf(weather->darkness,0,1)*255.0f)/255.0f;
 float ambient_amount=weather->ambient_amount;
 float saturation=clampf(weather->saturation,0,1);
 float ambient_luma=0,dark_luma=0,dark_scale[3];
 memset(matrix,0,sizeof *matrix);

 for(int c=0;c<3;c++){
  dark_scale[c]=1.0f-darkness+darkness*shade[c];
  dark_luma+=luma[c]*dark_scale[c];
  ambient_luma+=luma[c]*ambient[c];
 }
 for(int c=0;c<3;c++)
  matrix->luma_weights[c]=dark_luma>0?luma[c]*dark_scale[c]/dark_luma:luma[c];

 /* Collapse darkness -> ambient colorize -> saturation into two terms:
    pixel_scale[c]*pixel[c] + luma_scale[c]*source_luma(darkened_pixel). */
 float colorized_luma=(1.0f-ambient_amount)+ambient_amount*ambient_luma;
 float pixel_gain=saturation*(1.0f-ambient_amount);
 for(int c=0;c<3;c++){
  float luma_gain=saturation*ambient_amount*ambient[c]+(1.0f-saturation)*colorized_luma;
  matrix->pixel_scale[c]=pixel_gain*dark_scale[c];
  matrix->luma_scale[c]=luma_gain*dark_luma;
 }

 float k=fmaxf(0.0f,1.0f+weather->contrast);
 matrix->contrast_k=k;
 matrix->contrast_bias=(128.0f/255.0f)*(1.0f-k);
 if(k<=1.0f){
  /* Keep the positive offset after multiplying so the intermediate never
     clips for the source's low-contrast presets. */
  matrix->contrast_mode=KG_WEATHER_CONTRAST_ADD;
  matrix->contrast_multiplier=k;
 }else{
  matrix->contrast_mode=KG_WEATHER_CONTRAST_OFFSET_THEN_MULTIPLY;
  matrix->contrast_offset=0.5f+matrix->contrast_bias/k;
  matrix->contrast_stage_scale=k<=2.0f?1.0f:2.0f;
  matrix->contrast_multiplier=k/(matrix->contrast_stage_scale*4.0f);
 }
}

static uint32_t rgba(const float c[3]){return C2D_Color32f(c[0],c[1],c[2],1.0f);}
int Gpu3ds_weatherMode; /* autotest diagnostics: 1 = skip the contrast stages */
static void weather_stages(const KgWeatherMatrix *m){
 C3D_TexEnv *env=C3D_GetTexEnv(0);
 C3D_TexEnvInit(env);
 C3D_TexEnvSrc(env,C3D_RGB,GPU_TEXTURE0,GPU_CONSTANT,GPU_CONSTANT);
 C3D_TexEnvFunc(env,C3D_RGB,GPU_MULTIPLY_ADD);
 C3D_TexEnvColor(env,C2D_Color32(128,128,128,255));
 C3D_TexEnvSrc(env,C3D_Alpha,GPU_TEXTURE0,GPU_PRIMARY_COLOR,0);
 C3D_TexEnvFunc(env,C3D_Alpha,GPU_MODULATE);

 /* DOT3 on (pixel/2 + 1/2) and (lumaWeight/2 + 1/2) gives the
    darkness-weighted source luminance without an extra render target. */
 env=C3D_GetTexEnv(1);C3D_TexEnvInit(env);
 C3D_TexEnvSrc(env,C3D_RGB,GPU_PREVIOUS,GPU_CONSTANT,0);
 C3D_TexEnvFunc(env,C3D_RGB,GPU_DOT3_RGB);
 C3D_TexEnvColor(env,C2D_Color32f(0.5f+0.5f*m->luma_weights[0],0.5f+0.5f*m->luma_weights[1],0.5f+0.5f*m->luma_weights[2],1));
 C3D_TexEnvSrc(env,C3D_Alpha,GPU_PREVIOUS,0,0);C3D_TexEnvFunc(env,C3D_Alpha,GPU_REPLACE);

 env=C3D_GetTexEnv(2);C3D_TexEnvInit(env);
 C3D_TexEnvSrc(env,C3D_RGB,GPU_PREVIOUS,GPU_CONSTANT,0);
 C3D_TexEnvFunc(env,C3D_RGB,GPU_MODULATE);C3D_TexEnvColor(env,rgba(m->luma_scale));
 C3D_TexEnvSrc(env,C3D_Alpha,GPU_PREVIOUS,0,0);C3D_TexEnvFunc(env,C3D_Alpha,GPU_REPLACE);

 env=C3D_GetTexEnv(3);C3D_TexEnvInit(env);
 C3D_TexEnvSrc(env,C3D_RGB,GPU_TEXTURE0,GPU_CONSTANT,GPU_PREVIOUS);
 C3D_TexEnvFunc(env,C3D_RGB,GPU_MULTIPLY_ADD);C3D_TexEnvColor(env,rgba(m->pixel_scale));
 C3D_TexEnvSrc(env,C3D_Alpha,GPU_PREVIOUS,0,0);C3D_TexEnvFunc(env,C3D_Alpha,GPU_REPLACE);

 if(Gpu3ds_weatherMode==1)return;
 env=C3D_GetTexEnv(4);C3D_TexEnvInit(env);
 C3D_TexEnvSrc(env,C3D_RGB,GPU_PREVIOUS,GPU_CONSTANT,0);
 if(m->contrast_mode==KG_WEATHER_CONTRAST_ADD){
  C3D_TexEnvFunc(env,C3D_RGB,GPU_MODULATE);
  C3D_TexEnvColor(env,C2D_Color32f(m->contrast_k,m->contrast_k,m->contrast_k,1));
 }else{
  C3D_TexEnvFunc(env,C3D_RGB,GPU_ADD_SIGNED);
  C3D_TexEnvScale(env,C3D_RGB,m->contrast_stage_scale==2.0f?GPU_TEVSCALE_2:GPU_TEVSCALE_1);
  C3D_TexEnvColor(env,C2D_Color32f(m->contrast_offset,m->contrast_offset,m->contrast_offset,1));
 }
 C3D_TexEnvSrc(env,C3D_Alpha,GPU_PREVIOUS,0,0);C3D_TexEnvFunc(env,C3D_Alpha,GPU_REPLACE);

 env=C3D_GetTexEnv(5);C3D_TexEnvInit(env);
 C3D_TexEnvSrc(env,C3D_RGB,GPU_PREVIOUS,GPU_CONSTANT,0);
 if(m->contrast_mode==KG_WEATHER_CONTRAST_ADD){
  C3D_TexEnvFunc(env,C3D_RGB,GPU_ADD);
  C3D_TexEnvColor(env,C2D_Color32f(m->contrast_bias,m->contrast_bias,m->contrast_bias,1));
 }else{
  C3D_TexEnvFunc(env,C3D_RGB,GPU_MODULATE);C3D_TexEnvScale(env,C3D_RGB,GPU_TEVSCALE_4);
  C3D_TexEnvColor(env,C2D_Color32f(m->contrast_multiplier,m->contrast_multiplier,m->contrast_multiplier,1));
 }
 C3D_TexEnvSrc(env,C3D_Alpha,GPU_PREVIOUS,0,0);C3D_TexEnvFunc(env,C3D_Alpha,GPU_REPLACE);
}
static void reset_weather_stages(void){C3D_TexEnvInit(C3D_GetTexEnv(4));C3D_TexEnvInit(C3D_GetTexEnv(5));}


static void draw_scene(int eye, float x, float y, float scale, int w, int h)
{
	GpuCmd whole;
	whole_frame(&whole, w, h, scale, x, y);
	quad(sceneFiltered[eye] ? &filteredTex[eye] : &sceneTex[eye], 512, 256, &whole, 0);
}

/* The lower screen: the deep water surface at the top screen's scale (so it is the same size as the top screen's
   picture and shows its centre 320 px), continuing below the top screen's last row like a mirror image, rippled by Water's own displacement map (the
   DisplacementMapFilter Water applies to its bitmap: out(x,y) =
   in(x + R(map)-128, y + G(map)-128), scale 256/256). Cells are 32 x 2
   surface pixels (perlinNoise's base is 32 x 4); the map is read at the column perlinNoise fills (x + 8,
   gpuSparse) and, below its 44 rows, repeats (ADAPTATION). */
static void draw_deep(float viewW, float s, int sceneRows)
{
	const Water *wt = Gpu3ds_water;
	const BitmapData *map = wt->displacementBitmap;
	const int sw = wt->_pixels->width; /* 296: screen x = surface x - 4 */
	float left = (288 - viewW / s) / 2 + 4; /* surface x at the screen's left edge */
	int rippleScale = AS3_INT(wt->weather->wind * Water_WIND_RIPPLE_MULTIPLIER);
	/* the first row of the lower screen is the reflection row just below the top screen's last scene row: the
	   top screen shows the water from its line (screen y = wt->y - scroll.y) down to row sceneRows, so the
	   lower one continues at deep row sceneRows - waterY (a seamless, mirror-like continuation) */
	int startRow = sceneRows - (int)(wt->y - FlxG.camera->scroll.y);
	const int shownRows = (int)ceilf(240.0f / s) + 2; /* deep rows visible on the 240 px lower screen */
	startRow = startRow < 0 ? 0 : (startRow > DEEP_ROWS - shownRows ? DEEP_ROWS - shownRows : startRow);
	GpuCmd c;
	whole_frame(&c, sw, DEEP_ROWS, s, -left * s, -(float)startRow * s);
	if (rippleScale <= 0 || map == NULL) { /* calm water: one draw */
		quad(&deepTex, 512, 256, &c, 0);
		return;
	}
	/* below the reflected scene the deep surface is uniform: one draw */
	int scene = (int)(wt->y - FlxG.camera->scroll.y) + 18; /* + the largest ripple */
	int rows = scene < DEEP_ROWS ? (scene < 0 ? 0 : scene & ~1) : DEEP_ROWS;
	if (rows < DEEP_ROWS) {
		GpuCmd rest = c;
		rest.sy = (float)rows, rest.sh = (float)(DEEP_ROWS - rows);
		quad(&deepTex, 512, 256, &rest, 0);
	}
	for (int y = startRow & ~1; y < rows; y += 2) {
		if ((y - startRow) * s >= 240)
			break; /* below the lower screen */
		/* runs of cells with the same displacement are one draw */
		int runX = -1, runDx = 0, runDy = 0, runEnd = 0;
		for (int x = 0; x <= sw; x += 32) {
			bool flush = x >= sw;
			int ddx = 0, ddy = 0;
			bool visible = false;
			if (!flush) {
				float sx = (x - left) * s;
				visible = !(sx + 32 * s <= 0 || sx >= viewW);
				int mx = x + 8 < map->width ? x + 8 : map->width - 1, my = y % map->height;
				uint32_t mp = map->pixels[my * map->width + mx];
				ddx = (int)((mp >> 16) & 255) - 128, ddy = (int)((mp >> 8) & 255) - 128;
				if (visible && runX >= 0 && ddx == runDx && ddy == runDy) {
					runEnd = x + 32 < sw ? x + 32 : sw;
					continue;
				}
			}
			if (runX >= 0) {
				float u = fminf(fmaxf((float)(runX + runDx), 0), (float)(sw - (runEnd - runX)));
				float v = fminf(fmaxf((float)(y + runDy), 0), (float)(DEEP_ROWS - 2));
				c.sx = u, c.sy = v, c.sw = (float)(runEnd - runX), c.sh = 2;
				c.a = c.d = s;
				c.tx = (runX - left) * s - u * s, c.ty = (y - startRow) * s - v * s;
				quad(&deepTex, 512, 256, &c, 0);
				runX = -1;
			}
			if (visible)
				runX = x, runDx = ddx, runDy = ddy, runEnd = x + 32 < sw ? x + 32 : sw;
		}
	}
}

/* ---- bottom screen HUD (3DS-only extras) ---------------------------------- */
void Gpu3ds_hudImage(const BitmapData *bd, float sx, float sy, float sw, float sh, float x, float y, float scale, float alpha)
{
	GpuCmd c;
	memset(&c, 0, sizeof c);
	c.op = GPUCMD_IMAGE;
	c.src = (BitmapData *)bd;
	c.sx = sx, c.sy = sy, c.sw = sw, c.sh = sh;
	c.a = c.d = scale;
	c.tx = x - scale * sx;
	c.ty = y - scale * sy;
	c.tint[0] = c.tint[1] = c.tint[2] = 1;
	c.tint[3] = alpha;
	c.blend = GPUBLEND_NORMAL;
	draw_cmd(&c, 0);
}

void Gpu3ds_present(const GpuList *list, const Weather *weather, bool stereo, C3D_RenderTarget *top, C3D_RenderTarget *topRight,
	C3D_RenderTarget *bottom, bool bottomGame, float topScale, int w, int h)
{
	KgWeatherMatrix m;
	bool filter = weather != NULL;
	if (filter) {
		WeatherIn in = {weather->ambient, weather->darknessColor, 0, (float)weather->ambientAmount, (float)weather->saturation,
			(float)weather->contrast};
		kg_weather_matrix(&in, &m);
	}
	play(list, 0, w, h, filter ? &m : NULL);
	if (stereo)
		play(list, 1, w, h, filter ? &m : NULL);
	C3D_RenderTarget *targets[3] = {top, stereo ? topRight : NULL, bottomGame ? bottom : NULL};
	for (int i = 0; i < 3; i++) {
		if (targets[i] == NULL)
			continue;
		C3D_FrameSplit(0);
		C2D_SceneBegin(targets[i]);
		currentBlend = -1;
		blend(GPUBLEND_NORMAL);
		float s = i == 2 ? 1.0f : topScale;
		float sw = i == 2 ? 320.0f : 400.0f;
		bool deepDrawn = i == 2 && deepValid && Gpu3ds_water != NULL;
		if (deepDrawn)
			draw_deep(sw, topScale, h); /* same scale as the top screen: its centre 320 px, continuing below its last row */
		else
			draw_scene(i == 1 ? 1 : 0, (sw - w * s) / 2, (240 - h * s) / 2, s, w, h);
		/* the scene is already filtered when camera FX followed the filter point */
		if (filter && (deepDrawn || !sceneFiltered[i == 1 ? 1 : 0]))
			weather_stages(&m);
		C2D_Flush();
		reset_weather_stages();
		C2D_Prepare();
	}
	currentBlend = -1;
}
