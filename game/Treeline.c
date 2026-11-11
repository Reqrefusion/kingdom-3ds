/* Treeline.as */
#include "game.h"

static const char *TreelineImg = "/assets/gfx/treeline.png";

const As3Class Treeline_class = {"Treeline", &FlxSprite_class, NULL, {FLXSPRITE_VT}};

Treeline *Treeline_new(int32_t X, int32_t Y)
{
	Treeline *self = as3_alloc(sizeof *self);
	self->klass = &Treeline_class;
	SUPER(FlxSprite_ctor((FlxSprite *)self, X, Y, NULL));
	FlxSprite_loadGraphic((FlxSprite *)self, TreelineImg, false, true, 96, 160, false);
	if (X == 0)
		FlxSprite_set_facing((FlxSprite *)self, LEFT);
	return self;
}
