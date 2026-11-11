/* Scenario driver for the existing behavioural oracles in ../tests/check.py.
 *
 * ../tests/core_test.c drives the native prototype through fixed scenarios and
 * writes CSV traces; check.py recomputes the expected values from the pinned
 * ActionScript rules and compares. This file sets up the same scenarios on the
 * transliterated game (real PlayState, real Player.update, real FlxObject
 * motion) and writes the same CSV columns, so the oracles run unchanged.
 *
 * Each case starts from a fresh PlayState. Inputs go through FlxG.keys like
 * the platform hosts do; one step = one 16 ms Flixel step (FlxG.elapsed 0.016).
 */
#include "../game/game.h"
#include "../flash/gpu.h"
#include "sound_expected.h" /* generated from PlayState.as by tests/gen_sound_expected.py */
#include <assert.h>

extern Number FlxG_initialSeed;

static FlxGame *game;

static PlayState *fresh_playstate(void)
{
	FlxG_switchState((FlxState *)PlayState_new(NULL));
	FlxGame_onEnterFrame(game, 16); /* performs the switch + first step */
	PlayState *ps = (PlayState *)FlxG.state;
	assert(as3_is((FlxBasic *)ps, &PlayState_class));
	return ps;
}

static void set_axis(int axis)
{
	FlxKeyboard_setRaw("RIGHT", axis > 0);
	FlxKeyboard_setRaw("LEFT", axis < 0);
}

static void step(void)
{
	FlxGame_onEnterFrame(game, 16);
}

/* One frame, calling capture() after PlayState's collisions and before its
   members update (core_test samples kg_combat_collisions_step there). */
static void (*pending_capture)(void);
static void probe(void)
{
	FlxGroup_stateUpdateProbe = NULL;
	if (pending_capture)
		pending_capture();
}
static void step_capture(void (*capture)(void))
{
	pending_capture = capture;
	FlxGroup_stateUpdateProbe = probe;
	step();
	FlxGroup_stateUpdateProbe = NULL;
}

/* check.py's animation codes: 0 stand, 1 walk_slow, 2 walk_fast, 3 eat */
static int anim_code(Player *p)
{
	const char *n = p->_curAnim ? p->_curAnim->name : "";
	return as3_streq(n, "walk_slow") ? 1 : as3_streq(n, "walk_fast") ? 2 : as3_streq(n, "eat") ? 3 : 0;
}

static int facing_sign(Player *p)
{
	return FlxSprite_get_facing((FlxSprite *)p) == RIGHT ? 1 : -1;
}

static void place_player(Player *p, Number x, Number food, Number lastMoved, int facing)
{
	p->x = x;
	p->velocity.x = 0;
	p->acceleration.x = 0;
	p->food = food;
	p->lastMoved = lastMoved;
	p->maxVelocity.x = Player_MAX_SPEED;
	FlxSprite_set_facing((FlxSprite *)p, facing > 0 ? RIGHT : LEFT);
}

/* V006: 120 frames right, 60 idle, 120 left from x=1000, food 100. */
static void write_player_motion_trace(void)
{
	FILE *trace = fopen("player_motion_trace.csv", "w");
	assert(trace);
	fputs("frame,axis,x,vx,food,max_speed,facing\n", trace);
	PlayState *ps = fresh_playstate();
	Player *p = (Player *)ps->player;
	place_player(p, 1000, 100, 0, 1);
	for (unsigned frame = 1; frame <= 300; frame++) {
		int axis = frame <= 120 ? 1 : frame <= 180 ? 0 : -1;
		set_axis(axis);
		step();
		fprintf(trace, "%u,%d,%.9f,%.9f,%.9f,%.9f,%d\n", frame, axis, p->x, p->velocity.x, p->food, p->maxVelocity.x,
			facing_sign(p));
	}
	set_axis(0);
	fclose(trace);
}

/* V007: food / rest cases, same table as core_test.c verify_player_food_rest. */
static void record_food_case(FILE *trace, const char *name, Number food, Number lastMoved, int facing, Number x, int axis,
	unsigned steps)
{
	PlayState *ps = fresh_playstate();
	Player *p = (Player *)ps->player;
	ps->horseAdvice = false;
	place_player(p, x, food, lastMoved, facing);
	for (unsigned frame = 1; frame <= steps; frame++) {
		Number bf = p->food, bl = p->lastMoved, bx = p->x;
		int bfacing = facing_sign(p), badvice = ps->horseAdvice;
		set_axis(axis);
		step();
		/* message_queue_count: PlayState.showText has no queue (it replaces the
		   current text), so the source-side count is always 0. */
		fprintf(trace, "%s,%u,%d,%.9f,%.9f,%d,%.9f,%d,%.9f,%.9f,%.9f,%d,%d,%d,%u\n", name, frame, axis, bf, bl, bfacing, bx,
			badvice, p->food, p->lastMoved, p->maxVelocity.x, facing_sign(p), ps->horseAdvice, anim_code(p), 0u);
	}
	set_axis(0);
}

static void verify_player_food_rest(void)
{
	FILE *trace = fopen("player_food_trace.csv", "w");
	assert(trace);
	fputs("case,frame,axis,food_before,last_moved_before,facing_before,x_before,advice_before,food,last_moved,max_speed,"
	      "facing,horse_advice,animation,message_queue_count\n", trace);
	record_food_case(trace, "move_zero", 0, 0, 1, 1000, 1, 1);
	record_food_case(trace, "move_ten", 10, 0, 1, 1000, 1, 1);
	record_food_case(trace, "move_near_ten", 10.02, 0, 1, 1000, 1, 1);
	record_food_case(trace, "move_half", 50, 0, 1, 1000, 1, 1);
	record_food_case(trace, "move_full", 100, 0, 1, 1000, 1, 1);
	record_food_case(trace, "move_negative", -0.005, 0, 1, 1000, 1, 1);
	record_food_case(trace, "move_tired_once", 10, 0, 1, 1000, 1, 2);
	record_food_case(trace, "rest_strict_threshold", 50, 61 * 0.016, 1, 701, 0, 2);
	record_food_case(trace, "rest_grass_right", 50, 1, 1, 701, 0, 1);
	record_food_case(trace, "rest_grass_left", 50, 1, -1, 783, 0, 1);
	record_food_case(trace, "rest_non_grass_right", 50, 1, 1, 669, 0, 1);
	record_food_case(trace, "rest_non_grass_left", 50, 1, -1, 719, 0, 1);
	record_food_case(trace, "rest_near_full", 99.95, 1, 1, 701, 0, 1);
	record_food_case(trace, "rest_full", 100, 1, 1, 701, 0, 1);
	fclose(trace);
}


/* ---- world isolation helpers ------------------------------------------- */

int HostMedia_plays(const char *part);
void HostMedia_resetPlays(void);
int HostMedia_stops(void);

static void clear_group(FlxGroup *g)
{
	for (uint32_t i = 0; i < g->members.length; i++) {
		FlxBasic *b = As3Array_get(&g->members, i);
		if (b) {
			b->exists = false;
			b->alive = false;
		}
	}
}

/* Fresh PlayState with no NPCs, trolls, bunnies, coins or effects, the
   player parked at x (like core_test's g.x), phases and spawns frozen. */
static PlayState *isolated_playstate(Number playerX)
{
	PlayState *ps = fresh_playstate();
	FlxGroup *groups[] = {ps->beggars, ps->characters, ps->trolls, ps->trollsNoCollide, ps->bunnies, ps->coins, ps->gibs,
		ps->arrows, ps->fx};
	for (unsigned i = 0; i < sizeof groups / sizeof groups[0]; i++)
		clear_group(groups[i]);
	ps->trollsToSpawn.length = 0;
	ps->timeToNextPhase = 1e9;
	ps->bunnySpawnTimer = 1e9;
	ps->minBeggars = 0; /* PlayState.update would otherwise spawn a beggar */
	Player *p = (Player *)ps->player;
	p->x = playerX;
	p->velocity.x = 0;
	HostMedia_resetPlays();
	return ps;
}

static Citizen *add_citizen(PlayState *ps, Number x, Number y, int occ, int coins, int action)
{
	Citizen *c = Citizen_new((int32_t)x, (int32_t)y);
	FlxGroup_add(ps->beggars, (FlxBasic *)c);
	/* the game's own path: Beggar -> Poor (joins characters) -> job */
	if (occ != Citizen_BEGGAR)
		Citizen_morph(c, Citizen_POOR);
	if (occ != Citizen_BEGGAR && occ != Citizen_POOR)
		Citizen_morph(c, occ);
	c->x = x;
	c->y = y;
	c->coins = coins;
	c->action = action;
	c->t = 0;
	c->goal = x;
	c->velocity.x = 0;
	c->maxVelocity.x = 0;
	return c;
}

static Troll *add_troll(PlayState *ps, Number x, Number y, Number health, bool hasCoin)
{
	Troll *t = Troll_new();
	FlxGroup_add(ps->trolls, (FlxBasic *)t);
	Troll_reset(t, x, y);
	Troll_go(t);
	t->x = x;
	t->y = y;
	t->health = health;
	t->hasCoin = hasCoin;
	t->maxSpeed = 0;
	t->jumpiness = t->jumpCooldown = 1e6;
	t->confusion = t->confuseCooldown = 16;
	t->goal = x;
	t->t = 0;
	return t;
}

static int count_living(FlxGroup *g)
{
	int n = 0;
	for (uint32_t i = 0; i < g->members.length; i++) {
		FlxBasic *b = As3Array_get(&g->members, i);
		n += (b && b->exists && b->alive);
	}
	return n;
}

/* core_test owner codes: -1 none, -2 player, -3 troll */
static int owner_code(PlayState *ps, Coin *c)
{
	if (c->owner == NULL)
		return -1;
	if (c->owner == (FlxObject *)ps->player)
		return -2;
	if (as3_is((FlxBasic *)c->owner, &Troll_class))
		return -3;
	return -4;
}

static Coin *living_coin(PlayState *ps, int index)
{
	for (uint32_t i = 0; i < ps->coins->members.length; i++) {
		Coin *c = As3Array_get(&ps->coins->members, i);
		if (c && c->exists && c->alive && index-- == 0)
			return c;
	}
	return NULL;
}

static int index_in(FlxGroup *g, FlxBasic *b)
{
	int n = 0;
	for (uint32_t i = 0; i < g->members.length; i++) {
		FlxBasic *m = As3Array_get(&g->members, i);
		if (m && m->exists && m->alive) {
			if (m == b)
				return n;
			n++;
		}
	}
	return -1;
}

/* ---- V011 scenarios ------------------------------------------------------
 * core_test samples these right after kg_combat_collisions_step, i.e. after
 * PlayState.update's FlxG.overlap calls and before super.update(); the
 * translit samples at the same point through step_capture(). */

static FILE *cap_trace;
static PlayState *cap_ps;
static Citizen *cap_a, *cap_b;
static Troll *cap_troll;
static const void *cap_case;

typedef struct {
	const char *name;
	int occupation, coins, action;
	bool troll_has_coin;
	int e_occ, e_coins, e_action, e_health, e_drops, e_owner, e_sound, e_gib;
} MatrixCase;

static void capture_matrix(void)
{
	const MatrixCase *c = cap_case;
	PlayState *ps = cap_ps;
	Citizen *cit = cap_a;
	Troll *tr = cap_troll;
	Coin *d = living_coin(ps, 0);
	fprintf(cap_trace, "%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", c->name,
		c->occupation, c->coins, c->action, c->troll_has_coin, c->e_occ, c->e_coins, c->e_action, c->e_health, c->e_drops,
		c->e_owner, c->e_sound, c->e_gib, cit->occupation, cit->coins, cit->action, (int)tr->health, tr->alive,
		tr->hasCoin, count_living(ps->coins), d ? owner_code(ps, d) : -1, HostMedia_plays("hitcitizen"),
		count_living(ps->gibs) > 0, count_living(ps->characters), count_living(ps->beggars));
}

static void write_v011_troll_damage_matrix(void)
{
	static const MatrixCase cases[] = {
		{"poor_zero_coinless", 1, 0, 0, false, 1, 0, 0, 3, 0, -1, 0, 0},
		{"poor_one_coinless", 1, 1, 0, false, 0, 0, 6, 3, 1, -2, 1, 1},
		{"poor_two_coinless", 1, 2, 0, false, 1, 1, 0, 3, 1, -2, 1, 1},
		{"poor_four_coinless", 1, 4, 0, false, 1, 1, 0, 3, 1, -2, 1, 1},
		{"poor_two_troll_carrier", 1, 2, 0, true, 1, 2, 0, 3, 0, -1, 0, 0},
		{"beggar_zero_coinless", 0, 0, 0, false, 0, 0, 6, 3, 0, -1, 0, 0},
		{"farmer_first_coinless", 2, 2, 0, false, 2, 2, 5, 2, 0, -1, 0, 0},
		{"farmer_first_troll_carrier", 2, 2, 0, true, 2, 2, 5, 2, 0, -1, 0, 0},
		{"farmer_rehit_zero_coinless", 2, 0, 5, false, 2, 0, 5, 3, 0, -1, 0, 0},
		{"farmer_rehit_one_coinless", 2, 1, 5, false, 0, 0, 6, 3, 1, -2, 1, 1},
		{"farmer_rehit_two_coinless", 2, 2, 5, false, 1, 1, 0, 3, 1, -2, 1, 1},
		{"farmer_rehit_two_troll_carrier", 2, 2, 5, true, 2, 2, 5, 3, 0, -1, 0, 0},
	};
	cap_trace = fopen("troll_damage_matrix.csv", "w");
	assert(cap_trace);
	fputs("case,input_occupation,input_coins,input_action,troll_has_coin,expected_occupation,expected_coins,expected_action,"
	      "expected_troll_health,expected_drops,expected_drop_owner,expected_sound10,expected_gib,actual_occupation,"
	      "actual_coins,actual_action,actual_troll_health,actual_troll_alive,actual_troll_has_coin,actual_drops,"
	      "actual_drop_owner,actual_sound10,actual_gib,characters_order_count,beggars_order_count\n", cap_trace);
	for (unsigned k = 0; k < sizeof cases / sizeof cases[0]; k++) {
		const MatrixCase *c = &cases[k];
		cap_ps = isolated_playstate(1000);
		cap_a = add_citizen(cap_ps, 96, 108, c->occupation, c->coins, c->action);
		cap_troll = add_troll(cap_ps, 100, 112, 3, c->troll_has_coin);
		cap_case = c;
		step_capture(capture_matrix);
	}
	fclose(cap_trace);

	/* strict 4 s JUST_HACKED cooldown of a zero-coin Farmer (frames 249-251),
	   sampled after whole frames like core_test's kg_step */
	FILE *trace = fopen("farmer_hack_cooldown.csv", "w");
	assert(trace);
	fputs("frame,source_expected_action,source_expected_troll_health,native_action,native_troll_health,native_state_timer\n",
	      trace);
	PlayState *ps = isolated_playstate(1000);
	Citizen *farmer = add_citizen(ps, 96, 108, Citizen_FARMER, 0, Citizen_IDLE);
	farmer->coins = 0;
	Troll *tr = add_troll(ps, 100, 112, 3, false);
	static const int expAction[] = {5, 0, 5}, expHealth[] = {2, 2, 1};
	for (int frame = 1; frame <= 251; frame++) {
		tr->t = 0; /* keep the troll on its goal (core_test: goal_timer) */
		step();
		if (frame >= 249)
			fprintf(trace, "%d,%d,%d,%d,%d,%.9f\n", frame, expAction[frame - 249], expHealth[frame - 249], farmer->action,
				(int)tr->health, farmer->t);
	}
	fclose(trace);
}

static void capture_farmer_then_poor(void)
{
	PlayState *ps = cap_ps;
	Coin *d = living_coin(ps, 0);
	fprintf(cap_trace, "farmer_then_one_coin_poor,0,0|1,2,0,0,1,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", cap_a->action,
		(int)cap_troll->health, cap_troll->alive, cap_troll->hasCoin, cap_b->occupation, cap_b->coins, cap_b->action,
		count_living(ps->coins), d ? owner_code(ps, d) : -1, HostMedia_plays("hitcitizen"), count_living(ps->gibs) > 0,
		index_in(ps->characters, (FlxBasic *)cap_a), index_in(ps->beggars, (FlxBasic *)cap_b) >= 0 ? 1 : -1);
}

static void write_v011_farmer_then_poor(void)
{
	cap_trace = fopen("troll_citizen_overlap_trace.csv", "w");
	assert(cap_trace);
	fputs("case,source_troll_order,source_character_order,initial_troll_health,initial_troll_has_coin,farmer_action,"
	      "poor_coins,farmer_action_after,troll_health_after,troll_alive_after,troll_has_coin_after,poor_occupation_after,"
	      "poor_coins_after,poor_action_after,drops,drop_owner,sound10,gib,characters_order,beggars_order\n", cap_trace);
	cap_ps = isolated_playstate(1000);
	cap_a = add_citizen(cap_ps, 96, 108, Citizen_FARMER, 2, Citizen_IDLE);
	cap_b = add_citizen(cap_ps, 96, 108, Citizen_POOR, 1, Citizen_IDLE);
	cap_troll = add_troll(cap_ps, 100, 112, 2, false);
	step_capture(capture_farmer_then_poor);
	fclose(cap_trace);
}

static void capture_farmer_poor_player(void)
{
	PlayState *ps = cap_ps;
	Player *p = (Player *)ps->player;
	Coin *d0 = living_coin(ps, 0), *d1 = living_coin(ps, 1);
	fprintf(cap_trace, "0,0|1,3,1,0.200,0,1,%d,%d,%d,%d,%d,%d,%d,%d,%.3f,%d,%d,%d,%.1f,%.1f,%d,%d,%.1f,%.1f,%d,%d,%.1f,%d,%d\n",
		cap_a->action, (int)cap_troll->health, cap_troll->alive, cap_troll->hasCoin, cap_b->occupation, cap_b->coins,
		cap_b->action, p->coins, p->lastTrollHit, count_living(ps->coins), d0 ? owner_code(ps, d0) : -1,
		d0 ? d0->justThrown : 0, d0 ? d0->x : 0, d0 ? d0->y : 0, d1 ? owner_code(ps, d1) : -1, d1 ? d1->justThrown : 0,
		d1 ? d1->x : 0, d1 ? d1->y : 0, HostMedia_plays("hitcitizen"), HostMedia_plays("stolen"),
		/* camera_shake: FlxG.shake duration of the main camera */ FlxG.camera->_fxShakeDuration,
		index_in(ps->characters, (FlxBasic *)cap_a), index_in(ps->beggars, (FlxBasic *)cap_b) >= 0 ? 1 : -1);
}

static void write_v011_farmer_poor_player(void)
{
	cap_trace = fopen("troll_citizen_player_overlap_trace.csv", "w");
	assert(cap_trace);
	fputs("source_troll_order,source_character_order,initial_troll_health,initial_player_coins,initial_player_hit_timer,"
	      "farmer_action,poor_coins,farmer_action_after,troll_health_after,troll_alive_after,troll_has_coin_after,"
	      "poor_occupation_after,poor_coins_after,poor_action_after,player_coins_after,player_hit_timer_after,drop_count,"
	      "drop0_owner,drop0_thrown,drop0_x,drop0_y,drop1_owner,drop1_thrown,drop1_x,drop1_y,sound10,sound6,camera_shake,"
	      "characters_order,beggars_order\n", cap_trace);
	cap_ps = isolated_playstate(96);
	Player *p = (Player *)cap_ps->player;
	p->coins = 1;
	p->lastTrollHit = 0.2;
	cap_a = add_citizen(cap_ps, 96, 108, Citizen_FARMER, 0, Citizen_IDLE);
	cap_a->coins = 0;
	cap_b = add_citizen(cap_ps, 96, 108, Citizen_POOR, 1, Citizen_IDLE);
	cap_troll = add_troll(cap_ps, 100, 112, 3, false);
	step_capture(capture_farmer_poor_player);
	fclose(cap_trace);
}

/* ---- V014: tax AABB edges and recipient order (Citizen.giveTaxes) ---------- */

static Citizen *tax_citizens[2];
static int tax_count;

static void place_still(FlxObject *o, Number x, Number y)
{
	o->x = o->last.x = x;
	o->y = o->last.y = y;
	o->velocity.x = o->velocity.y = 0;
}

/* order of first notify per citizen in the logged FlxG.overlap callbacks */
static void tax_order(char *out, size_t n, FlxObject *player)
{
	out[0] = 0;
	bool seen[2] = {false, false};
	for (int i = 0; i < FlxQuadTree_testLogCount; i++) {
		if (FlxQuadTree_testLog[i][1] != player)
			continue;
		for (int k = 0; k < tax_count; k++)
			if (FlxQuadTree_testLog[i][0] == tax_citizens[k] && !seen[k]) {
				seen[k] = true;
				size_t l = strlen(out);
				snprintf(out + l, n - l, "%s%d", l ? "|" : "", k);
			}
	}
}

static const struct { const char *name; Number x, y; bool eligible; } tax_edges[] = {
	{"left_edge_touch", 92.0, 100.0, false}, {"left_edge_inside", 92.001, 100.0, true},
	{"right_edge_touch", 120.0, 100.0, false}, {"right_edge_inside", 119.999, 100.0, true},
	{"top_edge_touch", 100.0, 76.0, false}, {"top_edge_inside", 100.0, 76.001, true},
	{"bottom_edge_touch", 100.0, 132.0, false}, {"bottom_edge_inside", 100.0, 131.999, true},
};
static int tax_case;

static void capture_tax_edge(void)
{
	Player *p = (Player *)cap_ps->player;
	char order[32];
	tax_order(order, sizeof order, (FlxObject *)p);
	bool e = tax_edges[tax_case].eligible;
	fprintf(cap_trace, "%s,%s,%s,%d,%d,%d,%d,%d,%d\n", tax_edges[tax_case].name, e ? "0" : "", order, 7 + (e ? 1 : 0), p->coins,
		4 - (e ? 2 : 0), tax_citizens[0]->coins, e ? Citizen_GIVE : Citizen_IDLE, tax_citizens[0]->action);
}

static void capture_tax_two(void)
{
	Player *p = (Player *)cap_ps->player;
	char order[32];
	tax_order(order, sizeof order, (FlxObject *)p);
	/* index 0 = the Farmer, 1 = the Hunter, as in core_test */
	fprintf(cap_trace, "two_recipients_reversed,1|0,%s,9,%d,2|3,%d|%d,%d|%d,%d|%d\n", order, p->coins, tax_citizens[0]->coins,
		tax_citizens[1]->coins, Citizen_GIVE, Citizen_GIVE, tax_citizens[0]->action, tax_citizens[1]->action);
}

static void write_v014_tax(void)
{
	cap_trace = fopen("tax_overlap_trace.csv", "w");
	assert(cap_trace);
	fputs("case,source_expected_order,native_callback_order,source_expected_player_coins,native_player_coins,"
	      "source_expected_npc_coins,native_npc_coins,source_expected_action,native_action\n", cap_trace);
	FlxQuadTree_testLogEnabled = true;
	for (tax_case = 0; tax_case < 8; tax_case++) {
		cap_ps = isolated_playstate(100);
		Player *p = (Player *)cap_ps->player;
		place_still((FlxObject *)p, 100, 100);
		p->coins = 7;
		tax_count = 1;
		tax_citizens[0] = add_citizen(cap_ps, 0, 0, Citizen_FARMER, 4, Citizen_IDLE);
		place_still((FlxObject *)tax_citizens[0], tax_edges[tax_case].x, tax_edges[tax_case].y);
		FlxQuadTree_testLogCount = 0;
		step_capture(capture_tax_edge);
	}
	/* two recipients, live characters order reversed: Hunter (1) before Farmer (0) */
	cap_ps = isolated_playstate(100);
	Player *p = (Player *)cap_ps->player;
	place_still((FlxObject *)p, 100, 100);
	p->coins = 7;
	tax_count = 2;
	tax_citizens[1] = add_citizen(cap_ps, 0, 0, Citizen_HUNTER, 5, Citizen_IDLE);
	tax_citizens[0] = add_citizen(cap_ps, 0, 0, Citizen_FARMER, 4, Citizen_IDLE);
	place_still((FlxObject *)tax_citizens[0], 108, 100);
	place_still((FlxObject *)tax_citizens[1], 106, 100);
	FlxQuadTree_testLogCount = 0;
	step_capture(capture_tax_two);
	FlxQuadTree_testLogEnabled = false;
	fclose(cap_trace);
}

/* ---- direct source-derived checks (no CSV: expectations written here) -----
 * Each check cites the pinned ActionScript rule it encodes and writes one
 * PASS/FAIL line to translit_checks.txt (tests/run_native_oracles.py prints it).
 * Same scenarios and expectations as check.py::compare_work_dispatch_to_pinned_source
 * (V012), but on the real translated Citizen/Wall/Farmland/PlayState. */
static FILE *checks;
static int check_failures;

static void check(bool ok, const char *name, const char *detail)
{
	fprintf(checks, "%s: %s%s%s\n", ok ? "PASS" : "FAIL", name, ok ? "" : " -- ", ok ? "" : detail);
	check_failures += !ok;
	fflush(checks); /* a later hang still leaves the earlier results */
}

static int living(FlxGroup *g, const As3Class *klass)
{
	int n = 0;
	for (uint32_t i = 0; i < g->members.length; i++) {
		FlxBasic *b = As3Array_get(&g->members, i);
		if (b && b->exists && b->alive && (klass == NULL || as3_is(b, klass)))
			n++;
	}
	return n;
}

/* Citizen.checkWork: strict containment x > c.x && x+width < c.x+c.width, every
 * group member visited; Wall.needsWork: damaged && t < HURT_COOLDOWN, work heals
 * 4; Farmland.needsWork: t > WORK_COOLDOWN, work resets t and advances frame % 8. */
static void verify_work_dispatch(void)
{
	static const struct { const char *name; bool wall; Number object_x, citizen_x; } edges[] = {
		{"wall_left_edge", true, 100, 100}, {"wall_right_edge", true, 100, 116}, {"wall_inside", true, 100, 101},
		{"farm_left_edge", false, 200, 200}, {"farm_right_edge", false, 200, 256}, {"farm_inside", false, 200, 201},
	};
	for (unsigned i = 0; i < sizeof edges / sizeof edges[0]; i++) {
		PlayState *ps = isolated_playstate(100);
		clear_group(ps->walls);
		clear_group(ps->farmlands);
		Citizen *c = add_citizen(ps, edges[i].citizen_x, 108, Citizen_FARMER, 0, Citizen_IDLE);
		Number width = edges[i].wall ? 24 : 64;
		bool inside = edges[i].citizen_x > edges[i].object_x && edges[i].citizen_x + c->width < edges[i].object_x + width;
		char detail[128];
		if (edges[i].wall) {
			Wall *w = Wall_new(edges[i].object_x, 68);
			w->stage = 1;
			w->health = 30;
			w->t = 2; /* shipped SWF: a wall is workable once its hurt timer is older than 1 s */
			FlxGroup_add(ps->walls, (FlxBasic *)w);
			Citizen_checkWork(c, ps->walls);
			Citizen_checkWork(c, ps->farmlands);
			snprintf(detail, sizeof detail, "health %g, inside %d, width %g", w->health, inside, c->width);
			check(w->health == (inside ? 34 : 30), edges[i].name, detail);
		} else {
			Farmland *f = Farmland_new((int32_t)edges[i].object_x, 100);
			FlxSprite_set_frame((FlxSprite *)f, 2);
			f->t = 9;
			FlxGroup_add(ps->farmlands, (FlxBasic *)f);
			Citizen_checkWork(c, ps->walls);
			Citizen_checkWork(c, ps->farmlands);
			uint32_t frame = FlxSprite_get_frame((FlxSprite *)f);
			snprintf(detail, sizeof detail, "frame %u t %g, inside %d", frame, f->t, inside);
			check(frame == (inside ? 3u : 2u) && f->t == (inside ? 0 : 9), edges[i].name, detail);
		}
	}
	/* two walls and two mature fields under one farmer in one update: walls
	   first (W0 W1), then fields (F0 F1); each harvest drops 4 coins; the last
	   harvest retargets the farmer to its field's centre; the update's motion
	   uses the pre-work goal (facing left) and the first frame-zero dust is
	   emitted in that same update. */
	PlayState *ps = isolated_playstate(100);
	clear_group(ps->walls);
	clear_group(ps->farmlands);
	Citizen *c = add_citizen(ps, 112, 108, Citizen_FARMER, 0, Citizen_IDLE);
	c->goal = 50;
	c->maxVelocity.x = 22.5;
	Wall *w0 = Wall_new(100, 68), *w1 = Wall_new(98, 68);
	w0->stage = w1->stage = 1;
	w0->health = 30;
	w1->health = 32;
	w0->t = w1->t = 2; /* older than HURT_COOLDOWN, as in the shipped SWF rule */
	FlxGroup_add(ps->walls, (FlxBasic *)w0);
	FlxGroup_add(ps->walls, (FlxBasic *)w1);
	Farmland *f0 = Farmland_new(110, 100), *f1 = Farmland_new(108, 100);
	FlxSprite_set_frame((FlxSprite *)f0, 7);
	FlxSprite_set_frame((FlxSprite *)f1, 7);
	f0->t = f1->t = 9;
	FlxGroup_add(ps->farmlands, (FlxBasic *)f0);
	FlxGroup_add(ps->farmlands, (FlxBasic *)f1);
	c->shovelCooldown = 0;
	step();
	char detail[256];
	snprintf(detail, sizeof detail,
		"walls %g/%g farms %u/%u t %g/%g coins %d goal %g action %d cooldown %g facing %d x %.5f dust %d", w0->health, w1->health,
		FlxSprite_get_frame((FlxSprite *)f0), FlxSprite_get_frame((FlxSprite *)f1), f0->t, f1->t, living(ps->coins, NULL), c->goal,
		c->action, c->shovelCooldown, facing_sign((Player *)c), c->x, living(ps->fx, &Dust_class));
	check(w0->health == 34 && w1->health == 36, "mixed: both walls healed by 4 (W0 W1)", detail);
	check(FlxSprite_get_frame((FlxSprite *)f0) == 0 && FlxSprite_get_frame((FlxSprite *)f1) == 0 && f0->t < 0.1 && f1->t < 0.1,
		"mixed: both fields harvested and advanced to frame 0 (F0 F1)", detail);
	check(living(ps->coins, NULL) == 8, "mixed: two harvests drop 8 coins", detail);
	check(c->goal == 140, "mixed: goal = last harvested field centre (108 + 64/2)", detail);
	check(c->action == Citizen_SHOVEL && c->shovelCooldown > 3.9 && c->shovelCooldown <= 4, "mixed: SHOVEL with SHOVEL_PERIOD cooldown", detail);
	check(facing_sign((Player *)c) == -1 && c->x < 112, "mixed: same-update motion uses the pre-work goal (left)", detail);
	check(living(ps->fx, &Dust_class) == 1, "mixed: first shovel frame emits dust in the same update", detail);
}

/* Castle.morph selects ARCHER_POSITIONS[ARCHER_POSITION_INDEX[stage]].slice();
 * Citizen.checkGuard claims a post when |castle.x - x| < 192 and the strict
 * |castle.x + pos.x - x| < 4, moves the citizen onto the post and from
 * characters to archers, and removes that post (check.py
 * compare_guard_morph_to_pinned_source: stage table, strict gate, group moves). */
static void verify_guard(void)
{
	static const int stage_table[15] = {0, 0, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4};
	static const int counts[5] = {1, 2, 3, 4, 6};
	static const int table_x[5][6] = {{26, 0, 0, 0, 0, 0}, {16, 104, 0, 0, 0, 0}, {16, 104, 60, 0, 0, 0}, {16, 104, 45, 75, 0, 0},
		{16, 104, 38, 83, 27, 93}};
	static const int table_y[5][6] = {{60, 0, 0, 0, 0, 0}, {47, 47, 0, 0, 0, 0}, {47, 47, 2, 0, 0, 0}, {47, 47, 2, 2, 0, 0},
		{47, 47, 2, 2, 20, 20}};
	PlayState *ps = isolated_playstate(100);
	Castle *castle = ps->castle;
	bool tableOk = true;
	char detail[128] = "";
	for (int stage = 0; stage < 15 && tableOk; stage++) {
		Castle_morph(castle, stage);
		int t = stage_table[stage];
		tableOk = castle->archer_positions.length == (uint32_t)counts[t];
		for (int i = 0; tableOk && i < counts[t]; i++) {
			FlxPoint *pos = As3Array_get(&castle->archer_positions, (uint32_t)i);
			tableOk = pos != NULL && pos->x == table_x[t][i] && pos->y == table_y[t][i];
		}
		if (!tableOk)
			snprintf(detail, sizeof detail, "stage %d table %d", stage, t);
	}
	check(tableOk, "guard: Castle.morph archer posts for stages 0..14", detail);

	Castle_morph(castle, 2); /* table 1: posts (16,47) and (104,47) */
	Number postX = castle->x + 16;
	Citizen *near = add_citizen(ps, postX + 3.999, 108, Citizen_HUNTER, 0, Citizen_IDLE);
	Citizen *far = add_citizen(ps, postX + 4.0, 108, Citizen_HUNTER, 0, Citizen_IDLE);
	Citizen_checkGuard(far);
	check(!far->guarding && castle->archer_positions.length == 2, "guard: |castle.x+pos.x-x| == 4 does not claim (strict)", "claimed");
	Citizen_checkGuard(near);
	snprintf(detail, sizeof detail, "guarding %d posts %u x %g y %g archers %d chars %d", near->guarding,
		castle->archer_positions.length, near->x, near->y, living(ps->archers, NULL), living(ps->characters, near->klass));
	check(near->guarding && castle->archer_positions.length == 1, "guard: |...| < 4 claims the post and removes it", detail);
	check(near->x == postX && near->y == castle->y + 47, "guard: claimed citizen snaps onto the post", detail);
	check(living(ps->archers, NULL) == 1, "guard: claimed citizen moves from characters to archers", detail);
	Citizen_leaveGuard(near);
	check(!near->guarding && castle->archer_positions.length == 2, "guard: leaveGuard appends the citizen position and returns it to characters", detail);
}

/* Shop.as: geometry (width 56, height 46, offset 4/18), type by X > GAME_WIDTH/2,
 * frame = supply (+5 for bows), build(): boughtItem, supply+1, flicker(0.3),
 * equip: only a Poor citizen, scythes -> Farmer, bows -> Hunter, supply-1;
 * FlxSprite.draw hides every other drawn frame while flickering
 * (check.py compare_shop_flicker_to_pinned_source). */
static void verify_shop(void)
{
	PlayState *ps = isolated_playstate(100);
	Shop *scythes = Shop_new(100, 50), *bows = Shop_new(2000, 50);
	char detail[160];
	snprintf(detail, sizeof detail, "w %g h %g off %g/%g x %g y %g types %d/%d", scythes->width, scythes->height, scythes->offset.x,
		scythes->offset.y, scythes->x, scythes->y, scythes->type, bows->type);
	check(scythes->width == 56 && scythes->height == 46 && scythes->offset.x == 4 && scythes->offset.y == 18 &&
		scythes->x == 104 && scythes->y == 70, "shop: geometry (56x46, offset 4/18, position +4/+2+18)", detail);
	check(scythes->type == Shop_SCYTHES && bows->type == Shop_BOWS, "shop: type by X > GAME_WIDTH/2", detail);
	ps->boughtItem = false;
	Shop_build(scythes);
	Shop_build(bows);
	Shop_build(bows);
	snprintf(detail, sizeof detail, "supply %d/%d frames %u/%u bought %d timer %g", scythes->supply, bows->supply,
		FlxSprite_get_frame((FlxSprite *)scythes), FlxSprite_get_frame((FlxSprite *)bows), ps->boughtItem, scythes->_flickerTimer);
	check(ps->boughtItem && scythes->supply == 1 && bows->supply == 2, "shop: build() sets boughtItem and adds one supply", detail);
	check(FlxSprite_get_frame((FlxSprite *)scythes) == 1 && FlxSprite_get_frame((FlxSprite *)bows) == 7,
		"shop: frame = supply, +5 for bows", detail);
	check(scythes->_flickerTimer == 0.3, "shop: build() flickers for 0.3 s", detail);
	bool first = false, second = false;
	scythes->_flicker = false;
	FlxSprite_draw((FlxSprite *)scythes);
	first = scythes->_flicker; /* hidden: the draw returned before drawing */
	FlxSprite_draw((FlxSprite *)scythes);
	second = scythes->_flicker;
	check(first && !second, "shop: flicker hides the first draw and shows the next", "flicker sequence wrong");
	scythes->supply = 3;
	Shop_setSupply(scythes, 3);
	check(Shop_canBuild(scythes) && FlxSprite_get_frame((FlxSprite *)scythes) == 3, "shop: canBuild while supply < 4", "canBuild");
	Shop_setSupply(scythes, 4);
	check(!Shop_canBuild(scythes), "shop: cannot build at supply 4", "canBuild");
	/* equip */
	Citizen *poor = add_citizen(ps, 0, 0, Citizen_POOR, 0, Citizen_IDLE);
	Citizen *beggar = add_citizen(ps, 0, 0, Citizen_BEGGAR, 0, Citizen_IDLE);
	Shop_equip(scythes, (FlxObject *)scythes, (FlxObject *)beggar);
	check(beggar->occupation == Citizen_BEGGAR && scythes->supply == 4, "shop: equip ignores non-Poor", "beggar equipped");
	Shop_equip(scythes, (FlxObject *)scythes, (FlxObject *)poor);
	check(poor->occupation == Citizen_FARMER && scythes->supply == 3 && FlxSprite_get_frame((FlxSprite *)scythes) == 3,
		"shop: scythes turn a Poor citizen into a Farmer, supply -1", "scythe equip");
	Citizen *poor2 = add_citizen(ps, 0, 0, Citizen_POOR, 0, Citizen_IDLE);
	Shop_equip(bows, (FlxObject *)bows, (FlxObject *)poor2);
	check(poor2->occupation == Citizen_HUNTER && bows->supply == 1 && FlxSprite_get_frame((FlxSprite *)bows) == 6,
		"shop: bows turn a Poor citizen into a Hunter, supply -1, frame 5 + supply", "bow equip");
	Shop_setSupply(bows, 0);
	Citizen *poor3 = add_citizen(ps, 0, 0, Citizen_POOR, 0, Citizen_IDLE);
	Shop_equip(bows, (FlxObject *)bows, (FlxObject *)poor3);
	check(poor3->occupation == Citizen_POOR && bows->supply == 0, "shop: equip does nothing without supply", "equipped with 0 supply");
}

/* Bunny.as: 16x16 sprite with height 12, offset.y 4, maxVelocity (30,100),
 * drag.x 1000, y = groundHeight - 12; alive: acceleration = +-maxVelocity.x*4
 * by facing, every TURN_TIME (t > 1, strict) t resets and 40 % turn; getShot:
 * dead, vx 0, t 0, drops one coin for the arrow's shooter; dead: alpha =
 * 1 - t/DECAY_TIME and killed once t > DECAY_TIME (5); outside [0, GAME_WIDTH]
 * it is killed (check.py compare_v020_bunny_lifecycle_to_pinned_source). */
static void verify_bunny(void)
{
	PlayState *ps = isolated_playstate(100);
	char detail[160];
	Bunny *b = Bunny_new(500, 0);
	FlxGroup_add(ps->bunnies, (FlxBasic *)b);
	snprintf(detail, sizeof detail, "height %g offset.y %g maxV %g/%g drag %g y %g", b->height, b->offset.y, b->maxVelocity.x,
		b->maxVelocity.y, b->drag.x, b->y);
	check(b->height == 12 && b->offset.y == 4 && b->maxVelocity.x == 30 && b->maxVelocity.y == 100 && b->drag.x == 1000 &&
		b->y == ps->groundHeight - 12, "bunny: constructor geometry and ground anchor", detail);
	FlxSprite_set_facing((FlxSprite *)b, LEFT);
	FlxSprite_play((FlxSprite *)b, "stand", true);
	Bunny_update(b);
	snprintf(detail, sizeof detail, "acc %g anim %s alive %d", b->acceleration.x, b->_curAnim ? b->_curAnim->name : "-", b->alive);
	check(b->acceleration.x == -120 && as3_streq(b->_curAnim->name, "walk"), "bunny: facing left accelerates -maxVelocity.x*4 and walks", detail);
	/* turn gate: t resets exactly when t > TURN_TIME (1 s = 63 updates of 0.016) */
	b->t = 0;
	int resetAt = 0;
	for (int i = 1; i <= 80 && !resetAt; i++) {
		Bunny_update(b);
		if (b->t == 0)
			resetAt = i;
	}
	snprintf(detail, sizeof detail, "reset after %d updates", resetAt);
	check(resetAt == 63, "bunny: TURN_TIME gate is strict (t > 1)", detail);
	/* shot */
	Arrow *arrow = Arrow_new();
	arrow->shooter = (FlxObject *)ps->player;
	int coinsBefore = living(ps->coins, NULL);
	b->velocity.x = 17;
	b->t = 3;
	Bunny_getShot(b, arrow);
	snprintf(detail, sizeof detail, "alive %d vx %g t %g coins %d->%d", b->alive, b->velocity.x, b->t, coinsBefore, living(ps->coins, NULL));
	check(!b->alive && b->velocity.x == 0 && b->t == 0 && as3_streq(b->_curAnim->name, "dead"), "bunny: getShot kills, stops and restarts the corpse clock", detail);
	check(living(ps->coins, NULL) == coinsBefore + 1, "bunny: getShot drops one coin", detail);
	/* corpse decay */
	int killedAt = 0;
	Number alphaAt100 = 0;
	for (int i = 1; i <= 400 && !killedAt; i++) {
		Bunny_update(b);
		if (i == 100)
			alphaAt100 = FlxSprite_get_alpha((FlxSprite *)b);
		if (!b->exists)
			killedAt = i;
	}
	snprintf(detail, sizeof detail, "killed after %d updates, alpha@100 %g", killedAt, alphaAt100);
	check(killedAt == 313 && fabs(alphaAt100 - (1 - 100 * 0.016 / 5)) < 1e-9, "bunny: corpse fades as 1 - t/5 and is killed once t > 5", detail);
	/* leaving the world kills it */
	Bunny *edge = Bunny_new(3830, 0);
	FlxGroup_add(ps->bunnies, (FlxBasic *)edge);
	edge->x = 3830;
	Bunny_update(edge);
	Bunny *left = Bunny_new(-1, 0);
	FlxGroup_add(ps->bunnies, (FlxBasic *)left);
	left->x = -1;
	Bunny_update(left);
	check(!edge->exists && !left->exists, "bunny: killed outside [0, GAME_WIDTH]", "still exists");
}

static bool in_group(FlxGroup *g, const void *o)
{
	for (uint32_t i = 0; i < g->members.length; i++)
		if (As3Array_get(&g->members, i) == o)
			return true;
	return false;
}

static Number anim_fps(Troll *t, const char *name)
{
	for (uint32_t i = 0; i < t->_animations.length; i++) {
		FlxAnim *a = As3Array_get(&t->_animations, i);
		if (a != NULL && as3_streq(a->name, name))
			return 1.0 / a->delay;
	}
	return -1;
}

/* the animation table is append-only (FlxSprite.addAnimation pushes, never replaces) */
static Number anim_fps_last(Troll *t, const char *name)
{
	Number r = -1;
	for (uint32_t i = 0; i < t->_animations.length; i++) {
		FlxAnim *a = As3Array_get(&t->_animations, i);
		if (a != NULL && as3_streq(a->name, name))
			r = 1.0 / a->delay;
	}
	return r;
}

/* Troll.as: constructor (maxVelocity 60/275, gravity 900, small 8x20 offset 12/12),
 * loadAnims draws walk/crown/coin fps from FlxG.random once at construction;
 * reset: wait, hidden, stopped, not retreating, no coin (the crown flag is NOT
 * cleared), animations reloaded only when the big/small type changes, stats from
 * PlayState, t = 1; go(): live target = player.x, big trolls leave the collision
 * group; retreat(): goal = nearer world edge away from the player, trolls ->
 * trollsNoCollide; kill() returns the troll to trolls; getShot: crown carriers
 * are immune, health-1, gibs + coin drop + kill at 0
 * (check.py compare_v021_troll_same_type_reuse / edge_kill_reuse). */
static void verify_troll(void)
{
	PlayState *ps = isolated_playstate(100);
	char detail[200];
	Number seedBefore = FlxG.globalSeed;
	Troll *t = Troll_new();
	FlxGroup_add(ps->trolls, (FlxBasic *)t);
	Number walk = anim_fps(t, "walk"), crown = anim_fps(t, "walk_crown"), coin = anim_fps(t, "walk_coin");
	snprintf(detail, sizeof detail, "maxV %g/%g accY %g size %g x %g off %g/%g fps %g %g %g", t->maxVelocity.x, t->maxVelocity.y,
		t->acceleration.y, t->width, t->height, t->offset.x, t->offset.y, walk, crown, coin);
	check(t->maxVelocity.x == 60 && t->maxVelocity.y == 275 && t->acceleration.y == 900 && t->width == 8 && t->height == 20 &&
		t->offset.x == 12 && t->offset.y == 12, "troll: constructor geometry and physics", detail);
	check(walk >= 10 && walk < 15 && crown >= 10 && crown < 15 && coin >= 10 && coin < 15 && FlxG.globalSeed != seedBefore,
		"troll: small walk/crown/coin fps are 10 + random*5 drawn at construction", detail);

	Number seedBeforeReset = FlxG.globalSeed;
	t->hasCoin = true;
	t->hasCrown = true;
	t->retreating = true;
	t->velocity.x = 5;
	ps->trollHealth = 3;
	ps->trollMaxSpeed = 33;
	Troll_reset(t, 400, 90);
	snprintf(detail, sizeof detail, "wait %d visible %d v %g retreating %d coin %d crown %d health %g maxSpeed %g t %g seed same %d",
		t->wait, t->visible, t->velocity.x, t->retreating, t->hasCoin, t->hasCrown, t->health, t->maxSpeed, t->t,
		FlxG.globalSeed == seedBeforeReset);
	check(t->wait && !t->visible && t->velocity.x == 0 && !t->retreating && !t->hasCoin && t->t == 1,
		"troll: reset waits hidden and stopped, clears retreat and coin, t = 1", detail);
	check(t->hasCrown, "troll: reset keeps the crown flag (source quirk)", detail);
	check(t->health == 3 && t->maxSpeed == 33, "troll: reset takes health and speed from PlayState", detail);
	check(FlxG.globalSeed == seedBeforeReset && anim_fps(t, "walk") == walk && anim_fps(t, "walk_crown") == crown,
		"troll: same-type reset keeps the animation table and draws no FlxG.random", detail);

	ps->trollBig = true;
	Troll_reset(t, 400, 90);
	snprintf(detail, sizeof detail, "big %d size %g x %g off %g/%g fps first %g last %g", t->big, t->width, t->height, t->offset.x,
		t->offset.y, anim_fps(t, "walk"), anim_fps_last(t, "walk"));
	check(t->big && t->width == 16 && t->height == 40 && t->offset.x == 24 && t->offset.y == 24 && anim_fps_last(t, "walk") == 7,
		"troll: big type reloads the big set (16x40, walk 7 fps appended)", detail);
	/* source quirk: addAnimation appends and play() takes the first match, so the
	   small walk (random fps) keeps shadowing the big one after a type switch */
	check(anim_fps(t, "walk") == walk, "troll: play() resolves the first 'walk' (original shadowing quirk)", detail);
	ps->trollBig = false;
	Troll_reset(t, 400, 90);
	check(!t->big && t->width == 8 && t->height == 20, "troll: switching back reloads the small set", "small set");

	t->hasCrown = false;
	ps->player->x = 1000;
	t->x = 400;
	Troll_go(t);
	check(!t->wait && t->visible && t->goal == 1000 && in_group(ps->trolls, t), "troll: go() targets the player and stays collidable (small)", "go");
	ps->trollBig = true;
	Troll_reset(t, 400, 90);
	Troll_go(t);
	check(in_group(ps->trollsNoCollide, t) && !in_group(ps->trolls, t), "troll: big trolls leave the collision group on go()", "big go");
	ps->trollBig = false;
	Troll_reset(t, 400, 90);
	Troll_go(t);
	t->x = 400;
	Troll_retreat(t);
	snprintf(detail, sizeof detail, "retreating %d goal %g groups %d/%d", t->retreating, t->goal, in_group(ps->trolls, t),
		in_group(ps->trollsNoCollide, t));
	check(t->retreating && t->goal == 0 && !t->wait && in_group(ps->trollsNoCollide, t) && !in_group(ps->trolls, t),
		"troll: retreat() heads for the world edge behind it and joins trollsNoCollide", detail);
	Troll_kill(t);
	check(!t->alive && in_group(ps->trolls, t) && !in_group(ps->trollsNoCollide, t), "troll: kill() returns the troll to trolls", "kill");

	/* getShot */
	Troll *u = Troll_new();
	FlxGroup_add(ps->trolls, (FlxBasic *)u);
	ps->trollHealth = 2;
	Troll_reset(u, 700, 90);
	Troll_go(u);
	u->hasCrown = true;
	Troll_getShot(u);
	check(u->health == 2 && u->alive, "troll: a crown carrier is immune to arrows", "crown carrier hurt");
	u->hasCrown = false;
	u->hasCoin = true;
	int coinsBefore = living(ps->coins, NULL);
	Troll_getShot(u);
	check(u->health == 1 && u->alive, "troll: first arrow only lowers health", "health");
	Troll_getShot(u);
	snprintf(detail, sizeof detail, "alive %d coins %d->%d", u->alive, coinsBefore, living(ps->coins, NULL));
	check(!u->alive && living(ps->coins, NULL) == coinsBefore + 1, "troll: last arrow kills it and drops its stolen coin", detail);
}

/* Old-port parity cards (docs/PARITY_WORKLIST.md) rechecked on the translation:
 * V026 tutorial queue/read time/centre text/panTo, V009 coin owner windows
 * (called at 2 s, owner released at 4 s, 25 s life), V016 castle build gate
 * (t > 15, stage < 14), V028 camera easing (0.1, cap 20, literal asymmetric
 * Y branch). Expectations come from PlayState.as/Coin.as/Castle.as/CameraTarget.as. */
static void verify_flow(void)
{
	PlayState *ps = isolated_playstate(100);
	char detail[200];
	/* V026 tutorial text queue */
	ps->textTimeout = 0;
	ps->textQueue.length = 0;
	PlayState_showText(ps, "one\ntwo");
	snprintf(detail, sizeof detail, "shown '%s' queued %u timeout %g", FlxText_get_text(ps->text), ps->textQueue.length, ps->textTimeout);
	check(as3_streq(FlxText_get_text(ps->text), "one") && ps->textQueue.length == 1 && ps->textTimeout == 6,
		"tutorial: newline text is split FIFO, first line shown, read time max(6, 0.2*length)", detail);
	ps->textTimeout = 0;
	ps->textQueue.length = 0;
	PlayState_showText(ps, "0123456789012345678901234567890123456789");
	check(ps->textTimeout == 8, "tutorial: a 40 character line is read for 0.2*40 = 8 s", "timeout");
	PlayState_showText(ps, "next");
	snprintf(detail, sizeof detail, "shown '%s' queued %u", FlxText_get_text(ps->text), ps->textQueue.length);
	check(ps->textQueue.length == 1 && !as3_streq(FlxText_get_text(ps->text), "next"), "tutorial: a line waits while the previous timer is positive", detail);
	PlayState_showCenterText(ps, "II");
	check(ps->centerText->visible && FlxSprite_get_alpha((FlxSprite *)ps->centerText) == 0.999, "tutorial: centre text starts at alpha 0.999", "centre");
	PlayState_panTo(ps, (FlxSprite *)ps->player, 7.5, 12);
	check(ps->cameraTimeout == 7.5 && CameraTarget_get_target(ps->cameraTarget) == (FlxSprite *)ps->player && ps->cameraTarget->lead == 12,
		"tutorial: panTo sets timeout, target and lead", "panTo");

	/* V009 coin windows */
	Coin *coin = (Coin *)FlxGroup_recycle(ps->coins, &Coin_class);
	Citizen *owner = add_citizen(ps, 300, 108, Citizen_POOR, 0, Citizen_IDLE);
	Coin_drop(coin, (FlxSprite *)ps->castle, (FlxObject *)owner, false);
	check(coin->lifespan == 25 && coin->owner == (FlxObject *)owner && !coin->called, "coin: drop gives 25 s life, the owner and called = false", "drop");
	coin->lifespan = 23.001;
	Coin_update(coin);
	check(!coin->called, "coin: owner is not called before 2 s have passed (lifespan > 23)", "called early");
	coin->lifespan = 23.0;
	Coin_update(coin);
	check(coin->called && coin->owner == (FlxObject *)owner, "coin: owner is called at 2 s (lifespan <= 23) and keeps ownership", "called");
	coin->lifespan = 21.001;
	Coin_update(coin);
	check(coin->owner == (FlxObject *)owner, "coin: ownership lasts while lifespan > 21", "owner released early");
	coin->lifespan = 21.0;
	Coin_update(coin);
	check(coin->owner == NULL, "coin: owner released at 4 s (lifespan <= 21)", "owner kept");

	/* V016 castle build gate */
	Castle *castle = ps->castle;
	castle->t = 15;
	check(!Castle_canBuild(castle), "castle: canBuild needs t > 15 (t == 15 is not enough)", "t=15");
	castle->t = 15.01;
	check(Castle_canBuild(castle), "castle: canBuild at t = 15.01", "t=15.01");
	Castle_morph(castle, 14);
	check(!Castle_canBuild(castle), "castle: the last stage (14) cannot be built further", "stage 14");
	Castle_morph(castle, 0);
	castle->t = 20;
	Castle_build(castle);
	check(castle->stage == 1 && castle->t == 0, "castle: build() advances one stage and restarts the cooldown", "build");

	/* V028 camera easing */
	CameraTarget *ct = CameraTarget_new();
	Player *pl = (Player *)ps->player;
	FlxSprite_set_facing((FlxSprite *)pl, RIGHT);
	CameraTarget_set_target(ct, (FlxSprite *)pl);
	Number tx = pl->x + pl->width / 2 + ct->offset.x + ct->lead;
	Number ty = pl->y + pl->height / 2 + ct->offset.y;
	ct->x = tx - 5; /* delta*0.1 = 0.5: below the 1.0 threshold, no move */
	ct->y = ty;
	CameraTarget_update(ct);
	check(ct->x == tx - 5, "camera: X moves only when the eased step is >= 1", "x moved");
	ct->x = tx - 15; /* 1.5 */
	CameraTarget_update(ct);
	check(fabs(ct->x - (tx - 15 + 1.5)) < 1e-9, "camera: X eases by 0.1 of the distance", "x ease");
	ct->x = tx - 1000; /* 100 -> capped at 20 */
	CameraTarget_update(ct);
	check(fabs(ct->x - (tx - 1000 + 20)) < 1e-9, "camera: X step is capped at maxSpeed 20", "x cap");
	ct->y = ty - 5; /* 0.5: the source's literal else-if (<= 1.0) still moves Y */
	CameraTarget_update(ct);
	check(fabs(ct->y - (ty - 5 + 0.5)) < 1e-9, "camera: literal asymmetric Y branch moves for steps below 1", "y asymmetry");
	ct->y = ty + 5;
	CameraTarget_update(ct);
	check(fabs(ct->y - (ty + 5 - 0.5)) < 1e-9, "camera: Y eases upwards by 0.1 of the distance", "y negative");
}

/* Functions the game never calls, translated for parity: values follow the
 * ActionScript definitions (FlxU.as, Utils.as). */
static void verify_unused_helpers(void)
{
	check(as3_streq(FlxU_formatTime(125.5, true), "2:05.50"), "FlxU.formatTime(125.5, true) = 2:05.50", FlxU_formatTime(125.5, true));
	check(as3_streq(FlxU_formatTime(59, false), "0:59"), "FlxU.formatTime(59) = 0:59", FlxU_formatTime(59, false));
	/* int(1234567.89*100) is 123456788 in IEEE double (ActionScript Number too): the source truncates to .88 */
	check(as3_streq(FlxU_formatMoney(1234567.89, true, true), "1,234,567.88"), "FlxU.formatMoney(1234567.89) = 1,234,567.88 (source truncation)", FlxU_formatMoney(1234567.89, true, true));
	check(as3_streq(FlxU_formatMoney(1000, false, false), "1.000"), "FlxU.formatMoney(1000, false, false) = 1.000", FlxU_formatMoney(1000, false, false));
	check(as3_streq(FlxU_formatTicks(1000, 3500), "2.5s"), "FlxU.formatTicks = 2.5s", FlxU_formatTicks(1000, 3500));
	check(FlxU_makeColor(255, 128, 0, 1.0) == 0xFFFF8000u, "FlxU.makeColor(255,128,0)", "makeColor");
	check(FlxU_makeColor(1, 2, 3, 0.5) == 0x7F010203u, "FlxU.makeColor alpha 0.5 -> 0x7F", "makeColor alpha");
	Number hsb[4];
	FlxU_getHSB(0xFFFF0000u, hsb);
	check(hsb[0] == 0 && hsb[1] == 1 && hsb[2] == 1 && hsb[3] == 1, "FlxU.getHSB(red) = [0,1,1,1]", "getHSB");
	Number rgba[4];
	FlxU_getRGBA(0x80102030u, rgba);
	check(rgba[0] == 0x10 && rgba[1] == 0x20 && rgba[2] == 0x30 && fabs(rgba[3] - 128.0 / 255) < 1e-12, "FlxU.getRGBA", "getRGBA");
	check(FlxU_makeColorFromHSB(0, 1, 1, 1) == 0xFFFF0000u && FlxU_makeColorFromHSB(120, 1, 1, 1) == 0xFF00FF00u, "FlxU.makeColorFromHSB(red, green)", "HSB");
	UtilsRGB rgb = Utils_getRGB(0x80102030u);
	check(rgb.alpha == 0x80 && rgb.red == 0x10 && rgb.green == 0x20 && rgb.blue == 0x30, "Utils.getRGB", "getRGB");
	UtilsHSV hsv = Utils_RGBtoHSV(0xFFFF0000u);
	check(hsv.hue == 0 && hsv.saturation == 1 && hsv.lightness == 0.5 && hsv.value == 0.5, "Utils.RGBtoHSV(red)", "RGBtoHSV");
	hsv = Utils_RGBtoHSV(0xFF00FF00u);
	check(hsv.hue == 120, "Utils.RGBtoHSV(green) hue 120", "hue");
	check(Utils_interpolateColorAndAlpha(0x00000000u, 0xFFFFFFFFu, 2, 1) == 0x7F7F7F7Fu, "Utils.interpolateColorAndAlpha halfway", "interpolate");
	As3Array list = AS3_ARRAY();
	As3Array_push(&list, AS3_BOX(1));
	As3Array_push(&list, AS3_BOX(2));
	As3Array_push(&list, AS3_BOX(3));
	check(FlxU_getRandom(&list, 0, 0) != NULL && FlxU_getRandom(NULL, 0, 0) == NULL, "FlxU.getRandom returns a member, null for no array", "getRandom");
	FlxU_shuffle(&list, 20);
	int sum = (int)(intptr_t)As3Array_get(&list, 0) + (int)(intptr_t)As3Array_get(&list, 1) + (int)(intptr_t)As3Array_get(&list, 2);
	check(list.length == 3 && sum == 6, "FlxU.shuffle keeps the members", "shuffle");
	BitmapData *bd = BitmapData_new(8, 4, true, 0);
	BitmapData_fillRect(bd, Rectangle_new(2, 1, 3, 2), 0xFFFF0000u);
	As3Array *rects = Utils_findRectanglesWithColor(bd, 0xFFFF0000u);
	Rectangle *r = rects->length ? As3Array_get(rects, 0) : NULL;
	check(rects->length == 1 && r && r->x == 2 && r->y == 1 && r->width == 3 && r->height == 2, "Utils.findRectanglesWithColor finds the 3x2 block", "rects");
}

static bool matrix_is_identity(const ColorMatrix *m, Number tolerance)
{
	static const Number id[20] = {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0};
	for (int i = 0; i < 20; i++)
		if (!(fabs(m->matrix[i] - id[i]) <= tolerance))
			return false;
	return true;
}

/* com.quasimondo.geom.ColorMatrix operations the game never calls */
static void verify_color_matrix(void)
{
	ColorMatrix m;
	ColorMatrix_ctor(&m);
	ColorMatrix_invert(&m);
	ColorMatrix_invert(&m);
	check(matrix_is_identity(&m, 1e-12), "ColorMatrix: invert twice is the identity", "invert");
	ColorMatrix_reset(&m);
	ColorMatrix_adjustSaturation(&m, 0.5);
	ColorMatrix_adjustContrast(&m, 0.25, NAN, NAN);
	ColorMatrix_adjustBrightness(&m, 10, NAN, NAN);
	ColorMatrix copy = m;
	check(ColorMatrix_invertMatrix(&copy), "ColorMatrix: invertMatrix succeeds on a regular matrix", "invertMatrix");
	ColorMatrix_concat(&m, copy.matrix);
	check(matrix_is_identity(&m, 1e-9), "ColorMatrix: a matrix concatenated with its inverse is the identity", "inverse");
	ColorMatrix_reset(&m);
	ColorMatrix_desaturate(&m);
	check(ColorMatrix_applyMatrix(&m, 0xFFFF0000u) == 0xFF363636u, "ColorMatrix: desaturate(red) = luma 0.212671*255 -> 0x36", "desaturate");
	ColorMatrix_reset(&m);
	ColorMatrix_rotateHue(&m, 360);
	check(matrix_is_identity(&m, 1e-6), "ColorMatrix: rotateHue(360) is the identity", "rotateHue");
	ColorMatrix_reset(&m);
	ColorMatrix_normalize(&m);
	check(matrix_is_identity(&m, 1e-12), "ColorMatrix: normalize keeps the identity", "normalize");
	ColorMatrix_reset(&m);
	ColorMatrix_YIQ2RGB(&m);
	check(isnan(m.matrix[0]), "ColorMatrix: YIQ2RGB keeps the source's stray-comma NaN element", "YIQ2RGB");
	ColorMatrix_reset(&m);
	ColorMatrix_setChannels(&m, 4, 2, 1, 8); /* red<-blue, green<-green, blue<-red */
	check(ColorMatrix_applyMatrix(&m, 0xFF102030u) == 0xFF302010u, "ColorMatrix: setChannels swaps red and blue", "setChannels");
	ColorMatrix_reset(&m);
	check(as3_streq(ColorMatrix_toString(&m), "1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0"), "ColorMatrix: toString lists the 20 coefficients", ColorMatrix_toString(&m));
}

/* Flixel members the game never calls (FlxRect/FlxPoint, FlxCamera, FlxText, FlxG, FlxTilemap, FlxU) */
static void verify_flixel_extras(void)
{
	FlxRect r = FlxRect_new(10, 20, 30, 40);
	check(FlxRect_get_left(&r) == 10 && FlxRect_get_right(&r) == 40 && FlxRect_get_top(&r) == 20 && FlxRect_get_bottom(&r) == 60, "FlxRect left/right/top/bottom", "rect");
	Rectangle fr = Rectangle_new(0, 0, 0, 0);
	FlxRect_copyToFlash(&r, &fr);
	check(fr.x == 10 && fr.width == 30, "FlxRect.copyToFlash", "copyToFlash");
	check(as3_streq(FlxG_getLibraryName(), "flixel v2.55"), "FlxG.getLibraryName = flixel v2.55", FlxG_getLibraryName());
	check(as3_streq(FlxU_getClassName(FlxG.camera, false), "FlxCamera") && as3_streq(FlxBasic_toString((FlxBasic *)FlxG.camera), "FlxCamera") &&
		FlxU_compareClassNames(FlxG.camera, FlxG.camera), "FlxU.getClassName / FlxBasic.toString name the class", "className");
	check(FlxU_getClass("Troll") == &Troll_class && FlxU_getClass("NoSuchClass") == NULL, "FlxU.getClass looks classes up by name", "getClass");
	check(fabs(FlxG_get_framerate() - 62.5) < 1e-9, "FlxG.framerate follows the integer step (1000/16)", "framerate");
	FlxBasic *plugin = (FlxBasic *)FlxSprite_new(0, 0, NULL);
	FlxG_addPlugin(plugin);
	FlxG_addPlugin((FlxBasic *)FlxSprite_new(1, 1, NULL)); /* same class name: not added twice */
	check(FlxG.plugins.length == 1 && FlxG_getPlugin(&FlxSprite_class) == plugin, "FlxG.addPlugin adds a class once; getPlugin finds it", "plugins");
	check(FlxG_removePluginType(&FlxSprite_class) && FlxG.plugins.length == 0, "FlxG.removePluginType removes it", "removePluginType");
	As3Array shuffled = AS3_ARRAY();
	for (int i = 1; i <= 4; i++)
		As3Array_push(&shuffled, AS3_BOX(i));
	FlxG_shuffle(&shuffled, 10);
	int sum = 0;
	for (uint32_t i = 0; i < shuffled.length; i++)
		sum += (int)(intptr_t)As3Array_get(&shuffled, i);
	check(sum == 10 && shuffled.length == 4, "FlxG.shuffle keeps the members", "shuffle");
	/* FlxCamera properties go through the display stub */
	FlxCamera *cam = FlxG.camera;
	FlxCamera_set_alpha(cam, 0.5);
	FlxCamera_set_angle(cam, 30);
	FlxCamera_set_antialiasing(cam, true);
	FlxCamera_set_color(cam, 0xFF8040);
	check(FlxCamera_get_alpha(cam) == 0.5 && FlxCamera_get_angle(cam) == 30 && FlxCamera_get_antialiasing(cam) && FlxCamera_get_color(cam) == 0xFF8040,
		"FlxCamera alpha/angle/antialiasing/color", "camera props");
	FlxCamera_set_alpha(cam, 1);
	FlxCamera_set_angle(cam, 0);
	FlxCamera_set_color(cam, 0xFFFFFF);
	/* FlxText properties */
	FlxText *t = FlxText_new(0, 0, 100, "x", true);
	FlxText_set_color(t, 0x123456);
	FlxText_set_shadow(t, 0x654321);
	check(FlxText_get_color(t) == 0x123456 && FlxText_get_shadow(t) == 0x654321, "FlxText color and shadow", "text props");
	/* FlxTilemap data access on the real level floor */
	PlayState *ps = isolated_playstate(100);
	FlxTilemap *floor = ps->floor;
	uint32_t tile = FlxTilemap_getTile(floor, 5, 4);
	As3Array *where = FlxTilemap_getTileInstances(floor, tile);
	check(where != NULL && where->length > 0, "FlxTilemap.getTileInstances finds a used tile", "instances");
	check(FlxTilemap_getTileByIndex(floor, 4 * floor->widthInTiles + 5) == tile, "FlxTilemap.getTileByIndex = getTile(x, y)", "getTileByIndex");
	uint32_t keep = FlxTilemap_getTile(floor, 2, 4);
	check(FlxTilemap_setTile(floor, 2, 4, 3, false) && FlxTilemap_getTile(floor, 2, 4) == 3, "FlxTilemap.setTile changes the data", "setTile");
	FlxTilemap_setTile(floor, 2, 4, keep, false);
	check(!FlxTilemap_setTile(floor, floor->widthInTiles, 0, 1, false), "FlxTilemap.setTile rejects out-of-range X", "setTile range");
	FlxRect b = FlxRect_new(0, 0, 0, 0);
	FlxTilemap_getBounds(floor, &b);
	check(b.x == floor->x && b.width == floor->width, "FlxTilemap.getBounds", "bounds");
	const int32_t grid[6] = {1, 0, 1, 0, 1, 1};
	check(as3_streq(FlxTilemap_arrayToCSV(grid, 6, 3, false), "1, 0, 1\n0, 1, 1"), "FlxTilemap.arrayToCSV", FlxTilemap_arrayToCSV(grid, 6, 3, false));
	check(as3_streq(FlxTilemap_arrayToCSV(grid, 6, 3, true), "0, 1, 0\n1, 0, 0"), "FlxTilemap.arrayToCSV inverts 0 and 1", FlxTilemap_arrayToCSV(grid, 6, 3, true));
}

/* NPC pipeline on the real game objects and input path (original rules):
 * a coin thrown with DOWN near a beggar makes it a Poor citizen (recruitedCitizen),
 * a Poor citizen walks to a shop with stock and becomes Hunter (bows) or Farmer
 * (scythes), a Hunter shoots a bunny in range, a Farmer works a mature field. */
static bool run_until(PlayState *ps, int maxSteps, bool (*done)(PlayState *))
{
	for (int i = 0; i < maxSteps; i++) {
		if (done(ps))
			return true;
		step();
	}
	return done(ps);
}

static Citizen *npc_subject;
static bool npc_is_poor(PlayState *ps) { (void)ps; return npc_subject->occupation == Citizen_POOR; }
static bool npc_has_job(PlayState *ps)
{
	(void)ps;
	return npc_subject->occupation == Citizen_FARMER || npc_subject->occupation == Citizen_HUNTER;
}
static Bunny *npc_bunny;
static bool npc_bunny_dead(PlayState *ps) { (void)ps; return !npc_bunny->alive; }

static void verify_npc_pipeline(bool bowsOnly)
{
	char detail[200];
	PlayState *ps = isolated_playstate(1900);
	/* the pipeline depends on where the random-walking beggar goes: start from fixed generators
	   so the result does not depend on how many random numbers the earlier checks consumed */
	FlxG.globalSeed = bowsOnly ? 0.25 : 0.5;
	Math_random_seed(bowsOnly ? 2u : 1u);
	ps->untouchable = true;
	ps->cheatNoTrolls = true;
	Player *pl = (Player *)ps->player;
	pl->coins = 7;
	Citizen *beggar = add_citizen(ps, 1930, 108, Citizen_BEGGAR, 0, Citizen_IDLE);
	beggar->maxVelocity.x = 20; /* add_citizen zeroes it for the overlap tests */
	npc_subject = beggar;
	step();
	FlxKeyboard_setRaw("DOWN", true);
	step();
	FlxKeyboard_setRaw("DOWN", false);
	check(pl->coins == 6, "npc: DOWN near a beggar throws one coin", "coins");
	bool poor = run_until(ps, 3000, npc_is_poor);
	snprintf(detail, sizeof detail, "occupation %d coins %d recruited %d", beggar->occupation, pl->coins, ps->recruitedCitizen);
	check(poor && ps->recruitedCitizen, "npc: the beggar picks the coin up and becomes a Poor citizen (recruitedCitizen)", detail);
	check(in_group(ps->characters, beggar) && !in_group(ps->beggars, beggar), "npc: it moves from beggars to characters", "groups");
	/* both shops hold stock: the Poor citizen's goal is a random shop */
	for (uint32_t i = 0; i < ps->shops->members.length; i++) {
		Shop *sh = As3Array_get(&ps->shops->members, i);
		if (sh && (!bowsOnly || sh->type == Shop_BOWS))
			Shop_setSupply(sh, 2);
	}
	bool job = run_until(ps, 12000, npc_has_job);
	snprintf(detail, sizeof detail, "occupation %d (2 farmer, 3 hunter)", beggar->occupation);
	check(job && (!bowsOnly || beggar->occupation == Citizen_HUNTER), bowsOnly ? "npc: with only bows in stock the Poor citizen becomes a Hunter" : "npc: a Poor citizen walks to a stocked shop and gets a job", detail);
	if (!job)
		return;
	/* the job: a hunter shoots a bunny close to it; a farmer works a mature field */
	if (beggar->occupation == Citizen_HUNTER) {
		Bunny *b = Bunny_new((int32_t)beggar->x + 60, 0);
		FlxGroup_add(ps->bunnies, (FlxBasic *)b);
		b->x = beggar->x + 60;
		npc_bunny = b;
		bool killed = run_until(ps, 3000, npc_bunny_dead);
		check(killed, "npc: a Hunter shoots a bunny in range", "bunny survived 3000 steps");
	} else {
		clear_group(ps->farmlands);
		Farmland *f = Farmland_new((int32_t)beggar->x - 16, 100);
		FlxSprite_set_frame((FlxSprite *)f, 7);
		f->t = 9;
		FlxGroup_add(ps->farmlands, (FlxBasic *)f);
		int coinsBefore = living(ps->coins, NULL);
		bool harvested = false;
		for (int i = 0; i < 3000 && !harvested; i++) {
			step();
			harvested = living(ps->coins, NULL) > coinsBefore;
		}
		check(harvested, "npc: a Farmer works a mature field and it pays coins", "no harvest in 3000 steps");
	}
}

/* Idle wandering (Citizen.update IDLE branch + pickNewGoal): citizens roam the
   kingdom, which is the castle area (200 px) until walls are built
   (Wall.update widens kingdomLeft/Right) and then the whole built area. */
static void wander_run(PlayState *ps, Citizen *c, unsigned frames, Number *lo, Number *hi)
{
	*lo = *hi = c->x;
	for (unsigned i = 0; i < frames; i++) {
		step();
		if (c->x < *lo)
			*lo = c->x;
		if (c->x > *hi)
			*hi = c->x;
	}
	(void)ps;
}

static void verify_wander(void)
{
	char detail[240];
	PlayState *ps = fresh_playstate();
	ps->minBeggars = 0;
	ps->timeToNextPhase = 1e9;
	Citizen *ref = Citizen_new(0, 0);
	Citizen *farmer = add_citizen(ps, 1920, 108, Citizen_FARMER, 0, Citizen_IDLE);
	farmer->maxVelocity.x = ref->maxVelocity.x; /* add_citizen freezes citizens for the other checks */
	Number lo, hi;
	wander_run(ps, farmer, 3600, &lo, &hi); /* the kingdom shrinks 4 px/s for 25 s; goals picked earlier may lie outside */
	wander_run(ps, farmer, 3600, &lo, &hi);
	snprintf(detail, sizeof detail, "x range [%g, %g], kingdom [%g, %g]", lo, hi, ps->kingdomLeft, ps->kingdomRight);
	check(ps->kingdomLeft == 1820 && ps->kingdomRight == 2020 && lo >= 1820 - 4 && hi <= 2020 + 4,
		"wander: with no built walls the kingdom shrinks to 200 px and a farmer roams only inside it", detail);
	check(hi - lo > 100, "wander: the farmer really moves (picks new goals) inside that area", detail);

	for (int i = 0; i < ps->walls->length; i++) {
		Wall *w = As3Array_get(&ps->walls->members, (uint32_t)i);
		if (w) {
			w->stage = 1;
			w->building = false;
		}
	}
	wander_run(ps, farmer, 3600, &lo, &hi);
	snprintf(detail, sizeof detail, "x range [%g, %g], kingdom [%g, %g]", lo, hi, ps->kingdomLeft, ps->kingdomRight);
	check(ps->kingdomLeft <= 1020 && ps->kingdomRight >= 2688,
		"wander: built walls widen the kingdom to the outermost wall on each side (Wall.update)", detail);
	check(hi - lo > 300, "wander: with the wider kingdom the farmer roams well beyond the castle area", detail);
}

/* The background reeds (yellow grass): Reed.update can pick frame 3 of the 3-frame reed.png.
   The CPU path copies nothing then (framePixels keeps the previous frame); the GPU display
   list must not sample outside the sheet either (it drew a brown square from the texture
   page's neighbour). */
static void verify_reed_frames(void)
{
	char detail[240];
	PlayState *ps = isolated_playstate(1000);
	Reed *r = Reed_new((int32_t)FlxG.camera->scroll.x + 50, 100);
	FlxGroup_add((FlxGroup *)ps, (FlxBasic *)r);
	FlxSprite *s = (FlxSprite *)r;
	int bad = -1;
	float last_sx = -1, last_sy = -1, f3_sx = -2, f3_sy = -2;
	for (int frame = 0; frame <= 3; frame++) {
		FlxSprite_set_frame(s, (uint32_t)frame);
		Gpu_recording = true;
		Gpu_beginFrame(FlxG.camera->buffer);
		CALL0(r, draw);
		Gpu_recording = false;
		int found = 0;
		for (int i = 0; i < Gpu_list.count; i++) {
			const GpuCmd *c = &Gpu_list.cmds[i];
			if (c->op != GPUCMD_IMAGE || c->src != s->_pixels)
				continue;
			found++;
			if (c->sx < 0 || c->sy < 0 || c->sx + c->sw > s->_pixels->width || c->sy + c->sh > s->_pixels->height)
				bad = frame;
			if (frame == 2)
				last_sx = c->sx, last_sy = c->sy;
			if (frame == 3)
				f3_sx = c->sx, f3_sy = c->sy;
		}
		if (found == 0 && frame < 3)
			bad = frame;
	}
	snprintf(detail, sizeof detail, "first bad frame %d; sheet %dx%d", bad, s->_pixels->width, s->_pixels->height);
	check(s->_pixels->width == 96 && s->_pixels->height == 32 && bad < 0,
		"reed: every recorded reed draw reads inside reed.png (frames 0-3)", detail);
	snprintf(detail, sizeof detail, "frame 2 at (%g,%g), frame 3 at (%g,%g)", last_sx, last_sy, f3_sx, f3_sy);
	check(f3_sx == last_sx && f3_sy == last_sy,
		"reed: frame 3 is outside the sheet, so (as copyPixels does nothing) the previous frame stays", detail);
}

/* Citizen.checkWork walks group.members without looking at exists/alive (as in the
   source), so killed walls/fields would still be worked: really empty the group. */
static void empty_group(FlxGroup *g)
{
	g->members.length = 0;
	g->length = 0;
}

/* Full construction lifecycle (Codex review CODEX_NPC_CONSTRUCTION_BUG.md): a Farmer must
   walk to a wall under construction and finish it even when the wall's hurt
   timer is already old (Wall.needsWork adaptation). */
static void construction_case(const char *label, Number wallTimer, Number farmerX)
{
	char detail[240];
	PlayState *ps = isolated_playstate(1000);
	empty_group(ps->walls);
	empty_group(ps->farmlands); /* the Farmer could otherwise shovel a field on the way */
	Wall *w = Wall_new(1800, 68);
	FlxGroup_add(ps->walls, (FlxBasic *)w);
	Wall_buildTo(w, 1, false);
	w->t = wallTimer;
	Citizen *farmer = add_citizen(ps, farmerX, 108, Citizen_FARMER, 0, Citizen_IDLE);
	farmer->maxVelocity.x = 22.5; /* Citizen.morph: 21 + random * 3 (add_citizen freezes it) */
	int work_actions = 0;
	for (unsigned i = 0; i < 1600 && w->building; i++) {
		int before = w->heightToBuild;
		step();
		if (w->heightToBuild < before)
			work_actions++; /* one Wall.work by the Farmer */
	}
	snprintf(detail, sizeof detail, "building %d remaining %d work actions %d", w->building, w->heightToBuild, work_actions);
	char name[200];
	snprintf(name, sizeof name, "construction: a Farmer completes a level-1 wall (%s) within 26 s", label);
	check(!w->building && w->heightToBuild == 0 && work_actions >= 4, name, detail);
}

static void verify_construction(void)
{
	construction_case("hurt timer 5 s old, worker 50 px away", 5.0, 1750);
	construction_case("hurt timer 0, worker already inside", 0.0, 1805);
	/* repair window of a completed wall is unchanged (source one-second window) */
	PlayState *ps = isolated_playstate(1000);
	empty_group(ps->walls);
	Wall *w = Wall_new(1800, 68);
	FlxGroup_add(ps->walls, (FlxBasic *)w);
	Wall_buildTo(w, 1, true);
	check(!w->building && !Wall_needsWork(w), "construction: a finished, undamaged wall needs no work", "complete");
	w->health = Wall_HEALTH[w->stage] - 5;
	w->t = 0.5;
	check(!Wall_needsWork(w), "construction: a damaged finished wall is not worked during the 1 s after a hit (shipped SWF: t > HURT_COOLDOWN)", "t 0.5");
	w->t = 1.5;
	check(Wall_needsWork(w), "construction: ...and becomes workable once the hurt timer is older than 1 s", "t 1.5");
	Wall_buildTo(w, 2, false);
	w->t = 50;
	check(Wall_needsWork(w), "construction: an upgrade in progress needs work once the hurt timer is old", "upgrade");
	w->t = 0.5;
	check(!Wall_needsWork(w), "construction: an upgrade is not worked within 1 s of the hurt timer reset (shipped SWF rule)", "upgrade fresh");
}

static Number sound_clamp01(Number v)
{
	return v < 0 ? 0 : (v > 1 ? 1 : v);
}

static bool sound_file_exists(const char *embedName)
{
	char path[512];
	snprintf(path, sizeof path, "%s/%s.pcm", Embed_root, embedName[0] == '/' ? embedName + 1 : embedName);
	FILE *f = fopen(path, "rb");
	if (f)
		fclose(f);
	return f != NULL;
}

/* Sound logic of PlayState: phase table (music, durations, weather tweens),
   nextPhase music switching, ambient loops and their time-of-day volumes,
   proximity sounds, and that every referenced file exists. The expected
   phase table is generated from the original PlayState.as. */
static void verify_sound(void)
{
	char detail[300];
	const SoundExpectedRow phases[] = {SOUND_EXPECTED_PHASES};
	const SoundExpectedRow cycle[] = {SOUND_EXPECTED_CYCLE};
	const unsigned nPhases = sizeof phases / sizeof phases[0];
	const unsigned nCycle = sizeof cycle / sizeof cycle[0];

	/* construction: three looped ambient sounds start playing (volume then follows the weather) */
	HostMedia_resetPlays();
	PlayState *ps = fresh_playstate();
	snprintf(detail, sizeof detail, "cicada %d owls %d birds %d", HostMedia_plays("sound/cicada"), HostMedia_plays("sound/owls"),
		HostMedia_plays("sound/birds"));
	check(HostMedia_plays("sound/cicada") == 1 && HostMedia_plays("sound/owls") == 1 && HostMedia_plays("sound/birds") == 1,
		"sound: PlayState starts cicada, owls and birds once each", detail);
	check(ps->cicada && ps->owls && ps->birds && ps->cicada->_looped && ps->owls->_looped && ps->birds->_looped,
		"sound: the three ambient sounds exist and loop", "ambient");
	check(HostMedia_plays("assets/music") == 0 && ps->music == NULL, "sound: no music plays during the first (foggy intro) phase", "music");

	/* phase table versus the original source */
	snprintf(detail, sizeof detail, "C has %d phases, source %u", ps->PHASES_length, nPhases);
	check((unsigned)ps->PHASES_length == nPhases, "sound: PHASES has as many rows as the source", detail);
	int bad = -1;
	for (unsigned i = 0; i < nPhases && i < (unsigned)ps->PHASES_length && bad < 0; i++) {
		const Phase *c = &ps->PHASES[i];
		const SoundExpectedRow *e = &phases[i];
		bool music_same = (c->music == NULL && e->music == NULL) || (c->music && e->music && strcmp(c->music, e->music) == 0);
		if (c->weather != e->weather || c->duration != e->duration || c->hasTweenTime != e->hasTween || (e->hasTween && c->tweenTime != e->tween) ||
			(c->callback != NULL) != e->hasCallback || !music_same)
			bad = (int)i;
	}
	snprintf(detail, sizeof detail, "first differing row %d", bad);
	check(bad < 0, "sound: every PHASES row (weather, duration, tween, callback, music) equals the source", detail);
	bad = -1;
	for (unsigned i = 0; i < nCycle && bad < 0; i++) {
		const Phase *c = &ps->PHASES_CYCLE[i];
		const SoundExpectedRow *e = &cycle[i];
		bool music_same = (c->music == NULL && e->music == NULL) || (c->music && e->music && strcmp(c->music, e->music) == 0);
		if (c->weather != e->weather || c->duration != e->duration || c->hasTweenTime != e->hasTween || (e->hasTween && c->tweenTime != e->tween) ||
			(c->callback != NULL) != e->hasCallback || !music_same)
			bad = (int)i;
	}
	snprintf(detail, sizeof detail, "first differing cycle row %d", bad);
	check(nCycle == 5 && bad < 0, "sound: PHASES_CYCLE (endless nights) equals the source", detail);

	/* every sound the game can play exists as converted audio */
	const char *needed[] = {"/assets/sound/cicada.mp3", "/assets/sound/owls.mp3", "/assets/sound/birds.mp3", "/assets/sound/hit.mp3",
		"/assets/sound/hitbig.mp3"};
	int missing = 0;
	const char *missing_name = "";
	for (unsigned i = 0; i < sizeof needed / sizeof needed[0]; i++)
		if (!sound_file_exists(needed[i])) {
			missing++;
			missing_name = needed[i];
		}
	for (unsigned i = 0; i < nPhases; i++)
		if (phases[i].music && !sound_file_exists(phases[i].music)) {
			missing++;
			missing_name = phases[i].music;
		}
	for (unsigned i = 0; i < nCycle; i++)
		if (cycle[i].music && !sound_file_exists(cycle[i].music)) {
			missing++;
			missing_name = cycle[i].music;
		}
	snprintf(detail, sizeof detail, "%d missing, e.g. %s", missing, missing_name);
	check(missing == 0, "sound: every music and ambient file the phases reference exists in translit_assets", detail);

	/* nextPhase: walk the whole table and into the endless cycle; callbacks are removed
	   (they spawn trolls / save progress; their presence was compared above) */
	for (int i = 0; i < ps->PHASES_length; i++)
		ps->PHASES[i].callback = NULL;
	for (int i = 0; i < 5; i++)
		ps->PHASES_CYCLE[i].callback = NULL;
	ps->phasesPaused = false;
	ps->phase = 0;
	ps->music = NULL;
	int wrong_phase = -1, wrong_time = -1, wrong_tween = -1, wrong_music = -1, wrong_stop = -1, music_rows = 0, music_starts = 0;
	char first_bad[200] = "";
	HostMedia_resetPlays();
	for (unsigned i = 0; i < nPhases + 2 * nCycle; i++) {
		const SoundExpectedRow *e = i < nPhases ? &phases[i] : &cycle[(i - nPhases) % nCycle];
		FlxSound *before = ps->music;
		int stops_before = HostMedia_stops();
		int plays_before = HostMedia_plays("assets/music");
		int same_before = e->music ? HostMedia_plays(e->music + 1) : 0;
		PlayState_nextPhase(ps);
		if (ps->phase != (int)i + 1 && wrong_phase < 0)
			wrong_phase = (int)i;
		if (ps->timeToNextPhase != e->duration && wrong_time < 0)
			wrong_time = (int)i;
		if (e->weather) {
			Number want = e->hasTween ? e->tween : e->duration * 0.7;
			if ((ps->weather->targetState != e->weather || ps->weather->tweenDuration != want) && wrong_tween < 0)
				wrong_tween = (int)i;
		}
		int started = HostMedia_plays("assets/music") - plays_before;
		if (e->music) {
			music_rows++;
			music_starts += started;
			if ((started != 1 || HostMedia_plays(e->music + 1) - same_before != 1 || ps->music == NULL || ps->music->_looped) && wrong_music < 0) {
				wrong_music = (int)i;
				snprintf(first_bad, sizeof first_bad, "phase %u wants %s: started %d", i, e->music, started);
			}
			/* the previous music is stopped before the new one starts */
			if (before != NULL && HostMedia_stops() - stops_before < 1 && wrong_stop < 0)
				wrong_stop = (int)i;
		} else if ((started != 0 || ps->music != before) && wrong_music < 0) {
			wrong_music = (int)i;
			snprintf(first_bad, sizeof first_bad, "phase %u has no music but %d started / music changed", i, started);
		}
	}
	snprintf(detail, sizeof detail, "first bad phase %d", wrong_phase);
	check(wrong_phase < 0, "sound: nextPhase advances the phase counter through the table and the cycle", detail);
	snprintf(detail, sizeof detail, "first bad phase %d", wrong_time);
	check(wrong_time < 0, "sound: nextPhase sets timeToNextPhase to the row duration", detail);
	snprintf(detail, sizeof detail, "first bad phase %d", wrong_tween);
	check(wrong_tween < 0, "sound: nextPhase tweens to the row weather over its tween time (default 0.7 x duration)", detail);
	snprintf(detail, sizeof detail, "%s", first_bad);
	check(wrong_music < 0, "sound: a row with music starts exactly that file once; a row without music changes nothing", detail);
	snprintf(detail, sizeof detail, "first bad phase %d", wrong_stop);
	check(wrong_stop < 0, "sound: the running music is stopped before the next track starts", detail);
	snprintf(detail, sizeof detail, "%d rows with music, %d starts", music_rows, music_starts);
	check(music_rows > 0 && music_rows == music_starts, "sound: one music start per music row over the whole run", detail);

	/* phasesPaused: nextPhase does nothing */
	ps->phasesPaused = true;
	int phase_before = ps->phase;
	Number time_before = ps->timeToNextPhase;
	HostMedia_resetPlays();
	PlayState_nextPhase(ps);
	check(ps->phase == phase_before && ps->timeToNextPhase == time_before && HostMedia_plays("assets/music") == 0,
		"sound: nextPhase returns early while phases are paused (no new phase, no music)", "paused");
	ps->phasesPaused = false;

	/* ambient volumes follow the time of day (formulas transcribed from updateEnvironmentSounds) */
	const Number grid[] = {0.0, 0.1, 0.2, 0.3, 0.4, 0.45, 0.5, 0.55, 0.6, 0.65, 0.7, 0.75, 0.8, 0.9, 0.95, 1.0};
	int wrong_env = -1;
	char env_bad[260] = "";
	for (unsigned i = 0; i < sizeof grid / sizeof grid[0]; i++) {
		Number t = grid[i];
		ps->weather->timeOfDay = t;
		PlayState_updateEnvironmentSounds(ps);
		Number dc = fabs(t - 0.7) / 0.1;
		Number ec = sound_clamp01(1 - dc * dc);
		Number lo = t < fabs(t - 1.0) ? t : fabs(t - 1.0);
		Number eo = sound_clamp01(1 - (lo / 0.2) * (lo / 0.2));
		Number db = fabs(t - 0.4) / 0.25;
		Number eb = sound_clamp01(1 - db * db);
		if ((fabs(ps->cicada->_volume - ec) > 1e-12 || fabs(ps->owls->_volume - eo) > 1e-12 || fabs(ps->birds->_volume - eb) > 1e-12) && wrong_env < 0) {
			wrong_env = (int)i;
			snprintf(env_bad, sizeof env_bad, "t=%g cicada %g/%g owls %g/%g birds %g/%g", t, ps->cicada->_volume, ec, ps->owls->_volume, eo,
				ps->birds->_volume, eb);
		}
	}
	check(wrong_env < 0, "sound: cicada/owls/birds volumes equal the source equations on a 16-point time-of-day grid (clamped to 0..1)", env_bad);
	ps->weather->timeOfDay = 0.7;
	PlayState_updateEnvironmentSounds(ps);
	check(ps->cicada->_volume == 1 && ps->owls->_volume == 0 && ps->birds->_volume == 0, "sound: at dusk (0.7) only the cicadas are loud", "0.7");
	ps->weather->timeOfDay = 0.4;
	PlayState_updateEnvironmentSounds(ps);
	check(ps->birds->_volume == 1 && ps->cicada->_volume == 0 && ps->owls->_volume == 0, "sound: in the morning (0.4) only the birds are loud", "0.4");
	ps->weather->timeOfDay = 1.0;
	PlayState_updateEnvironmentSounds(ps);
	check(ps->owls->_volume == 1, "sound: at midnight (1.0) the owls are at full volume", "1.0");
	ps->weather->timeOfDay = 0.0;
	PlayState_updateEnvironmentSounds(ps);
	check(ps->owls->_volume == 1, "sound: at midnight (0.0, circular day) the owls are at full volume", "0.0");

	/* proximity (arrow hits): volume falls linearly with distance, pan follows x */
	FlxObject *listener = (FlxObject *)ps->player;
	listener->x = 1000;
	listener->y = 100;
	FlxSound *hit = FlxG_play("/assets/sound/hit.mp3", 1.0, false, true);
	FlxSound_proximity(hit, 1000 - FlxG.width / 2, 100, listener, FlxG.width, true);
	FlxSound_update(hit);
	snprintf(detail, sizeof detail, "adjust %g pan %g", hit->_volumeAdjust, hit->_transform->pan);
	check(fabs(hit->_volumeAdjust - 0.5) < 1e-9 && fabs(hit->_transform->pan + 0.5) < 1e-9,
		"sound: a hit half a screen away plays at half volume, panned left", detail);
	FlxSound_proximity(hit, 1000 + 2 * FlxG.width, 100, listener, FlxG.width, true);
	FlxSound_update(hit);
	snprintf(detail, sizeof detail, "adjust %g pan %g", hit->_volumeAdjust, hit->_transform->pan);
	check(hit->_volumeAdjust == 0 && hit->_transform->pan == 1, "sound: beyond the radius the hit is silent, pan clamps to +1", detail);
}

int main(int argc, char **argv)
{
	if (argc > 1)
	{
		Embed_root = argv[1];
		static char fonts[512];
		snprintf(fonts, sizeof fonts, "%s/fonts", argv[1]);
		TextField_fontRoot = fonts;
	}
	/* --one-callback-per-pair: variant run for oracles that model one
	   FlxG.overlap callback per pair (Flixel 2.55: one per shared leaf) */
	if (argc > 2 && strcmp(argv[2], "--one-callback-per-pair") == 0)
		FlxQuadTree_testOneCallbackPerPair = true;
	FlxG_initialSeed = 0.5;
	game = king_new();
	FlxGame_onEnterFrame(game, 16); /* FlxGame creates its initial MenuState */
	write_player_motion_trace();
	verify_player_food_rest();
	write_v011_troll_damage_matrix();
	write_v011_farmer_then_poor();
	write_v011_farmer_poor_player();
	write_v014_tax();
	checks = fopen("translit_checks.txt", "w");
	assert(checks);
	verify_work_dispatch();
	verify_guard();
	verify_shop();
	verify_bunny();
	verify_troll();
	verify_flow();
	verify_unused_helpers();
	verify_color_matrix();
	verify_flixel_extras();
	verify_npc_pipeline(false);
	verify_npc_pipeline(true); /* only the bow shop has stock: the Poor citizen must become a Hunter */
	verify_sound();
	verify_wander();
	verify_construction();
	verify_reed_frames();
	fclose(checks);
	puts("translit scenario traces written");
	return 0;
}
