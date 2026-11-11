/* PlayState.as
 * Not translated (desktop/debug only, unreachable with CHEATS = false and
 * WEATHERCONTROLS = false): setWeatherFromInput (JSON text field), Minimap,
 * FlxG.watch calls, the "S" fullscreen toggle. */
#include "game.h"

/* Graphics */
static const char *TilesImg = "/assets/gfx/tiles.png";
static const char *SkylineHillsImg = "/assets/gfx/skyline_hills.png";
static const char *SkylineTreesImg = "/assets/gfx/skyline_trees.png";
static const char *HillImg = "/assets/gfx/hill.png";
/* Sounds */
static const char *HitSound = "/assets/sound/hit.mp3";
static const char *HitbigSound = "/assets/sound/hitbig.mp3";
/* Env sounds */
static const char *CicadaSound = "/assets/sound/cicada.mp3";
static const char *OwlsSound = "/assets/sound/owls.mp3";
static const char *BirdsSound = "/assets/sound/birds.mp3";
/* Music */
static const char *MusicNight2 = "/assets/music/night2.mp3";
static const char *MusicNight3 = "/assets/music/night3.mp3";
static const char *MusicNight4 = "/assets/music/night4.mp3";
static const char *MusicNight5 = "/assets/music/night5.mp3";
static const char *MusicDay1 = "/assets/music/day1.mp3";
static const char *MusicDay2 = "/assets/music/day2.mp3";
static const char *MusicDay3 = "/assets/music/day3.mp3";
static const char *MusicDay4 = "/assets/music/day4.mp3";
static const char *MusicDay5 = "/assets/music/day5.mp3";

/* this[oel.@backdropFarImg] */
static const char *PlayState_imageByName(const char *name)
{
	if (as3_streq(name, "SkylineHillsImg")) return SkylineHillsImg;
	if (as3_streq(name, "SkylineTreesImg")) return SkylineTreesImg;
	if (as3_streq(name, "HillImg")) return HillImg;
	if (as3_streq(name, "TilesImg")) return TilesImg;
	return NULL;
}

static void phaseFirst(PlayState *self);
static void phaseBeforeNightOne(PlayState *self);
static void phaseNightOne(PlayState *self);
static void phaseNightTwo(PlayState *self);
static void phaseNightThree(PlayState *self);
static void phaseNightFour(PlayState *self);
static void phaseNightFive(PlayState *self);
static void phaseNightSix(PlayState *self);
static void phaseNightSeven(PlayState *self);
static void phaseNightEight(PlayState *self);
static void phaseNightNine(PlayState *self);
static void phaseNightTen(PlayState *self);
static void phaseNightCycle(PlayState *self);
static void daybreak(PlayState *self) { PlayState_daybreak(self); }
static void trollRetreat(PlayState *self) { PlayState_trollRetreat(self, 10); }
static bool pickUpCoin(void *self, FlxObject *coin, FlxObject *chr);
static bool giveTaxes(void *self, FlxObject *chr, FlxObject *player);
static bool trollWall(void *self, FlxObject *troll, FlxObject *wall);
static bool trollShot(void *self, FlxObject *arrow, FlxObject *troll);
static bool bunnyShot(void *self, FlxObject *arrow, FlxObject *bunny);
static bool trollHit(void *self, FlxObject *troll, FlxObject *chr);

static FlxBasic *PlayState_construct(void)
{
	return (FlxBasic *)PlayState_new(NULL);
}

const As3Class PlayState_class = {
	"PlayState", &FlxState_class, PlayState_construct,
	{
		FLXSTATE_VT,
		.create = (void (*)(FlxBasic *))PlayState_create,
		.update = (void (*)(FlxBasic *))PlayState_update,
		.draw = (void (*)(FlxBasic *))PlayState_draw,
	},
};

#define P(w, d, tween, fn, music) {(w), (d), (tween) >= 0, (tween), (fn), (music)}
#define NO_TWEEN -1

PlayState *PlayState_new(const char *progress)
{
	PlayState *self = as3_alloc(sizeof *self);
	self->klass = &PlayState_class;
	/* Game vars */
	self->kingdomLeft = 1920 - 200;
	self->kingdomRight = 1920 + 200;
	self->groundHeight = 132;
	self->phase = 0;
	self->phasesPaused = false;
	self->timeToNextPhase = 0;
	self->bunnySpawnTimer = 0.0;
	self->trollSpawnTimer = 0.0;
	self->trollsToSpawn = (As3Array){0};
	self->minBeggars = 0;
	self->retreatDelay = 0;
	self->gameover = false;
	self->day = (MochiDigits){0};
	self->trollHealth = 1;
	self->trollMaxSpeed = 20;
	self->trollJumpHeight = 20;
	self->trollJumpiness = 30;
	self->trollConfusion = 30;
	self->trollBig = false;
	/* Progress variables */
	self->reachedVillage = false;
	self->recruitedCitizen = false;
	self->boughtItem = false;
	self->buyBowAdvice = false;
	self->buyScytheAdvice = false;
	self->expandedKingdomAdvice = false;
	self->horseAdvice = false;
	self->outOfGoldAdvice = false;
	self->savedProgress = NULL;
	self->restoreProgress = NULL;
	/* Internals */
	self->textTimeout = 0;
	self->textQueue = (As3Array){0};
	self->cameraTimeout = 0;
	self->music = NULL;
	self->cicada = NULL;
	self->owls = NULL;
	self->birds = NULL;
	self->cheatNoTrolls = false;
	self->untouchable = false;

	const Phase phases[] = {
		/* INTRO (0-3) */
		P(WeatherPresets_FOGGY, 10, NO_TWEEN, phaseFirst, NULL),
		/* ONE (4-9) */
		P(WeatherPresets_DAWN, 25, NO_TWEEN, daybreak, NULL),
		P(WeatherPresets_SUNNY, 30, NO_TWEEN, NULL, NULL),
		P(WeatherPresets_EVENING, 20, NO_TWEEN, NULL, NULL),
		P(WeatherPresets_NIGHT, 20, NO_TWEEN, phaseBeforeNightOne, MusicNight2),
		P(NULL, 50, NO_TWEEN, phaseNightOne, NULL),
		/* TWO (10-14) */
		P(WeatherPresets_DAWNLIGHTPINK, 20, NO_TWEEN, daybreak, NULL),
		P(WeatherPresets_DAYWINDYCLEAR, 30, NO_TWEEN, NULL, MusicDay1),
		P(WeatherPresets_DUSKYELLOW, 20, NO_TWEEN, NULL, NULL),
		P(WeatherPresets_EVENINGORANGE, 20, NO_TWEEN, NULL, MusicNight3),
		P(WeatherPresets_NIGHTGREEN, 60, 30, phaseNightTwo, NULL), /* GREEN */
		/* THREE (15-18) */
		P(WeatherPresets_DAWNGREY, 20, NO_TWEEN, daybreak, NULL),
		P(WeatherPresets_DAYBLEAK, 50, NO_TWEEN, NULL, MusicDay2),
		P(WeatherPresets_DUSKWARM, 20, NO_TWEEN, NULL, NULL),
		P(WeatherPresets_EVENINGBLACK, 20, NO_TWEEN, NULL, MusicNight4),
		P(WeatherPresets_NIGHTDARK, 60, 30, phaseNightThree, NULL),
		/* FOUR (19-22) */
		P(WeatherPresets_DAWNBLEAK, 20, NO_TWEEN, daybreak, NULL),
		P(WeatherPresets_DAYSOFT, 40, NO_TWEEN, NULL, NULL),
		P(WeatherPresets_EVENINGMONOTONE, 30, NO_TWEEN, NULL, MusicNight5),
		P(WeatherPresets_NIGHTSUPERDARK, 65, 30, phaseNightFour, NULL),
		/* FIVE (23-26) */
		P(WeatherPresets_DAWNLIGHTPINK, 20, NO_TWEEN, daybreak, MusicDay3),
		P(WeatherPresets_DAYBLEAK, 55, NO_TWEEN, NULL, NULL),
		P(WeatherPresets_EVENINGFOGGY, 40, NO_TWEEN, NULL, MusicNight4),
		P(WeatherPresets_NIGHTFOGGY, 60, 30, phaseNightFive, NULL),
		/* SIX (27-30) */
		P(WeatherPresets_DAWNBLEAK, 25, NO_TWEEN, daybreak, MusicDay4),
		P(WeatherPresets_DAYMONOCHROME, 60, NO_TWEEN, NULL, NULL),
		P(WeatherPresets_DUSKPINK, 15, NO_TWEEN, NULL, NULL),
		P(WeatherPresets_NIGHTCLEAR, 70, 30, phaseNightSix, MusicNight4),
		/* SEVEN (31-34) */
		P(WeatherPresets_DAWNCLEARORANGE, 20, NO_TWEEN, daybreak, NULL),
		P(WeatherPresets_DAYCLEARCOLD, 40, NO_TWEEN, NULL, MusicDay3),
		P(WeatherPresets_DUSKCLEAR, 20, NO_TWEEN, NULL, NULL),
		P(WeatherPresets_NIGHTSHINE, 70, 30, phaseNightSeven, MusicNight3),
		/* EIGHT (35-38) */
		P(WeatherPresets_DAWNREDMOON, 40, NO_TWEEN, daybreak, MusicDay5),
		P(WeatherPresets_DAYORANGESKY, 40, NO_TWEEN, NULL, NULL),
		P(WeatherPresets_DUSKFOGGY, 20, NO_TWEEN, NULL, NULL),
		/* BIG WAVE */
		P(WeatherPresets_NIGHTPURPLE, 80, 30, phaseNightEight, MusicNight4),
		/* NINE (39-42) */
		P(WeatherPresets_DAWNBRIGHT, 20, NO_TWEEN, daybreak, NULL),
		P(WeatherPresets_DAYPASTEL, 75, NO_TWEEN, NULL, MusicDay2),
		P(WeatherPresets_DUSKTAN, 20, NO_TWEEN, NULL, MusicNight4),
		/* SINGLE TROLL, MASSIVE HEALTH */
		P(WeatherPresets_NIGHTREDMOON, 60, 30, phaseNightNine, NULL),
		/* TEN (43) */
		P(WeatherPresets_DAWNBROWN, 20, NO_TWEEN, daybreak, NULL),
		P(WeatherPresets_DAYDUSTY, 40, NO_TWEEN, NULL, NULL),
		P(WeatherPresets_DUSKRED, 20, NO_TWEEN, NULL, MusicNight3),
		/* EVERYTHING, YOU DIE HERE. */
		P(WeatherPresets_NIGHTLONG, 60, 30, phaseNightTen, NULL),
		P(WeatherPresets_DAWNEARLY, 15, NO_TWEEN, trollRetreat, NULL),
	};
	memcpy(self->PHASES, phases, sizeof phases);
	self->PHASES_length = sizeof phases / sizeof phases[0];
	const Phase cycle[] = {
		P(WeatherPresets_DAWNREDMOON, 20, NO_TWEEN, daybreak, NULL),
		P(WeatherPresets_DAYPASTEL, 40, NO_TWEEN, NULL, NULL),
		P(WeatherPresets_DUSKTAN, 20, NO_TWEEN, NULL, NULL),
		P(WeatherPresets_NIGHTLONG, 30, NO_TWEEN, NULL, MusicNight5),
		P(NULL, 55, NO_TWEEN, phaseNightCycle, NULL),
	};
	memcpy(self->PHASES_CYCLE, cycle, sizeof cycle);

	SUPER(FlxState_ctor((FlxState *)self));
	self->restoreProgress = progress ? strdup(progress) : NULL;
	return self;
}

/*=== INITIALIZATION ==*/
void PlayState_create(PlayState *self)
{
	FlxG.camera->bgColor = 0xFFafb4c2;
	FlxG.camera->bounds = FlxRect_alloc(0, 0, PlayState_GAME_WIDTH, 196);
	FlxG.worldBounds.width = PlayState_GAME_WIDTH;
	FlxG.worldBounds.height = 300;
	PlayState_buildLevel(self, &Level_fields);
	Weather_tweenTo(self->weather, WeatherPresets_FOGGY, 0);

	if (PlayState_CHEATS) {
		FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->minimap = Minimap_new(0, FlxG.height - 1, (int32_t)FlxG.width, 1)));
		Minimap_add(self->minimap, (FlxBasic *)self->trolls, 0xFF87B587);
		Minimap_add(self->minimap, (FlxBasic *)self->trollsNoCollide, 0xFF0000FF);
		Minimap_add(self->minimap, (FlxBasic *)self->player, 0xff765DB3);
		Minimap_add(self->minimap, (FlxBasic *)self->beggars, 0xFF7D6841);
		Minimap_add(self->minimap, (FlxBasic *)self->characters, 0xFFA281F8);
		Minimap_add(self->minimap, (FlxBasic *)self->walls, 0xFF969696);
	}

	PlayState_showCoins(self);

	/* Load up environment sounds */
	self->cicada = FlxG_play(CicadaSound, 0.0, true, true);
	self->owls = FlxG_play(OwlsSound, 0.0, true, true);
	self->birds = FlxG_play(BirdsSound, 0.0, true, true);

	/* Camera */
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->cameraTarget = CameraTarget_new()));
	CameraTarget_set_target(self->cameraTarget, self->player);
	self->cameraTarget->offset.y = -4;
	CameraTarget_snap(self->cameraTarget);
	FlxCamera_follow(FlxG.camera, (FlxObject *)self->cameraTarget, FlxCamera_STYLE_LOCKON);

	FlxG_watch(self, "timeToNextPhase", NULL);
	FlxG_watch(self->weather, "timeOfDay", NULL);
	FlxG_watch(self->weather, "progress", NULL);
	FlxG_watch(self->weather, "ambient", NULL);
	FlxG_watch(self->weather, "ambientAmount", NULL);
	FlxG_watch(self, "phase", NULL);
	/* if (WEATHERCONTROLS) { ... } */
}

void PlayState_progressAll(PlayState *self)
{
	self->reachedVillage = true;
	self->recruitedCitizen = true;
	self->boughtItem = true;
	self->buyBowAdvice = true;
	self->buyScytheAdvice = true;
	self->expandedKingdomAdvice = true;
}

void PlayState_buildLevel(PlayState *self, const OelLevel *levelXML)
{
	/* Load XML */
	const OelLevel *oel = levelXML;
	/* Variables */
	const char *backdropFarGraphic = PlayState_imageByName(oel->backdropFarImg);
	const char *backdropCloseGraphic = PlayState_imageByName(oel->backdropCloseImg);
	int32_t waterHeight = oel->waterHeight;
	self->darkness = FlxSprite_makeGraphic(FlxSprite_new(0, 0, NULL), FlxG.width, FlxG.height, 0x88000000, false, NULL);

	/* Basic setup */
	self->weather = Weather_new();
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->sky = (FlxSprite *)Sky_new(self->weather)));
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->sunmoon = SunMoon_new(self->weather)));
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->backdropFar = FlxBackdrop_new(backdropFarGraphic, 0.15, 0.2, 0xFF717565)));
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->backdropClose = FlxBackdrop_new(backdropCloseGraphic, 0.3, 0.2, 0xFF555849)));
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->backdrop = FlxGroup_new(0)));
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->haze = Haze_new(0, 0, self->weather)));
	/* Movables */
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->archers = FlxGroup_new(10)));
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->objects = FlxGroup_new(0)));
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->shops = FlxGroup_new(0)));
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->bunnies = FlxGroup_new(0)));
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->beggars = FlxGroup_new(0)));
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->player = (FlxSprite *)Player_new(100, 68)));
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->characters = FlxGroup_new(0)));

	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->trolls = FlxGroup_new(0)));
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->trollsNoCollide = FlxGroup_new(0)));
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->walls = FlxGroup_new(0)));
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->coins = FlxGroup_new(100)));
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->gibs = FlxGroup_new(200)));
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->indicators = FlxGroup_new(0)));

	/* Level */
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->level = FlxGroup_new(0)));
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->floor = FlxTilemap_new()));
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->farmlands = FlxGroup_new(0)));
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->props = FlxGroup_new(0)));
	/* Effects */
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->lights = FlxGroup_new(0)));
	self->darkness->scrollFactor.x = self->darkness->scrollFactor.y = 0;
	self->darkness->blend = "multiply";
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)self->darkness);

	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->text = FlxText_new(10, 138, FlxG.width, "TEXT", true)));
	FlxText_setFormat(self->text, "04b03", 8, 0xFFFFFFFF, "left", 0xCC333333);
	self->text->visible = false;
	self->text->scrollFactor.x = 0;
	FlxSprite_set_alpha((FlxSprite *)self->text, 1.0);

	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->centerText = FlxText_new(0, FlxG.height / 2.0 - 32, FlxG.width, "TEXT", true)));

	FlxText_setFormat(self->centerText, "04b03", 32, 0xFFFFFFFF, "center", 0xAA333333);
	self->centerText->visible = false;
	self->centerText->scrollFactor.x = 0;
	FlxSprite_set_alpha((FlxSprite *)self->centerText, 1.0);

	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->water = Water_new(-4, waterHeight, (int32_t)FlxG.width + 8, 44, self->lights, self->weather, 0xFF686C53, 0.3)));
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->arrows = FlxGroup_new(64)));
	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->fx = FlxGroup_new(0)));

	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->sack = Coinsack_new(270, 2)));

	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->fog = Fog_new(self->weather)));

	FlxGroup_add((FlxGroup *)self, (FlxBasic *)(self->noise = FlxSprite_new(0, 0, NULL)));
	self->noise->scrollFactor.x = self->noise->scrollFactor.y = 0;
	FlxSprite_makeGraphic(self->noise, FlxG.width, FlxG.height, 0xFFFF00FF, false, NULL);
	BitmapData_noise(FlxSprite_get_pixels(self->noise), 0, 0, 255, 7, true);
	FlxSprite_set_alpha(self->noise, 0.015);

	/* Add backdrop objects */
	int32_t i;
	if (OelLevel_section(oel, "backdrop") != NULL) {
		PlayState_buildObjects(self, OelLevel_section(oel, "backdrop"), NULL, self->backdrop);
		for (i = 0; i < self->backdrop->length; i++) {
			((FlxObject *)As3Array_get(&self->backdrop->members, (uint32_t)i))->scrollFactor.x = 0.5;
		}
	}
	/* Add Ground Tiles */
	if (oel->ground != NULL) {
		uint32_t tileWidth = (uint32_t)oel->groundTileWidth;
		uint32_t tileHeight = (uint32_t)oel->groundTileHeight;
		const char *mapData = oel->ground;
		FlxTilemap_loadMap(self->floor, mapData, TilesImg, tileWidth, tileHeight, FlxTilemap_OFF, 0, 1, 1);
	}
	self->grassTiles = as3_alloc(sizeof(int32_t) * (self->floor->widthInTiles + 1));
	self->grassTiles_length = 0;
	for (i = 0; (uint32_t)i < self->floor->widthInTiles; i++) {
		int32_t t = (int32_t)FlxTilemap_getTile(self->floor, (uint32_t)i, 4);
		if ((t >= 7 && t <= 11) || (t >= 17 && t <= 18)) {
			self->grassTiles[self->grassTiles_length++] = i;
		}
	}
	/* Add ground collision proxy because this is a flat level. */
	FlxSprite *collider = FlxSprite_makeGraphic(FlxSprite_new(0, 132.2, NULL), AS3_UINT(FlxG.worldBounds.width), 32, 0x00FF00FF, false, NULL);
	collider->immovable = true;
	FlxGroup_add(self->level, (FlxBasic *)collider);
	collider = FlxSprite_makeGraphic(FlxSprite_new(0, 0, NULL), 8, 200, 0x00FF00FF, false, NULL);
	collider->immovable = true;
	FlxGroup_add(self->level, (FlxBasic *)collider);
	collider = FlxSprite_makeGraphic(FlxSprite_new(FlxG.worldBounds.width - 8, 0, NULL), 8, 200, 0x00FF00FF, false, NULL);
	collider->immovable = true;
	FlxGroup_add(self->level, (FlxBasic *)collider);

	/* Add Walls */
	if (OelLevel_section(oel, "walls") != NULL) {
		PlayState_buildObjects(self, OelLevel_section(oel, "walls"), NULL, self->walls);
	}
	/* Set the closest walls to a first build stage */
	for (i = 0; i < self->walls->length; i++) {
		Wall *w = As3Array_get(&self->walls->members, (uint32_t)i);
		if ((w->x + w->width) > self->kingdomLeft && w->x < self->kingdomRight) {
			Wall_build(w);
		}
	}
	/* Add level objects */
	if (OelLevel_section(oel, "objects") != NULL) {
		PlayState_buildObjects(self, OelLevel_section(oel, "objects"), "Shop", self->shops);
		PlayState_buildObjects(self, OelLevel_section(oel, "objects"), "Castle", self->objects);
	}
	/* Add level objects */
	if (OelLevel_section(oel, "farmlands") != NULL) {
		PlayState_buildObjects(self, OelLevel_section(oel, "farmlands"), NULL, self->farmlands);
	}
	/* Add props */
	if (OelLevel_section(oel, "props") != NULL) {
		PlayState_buildObjects(self, OelLevel_section(oel, "props"), NULL, self->props);
	}
	/* Add lights */
	if (OelLevel_section(oel, "lights") != NULL) {
		PlayState_buildObjects(self, OelLevel_section(oel, "lights"), NULL, self->lights);
	}
}

/* buildObjects(nodes, group): `onlyType` selects oel.objects[0].Shop etc. */
void PlayState_buildObjects(PlayState *self, const OelSection *nodes, const char *onlyType, FlxGroup *group)
{
	(void)self;
	for (int n = 0; n < nodes->count; n++) {
		const OelNode *node = &nodes->nodes[n];
		if (onlyType != NULL && !as3_streq(node->name, onlyType))
			continue;
		const char *objType = node->name;
		FlxSprite *obj;
		/* getDefinitionByName(objType) */
		if (as3_streq(objType, "Reed")) obj = (FlxSprite *)Reed_new(node->x, node->y);
		else if (as3_streq(objType, "Castle")) obj = (FlxSprite *)Castle_new(node->x, node->y);
		else if (as3_streq(objType, "Treeline")) obj = (FlxSprite *)Treeline_new(node->x, node->y);
		else if (as3_streq(objType, "Farmland")) obj = (FlxSprite *)Farmland_new(node->x, node->y);
		else if (as3_streq(objType, "Wall")) obj = (FlxSprite *)Wall_new(node->x, node->y);
		else if (as3_streq(objType, "Torch")) obj = (FlxSprite *)Torch_new(node->x, node->y);
		else if (as3_streq(objType, "Shop")) obj = (FlxSprite *)Shop_new(node->x, node->y);
		else if (as3_streq(objType, "Firefly")) obj = (FlxSprite *)Firefly_new(node->x, node->y);
		else {
			/* catch(ReferenceError): this[objType+"Img"] */
			char key[64];
			snprintf(key, sizeof key, "%sImg", objType);
			const char *simpleGraphic = PlayState_imageByName(key);
			obj = FlxSprite_new(node->x, node->y, simpleGraphic);
		}
		FlxGroup_add(group, (FlxBasic *)obj);
	}
}

/* callAll("retreat") / callAll("kill") */
static void call_retreat(FlxBasic *b) { Troll_retreat((Troll *)b); }
static void call_kill(FlxBasic *b) { CALL0(b, kill); }

/*=== GAME LOGIC ===*/
void PlayState_update(PlayState *self)
{
	/* Collisions */
	if (self->restoreProgress) {
		PlayState_setProgress(self, self->restoreProgress);
		self->restoreProgress = NULL;
	}
	FlxG_collide((FlxBasic *)self->level, (FlxBasic *)self->coins, (As3Callback2)AS3_NULL_FN);
	FlxG_collide((FlxBasic *)self->level, (FlxBasic *)self->trolls, (As3Callback2)AS3_NULL_FN);
	FlxG_collide((FlxBasic *)self->level, (FlxBasic *)self->trollsNoCollide, (As3Callback2)AS3_NULL_FN);
	FlxG_collide((FlxBasic *)self->level, (FlxBasic *)self->gibs, (As3Callback2)AS3_NULL_FN);
	FlxG_collide((FlxBasic *)self->trolls, (FlxBasic *)self->trolls, (As3Callback2)AS3_NULL_FN);
	FlxG_overlap((FlxBasic *)self->trolls, (FlxBasic *)self->walls, (As3Callback2)AS3_FN(self, trollWall), (As3Callback2)AS3_NULL_FN);
	FlxG_overlap((FlxBasic *)self->trollsNoCollide, (FlxBasic *)self->walls, (As3Callback2)AS3_FN(self, trollWall), (As3Callback2)AS3_NULL_FN);
	FlxG_overlap((FlxBasic *)self->arrows, (FlxBasic *)self->trolls, (As3Callback2)AS3_FN(self, trollShot), (As3Callback2)AS3_NULL_FN);
	FlxG_overlap((FlxBasic *)self->arrows, (FlxBasic *)self->trollsNoCollide, (As3Callback2)AS3_FN(self, trollShot), (As3Callback2)AS3_NULL_FN);
	FlxG_overlap((FlxBasic *)self->arrows, (FlxBasic *)self->bunnies, (As3Callback2)AS3_FN(self, bunnyShot), (As3Callback2)AS3_NULL_FN);
	FlxG_overlap((FlxBasic *)self->coins, (FlxBasic *)self->characters, (As3Callback2)AS3_FN(self, pickUpCoin), (As3Callback2)AS3_NULL_FN);
	FlxG_overlap((FlxBasic *)self->coins, (FlxBasic *)self->player, (As3Callback2)AS3_FN(self, pickUpCoin), (As3Callback2)AS3_NULL_FN);
	FlxG_overlap((FlxBasic *)self->coins, (FlxBasic *)self->beggars, (As3Callback2)AS3_FN(self, pickUpCoin), (As3Callback2)AS3_NULL_FN);
	FlxG_overlap((FlxBasic *)self->coins, (FlxBasic *)self->trolls, (As3Callback2)AS3_FN(self, pickUpCoin), (As3Callback2)AS3_NULL_FN);
	FlxG_overlap((FlxBasic *)self->coins, (FlxBasic *)self->trollsNoCollide, (As3Callback2)AS3_FN(self, pickUpCoin), (As3Callback2)AS3_NULL_FN);
	FlxG_overlap((FlxBasic *)self->trolls, (FlxBasic *)self->characters, (As3Callback2)AS3_FN(self, trollHit), (As3Callback2)AS3_NULL_FN);
	FlxG_overlap((FlxBasic *)self->trollsNoCollide, (FlxBasic *)self->characters, (As3Callback2)AS3_FN(self, trollHit), (As3Callback2)AS3_NULL_FN);
	FlxG_overlap((FlxBasic *)self->trolls, (FlxBasic *)self->beggars, (As3Callback2)AS3_FN(self, trollHit), (As3Callback2)AS3_NULL_FN);
	FlxG_overlap((FlxBasic *)self->trollsNoCollide, (FlxBasic *)self->beggars, (As3Callback2)AS3_FN(self, trollHit), (As3Callback2)AS3_NULL_FN);
	if (!(PlayState_CHEATS && self->untouchable)) {
		FlxG_overlap((FlxBasic *)self->trolls, (FlxBasic *)self->player, (As3Callback2)AS3_FN(self, trollHit), (As3Callback2)AS3_NULL_FN);
		FlxG_overlap((FlxBasic *)self->trollsNoCollide, (FlxBasic *)self->player, (As3Callback2)AS3_FN(self, trollHit), (As3Callback2)AS3_NULL_FN);
	}
	FlxG_overlap((FlxBasic *)self->characters, (FlxBasic *)self->player, (As3Callback2)AS3_FN(self, giveTaxes), (As3Callback2)AS3_NULL_FN);

	/* Update weather */
	Weather_update(self->weather);

	/* Gamestate */
	if (self->timeToNextPhase <= 0) {
		PlayState_nextPhase(self);
	} else if (!self->phasesPaused) {
		self->timeToNextPhase -= FlxG.elapsed;
	}

	self->kingdomRight = Math_max(PlayState_GAME_WIDTH / 2.0 + PlayState_MIN_KINGDOM_WIDTH / 2.0, self->kingdomRight - FlxG.elapsed * 4);
	self->kingdomLeft = Math_min(PlayState_GAME_WIDTH / 2.0 - PlayState_MIN_KINGDOM_WIDTH / 2.0, self->kingdomLeft + FlxG.elapsed * 4);

	/* Spawn bunnies using logistic growth */
	Number p = (FlxGroup_countLiving(self->bunnies) + 2) / (Number)(PlayState_MAX_BUNNIES + 2);
	if (self->bunnySpawnTimer <= 0) {
		self->bunnySpawnTimer = PlayState_MIN_BUNNY_SPAWNTIME;
		Number probAdd = 0.5 + 2 * p * (1 - p);
		if (FlxG_random() < probAdd) {
			int32_t rx = AS3_INT(FlxG_random() * self->grassTiles_length);
			FlxGroup_add(self->bunnies, (FlxBasic *)Bunny_new(self->grassTiles[rx] * 32, 0));
		}
	} else {
		self->bunnySpawnTimer -= FlxG.elapsed;
	}
	/* Spawn beggars */
	if (FlxGroup_countLiving(self->beggars) < self->minBeggars) {
		FlxGroup_add(self->beggars, (FlxBasic *)Citizen_new((FlxG_random() < 0.5) ? 16 : PlayState_GAME_WIDTH - 16, 0));
	}

	/* Spawn trolls */
	PlayState_updateTrollSpawn(self);
	self->trollSpawnTimer -= FlxG.elapsed;
	if (self->retreatDelay > 0) {
		self->retreatDelay -= FlxG.elapsed;
		if (self->retreatDelay <= 0) {
			FlxGroup_callAll(self->trolls, call_retreat, true);
			FlxGroup_callAll(self->trollsNoCollide, call_retreat, true);
		}
	}

	/* Text update */
	if (self->textTimeout <= 0) {
		PlayState_showText(self, NULL);
	} else {
		FlxSprite_set_alpha((FlxSprite *)self->text, Math_min(PlayState_TEXT_MAX_ALPHA, self->textTimeout));
		self->textTimeout -= FlxG.elapsed;
	}

	if (self->centerText->visible && FlxSprite_get_alpha((FlxSprite *)self->centerText) < 0.001) {
		self->centerText->visible = false;
	} else {
		FlxSprite_set_alpha((FlxSprite *)self->centerText, FlxSprite_get_alpha((FlxSprite *)self->centerText) - 0.05 * FlxG.elapsed);
	}

	/* Camera follow timeout */
	if (CameraTarget_get_target(self->cameraTarget) != self->player) {
		if (self->cameraTimeout <= 0) {
			/* Reset the cameratarget. */
			CameraTarget_set_target(self->cameraTarget, self->player);
			self->cameraTarget->lead = 48;
		} else {
			self->cameraTimeout -= FlxG.elapsed;
		}
	}

	/* Progress update */
	if (self->player->x > PlayState_GAME_WIDTH / 2 && !self->reachedVillage) {
		self->reachedVillage = true;
		if (self->beggars->length > 0) {
			PlayState_panTo(self, As3Array_get(&self->beggars->members, 0), 5.0, 0);
			PlayState_showText(self, "Throw some coins [DOWN] near them.");
		}
	}
	if (self->recruitedCitizen && !self->boughtItem && !self->buyBowAdvice) {
		self->buyBowAdvice = true;
		PlayState_showText(self, "Buy them bows to defend and hunt for you.");
		PlayState_panTo(self, As3Array_get(&self->shops->members, 1), 7.5, 0);
	}
	if (self->buyBowAdvice && !self->buyScytheAdvice && CameraTarget_get_target(self->cameraTarget) == self->player) {
		self->buyScytheAdvice = true;
		PlayState_showText(self, "Buy them scythes to build and farm for you.");
		PlayState_panTo(self, As3Array_get(&self->shops->members, 0), 7.5, 0);
	}
	if (self->boughtItem && !self->expandedKingdomAdvice && self->characters->length >= 4 && self->weather->timeOfDay > 0.3 && self->weather->timeOfDay < 0.6) {
		self->expandedKingdomAdvice = true;
		PlayState_showText(self, "Expand your kingdom by building a wall here.");
		PlayState_panTo(self, As3Array_get(&self->walls->members, 1), 5.0, -12);
	}

	PlayState_updateEnvironmentSounds(self);

	if (self->gameover && FlxMouse_justPressed(FlxG.mouse)) {
		FlxMouse_hide(FlxG.mouse);
		FlxG_switchState((FlxState *)PlayState_new(self->savedProgress));
	}

	SUPER(FlxGroup_update((FlxGroup *)self));

	/* if (FlxG.keys.justPressed("S")): desktop fullscreen toggle */

	if (PlayState_CHEATS) {
		if (FlxKeyboard_justPressed(FlxG.keys, "F")) {
			Citizen *c = Citizen_new(AS3_INT((self->kingdomRight + self->kingdomLeft) / 2), 0);
			FlxGroup_add(self->characters, (FlxBasic *)c);
			Citizen_morph(c, Citizen_FARMER);
			PlayState_showText(self, "Spawned farmer.");
		}
		if (FlxKeyboard_justPressed(FlxG.keys, "H")) {
			Citizen *h = Citizen_new(AS3_INT((self->kingdomRight + self->kingdomLeft) / 2), 0);
			FlxGroup_add(self->characters, (FlxBasic *)h);
			Citizen_morph(h, Citizen_HUNTER);
			PlayState_showText(self, "Spawned farmer.");
		}
		if (FlxKeyboard_justPressed(FlxG.keys, "T")) {
			self->cheatNoTrolls = !self->cheatNoTrolls;
			PlayState_showText(self, self->cheatNoTrolls ? "Trolls disabled" : "Trolls enabled");
		}
		if (FlxKeyboard_justPressed(FlxG.keys, "U")) {
			self->untouchable = !self->untouchable;
			PlayState_showText(self, self->untouchable ? "Untouchable enabled" : "Untouchable disabled");
		}
		if (FlxKeyboard_justPressed(FlxG.keys, "N")) {
			self->timeToNextPhase = 1.0;
			PlayState_showText(self, "Skip phase.");
		}
		if (FlxKeyboard_justPressed(FlxG.keys, "B")) {
			FlxGroup_add(self->beggars, (FlxBasic *)Citizen_new(AS3_INT((self->kingdomRight + self->kingdomLeft) / 2), 0));
			PlayState_showText(self, "Spawned beggar.");
		}
		if (FlxKeyboard_justPressed(FlxG.keys, "I")) {
			self->trollBig = !self->trollBig;
			PlayState_showText(self, self->trollBig ? "Trolls big." : "Trolls normal.");
		}
		if (FlxKeyboard_justPressed(FlxG.keys, "A")) {
			PlayState_progressAll(self);
			PlayState_showText(self, "Full progress");
		}
		if (FlxKeyboard_justPressed(FlxG.keys, "R")) {
			PlayState_spawnTrolls(self, 2);
			PlayState_showText(self, "Spawned 2 trolls");
		}
		if (FlxKeyboard_justPressed(FlxG.keys, "P")) {
			self->phasesPaused = !self->phasesPaused;
			PlayState_showText(self, self->phasesPaused ? "Phases paused" : "Phases resumed");
		}
		/* "ENTER": setWeatherFromInput (WEATHERCONTROLS only) */
		if (FlxKeyboard_justPressed(FlxG.keys, "C")) {
			char s[32];
			((Player *)self->player)->coins += 1;
			snprintf(s, sizeof s, "%d coins.", ((Player *)self->player)->coins);
			PlayState_showText(self, s);
		}
		if (FlxKeyboard_justPressed(FlxG.keys, "THREE")) PlayState_setProgress(self, "D3 A12 X1713 B2 P0 F2 H4 W010011 C1 S01 G3");
		if (FlxKeyboard_justPressed(FlxG.keys, "FOUR")) PlayState_setProgress(self, "D4 A17 X1932 B2 P1 F3 H6 W220021 C1 S02 G7");
		if (FlxKeyboard_justPressed(FlxG.keys, "FIVE")) PlayState_setProgress(self, "D5 A21 X1899 B4 P1 F3 H6 W010031 C2 S00 G0");
		if (FlxKeyboard_justPressed(FlxG.keys, "SIX")) PlayState_setProgress(self, "D6 A25 X2235 B2 P2 F1 H10 W010031 C2 S11 G0");
		if (FlxKeyboard_justPressed(FlxG.keys, "SEVEN")) PlayState_setProgress(self, "D7 A29 X2146 B2 P5 F2 H8 W030031 C2 S00 G0");
		if (FlxKeyboard_justPressed(FlxG.keys, "EIGHT")) PlayState_setProgress(self, "D8 A33 X2318 B3 P1 F6 H9 W040011 C2 S01 G2");
		if (FlxKeyboard_justPressed(FlxG.keys, "NINE")) PlayState_setProgress(self, "D9 A37 X1467 B2 P1 F6 H7 W140041 C2 S02 G0");
	}
}

static void phaseFirst(PlayState *self)
{
	FlxGroup_add(self->beggars, (FlxBasic *)Citizen_new(AS3_INT(self->kingdomRight + 580), 0));
	FlxGroup_add(self->beggars, (FlxBasic *)Citizen_new(AS3_INT(self->kingdomRight + 600), 0));
	self->minBeggars = 2;
}

static void phaseBeforeNightOne(PlayState *self)
{
	PlayState_showText(self, "Night comes, be careful.");
}

static void phaseNightOne(PlayState *self)
{
	PlayState_trollStats(self, 24, 1, 20, 999999, false, 16.0); /* Nojump */
	PlayState_spawnTrolls(self, 2);
	if (self->player->x < PlayState_GAME_WIDTH / 2) {
		PlayState_panTo(self, As3Array_get(&self->trolls->members, 0), 8.0, 0);
	} else {
		PlayState_panTo(self, As3Array_get(&self->trolls->members, 1), 8.0, 0);
	}
	PlayState_showText(self, "They will noodle your stuff away.");
}

/* These trolls still won't scale your lowest walls */
static void phaseNightTwo(PlayState *self)
{
	PlayState_trollStats(self, 26, 1, 20, 2, false, 12.0); /* Jump0 */
	PlayState_spawnTrolls(self, 12);
}

/* These WILL scale the lowest walls */
static void phaseNightThree(PlayState *self)
{
	PlayState_trollStats(self, 26, 1, 30, 2, false, 12.0); /* Grunts */
	PlayState_spawnTrolls(self, 20);
}

/* The trolls are a little tougher now. */
static void phaseNightFour(PlayState *self)
{
	PlayState_trollStats(self, 26, 3, 30, 2, false, 12.0);
	PlayState_spawnTrolls(self, 24);
}

/* They are faster but more chaotic, they might break your walls,
   which will kill you in the next wave. */
static void phaseNightFive(PlayState *self)
{
	PlayState_trollStats(self, 35, 2, 38, 2, false, 4.0); /* Chaotic */
	PlayState_spawnTrolls(self, 36);
}

/* These trolls will scale the stone walls */
static void phaseNightSix(PlayState *self)
{
	PlayState_trollStats(self, 30, 3, 45, 2, false, 10.0);
	PlayState_spawnTrolls(self, 8);
}

/* Boss wave trolls */
static void phaseNightSeven(PlayState *self)
{
	PlayState_trollStats(self, 20, 30, 10, 999999, true, 16.0);
	PlayState_spawnTrolls(self, 2);
}

/* Since the boss probably broke your walls these trolls jump very high,
   there is no disadvantage to not having walls. You will need them back in
   the next wave though. */
static void phaseNightEight(PlayState *self)
{
	PlayState_trollStats(self, 40, 4, 50, 3, false, 12.0);
	PlayState_spawnTrolls(self, 16);
}

/* You need the highest walls here */
static void phaseNightNine(PlayState *self)
{
	PlayState_trollStats(self, 30, 4, 45, 4, false, 8.0);
	PlayState_spawnTrolls(self, 24);
}

/* Kill the player off */
static void phaseNightTen(PlayState *self)
{
	PlayState_trollStats(self, 20, 30, 10, 999999, true, 16.0); /* Boss */
	PlayState_spawnTrolls(self, 4);
	PlayState_trollStats(self, 30, 4, 45, 4, false, 8.0); /* Strong */
	PlayState_spawnTrolls(self, 20);
	PlayState_trollStats(self, 40, 2, 50, 3, false, 12.0); /* Jumper */
	PlayState_spawnTrolls(self, 10);
	PlayState_trollStats(self, 26, 1, 30, 2, false, 12.0); /* Grunts */
	PlayState_spawnTrolls(self, 40);
}

static void phaseNightCycle(PlayState *self)
{
	Number difficulty = self->day.value - 10;
	PlayState_trollStats(self, 30, 3 + 2 * difficulty, 45, 4, false, 8.0); /* Strong */
	PlayState_spawnTrolls(self, AS3_INT(10 + difficulty));
	if (self->day.value % 2 == 0) {
		PlayState_trollStats(self, 20, 30, 10, 999999, true, 16.0); /* Boss */
		PlayState_spawnTrolls(self, AS3_INT(2 * difficulty));
	}
}

void PlayState_nextPhase(PlayState *self)
{
	if (self->phasesPaused) {
		return;
	}
	const Phase *currentPhase;
	if ((uint32_t)self->phase < self->PHASES_length) {
		currentPhase = &self->PHASES[self->phase];
	} else {
		int32_t p = (self->phase - (int32_t)self->PHASES_length) % 5;
		currentPhase = &self->PHASES_CYCLE[p];
	}
	Number weatherTweenTime;
	self->timeToNextPhase = currentPhase->duration;
	/* Transform weather */
	if (!currentPhase->hasTweenTime) {
		weatherTweenTime = self->timeToNextPhase * 0.7;
	} else {
		weatherTweenTime = currentPhase->tweenTime;
	}
	if (currentPhase->weather != NULL) {
		Weather_tweenTo(self->weather, currentPhase->weather, weatherTweenTime);
	}
	self->phase += 1;
	/* Call the function to do custom actions if there is one */
	if (currentPhase->callback != NULL) {
		currentPhase->callback(self);
	}
	/* Play music */
	if (currentPhase->music != NULL) {
		if (self->music != NULL) {
			FlxSound_stop(self->music);
		}
		self->music = FlxG_play(currentPhase->music, 1.0, false, true);
		FlxG_log(currentPhase->music);
	}
}

void PlayState_updateEnvironmentSounds(PlayState *self)
{
	Number v;
	v = 1 - pow(Math_abs(self->weather->timeOfDay - 0.7) / 0.1, 2);
	FlxSound_set_volume(self->cicada, v);
	v = 1 - pow(Math_min(self->weather->timeOfDay, Math_abs(self->weather->timeOfDay - 1.0)) / 0.2, 2);
	FlxSound_set_volume(self->owls, v);
	v = 1 - pow(Math_abs(self->weather->timeOfDay - 0.4) / 0.25, 2);
	FlxSound_set_volume(self->birds, v);
}

void PlayState_trollStats(PlayState *self, Number speed, Number health, Number jumpheight, Number jumpiness, bool big, Number confusion)
{
	self->trollMaxSpeed = speed;
	self->trollHealth = health;
	self->trollJumpHeight = jumpheight;
	self->trollJumpiness = jumpiness;
	self->trollBig = big;
	self->trollConfusion = confusion;
}

void PlayState_spawnTrolls(PlayState *self, int32_t amount)
{
	if (self->cheatNoTrolls)
		return;
	/* SWF FIX: the shipped SWF loops `while (amount > 0)` (tools/abc_dump.py PlayState spawnTrolls);
	   the pinned snapshot has `while(amount)`, which never ends for an odd amount. Same for even ones. */
	while (amount > 0) {
		amount -= 2;
		Troll *troll = (Troll *)FlxGroup_recycle(self->trolls, &Troll_class);
		CALL(troll, reset, 64, self->groundHeight - 40);
		As3Array_set(&self->trollsToSpawn, self->trollsToSpawn.length, troll);
		troll = (Troll *)FlxGroup_recycle(self->trolls, &Troll_class);
		CALL(troll, reset, PlayState_GAME_WIDTH - 64, self->groundHeight - 40);
		As3Array_set(&self->trollsToSpawn, self->trollsToSpawn.length, troll);
		PlayState_updateTrollSpawn(self);
	}
}

/* Array.shift() */
static void *array_shift(As3Array *a)
{
	if (a->length == 0)
		return NULL;
	void *first = a->items[0];
	As3Array_splice1(a, 0);
	return first;
}

void PlayState_updateTrollSpawn(PlayState *self)
{
	if (self->trollsToSpawn.length > 0 && self->trollSpawnTimer <= 0) {
		Troll_go((Troll *)array_shift(&self->trollsToSpawn));
		Troll_go((Troll *)array_shift(&self->trollsToSpawn));
		self->trollSpawnTimer = PlayState_MIN_TROLL_SPAWNTIME;
	}
}

void PlayState_daybreak(PlayState *self)
{
	PlayState_trollRetreat(self, 10);
	Coin_drop((Coin *)FlxGroup_recycle(self->coins, &Coin_class), (FlxSprite *)self->castle, (FlxObject *)self->player, false);
	if (self->castle->stage >= 2) {
		Coin_drop((Coin *)FlxGroup_recycle(self->coins, &Coin_class), (FlxSprite *)self->castle, (FlxObject *)self->player, false);
	}
	MochiDigits_addValue(&self->day, 1);
	PlayState_showCenterText(self, Utils_toRoman(self->day.value));
	PlayState_saveProgress(self);
}

void PlayState_saveProgress(PlayState *self)
{
	int32_t numBeggars = FlxGroup_countLiving(self->beggars);
	int32_t numCitizens[] = AS3_LIST(0, 0, 0, 0);
	for (int32_t i = 0; i < self->characters->length; i++) {
		if (As3Array_get(&self->characters->members, (uint32_t)i) != NULL && (((FlxBasic *)As3Array_get(&self->characters->members, (uint32_t)i))->alive)) {
			numCitizens[((Citizen *)As3Array_get(&self->characters->members, (uint32_t)i))->occupation]++;
		}
	}
	numCitizens[Citizen_HUNTER] += AS3_INT(Math_max(0, FlxGroup_countLiving(self->archers)));
	As3Array wallStages = AS3_ARRAY();
	for (int32_t i = 0; i < self->walls->length; i++) {
		As3Array_push(&wallStages, AS3_BOX(((Wall *)As3Array_get(&self->walls->members, (uint32_t)i))->stage));
	}

	char *s = "";
	s = AS3_CAT(s, "D", AS3_STR(self->day.value), " ");
	s = AS3_CAT(s, "A", AS3_STR(self->phase), " ");
	s = AS3_CAT(s, "X", AS3_STR(AS3_INT(self->player->x)), " ");
	s = AS3_CAT(s, "B", AS3_STR(numBeggars), " ");
	s = AS3_CAT(s, "P", AS3_STR(numCitizens[Citizen_POOR]), " ");
	s = AS3_CAT(s, "F", AS3_STR(numCitizens[Citizen_FARMER]), " ");
	s = AS3_CAT(s, "H", AS3_STR(numCitizens[Citizen_HUNTER]), " ");
	s = AS3_CAT(s, "W", As3Array_join(&wallStages, ""), " ");
	s = AS3_CAT(s, "C", AS3_STR(self->castle->stage), " ");
	s = AS3_CAT(s, "S", AS3_STR(((Shop *)As3Array_get(&self->shops->members, 0))->supply), AS3_STR(((Shop *)As3Array_get(&self->shops->members, 1))->supply), " ");
	s = AS3_CAT(s, "G", AS3_STR(((Player *)self->player)->coins));
	FlxG_log(s);
	self->savedProgress = s;
}

void PlayState_setProgress(PlayState *self, const char *s)
{
	/* Parse the string 'N1 X1 B2 P0 F0 H0 W000011 C0 G7' */
	PlayState_progressAll(self);
	FlxG_flash(0xFFFFFFFF, 3, (As3Callback0)AS3_NULL_FN, false);
	FlxG_log(AS3_CAT("Skip to ", s));

	int32_t newDay = AS3_INT(parseInt(As3Array_get(As3String_match(s, AS3_REGEX("D(\\d+)")), 1)));
	int32_t ph = AS3_INT(parseInt(As3Array_get(As3String_match(s, AS3_REGEX("A(\\d+)")), 1)));
	int32_t playerX = AS3_INT(parseInt(As3Array_get(As3String_match(s, AS3_REGEX("X(\\d+)")), 1)));
	int32_t numBeggars = AS3_INT(parseInt(As3Array_get(As3String_match(s, AS3_REGEX("B(\\d+)")), 1)));
	int32_t numPoor = AS3_INT(parseInt(As3Array_get(As3String_match(s, AS3_REGEX("P(\\d+)")), 1)));
	int32_t numFarmers = AS3_INT(parseInt(As3Array_get(As3String_match(s, AS3_REGEX("F(\\d+)")), 1)));
	int32_t numHunters = AS3_INT(parseInt(As3Array_get(As3String_match(s, AS3_REGEX("H(\\d+)")), 1)));
	As3Array *wallStages = As3String_match(s, AS3_REGEX("W(\\d)(\\d)(\\d)(\\d)(\\d)(\\d)"));
	int32_t castleStage = AS3_INT(parseInt(As3Array_get(As3String_match(s, AS3_REGEX("C(\\d)")), 1)));
	As3Array *shopSupply = As3String_match(s, AS3_REGEX("S(\\d)(\\d)"));
	int32_t gold = AS3_INT(parseInt(As3Array_get(As3String_match(s, AS3_REGEX("G(\\d)")), 1)));

	while (FlxGroup_countLiving(self->beggars) < numBeggars) {
		FlxGroup_add(self->beggars, (FlxBasic *)Citizen_new(AS3_INT((self->kingdomRight + self->kingdomLeft) / 2), 0));
	}
	self->player->x = playerX;
	FlxGroup_callAll(self->characters, call_kill, true);
	FlxGroup_callAll(self->archers, call_kill, true);
	Citizen *c;
	while (numPoor) {
		c = Citizen_new(AS3_INT((self->kingdomRight + self->kingdomLeft) / 2), 0);
		Citizen_morph(c, Citizen_POOR);
		FlxGroup_add(self->characters, (FlxBasic *)c);
		numPoor--;
	}
	while (numFarmers) {
		c = Citizen_new(AS3_INT((self->kingdomRight + self->kingdomLeft) / 2), 0);
		Citizen_morph(c, Citizen_FARMER);
		FlxGroup_add(self->characters, (FlxBasic *)c);
		numFarmers--;
	}
	while (numHunters) {
		c = Citizen_new(AS3_INT((self->kingdomRight + self->kingdomLeft) / 2), 0);
		Citizen_morph(c, Citizen_HUNTER);
		FlxGroup_add(self->characters, (FlxBasic *)c);
		numHunters--;
	}
	for (int32_t i = 0; i < self->walls->length; i++) {
		Wall_buildTo(As3Array_get(&self->walls->members, (uint32_t)i), AS3_INT(parseInt(As3Array_get(wallStages, (uint32_t)(i + 1)))), true);
	}
	/* source quirk kept: shopSupply[0] is the whole "Sxy" match (parseInt: NaN -> 0) */
	Shop_setSupply(As3Array_get(&self->shops->members, 0), AS3_INT(parseInt(As3Array_get(shopSupply, 0))));
	Shop_setSupply(As3Array_get(&self->shops->members, 1), AS3_INT(parseInt(As3Array_get(shopSupply, 1))));
	Castle_morph(self->castle, castleStage);
	Player_changeCoins((Player *)self->player, gold - ((Player *)self->player)->coins);
	self->phase = ph - 1;
	MochiDigits_setValue(&self->day, newDay - 1);
	PlayState_nextPhase(self);
	FlxGroup_callAll(self->trolls, call_kill, true);
	FlxGroup_callAll(self->trollsNoCollide, call_kill, true);
	FlxGroup_callAll(self->gibs, call_kill, true);
}

void PlayState_trollRetreat(PlayState *self, Number delay)
{
	self->retreatDelay = delay;
	if (self->retreatDelay <= 0) {
		As3Array_set_length(&self->trollsToSpawn, 0); /* splice(0) */
		FlxGroup_callAll(self->trolls, call_retreat, true);
		FlxGroup_callAll(self->trollsNoCollide, call_retreat, true);
	}
}

static bool pickUpCoin(void *object, FlxObject *coin, FlxObject *chr)
{
	(void)object;
	if (as3_is(chr, &Player_class)) {
		Player_pickup((Player *)chr, coin);
	} else if (as3_is(chr, &Citizen_class)) {
		Citizen_pickup((Citizen *)chr, coin);
	} else if (as3_is(chr, &Troll_class)) {
		Troll_pickup((Troll *)chr, coin);
	}
	return false;
}

static bool giveTaxes(void *object, FlxObject *chr, FlxObject *player)
{
	(void)object;
	if (chr != player) {
		Citizen_giveTaxes((Citizen *)chr, (Player *)player);
	}
	return false;
}

static bool trollWall(void *object, FlxObject *troll, FlxObject *wall)
{
	(void)object;
	FlxObject_separate(troll, wall);
	CALL(wall, hurt, ((Troll *)troll)->big ? 2 * PlayState_TROLL_WALL_DAMAGE : PlayState_TROLL_WALL_DAMAGE);
	return false;
}

static bool trollShot(void *object, FlxObject *arrow, FlxObject *troll)
{
	PlayState *self = object;
	if (troll->alive && arrow->exists) {
		FlxSound_proximity(FlxG_play(HitbigSound, 1.0, false, true), arrow->x, arrow->y, (FlxObject *)self->player, FlxG.width, true);
		CALL0(arrow, kill);
		Troll_getShot((Troll *)troll);
	}
	return false;
}

static bool bunnyShot(void *object, FlxObject *arrow, FlxObject *bunny)
{
	PlayState *self = object;
	if (bunny->alive && arrow->exists) {
		FlxSound_proximity(FlxG_play(HitSound, 1.0, false, true), arrow->x, arrow->y, (FlxObject *)self->player, FlxG.width, true);
		CALL0(arrow, kill);
		Bunny_getShot((Bunny *)bunny, (Arrow *)arrow);
	}
	return false;
}

static bool trollHit(void *object, FlxObject *troll, FlxObject *chr)
{
	PlayState *self = object;
	if (as3_is(chr, &Citizen_class)) {
		Citizen_hitByTroll((Citizen *)chr, (Troll *)troll);
	}
	if (chr == (FlxObject *)self->player) {
		Player_hitByTroll((Player *)chr, (Troll *)troll);
	}
	return false;
}

void PlayState_crownStolen(PlayState *self)
{
	self->gameover = true;
	self->phasesPaused = true;
	PlayState_trollRetreat(self, 0);
	FlxMouse_show(FlxG.mouse);
	PlayState_showText(self, "No crown, no king. Game over.");
	PlayState_showText(self, "Click to continue or wait to enter highscore.");
	PlayState_showText(self, "Click to continue or wait to enter highscore.");
	FlxG_fade(0, 20, (As3Callback0)AS3_FN(self, PlayState_endGame), false);
}

void PlayState_endGame(void *object)
{
	PlayState *self = object;
	FlxG_switchState((FlxState *)GameOverState_new(self->day.value - 1));
}

/*=== RENDERING ==*/
void PlayState_draw(PlayState *self)
{
	self->darkness->dirty = true;
	FlxSprite_fill(self->darkness, self->weather->darknessColor);
	SUPER(FlxGroup_draw((FlxGroup *)self));
	ColorMatrix_applyFilter(&self->weather->ambientTransform, FlxG.camera->buffer);
}

void PlayState_showCoins(PlayState *self)
{
	int32_t c = ((Player *)self->player)->coins;
	Coinsack_show(self->sack, c);
}

void PlayState_showText(PlayState *self, const char *t)
{
	if (t != NULL) {
		/* textQueue = textQueue.concat(t.split('\n')) */
		const char *line = t;
		for (;;) {
			const char *nl = strchr(line, '\n');
			size_t n = nl ? (size_t)(nl - line) : strlen(line);
			char *part = as3_alloc(n + 1);
			memcpy(part, line, n);
			As3Array_set(&self->textQueue, self->textQueue.length, part);
			if (!nl)
				break;
			line = nl + 1;
		}
	}
	if (self->textQueue.length > 0 && self->textTimeout <= 0) {
		char *next = array_shift(&self->textQueue);
		FlxText_set_text(self->text, next);
		free(next);
		self->text->visible = true;
		self->textTimeout = Math_max(PlayState_TEXT_MIN_TIME, PlayState_TEXT_READ_SPEED * strlen(FlxText_get_text(self->text)));
	}
}

void PlayState_showCenterText(PlayState *self, const char *t)
{
	FlxText_set_text(self->centerText, t);
	self->centerText->visible = true;
	FlxSprite_set_alpha((FlxSprite *)self->centerText, 0.999);
}

void PlayState_panTo(PlayState *self, FlxSprite *o, Number duration, Number lead)
{
	self->cameraTimeout = duration;
	CameraTarget_set_target(self->cameraTarget, o);
	self->cameraTarget->lead = lead;
}
