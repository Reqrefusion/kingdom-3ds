/* Kingdom game classes (pinned noio/kingdom 56d4e62), one struct per
 * ActionScript class, fields in source order. Inheritance uses the
 * *_FIELDS macros from flixel.h. */
#ifndef TRANSLIT_GAME_H
#define TRANSLIT_GAME_H
#include "../flixel/flixel.h"
#include "../flixel/FlxSound.h"
#include "../flixel/FlxText.h"
#include "../flixel/FlxTilemap.h"
#include "../flixel/FlxInput.h"
#include "../flash/filters.h"
#include "level.h"

typedef struct PlayState PlayState;
typedef struct Citizen Citizen;
typedef struct Weather Weather;
typedef struct Light Light;
typedef struct Castle Castle;
typedef struct Player Player;
typedef struct Troll Troll;
typedef struct Coin Coin;
typedef struct Arrow Arrow;
typedef struct Minimap Minimap;

/* AS3 Math.random(): a second generator, separate from FlxG.random().
   ADAPTATION: Flash's Math.random algorithm is not documented; this is a
   seeded generator so runs are reproducible. */
Number Math_random(void);
void Math_random_seed(uint32_t seed);

/* mochi.as3.MochiDigits: an obfuscated int (anti-cheat). Same value. */
typedef struct MochiDigits {
	int32_t value;
} MochiDigits;
static inline void MochiDigits_addValue(MochiDigits *d, int32_t v) { d->value += v; }
static inline void MochiDigits_setValue(MochiDigits *d, int32_t v) { d->value = v; }

/* ---- com.quasimondo.geom.ColorMatrix ------------------------------------- */

typedef struct ColorMatrix {
	Number matrix[20];
	struct ColorMatrix *preHue; /* initHue() state */
	struct ColorMatrix *postHue;
	bool hueInitialized;
} ColorMatrix;
#define ColorMatrix_LUMA_R 0.212671
#define ColorMatrix_LUMA_G 0.71516
#define ColorMatrix_LUMA_B 0.072169
void ColorMatrix_ctor(ColorMatrix *self);
void ColorMatrix_reset(ColorMatrix *self);
void ColorMatrix_adjustSaturation(ColorMatrix *self, Number s);
void ColorMatrix_adjustContrast(ColorMatrix *self, Number r, Number g, Number b);
void ColorMatrix_colorize(ColorMatrix *self, uint32_t rgb, Number amount);
void ColorMatrix_concat(ColorMatrix *self, const Number mat[20]);
void ColorMatrix_applyFilter(ColorMatrix *self, BitmapData *bitmapData);
/* the remaining Quasimondo operations: never called by the game, translated for
   function parity. Array arguments are Number[]; ColorMatrix(mat) is
   ColorMatrix_ctorFrom, `get filter` returns the 20 coefficients that
   new ColorMatrixFilter(matrix) would take. */
#define ColorMatrix_LUMA_R2 0.3086
#define ColorMatrix_LUMA_G2 0.6094
#define ColorMatrix_LUMA_B2 0.0820
#define ColorMatrix_ONETHIRD (1.0 / 3)
extern const char *const ColorMatrix_COLOR_DEFICIENCY_TYPES[8];
void ColorMatrix_ctorFrom(ColorMatrix *self, const Number *mat);
ColorMatrix *ColorMatrix_clone(ColorMatrix *self);
void ColorMatrix_invert(ColorMatrix *self);
void ColorMatrix_adjustBrightness(ColorMatrix *self, Number r, Number g, Number b);
void ColorMatrix_toGreyscale(ColorMatrix *self, Number r, Number g, Number b);
void ColorMatrix_adjustHue(ColorMatrix *self, Number degrees);
void ColorMatrix_rotateHue(ColorMatrix *self, Number degrees);
void ColorMatrix_luminance2Alpha(ColorMatrix *self);
void ColorMatrix_adjustAlphaContrast(ColorMatrix *self, Number amount);
void ColorMatrix_setChannels(ColorMatrix *self, int32_t r, int32_t g, int32_t b, int32_t a);
void ColorMatrix_blend(ColorMatrix *self, const ColorMatrix *mat, Number amount);
void ColorMatrix_average(ColorMatrix *self, Number r, Number g, Number b);
void ColorMatrix_threshold(ColorMatrix *self, Number threshold, Number factor);
void ColorMatrix_threshold_rgb(ColorMatrix *self, Number threshold, Number factor);
void ColorMatrix_desaturate(ColorMatrix *self);
void ColorMatrix_randomize(ColorMatrix *self, Number amount, bool normalize);
void ColorMatrix_setMultiplicators(ColorMatrix *self, Number red, Number green, Number blue, Number alpha);
void ColorMatrix_clearChannels(ColorMatrix *self, bool red, bool green, bool blue, bool alpha);
void ColorMatrix_thresholdAlpha(ColorMatrix *self, Number threshold, Number factor);
void ColorMatrix_averageRGB2Alpha(ColorMatrix *self);
void ColorMatrix_invertAlpha(ColorMatrix *self);
void ColorMatrix_rgb2Alpha(ColorMatrix *self, Number r, Number g, Number b);
void ColorMatrix_setAlpha(ColorMatrix *self, Number alpha);
const Number *ColorMatrix_get_filter(ColorMatrix *self);
void ColorMatrix_rotateRed(ColorMatrix *self, Number degrees);
void ColorMatrix_rotateGreen(ColorMatrix *self, Number degrees);
void ColorMatrix_rotateBlue(ColorMatrix *self, Number degrees);
void ColorMatrix_normalize(ColorMatrix *self);
void ColorMatrix_fitRange(ColorMatrix *self);
void ColorMatrix_shearRed(ColorMatrix *self, Number green, Number blue);
void ColorMatrix_shearGreen(ColorMatrix *self, Number red, Number blue);
void ColorMatrix_shearBlue(ColorMatrix *self, Number red, Number green);
void ColorMatrix_applyColorDeficiency(ColorMatrix *self, const char *type);
void ColorMatrix_RGB2YUV(ColorMatrix *self);
void ColorMatrix_YUV2RGB(ColorMatrix *self);
void ColorMatrix_RGB2YIQ(ColorMatrix *self);
void ColorMatrix_YIQ2RGB(ColorMatrix *self);
void ColorMatrix_autoDesaturate(ColorMatrix *self, BitmapData *bitmapData, bool stretchLevels, bool outputToBlueOnly);
bool ColorMatrix_invertMatrix(ColorMatrix *self);
uint32_t ColorMatrix_applyMatrix(ColorMatrix *self, uint32_t rgba);
void ColorMatrix_transformVector(ColorMatrix *self, Number *values, uint32_t length);
char *ColorMatrix_toString(ColorMatrix *self);

/* ---- WeatherPresets / Weather -------------------------------------------- */

enum {
	WEATHER_HAS_sky = 1 << 0,
	WEATHER_HAS_horizon = 1 << 1,
	WEATHER_HAS_haze = 1 << 2,
	WEATHER_HAS_darknessColor = 1 << 3,
	WEATHER_HAS_darkness = 1 << 4,
	WEATHER_HAS_contrast = 1 << 5,
	WEATHER_HAS_saturation = 1 << 6,
	WEATHER_HAS_ambient = 1 << 7,
	WEATHER_HAS_wind = 1 << 8,
	WEATHER_HAS_fog = 1 << 9,
	WEATHER_HAS_rain = 1 << 10,
	WEATHER_HAS_timeOfDay = 1 << 11,
	WEATHER_HAS_sunTint = 1 << 12,
	WEATHER_HAS_ambientAmount = 1 << 13,
};

/* A preset Object literal. Values are AS3 Numbers. */
typedef struct WeatherState {
	const char *name;
	uint32_t has;
	Number sky, horizon, haze, darknessColor, darkness, contrast, saturation, ambient, wind, fog, rain,
		timeOfDay, sunTint, ambientAmount;
} WeatherState;
#include "WeatherPresets.h"

struct Weather {
	uint32_t sky;
	uint32_t horizon;
	uint32_t haze;
	uint32_t darknessColor;
	Number darkness;
	Number contrast;
	Number saturation;
	uint32_t ambient;
	Number wind;
	Number fog;
	Number rain;
	Number timeOfDay;
	uint32_t sunTint;
	ColorMatrix ambientTransform;
	Number t;
	Number changed;
	Number progress;
	Number ambientAmount;
	Number tweenStart;
	Number tweenDuration;
	WeatherState *previousState;
	WeatherState *targetState;
};
Weather *Weather_new(void);
void Weather_update(Weather *self);
void Weather_tweenTo(Weather *self, WeatherState *state, Number d);
void Weather_updateTween(Weather *self);
void Weather_setVariables(Weather *self, WeatherState *target, WeatherState *previous, Number f);

/* ---- Utils ------------------------------------------------------------------ */

#define Utils_DEG_TO_RAD (Math_PI / 180)
void Utils_replaceColor(BitmapData *bitmap, uint32_t fromColor, uint32_t toColor);
void Utils_gradientOverlay(BitmapData *bitmap, const uint32_t *colors, uint32_t colorsLength, Number rotation, int32_t chunks);
uint32_t Utils_HSVtoRGB(Number h, Number s, Number v);
uint32_t Utils_interpolateColor(uint32_t color1, uint32_t color2, Number f);
uint32_t Utils_getColor32(uint32_t alpha, uint32_t red, uint32_t green, uint32_t blue);
void Utils_explode(FlxSprite *object, FlxGroup *group, Number portion, int32_t gibsize, bool rounded);
/* never called by the game, translated for function parity */
typedef struct UtilsRGB { uint32_t alpha, red, green, blue; } UtilsRGB; /* getRGB(): Object */
typedef struct UtilsHSV { Number hue, saturation, lightness, value; } UtilsHSV; /* RGBtoHSV(): Object */
void Utils_shrinkHitbox(FlxSprite *sprite, Number minXCover, Number minYCover);
As3Array *Utils_findRectanglesWithColor(BitmapData *bitmap, uint32_t color); /* Vector.<Rectangle>; items are Rectangle* */
UtilsHSV Utils_RGBtoHSV(uint32_t color);
uint32_t Utils_interpolateColorAndAlpha(uint32_t color1, uint32_t color2, uint32_t steps, uint32_t currentStep);
UtilsRGB Utils_getRGB(uint32_t color);
/* returns a static buffer */
const char *Utils_toRoman(int32_t n);

/* ---- small sprites -------------------------------------------------------------- */

typedef struct Attention {
	FLXPARTICLE_FIELDS
	Citizen *citizen;
} Attention;
extern const As3Class Attention_class;
Attention *Attention_new(void);
void Attention_appearAt(Attention *self, Citizen *at);
void Attention_update(Attention *self);

typedef struct Dust {
	FLXPARTICLE_FIELDS
} Dust;
extern const As3Class Dust_class;
Dust *Dust_new(void);
void Dust_reset(Dust *self, Number X, Number Y);

typedef struct Sparkle {
	FLXPARTICLE_FIELDS
} Sparkle;
extern const As3Class Sparkle_class;
Sparkle *Sparkle_new(void);
void Sparkle_reset(Sparkle *self, Number X, Number Y);

typedef struct Splash {
	FLXPARTICLE_FIELDS
} Splash;
extern const As3Class Splash_class;
Splash *Splash_new(void);
void Splash_reset(Splash *self, Number X, Number Y);

struct Coin {
	FLXPARTICLE_FIELDS
	FlxObject *owner;
	bool justThrown;
	bool called;
};
#define Coin_TOTAL_LIFESPAN 25.0
#define Coin_OWNER_LIFESPAN 4.0
extern const As3Class Coin_class;
Coin *Coin_new(void);
Coin *Coin_drop(Coin *self, FlxSprite *from, FlxObject *owner, bool far);
void Coin_update(Coin *self);

struct Arrow {
	FLXPARTICLE_FIELDS
	Citizen *shooter;
};
extern const As3Class Arrow_class;
Arrow *Arrow_new(void);
void Arrow_shotFrom(Arrow *self, Citizen *from, FlxObject *at);
void Arrow_update(Arrow *self);

typedef struct Bunny {
	FLXSPRITE_FIELDS
	Number DECAY_TIME;
	Number TURN_TIME;
	Number t;
} Bunny;
extern const As3Class Bunny_class;
Bunny *Bunny_new(int32_t X, int32_t Y);
void Bunny_getShot(Bunny *self, Arrow *arrow);
void Bunny_update(Bunny *self);

typedef struct CoinFloat {
	FLXSPRITE_FIELDS
} CoinFloat;
extern const As3Class CoinFloat_class;
CoinFloat *CoinFloat_new(void);
void CoinFloat_float(CoinFloat *self, FlxSprite *above);

typedef struct Coinsack {
	FLXSPRITE_FIELDS
} Coinsack;
#define Coinsack_FADE_TIME 20.0
extern Number Coinsack_t;
extern const As3Class Coinsack_class;
Coinsack *Coinsack_new(Number X, Number Y);
void Coinsack_show(Coinsack *self, int32_t c);
void Coinsack_update(Coinsack *self);

typedef struct Scaffold {
	FLXSPRITE_FIELDS
} Scaffold;
extern const As3Class Scaffold_class;
Scaffold *Scaffold_new(void);
Scaffold *Scaffold_build(Scaffold *self, FlxSprite *over);

typedef struct Treeline {
	FLXSPRITE_FIELDS
} Treeline;
extern const As3Class Treeline_class;
Treeline *Treeline_new(int32_t X, int32_t Y);

typedef struct Reed {
	FLXSPRITE_FIELDS
	Weather *weather;
	Number weatherChanged;
	Number t;
} Reed;
extern const As3Class Reed_class;
Reed *Reed_new(int32_t X, int32_t Y);
void Reed_update(Reed *self);

typedef struct Farmland {
	FLXSPRITE_FIELDS
	Number t;
} Farmland;
#define Farmland_WORK_COOLDOWN 8.0
extern const As3Class Farmland_class;
Farmland *Farmland_new(int32_t X, int32_t Y);
bool Farmland_needsWork(Farmland *self);
void Farmland_work(Farmland *self, Citizen *by);
void Farmland_update(Farmland *self);

typedef struct CameraTarget {
	FLXOBJECT_FIELDS
	Number lead;
	FlxPoint speed;
	FlxPoint maxSpeed;
	FlxPoint offset;
	FlxSprite *_target;
	Number _targetX;
	Number _targetY;
} CameraTarget;
extern const As3Class CameraTarget_class;
CameraTarget *CameraTarget_new(void);
void CameraTarget_set_target(CameraTarget *self, FlxSprite *object);
FlxSprite *CameraTarget_get_target(CameraTarget *self);
void CameraTarget_snap(CameraTarget *self);
void CameraTarget_update(CameraTarget *self);
void CameraTarget_draw(CameraTarget *self);

/* ---- scenery ------------------------------------------------------------------- */

typedef struct Sky {
	FLXSPRITE_FIELDS
	Weather *weather;
	Number weatherChanged;
} Sky;
extern const As3Class Sky_class;
Sky *Sky_new(Weather *weather);
void Sky_update(Sky *self);

typedef struct Haze {
	FLXSPRITE_FIELDS
	Weather *weather;
	Number weatherChanged;
} Haze;
extern const As3Class Haze_class;
Haze *Haze_new(int32_t X, int32_t Y, Weather *weather);
void Haze_draw(Haze *self);

typedef struct FlxBackdrop {
	FLXSPRITE_FIELDS
	int32_t w;
} FlxBackdrop;
extern const As3Class FlxBackdrop_class;
FlxBackdrop *FlxBackdrop_new(const char *graphicClass, Number XScroll, Number YScroll, uint32_t Color);
void FlxBackdrop_update(FlxBackdrop *self);

typedef struct Fog {
	FLXGROUP_FIELDS
	Weather *weather;
	Number weatherChanged;
	FlxSprite *_fg;
	FlxPoint _point;
} Fog;
#define Fog_MAXFOG 5
extern const As3Class Fog_class;
Fog *Fog_new(Weather *weather);
void Fog_update(Fog *self);

typedef struct Water {
	FLXSPRITE_FIELDS
	Rectangle rect;
	Point zeroPoint;
	Point perlinOffset;
	Matrix matrix;
	ColorTransform transform;
	ColorTransform noiseRange;
	DisplacementMapFilter *displacementFilter;
	BitmapData *displacementBitmap;
	uint32_t baseColor;
	uint32_t currentBase;
	Number timer;
	FlxGroup *lights;
	Weather *weather;
	Number weatherChanged;
} Water;
#define Water_NOISE_BIAS 100
#define Water_WIND_RIPPLE_MULTIPLIER 25.0
extern const As3Class Water_class;
Water *Water_new(int32_t x, int32_t y, int32_t width, int32_t height, FlxGroup *lights, Weather *weather, uint32_t baseColor, Number reflectivity);
void Water_update(Water *self);
void Water_draw(Water *self);

/* ---- lights ---------------------------------------------------------------------- */

#define LIGHT_FIELDS \
	FLXSPRITE_FIELDS \
	FlxSprite *beam; \
	FlxSprite *reflected; \
	FlxSprite *darkness; \
	bool burning; \
	PlayState *playstate; \
	Weather *weather;

struct Light {
	LIGHT_FIELDS
};
extern const As3Class Light_class;
void Light_ctor(Light *self, Number X, Number Y);
void Light_setLight(Light *self);
void Light_update(Light *self);
void Light_draw(Light *self);
void Light_dim(void *self, const char *animName, uint32_t frameNumber, uint32_t frameIndex);
#define LIGHT_VT \
	FLXSPRITE_VT, \
	.update = (void (*)(FlxBasic *))Light_update, \
	.draw = (void (*)(FlxBasic *))Light_draw

typedef struct Campfire {
	LIGHT_FIELDS
} Campfire;
extern const As3Class Campfire_class;
Campfire *Campfire_new(Number X, Number Y);

typedef struct Firefly {
	LIGHT_FIELDS
	Number weatherChanged;
	FlxPoint startPos;
} Firefly;
#define Firefly_COLOR 0xFF7affa0u
#define Firefly_MAX_DIST 100.0
extern const As3Class Firefly_class;
Firefly *Firefly_new(Number X, Number Y);
void Firefly_update(Firefly *self);

typedef struct SunMoon {
	LIGHT_FIELDS
	Number weatherChanged;
} SunMoon;
#define SunMoon_ZENITH 20.0
#define SunMoon_HORIZON 100.0
extern const As3Class SunMoon_class;
SunMoon *SunMoon_new(Weather *weather);
void SunMoon_update(SunMoon *self);

typedef struct Torch {
	LIGHT_FIELDS
} Torch;
extern const As3Class Torch_class;
Torch *Torch_new(Number X, Number Y);
void Torch_update(Torch *self);

/* ---- buildings ---------------------------------------------------------------------- */

#define Castle_POST 0
#define Castle_PLATFORM 1
#define Castle_WATCHTOWER 2
#define Castle_STONETOWER 3
#define Castle_CASTLE 4
struct Castle {
	FLXSPRITE_FIELDS
	PlayState *playstate;
	Light *light;
	FlxGroup *lights;
	Number t;
	int32_t stage;
	FlxGroup *archers;
	As3Array archer_positions; /* of FlxPoint* */
	int32_t capacity;
	Number baseY;
};
extern const As3Class Castle_class;
Castle *Castle_new(Number X, Number Y);
void Castle_morph(Castle *self, int32_t stage);
void Castle_build(Castle *self);
bool Castle_canBuild(Castle *self);
void Castle_update(Castle *self);

#define Shop_SCYTHES 1
#define Shop_BOWS 2
typedef struct Shop {
	FLXSPRITE_FIELDS
	int32_t type;
	int32_t supply;
} Shop;
extern const As3Class Shop_class;
Shop *Shop_new(int32_t X, int32_t Y);
void Shop_update(Shop *self);
bool Shop_equip(void *self, FlxObject *shop, FlxObject *chr);
void Shop_setSupply(Shop *self, int32_t s);
void Shop_updateAppearance(Shop *self);
bool Shop_canBuild(Shop *self);
void Shop_build(Shop *self);

typedef struct Wall {
	FLXSPRITE_FIELDS
	PlayState *playstate;
	Scaffold *scaffold;
	bool building;
	int32_t heightToBuild;
	Number baseY;
	int32_t stage;
	Number t;
} Wall;
extern const int32_t Wall_HEIGHT[5];
extern const int32_t Wall_HEALTH[5];
#define Wall_HURT_COOLDOWN 1.0
#define Wall_WORK_BUILD_HEIGHT 10
#define Wall_WORK_HEAL_AMOUNT 4
extern const As3Class Wall_class;
Wall *Wall_new(Number X, Number Y);
void Wall_build(Wall *self);
void Wall_buildTo(Wall *self, int32_t s, bool instant);
void Wall_work(Wall *self, Citizen *citizen);
void Wall_hurt(Wall *self, Number Damage);
bool Wall_needsWork(Wall *self);
bool Wall_canBuild(Wall *self);
void Wall_updateAppearance(Wall *self);
void Wall_update(Wall *self);

/* interfaces Buildable / Workable: dispatch on the implementing class */
bool Buildable_canBuild(FlxBasic *b);
void Buildable_build(FlxBasic *b);
bool Workable_needsWork(FlxBasic *w);
void Workable_work(FlxBasic *w, Citizen *citizen);

/* ---- characters ----------------------------------------------------------------- */

#define Citizen_BEGGAR 0
#define Citizen_POOR 1
#define Citizen_FARMER 2
#define Citizen_HUNTER 3
#define Citizen_IDLE 0
#define Citizen_SHOOT 1
#define Citizen_JUST_SHOT 2
#define Citizen_SHOVEL 3
#define Citizen_GIVE 4
#define Citizen_JUST_HACKED 5
#define Citizen_COWER 6
struct Citizen {
	FLXSPRITE_FIELDS
	int32_t occupation;
	int32_t action;
	bool guarding;
	Number t;
	Number goal;
	uint32_t myColor;
	uint32_t skin;
	int32_t coins;
	Number giveCooldown;
	Number shovelCooldown;
	FlxObject *target;
	bool guardLeftBorder;
	int32_t hungry;
	PlayState *playstate;
	Castle *castle;
};
extern const As3Class Citizen_class;
Citizen *Citizen_new(int32_t X, int32_t Y);
Citizen *Citizen_morph(Citizen *self, int32_t occ);
void Citizen_pickup(Citizen *self, FlxObject *coin);
void Citizen_giveTaxes(Citizen *self, Player *p);
void Citizen_hitByTroll(Citizen *self, Troll *troll);
void Citizen_checkShootable(Citizen *self, FlxGroup *group);
void Citizen_checkWork(Citizen *self, FlxGroup *group);
void Citizen_checkGuard(Citizen *self);
void Citizen_leaveGuard(Citizen *self);
void Citizen_pickNewGoal(Citizen *self, Number preset);
void Citizen_animationFrame(void *self, const char *animName, uint32_t frameNum, uint32_t frameIndex);
void Citizen_update(Citizen *self);

struct Player {
	FLXSPRITE_FIELDS
	PlayState *playstate;
	FlxSprite *selectedBuilding;
	CoinFloat *floatCoin;
	Number lastMoved;
	Number food;
	bool hasCrown;
	Number lastTrollHit;
	int32_t coins;
};
#define Player_MAX_SPEED 80.0
#define Player_MIN_SPEED 25.0
#define Player_MAX_FOOD_BONUS 50.0
#define Player_MAX_FOOD 100.0
#define Player_HIT_RATE 0.2
#define Player_SELECT_DISTANCE 10.0
extern const As3Class Player_class;
Player *Player_new(int32_t X, int32_t Y);
void Player_changeCoins(Player *self, int32_t amt);
void Player_hitByTroll(Player *self, Troll *troll);
void Player_lostCrown(Player *self, FlxObject *troll);
void Player_pickup(Player *self, FlxObject *coin);
void Player_update(Player *self);

struct Troll {
	FLXSPRITE_FIELDS
	Number t;
	Number goal;
	bool hasCoin;
	bool hasCrown;
	bool wait;
	bool retreating;
	Number maxSpeed;
	Number jumpHeight;
	Number jumpiness;
	Number confusion;
	bool big;
	Number safeDistance;
	Number maxHeightReached;
	Number jumpCooldown;
	Number confuseCooldown;
	PlayState *playstate;
};
extern const As3Class Troll_class;
Troll *Troll_new(void);
void Troll_reset(Troll *self, Number X, Number Y);
void Troll_getsCoin(Troll *self);
void Troll_pickup(Troll *self, FlxObject *coin);
void Troll_stealCrown(Troll *self);
void Troll_getShot(Troll *self);
void Troll_kill(Troll *self);
void Troll_retreat(Troll *self);
void Troll_go(Troll *self);
void Troll_update(Troll *self);

/* ---- states ---------------------------------------------------------------------- */

/* One row of PlayState.PHASES: [weather, duration, tweenTime, callback, music] */
typedef struct Phase {
	WeatherState *weather;
	Number duration;
	bool hasTweenTime;
	Number tweenTime;
	void (*callback)(PlayState *self);
	const char *music;
} Phase;

struct PlayState {
	FLXSTATE_FIELDS
	Minimap *minimap; /* CHEATS only */
	FlxSprite *sky; /* Sky */
	SunMoon *sunmoon;
	FlxBackdrop *backdropFar;
	FlxBackdrop *backdropClose;
	FlxGroup *backdrop;
	Haze *haze;
	FlxSprite *player;
	FlxGroup *bunnies;
	FlxGroup *farmland;
	FlxGroup *coins;
	FlxGroup *beggars;
	FlxGroup *characters;
	FlxGroup *trolls;
	FlxGroup *trollsNoCollide;
	FlxGroup *gibs;
	FlxGroup *indicators;
	FlxGroup *walls;
	FlxGroup *level;
	FlxGroup *archers;
	FlxGroup *objects;
	FlxGroup *shops;
	FlxTilemap *floor;
	FlxGroup *farmlands;
	FlxGroup *props;
	FlxGroup *lights;
	FlxSprite *darkness;
	Water *water;
	FlxGroup *arrows;
	FlxGroup *fx;
	Fog *fog;
	FlxText *text;
	FlxText *centerText;
	Coinsack *sack;
	FlxSprite *noise;
	Weather *weather;
	Castle *castle;
	Number kingdomLeft;
	Number kingdomRight;
	int32_t groundHeight;
	int32_t phase;
	bool phasesPaused;
	Number timeToNextPhase;
	Number bunnySpawnTimer;
	Number trollSpawnTimer;
	As3Array trollsToSpawn;
	int32_t minBeggars;
	Number retreatDelay;
	bool gameover;
	MochiDigits day;
	Number trollHealth;
	Number trollMaxSpeed;
	Number trollJumpHeight;
	Number trollJumpiness;
	Number trollConfusion;
	bool trollBig;
	int32_t *grassTiles;
	uint32_t grassTiles_length;
	bool reachedVillage;
	bool recruitedCitizen;
	bool boughtItem;
	bool buyBowAdvice;
	bool buyScytheAdvice;
	bool expandedKingdomAdvice;
	bool horseAdvice;
	bool outOfGoldAdvice;
	char *savedProgress;
	char *restoreProgress;
	Number textTimeout;
	As3Array textQueue; /* of char* */
	CameraTarget *cameraTarget;
	Number cameraTimeout;
	FlxSound *music;
	FlxSound *cicada;
	FlxSound *owls;
	FlxSound *birds;
	bool cheatNoTrolls;
	bool untouchable;
	Phase PHASES[64];
	uint32_t PHASES_length;
	Phase PHASES_CYCLE[5];
};
/* ---- Minimap (debug, CHEATS only) ---- */
struct Minimap {
	FLXSPRITE_FIELDS
	As3Array members;
	As3Array colors;
};
extern const As3Class Minimap_class;
Minimap *Minimap_new(Number X, Number Y, int32_t w, int32_t h);
void Minimap_add(Minimap *self, FlxBasic *member, uint32_t color);
void Minimap_draw(Minimap *self);
void Minimap_drawDot(Minimap *self, FlxBasic *m, uint32_t color);

#define PlayState_CHEATS false
#define PlayState_WEATHERCONTROLS false
#define PlayState_GAME_WIDTH 3840
#define PlayState_MIN_KINGDOM_WIDTH 200
#define PlayState_MAX_BUNNIES 50
#define PlayState_MIN_BUNNY_SPAWNTIME 6.0
#define PlayState_MIN_TROLL_SPAWNTIME 1.0
#define PlayState_TROLL_WALL_DAMAGE 2.0
#define PlayState_TEXT_MAX_ALPHA 0.7
#define PlayState_TEXT_READ_SPEED 0.20
#define PlayState_TEXT_MIN_TIME 6.0
extern const As3Class PlayState_class;
PlayState *PlayState_new(const char *progress);
void PlayState_create(PlayState *self);
void PlayState_progressAll(PlayState *self);
void PlayState_buildLevel(PlayState *self, const OelLevel *levelXML);
void PlayState_buildObjects(PlayState *self, const OelSection *nodes, const char *onlyType, FlxGroup *group);
void PlayState_update(PlayState *self);
void PlayState_nextPhase(PlayState *self);
void PlayState_updateEnvironmentSounds(PlayState *self);
void PlayState_trollStats(PlayState *self, Number speed, Number health, Number jumpheight, Number jumpiness, bool big, Number confusion);
void PlayState_spawnTrolls(PlayState *self, int32_t amount);
void PlayState_updateTrollSpawn(PlayState *self);
void PlayState_daybreak(PlayState *self);
void PlayState_saveProgress(PlayState *self);
void PlayState_setProgress(PlayState *self, const char *s);
void PlayState_trollRetreat(PlayState *self, Number delay);
void PlayState_crownStolen(PlayState *self);
void PlayState_endGame(void *self);
void PlayState_draw(PlayState *self);
void PlayState_showCoins(PlayState *self);
void PlayState_showText(PlayState *self, const char *t);
void PlayState_showCenterText(PlayState *self, const char *t);
void PlayState_panTo(PlayState *self, FlxSprite *o, Number duration, Number lead);

typedef struct MenuState {
	FLXSTATE_FIELDS
	FlxSprite *noioHighlight;
	FlxSprite *pezHighlight;
} MenuState;
extern const As3Class MenuState_class;
MenuState *MenuState_new(void);
void MenuState_create(MenuState *self);
void MenuState_update(MenuState *self);

typedef struct GameOverState {
	FLXSTATE_FIELDS
	int32_t nights;
} GameOverState;
extern const As3Class GameOverState_class;
GameOverState *GameOverState_new(int32_t nightsSurvived);
void GameOverState_create(GameOverState *self);
void GameOverState_update(GameOverState *self);

/* king.as */
#define king_VERSION "v1.1.3"
FlxGame *king_new(void);

/* FlxG.state as PlayState */
#define PLAYSTATE ((PlayState *)FlxG.state)

#endif
