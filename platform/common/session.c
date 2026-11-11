#include "session.h"
#include <sys/stat.h>

bool FlashMediaBackend_musicEnabled = true;

static void path_of(Session *s, char *out, size_t n, const char *name)
{
	snprintf(out, n, "%s/%s", s->dir, name);
}

static bool replace_file(const char *p, const char *text);

static void logf_(Session *s, const char *fmt, const char *arg)
{
	if (s->log) {
		fprintf(s->log, fmt, arg);
		fflush(s->log);
	}
}

void Session_init(Session *s, const char *dir)
{
	memset(s, 0, sizeof *s);
	snprintf(s->dir, sizeof s->dir, "%s", dir);
	mkdir(dir, 0777);
	s->settings.music = true;
	s->settings.stereo = 100;
	s->settings.water3d = 2;
	char p[320];
	path_of(s, p, sizeof p, "session.log");
	s->log = fopen(p, "w");
	path_of(s, p, sizeof p, "settings.cfg");
	FILE *f = fopen(p, "r");
	if (f) {
		int music, stereo, fill, water;
		int n = fscanf(f, "music %d stereo %d fill %d water %d", &music, &stereo, &fill, &water);
		if (n >= 3)
			s->settings.fill = fill != 0;
		if (n == 4)
			s->settings.water3d = water < 0 ? 0 : water > 2 ? 2 : water;
		if (n >= 1)
			s->settings.music = music != 0;
		if (n >= 2)
			s->settings.stereo = stereo < 0 ? 0 : stereo > 200 ? 200 : stereo;
		fclose(f);
	}
	FlashMediaBackend_musicEnabled = s->settings.music;
}

void Session_saveSettings(Session *s)
{
	char p[320];
	path_of(s, p, sizeof p, "settings.cfg");
	char text[64];
	snprintf(text, sizeof text, "music %d stereo %d fill %d water %d", s->settings.music ? 1 : 0, s->settings.stereo,
		s->settings.fill ? 1 : 0, s->settings.water3d);
	replace_file(p, text); /* a failed write keeps the previous settings.cfg */
}

/* Writes text to dir/name through name.tmp. The previous file is kept as
   name.bak until the new one is in place and put back if the final rename
   fails (the earlier code removed it first: a failed rename lost the save).
   Same scheme as ../source/progress.c::progress_save. Rollback of a failed
   replace, not a power-loss guarantee on FAT. */
static bool replace_file(const char *p, const char *text)
{
	char tmp[340], bak[340];
	snprintf(tmp, sizeof tmp, "%s.tmp", p);
	snprintf(bak, sizeof bak, "%s.bak", p);
	FILE *f = fopen(tmp, "w");
	if (!f)
		return false;
	bool ok = fputs(text, f) >= 0 && fputc('\n', f) != EOF;
	ok = fflush(f) == 0 && ok;
	ok = fclose(f) == 0 && ok;
	if (!ok) {
		remove(tmp);
		return false;
	}
	bool hadPrevious = false;
	FILE *prev = fopen(p, "r");
	if (prev) {
		fclose(prev);
		remove(bak);
		if (rename(p, bak) != 0) {
			remove(tmp);
			return false;
		}
		hadPrevious = true;
	}
	if (rename(tmp, p) != 0) {
		remove(tmp);
		if (hadPrevious)
			rename(bak, p);
		return false;
	}
	if (hadPrevious)
		remove(bak);
	return true;
}

static bool write_atomic(Session *s, const char *name, const char *text)
{
	char p[320];
	path_of(s, p, sizeof p, name);
	return replace_file(p, text);
}

/* one checkpoint file: true and the text in line when it is well-formed */
static bool read_progress(const char *p, char line[256])
{
	FILE *f = fopen(p, "r");
	if (!f)
		return false;
	line[0] = 0;
	char *got = fgets(line, 256, f);
	fclose(f);
	if (!got)
		return false;
	line[strcspn(line, "\r\n")] = 0;
	/* PlayState.setProgress needs at least the day and phase fields */
	return line[0] == 'D' && strstr(line, " A") != NULL;
}

const char *Session_savedProgress(Session *s)
{
	char p[320], bak[330], line[256];
	path_of(s, p, sizeof p, "progress.sav");
	snprintf(bak, sizeof bak, "%s.bak", p);
	/* a malformed progress.sav never replaces live state; the backup left by
	   an interrupted replace_file is the fallback */
	if (!read_progress(p, line) && !read_progress(bak, line))
		return NULL;
	snprintf(s->lastSaved, sizeof s->lastSaved, "%s", line);
	return s->lastSaved;
}

bool Session_resume(Session *s)
{
	const char *saved = Session_savedProgress(s);
	if (saved == NULL)
		return false;
	logf_(s, "resume from progress.sav: %s\n", saved);
	FlxG_switchState((FlxState *)PlayState_new(saved));
	return true;
}

void Session_afterFrame(Session *s)
{
	if (FlxG.state == NULL || !as3_is((FlxBasic *)FlxG.state, &PlayState_class))
		return;
	const char *p = ((PlayState *)FlxG.state)->savedProgress;
	if (p != NULL && strcmp(p, s->lastSaved) != 0) {
		snprintf(s->lastSaved, sizeof s->lastSaved, "%s", p);
		bool ok = write_atomic(s, "progress.sav", p);
		logf_(s, ok ? "progress.sav <- %s\n" : "progress.sav write FAILED: %s\n", p);
	}
}

static void new_game(Session *s)
{
	char p[320];
	path_of(s, p, sizeof p, "progress.sav");
	remove(p);
	strcat(p, ".bak");
	remove(p);
	s->lastSaved[0] = 0;
	logf_(s, "%s\n", "new game");
	FlxG_switchState((FlxState *)PlayState_new(NULL));
}

void Session_menuInput(Session *s, bool up, bool down, bool left, bool right, bool a)
{
	if (up)
		s->menu = (s->menu + MENU_COUNT - 1) % MENU_COUNT;
	if (down)
		s->menu = (s->menu + 1) % MENU_COUNT;
	if (s->menu == MENU_STEREO && (left || right)) {
		s->settings.stereo += right ? 25 : -25;
		s->settings.stereo = s->settings.stereo < 0 ? 0 : s->settings.stereo > 200 ? 200 : s->settings.stereo;
		Session_saveSettings(s);
	}
	if (s->menu == MENU_WATER && (left || right || a)) {
		s->settings.water3d = (s->settings.water3d + (left ? 2 : 1)) % 3;
		Session_saveSettings(s);
		return;
	}
	if (s->menu == MENU_VIEW && (left || right || a)) {
		s->settings.fill = !s->settings.fill;
		Session_saveSettings(s);
		return;
	}
	if (s->menu == MENU_MUSIC && (left || right || a)) {
		s->settings.music = !s->settings.music;
		FlashMediaBackend_musicEnabled = s->settings.music;
		FlashMediaBackend_refreshMix();
		Session_saveSettings(s);
		return;
	}
	if (!a)
		return;
	switch (s->menu) {
	case MENU_RESUME:
		s->paused = false;
		break;
	case MENU_NEW_GAME:
		new_game(s);
		s->paused = false;
		break;
	case MENU_EXIT:
		s->exitRequested = true;
		break;
	}
}

static void text_at(BitmapData *dst, const char *text, int x, int y, int w, uint32_t color, const char *align, int size)
{
	static TextField *tf;
	if (tf == NULL)
		tf = TextField_new();
	tf->width = w;
	tf->height = 20;
	tf->wordWrap = false;
	TextField_setTextFormat(tf, TextFormat_new("system", size, color, align));
	TextField_set_text(tf, text);
	Matrix m = Matrix_new();
	Matrix_translate(&m, x, y);
	BitmapData_drawTextField(dst, tf, &m, NULL);
}

void Session_drawMenu(Session *s, BitmapData *dst)
{
	Rectangle all = Rectangle_new(0, 0, dst->width, dst->height);
	BitmapData_fillRect(dst, all, 0xFF141518);
	int w = dst->width;
	text_at(dst, "PAUSED", 0, 28, w, 0xFFFFFF, "center", 16);
	char items[MENU_COUNT][48];
	snprintf(items[MENU_RESUME], 48, "Resume");
	snprintf(items[MENU_MUSIC], 48, "Music: %s", s->settings.music ? "on" : "off");
	snprintf(items[MENU_STEREO], 48, "3D depth: < %d%% >", s->settings.stereo);
	snprintf(items[MENU_WATER], 48, "3D water: %s", s->settings.water3d == 0 ? "mirror" : s->settings.water3d == 1 ? "soft tilt" : "tilt");
	snprintf(items[MENU_VIEW], 48, "Top screen: %s", s->settings.fill ? "fill (1.5x)" : "whole frame");
	snprintf(items[MENU_NEW_GAME], 48, "New game (erases save)");
	snprintf(items[MENU_EXIT], 48, "Save and exit");
	for (int i = 0; i < MENU_COUNT; i++) {
		int y = 70 + i * 20;
		if (i == s->menu) {
			Rectangle r = Rectangle_new(40, y - 3, w - 80, 18);
			BitmapData_fillRect(dst, r, 0xFF3A3D44);
		}
		text_at(dst, items[i], 0, y, w, i == s->menu ? 0xFFE9C46A : 0xBBBBBB, "center", 8);
	}
	char line[300];
	snprintf(line, sizeof line, "checkpoint: %s", s->lastSaved[0] ? s->lastSaved : "none yet (saved at daybreak)");
	text_at(dst, line, 0, dst->height - 30, w, 0x888888, "center", 8);
	text_at(dst, "Up/Down select  A choose  START resume", 0, dst->height - 16, w, 0x888888, "center", 8);
}

void Session_close(Session *s)
{
	Session_saveSettings(s);
	if (s->log)
		fclose(s->log);
	s->log = NULL;
}
