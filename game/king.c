/* king.as: super(288,160,MenuState,3, 60, 60, false); FlxG.debug = true; */
#include "game.h"

static FlxState *new_MenuState(void)
{
	return (FlxState *)MenuState_new();
}

FlxGame *king_new(void)
{
	FlxGame *self = FlxGame_new(288, 160, new_MenuState, 3, 60, 60);
	FlxG.debug = true;
	return self;
}
