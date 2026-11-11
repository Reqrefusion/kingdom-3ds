/* Bunny.as */
#include "game.h"

static const char *Img = "/assets/gfx/bunny.png";

const As3Class Bunny_class = {
	"Bunny", &FlxSprite_class, NULL,
	{FLXSPRITE_VT, .update = (void (*)(FlxBasic *))Bunny_update},
};

Bunny *Bunny_new(int32_t X, int32_t Y)
{
	Bunny *self = as3_alloc(sizeof *self);
	self->klass = &Bunny_class;
	self->DECAY_TIME = 5;
	self->TURN_TIME = 1;
	self->t = 0;
	SUPER(FlxSprite_ctor((FlxSprite *)self, X, Y, NULL));
	FlxSprite_loadGraphic((FlxSprite *)self, Img, true, true, 16, 16, false);
	self->offset.y = 4;
	self->height = 12;
	self->maxVelocity.y = 100;
	self->maxVelocity.x = 30;
	self->drag.x = 1000;
	FlxSprite_addAnimation((FlxSprite *)self, "walk", ARR(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10), (10 + FlxG_random() * 5), true);
	FlxSprite_addAnimation((FlxSprite *)self, "stand", ARR(0), 10, true);
	FlxSprite_addAnimation((FlxSprite *)self, "dead", ARR(11), 10, true);
	self->y = PLAYSTATE->groundHeight - 12;
	return self;
}

void Bunny_getShot(Bunny *self, Arrow *arrow)
{
	FlxSprite_play((FlxSprite *)self, "dead", false);
	self->alive = false;
	self->velocity.x = 0;
	self->t = 0;
	Coin_drop((Coin *)FlxGroup_recycle(PLAYSTATE->coins, &Coin_class), (FlxSprite *)self, (FlxObject *)arrow->shooter, false);
}

void Bunny_update(Bunny *self)
{
	/* Check for movement input */
	self->acceleration.x = 0;
	self->t += FlxG.elapsed;
	if (self->x + self->width > PlayState_GAME_WIDTH || self->x < 0) {
		CALL0(self, kill);
	}
	if (self->alive) {
		if (self->t > self->TURN_TIME) {
			self->t = 0;
			if (FlxG_random() < 0.4)
				FlxSprite_set_facing((FlxSprite *)self, (FlxSprite_get_facing((FlxSprite *)self) == LEFT) ? RIGHT : LEFT);
		}
		if (FlxSprite_get_facing((FlxSprite *)self) == LEFT) {
			self->acceleration.x = -self->maxVelocity.x * 4;
			FlxSprite_play((FlxSprite *)self, "walk", false);
		} else {
			self->acceleration.x = self->maxVelocity.x * 4;
			FlxSprite_play((FlxSprite *)self, "walk", false);
		}
		SUPER(FlxSprite_update(self));
	} else {
		FlxSprite_set_alpha((FlxSprite *)self, 1 - (self->t / self->DECAY_TIME));
		if (self->t > self->DECAY_TIME) {
			CALL0(self, kill);
		}
	}
}
