/* Sparkle.as */
#include "game.h"

static const char *Img = "/assets/gfx/sparkle.png";

const As3Class Sparkle_class = {
	"Sparkle", &FlxParticle_class, (FlxBasic * (*)(void)) Sparkle_new,
	{FLXPARTICLE_VT, .reset = (void (*)(FlxBasic *, Number, Number))Sparkle_reset},
};

Sparkle *Sparkle_new(void)
{
	Sparkle *self = as3_alloc(sizeof *self);
	self->klass = &Sparkle_class;
	SUPER(FlxParticle_ctor((FlxParticle *)self));
	FlxSprite_loadGraphic((FlxSprite *)self, Img, true, false, 0, 0, false);
	FlxSprite_addAnimation((FlxSprite *)self, "splash", ARR(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14), 30, false);
	return self;
}

void Sparkle_reset(Sparkle *self, Number X, Number Y)
{
	SUPER(FlxObject_reset((FlxObject *)self, X, Y));
	self->lifespan = 1.0;
	FlxSprite_play((FlxSprite *)self, "splash", true);
}
