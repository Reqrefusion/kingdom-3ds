/* Shop.as */
#include "game.h"

static const char *Img = "/assets/gfx/shop.png";

const As3Class Shop_class = {
	"Shop", &FlxSprite_class, NULL,
	{FLXSPRITE_VT, .update = (void (*)(FlxBasic *))Shop_update},
};

Shop *Shop_new(int32_t X, int32_t Y)
{
	Shop *self = as3_alloc(sizeof *self);
	self->klass = &Shop_class;
	self->supply = 0;
	SUPER(FlxSprite_ctor((FlxSprite *)self, X, Y + 2, NULL));
	if (X > PlayState_GAME_WIDTH / 2) {
		self->type = Shop_BOWS;
	} else {
		self->type = Shop_SCYTHES;
	}
	FlxSprite_loadGraphic((FlxSprite *)self, Img, true, false, 0, 0, false);
	self->width = 56;
	self->offset.x = 4;
	self->x += 4;
	self->height = 46;
	self->offset.y = 18;
	self->y += 18;
	self->moves = false;
	Shop_updateAppearance(self);
	return self;
}

void Shop_update(Shop *self)
{
	if (self->supply > 0) {
		FlxG_overlap((FlxBasic *)self, (FlxBasic *)PLAYSTATE->characters, (As3Callback2)AS3_FN(self, Shop_equip), (As3Callback2)AS3_NULL_FN);
	}
}

bool Shop_equip(void *object, FlxObject *shop, FlxObject *chr)
{
	Shop *self = object;
	(void)shop;
	if (self->supply <= 0)
		return false;
	Citizen *cit = (Citizen *)chr;
	if (cit->occupation == Citizen_POOR) {
		self->supply--;
		if (self->type == Shop_BOWS) {
			Citizen_morph(cit, Citizen_HUNTER);
		} else {
			Citizen_morph(cit, Citizen_FARMER);
		}
		Shop_updateAppearance(self);
	}
	return false;
}

void Shop_setSupply(Shop *self, int32_t s)
{
	self->supply = s;
	Shop_updateAppearance(self);
}

void Shop_updateAppearance(Shop *self)
{
	FlxSprite_set_frame((FlxSprite *)self, (uint32_t)self->supply);
	if (self->type == Shop_BOWS) {
		FlxSprite_set_frame((FlxSprite *)self, FlxSprite_get_frame((FlxSprite *)self) + 5);
	}
}

bool Shop_canBuild(Shop *self)
{
	return (self->supply < 4);
}

void Shop_build(Shop *self)
{
	PLAYSTATE->boughtItem = true;
	self->supply += 1;
	FlxObject_flicker((FlxObject *)self, 0.3);
	Shop_updateAppearance(self);
}
