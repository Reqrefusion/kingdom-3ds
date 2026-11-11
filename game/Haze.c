/* Haze.as */
#include "game.h"

const As3Class Haze_class = {
	"Haze", &FlxSprite_class, NULL,
	{FLXSPRITE_VT, .draw = (void (*)(FlxBasic *))Haze_draw},
};

Haze *Haze_new(int32_t X, int32_t Y, Weather *weather)
{
	Haze *self = as3_alloc(sizeof *self);
	self->klass = &Haze_class;
	self->weatherChanged = -1;
	SUPER(FlxSprite_ctor((FlxSprite *)self, X, Y, NULL));
	self->weather = weather;
	FlxSprite_makeGraphic((FlxSprite *)self, FlxG.width, FlxG.height, 0x00000000, false, NULL);
	self->scrollFactor.x = 0;
	return self;
}

void Haze_draw(Haze *self)
{
	if (self->weather->changed > self->weatherChanged) {
		FlxSprite_fill((FlxSprite *)self, 0);
		Utils_gradientOverlay(FlxSprite_get_pixels((FlxSprite *)self), (const uint32_t[]){self->weather->haze & 0xFFFFFF, self->weather->haze}, 2, 90, 1);
		self->weatherChanged = self->weather->t;
		self->dirty = true;
	}
	SUPER(FlxSprite_draw((FlxSprite *)self));
}
