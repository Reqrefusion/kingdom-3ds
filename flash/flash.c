#include "flash.h"
#include "gpu.h"
#include <stdio.h>

/* ---- Matrix ------------------------------------------------------------ */

void Matrix_identity(Matrix *m)
{
	*m = Matrix_new();
}

void Matrix_translate(Matrix *m, Number dx, Number dy)
{
	m->tx += dx;
	m->ty += dy;
}

void Matrix_scale(Matrix *m, Number sx, Number sy)
{
	m->a *= sx;
	m->b *= sy;
	m->c *= sx;
	m->d *= sy;
	m->tx *= sx;
	m->ty *= sy;
}

void Matrix_concat(Matrix *m, const Matrix *n)
{
	Matrix r;
	r.a = m->a * n->a + m->b * n->c;
	r.b = m->a * n->b + m->b * n->d;
	r.c = m->c * n->a + m->d * n->c;
	r.d = m->c * n->b + m->d * n->d;
	r.tx = m->tx * n->a + m->ty * n->c + n->tx;
	r.ty = m->tx * n->b + m->ty * n->d + n->ty;
	*m = r;
}

void Matrix_rotate(Matrix *m, Number angle)
{
	Number c = cos(angle), s = sin(angle);
	Matrix r = {c, s, -s, c, 0, 0};
	Matrix_concat(m, &r);
}

ColorTransform ColorTransform_new(Number redMultiplier, Number greenMultiplier, Number blueMultiplier,
	Number alphaMultiplier, Number redOffset, Number greenOffset, Number blueOffset, Number alphaOffset)
{
	return (ColorTransform){redMultiplier, greenMultiplier, blueMultiplier, alphaMultiplier,
		redOffset, greenOffset, blueOffset, alphaOffset};
}

/* ---- BitmapData -------------------------------------------------------- */

static inline uint32_t argb(uint32_t a, uint32_t r, uint32_t g, uint32_t b)
{
	return (a << 24) | (r << 16) | (g << 8) | b;
}

BitmapData *BitmapData_new(int32_t width, int32_t height, bool transparent, uint32_t fillColor)
{
	BitmapData *bd = as3_alloc(sizeof *bd);
	if (width < 0) width = 0;
	if (height < 0) height = 0;
	bd->width = width;
	bd->height = height;
	bd->transparent = transparent;
	bd->gpuSparse = -1;
	bd->pixels = as3_alloc((size_t)(width ? width : 1) * (size_t)(height ? height : 1) * sizeof(uint32_t));
	if (!transparent)
		fillColor |= 0xFF000000u;
	for (int32_t i = 0; i < width * height; i++)
		bd->pixels[i] = fillColor;
	return bd;
}

BitmapData *BitmapData_clone(const BitmapData *self)
{
	BitmapData *bd = BitmapData_new(self->width, self->height, self->transparent, 0);
	memcpy(bd->pixels, self->pixels, (size_t)self->width * (size_t)self->height * sizeof(uint32_t));
	return bd;
}

void (*Gpu_releaseTexture)(BitmapData *self);

static void dispose_now(BitmapData *self)
{
	if (self->gpu != NULL && Gpu_releaseTexture != NULL)
		Gpu_releaseTexture(self);
	free(self->pixels);
	free(self);
}

/* bitmaps disposed while a GPU display list may still reference them are
   freed when the next frame starts (Gpu_beginFrame -> BitmapData_flushDisposed) */
static BitmapData **deferred;
static int deferredCount, deferredCap;

void BitmapData_flushDisposed(void)
{
	for (int i = 0; i < deferredCount; i++)
		dispose_now(deferred[i]);
	deferredCount = 0;
}

void BitmapData_dispose(BitmapData *self)
{
	if (self == NULL)
		return;
	if (Gpu_recording) {
		if (deferredCount == deferredCap) {
			deferredCap = deferredCap ? deferredCap * 2 : 32;
			deferred = realloc(deferred, sizeof *deferred * (size_t)deferredCap);
		}
		deferred[deferredCount++] = self;
		return;
	}
	dispose_now(self);
}

uint32_t BitmapData_getPixel32(const BitmapData *self, int32_t x, int32_t y)
{
	if (x < 0 || y < 0 || x >= self->width || y >= self->height)
		return 0;
	return self->pixels[y * self->width + x];
}

uint32_t BitmapData_getPixel(const BitmapData *self, int32_t x, int32_t y)
{
	return BitmapData_getPixel32(self, x, y) & 0x00FFFFFFu;
}

void BitmapData_setPixel32(BitmapData *self, int32_t x, int32_t y, uint32_t color)
{
	self->gen++;
	self->solid = false;
	if (x < 0 || y < 0 || x >= self->width || y >= self->height)
		return;
	if (!self->transparent)
		color |= 0xFF000000u;
	self->pixels[y * self->width + x] = color;
}

void BitmapData_setPixel(BitmapData *self, int32_t x, int32_t y, uint32_t color)
{
	self->gen++;
	self->solid = false;
	if (x < 0 || y < 0 || x >= self->width || y >= self->height)
		return;
	uint32_t *p = &self->pixels[y * self->width + x];
	*p = (*p & 0xFF000000u) | (color & 0x00FFFFFFu);
}

/* Integer pixel bounds of a Flash rectangle (Flash rounds toward the pixel
   grid by truncation of the left/top edge). */
typedef struct IRect {
	int32_t x0, y0, x1, y1;
} IRect;

static IRect clip(Rectangle r, int32_t w, int32_t h)
{
	IRect c;
	c.x0 = (int32_t)floor(r.x);
	c.y0 = (int32_t)floor(r.y);
	c.x1 = (int32_t)floor(r.x + r.width);
	c.y1 = (int32_t)floor(r.y + r.height);
	if (c.x0 < 0) c.x0 = 0;
	if (c.y0 < 0) c.y0 = 0;
	if (c.x1 > w) c.x1 = w;
	if (c.y1 > h) c.y1 = h;
	return c;
}

static void BitmapData_fillRect_impl(BitmapData *self, Rectangle rect, uint32_t color)
{
	/* already filled with exactly this colour everywhere: nothing changes */
	if (self->solid && rect.x <= 0 && rect.y <= 0 && rect.x + rect.width >= self->width && rect.y + rect.height >= self->height &&
		self->solidColor == (self->transparent ? color : (color | 0xFF000000u)))
		return;
	self->gen++;
	self->solid = rect.x <= 0 && rect.y <= 0 && rect.x + rect.width >= self->width && rect.y + rect.height >= self->height;
	self->solidColor = self->transparent ? color : (color | 0xFF000000u);
	IRect c = clip(rect, self->width, self->height);
	if (!self->transparent)
		color |= 0xFF000000u;
	for (int32_t y = c.y0; y < c.y1; y++)
		for (int32_t x = c.x0; x < c.x1; x++)
			self->pixels[y * self->width + x] = color;
}

/* Source-over composite of unpremultiplied colours.
   ADAPTATION: Flash composites premultiplied 8-bit values; results can
   differ by one step per channel for partially transparent pixels. */
static inline uint32_t over(uint32_t src, uint32_t dst, bool dstTransparent)
{
	uint32_t sa = src >> 24;
	if (sa == 255)
		return src;
	if (sa == 0)
		return dst;
	uint32_t da = dstTransparent ? (dst >> 24) : 255;
	if (da == 255) {
		/* opaque destination (the camera buffer): oa = 255 and
		   d*da*(255-sa)/255 = d*(255-sa) exactly, so the general formula
		   reduces to constant divisions -- same results, no runtime divide
		   (ARM11 has no divide instruction). Lanes as in the engine's
		   lume_blend_pixel: red/blue together, green apart. */
		uint32_t inv = 255 - sa;
		uint32_t rb = (src & 0x00FF00FFu) * sa + (dst & 0x00FF00FFu) * inv;
		uint32_t g = ((src >> 8) & 255) * sa + ((dst >> 8) & 255) * inv;
		uint32_t r = (rb >> 16) / 255, b = (rb & 0xFFFFu) / 255;
		return argb(dstTransparent ? 255 : 255, r, g / 255, b);
	}
	uint32_t oa = sa + da * (255 - sa) / 255;
	if (oa == 0)
		return 0;
	uint32_t ch[3];
	for (int i = 0; i < 3; i++) {
		uint32_t s = (src >> (16 - 8 * i)) & 255, d = (dst >> (16 - 8 * i)) & 255;
		ch[i] = (s * sa + d * da * (255 - sa) / 255) / oa;
	}
	return argb(dstTransparent ? oa : 255, ch[0], ch[1], ch[2]);
}

uint32_t BitmapData_blendOver(uint32_t src, uint32_t dst, bool dstTransparent)
{
	return over(src, dst, dstTransparent);
}

static void BitmapData_copyPixels_impl(BitmapData *self, const BitmapData *source, Rectangle sourceRect,
	Point destPoint, bool mergeAlpha)
{
	self->gen++;
	self->solid = false;
	int32_t sx0 = (int32_t)floor(sourceRect.x), sy0 = (int32_t)floor(sourceRect.y);
	int32_t w = (int32_t)floor(sourceRect.x + sourceRect.width) - sx0;
	int32_t h = (int32_t)floor(sourceRect.y + sourceRect.height) - sy0;
	int32_t dx0 = (int32_t)floor(destPoint.x), dy0 = (int32_t)floor(destPoint.y);
	/* clip the copied area against both bitmaps once */
	if (sx0 < 0) { w += sx0; dx0 -= sx0; sx0 = 0; }
	if (sy0 < 0) { h += sy0; dy0 -= sy0; sy0 = 0; }
	if (dx0 < 0) { w += dx0; sx0 -= dx0; dx0 = 0; }
	if (dy0 < 0) { h += dy0; sy0 -= dy0; dy0 = 0; }
	if (sx0 + w > source->width) w = source->width - sx0;
	if (sy0 + h > source->height) h = source->height - sy0;
	if (dx0 + w > self->width) w = self->width - dx0;
	if (dy0 + h > self->height) h = self->height - dy0;
	if (w <= 0 || h <= 0)
		return;
	uint32_t force = source->transparent ? 0 : 0xFF000000u;
	for (int32_t j = 0; j < h; j++) {
		const uint32_t *srow = &source->pixels[(sy0 + j) * source->width + sx0];
		uint32_t *drow = &self->pixels[(dy0 + j) * self->width + dx0];
		if (!mergeAlpha) {
			if (self->transparent && !force)
				memcpy(drow, srow, (size_t)w * sizeof(uint32_t));
			else
				for (int32_t i = 0; i < w; i++)
					drow[i] = srow[i] | force | (self->transparent ? 0 : 0xFF000000u);
			continue;
		}
		for (int32_t i = 0; i < w; i++) {
			uint32_t sp = srow[i] | force;
			uint32_t sa = sp >> 24;
			if (sa == 255)
				drow[i] = sp;
			else if (sa != 0)
				drow[i] = over(sp, drow[i], self->transparent);
		}
	}
}

static inline uint32_t clamp255(Number v)
{
	if (v <= 0)
		return 0;
	if (v >= 255)
		return 255;
	return (uint32_t)v;
}

static inline uint32_t apply_ct(uint32_t c, const ColorTransform *t)
{
	uint32_t a = c >> 24, r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
	return argb(clamp255(a * t->alphaMultiplier + t->alphaOffset), clamp255(r * t->redMultiplier + t->redOffset),
		clamp255(g * t->greenMultiplier + t->greenOffset), clamp255(b * t->blueMultiplier + t->blueOffset));
}

static void BitmapData_colorTransform_impl(BitmapData *self, Rectangle rect, const ColorTransform *colorTransform)
{
	self->gen++;
	self->solid = false;
	IRect c = clip(rect, self->width, self->height);
	/* Per-channel lookup tables: each 8-bit input maps to one output, so the
	   result is identical to evaluating the transform per pixel. */
	uint8_t lut[4][256];
	const Number mul[4] = {colorTransform->alphaMultiplier, colorTransform->redMultiplier, colorTransform->greenMultiplier, colorTransform->blueMultiplier};
	const Number off[4] = {colorTransform->alphaOffset, colorTransform->redOffset, colorTransform->greenOffset, colorTransform->blueOffset};
	for (int k = 0; k < 4; k++)
		for (int v = 0; v < 256; v++)
			lut[k][v] = (uint8_t)clamp255(v * mul[k] + off[k]);
	for (int32_t y = c.y0; y < c.y1; y++) {
		uint32_t *row = &self->pixels[y * self->width];
		for (int32_t x = c.x0; x < c.x1; x++) {
			uint32_t p = row[x];
			p = (uint32_t)lut[0][p >> 24] << 24 | (uint32_t)lut[1][(p >> 16) & 255] << 16 | (uint32_t)lut[2][(p >> 8) & 255] << 8 | lut[3][p & 255];
			row[x] = self->transparent ? p : (p | 0xFF000000u);
		}
	}
}

BlendMode BlendMode_fromString(const char *blend)
{
	if (blend == NULL || as3_streq(blend, "normal"))
		return BLEND_NORMAL;
	if (as3_streq(blend, "multiply"))
		return BLEND_MULTIPLY;
	if (as3_streq(blend, "screen"))
		return BLEND_SCREEN;
	if (as3_streq(blend, "add"))
		return BLEND_ADD;
	if (as3_streq(blend, "overlay"))
		return BLEND_OVERLAY;
	if (as3_streq(blend, "hardlight"))
		return BLEND_HARDLIGHT;
	return BLEND_NORMAL;
}

static inline uint32_t blend_channel(BlendMode mode, uint32_t s, uint32_t d)
{
	switch (mode) {
	case BLEND_MULTIPLY: return s * d / 255;
	case BLEND_SCREEN: return 255 - (255 - s) * (255 - d) / 255;
	case BLEND_ADD: return s + d > 255 ? 255 : s + d;
	case BLEND_OVERLAY: return d < 128 ? 2 * s * d / 255 : 255 - 2 * (255 - s) * (255 - d) / 255;
	case BLEND_HARDLIGHT: return s < 128 ? 2 * s * d / 255 : 255 - 2 * (255 - s) * (255 - d) / 255;
	default: return s;
	}
}

/* ADAPTATION: separable blend then source-over, as in the W3C compositing
   model that Flash's blend modes follow. */
static inline uint32_t blend(BlendMode mode, uint32_t src, uint32_t dst, bool dstTransparent)
{
	if (mode == BLEND_NORMAL)
		return over(src, dst, dstTransparent);
	uint32_t sa = src >> 24;
	if (sa == 0)
		return dst;
	uint32_t da = dstTransparent ? (dst >> 24) : 255;
	uint32_t mixed = 0;
	if (da == 255) {
		/* opaque destination: c = (s*0 + b*255)/255 = b exactly */
		uint32_t sr = (src >> 16) & 255, sg = (src >> 8) & 255, sb = src & 255;
		uint32_t dr = (dst >> 16) & 255, dg = (dst >> 8) & 255, db = dst & 255;
		switch (mode) {
		case BLEND_MULTIPLY:
			mixed = (sr * dr / 255) << 16 | (sg * dg / 255) << 8 | (sb * db / 255);
			break;
		case BLEND_SCREEN:
			mixed = (255 - (255 - sr) * (255 - dr) / 255) << 16 | (255 - (255 - sg) * (255 - dg) / 255) << 8 |
				(255 - (255 - sb) * (255 - db) / 255);
			break;
		default:
			mixed = blend_channel(mode, sr, dr) << 16 | blend_channel(mode, sg, dg) << 8 | blend_channel(mode, sb, db);
		}
		return over((sa << 24) | mixed, dst, dstTransparent);
	}
	for (int i = 0; i < 3; i++) {
		int shift = 16 - 8 * i;
		uint32_t s = (src >> shift) & 255, d = (dst >> shift) & 255;
		uint32_t b = blend_channel(mode, s, d);
		uint32_t c = (s * (255 - da) + b * da) / 255;
		mixed |= c << shift;
	}
	return over((sa << 24) | mixed, dst, dstTransparent);
}

static void BitmapData_draw_impl(BitmapData *self, const BitmapData *source, const Matrix *matrix,
	const ColorTransform *colorTransform, const char *blendMode, const Rectangle *clipRect, bool smoothing)
{
	self->gen++;
	self->solid = false;
	(void)smoothing;
	Matrix m = matrix ? *matrix : Matrix_new();
	Number det = m.a * m.d - m.b * m.c;
	if (det == 0)
		return;
	/* Fast path: integer translation without clipping (most sprite draws). */
	if (m.a == 1 && m.b == 0 && m.c == 0 && m.d == 1 && clipRect == NULL) {
		int32_t ox = (int32_t)floor(m.tx + 0.5), oy = (int32_t)floor(m.ty + 0.5);
		BlendMode fmode = BlendMode_fromString(blendMode);
		for (int32_t sy = 0; sy < source->height; sy++) {
			int32_t y = sy + oy;
			if (y < 0 || y >= self->height)
				continue;
			const uint32_t *srow = &source->pixels[sy * source->width];
			uint32_t *drow = &self->pixels[y * self->width];
			for (int32_t sx = 0; sx < source->width; sx++) {
				int32_t x = sx + ox;
				if (x < 0 || x >= self->width)
					continue;
				uint32_t sp = srow[sx];
				if (!source->transparent)
					sp |= 0xFF000000u;
				if (colorTransform)
					sp = apply_ct(sp, colorTransform);
				drow[x] = blend(fmode, sp, drow[x], self->transparent);
			}
		}
		return;
	}
	/* Destination bounds of the transformed source. */
	Number xs[4] = {0, (Number)source->width, 0, (Number)source->width};
	Number ys[4] = {0, 0, (Number)source->height, (Number)source->height};
	Number minx = INFINITY, miny = INFINITY, maxx = -INFINITY, maxy = -INFINITY;
	for (int i = 0; i < 4; i++) {
		Number x = m.a * xs[i] + m.c * ys[i] + m.tx, y = m.b * xs[i] + m.d * ys[i] + m.ty;
		if (x < minx) minx = x;
		if (x > maxx) maxx = x;
		if (y < miny) miny = y;
		if (y > maxy) maxy = y;
	}
	Rectangle bounds = {minx, miny, maxx - minx + 1, maxy - miny + 1};
	BlendMode amode = BlendMode_fromString(blendMode);
	if (m.b == 0 && m.c == 0 && clipRect == NULL) {
		/* axis-aligned scale/flip: the source column/row of each destination
		   column/row is computed once. Same nearest-sample rule as below. */
		IRect c = clip(bounds, self->width, self->height);
		int32_t *cols = malloc(sizeof(int32_t) * (size_t)(c.x1 > c.x0 ? c.x1 - c.x0 : 1));
		for (int32_t x = c.x0; x < c.x1; x++)
			cols[x - c.x0] = (int32_t)floor((x + 0.5 - m.tx) / m.a);
		for (int32_t y = c.y0; y < c.y1; y++) {
			int32_t sy = (int32_t)floor((y + 0.5 - m.ty) / m.d);
			if (sy < 0 || sy >= source->height)
				continue;
			const uint32_t *srow = &source->pixels[sy * source->width];
			uint32_t *drow = &self->pixels[y * self->width];
			for (int32_t x = c.x0; x < c.x1; x++) {
				int32_t sx = cols[x - c.x0];
				if (sx < 0 || sx >= source->width)
					continue;
				uint32_t sp = srow[sx];
				if (!source->transparent)
					sp |= 0xFF000000u;
				if (colorTransform)
					sp = apply_ct(sp, colorTransform);
				drow[x] = blend(amode, sp, drow[x], self->transparent);
			}
		}
		free(cols);
		return;
	}
	if (clipRect) {
		Number x0 = Math_max(bounds.x, clipRect->x), y0 = Math_max(bounds.y, clipRect->y);
		Number x1 = Math_min(bounds.x + bounds.width, clipRect->x + clipRect->width);
		Number y1 = Math_min(bounds.y + bounds.height, clipRect->y + clipRect->height);
		bounds = (Rectangle){x0, y0, x1 - x0, y1 - y0};
	}
	IRect c = clip(bounds, self->width, self->height);
	Number ia = m.d / det, ib = -m.b / det, ic = -m.c / det, id = m.a / det;
	BlendMode mode = BlendMode_fromString(blendMode);
	for (int32_t y = c.y0; y < c.y1; y++)
		for (int32_t x = c.x0; x < c.x1; x++) {
			Number px = x + 0.5 - m.tx, py = y + 0.5 - m.ty;
			int32_t sx = (int32_t)floor(ia * px + ic * py), sy = (int32_t)floor(ib * px + id * py);
			if (sx < 0 || sy < 0 || sx >= source->width || sy >= source->height)
				continue;
			uint32_t s = source->pixels[sy * source->width + sx];
			if (!source->transparent)
				s |= 0xFF000000u;
			if (colorTransform)
				s = apply_ct(s, colorTransform);
			uint32_t *d = &self->pixels[y * self->width + x];
			*d = blend(mode, s, *d, self->transparent);
		}
}

void BitmapData_noise(BitmapData *self, int32_t randomSeed, uint32_t low, uint32_t high,
	uint32_t channelOptions, bool grayScale)
{
	self->gen++;
	self->solid = false;
	uint32_t state = (uint32_t)randomSeed;
	if (state == 0)
		state = 1;
	uint32_t range = high >= low ? high - low + 1 : 1;
	for (int32_t i = 0; i < self->width * self->height; i++) {
		uint32_t v[4];
		for (int k = 0; k < 4; k++) {
			state = (uint32_t)(((uint64_t)state * 16807u) % 2147483647u);
			v[k] = low + state % range;
		}
		uint32_t a = self->transparent && (channelOptions & 8) ? v[3] : 255;
		uint32_t r, g, b;
		if (grayScale)
			r = g = b = v[0];
		else {
			r = (channelOptions & 1) ? v[0] : 0;
			g = (channelOptions & 2) ? v[1] : 0;
			b = (channelOptions & 4) ? v[2] : 0;
		}
		self->pixels[i] = argb(a, r, g, b);
	}
}

/* ---- Embedded images ----------------------------------------------------- */

const char *Embed_root = "translit_assets";

BitmapData *Embed_bitmapData(const char *embedName)
{
	/* "/assets/gfx/bunny.png" -> <root>/assets/gfx/bunny.png.argb */
	char path[512];
	snprintf(path, sizeof path, "%s/%s.argb", Embed_root, embedName[0] == '/' ? embedName + 1 : embedName);
	FILE *f = fopen(path, "rb");
	if (f == NULL) {
		fprintf(stderr, "missing embedded image %s\n", path);
		return BitmapData_new(1, 1, true, 0xFFFF00FFu);
	}
	uint32_t header[2] = {0, 0};
	if (fread(header, sizeof header, 1, f) != 1) {
		fclose(f);
		return BitmapData_new(1, 1, true, 0xFFFF00FFu);
	}
	BitmapData *bd = BitmapData_new((int32_t)header[0], (int32_t)header[1], true, 0);
	size_t n = (size_t)header[0] * header[1];
	if (fread(bd->pixels, sizeof(uint32_t), n, f) != n)
		fprintf(stderr, "truncated embedded image %s\n", path);
	fclose(f);
	return bd;
}

int32_t Embed_bitmapWidth(const char *embedName)
{
	char path[512];
	snprintf(path, sizeof path, "%s/%s.argb", Embed_root, embedName[0] == '/' ? embedName + 1 : embedName);
	FILE *f = fopen(path, "rb");
	uint32_t header[2] = {1, 1};
	if (f != NULL) {
		if (fread(header, sizeof header, 1, f) != 1)
			header[0] = 1;
		fclose(f);
	}
	return (int32_t)header[0];
}

/* ---- Stereo mirror ---------------------------------------------------------
 * ADAPTATION (3DS only, off unless Stereo_right is set): every write into
 * Stereo_left (the camera buffer) is repeated into Stereo_right, shifted by
 * Stereo_dx pixels. The flixel draw adapters set Stereo_dx per object from its
 * depth. One draw pass feeds both eyes, so no game state is touched twice. */
BitmapData *Stereo_left, *Stereo_right;
int32_t Stereo_dx;
int32_t Stereo_maxShift;

void BitmapData_fillRect(BitmapData *self, Rectangle rect, uint32_t color)
{
	if (Gpu_fillRect(self, rect, color))
		return;
	BitmapData_fillRect_impl(self, rect, color);
	if (self == Stereo_left && Stereo_right) {
		rect.x += Stereo_dx;
		BitmapData_fillRect_impl(Stereo_right, rect, color);
	}
}

void BitmapData_copyPixels(BitmapData *self, const BitmapData *source, Rectangle sourceRect, Point destPoint, bool mergeAlpha)
{
	if (Gpu_copyPixels(self, source, sourceRect, destPoint))
		return;
	BitmapData_copyPixels_impl(self, source, sourceRect, destPoint, mergeAlpha);
	if (self == Stereo_left && Stereo_right && source != Stereo_left) {
		destPoint.x += Stereo_dx;
		BitmapData_copyPixels_impl(Stereo_right, source, sourceRect, destPoint, mergeAlpha);
	}
}

void BitmapData_colorTransform(BitmapData *self, Rectangle rect, const ColorTransform *colorTransform)
{
	if (Gpu_screenFilter(self))
		return;
	BitmapData_colorTransform_impl(self, rect, colorTransform);
	if (self == Stereo_left && Stereo_right) {
		rect.x += Stereo_dx;
		BitmapData_colorTransform_impl(Stereo_right, rect, colorTransform);
	}
}

void BitmapData_draw(BitmapData *self, const BitmapData *source, const Matrix *matrix,
	const ColorTransform *colorTransform, const char *blendMode, const Rectangle *clipRect, bool smoothing)
{
	if (Gpu_draw(self, source, matrix, colorTransform, blendMode))
		return;
	BitmapData_draw_impl(self, source, matrix, colorTransform, blendMode, clipRect, smoothing);
	if (self == Stereo_left && Stereo_right && source != Stereo_left) {
		Matrix m = matrix ? *matrix : Matrix_new();
		m.tx += Stereo_dx;
		BitmapData_draw_impl(Stereo_right, source, &m, colorTransform, blendMode, clipRect, smoothing);
	}
}

#include <time.h>
uint32_t getTimer(void)
{
	return (uint32_t)((double)clock() * 1000.0 / CLOCKS_PER_SEC);
}

/* BitmapData.hitTest(firstPoint, firstAlphaThreshold, secondObject:Point): is the
   pixel of the bitmap under secondObject at least as opaque as the threshold */
bool BitmapData_hitTest(const BitmapData *self, Point firstPoint, uint32_t firstAlphaThreshold, Point secondPoint)
{
	int32_t x = (int32_t)floor(secondPoint.x - firstPoint.x), y = (int32_t)floor(secondPoint.y - firstPoint.y);
	if (x < 0 || y < 0 || x >= self->width || y >= self->height)
		return false;
	uint32_t alpha = self->transparent ? self->pixels[y * self->width + x] >> 24 : 255;
	return alpha >= firstAlphaThreshold;
}
