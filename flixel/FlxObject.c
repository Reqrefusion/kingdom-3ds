/* org/flixel/FlxObject.as
 * Not translated: drawDebug (debugger overlay), followPath/stopFollowingPath/
 * advancePath/updatePathMotion (FlxPath is never used by Kingdom; `path`
 * always stays null), overlaps against FlxTilemap (Kingdom's floor tilemap
 * is never passed to overlaps()). */
#include "flixel.h"

const As3Class FlxObject_class = {
	"FlxObject", &FlxBasic_class, NULL,
	{
		.destroy = (void (*)(FlxBasic *))FlxObject_destroy,
		.preUpdate = (void (*)(FlxBasic *))FlxObject_preUpdate,
		.update = FlxBasic_update,
		.postUpdate = (void (*)(FlxBasic *))FlxObject_postUpdate,
		.draw = (void (*)(FlxBasic *))FlxObject_draw,
		.kill = FlxBasic_kill,
		.revive = FlxBasic_revive,
		.reset = (void (*)(FlxBasic *, Number, Number))FlxObject_reset,
		.onScreen = (bool (*)(FlxBasic *, FlxCamera *))FlxObject_onScreen,
		.hurt = (void (*)(FlxBasic *, Number))FlxObject_hurt,
	},
};

void FlxObject_ctor(FlxObject *self, Number X, Number Y, Number Width, Number Height)
{
	SUPER(FlxBasic_ctor((FlxBasic *)self));

	self->x = X;
	self->y = Y;
	self->last = FlxPoint_new(self->x, self->y);
	self->width = Width;
	self->height = Height;
	self->mass = 1.0;
	self->elasticity = 0.0;

	self->health = 1;

	self->immovable = false;
	self->moves = true;

	self->touching = NONE;
	self->wasTouching = NONE;
	self->allowCollisions = ANY;

	self->velocity = FlxPoint_new(0, 0);
	self->acceleration = FlxPoint_new(0, 0);
	self->drag = FlxPoint_new(0, 0);
	self->maxVelocity = FlxPoint_new(10000, 10000);

	self->angle = 0;
	self->angularVelocity = 0;
	self->angularAcceleration = 0;
	self->angularDrag = 0;
	self->maxAngular = 10000;

	self->scrollFactor = FlxPoint_new(1.0, 1.0);
	self->_flicker = false;
	self->_flickerTimer = 0;

	self->_point = FlxPoint_new(0, 0);
	self->_rect = FlxRect_new(0, 0, 0, 0);

	self->path = NULL;
	self->pathSpeed = 0;
	self->pathAngle = 0;
}

void FlxObject_destroy(FlxObject *self)
{
	self->cameras = NULL;
	self->path = NULL;
}

void FlxObject_preUpdate(FlxObject *self)
{
	FlxBasic__ACTIVECOUNT++;

	if (self->_flickerTimer != 0) {
		if (self->_flickerTimer > 0) {
			self->_flickerTimer = self->_flickerTimer - FlxG.elapsed;
			if (self->_flickerTimer <= 0) {
				self->_flickerTimer = 0;
				self->_flicker = false;
			}
		}
	}

	self->last.x = self->x;
	self->last.y = self->y;

	/* (path != null) && (pathSpeed != 0) && ... -> updatePathMotion():
	   path is always null in Kingdom. */
}

void FlxObject_postUpdate(FlxObject *self)
{
	if (self->moves)
		FlxObject_updateMotion(self);

	self->wasTouching = self->touching;
	self->touching = NONE;
}

void FlxObject_updateMotion(FlxObject *self)
{
	Number delta;
	Number velocityDelta;

	velocityDelta = (FlxU_computeVelocity(self->angularVelocity, self->angularAcceleration, self->angularDrag, self->maxAngular) - self->angularVelocity) / 2;
	self->angularVelocity += velocityDelta;
	self->angle += self->angularVelocity * FlxG.elapsed;
	self->angularVelocity += velocityDelta;

	velocityDelta = (FlxU_computeVelocity(self->velocity.x, self->acceleration.x, self->drag.x, self->maxVelocity.x) - self->velocity.x) / 2;
	self->velocity.x += velocityDelta;
	delta = self->velocity.x * FlxG.elapsed;
	self->velocity.x += velocityDelta;
	self->x += delta;

	velocityDelta = (FlxU_computeVelocity(self->velocity.y, self->acceleration.y, self->drag.y, self->maxVelocity.y) - self->velocity.y) / 2;
	self->velocity.y += velocityDelta;
	delta = self->velocity.y * FlxG.elapsed;
	self->velocity.y += velocityDelta;
	self->y += delta;
}

void FlxObject_draw(FlxObject *self)
{
	if (self->cameras == NULL)
		self->cameras = &FlxG.cameras;
	FlxCamera *camera;
	uint32_t i = 0;
	uint32_t l = self->cameras->length;
	while (i < l) {
		camera = As3Array_get(self->cameras, i++);
		if (!CALL(self, onScreen, camera))
			continue;
		FlxBasic__VISIBLECOUNT++;
		if (FlxG.visualDebug && !self->ignoreDrawDebug)
			FlxBasic_drawDebug((FlxBasic *)self, camera);
	}
}

bool FlxObject_overlaps(FlxObject *self, FlxBasic *ObjectOrGroup, bool InScreenSpace, FlxCamera *Camera)
{
	if (as3_is(ObjectOrGroup, &FlxGroup_class)) {
		bool results = false;
		uint32_t i = 0;
		As3Array *members = &((FlxGroup *)ObjectOrGroup)->members;
		/* Source bug kept visible: the loop bound is an unqualified
		   `length`, which FlxObject does not define. Kingdom never passes a
		   group here, so the loop is treated as never running. */
		Number length = NAN;
		while (i < length) {
			if (FlxObject_overlaps(self, As3Array_get(members, i++), InScreenSpace, Camera))
				results = true;
		}
		return results;
	}

	FlxObject *object = (FlxObject *)ObjectOrGroup;
	if (!InScreenSpace) {
		return (object->x + object->width > self->x) && (object->x < self->x + self->width) &&
			(object->y + object->height > self->y) && (object->y < self->y + self->height);
	}

	if (Camera == NULL)
		Camera = FlxG.camera;
	FlxPoint objectScreenPos_value;
	FlxPoint *objectScreenPos = FlxObject_getScreenXY(object, &objectScreenPos_value, Camera);
	FlxObject_getScreenXY(self, &self->_point, Camera);
	return (objectScreenPos->x + object->width > self->_point.x) && (objectScreenPos->x < self->_point.x + self->width) &&
		(objectScreenPos->y + object->height > self->_point.y) && (objectScreenPos->y < self->_point.y + self->height);
}

bool FlxObject_overlapsAt(FlxObject *self, Number X, Number Y, FlxBasic *ObjectOrGroup, bool InScreenSpace, FlxCamera *Camera)
{
	if (as3_is(ObjectOrGroup, &FlxGroup_class)) {
		bool results = false;
		uint32_t i = 0;
		As3Array *members = &((FlxGroup *)ObjectOrGroup)->members;
		Number length = NAN; /* see FlxObject_overlaps */
		while (i < length) {
			if (FlxObject_overlapsAt(self, X, Y, As3Array_get(members, i++), InScreenSpace, Camera))
				results = true;
		}
		return results;
	}

	FlxObject *object = (FlxObject *)ObjectOrGroup;
	if (!InScreenSpace) {
		return (object->x + object->width > X) && (object->x < X + self->width) &&
			(object->y + object->height > Y) && (object->y < Y + self->height);
	}

	if (Camera == NULL)
		Camera = FlxG.camera;
	FlxPoint objectScreenPos_value;
	FlxPoint *objectScreenPos = FlxObject_getScreenXY(object, &objectScreenPos_value, Camera);
	self->_point.x = X - AS3_INT(Camera->scroll.x * self->scrollFactor.x);
	self->_point.y = Y - AS3_INT(Camera->scroll.y * self->scrollFactor.y);
	self->_point.x += (self->_point.x > 0) ? 0.0000001 : -0.0000001;
	self->_point.y += (self->_point.y > 0) ? 0.0000001 : -0.0000001;
	return (objectScreenPos->x + object->width > self->_point.x) && (objectScreenPos->x < self->_point.x + self->width) &&
		(objectScreenPos->y + object->height > self->_point.y) && (objectScreenPos->y < self->_point.y + self->height);
}

bool FlxObject_overlapsPoint(FlxObject *self, FlxPoint *Point, bool InScreenSpace, FlxCamera *Camera)
{
	if (!InScreenSpace)
		return (Point->x > self->x) && (Point->x < self->x + self->width) && (Point->y > self->y) && (Point->y < self->y + self->height);

	if (Camera == NULL)
		Camera = FlxG.camera;
	Number X = Point->x - Camera->scroll.x;
	Number Y = Point->y - Camera->scroll.y;
	FlxObject_getScreenXY(self, &self->_point, Camera);
	return (X > self->_point.x) && (X < self->_point.x + self->width) && (Y > self->_point.y) && (Y < self->_point.y + self->height);
}

bool FlxObject_onScreen(FlxObject *self, FlxCamera *Camera)
{
	if (Camera == NULL)
		Camera = FlxG.camera;
	FlxObject_getScreenXY(self, &self->_point, Camera);
	return (self->_point.x + self->width > 0) && (self->_point.x < Camera->width) && (self->_point.y + self->height > 0) && (self->_point.y < Camera->height);
}

FlxPoint *FlxObject_getScreenXY(FlxObject *self, FlxPoint *Point, FlxCamera *Camera)
{
	if (Point == NULL)
		Point = FlxPoint_alloc(0, 0);
	if (Camera == NULL)
		Camera = FlxG.camera;
	Point->x = self->x - AS3_INT(Camera->scroll.x * self->scrollFactor.x);
	Point->y = self->y - AS3_INT(Camera->scroll.y * self->scrollFactor.y);
	Point->x += (Point->x > 0) ? 0.0000001 : -0.0000001;
	Point->y += (Point->y > 0) ? 0.0000001 : -0.0000001;
	return Point;
}

void FlxObject_flicker(FlxObject *self, Number Duration)
{
	self->_flickerTimer = Duration;
	if (self->_flickerTimer == 0)
		self->_flicker = false;
}

bool FlxObject_get_flickering(FlxObject *self)
{
	return self->_flickerTimer != 0;
}

bool FlxObject_get_solid(FlxObject *self)
{
	return (self->allowCollisions & ANY) > NONE;
}

void FlxObject_set_solid(FlxObject *self, bool Solid)
{
	if (Solid)
		self->allowCollisions = ANY;
	else
		self->allowCollisions = NONE;
}

FlxPoint *FlxObject_getMidpoint(FlxObject *self, FlxPoint *Point)
{
	if (Point == NULL)
		Point = FlxPoint_alloc(0, 0);
	Point->x = self->x + self->width * 0.5;
	Point->y = self->y + self->height * 0.5;
	return Point;
}

void FlxObject_reset(FlxObject *self, Number X, Number Y)
{
	CALL0(self, revive);
	self->touching = NONE;
	self->wasTouching = NONE;
	self->x = X;
	self->y = Y;
	self->last.x = self->x;
	self->last.y = self->y;
	self->velocity.x = 0;
	self->velocity.y = 0;
}

bool FlxObject_isTouching(FlxObject *self, uint32_t Direction)
{
	return (self->touching & Direction) > NONE;
}

bool FlxObject_justTouched(FlxObject *self, uint32_t Direction)
{
	return ((self->touching & Direction) > NONE) && ((self->wasTouching & Direction) <= NONE);
}

void FlxObject_hurt(FlxObject *self, Number Damage)
{
	self->health = self->health - Damage;
	if (self->health <= 0)
		CALL0(self, kill);
}

bool FlxObject_separate(FlxObject *Object1, FlxObject *Object2)
{
	bool separatedX = FlxObject_separateX(Object1, Object2);
	bool separatedY = FlxObject_separateY(Object1, Object2);
	return separatedX || separatedY;
}

bool FlxObject_separate_fn(void *unused, FlxObject *Object1, FlxObject *Object2)
{
	(void)unused;
	return FlxObject_separate(Object1, Object2);
}

bool FlxObject_separateX(FlxObject *Object1, FlxObject *Object2)
{
	/* can't separate two immovable objects */
	bool obj1immovable = Object1->immovable;
	bool obj2immovable = Object2->immovable;
	if (obj1immovable && obj2immovable)
		return false;

	/* If one of the objects is a tilemap, just pass it off: Kingdom never
	   collides against its FlxTilemap (the floor is decoration only). */

	/* First, get the two object deltas */
	Number overlap = 0;
	Number obj1delta = Object1->x - Object1->last.x;
	Number obj2delta = Object2->x - Object2->last.x;
	if (obj1delta != obj2delta) {
		/* Check if the X hulls actually overlap */
		Number obj1deltaAbs = (obj1delta > 0) ? obj1delta : -obj1delta;
		Number obj2deltaAbs = (obj2delta > 0) ? obj2delta : -obj2delta;
		FlxRect obj1rect = FlxRect_new(Object1->x - ((obj1delta > 0) ? obj1delta : 0), Object1->last.y, Object1->width + ((obj1delta > 0) ? obj1delta : -obj1delta), Object1->height);
		FlxRect obj2rect = FlxRect_new(Object2->x - ((obj2delta > 0) ? obj2delta : 0), Object2->last.y, Object2->width + ((obj2delta > 0) ? obj2delta : -obj2delta), Object2->height);
		if ((obj1rect.x + obj1rect.width > obj2rect.x) && (obj1rect.x < obj2rect.x + obj2rect.width) && (obj1rect.y + obj1rect.height > obj2rect.y) && (obj1rect.y < obj2rect.y + obj2rect.height)) {
			Number maxOverlap = obj1deltaAbs + obj2deltaAbs + FlxObject_OVERLAP_BIAS;

			/* If they did overlap (and can), figure out by how much and flip the corresponding flags */
			if (obj1delta > obj2delta) {
				overlap = Object1->x + Object1->width - Object2->x;
				if ((overlap > maxOverlap) || !(Object1->allowCollisions & RIGHT) || !(Object2->allowCollisions & LEFT))
					overlap = 0;
				else {
					Object1->touching |= RIGHT;
					Object2->touching |= LEFT;
				}
			} else if (obj1delta < obj2delta) {
				overlap = Object1->x - Object2->width - Object2->x;
				if ((-overlap > maxOverlap) || !(Object1->allowCollisions & LEFT) || !(Object2->allowCollisions & RIGHT))
					overlap = 0;
				else {
					Object1->touching |= LEFT;
					Object2->touching |= RIGHT;
				}
			}
		}
	}

	/* Then adjust their positions and velocities accordingly (if there was any overlap) */
	if (overlap != 0) {
		Number obj1v = Object1->velocity.x;
		Number obj2v = Object2->velocity.x;

		if (!obj1immovable && !obj2immovable) {
			overlap *= 0.5;
			Object1->x = Object1->x - overlap;
			Object2->x += overlap;

			Number obj1velocity = Math_sqrt((obj2v * obj2v * Object2->mass) / Object1->mass) * ((obj2v > 0) ? 1 : -1);
			Number obj2velocity = Math_sqrt((obj1v * obj1v * Object1->mass) / Object2->mass) * ((obj1v > 0) ? 1 : -1);
			Number average = (obj1velocity + obj2velocity) * 0.5;
			obj1velocity -= average;
			obj2velocity -= average;
			Object1->velocity.x = average + obj1velocity * Object1->elasticity;
			Object2->velocity.x = average + obj2velocity * Object2->elasticity;
		} else if (!obj1immovable) {
			Object1->x = Object1->x - overlap;
			Object1->velocity.x = obj2v - obj1v * Object1->elasticity;
		} else if (!obj2immovable) {
			Object2->x += overlap;
			Object2->velocity.x = obj1v - obj2v * Object2->elasticity;
		}
		return true;
	} else
		return false;
}

bool FlxObject_separateY(FlxObject *Object1, FlxObject *Object2)
{
	/* can't separate two immovable objects */
	bool obj1immovable = Object1->immovable;
	bool obj2immovable = Object2->immovable;
	if (obj1immovable && obj2immovable)
		return false;

	/* If one of the objects is a tilemap, just pass it off: see separateX. */

	/* First, get the two object deltas */
	Number overlap = 0;
	Number obj1delta = Object1->y - Object1->last.y;
	Number obj2delta = Object2->y - Object2->last.y;
	if (obj1delta != obj2delta) {
		/* Check if the Y hulls actually overlap */
		Number obj1deltaAbs = (obj1delta > 0) ? obj1delta : -obj1delta;
		Number obj2deltaAbs = (obj2delta > 0) ? obj2delta : -obj2delta;
		FlxRect obj1rect = FlxRect_new(Object1->x, Object1->y - ((obj1delta > 0) ? obj1delta : 0), Object1->width, Object1->height + obj1deltaAbs);
		FlxRect obj2rect = FlxRect_new(Object2->x, Object2->y - ((obj2delta > 0) ? obj2delta : 0), Object2->width, Object2->height + obj2deltaAbs);
		if ((obj1rect.x + obj1rect.width > obj2rect.x) && (obj1rect.x < obj2rect.x + obj2rect.width) && (obj1rect.y + obj1rect.height > obj2rect.y) && (obj1rect.y < obj2rect.y + obj2rect.height)) {
			Number maxOverlap = obj1deltaAbs + obj2deltaAbs + FlxObject_OVERLAP_BIAS;

			/* If they did overlap (and can), figure out by how much and flip the corresponding flags */
			if (obj1delta > obj2delta) {
				overlap = Object1->y + Object1->height - Object2->y;
				if ((overlap > maxOverlap) || !(Object1->allowCollisions & DOWN) || !(Object2->allowCollisions & UP))
					overlap = 0;
				else {
					Object1->touching |= DOWN;
					Object2->touching |= UP;
				}
			} else if (obj1delta < obj2delta) {
				overlap = Object1->y - Object2->height - Object2->y;
				if ((-overlap > maxOverlap) || !(Object1->allowCollisions & UP) || !(Object2->allowCollisions & DOWN))
					overlap = 0;
				else {
					Object1->touching |= UP;
					Object2->touching |= DOWN;
				}
			}
		}
	}

	/* Then adjust their positions and velocities accordingly (if there was any overlap) */
	if (overlap != 0) {
		Number obj1v = Object1->velocity.y;
		Number obj2v = Object2->velocity.y;

		if (!obj1immovable && !obj2immovable) {
			overlap *= 0.5;
			Object1->y = Object1->y - overlap;
			Object2->y += overlap;

			Number obj1velocity = Math_sqrt((obj2v * obj2v * Object2->mass) / Object1->mass) * ((obj2v > 0) ? 1 : -1);
			Number obj2velocity = Math_sqrt((obj1v * obj1v * Object1->mass) / Object2->mass) * ((obj1v > 0) ? 1 : -1);
			Number average = (obj1velocity + obj2velocity) * 0.5;
			obj1velocity -= average;
			obj2velocity -= average;
			Object1->velocity.y = average + obj1velocity * Object1->elasticity;
			Object2->velocity.y = average + obj2velocity * Object2->elasticity;
		} else if (!obj1immovable) {
			Object1->y = Object1->y - overlap;
			Object1->velocity.y = obj2v - obj1v * Object1->elasticity;
			/* This is special case code that handles cases like horizontal moving platforms you can ride */
			if (Object2->active && Object2->moves && (obj1delta > obj2delta))
				Object1->x += Object2->x - Object2->last.x;
		} else if (!obj2immovable) {
			Object2->y += overlap;
			Object2->velocity.y = obj1v - obj2v * Object2->elasticity;
			/* This is special case code that handles cases like horizontal moving platforms you can ride */
			if (Object1->active && Object1->moves && (obj1delta < obj2delta))
				Object2->x += Object1->x - Object1->last.x;
		}
		return true;
	} else
		return false;
}
