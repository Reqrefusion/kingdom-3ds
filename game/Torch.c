/* Torch.as */
#include "game.h"

static const char *TorchImg = "/assets/gfx/torch.png";
static const char *LightMidImg = "/assets/gfx/light_mid.png";
static const char *LightReflectSmallImg = "/assets/gfx/light_reflect_small.png";

const As3Class Torch_class = {
	"Torch", &Light_class, NULL,
	{LIGHT_VT, .update = (void (*)(FlxBasic *))Torch_update},
};

Torch *Torch_new(Number X, Number Y)
{
	Torch *self = as3_alloc(sizeof *self);
	self->klass = &Torch_class;
	Y += 8;
	SUPER(Light_ctor((Light *)self, X, Y));
	self->offset.x = self->width / 2;
	self->offset.y = 8;
	FlxSprite_loadGraphic((FlxSprite *)self, TorchImg, true, true, 16, 32, false);
	FlxSprite_loadGraphic(self->beam, LightMidImg, false, false, 0, 0, false);
	FlxSprite_loadGraphic(self->reflected, LightReflectSmallImg, false, false, 0, 0, false);
	FlxSprite_set_color(self->reflected, 0xFFfc8f53);
	if (FlxG_random() < 0.5) {
		FlxSprite_addAnimation((FlxSprite *)self, "on", ARR(0, 1, 2, 3, 4, 5, 6, 7), 6, true);
	} else {
		FlxSprite_addAnimation((FlxSprite *)self, "on", ARR(8, 9, 10, 11, 12, 13, 14, 15), 6, true);
	}
	FlxSprite_set_facing((FlxSprite *)self, (FlxG_random() < 0.5) ? LEFT : RIGHT);
	FlxSprite_addAnimationCallback((FlxSprite *)self, (As3AnimCallback)AS3_FN(self, Light_dim));
	FlxSprite_play((FlxSprite *)self, "on", false);
	Light_setLight((Light *)self);
	return self;
}

void Torch_update(Torch *self)
{
	if (self->x < self->playstate->kingdomRight + 64 && self->x > self->playstate->kingdomLeft - 64) {
		self->visible = true;
	} else {
		self->visible = false;
	}
	SUPER(Light_update((Light *)self));
}
