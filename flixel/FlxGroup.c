/* org/flixel/FlxGroup.as, org/flixel/FlxParticle.as, org/flixel/FlxState.as
 * Not translated: setAll (reflection by property name; unused by Kingdom).
 * callAll and sort() take the reflected name as an explicit function. */
#include "flixel.h"

/* ---- FlxGroup ----------------------------------------------------------- */

const As3Class FlxGroup_class = {
	"FlxGroup", &FlxBasic_class, NULL,
	{
		.destroy = (void (*)(FlxBasic *))FlxGroup_destroy,
		.preUpdate = (void (*)(FlxBasic *))FlxGroup_preUpdate,
		.update = (void (*)(FlxBasic *))FlxGroup_update,
		.postUpdate = FlxBasic_postUpdate,
		.draw = (void (*)(FlxBasic *))FlxGroup_draw,
		.kill = (void (*)(FlxBasic *))FlxGroup_kill,
		.revive = FlxBasic_revive,
	},
};

FlxGroup *FlxGroup_new(uint32_t MaxSize)
{
	FlxGroup *self = as3_alloc(sizeof *self);
	self->klass = &FlxGroup_class;
	FlxGroup_ctor(self, MaxSize);
	return self;
}

void FlxGroup_ctor(FlxGroup *self, uint32_t MaxSize)
{
	SUPER(FlxBasic_ctor((FlxBasic *)self));
	self->members = (As3Array){0};
	self->length = 0;
	self->_maxSize = MaxSize;
	self->_marker = 0;
	self->_sortIndex = NULL;
}

void FlxGroup_destroy(FlxGroup *self)
{
	FlxBasic *basic;
	uint32_t i = 0;
	while (i < self->length) {
		basic = As3Array_get(&self->members, i++);
		if (basic != NULL)
			CALL0(basic, destroy);
	}
	As3Array_set_length(&self->members, 0);
	self->_sortIndex = NULL;
}

void FlxGroup_preUpdate(FlxGroup *self)
{
	(void)self;
}

/* Test-only probe (NULL in the game): called when the current state's
   super.update() starts, i.e. after PlayState.update ran its collisions and
   before the members update. Lets scenario tests sample mid-frame state. */
void (*FlxGroup_stateUpdateProbe)(void);

void FlxGroup_update(FlxGroup *self)
{
	FlxBasic *basic;
	uint32_t i = 0;
	if (FlxGroup_stateUpdateProbe != NULL && (FlxBasic *)self == (FlxBasic *)FlxG.state)
		FlxGroup_stateUpdateProbe();
	while (i < self->length) {
		basic = As3Array_get(&self->members, i++);
		if ((basic != NULL) && basic->exists && basic->active) {
			CALL0(basic, preUpdate);
			CALL0(basic, update);
			CALL0(basic, postUpdate);
		}
	}
}

void FlxGroup_draw(FlxGroup *self)
{
	FlxBasic *basic;
	uint32_t i = 0;
	while (i < self->length) {
		basic = As3Array_get(&self->members, i++);
		if ((basic != NULL) && basic->exists && basic->visible)
			CALL0(basic, draw);
	}
}

uint32_t FlxGroup_get_maxSize(FlxGroup *self)
{
	return self->_maxSize;
}

void FlxGroup_set_maxSize(FlxGroup *self, uint32_t Size)
{
	self->_maxSize = Size;
	if (self->_marker >= self->_maxSize)
		self->_marker = 0;
	if ((self->_maxSize == 0) || (self->_maxSize >= self->members.length))
		return;

	/* If the max size has shrunk, we need to get rid of some objects */
	FlxBasic *basic;
	uint32_t i = self->_maxSize;
	uint32_t l = self->members.length;
	while (i < l) {
		basic = As3Array_get(&self->members, i++);
		if (basic != NULL)
			CALL0(basic, destroy);
	}
	As3Array_set_length(&self->members, self->_maxSize);
	self->length = self->_maxSize;
}

FlxBasic *FlxGroup_add(FlxGroup *self, FlxBasic *Object)
{
	/* Don't bother adding an object twice. */
	if (As3Array_indexOf(&self->members, Object) >= 0)
		return Object;

	/* First, look for a null entry where we can add the object. */
	int32_t i = 0;
	uint32_t l = self->members.length;
	for (i = 0; (uint32_t)i < l; i++) {
		if (As3Array_get(&self->members, (uint32_t)i) == NULL) {
			As3Array_set(&self->members, (uint32_t)i, Object);
			if (i >= self->length)
				self->length = i + 1;
			return Object;
		}
	}

	/* Failing that, expand the array (if we can) and add the object. */
	if (self->_maxSize > 0) {
		if (self->members.length >= self->_maxSize)
			return Object;
		else if (self->members.length * 2 <= self->_maxSize)
			As3Array_set_length(&self->members, self->members.length * 2);
		else
			As3Array_set_length(&self->members, self->_maxSize);
	} else
		As3Array_set_length(&self->members, self->members.length * 2);

	/* If we made it this far, then we successfully grew the group,
	   and we can go ahead and add the object at the first open slot. */
	As3Array_set(&self->members, (uint32_t)i, Object);
	self->length = i + 1;
	return Object;
}

FlxBasic *FlxGroup_recycle(FlxGroup *self, const As3Class *ObjectClass)
{
	FlxBasic *basic;
	if (self->_maxSize > 0) {
		if (self->length < self->_maxSize) {
			if (ObjectClass == NULL)
				return NULL;
			return FlxGroup_add(self, ObjectClass->construct());
		} else {
			basic = As3Array_get(&self->members, self->_marker++);
			if (self->_marker >= self->_maxSize)
				self->_marker = 0;
			return basic;
		}
	} else {
		basic = FlxGroup_getFirstAvailable(self, ObjectClass);
		if (basic != NULL)
			return basic;
		if (ObjectClass == NULL)
			return NULL;
		return FlxGroup_add(self, ObjectClass->construct());
	}
}

FlxBasic *FlxGroup_remove(FlxGroup *self, FlxBasic *Object, bool Splice)
{
	int32_t index = As3Array_indexOf(&self->members, Object);
	if ((index < 0) || ((uint32_t)index >= self->members.length))
		return NULL;
	if (Splice) {
		As3Array_splice1(&self->members, (uint32_t)index);
		self->length--;
	} else
		As3Array_set(&self->members, (uint32_t)index, NULL);
	return Object;
}

FlxBasic *FlxGroup_replace(FlxGroup *self, FlxBasic *OldObject, FlxBasic *NewObject)
{
	int32_t index = As3Array_indexOf(&self->members, OldObject);
	if ((index < 0) || ((uint32_t)index >= self->members.length))
		return NULL;
	As3Array_set(&self->members, (uint32_t)index, NewObject);
	return NewObject;
}

static FlxGroup *sort_group;

static int FlxGroup_sortHandler_qsort(const void *a, const void *b);

void FlxGroup_sort(FlxGroup *self, const char *Index, int32_t Order)
{
	self->_sortIndex = Index;
	self->_sortOrder = Order;
	/* members.sort(sortHandler)
	   ADAPTATION: Array.sort in Flash is not stable either, but its exact
	   ordering of equal keys differs from qsort's. */
	sort_group = self;
	qsort(self->members.items, self->members.length, sizeof(void *), FlxGroup_sortHandler_qsort);
}

static int32_t FlxGroup_sortHandler(FlxGroup *self, FlxBasic *Obj1, FlxBasic *Obj2)
{
	if (FlxGroup_sortValue(Obj1, self->_sortIndex) < FlxGroup_sortValue(Obj2, self->_sortIndex))
		return self->_sortOrder;
	else if (FlxGroup_sortValue(Obj1, self->_sortIndex) > FlxGroup_sortValue(Obj2, self->_sortIndex))
		return -self->_sortOrder;
	return 0;
}

static int FlxGroup_sortHandler_qsort(const void *a, const void *b)
{
	FlxBasic *Obj1 = *(FlxBasic *const *)a, *Obj2 = *(FlxBasic *const *)b;
	/* AS3 sorts undefined/null entries to the end. */
	if (Obj1 == NULL || Obj2 == NULL)
		return (Obj1 == NULL) - (Obj2 == NULL);
	return FlxGroup_sortHandler(sort_group, Obj1, Obj2);
}

Number FlxGroup_sortValue(FlxBasic *Obj, const char *Index)
{
	if (as3_streq(Index, "y") && as3_is(Obj, &FlxObject_class))
		return ((FlxObject *)Obj)->y;
	if (as3_streq(Index, "x") && as3_is(Obj, &FlxObject_class))
		return ((FlxObject *)Obj)->x;
	return 0;
}

FlxBasic *FlxGroup_getFirstAvailable(FlxGroup *self, const As3Class *ObjectClass)
{
	FlxBasic *basic;
	uint32_t i = 0;
	while (i < self->length) {
		basic = As3Array_get(&self->members, i++);
		if ((basic != NULL) && !basic->exists && ((ObjectClass == NULL) || as3_is(basic, ObjectClass)))
			return basic;
	}
	return NULL;
}

int32_t FlxGroup_getFirstNull(FlxGroup *self)
{
	uint32_t i = 0;
	uint32_t l = self->members.length;
	while (i < l) {
		if (As3Array_get(&self->members, i) == NULL)
			return (int32_t)i;
		else
			i++;
	}
	return -1;
}

FlxBasic *FlxGroup_getFirstExtant(FlxGroup *self)
{
	FlxBasic *basic;
	uint32_t i = 0;
	while (i < self->length) {
		basic = As3Array_get(&self->members, i++);
		if ((basic != NULL) && basic->exists)
			return basic;
	}
	return NULL;
}

FlxBasic *FlxGroup_getFirstAlive(FlxGroup *self)
{
	FlxBasic *basic;
	uint32_t i = 0;
	while (i < self->length) {
		basic = As3Array_get(&self->members, i++);
		if ((basic != NULL) && basic->exists && basic->alive)
			return basic;
	}
	return NULL;
}

FlxBasic *FlxGroup_getFirstDead(FlxGroup *self)
{
	FlxBasic *basic;
	uint32_t i = 0;
	while (i < self->length) {
		basic = As3Array_get(&self->members, i++);
		if ((basic != NULL) && !basic->alive)
			return basic;
	}
	return NULL;
}

int32_t FlxGroup_countLiving(FlxGroup *self)
{
	int32_t count = -1;
	FlxBasic *basic;
	uint32_t i = 0;
	while (i < self->length) {
		basic = As3Array_get(&self->members, i++);
		if (basic != NULL) {
			if (count < 0)
				count = 0;
			if (basic->exists && basic->alive)
				count++;
		}
	}
	return count;
}

int32_t FlxGroup_countDead(FlxGroup *self)
{
	int32_t count = -1;
	FlxBasic *basic;
	uint32_t i = 0;
	while (i < self->length) {
		basic = As3Array_get(&self->members, i++);
		if (basic != NULL) {
			if (count < 0)
				count = 0;
			if (!basic->alive)
				count++;
		}
	}
	return count;
}

FlxBasic *FlxGroup_getRandom(FlxGroup *self, uint32_t StartIndex, uint32_t Length)
{
	if (Length == 0)
		Length = AS3_UINT(self->length);
	return FlxG_getRandom(&self->members, StartIndex, Length);
}

void FlxGroup_clear(FlxGroup *self)
{
	As3Array_set_length(&self->members, 0);
	self->length = 0;
}

void FlxGroup_callAll(FlxGroup *self, void (*Function)(FlxBasic *), bool Recurse)
{
	FlxBasic *basic;
	uint32_t i = 0;
	while (i < self->length) {
		basic = As3Array_get(&self->members, i++);
		if (basic != NULL) {
			if (Recurse && as3_is(basic, &FlxGroup_class))
				FlxGroup_callAll((FlxGroup *)basic, Function, Recurse);
			else
				Function(basic);
		}
	}
}

void FlxGroup_kill(FlxGroup *self)
{
	FlxBasic *basic;
	uint32_t i = 0;
	while (i < self->length) {
		basic = As3Array_get(&self->members, i++);
		if ((basic != NULL) && basic->exists)
			CALL0(basic, kill);
	}
	SUPER(FlxBasic_kill((FlxBasic *)self));
}

/* ---- FlxParticle -------------------------------------------------------- */

const As3Class FlxParticle_class = {
	"FlxParticle", &FlxSprite_class, (FlxBasic * (*)(void)) FlxParticle_new,
	{
		.destroy = (void (*)(FlxBasic *))FlxSprite_destroy,
		.preUpdate = (void (*)(FlxBasic *))FlxObject_preUpdate,
		.update = (void (*)(FlxBasic *))FlxParticle_update,
		.postUpdate = (void (*)(FlxBasic *))FlxSprite_postUpdate,
		.draw = (void (*)(FlxBasic *))FlxSprite_draw,
		.kill = FlxBasic_kill,
		.revive = FlxBasic_revive,
		.reset = (void (*)(FlxBasic *, Number, Number))FlxObject_reset,
		.onScreen = (bool (*)(FlxBasic *, FlxCamera *))FlxSprite_onScreen,
		.calcFrame = (void (*)(FlxBasic *))FlxSprite_calcFrame,
		.hurt = (void (*)(FlxBasic *, Number))FlxObject_hurt,
	},
};

/* `new FlxParticle()` (used by FlxGroup.recycle(FlxParticle)) */
FlxParticle *FlxParticle_new(void)
{
	FlxParticle *self = as3_alloc(sizeof *self);
	self->klass = &FlxParticle_class;
	FlxParticle_ctor(self);
	return self;
}

void FlxParticle_ctor(FlxParticle *self)
{
	SUPER(FlxSprite_ctor((FlxSprite *)self, 0, 0, NULL));
	self->lifespan = 0;
	self->friction = 500;
}

void FlxParticle_update(FlxParticle *self)
{
	/* lifespan behavior */
	if (self->lifespan <= 0)
		return;
	self->lifespan -= FlxG.elapsed;
	if (self->lifespan <= 0)
		CALL0(self, kill);

	/* simpler bounce/spin behavior for now */
	if (self->touching) {
		if (self->angularVelocity != 0)
			self->angularVelocity = -self->angularVelocity;
	}
	if (self->acceleration.y > 0) { /* special behavior for particles with gravity */
		if (self->touching & FLOOR) {
			self->drag.x = self->friction;

			if (!(self->wasTouching & FLOOR)) {
				if (self->velocity.y < -self->elasticity * 10) {
					if (self->angularVelocity != 0)
						self->angularVelocity *= -self->elasticity;
				} else {
					self->velocity.y = 0;
					self->angularVelocity = 0;
				}
			}
		} else
			self->drag.x = 0;
	}
}

void FlxParticle_onEmit(FlxParticle *self)
{
	(void)self;
}

/* ---- FlxState ----------------------------------------------------------- */

const As3Class FlxState_class = {
	"FlxState", &FlxGroup_class, NULL,
	{
		.destroy = (void (*)(FlxBasic *))FlxGroup_destroy,
		.preUpdate = (void (*)(FlxBasic *))FlxGroup_preUpdate,
		.update = (void (*)(FlxBasic *))FlxGroup_update,
		.postUpdate = FlxBasic_postUpdate,
		.draw = (void (*)(FlxBasic *))FlxGroup_draw,
		.kill = (void (*)(FlxBasic *))FlxGroup_kill,
		.revive = FlxBasic_revive,
		.create = (void (*)(FlxBasic *))FlxState_create,
	},
};

void FlxState_ctor(FlxState *self)
{
	SUPER(FlxGroup_ctor((FlxGroup *)self, 0));
}

void FlxState_create(FlxState *self)
{
	(void)self;
}

/* FlxGroup.setAll(VariableName, Value, Recurse): AS assigns a property by name; here the
   property is a setter function (ADAPTATION: no dynamic property access in C). */
void FlxGroup_setAll(FlxGroup *self, FlxBasicSetter VariableName, const void *Value, bool Recurse)
{
	FlxBasic *basic;
	uint32_t i = 0;
	while (i < (uint32_t)self->length) {
		basic = As3Array_get(&self->members, i++);
		if (basic != NULL) {
			if (Recurse && as3_is(basic, &FlxGroup_class))
				FlxGroup_setAll((FlxGroup *)basic, VariableName, Value, Recurse);
			else
				VariableName(basic, Value);
		}
	}
}
