/* org/flixel/FlxText.as */
#ifndef TRANSLIT_FLXTEXT_H
#define TRANSLIT_FLXTEXT_H
#include "flixel.h"
#include "../flash/text.h"

typedef struct FlxText {
	FLXSPRITE_FIELDS
	TextField *_textField;
	bool _regen;
	uint32_t _shadow;
	char *_gpuKey; /* GPU adapter: what the untinted glyphs were rendered from */
} FlxText;

extern const As3Class FlxText_class;
FlxText *FlxText_new(Number X, Number Y, uint32_t Width, const char *Text, bool EmbeddedFont);
void FlxText_destroy(FlxText *self);
FlxText *FlxText_setFormat(FlxText *self, const char *Font, Number Size, uint32_t Color, const char *Alignment, uint32_t ShadowColor);
const char *FlxText_get_text(FlxText *self);
void FlxText_set_text(FlxText *self, const char *Text);
Number FlxText_get_size(FlxText *self);
void FlxText_set_size(FlxText *self, Number Size);
const char *FlxText_get_alignment(FlxText *self);
void FlxText_set_alignment(FlxText *self, const char *Alignment);
void FlxText_calcFrame(FlxText *self);
TextFormat FlxText_dtfCopy(FlxText *self);
uint32_t FlxText_get_color(FlxText *self);
void FlxText_set_color(FlxText *self, uint32_t Color);
const char *FlxText_get_font(FlxText *self);
void FlxText_set_font(FlxText *self, const char *Font);
uint32_t FlxText_get_shadow(FlxText *self);
void FlxText_set_shadow(FlxText *self, uint32_t Color);

#endif
