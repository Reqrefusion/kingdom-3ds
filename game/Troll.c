/* Troll.as */
#include "game.h"

static const char *Img = "/assets/gfx/troll.png";
static const char *ImgBig = "/assets/gfx/trollbig.png";

static void Troll_loadAnims(Troll *self);

const As3Class Troll_class = {
	"Troll", &FlxSprite_class, (FlxBasic * (*)(void)) Troll_new,
	{
		FLXSPRITE_VT,
		.update = (void (*)(FlxBasic *))Troll_update,
		.reset = (void (*)(FlxBasic *, Number, Number))Troll_reset,
		.kill = (void (*)(FlxBasic *))Troll_kill,
	},
};

Troll *Troll_new(void)
{
	Troll *self = as3_alloc(sizeof *self);
	self->klass = &Troll_class;
	self->t = 0;
	self->goal = 1600;
	self->hasCoin = false;
	self->hasCrown = false;
	self->wait = false;
	self->retreating = false;
	self->maxSpeed = 20;
	self->jumpHeight = 100;
	self->jumpiness = 0.01;
	self->confusion = 0.01;
	self->big = false;
	self->safeDistance = 200;
	self->maxHeightReached = 0;
	self->jumpCooldown = 0;
	self->confuseCooldown = 0;
	SUPER(FlxSprite_ctor((FlxSprite *)self, 0, 0, NULL));
	self->maxVelocity.y = 275;
	self->maxVelocity.x = 60;
	self->acceleration.y = 900;
	Troll_loadAnims(self);
	self->playstate = PLAYSTATE;
	return self;
}

void Troll_reset(Troll *self, Number X, Number Y)
{
	self->retreating = false;
	self->hasCoin = false;
	self->wait = true;
	self->visible = false;
	self->velocity.x = self->velocity.y = 0;
	if (!self->big == self->playstate->trollBig) {
		self->big = self->playstate->trollBig;
		Troll_loadAnims(self);
	}
	X -= self->width / 2;
	SUPER(FlxObject_reset((FlxObject *)self, X - self->width / 2, Y));
	self->health = self->playstate->trollHealth;
	self->maxSpeed = self->playstate->trollMaxSpeed;
	self->jumpHeight = self->playstate->trollJumpHeight;
	self->jumpiness = self->playstate->trollJumpiness;
	self->jumpCooldown = self->jumpiness + Math_random() * 2 * self->jumpiness;
	self->confusion = self->playstate->trollConfusion;
	self->confuseCooldown = self->confusion + Math_random() * 2 * self->confusion;
	self->t = 1;
	if (FlxGroup_remove(self->playstate->trollsNoCollide, (FlxBasic *)self, false)) {
		FlxGroup_add(self->playstate->trolls, (FlxBasic *)self);
	}
}

static void Troll_loadAnims(Troll *self)
{
	if (self->big) {
		FlxSprite_loadGraphic((FlxSprite *)self, ImgBig, true, true, 64, 64, false);
		self->offset.x = 24;
		self->offset.y = 24;
		self->width = 16;
		self->height = 40;
		FlxSprite_addAnimation((FlxSprite *)self, "walk", ARR(0, 1, 2, 3, 4, 5, 6, 7, 8), 7, true);
		FlxSprite_addAnimation((FlxSprite *)self, "walk_crown", ARR(9, 10, 11, 12, 13, 14, 15, 16, 17), 7, true);
		FlxSprite_addAnimation((FlxSprite *)self, "stand", ARR(0), 10, true);
	} else {
		FlxSprite_loadGraphic((FlxSprite *)self, Img, true, true, 32, 32, false);
		self->offset.x = 12;
		self->offset.y = 12;
		self->width = 8;
		self->height = 20;
		FlxSprite_addAnimation((FlxSprite *)self, "walk", ARR(0, 1, 2, 3, 4, 5, 6, 7, 8), (10 + FlxG_random() * 5), true);
		FlxSprite_addAnimation((FlxSprite *)self, "walk_crown", ARR(9, 10, 11, 12, 13, 14, 15, 16, 17), (10 + FlxG_random() * 5), true);
		FlxSprite_addAnimation((FlxSprite *)self, "walk_coin", ARR(18, 19, 20, 21, 22, 23, 24, 25, 26), (10 + FlxG_random() * 5), true);
		FlxSprite_addAnimation((FlxSprite *)self, "stand", ARR(0), 10, true);
	}
}

void Troll_getsCoin(Troll *self)
{
	self->hasCoin = true;
	Troll_retreat(self);
}

void Troll_pickup(Troll *self, FlxObject *coin)
{
	if (!self->hasCoin && coin->alive && !self->retreating && !self->big) {
		CALL0(coin, kill);
		self->hasCoin = true;
		Troll_retreat(self);
	}
}

void Troll_stealCrown(Troll *self)
{
	self->hasCrown = true;
	PlayState_panTo(self->playstate, (FlxSprite *)self, 20, 0);
	Troll_retreat(self);
}

void Troll_getShot(Troll *self)
{
	if (self->hasCrown)
		return;
	self->health--;
	if (self->health > 0) {
	} else {
		Utils_explode((FlxSprite *)self, self->playstate->gibs, 1.0, 4, true);
		if (self->hasCoin) {
			Coin_drop((Coin *)FlxGroup_recycle(self->playstate->coins, &Coin_class), (FlxSprite *)self, NULL, false);
		}
		CALL0(self, kill);
	}
}

void Troll_kill(Troll *self)
{
	FlxGroup_remove(self->playstate->trollsNoCollide, (FlxBasic *)self, false);
	FlxGroup_add(self->playstate->trolls, (FlxBasic *)self);
	SUPER(FlxBasic_kill((FlxBasic *)self));
}

void Troll_retreat(Troll *self)
{
	self->retreating = true;
	self->goal = (self->x < self->playstate->player->x) ? 0 : FlxG.worldBounds.width;
	self->wait = false;
	FlxGroup_remove(self->playstate->trolls, (FlxBasic *)self, false);
	FlxGroup_add(self->playstate->trollsNoCollide, (FlxBasic *)self);
}

void Troll_go(Troll *self)
{
	self->wait = false;
	self->visible = true;
	self->goal = self->playstate->player->x;
	if (self->big) {
		FlxGroup_remove(self->playstate->trolls, (FlxBasic *)self, false);
		FlxGroup_add(self->playstate->trollsNoCollide, (FlxBasic *)self);
	}
}

void Troll_update(Troll *self)
{
	if (self->wait) {
		self->acceleration.x = 0;
		self->velocity.x = 0;
		return;
	}
	self->confuseCooldown -= FlxG.elapsed;
	self->jumpCooldown -= FlxG.elapsed;
	self->acceleration.x = 0;
	self->t += FlxG.elapsed;
	if (!self->hasCoin && self->t > 1.8) {
		if (self->retreating || self->confuseCooldown < 0) {
			self->goal = (self->x < self->playstate->player->x) ? 0 : FlxG.worldBounds.width;
			self->confuseCooldown = self->confusion + Math_random() * 2 * self->confusion;
		} else {
			self->goal = self->playstate->player->x;
		}
		self->t = 0;
	}
	if (self->x <= 24 || self->x + self->width >= FlxG.worldBounds.width - 24) {
		if (self->retreating)
			CALL0(self, kill);
	}
	if (self->y > 200) {
		FlxG_log("TROLL FELL");
		CALL0(self, kill);
	}
	FlxSprite_set_facing((FlxSprite *)self, (self->goal > self->x) ? RIGHT : LEFT);
	if (self->touching & FLOOR) {
		self->maxVelocity.x = self->maxSpeed;
		if (self->x > self->playstate->kingdomRight + self->safeDistance || self->x < self->playstate->kingdomLeft - self->safeDistance) {
			self->maxVelocity.x += 40;
		}
		self->drag.x = self->maxVelocity.x * 10;
		if (FlxSprite_get_facing((FlxSprite *)self) == LEFT) {
			self->acceleration.x = -self->maxVelocity.x * 4;
		} else {
			self->acceleration.x = self->maxVelocity.x * 4;
		}
		if (self->hasCrown)
			FlxSprite_play((FlxSprite *)self, "walk_crown", false);
		else if (self->hasCoin)
			FlxSprite_play((FlxSprite *)self, "walk_coin", false);
		else
			FlxSprite_play((FlxSprite *)self, "walk", false);
		if (self->jumpCooldown < 0) {
			self->maxHeightReached = 0;
			Number v = Math_sqrt(self->jumpHeight * 2 * self->acceleration.y);
			self->velocity.y = -v;
			self->maxVelocity.x = self->maxSpeed * 2;
			self->velocity.x *= 2;
			self->jumpCooldown = self->jumpiness + Math_random() * 2 * self->jumpiness;
		}
	} else {
		self->maxHeightReached = Math_max(112 - self->y, self->maxHeightReached);
		self->drag.x = self->maxVelocity.x * 0.1;
		self->maxVelocity.x = self->maxSpeed * 2;
		if (FlxSprite_get_facing((FlxSprite *)self) == LEFT)
			self->acceleration.x = -self->maxVelocity.x;
		else
			self->acceleration.x = self->maxVelocity.x;
	}
	SUPER(FlxSprite_update(self));
}
