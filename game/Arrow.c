/* Arrow.as */
#include "game.h"

static const char *Img = "/assets/gfx/arrow.png";

const As3Class Arrow_class = {
	"Arrow", &FlxParticle_class, (FlxBasic * (*)(void)) Arrow_new,
	{FLXPARTICLE_VT, .update = (void (*)(FlxBasic *))Arrow_update},
};

Arrow *Arrow_new(void)
{
	Arrow *self = as3_alloc(sizeof *self);
	self->klass = &Arrow_class;
	self->shooter = NULL;
	SUPER(FlxParticle_ctor((FlxParticle *)self));
	FlxSprite_loadRotatedGraphic((FlxSprite *)self, Img, 16, -1, false, false);
	self->maxVelocity.x = 200;
	self->maxVelocity.y = 275;
	self->acceleration.y = 500;
	self->offset.x = 3;
	self->offset.y = 3;
	self->height = 2;
	self->width = 2;
	self->elasticity = 0.5;
	return self;
}

void Arrow_shotFrom(Arrow *self, Citizen *from, FlxObject *at)
{
	self->x = from->x + from->width / 2 + (FlxSprite_get_facing((FlxSprite *)from) == RIGHT ? 6 : -6);
	self->y = from->y + 10;
	CALL0(self, revive);
	self->velocity.x = (FlxSprite_get_facing((FlxSprite *)from) == RIGHT ? self->maxVelocity.x : -self->maxVelocity.x);
	self->velocity.y = -Math_abs(at->x - from->x) + FlxG_random() * 40;
	self->lifespan = 10;
	self->shooter = from;
}

void Arrow_update(Arrow *self)
{
	if (self->y > PLAYSTATE->water->y) {
		CALL0(self, kill);
		Splash *s = (Splash *)FlxGroup_recycle(PLAYSTATE->fx, &Splash_class);
		CALL(s, reset, self->x, self->y);
	}
	self->angle = (180 / Math_PI) * Math_atan2(self->velocity.y, self->velocity.x);
}
