/* GPU display list for the camera buffer (3DS speed path).
 *
 * When Gpu_recording is on, writes into Gpu_screen (FlxG.camera.buffer) are
 * not rasterised on the CPU: copyPixels / draw / fillRect become commands that
 * the platform plays back on the GPU (platform/3ds/gpu.c, built on the
 * previous native prototype's Citro2D path). Sprites keep their CPU
 * BitmapData; the GPU backend uploads one texture per BitmapData and only
 * re-uploads it when its `gen` changed.
 *
 * ADAPTATION (speed over pixel identity, by request): blends are done by the
 * GPU (normal / multiply / screen are exact equations, rounding differs);
 * Water's reflection is a flipped copy of the scene without the displacement
 * ripple; the final Weather ColorMatrix is the prototype's TEV approximation;
 * colour-transform offsets on the camera buffer are ignored.
 */
#ifndef TRANSLIT_GPU_H
#define TRANSLIT_GPU_H
#include "flash.h"

enum { GPUCMD_FILL, GPUCMD_IMAGE, GPUCMD_REFLECT };
enum { GPUBLEND_NORMAL, GPUBLEND_MULTIPLY, GPUBLEND_SCREEN };

typedef struct GpuCmd {
	uint8_t op, blend;
	uint8_t cont;          /* REFLECT strip: reuse the scene copy of the previous strip */
	int16_t dx;
	BitmapData *target;    /* NULL = screen, else a gpuSurface bitmap */            /* right-eye shift (Stereo_dx when recorded) */
	BitmapData *src;       /* IMAGE source; REFLECT: NULL = scene so far */
	float sx, sy, sw, sh;  /* source rectangle in source pixels */
	float a, b, c, d, tx, ty; /* source pixel -> screen */
	float tint[4];         /* r, g, b, a multipliers */
	uint32_t color;        /* FILL: ARGB */
	float fx, fy, fw, fh;  /* FILL rectangle */
} GpuCmd;

typedef struct GpuList {
	GpuCmd *cmds;
	int count, cap;
	/* index of the first command after the camera-buffer ColorMatrix filter
	   (PlayState.draw's weather.ambientTransform.applyFilter), -1 if none. The
	   original filters the buffer there, so what is drawn after it (camera
	   flash/fade in FlxGame.draw -> FlxCamera.drawFX) must not be filtered. */
	int filterAt;
} GpuList;

extern bool Gpu_recording;
extern BitmapData *Gpu_screen;
extern GpuList Gpu_list;

void Gpu_beginFrame(BitmapData *screen);
/* platform: frees the texture of a bitmap being disposed */
extern void (*Gpu_releaseTexture)(BitmapData *self);
void BitmapData_flushDisposed(void);
/* hooks used by flash.c / filters.c; return true when the call was recorded
   (or must be skipped) instead of rasterised */
bool Gpu_fillRect(BitmapData *self, Rectangle rect, uint32_t color);
bool Gpu_copyPixels(BitmapData *self, const BitmapData *source, Rectangle r, Point p);
bool Gpu_draw(BitmapData *self, const BitmapData *source, const Matrix *m, const ColorTransform *ct, const char *blend);
bool Gpu_screenFilter(BitmapData *self);
bool Gpu_skipDisplacement(const BitmapData *self, const BitmapData *map, Point mapPoint, uint32_t componentX,
	uint32_t componentY, Number scaleX, Number scaleY);
/* FlxSprite adapter: draw frame (sx,sy,w,h) of a sprite sheet with the frame
   matrix m (frame pixel -> screen); ct multipliers become the GPU tint */
bool Gpu_drawSprite(BitmapData *screen, BitmapData *sheet, BitmapData *frame, Number sx, Number sy, Number w, Number h,
	const Matrix *m, const ColorTransform *ct, const char *blend);

/* reference player on the CPU (host checks of the recorded list) */
void Gpu_playSoftware(const GpuList *list, BitmapData *dst, bool rightEye);
/* same, applying filter(dst) at the recorded filter point (commands before it
   first, the camera FX after it on the filtered image) */
/* stereo: the Water sprite's surface bitmap (set by the platform each frame); the recorder gives it a
   depth slope (rows shift further in the right eye towards the front). NULL = no water effect. */
/* GPU water ripple: the DisplacementMapFilter is applied per strip of this many pixels (the map is read at the
   strip's middle column; perlinNoise fills every SEG/2-th column). Smaller = closer to the exact per-pixel
   filter (the original) but more draw commands. Must divide 16 and be >= 4 (the lower screen's deep water
   reads map columns x + 8). */
extern int Gpu_waterSlope; /* 3D water: extra right-eye shift (px) reached at the screen bottom: 0 mirror, 2 soft, 4 tilt */
extern int Gpu_waterSeg; /* 4, 8 or 16; 8 on the host and New 3DS, 16 on Old 3DS (cost: ~700 more commands at 8) */
extern const BitmapData *Gpu_stereoWater;
/* right-eye shift of the water at a screen row (0 if the water was not recorded yet this frame) */
int Gpu_stereoWaterDxAt(Number screenY);
void Gpu_playSoftwareFiltered(const GpuList *list, BitmapData *dst, bool rightEye, void (*filter)(BitmapData *dst));
#endif
