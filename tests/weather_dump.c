/* Prints Weather fields and the translated ambientTransform for phases
 * (tools/build_host.py --main tests/weather_dump.c). Used to check the 3DS TEV
 * approximation (platform/3ds/gpu.c kg_weather_matrix) against the real matrix.
 *   weather_dump.exe <assets> A1 A3 ...
 */
#include "../game/game.h"
#include <assert.h>

extern Number FlxG_initialSeed;

int main(int argc, char **argv)
{
	Embed_root = argv[1];
	static char fonts[512];
	snprintf(fonts, sizeof fonts, "%s/fonts", argv[1]);
	TextField_fontRoot = fonts;
	FlxG_initialSeed = 0.5;
	FlxGame *game = king_new();
	FlxGame_onEnterFrame(game, 16);
	for (int i = 2; i < argc; i++) {
		char save[96];
		snprintf(save, sizeof save, "D2 %s X1920 B2 P2 F2 H2 W000011 C1 S00 G4", argv[i]);
		FlxG_switchState((FlxState *)PlayState_new(save));
		for (int f = 0; f < 90; f++)
			FlxGame_onEnterFrame(game, 16);
		PlayState *ps = (PlayState *)FlxG.state;
		Weather *w = ps->weather;
		printf("%s ambient=0x%08X amount=%.6f darkness=%.6f darknessColor=0x%08X saturation=%.6f contrast=%.6f wind=%.4f fog=%.4f\n",
			argv[i], (unsigned)w->ambient, w->ambientAmount, w->darkness, (unsigned)w->darknessColor, w->saturation, w->contrast,
			w->wind, w->fog);
		printf("  matrix:");
		for (int k = 0; k < 20; k++)
			printf("%s%.6f", k % 5 == 0 ? "\n   " : " ", w->ambientTransform.matrix[k]);
		printf("\n");
	}
	return 0;
}
