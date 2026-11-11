/* CameraTarget.as */
#include "game.h"

const As3Class CameraTarget_class = {
	"CameraTarget", &FlxObject_class, NULL,
	{
		FLXOBJECT_VT,
		.update = (void (*)(FlxBasic *))CameraTarget_update,
		.draw = (void (*)(FlxBasic *))CameraTarget_draw,
	},
};

CameraTarget *CameraTarget_new(void)
{
	CameraTarget *self = as3_alloc(sizeof *self);
	self->klass = &CameraTarget_class;
	self->lead = 48;
	self->speed = FlxPoint_new(0.1, 0.1);
	self->maxSpeed = FlxPoint_new(20, 20);
	self->offset = FlxPoint_new(0, 0);
	SUPER(FlxObject_ctor((FlxObject *)self, 0, 0, 1, 1));
	return self;
}

void CameraTarget_set_target(CameraTarget *self, FlxSprite *object)
{
	self->_target = object;
}

FlxSprite *CameraTarget_get_target(CameraTarget *self)
{
	return self->_target;
}

void CameraTarget_snap(CameraTarget *self)
{
	self->x = self->_target->x + self->_target->width / 2 + self->offset.x;
	self->y = self->_target->y + self->_target->height / 2 + self->offset.y;
}

void CameraTarget_update(CameraTarget *self)
{
	if (self->_target == NULL)
		return;
	self->_targetX = self->_target->x + self->_target->width / 2 + self->offset.x;
	self->_targetY = self->_target->y + self->_target->height / 2 + self->offset.y;
	if (FlxSprite_get_facing(self->_target) == RIGHT) {
		self->_targetX += self->lead;
	} else if (FlxSprite_get_facing(self->_target) == LEFT) {
		self->_targetX -= self->lead;
	} else if (FlxSprite_get_facing(self->_target) == UP) {
		self->_targetY -= self->lead;
	} else if (FlxSprite_get_facing(self->_target) == DOWN) {
		self->_targetY += self->lead;
	}
	self->_targetX = (self->_targetX - self->x) * self->speed.x;
	self->_targetY = (self->_targetY - self->y) * self->speed.y;
	if (self->_targetX >= 1.0) {
		self->x += Math_min(self->maxSpeed.x, self->_targetX);
	} else if (self->_targetX <= -1.0) {
		self->x += Math_max(-self->maxSpeed.x, self->_targetX);
	}
	if (self->_targetY >= 1.0) {
		self->y += Math_min(self->maxSpeed.y, self->_targetY);
	} else if (self->_targetY <= 1.0) {
		self->y += Math_max(-self->maxSpeed.y, self->_targetY);
	}
}

void CameraTarget_draw(CameraTarget *self)
{
	(void)self;
}
