/* Every NPC of PlayState, one block per frame, for comparing two builds of Kingdom NPC by NPC
 * (tests/compare_npcs.py). The same file is in the hand port (kingdom-3ds, its PC host folder) and in kingdomgen-3ds/game: both
 * builds have the same field names (the hand port's game.h and as2c's gen.h).
 * Written only when the environment variable KINGDOM_NPC_TRACE names a file. */
#ifndef KINGDOM_NPC_TRACE_H
#define KINGDOM_NPC_TRACE_H
#include <stdio.h>
#include <stdlib.h>

static void npc_trace_group(FILE *f, const char *name, FlxGroup *g, int kind)
{
	if (g == NULL)
		return;
	for (uint32_t i = 0; i < g->members.length; i++) {
		FlxSprite *s = As3Array_get(&g->members, i);
		if (s == NULL || !s->exists)
			continue;
		fprintf(f, "%s %u %d %.17g %.17g %.17g", name, i, s->alive ? 1 : 0, s->x, s->y, s->velocity.x);
		if (kind == 0) {
			Citizen *c = (Citizen *)s;
			fprintf(f, " occ%d act%d goal%.17g coins%d hungry%d t%.17g guard%d", (int)c->occupation, (int)c->action,
				(double)c->goal, (int)c->coins, (int)c->hungry, (double)c->t, c->guarding ? 1 : 0);
		} else if (kind == 1) {
			Troll *t = (Troll *)s;
			fprintf(f, " goal%.17g coin%d crown%d wait%d retreat%d", (double)t->goal, t->hasCoin ? 1 : 0, t->hasCrown ? 1 : 0,
				t->wait ? 1 : 0, t->retreating ? 1 : 0);
		}
		fputc('\n', f);
	}
}

static void npc_trace_frame(PlayState *ps, unsigned frame)
{
	static FILE *f;
	static int opened;
	if (!opened) {
		opened = 1;
		const char *path = getenv("KINGDOM_NPC_TRACE");
		if (path != NULL && path[0])
			f = fopen(path, "w");
	}
	if (f == NULL || ps == NULL)
		return;
	fprintf(f, "F %u\n", frame);
	npc_trace_group(f, "beggar", (FlxGroup *)ps->beggars, 0);
	npc_trace_group(f, "char", (FlxGroup *)ps->characters, 0);
	npc_trace_group(f, "archer", (FlxGroup *)ps->archers, 0);
	npc_trace_group(f, "troll", (FlxGroup *)ps->trolls, 1);
	npc_trace_group(f, "bunny", (FlxGroup *)ps->bunnies, 2);
	fflush(f);
}

#endif
