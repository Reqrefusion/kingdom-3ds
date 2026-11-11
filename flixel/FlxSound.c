/* org/flixel/FlxSound.as and the sound functions of org/flixel/FlxG.as.
 * Not translated: loadStream/gotID3 (network streaming; Kingdom only uses
 * embedded sounds). */
#include "FlxSound.h"

const As3Class FlxSound_class = {
	"FlxSound", &FlxBasic_class, (FlxBasic * (*)(void)) FlxSound_new,
	{
		.destroy = (void (*)(FlxBasic *))FlxSound_destroy,
		.preUpdate = FlxBasic_preUpdate,
		.update = (void (*)(FlxBasic *))FlxSound_update,
		.postUpdate = FlxBasic_postUpdate,
		.draw = FlxBasic_draw,
		.kill = (void (*)(FlxBasic *))FlxSound_kill,
		.revive = FlxBasic_revive,
	},
};

static void FlxSound_looped(void *self);
static void FlxSound_stopped(void *self);

FlxSound *FlxSound_new(void)
{
	FlxSound *self = as3_alloc(sizeof *self);
	self->klass = &FlxSound_class;
	SUPER(FlxBasic_ctor((FlxBasic *)self));
	FlxSound_createSound(self);
	return self;
}

void FlxSound_createSound(FlxSound *self)
{
	FlxSound_destroy(self);
	self->x = 0;
	self->y = 0;
	if (self->_transform == NULL)
		self->_transform = SoundTransform_new();
	self->_transform->pan = 0;
	self->_sound = NULL;
	self->_position = 0;
	self->_volume = 1.0;
	self->_volumeAdjust = 1.0;
	self->_looped = false;
	self->_target = NULL;
	self->_radius = 0;
	self->_pan = false;
	self->_fadeOutTimer = 0;
	self->_fadeOutTotal = 0;
	self->_pauseOnFadeOut = false;
	self->_fadeInTimer = 0;
	self->_fadeInTotal = 0;
	self->exists = false;
	self->active = false;
	self->visible = false;
	self->name = NULL;
	self->artist = NULL;
	self->amplitude = 0;
	self->amplitudeLeft = 0;
	self->amplitudeRight = 0;
	self->autoDestroy = false;
}

void FlxSound_destroy(FlxSound *self)
{
	CALL0(self, kill);

	/* _transform = null: it is recreated by createSound(); C keeps the
	   allocation so the next createSound() reuses it. */
	self->_sound = NULL;
	self->_channel = NULL;
	self->_target = NULL;
	self->name = NULL;
	self->artist = NULL;

	SUPER(FlxBasic_destroy((FlxBasic *)self));
}

void FlxSound_update(FlxSound *self)
{
	if (self->_position != 0)
		return;

	Number radial = 0.0;
	Number fade = 1.0;

	/* Distance-based volume control */
	if (self->_target != NULL) {
		FlxPoint a = FlxPoint_new(self->_target->x, self->_target->y);
		FlxPoint b = FlxPoint_new(self->x, self->y);
		radial = FlxU_getDistance(&a, &b) / self->_radius;
		if (radial < 0) radial = 0;
		if (radial > 1) radial = 1;

		if (self->_pan) {
			Number d = (self->x - self->_target->x) / self->_radius;
			if (d < -1) d = -1;
			else if (d > 1) d = 1;
			self->_transform->pan = d;
		}
	}

	/* Cross-fading volume control */
	if (self->_fadeOutTimer > 0) {
		self->_fadeOutTimer -= FlxG.elapsed;
		if (self->_fadeOutTimer <= 0) {
			if (self->_pauseOnFadeOut)
				FlxSound_pause(self);
			else
				FlxSound_stop(self);
		}
		fade = self->_fadeOutTimer / self->_fadeOutTotal;
		if (fade < 0) fade = 0;
	} else if (self->_fadeInTimer > 0) {
		self->_fadeInTimer -= FlxG.elapsed;
		fade = self->_fadeInTimer / self->_fadeInTotal;
		if (fade < 0) fade = 0;
		fade = 1 - fade;
	}

	self->_volumeAdjust = (1 - radial) * fade;
	FlxSound_updateTransform(self);

	if ((self->_transform->volume > 0) && (self->_channel != NULL)) {
		self->amplitudeLeft = self->_channel->leftPeak / self->_transform->volume;
		self->amplitudeRight = self->_channel->rightPeak / self->_transform->volume;
		self->amplitude = (self->amplitudeLeft + self->amplitudeRight) * 0.5;
	}
}

void FlxSound_kill(FlxSound *self)
{
	SUPER(FlxBasic_kill((FlxBasic *)self));
	if (self->_channel != NULL)
		FlxSound_stop(self);
}

FlxSound *FlxSound_loadEmbedded(FlxSound *self, const char *EmbeddedSound, bool Looped, bool AutoDestroy)
{
	(void)AutoDestroy; /* unused in the pinned source as well */
	FlxSound_stop(self);
	FlxSound_createSound(self);
	self->_sound = Sound_new(EmbeddedSound);
	/* NOTE: can't pull ID3 info from embedded sound currently */
	self->_looped = Looped;
	FlxSound_updateTransform(self);
	self->exists = true;
	return self;
}

FlxSound *FlxSound_proximity(FlxSound *self, Number X, Number Y, FlxObject *Object, Number Radius, bool Pan)
{
	self->x = X;
	self->y = Y;
	self->_target = Object;
	self->_radius = Radius;
	self->_pan = Pan;
	return self;
}

void FlxSound_play(FlxSound *self, bool ForceRestart)
{
	if (self->_position < 0)
		return;
	if (ForceRestart) {
		bool oldAutoDestroy = self->autoDestroy;
		self->autoDestroy = false;
		FlxSound_stop(self);
		self->autoDestroy = oldAutoDestroy;
	}
	if (self->_looped) {
		if (self->_position == 0) {
			if (self->_channel == NULL)
				self->_channel = Sound_play(self->_sound, 0, 9999, self->_transform);
			if (self->_channel == NULL)
				self->exists = false;
		} else {
			self->_channel = Sound_play(self->_sound, self->_position, 0, self->_transform);
			if (self->_channel == NULL)
				self->exists = false;
			else
				SoundChannel_addCompleteListener(self->_channel, self, FlxSound_looped);
		}
	} else {
		if (self->_position == 0) {
			if (self->_channel == NULL) {
				self->_channel = Sound_play(self->_sound, 0, 0, self->_transform);
				if (self->_channel == NULL)
					self->exists = false;
				else
					SoundChannel_addCompleteListener(self->_channel, self, FlxSound_stopped);
			}
		} else {
			self->_channel = Sound_play(self->_sound, self->_position, 0, self->_transform);
			if (self->_channel == NULL)
				self->exists = false;
		}
	}
	self->active = (self->_channel != NULL);
	self->_position = 0;
}

void FlxSound_resume(FlxSound *self)
{
	if (self->_position <= 0)
		return;
	if (self->_looped) {
		self->_channel = Sound_play(self->_sound, self->_position, 0, self->_transform);
		if (self->_channel == NULL)
			self->exists = false;
		else
			SoundChannel_addCompleteListener(self->_channel, self, FlxSound_looped);
	} else {
		self->_channel = Sound_play(self->_sound, self->_position, 0, self->_transform);
		if (self->_channel == NULL)
			self->exists = false;
	}
	self->active = (self->_channel != NULL);
}

void FlxSound_pause(FlxSound *self)
{
	if (self->_channel == NULL) {
		self->_position = -1;
		return;
	}
	self->_position = SoundChannel_get_position(self->_channel);
	SoundChannel_stop(self->_channel);
	if (self->_looped) {
		while (self->_sound->length > 0 && self->_position >= self->_sound->length)
			self->_position -= self->_sound->length;
	}
	if (self->_position <= 0)
		self->_position = 1;
	self->_channel = NULL;
	self->active = false;
}

void FlxSound_stop(FlxSound *self)
{
	self->_position = 0;
	if (self->_channel != NULL) {
		SoundChannel_stop(self->_channel);
		FlxSound_stopped(self);
	}
}

void FlxSound_fadeOut(FlxSound *self, Number Seconds, bool PauseInstead)
{
	self->_pauseOnFadeOut = PauseInstead;
	self->_fadeInTimer = 0;
	self->_fadeOutTimer = Seconds;
	self->_fadeOutTotal = self->_fadeOutTimer;
}

void FlxSound_fadeIn(FlxSound *self, Number Seconds)
{
	self->_fadeOutTimer = 0;
	self->_fadeInTimer = Seconds;
	self->_fadeInTotal = self->_fadeInTimer;
	FlxSound_play(self, false);
}

Number FlxSound_get_volume(FlxSound *self)
{
	return self->_volume;
}

void FlxSound_set_volume(FlxSound *self, Number Volume)
{
	self->_volume = Volume;
	if (self->_volume < 0)
		self->_volume = 0;
	else if (self->_volume > 1)
		self->_volume = 1;
	FlxSound_updateTransform(self);
}

Number FlxSound_getActualVolume(FlxSound *self)
{
	return self->_volume * self->_volumeAdjust;
}

void FlxSound_updateTransform(FlxSound *self)
{
	self->_transform->volume = (FlxG.mute ? 0 : 1) * FlxG_get_volume() * self->_volume * self->_volumeAdjust;
	if (self->_channel != NULL)
		SoundChannel_set_soundTransform(self->_channel, self->_transform);
}

static void FlxSound_looped(void *object)
{
	FlxSound *self = object;
	if (self->_channel == NULL)
		return;
	SoundChannel_removeCompleteListener(self->_channel);
	self->_channel = NULL;
	FlxSound_play(self, false);
}

static void FlxSound_stopped(void *object)
{
	FlxSound *self = object;
	SoundChannel_removeCompleteListener(self->_channel);
	self->_channel = NULL;
	self->active = false;
	if (self->autoDestroy)
		CALL0(self, destroy);
}

/* ---- FlxG sound functions ------------------------------------------------- */

void FlxG_playMusic(const char *Music, Number Volume)
{
	if (FlxG.music == NULL)
		FlxG.music = FlxSound_new();
	else if (FlxG.music->active)
		FlxSound_stop(FlxG.music);
	FlxSound_loadEmbedded(FlxG.music, Music, true, false);
	FlxSound_set_volume(FlxG.music, Volume);
	FlxG.music->survive = true;
	FlxSound_play(FlxG.music, false);
}

FlxSound *FlxG_loadSound(const char *EmbeddedSound, Number Volume, bool Looped, bool AutoDestroy, bool AutoPlay)
{
	if (EmbeddedSound == NULL) {
		FlxG_log("WARNING: FlxG.loadSound() requires either\nan embedded sound or a URL to work.");
		return NULL;
	}
	FlxSound *sound = (FlxSound *)FlxGroup_recycle(FlxG.sounds, &FlxSound_class);
	FlxSound_loadEmbedded(sound, EmbeddedSound, Looped, AutoDestroy);
	FlxSound_set_volume(sound, Volume);
	if (AutoPlay)
		FlxSound_play(sound, false);
	return sound;
}

FlxSound *FlxG_play(const char *EmbeddedSound, Number Volume, bool Looped, bool AutoDestroy)
{
	return FlxG_loadSound(EmbeddedSound, Volume, Looped, AutoDestroy, true);
}

Number FlxG_get_volume(void)
{
	return FlxG._volume;
}

void FlxG_set_volume(Number Volume)
{
	FlxG._volume = Volume;
	if (FlxG._volume < 0)
		FlxG._volume = 0;
	else if (FlxG._volume > 1)
		FlxG._volume = 1;
}

void FlxG_destroySounds(bool ForceDestroy)
{
	if ((FlxG.music != NULL) && (ForceDestroy || !FlxG.music->survive)) {
		CALL0(FlxG.music, destroy);
		FlxG.music = NULL;
	}
	uint32_t i = 0;
	FlxSound *sound;
	uint32_t l = FlxG.sounds->members.length;
	while (i < l) {
		sound = As3Array_get(&FlxG.sounds->members, i++);
		if ((sound != NULL) && (ForceDestroy || !sound->survive))
			CALL0(sound, destroy);
	}
}

void FlxG_updateSounds(void)
{
	/* Flash delivers SOUND_COMPLETE between frames; doing it here keeps
	   that before any sound is updated in this step. */
	FlashMedia_dispatchEvents();
	if ((FlxG.music != NULL) && FlxG.music->active)
		CALL0(FlxG.music, update);
	if ((FlxG.sounds != NULL) && FlxG.sounds->active)
		CALL0(FlxG.sounds, update);
}

void FlxG_pauseSounds(void)
{
	if ((FlxG.music != NULL) && FlxG.music->exists && FlxG.music->active)
		FlxSound_pause(FlxG.music);
	uint32_t i = 0;
	FlxSound *sound;
	uint32_t l = AS3_UINT(FlxG.sounds->length);
	while (i < l) {
		sound = As3Array_get(&FlxG.sounds->members, i++);
		if ((sound != NULL) && sound->exists && sound->active)
			FlxSound_pause(sound);
	}
}

void FlxG_resumeSounds(void)
{
	if ((FlxG.music != NULL) && FlxG.music->exists)
		FlxSound_play(FlxG.music, false);
	uint32_t i = 0;
	FlxSound *sound;
	uint32_t l = AS3_UINT(FlxG.sounds->length);
	while (i < l) {
		sound = As3Array_get(&FlxG.sounds->members, i++);
		if ((sound != NULL) && sound->exists)
			FlxSound_resume(sound);
	}
}

/* ---- FlxTimer / TimerManager: no timers are used by Kingdom ---------------- */

void FlxTimer_updateManager(void) {}
void FlxTimer_clearManager(void) {}
