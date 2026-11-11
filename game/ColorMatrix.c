/* com/quasimondo/geom/ColorMatrix.as (functions used by Weather and
 * PlayState: constructor, reset, adjustSaturation, adjustContrast,
 * colorize, concat, applyFilter); the other operations follow at the end of the file. */
#include "game.h"

static const Number IDENTITY[20] = {1, 0, 0, 0, 0,
	0, 1, 0, 0, 0,
	0, 0, 1, 0, 0,
	0, 0, 0, 1, 0};

void ColorMatrix_ctor(ColorMatrix *self)
{
	self->preHue = NULL; /* private var preHue / postHue / hueInitialized */
	self->postHue = NULL;
	self->hueInitialized = false;
	ColorMatrix_reset(self);
}

void ColorMatrix_reset(ColorMatrix *self)
{
	memcpy(self->matrix, IDENTITY, sizeof IDENTITY);
}

void ColorMatrix_adjustSaturation(ColorMatrix *self, Number s)
{
	Number sInv;
	Number irlum;
	Number iglum;
	Number iblum;
	sInv = (1 - s);
	irlum = (sInv * ColorMatrix_LUMA_R);
	iglum = (sInv * ColorMatrix_LUMA_G);
	iblum = (sInv * ColorMatrix_LUMA_B);
	ColorMatrix_concat(self, (const Number[20]){(irlum + s), iglum, iblum, 0, 0,
		irlum, (iglum + s), iblum, 0, 0,
		irlum, iglum, (iblum + s), 0, 0,
		0, 0, 0, 1, 0});
}

void ColorMatrix_adjustContrast(ColorMatrix *self, Number r, Number g, Number b)
{
	if (isnan(g)) g = r;
	if (isnan(b)) b = r;
	r += 1;
	g += 1;
	b += 1;
	ColorMatrix_concat(self, (const Number[20]){r, 0, 0, 0, (128 * (1 - r)),
		0, g, 0, 0, (128 * (1 - g)),
		0, 0, b, 0, (128 * (1 - b)),
		0, 0, 0, 1, 0});
}

void ColorMatrix_colorize(ColorMatrix *self, uint32_t rgb, Number amount)
{
	Number a;
	Number r;
	Number g;
	Number b;
	Number inv_amount;

	a = (AS3_SAR(rgb, 24) / (Number)0xFF);
	r = ((AS3_SAR(rgb, 16) & 0xFF) / (Number)0xFF);
	g = ((AS3_SAR(rgb, 8) & 0xFF) / (Number)0xFF);
	b = ((rgb & 0xFF) / (Number)0xFF);
	if (amount == -1) {
		amount = a;
	}
	inv_amount = (1 - amount);

	ColorMatrix_concat(self, (const Number[20]){(inv_amount + ((amount * r) * ColorMatrix_LUMA_R)), ((amount * r) * ColorMatrix_LUMA_G), ((amount * r) * ColorMatrix_LUMA_B), 0, 0,
		((amount * g) * ColorMatrix_LUMA_R), (inv_amount + ((amount * g) * ColorMatrix_LUMA_G)), ((amount * g) * ColorMatrix_LUMA_B), 0, 0,
		((amount * b) * ColorMatrix_LUMA_R), ((amount * b) * ColorMatrix_LUMA_G), (inv_amount + ((amount * b) * ColorMatrix_LUMA_B)), 0, 0,
		0, 0, 0, 1, 0});
}

void ColorMatrix_concat(ColorMatrix *self, const Number mat[20])
{
	Number temp[20];
	int32_t i = 0;
	int32_t x, y;
	for (y = 0; y < 4; y++) {
		for (x = 0; x < 5; x++) {
			temp[i + x] = mat[i] * self->matrix[x] +
				mat[i + 1] * self->matrix[x + 5] +
				mat[i + 2] * self->matrix[x + 10] +
				mat[i + 3] * self->matrix[x + 15] +
				(x == 4 ? mat[i + 4] : 0);
		}
		i += 5;
	}
	memcpy(self->matrix, temp, sizeof temp);
}

void ColorMatrix_applyFilter(ColorMatrix *self, BitmapData *bitmapData)
{
	BitmapData_applyColorMatrixFilter(bitmapData, bitmapData, Rectangle_new(0, 0, bitmapData->width, bitmapData->height), Point_new(0, 0), self->matrix);
}

/* ---- the rest of the class: never called by the game ------------------------ */

#define ColorMatrix_RAD (Math_PI / 180)

const char *const ColorMatrix_COLOR_DEFICIENCY_TYPES[8] = {"Protanopia", "Protanomaly", "Deuteranopia", "Deuteranomaly",
	"Tritanopia", "Tritanomaly", "Achromatopsia", "Achromatomaly"};

static void rotateColor(ColorMatrix *self, Number degrees, int32_t x, int32_t y);
static void shearColor(ColorMatrix *self, int32_t x, int32_t y1, Number d1, int32_t y2, Number d2);
static void initHue(ColorMatrix *self);

void ColorMatrix_ctorFrom(ColorMatrix *self, const Number *mat)
{
	if (mat != NULL) {
		memcpy(self->matrix, mat, sizeof self->matrix);
	} else {
		ColorMatrix_reset(self);
	}
}

ColorMatrix *ColorMatrix_clone(ColorMatrix *self)
{
	ColorMatrix *c = as3_alloc(sizeof *c);
	ColorMatrix_ctorFrom(c, self->matrix);
	return c;
}

void ColorMatrix_invert(ColorMatrix *self)
{
	ColorMatrix_concat(self, (const Number[20]){-1, 0, 0, 0, 255,
		0, -1, 0, 0, 255,
		0, 0, -1, 0, 255,
		0, 0, 0, 1, 0});
}

void ColorMatrix_adjustBrightness(ColorMatrix *self, Number r, Number g, Number b)
{
	if (isnan(g)) g = r;
	if (isnan(b)) b = r;
	ColorMatrix_concat(self, (const Number[20]){1, 0, 0, 0, r,
		0, 1, 0, 0, g,
		0, 0, 1, 0, b,
		0, 0, 0, 1, 0});
}

void ColorMatrix_toGreyscale(ColorMatrix *self, Number r, Number g, Number b)
{
	ColorMatrix_concat(self, (const Number[20]){r, g, b, 0, 0,
		r, g, b, 0, 0,
		r, g, b, 0, 0,
		0, 0, 0, 1, 0});
}

void ColorMatrix_adjustHue(ColorMatrix *self, Number degrees)
{
	degrees *= ColorMatrix_RAD;
	Number cos_ = cos(degrees);
	Number sin_ = sin(degrees);
	ColorMatrix_concat(self, (const Number[20]){((ColorMatrix_LUMA_R + (cos_ * (1 - ColorMatrix_LUMA_R))) + (sin_ * -(ColorMatrix_LUMA_R))), ((ColorMatrix_LUMA_G + (cos_ * -(ColorMatrix_LUMA_G))) + (sin_ * -(ColorMatrix_LUMA_G))), ((ColorMatrix_LUMA_B + (cos_ * -(ColorMatrix_LUMA_B))) + (sin_ * (1 - ColorMatrix_LUMA_B))), 0, 0,
		((ColorMatrix_LUMA_R + (cos_ * -(ColorMatrix_LUMA_R))) + (sin_ * 0.143)), ((ColorMatrix_LUMA_G + (cos_ * (1 - ColorMatrix_LUMA_G))) + (sin_ * 0.14)), ((ColorMatrix_LUMA_B + (cos_ * -(ColorMatrix_LUMA_B))) + (sin_ * -0.283)), 0, 0,
		((ColorMatrix_LUMA_R + (cos_ * -(ColorMatrix_LUMA_R))) + (sin_ * -((1 - ColorMatrix_LUMA_R)))), ((ColorMatrix_LUMA_G + (cos_ * -(ColorMatrix_LUMA_G))) + (sin_ * ColorMatrix_LUMA_G)), ((ColorMatrix_LUMA_B + (cos_ * (1 - ColorMatrix_LUMA_B))) + (sin_ * ColorMatrix_LUMA_B)), 0, 0,
		0, 0, 0, 1, 0});
}

void ColorMatrix_rotateHue(ColorMatrix *self, Number degrees)
{
	initHue(self);
	ColorMatrix_concat(self, self->preHue->matrix);
	ColorMatrix_rotateBlue(self, degrees);
	ColorMatrix_concat(self, self->postHue->matrix);
}

void ColorMatrix_luminance2Alpha(ColorMatrix *self)
{
	ColorMatrix_concat(self, (const Number[20]){0, 0, 0, 0, 255,
		0, 0, 0, 0, 255,
		0, 0, 0, 0, 255,
		ColorMatrix_LUMA_R, ColorMatrix_LUMA_G, ColorMatrix_LUMA_B, 0, 0});
}

void ColorMatrix_adjustAlphaContrast(ColorMatrix *self, Number amount)
{
	amount += 1;
	ColorMatrix_concat(self, (const Number[20]){1, 0, 0, 0, 0,
		0, 1, 0, 0, 0,
		0, 0, 1, 0, 0,
		0, 0, 0, amount, (128 * (1 - amount))});
}

void ColorMatrix_setChannels(ColorMatrix *self, int32_t r, int32_t g, int32_t b, int32_t a)
{
	Number rf = ((((((r & 1) == 1)) ? 1 : 0 + (((r & 2) == 2)) ? 1 : 0) + (((r & 4) == 4)) ? 1 : 0) + (((r & 8) == 8)) ? 1 : 0);
	if (rf > 0) {
		rf = (1 / rf);
	};
	Number gf = ((((((g & 1) == 1)) ? 1 : 0 + (((g & 2) == 2)) ? 1 : 0) + (((g & 4) == 4)) ? 1 : 0) + (((g & 8) == 8)) ? 1 : 0);
	if (gf > 0) {
		gf = (1 / gf);
	};
	Number bf = ((((((b & 1) == 1)) ? 1 : 0 + (((b & 2) == 2)) ? 1 : 0) + (((b & 4) == 4)) ? 1 : 0) + (((b & 8) == 8)) ? 1 : 0);
	if (bf > 0) {
		bf = (1 / bf);
	};
	Number af = ((((((a & 1) == 1)) ? 1 : 0 + (((a & 2) == 2)) ? 1 : 0) + (((a & 4) == 4)) ? 1 : 0) + (((a & 8) == 8)) ? 1 : 0);
	if (af > 0) {
		af = (1 / af);
	};
	ColorMatrix_concat(self, (const Number[20]){(((r & 1) == 1)) ? rf : 0, (((r & 2) == 2)) ? rf : 0, (((r & 4) == 4)) ? rf : 0, (((r & 8) == 8)) ? rf : 0, 0, (((g & 1) == 1)) ? gf : 0, (((g & 2) == 2)) ? gf : 0, (((g & 4) == 4)) ? gf : 0, (((g & 8) == 8)) ? gf : 0, 0, (((b & 1) == 1)) ? bf : 0, (((b & 2) == 2)) ? bf : 0, (((b & 4) == 4)) ? bf : 0, (((b & 8) == 8)) ? bf : 0, 0, (((a & 1) == 1)) ? af : 0, (((a & 2) == 2)) ? af : 0, (((a & 4) == 4)) ? af : 0, (((a & 8) == 8)) ? af : 0, 0});
}

void ColorMatrix_blend(ColorMatrix *self, const ColorMatrix *mat, Number amount)
{
	Number inv_amount = (1 - amount);
	int32_t i = 0;
	while (i < 20) {
		self->matrix[i] = ((inv_amount * self->matrix[i]) + (amount * mat->matrix[i]));
		i++;
	};
}

void ColorMatrix_average(ColorMatrix *self, Number r, Number g, Number b)
{
	ColorMatrix_concat(self, (const Number[20]){r, g, b, 0, 0,
		r, g, b, 0, 0,
		r, g, b, 0, 0,
		0, 0, 0, 1, 0});
}

void ColorMatrix_threshold(ColorMatrix *self, Number threshold, Number factor)
{
	ColorMatrix_concat(self, (const Number[20]){(ColorMatrix_LUMA_R * factor), (ColorMatrix_LUMA_G * factor), (ColorMatrix_LUMA_B * factor), 0, (-(factor - 1) * threshold),
		(ColorMatrix_LUMA_R * factor), (ColorMatrix_LUMA_G * factor), (ColorMatrix_LUMA_B * factor), 0, (-(factor - 1) * threshold),
		(ColorMatrix_LUMA_R * factor), (ColorMatrix_LUMA_G * factor), (ColorMatrix_LUMA_B * factor), 0, (-(factor - 1) * threshold),
		0, 0, 0, 1, 0});
}

void ColorMatrix_threshold_rgb(ColorMatrix *self, Number threshold, Number factor)
{
	ColorMatrix_concat(self, (const Number[20]){factor, 0, 0, 0, (-(factor - 1) * threshold),
		0, factor, 0, 0, (-(factor - 1) * threshold),
		0, 0, factor, 0, (-(factor - 1) * threshold),
		0, 0, 0, 1, 0});
}

void ColorMatrix_desaturate(ColorMatrix *self)
{
	ColorMatrix_concat(self, (const Number[20]){ColorMatrix_LUMA_R, ColorMatrix_LUMA_G, ColorMatrix_LUMA_B, 0, 0,
		ColorMatrix_LUMA_R, ColorMatrix_LUMA_G, ColorMatrix_LUMA_B, 0, 0,
		ColorMatrix_LUMA_R, ColorMatrix_LUMA_G, ColorMatrix_LUMA_B, 0, 0,
		0, 0, 0, 1, 0});
}

void ColorMatrix_randomize(ColorMatrix *self, Number amount, bool normalize)
{
	Number inv_amount = (1 - amount);
	Number r1 = (inv_amount + (amount * (Math_random() - Math_random())));
	Number g1 = (amount * (Math_random() - Math_random()));
	Number b1 = (amount * (Math_random() - Math_random()));
	Number o1 = ((amount * 0xFF) * (Math_random() - Math_random()));
	Number r2 = (amount * (Math_random() - Math_random()));
	Number g2 = (inv_amount + (amount * (Math_random() - Math_random())));
	Number b2 = (amount * (Math_random() - Math_random()));
	Number o2 = ((amount * 0xFF) * (Math_random() - Math_random()));
	Number r3 = (amount * (Math_random() - Math_random()));
	Number g3 = (amount * (Math_random() - Math_random()));
	Number b3 = (inv_amount + (amount * (Math_random() - Math_random())));
	Number o3 = ((amount * 0xFF) * (Math_random() - Math_random()));
	ColorMatrix_concat(self, (const Number[20]){r1, g1, b1, 0, o1,
		r2, g2, b2, 0, o2,
		r3, g3, b3, 0, o3,
		0, 0, 0, 1, 0});
	if (normalize) ColorMatrix_normalize(self);
}

void ColorMatrix_setMultiplicators(ColorMatrix *self, Number red, Number green, Number blue, Number alpha)
{
	const Number mat[20] = {red, 0, 0, 0, 0,
		0, green, 0, 0, 0,
		0, 0, blue, 0, 0,
		0, 0, 0, alpha, 0};
	ColorMatrix_concat(self, mat);
}

void ColorMatrix_clearChannels(ColorMatrix *self, bool red, bool green, bool blue, bool alpha)
{
	if (red) {
		self->matrix[0] = self->matrix[1] = self->matrix[2] = self->matrix[3] = self->matrix[4] = 0;
	}
	if (green) {
		self->matrix[5] = self->matrix[6] = self->matrix[7] = self->matrix[8] = self->matrix[9] = 0;
	}
	if (blue) {
		self->matrix[10] = self->matrix[11] = self->matrix[12] = self->matrix[13] = self->matrix[14] = 0;
	}
	if (alpha) {
		self->matrix[15] = self->matrix[16] = self->matrix[17] = self->matrix[18] = self->matrix[19] = 0;
	}
}

void ColorMatrix_thresholdAlpha(ColorMatrix *self, Number threshold, Number factor)
{
	ColorMatrix_concat(self, (const Number[20]){1, 0, 0, 0, 0,
		0, 1, 0, 0, 0,
		0, 0, 1, 0, 0,
		0, 0, 0, factor, (-factor * threshold)});
}

void ColorMatrix_averageRGB2Alpha(ColorMatrix *self)
{
	ColorMatrix_concat(self, (const Number[20]){0, 0, 0, 0, 255,
		0, 0, 0, 0, 255,
		0, 0, 0, 0, 255,
		ColorMatrix_ONETHIRD, ColorMatrix_ONETHIRD, ColorMatrix_ONETHIRD, 0, 0});
}

void ColorMatrix_invertAlpha(ColorMatrix *self)
{
	ColorMatrix_concat(self, (const Number[20]){1, 0, 0, 0, 0,
		0, 1, 0, 0, 0,
		0, 0, 1, 0, 0,
		0, 0, 0, -1, 255});
}

void ColorMatrix_rgb2Alpha(ColorMatrix *self, Number r, Number g, Number b)
{
	ColorMatrix_concat(self, (const Number[20]){0, 0, 0, 0, 255,
		0, 0, 0, 0, 255,
		0, 0, 0, 0, 255,
		r, g, b, 0, 0});
}

void ColorMatrix_setAlpha(ColorMatrix *self, Number alpha)
{
	ColorMatrix_concat(self, (const Number[20]){1, 0, 0, 0, 0,
		0, 1, 0, 0, 0,
		0, 0, 1, 0, 0,
		0, 0, 0, alpha, 0});
}

const Number *ColorMatrix_get_filter(ColorMatrix *self)
{
	return self->matrix; /* new ColorMatrixFilter(matrix) */
}

void ColorMatrix_rotateRed(ColorMatrix *self, Number degrees)
{
	rotateColor(self, degrees, 2, 1);
}

void ColorMatrix_rotateGreen(ColorMatrix *self, Number degrees)
{
	rotateColor(self, degrees, 0, 2);
}

void ColorMatrix_rotateBlue(ColorMatrix *self, Number degrees)
{
	rotateColor(self, degrees, 1, 0);
}

void ColorMatrix_normalize(ColorMatrix *self)
{
	for (int32_t i = 0; i < 4; i++) {
		Number sum = 0;
		int32_t j;
		for (j = 0; j < 4; j++) {
			sum += self->matrix[i * 5 + j] * self->matrix[i * 5 + j];
		}
		sum = 1 / sqrt(sum);
		if (sum != 1) {
			for (j = 0; j < 4; j++) {
				self->matrix[i * 5 + j] *= sum;
			}
		}
	}
}

void ColorMatrix_fitRange(ColorMatrix *self)
{
	for (int32_t i = 0; i < 4; i++) {
		Number minFactor = 0;
		Number maxFactor = 0;
		int32_t j;
		for (j = 0; j < 4; j++) {
			if (self->matrix[i * 5 + j] < 0) minFactor += self->matrix[i * 5 + j];
			else maxFactor += self->matrix[i * 5 + j];
		}
		Number range = maxFactor * 255 - minFactor * 255;
		Number rangeCorrection = 255 / range;
		if (rangeCorrection != 1) {
			for (j = 0; j < 4; j++) {
				self->matrix[i * 5 + j] *= rangeCorrection;
			}
		}
		minFactor = 0;
		maxFactor = 0;
		for (j = 0; j < 4; j++) {
			if (self->matrix[i * 5 + j] < 0) minFactor += self->matrix[i * 5 + j];
			else maxFactor += self->matrix[i * 5 + j];
		}
		Number worstMin = minFactor * 255;
		Number worstMax = maxFactor * 255;
		self->matrix[i * 5 + 4] = -(worstMin + (worstMax - worstMin) * 0.5 - 127.5);
	}
}

void ColorMatrix_shearRed(ColorMatrix *self, Number green, Number blue)
{
	shearColor(self, 0, 1, green, 2, blue);
}

void ColorMatrix_shearGreen(ColorMatrix *self, Number red, Number blue)
{
	shearColor(self, 1, 0, red, 2, blue);
}

void ColorMatrix_shearBlue(ColorMatrix *self, Number red, Number green)
{
	shearColor(self, 2, 0, red, 1, green);
}

void ColorMatrix_applyColorDeficiency(ColorMatrix *self, const char *type)
{
	if (as3_streq(type, "Protanopia")) {
		ColorMatrix_concat(self, (const Number[20]){0.567, 0.433, 0, 0, 0, 0.558, 0.442, 0, 0, 0, 0, 0.242, 0.758, 0, 0, 0, 0, 0, 1, 0});
	} else if (as3_streq(type, "Protanomaly")) {
		ColorMatrix_concat(self, (const Number[20]){0.817, 0.183, 0, 0, 0, 0.333, 0.667, 0, 0, 0, 0, 0.125, 0.875, 0, 0, 0, 0, 0, 1, 0});
	} else if (as3_streq(type, "Deuteranopia")) {
		ColorMatrix_concat(self, (const Number[20]){0.625, 0.375, 0, 0, 0, 0.7, 0.3, 0, 0, 0, 0, 0.3, 0.7, 0, 0, 0, 0, 0, 1, 0});
	} else if (as3_streq(type, "Deuteranomaly")) {
		ColorMatrix_concat(self, (const Number[20]){0.8, 0.2, 0, 0, 0, 0.258, 0.742, 0, 0, 0, 0, 0.142, 0.858, 0, 0, 0, 0, 0, 1, 0});
	} else if (as3_streq(type, "Tritanopia")) {
		ColorMatrix_concat(self, (const Number[20]){0.95, 0.05, 0, 0, 0, 0, 0.433, 0.567, 0, 0, 0, 0.475, 0.525, 0, 0, 0, 0, 0, 1, 0});
	} else if (as3_streq(type, "Tritanomaly")) {
		ColorMatrix_concat(self, (const Number[20]){0.967, 0.033, 0, 0, 0, 0, 0.733, 0.267, 0, 0, 0, 0.183, 0.817, 0, 0, 0, 0, 0, 1, 0});
	} else if (as3_streq(type, "Achromatopsia")) {
		ColorMatrix_concat(self, (const Number[20]){0.299, 0.587, 0.114, 0, 0, 0.299, 0.587, 0.114, 0, 0, 0.299, 0.587, 0.114, 0, 0, 0, 0, 0, 1, 0});
	} else if (as3_streq(type, "Achromatomaly")) {
		ColorMatrix_concat(self, (const Number[20]){0.618, 0.320, 0.062, 0, 0, 0.163, 0.775, 0.062, 0, 0, 0.163, 0.320, 0.516, 0, 0, 0, 0, 0, 1, 0});
	}
}

void ColorMatrix_RGB2YUV(ColorMatrix *self)
{
	ColorMatrix_concat(self, (const Number[20]){0.29900, 0.58700, 0.11400, 0, 0,
		-0.16874, -0.33126, 0.50000, 0, 128,
		0.50000, -0.41869, -0.08131, 0, 128,
		0, 0, 0, 1, 0});
}

void ColorMatrix_YUV2RGB(ColorMatrix *self)
{
	ColorMatrix_concat(self, (const Number[20]){1, -0.000007154783816076815, 1.4019975662231445, 0, -179.45477266423404,
		1, -0.3441331386566162, -0.7141380310058594, 0, 135.45870971679688,
		1, 1.7720025777816772, 0.00001542569043522235, 0, -226.8183044444304,
		0, 0, 0, 1, 0});
}

void ColorMatrix_RGB2YIQ(ColorMatrix *self)
{
	ColorMatrix_concat(self, (const Number[20]){0.2990, 0.5870, 0.1140, 0, 0,
		0.595716, -0.274453, -0.321263, 0, 128,
		0.211456, -0.522591, -0.311135, 0, 128,
		0, 0, 0, 1, 0});
}

void ColorMatrix_YIQ2RGB(ColorMatrix *self)
{
	/* The source's first row is "1, ,-0.0000071...": the stray comma is an undefined
	   element (Number(undefined) = NaN) and shifts the rest of the 21-element
	   literal by one; concat() reads the first 20 only. Kept exactly. */
	const Number mat[21] = {1, NAN, -0.000007154783816076815, 1.4019975662231445, 0, -179.45477266423404,
		1, -0.3441331386566162, -0.7141380310058594, 0, 135.45870971679688,
		1, 1.7720025777816772, 0.00001542569043522235, 0, -226.8183044444304,
		0, 0, 0, 1, 0};
	ColorMatrix_concat(self, mat);
}

/* BitmapData.histogram(rect): per-channel counts of the R, G, B (and A) values */
static void bitmap_histogram(const BitmapData *bd, Number histogram[4][256])
{
	memset(histogram, 0, sizeof(Number) * 4 * 256);
	for (int32_t i = 0; i < bd->width * bd->height; i++) {
		uint32_t p = bd->pixels[i];
		histogram[0][(p >> 16) & 255] += 1;
		histogram[1][(p >> 8) & 255] += 1;
		histogram[2][p & 255] += 1;
		histogram[3][p >> 24] += 1;
	}
}

void ColorMatrix_autoDesaturate(ColorMatrix *self, BitmapData *bitmapData, bool stretchLevels, bool outputToBlueOnly)
{
	Number histogram[4][256];
	bitmap_histogram(bitmapData, histogram);
	Number sum_r = 0;
	Number sum_g = 0;
	Number sum_b = 0;
	Number min = 0;
	Number max = 0;
	bool minFound = false;
	for (int32_t i = 0; i < 256; i++) {
		sum_r += histogram[0][i] * i;
		sum_g += histogram[1][i] * i;
		sum_b += histogram[2][i] * i;
		if (stretchLevels) {
			if (histogram[0][i] != 0 || histogram[1][i] != 0 || histogram[2][i] != 0) {
				max = i;
				if (!minFound) {
					min = i;
					minFound = true;
				}
			}
		}
	}
	Number total = sum_r + sum_g + sum_b;
	if (total == 0) {
		total = 3;
		sum_r = sum_g = sum_b = 3;
	}
	sum_r /= total;
	sum_g /= total;
	sum_b /= total;
	Number offset = 0;
	Number f;
	if (stretchLevels && max - min < 255) {
		f = 256 / ((max - min) + 1);
		sum_r *= f;
		sum_g *= f;
		sum_b *= f;
		offset = -min;
	}
	f = 1 / sqrt(sum_r * sum_r + sum_g * sum_g + sum_b * sum_b);
	sum_r *= f;
	sum_g *= f;
	sum_b *= f;
	if (!outputToBlueOnly)
		ColorMatrix_concat(self, (const Number[20]){sum_r, sum_g, sum_b, 0, offset,
			sum_r, sum_g, sum_b, 0, offset,
			sum_r, sum_g, sum_b, 0, offset,
			0, 0, 0, 1, 0});
	else
		ColorMatrix_concat(self, (const Number[20]){0, 0, 0, 0, 0,
			0, 0, 0, 0, 0,
			sum_r, sum_g, sum_b, 0, offset,
			0, 0, 0, 1, 0});
}

/* Matrix3D.invert() on the 4x4 coefficient block (row-major in, row-major out);
   false (and unchanged) when singular */
static bool invert4x4(Number m[16])
{
	Number a[4][8];
	for (int r = 0; r < 4; r++)
		for (int c = 0; c < 4; c++) {
			a[r][c] = m[r * 4 + c];
			a[r][4 + c] = r == c ? 1 : 0;
		}
	for (int col = 0; col < 4; col++) {
		int pivot = col;
		for (int r = col + 1; r < 4; r++)
			if (fabs(a[r][col]) > fabs(a[pivot][col]))
				pivot = r;
		if (a[pivot][col] == 0)
			return false;
		if (pivot != col)
			for (int c = 0; c < 8; c++) {
				Number t = a[col][c];
				a[col][c] = a[pivot][c];
				a[pivot][c] = t;
			}
		Number d = a[col][col];
		for (int c = 0; c < 8; c++)
			a[col][c] /= d;
		for (int r = 0; r < 4; r++)
			if (r != col) {
				Number factor = a[r][col];
				for (int c = 0; c < 8; c++)
					a[r][c] -= factor * a[col][c];
			}
	}
	for (int r = 0; r < 4; r++)
		for (int c = 0; c < 4; c++)
			m[r * 4 + c] = a[r][4 + c];
	return true;
}

bool ColorMatrix_invertMatrix(ColorMatrix *self)
{
	Number rawData[16] = {self->matrix[0], self->matrix[1], self->matrix[2], self->matrix[3],
		self->matrix[5], self->matrix[6], self->matrix[7], self->matrix[8],
		self->matrix[10], self->matrix[11], self->matrix[12], self->matrix[13],
		self->matrix[15], self->matrix[16], self->matrix[17], self->matrix[18]};
	bool check = invert4x4(rawData);
	if (!check) return false;
	self->matrix[0] = rawData[0];
	self->matrix[1] = rawData[1];
	self->matrix[2] = rawData[2];
	self->matrix[3] = rawData[3];
	Number tmp1 = -(rawData[0] * self->matrix[4] + rawData[1] * self->matrix[9] + rawData[2] * self->matrix[14] + rawData[3] * self->matrix[15]);
	self->matrix[5] = rawData[4];
	self->matrix[6] = rawData[5];
	self->matrix[7] = rawData[6];
	self->matrix[8] = rawData[7];
	Number tmp2 = -(rawData[4] * self->matrix[4] + rawData[5] * self->matrix[9] + rawData[6] * self->matrix[14] + rawData[7] * self->matrix[15]);
	self->matrix[10] = rawData[8];
	self->matrix[11] = rawData[9];
	self->matrix[12] = rawData[10];
	self->matrix[13] = rawData[11];
	Number tmp3 = -(rawData[8] * self->matrix[4] + rawData[9] * self->matrix[9] + rawData[10] * self->matrix[14] + rawData[11] * self->matrix[15]);
	self->matrix[15] = rawData[12];
	self->matrix[16] = rawData[13];
	self->matrix[17] = rawData[14];
	self->matrix[18] = rawData[15];
	Number tmp4 = -(rawData[12] * self->matrix[4] + rawData[13] * self->matrix[9] + rawData[14] * self->matrix[14] + rawData[15] * self->matrix[15]);
	self->matrix[4] = tmp1;
	self->matrix[9] = tmp2;
	self->matrix[14] = tmp3;
	self->matrix[19] = tmp4;
	return true;
}

uint32_t ColorMatrix_applyMatrix(ColorMatrix *self, uint32_t rgba)
{
	Number a = AS3_SHR(rgba, 24) & 0xff;
	Number r = AS3_SHR(rgba, 16) & 0xff;
	Number g = AS3_SHR(rgba, 8) & 0xff;
	Number b = rgba & 0xff;
	int32_t r2 = AS3_INT(0.5 + r * self->matrix[0] + g * self->matrix[1] + b * self->matrix[2] + a * self->matrix[3] + self->matrix[4]);
	int32_t g2 = AS3_INT(0.5 + r * self->matrix[5] + g * self->matrix[6] + b * self->matrix[7] + a * self->matrix[8] + self->matrix[9]);
	int32_t b2 = AS3_INT(0.5 + r * self->matrix[10] + g * self->matrix[11] + b * self->matrix[12] + a * self->matrix[13] + self->matrix[14]);
	int32_t a2 = AS3_INT(0.5 + r * self->matrix[15] + g * self->matrix[16] + b * self->matrix[17] + a * self->matrix[18] + self->matrix[19]);
	if (a2 < 0) a2 = 0;
	if (a2 > 255) a2 = 255;
	if (r2 < 0) r2 = 0;
	if (r2 > 255) r2 = 255;
	if (g2 < 0) g2 = 0;
	if (g2 > 255) g2 = 255;
	if (b2 < 0) b2 = 0;
	if (b2 > 255) b2 = 255;
	return (uint32_t)(a2 << 24 | r2 << 16 | g2 << 8 | b2);
}

void ColorMatrix_transformVector(ColorMatrix *self, Number *values, uint32_t length)
{
	if (length != 4) return;
	Number r = values[0] * self->matrix[0] + values[1] * self->matrix[1] + values[2] * self->matrix[2] + values[3] * self->matrix[3] + self->matrix[4];
	Number g = values[0] * self->matrix[5] + values[1] * self->matrix[6] + values[2] * self->matrix[7] + values[3] * self->matrix[8] + self->matrix[9];
	Number b = values[0] * self->matrix[10] + values[1] * self->matrix[11] + values[2] * self->matrix[12] + values[3] * self->matrix[13] + self->matrix[14];
	Number a = values[0] * self->matrix[15] + values[1] * self->matrix[16] + values[2] * self->matrix[17] + values[3] * self->matrix[18] + self->matrix[19];
	values[0] = r;
	values[1] = g;
	values[2] = b;
	values[3] = a;
}

static void initHue(ColorMatrix *self)
{
	Number greenRotation = 39.182655;
	if (!self->hueInitialized) {
		self->hueInitialized = true;
		self->preHue = as3_alloc(sizeof *self->preHue);
		ColorMatrix_ctor(self->preHue);
		ColorMatrix_rotateRed(self->preHue, 45);
		ColorMatrix_rotateGreen(self->preHue, -greenRotation);
		Number lum[4] = {ColorMatrix_LUMA_R2, ColorMatrix_LUMA_G2, ColorMatrix_LUMA_B2, 1.0};
		ColorMatrix_transformVector(self->preHue, lum, 4);
		Number red = lum[0] / lum[2];
		Number green = lum[1] / lum[2];
		ColorMatrix_shearBlue(self->preHue, red, green);
		self->postHue = as3_alloc(sizeof *self->postHue);
		ColorMatrix_ctor(self->postHue);
		ColorMatrix_shearBlue(self->postHue, -red, -green);
		ColorMatrix_rotateGreen(self->postHue, greenRotation);
		ColorMatrix_rotateRed(self->postHue, -45.0);
	}
}

static void rotateColor(ColorMatrix *self, Number degrees, int32_t x, int32_t y)
{
	degrees *= ColorMatrix_RAD;
	Number mat[20];
	memcpy(mat, IDENTITY, sizeof mat);
	mat[x + x * 5] = mat[y + y * 5] = cos(degrees);
	mat[y + x * 5] = sin(degrees);
	mat[x + y * 5] = -sin(degrees);
	ColorMatrix_concat(self, mat);
}

static void shearColor(ColorMatrix *self, int32_t x, int32_t y1, Number d1, int32_t y2, Number d2)
{
	Number mat[20];
	memcpy(mat, IDENTITY, sizeof mat);
	mat[y1 + x * 5] = d1;
	mat[y2 + x * 5] = d2;
	ColorMatrix_concat(self, mat);
}

char *ColorMatrix_toString(ColorMatrix *self)
{
	char *s = "";
	for (int32_t i = 0; i < 20; i++) {
		char n[40];
		snprintf(n, sizeof n, "%.15g", self->matrix[i]);
		s = i ? AS3_CAT(s, ",", n) : AS3_STR(n);
	}
	return s;
}
