/* MenuState.as */
#include "game.h"

static const char *TitleImg = "/assets/gfx/title.png";
static const char *NoioImg = "/assets/gfx/outline_noio.png";
static const char *PezImg = "/assets/gfx/outline_pez.png";

static FlxBasic *MenuState_construct(void)
{
	return (FlxBasic *)MenuState_new();
}

const As3Class MenuState_class = {
	"MenuState", &FlxState_class, MenuState_construct,
	{
		FLXSTATE_VT,
		.create = (void (*)(FlxBasic *))MenuState_create,
		.update = (void (*)(FlxBasic *))MenuState_update,
	},
};

MenuState *MenuState_new(void)
{
	MenuState *self = as3_alloc(sizeof *self);
	self->klass = &MenuState_class;
	SUPER(FlxState_ctor((FlxState *)self));
	return self;
}

void MenuState_create(MenuState *self)
{
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)FlxSprite_new(0, 0, TitleImg));
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->noioHighlight = FlxSprite_new(228, 123, NoioImg)));
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->pezHighlight = FlxSprite_new(258, 123, PezImg)));
	self->noioHighlight->width = 30;
	self->noioHighlight->visible = false;
	self->pezHighlight->visible = false;
	FlxText *t = FlxText_new(0, 0, 100, king_VERSION, true);
	FlxText_set_alignment(t, "left");
	FlxSprite_set_alpha((FlxSprite *)t, 0.24);
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)t);
	FlxMouse_show(FlxG.mouse);
}

void MenuState_update(MenuState *self)
{
	SUPER(FlxGroup_update((FlxGroup *)self));
	if (FlxG.mouse->x > self->noioHighlight->x && FlxG.mouse->x < self->noioHighlight->x + self->noioHighlight->width &&
		FlxG.mouse->y > self->noioHighlight->y && FlxG.mouse->y < self->noioHighlight->y + self->noioHighlight->height) {
		self->noioHighlight->visible = true;
		if (FlxMouse_justPressed(FlxG.mouse)) {
			/* FlxU.openURL("http://www.noio.nl"): no browser on the console */
		}
	} else {
		self->noioHighlight->visible = false;
	}
	if (FlxG.mouse->x > self->pezHighlight->x && FlxG.mouse->x < self->pezHighlight->x + self->pezHighlight->width &&
		FlxG.mouse->y > self->pezHighlight->y && FlxG.mouse->y < self->pezHighlight->y + self->pezHighlight->height) {
		self->pezHighlight->visible = true;
		if (FlxMouse_justPressed(FlxG.mouse)) {
			/* FlxU.openURL("http://soundcloud.com/pez_pez") */
		}
	} else {
		self->pezHighlight->visible = false;
	}
	if (!self->noioHighlight->visible && !self->pezHighlight->visible && FlxMouse_justPressed(FlxG.mouse)) {
		FlxMouse_hide(FlxG.mouse);
		FlxG_switchState((FlxState *)PlayState_new(NULL));
	}
}
