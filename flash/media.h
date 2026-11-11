/* flash.media.Sound / SoundChannel / SoundTransform.
 * The actual mixing is done by the platform backend (NDSP on 3DS, silent
 * timing-only backend on the host). SOUND_COMPLETE events are queued by the
 * backend and dispatched once per frame by FlashMedia_dispatchEvents(),
 * which mirrors Flash delivering events between frames. */
#ifndef TRANSLIT_FLASH_MEDIA_H
#define TRANSLIT_FLASH_MEDIA_H
#include "../as3/as3.h"

typedef struct SoundTransform {
	Number volume;
	Number pan;
} SoundTransform;

typedef struct Sound {
	const char *embedName;
	Number length; /* milliseconds */
	int backendId;
} Sound;

typedef struct SoundChannel {
	int voice; /* backend voice, -1 when stopped */
	Number leftPeak;
	Number rightPeak;
	/* addEventListener(Event.SOUND_COMPLETE, listener) */
	void *completeSelf;
	void (*completeListener)(void *self);
	bool completed;
	bool released; /* stop() was called: the owner forgets it */
	struct SoundChannel *nextLive;
} SoundChannel;

SoundTransform *SoundTransform_new(void);
Sound *Sound_new(const char *embedName);
/* Sound.play(startTime, loops, sndTransform); null when no voice is free. */
SoundChannel *Sound_play(Sound *self, Number startTime, int32_t loops, SoundTransform *sndTransform);
Number SoundChannel_get_position(SoundChannel *self);
void SoundChannel_stop(SoundChannel *self);
void SoundChannel_set_soundTransform(SoundChannel *self, SoundTransform *transform);
void SoundChannel_addCompleteListener(SoundChannel *self, void *listenerSelf, void (*listener)(void *));
void SoundChannel_removeCompleteListener(SoundChannel *self);

/* Called by the platform once per frame. */
void FlashMedia_dispatchEvents(void);

/* ---- backend interface (implemented in platform/) ----------------------- */
int FlashMediaBackend_load(const char *embedName, Number *lengthMs);
int FlashMediaBackend_play(int soundId, Number startTimeMs, int32_t loops, Number volume, Number pan);
void FlashMediaBackend_stop(int voice);
void FlashMediaBackend_setTransform(int voice, Number volume, Number pan);
Number FlashMediaBackend_position(int voice);
/* true once the voice has played to the end of its last loop */
bool FlashMediaBackend_finished(int voice);

#endif
