/* Coinsack.as */
#include "game.h"

static const char *Img = "/assets/gfx/sack.png";

Number Coinsack_t = 0;

const As3Class Coinsack_class = {
	"Coinsack", &FlxSprite_class, NULL,
	{FLXSPRITE_VT, .update = (void (*)(FlxBasic *))Coinsack_update},
};

Coinsack *Coinsack_new(Number X, Number Y)
{
	Coinsack *self = as3_alloc(sizeof *self);
	self->klass = &Coinsack_class;
	SUPER(FlxSprite_ctor((FlxSprite *)self, X, Y, NULL));
	FlxSprite_loadGraphic((FlxSprite *)self, Img, true, false, 16, 16, false);
	self->scrollFactor.x = self->scrollFactor.y = 0;
	FlxSprite_addAnimation((FlxSprite *)self, "blink", ARR(8, 0, 8, 0, 8, 0, 8, 0, 8, 0), 5, false);
	return self;
}

void Coinsack_show(Coinsack *self, int32_t c)
{
	if (c == 0) {
		FlxSprite_play((FlxSprite *)self, "blink", true);
	} else if (c == 1) {
		FlxSprite_set_frame((FlxSprite *)self, 1);
	} else if (c >= 2 && c <= 3) {
		FlxSprite_set_frame((FlxSprite *)self, 2);
	} else if (c >= 4 && c <= 5) {
		FlxSprite_set_frame((FlxSprite *)self, 3);
	} else if (c >= 6 && c <= 8) {
		FlxSprite_set_frame((FlxSprite *)self, 4);
	} else if (c >= 9 && c <= 11) {
		FlxSprite_set_frame((FlxSprite *)self, 5);
	} else if (c >= 12 && c <= 15) {
		FlxSprite_set_frame((FlxSprite *)self, 6);
	} else if (c >= 15) {
		FlxSprite_set_frame((FlxSprite *)self, 7);
	}
	Coinsack_t = 0;
}

void Coinsack_update(Coinsack *self)
{
	Coinsack_t += FlxG.elapsed;
	FlxSprite_set_alpha((FlxSprite *)self, Coinsack_FADE_TIME - Coinsack_t);
}
