/* Wall.as */
#include "game.h"

static const char *WallImg = "/assets/gfx/wall.png";
static const char *HitwallSound = "/assets/sound/hitwall.mp3";

const int32_t Wall_HEIGHT[5] = {11, 38, 46, 54, 59}; /* Effective Height: [26, 34, 42, 47] */
const int32_t Wall_HEALTH[5] = {2, 38, 50, 60, 75};

const As3Class Wall_class = {
	"Wall", &FlxSprite_class, NULL,
	{
		FLXSPRITE_VT,
		.update = (void (*)(FlxBasic *))Wall_update,
		.hurt = (void (*)(FlxBasic *, Number))Wall_hurt,
	},
};

Wall *Wall_new(Number X, Number Y)
{
	Wall *self = as3_alloc(sizeof *self);
	self->klass = &Wall_class;
	self->scaffold = NULL;
	self->building = false;
	self->heightToBuild = 0;
	self->stage = 0;
	self->t = 0;
	self->baseY = Y;
	SUPER(FlxSprite_ctor((FlxSprite *)self, X, Y, NULL));
	self->immovable = true;
	self->moves = false;
	FlxObject_set_solid((FlxObject *)self, false);
	FlxSprite_loadGraphic((FlxSprite *)self, WallImg, true, true, 32, 64, false);
	FlxSprite_addAnimation((FlxSprite *)self, "grow", ARR(0, 1, 2, 3, 4, 5, 6, 7, 8, 9), 1, true);
	if (X > 1920) {
		FlxSprite_set_facing((FlxSprite *)self, LEFT);
	}
	self->offset.x = 4;
	self->width = 24;
	self->health = Wall_HEALTH[self->stage];
	Wall_updateAppearance(self);
	self->playstate = PLAYSTATE;
	return self;
}

void Wall_build(Wall *self)
{
	Wall_buildTo(self, self->stage + 1, false);
}

void Wall_buildTo(Wall *self, int32_t s, bool instant)
{
	if (!instant && s < self->stage)
		return;
	self->building = true;
	self->stage = s;
	self->heightToBuild = Wall_HEIGHT[self->stage];
	self->health = Wall_HEALTH[self->stage];
	Wall_updateAppearance(self);
	if (self->scaffold != NULL) {
		CALL0(self->scaffold, kill);
	}
	self->scaffold = Scaffold_build((Scaffold *)FlxGroup_recycle(self->playstate->indicators, &Scaffold_class), (FlxSprite *)self);
	self->scaffold->y = self->y - Wall_HEIGHT[self->stage];
	FlxObject_set_solid((FlxObject *)self, false);
	if (instant) {
		self->heightToBuild = 1;
		Wall_work(self, NULL);
	}
}

void Wall_work(Wall *self, Citizen *citizen)
{
	(void)citizen;
	if (self->heightToBuild > 0) {
		self->heightToBuild -= Wall_WORK_BUILD_HEIGHT;
		if (self->heightToBuild <= 0) {
			self->heightToBuild = 0;
			self->building = false;
			FlxObject_set_solid((FlxObject *)self, true);
			Utils_explode((FlxSprite *)self->scaffold, self->playstate->gibs, 1, 4, true);
			CALL0(self->scaffold, kill);
			self->scaffold = NULL;
		}
	} else {
		if (self->health < Wall_HEALTH[self->stage]) {
			self->health += Math_min(Wall_WORK_HEAL_AMOUNT, Wall_HEALTH[self->stage] - self->health);
		}
	}
	Wall_updateAppearance(self);
}

void Wall_hurt(Wall *self, Number Damage)
{
	if (self->t > Wall_HURT_COOLDOWN) {
		self->health -= Damage;
		FlxSound_proximity(FlxG_play(HitwallSound, 1.0, false, true), self->x, self->y, (FlxObject *)self->playstate->player, FlxG.width, true);
		Utils_explode((FlxSprite *)self, self->playstate->gibs, 0.1, 4, true);
		self->t = 0;
	}
	if (self->health <= 0 && self->stage > 0) {
		Utils_explode((FlxSprite *)self, self->playstate->gibs, 1.0, 4, true);
		self->stage = 0;
		self->health = Wall_HEALTH[self->stage];
	}
	self->health = Math_max(self->health, Wall_HEALTH[0]);
	Wall_updateAppearance(self);
}

bool Wall_needsWork(Wall *self)
{
	/* SWF FIX: the shipped SWF (626899_main.swf, tools/abc_dump.py Wall needsWork) computes
	   (building || health < HEALTH[stage]) && t > HURT_COOLDOWN. The pinned source snapshot
	   has `t < HURT_COOLDOWN` (a typo): with it a wall under construction was workable only
	   during the first second after its hurt timer reset, so Farmers never finished walls and
	   the kingdom stayed 200 px wide. The shipped rule: no work during the 1 s after a hit. */
	return ((self->building || self->health < Wall_HEALTH[self->stage]) && self->t > Wall_HURT_COOLDOWN);
}

bool Wall_canBuild(Wall *self)
{
	return (!self->building && self->stage < 4);
}

void Wall_updateAppearance(Wall *self)
{
	self->height = Wall_HEIGHT[self->stage] - self->heightToBuild;
	self->y = self->baseY + 64 - self->height;
	self->offset.y = 64 - Wall_HEIGHT[self->stage];
	if (self->health < Wall_HEALTH[self->stage] / 2.0) {
		FlxSprite_set_frame((FlxSprite *)self, (uint32_t)(self->stage + 5));
	} else {
		FlxSprite_set_frame((FlxSprite *)self, (uint32_t)self->stage);
	}
}

void Wall_update(Wall *self)
{
	self->t += FlxG.elapsed;
	if (self->stage > 0 && !self->building) {
		if (self->x > PlayState_GAME_WIDTH / 2) {
			self->playstate->kingdomRight = Math_max(self->playstate->kingdomRight, self->x);
		} else {
			self->playstate->kingdomLeft = Math_min(self->playstate->kingdomLeft, self->x + self->width);
		}
	}
}

/* ---- interfaces Buildable and Workable --------------------------------------
   AS3 dispatches (b as Buildable).canBuild() on the object's class. */

bool Buildable_canBuild(FlxBasic *b)
{
	if (as3_is(b, &Castle_class)) return Castle_canBuild((Castle *)b);
	if (as3_is(b, &Shop_class)) return Shop_canBuild((Shop *)b);
	if (as3_is(b, &Wall_class)) return Wall_canBuild((Wall *)b);
	/* `b as Buildable` is null for other classes: calling it throws a
	   TypeError in Flash, which aborts the current frame's update. */
	FlxG_log("TypeError: not Buildable");
	return false;
}

void Buildable_build(FlxBasic *b)
{
	if (as3_is(b, &Castle_class)) Castle_build((Castle *)b);
	else if (as3_is(b, &Shop_class)) Shop_build((Shop *)b);
	else if (as3_is(b, &Wall_class)) Wall_build((Wall *)b);
}

bool Workable_needsWork(FlxBasic *w)
{
	if (as3_is(w, &Wall_class)) return Wall_needsWork((Wall *)w);
	if (as3_is(w, &Farmland_class)) return Farmland_needsWork((Farmland *)w);
	return false;
}

void Workable_work(FlxBasic *w, Citizen *citizen)
{
	if (as3_is(w, &Wall_class)) Wall_work((Wall *)w, citizen);
	else if (as3_is(w, &Farmland_class)) Farmland_work((Farmland *)w, citizen);
}
