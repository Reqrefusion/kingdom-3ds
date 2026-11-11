#ifndef TRANSLIT_FLXTILEMAP_H
#define TRANSLIT_FLXTILEMAP_H
#include "flixel.h"

#define FlxTilemap_OFF 0
#define FlxTilemap_AUTO 1
#define FlxTilemap_ALT 2

typedef struct FlxTilemapBuffer {
	Number x, y, width, height;
	bool dirty;
	uint32_t rows, columns;
	BitmapData *_pixels;
	Rectangle _flashRect;
} FlxTilemapBuffer;

void FlxTilemapBuffer_destroy(FlxTilemapBuffer *self);

typedef struct FlxTile {
	FLXOBJECT_FIELDS
	struct FlxTilemap *tilemap;
	uint32_t index;
	uint32_t mapIndex;
	As3Callback2 callback; /* tile callback(tile, object) */
	const As3Class *filter; /* only objects of this class */
} FlxTile;

typedef struct FlxTilemap {
	FLXOBJECT_FIELDS
	uint32_t auto_; /* `auto` is a C keyword */
	uint32_t widthInTiles;
	uint32_t heightInTiles;
	uint32_t totalTiles;
	Point _flashPoint;
	Rectangle *_flashRect;
	BitmapData *_tiles;
	As3Array _buffers;
	uint32_t *_data;
	Rectangle **_rects;
	uint32_t _tileWidth;
	uint32_t _tileHeight;
	FlxTile **_tileObjects;
	uint32_t _tileObjectCount;
	bool _lastVisualDebug;
	uint32_t _startingIndex;
} FlxTilemap;

extern const As3Class FlxTilemap_class;
FlxTilemapBuffer *FlxTilemapBuffer_new(Number TileWidth, Number TileHeight, uint32_t WidthInTiles, uint32_t HeightInTiles, FlxCamera *Camera);
void FlxTilemapBuffer_fill(FlxTilemapBuffer *self, uint32_t Color);
BitmapData *FlxTilemapBuffer_get_pixels(FlxTilemapBuffer *self);
void FlxTilemapBuffer_draw(FlxTilemapBuffer *self, FlxCamera *Camera, Point FlashPoint);
FlxTilemap *FlxTilemap_new(void);
FlxTilemap *FlxTilemap_loadMap(FlxTilemap *self, const char *MapData, const char *TileGraphic, uint32_t TileWidth, uint32_t TileHeight, uint32_t AutoTile, uint32_t StartingIndex, uint32_t DrawIndex, uint32_t CollideIndex);
void FlxTilemap_update(FlxTilemap *self);
void FlxTilemap_drawTilemap(FlxTilemap *self, FlxTilemapBuffer *Buffer, FlxCamera *Camera);
void FlxTilemap_draw(FlxTilemap *self);
uint32_t FlxTilemap_getTile(FlxTilemap *self, uint32_t X, uint32_t Y);
void FlxTilemap_updateTile(FlxTilemap *self, uint32_t Index);
/* members the game never calls (translated for function parity) */
void FlxTilemap_destroy(FlxTilemap *self);
void FlxTilemap_follow(FlxTilemap *self, FlxCamera *Camera, int32_t Border, bool UpdateWorld);
As3Array *FlxTilemap_getTileInstances(FlxTilemap *self, uint32_t Index);
As3Array *FlxTilemap_getTileCoords(FlxTilemap *self, uint32_t Index, bool Midpoint);
FlxRect *FlxTilemap_getBounds(FlxTilemap *self, FlxRect *Bounds);
uint32_t *FlxTilemap_getData(FlxTilemap *self, bool Simple);
uint32_t FlxTilemap_getTileByIndex(FlxTilemap *self, uint32_t Index);
void FlxTilemap_setDirty(FlxTilemap *self, bool Dirty);
void FlxTilemap_autoTile(FlxTilemap *self, uint32_t Index);
bool FlxTilemap_setTileByIndex(FlxTilemap *self, uint32_t Index, uint32_t Tile, bool UpdateGraphics);
bool FlxTilemap_setTile(FlxTilemap *self, uint32_t X, uint32_t Y, uint32_t Tile, bool UpdateGraphics);
void FlxTilemap_setTileProperties(FlxTilemap *self, uint32_t Tile, uint32_t AllowCollisions, As3Callback2 Callback, const As3Class *CallbackFilter, uint32_t Range);
bool FlxTilemap_overlapsWithCallback(FlxTilemap *self, FlxObject *Object, As3Callback2 Callback, bool FlipCallbackParams, const FlxPoint *Position);
bool FlxTilemap_overlaps(FlxTilemap *self, FlxBasic *ObjectOrGroup, bool InScreenSpace, FlxCamera *Camera);
bool FlxTilemap_overlapsAt(FlxTilemap *self, Number X, Number Y, FlxBasic *ObjectOrGroup, bool InScreenSpace, FlxCamera *Camera);
bool FlxTilemap_overlapsPoint(FlxTilemap *self, FlxPoint *Point, bool InScreenSpace, FlxCamera *Camera);
BitmapData *FlxTilemap_makeDebugTile(FlxTilemap *self, uint32_t Color);
char *FlxTilemap_arrayToCSV(const int32_t *Data, uint32_t DataLength, int32_t Width, bool Invert);
char *FlxTilemap_bitmapToCSV(BitmapData *bitmapData, bool Invert, uint32_t Scale, const uint32_t *ColorMap, uint32_t ColorMapLength);
char *FlxTilemap_imageToCSV(BitmapData *ImageFile, bool Invert, uint32_t Scale);

#endif
