/* org/flixel/FlxSound.as */
#ifndef TRANSLIT_FLXSOUND_H
#define TRANSLIT_FLXSOUND_H
#include "flixel.h"
#include "../flash/media.h"

typedef struct FlxSound {
	FLXBASIC_FIELDS
	Number x;
	Number y;
	bool survive;
	const char *name;
	const char *artist;
	Number amplitude;
	Number amplitudeLeft;
	Number amplitudeRight;
	bool autoDestroy;
	Sound *_sound;
	SoundChannel *_channel;
	SoundTransform *_transform;
	Number _position;
	Number _volume;
	Number _volumeAdjust;
	bool _looped;
	FlxObject *_target;
	Number _radius;
	bool _pan;
	Number _fadeOutTimer;
	Number _fadeOutTotal;
	bool _pauseOnFadeOut;
	Number _fadeInTimer;
	Number _fadeInTotal;
} FlxSound;

extern const As3Class FlxSound_class;
FlxSound *FlxSound_new(void);
void FlxSound_createSound(FlxSound *self);
void FlxSound_destroy(FlxSound *self);
void FlxSound_update(FlxSound *self);
void FlxSound_kill(FlxSound *self);
FlxSound *FlxSound_loadEmbedded(FlxSound *self, const char *EmbeddedSound, bool Looped, bool AutoDestroy);
FlxSound *FlxSound_proximity(FlxSound *self, Number X, Number Y, FlxObject *Object, Number Radius, bool Pan);
void FlxSound_play(FlxSound *self, bool ForceRestart);
void FlxSound_resume(FlxSound *self);
void FlxSound_pause(FlxSound *self);
void FlxSound_stop(FlxSound *self);
void FlxSound_fadeOut(FlxSound *self, Number Seconds, bool PauseInstead);
void FlxSound_fadeIn(FlxSound *self, Number Seconds);
Number FlxSound_get_volume(FlxSound *self);
void FlxSound_set_volume(FlxSound *self, Number Volume);
Number FlxSound_getActualVolume(FlxSound *self);
void FlxSound_updateTransform(FlxSound *self);

void FlxG_playMusic(const char *Music, Number Volume);
FlxSound *FlxG_loadSound(const char *EmbeddedSound, Number Volume, bool Looped, bool AutoDestroy, bool AutoPlay);
FlxSound *FlxG_play(const char *EmbeddedSound, Number Volume, bool Looped, bool AutoDestroy);
Number FlxG_get_volume(void);
void FlxG_set_volume(Number Volume);
void FlxG_destroySounds(bool ForceDestroy);
void FlxG_updateSounds(void);
void FlxG_pauseSounds(void);
void FlxG_resumeSounds(void);

#endif
