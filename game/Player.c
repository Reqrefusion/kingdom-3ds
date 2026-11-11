/* Player.as */
#include "game.h"

static const char *PlayerImg = "/assets/gfx/king.png";
static const char *PickupSound = "/assets/sound/pickup.mp3";
static const char *BuildSound = "/assets/sound/build.mp3";
static const char *ThrowSound = "/assets/sound/throw.mp3";
static const char *StolenSound = "/assets/sound/stolen.mp3";

/* public static var pickupSound:FlxSound = FlxG.loadSound(PickupSound) */
static FlxSound *Player_pickupSound;
static void Player_staticInit(void)
{
	if (Player_pickupSound == NULL)
		Player_pickupSound = FlxG_loadSound(PickupSound, 1.0, false, false, false);
}

#define BASE_SKIN 0xFFedbebfu
#define BASE_DARK 0xFFbd9898u
#define BASE_EYES 0xFFa18383u

static void Player_checkSelectable(Player *self, FlxGroup *group);
static void Player_select(Player *self, FlxSprite *building);
static void Player_deselect(Player *self, FlxSprite *building);
static void Player_giveCoin(Player *self, FlxSprite *building);

const As3Class Player_class = {
	"Player", &FlxSprite_class, NULL,
	{FLXSPRITE_VT, .update = (void (*)(FlxBasic *))Player_update},
};

Player *Player_new(int32_t X, int32_t Y)
{
	Player_staticInit();
	Player *self = as3_alloc(sizeof *self);
	self->klass = &Player_class;
	self->selectedBuilding = NULL;
	self->floatCoin = NULL;
	self->lastMoved = 0;
	self->food = 100;
	self->hasCrown = true;
	self->lastTrollHit = 0;
	self->coins = 7;
	SUPER(FlxSprite_ctor((FlxSprite *)self, X, Y, NULL));
	self->y = 100; /* 132 (land height) - 32 (player height) */
	FlxSprite_loadGraphic((FlxSprite *)self, PlayerImg, true, true, 64, 64, false);
	self->width = 20;
	self->height = 32;
	self->offset.x = 22;
	self->offset.y = 32;
	self->maxVelocity.x = Player_MAX_SPEED;
	self->drag.x = self->maxVelocity.x * 5;
	self->playstate = PLAYSTATE;
	FlxSprite_addAnimation((FlxSprite *)self, "walk_slow", ARR(0, 1, 2, 3, 4, 5, 6, 7), 10, true);
	FlxSprite_addAnimation((FlxSprite *)self, "walk_fast", ARR(0, 1, 2, 3, 4, 5, 6, 7), 15, true);
	FlxSprite_addAnimation((FlxSprite *)self, "stand", ARR(8), 10, true);
	FlxSprite_addAnimation((FlxSprite *)self, "eat", ARR(9, 10), 5, true);
	FlxSprite_addAnimation((FlxSprite *)self, "nocrown", ARR(11), 10, true);
	FlxSprite_play((FlxSprite *)self, "stand", false);
	self->playstate->player = (FlxSprite *)self;
	Number d = Math_random() * 20;
	uint32_t skin = Utils_HSVtoRGB(d, 0.19 + (d / 100), 0.97 - (d / 33));
	Utils_replaceColor(FlxSprite_get_pixels((FlxSprite *)self), BASE_SKIN, skin);
	Utils_replaceColor(FlxSprite_get_pixels((FlxSprite *)self), BASE_DARK, Utils_interpolateColor(skin, 0xFF000000, 0.2));
	Utils_replaceColor(FlxSprite_get_pixels((FlxSprite *)self), BASE_EYES, Utils_interpolateColor(skin, 0xFF000000, 0.5));
	return self;
}

void Player_changeCoins(Player *self, int32_t amt)
{
	if (amt > 0) {
		FlxSound_play(Player_pickupSound, false);
		FlxSound_proximity(Player_pickupSound, self->x, self->y, (FlxObject *)self, FlxG.width, true);
	}
	self->coins += amt;
	PlayState_showCoins(self->playstate);
}

void Player_hitByTroll(Player *self, Troll *troll)
{
	if (troll->hasCoin)
		return;
	if (self->lastTrollHit < Player_HIT_RATE)
		return;
	self->lastTrollHit = 0;
	if (self->coins > 0) {
		Coin *c = (Coin *)FlxGroup_recycle(self->playstate->coins, &Coin_class);
		Coin_drop(c, (FlxSprite *)self, (FlxObject *)troll, true);
		c->justThrown = true;
		FlxSound_proximity(FlxG_play(StolenSound, 1.0, false, true), self->x, self->y, (FlxObject *)self, FlxG.width, true);
		Player_changeCoins(self, -1);
		FlxG_shake(0.05, 0.5, (As3Callback0)AS3_NULL_FN, true, 0);
		return;
	}
	if (self->hasCrown) {
		FlxG_flash(0xFFFFFFFF, 0.1, (As3Callback0)AS3_NULL_FN, false);
		Troll_stealCrown(troll);
		Player_lostCrown(self, (FlxObject *)troll);
		PlayState_crownStolen(self->playstate);
	}
}

void Player_lostCrown(Player *self, FlxObject *troll)
{
	self->hasCrown = false;
	FlxSprite_set_facing((FlxSprite *)self, troll->x > self->x ? RIGHT : LEFT);
	FlxSprite_play((FlxSprite *)self, "nocrown", false);
	FlxSound_proximity(FlxG_play(StolenSound, 1.0, false, true), self->x, self->y, (FlxObject *)self, FlxG.width, true);
	Utils_explode((FlxSprite *)self, self->playstate->gibs, 1, 4, true);
}

void Player_pickup(Player *self, FlxObject *coin)
{
	if (!coin->alive)
		return;
	Coin *c = (Coin *)coin;
	if (c->justThrown) {
		return;
	}
	CALL0(c, kill);
	Player_changeCoins(self, 1);
}

void Player_update(Player *self)
{
	self->lastTrollHit += FlxG.elapsed;
	self->acceleration.x = 0;
	if (!self->hasCrown) {
		return;
	}
	if (FlxG.keys->LEFT || FlxG.keys->RIGHT) {
		self->lastMoved = 0;
		if (self->food > 0) {
			self->food -= FlxG.elapsed;
		}
		self->maxVelocity.x = Player_MIN_SPEED + Math_min(1, self->food / Player_MAX_FOOD_BONUS) * (Player_MAX_SPEED - Player_MIN_SPEED);
		if (!self->playstate->horseAdvice && self->food < 10) {
			self->playstate->horseAdvice = true;
			PlayState_showText(self->playstate, "Horse is tired. Let him rest on the grass.");
		}
	}
	if (FlxG.keys->LEFT) {
		self->acceleration.x = -self->maxVelocity.x * 4;
		FlxSprite_set_facing((FlxSprite *)self, LEFT);
		if (self->maxVelocity.x > Player_MIN_SPEED + 15) {
			FlxSprite_play((FlxSprite *)self, "walk_fast", false);
		} else {
			FlxSprite_play((FlxSprite *)self, "walk_slow", false);
		}
	} else if (FlxG.keys->RIGHT) {
		self->acceleration.x = self->maxVelocity.x * 4;
		FlxSprite_set_facing((FlxSprite *)self, RIGHT);
		if (self->maxVelocity.x > Player_MIN_SPEED + 15) {
			FlxSprite_play((FlxSprite *)self, "walk_fast", false);
		} else {
			FlxSprite_play((FlxSprite *)self, "walk_slow", false);
		}
	} else {
		self->lastMoved += FlxG.elapsed;
		if (self->lastMoved > 1 && self->food < Player_MAX_FOOD) {
			Number headPos = (self->x + self->width / 2) + (FlxSprite_get_facing((FlxSprite *)self) == RIGHT ? 25 : -25);
			int32_t onTile = (int32_t)FlxTilemap_getTile(self->playstate->floor, AS3_UINT(headPos / 32), 4);
			if ((onTile >= 7 && onTile <= 11) || (onTile >= 17 && onTile <= 18)) {
				FlxSprite_play((FlxSprite *)self, "eat", false);
				self->food += FlxG.elapsed * 10;
			} else {
				FlxSprite_play((FlxSprite *)self, "stand", false);
			}
		} else {
			FlxSprite_play((FlxSprite *)self, "stand", false);
		}
	}
	if (FlxG.keys->SHIFT && PlayState_CHEATS) {
		self->velocity.x *= 10;
	}
	if (FlxKeyboard_justPressed(FlxG.keys, "DOWN")) {
		if (self->coins <= 0) {
			PlayState_showCoins(self->playstate);
		} else {
			if (self->selectedBuilding != NULL) {
				Player_changeCoins(self, -1);
				Player_giveCoin(self, self->selectedBuilding);
			} else {
				Citizen *cit;
				Citizen *closestCitizen = NULL;
				Number closest = 1000000;
				for (int32_t i = 0; i < self->playstate->beggars->length; i++) {
					cit = As3Array_get(&self->playstate->beggars->members, (uint32_t)i);
					if (Math_abs((cit->x + cit->width / 2) - (self->x + self->width / 2)) < closest) {
						closestCitizen = cit;
						closest = Math_abs((cit->x + cit->width / 2) - (self->x + self->width / 2));
					}
				}
				if (self->playstate->recruitedCitizen || closest < 64) {
					Coin *c = (Coin *)FlxGroup_recycle(self->playstate->coins, &Coin_class);
					Coin_drop(c, (FlxSprite *)self, (FlxObject *)closestCitizen, false);
					c->justThrown = true;
					FlxSound_proximity(FlxG_play(ThrowSound, 1.0, false, true), self->x, self->y, (FlxObject *)self, FlxG.width, true);
					Player_changeCoins(self, -1);
				}
			}
		}
	}
	SUPER(FlxSprite_update(self));
	if (self->selectedBuilding != NULL) {
		if (Math_abs((self->selectedBuilding->x + self->selectedBuilding->width / 2) - (self->x + self->width / 2)) > Player_SELECT_DISTANCE * 2) {
			Player_deselect(self, self->selectedBuilding);
		}
	} else if (self->playstate->recruitedCitizen) {
		Player_checkSelectable(self, self->playstate->objects);
		Player_checkSelectable(self, self->playstate->shops);
		Player_checkSelectable(self, self->playstate->walls);
	}
	if (self->x < 0) {
		self->velocity.x = Math_max(self->velocity.x, 0);
	} else if (self->x + self->width > PlayState_GAME_WIDTH) {
		self->velocity.x = Math_min(self->velocity.x, 0);
	}
}

static void Player_checkSelectable(Player *self, FlxGroup *group)
{
	for (int32_t i = 0; i < group->length; i++) {
		FlxSprite *b = As3Array_get(&group->members, (uint32_t)i);
		if (b != NULL && Math_abs((b->x + b->width / 2) - (self->x + self->width / 2)) <= Player_SELECT_DISTANCE) {
			if (Buildable_canBuild((FlxBasic *)b))
				Player_select(self, b);
		}
	}
}

static void Player_select(Player *self, FlxSprite *building)
{
	self->selectedBuilding = building;
	if (self->floatCoin == NULL) {
		FlxGroup_add((FlxGroup *)self->playstate, (FlxBasic *)(self->floatCoin = CoinFloat_new()));
	}
	self->floatCoin->visible = true;
	CoinFloat_float(self->floatCoin, self->selectedBuilding);
}

static void Player_deselect(Player *self, FlxSprite *building)
{
	(void)building;
	FlxSprite_set_color(self->selectedBuilding, 0xFFFFFFFF);
	self->selectedBuilding = NULL;
	self->floatCoin->visible = false;
}

static void Player_giveCoin(Player *self, FlxSprite *building)
{
	Buildable_build((FlxBasic *)building);
	FlxSound_proximity(FlxG_play(BuildSound, 1.0, false, true), self->x, self->y, (FlxObject *)self, FlxG.width, true);
	Player_deselect(self, building);
}
