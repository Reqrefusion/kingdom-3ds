/* GameOverState.as
 * Not translated: the MochiScores online leaderboard (show/close and its
 * Mouse.show/hide callbacks); the console has no Mochi service. */
#include "game.h"

const As3Class GameOverState_class = {
	"GameOverState", &FlxState_class, NULL,
	{
		FLXSTATE_VT,
		.create = (void (*)(FlxBasic *))GameOverState_create,
		.update = (void (*)(FlxBasic *))GameOverState_update,
	},
};

GameOverState *GameOverState_new(int32_t nightsSurvived)
{
	GameOverState *self = as3_alloc(sizeof *self);
	self->klass = &GameOverState_class;
	self->nights = 0;
	SUPER(FlxState_ctor((FlxState *)self)); /* implicit super() */
	self->nights = nightsSurvived;
	return self;
}

void GameOverState_create(GameOverState *self)
{
	FlxText *t;
	t = FlxText_new(0, 10, FlxG.width, "Game Over", true);
	FlxText_set_size(t, 16);
	FlxText_set_alignment(t, "center");
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)t);
	t = FlxText_new(0, FlxG.height - 20.0, FlxG.width, "click to retry", true);
	FlxText_set_alignment(t, "center");
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)t);
	t = FlxText_new(0, 32, FlxG.width, "'Kingdom' by noio", true);
	FlxText_set_alignment(t, "center");
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)t);
	/* FlxG.stage.displayState = 'normal'; MochiScores.showLeaderboard(...) */
}

void GameOverState_update(GameOverState *self)
{
	SUPER(FlxGroup_update((FlxGroup *)self));
	if (FlxMouse_justPressed(FlxG.mouse)) {
		FlxMouse_hide(FlxG.mouse);
		FlxG_switchState((FlxState *)MenuState_new());
		/* MochiScores.closeLeaderboard() */
	}
}
