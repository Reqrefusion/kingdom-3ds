#ifndef TRANSLIT_GPU3DS_H
#define TRANSLIT_GPU3DS_H
#include <citro2d.h>
#include "../../game/game.h"
#include "../../flash/gpu.h"
bool Gpu3ds_init(void);
/* plays Gpu_list into the scene texture(s) and shows it on the targets;
   weather = PlayState's Weather (NULL outside PlayState) */
void Gpu3ds_present(const GpuList *list, const Weather *weather, bool stereo, C3D_RenderTarget *top, C3D_RenderTarget *topRight,
	C3D_RenderTarget *bottom, bool bottomGame, float topScale, int w, int h);
/* PlayState.water: the lower screen shows its deep reflection (NULL: the game 1:1) */
extern Water *Gpu3ds_water;
void Gpu3ds_uploadReport(FILE *f, uint32_t frames);
/* Bottom-screen HUD helpers (3DS-only extras, outside the game's parity): call between C2D_SceneBegin(bottom)
   and C3D_FrameEnd. An image is a rectangle of a game bitmap at x,y (screen px),
   scaled (nearest). */
void Gpu3ds_hudImage(const BitmapData *bd, float sx, float sy, float sw, float sh, float x, float y, float scale, float alpha);
#endif
