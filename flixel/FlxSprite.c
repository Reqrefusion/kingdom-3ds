/* org/flixel/FlxSprite.as
 * Not translated: drawLine (needs flash.display.Graphics; unused by
 * Kingdom), pixelsOverlapPoint (mouse picking; unused). */
#include "flixel.h"
#include "../flash/gpu.h"

const char *FlxSprite_ImgDefault = "org/flixel/data/default.png";

const As3Class FlxSprite_class = {
	"FlxSprite", &FlxObject_class, NULL,
	{
		.destroy = (void (*)(FlxBasic *))FlxSprite_destroy,
		.preUpdate = (void (*)(FlxBasic *))FlxObject_preUpdate,
		.update = FlxBasic_update,
		.postUpdate = (void (*)(FlxBasic *))FlxSprite_postUpdate,
		.draw = (void (*)(FlxBasic *))FlxSprite_draw,
		.kill = FlxBasic_kill,
		.revive = FlxBasic_revive,
		.reset = (void (*)(FlxBasic *, Number, Number))FlxObject_reset,
		.onScreen = (bool (*)(FlxBasic *, FlxCamera *))FlxSprite_onScreen,
		.calcFrame = (void (*)(FlxBasic *))FlxSprite_calcFrame,
		.hurt = (void (*)(FlxBasic *, Number))FlxObject_hurt,
	},
};

FlxSprite *FlxSprite_new(Number X, Number Y, const char *SimpleGraphic)
{
	FlxSprite *self = as3_alloc(sizeof *self);
	self->klass = &FlxSprite_class;
	FlxSprite_ctor(self, X, Y, SimpleGraphic);
	return self;
}

void FlxSprite_ctor(FlxSprite *self, Number X, Number Y, const char *SimpleGraphic)
{
	SUPER(FlxObject_ctor((FlxObject *)self, X, Y, 0, 0));

	self->_flashPoint = Point_new(0, 0);
	self->_flashRect = Rectangle_new(0, 0, 0, 0);
	self->_flashRect2 = Rectangle_new(0, 0, 0, 0);
	self->_flashPointZero = Point_new(0, 0);
	self->offset = FlxPoint_new(0, 0);
	self->origin = FlxPoint_new(0, 0);

	self->scale = FlxPoint_new(1.0, 1.0);
	self->_alpha = 1;
	self->_color = 0x00ffffff;
	self->blend = NULL;
	self->antialiasing = false;
	self->cameras = NULL;

	self->finished = false;
	self->_facing = RIGHT;
	self->_animations = (As3Array){0};
	self->_flipped = 0;
	self->_curAnim = NULL;
	self->_curFrame = 0;
	self->_curIndex = 0;
	self->_frameTimer = 0;

	self->_matrix = Matrix_new();
	self->_callback = (As3AnimCallback)AS3_NULL_FN;

	if (SimpleGraphic == NULL)
		SimpleGraphic = FlxSprite_ImgDefault;
	FlxSprite_loadGraphic(self, SimpleGraphic, false, false, 0, 0, false);
}

void FlxSprite_destroy(FlxSprite *self)
{
	self->_curAnim = NULL;
	self->_callback = (As3AnimCallback)AS3_NULL_FN;
	self->framePixels = NULL;
}

FlxSprite *FlxSprite_loadGraphic(FlxSprite *self, const char *Graphic, bool Animated, bool Reverse, uint32_t Width, uint32_t Height, bool Unique)
{
	self->_bakedRotation = 0;
	self->_pixels = FlxG_addBitmap(Graphic, Reverse, Unique, NULL);
	if (Reverse)
		self->_flipped = (uint32_t)self->_pixels->width >> 1;
	else
		self->_flipped = 0;
	if (Width == 0) {
		if (Animated)
			Width = (uint32_t)self->_pixels->height;
		else if (self->_flipped > 0)
			Width = AS3_UINT(self->_pixels->width * 0.5);
		else
			Width = (uint32_t)self->_pixels->width;
	}
	self->width = self->frameWidth = Width;
	if (Height == 0) {
		if (Animated)
			Height = AS3_UINT(self->width);
		else
			Height = (uint32_t)self->_pixels->height;
	}
	self->height = self->frameHeight = Height;
	FlxSprite_resetHelpers(self);
	return self;
}

FlxSprite *FlxSprite_loadRotatedGraphic(FlxSprite *self, const char *Graphic, uint32_t Rotations, int32_t Frame, bool AntiAliasing, bool AutoBuffer)
{
	/* Create the brush and canvas */
	uint32_t rows = AS3_UINT(Math_sqrt(Rotations));
	BitmapData *brush = FlxG_addBitmap(Graphic, false, false, NULL);
	if (Frame >= 0) {
		/* Using just a segment of the graphic - find the right bit here */
		BitmapData *full = brush;
		brush = BitmapData_new(full->height, full->height, true, 0xFFFFFFFFu);
		uint32_t rx = (uint32_t)(Frame * brush->width);
		uint32_t ry = 0;
		uint32_t fw = (uint32_t)full->width;
		if (rx >= fw) {
			ry = AS3_UINT(rx / fw) * (uint32_t)brush->height;
			rx %= fw;
		}
		self->_flashRect.x = rx;
		self->_flashRect.y = ry;
		self->_flashRect.width = brush->width;
		self->_flashRect.height = brush->height;
		BitmapData_copyPixels(brush, full, self->_flashRect, self->_flashPointZero, false);
	}

	uint32_t max = (uint32_t)brush->width;
	if ((uint32_t)brush->height > max)
		max = (uint32_t)brush->height;
	if (AutoBuffer)
		max = AS3_UINT(max * 1.5);
	uint32_t columns = AS3_UINT(FlxU_ceil((Number)Rotations / rows));
	self->width = max * columns;
	self->height = max * rows;
	char key[256];
	snprintf(key, sizeof key, "%s:%d:%gx%g", Graphic, Frame, self->width, self->height);
	bool skipGen = FlxG_checkBitmapCache(key);
	self->_pixels = FlxG_createBitmap(AS3_UINT(self->width), AS3_UINT(self->height), 0, true, strdup(key));
	self->width = self->frameWidth = (uint32_t)self->_pixels->width;
	self->height = self->frameHeight = (uint32_t)self->_pixels->height;
	self->_bakedRotation = 360.0 / Rotations;

	/* Generate a new sheet if necessary, then fix up the width and height */
	if (!skipGen) {
		uint32_t row = 0;
		uint32_t column;
		Number bakedAngle = 0;
		uint32_t halfBrushWidth = AS3_UINT(brush->width * 0.5);
		uint32_t halfBrushHeight = AS3_UINT(brush->height * 0.5);
		uint32_t midpointX = AS3_UINT(max * 0.5);
		uint32_t midpointY = AS3_UINT(max * 0.5);
		while (row < rows) {
			column = 0;
			while (column < columns) {
				Matrix_identity(&self->_matrix);
				Matrix_translate(&self->_matrix, -(Number)halfBrushWidth, -(Number)halfBrushHeight);
				Matrix_rotate(&self->_matrix, bakedAngle * 0.017453293);
				Matrix_translate(&self->_matrix, max * column + midpointX, midpointY);
				bakedAngle += self->_bakedRotation;
				BitmapData_draw(self->_pixels, brush, &self->_matrix, NULL, NULL, NULL, AntiAliasing);
				column++;
			}
			midpointY += max;
			row++;
		}
	}
	self->frameWidth = self->frameHeight = max;
	self->width = self->height = max;
	FlxSprite_resetHelpers(self);
	if (AutoBuffer) {
		self->width = brush->width;
		self->height = brush->height;
		FlxSprite_centerOffsets(self, false);
	}
	return self;
}

FlxSprite *FlxSprite_makeGraphic(FlxSprite *self, uint32_t Width, uint32_t Height, uint32_t Color, bool Unique, const char *Key)
{
	self->_bakedRotation = 0;
	self->_pixels = FlxG_createBitmap(Width, Height, Color, Unique, Key);
	self->width = self->frameWidth = (uint32_t)self->_pixels->width;
	self->height = self->frameHeight = (uint32_t)self->_pixels->height;
	FlxSprite_resetHelpers(self);
	return self;
}

void FlxSprite_resetHelpers(FlxSprite *self)
{
	self->_flashRect.x = 0;
	self->_flashRect.y = 0;
	self->_flashRect.width = self->frameWidth;
	self->_flashRect.height = self->frameHeight;
	self->_flashRect2.x = 0;
	self->_flashRect2.y = 0;
	self->_flashRect2.width = self->_pixels->width;
	self->_flashRect2.height = self->_pixels->height;
	if ((self->framePixels == NULL) || (self->framePixels->width != self->width) || (self->framePixels->height != self->height))
		self->framePixels = BitmapData_new(AS3_INT(self->width), AS3_INT(self->height), true, 0xFFFFFFFFu);
	FlxPoint_make(&self->origin, self->frameWidth * 0.5, self->frameHeight * 0.5);
	BitmapData_copyPixels(self->framePixels, self->_pixels, self->_flashRect, self->_flashPointZero, false);
	self->frames = AS3_UINT((self->_flashRect2.width / self->_flashRect.width) * (self->_flashRect2.height / self->_flashRect.height));
	if (self->_colorTransform != NULL)
		BitmapData_colorTransform(self->framePixels, self->_flashRect, self->_colorTransform);
	self->_curIndex = 0;
	self->_gpuLazy = false; /* GPU adapter: framePixels = frame 0 */
	self->_gpuSheetX = self->_gpuSheetY = 0;
	self->_gpuFrameGen = self->framePixels->gen;
	self->_gpuSheetGen = self->_pixels->gen;
}

void FlxSprite_postUpdate(FlxSprite *self)
{
	SUPER(FlxObject_postUpdate((FlxObject *)self));
	FlxSprite_updateAnimation(self);
}

/* GPU adapter: draws the current frame straight from the sprite sheet
   (static texture, colourTransform as GPU tint) into the camera buffer or a
   GPU surface. While that works framePixels is never built ("lazy");
   FlxSprite_gpuFramePixels builds it for every other reader. */
static bool gpu_frame(FlxSprite *self, BitmapData *target, const Matrix *m, const char *blend)
{
	if (!(self->_gpuLazy || self->framePixels->gen == self->_gpuFrameGen) || self->_pixels->gen != self->_gpuSheetGen)
		return false;
	return Gpu_drawSprite(target, self->_pixels, self->framePixels, self->_gpuSheetX, self->_gpuSheetY, self->frameWidth,
		self->frameHeight, m, self->_colorTransform, blend);
}

BitmapData *FlxSprite_gpuFramePixels(FlxSprite *self)
{
	if (self->_gpuLazy) {
		self->_gpuLazy = false;
		Rectangle r = Rectangle_new(self->_gpuSheetX, self->_gpuSheetY, self->frameWidth, self->frameHeight);
		BitmapData_copyPixels(self->framePixels, self->_pixels, r, self->_flashPointZero, false);
		r.x = r.y = 0;
		if (self->_colorTransform != NULL)
			BitmapData_colorTransform(self->framePixels, r, self->_colorTransform);
		self->_gpuFrameGen = self->framePixels->gen;
	}
	return self->framePixels;
}

void FlxSprite_draw(FlxSprite *self)
{
	if (self->_flickerTimer != 0) {
		self->_flicker = !self->_flicker;
		if (self->_flicker)
			return;
	}

	if (self->dirty) /* rarely */
		CALL0(self, calcFrame);

	if (self->cameras == NULL)
		self->cameras = &FlxG.cameras;
	FlxCamera *camera;
	uint32_t i = 0;
	uint32_t l = self->cameras->length;
	while (i < l) {
		camera = As3Array_get(self->cameras, i++);
		if (!CALL(self, onScreen, camera))
			continue;
		self->_point.x = self->x - AS3_INT(camera->scroll.x * self->scrollFactor.x) - self->offset.x;
		self->_point.y = self->y - AS3_INT(camera->scroll.y * self->scrollFactor.y) - self->offset.y;
		self->_point.x += (self->_point.x > 0) ? 0.0000001 : -0.0000001;
		self->_point.y += (self->_point.y > 0) ? 0.0000001 : -0.0000001;
		Stereo_dx = Stereo_depth ? Stereo_depth((FlxObject *)self) : 0; /* 3DS stereo adapter */
		if (((self->angle == 0) || (self->_bakedRotation > 0)) && (self->scale.x == 1) && (self->scale.y == 1) && (self->blend == NULL)) {
			/* Simple render */
			self->_flashPoint.x = self->_point.x;
			self->_flashPoint.y = self->_point.y;
			Matrix gm = {1, 0, 0, 1, floor(self->_flashPoint.x), floor(self->_flashPoint.y)}; /* GPU adapter */
			if (!gpu_frame(self, camera->buffer, &gm, NULL) && FlxSprite_gpuFramePixels(self))
				BitmapData_copyPixels(camera->buffer, self->framePixels, self->_flashRect, self->_flashPoint, true);
		} else {
			/* Advanced render */
			Matrix_identity(&self->_matrix);
			Matrix_translate(&self->_matrix, -self->origin.x, -self->origin.y);
			Matrix_scale(&self->_matrix, self->scale.x, self->scale.y);
			if ((self->angle != 0) && (self->_bakedRotation <= 0))
				Matrix_rotate(&self->_matrix, self->angle * 0.017453293);
			Matrix_translate(&self->_matrix, self->_point.x + self->origin.x, self->_point.y + self->origin.y);
			if (!gpu_frame(self, camera->buffer, &self->_matrix, self->blend) && FlxSprite_gpuFramePixels(self)) /* GPU adapter */
				BitmapData_draw(camera->buffer, self->framePixels, &self->_matrix, NULL, self->blend, NULL, self->antialiasing);
		}
		Stereo_dx = 0;
		FlxBasic__VISIBLECOUNT++;
		if (FlxG.visualDebug && !self->ignoreDrawDebug)
			FlxBasic_drawDebug((FlxBasic *)self, camera);
	}
}

/* 3DS stereo adapter: depth of an object in right-eye pixels (NULL = off) */
int32_t (*Stereo_depth)(FlxObject *object);

void FlxSprite_stamp(FlxSprite *self, FlxSprite *Brush, int32_t X, int32_t Y)
{
	FlxSprite_drawFrame(Brush, false);
	BitmapData *bitmapData = Brush->framePixels;

	/* Simple draw */
	if (((Brush->angle == 0) || (Brush->_bakedRotation > 0)) && (Brush->scale.x == 1) && (Brush->scale.y == 1) && (Brush->blend == NULL)) {
		self->_flashPoint.x = X;
		self->_flashPoint.y = Y;
		self->_flashRect2.width = bitmapData->width;
		self->_flashRect2.height = bitmapData->height;
		Matrix gm = {1, 0, 0, 1, X, Y}; /* GPU adapter */
		if (!gpu_frame(Brush, self->_pixels, &gm, NULL) && FlxSprite_gpuFramePixels(Brush))
			BitmapData_copyPixels(self->_pixels, bitmapData, self->_flashRect2, self->_flashPoint, true);
		self->_flashRect2.width = self->_pixels->width;
		self->_flashRect2.height = self->_pixels->height;
		CALL0(self, calcFrame);
		return;
	}

	/* Advanced draw */
	Matrix_identity(&self->_matrix);
	Matrix_translate(&self->_matrix, -Brush->origin.x, -Brush->origin.y);
	Matrix_scale(&self->_matrix, Brush->scale.x, Brush->scale.y);
	if (Brush->angle != 0)
		Matrix_rotate(&self->_matrix, Brush->angle * 0.017453293);
	Matrix_translate(&self->_matrix, X + Brush->origin.x, Y + Brush->origin.y);
	if (!gpu_frame(Brush, self->_pixels, &self->_matrix, Brush->blend) && FlxSprite_gpuFramePixels(Brush)) /* GPU adapter */
		BitmapData_draw(self->_pixels, bitmapData, &self->_matrix, NULL, Brush->blend, NULL, Brush->antialiasing);
	CALL0(self, calcFrame);
}

void FlxSprite_fill(FlxSprite *self, uint32_t Color)
{
	BitmapData_fillRect(self->_pixels, self->_flashRect2, Color);
	if (self->_pixels != self->framePixels)
		self->dirty = true;
}

void FlxSprite_updateAnimation(FlxSprite *self)
{
	if (self->_bakedRotation > 0) {
		uint32_t oldIndex = self->_curIndex;
		int32_t angleHelper = AS3_INT(as3_mod(self->angle, 360));
		if (angleHelper < 0)
			angleHelper += 360;
		self->_curIndex = AS3_UINT(angleHelper / self->_bakedRotation + 0.5);
		if (oldIndex != self->_curIndex)
			self->dirty = true;
	} else if ((self->_curAnim != NULL) && (self->_curAnim->delay > 0) && (self->_curAnim->looped || !self->finished)) {
		self->_frameTimer += FlxG.elapsed;
		while (self->_frameTimer > self->_curAnim->delay) {
			self->_frameTimer = self->_frameTimer - self->_curAnim->delay;
			if (self->_curFrame == self->_curAnim->frames_length - 1) {
				if (self->_curAnim->looped)
					self->_curFrame = 0;
				self->finished = true;
			} else
				self->_curFrame++;
			self->_curIndex = (uint32_t)self->_curAnim->frames[self->_curFrame];
			self->dirty = true;
		}
	}

	if (self->dirty)
		CALL0(self, calcFrame);
}

void FlxSprite_drawFrame(FlxSprite *self, bool Force)
{
	if (Force || self->dirty)
		CALL0(self, calcFrame);
}

void FlxSprite_addAnimation(FlxSprite *self, const char *Name, const int32_t *Frames, uint32_t FramesLength, Number FrameRate, bool Looped)
{
	As3Array_set(&self->_animations, self->_animations.length, FlxAnim_new(Name, Frames, FramesLength, FrameRate, Looped));
}

void FlxSprite_addAnimationCallback(FlxSprite *self, As3AnimCallback AnimationCallback)
{
	self->_callback = AnimationCallback;
}

void FlxSprite_play(FlxSprite *self, const char *AnimName, bool Force)
{
	if (!Force && (self->_curAnim != NULL) && as3_streq(AnimName, self->_curAnim->name) && (!self->_curAnim->looped || !self->finished))
		return;
	self->_curFrame = 0;
	self->_curIndex = 0;
	self->_frameTimer = 0;
	uint32_t i = 0;
	uint32_t l = self->_animations.length;
	while (i < l) {
		if (as3_streq(((FlxAnim *)As3Array_get(&self->_animations, i))->name, AnimName)) {
			self->_curAnim = As3Array_get(&self->_animations, i);
			if (self->_curAnim->delay <= 0)
				self->finished = true;
			else
				self->finished = false;
			self->_curIndex = (uint32_t)self->_curAnim->frames[self->_curFrame];
			self->dirty = true;
			return;
		}
		i++;
	}
	FlxG_log("WARNING: No animation called");
}

void FlxSprite_randomFrame(FlxSprite *self)
{
	self->_curAnim = NULL;
	self->_curIndex = AS3_INT(FlxG_random() * ((Number)self->_pixels->width / self->frameWidth));
	self->dirty = true;
}

void FlxSprite_setOriginToCorner(FlxSprite *self)
{
	self->origin.x = self->origin.y = 0;
}

void FlxSprite_centerOffsets(FlxSprite *self, bool AdjustPosition)
{
	self->offset.x = (self->frameWidth - self->width) * 0.5;
	self->offset.y = (self->frameHeight - self->height) * 0.5;
	if (AdjustPosition) {
		self->x += self->offset.x;
		self->y += self->offset.y;
	}
}

As3Array *FlxSprite_replaceColor(FlxSprite *self, uint32_t Color, uint32_t NewColor, bool FetchPositions)
{
	As3Array *positions = NULL;
	if (FetchPositions)
		positions = as3_alloc(sizeof *positions);
	uint32_t row = 0;
	uint32_t column;
	uint32_t rows = (uint32_t)self->_pixels->height;
	uint32_t columns = (uint32_t)self->_pixels->width;
	while (row < rows) {
		column = 0;
		while (column < columns) {
			if (BitmapData_getPixel32(self->_pixels, (int32_t)column, (int32_t)row) == Color) {
				BitmapData_setPixel32(self->_pixels, (int32_t)column, (int32_t)row, NewColor);
				if (FetchPositions)
					As3Array_set(positions, positions->length, FlxPoint_alloc(column, row));
				self->dirty = true;
			}
			column++;
		}
		row++;
	}
	return positions;
}

BitmapData *FlxSprite_get_pixels(FlxSprite *self)
{
	return self->_pixels;
}

void FlxSprite_set_pixels(FlxSprite *self, BitmapData *Pixels)
{
	self->_pixels = Pixels;
	self->width = self->frameWidth = (uint32_t)self->_pixels->width;
	self->height = self->frameHeight = (uint32_t)self->_pixels->height;
	FlxSprite_resetHelpers(self);
}

uint32_t FlxSprite_get_facing(FlxSprite *self)
{
	return self->_facing;
}

void FlxSprite_set_facing(FlxSprite *self, uint32_t Direction)
{
	if (self->_facing != Direction)
		self->dirty = true;
	self->_facing = Direction;
}

Number FlxSprite_get_alpha(FlxSprite *self)
{
	return self->_alpha;
}

static ColorTransform *new_ColorTransform(Number r, Number g, Number b, Number a)
{
	ColorTransform *ct = as3_alloc(sizeof *ct);
	*ct = ColorTransform_new(r, g, b, a, 0, 0, 0, 0);
	return ct;
}

void FlxSprite_set_alpha(FlxSprite *self, Number Alpha)
{
	if (Alpha > 1)
		Alpha = 1;
	if (Alpha < 0)
		Alpha = 0;
	if (Alpha == self->_alpha)
		return;
	self->_alpha = Alpha;
	free(self->_colorTransform);
	if ((self->_alpha != 1) || (self->_color != 0x00ffffff))
		self->_colorTransform = new_ColorTransform((self->_color >> 16) * 0.00392, (self->_color >> 8 & 0xff) * 0.00392, (self->_color & 0xff) * 0.00392, self->_alpha);
	else
		self->_colorTransform = NULL;
	self->dirty = true;
}

uint32_t FlxSprite_get_color(FlxSprite *self)
{
	return self->_color;
}

void FlxSprite_set_color(FlxSprite *self, uint32_t Color)
{
	Color &= 0x00ffffff;
	if (self->_color == Color)
		return;
	self->_color = Color;
	free(self->_colorTransform);
	if ((self->_alpha != 1) || (self->_color != 0x00ffffff))
		self->_colorTransform = new_ColorTransform((self->_color >> 16) * 0.00392, (self->_color >> 8 & 0xff) * 0.00392, (self->_color & 0xff) * 0.00392, self->_alpha);
	else
		self->_colorTransform = NULL;
	self->dirty = true;
}

uint32_t FlxSprite_get_frame(FlxSprite *self)
{
	return self->_curIndex;
}

void FlxSprite_set_frame(FlxSprite *self, uint32_t Frame)
{
	self->_curAnim = NULL;
	self->_curIndex = Frame;
	self->dirty = true;
}

bool FlxSprite_onScreen(FlxSprite *self, FlxCamera *Camera)
{
	if (Camera == NULL)
		Camera = FlxG.camera;
	FlxObject_getScreenXY((FlxObject *)self, &self->_point, Camera);
	self->_point.x = self->_point.x - self->offset.x;
	self->_point.y = self->_point.y - self->offset.y;

	if (((self->angle == 0) || (self->_bakedRotation > 0)) && (self->scale.x == 1) && (self->scale.y == 1))
		return ((self->_point.x + self->frameWidth > 0) && (self->_point.x < Camera->width) && (self->_point.y + self->frameHeight > 0) && (self->_point.y < Camera->height));

	Number halfWidth = self->frameWidth / 2.0;
	Number halfHeight = self->frameHeight / 2.0;
	Number absScaleX = (self->scale.x > 0) ? self->scale.x : -self->scale.x;
	Number absScaleY = (self->scale.y > 0) ? self->scale.y : -self->scale.y;
	Number radius = Math_sqrt(halfWidth * halfWidth + halfHeight * halfHeight) * ((absScaleX >= absScaleY) ? absScaleX : absScaleY);
	self->_point.x += halfWidth;
	self->_point.y += halfHeight;
	return ((self->_point.x + radius > 0) && (self->_point.x - radius < Camera->width) && (self->_point.y + radius > 0) && (self->_point.y - radius < Camera->height));
}

void FlxSprite_calcFrame(FlxSprite *self)
{
	uint32_t indexX = self->_curIndex * self->frameWidth;
	uint32_t indexY = 0;

	/* Handle sprite sheets */
	uint32_t widthHelper = self->_flipped ? self->_flipped : (uint32_t)self->_pixels->width;
	if (indexX >= widthHelper) {
		indexY = AS3_UINT(indexX / widthHelper) * self->frameHeight;
		indexX %= widthHelper;
	}

	/* handle reversed sprites */
	if (self->_flipped && (self->_facing == LEFT))
		indexX = (self->_flipped << 1) - indexX - self->frameWidth;

	/* Update display bitmap */
	self->_flashRect.x = indexX;
	self->_flashRect.y = indexY;
	/* GPU adapter: a frame rectangle completely outside the sheet (Reed.update can pick frame 3
	   of the 3-frame reed.png) makes copyPixels a no-op, so framePixels keeps the previous frame.
	   Keep the previous sheet position too, instead of sampling the neighbouring texture. */
	if (!(indexX >= self->_pixels->width || indexY >= self->_pixels->height || indexX + self->frameWidth <= 0 ||
	      indexY + self->frameHeight <= 0)) {
		self->_gpuSheetX = indexX;
		self->_gpuSheetY = indexY;
	}
	self->_gpuLazy = Gpu_recording; /* GPU adapter: framePixels built on demand */
	if (!self->_gpuLazy)
		BitmapData_copyPixels(self->framePixels, self->_pixels, self->_flashRect, self->_flashPointZero, false);
	self->_flashRect.x = self->_flashRect.y = 0;
	if (self->_colorTransform != NULL && !self->_gpuLazy)
		BitmapData_colorTransform(self->framePixels, self->_flashRect, self->_colorTransform);
	if (self->_callback.fn != NULL)
		self->_callback.fn(self->_callback.self, ((self->_curAnim != NULL) ? (self->_curAnim->name) : NULL), self->_curFrame, self->_curIndex);
	self->dirty = false;
	self->_gpuFrameGen = self->framePixels->gen; /* GPU adapter */
	self->_gpuSheetGen = self->_pixels->gen;
}

/* ---- FlxSprite members the game never calls --------------------------------- */

/* ADAPTATION: the Flash drawing API (Graphics.lineStyle/lineTo into a Sprite, then
   BitmapData.draw) is replaced by a software line of the same colour, thickness
   and alpha (no anti-aliasing). */
void FlxSprite_drawLine(FlxSprite *self, Number StartX, Number StartY, Number EndX, Number EndY, uint32_t Color, uint32_t Thickness)
{
	Number alphaComponent = (Number)(AS3_SAR(Color, 24) & 0xFF) / 255;
	if (alphaComponent <= 0)
		alphaComponent = 1;
	uint32_t argb = ((uint32_t)AS3_UINT(alphaComponent * 255) << 24) | (Color & 0x00FFFFFFu);
	Number dx = EndX - StartX, dy = EndY - StartY;
	int32_t steps = AS3_INT(Math_max(fabs(dx), fabs(dy))) + 1;
	BitmapData *pixels = FlxSprite_get_pixels(self);
	for (int32_t i = 0; i <= steps; i++) {
		Number t = steps ? (Number)i / steps : 0;
		int32_t cx = AS3_INT(StartX + dx * t - (Thickness - 1) / 2.0), cy = AS3_INT(StartY + dy * t - (Thickness - 1) / 2.0);
		for (uint32_t ty = 0; ty < Thickness; ty++)
			for (uint32_t tx = 0; tx < Thickness; tx++) {
				int32_t px = cx + (int32_t)tx, py = cy + (int32_t)ty;
				if (px >= 0 && py >= 0 && px < pixels->width && py < pixels->height)
					BitmapData_setPixel32(pixels, px, py, BitmapData_blendOver(argb, BitmapData_getPixel32(pixels, px, py), pixels->transparent));
			}
	}
	self->dirty = true;
}

bool FlxSprite_pixelsOverlapPoint(FlxSprite *self, const FlxPoint *Point, uint32_t Mask, FlxCamera *Camera)
{
	if (Camera == NULL)
		Camera = FlxG.camera;
	FlxObject_getScreenXY((FlxObject *)self, &self->_point, Camera);
	self->_point.x = self->_point.x - self->offset.x;
	self->_point.y = self->_point.y - self->offset.y;
	self->_flashPoint.x = (Point->x - Camera->scroll.x) - self->_point.x;
	self->_flashPoint.y = (Point->y - Camera->scroll.y) - self->_point.y;
	return BitmapData_hitTest(self->framePixels, self->_flashPointZero, Mask, self->_flashPoint);
}
