/* Water.as */
#include "game.h"

const As3Class Water_class = {
	"Water", &FlxSprite_class, NULL,
	{
		FLXSPRITE_VT,
		.update = (void (*)(FlxBasic *))Water_update,
		.draw = (void (*)(FlxBasic *))Water_draw,
	},
};

Water *Water_new(int32_t x, int32_t y, int32_t width, int32_t height, FlxGroup *lights, Weather *weather, uint32_t baseColor, Number reflectivity)
{
	Water *self = as3_alloc(sizeof *self);
	self->klass = &Water_class;
	self->rect = Rectangle_new(0, 0, 480, 160);
	self->zeroPoint = Point_new(0, 0);
	self->perlinOffset = Point_new(0, 0);
	self->matrix = Matrix_new();
	self->timer = 0;
	self->weatherChanged = -1;
	SUPER(FlxSprite_ctor((FlxSprite *)self, 0, 0, NULL)); /* implicit super() */

	self->x = x;
	self->y = y;
	self->moves = false;
	self->scrollFactor.x = 0;

	self->transform = ColorTransform_new(1, 1, 1, reflectivity, 0, 0, 0, 0);
	self->rect = Rectangle_new(0, 0, width, height);
	self->displacementBitmap = BitmapData_new(width, height, false, 0);
	FlxSprite_makeGraphic((FlxSprite *)self, (uint32_t)width, (uint32_t)height, baseColor, false, NULL);
	self->lights = lights;
	self->weather = weather;
	self->baseColor = baseColor;
	self->displacementFilter = DisplacementMapFilter_new(self->displacementBitmap, self->zeroPoint, 1, 2, 256, 256, DisplacementMapFilterMode_COLOR, baseColor, 0.5);
	return self;
}

void Water_update(Water *self)
{
	self->timer += FlxG.elapsed;
	if (self->weather->changed > self->weatherChanged) {
		self->currentBase = 0xFF000000 | Utils_interpolateColor(self->baseColor, self->weather->darknessColor, self->weather->darkness);
		int32_t rippleScale = AS3_INT(self->weather->wind * Water_WIND_RIPPLE_MULTIPLIER);
		int32_t xscale = AS3_INT(rippleScale / 2.0);
		int32_t yscale = rippleScale;
		self->noiseRange = ColorTransform_new(xscale / 128.0, yscale / 128.0, 1, 1, (128 - xscale + (Water_NOISE_BIAS * xscale / 128.0)), (128 - yscale + (Water_NOISE_BIAS * yscale / 128.0)), 1, 1);
		self->weatherChanged = self->weather->t;
	}
}

void Water_draw(Water *self)
{
	if (self->timer > 0.1) { /* Update the water ripple */
		self->perlinOffset.y += 1.0 / 5;
		self->perlinOffset.x = FlxG.camera->scroll.x * 1.5;
		BitmapData_perlinNoise(self->displacementBitmap, 32, 4, 1, 12312, false, false, 1 | 2, true, &self->perlinOffset, 1);
		BitmapData_colorTransform(self->displacementBitmap, self->rect, &self->noiseRange);
		self->displacementFilter->color = self->currentBase;
		self->timer = 0;
	}
	BitmapData *px = FlxSprite_get_pixels((FlxSprite *)self);
	Matrix_identity(&self->matrix);
	Matrix_scale(&self->matrix, 1, -1);
	FlxObject_getScreenXY((FlxObject *)self, &self->_point, NULL);
	Matrix_translate(&self->matrix, -self->_point.x, self->_point.y);
	BitmapData_fillRect(px, self->rect, self->currentBase);
	Utils_gradientOverlay(px, (const uint32_t[]){0x00000000, 0x66000000}, 2, 90, 4);
	BitmapData_draw(px, FlxG.camera->buffer, &self->matrix, &self->transform, NULL, NULL, false);
	Light *l;
	for (int32_t i = 0; i < self->lights->length; i++) {
		l = As3Array_get(&self->lights->members, (uint32_t)i);
		FlxObject_getScreenXY((FlxObject *)l, &self->_point, NULL);
		if (l->visible && -64 < self->_point.x && self->_point.x < FlxG.width + 64) {
			FlxSprite_set_alpha(l->reflected, self->weather->darkness * 0.8);
			FlxSprite_set_alpha(l->reflected, FlxSprite_get_alpha(l->reflected) * Math_min(1.0, (self->weather->wind * 10)));
			FlxSprite_drawFrame(l->reflected, false);
			FlxSprite_stamp((FlxSprite *)self, l->reflected, AS3_INT(self->_point.x - l->reflected->width / 2 + 4), AS3_INT((self->y - l->y) * 0.3));
		}
	}
	BitmapData_applyDisplacementFilter(px, px, self->rect, self->zeroPoint, self->displacementFilter);
	self->dirty = true;
	SUPER(FlxSprite_draw((FlxSprite *)self));
}
