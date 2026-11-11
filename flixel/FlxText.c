/* org/flixel/FlxText.as */
#include "FlxText.h"
#include "../flash/gpu.h"
#include <stdlib.h>
#include <string.h>

const As3Class FlxText_class = {
	"FlxText", &FlxSprite_class, NULL,
	{
		FLXSPRITE_VT,
		.destroy = (void (*)(FlxBasic *))FlxText_destroy,
		.calcFrame = (void (*)(FlxBasic *))FlxText_calcFrame,
	},
};

FlxText *FlxText_new(Number X, Number Y, uint32_t Width, const char *Text, bool EmbeddedFont)
{
	FlxText *self = as3_alloc(sizeof *self);
	self->klass = &FlxText_class;
	SUPER(FlxSprite_ctor((FlxSprite *)self, X, Y, NULL));
	FlxSprite_makeGraphic((FlxSprite *)self, Width, 1, 0, false, NULL);

	if (Text == NULL)
		Text = "";
	self->_textField = TextField_new();
	self->_textField->width = Width;
	self->_textField->embedFonts = EmbeddedFont;
	self->_textField->selectable = false;
	self->_textField->sharpness = 100;
	self->_textField->multiline = true;
	self->_textField->wordWrap = true;
	TextField_set_text(self->_textField, Text);
	TextFormat format = TextFormat_new("system", 8, 0xffffff, NULL);
	self->_textField->defaultTextFormat = format;
	TextField_setTextFormat(self->_textField, format);
	if (strlen(Text) <= 0)
		self->_textField->height = 1;
	else
		self->_textField->height = 10;

	self->_regen = true;
	self->_shadow = 0;
	self->allowCollisions = NONE;
	CALL0(self, calcFrame);
	return self;
}

void FlxText_destroy(FlxText *self)
{
	self->_textField = NULL;
	SUPER(FlxSprite_destroy((FlxSprite *)self));
}

FlxText *FlxText_setFormat(FlxText *self, const char *Font, Number Size, uint32_t Color, const char *Alignment, uint32_t ShadowColor)
{
	if (Font == NULL)
		Font = "";
	TextFormat format = FlxText_dtfCopy(self);
	format.font = Font;
	format.size = Size;
	format.color = Color;
	format.align = Alignment;
	self->_textField->defaultTextFormat = format;
	TextField_setTextFormat(self->_textField, format);
	self->_shadow = ShadowColor;
	self->_regen = true;
	CALL0(self, calcFrame);
	return self;
}

const char *FlxText_get_text(FlxText *self)
{
	return self->_textField->text;
}

void FlxText_set_text(FlxText *self, const char *Text)
{
	char *ot = strdup(self->_textField->text);
	TextField_set_text(self->_textField, Text);
	if (!as3_streq(self->_textField->text, ot)) {
		self->_regen = true;
		CALL0(self, calcFrame);
	}
	free(ot);
}

Number FlxText_get_size(FlxText *self)
{
	return self->_textField->defaultTextFormat.size;
}

void FlxText_set_size(FlxText *self, Number Size)
{
	TextFormat format = FlxText_dtfCopy(self);
	format.size = Size;
	self->_textField->defaultTextFormat = format;
	TextField_setTextFormat(self->_textField, format);
	self->_regen = true;
	CALL0(self, calcFrame);
}

const char *FlxText_get_alignment(FlxText *self)
{
	return self->_textField->defaultTextFormat.align;
}

void FlxText_set_alignment(FlxText *self, const char *Alignment)
{
	TextFormat format = FlxText_dtfCopy(self);
	format.align = Alignment;
	self->_textField->defaultTextFormat = format;
	TextField_setTextFormat(self->_textField, format);
	CALL0(self, calcFrame);
}

/* GPU adapter: with the GPU path the glyphs are rendered untinted and the
   colourTransform (alpha fades of the day counter, hints) is a GPU tint, so
   a fade no longer re-renders the text every frame. Returns true when the
   untinted buffer is already up to date. */
static bool gpu_text_cached(FlxText *self)
{
	if (!Gpu_recording)
		return false;
	TextFormat f = self->_textField->defaultTextFormat;
	const char *text = self->_textField->text ? self->_textField->text : "";
	size_t n = strlen(text) + 160;
	char *key = malloc(n);
	snprintf(key, n, "%s|%g|%lx|%s|%lx|%g|%g|%d", f.font ? f.font : "", f.size, (unsigned long)f.color, f.align ? f.align : "",
		(unsigned long)self->_shadow, self->width, self->height, (int)self->_regen);
	bool same = !self->_regen && self->_pixels != NULL && self->_gpuKey != NULL && strcmp(key, self->_gpuKey) == 0;
	free(self->_gpuKey);
	self->_gpuKey = key;
	if (same)
		self->_gpuLazy = true;
	return same;
}

void FlxText_calcFrame(FlxText *self)
{
	if (gpu_text_cached(self))
		return;
	ColorTransform *gpuTint = self->_colorTransform; /* GPU adapter: glyphs untinted, tint on the GPU */
	if (Gpu_recording)
		self->_colorTransform = NULL;
	if (self->_regen) {
		/* Need to generate a new buffer to store the text graphic */
		uint32_t i = 0;
		uint32_t nl = TextField_get_numLines(self->_textField);
		self->height = 0;
		while (i < nl)
			self->height += TextField_getLineMetrics(self->_textField, i++).height;
		self->height += 4; /* account for 2px gutter on top and bottom */
		self->_pixels = BitmapData_new(AS3_INT(self->width), AS3_INT(self->height), true, 0);
		self->frameHeight = AS3_UINT(self->height);
		self->_textField->height = self->height * 1.2;
		self->_flashRect.x = 0;
		self->_flashRect.y = 0;
		self->_flashRect.width = self->width;
		self->_flashRect.height = self->height;
		self->_regen = false;
	} else /* Else just clear the old buffer before redrawing the text */
		BitmapData_fillRect(self->_pixels, self->_flashRect, 0);

	if ((self->_textField != NULL) && (self->_textField->text != NULL) && (strlen(self->_textField->text) > 0)) {
		/* Now that we've cleared a buffer, we need to actually render the text to it */
		TextFormat format = self->_textField->defaultTextFormat;
		TextFormat formatAdjusted = format;
		Matrix_identity(&self->_matrix);
		/* If it's a single, centered line of text, we center it ourselves so it doesn't blur to hell */
		if (as3_streq(format.align, "center") && (TextField_get_numLines(self->_textField) == 1)) {
			formatAdjusted = TextFormat_new(format.font, format.size, format.color, "left");
			TextField_setTextFormat(self->_textField, formatAdjusted);
			Matrix_translate(&self->_matrix, floor((self->width - TextField_getLineMetrics(self->_textField, 0).width) / 2), 0);
		}
		/* Render a single pixel shadow beneath the text */
		if (self->_shadow > 0) {
			TextField_setTextFormat(self->_textField, TextFormat_new(formatAdjusted.font, formatAdjusted.size, self->_shadow, formatAdjusted.align));
			Matrix_translate(&self->_matrix, 1, 1);
			BitmapData_drawTextField(self->_pixels, self->_textField, &self->_matrix, self->_colorTransform);
			Matrix_translate(&self->_matrix, -1, -1);
			TextField_setTextFormat(self->_textField, TextFormat_new(formatAdjusted.font, formatAdjusted.size, formatAdjusted.color, formatAdjusted.align));
		}
		/* Actually draw the text onto the buffer */
		BitmapData_drawTextField(self->_pixels, self->_textField, &self->_matrix, self->_colorTransform);
		TextField_setTextFormat(self->_textField, TextFormat_new(format.font, format.size, format.color, format.align));
	}

	/* Finally, update the visible pixels */
	if ((self->framePixels == NULL) || (self->framePixels->width != self->_pixels->width) || (self->framePixels->height != self->_pixels->height))
		self->framePixels = BitmapData_new(self->_pixels->width, self->_pixels->height, true, 0);
	BitmapData_copyPixels(self->framePixels, self->_pixels, self->_flashRect, self->_flashPointZero, false);
	self->_colorTransform = gpuTint; /* GPU adapter: draw the untinted buffer with the tint */
	self->_gpuLazy = Gpu_recording;
	self->_gpuSheetX = self->_gpuSheetY = 0;
	self->_gpuFrameGen = self->framePixels->gen;
	self->_gpuSheetGen = self->_pixels->gen;
	if (Gpu_recording) /* key after the render: _regen and height are final now */
		gpu_text_cached(self);
}

TextFormat FlxText_dtfCopy(FlxText *self)
{
	TextFormat defaultTextFormat = self->_textField->defaultTextFormat;
	return TextFormat_new(defaultTextFormat.font, defaultTextFormat.size, defaultTextFormat.color, defaultTextFormat.align);
}

/* ---- FlxText members the game never calls ---------------------------------- */

uint32_t FlxText_get_color(FlxText *self)
{
	return self->_textField->defaultTextFormat.color;
}

void FlxText_set_color(FlxText *self, uint32_t Color)
{
	TextFormat format = FlxText_dtfCopy(self);
	format.color = Color;
	self->_textField->defaultTextFormat = format;
	TextField_setTextFormat(self->_textField, format);
	self->_regen = true;
	CALL0(self, calcFrame);
}

const char *FlxText_get_font(FlxText *self)
{
	return self->_textField->defaultTextFormat.font;
}

void FlxText_set_font(FlxText *self, const char *Font)
{
	TextFormat format = FlxText_dtfCopy(self);
	format.font = Font;
	self->_textField->defaultTextFormat = format;
	TextField_setTextFormat(self->_textField, format);
	self->_regen = true;
	CALL0(self, calcFrame);
}

uint32_t FlxText_get_shadow(FlxText *self)
{
	return self->_shadow;
}

void FlxText_set_shadow(FlxText *self, uint32_t Color)
{
	self->_shadow = Color;
	CALL0(self, calcFrame);
}
