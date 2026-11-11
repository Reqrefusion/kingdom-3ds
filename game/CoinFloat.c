/* CoinFloat.as */
#include "game.h"

static const char *Img = "/assets/gfx/coin_drop.png";

const As3Class CoinFloat_class = {"CoinFloat", &FlxSprite_class, NULL, {FLXSPRITE_VT}};

CoinFloat *CoinFloat_new(void)
{
	CoinFloat *self = as3_alloc(sizeof *self);
	self->klass = &CoinFloat_class;
	SUPER(FlxSprite_ctor((FlxSprite *)self, 0, 0, NULL));
	FlxSprite_loadGraphic((FlxSprite *)self, Img, true, false, 10, 24, false);
	FlxSprite_addAnimation((FlxSprite *)self, "spin", ARR(0, 1, 2, 3, 4, 5, 6, 7), 10, true);
	FlxSprite_play((FlxSprite *)self, "spin", false);
	return self;
}

void CoinFloat_float(CoinFloat *self, FlxSprite *above)
{
	self->acceleration.y = 0;
	self->x = above->x + above->width / 2 - self->width / 2;
	self->y = Math_max(above->y - self->height - 4, 40);
	FlxSprite_set_color(above, 0xCCCC00);
}
