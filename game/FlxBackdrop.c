/* FlxBackdrop.as */
#include "game.h"

const As3Class FlxBackdrop_class = {
	"FlxBackdrop", &FlxSprite_class, NULL,
	{FLXSPRITE_VT, .update = (void (*)(FlxBasic *))FlxBackdrop_update},
};

FlxBackdrop *FlxBackdrop_new(const char *graphicClass, Number XScroll, Number YScroll, uint32_t Color)
{
	FlxBackdrop *self = as3_alloc(sizeof *self);
	self->klass = &FlxBackdrop_class;
	SUPER(FlxSprite_ctor((FlxSprite *)self, 0, 0, NULL)); /* implicit super() */
	BitmapData *graphic = FlxG_addBitmap(graphicClass, false, false, NULL);
	self->w = graphic->width;
	FlxSprite_makeGraphic((FlxSprite *)self, (uint32_t)(self->w + (int32_t)FlxG.width), (uint32_t)graphic->height, 0x00000000, true, NULL);
	self->y = ((Number)FlxG.height - graphic->height) / 2;
	Point p = Point_new(0, 0);
	BitmapData_copyPixels(FlxSprite_get_pixels((FlxSprite *)self), graphic, Rectangle_new(0, 0, graphic->width, graphic->height), p, false);
	p.x = self->w;
	BitmapData_copyPixels(FlxSprite_get_pixels((FlxSprite *)self), graphic, Rectangle_new(0, 0, graphic->width, graphic->height), p, false);
	self->dirty = true;
	self->scrollFactor.x = XScroll;
	self->scrollFactor.y = YScroll;
	FlxSprite_set_color((FlxSprite *)self, Color);
	return self;
}

void FlxBackdrop_update(FlxBackdrop *self)
{
	FlxObject_getScreenXY((FlxObject *)self, &self->_point, NULL);
	if (self->_point.x < -self->w) {
		self->x += self->w;
	} else if (self->_point.x > 0) {
		self->x -= self->w;
	}
	SUPER(FlxSprite_update(self));
}
