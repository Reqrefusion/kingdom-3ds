/* Long-run NPC behaviour probe (not a pass/fail test): restores a saved game, leaves the
 * player idle and prints, every 1800 frames, what the kingdom and its citizens are doing.
 *   python tools/build_host.py --main tests/npc_probe.c --out build_host/npc_probe.exe
 *   build_host/npc_probe.exe translit_assets "D4 A17 X1932 B2 P1 F3 H6 W220021 C1 S02 G7" 36000
 * Used to look for NPCs that stand still / never work (see CLAUDE.md, 9th round).
 */
#include "../game/game.h"
#include <assert.h>

extern Number FlxG_initialSeed;

static const char *occ_name(int o)
{
	return o == Citizen_BEGGAR ? "beggar" : o == Citizen_POOR ? "poor" : o == Citizen_FARMER ? "farmer" : "hunter";
}

int main(int argc, char **argv)
{
	if (argc < 4)
		return 2;
	Embed_root = argv[1];
	static char fonts[512];
	snprintf(fonts, sizeof fonts, "%s/fonts", argv[1]);
	TextField_fontRoot = fonts;
	FlxG_initialSeed = 0.5;
	FlxGame *game = king_new();
	FlxGame_onEnterFrame(game, 16);
	FlxG_switchState((FlxState *)PlayState_new(argv[2]));
	FlxGame_onEnterFrame(game, 16);
	PlayState *ps = (PlayState *)FlxG.state;
	unsigned frames = (unsigned)atoi(argv[3]);
	/* per citizen: frames spent walking vs standing, to spot NPCs that never move */
	enum { MAXC = 64 };
	static unsigned walked[MAXC], total[MAXC];
	static Number lastx[MAXC], lo[MAXC], hi[MAXC];
	Citizen *track[MAXC] = {0};
	for (unsigned f = 0; f <= frames; f++) {
		if (f % 1800 == 0) {
			int occ[4] = {0}, shovel = 0, shoot = 0;
			for (int i = 0; i < ps->characters->length; i++) {
				Citizen *c = As3Array_get(&ps->characters->members, (uint32_t)i);
				if (c && c->alive) {
					occ[c->occupation]++;
					shovel += c->action == Citizen_SHOVEL;
					shoot += c->action == Citizen_SHOOT;
				}
			}
			int archers = 0;
			for (int i = 0; i < ps->archers->length; i++) {
				Citizen *c = As3Array_get(&ps->archers->members, (uint32_t)i);
				archers += c && c->alive;
			}
			int farm_ready = 0, harvest = 0;
			for (int i = 0; i < ps->farmlands->length; i++) {
				Farmland *fl = As3Array_get(&ps->farmlands->members, (uint32_t)i);
				if (fl) {
					farm_ready += Farmland_needsWork(fl);
					harvest += FlxSprite_get_frame((FlxSprite *)fl) == 7;
				}
			}
			int wall_work = 0, wall_building = 0;
			char walls[200] = "";
			for (int i = 0; i < ps->walls->length; i++) {
				Wall *w = As3Array_get(&ps->walls->members, (uint32_t)i);
				if (w) {
					wall_work += Wall_needsWork(w);
					wall_building += w->building;
					char one[24];
					snprintf(one, sizeof one, "%d%s ", w->stage, w->building ? "b" : "");
					strncat(walls, one, sizeof walls - strlen(walls) - 1);
				}
			}
			printf("f%-6u day %d phase %d t %.0f | kingdom [%.0f,%.0f] | beggar %d poor %d farmer %d hunter %d archers %d | farmers shoveling %d "
			       "hunters shooting %d | farms ready %d ripe %d | walls[%s] needWork %d building %d | trolls %d coins %d\n",
				f, ps->day.value, ps->phase, ps->timeToNextPhase, ps->kingdomLeft, ps->kingdomRight,
				FlxGroup_countLiving(ps->beggars), occ[1], occ[2], occ[3], archers, shovel, shoot, farm_ready, harvest, walls, wall_work,
				wall_building, FlxGroup_countLiving(ps->trolls), FlxGroup_countLiving(ps->coins));
		}
		if (f % 3600 == 0 && f > 0) {
			for (int i = 0; i < MAXC && i < ps->characters->length; i++) {
				Citizen *c = As3Array_get(&ps->characters->members, (uint32_t)i);
				if (c && c->alive && total[i])
					printf("   %s #%d walked %u%% of %u frames, x range [%.0f, %.0f]\n", occ_name(c->occupation), i, walked[i] * 100 / total[i],
						total[i], lo[i], hi[i]);
				total[i] = walked[i] = 0;
				lo[i] = 1e9, hi[i] = -1e9;
			}
		}
		FlxGame_onEnterFrame(game, 16);
		ps = (PlayState *)FlxG.state;
		/* cumulative counters: field work steps, harvests, hunter shots (SHOOT entries) */
		static int fl_last[32];
		static unsigned fl_work, fl_harvest, shots, prev_shoot[MAXC];
		for (int i = 0; i < ps->farmlands->length && i < 32; i++) {
			Farmland *fl = As3Array_get(&ps->farmlands->members, (uint32_t)i);
			if (!fl)
				continue;
			int fr = (int)FlxSprite_get_frame((FlxSprite *)fl);
			if (fr != fl_last[i]) {
				fl_work++;
				fl_harvest += fl_last[i] == 7;
			}
			fl_last[i] = fr;
		}
		for (int i = 0; i < MAXC && i < ps->characters->length; i++) {
			Citizen *c = As3Array_get(&ps->characters->members, (uint32_t)i);
			unsigned now = c && c->alive && c->action == Citizen_SHOOT;
			shots += now && !prev_shoot[i];
			prev_shoot[i] = now;
		}
		if (getenv("NPC_DEBUG") && f >= 1800 && f < 3600 && f % 30 == 0) {
			for (int i = 0; i < ps->characters->length; i++) {
				Citizen *c = As3Array_get(&ps->characters->members, (uint32_t)i);
				if (!c || !c->alive || c->occupation != Citizen_FARMER)
					continue;
				for (int k = 0; k < ps->farmlands->length; k++) {
					Farmland *fl = As3Array_get(&ps->farmlands->members, (uint32_t)k);
					if (fl && c->x > fl->x && c->x + c->width < fl->x + fl->width)
						printf("   f%u farmer#%d x=%.1f inside farm#%d [%.0f..%.0f] w=%.0f ready=%d act=%d cd=%.2f t=%.2f exists=%d\n", f, i, c->x, k,
							fl->x, fl->x + fl->width, fl->width, Farmland_needsWork(fl), c->action, c->shovelCooldown, fl->t, fl->exists);
				}
			}
		}
		if (f % 1800 == 1799)
			printf("   cumulative: field work steps %u, harvests %u, hunter shots %u\n", fl_work, fl_harvest, shots);
		for (int i = 0; i < MAXC && i < ps->characters->length; i++) {
			Citizen *c = As3Array_get(&ps->characters->members, (uint32_t)i);
			if (!c || !c->alive)
				continue;
			if (track[i] != c) {
				track[i] = c;
				total[i] = walked[i] = 0;
				lastx[i] = c->x;
				lo[i] = 1e9, hi[i] = -1e9;
			}
			total[i]++;
			walked[i] += fabs(c->x - lastx[i]) > 0.01;
			lastx[i] = c->x;
			lo[i] = c->x < lo[i] ? c->x : lo[i];
			hi[i] = c->x > hi[i] ? c->x : hi[i];
		}
	}
	return 0;
}
