/* Sky.as */
#include "game.h"

const As3Class Sky_class = {
	"Sky", &FlxSprite_class, NULL,
	{FLXSPRITE_VT, .update = (void (*)(FlxBasic *))Sky_update},
};

Sky *Sky_new(Weather *weather)
{
	Sky *self = as3_alloc(sizeof *self);
	self->klass = &Sky_class;
	self->weatherChanged = 0;
	SUPER(FlxSprite_ctor((FlxSprite *)self, 0, 0, NULL)); /* implicit super() */
	self->weather = weather;
	self->scrollFactor.x = self->scrollFactor.y = 0;
	FlxSprite_makeGraphic((FlxSprite *)self, FlxG.width, FlxG.height, 0x00000000, true, NULL);
	return self;
}

void Sky_update(Sky *self)
{
	if (self->weather->changed > self->weatherChanged) {
		Utils_gradientOverlay(FlxSprite_get_pixels((FlxSprite *)self), (const uint32_t[]){self->weather->sky, self->weather->horizon, self->weather->haze}, 3, 90, 1);
		self->dirty = true;
		self->weatherChanged = self->weather->t;
	}
}
