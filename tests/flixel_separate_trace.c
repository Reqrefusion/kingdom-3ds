/* Same 4096 random bodies as ../tests/flixel_trace.c, run through the
   translated FlxObject.separateX/separateY/separate. */
#include "../flixel/flixel.h"
#include <stdio.h>

static uint32_t seed = 0x12345678;
static unsigned rnd(unsigned n) { seed = seed * 1664525u + 1013904223u; return (seed >> 8) % n; }

static FlxObject make_body(void)
{
	FlxObject b;
	memset(&b, 0, sizeof b);
	b.x = (float)rnd(64) / 4; b.y = (float)rnd(64) / 4;
	b.last.x = b.x + ((float)rnd(17) - 8) / 4; b.last.y = b.y + ((float)rnd(17) - 8) / 4;
	b.width = 1 + (float)rnd(8); b.height = 1 + (float)rnd(8);
	b.velocity.x = (float)rnd(33) - 16; b.velocity.y = (float)rnd(33) - 16; b.mass = 1 + (float)rnd(4); b.elasticity = (float)rnd(3) / 2;
	unsigned mask[] = {ANY, NONE, DOWN, UP, LEFT, RIGHT};
	b.allowCollisions = mask[rnd(6)]; b.immovable = rnd(4) == 0; b.moves = rnd(2) != 0; b.active = rnd(2) != 0;
	return b;
}

static void print_body(FlxObject b)
{
	printf(",%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%u,%u,%u,%u",
		b.x, b.y, b.last.x, b.last.y, b.width, b.height, b.velocity.x, b.velocity.y, b.mass, b.elasticity,
		b.allowCollisions, b.immovable, b.active, b.moves);
}

int main(void)
{
	puts("case,mode,inputs_and_outputs");
	for (unsigned c = 0; c < 4096; c++) {
		FlxObject a = make_body(), b = make_body();
		unsigned mode = c % 3;
		if (c % 2 == 0) {
			b.x = a.x + ((float)rnd(17) - 8) / 4; b.y = a.y + ((float)rnd(17) - 8) / 4;
			b.last.x = b.x + ((float)rnd(17) - 8) / 4; b.last.y = b.y + ((float)rnd(17) - 8) / 4;
		}
		if (c & 4) a.allowCollisions = b.allowCollisions = ANY;
		printf("%u,%u", c, mode);
		print_body(a);
		print_body(b);
		bool hit = mode == 2 ? FlxObject_separate(&a, &b) : (mode == 1 ? FlxObject_separateY(&a, &b) : FlxObject_separateX(&a, &b));
		printf(",%u,%.17g,%.17g,%.17g,%.17g,%u,%.17g,%.17g,%.17g,%.17g,%u\n", hit, a.x, a.y, a.velocity.x, a.velocity.y,
			a.touching, b.x, b.y, b.velocity.x, b.velocity.y, b.touching);
	}
	return 0;
}
