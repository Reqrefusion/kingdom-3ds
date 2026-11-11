/* Dust.as */
#include "game.h"

static const char *Img = "/assets/gfx/dust.png";

const As3Class Dust_class = {
	"Dust", &FlxParticle_class, (FlxBasic * (*)(void)) Dust_new,
	{FLXPARTICLE_VT, .reset = (void (*)(FlxBasic *, Number, Number))Dust_reset},
};

Dust *Dust_new(void)
{
	Dust *self = as3_alloc(sizeof *self);
	self->klass = &Dust_class;
	SUPER(FlxParticle_ctor((FlxParticle *)self));
	FlxSprite_loadGraphic((FlxSprite *)self, Img, true, false, 0, 0, false);
	FlxSprite_addAnimation((FlxSprite *)self, "fade", ARR(0, 1, 2, 3), 5, false);
	self->drag.x = self->drag.y = 20;
	return self;
}

void Dust_reset(Dust *self, Number X, Number Y)
{
	SUPER(FlxObject_reset((FlxObject *)self, X, Y));
	self->x += (Math_random() < 0.5) ? 4 : -4;
	self->velocity.x = Math_random() * 40 - 20;
	self->velocity.y = -Math_random() * 20 - 4;
	self->lifespan = 1.0;
	FlxSprite_play((FlxSprite *)self, "fade", true);
}
