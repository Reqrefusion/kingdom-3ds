#include "stereo.h"
#include "../../flash/gpu.h"

static int32_t shift;
static BitmapData *right;

static int32_t depth(FlxObject *o)
{
	Number sf = o->scrollFactor.x;
	if (sf == 0) {
		FlxBasic *b = (FlxBasic *)o;
		bool far = as3_is(b, &Sky_class) || as3_is(b, &SunMoon_class) || as3_is(b, &Haze_class);
		return far ? shift : 0;
	}
	Number d = shift * (1 - sf);
	if (d > shift) d = shift;
	if (d < -shift) d = -shift;
	/* Background layers round DOWN so that neighbouring planes keep distinct depths (far hills sf 0.15 -> 4,
	   near hills 0.3 -> 3, tree lines 0.5 -> 2 instead of 4 / 4 / 3); the swaying wheat in front of the trees
	   sits one plane further forward (1). Foreground layers keep the nearest rounding. */
	int32_t dx = d > 0 ? (int32_t)floor(d + 1e-6) : (int32_t)floor(d + 0.5);
	if (d > 0 && as3_is((FlxBasic *)o, &Reed_class) && dx > (int32_t)floor(shift * 0.2 + 0.5))
		dx = (int32_t)floor(shift * 0.2 + 0.5);
	if (sf > 1 && as3_is((FlxBasic *)o, &FlxSprite_class)) {
		/* foreground fog floats above the water: at its height it must stay at least one pixel in front of
		   the water surface (which comes towards the viewer, see Gpu_stereoWaterDxAt), or the fog would
		   sink behind the water's front rows */
		const FlxSprite *s = (const FlxSprite *)o;
		int32_t water = Gpu_stereoWaterDxAt(s->_point.y + o->height * 0.5);
		if (dx > water - 1)
			dx = water - 1;
		if (dx < -shift)
			dx = -shift;
	}
	return dx;
}

BitmapData *Stereo_begin(int32_t maxShift)
{
	if (maxShift <= 0 || FlxG.camera == NULL) {
		Stereo_maxShift = 0;
		Stereo_right = NULL;
		Stereo_depth = NULL;
		return NULL;
	}
	BitmapData *left = FlxG.camera->buffer;
	if (right == NULL || right->width != left->width || right->height != left->height)
		right = BitmapData_new(left->width, left->height, left->transparent, 0xFF000000);
	shift = maxShift;
	Stereo_maxShift = maxShift;
	Stereo_left = left;
	Stereo_right = right;
	Stereo_depth = depth;
	return right;
}

void Stereo_end(void)
{
	/* far layers shifted right leave the first columns of the right eye
	   empty: repeat the first drawn column (edge clamp) */
	if (Stereo_right != NULL && shift > 0 && shift < right->width)
		for (int32_t y = 0; y < right->height; y++) {
			uint32_t *row = &right->pixels[y * right->width];
			for (int32_t x = 0; x < shift; x++)
				row[x] = row[shift];
		}
	Stereo_right = NULL;
	Stereo_depth = NULL;
}

/* GPU path: only per-object depth (Stereo_dx); no CPU right-eye buffer */
void Stereo_beginDepthOnly(int32_t maxShift)
{
	shift = maxShift;
	Stereo_maxShift = maxShift > 0 ? maxShift : 0;
	Stereo_right = NULL;
	Stereo_depth = maxShift > 0 ? depth : NULL;
}
