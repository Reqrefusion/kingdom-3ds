#include <stdlib.h>
#include "gpu.h"
#include <math.h>

bool Gpu_recording;
BitmapData *Gpu_screen;
GpuList Gpu_list;

/* Water draws the camera buffer into its own bitmap; remember that, and emit
   the reflection when that bitmap is drawn onto the screen. */
static struct {
	const BitmapData *target;
	Matrix m; /* screen -> target */
	float tint[4];
	/* Water's DisplacementMapFilter, captured to ripple the reflection */
	const BitmapData *map;
	Point mapPoint;
	uint32_t cx, cy;
	Number scaleX, scaleY;
} pending;

static BitmapData *current_target; /* NULL = screen */

static GpuCmd *push(void)
{
	if (Gpu_list.count == Gpu_list.cap) {
		int cap = Gpu_list.cap ? Gpu_list.cap * 2 : 512;
		GpuCmd *grown = realloc(Gpu_list.cmds, sizeof(GpuCmd) * (size_t)cap);
		if (grown == NULL) {
			/* keep the old list intact; overwrite the last command instead of
			   losing the pointer (one wrong draw instead of a crash) */
			static GpuCmd overflow;
			memset(&overflow, 0, sizeof overflow);
			return &overflow;
		}
		Gpu_list.cmds = grown;
		Gpu_list.cap = cap;
	}
	GpuCmd *c = &Gpu_list.cmds[Gpu_list.count++];
	memset(c, 0, sizeof *c);
	c->dx = (int16_t)Stereo_dx;
	c->target = current_target;
	c->tint[0] = c->tint[1] = c->tint[2] = c->tint[3] = 1;
	return c;
}

/* displacement captured per GPU surface (Water) */
typedef struct {
	const BitmapData *surface;
	const BitmapData *map;
	Point mapPoint;
	uint32_t cx, cy;
	Number scaleX, scaleY;
	bool valid;
} SurfaceRipple;
static SurfaceRipple ripples[4];

static SurfaceRipple *ripple_of(const BitmapData *surface, bool create)
{
	for (int i = 0; i < 4; i++)
		if (ripples[i].surface == surface)
			return &ripples[i];
	if (!create)
		return NULL;
	for (int i = 0; i < 4; i++)
		if (ripples[i].surface == NULL) {
			ripples[i].surface = surface;
			return &ripples[i];
		}
	return NULL;
}

/* screen y of the water's top row this frame (-1e9 = no water drawn yet); see Gpu_stereoWaterDxAt */
static Number Gpu_stereoWaterTop = -1e9;

void Gpu_beginFrame(BitmapData *screen)
{
	BitmapData_flushDisposed();
	for (int i = 0; i < 4; i++)
		ripples[i].valid = false;
	Gpu_screen = screen;
	Gpu_stereoWaterTop = -1e9; /* set again when the water is recorded this frame */
	Gpu_list.count = 0;
	Gpu_list.filterAt = -1;
	pending.target = NULL;
	pending.map = NULL;
	current_target = NULL;
}

static bool active(const BitmapData *self)
{
	return Gpu_recording && self != NULL && (self == Gpu_screen || self->gpuSurface);
}


static void fill_cmd(Rectangle r, uint32_t color)
{
	GpuCmd *c = push();
	c->op = GPUCMD_FILL;
	c->color = color;
	c->fx = (float)r.x;
	c->fy = (float)r.y;
	c->fw = (float)r.width;
	c->fh = (float)r.height;
}

/* stereo: a fill that covers a screen edge is extended past it by the shift (see stereo_extend) */
static Rectangle stereo_fill(Rectangle rect)
{
	int dx = Stereo_dx;
	if (dx == 0 || current_target != NULL || Gpu_screen == NULL)
		return rect;
	if (dx > 0 && rect.x <= 0 && rect.x + rect.width > 0) {
		rect.x -= dx;
		rect.width += dx;
	} else if (dx < 0 && rect.x + rect.width >= Gpu_screen->width && rect.x < Gpu_screen->width) {
		rect.width += -dx;
	}
	return rect;
}

bool Gpu_fillRect(BitmapData *self, Rectangle rect, uint32_t color)
{
	if (!active(self))
		return false;
	current_target = self == Gpu_screen ? NULL : self;
	fill_cmd(stereo_fill(rect), self->transparent ? color : (color | 0xFF000000u));
	return true;
}

static uint32_t channel(uint32_t p, uint32_t component)
{
	/* BitmapDataChannel: 1 red, 2 green, 4 blue, 8 alpha */
	return component == 1 ? (p >> 16) & 255 : component == 2 ? (p >> 8) & 255 : component == 4 ? p & 255 : p >> 24;
}

/* one strip of the reflection: target pixels [x0,x1) x [y0,y1) showing the
   scene through pending.m, displaced by (ddx, ddy) in target pixels */
static void strip(const BitmapData *drawn, const Matrix *toScreen, Number x0, Number y0, Number x1, Number y1, Number ddx, Number ddy,
	bool cont)
{
	Matrix p = pending.m, s = *toScreen;
	/* target <- scene: q = P p - d ; screen <- target: S q */
	Matrix pd = {p.a, p.b, p.c, p.d, p.tx - ddx, p.ty - ddy};
	GpuCmd *c = push();
	c->op = GPUCMD_REFLECT;
	c->target = NULL;
	c->cont = cont;
	c->a = (float)(s.a * pd.a + s.c * pd.b);
	c->b = (float)(s.b * pd.a + s.d * pd.b);
	c->c = (float)(s.a * pd.c + s.c * pd.d);
	c->d = (float)(s.b * pd.c + s.d * pd.d);
	c->tx = (float)(s.a * pd.tx + s.c * pd.ty + s.tx);
	c->ty = (float)(s.b * pd.tx + s.d * pd.ty + s.ty);
	Number det = pd.a * pd.d - pd.b * pd.c;
	Number xs[4] = {x0, x1, x0, x1}, ys[4] = {y0, y0, y1, y1};
	Number a0 = 1e9, b0 = 1e9, a1 = -1e9, b1 = -1e9;
	for (int i = 0; i < 4; i++) {
		Number u = xs[i] - pd.tx, v = ys[i] - pd.ty;
		Number x = (pd.d * u - pd.c * v) / det, y = (-pd.b * u + pd.a * v) / det;
		a0 = fmin(a0, x), b0 = fmin(b0, y), a1 = fmax(a1, x), b1 = fmax(b1, y);
	}
	a0 = fmax(a0, 0), b0 = fmax(b0, 0);
	a1 = fmin(a1, Gpu_screen->width), b1 = fmin(b1, Gpu_screen->height);
	c->sx = (float)a0, c->sy = (float)b0, c->sw = (float)(a1 - a0), c->sh = (float)(b1 - b0);
	memcpy(c->tint, pending.tint, sizeof c->tint);
	(void)drawn;
}

static void reflection_after(const BitmapData *drawn, const BitmapData *alias, const Matrix *toScreen)
{
	if (pending.target == NULL || (drawn != pending.target && alias != pending.target))
		return;
	BitmapData *saved = current_target;
	current_target = NULL;
	const BitmapData *t = pending.target;
	if (pending.map == NULL) {
		strip(t, toScreen, 0, 0, t->width, t->height, 0, 0, false);
	} else {
		/* DisplacementMapFilter: out(x,y) = in(x + (C(map)-128)*scaleX/256, ...)
		   sampled per row and per 16-pixel segment */
		bool cont = false;
		for (int32_t y = 0; y < t->height; y++)
			for (int32_t x = 0; x < t->width; x += 16) {
				int32_t mx = (int32_t)(x + 8 - pending.mapPoint.x), my = (int32_t)(y - pending.mapPoint.y);
				Number ddx = 0, ddy = 0;
				if (mx >= 0 && my >= 0 && mx < pending.map->width && my < pending.map->height) {
					uint32_t m = pending.map->pixels[my * pending.map->width + mx];
					ddx = ((Number)channel(m, pending.cx) - 128) * pending.scaleX / 256;
					ddy = ((Number)channel(m, pending.cy) - 128) * pending.scaleY / 256;
				}
				strip(t, toScreen, x, y, x + 16 < t->width ? x + 16 : t->width, y + 1, floor(ddx), floor(ddy), cont);
				cont = true;
			}
	}
	pending.target = NULL;
	pending.map = NULL;
	current_target = saved;
}

/* Stereo (right eye): every command is played shifted by its Stereo_dx. A layer that covers the left
   (right) screen edge then leaves |dx| columns of that edge uncovered. The world is horizontal and
   continues past the screen edge, so the layer is extended there with its own content: the source
   pixels beside the rectangle when the bitmap has them, otherwise (a layer that is exactly screen
   sized, like the sky or the haze) its edge column stretched over the gap. The extension lies
   outside the screen for the left eye (no shift), so it is invisible there. */
static void stereo_extend(const BitmapData *source, Rectangle r, Point p)
{
	int dx = Stereo_dx;
	if (dx == 0 || current_target != NULL || Gpu_screen == NULL)
		return;
	int W = Gpu_screen->width;
	bool full = r.width >= W - 2; /* a screen-sized layer: nothing exists beside it */
	int need = 0;
	bool left = false;
	if (dx > 0 && p.x <= 0 && p.x + r.width > 0) {
		need = (int)ceil(p.x + dx); /* columns [0, p.x + dx) stay uncovered after the shift */
		left = true;
	} else if (dx < 0 && p.x < W && p.x + r.width >= W) {
		need = (int)ceil(W - (p.x + r.width + dx)); /* columns [p.x + w + dx, W) */
	} else {
		return;
	}
	if (need <= 0)
		return;
	GpuCmd *c = push();
	c->op = GPUCMD_IMAGE;
	c->src = (BitmapData *)source;
	c->sh = (float)r.height;
	c->a = c->d = 1;
	c->ty = (float)(p.y - r.y);
	if (left) {
		if (r.x >= need) { /* real content beside the rectangle */
			c->sx = (float)(r.x - need), c->sy = (float)r.y, c->sw = (float)need;
			c->tx = (float)(p.x - r.x); /* continues at the same scale: dest = src + (p.x - r.x) */
		} else if (full) { /* stretch the first column */
			c->sx = (float)r.x, c->sy = (float)r.y, c->sw = 1;
			c->a = (float)need;
			c->tx = (float)(p.x - need) - c->a * c->sx;
		} else {
			Gpu_list.count--; /* nothing to extend with */
			return;
		}
	} else {
		if (r.x + r.width + need <= source->width) {
			c->sx = (float)(r.x + r.width), c->sy = (float)r.y, c->sw = (float)need;
			c->tx = (float)(p.x - r.x);
		} else if (full) { /* stretch the last column */
			c->sx = (float)(r.x + r.width - 1), c->sy = (float)r.y, c->sw = 1;
			c->a = (float)need;
			c->tx = (float)(p.x + r.width) - c->a * c->sx;
		} else {
			Gpu_list.count--;
			return;
		}
	}
}

bool Gpu_copyPixels(BitmapData *self, const BitmapData *source, Rectangle r, Point p)
{
	if (Gpu_recording && source != NULL && source->gpuSurface && !active(self))
		return true; /* e.g. calcFrame of the darkness sprite: drawn from the surface instead */
	if (!active(self))
		return false;
	current_target = self == Gpu_screen ? NULL : self;
	/* BitmapData.copyPixels works on whole pixels: the CPU path (copyPixels_impl) floors the
	   source rectangle and the destination point. Recording the fractional values made the
	   GPU round to nearest instead (a 1 px shift of the tilemap and every copyPixels sprite
	   whenever the camera scroll's fraction was >= 0.5). */
	double sx0 = floor(r.x), sy0 = floor(r.y);
	double w = floor(r.x + r.width) - sx0, h = floor(r.y + r.height) - sy0;
	double dx0 = floor(p.x), dy0 = floor(p.y);
	/* copyPixels also clips the source rectangle to the source bitmap (copyPixels_impl).
	   Without this a rectangle outside the bitmap sampled the neighbouring bitmap of the
	   texture page: Reed.update can pick frame 3 of the 3-frame reed.png (calcFrame then
	   reads the row below the image), which drew a brown square instead of nothing. */
	if (sx0 < 0) { w += sx0; dx0 -= sx0; sx0 = 0; }
	if (sy0 < 0) { h += sy0; dy0 -= sy0; sy0 = 0; }
	if (sx0 + w > source->width) w = source->width - sx0;
	if (sy0 + h > source->height) h = source->height - sy0;
	if (w <= 0 || h <= 0)
		return true; /* nothing to copy: the CPU path draws nothing either */
	r = Rectangle_new(sx0, sy0, w, h);
	p = Point_new(dx0, dy0);
	if (source->solid) {
		fill_cmd(stereo_fill(Rectangle_new(p.x, p.y, r.width, r.height)), source->solidColor);
		return true;
	}
	GpuCmd *c = push();
	c->op = GPUCMD_IMAGE;
	c->src = (BitmapData *)source;
	c->sx = (float)r.x, c->sy = (float)r.y, c->sw = (float)r.width, c->sh = (float)r.height;
	c->a = c->d = 1;
	c->tx = (float)(p.x - r.x), c->ty = (float)(p.y - r.y);
	Matrix m = {1, 0, 0, 1, p.x - r.x, p.y - r.y};
	stereo_extend(source, r, p);
	if (current_target == NULL)
		reflection_after(source, NULL, &m);
	return true;
}

bool Gpu_draw(BitmapData *self, const BitmapData *source, const Matrix *matrix, const ColorTransform *ct, const char *blend)
{
	if (Gpu_recording && source == Gpu_screen && self != NULL && self->gpuSurface && Gpu_screen != NULL) {
		/* Water: the flipped camera buffer composed inside the surface
		   (prototype: KG_WATER_COMPOSE) */
		Matrix m = matrix ? *matrix : Matrix_new();
		current_target = self;
		GpuCmd *c = push();
		c->op = GPUCMD_REFLECT;
		c->sw = (float)Gpu_screen->width, c->sh = (float)Gpu_screen->height;
		c->a = (float)m.a, c->b = (float)m.b, c->c = (float)m.c, c->d = (float)m.d, c->tx = (float)m.tx, c->ty = (float)m.ty;
		if (ct) {
			c->tint[0] = (float)ct->redMultiplier, c->tint[1] = (float)ct->greenMultiplier;
			c->tint[2] = (float)ct->blueMultiplier, c->tint[3] = (float)ct->alphaMultiplier;
		}
		return true;
	}
	if (Gpu_recording && source == Gpu_screen && self != Gpu_screen && Gpu_screen != NULL) {
		/* Water.draw: target <- flipped camera buffer; replayed later */
		Matrix m = matrix ? *matrix : Matrix_new();
		pending.target = self;
		pending.m = m;
		pending.tint[0] = ct ? (float)ct->redMultiplier : 1;
		pending.tint[1] = ct ? (float)ct->greenMultiplier : 1;
		pending.tint[2] = ct ? (float)ct->blueMultiplier : 1;
		pending.tint[3] = ct ? (float)ct->alphaMultiplier : 1;
		return true;
	}
	if (Gpu_recording && source != NULL && source->gpuSurface && !active(self))
		return true;
	if (!active(self))
		return false;
	current_target = self == Gpu_screen ? NULL : self;
	Matrix m = matrix ? *matrix : Matrix_new();
	GpuCmd *c = push();
	c->op = GPUCMD_IMAGE;
	c->src = (BitmapData *)source;
	c->sw = (float)source->width, c->sh = (float)source->height;
	c->a = (float)m.a, c->b = (float)m.b, c->c = (float)m.c, c->d = (float)m.d, c->tx = (float)m.tx, c->ty = (float)m.ty;
	if (ct) {
		c->tint[0] = (float)ct->redMultiplier, c->tint[1] = (float)ct->greenMultiplier;
		c->tint[2] = (float)ct->blueMultiplier, c->tint[3] = (float)ct->alphaMultiplier;
	}
	c->blend = blend == NULL ? GPUBLEND_NORMAL : as3_streq(blend, "multiply") ? GPUBLEND_MULTIPLY :
		as3_streq(blend, "screen") ? GPUBLEND_SCREEN : GPUBLEND_NORMAL;
	if (current_target == NULL)
		reflection_after(source, NULL, &m);
	return true;
}

/* Stereo water: the surface is a plane that comes towards the viewer. Row at screen y shifts by
   Stereo_dx - round(slope * t), t = 0 at the waterline (the back) .. 1 at the bottom of the
   screen (the front), so the nearer rows move further to the left in the right eye. */
int Gpu_waterSeg = 8;
int Gpu_waterSlope = 4;
const BitmapData *Gpu_stereoWater; /* set by the platform: the Water sprite's surface bitmap */

/* the slope in pixels for this sheet: the water only, capped at 4 so the 296 px wide surface still
   covers the right screen edge */
static int water_slope(const BitmapData *sheet)
{
	if (sheet == NULL || sheet != Gpu_stereoWater || Stereo_maxShift <= 0 || current_target != NULL || Gpu_screen == NULL)
		return 0;
	return Stereo_maxShift < Gpu_waterSlope ? Stereo_maxShift : Gpu_waterSlope;
}

/* right-eye shift of the water surface at a screen row (0 when the water was not drawn this frame):
   what an object floating above the water at that row has to stay in front of (fog) */
int Gpu_stereoWaterDxAt(Number screenY)
{
	if (Gpu_stereoWaterTop < -1e8 || Gpu_screen == NULL || Stereo_maxShift <= 0)
		return 0;
	int slope = Stereo_maxShift < Gpu_waterSlope ? Stereo_maxShift : Gpu_waterSlope;
	Number span = Gpu_screen->height - Gpu_stereoWaterTop;
	Number t = span > 0 ? (screenY - Gpu_stereoWaterTop) / span : 0;
	t = t < 0 ? 0 : (t > 1 ? 1 : t);
	return -(int)floor(slope * t + 0.5);
}

static int water_row_dx(int slope, Number rowY, Number topY)
{
	if (slope <= 0)
		return Stereo_dx;
	Number span = Gpu_screen->height - topY;
	Number t = span > 0 ? (rowY - topY) / span : 0;
	t = t < 0 ? 0 : (t > 1 ? 1 : t);
	return Stereo_dx - (int)floor(slope * t + 0.5);
}

bool Gpu_drawSprite(BitmapData *screen, BitmapData *sheet, BitmapData *frame, Number sx, Number sy, Number w, Number h,
	const Matrix *m, const ColorTransform *ct, const char *blend)
{
	/* the camera buffer, or a GPU surface (stamp into PlayState.darkness / Water) */
	if (!active(screen))
		return false;
	/* offsets cannot be expressed as a GPU tint */
	if (ct && (ct->redOffset || ct->greenOffset || ct->blueOffset || ct->alphaOffset))
		return false;
	/* a plain single-frame sprite gains nothing (framePixels only changes
	   with the bitmap); tinted ones do: no per-frame colourTransform+upload */
	if (ct == NULL && !sheet->gpuSurface && sheet->width == (int32_t)w && sheet->height == (int32_t)h)
		return false;
	current_target = screen == Gpu_screen ? NULL : screen;
	SurfaceRipple *r = sheet->gpuSurface && current_target == NULL ? ripple_of(sheet, false) : NULL;
	const int slope = water_slope(sheet); /* stereo water depth: 0 unless this is the water in stereo mode */
	if (slope > 0)
		Gpu_stereoWaterTop = m->ty;
	if (r != NULL && r->valid) {
		/* DisplacementMapFilter on the surface, as strips (prototype:
		   KG_WATER_SAMPLE): out(x,y) = in(x + (C(map)-128)*scaleX/256, ...) */
		for (int32_t y = 0; y < (int32_t)h; y++) {
			/* only rows that land on the screen */
			Number rowY = m->b * 0 + m->d * y + m->ty;
			if (rowY + fabs(m->d) <= 0 || rowY >= Gpu_screen->height)
				continue;
			int run = -1; /* index of the last strip of this row, unclamped */
			Number runDx = 0, runDy = 0;
			for (int32_t x = 0; x < (int32_t)w; x += Gpu_waterSeg) {
				Number colX = m->a * x + m->tx;
				if (colX + Gpu_waterSeg * fabs(m->a) <= 0 || colX >= Gpu_screen->width)
					continue;
				int32_t mx = (int32_t)(x + Gpu_waterSeg / 2 - r->mapPoint.x), my = (int32_t)(y - r->mapPoint.y);
				Number ddx = 0, ddy = 0;
				if (mx >= 0 && my >= 0 && mx < r->map->width && my < r->map->height) {
					uint32_t mp = r->map->pixels[my * r->map->width + mx];
					ddx = floor(((Number)channel(mp, r->cx) - 128) * r->scaleX / 256);
					ddy = floor(((Number)channel(mp, r->cy) - 128) * r->scaleY / 256);
				}
				Number sx0 = fmin(fmax(x + ddx, 0), w - 1), sy0 = fmin(fmax(y + ddy, 0), h - 1);
				Number sw0 = fmin(Gpu_waterSeg, w - x);
				if (sx0 + sw0 > w)
					sx0 = w - sw0;
				/* neighbours with the same displacement read adjacent source
				   pixels: one longer strip, same pixels */
				bool exact = sx0 == x + ddx && sy0 == y + ddy;
				if (exact && run >= 0 && runDx == ddx && runDy == ddy &&
					Gpu_list.cmds[run].sx + Gpu_list.cmds[run].sw == (float)sx0) {
					Gpu_list.cmds[run].sw += (float)sw0;
					continue;
				}
				GpuCmd *c = push();
				run = exact ? Gpu_list.count - 1 : -1, runDx = ddx, runDy = ddy;
				c->dx = (int16_t)water_row_dx(slope, rowY, m->ty); /* stereo water slope (no-op without it) */
				c->op = GPUCMD_IMAGE;
				c->src = sheet;
				c->sx = (float)sx0, c->sy = (float)sy0, c->sw = (float)sw0, c->sh = 1;
				/* source (sx0,sy0) lands on frame pixel (x,y) */
				c->a = (float)m->a, c->b = (float)m->b, c->c = (float)m->c, c->d = (float)m->d;
				c->tx = (float)(m->a * (x - sx0) + m->c * (y - sy0) + m->tx);
				c->ty = (float)(m->b * (x - sx0) + m->d * (y - sy0) + m->ty);
				if (ct) {
					c->tint[0] = (float)ct->redMultiplier, c->tint[1] = (float)ct->greenMultiplier;
					c->tint[2] = (float)ct->blueMultiplier, c->tint[3] = (float)ct->alphaMultiplier;
				}
			}
		}
		return true;
	}
	GpuCmd *c = push();
	c->op = GPUCMD_IMAGE;
	c->src = sheet;
	c->sx = (float)sx, c->sy = (float)sy, c->sw = (float)w, c->sh = (float)h;
	/* texture pixel -> frame pixel (minus sx,sy) -> screen */
	c->a = (float)m->a, c->b = (float)m->b, c->c = (float)m->c, c->d = (float)m->d;
	c->tx = (float)(m->tx - m->a * sx - m->c * sy);
	c->ty = (float)(m->ty - m->b * sx - m->d * sy);
	if (ct) {
		c->tint[0] = (float)ct->redMultiplier, c->tint[1] = (float)ct->greenMultiplier;
		c->tint[2] = (float)ct->blueMultiplier, c->tint[3] = (float)ct->alphaMultiplier;
	}
	c->blend = blend == NULL ? GPUBLEND_NORMAL : as3_streq(blend, "multiply") ? GPUBLEND_MULTIPLY :
		as3_streq(blend, "screen") ? GPUBLEND_SCREEN : GPUBLEND_NORMAL;
	if (slope > 0 && current_target == NULL && m->b == 0 && m->c == 0 && m->d == 1 && h > 1) {
		/* stereo water without ripples: bands of rows that share one shift (see water_row_dx) */
		GpuCmd base = *c;
		Gpu_list.count--;
		Number span = Gpu_screen->height - m->ty;
		for (int k = 0; k <= slope; k++) {
			int r0 = k == 0 ? 0 : (int)ceil(span * (k - 0.5) / slope);
			int r1 = k == slope ? (int)h : (int)ceil(span * (k + 0.5) / slope);
			r0 = r0 < 0 ? 0 : r0;
			r1 = r1 > (int)h ? (int)h : r1;
			if (r1 <= r0)
				continue;
			GpuCmd *band = push();
			*band = base;
			band->dx = (int16_t)(Stereo_dx - k);
			band->sy = (float)(sy + r0);
			band->sh = (float)(r1 - r0);
		}
	}
	Matrix toScreen = {m->a, m->b, m->c, m->d, m->tx, m->ty};
	if (current_target == NULL)
		reflection_after(frame, sheet, &toScreen);
	return true;
}

bool Gpu_screenFilter(BitmapData *self)
{
	/* the final Weather ColorMatrix (and colourTransforms) on the camera
	   buffer are applied by the platform's present pass, at the recorded point */
	if (self == Gpu_screen && Gpu_recording && Gpu_list.filterAt < 0)
		Gpu_list.filterAt = Gpu_list.count;
	return active(self);
}

/* ---- CPU reference player ----------------------------------------------- */

static void play_into(const GpuList *list, int from, int upto, BitmapData *dst, const BitmapData *target, bool rightEye, BitmapData **copy);

static const BitmapData *reference_scene;

static BitmapData *surface_image(const GpuList *list, int upto, const BitmapData *surface)
{
	/* compose a gpuSurface from its recorded commands (CPU reference) */
	BitmapData *img = BitmapData_new(surface->width, surface->height, surface->transparent, 0);
	BitmapData *copy = (BitmapData *)reference_scene; /* REFLECT inside a surface reads the scene */
	play_into(list, 0, upto, img, surface, false, &copy);
	return img;
}

static void play_into(const GpuList *list, int from, int upto, BitmapData *dst, const BitmapData *target, bool rightEye, BitmapData **copy)
{
	for (int i = from; i < upto; i++) {
		const GpuCmd *c = &list->cmds[i];
		if (c->target != target)
			continue;
		float dx = rightEye ? c->dx : 0;
		if (c->op == GPUCMD_FILL) {
			BitmapData_fillRect(dst, Rectangle_new(c->fx + dx, c->fy, c->fw, c->fh), c->color);
			continue;
		}
		const BitmapData *src = c->src;
		if (c->op == GPUCMD_REFLECT && target != NULL) {
			src = *copy; /* the scene so far */
		} else if (c->op == GPUCMD_REFLECT) {
			if (!c->cont) {
				if (*copy == NULL || (*copy)->width != dst->width || (*copy)->height != dst->height)
					*copy = BitmapData_new(dst->width, dst->height, true, 0);
				memcpy((*copy)->pixels, dst->pixels, sizeof(uint32_t) * (size_t)dst->width * (size_t)dst->height);
			}
			src = *copy;
		} else if (src->gpuSurface) {
			/* compose once per run of draws from the same surface */
			static const BitmapData *cachedSurface;
			static BitmapData *cachedImage;
			static int cachedAt = -1;
			bool fresh = cachedSurface != src || cachedAt < 0 || i < cachedAt;
			for (int k = cachedAt; !fresh && k < i; k++)
				fresh = list->cmds[k].target == src;
			if (fresh) {
				reference_scene = dst;
				cachedImage = surface_image(list, i, src);
				cachedSurface = src;
			}
			cachedAt = i;
			src = cachedImage;
		}
		if (c->sw <= 0 || c->sh <= 0)
			continue;
		BitmapData *part = BitmapData_new((int32_t)ceil(c->sw), (int32_t)ceil(c->sh), true, 0);
		BitmapData_copyPixels(part, src, Rectangle_new(c->sx, c->sy, c->sw, c->sh), Point_new(0, 0), false);
		Matrix m = {c->a, c->b, c->c, c->d, c->a * c->sx + c->c * c->sy + c->tx + dx, c->b * c->sx + c->d * c->sy + c->ty};
		ColorTransform ct = ColorTransform_new(c->tint[0], c->tint[1], c->tint[2], c->tint[3], 0, 0, 0, 0);
		static const char *names[] = {"normal", "multiply", "screen"};
		BitmapData_draw(dst, part, &m, &ct, names[c->blend], NULL, false);
	}
}

void Gpu_playSoftware(const GpuList *list, BitmapData *dst, bool rightEye)
{
	bool was = Gpu_recording;
	Gpu_recording = false;
	BitmapData *copy = NULL;
	play_into(list, 0, list->count, dst, NULL, rightEye, &copy);
	Gpu_recording = was;
}

void Gpu_playSoftwareFiltered(const GpuList *list, BitmapData *dst, bool rightEye, void (*filter)(BitmapData *dst))
{
	bool was = Gpu_recording;
	Gpu_recording = false;
	BitmapData *copy = NULL;
	int at = list->filterAt >= 0 && list->filterAt <= list->count ? list->filterAt : list->count;
	play_into(list, 0, at, dst, NULL, rightEye, &copy);
	if (filter != NULL && list->filterAt >= 0)
		filter(dst);
	play_into(list, at, list->count, dst, NULL, rightEye, &copy);
	Gpu_recording = was;
}

bool Gpu_skipDisplacement(const BitmapData *self, const BitmapData *map, Point mapPoint, uint32_t componentX,
	uint32_t componentY, Number scaleX, Number scaleY)
{
	/* Water ripples its own bitmap after drawing the camera buffer into it.
	   On the GPU path the reflection is drawn from the scene, so the filter
	   is captured and applied to the reflection strips; the bitmap itself is
	   still displaced on the CPU (torch reflections etc. ripple as before). */
	if (Gpu_recording && self != NULL && self->gpuSurface) {
		SurfaceRipple *r = ripple_of(self, true);
		if (r) {
			r->map = map, r->mapPoint = mapPoint, r->cx = componentX, r->cy = componentY;
			r->scaleX = scaleX, r->scaleY = scaleY, r->valid = true;
			/* the strips read one map column per 16 pixels (x + 8 - mapPoint.x):
			   perlinNoise may skip the others */
			if (map != NULL && mapPoint.x == floor(mapPoint.x))
				((BitmapData *)map)->gpuSparse = (int8_t)((((8 - (int32_t)mapPoint.x) % 16) + 16) % 16);
			return true; /* applied by the GPU when the surface is drawn */
		}
	}
	if (Gpu_recording && self != NULL && self == pending.target) {
		pending.map = map;
		pending.mapPoint = mapPoint;
		pending.cx = componentX;
		pending.cy = componentY;
		pending.scaleX = scaleX;
		pending.scaleY = scaleY;
	}
	return false;
}

BitmapData *Gpu_debugSurface(const BitmapData *surface)
{
	bool was = Gpu_recording;
	Gpu_recording = false;
	BitmapData *b = surface_image(&Gpu_list, Gpu_list.count, surface);
	Gpu_recording = was;
	return b;
}
