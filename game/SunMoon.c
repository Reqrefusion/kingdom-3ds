/* SunMoon.as */
#include "game.h"

static const char *Img = "/assets/gfx/sunmoon.png";
static const char *LightMidImg = "/assets/gfx/light_mid.png";
static const char *LightReflectWideImg = "/assets/gfx/light_reflect_wide.png";

const As3Class SunMoon_class = {
	"SunMoon", &Light_class, NULL,
	{LIGHT_VT, .update = (void (*)(FlxBasic *))SunMoon_update},
};

SunMoon *SunMoon_new(Weather *weather)
{
	SunMoon *self = as3_alloc(sizeof *self);
	self->klass = &SunMoon_class;
	self->weatherChanged = 0;
	(void)weather; /* the constructor argument is unused: Light reads playstate.weather */
	SUPER(Light_ctor((Light *)self, 0, 0));
	self->offset.x = self->offset.y = 16;
	self->scrollFactor.x = self->scrollFactor.y = 0.0;
	FlxSprite_loadGraphic((FlxSprite *)self, Img, true, false, 0, 0, false);
	FlxSprite_loadGraphic(self->beam, LightMidImg, false, false, 0, 0, false);
	FlxSprite_loadGraphic(self->reflected, LightReflectWideImg, false, false, 0, 0, false);
	FlxSprite_set_color(self->reflected, 0xFFfc8f53);
	self->beam->blend = "screen";
	return self;
}

void SunMoon_update(SunMoon *self)
{
	if (self->weather->changed > self->weatherChanged) {
		Number progressX = as3_mod(self->weather->timeOfDay * 2 + 0.5, 1);
		Number progressY = sin(Math_PI * progressX);
		self->x = self->width + (FlxG.width - 2 * self->width) * progressX;
		self->y = SunMoon_HORIZON - progressY * (SunMoon_HORIZON - SunMoon_ZENITH);
		FlxSprite_set_color((FlxSprite *)self, self->weather->sunTint);
		FlxSprite_set_alpha(self->beam, progressY * 2);
		FlxSprite_set_frame((FlxSprite *)self, (self->weather->timeOfDay > 0.25 && self->weather->timeOfDay < 0.75) ? 0 : 1);
		self->dirty = true;
		FlxSprite_drawFrame(self->beam, true);
		self->weatherChanged = self->weather->t;
	}
	SUPER(Light_update((Light *)self));
}
