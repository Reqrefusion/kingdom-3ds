/* Utils.as
 * shrinkHitbox, findRectanglesWithColor, RGBtoHSV, interpolateColorAndAlpha and
 * getRGB are never called by the game; translated for function parity. */
#include "game.h"

static const int32_t Utils_ROMAN_VALUES[] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
static const char *const Utils_ROMAN_LETTERS[] = {"M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV", "I"};

void Utils_replaceColor(BitmapData *bitmap, uint32_t fromColor, uint32_t toColor)
{
	for (int32_t i = 0; i < bitmap->height; i++) {
		for (int32_t j = 0; j < bitmap->width; j++) {
			if (BitmapData_getPixel32(bitmap, j, i) == fromColor) {
				BitmapData_setPixel32(bitmap, j, i, toColor);
			}
		}
	}
}

void Utils_gradientOverlay(BitmapData *bitmap, const uint32_t *colors, uint32_t colorsLength, Number rotation, int32_t chunks)
{
	Matrix matrix = Matrix_new();
	Matrix_createGradientBox(&matrix, (Number)bitmap->width / chunks, (Number)bitmap->height / chunks, rotation * Utils_DEG_TO_RAD, 0, 0);
	Shape *s = Shape_new();
	Number ratios[SHAPE_MAX_STOPS];
	Number alphas[SHAPE_MAX_STOPS];
	for (int32_t i = 0; (uint32_t)i < colorsLength; i++) {
		alphas[i] = (AS3_SHR(colors[i], 24)) / 255.0;
		ratios[i] = (i * (1.0 / (colorsLength - 1))) * 255;
	}
	Graphics_beginGradientFill(&s->graphics, "linear", colors, alphas, ratios, colorsLength, &matrix, "pad", "rgb");
	Graphics_drawRect(&s->graphics, 0, 0, (Number)bitmap->width / chunks, (Number)bitmap->height / chunks);
	if (chunks == 1) {
		BitmapData_drawShape(bitmap, s, NULL);
	} else {
		Matrix transform = Matrix_new();
		BitmapData *tempBitmap = BitmapData_new(AS3_INT((Number)bitmap->width / chunks), AS3_INT((Number)bitmap->height / chunks), true, 0x000000);
		BitmapData_drawShape(tempBitmap, s, NULL);
		Matrix_scale(&transform, (Number)bitmap->width / tempBitmap->width, (Number)bitmap->height / tempBitmap->height);
		BitmapData_draw(bitmap, tempBitmap, &transform, NULL, NULL, NULL, false);
		BitmapData_dispose(tempBitmap);
	}
	free(s);
}

uint32_t Utils_HSVtoRGB(Number h, Number s, Number v)
{
	uint32_t result = 0;
	if (s == 0.0) {
		result = Utils_getColor32(255, AS3_UINT(v * 255), AS3_UINT(v * 255), AS3_UINT(v * 255));
	} else {
		h = h / 60.0;
		Number f = h - AS3_INT(h);
		Number p = v * (1.0 - s);
		Number q = v * (1.0 - s * f);
		Number t = v * (1.0 - s * (1.0 - f));
		switch (AS3_INT(h)) {
		case 0:
			result = Utils_getColor32(255, AS3_UINT(v * 255), AS3_UINT(t * 255), AS3_UINT(p * 255));
			break;
		case 1:
			result = Utils_getColor32(255, AS3_UINT(q * 255), AS3_UINT(v * 255), AS3_UINT(p * 255));
			break;
		case 2:
			result = Utils_getColor32(255, AS3_UINT(p * 255), AS3_UINT(v * 255), AS3_UINT(t * 255));
			break;
		case 3:
			result = Utils_getColor32(255, AS3_UINT(p * 255), AS3_UINT(q * 255), AS3_UINT(v * 255));
			break;
		case 4:
			result = Utils_getColor32(255, AS3_UINT(t * 255), AS3_UINT(p * 255), AS3_UINT(v * 255));
			break;
		case 5:
			result = Utils_getColor32(255, AS3_UINT(v * 255), AS3_UINT(p * 255), AS3_UINT(q * 255));
			break;
		default:
			FlxG_log("FlxColor Error: HSVtoRGB : Unknown color");
		}
	}
	return result;
}

uint32_t Utils_interpolateColor(uint32_t color1, uint32_t color2, Number f)
{
	uint32_t a1 = AS3_SHR(color1, 24);
	uint32_t r1 = AS3_SAR(color1, 16) & 0xFF;
	uint32_t g1 = AS3_SAR(color1, 8) & 0xFF;
	uint32_t b1 = color1 & 0xFF;

	uint32_t a2 = AS3_SHR(color2, 24);
	uint32_t r2 = AS3_SAR(color2, 16) & 0xFF;
	uint32_t g2 = AS3_SAR(color2, 8) & 0xFF;
	uint32_t b2 = color2 & 0xFF;

	Number fi = (1 - f);
	a1 = AS3_UINT((fi * a1) + (f * a2));
	r1 = AS3_UINT((fi * r1) + (f * r2));
	g1 = AS3_UINT((fi * g1) + (f * g2));
	b1 = AS3_UINT((fi * b1) + (f * b2));
	return a1 << 24 | r1 << 16 | g1 << 8 | b1;
}

uint32_t Utils_getColor32(uint32_t alpha, uint32_t red, uint32_t green, uint32_t blue)
{
	return alpha << 24 | red << 16 | green << 8 | blue;
}

void Utils_explode(FlxSprite *object, FlxGroup *group, Number portion, int32_t gibsize, bool rounded)
{
	/* returns Vector.<FlxParticle>: no caller uses the result */
	FlxParticle *gib;
	FlxSprite_gpuFramePixels(object); /* GPU adapter */
	for (int32_t x = 0; x < object->framePixels->width; x += gibsize) {
		for (int32_t y = 0; y < object->framePixels->height; y += gibsize) {
			if ((AS3_SHR(BitmapData_getPixel32(object->framePixels, x + gibsize / 2, y + gibsize / 2), 24)) > 0) {
				if (FlxG_random() < portion) {
					gib = (FlxParticle *)FlxGroup_recycle(group, &FlxParticle_class);
					if (gib->frameWidth != (uint32_t)gibsize || gib->frameHeight != (uint32_t)gibsize) {
						FlxSprite_makeGraphic((FlxSprite *)gib, (uint32_t)gibsize, (uint32_t)gibsize, 0, true, NULL);
					}
					CALL0(gib, revive);
					FlxSprite_stamp((FlxSprite *)gib, object, -x, -y);
					if (rounded) {
						FlxSprite_gpuFramePixels((FlxSprite *)gib); /* GPU adapter */
						BitmapData_setPixel32(gib->framePixels, 0, 0, 0);
						BitmapData_setPixel32(gib->framePixels, 0, gibsize - 1, 0);
						BitmapData_setPixel32(gib->framePixels, gibsize - 1, 0, 0);
						BitmapData_setPixel32(gib->framePixels, gibsize - 1, gibsize - 1, 0);
					}
					/* gibs.push(gib) */
					gib->elasticity = 0.5;
					gib->lifespan = 7;
					gib->x = object->x - object->offset.x + x;
					gib->y = object->y - object->offset.y + y;
					gib->acceleration.y = 900;
					gib->velocity.x = FlxG_random() * 80 - 40;
					gib->velocity.y = -130 - FlxG_random() * 30;
				}
			}
		}
	}
}

const char *Utils_toRoman(int32_t n)
{
	static char s[64];
	s[0] = 0;
	for (int32_t i = 0; i < 13; i++) {
		int32_t c = (int32_t)floor((Number)n / Utils_ROMAN_VALUES[i]);
		n -= c * Utils_ROMAN_VALUES[i];
		while (c > 0) {
			if (strlen(s) + 3 < sizeof s)
				strcat(s, Utils_ROMAN_LETTERS[i]);
			c--;
		}
	}
	return s;
}

void Utils_shrinkHitbox(FlxSprite *sprite, Number minXCover, Number minYCover)
{
	BitmapData *pix = FlxSprite_get_pixels(sprite);
	int32_t x, y, sum = 0;
	int32_t minx = pix->width;
	int32_t maxx = 0;
	int32_t miny = pix->height;
	int32_t maxy = 0;
	for (x = 0; x < pix->width; x++) {
		sum = 0;
		for (y = 0; y < pix->height; y++) {
			if (BitmapData_getPixel32(pix, x, y) > 0x00FFFFFF)
				sum++;
		}
		if (sum >= minXCover * pix->height) {
			minx = AS3_INT(Math_min(minx, x));
			maxx = AS3_INT(Math_max(maxx, x));
		}
	}
	for (y = 0; y < pix->height; y++) {
		sum = 0;
		for (x = 0; x < pix->width; x++) {
			if (BitmapData_getPixel32(pix, x, y) > 0x00FFFFFF)
				sum++;
		}
		if (sum >= minYCover * pix->width) {
			miny = AS3_INT(Math_min(miny, y));
			maxy = AS3_INT(Math_max(maxy, y));
		}
	}
	sprite->width = maxx - minx;
	sprite->height = maxy - miny;
	sprite->offset.x = minx;
	sprite->offset.y = miny;
	sprite->x += minx / 2;
	sprite->y += miny / 2;
}

As3Array *Utils_findRectanglesWithColor(BitmapData *bitmap, uint32_t color)
{
	Rectangle *cur = NULL;
	As3Array *rects = as3_alloc(sizeof *rects);
	int32_t i, j, k;
	for (i = 0; i < bitmap->height; i++) {
		for (j = 0; j < bitmap->width; j++) {
			for (k = 0; (uint32_t)k < rects->length; k++) {
				cur = As3Array_get(rects, (uint32_t)k);
				/* If we are in a rect, skip to the right side. */
				if (Rectangle_contains(cur, j, i)) {
					j = AS3_INT(Rectangle_get_right(cur));
					/* If we are outside of the image, next line. */
					if (j >= bitmap->width) {
						j = 0;
						i++;
					}
				}
			}
			if (BitmapData_getPixel32(bitmap, j, i) == color) {
				cur = as3_alloc(sizeof *cur);
				*cur = Rectangle_new(j, i, 1, 1);
				As3Array_push(rects, cur);
				/* Traverse to right */
				while (BitmapData_getPixel32(bitmap, j, i) == color) {
					j++;
				}
				cur->width = j - cur->x;
				/* Traverse down */
				while (BitmapData_getPixel32(bitmap, j - 1, i) == color) {
					i++;
				}
				cur->height = i - cur->y;
				/* Reset I and J */
				j = AS3_INT(Rectangle_get_right(cur));
				i = AS3_INT(cur->y);
			}
		}
	}
	return rects;
}

UtilsHSV Utils_RGBtoHSV(uint32_t color)
{
	UtilsRGB rgb = Utils_getRGB(color);

	Number red = rgb.red / 255.0;
	Number green = rgb.green / 255.0;
	Number blue = rgb.blue / 255.0;

	Number min = Math_min(Math_min(red, green), blue);
	Number max = Math_max(Math_max(red, green), blue);
	Number delta = max - min;
	Number lightness = (max + min) / 2;
	Number hue = NAN;
	Number saturation = NAN;

	/* Grey color, no chroma */
	if (delta == 0) {
		hue = 0;
		saturation = 0;
	} else {
		if (lightness < 0.5) {
			saturation = delta / (max + min);
		} else {
			saturation = delta / (2 - max - min);
		}

		Number delta_r = (((max - red) / 6) + (delta / 2)) / delta;
		Number delta_g = (((max - green) / 6) + (delta / 2)) / delta;
		Number delta_b = (((max - blue) / 6) + (delta / 2)) / delta;

		if (red == max) {
			hue = delta_b - delta_g;
		} else if (green == max) {
			hue = (1.0 / 3) + delta_r - delta_b;
		} else if (blue == max) {
			hue = (2.0 / 3) + delta_g - delta_r;
		}

		if (hue < 0) {
			hue += 1;
		}

		if (hue > 1) {
			hue -= 1;
		}
	}

	/* Keep the value with 0 to 359 */
	hue *= 360;
	hue = Math_round(hue);

	return (UtilsHSV){hue, saturation, lightness, lightness};
}

uint32_t Utils_interpolateColorAndAlpha(uint32_t color1, uint32_t color2, uint32_t steps, uint32_t currentStep)
{
	UtilsRGB src1 = Utils_getRGB(color1);
	UtilsRGB src2 = Utils_getRGB(color2);

	uint32_t a = AS3_UINT((((Number)src2.alpha - src1.alpha) * currentStep) / steps + src1.alpha);
	uint32_t r = AS3_UINT((((Number)src2.red - src1.red) * currentStep) / steps + src1.red);
	uint32_t g = AS3_UINT((((Number)src2.green - src1.green) * currentStep) / steps + src1.green);
	uint32_t b = AS3_UINT((((Number)src2.blue - src1.blue) * currentStep) / steps + src1.blue);

	return Utils_getColor32(a, r, g, b);
}

UtilsRGB Utils_getRGB(uint32_t color)
{
	uint32_t alpha = AS3_SHR(color, 24);
	uint32_t red = AS3_SAR(color, 16) & 0xFF;
	uint32_t green = AS3_SAR(color, 8) & 0xFF;
	uint32_t blue = color & 0xFF;

	return (UtilsRGB){alpha, red, green, blue};
}

/* ---- Math.random ----------------------------------------------------------- */

static uint64_t math_random_state = 0x853c49e6748fea9bULL;

void Math_random_seed(uint32_t seed)
{
	math_random_state = 0x853c49e6748fea9bULL ^ seed;
}

Number Math_random(void)
{
	/* xorshift64*: uniform in [0, 1) */
	math_random_state ^= math_random_state >> 12;
	math_random_state ^= math_random_state << 25;
	math_random_state ^= math_random_state >> 27;
	uint64_t r = math_random_state * 2685821657736338717ULL;
	return (Number)(r >> 11) / 9007199254740992.0;
}
