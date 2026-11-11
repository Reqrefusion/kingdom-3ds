/* org/flixel/FlxBasic.as */
#include "flixel.h"

uint32_t FlxBasic__ACTIVECOUNT;
uint32_t FlxBasic__VISIBLECOUNT;

const As3Class FlxBasic_class = {
	"FlxBasic", NULL, NULL,
	{
		.destroy = FlxBasic_destroy,
		.preUpdate = FlxBasic_preUpdate,
		.update = FlxBasic_update,
		.postUpdate = FlxBasic_postUpdate,
		.draw = FlxBasic_draw,
		.kill = FlxBasic_kill,
		.revive = FlxBasic_revive,
	},
};

void FlxBasic_ctor(FlxBasic *self)
{
	self->ID = -1;
	self->exists = true;
	self->active = true;
	self->visible = true;
	self->alive = true;
	self->ignoreDrawDebug = false;
}

void FlxBasic_destroy(FlxBasic *self)
{
	(void)self;
}

void FlxBasic_preUpdate(FlxBasic *self)
{
	(void)self;
	FlxBasic__ACTIVECOUNT++;
}

void FlxBasic_update(FlxBasic *self)
{
	(void)self;
}

void FlxBasic_postUpdate(FlxBasic *self)
{
	(void)self;
}

void FlxBasic_draw(FlxBasic *self)
{
	if (self->cameras == NULL)
		self->cameras = &FlxG.cameras;
	FlxCamera *camera;
	uint32_t i = 0;
	uint32_t l = self->cameras->length;
	while (i < l) {
		camera = As3Array_get(self->cameras, i++);
		FlxBasic__VISIBLECOUNT++;
		if (FlxG.visualDebug && !self->ignoreDrawDebug)
			FlxBasic_drawDebug(self, camera);
	}
}

/* Debugger overlay only; the console build has no visual debugger. */
void FlxBasic_drawDebug(FlxBasic *self, FlxCamera *Camera)
{
	(void)self;
	(void)Camera;
}

void FlxBasic_kill(FlxBasic *self)
{
	self->alive = false;
	self->exists = false;
}

void FlxBasic_revive(FlxBasic *self)
{
	self->alive = true;
	self->exists = true;
}

char *FlxBasic_toString(FlxBasic *self)
{
	return as3_strdup(FlxU_getClassName(self, true));
}
