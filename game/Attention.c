/* Attention.as */
#include "game.h"

static const char *Img = "/assets/gfx/attention.png";

const As3Class Attention_class = {
	"Attention", &FlxParticle_class, (FlxBasic * (*)(void)) Attention_new,
	{FLXPARTICLE_VT, .update = (void (*)(FlxBasic *))Attention_update},
};

Attention *Attention_new(void)
{
	Attention *self = as3_alloc(sizeof *self);
	self->klass = &Attention_class;
	SUPER(FlxParticle_ctor((FlxParticle *)self));
	FlxSprite_loadGraphic((FlxSprite *)self, Img, false, false, 0, 0, false);
	self->maxVelocity.x = 0;
	self->maxVelocity.y = 0;
	self->height = 8;
	self->width = 8;
	FlxSprite_set_alpha((FlxSprite *)self, 0.5);
	return self;
}

void Attention_appearAt(Attention *self, Citizen *at)
{
	self->citizen = at;
	self->y = at->y - 4;
	CALL0(self, revive);
	self->lifespan = 1;
}

void Attention_update(Attention *self)
{
	self->x = self->citizen->x + self->citizen->width / 2 - 4;
	SUPER(FlxParticle_update((FlxParticle *)self));
}
