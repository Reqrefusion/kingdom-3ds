/* Coin.as */
#include "game.h"

static const char *Img = "/assets/gfx/coin.png";

const As3Class Coin_class = {
	"Coin", &FlxParticle_class, (FlxBasic * (*)(void)) Coin_new,
	{FLXPARTICLE_VT, .update = (void (*)(FlxBasic *))Coin_update},
};

Coin *Coin_new(void)
{
	Coin *self = as3_alloc(sizeof *self);
	self->klass = &Coin_class;
	self->owner = NULL;
	self->justThrown = false;
	self->called = false;
	SUPER(FlxParticle_ctor((FlxParticle *)self));

	FlxSprite_loadGraphic((FlxSprite *)self, Img, true, false, 10, 10, false);
	self->maxVelocity.x = 20;
	self->maxVelocity.y = 275;
	self->acceleration.y = 900;
	FlxSprite_addAnimation((FlxSprite *)self, "spin", ARR(0, 1, 2, 3, 4, 5, 6, 7), 10, true);
	FlxSprite_play((FlxSprite *)self, "spin", false);
	self->elasticity = 0.5;
	return self;
}

Coin *Coin_drop(Coin *self, FlxSprite *from, FlxObject *owner, bool far)
{
	CALL(self, reset, from->x + from->width / 2 - 5, Math_max(40, from->y - 10));
	self->lifespan = Coin_TOTAL_LIFESPAN;
	if (far) {
		self->velocity.x = FlxG_random() * 140 - 70;
		self->velocity.y = -180;
	} else {
		self->velocity.x = FlxG_random() * 60 - 30;
		self->velocity.y = -180;
	}
	self->called = false;
	self->owner = owner;
	if (owner != NULL && as3_is(owner, &Citizen_class)) {
		Citizen_pickNewGoal((Citizen *)owner, self->x + self->width / 2 + self->velocity.x);
	}
	return self;
}

void Coin_update(Coin *self)
{
	if (!self->called && self->lifespan <= Coin_TOTAL_LIFESPAN - Coin_OWNER_LIFESPAN / 2) {
		self->justThrown = false;
		Citizen *cit = as3_is(self->owner, &Citizen_class) ? (Citizen *)self->owner : NULL;
		if (cit) {
			self->called = true;
			self->justThrown = false;
			FlxObject_flicker((FlxObject *)self, 1);
			Citizen_pickNewGoal(cit, self->x + self->width / 2);
		}
	}
	if (self->owner != NULL && self->lifespan <= Coin_TOTAL_LIFESPAN - Coin_OWNER_LIFESPAN) {
		FlxObject_flicker((FlxObject *)self, 1);
		self->owner = NULL;
	}
	SUPER(FlxParticle_update((FlxParticle *)self));
}
