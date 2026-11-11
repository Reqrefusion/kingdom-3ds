/* Campfire.as */
#include "game.h"

static const char *CampfireImg = "/assets/gfx/campfire.png";
static const char *LightLargeImg = "/assets/gfx/light_large.png";
static const char *LightReflectWideImg = "/assets/gfx/light_reflect_wide.png";

const As3Class Campfire_class = {"Campfire", &Light_class, NULL, {LIGHT_VT}};

Campfire *Campfire_new(Number X, Number Y)
{
	Campfire *self = as3_alloc(sizeof *self);
	self->klass = &Campfire_class;
	Y -= 12;
	SUPER(Light_ctor((Light *)self, X, Y));
	self->offset.x = 16;
	self->offset.y = 52;
	FlxSprite_loadGraphic((FlxSprite *)self, CampfireImg, true, false, 32, 64, false);
	FlxSprite_loadGraphic(self->beam, LightLargeImg, false, false, 0, 0, false);
	FlxSprite_loadGraphic(self->reflected, LightReflectWideImg, false, false, 0, 0, false);
	FlxSprite_set_color(self->reflected, 0xFFfc8f53);
	FlxSprite_addAnimation((FlxSprite *)self, "on", ARR(0, 1, 2, 3, 4, 5, 6, 7), 10, true);
	FlxSprite_addAnimationCallback((FlxSprite *)self, (As3AnimCallback)AS3_FN(self, Light_dim));
	FlxSprite_play((FlxSprite *)self, "on", false);
	Light_setLight((Light *)self);
	return self;
}
