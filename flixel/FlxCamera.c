/* org/flixel/FlxCamera.as
 * The flash.display.Sprite/Bitmap pair that shows the camera buffer is kept
 * as plain data (_flashSprite/_flashBitmap); the platform layer reads it to
 * place, scale and shake the final image. Not translated: copyFrom,
 * getContainerSprite (display list access), alpha/angle/antialiasing
 * accessors (unused by Kingdom). */
#include "flixel.h"

Number FlxCamera_defaultZoom;

const As3Class FlxCamera_class = {
	"FlxCamera", &FlxBasic_class, NULL,
	{
		.destroy = (void (*)(FlxBasic *))FlxCamera_destroy,
		.preUpdate = FlxBasic_preUpdate,
		.update = (void (*)(FlxBasic *))FlxCamera_update,
		.postUpdate = FlxBasic_postUpdate,
		.draw = FlxBasic_draw,
		.kill = FlxBasic_kill,
		.revive = FlxBasic_revive,
	},
};

FlxCamera *FlxCamera_new(int32_t X, int32_t Y, int32_t Width, int32_t Height, Number Zoom)
{
	FlxCamera *self = as3_alloc(sizeof *self);
	self->klass = &FlxCamera_class;
	FlxBasic_ctor((FlxBasic *)self);

	self->x = X;
	self->y = Y;
	self->width = (uint32_t)Width;
	self->height = (uint32_t)Height;
	self->target = NULL;
	self->deadzone = NULL;
	self->scroll = FlxPoint_new(0, 0);
	self->_point = FlxPoint_new(0, 0);
	self->bounds = NULL;
	self->screen = FlxSprite_new(0, 0, NULL);
	FlxSprite_makeGraphic(self->screen, self->width, self->height, 0, true, NULL);
	FlxSprite_setOriginToCorner(self->screen);
	self->buffer = FlxSprite_get_pixels(self->screen);
	self->bgColor = FlxG_get_bgColor();
	self->_color = 0xffffff;

	self->_flashBitmap = (FlashDisplayStub){.scaleX = 1, .scaleY = 1, .alpha = 1, .visible = true};
	self->_flashBitmap.x = -(Number)self->width * 0.5;
	self->_flashBitmap.y = -(Number)self->height * 0.5;
	self->_flashSprite = (FlashDisplayStub){.scaleX = 1, .scaleY = 1, .alpha = 1, .visible = true};
	FlxCamera_set_zoom(self, Zoom); /* sets the scale of flash sprite, which in turn loads flashoffset values */
	self->_flashOffsetX = self->width * 0.5 * FlxCamera_get_zoom(self);
	self->_flashOffsetY = self->height * 0.5 * FlxCamera_get_zoom(self);
	self->_flashSprite.x = self->x + self->_flashOffsetX;
	self->_flashSprite.y = self->y + self->_flashOffsetY;
	self->_flashRect = Rectangle_new(0, 0, self->width, self->height);
	self->_flashPoint = Point_new(0, 0);

	self->_fxFlashColor = 0;
	self->_fxFlashDuration = 0.0;
	self->_fxFlashComplete = (As3Callback0)AS3_NULL_FN;
	self->_fxFlashAlpha = 0.0;

	self->_fxFadeColor = 0;
	self->_fxFadeDuration = 0.0;
	self->_fxFadeComplete = (As3Callback0)AS3_NULL_FN;
	self->_fxFadeAlpha = 0.0;

	self->_fxShakeIntensity = 0.0;
	self->_fxShakeDuration = 0.0;
	self->_fxShakeComplete = (As3Callback0)AS3_NULL_FN;
	self->_fxShakeOffset = FlxPoint_new(0, 0);
	self->_fxShakeDirection = 0;

	self->_fill = BitmapData_new((int32_t)self->width, (int32_t)self->height, true, 0);
	return self;
}

void FlxCamera_update(FlxCamera *self)
{
	/* Either follow the object closely, or double check our deadzone and update accordingly. */
	if (self->target != NULL) {
		if (self->deadzone == NULL)
			FlxCamera_focusOn(self, FlxObject_getMidpoint(self->target, &self->_point));
		else {
			Number edge;
			Number targetX = self->target->x + ((self->target->x > 0) ? 0.0000001 : -0.0000001);
			Number targetY = self->target->y + ((self->target->y > 0) ? 0.0000001 : -0.0000001);

			edge = targetX - self->deadzone->x;
			if (self->scroll.x > edge)
				self->scroll.x = edge;
			edge = targetX + self->target->width - self->deadzone->x - self->deadzone->width;
			if (self->scroll.x < edge)
				self->scroll.x = edge;

			edge = targetY - self->deadzone->y;
			if (self->scroll.y > edge)
				self->scroll.y = edge;
			edge = targetY + self->target->height - self->deadzone->y - self->deadzone->height;
			if (self->scroll.y < edge)
				self->scroll.y = edge;
		}
	}

	/* Make sure we didn't go outside the camera's bounds */
	if (self->bounds != NULL) {
		if (self->scroll.x < FlxRect_get_left(self->bounds))
			self->scroll.x = FlxRect_get_left(self->bounds);
		if (self->scroll.x > FlxRect_get_right(self->bounds) - self->width)
			self->scroll.x = FlxRect_get_right(self->bounds) - self->width;
		if (self->scroll.y < FlxRect_get_top(self->bounds))
			self->scroll.y = FlxRect_get_top(self->bounds);
		if (self->scroll.y > FlxRect_get_bottom(self->bounds) - self->height)
			self->scroll.y = FlxRect_get_bottom(self->bounds) - self->height;
	}

	/* Update the "flash" special effect */
	if (self->_fxFlashAlpha > 0.0) {
		self->_fxFlashAlpha -= FlxG.elapsed / self->_fxFlashDuration;
		if ((self->_fxFlashAlpha <= 0) && (self->_fxFlashComplete.fn != NULL))
			self->_fxFlashComplete.fn(self->_fxFlashComplete.self);
	}

	/* Update the "fade" special effect */
	if ((self->_fxFadeAlpha > 0.0) && (self->_fxFadeAlpha < 1.0)) {
		self->_fxFadeAlpha += FlxG.elapsed / self->_fxFadeDuration;
		if (self->_fxFadeAlpha >= 1.0) {
			self->_fxFadeAlpha = 1.0;
			if (self->_fxFadeComplete.fn != NULL)
				self->_fxFadeComplete.fn(self->_fxFadeComplete.self);
		}
	}

	/* Update the "shake" special effect */
	if (self->_fxShakeDuration > 0) {
		self->_fxShakeDuration -= FlxG.elapsed;
		if (self->_fxShakeDuration <= 0) {
			FlxPoint_make(&self->_fxShakeOffset, 0, 0);
			if (self->_fxShakeComplete.fn != NULL)
				self->_fxShakeComplete.fn(self->_fxShakeComplete.self);
		} else {
			if ((self->_fxShakeDirection == FlxCamera_SHAKE_BOTH_AXES) || (self->_fxShakeDirection == FlxCamera_SHAKE_HORIZONTAL_ONLY))
				self->_fxShakeOffset.x = (FlxG_random() * self->_fxShakeIntensity * self->width * 2 - self->_fxShakeIntensity * self->width) * self->_zoom;
			if ((self->_fxShakeDirection == FlxCamera_SHAKE_BOTH_AXES) || (self->_fxShakeDirection == FlxCamera_SHAKE_VERTICAL_ONLY))
				self->_fxShakeOffset.y = (FlxG_random() * self->_fxShakeIntensity * self->height * 2 - self->_fxShakeIntensity * self->height) * self->_zoom;
		}
	}
}

void FlxCamera_follow(FlxCamera *self, FlxObject *Target, uint32_t Style)
{
	self->target = Target;
	Number helper;
	switch (Style) {
	case FlxCamera_STYLE_PLATFORMER: {
		Number w = self->width / 8.0;
		Number h = self->height / 3.0;
		self->deadzone = FlxRect_alloc((self->width - w) / 2, (self->height - h) / 2 - h * 0.25, w, h);
		break;
	}
	case FlxCamera_STYLE_TOPDOWN:
		helper = FlxU_max(self->width, self->height) / 4;
		self->deadzone = FlxRect_alloc((self->width - helper) / 2, (self->height - helper) / 2, helper, helper);
		break;
	case FlxCamera_STYLE_TOPDOWN_TIGHT:
		helper = FlxU_max(self->width, self->height) / 8;
		self->deadzone = FlxRect_alloc((self->width - helper) / 2, (self->height - helper) / 2, helper, helper);
		break;
	case FlxCamera_STYLE_LOCKON:
	default:
		self->deadzone = NULL;
		break;
	}
}

void FlxCamera_focusOn(FlxCamera *self, FlxPoint *Point)
{
	Point->x += (Point->x > 0) ? 0.0000001 : -0.0000001;
	Point->y += (Point->y > 0) ? 0.0000001 : -0.0000001;
	FlxPoint_make(&self->scroll, Point->x - self->width * 0.5, Point->y - self->height * 0.5);
}

void FlxCamera_setBounds(FlxCamera *self, Number X, Number Y, Number Width, Number Height, bool UpdateWorld)
{
	if (self->bounds == NULL)
		self->bounds = FlxRect_alloc(0, 0, 0, 0);
	FlxRect_make(self->bounds, X, Y, Width, Height);
	if (UpdateWorld)
		FlxRect_copyFrom(&FlxG.worldBounds, self->bounds);
	FlxCamera_update(self);
}

void FlxCamera_flash(FlxCamera *self, uint32_t Color, Number Duration, As3Callback0 OnComplete, bool Force)
{
	if (!Force && (self->_fxFlashAlpha > 0.0))
		return;
	self->_fxFlashColor = Color;
	if (Duration <= 0)
		Duration = 4.9406564584124654e-324; /* Number.MIN_VALUE */
	self->_fxFlashDuration = Duration;
	self->_fxFlashComplete = OnComplete;
	self->_fxFlashAlpha = 1.0;
}

void FlxCamera_fade(FlxCamera *self, uint32_t Color, Number Duration, As3Callback0 OnComplete, bool Force)
{
	if (!Force && (self->_fxFadeAlpha > 0.0))
		return;
	self->_fxFadeColor = Color;
	if (Duration <= 0)
		Duration = 4.9406564584124654e-324; /* Number.MIN_VALUE */
	self->_fxFadeDuration = Duration;
	self->_fxFadeComplete = OnComplete;
	self->_fxFadeAlpha = 4.9406564584124654e-324; /* Number.MIN_VALUE */
}

void FlxCamera_shake(FlxCamera *self, Number Intensity, Number Duration, As3Callback0 OnComplete, bool Force, uint32_t Direction)
{
	if (!Force && ((self->_fxShakeOffset.x != 0) || (self->_fxShakeOffset.y != 0)))
		return;
	self->_fxShakeIntensity = Intensity;
	self->_fxShakeDuration = Duration;
	self->_fxShakeComplete = OnComplete;
	self->_fxShakeDirection = Direction;
	FlxPoint_make(&self->_fxShakeOffset, 0, 0);
}

void FlxCamera_stopFX(FlxCamera *self)
{
	self->_fxFlashAlpha = 0.0;
	self->_fxFadeAlpha = 0.0;
	self->_fxShakeDuration = 0;
	self->_flashSprite.x = self->x + self->width * 0.5;
	self->_flashSprite.y = self->y + self->height * 0.5;
}

Number FlxCamera_get_zoom(FlxCamera *self)
{
	return self->_zoom;
}

void FlxCamera_set_zoom(FlxCamera *self, Number Zoom)
{
	if (Zoom == 0)
		self->_zoom = FlxCamera_defaultZoom;
	else
		self->_zoom = Zoom;
	FlxCamera_setScale(self, self->_zoom, self->_zoom);
}

void FlxCamera_setScale(FlxCamera *self, Number X, Number Y)
{
	self->_flashSprite.scaleX = X;
	self->_flashSprite.scaleY = Y;
}

void FlxCamera_fill(FlxCamera *self, uint32_t Color, bool BlendAlpha)
{
	BitmapData_fillRect(self->_fill, self->_flashRect, Color);
	BitmapData_copyPixels(self->buffer, self->_fill, self->_flashRect, self->_flashPoint, BlendAlpha);
}

void FlxCamera_drawFX(FlxCamera *self)
{
	Number alphaComponent;

	/* Draw the "flash" special effect onto the buffer */
	if (self->_fxFlashAlpha > 0.0) {
		alphaComponent = AS3_SAR(self->_fxFlashColor, 24);
		FlxCamera_fill(self, (AS3_UINT(((alphaComponent <= 0) ? 0xff : alphaComponent) * self->_fxFlashAlpha) << 24) + (self->_fxFlashColor & 0x00ffffff), true);
	}

	/* Draw the "fade" special effect onto the buffer */
	if (self->_fxFadeAlpha > 0.0) {
		alphaComponent = AS3_SAR(self->_fxFadeColor, 24);
		FlxCamera_fill(self, (AS3_UINT(((alphaComponent <= 0) ? 0xff : alphaComponent) * self->_fxFadeAlpha) << 24) + (self->_fxFadeColor & 0x00ffffff), true);
	}

	if ((self->_fxShakeOffset.x != 0) || (self->_fxShakeOffset.y != 0)) {
		self->_flashSprite.x = self->x + self->_flashOffsetX + self->_fxShakeOffset.x;
		self->_flashSprite.y = self->y + self->_flashOffsetY + self->_fxShakeOffset.y;
	}
}

/* ---- members the game never calls ------------------------------------------- */

void FlxCamera_destroy(FlxCamera *self)
{
	CALL0(self->screen, destroy);
	self->screen = NULL;
	self->target = NULL;
	/* scroll, _flashRect, _flashPoint, _fxShakeOffset are value members here: nothing to release */
	self->deadzone = NULL;
	self->bounds = NULL;
	self->buffer = NULL;
	self->_fxFlashComplete = (As3Callback0)AS3_NULL_FN;
	self->_fxFadeComplete = (As3Callback0)AS3_NULL_FN;
	self->_fxShakeComplete = (As3Callback0)AS3_NULL_FN;
	self->_fill = NULL;
}

FlxCamera *FlxCamera_copyFrom(FlxCamera *self, const FlxCamera *Camera)
{
	if (Camera->bounds == NULL)
		self->bounds = NULL;
	else {
		if (self->bounds == NULL)
			self->bounds = FlxRect_alloc(0, 0, 0, 0);
		FlxRect_copyFrom(self->bounds, Camera->bounds);
	}
	self->target = Camera->target;
	if (self->target != NULL) {
		if (Camera->deadzone == NULL)
			self->deadzone = NULL;
		else {
			if (self->deadzone == NULL)
				self->deadzone = FlxRect_alloc(0, 0, 0, 0);
			FlxRect_copyFrom(self->deadzone, Camera->deadzone);
		}
	}
	return self;
}

Number FlxCamera_get_alpha(FlxCamera *self)
{
	return self->_flashBitmap.alpha;
}

void FlxCamera_set_alpha(FlxCamera *self, Number Alpha)
{
	self->_flashBitmap.alpha = Alpha;
}

Number FlxCamera_get_angle(FlxCamera *self)
{
	return self->_flashSprite.rotation;
}

void FlxCamera_set_angle(FlxCamera *self, Number Angle)
{
	self->_flashSprite.rotation = Angle;
}

uint32_t FlxCamera_get_color(FlxCamera *self)
{
	return self->_color;
}

void FlxCamera_set_color(FlxCamera *self, uint32_t Color)
{
	self->_color = Color;
	ColorTransform colorTransform = self->_flashBitmap.colorTransform;
	colorTransform.redMultiplier = (AS3_SHR(self->_color, 16)) * 0.00392;
	colorTransform.greenMultiplier = (AS3_SAR(self->_color, 8) & 0xff) * 0.00392;
	colorTransform.blueMultiplier = (self->_color & 0xff) * 0.00392;
	self->_flashBitmap.colorTransform = colorTransform;
}

bool FlxCamera_get_antialiasing(FlxCamera *self)
{
	return self->_flashBitmap.smoothing;
}

void FlxCamera_set_antialiasing(FlxCamera *self, bool Antialiasing)
{
	self->_flashBitmap.smoothing = Antialiasing;
}

FlxPoint *FlxCamera_getScale(FlxCamera *self)
{
	return FlxPoint_make(&self->_point, self->_flashSprite.scaleX, self->_flashSprite.scaleY);
}

FlashDisplayStub *FlxCamera_getContainerSprite(FlxCamera *self)
{
	return &self->_flashSprite;
}
