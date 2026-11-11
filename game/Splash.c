/* Splash.as */
#include "game.h"

static const char *Img = "/assets/gfx/splash.png";

const As3Class Splash_class = {
	"Splash", &FlxParticle_class, (FlxBasic * (*)(void)) Splash_new,
	{FLXPARTICLE_VT, .reset = (void (*)(FlxBasic *, Number, Number))Splash_reset},
};

Splash *Splash_new(void)
{
	Splash *self = as3_alloc(sizeof *self);
	self->klass = &Splash_class;
	SUPER(FlxParticle_ctor((FlxParticle *)self));
	FlxSprite_loadGraphic((FlxSprite *)self, Img, true, false, 0, 0, false);
	FlxSprite_addAnimation((FlxSprite *)self, "splash", ARR(0, 1, 2, 3, 4), 10, true);
	return self;
}

void Splash_reset(Splash *self, Number X, Number Y)
{
	SUPER(FlxObject_reset((FlxObject *)self, X, Y));
	self->lifespan = 0.5;
	FlxSprite_play((FlxSprite *)self, "splash", false);
}
