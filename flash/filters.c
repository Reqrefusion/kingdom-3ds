#include <string.h>
/* flash.display.BitmapData.perlinNoise / applyFilter, flash.filters
 * (ColorMatrixFilter, DisplacementMapFilter), flash.display.Shape with a
 * linear gradient fill, Matrix.createGradientBox. */
#include "filters.h"
#include "gpu.h"

/* ---- Matrix.createGradientBox ------------------------------------------- */

void Matrix_createGradientBox(Matrix *m, Number width, Number height, Number rotation, Number tx, Number ty)
{
	/* The gradient square spans -819.2..819.2 twips-units in gradient space. */
	Number c = cos(rotation), s = sin(rotation);
	m->a = c * width / 1638.4;
	m->b = s * height / 1638.4;
	m->c = -s * width / 1638.4;
	m->d = c * height / 1638.4;
	m->tx = tx + width / 2;
	m->ty = ty + height / 2;
}

/* ---- Shape / Graphics (linear gradient rectangles only) ----------------- */

Shape *Shape_new(void)
{
	return as3_alloc(sizeof(Shape));
}

void Graphics_beginGradientFill(Graphics *g, const char *type, const uint32_t *colors, const Number *alphas,
	const Number *ratios, uint32_t count, const Matrix *matrix, const char *spreadMethod, const char *interpolationMethod)
{
	(void)type; /* "linear" */
	(void)spreadMethod; /* "pad" */
	(void)interpolationMethod; /* "rgb" */
	if (count > SHAPE_MAX_STOPS)
		count = SHAPE_MAX_STOPS;
	g->count = count;
	for (uint32_t i = 0; i < count; i++) {
		g->colors[i] = colors[i] & 0x00FFFFFFu;
		g->alphas[i] = alphas[i];
		g->ratios[i] = ratios[i];
	}
	g->matrix = *matrix;
	g->hasFill = true;
}

void Graphics_drawRect(Graphics *g, Number x, Number y, Number width, Number height)
{
	g->rect = Rectangle_new(x, y, width, height);
	g->hasRect = true;
}

/* Colour of the gradient at ratio position r (0..255), "pad" spread.
   ADAPTATION: Flash quantises gradients to a 256-entry ramp; this computes
   the same ramp directly. */
static uint32_t gradient_at(const Graphics *g, Number r)
{
	if (g->count == 0)
		return 0;
	if (r <= g->ratios[0])
		r = g->ratios[0];
	if (r >= g->ratios[g->count - 1])
		r = g->ratios[g->count - 1];
	uint32_t i = 0;
	while (i + 1 < g->count - 1 && r > g->ratios[i + 1])
		i++;
	uint32_t j = g->count > 1 ? i + 1 : i;
	Number span = g->ratios[j] - g->ratios[i];
	Number f = span > 0 ? (r - g->ratios[i]) / span : 0;
	uint32_t c0 = g->colors[i], c1 = g->colors[j];
	Number a = g->alphas[i] + (g->alphas[j] - g->alphas[i]) * f;
	uint32_t out = 0;
	for (int k = 0; k < 3; k++) {
		int sh = 16 - 8 * k;
		Number v = ((c0 >> sh) & 255) + ((Number)((c1 >> sh) & 255) - ((c0 >> sh) & 255)) * f;
		out |= (uint32_t)(v + 0.5) << sh;
	}
	uint32_t alpha = (uint32_t)(a * 255 + 0.5);
	return (alpha > 255 ? 255u : alpha) << 24 | out;
}

/* index into the 256-entry ramp of the pixel centre (x, y) */
static inline int32_t ramp_index(Number ga, Number gb, Number gc, int32_t x, int32_t y)
{
	Number gx = ga * (x + 0.5) + gb * (y + 0.5) + gc;
	int32_t idx = (int32_t)((gx + 819.2) / 1638.4 * 255 + 0.5);
	return idx < 0 ? 0 : idx > 255 ? 255 : idx;
}

/* bitmap.draw(shape, matrix): rasterise the filled rectangle.
   Flash renders gradients through a 256-entry colour ramp indexed by the
   position in the gradient box; the same ramp is used here. */
void BitmapData_drawShape(BitmapData *self, const Shape *shape, const Matrix *matrix)
{
	self->gen++;
	self->solid = false;
	const Graphics *g = &shape->graphics;
	if (!g->hasFill || !g->hasRect)
		return;
	Matrix m = matrix ? *matrix : Matrix_new();
	const Matrix *gm = &g->matrix;
	Number det = gm->a * gm->d - gm->b * gm->c;
	if (det == 0)
		return;
	uint32_t ramp[256];
	for (int i = 0; i < 256; i++)
		ramp[i] = gradient_at(g, i);
	/* The rectangle is axis aligned in shape space; Kingdom only draws
	   shapes with scale+translate matrices. */
	Number x0 = g->rect.x * m.a + m.tx, y0 = g->rect.y * m.d + m.ty;
	Number x1 = (g->rect.x + g->rect.width) * m.a + m.tx, y1 = (g->rect.y + g->rect.height) * m.d + m.ty;
	int32_t ix0 = (int32_t)floor(Math_min(x0, x1)), ix1 = (int32_t)ceil(Math_max(x0, x1));
	int32_t iy0 = (int32_t)floor(Math_min(y0, y1)), iy1 = (int32_t)ceil(Math_max(y0, y1));
	if (ix0 < 0) ix0 = 0;
	if (iy0 < 0) iy0 = 0;
	if (ix1 > self->width) ix1 = self->width;
	if (iy1 > self->height) iy1 = self->height;
	/* gradient-space x as a linear function of the pixel centre */
	Number ga = (gm->d / det) / m.a, gb = (-gm->c / det) / m.d;
	Number gc = (gm->d * (-m.tx / m.a - gm->tx) - gm->c * (-m.ty / m.d - gm->ty)) / det;
	for (int32_t y = iy0; y < iy1; y++) {
		uint32_t *row = &self->pixels[y * self->width];
		if (ix0 >= ix1)
			break;
		/* Same results, less work (Sky/Haze redraw a full-screen gradient
		   whenever the weather moves): every step of ramp_index is monotonic
		   in x, so equal indices at both ends mean one colour for the whole
		   row; and the blend of a colour over a pixel is computed once per
		   run of equal destination pixels. */
		int32_t first = ramp_index(ga, gb, gc, ix0, y), last = ramp_index(ga, gb, gc, ix1 - 1, y);
		uint32_t lastC = ramp[first], lastD = row[ix0], lastR = BitmapData_blendOver(lastC, lastD, self->transparent);
		if (first == last) {
			if ((lastC >> 24) == 255) { /* opaque: the blend is the colour itself */
				for (int32_t x = ix0; x < ix1; x++)
					row[x] = lastC;
				continue;
			}
			for (int32_t x = ix0; x < ix1; x++) {
				uint32_t d = row[x];
				if (d != lastD)
					lastD = d, lastR = BitmapData_blendOver(lastC, d, self->transparent);
				row[x] = lastR;
			}
			continue;
		}
		for (int32_t x = ix0; x < ix1; x++) {
			uint32_t c = ramp[ramp_index(ga, gb, gc, x, y)], d = row[x];
			if (c != lastC || d != lastD)
				lastC = c, lastD = d, lastR = BitmapData_blendOver(c, d, self->transparent);
			row[x] = lastR;
		}
	}
}

/* ---- perlinNoise -----------------------------------------------------------
   Flash's perlinNoise follows the SVG 1.1 feTurbulence reference algorithm
   (same seed generator, lattice and gradients); this is that reference code.
   ADAPTATION: offsets are applied before the base frequency, one offset per
   octave, matching documented behaviour; no Flash reference image has been
   compared yet. */

#define BSize 0x100
#define BM 0xff
#define PerlinN 0x1000
#define RAND_m 2147483647
#define RAND_a 16807
#define RAND_q 127773
#define RAND_r 2836

typedef struct Turbulence {
	int32_t latticeSelector[BSize + BSize + 2];
	double gradient[4][BSize + BSize + 2][2];
} Turbulence;

static int32_t turb_setup_seed(int32_t seed)
{
	if (seed <= 0)
		seed = -(seed % (RAND_m - 1)) + 1;
	if (seed > RAND_m - 1)
		seed = RAND_m - 1;
	return seed;
}

static int32_t turb_random(int32_t seed)
{
	int32_t result = RAND_a * (seed % RAND_q) - RAND_r * (seed / RAND_q);
	if (result <= 0)
		result += RAND_m;
	return result;
}

static void turb_init(Turbulence *t, int32_t seed)
{
	int i, j, k;
	seed = turb_setup_seed(seed);
	for (k = 0; k < 4; k++) {
		for (i = 0; i < BSize; i++) {
			t->latticeSelector[i] = i;
			for (j = 0; j < 2; j++)
				t->gradient[k][i][j] = (double)(((seed = turb_random(seed)) % (BSize + BSize)) - BSize) / BSize;
			double s = sqrt(t->gradient[k][i][0] * t->gradient[k][i][0] + t->gradient[k][i][1] * t->gradient[k][i][1]);
			if (s != 0) {
				t->gradient[k][i][0] /= s;
				t->gradient[k][i][1] /= s;
			}
		}
	}
	while (--i) {
		k = t->latticeSelector[i];
		t->latticeSelector[i] = t->latticeSelector[j = (seed = turb_random(seed)) % BSize];
		t->latticeSelector[j] = k;
	}
	for (i = 0; i < BSize + 2; i++) {
		t->latticeSelector[BSize + i] = t->latticeSelector[i];
		for (k = 0; k < 4; k++)
			for (j = 0; j < 2; j++)
				t->gradient[k][BSize + i][j] = t->gradient[k][i][j];
	}
}

#define s_curve(t) ((t) * (t) * (3. - 2. * (t)))
#define lerp(t, a, b) ((a) + (t) * ((b) - (a)))

static double turb_noise2(const Turbulence *T, int channel, double vx, double vy)
{
	double t = vx + PerlinN;
	int bx0 = ((int)t) & BM, bx1 = (bx0 + 1) & BM;
	double rx0 = t - (int)t, rx1 = rx0 - 1.0;
	t = vy + PerlinN;
	int by0 = ((int)t) & BM, by1 = (by0 + 1) & BM;
	double ry0 = t - (int)t, ry1 = ry0 - 1.0;
	int i = T->latticeSelector[bx0], j = T->latticeSelector[bx1];
	int b00 = T->latticeSelector[i + by0], b10 = T->latticeSelector[j + by0];
	int b01 = T->latticeSelector[i + by1], b11 = T->latticeSelector[j + by1];
	double sx = s_curve(rx0), sy = s_curve(ry0);
	const double *q;
	double u, v, a, b;
	q = T->gradient[channel][b00]; u = rx0 * q[0] + ry0 * q[1];
	q = T->gradient[channel][b10]; v = rx1 * q[0] + ry0 * q[1];
	a = lerp(sx, u, v);
	q = T->gradient[channel][b01]; u = rx0 * q[0] + ry1 * q[1];
	q = T->gradient[channel][b11]; v = rx1 * q[0] + ry1 * q[1];
	b = lerp(sx, u, v);
	return lerp(sy, a, b);
}

void BitmapData_perlinNoise(BitmapData *self, Number baseX, Number baseY, uint32_t numOctaves, int32_t randomSeed,
	bool stitch, bool fractalNoise, uint32_t channelOptions, bool grayScale, const Point *offsets, uint32_t offsetCount)
{
	self->gen++;
	self->solid = false;
	(void)stitch; /* Kingdom passes false */
	static Turbulence T;
	static int32_t T_seed;
	static bool T_ready;
	if (!T_ready || T_seed != randomSeed) {
		turb_init(&T, randomSeed);
		T_seed = randomSeed;
		T_ready = true;
	}
	double fx = baseX != 0 ? 1.0 / baseX : 0, fy = baseY != 0 ? 1.0 / baseY : 0;
	int channels[4] = {(channelOptions & 1) != 0, (channelOptions & 2) != 0, (channelOptions & 4) != 0, (channelOptions & 8) != 0};
	/* GPU adapter: a ripple map read by the GPU strips needs every (Gpu_waterSeg / 2)-th column */
	int32_t step = Gpu_recording && self->gpuSparse >= 0 ? Gpu_waterSeg / 2 : 1, first = 0;
	for (int32_t y = 0; y < self->height; y++)
		for (int32_t x = first; x < self->width; x += step) {
			uint32_t value[4] = {0, 0, 0, 255};
			for (int c = 0; c < 4; c++) {
				if (!channels[c] && !(grayScale && c < 3))
					continue;
				if (grayScale && c > 0 && c < 3) {
					value[c] = value[0];
					continue;
				}
				double sum = 0, ratio = 1;
				for (uint32_t o = 0; o < numOctaves; o++) {
					Number ox = o < offsetCount ? offsets[o].x : 0, oy = o < offsetCount ? offsets[o].y : 0;
					double vx = (x + ox) * fx * ratio, vy = (y + oy) * fy * ratio;
					double n = turb_noise2(&T, c, vx, vy);
					sum += (fractalNoise ? n : fabs(n)) / ratio;
					ratio *= 2;
				}
				double col = fractalNoise ? (sum * 255 + 255) / 2 : sum * 255;
				if (col < 0) col = 0;
				if (col > 255) col = 255;
				value[c] = (uint32_t)col;
			}
			if (!self->transparent || !channels[3])
				value[3] = 255;
			self->pixels[y * self->width + x] = value[3] << 24 | value[0] << 16 | value[1] << 8 | value[2];
		}
}

/* ---- DisplacementMapFilter ------------------------------------------------ */

DisplacementMapFilter *DisplacementMapFilter_new(BitmapData *mapBitmap, Point mapPoint, uint32_t componentX,
	uint32_t componentY, Number scaleX, Number scaleY, const char *mode, uint32_t color, Number alpha)
{
	DisplacementMapFilter *f = as3_alloc(sizeof *f);
	f->mapBitmap = mapBitmap;
	f->mapPoint = mapPoint;
	f->componentX = componentX;
	f->componentY = componentY;
	f->scaleX = scaleX;
	f->scaleY = scaleY;
	f->mode = mode;
	f->color = color;
	f->alpha = alpha;
	return f;
}

static uint32_t channel_of(uint32_t argb, uint32_t component)
{
	switch (component) {
	case 1: return (argb >> 16) & 255; /* BitmapDataChannel.RED */
	case 2: return (argb >> 8) & 255; /* GREEN */
	case 4: return argb & 255; /* BLUE */
	case 8: return argb >> 24; /* ALPHA */
	default: return 128;
	}
}

void BitmapData_applyDisplacementFilter(BitmapData *self, const BitmapData *source, Rectangle sourceRect,
	Point destPoint, const DisplacementMapFilter *f)
{
	if (Gpu_skipDisplacement(self, f->mapBitmap, f->mapPoint, f->componentX, f->componentY, f->scaleX, f->scaleY))
		return;
	self->gen++;
	self->solid = false;
	BitmapData *src = BitmapData_clone(source);
	int32_t x0 = (int32_t)floor(sourceRect.x), y0 = (int32_t)floor(sourceRect.y);
	int32_t w = (int32_t)sourceRect.width, h = (int32_t)sourceRect.height;
	uint32_t outside = ((uint32_t)(f->alpha * 255 + 0.5) << 24) | (f->color & 0x00FFFFFFu);
	for (int32_t j = 0; j < h; j++)
		for (int32_t i = 0; i < w; i++) {
			int32_t dx = (int32_t)floor(destPoint.x) + i, dy = (int32_t)floor(destPoint.y) + j;
			if (dx < 0 || dy < 0 || dx >= self->width || dy >= self->height)
				continue;
			int32_t mx = i - (int32_t)f->mapPoint.x, my = j - (int32_t)f->mapPoint.y;
			uint32_t m = BitmapData_getPixel32(f->mapBitmap, mx, my);
			if (!f->mapBitmap->transparent)
				m |= 0xFF000000u;
			/* ADAPTATION: nearest-pixel sampling of the displaced position. */
			Number ox = ((Number)channel_of(m, f->componentX) - 128) * f->scaleX / 256;
			Number oy = ((Number)channel_of(m, f->componentY) - 128) * f->scaleY / 256;
			int32_t sx = x0 + i + (int32_t)floor(ox), sy = y0 + j + (int32_t)floor(oy);
			uint32_t c;
			if (sx < 0 || sy < 0 || sx >= src->width || sy >= src->height)
				c = as3_streq(f->mode, "color") ? outside : (as3_streq(f->mode, "ignore") ? BitmapData_getPixel32(src, x0 + i, y0 + j) : 0);
			else
				c = src->pixels[sy * src->width + sx];
			if (!self->transparent)
				c |= 0xFF000000u;
			self->pixels[dy * self->width + dx] = c;
		}
	BitmapData_dispose(src);
}

/* ---- ColorMatrixFilter -------------------------------------------------- */


/* rows [j0, j1) of a colour-matrix pass; independent per row, so the
   platform may split them across cores (FlashParallel_split) */
typedef struct {
	BitmapData *self;
	const BitmapData *source;
	int32_t x0, y0, w, dxo, dyo;
	const float *fm;
} ColorMatrixRows;

static float prod[16][256];

static void color_matrix_rows(void *ctx, int32_t j0, int32_t j1)
{
	ColorMatrixRows *c = ctx;
	BitmapData *self = c->self;
	const BitmapData *source = c->source;
	const float *fm = c->fm;
	for (int32_t j = j0; j < j1; j++) {
		int32_t sy = c->y0 + j, dy = c->dyo + j;
		if (sy < 0 || sy >= source->height || dy < 0 || dy >= self->height)
			continue;
		for (int32_t i = 0; i < c->w; i++) {
			int32_t sx = c->x0 + i, dx = c->dxo + i;
			if (sx < 0 || sx >= source->width || dx < 0 || dx >= self->width)
				continue;
			uint32_t p = source->pixels[sy * source->width + sx];
			uint32_t c0 = (p >> 16) & 255, c1 = (p >> 8) & 255, c2 = p & 255, c3 = p >> 24;
			uint32_t out[4];
			for (int r = 0; r < 4; r++) {
				const float *t = prod[r * 4];
				float v = t[c0] + t[256 + c1] + t[512 + c2] + t[768 + c3] + fm[r * 5 + 4];
				out[r] = v <= 0 ? 0 : v >= 255 ? 255 : (uint32_t)(v + 0.5f);
			}
			if (!self->transparent)
				out[3] = 255;
			self->pixels[dy * self->width + dx] = out[3] << 24 | out[0] << 16 | out[1] << 8 | out[2];
		}
	}
}

void (*FlashParallel_split)(void (*fn)(void *ctx, int32_t y0, int32_t y1), void *ctx, int32_t rows);

static void BitmapData_applyColorMatrixFilter_impl(BitmapData *self, const BitmapData *source, Rectangle sourceRect,
	Point destPoint, const Number matrix[20])
{
	self->gen++;
	self->solid = false;
	int32_t x0 = (int32_t)floor(sourceRect.x), y0 = (int32_t)floor(sourceRect.y);
	int32_t w = (int32_t)sourceRect.width, h = (int32_t)sourceRect.height;
	/* ADAPTATION: single precision is enough for 8-bit channels */
	float fm[20];
	for (int k = 0; k < 20; k++)
		fm[k] = (float)matrix[k];
	/* products m*c for every 8-bit input, per matrix entry: the per-pixel
	   sums below then use exactly the same float values and order as the
	   direct formula (fm[r*5+k] * in[k]). */
	static float lastM[20];
	static bool prodValid;
	if (!prodValid || memcmp(lastM, fm, sizeof fm) != 0) {
		for (int r = 0; r < 4; r++)
			for (int k = 0; k < 4; k++)
				for (int c = 0; c < 256; c++)
					prod[r * 4 + k][c] = fm[r * 5 + k] * (float)c;
		memcpy(lastM, fm, sizeof fm);
		prodValid = true;
	}
	ColorMatrixRows job = {self, source, x0, y0, w, (int32_t)floor(destPoint.x), (int32_t)floor(destPoint.y), fm};
	if (FlashParallel_split != NULL && h >= 32)
		FlashParallel_split(color_matrix_rows, &job, h);
	else
		color_matrix_rows(&job, 0, h);
}

void BitmapData_applyColorMatrixFilter(BitmapData *self, const BitmapData *source, Rectangle sourceRect,
	Point destPoint, const Number matrix[20])
{
	if (Gpu_screenFilter(self))
		return;
	BitmapData_applyColorMatrixFilter_impl(self, source, sourceRect, destPoint, matrix);
	/* stereo mirror (see flash.c): a full-buffer in-place filter on the left
	   eye is applied to the right eye as well */
	if (self == Stereo_left && source == Stereo_left && Stereo_right)
		BitmapData_applyColorMatrixFilter_impl(Stereo_right, Stereo_right, sourceRect, destPoint, matrix);
}
