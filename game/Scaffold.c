/* Scaffold.as */
#include "game.h"

static const char *Img = "/assets/gfx/scaffold.png";

const As3Class Scaffold_class = {"Scaffold", &FlxSprite_class, (FlxBasic * (*)(void)) Scaffold_new, {FLXSPRITE_VT}};

Scaffold *Scaffold_new(void)
{
	Scaffold *self = as3_alloc(sizeof *self);
	self->klass = &Scaffold_class;
	SUPER(FlxSprite_ctor((FlxSprite *)self, 0, 0, NULL));
	FlxSprite_loadGraphic((FlxSprite *)self, Img, false, false, 0, 0, false);
	self->offset.x = 4;
	self->width = 24;
	return self;
}

Scaffold *Scaffold_build(Scaffold *self, FlxSprite *over)
{
	CALL0(self, revive);
	self->x = over->x;
	self->y = over->y;
	return self;
}
