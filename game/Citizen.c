/* Citizen.as */
#include "game.h"

static const char *BeggarImg = "/assets/gfx/beggar.png";
static const char *PoorImg = "/assets/gfx/citizen.png";
static const char *HunterImg = "/assets/gfx/hunter.png";
static const char *FarmerImg = "/assets/gfx/farmer.png";
static const char *ShootSound = "/assets/sound/shoot.mp3";
static const char *PowerupSound = "/assets/sound/powerup.mp3";
static const char *HitSound = "/assets/sound/hitcitizen.mp3";

/* public static var shootSound:FlxSound = FlxG.loadSound(ShootSound):
   AS3 runs static initializers when the class is first used. */
static FlxSound *Citizen_shootSound;
static void Citizen_staticInit(void)
{
	if (Citizen_shootSound == NULL)
		Citizen_shootSound = FlxG_loadSound(ShootSound, 1.0, false, false, false);
}

#define BASE_COLOR 0xFF567271u
#define BASE_SHADE 0xFF394b4au
#define BASE_SKIN 0xFFedbebfu
#define BASE_DARK 0xFFbd9898u
#define BASE_EYES 0xFFa18383u

#define SHOOT_COOLDOWN_GUARD 1.4
#define SHOOT_COOLDOWN 2.0
#define HACK_COOLDOWN 4.0
#define SHOVEL_PERIOD 4.0
#define SHOVEL_TIME 1.0
#define SHOVEL_GOAL_DIST 600.0
#define GIVE_COOLDOWN 10.0
#define COWER_COOLDOWN 5.0
#define HUNTER_BORDER_RANGE 256.0
#define MAX_HUNGRY 5.0

const As3Class Citizen_class = {
	"Citizen", &FlxSprite_class, NULL,
	{FLXSPRITE_VT, .update = (void (*)(FlxBasic *))Citizen_update},
};

Citizen *Citizen_new(int32_t X, int32_t Y)
{
	Citizen_staticInit();
	Citizen *self = as3_alloc(sizeof *self);
	self->klass = &Citizen_class;
	self->occupation = Citizen_BEGGAR;
	self->action = Citizen_IDLE;
	self->guarding = false;
	self->t = 0;
	self->coins = 0;
	self->giveCooldown = 0;
	self->shovelCooldown = 0;
	self->hungry = 0;
	self->goal = NAN;
	SUPER(FlxSprite_ctor((FlxSprite *)self, X, Y, NULL));
	self->goal = FlxG.worldBounds.width / 2;
	self->drag.x = 500;
	self->guardLeftBorder = (FlxG_random() > 0.5);
	/* AS3 evaluates arguments left to right; C does not, so the random
	   draws are taken in source order first. */
	Number h = FlxG_random() * 360, sat = 0.1 + FlxG_random() * 0.2;
	self->myColor = Utils_HSVtoRGB(h, sat, 0.6);
	Number d = Math_random() * 20;
	self->skin = Utils_HSVtoRGB(d, 0.19 + (d / 100), 0.97 - (d / 33));
	self->playstate = PLAYSTATE;
	self->castle = self->playstate->castle;
	FlxSprite_addAnimationCallback((FlxSprite *)self, (As3AnimCallback)AS3_FN(self, Citizen_animationFrame));
	Citizen_morph(self, Citizen_BEGGAR);
	return self;
}

Citizen *Citizen_morph(Citizen *self, int32_t occ)
{
	self->action = Citizen_IDLE;
	self->_animations = (As3Array){0};
	if (occ != Citizen_BEGGAR && self->coins <= 0) {
		self->coins++;
	}
	switch (occ) {
	case Citizen_BEGGAR:
		if (self->occupation != Citizen_BEGGAR)
			FlxGroup_add(self->playstate->beggars, FlxGroup_remove(self->playstate->characters, (FlxBasic *)self, true));
		FlxSprite_loadGraphic((FlxSprite *)self, BeggarImg, true, true, 32, 32, true);
		FlxSprite_addAnimation((FlxSprite *)self, "walk", ARR(0, 1, 2, 3, 4, 5), 5, true);
		FlxSprite_addAnimation((FlxSprite *)self, "idle", ARR(7, 8, 7, 8, 7, 6), 2, true);
		FlxSprite_addAnimation((FlxSprite *)self, "cower", ARR(9, 10), 2, false);
		self->maxVelocity.x = 15;
		self->hungry = 0;
		break;
	case Citizen_POOR:
		if (self->occupation == Citizen_BEGGAR)
			FlxGroup_add(self->playstate->characters, FlxGroup_remove(self->playstate->beggars, (FlxBasic *)self, true));
		FlxSprite_loadGraphic((FlxSprite *)self, PoorImg, true, true, 32, 32, true);
		Utils_replaceColor(FlxSprite_get_pixels((FlxSprite *)self), BASE_COLOR, self->myColor);
		Utils_replaceColor(FlxSprite_get_pixels((FlxSprite *)self), BASE_SHADE, Utils_interpolateColor(self->myColor, 0xFF000000, 0.2));
		self->maxVelocity.x = 17;
		FlxSprite_addAnimation((FlxSprite *)self, "walk", ARR(0, 1, 2, 3, 4, 5), 10, true);
		FlxSprite_addAnimation((FlxSprite *)self, "idle", ARR(0, 6, 0, 6, 0, 7), 2, true);
		break;
	case Citizen_HUNTER:
		if (self->guardLeftBorder) {
			Number h = 220 + FlxG_random() * 20, sat = 0.2 + FlxG_random() * 0.3; /* left-to-right */
			self->myColor = Utils_HSVtoRGB(h, sat, 0.7);
		} else {
			Number h = 0 + FlxG_random() * 20, sat = 0.2 + FlxG_random() * 0.3; /* left-to-right */
			self->myColor = Utils_HSVtoRGB(h, sat, 0.7);
		}
		FlxSprite_loadGraphic((FlxSprite *)self, HunterImg, true, true, 32, 32, true);
		Utils_replaceColor(FlxSprite_get_pixels((FlxSprite *)self), BASE_COLOR, self->myColor);
		Utils_replaceColor(FlxSprite_get_pixels((FlxSprite *)self), BASE_SHADE, Utils_interpolateColor(self->myColor, 0xFF000000, 0.2));
		self->maxVelocity.x = 18;
		FlxSprite_addAnimation((FlxSprite *)self, "walk", ARR(0, 1, 2, 3, 4, 5), 10, true);
		FlxSprite_addAnimation((FlxSprite *)self, "idle", ARR(6, 7, 6, 7, 6, 8), 2, true);
		FlxSprite_addAnimation((FlxSprite *)self, "shoot", ARR(9, 10, 0), 6, false);
		FlxSprite_addAnimation((FlxSprite *)self, "give", ARR(11, 12, 13), 15, false);
		break;
	case Citizen_FARMER:
		FlxSprite_loadGraphic((FlxSprite *)self, FarmerImg, true, true, 32, 32, true);
		Utils_replaceColor(FlxSprite_get_pixels((FlxSprite *)self), BASE_COLOR, self->myColor);
		Utils_replaceColor(FlxSprite_get_pixels((FlxSprite *)self), BASE_SHADE, Utils_interpolateColor(self->myColor, 0xFF000000, 0.2));
		self->maxVelocity.x = 21 + Math_random() * 3;
		FlxSprite_addAnimation((FlxSprite *)self, "walk", ARR(0, 1, 2, 3, 4, 5), 12, true);
		FlxSprite_addAnimation((FlxSprite *)self, "idle", ARR(6, 7, 6, 7, 6, 8), 2, true);
		FlxSprite_addAnimation((FlxSprite *)self, "shovel", ARR(8, 9, 10, 9), 6, true);
		FlxSprite_addAnimation((FlxSprite *)self, "give", ARR(11, 12, 13), 15, false);
		FlxSprite_addAnimation((FlxSprite *)self, "hack", ARR(14), 15, false);
		break;
	}
	Utils_replaceColor(FlxSprite_get_pixels((FlxSprite *)self), BASE_SKIN, self->skin);
	Utils_replaceColor(FlxSprite_get_pixels((FlxSprite *)self), BASE_DARK, Utils_interpolateColor(self->skin, 0xFF000000, 0.2));
	Utils_replaceColor(FlxSprite_get_pixels((FlxSprite *)self), BASE_EYES, Utils_interpolateColor(self->skin, 0xFF000000, 0.5));
	FlxSprite_drawFrame((FlxSprite *)self, true);
	self->occupation = occ;
	self->offset.x = 12;
	self->offset.y = 8;
	self->width = 8;
	self->height = 24;
	Citizen_pickNewGoal(self, NAN);
	return self;
}

void Citizen_pickup(Citizen *self, FlxObject *coin)
{
	if (!coin->alive)
		return;
	Coin *c = (Coin *)coin;
	if (c->owner != NULL && c->owner != (FlxObject *)self) {
		return;
	}
	CALL0(c, kill);
	Sparkle *s = (Sparkle *)FlxGroup_recycle(PLAYSTATE->fx, &Sparkle_class);
	CALL(s, reset, self->x - 4, self->y + 8);
	if (self->occupation == Citizen_BEGGAR) {
		self->playstate->recruitedCitizen = true;
		Citizen_morph(self, Citizen_POOR);
		FlxSound_proximity(FlxG_play(PowerupSound, 1.0, false, true), self->x, self->y, (FlxObject *)self->playstate->player, FlxG.width * 0.75, true);
	}
	self->coins++;
}

void Citizen_giveTaxes(Citizen *self, Player *p)
{
	if (self->occupation == Citizen_HUNTER || self->occupation == Citizen_FARMER) {
		if (self->action == Citizen_IDLE && self->coins > 3 && self->giveCooldown <= 0) {
			self->action = Citizen_GIVE;
			self->coins -= 2;
			FlxSprite_play((FlxSprite *)self, "give", false);
			Player_changeCoins(p, 1);
			self->giveCooldown = GIVE_COOLDOWN;
		}
	}
}

void Citizen_hitByTroll(Citizen *self, Troll *troll)
{
	if (self->occupation == Citizen_FARMER && self->action != Citizen_JUST_HACKED) {
		self->action = Citizen_JUST_HACKED;
		FlxSprite_play((FlxSprite *)self, "hack", false);
		self->t = 0;
		Troll_getShot(troll);
	} else if (self->coins > 0 && !troll->hasCoin) {
		Coin_drop((Coin *)FlxGroup_recycle(self->playstate->coins, &Coin_class), (FlxSprite *)self, (FlxObject *)self->playstate->player, false);
		FlxSound_proximity(FlxG_play(HitSound, 1.0, false, true), self->x, self->y, (FlxObject *)self->playstate->player, FlxG.width, true);
		self->coins = (self->coins > 1) ? 1 : 0;
		Utils_explode((FlxSprite *)self, self->playstate->gibs, 1, 4, true);
		if (self->coins == 0) {
			Citizen_morph(self, Citizen_BEGGAR);
		} else if (self->coins == 1) {
			Citizen_morph(self, Citizen_POOR);
		}
	}
	if (self->occupation == Citizen_BEGGAR && self->action == Citizen_IDLE) {
		self->action = Citizen_COWER;
		FlxSprite_play((FlxSprite *)self, "cower", true);
	}
}

void Citizen_checkShootable(Citizen *self, FlxGroup *group)
{
	FlxObject *c;
	for (int32_t i = 0; i < group->length; i++) {
		c = As3Array_get(&group->members, (uint32_t)i);
		if (c != NULL && c->alive && c->exists && Math_abs(c->x - self->x) < 96) {
			FlxSprite_play((FlxSprite *)self, "shoot", true);
			FlxSound_play(Citizen_shootSound, false);
			FlxSound_proximity(Citizen_shootSound, self->x, self->y, (FlxObject *)self->playstate->player, FlxG.width, true);
			self->goal = (c->x > self->x) ? self->x + 1 : self->x - 1;
			FlxSprite_set_facing((FlxSprite *)self, (self->goal > self->x) ? RIGHT : LEFT);
			self->target = c;
			self->action = Citizen_SHOOT;
			self->t = 0;
			break;
		}
	}
}

void Citizen_checkWork(Citizen *self, FlxGroup *group)
{
	FlxObject *c;
	for (int32_t i = 0; i < group->length; i++) {
		c = As3Array_get(&group->members, (uint32_t)i);
		if (c != NULL) {
			if (self->x > c->x && self->x + self->width < c->x + c->width) {
				if (Workable_needsWork((FlxBasic *)c)) {
					Workable_work((FlxBasic *)c, self);
					FlxSprite_play((FlxSprite *)self, "shovel", true);
					self->action = Citizen_SHOVEL;
					self->shovelCooldown = SHOVEL_PERIOD;
					self->t = 0;
				}
			}
		}
	}
}

void Citizen_checkGuard(Citizen *self)
{
	if (self->action == Citizen_IDLE && self->castle->archer_positions.length > 0) {
		if (Math_abs(self->castle->x - self->x) < 192) {
			for (int32_t i = 0; (uint32_t)i < self->castle->archer_positions.length; i++) {
				FlxPoint *pos = As3Array_get(&self->castle->archer_positions, (uint32_t)i);
				if (Math_abs(self->castle->x + pos->x - self->x) < 4) {
					self->x = self->castle->x + pos->x;
					self->y = self->castle->y + pos->y;
					self->guarding = true;
					FlxGroup_add(self->playstate->archers, FlxGroup_remove(self->playstate->characters, (FlxBasic *)self, true));
					As3Array_splice1(&self->castle->archer_positions, (uint32_t)i);
					break;
				}
			}
		}
	}
}

void Citizen_leaveGuard(Citizen *self)
{
	As3Array_set(&self->castle->archer_positions, self->castle->archer_positions.length, FlxPoint_alloc(self->x, self->y));
	FlxGroup_add(self->playstate->characters, FlxGroup_remove(self->playstate->archers, (FlxBasic *)self, false));
	/* `action == IDLE;` in the source is a comparison with no effect */
	self->guarding = false;
}

void Citizen_pickNewGoal(Citizen *self, Number preset)
{
	Attention *a = (Attention *)FlxGroup_recycle(self->playstate->fx, &Attention_class);
	Attention_appearAt(a, self);
	if (!isnan(preset)) {
		self->goal = preset;
		return;
	}
	if (self->occupation == Citizen_POOR) {
		Shop *shop = (Shop *)FlxGroup_getRandom(self->playstate->shops, 0, 0);
		self->goal = shop->x + shop->width / 2;
		return;
	}
	if (self->coins > 4) {
		self->goal = self->playstate->player->x;
		return;
	}
	if (self->occupation == Citizen_FARMER) {
		/* var needWork:Array = new Array(); (unused) */
		Number dist = 1.79769313486231e+308; /* Number.MAX_VALUE */
		Wall *wall, *closestWall = NULL;
		for (int32_t i = 0; i < self->playstate->walls->length; i++) {
			wall = As3Array_get(&self->playstate->walls->members, (uint32_t)i);
			if (wall != NULL && Wall_needsWork(wall) && (Math_abs(wall->x - self->x) < dist)) {
				closestWall = wall;
				dist = Math_abs(wall->x - self->x);
			}
		}
		if (closestWall != NULL) {
			self->goal = closestWall->x + closestWall->width / 2;
			return;
		}
	}
	int32_t l = 0, r = 0;
	if (self->occupation == Citizen_HUNTER) {
		if (self->playstate->weather->timeOfDay >= 0.65 || self->playstate->weather->timeOfDay < 0.20) {
			if (self->guardLeftBorder) {
				l = AS3_INT(self->playstate->kingdomLeft);
				r = AS3_INT(self->playstate->kingdomLeft + 32);
			} else {
				l = AS3_INT(self->playstate->kingdomRight - 32);
				r = AS3_INT(self->playstate->kingdomRight);
			}
		} else {
			l = AS3_INT(self->playstate->kingdomLeft - HUNTER_BORDER_RANGE);
			r = AS3_INT(self->playstate->kingdomRight + HUNTER_BORDER_RANGE);
		}
	} else if (self->occupation == Citizen_BEGGAR) {
		if (FlxGroup_countLiving(self->playstate->beggars) > self->playstate->minBeggars) {
			self->hungry++;
			if (self->hungry > MAX_HUNGRY) {
				Utils_explode((FlxSprite *)self, self->playstate->gibs, 1.0, 4, true);
				CALL0(self, kill);
			}
		}
		if (self->x < PlayState_GAME_WIDTH / 2) {
			l = AS3_INT(self->playstate->kingdomLeft - 256);
			r = AS3_INT(self->playstate->kingdomLeft);
		} else {
			l = AS3_INT(self->playstate->kingdomRight);
			r = AS3_INT(self->playstate->kingdomRight + 256);
		}
	} else {
		l = AS3_INT(self->playstate->kingdomLeft);
		r = AS3_INT(self->playstate->kingdomRight);
	}
	self->goal = AS3_INT(FlxG_random() * (r - l) + l);
}

void Citizen_animationFrame(void *object, const char *animName, uint32_t frameNum, uint32_t frameIndex)
{
	Citizen *self = object;
	(void)frameIndex;
	if (as3_streq(animName, "give") && frameNum == 2) {
		self->action = Citizen_IDLE;
		FlxSprite_play((FlxSprite *)self, "idle", false);
	}
	if (as3_streq(animName, "shovel")) {
		Dust *d = (Dust *)FlxGroup_recycle(self->playstate->fx, &Dust_class);
		CALL(d, reset, self->x + ((FlxSprite_get_facing((FlxSprite *)self) == RIGHT) ? 14 : -6), self->y + 19);
	}
}

void Citizen_update(Citizen *self)
{
	self->acceleration.x = 0;
	self->t += FlxG.elapsed;
	self->shovelCooldown -= FlxG.elapsed;
	self->giveCooldown -= FlxG.elapsed;
	if (self->guarding && self->occupation == Citizen_HUNTER) {
		FlxSprite_play((FlxSprite *)self, "idle", false);
		FlxSprite_set_facing((FlxSprite *)self, (self->goal > self->x) ? RIGHT : LEFT);
	} else if (self->action == Citizen_IDLE) {
		FlxSprite_set_facing((FlxSprite *)self, (self->goal > self->x) ? RIGHT : LEFT);
		if (Math_abs(self->goal - self->x) < 2) {
			if (self->t > 2.0 && FlxG_random() < 0.3) {
				self->t = 0;
				Citizen_pickNewGoal(self, NAN);
			} else {
				FlxSprite_play((FlxSprite *)self, "idle", false);
			}
		} else {
			FlxSprite_play((FlxSprite *)self, "walk", false);
			self->acceleration.x = (FlxSprite_get_facing((FlxSprite *)self) == RIGHT) ? self->maxVelocity.x * 10 : -self->maxVelocity.x * 10;
			self->y = self->playstate->groundHeight - self->height;
		}
	}
	if (self->occupation == Citizen_HUNTER) {
		if (self->action == Citizen_SHOOT && self->t > 0.16) {
			Arrow_shotFrom((Arrow *)FlxGroup_recycle(self->playstate->arrows, &Arrow_class), self, self->target);
			self->t = 0;
			self->action = Citizen_JUST_SHOT;
		} else if (self->action == Citizen_JUST_SHOT && self->t > (self->guarding ? SHOOT_COOLDOWN_GUARD : SHOOT_COOLDOWN)) {
			self->t = 0;
			self->action = Citizen_IDLE;
		} else if (self->action == Citizen_IDLE) {
			Citizen_checkShootable(self, self->playstate->trolls);
			Citizen_checkShootable(self, self->playstate->trollsNoCollide);
			if (self->action == Citizen_IDLE) {
				Citizen_checkShootable(self, self->playstate->bunnies);
			}
		}
		Citizen_checkGuard(self);
	} else if (self->occupation == Citizen_FARMER) {
		if (self->action == Citizen_JUST_HACKED && self->t > HACK_COOLDOWN) {
			self->t = 0;
			self->action = Citizen_IDLE;
		}
		if (self->shovelCooldown <= 0 && self->action == Citizen_IDLE) {
			Citizen_checkWork(self, self->playstate->walls);
			Citizen_checkWork(self, self->playstate->farmlands);
		} else if (self->action == Citizen_SHOVEL && self->t > SHOVEL_TIME) {
			self->t = 0;
			self->action = Citizen_IDLE;
		}
	} else if (self->occupation == Citizen_BEGGAR) {
		if (self->action == Citizen_COWER && self->t > COWER_COOLDOWN) {
			self->t = 0;
			self->action = Citizen_IDLE;
		}
	}
	SUPER(FlxSprite_update(self));
}
