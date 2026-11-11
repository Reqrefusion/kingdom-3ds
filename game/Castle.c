/* Castle.as */
#include "game.h"

static const char *CastleImg = "/assets/gfx/castle.png";

#define Castle_BUILD_COOLDOWN (PlayState_CHEATS ? 1 : 15)

/* ARCHER_POSITIONS: arrays of FlxPoint per castle type */
static const FlxPoint ARCHER_POSITIONS_0[] = {{58 - 32, 104 - 32 - 12}};
static const FlxPoint ARCHER_POSITIONS_1[] = {{48 - 32, 91 - 32 - 12}, {136 - 32, 91 - 32 - 12}};
static const FlxPoint ARCHER_POSITIONS_2[] = {{48 - 32, 91 - 32 - 12}, {136 - 32, 91 - 32 - 12}, {60, 46 - 32 - 12}};
static const FlxPoint ARCHER_POSITIONS_3[] = {{48 - 32, 91 - 32 - 12}, {136 - 32, 91 - 32 - 12}, {45, 46 - 32 - 12}, {75, 46 - 32 - 12}};
static const FlxPoint ARCHER_POSITIONS_4[] = {{48 - 32, 91 - 32 - 12}, {136 - 32, 91 - 32 - 12},
	{38, 46 - 32 - 12}, {83, 46 - 32 - 12},
	{27, 64 - 32 - 12}, {93, 64 - 32 - 12}};
static const struct {
	const FlxPoint *points;
	uint32_t length;
} ARCHER_POSITIONS[] = {
	{ARCHER_POSITIONS_0, 1}, {ARCHER_POSITIONS_1, 2}, {ARCHER_POSITIONS_2, 3}, {ARCHER_POSITIONS_3, 4}, {ARCHER_POSITIONS_4, 6}};
static const int32_t ARCHER_POSITION_INDEX[] = {0, 0, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4};
#define ARCHER_POSITION_INDEX_length 15

const As3Class Castle_class = {
	"Castle", &FlxSprite_class, NULL,
	{FLXSPRITE_VT, .update = (void (*)(FlxBasic *))Castle_update},
};

Castle *Castle_new(Number X, Number Y)
{
	Castle *self = as3_alloc(sizeof *self);
	self->klass = &Castle_class;
	self->t = 0;
	self->archer_positions = (As3Array){0};
	self->capacity = 0;
	SUPER(FlxSprite_ctor((FlxSprite *)self, X, Y, NULL));
	self->baseY = Y + 1;
	self->moves = false;
	self->playstate = PLAYSTATE;
	self->lights = self->playstate->lights;
	self->archers = self->playstate->archers;
	self->playstate->castle = self;
	FlxSprite_loadGraphic((FlxSprite *)self, CastleImg, true, true, 128, 96, false);
	FlxSprite_addAnimation((FlxSprite *)self, "stages", ARR(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14), 1, true);
	Castle_morph(self, Castle_POST);
	self->light = (Light *)Campfire_new(self->x + self->width / 2, self->y + self->height);
	FlxGroup_add(self->lights, (FlxBasic *)self->light);
	return self;
}

void Castle_morph(Castle *self, int32_t stage)
{
	/* archer_positions = ARCHER_POSITIONS[...].slice(): new FlxPoint references */
	As3Array_set_length(&self->archer_positions, 0);
	int32_t type = ARCHER_POSITION_INDEX[stage];
	for (uint32_t i = 0; i < ARCHER_POSITIONS[type].length; i++)
		As3Array_set(&self->archer_positions, i, FlxPoint_alloc(ARCHER_POSITIONS[type].points[i].x, ARCHER_POSITIONS[type].points[i].y));
	/* for each (var archer:Citizen in archers.members) */
	for (uint32_t i = 0; i < self->archers->members.length; i++) {
		Citizen *archer = As3Array_get(&self->archers->members, i);
		if (archer != NULL) {
			Citizen_leaveGuard(archer);
		}
	}
	FlxSprite_set_frame((FlxSprite *)self, (uint32_t)stage);
	self->stage = stage;
	self->height = 84;
	self->y = self->baseY + 96 - self->height;
	self->offset.y = 96 - self->height;
}

void Castle_build(Castle *self)
{
	if (self->stage < ARCHER_POSITION_INDEX_length) {
		self->t = 0;
		Utils_explode((FlxSprite *)self, self->playstate->gibs, 0.4, 4, true);
		Castle_morph(self, self->stage + 1);
	}
}

bool Castle_canBuild(Castle *self)
{
	return (self->t > Castle_BUILD_COOLDOWN && self->stage < 14);
}

void Castle_update(Castle *self)
{
	self->t += FlxG.elapsed;
	self->playstate->kingdomRight = Math_max(self->playstate->kingdomRight, self->x + self->width);
	self->playstate->kingdomLeft = Math_min(self->playstate->kingdomLeft, self->x);
}
