/* Platform session layer shared by the 3DS host and the host runner.
 *
 * Everything here is outside the transliterated game: it plays the role of
 * the Flash Player / browser around the SWF (persistence, pause, settings).
 * Ported from the previous native prototype (../source/main.c):
 *   - settings.cfg (music on/off, 3D depth, top-screen scale)
 *   - Start pause menu with Resume / Music / New game / Exit
 *   - progress kept across launches
 *
 * Progress uses the game's own checkpoint: PlayState.saveProgress() builds
 * the source string ("D3 A12 X1713 ...") at daybreak and keeps it in
 * PlayState.savedProgress; the original only uses it for the crown-loss retry
 * (new PlayState(savedProgress)). The session writes that same string to
 * progress.sav when it changes, and a later launch starts with
 * new PlayState(saved) -- PlayState.setProgress restores it exactly as the
 * retry path does. No game code is changed.
 */
#ifndef TRANSLIT_SESSION_H
#define TRANSLIT_SESSION_H
#include "../../game/game.h"

typedef struct Settings {
	bool music;
	int stereo; /* 3D depth, percent 0..200 (100 = 5 px at full slider) */
	bool fill;  /* top screen: false = whole frame (1.39x), true = 1.5x filling the screen */
	int water3d; /* 3D water: 0 mirror (reflection keeps its objects' depth), 1 soft tilt, 2 tilt (default) */
} Settings;

enum { MENU_RESUME, MENU_MUSIC, MENU_STEREO, MENU_WATER, MENU_VIEW, MENU_NEW_GAME, MENU_EXIT, MENU_COUNT };

typedef struct Session {
	char dir[256];
	Settings settings;
	bool paused;
	int menu;
	char lastSaved[256];
	bool exitRequested;
	FILE *log;
} Session;

void Session_init(Session *s, const char *dir);
void Session_saveSettings(Session *s);
/* progress string saved by an earlier launch, or NULL */
const char *Session_savedProgress(Session *s);
/* Call once after the first FlxGame frame (MenuState exists): resumes a saved
   game by switching to new PlayState(saved). Returns true when resumed. */
bool Session_resume(Session *s);
/* Call after every frame: persists PlayState.savedProgress when it changed. */
void Session_afterFrame(Session *s);
/* Pause menu input: up/down select, left/right change, a = activate. */
void Session_menuInput(Session *s, bool up, bool down, bool left, bool right, bool a);
/* Draws the pause menu into dst (any size). */
void Session_drawMenu(Session *s, BitmapData *dst);
void Session_close(Session *s);

/* music on/off for the audio backend (sounds under /assets/music/) */
extern bool FlashMediaBackend_musicEnabled;
void FlashMediaBackend_refreshMix(void);

#endif
