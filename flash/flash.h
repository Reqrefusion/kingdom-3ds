/* Software implementation of the flash.* API surface used by Flixel and
 * Kingdom: Point, Rectangle, Matrix, ColorTransform and BitmapData.
 *
 * Pixels are stored as unpremultiplied 0xAARRGGBB, the format AS3 code sees
 * through getPixel32/setPixel32. Blending follows the documented Flash
 * formulas with 8-bit channels; Flash's exact internal rounding (it stores
 * premultiplied pixels) is not reproduced bit for bit. Every such place is
 * marked "ADAPTATION" so it can be checked against a Flash reference later.
 */
#ifndef TRANSLIT_FLASH_H
#define TRANSLIT_FLASH_H
#include "../as3/as3.h"

/* ---- flash.geom ------------------------------------------------------- */

typedef struct Point {
	Number x, y;
} Point;

typedef struct Rectangle {
	Number x, y, width, height;
} Rectangle;

typedef struct Matrix {
	Number a, b, c, d, tx, ty;
} Matrix;

typedef struct ColorTransform {
	Number redMultiplier, greenMultiplier, blueMultiplier, alphaMultiplier;
	Number redOffset, greenOffset, blueOffset, alphaOffset;
} ColorTransform;

/* flash.utils.getTimer(): milliseconds since the program started */
uint32_t getTimer(void);

static inline Point Point_new(Number x, Number y) { return (Point){x, y}; }
static inline Rectangle Rectangle_new(Number x, Number y, Number w, Number h) { return (Rectangle){x, y, w, h}; }
/* flash.geom.Rectangle.right/bottom/contains */
static inline Number Rectangle_get_right(const Rectangle *r) { return r->x + r->width; }
static inline Number Rectangle_get_bottom(const Rectangle *r) { return r->y + r->height; }
static inline bool Rectangle_contains(const Rectangle *r, Number x, Number y)
{
	return x >= r->x && x < r->x + r->width && y >= r->y && y < r->y + r->height;
}

static inline Matrix Matrix_new(void) { return (Matrix){1, 0, 0, 1, 0, 0}; }
void Matrix_identity(Matrix *m);
void Matrix_translate(Matrix *m, Number dx, Number dy);
void Matrix_scale(Matrix *m, Number sx, Number sy);
void Matrix_rotate(Matrix *m, Number angle);
void Matrix_concat(Matrix *m, const Matrix *other);

ColorTransform ColorTransform_new(Number redMultiplier, Number greenMultiplier, Number blueMultiplier,
	Number alphaMultiplier, Number redOffset, Number greenOffset, Number blueOffset, Number alphaOffset);

/* ---- flash.display.BitmapData ------------------------------------------ */

typedef struct BitmapData {
	int32_t width;
	int32_t height;
	bool transparent;
	uint32_t *pixels; /* width*height, row-major, 0xAARRGGBB unpremultiplied */
	uint32_t gen;     /* bumped on every pixel write (GPU texture cache) */
	void *gpu;        /* platform texture, owned by the GPU backend */
	uint32_t gpuGen;  /* gen the texture was uploaded from */
	bool gpuSurface;
	int8_t gpuSparse;  /* >= 0: only columns x % 16 == gpuSparse are read (GPU ripple map) */  /* composed on the GPU (e.g. PlayState.darkness) */
	bool solid;       /* last write was a fillRect over the whole bitmap */
	uint32_t solidColor;
} BitmapData;

/* new BitmapData(width, height, transparent=true, fillColor=0xFFFFFFFF) */
BitmapData *BitmapData_new(int32_t width, int32_t height, bool transparent, uint32_t fillColor);
BitmapData *BitmapData_clone(const BitmapData *self);
void BitmapData_dispose(BitmapData *self);

uint32_t BitmapData_getPixel(const BitmapData *self, int32_t x, int32_t y);
uint32_t BitmapData_getPixel32(const BitmapData *self, int32_t x, int32_t y);
bool BitmapData_hitTest(const BitmapData *self, Point firstPoint, uint32_t firstAlphaThreshold, Point secondPoint);
void BitmapData_setPixel(BitmapData *self, int32_t x, int32_t y, uint32_t color);
void BitmapData_setPixel32(BitmapData *self, int32_t x, int32_t y, uint32_t color);
/* Source-over of one unpremultiplied pixel (shared by the rasterisers). */
uint32_t BitmapData_blendOver(uint32_t src, uint32_t dst, bool dstTransparent);
void BitmapData_fillRect(BitmapData *self, Rectangle rect, uint32_t color);
/* stereo mirror (3DS): writes into Stereo_left are repeated into Stereo_right
   shifted by Stereo_dx; inactive while Stereo_right is NULL */
extern BitmapData *Stereo_left, *Stereo_right;
extern int32_t Stereo_dx;
/* the configured maximum right-eye shift in pixels (0 = stereo off); read by the GPU recorder for
   effects that live outside the game (water depth slope, layer edge extension) */
extern int32_t Stereo_maxShift;

/* copyPixels(source, sourceRect, destPoint, alphaBitmapData=null,
   alphaPoint=null, mergeAlpha=false). alphaBitmapData is not used by the
   game and is not supported. */
void BitmapData_copyPixels(BitmapData *self, const BitmapData *source, Rectangle sourceRect,
	Point destPoint, bool mergeAlpha);

void BitmapData_colorTransform(BitmapData *self, Rectangle rect, const ColorTransform *colorTransform);

/* Blend modes accepted by draw(). NULL / "normal" is BLEND_NORMAL. */
typedef enum BlendMode {
	BLEND_NORMAL = 0,
	BLEND_MULTIPLY,
	BLEND_SCREEN,
	BLEND_ADD,
	BLEND_OVERLAY,
	BLEND_HARDLIGHT,
} BlendMode;
BlendMode BlendMode_fromString(const char *blend);

/* draw(source, matrix=null, colorTransform=null, blendMode=null,
   clipRect=null, smoothing=false). Nearest-neighbour sampling through the
   inverse matrix; `smoothing` is accepted but ignored (ADAPTATION). */
void BitmapData_draw(BitmapData *self, const BitmapData *source, const Matrix *matrix,
	const ColorTransform *colorTransform, const char *blendMode, const Rectangle *clipRect, bool smoothing);

/* noise(randomSeed, low=0, high=255, channelOptions=7, grayScale=false).
   ADAPTATION: Flash's internal generator is undocumented; this uses the
   Park-Miller generator, which matches documented range/channel behaviour. */
void BitmapData_noise(BitmapData *self, int32_t randomSeed, uint32_t low, uint32_t high,
	uint32_t channelOptions, bool grayScale);

/* Bitmap/Shape stand-in for `(new Graphic).bitmapData` of embedded images.
   Loaded once from the generated asset store. */
BitmapData *Embed_bitmapData(const char *embedName);
/* `(new Graphic).bitmapData.width` without decoding the pixels. */
int32_t Embed_bitmapWidth(const char *embedName);
extern const char *Embed_root;

#endif
