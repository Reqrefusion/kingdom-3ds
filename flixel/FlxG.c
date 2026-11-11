/* org/flixel/FlxG.as, org/flixel/FlxGame.as (update/draw loop),
 * org/flixel/system/FlxQuadTree.as
 *
 * Not translated (desktop/debugger only): replays, debugger log/watch,
 * plugins (DebugPathDisplay, TimerManager are handled by FlxTimer),
 * fullscreen, stage access, sound tray, focus screen. Sound functions live
 * in FlxSound.c. */
#include "flixel.h"

bool FlxQuadTree_testOneCallbackPerPair;
/* test-only notify log (off unless FlxQuadTree_testLogEnabled) */
bool FlxQuadTree_testLogEnabled;
const void *FlxQuadTree_testLog[256][2];
int FlxQuadTree_testLogCount;
static const void *testPairs[512][2];
static int testPairCount;
static bool FlxQuadTree_testSeen(const void *a, const void *b)
{
	for (int i = 0; i < testPairCount; i++)
		if (testPairs[i][0] == a && testPairs[i][1] == b)
			return true;
	if (testPairCount < 512) {
		testPairs[testPairCount][0] = a;
		testPairs[testPairCount][1] = b;
		testPairCount++;
	}
	return false;
}

FlxG_t FlxG;
FlxGame *FlxG__game;

/* ---- debugger stand-ins ------------------------------------------------- */

void FlxG_log(const char *Data)
{
	/* Only the desktop debugger shows these; keep them on stderr for host runs. */
#ifndef __3DS__
	fprintf(stderr, "FlxG.log: %s\n", Data ? Data : "ERROR: null object");
#else
	(void)Data;
#endif
}

/* ---- random --------------------------------------------------------------- */

Number FlxG_random(void)
{
	return FlxG.globalSeed = FlxU_srand(FlxG.globalSeed);
}

void *FlxG_getRandom(As3Array *Objects, uint32_t StartIndex, uint32_t Length)
{
	if (Objects != NULL) {
		uint32_t l = Length;
		if ((l == 0) || (l > Objects->length - StartIndex))
			l = Objects->length - StartIndex;
		if (l > 0)
			return As3Array_get(Objects, StartIndex + AS3_UINT(FlxG_random() * l));
	}
	return NULL;
}

/* ---- bitmap cache (_cache:Object keyed by String) ------------------------ */

typedef struct CacheEntry {
	char *key;
	BitmapData *bitmap;
	struct CacheEntry *next;
} CacheEntry;
static CacheEntry *FlxG__cache;

static BitmapData *cache_get(const char *Key)
{
	for (CacheEntry *e = FlxG__cache; e != NULL; e = e->next)
		if (strcmp(e->key, Key) == 0)
			return e->bitmap;
	return NULL;
}

static void cache_set(const char *Key, BitmapData *bitmap)
{
	for (CacheEntry *e = FlxG__cache; e != NULL; e = e->next)
		if (strcmp(e->key, Key) == 0) {
			e->bitmap = bitmap;
			return;
		}
	CacheEntry *e = as3_alloc(sizeof *e);
	e->key = strdup(Key);
	e->bitmap = bitmap;
	e->next = FlxG__cache;
	FlxG__cache = e;
}

bool FlxG_checkBitmapCache(const char *Key)
{
	return cache_get(Key) != NULL;
}

BitmapData *FlxG_createBitmap(uint32_t Width, uint32_t Height, uint32_t Color, bool Unique, const char *Key)
{
	char key[128];
	if (Key == NULL) {
		snprintf(key, sizeof key, "%ux%u:%u", Width, Height, Color);
		Key = key;
		if (Unique && FlxG_checkBitmapCache(Key)) {
			uint32_t inc = 0;
			char ukey[160];
			do {
				snprintf(ukey, sizeof ukey, "%s%u", key, inc++);
			} while (FlxG_checkBitmapCache(ukey));
			snprintf(key, sizeof key, "%s", ukey);
		}
	}
	if (!FlxG_checkBitmapCache(Key))
		cache_set(Key, BitmapData_new((int32_t)Width, (int32_t)Height, true, Color));
	return cache_get(Key);
}

BitmapData *FlxG_addBitmap(const char *Graphic, bool Reverse, bool Unique, const char *Key)
{
	bool needReverse = false;
	char key[512];
	if (Key == NULL) {
		snprintf(key, sizeof key, "%s%s", Graphic, Reverse ? "_REVERSE_" : "");
		Key = key;
		if (Unique && FlxG_checkBitmapCache(Key)) {
			uint32_t inc = 0;
			char ukey[540];
			do {
				snprintf(ukey, sizeof ukey, "%s%u", key, inc++);
			} while (FlxG_checkBitmapCache(ukey));
			snprintf(key, sizeof key, "%s", ukey);
		}
	}

	/* If there is no data for this key, generate the requested graphic */
	if (!FlxG_checkBitmapCache(Key)) {
		cache_set(Key, Embed_bitmapData(Graphic));
		if (Reverse)
			needReverse = true;
	}
	BitmapData *pixels = cache_get(Key);
	if (!needReverse && Reverse && (pixels->width == Embed_bitmapWidth(Graphic)))
		needReverse = true;
	if (needReverse) {
		BitmapData *newPixels = BitmapData_new(pixels->width << 1, pixels->height, true, 0x00000000);
		BitmapData_draw(newPixels, pixels, NULL, NULL, NULL, NULL, false);
		Matrix mtx = Matrix_new();
		Matrix_scale(&mtx, -1, 1);
		Matrix_translate(&mtx, newPixels->width, 0);
		BitmapData_draw(newPixels, pixels, &mtx, NULL, NULL, NULL, false);
		pixels = newPixels;
		cache_set(Key, pixels);
	}
	return pixels;
}

void FlxG_clearBitmapCache(void)
{
	/* _cache = new Object(): the old bitmaps stay referenced by sprites that
	   still use them, exactly like the garbage-collected original. */
	FlxG__cache = NULL;
}

/* ---- state and cameras ---------------------------------------------------- */

void FlxG_switchState(FlxState *State)
{
	FlxG._requestedState = State;
}

FlxCamera *FlxG_addCamera(FlxCamera *NewCamera)
{
	As3Array_set(&FlxG.cameras, FlxG.cameras.length, NewCamera);
	return NewCamera;
}

void FlxG_resetCameras(FlxCamera *NewCamera)
{
	FlxCamera *cam;
	uint32_t i = 0;
	uint32_t l = FlxG.cameras.length;
	while (i < l) {
		cam = As3Array_get(&FlxG.cameras, i++);
		CALL0(cam, destroy);
	}
	As3Array_set_length(&FlxG.cameras, 0);

	if (NewCamera == NULL)
		NewCamera = FlxCamera_new(0, 0, (int32_t)FlxG.width, (int32_t)FlxG.height, 0);
	FlxG.camera = FlxG_addCamera(NewCamera);
}

void FlxG_flash(uint32_t Color, Number Duration, As3Callback0 OnComplete, bool Force)
{
	uint32_t i = 0;
	uint32_t l = FlxG.cameras.length;
	while (i < l)
		FlxCamera_flash(As3Array_get(&FlxG.cameras, i++), Color, Duration, OnComplete, Force);
}

void FlxG_fade(uint32_t Color, Number Duration, As3Callback0 OnComplete, bool Force)
{
	uint32_t i = 0;
	uint32_t l = FlxG.cameras.length;
	while (i < l)
		FlxCamera_fade(As3Array_get(&FlxG.cameras, i++), Color, Duration, OnComplete, Force);
}

void FlxG_shake(Number Intensity, Number Duration, As3Callback0 OnComplete, bool Force, uint32_t Direction)
{
	uint32_t i = 0;
	uint32_t l = FlxG.cameras.length;
	while (i < l)
		FlxCamera_shake(As3Array_get(&FlxG.cameras, i++), Intensity, Duration, OnComplete, Force, Direction);
}

uint32_t FlxG_get_bgColor(void)
{
	if (FlxG.camera == NULL)
		return 0xff000000;
	else
		return FlxG.camera->bgColor;
}

void FlxG_set_bgColor(uint32_t Color)
{
	uint32_t i = 0;
	uint32_t l = FlxG.cameras.length;
	while (i < l)
		((FlxCamera *)As3Array_get(&FlxG.cameras, i++))->bgColor = Color;
}

/* ---- collision ------------------------------------------------------------ */

bool FlxG_overlap(FlxBasic *ObjectOrGroup1, FlxBasic *ObjectOrGroup2, As3Callback2 NotifyCallback, As3Callback2 ProcessCallback)
{
	if (ObjectOrGroup1 == NULL)
		ObjectOrGroup1 = (FlxBasic *)FlxG.state;
	if (ObjectOrGroup2 == ObjectOrGroup1)
		ObjectOrGroup2 = NULL;
	FlxQuadTree_divisions = FlxG.worldDivisions;
	testPairCount = 0;
	FlxQuadTree *quadTree = FlxQuadTree_new(FlxG.worldBounds.x, FlxG.worldBounds.y, FlxG.worldBounds.width, FlxG.worldBounds.height, NULL);
	FlxQuadTree_load(quadTree, ObjectOrGroup1, ObjectOrGroup2, NotifyCallback, ProcessCallback);
	bool result = FlxQuadTree_execute(quadTree);
	FlxQuadTree_destroy(quadTree);
	return result;
}

bool FlxG_collide(FlxBasic *ObjectOrGroup1, FlxBasic *ObjectOrGroup2, As3Callback2 NotifyCallback)
{
	return FlxG_overlap(ObjectOrGroup1, ObjectOrGroup2, NotifyCallback, (As3Callback2)AS3_FN(NULL, FlxObject_separate_fn));
}

/* ---- init / reset / per-frame camera work -------------------------------- */

void FlxG_init(uint32_t Width, uint32_t Height, Number Zoom)
{
	FlxG.width = Width;
	FlxG.height = Height;

	FlxG.mute = false;
	FlxG._volume = 0.5;
	FlxG.sounds = FlxGroup_new(0);

	FlxG_clearBitmapCache();

	FlxCamera_defaultZoom = Zoom;
	FlxG.cameras = (As3Array){0};
	FlxG.useBufferLocking = false;

	FlxG.mobile = false;
	FlxG.visualDebug = false;
}

/* Math.random() seeds FlxG.globalSeed in the original. The platform layer
   sets this before FlxG_reset so runs can be made reproducible. */
Number FlxG_initialSeed = 0.5;

void FlxG_reset(void)
{
	FlxG_clearBitmapCache();
	FlxG.paused = false;
	FlxG.timeScale = 1.0;
	FlxG.elapsed = 0;
	FlxG.globalSeed = FlxG_initialSeed;
	FlxG.worldBounds = FlxRect_new(-10, -10, FlxG.width + 20, FlxG.height + 20);
	FlxG.worldDivisions = 6;
}

void FlxG_lockCameras(void)
{
	FlxCamera *cam;
	As3Array *cams = &FlxG.cameras;
	uint32_t i = 0;
	uint32_t l = cams->length;
	while (i < l) {
		cam = As3Array_get(cams, i++);
		if ((cam == NULL) || !cam->exists || !cam->visible)
			continue;
		FlxCamera_fill(cam, cam->bgColor, true);
		cam->screen->dirty = true;
	}
}

void FlxG_unlockCameras(void)
{
	FlxCamera *cam;
	As3Array *cams = &FlxG.cameras;
	uint32_t i = 0;
	uint32_t l = cams->length;
	while (i < l) {
		cam = As3Array_get(cams, i++);
		if ((cam == NULL) || !cam->exists || !cam->visible)
			continue;
		FlxCamera_drawFX(cam);
	}
}

void FlxG_updateCameras(void)
{
	FlxCamera *cam;
	As3Array *cams = &FlxG.cameras;
	uint32_t i = 0;
	uint32_t l = cams->length;
	while (i < l) {
		cam = As3Array_get(cams, i++);
		if ((cam != NULL) && cam->exists) {
			if (cam->active)
				CALL0(cam, update);
			cam->_flashSprite.x = cam->x + cam->_flashOffsetX;
			cam->_flashSprite.y = cam->y + cam->_flashOffsetY;
			cam->_flashSprite.visible = cam->visible;
		}
	}
}

/* ---- FlxGame ---------------------------------------------------------------- */

void FlxG_updateSounds(void);
void FlxG_updateInput(void);
void FlxG_resetInput(void);
void FlxG_destroySounds(bool ForceDestroy);
void FlxTimer_updateManager(void);
void FlxTimer_clearManager(void);

FlxGame *FlxGame_new(uint32_t GameSizeX, uint32_t GameSizeY, FlxState *(*InitialState)(void), Number Zoom, uint32_t GameFramerate, uint32_t FlashFramerate)
{
	FlxGame *self = as3_alloc(sizeof *self);
	FlxG__game = self;
	FlxG_init(GameSizeX, GameSizeY, Zoom);
	self->_step = AS3_UINT(1000.0 / GameFramerate);
	self->_flashFramerate = FlashFramerate;
	self->_maxAccumulation = AS3_UINT(2000.0 / FlashFramerate - 1);
	if (self->_maxAccumulation < self->_step)
		self->_maxAccumulation = self->_step;
	self->_accumulator = (int32_t)self->_step;
	self->_iState = InitialState;
	self->_requestedReset = true;
	self->_state = NULL;
	return self;
}

void FlxGame_onEnterFrame(FlxGame *self, uint32_t elapsedMS)
{
	self->_accumulator += (int32_t)elapsedMS;
	if (self->_accumulator > (int32_t)self->_maxAccumulation)
		self->_accumulator = (int32_t)self->_maxAccumulation;
	while (self->_accumulator >= (int32_t)self->_step) {
		FlxGame_step(self);
		self->_accumulator = self->_accumulator - (int32_t)self->_step;
	}
	FlxBasic__VISIBLECOUNT = 0;
	FlxGame_draw(self);
}

void FlxGame_switchState(FlxGame *self)
{
	/* Basic reset stuff */
	FlxG_resetCameras(NULL);
	FlxG_resetInput();
	FlxG_destroySounds(false);
	FlxG_clearBitmapCache();

	/* Clear any timers left in the timer manager */
	FlxTimer_clearManager();

	/* Destroy the old state (if there is an old state) */
	if (self->_state != NULL)
		CALL0(self->_state, destroy);

	/* Finally assign and create the new state */
	self->_state = FlxG._requestedState;
	FlxG.state = self->_state;
	CALL0(self->_state, create);
}

void FlxGame_step(FlxGame *self)
{
	/* handle game reset request */
	if (self->_requestedReset) {
		self->_requestedReset = false;
		FlxG._requestedState = self->_iState();
		FlxG_reset();
	}

	/* handle state switching requests */
	if (self->_state != FlxG._requestedState)
		FlxGame_switchState(self);

	/* finally actually step through the game physics */
	FlxBasic__ACTIVECOUNT = 0;
	FlxG_updateInput();
	FlxGame_update(self);
}

void FlxGame_update(FlxGame *self)
{
	FlxG.elapsed = FlxG.timeScale * (self->_step / 1000.0);
	FlxG_updateSounds();
	FlxTimer_updateManager(); /* FlxG.updatePlugins(): TimerManager */
	CALL0(self->_state, update);
	FlxG_updateCameras();
}

void FlxGame_draw(FlxGame *self)
{
	FlxG_lockCameras();
	CALL0(self->_state, draw);
	FlxG_unlockCameras();
}

/* ---- FlxQuadTree ------------------------------------------------------------ */

uint32_t FlxQuadTree_divisions;

static uint32_t FlxQuadTree__min;
static FlxObject *FlxQuadTree__object;
static Number FlxQuadTree__objectLeftEdge;
static Number FlxQuadTree__objectTopEdge;
static Number FlxQuadTree__objectRightEdge;
static Number FlxQuadTree__objectBottomEdge;
static uint32_t FlxQuadTree__list;
static bool FlxQuadTree__useBothLists;
static As3Callback2 FlxQuadTree__processingCallback;
static As3Callback2 FlxQuadTree__notifyCallback;
static FlxList *FlxQuadTree__iterator;
static Number FlxQuadTree__objectHullX;
static Number FlxQuadTree__objectHullY;
static Number FlxQuadTree__objectHullWidth;
static Number FlxQuadTree__objectHullHeight;
static Number FlxQuadTree__checkObjectHullX;
static Number FlxQuadTree__checkObjectHullY;
static Number FlxQuadTree__checkObjectHullWidth;
static Number FlxQuadTree__checkObjectHullHeight;

/* ADAPTATION: destroyed nodes are reused (allocation only, see FlxList) */
static FlxQuadTree *FlxQuadTree__pool;

FlxQuadTree *FlxQuadTree_new(Number X, Number Y, Number Width, Number Height, FlxQuadTree *Parent)
{
	FlxQuadTree *self = FlxQuadTree__pool;
	if (self != NULL) {
		FlxQuadTree__pool = self->_northWestTree;
		memset(self, 0, sizeof *self);
	} else
		self = as3_alloc(sizeof *self);
	self->x = X;
	self->y = Y;
	self->width = Width;
	self->height = Height;
	self->_headA = self->_tailA = FlxList_new();
	self->_headB = self->_tailB = FlxList_new();

	/* Copy the parent's children (if there are any) */
	if (Parent != NULL) {
		FlxList *iterator;
		FlxList *ot;
		if (Parent->_headA->object != NULL) {
			iterator = Parent->_headA;
			while (iterator != NULL) {
				if (self->_tailA->object != NULL) {
					ot = self->_tailA;
					self->_tailA = FlxList_new();
					ot->next = self->_tailA;
				}
				self->_tailA->object = iterator->object;
				iterator = iterator->next;
			}
		}
		if (Parent->_headB->object != NULL) {
			iterator = Parent->_headB;
			while (iterator != NULL) {
				if (self->_tailB->object != NULL) {
					ot = self->_tailB;
					self->_tailB = FlxList_new();
					ot->next = self->_tailB;
				}
				self->_tailB->object = iterator->object;
				iterator = iterator->next;
			}
		}
	} else
		FlxQuadTree__min = AS3_UINT((self->width + self->height) / (2 * FlxQuadTree_divisions));
	self->_canSubdivide = (self->width > FlxQuadTree__min) || (self->height > FlxQuadTree__min);

	/* Set up comparison/sort helpers */
	self->_northWestTree = NULL;
	self->_northEastTree = NULL;
	self->_southEastTree = NULL;
	self->_southWestTree = NULL;
	self->_leftEdge = self->x;
	self->_rightEdge = self->x + self->width;
	self->_halfWidth = self->width / 2;
	self->_midpointX = self->_leftEdge + self->_halfWidth;
	self->_topEdge = self->y;
	self->_bottomEdge = self->y + self->height;
	self->_halfHeight = self->height / 2;
	self->_midpointY = self->_topEdge + self->_halfHeight;
	return self;
}

void FlxQuadTree_destroy(FlxQuadTree *self)
{
	/* _headA.destroy() releases the whole A chain including _tailA. */
	FlxList_destroy(self->_headA);
	self->_headA = NULL;
	self->_tailA = NULL;
	FlxList_destroy(self->_headB);
	self->_headB = NULL;
	self->_tailB = NULL;

	if (self->_northWestTree != NULL)
		FlxQuadTree_destroy(self->_northWestTree);
	self->_northWestTree = NULL;
	if (self->_northEastTree != NULL)
		FlxQuadTree_destroy(self->_northEastTree);
	self->_northEastTree = NULL;
	if (self->_southEastTree != NULL)
		FlxQuadTree_destroy(self->_southEastTree);
	self->_southEastTree = NULL;
	if (self->_southWestTree != NULL)
		FlxQuadTree_destroy(self->_southWestTree);
	self->_southWestTree = NULL;

	FlxQuadTree__object = NULL;
	FlxQuadTree__processingCallback = (As3Callback2)AS3_NULL_FN;
	FlxQuadTree__notifyCallback = (As3Callback2)AS3_NULL_FN;
	self->_northWestTree = FlxQuadTree__pool; /* free list link */
	FlxQuadTree__pool = self;
}

void FlxQuadTree_load(FlxQuadTree *self, FlxBasic *ObjectOrGroup1, FlxBasic *ObjectOrGroup2, As3Callback2 NotifyCallback, As3Callback2 ProcessCallback)
{
	FlxQuadTree_add(self, ObjectOrGroup1, FlxQuadTree_A_LIST);
	if (ObjectOrGroup2 != NULL) {
		FlxQuadTree_add(self, ObjectOrGroup2, FlxQuadTree_B_LIST);
		FlxQuadTree__useBothLists = true;
	} else
		FlxQuadTree__useBothLists = false;
	FlxQuadTree__notifyCallback = NotifyCallback;
	FlxQuadTree__processingCallback = ProcessCallback;
}

void FlxQuadTree_add(FlxQuadTree *self, FlxBasic *ObjectOrGroup, uint32_t List)
{
	FlxQuadTree__list = List;
	if (as3_is(ObjectOrGroup, &FlxGroup_class)) {
		uint32_t i = 0;
		FlxBasic *basic;
		As3Array *members = &((FlxGroup *)ObjectOrGroup)->members;
		uint32_t l = AS3_UINT(((FlxGroup *)ObjectOrGroup)->length);
		while (i < l) {
			basic = As3Array_get(members, i++);
			if ((basic != NULL) && basic->exists) {
				if (as3_is(basic, &FlxGroup_class))
					FlxQuadTree_add(self, basic, List);
				else if (as3_is(basic, &FlxObject_class)) {
					FlxQuadTree__object = (FlxObject *)basic;
					if (FlxQuadTree__object->exists && FlxQuadTree__object->allowCollisions) {
						FlxQuadTree__objectLeftEdge = FlxQuadTree__object->x;
						FlxQuadTree__objectTopEdge = FlxQuadTree__object->y;
						FlxQuadTree__objectRightEdge = FlxQuadTree__object->x + FlxQuadTree__object->width;
						FlxQuadTree__objectBottomEdge = FlxQuadTree__object->y + FlxQuadTree__object->height;
						FlxQuadTree_addObject(self);
					}
				}
			}
		}
	} else {
		FlxQuadTree__object = (FlxObject *)ObjectOrGroup;
		if (FlxQuadTree__object->exists && FlxQuadTree__object->allowCollisions) {
			FlxQuadTree__objectLeftEdge = FlxQuadTree__object->x;
			FlxQuadTree__objectTopEdge = FlxQuadTree__object->y;
			FlxQuadTree__objectRightEdge = FlxQuadTree__object->x + FlxQuadTree__object->width;
			FlxQuadTree__objectBottomEdge = FlxQuadTree__object->y + FlxQuadTree__object->height;
			FlxQuadTree_addObject(self);
		}
	}
}

void FlxQuadTree_addObject(FlxQuadTree *self)
{
	/* If this quad (not its children) lies entirely inside this object, add it here */
	if (!self->_canSubdivide || ((self->_leftEdge >= FlxQuadTree__objectLeftEdge) && (self->_rightEdge <= FlxQuadTree__objectRightEdge) && (self->_topEdge >= FlxQuadTree__objectTopEdge) && (self->_bottomEdge <= FlxQuadTree__objectBottomEdge))) {
		FlxQuadTree_addToList(self);
		return;
	}

	/* See if the selected object fits completely inside any of the quadrants */
	if ((FlxQuadTree__objectLeftEdge > self->_leftEdge) && (FlxQuadTree__objectRightEdge < self->_midpointX)) {
		if ((FlxQuadTree__objectTopEdge > self->_topEdge) && (FlxQuadTree__objectBottomEdge < self->_midpointY)) {
			if (self->_northWestTree == NULL)
				self->_northWestTree = FlxQuadTree_new(self->_leftEdge, self->_topEdge, self->_halfWidth, self->_halfHeight, self);
			FlxQuadTree_addObject(self->_northWestTree);
			return;
		}
		if ((FlxQuadTree__objectTopEdge > self->_midpointY) && (FlxQuadTree__objectBottomEdge < self->_bottomEdge)) {
			if (self->_southWestTree == NULL)
				self->_southWestTree = FlxQuadTree_new(self->_leftEdge, self->_midpointY, self->_halfWidth, self->_halfHeight, self);
			FlxQuadTree_addObject(self->_southWestTree);
			return;
		}
	}
	if ((FlxQuadTree__objectLeftEdge > self->_midpointX) && (FlxQuadTree__objectRightEdge < self->_rightEdge)) {
		if ((FlxQuadTree__objectTopEdge > self->_topEdge) && (FlxQuadTree__objectBottomEdge < self->_midpointY)) {
			if (self->_northEastTree == NULL)
				self->_northEastTree = FlxQuadTree_new(self->_midpointX, self->_topEdge, self->_halfWidth, self->_halfHeight, self);
			FlxQuadTree_addObject(self->_northEastTree);
			return;
		}
		if ((FlxQuadTree__objectTopEdge > self->_midpointY) && (FlxQuadTree__objectBottomEdge < self->_bottomEdge)) {
			if (self->_southEastTree == NULL)
				self->_southEastTree = FlxQuadTree_new(self->_midpointX, self->_midpointY, self->_halfWidth, self->_halfHeight, self);
			FlxQuadTree_addObject(self->_southEastTree);
			return;
		}
	}

	/* If it wasn't completely contained we have to check out the partial overlaps */
	if ((FlxQuadTree__objectRightEdge > self->_leftEdge) && (FlxQuadTree__objectLeftEdge < self->_midpointX) && (FlxQuadTree__objectBottomEdge > self->_topEdge) && (FlxQuadTree__objectTopEdge < self->_midpointY)) {
		if (self->_northWestTree == NULL)
			self->_northWestTree = FlxQuadTree_new(self->_leftEdge, self->_topEdge, self->_halfWidth, self->_halfHeight, self);
		FlxQuadTree_addObject(self->_northWestTree);
	}
	if ((FlxQuadTree__objectRightEdge > self->_midpointX) && (FlxQuadTree__objectLeftEdge < self->_rightEdge) && (FlxQuadTree__objectBottomEdge > self->_topEdge) && (FlxQuadTree__objectTopEdge < self->_midpointY)) {
		if (self->_northEastTree == NULL)
			self->_northEastTree = FlxQuadTree_new(self->_midpointX, self->_topEdge, self->_halfWidth, self->_halfHeight, self);
		FlxQuadTree_addObject(self->_northEastTree);
	}
	if ((FlxQuadTree__objectRightEdge > self->_midpointX) && (FlxQuadTree__objectLeftEdge < self->_rightEdge) && (FlxQuadTree__objectBottomEdge > self->_midpointY) && (FlxQuadTree__objectTopEdge < self->_bottomEdge)) {
		if (self->_southEastTree == NULL)
			self->_southEastTree = FlxQuadTree_new(self->_midpointX, self->_midpointY, self->_halfWidth, self->_halfHeight, self);
		FlxQuadTree_addObject(self->_southEastTree);
	}
	if ((FlxQuadTree__objectRightEdge > self->_leftEdge) && (FlxQuadTree__objectLeftEdge < self->_midpointX) && (FlxQuadTree__objectBottomEdge > self->_midpointY) && (FlxQuadTree__objectTopEdge < self->_bottomEdge)) {
		if (self->_southWestTree == NULL)
			self->_southWestTree = FlxQuadTree_new(self->_leftEdge, self->_midpointY, self->_halfWidth, self->_halfHeight, self);
		FlxQuadTree_addObject(self->_southWestTree);
	}
}

void FlxQuadTree_addToList(FlxQuadTree *self)
{
	FlxList *ot;
	if (FlxQuadTree__list == FlxQuadTree_A_LIST) {
		if (self->_tailA->object != NULL) {
			ot = self->_tailA;
			self->_tailA = FlxList_new();
			ot->next = self->_tailA;
		}
		self->_tailA->object = FlxQuadTree__object;
	} else {
		if (self->_tailB->object != NULL) {
			ot = self->_tailB;
			self->_tailB = FlxList_new();
			ot->next = self->_tailB;
		}
		self->_tailB->object = FlxQuadTree__object;
	}
	if (!self->_canSubdivide)
		return;
	if (self->_northWestTree != NULL)
		FlxQuadTree_addToList(self->_northWestTree);
	if (self->_northEastTree != NULL)
		FlxQuadTree_addToList(self->_northEastTree);
	if (self->_southEastTree != NULL)
		FlxQuadTree_addToList(self->_southEastTree);
	if (self->_southWestTree != NULL)
		FlxQuadTree_addToList(self->_southWestTree);
}

bool FlxQuadTree_execute(FlxQuadTree *self)
{
	bool overlapProcessed = false;
	FlxList *iterator;

	if (self->_headA->object != NULL) {
		iterator = self->_headA;
		while (iterator != NULL) {
			FlxQuadTree__object = iterator->object;
			if (FlxQuadTree__useBothLists)
				FlxQuadTree__iterator = self->_headB;
			else
				FlxQuadTree__iterator = iterator->next;
			if (FlxQuadTree__object->exists && (FlxQuadTree__object->allowCollisions > 0) &&
				(FlxQuadTree__iterator != NULL) && (FlxQuadTree__iterator->object != NULL) &&
				FlxQuadTree__iterator->object->exists && FlxQuadTree_overlapNode(self)) {
				overlapProcessed = true;
			}
			iterator = iterator->next;
		}
	}

	/* Advance through the tree by calling overlap on each child */
	if ((self->_northWestTree != NULL) && FlxQuadTree_execute(self->_northWestTree))
		overlapProcessed = true;
	if ((self->_northEastTree != NULL) && FlxQuadTree_execute(self->_northEastTree))
		overlapProcessed = true;
	if ((self->_southEastTree != NULL) && FlxQuadTree_execute(self->_southEastTree))
		overlapProcessed = true;
	if ((self->_southWestTree != NULL) && FlxQuadTree_execute(self->_southWestTree))
		overlapProcessed = true;

	return overlapProcessed;
}

bool FlxQuadTree_overlapNode(FlxQuadTree *self)
{
	(void)self;
	/* Walk the list and check for overlaps */
	bool overlapProcessed = false;
	FlxObject *checkObject;
	while (FlxQuadTree__iterator != NULL) {
		if (!FlxQuadTree__object->exists || (FlxQuadTree__object->allowCollisions <= 0))
			break;

		checkObject = FlxQuadTree__iterator->object;
		if ((FlxQuadTree__object == checkObject) || !checkObject->exists || (checkObject->allowCollisions <= 0)) {
			FlxQuadTree__iterator = FlxQuadTree__iterator->next;
			continue;
		}

		/* calculate bulk hull for _object */
		FlxQuadTree__objectHullX = (FlxQuadTree__object->x < FlxQuadTree__object->last.x) ? FlxQuadTree__object->x : FlxQuadTree__object->last.x;
		FlxQuadTree__objectHullY = (FlxQuadTree__object->y < FlxQuadTree__object->last.y) ? FlxQuadTree__object->y : FlxQuadTree__object->last.y;
		FlxQuadTree__objectHullWidth = FlxQuadTree__object->x - FlxQuadTree__object->last.x;
		FlxQuadTree__objectHullWidth = FlxQuadTree__object->width + ((FlxQuadTree__objectHullWidth > 0) ? FlxQuadTree__objectHullWidth : -FlxQuadTree__objectHullWidth);
		FlxQuadTree__objectHullHeight = FlxQuadTree__object->y - FlxQuadTree__object->last.y;
		FlxQuadTree__objectHullHeight = FlxQuadTree__object->height + ((FlxQuadTree__objectHullHeight > 0) ? FlxQuadTree__objectHullHeight : -FlxQuadTree__objectHullHeight);

		/* calculate bulk hull for checkObject */
		FlxQuadTree__checkObjectHullX = (checkObject->x < checkObject->last.x) ? checkObject->x : checkObject->last.x;
		FlxQuadTree__checkObjectHullY = (checkObject->y < checkObject->last.y) ? checkObject->y : checkObject->last.y;
		FlxQuadTree__checkObjectHullWidth = checkObject->x - checkObject->last.x;
		FlxQuadTree__checkObjectHullWidth = checkObject->width + ((FlxQuadTree__checkObjectHullWidth > 0) ? FlxQuadTree__checkObjectHullWidth : -FlxQuadTree__checkObjectHullWidth);
		FlxQuadTree__checkObjectHullHeight = checkObject->y - checkObject->last.y;
		FlxQuadTree__checkObjectHullHeight = checkObject->height + ((FlxQuadTree__checkObjectHullHeight > 0) ? FlxQuadTree__checkObjectHullHeight : -FlxQuadTree__checkObjectHullHeight);

		/* check for intersection of the two hulls */
		if ((FlxQuadTree__objectHullX + FlxQuadTree__objectHullWidth > FlxQuadTree__checkObjectHullX) &&
			(FlxQuadTree__objectHullX < FlxQuadTree__checkObjectHullX + FlxQuadTree__checkObjectHullWidth) &&
			(FlxQuadTree__objectHullY + FlxQuadTree__objectHullHeight > FlxQuadTree__checkObjectHullY) &&
			(FlxQuadTree__objectHullY < FlxQuadTree__checkObjectHullY + FlxQuadTree__checkObjectHullHeight)) {
			/* Test-only (off in the game): report each pair once per
			   FlxG.overlap call, for comparing against models that assume
			   that. Flixel 2.55 itself reports a pair once per shared leaf. */
			if (FlxQuadTree_testOneCallbackPerPair && FlxQuadTree_testSeen(FlxQuadTree__object, checkObject)) {
				FlxQuadTree__iterator = FlxQuadTree__iterator->next;
				continue;
			}
			/* Execute callback functions if they exist */
			if ((FlxQuadTree__processingCallback.fn == NULL) || FlxQuadTree__processingCallback.fn(FlxQuadTree__processingCallback.self, FlxQuadTree__object, checkObject))
				overlapProcessed = true;
			if (overlapProcessed && FlxQuadTree_testLogEnabled && FlxQuadTree_testLogCount < 256) {
				FlxQuadTree_testLog[FlxQuadTree_testLogCount][0] = FlxQuadTree__object;
				FlxQuadTree_testLog[FlxQuadTree_testLogCount++][1] = checkObject;
			}
			if (overlapProcessed && (FlxQuadTree__notifyCallback.fn != NULL))
				FlxQuadTree__notifyCallback.fn(FlxQuadTree__notifyCallback.self, FlxQuadTree__object, checkObject);
		}
		FlxQuadTree__iterator = FlxQuadTree__iterator->next;
	}

	return overlapProcessed;
}

/* FlxG.watch: shows a variable in the Flixel debugger, which the 3DS build
   does not have (FlxG.debug is false), so like FlxG.log it records nothing. */
void FlxG_watch(void *AnyObject, const char *VariableName, const char *DisplayName)
{
	(void)AnyObject;
	(void)VariableName;
	(void)DisplayName;
}

/* ---- FlxG members the game never calls --------------------------------------
 * Not translated: recordReplay, loadReplay, reloadReplay, stopRecording, stopReplay
 * (FlxReplay), stream (FlxSound.loadStream), the debugger layout calls (there is no
 * debugger: both are no-ops below) and fullscreen (no Flash stage). */

Number FlxG_get_framerate(void)
{
	return 1000.0 / FlxG__game->_step;
}

void FlxG_set_framerate(Number Framerate)
{
	FlxG__game->_step = AS3_UINT(1000 / Framerate);
	if (FlxG__game->_maxAccumulation < FlxG__game->_step)
		FlxG__game->_maxAccumulation = FlxG__game->_step;
}

Number FlxG_get_flashFramerate(void)
{
	return 0; /* _game.root == null: no Flash stage on the console */
}

void FlxG_set_flashFramerate(Number Framerate)
{
	FlxG__game->_flashFramerate = AS3_UINT(Framerate);
	FlxG__game->_maxAccumulation = AS3_UINT(2000.0 / FlxG__game->_flashFramerate - 1);
	if (FlxG__game->_maxAccumulation < FlxG__game->_step)
		FlxG__game->_maxAccumulation = FlxG__game->_step;
}

FlxState *FlxG_get_state(void)
{
	return FlxG__game->_state;
}

void *FlxG_get_stage(void)
{
	return NULL; /* _game.root == null */
}

void FlxG_resetState(void)
{
	const As3Class *klass = FlxU_getClass(FlxU_getClassName(FlxG__game->_state, false));
	FlxG._requestedState = (FlxState *)klass->construct();
}

void FlxG_resetGame(void)
{
	FlxG__game->_requestedReset = true;
}

void FlxG_removeCamera(FlxCamera *Camera, bool Destroy)
{
	/* FlxG._game.removeChild(Camera._flashSprite): the cameras are not display objects here */
	if (Destroy)
		CALL0(Camera, destroy);
}

As3Array *FlxG_shuffle(As3Array *Objects, uint32_t HowManyTimes)
{
	uint32_t i = 0;
	uint32_t index1;
	uint32_t index2;
	void *object;
	while (i < HowManyTimes) {
		index1 = AS3_UINT(FlxG_random() * Objects->length);
		index2 = AS3_UINT(FlxG_random() * Objects->length);
		object = As3Array_get(Objects, index2);
		As3Array_set(Objects, index2, As3Array_get(Objects, index1));
		As3Array_set(Objects, index1, object);
		i++;
	}
	return Objects;
}

char *FlxG_getLibraryName(void)
{
	return AS3_CAT(FlxG_LIBRARY_NAME, " v", AS3_STR(FlxG_LIBRARY_MAJOR_VERSION), ".", AS3_STR(FlxG_LIBRARY_MINOR_VERSION));
}

FlxBasic *FlxG_addPlugin(FlxBasic *Plugin)
{
	As3Array *pluginList = &FlxG.plugins;
	uint32_t i = 0;
	uint32_t l = pluginList->length;
	while (i < l) {
		char *a = FlxBasic_toString(As3Array_get(pluginList, i++));
		char *b = FlxBasic_toString(Plugin);
		if (as3_streq(a, b))
			return Plugin;
	}
	As3Array_push(pluginList, Plugin);
	return Plugin;
}

FlxBasic *FlxG_getPlugin(const As3Class *ClassType)
{
	As3Array *pluginList = &FlxG.plugins;
	uint32_t i = 0;
	while (i < pluginList->length) {
		if (as3_is(As3Array_get(pluginList, i), ClassType))
			return As3Array_get(&FlxG.plugins, i);
		i++;
	}
	return NULL;
}

FlxBasic *FlxG_removePlugin(FlxBasic *Plugin)
{
	As3Array *pluginList = &FlxG.plugins;
	int32_t i = (int32_t)pluginList->length - 1;
	while (i >= 0) {
		if (As3Array_get(pluginList, (uint32_t)i) == Plugin)
			As3Array_splice1(pluginList, (uint32_t)i);
		i--;
	}
	return Plugin;
}

bool FlxG_removePluginType(const As3Class *ClassType)
{
	bool results = false;
	As3Array *pluginList = &FlxG.plugins;
	int32_t i = (int32_t)pluginList->length - 1;
	while (i >= 0) {
		if (as3_is(As3Array_get(pluginList, (uint32_t)i), ClassType)) {
			As3Array_splice1(pluginList, (uint32_t)i);
			results = true;
		}
		i--;
	}
	return results;
}

void FlxG_updatePlugins(void)
{
	FlxBasic *plugin;
	As3Array *pluginList = &FlxG.plugins;
	uint32_t i = 0;
	uint32_t l = pluginList->length;
	while (i < l) {
		plugin = As3Array_get(pluginList, i++);
		if (plugin->exists && plugin->active)
			CALL0(plugin, update);
	}
}

void FlxG_drawPlugins(void)
{
	FlxBasic *plugin;
	As3Array *pluginList = &FlxG.plugins;
	uint32_t i = 0;
	uint32_t l = pluginList->length;
	while (i < l) {
		plugin = As3Array_get(pluginList, i++);
		if (plugin->exists && plugin->visible)
			CALL0(plugin, draw);
	}
}

void FlxG_unwatch(void *AnyObject, const char *VariableName)
{
	(void)AnyObject;
	(void)VariableName; /* (_game._debugger != null): there is no debugger */
}

void FlxG_resetDebuggerLayout(void)
{
	/* if(_game._debugger != null): there is no debugger */
}

void FlxG_setDebuggerLayout(uint32_t Layout)
{
	(void)Layout; /* if(_game._debugger != null): there is no debugger */
}
