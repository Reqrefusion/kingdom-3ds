#ifndef TRANSLIT_FLASH_FILTERS_H
#define TRANSLIT_FLASH_FILTERS_H
#include "flash.h"

void Matrix_createGradientBox(Matrix *m, Number width, Number height, Number rotation, Number tx, Number ty);

#define SHAPE_MAX_STOPS 8
typedef struct Graphics {
	bool hasFill, hasRect;
	uint32_t count;
	uint32_t colors[SHAPE_MAX_STOPS];
	Number alphas[SHAPE_MAX_STOPS];
	Number ratios[SHAPE_MAX_STOPS];
	Matrix matrix;
	Rectangle rect;
} Graphics;
typedef struct Shape {
	Graphics graphics;
} Shape;
Shape *Shape_new(void);
void Graphics_beginGradientFill(Graphics *g, const char *type, const uint32_t *colors, const Number *alphas,
	const Number *ratios, uint32_t count, const Matrix *matrix, const char *spreadMethod, const char *interpolationMethod);
void Graphics_drawRect(Graphics *g, Number x, Number y, Number width, Number height);
void BitmapData_drawShape(BitmapData *self, const Shape *shape, const Matrix *matrix);

void BitmapData_perlinNoise(BitmapData *self, Number baseX, Number baseY, uint32_t numOctaves, int32_t randomSeed,
	bool stitch, bool fractalNoise, uint32_t channelOptions, bool grayScale, const Point *offsets, uint32_t offsetCount);

typedef struct DisplacementMapFilter {
	BitmapData *mapBitmap;
	Point mapPoint;
	uint32_t componentX, componentY;
	Number scaleX, scaleY;
	const char *mode;
	uint32_t color;
	Number alpha;
} DisplacementMapFilter;
#define DisplacementMapFilterMode_COLOR "color"
DisplacementMapFilter *DisplacementMapFilter_new(BitmapData *mapBitmap, Point mapPoint, uint32_t componentX,
	uint32_t componentY, Number scaleX, Number scaleY, const char *mode, uint32_t color, Number alpha);
void BitmapData_applyDisplacementFilter(BitmapData *self, const BitmapData *source, Rectangle sourceRect,
	Point destPoint, const DisplacementMapFilter *f);
void BitmapData_applyColorMatrixFilter(BitmapData *self, const BitmapData *source, Rectangle sourceRect,
	Point destPoint, const Number matrix[20]);

/* Platform hook: run fn over rows [0, rows) possibly split across cores
   (fn(ctx, y0, y1) for disjoint ranges); NULL = single thread. */
extern void (*FlashParallel_split)(void (*fn)(void *ctx, int32_t y0, int32_t y1), void *ctx, int32_t rows);

#endif
