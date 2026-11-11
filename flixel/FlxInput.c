/* org/flixel/system/input/Input.as, Keyboard.as, Mouse.as (the parts the
 * game calls). Keys are addressed by their Flixel names; the platform layer
 * reports raw key state with FlxKeyboard_setRaw / FlxMouse_setRaw, the way
 * Flash would deliver keyDown/keyUp and mouseDown/mouseUp events. */
#include "FlxInput.h"

static FlxKeyboard keyboard;
static FlxMouse mouse;

static FlxKey *find(const char *KeyName)
{
	for (uint32_t i = 0; i < keyboard.count; i++)
		if (strcmp(keyboard.keys[i].name, KeyName) == 0)
			return &keyboard.keys[i];
	if (keyboard.count < FLX_MAX_KEYS) {
		FlxKey *k = &keyboard.keys[keyboard.count++];
		snprintf(k->name, sizeof k->name, "%s", KeyName);
		return k;
	}
	return NULL;
}

void FlxKeyboard_setRaw(const char *KeyName, bool down)
{
	FlxKey *k = find(KeyName);
	if (k != NULL)
		k->raw = down;
}

/* Input.update(): last/current state machine (2 = just pressed,
   1 = pressed, 0 = up, -1 = just released). */
static void Input_update(int32_t *current, int32_t *last)
{
	if ((*last == -1) && (*current == -1))
		*current = 0;
	else if ((*last == 2) && (*current == 2))
		*current = 1;
	*last = *current;
}

/* handleKeyDown/handleKeyUp equivalent, then update(). */
void FlxKeyboard_update(FlxKeyboard *self)
{
	for (uint32_t i = 0; i < self->count; i++) {
		FlxKey *k = &self->keys[i];
		if (k->raw && k->current <= 0)
			k->current = (k->current > 0) ? 1 : 2;
		else if (!k->raw && k->current > 0)
			k->current = (k->current > 0) ? -1 : 0;
		Input_update(&k->current, &k->last);
	}
	self->LEFT = FlxKeyboard_pressed(self, "LEFT");
	self->RIGHT = FlxKeyboard_pressed(self, "RIGHT");
	self->UP = FlxKeyboard_pressed(self, "UP");
	self->DOWN = FlxKeyboard_pressed(self, "DOWN");
	self->SHIFT = FlxKeyboard_pressed(self, "SHIFT");
	self->SPACE = FlxKeyboard_pressed(self, "SPACE");
	self->ENTER = FlxKeyboard_pressed(self, "ENTER");
	self->ESCAPE = FlxKeyboard_pressed(self, "ESCAPE");
}

bool FlxKeyboard_pressed(FlxKeyboard *self, const char *Key)
{
	(void)self;
	FlxKey *k = find(Key);
	return k != NULL && k->current > 0;
}

bool FlxKeyboard_justPressed(FlxKeyboard *self, const char *Key)
{
	(void)self;
	FlxKey *k = find(Key);
	return k != NULL && k->current == 2;
}

bool FlxKeyboard_justReleased(FlxKeyboard *self, const char *Key)
{
	(void)self;
	FlxKey *k = find(Key);
	return k != NULL && k->current == -1;
}

bool FlxKeyboard_any(FlxKeyboard *self)
{
	for (uint32_t i = 0; i < self->count; i++)
		if (self->keys[i].current > 0)
			return true;
	return false;
}

void FlxKeyboard_reset(FlxKeyboard *self)
{
	for (uint32_t i = 0; i < self->count; i++) {
		self->keys[i].current = 0;
		self->keys[i].last = 0;
	}
}

void FlxMouse_setRaw(bool down, Number screenX, Number screenY)
{
	mouse.raw = down;
	mouse.screenX = AS3_INT(screenX);
	mouse.screenY = AS3_INT(screenY);
}

void FlxMouse_update(FlxMouse *self)
{
	if (self->raw && self->_current <= 0)
		self->_current = (self->_current > 0) ? 1 : 2;
	else if (!self->raw && self->_current > 0)
		self->_current = (self->_current > 0) ? -1 : 0;
	if ((self->_last == -1) && (self->_current == -1))
		self->_current = 0;
	else if ((self->_last == 2) && (self->_current == 2))
		self->_current = 1;
	self->_last = self->_current;
	if (FlxG.camera != NULL) {
		self->x = self->screenX + FlxG.camera->scroll.x;
		self->y = self->screenY + FlxG.camera->scroll.y;
	}
}

bool FlxMouse_pressed(FlxMouse *self) { return self->_current > 0; }
bool FlxMouse_justPressed(FlxMouse *self) { return self->_current == 2; }
bool FlxMouse_justReleased(FlxMouse *self) { return self->_current == -1; }
void FlxMouse_show(FlxMouse *self) { self->visible = true; }
void FlxMouse_hide(FlxMouse *self) { self->visible = false; }

void FlxMouse_reset(FlxMouse *self)
{
	self->_current = 0;
	self->_last = 0;
}

/* FlxG.updateInput / FlxG.resetInput */
void FlxG_updateInput(void)
{
	FlxG.keys = &keyboard;
	FlxG.mouse = &mouse;
	FlxKeyboard_update(FlxG.keys);
	FlxMouse_update(FlxG.mouse);
}

void FlxG_resetInput(void)
{
	FlxG.keys = &keyboard;
	FlxG.mouse = &mouse;
	FlxKeyboard_reset(FlxG.keys);
	FlxMouse_reset(FlxG.mouse);
}
