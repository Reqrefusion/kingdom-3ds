/* Farmland.as */
#include "game.h"

static const char *FarmlandImg = "/assets/gfx/farmland.png";

const As3Class Farmland_class = {
	"Farmland", &FlxSprite_class, NULL,
	{FLXSPRITE_VT, .update = (void (*)(FlxBasic *))Farmland_update},
};

Farmland *Farmland_new(int32_t X, int32_t Y)
{
	Farmland *self = as3_alloc(sizeof *self);
	self->klass = &Farmland_class;
	self->t = 0;
	SUPER(FlxSprite_ctor((FlxSprite *)self, X, Y, NULL));
	FlxSprite_loadGraphic((FlxSprite *)self, FarmlandImg, true, false, 64, 32, false);
	FlxSprite_addAnimation((FlxSprite *)self, "grow", ARR(0, 1, 2, 3, 4, 5, 6, 7), 10, true);
	return self;
}

bool Farmland_needsWork(Farmland *self)
{
	return self->t > Farmland_WORK_COOLDOWN;
}

void Farmland_work(Farmland *self, Citizen *by)
{
	self->t = 0;
	if (FlxSprite_get_frame((FlxSprite *)self) == 7) {
		Coin_drop((Coin *)FlxGroup_recycle(PLAYSTATE->coins, &Coin_class), (FlxSprite *)self, (FlxObject *)by, false);
		Coin_drop((Coin *)FlxGroup_recycle(PLAYSTATE->coins, &Coin_class), (FlxSprite *)self, (FlxObject *)by, false);
		Coin_drop((Coin *)FlxGroup_recycle(PLAYSTATE->coins, &Coin_class), (FlxSprite *)self, (FlxObject *)by, false);
		Coin_drop((Coin *)FlxGroup_recycle(PLAYSTATE->coins, &Coin_class), (FlxSprite *)self, (FlxObject *)by, false);
		Citizen_pickNewGoal(by, self->x + self->width / 2);
	}
	FlxSprite_set_frame((FlxSprite *)self, (FlxSprite_get_frame((FlxSprite *)self) + 1) % 8);
}

void Farmland_update(Farmland *self)
{
	self->t += FlxG.elapsed;
}
