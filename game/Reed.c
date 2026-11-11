/* Reed.as */
#include "game.h"

static const char *ReedImg = "/assets/gfx/reed.png";

const As3Class Reed_class = {
	"Reed", &FlxSprite_class, NULL,
	{FLXSPRITE_VT, .update = (void (*)(FlxBasic *))Reed_update},
};

Reed *Reed_new(int32_t X, int32_t Y)
{
	Reed *self = as3_alloc(sizeof *self);
	self->klass = &Reed_class;
	self->weatherChanged = 0;
	self->t = 0;
	SUPER(FlxSprite_ctor((FlxSprite *)self, X, Y, NULL));
	FlxSprite_loadGraphic((FlxSprite *)self, ReedImg, true, false, 32, 32, false);
	self->weather = PLAYSTATE->weather;
	return self;
}

void Reed_update(Reed *self)
{
	self->t += self->weather->wind;
	FlxSprite_set_frame((FlxSprite *)self, AS3_UINT(AS3_INT(3 * (0.5 + 0.5 * sin(0.05 * self->t + self->x)) + 0.3 * sin(0.2 * self->t))));
}
