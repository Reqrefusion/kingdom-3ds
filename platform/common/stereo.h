/* Stereoscopic top screen (ported idea from the previous native prototype:
 * "stereo scenery layers"). Depth comes from the source's own parallax:
 * an object's scrollFactor says how far it is (1 = gameplay plane, 0.5 =
 * backdrop hills, 0 = sky at infinity, >1 = foreground fog). The right eye
 * gets the same draw calls shifted by maxShift * (1 - scrollFactor), clamped
 * to +-maxShift. HUD-like objects with scrollFactor 0 (darkness overlay, text,
 * coin sack, water) stay on the screen plane; Sky, SunMoon and Haze are far.
 *
 * ADAPTATION: the original is monoscopic; this only adds a second view and
 * never changes what the left eye (= the original frame) shows. */
#ifndef TRANSLIT_STEREO_H
#define TRANSLIT_STEREO_H
#include "../../game/game.h"

/* maxShift in game pixels (0 disables); returns the right-eye buffer or NULL */
BitmapData *Stereo_begin(int32_t maxShift);
void Stereo_end(void);
/* GPU path: set Stereo_dx per object only (recorded into the display list) */
void Stereo_beginDepthOnly(int32_t maxShift);
#endif
