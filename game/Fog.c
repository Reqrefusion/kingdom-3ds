/* Fog.as */
#include "game.h"

static const char *FogImg = "/assets/gfx/fog.png";

const As3Class Fog_class = {
	"Fog", &FlxGroup_class, NULL,
	{FLXGROUP_VT, .update = (void (*)(FlxBasic *))Fog_update},
};

Fog *Fog_new(Weather *weather)
{
	Fog *self = as3_alloc(sizeof *self);
	self->klass = &Fog_class;
	self->weatherChanged = -1;
	self->_point = FlxPoint_new(0, 0);
	SUPER(FlxGroup_ctor((FlxGroup *)self, Fog_MAXFOG));
	self->weather = weather;
	for (int32_t i = 0; i < Fog_MAXFOG; i++) {
		self->_fg = FlxSprite_loadGraphic(FlxSprite_new(0, 0, NULL), FogImg, true, true, 256, 96, false);
		self->_fg->scrollFactor.y = 1.2;
		self->_fg->scrollFactor.x = (FlxG_random() < 0.5) ? 1.5 : 2.5;
		FlxSprite_set_facing(self->_fg, (FlxG_random() < 0.5) ? LEFT : RIGHT);
		FlxSprite_set_frame(self->_fg, AS3_UINT(AS3_INT(FlxG_random() * 4)));
		CALL0(self->_fg, kill);
		FlxGroup_add((FlxGroup *)self, (FlxBasic *)self->_fg);
	}
	return self;
}

void Fog_update(Fog *self)
{
	for (int32_t i = 0; (uint32_t)i < self->members.length; i++) {
		self->_fg = As3Array_get(&self->members, (uint32_t)i);
		if (self->_fg->exists) {
			FlxObject_getScreenXY((FlxObject *)self->_fg, &self->_point, NULL);
			if (self->_point.x + self->_fg->width < -100 || self->_point.x > FlxG.width + 100) {
				CALL0(self->_fg, kill);
			} else {
				self->_fg->velocity.x = -self->weather->wind * 20;
			}
		}
	}
	if (self->weather->changed > self->weatherChanged) {
		if (FlxGroup_countLiving((FlxGroup *)self) < Fog_MAXFOG * self->weather->fog) {
			self->_fg = (FlxSprite *)FlxGroup_getFirstAvailable((FlxGroup *)self, NULL);
			CALL(self->_fg, reset, 0, 0);
			if (FlxG_random() < 0.5) {
				self->_fg->x = FlxG.camera->scroll.x * self->_fg->scrollFactor.x - self->_fg->width;
			} else {
				self->_fg->x = (FlxG.camera->scroll.x) * self->_fg->scrollFactor.x + FlxG.width;
			}
			self->_fg->y = 112 + 50 * FlxG_random();
			uint32_t comp = AS3_UINT((1 - self->weather->darkness) * 255);
			uint32_t color = comp << 16 | comp << 8 | comp;
			FlxSprite_set_color(self->_fg, color);
			FlxSprite_set_alpha(self->_fg, self->weather->fog / 6 + 0.3);
		}
		self->weatherChanged = self->weather->t;
	}
	SUPER(FlxGroup_update((FlxGroup *)self));
}
