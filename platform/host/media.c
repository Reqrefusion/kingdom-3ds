/* Silent audio backend for host runs: sounds have no length and never end,
   which is enough for gameplay/static checks. Plays are counted per embed
   name so scenario tests can observe FlxG.play / Sound.play calls. */
#include "../../flash/media.h"

#define MAX_HOST_SOUNDS 64
static char names[MAX_HOST_SOUNDS][96];
static int plays[MAX_HOST_SOUNDS];
static int count;
static int nextVoice;
static int voiceSound[4096];

int FlashMediaBackend_load(const char *embedName, Number *lengthMs)
{
	*lengthMs = 0;
	for (int i = 0; i < count; i++)
		if (strcmp(names[i], embedName) == 0)
			return i;
	if (count >= MAX_HOST_SOUNDS)
		return -1;
	snprintf(names[count], sizeof names[count], "%s", embedName);
	return count++;
}
int FlashMediaBackend_play(int soundId, Number startTimeMs, int32_t loops, Number volume, Number pan)
{
	(void)startTimeMs; (void)loops; (void)volume; (void)pan;
	if (soundId >= 0 && soundId < count)
		plays[soundId]++;
	int v = nextVoice++ & 0x7fffffff;
	voiceSound[v & 4095] = soundId;
	return v;
}
static int stops;
void FlashMediaBackend_stop(int voice) { (void)voice; stops++; }
void FlashMediaBackend_setTransform(int voice, Number volume, Number pan) { (void)voice; (void)volume; (void)pan; }
Number FlashMediaBackend_position(int voice) { (void)voice; return 0; }
bool FlashMediaBackend_finished(int voice) { (void)voice; return false; }
void FlashMediaBackend_refreshMix(void) {}

/* test hooks: plays of sounds whose embed name contains `part` */
int HostMedia_plays(const char *part)
{
	int n = 0;
	for (int i = 0; i < count; i++)
		if (strstr(names[i], part))
			n += plays[i];
	return n;
}
void HostMedia_resetPlays(void)
{
	memset(plays, 0, sizeof plays);
	stops = 0;
}
int HostMedia_stops(void)
{
	return stops;
}
