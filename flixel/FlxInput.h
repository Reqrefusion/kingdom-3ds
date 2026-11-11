#ifndef TRANSLIT_FLXINPUT_H
#define TRANSLIT_FLXINPUT_H
#include "flixel.h"

#define FLX_MAX_KEYS 64

typedef struct FlxKey {
	char name[16];
	bool raw;
	int32_t current;
	int32_t last;
} FlxKey;

struct FlxKeyboard {
	/* Properties read directly as FlxG.keys.LEFT etc. */
	bool LEFT, RIGHT, UP, DOWN, SHIFT, SPACE, ENTER, ESCAPE;
	FlxKey keys[FLX_MAX_KEYS];
	uint32_t count;
};

struct FlxMouse {
	Number x, y;
	int32_t screenX, screenY;
	bool visible;
	bool raw;
	int32_t _current;
	int32_t _last;
};

void FlxKeyboard_setRaw(const char *KeyName, bool down);
void FlxKeyboard_update(FlxKeyboard *self);
bool FlxKeyboard_pressed(FlxKeyboard *self, const char *Key);
bool FlxKeyboard_justPressed(FlxKeyboard *self, const char *Key);
bool FlxKeyboard_justReleased(FlxKeyboard *self, const char *Key);
bool FlxKeyboard_any(FlxKeyboard *self);
void FlxKeyboard_reset(FlxKeyboard *self);

void FlxMouse_setRaw(bool down, Number screenX, Number screenY);
void FlxMouse_update(FlxMouse *self);
bool FlxMouse_pressed(FlxMouse *self);
bool FlxMouse_justPressed(FlxMouse *self);
bool FlxMouse_justReleased(FlxMouse *self);
void FlxMouse_show(FlxMouse *self);
void FlxMouse_hide(FlxMouse *self);
void FlxMouse_reset(FlxMouse *self);

void FlxG_updateInput(void);
void FlxG_resetInput(void);

#endif
