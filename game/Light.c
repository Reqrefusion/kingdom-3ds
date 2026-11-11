/* Light.as */
#include "game.h"

const As3Class Light_class = {"Light", &FlxSprite_class, NULL, {LIGHT_VT}};

void Light_ctor(Light *self, Number X, Number Y)
{
	self->beam = FlxSprite_new(0, 0, NULL);
	self->reflected = FlxSprite_new(0, 0, NULL);
	SUPER(FlxSprite_ctor((FlxSprite *)self, X, Y, NULL));
	self->playstate = PLAYSTATE;
	self->darkness = self->playstate->darkness;
	self->weather = self->playstate->weather;
}

void Light_setLight(Light *self)
{
	self->beam->blend = "screen";
}

void Light_update(Light *self)
{
	FlxObject_getScreenXY((FlxObject *)self, &self->_point, NULL);
	self->burning = (-128 < self->_point.x && self->_point.x < FlxG.width + 128);
}

void Light_draw(Light *self)
{
	if (self->burning) {
		FlxObject_getScreenXY((FlxObject *)self, &self->_point, NULL);
		FlxSprite_stamp(self->darkness, self->beam, AS3_INT(floor(self->_point.x - self->beam->width / 2)), AS3_INT(floor(self->_point.y - self->beam->height / 2)));
	}
	SUPER(FlxSprite_draw((FlxSprite *)self));
}

void Light_dim(void *object, const char *animName, uint32_t frameNumber, uint32_t frameIndex)
{
	Light *self = object;
	(void)animName;
	(void)frameNumber;
	(void)frameIndex;
	if (self->burning) {
		FlxSprite_set_alpha(self->beam, FlxSprite_get_alpha(self->beam) + FlxG_random() * 0.15 - 0.075);
		if (FlxSprite_get_alpha(self->beam) < 0.3) {
			FlxSprite_set_alpha(self->beam, FlxSprite_get_alpha(self->beam) + 0.01);
		}
		FlxSprite_drawFrame(self->beam, true);
	}
}
