/* Minimap.as -- debug overlay, only created when PlayState.CHEATS is set. */
#include "game.h"

const As3Class Minimap_class = {
	"Minimap", &FlxSprite_class, NULL,
	{FLXSPRITE_VT, .draw = (void (*)(FlxBasic *))Minimap_draw},
};

Minimap *Minimap_new(Number X, Number Y, int32_t w, int32_t h)
{
	Minimap *self = as3_alloc(sizeof *self);
	self->klass = &Minimap_class;
	self->members = (As3Array){0};
	self->colors = (As3Array){0};
	SUPER(FlxSprite_ctor((FlxSprite *)self, X, Y, NULL));
	self->scrollFactor.x = self->scrollFactor.y = 0;
	FlxSprite_makeGraphic((FlxSprite *)self, (uint32_t)w, (uint32_t)h, 0, true, NULL);
	return self;
}

void Minimap_add(Minimap *self, FlxBasic *member, uint32_t color)
{
	As3Array_set(&self->members, self->members.length, member);
	As3Array_set(&self->colors, self->colors.length, (void *)(uintptr_t)color);
}

void Minimap_draw(Minimap *self)
{
	FlxSprite_fill((FlxSprite *)self, 0x55000000);

	for (int32_t i = 0; i < (int32_t)self->members.length; i++) {
		Minimap_drawDot(self, As3Array_get(&self->members, (uint32_t)i), (uint32_t)(uintptr_t)As3Array_get(&self->colors, (uint32_t)i));
	}
	self->dirty = true;
	SUPER(FlxSprite_draw((FlxSprite *)self));
}

void Minimap_drawDot(Minimap *self, FlxBasic *m, uint32_t color)
{
	if (as3_is(m, &FlxGroup_class)) {
		FlxGroup *group = (FlxGroup *)m;
		for (int32_t i = 0; i < (int32_t)group->length; i++) {
			Minimap_drawDot(self, As3Array_get(&group->members, (uint32_t)i), color);
		}
	} else if (as3_is(m, &FlxSprite_class)) {
		if (!m->alive || !m->visible) {
			return;
		}
		if (as3_is(m, &Wall_class) && ((Wall *)m)->stage == 0) {
			return;
		}
		FlxSprite *sprite = (FlxSprite *)m;
		int32_t ex = AS3_INT((sprite->x / FlxG.worldBounds.width) * self->width);
		int32_t ey = AS3_INT((sprite->y / FlxG.worldBounds.height) * self->height);
		BitmapData_setPixel32(FlxSprite_get_pixels((FlxSprite *)self), ex, ey, color);
	}
}
