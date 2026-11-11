/* Flixel 2.55 (as pinned in the Kingdom source) translated to C.
 *
 * Field layout: every class lists its parent's fields first through the
 * *_FIELDS macros, so `self->x` works on any subclass exactly like `x` does
 * in ActionScript, and a pointer to a subclass is a valid pointer to its
 * parent. One .c file per .as file; functions are named Class_method.
 */
#ifndef TRANSLIT_FLIXEL_H
#define TRANSLIT_FLIXEL_H
#include "../flash/flash.h"

struct FlxBasic;
struct FlxObject;
struct FlxSprite;
struct FlxGroup;
struct FlxCamera;
struct FlxSound;

/* ---- Function objects ---------------------------------------------------
   An AS3 Function value is a method closure: (object, method). */
typedef struct As3Callback2 {
	void *self;
	bool (*fn)(void *self, struct FlxObject *a, struct FlxObject *b);
} As3Callback2;
typedef struct As3Callback0 {
	void *self;
	void (*fn)(void *self);
} As3Callback0;
typedef struct As3AnimCallback {
	void *self;
	void (*fn)(void *self, const char *name, uint32_t frame, uint32_t index);
} As3AnimCallback;
#define AS3_NULL_FN {NULL, NULL}
#define AS3_FN(object, function) {(void *)(object), (function)}

/* ---- Default virtual tables ----------------------------------------------
   A subclass starts from its parent's table and overrides entries:
   { FLXSPRITE_VT, .update = (...)Bunny_update } */
#define FLXBASIC_VT \
	.destroy = FlxBasic_destroy, \
	.preUpdate = FlxBasic_preUpdate, \
	.update = FlxBasic_update, \
	.postUpdate = FlxBasic_postUpdate, \
	.draw = FlxBasic_draw, \
	.kill = FlxBasic_kill, \
	.revive = FlxBasic_revive
#define FLXOBJECT_VT \
	FLXBASIC_VT, \
	.destroy = (void (*)(FlxBasic *))FlxObject_destroy, \
	.preUpdate = (void (*)(FlxBasic *))FlxObject_preUpdate, \
	.postUpdate = (void (*)(FlxBasic *))FlxObject_postUpdate, \
	.draw = (void (*)(FlxBasic *))FlxObject_draw, \
	.reset = (void (*)(FlxBasic *, Number, Number))FlxObject_reset, \
	.onScreen = (bool (*)(FlxBasic *, struct FlxCamera *))FlxObject_onScreen, \
	.hurt = (void (*)(FlxBasic *, Number))FlxObject_hurt
#define FLXSPRITE_VT \
	FLXOBJECT_VT, \
	.destroy = (void (*)(FlxBasic *))FlxSprite_destroy, \
	.postUpdate = (void (*)(FlxBasic *))FlxSprite_postUpdate, \
	.draw = (void (*)(FlxBasic *))FlxSprite_draw, \
	.onScreen = (bool (*)(FlxBasic *, struct FlxCamera *))FlxSprite_onScreen, \
	.calcFrame = (void (*)(FlxBasic *))FlxSprite_calcFrame
#define FLXPARTICLE_VT \
	FLXSPRITE_VT, \
	.update = (void (*)(FlxBasic *))FlxParticle_update
#define FLXGROUP_VT \
	FLXBASIC_VT, \
	.destroy = (void (*)(FlxBasic *))FlxGroup_destroy, \
	.preUpdate = (void (*)(FlxBasic *))FlxGroup_preUpdate, \
	.update = (void (*)(FlxBasic *))FlxGroup_update, \
	.draw = (void (*)(FlxBasic *))FlxGroup_draw, \
	.kill = (void (*)(FlxBasic *))FlxGroup_kill
#define FLXSTATE_VT \
	FLXGROUP_VT, \
	.create = (void (*)(FlxBasic *))FlxState_create

/* ---- FlxPoint / FlxRect ------------------------------------------------- */

typedef struct FlxPoint {
	Number x;
	Number y;
} FlxPoint;
static inline FlxPoint FlxPoint_new(Number X, Number Y) { return (FlxPoint){X, Y}; }
FlxPoint *FlxPoint_alloc(Number X, Number Y);
FlxPoint *FlxPoint_make(FlxPoint *self, Number X, Number Y);
FlxPoint *FlxPoint_copyFrom(FlxPoint *self, const FlxPoint *Point);
FlxPoint *FlxPoint_copyTo(const FlxPoint *self, FlxPoint *Point);

typedef struct FlxRect {
	Number x;
	Number y;
	Number width;
	Number height;
} FlxRect;
static inline FlxRect FlxRect_new(Number X, Number Y, Number Width, Number Height) { return (FlxRect){X, Y, Width, Height}; }
FlxRect *FlxRect_alloc(Number X, Number Y, Number Width, Number Height);
Number FlxRect_get_left(const FlxRect *self);
Number FlxRect_get_right(const FlxRect *self);
Number FlxRect_get_top(const FlxRect *self);
Number FlxRect_get_bottom(const FlxRect *self);
FlxRect *FlxRect_copyTo(const FlxRect *self, FlxRect *Rect);
FlxRect *FlxRect_copyFromFlash(FlxRect *self, const Rectangle *FlashRect);
Rectangle *FlxRect_copyToFlash(const FlxRect *self, Rectangle *FlashRect);
void FlxPoint_copyFromFlash(FlxPoint *self, const Point *FlashPoint);
Point *FlxPoint_copyToFlash(const FlxPoint *self, Point *FlashPoint);
/* reflection helpers (flixel/FlxU.c, game/class_table.c) */
const char *FlxU_getClassName(const void *Obj, bool Simple);
bool FlxU_compareClassNames(const void *Obj1, const void *Obj2);
const As3Class *FlxU_getClass(const char *Name);
char *FlxU_formatArray(const As3Array *AnyArray);
void FlxU_openURL(const char *URL, const char *Window);
FlxRect *FlxRect_make(FlxRect *self, Number X, Number Y, Number Width, Number Height);
FlxRect *FlxRect_copyFrom(FlxRect *self, const FlxRect *Rect);
bool FlxRect_overlaps(const FlxRect *self, const FlxRect *Rect);

/* ---- FlxAnim (org.flixel.system) ---------------------------------------- */

typedef struct FlxAnim {
	const char *name;
	Number delay;
	int32_t *frames;
	uint32_t frames_length;
	bool looped;
} FlxAnim;
FlxAnim *FlxAnim_new(const char *Name, const int32_t *Frames, uint32_t FramesLength, Number FrameRate, bool Looped);

/* ---- FlxBasic ------------------------------------------------------------ */

#define FLXBASIC_FIELDS \
	AS3_OBJECT_FIELDS \
	int32_t ID; \
	bool exists; \
	bool active; \
	bool visible; \
	bool alive; \
	As3Array *cameras; \
	bool ignoreDrawDebug;

typedef struct FlxBasic {
	FLXBASIC_FIELDS
} FlxBasic;

extern const As3Class FlxBasic_class;
extern uint32_t FlxBasic__ACTIVECOUNT;
extern uint32_t FlxBasic__VISIBLECOUNT;
void FlxBasic_ctor(FlxBasic *self);
void FlxBasic_destroy(FlxBasic *self);
void FlxBasic_preUpdate(FlxBasic *self);
void FlxBasic_update(FlxBasic *self);
void FlxBasic_postUpdate(FlxBasic *self);
void FlxBasic_draw(FlxBasic *self);
void FlxBasic_drawDebug(FlxBasic *self, struct FlxCamera *Camera);
void FlxBasic_kill(FlxBasic *self);
void FlxBasic_revive(FlxBasic *self);

/* ---- FlxObject ------------------------------------------------------------ */

enum {
	LEFT = 0x0001,
	RIGHT = 0x0010,
	UP = 0x0100,
	DOWN = 0x1000,
	NONE = 0,
	CEILING = UP,
	FLOOR = DOWN,
	WALL = LEFT | RIGHT,
	ANY = LEFT | RIGHT | UP | DOWN,
};
#define FlxObject_OVERLAP_BIAS 4.0

#define FLXOBJECT_FIELDS \
	FLXBASIC_FIELDS \
	Number x; \
	Number y; \
	Number width; \
	Number height; \
	bool immovable; \
	FlxPoint velocity; \
	Number mass; \
	Number elasticity; \
	FlxPoint acceleration; \
	FlxPoint drag; \
	FlxPoint maxVelocity; \
	Number angle; \
	Number angularVelocity; \
	Number angularAcceleration; \
	Number angularDrag; \
	Number maxAngular; \
	FlxPoint scrollFactor; \
	bool _flicker; \
	Number _flickerTimer; \
	Number health; \
	FlxPoint _point; \
	FlxRect _rect; \
	bool moves; \
	uint32_t touching; \
	uint32_t wasTouching; \
	uint32_t allowCollisions; \
	FlxPoint last; \
	void *path; \
	Number pathSpeed; \
	Number pathAngle;

typedef struct FlxObject {
	FLXOBJECT_FIELDS
} FlxObject;

extern const As3Class FlxObject_class;
void FlxObject_ctor(FlxObject *self, Number X, Number Y, Number Width, Number Height);
void FlxObject_destroy(FlxObject *self);
void FlxObject_preUpdate(FlxObject *self);
void FlxObject_postUpdate(FlxObject *self);
void FlxObject_updateMotion(FlxObject *self);
/* FlxObject/FlxSprite inherit update() from FlxBasic */
#define FlxObject_update(self) FlxBasic_update((FlxBasic *)(self))
#define FlxSprite_update(self) FlxBasic_update((FlxBasic *)(self))
void FlxObject_draw(FlxObject *self);
bool FlxObject_overlaps(FlxObject *self, FlxBasic *ObjectOrGroup, bool InScreenSpace, struct FlxCamera *Camera);
bool FlxObject_overlapsAt(FlxObject *self, Number X, Number Y, FlxBasic *ObjectOrGroup, bool InScreenSpace, struct FlxCamera *Camera);
bool FlxObject_overlapsPoint(FlxObject *self, FlxPoint *Point, bool InScreenSpace, struct FlxCamera *Camera);
bool FlxObject_onScreen(FlxObject *self, struct FlxCamera *Camera);
FlxPoint *FlxObject_getScreenXY(FlxObject *self, FlxPoint *Point, struct FlxCamera *Camera);
void FlxObject_flicker(FlxObject *self, Number Duration);
bool FlxObject_get_flickering(FlxObject *self);
bool FlxObject_get_solid(FlxObject *self);
void FlxObject_set_solid(FlxObject *self, bool Solid);
FlxPoint *FlxObject_getMidpoint(FlxObject *self, FlxPoint *Point);
void FlxObject_reset(FlxObject *self, Number X, Number Y);
bool FlxObject_isTouching(FlxObject *self, uint32_t Direction);
bool FlxObject_justTouched(FlxObject *self, uint32_t Direction);
void FlxObject_hurt(FlxObject *self, Number Damage);
bool FlxObject_separate(FlxObject *Object1, FlxObject *Object2);
bool FlxObject_separateX(FlxObject *Object1, FlxObject *Object2);
bool FlxObject_separateY(FlxObject *Object1, FlxObject *Object2);
/* FlxObject.separate as a Function value (used by FlxG.collide). */
bool FlxObject_separate_fn(void *unused, FlxObject *Object1, FlxObject *Object2);

/* ---- FlxSprite ------------------------------------------------------------ */

#define FLXSPRITE_FIELDS \
	FLXOBJECT_FIELDS \
	FlxPoint origin; \
	FlxPoint offset; \
	FlxPoint scale; \
	const char *blend; \
	bool antialiasing; \
	bool finished; \
	uint32_t frameWidth; \
	uint32_t frameHeight; \
	uint32_t frames; \
	BitmapData *framePixels; \
	bool dirty; \
	As3Array _animations; \
	uint32_t _flipped; \
	FlxAnim *_curAnim; \
	uint32_t _curFrame; \
	uint32_t _curIndex; \
	Number _frameTimer; \
	As3AnimCallback _callback; \
	uint32_t _facing; \
	Number _alpha; \
	uint32_t _color; \
	Number _bakedRotation; \
	BitmapData *_pixels; \
	Point _flashPoint; \
	Rectangle _flashRect; \
	Rectangle _flashRect2; \
	Point _flashPointZero; \
	ColorTransform *_colorTransform; \
	uint32_t _gpuSheetX, _gpuSheetY, _gpuFrameGen, _gpuSheetGen; /* GPU adapter: frame rect in _pixels */ \
	bool _gpuLazy; /* GPU adapter: framePixels not built yet (FlxSprite_gpuFramePixels) */ \
	Matrix _matrix;

typedef struct FlxSprite {
	FLXSPRITE_FIELDS
} FlxSprite;

extern const As3Class FlxSprite_class;
extern const char *FlxSprite_ImgDefault;
void FlxSprite_ctor(FlxSprite *self, Number X, Number Y, const char *SimpleGraphic);
FlxSprite *FlxSprite_new(Number X, Number Y, const char *SimpleGraphic);
void FlxSprite_destroy(FlxSprite *self);
FlxSprite *FlxSprite_loadGraphic(FlxSprite *self, const char *Graphic, bool Animated, bool Reverse, uint32_t Width, uint32_t Height, bool Unique);
FlxSprite *FlxSprite_loadRotatedGraphic(FlxSprite *self, const char *Graphic, uint32_t Rotations, int32_t Frame, bool AntiAliasing, bool AutoBuffer);
FlxSprite *FlxSprite_makeGraphic(FlxSprite *self, uint32_t Width, uint32_t Height, uint32_t Color, bool Unique, const char *Key);
void FlxSprite_resetHelpers(FlxSprite *self);
void FlxSprite_postUpdate(FlxSprite *self);
void FlxSprite_draw(FlxSprite *self);
void FlxSprite_stamp(FlxSprite *self, FlxSprite *Brush, int32_t X, int32_t Y);
void FlxSprite_fill(FlxSprite *self, uint32_t Color);
void FlxSprite_updateAnimation(FlxSprite *self);
void FlxSprite_drawFrame(FlxSprite *self, bool Force);
void FlxSprite_addAnimation(FlxSprite *self, const char *Name, const int32_t *Frames, uint32_t FramesLength, Number FrameRate, bool Looped);
void FlxSprite_addAnimationCallback(FlxSprite *self, As3AnimCallback AnimationCallback);
void FlxSprite_play(FlxSprite *self, const char *AnimName, bool Force);
void FlxSprite_randomFrame(FlxSprite *self);
void FlxSprite_setOriginToCorner(FlxSprite *self);
void FlxSprite_centerOffsets(FlxSprite *self, bool AdjustPosition);
As3Array *FlxSprite_replaceColor(FlxSprite *self, uint32_t Color, uint32_t NewColor, bool FetchPositions);
BitmapData *FlxSprite_get_pixels(FlxSprite *self);
void FlxSprite_set_pixels(FlxSprite *self, BitmapData *Pixels);
uint32_t FlxSprite_get_facing(FlxSprite *self);
void FlxSprite_set_facing(FlxSprite *self, uint32_t Direction);
Number FlxSprite_get_alpha(FlxSprite *self);
void FlxSprite_set_alpha(FlxSprite *self, Number Alpha);
uint32_t FlxSprite_get_color(FlxSprite *self);
void FlxSprite_set_color(FlxSprite *self, uint32_t Color);
uint32_t FlxSprite_get_frame(FlxSprite *self);
void FlxSprite_set_frame(FlxSprite *self, uint32_t Frame);
bool FlxSprite_onScreen(FlxSprite *self, struct FlxCamera *Camera);
BitmapData *FlxSprite_gpuFramePixels(FlxSprite *self); /* GPU adapter */
void FlxSprite_calcFrame(FlxSprite *self);

/* ---- FlxParticle ---------------------------------------------------------- */

#define FLXPARTICLE_FIELDS \
	FLXSPRITE_FIELDS \
	Number lifespan; \
	Number friction;

typedef struct FlxParticle {
	FLXPARTICLE_FIELDS
} FlxParticle;

extern const As3Class FlxParticle_class;
FlxParticle *FlxParticle_new(void);
void FlxParticle_ctor(FlxParticle *self);

/* test-only: FlxG.overlap notifies each pair once (Flixel: once per shared leaf) */
void FlxG_watch(void *AnyObject, const char *VariableName, const char *DisplayName);
extern bool FlxQuadTree_testOneCallbackPerPair;
extern bool FlxQuadTree_testLogEnabled;
extern const void *FlxQuadTree_testLog[256][2];
extern int FlxQuadTree_testLogCount;

/* test-only probe at the start of the current state's FlxGroup.update */
extern void (*FlxGroup_stateUpdateProbe)(void);

/* 3DS stereo adapter (platform/common/stereo.c): right-eye shift per object */
extern int32_t (*Stereo_depth)(FlxObject *object);
void FlxParticle_update(FlxParticle *self);
void FlxParticle_onEmit(FlxParticle *self);

/* ---- FlxGroup / FlxState ------------------------------------------------- */

#define FlxGroup_ASCENDING (-1)
#define FlxGroup_DESCENDING 1

#define FLXGROUP_FIELDS \
	FLXBASIC_FIELDS \
	As3Array members; \
	Number length; \
	uint32_t _maxSize; \
	uint32_t _marker; \
	const char *_sortIndex; \
	int32_t _sortOrder;

typedef struct FlxGroup {
	FLXGROUP_FIELDS
} FlxGroup;

extern const As3Class FlxGroup_class;
void FlxGroup_ctor(FlxGroup *self, uint32_t MaxSize);
FlxGroup *FlxGroup_new(uint32_t MaxSize);
void FlxGroup_destroy(FlxGroup *self);
void FlxGroup_preUpdate(FlxGroup *self);
void FlxGroup_update(FlxGroup *self);
void FlxGroup_draw(FlxGroup *self);
uint32_t FlxGroup_get_maxSize(FlxGroup *self);
void FlxGroup_set_maxSize(FlxGroup *self, uint32_t Size);
FlxBasic *FlxGroup_add(FlxGroup *self, FlxBasic *Object);
FlxBasic *FlxGroup_recycle(FlxGroup *self, const As3Class *ObjectClass);
FlxBasic *FlxGroup_remove(FlxGroup *self, FlxBasic *Object, bool Splice);
FlxBasic *FlxGroup_replace(FlxGroup *self, FlxBasic *OldObject, FlxBasic *NewObject);
void FlxGroup_sort(FlxGroup *self, const char *Index, int32_t Order);
FlxBasic *FlxGroup_getFirstAvailable(FlxGroup *self, const As3Class *ObjectClass);
int32_t FlxGroup_getFirstNull(FlxGroup *self);
FlxBasic *FlxGroup_getFirstExtant(FlxGroup *self);
FlxBasic *FlxGroup_getFirstAlive(FlxGroup *self);
FlxBasic *FlxGroup_getFirstDead(FlxGroup *self);
int32_t FlxGroup_countLiving(FlxGroup *self);
int32_t FlxGroup_countDead(FlxGroup *self);
FlxBasic *FlxGroup_getRandom(FlxGroup *self, uint32_t StartIndex, uint32_t Length);
void FlxGroup_clear(FlxGroup *self);
/* callAll(FunctionName): the method is passed as a function pointer. */
void FlxGroup_callAll(FlxGroup *self, void (*Function)(FlxBasic *), bool Recurse);
void FlxGroup_kill(FlxGroup *self);

/* AS3 `group.sort(Index)` reads Obj[_sortIndex] by reflection. In C the
   only index Kingdom uses ("y" default) is resolved by this accessor. */
Number FlxGroup_sortValue(FlxBasic *Obj, const char *Index);

#define FLXSTATE_FIELDS FLXGROUP_FIELDS
typedef struct FlxState {
	FLXSTATE_FIELDS
} FlxState;
extern const As3Class FlxState_class;
void FlxState_ctor(FlxState *self);
void FlxState_create(FlxState *self);

/* ---- FlxCamera ------------------------------------------------------------ */

#define FlxCamera_STYLE_LOCKON 0
#define FlxCamera_STYLE_PLATFORMER 1
#define FlxCamera_STYLE_TOPDOWN 2
#define FlxCamera_STYLE_TOPDOWN_TIGHT 3
#define FlxCamera_SHAKE_BOTH_AXES 0
#define FlxCamera_SHAKE_HORIZONTAL_ONLY 1
#define FlxCamera_SHAKE_VERTICAL_ONLY 2

/* The flash.display.Sprite/Bitmap that present the camera buffer. On the
   console the platform layer reads these to place the final image. */
typedef struct FlashDisplayStub {
	Number x, y, scaleX, scaleY, alpha, rotation;
	bool visible;
	bool smoothing;
	ColorTransform colorTransform;
} FlashDisplayStub;

typedef struct FlxCamera {
	FLXBASIC_FIELDS
	Number x;
	Number y;
	uint32_t width;
	uint32_t height;
	FlxObject *target;
	FlxRect *deadzone;
	FlxRect *bounds;
	FlxPoint scroll;
	BitmapData *buffer;
	uint32_t bgColor;
	FlxSprite *screen;
	Number _zoom;
	FlxPoint _point;
	uint32_t _color;
	FlashDisplayStub _flashBitmap;
	FlashDisplayStub _flashSprite;
	Number _flashOffsetX;
	Number _flashOffsetY;
	Rectangle _flashRect;
	Point _flashPoint;
	uint32_t _fxFlashColor;
	Number _fxFlashDuration;
	As3Callback0 _fxFlashComplete;
	Number _fxFlashAlpha;
	uint32_t _fxFadeColor;
	Number _fxFadeDuration;
	As3Callback0 _fxFadeComplete;
	Number _fxFadeAlpha;
	Number _fxShakeIntensity;
	Number _fxShakeDuration;
	As3Callback0 _fxShakeComplete;
	FlxPoint _fxShakeOffset;
	uint32_t _fxShakeDirection;
	BitmapData *_fill;
} FlxCamera;

extern const As3Class FlxCamera_class;
extern Number FlxCamera_defaultZoom;
FlxCamera *FlxCamera_new(int32_t X, int32_t Y, int32_t Width, int32_t Height, Number Zoom);
void FlxCamera_update(FlxCamera *self);
void FlxCamera_follow(FlxCamera *self, FlxObject *Target, uint32_t Style);
void FlxCamera_focusOn(FlxCamera *self, FlxPoint *Point);
void FlxCamera_setBounds(FlxCamera *self, Number X, Number Y, Number Width, Number Height, bool UpdateWorld);
void FlxCamera_flash(FlxCamera *self, uint32_t Color, Number Duration, As3Callback0 OnComplete, bool Force);
void FlxCamera_fade(FlxCamera *self, uint32_t Color, Number Duration, As3Callback0 OnComplete, bool Force);
void FlxCamera_shake(FlxCamera *self, Number Intensity, Number Duration, As3Callback0 OnComplete, bool Force, uint32_t Direction);
void FlxCamera_stopFX(FlxCamera *self);
Number FlxCamera_get_zoom(FlxCamera *self);
void FlxCamera_set_zoom(FlxCamera *self, Number Zoom);
void FlxCamera_setScale(FlxCamera *self, Number X, Number Y);
void FlxCamera_fill(FlxCamera *self, uint32_t Color, bool BlendAlpha);
void FlxCamera_drawFX(FlxCamera *self);

/* ---- FlxU ------------------------------------------------------------------- */

Number FlxU_abs(Number Value);
Number FlxU_floor(Number Value);
Number FlxU_ceil(Number Value);
Number FlxU_round(Number Value);
Number FlxU_min(Number Number1, Number Number2);
Number FlxU_max(Number Number1, Number Number2);
Number FlxU_bound(Number Value, Number Min, Number Max);
Number FlxU_srand(Number Seed);
Number FlxU_computeVelocity(Number Velocity, Number Acceleration, Number Drag, Number Max);
FlxPoint *FlxU_rotatePoint(Number X, Number Y, Number PivotX, Number PivotY, Number Angle, FlxPoint *Point);
Number FlxU_getAngle(FlxPoint *Point1, FlxPoint *Point2);
Number FlxU_getDistance(FlxPoint *Point1, FlxPoint *Point2);
/* never called by the game, translated for function parity. Array results are
   written to a caller (or newly allocated) Number[4]; Object arrays are As3Array. */
Number *FlxU_getRGBA(uint32_t Color, Number *Results);
Number *FlxU_getHSB(uint32_t Color, Number *Results);
uint32_t FlxU_makeColor(uint32_t Red, uint32_t Green, uint32_t Blue, Number Alpha);
uint32_t FlxU_makeColorFromHSB(Number Hue, Number Saturation, Number Brightness, Number Alpha);
char *FlxU_formatTime(Number Seconds, bool ShowMS);
char *FlxU_formatTicks(uint32_t StartTicks, uint32_t EndTicks);
char *FlxU_formatMoney(Number Amount, bool ShowDecimal, bool EnglishStyle);
void *FlxU_getRandom(const As3Array *Objects, uint32_t StartIndex, uint32_t Length);
As3Array *FlxU_shuffle(As3Array *Objects, uint32_t HowManyTimes);
uint32_t FlxU_getTicks(void);

/* ---- FlxG ------------------------------------------------------------------- */

typedef struct FlxKeyboard FlxKeyboard;
typedef struct FlxMouse FlxMouse;

typedef struct FlxG_t {
	bool paused;
	bool debug;
	Number elapsed;
	Number timeScale;
	uint32_t width;
	uint32_t height;
	FlxRect worldBounds;
	uint32_t worldDivisions;
	bool visualDebug;
	bool mobile;
	Number globalSeed;
	struct FlxSound *music;
	FlxGroup *sounds;
	bool mute;
	Number _volume;
	As3Array cameras;
	FlxCamera *camera;
	bool useBufferLocking;
	FlxKeyboard *keys;
	FlxMouse *mouse;
	/* FlxG.state / FlxG._game._state */
	FlxState *state;
	FlxState *_requestedState;
	As3Array plugins; /* FlxG.plugins */
} FlxG_t;
#define FlxG_LIBRARY_NAME "flixel"
#define FlxG_LIBRARY_MAJOR_VERSION 2
#define FlxG_LIBRARY_MINOR_VERSION 55

extern FlxG_t FlxG;

void FlxG_log(const char *Data);
Number FlxG_random(void);
void *FlxG_getRandom(As3Array *Objects, uint32_t StartIndex, uint32_t Length);
bool FlxG_checkBitmapCache(const char *Key);
BitmapData *FlxG_createBitmap(uint32_t Width, uint32_t Height, uint32_t Color, bool Unique, const char *Key);
BitmapData *FlxG_addBitmap(const char *Graphic, bool Reverse, bool Unique, const char *Key);
void FlxG_clearBitmapCache(void);
void FlxG_switchState(FlxState *State);
FlxCamera *FlxG_addCamera(FlxCamera *NewCamera);
void FlxG_resetCameras(FlxCamera *NewCamera);
void FlxG_flash(uint32_t Color, Number Duration, As3Callback0 OnComplete, bool Force);
void FlxG_fade(uint32_t Color, Number Duration, As3Callback0 OnComplete, bool Force);
void FlxG_shake(Number Intensity, Number Duration, As3Callback0 OnComplete, bool Force, uint32_t Direction);
uint32_t FlxG_get_bgColor(void);
void FlxG_set_bgColor(uint32_t Color);
bool FlxG_overlap(FlxBasic *ObjectOrGroup1, FlxBasic *ObjectOrGroup2, As3Callback2 NotifyCallback, As3Callback2 ProcessCallback);
bool FlxG_collide(FlxBasic *ObjectOrGroup1, FlxBasic *ObjectOrGroup2, As3Callback2 NotifyCallback);
void FlxG_init(uint32_t Width, uint32_t Height, Number Zoom);
void FlxG_reset(void);
void FlxG_lockCameras(void);
void FlxG_unlockCameras(void);
void FlxG_updateCameras(void);

/* ---- FlxList / FlxQuadTree (org.flixel.system) ----------------------------- */

typedef struct FlxList {
	FlxObject *object;
	struct FlxList *next;
} FlxList;
FlxList *FlxList_new(void);
void FlxList_destroy(FlxList *self);

#define FlxQuadTree_A_LIST 0
#define FlxQuadTree_B_LIST 1

typedef struct FlxQuadTree {
	Number x, y, width, height; /* extends FlxRect */
	bool _canSubdivide;
	FlxList *_headA;
	FlxList *_tailA;
	FlxList *_headB;
	FlxList *_tailB;
	struct FlxQuadTree *_northWestTree;
	struct FlxQuadTree *_northEastTree;
	struct FlxQuadTree *_southEastTree;
	struct FlxQuadTree *_southWestTree;
	Number _leftEdge;
	Number _rightEdge;
	Number _topEdge;
	Number _bottomEdge;
	Number _halfWidth;
	Number _halfHeight;
	Number _midpointX;
	Number _midpointY;
} FlxQuadTree;

extern uint32_t FlxQuadTree_divisions;
FlxQuadTree *FlxQuadTree_new(Number X, Number Y, Number Width, Number Height, FlxQuadTree *Parent);
void FlxQuadTree_destroy(FlxQuadTree *self);
void FlxQuadTree_load(FlxQuadTree *self, FlxBasic *ObjectOrGroup1, FlxBasic *ObjectOrGroup2, As3Callback2 NotifyCallback, As3Callback2 ProcessCallback);
void FlxQuadTree_add(FlxQuadTree *self, FlxBasic *ObjectOrGroup, uint32_t List);
void FlxQuadTree_addObject(FlxQuadTree *self);
void FlxQuadTree_addToList(FlxQuadTree *self);
bool FlxQuadTree_execute(FlxQuadTree *self);
bool FlxQuadTree_overlapNode(FlxQuadTree *self);

/* ---- FlxGame main loop (only the parts that drive the state) --------------- */

typedef struct FlxGame {
	uint32_t _step;
	int32_t _accumulator;
	uint32_t _maxAccumulation;
	uint32_t _flashFramerate;
	FlxState *_state;
	bool _requestedReset;
	FlxState *(*_iState)(void);
} FlxGame;
extern FlxGame *FlxG__game;
FlxGame *FlxGame_new(uint32_t GameSizeX, uint32_t GameSizeY, FlxState *(*InitialState)(void), Number Zoom, uint32_t GameFramerate, uint32_t FlashFramerate);
void FlxGame_onEnterFrame(FlxGame *self, uint32_t elapsedMS);
void FlxGame_switchState(FlxGame *self);
void FlxGame_step(FlxGame *self);
void FlxGame_update(FlxGame *self);
void FlxGame_draw(FlxGame *self);

/* members the game never calls (translated for function parity) */
void FlxAnim_destroy(FlxAnim *self);
char *FlxBasic_toString(FlxBasic *self);
typedef void (*FlxBasicSetter)(FlxBasic *basic, const void *Value);
void FlxGroup_setAll(FlxGroup *self, FlxBasicSetter VariableName, const void *Value, bool Recurse);
void FlxCamera_destroy(FlxCamera *self);
FlxCamera *FlxCamera_copyFrom(FlxCamera *self, const FlxCamera *Camera);
Number FlxCamera_get_alpha(FlxCamera *self);
void FlxCamera_set_alpha(FlxCamera *self, Number Alpha);
Number FlxCamera_get_angle(FlxCamera *self);
void FlxCamera_set_angle(FlxCamera *self, Number Angle);
uint32_t FlxCamera_get_color(FlxCamera *self);
void FlxCamera_set_color(FlxCamera *self, uint32_t Color);
bool FlxCamera_get_antialiasing(FlxCamera *self);
void FlxCamera_set_antialiasing(FlxCamera *self, bool Antialiasing);
FlxPoint *FlxCamera_getScale(FlxCamera *self);
FlashDisplayStub *FlxCamera_getContainerSprite(FlxCamera *self);
void FlxSprite_drawLine(FlxSprite *self, Number StartX, Number StartY, Number EndX, Number EndY, uint32_t Color, uint32_t Thickness);
bool FlxSprite_pixelsOverlapPoint(FlxSprite *self, const FlxPoint *Point, uint32_t Mask, FlxCamera *Camera);

Number FlxG_get_framerate(void);
void FlxG_set_framerate(Number Framerate);
Number FlxG_get_flashFramerate(void);
void FlxG_set_flashFramerate(Number Framerate);
FlxState *FlxG_get_state(void);
void *FlxG_get_stage(void);
void FlxG_resetState(void);
void FlxG_resetGame(void);
void FlxG_removeCamera(FlxCamera *Camera, bool Destroy);
As3Array *FlxG_shuffle(As3Array *Objects, uint32_t HowManyTimes);
char *FlxG_getLibraryName(void);
FlxBasic *FlxG_addPlugin(FlxBasic *Plugin);
FlxBasic *FlxG_getPlugin(const As3Class *ClassType);
FlxBasic *FlxG_removePlugin(FlxBasic *Plugin);
bool FlxG_removePluginType(const As3Class *ClassType);
void FlxG_updatePlugins(void);
void FlxG_drawPlugins(void);
void FlxG_unwatch(void *AnyObject, const char *VariableName);
void FlxG_resetDebuggerLayout(void);
void FlxG_setDebuggerLayout(uint32_t Layout);

#endif
