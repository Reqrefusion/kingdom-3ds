/* Firefly.as */
#include "game.h"

static const char *LightSmallImg = "/assets/gfx/light_small.png";
static const char *LightReflectSmallImg = "/assets/gfx/light_reflect_small.png";

const As3Class Firefly_class = {
	"Firefly", &Light_class, NULL,
	{LIGHT_VT, .update = (void (*)(FlxBasic *))Firefly_update},
};

Firefly *Firefly_new(Number X, Number Y)
{
	Firefly *self = as3_alloc(sizeof *self);
	self->klass = &Firefly_class;
	self->weatherChanged = 0;
	self->startPos = FlxPoint_new(0, 0);
	SUPER(Light_ctor((Light *)self, X, Y));
	self->startPos.x = X;
	self->startPos.y = Y;
	self->offset.x = 0;
	self->offset.y = 0;
	FlxSprite_makeGraphic((FlxSprite *)self, 1, 1, Firefly_COLOR, false, NULL);
	FlxSprite_loadGraphic(self->beam, LightSmallImg, false, false, 0, 0, false);
	FlxSprite_loadGraphic(self->reflected, LightReflectSmallImg, false, false, 0, 0, false);
	FlxSprite_set_color(self->reflected, Firefly_COLOR);
	FlxSprite_addAnimation((FlxSprite *)self, "on", ARR(0), 6, true);
	FlxSprite_addAnimationCallback((FlxSprite *)self, (As3AnimCallback)AS3_FN(self, Light_dim));
	FlxSprite_play((FlxSprite *)self, "on", false);
	Light_setLight((Light *)self);
	return self;
}

void Firefly_update(Firefly *self)
{
	self->velocity.x += (FlxG_random() - 0.5) * 0.5;
	self->velocity.y += (FlxG_random() - 0.5) * 0.2;
	if (Math_abs(self->startPos.x - self->x) > Firefly_MAX_DIST || Math_abs(self->startPos.y - self->y) > Firefly_MAX_DIST) {
		self->x = self->startPos.x;
		self->y = self->startPos.y;
		self->velocity.x = 0;
		self->velocity.y = 0;
	}
	if (self->weather->changed > self->weatherChanged) {
		FlxSprite_set_alpha(self->beam, 1.5 * (self->weather->darkness) * (1 - self->weather->wind));
		FlxSprite_drawFrame(self->beam, true);
		FlxSprite_set_alpha((FlxSprite *)self, FlxSprite_get_alpha(self->beam));
		if (FlxSprite_get_alpha((FlxSprite *)self) < 0.1 && self->visible) {
			if (FlxG_random() < 0.05)
				self->visible = false;
		}
		if (FlxSprite_get_alpha((FlxSprite *)self) >= 0.1 && !self->visible) {
			if (FlxG_random() < 0.05)
				self->visible = true;
		}
		self->weatherChanged = self->weather->t;
	}
	SUPER(Light_update((Light *)self));
}
