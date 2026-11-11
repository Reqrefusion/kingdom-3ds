#include "media.h"

static SoundChannel *live_channels;

SoundTransform *SoundTransform_new(void)
{
	SoundTransform *t = as3_alloc(sizeof *t);
	t->volume = 1;
	t->pan = 0;
	return t;
}

Sound *Sound_new(const char *embedName)
{
	Sound *s = as3_alloc(sizeof *s);
	s->embedName = embedName;
	s->backendId = FlashMediaBackend_load(embedName, &s->length);
	return s;
}

SoundChannel *Sound_play(Sound *self, Number startTime, int32_t loops, SoundTransform *sndTransform)
{
	int voice = FlashMediaBackend_play(self->backendId, startTime, loops,
		sndTransform ? sndTransform->volume : 1, sndTransform ? sndTransform->pan : 0);
	if (voice < 0)
		return NULL;
	SoundChannel *c = as3_alloc(sizeof *c);
	c->voice = voice;
	c->nextLive = live_channels;
	live_channels = c;
	return c;
}

Number SoundChannel_get_position(SoundChannel *self)
{
	return self->voice >= 0 ? FlashMediaBackend_position(self->voice) : 0;
}

void SoundChannel_stop(SoundChannel *self)
{
	if (self->voice >= 0)
		FlashMediaBackend_stop(self->voice);
	self->voice = -1;
	/* Every caller (FlxSound.stop/pause) drops its reference right after. */
	self->released = true;
}

void SoundChannel_set_soundTransform(SoundChannel *self, SoundTransform *transform)
{
	if (self->voice >= 0)
		FlashMediaBackend_setTransform(self->voice, transform->volume, transform->pan);
}

void SoundChannel_addCompleteListener(SoundChannel *self, void *listenerSelf, void (*listener)(void *))
{
	self->completeSelf = listenerSelf;
	self->completeListener = listener;
}

void SoundChannel_removeCompleteListener(SoundChannel *self)
{
	self->completeSelf = NULL;
	self->completeListener = NULL;
}

void FlashMedia_dispatchEvents(void)
{
	/* 1. find channels whose sound ended */
	SoundChannel *pending[64];
	int count = 0;
	for (SoundChannel *c = live_channels; c != NULL; c = c->nextLive)
		if (c->voice >= 0 && !c->completed && FlashMediaBackend_finished(c->voice) && count < 64) {
			FlashMediaBackend_stop(c->voice);
			c->voice = -1;
			c->completed = true;
			pending[count++] = c;
		}
	/* 2. deliver SOUND_COMPLETE (listeners may start new channels) */
	for (int i = 0; i < count; i++)
		if (pending[i]->completeListener != NULL) {
			pending[i]->completeListener(pending[i]->completeSelf);
			/* FlxSound.stopped/looped set _channel = null */
			pending[i]->released = true;
		}
	/* 3. drop channels nobody references any more */
	SoundChannel **link = &live_channels;
	while (*link != NULL) {
		SoundChannel *c = *link;
		if (c->released) {
			*link = c->nextLive;
			free(c);
			continue;
		}
		link = &c->nextLive;
	}
}
