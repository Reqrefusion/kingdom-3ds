/* org/flixel/FlxTilemap.as, system/FlxTilemapBuffer.as, system/FlxTile.as
 * Kingdom uses the tilemap only as drawn scenery and for getTile(); it is
 * never collided against. Translated: constructor, loadMap (no auto-tiling:
 * Kingdom passes OFF), update, drawTilemap, draw, getTile, updateTile.
 * Not translated: auto-tiling, collision/overlap/ray/path functions,
 * debug tiles. */
#include "FlxTilemap.h"

const As3Class FlxTilemap_class = {
	"FlxTilemap", &FlxObject_class, NULL,
	{
		FLXOBJECT_VT,
		.update = (void (*)(FlxBasic *))FlxTilemap_update,
		.draw = (void (*)(FlxBasic *))FlxTilemap_draw,
	},
};

/* ---- FlxTilemapBuffer ------------------------------------------------------- */

FlxTilemapBuffer *FlxTilemapBuffer_new(Number TileWidth, Number TileHeight, uint32_t WidthInTiles, uint32_t HeightInTiles, FlxCamera *Camera)
{
	FlxTilemapBuffer *self = as3_alloc(sizeof *self);
	if (Camera == NULL)
		Camera = FlxG.camera;

	self->columns = AS3_UINT(FlxU_ceil(Camera->width / TileWidth) + 1);
	if (self->columns > WidthInTiles)
		self->columns = WidthInTiles;
	self->rows = AS3_UINT(FlxU_ceil(Camera->height / TileHeight) + 1);
	if (self->rows > HeightInTiles)
		self->rows = HeightInTiles;

	self->_pixels = BitmapData_new(AS3_INT(self->columns * TileWidth), AS3_INT(self->rows * TileHeight), true, 0);
	self->width = self->_pixels->width;
	self->height = self->_pixels->height;
	self->_flashRect = Rectangle_new(0, 0, self->width, self->height);
	self->dirty = true;
	return self;
}

void FlxTilemapBuffer_fill(FlxTilemapBuffer *self, uint32_t Color)
{
	BitmapData_fillRect(self->_pixels, self->_flashRect, Color);
}

BitmapData *FlxTilemapBuffer_get_pixels(FlxTilemapBuffer *self)
{
	return self->_pixels;
}

void FlxTilemapBuffer_draw(FlxTilemapBuffer *self, FlxCamera *Camera, Point FlashPoint)
{
	BitmapData_copyPixels(Camera->buffer, self->_pixels, self->_flashRect, FlashPoint, true);
}

/* ---- FlxTile ----------------------------------------------------------------- */

static FlxTile *FlxTile_new(FlxTilemap *Tilemap, uint32_t Index, Number Width, Number Height, bool Visible, uint32_t AllowCollisions)
{
	FlxTile *self = as3_alloc(sizeof *self);
	self->klass = &FlxObject_class;
	SUPER(FlxObject_ctor((FlxObject *)self, 0, 0, Width, Height));
	self->immovable = true;
	self->moves = false;
	self->tilemap = Tilemap;
	self->index = Index;
	self->visible = Visible;
	self->allowCollisions = AllowCollisions;
	self->mapIndex = 0;
	return self;
}

/* ---- FlxTilemap ------------------------------------------------------------------ */

FlxTilemap *FlxTilemap_new(void)
{
	FlxTilemap *self = as3_alloc(sizeof *self);
	self->klass = &FlxTilemap_class;
	SUPER(FlxObject_ctor((FlxObject *)self, 0, 0, 0, 0));
	self->auto_ = FlxTilemap_OFF;
	self->widthInTiles = 0;
	self->heightInTiles = 0;
	self->totalTiles = 0;
	self->_buffers = (As3Array){0};
	self->_flashPoint = Point_new(0, 0);
	self->_flashRect = NULL;
	self->_data = NULL;
	self->_tileWidth = 0;
	self->_tileHeight = 0;
	self->_rects = NULL;
	self->_tiles = NULL;
	self->_tileObjects = NULL;
	self->immovable = true;
	self->cameras = NULL;
	self->_lastVisualDebug = FlxG.visualDebug;
	self->_startingIndex = 0;
	return self;
}

FlxTilemap *FlxTilemap_loadMap(FlxTilemap *self, const char *MapData, const char *TileGraphic, uint32_t TileWidth, uint32_t TileHeight, uint32_t AutoTile, uint32_t StartingIndex, uint32_t DrawIndex, uint32_t CollideIndex)
{
	self->auto_ = AutoTile;
	self->_startingIndex = StartingIndex;

	/* Figure out the map dimensions based on the data string */
	char *copy = strdup(MapData);
	char *rowsBuf[256];
	uint32_t rowCount = 0;
	for (char *row = copy, *next; row != NULL && rowCount < 256; row = next) {
		next = strchr(row, '\n');
		if (next != NULL)
			*next++ = 0;
		rowsBuf[rowCount++] = row;
	}
	self->heightInTiles = rowCount;
	self->_data = as3_alloc(sizeof(uint32_t) * 65536);
	uint32_t dataLength = 0;
	uint32_t row = 0;
	uint32_t column;
	while (row < self->heightInTiles) {
		/* columns = rows[row++].split(",") */
		char *columns[1024];
		uint32_t columnCount = 0;
		for (char *c = rowsBuf[row++], *next; c != NULL && columnCount < 1024; c = next) {
			next = strchr(c, ',');
			if (next != NULL)
				*next++ = 0;
			columns[columnCount++] = c;
		}
		if (columnCount <= 1) {
			self->heightInTiles = self->heightInTiles - 1;
			continue;
		}
		if (self->widthInTiles == 0)
			self->widthInTiles = columnCount;
		column = 0;
		while (column < self->widthInTiles)
			self->_data[dataLength++] = AS3_UINT(atof(columns[column++])); /* uint(String) */
	}
	free(copy);

	/* Pre-process the map data if it's auto-tiled: Kingdom uses OFF. */
	uint32_t i;
	self->totalTiles = self->widthInTiles * self->heightInTiles;

	/* Figure out the size of the tiles */
	self->_tiles = FlxG_addBitmap(TileGraphic, false, false, NULL);
	self->_tileWidth = TileWidth;
	if (self->_tileWidth == 0)
		self->_tileWidth = (uint32_t)self->_tiles->height;
	self->_tileHeight = TileHeight;
	if (self->_tileHeight == 0)
		self->_tileHeight = self->_tileWidth;

	/* create some tile objects that we'll use for overlap checks (one for each tile) */
	i = 0;
	uint32_t l = AS3_UINT(((Number)self->_tiles->width / self->_tileWidth) * ((Number)self->_tiles->height / self->_tileHeight));
	self->_tileObjects = as3_alloc(sizeof(FlxTile *) * (l + 1));
	self->_tileObjectCount = l;
	while (i < l) {
		self->_tileObjects[i] = FlxTile_new(self, i, self->_tileWidth, self->_tileHeight, (i >= DrawIndex), (i >= CollideIndex) ? self->allowCollisions : NONE);
		i++;
	}

	/* Then go through and create the actual map */
	self->width = self->widthInTiles * self->_tileWidth;
	self->height = self->heightInTiles * self->_tileHeight;
	self->_rects = as3_alloc(sizeof(Rectangle *) * (self->totalTiles + 1));
	i = 0;
	while (i < self->totalTiles)
		FlxTilemap_updateTile(self, i++);

	return self;
}

void FlxTilemap_update(FlxTilemap *self)
{
	if (self->_lastVisualDebug != FlxG.visualDebug) {
		self->_lastVisualDebug = FlxG.visualDebug;
		/* setDirty() */
		for (uint32_t i = 0; i < self->_buffers.length; i++)
			((FlxTilemapBuffer *)As3Array_get(&self->_buffers, i))->dirty = true;
	}
}

void FlxTilemap_drawTilemap(FlxTilemap *self, FlxTilemapBuffer *Buffer, FlxCamera *Camera)
{
	FlxTilemapBuffer_fill(Buffer, 0);

	/* Copy tile images into the tile buffer */
	self->_point.x = AS3_INT(Camera->scroll.x * self->scrollFactor.x) - self->x; /* modified from getScreenXY() */
	self->_point.y = AS3_INT(Camera->scroll.y * self->scrollFactor.y) - self->y;
	int32_t screenXInTiles = AS3_INT((self->_point.x + ((self->_point.x > 0) ? 0.0000001 : -0.0000001)) / self->_tileWidth);
	int32_t screenYInTiles = AS3_INT((self->_point.y + ((self->_point.y > 0) ? 0.0000001 : -0.0000001)) / self->_tileHeight);
	uint32_t screenRows = Buffer->rows;
	uint32_t screenColumns = Buffer->columns;

	/* Bound the upper left corner */
	if (screenXInTiles < 0)
		screenXInTiles = 0;
	if (screenXInTiles > (int32_t)(self->widthInTiles - screenColumns))
		screenXInTiles = (int32_t)(self->widthInTiles - screenColumns);
	if (screenYInTiles < 0)
		screenYInTiles = 0;
	if (screenYInTiles > (int32_t)(self->heightInTiles - screenRows))
		screenYInTiles = (int32_t)(self->heightInTiles - screenRows);

	int32_t rowIndex = screenYInTiles * (int32_t)self->widthInTiles + screenXInTiles;
	self->_flashPoint.y = 0;
	uint32_t row = 0;
	uint32_t column;
	uint32_t columnIndex;
	while (row < screenRows) {
		columnIndex = (uint32_t)rowIndex;
		column = 0;
		self->_flashPoint.x = 0;
		while (column < screenColumns) {
			self->_flashRect = self->_rects[columnIndex];
			if (self->_flashRect != NULL)
				BitmapData_copyPixels(FlxTilemapBuffer_get_pixels(Buffer), self->_tiles, *self->_flashRect, self->_flashPoint, true);
			/* (visual debug tiles omitted) */
			self->_flashPoint.x += self->_tileWidth;
			column++;
			columnIndex++;
		}
		rowIndex += (int32_t)self->widthInTiles;
		self->_flashPoint.y += self->_tileHeight;
		row++;
	}
	Buffer->x = screenXInTiles * (Number)self->_tileWidth;
	Buffer->y = screenYInTiles * (Number)self->_tileHeight;
}

void FlxTilemap_draw(FlxTilemap *self)
{
	if (self->_flickerTimer != 0) {
		self->_flicker = !self->_flicker;
		if (self->_flicker)
			return;
	}

	if (self->cameras == NULL)
		self->cameras = &FlxG.cameras;
	FlxCamera *camera;
	FlxTilemapBuffer *buffer;
	uint32_t i = 0;
	uint32_t l = self->cameras->length;
	while (i < l) {
		camera = As3Array_get(self->cameras, i);
		if (As3Array_get(&self->_buffers, i) == NULL)
			As3Array_set(&self->_buffers, i, FlxTilemapBuffer_new(self->_tileWidth, self->_tileHeight, self->widthInTiles, self->heightInTiles, camera));
		buffer = As3Array_get(&self->_buffers, i++);
		if (!buffer->dirty) {
			self->_point.x = self->x - AS3_INT(camera->scroll.x * self->scrollFactor.x) + buffer->x; /* copied from getScreenXY() */
			self->_point.y = self->y - AS3_INT(camera->scroll.y * self->scrollFactor.y) + buffer->y;
			buffer->dirty = (self->_point.x > 0) || (self->_point.y > 0) || (self->_point.x + buffer->width < camera->width) || (self->_point.y + buffer->height < camera->height);
		}
		if (buffer->dirty) {
			FlxTilemap_drawTilemap(self, buffer, camera);
			buffer->dirty = false;
		}
		self->_flashPoint.x = self->x - AS3_INT(camera->scroll.x * self->scrollFactor.x) + buffer->x; /* copied from getScreenXY() */
		self->_flashPoint.y = self->y - AS3_INT(camera->scroll.y * self->scrollFactor.y) + buffer->y;
		self->_flashPoint.x += (self->_flashPoint.x > 0) ? 0.0000001 : -0.0000001;
		self->_flashPoint.y += (self->_flashPoint.y > 0) ? 0.0000001 : -0.0000001;
		Stereo_dx = Stereo_depth ? Stereo_depth((FlxObject *)self) : 0; /* 3DS stereo adapter */
		FlxTilemapBuffer_draw(buffer, camera, self->_flashPoint);
		Stereo_dx = 0;
		FlxBasic__VISIBLECOUNT++;
	}
}

uint32_t FlxTilemap_getTile(FlxTilemap *self, uint32_t X, uint32_t Y)
{
	uint32_t index = Y * self->widthInTiles + X;
	/* _data[i] as uint: reading past the end gives undefined -> 0 */
	return index < self->totalTiles ? self->_data[index] : 0;
}

void FlxTilemap_updateTile(FlxTilemap *self, uint32_t Index)
{
	FlxTile *tile = self->_data[Index] < self->_tileObjectCount ? self->_tileObjects[self->_data[Index]] : NULL;
	if ((tile == NULL) || !tile->visible) {
		self->_rects[Index] = NULL;
		return;
	}
	uint32_t rx = (self->_data[Index] - self->_startingIndex) * self->_tileWidth;
	uint32_t ry = 0;
	if (rx >= (uint32_t)self->_tiles->width) {
		ry = AS3_UINT(rx / (uint32_t)self->_tiles->width) * self->_tileHeight;
		rx %= (uint32_t)self->_tiles->width;
	}
	Rectangle *r = as3_alloc(sizeof *r);
	*r = Rectangle_new(rx, ry, self->_tileWidth, self->_tileHeight);
	self->_rects[Index] = r;
}

void FlxTilemapBuffer_destroy(FlxTilemapBuffer *self)
{
	self->_pixels = NULL;
}

/* ---- members the game never calls (Kingdom only builds and reads its floor) ---- */

void FlxTilemap_destroy(FlxTilemap *self)
{
	self->_flashRect = NULL;
	self->_tiles = NULL;
	uint32_t i = 0;
	uint32_t l = self->_tileObjectCount;
	while (i < l)
		CALL0(self->_tileObjects[i++], destroy);
	self->_tileObjects = NULL;
	i = 0;
	l = self->_buffers.length;
	while (i < l)
		FlxTilemapBuffer_destroy(As3Array_get(&self->_buffers, i++));
	As3Array_set_length(&self->_buffers, 0);
	self->_data = NULL;
	self->_rects = NULL;
	SUPER(FlxObject_destroy((FlxObject *)self));
}

void FlxTilemap_follow(FlxTilemap *self, FlxCamera *Camera, int32_t Border, bool UpdateWorld)
{
	if (Camera == NULL)
		Camera = FlxG.camera;
	FlxCamera_setBounds(Camera, self->x + Border * self->_tileWidth, self->y + Border * self->_tileHeight,
		self->width - Border * self->_tileWidth * 2, self->height - Border * self->_tileHeight * 2, UpdateWorld);
}

As3Array *FlxTilemap_getTileInstances(FlxTilemap *self, uint32_t Index)
{
	As3Array *array = NULL;
	uint32_t i = 0;
	uint32_t l = self->widthInTiles * self->heightInTiles;
	while (i < l) {
		if (self->_data[i] == Index) {
			if (array == NULL)
				array = as3_alloc(sizeof *array);
			As3Array_push(array, AS3_BOX(i));
		}
		i++;
	}
	return array;
}

As3Array *FlxTilemap_getTileCoords(FlxTilemap *self, uint32_t Index, bool Midpoint)
{
	As3Array *array = NULL;
	FlxPoint *point;
	uint32_t i = 0;
	uint32_t l = self->widthInTiles * self->heightInTiles;
	while (i < l) {
		if (self->_data[i] == Index) {
			point = FlxPoint_alloc(self->x + AS3_UINT(i % self->widthInTiles) * self->_tileWidth, self->y + AS3_UINT(i / self->widthInTiles) * self->_tileHeight);
			if (Midpoint) {
				point->x += self->_tileWidth * 0.5;
				point->y += self->_tileHeight * 0.5;
			}
			if (array == NULL)
				array = as3_alloc(sizeof *array);
			As3Array_push(array, point);
		}
		i++;
	}
	return array;
}

FlxRect *FlxTilemap_getBounds(FlxTilemap *self, FlxRect *Bounds)
{
	if (Bounds == NULL)
		Bounds = FlxRect_alloc(0, 0, 0, 0);
	return FlxRect_make(Bounds, self->x, self->y, self->width, self->height);
}

/* Simple = false returns the tile index array itself (uint32_t[]); Simple = true a new 0/1
   array (collidable or not), both widthInTiles * heightInTiles long */
uint32_t *FlxTilemap_getData(FlxTilemap *self, bool Simple)
{
	if (!Simple)
		return self->_data;
	uint32_t i = 0;
	uint32_t l = self->widthInTiles * self->heightInTiles;
	uint32_t *data = as3_alloc(l * sizeof *data);
	while (i < l) {
		data[i] = (self->_tileObjects[self->_data[i]]->allowCollisions > 0) ? 1 : 0;
		i++;
	}
	return data;
}

uint32_t FlxTilemap_getTileByIndex(FlxTilemap *self, uint32_t Index)
{
	return self->_data[Index];
}

void FlxTilemap_setDirty(FlxTilemap *self, bool Dirty)
{
	uint32_t i = 0;
	uint32_t l = self->_buffers.length;
	while (i < l)
		((FlxTilemapBuffer *)As3Array_get(&self->_buffers, i++))->dirty = Dirty;
}

void FlxTilemap_autoTile(FlxTilemap *self, uint32_t Index)
{
	if (self->_data[Index] == 0)
		return;
	self->_data[Index] = 0;
	if ((Index - self->widthInTiles < 0) || (self->_data[Index - self->widthInTiles] > 0))
		self->_data[Index] += 1;
	if ((Index % self->widthInTiles >= self->widthInTiles - 1) || (self->_data[Index + 1] > 0))
		self->_data[Index] += 2;
	if ((Index + self->widthInTiles >= self->totalTiles) || (self->_data[Index + self->widthInTiles] > 0))
		self->_data[Index] += 4;
	if ((Index % self->widthInTiles <= 0) || (self->_data[Index - 1] > 0))
		self->_data[Index] += 8;
	if ((self->auto_ == FlxTilemap_ALT) && (self->_data[Index] == 15)) {
		if ((Index % self->widthInTiles > 0) && (Index + self->widthInTiles < self->totalTiles) && (self->_data[Index + self->widthInTiles - 1] <= 0))
			self->_data[Index] = 1;
		if ((Index % self->widthInTiles > 0) && (Index - self->widthInTiles >= 0) && (self->_data[Index - self->widthInTiles - 1] <= 0))
			self->_data[Index] = 2;
		if ((Index % self->widthInTiles < self->widthInTiles - 1) && (Index - self->widthInTiles >= 0) && (self->_data[Index - self->widthInTiles + 1] <= 0))
			self->_data[Index] = 4;
		if ((Index % self->widthInTiles < self->widthInTiles - 1) && (Index + self->widthInTiles < self->totalTiles) && (self->_data[Index + self->widthInTiles + 1] <= 0))
			self->_data[Index] = 8;
	}
	self->_data[Index] += 1;
}

bool FlxTilemap_setTileByIndex(FlxTilemap *self, uint32_t Index, uint32_t Tile, bool UpdateGraphics)
{
	if (Index >= self->widthInTiles * self->heightInTiles)
		return false;
	bool ok = true;
	self->_data[Index] = Tile;
	if (!UpdateGraphics)
		return ok;
	FlxTilemap_setDirty(self, true);
	if (self->auto_ == FlxTilemap_OFF) {
		FlxTilemap_updateTile(self, Index);
		return ok;
	}
	uint32_t i;
	int32_t row = AS3_INT(Index / self->widthInTiles) - 1;
	int32_t rowLength = row + 3;
	int32_t column = AS3_INT(Index % self->widthInTiles) - 1;
	int32_t columnHeight = column + 3;
	while (row < rowLength) {
		column = columnHeight - 3;
		while (column < columnHeight) {
			if ((row >= 0) && (row < (int32_t)self->heightInTiles) && (column >= 0) && (column < (int32_t)self->widthInTiles)) {
				i = row * self->widthInTiles + column;
				FlxTilemap_autoTile(self, i);
				FlxTilemap_updateTile(self, i);
			}
			column++;
		}
		row++;
	}
	return ok;
}

bool FlxTilemap_setTile(FlxTilemap *self, uint32_t X, uint32_t Y, uint32_t Tile, bool UpdateGraphics)
{
	if ((X >= self->widthInTiles) || (Y >= self->heightInTiles))
		return false;
	return FlxTilemap_setTileByIndex(self, Y * self->widthInTiles + X, Tile, UpdateGraphics);
}

void FlxTilemap_setTileProperties(FlxTilemap *self, uint32_t Tile, uint32_t AllowCollisions, As3Callback2 Callback, const As3Class *CallbackFilter, uint32_t Range)
{
	if (Range <= 0)
		Range = 1;
	FlxTile *tile;
	uint32_t i = Tile;
	uint32_t l = Tile + Range;
	while (i < l) {
		tile = self->_tileObjects[i++];
		tile->allowCollisions = AllowCollisions;
		tile->callback = Callback;
		tile->filter = CallbackFilter;
	}
}

bool FlxTilemap_overlapsWithCallback(FlxTilemap *self, FlxObject *Object, As3Callback2 Callback, bool FlipCallbackParams, const FlxPoint *Position)
{
	bool results = false;
	Number X = self->x;
	Number Y = self->y;
	if (Position != NULL) {
		X = Position->x;
		Y = Position->y;
	}

	/* Figure out what tiles we need to check against */
	int32_t selectionX = AS3_INT(FlxU_floor((Object->x - X) / self->_tileWidth));
	int32_t selectionY = AS3_INT(FlxU_floor((Object->y - Y) / self->_tileHeight));
	uint32_t selectionWidth = AS3_UINT(selectionX + (FlxU_ceil(Object->width / self->_tileWidth)) + 1);
	uint32_t selectionHeight = AS3_UINT(selectionY + FlxU_ceil(Object->height / self->_tileHeight) + 1);

	/* Then bound these coordinates by the map edges */
	if (selectionX < 0)
		selectionX = 0;
	if (selectionY < 0)
		selectionY = 0;
	if (selectionWidth > self->widthInTiles)
		selectionWidth = self->widthInTiles;
	if (selectionHeight > self->heightInTiles)
		selectionHeight = self->heightInTiles;

	/* Then loop through this selection of tiles and call FlxObject.separate() accordingly */
	uint32_t rowStart = selectionY * self->widthInTiles;
	uint32_t row = selectionY;
	uint32_t column;
	FlxTile *tile;
	bool overlapFound;
	Number deltaX = X - self->last.x;
	Number deltaY = Y - self->last.y;
	while (row < selectionHeight) {
		column = selectionX;
		while (column < selectionWidth) {
			overlapFound = false;
			tile = self->_tileObjects[self->_data[rowStart + column]];
			if (tile->allowCollisions) {
				tile->x = X + column * self->_tileWidth;
				tile->y = Y + row * self->_tileHeight;
				tile->last.x = tile->x - deltaX;
				tile->last.y = tile->y - deltaY;
				if (Callback.fn != NULL) {
					if (FlipCallbackParams)
						overlapFound = Callback.fn(Callback.self, Object, (FlxObject *)tile);
					else
						overlapFound = Callback.fn(Callback.self, (FlxObject *)tile, Object);
				} else
					overlapFound = (Object->x + Object->width > tile->x) && (Object->x < tile->x + tile->width) && (Object->y + Object->height > tile->y) && (Object->y < tile->y + tile->height);
				if (overlapFound) {
					if ((tile->callback.fn != NULL) && ((tile->filter == NULL) || as3_is(Object, tile->filter))) {
						tile->mapIndex = rowStart + column;
						tile->callback.fn(tile->callback.self, (FlxObject *)tile, Object);
					}
					results = true;
				}
			} else if ((tile->callback.fn != NULL) && ((tile->filter == NULL) || as3_is(Object, tile->filter))) {
				tile->mapIndex = rowStart + column;
				tile->callback.fn(tile->callback.self, (FlxObject *)tile, Object);
			}
			column++;
		}
		rowStart += self->widthInTiles;
		row++;
	}
	return results;
}

bool FlxTilemap_overlaps(FlxTilemap *self, FlxBasic *ObjectOrGroup, bool InScreenSpace, FlxCamera *Camera)
{
	if (as3_is(ObjectOrGroup, &FlxGroup_class)) {
		bool results = false;
		FlxBasic *basic;
		uint32_t i = 0;
		As3Array *members = &((FlxGroup *)ObjectOrGroup)->members;
		/* Source bug kept: the loop bound is an unqualified `length` (undefined in FlxTilemap), so the loop never runs */
		Number length = NAN;
		while (i < length) {
			basic = As3Array_get(members, i++);
			if (as3_is(basic, &FlxObject_class)) {
				if (FlxTilemap_overlapsWithCallback(self, (FlxObject *)basic, (As3Callback2)AS3_NULL_FN, false, NULL))
					results = true;
			} else {
				if (FlxTilemap_overlaps(self, basic, InScreenSpace, Camera))
					results = true;
			}
		}
		return results;
	} else if (as3_is(ObjectOrGroup, &FlxObject_class))
		return FlxTilemap_overlapsWithCallback(self, (FlxObject *)ObjectOrGroup, (As3Callback2)AS3_NULL_FN, false, NULL);
	return false;
}

bool FlxTilemap_overlapsAt(FlxTilemap *self, Number X, Number Y, FlxBasic *ObjectOrGroup, bool InScreenSpace, FlxCamera *Camera)
{
	if (as3_is(ObjectOrGroup, &FlxGroup_class)) {
		bool results = false;
		FlxBasic *basic;
		uint32_t i = 0;
		As3Array *members = &((FlxGroup *)ObjectOrGroup)->members;
		/* Source bug kept: the loop bound is an unqualified `length` (undefined in FlxTilemap), so the loop never runs */
		Number length = NAN;
		while (i < length) {
			basic = As3Array_get(members, i++);
			if (as3_is(basic, &FlxObject_class)) {
				self->_point.x = X;
				self->_point.y = Y;
				if (FlxTilemap_overlapsWithCallback(self, (FlxObject *)basic, (As3Callback2)AS3_NULL_FN, false, &self->_point))
					results = true;
			} else {
				if (FlxTilemap_overlapsAt(self, X, Y, basic, InScreenSpace, Camera))
					results = true;
			}
		}
		return results;
	} else if (as3_is(ObjectOrGroup, &FlxObject_class)) {
		self->_point.x = X;
		self->_point.y = Y;
		return FlxTilemap_overlapsWithCallback(self, (FlxObject *)ObjectOrGroup, (As3Callback2)AS3_NULL_FN, false, &self->_point);
	}
	return false;
}

bool FlxTilemap_overlapsPoint(FlxTilemap *self, FlxPoint *Point, bool InScreenSpace, FlxCamera *Camera)
{
	if (!InScreenSpace)
		return self->_tileObjects[self->_data[AS3_UINT(AS3_UINT((Point->y - self->y) / self->_tileHeight) * self->widthInTiles + (Point->x - self->x) / self->_tileWidth)]]->allowCollisions > 0;

	if (Camera == NULL)
		Camera = FlxG.camera;
	Point->x = Point->x - Camera->scroll.x;
	Point->y = Point->y - Camera->scroll.y;
	FlxObject_getScreenXY((FlxObject *)self, &self->_point, Camera);
	return self->_tileObjects[self->_data[AS3_UINT(AS3_UINT((Point->y - self->_point.y) / self->_tileHeight) * self->widthInTiles + (Point->x - self->_point.x) / self->_tileWidth)]]->allowCollisions > 0;
}

/* ADAPTATION: the Flash Graphics rectangle outline is drawn as pixels (50 % alpha, 1 px) */
BitmapData *FlxTilemap_makeDebugTile(FlxTilemap *self, uint32_t Color)
{
	BitmapData *debugTile = BitmapData_new(self->_tileWidth, self->_tileHeight, true, 0);
	uint32_t outline = (0x7Fu << 24) | (Color & 0x00FFFFFFu);
	for (uint32_t x = 0; x < self->_tileWidth; x++) {
		BitmapData_setPixel32(debugTile, (int32_t)x, 0, outline);
		BitmapData_setPixel32(debugTile, (int32_t)x, (int32_t)self->_tileHeight - 1, outline);
	}
	for (uint32_t y = 0; y < self->_tileHeight; y++) {
		BitmapData_setPixel32(debugTile, 0, (int32_t)y, outline);
		BitmapData_setPixel32(debugTile, (int32_t)self->_tileWidth - 1, (int32_t)y, outline);
	}
	return debugTile;
}

char *FlxTilemap_arrayToCSV(const int32_t *Data, uint32_t DataLength, int32_t Width, bool Invert)
{
	uint32_t row = 0;
	int32_t column;
	char *csv = "";
	int32_t Height = (int32_t)(DataLength / Width);
	int32_t index;
	while (row < (uint32_t)Height) {
		column = 0;
		while (column < Width) {
			index = Data[row * Width + column];
			if (Invert) {
				if (index == 0)
					index = 1;
				else if (index == 1)
					index = 0;
			}
			if (column == 0) {
				if (row == 0)
					csv = AS3_CAT(csv, AS3_STR(index));
				else
					csv = AS3_CAT(csv, "\n", AS3_STR(index));
			} else
				csv = AS3_CAT(csv, ", ", AS3_STR(index));
			column++;
		}
		row++;
	}
	return csv;
}

char *FlxTilemap_bitmapToCSV(BitmapData *bitmapData, bool Invert, uint32_t Scale, const uint32_t *ColorMap, uint32_t ColorMapLength)
{
	if (Scale > 1) {
		BitmapData *bd = bitmapData;
		bitmapData = BitmapData_new(bitmapData->width * Scale, bitmapData->height * Scale, true, 0xFFFFFFFFu);
		Matrix mtx = Matrix_new();
		Matrix_scale(&mtx, Scale, Scale);
		BitmapData_draw(bitmapData, bd, &mtx, NULL, NULL, NULL, false);
	}
	uint32_t row = 0;
	uint32_t column;
	uint32_t pixel;
	char *csv = "";
	uint32_t bitmapWidth = bitmapData->width;
	uint32_t bitmapHeight = bitmapData->height;
	while (row < bitmapHeight) {
		column = 0;
		while (column < bitmapWidth) {
			pixel = BitmapData_getPixel(bitmapData, column, row);
			if (ColorMap != NULL) {
				uint32_t found = (uint32_t)-1; /* Array.indexOf: -1 when absent */
				for (uint32_t k = 0; k < ColorMapLength; k++)
					if (ColorMap[k] == pixel) {
						found = k;
						break;
					}
				pixel = found;
			} else if ((Invert && (pixel > 0)) || (!Invert && (pixel == 0)))
				pixel = 1;
			else
				pixel = 0;
			if (column == 0) {
				if (row == 0)
					csv = AS3_CAT(csv, AS3_STR((int32_t)pixel));
				else
					csv = AS3_CAT(csv, "\n", AS3_STR((int32_t)pixel));
			} else
				csv = AS3_CAT(csv, ", ", AS3_STR((int32_t)pixel));
			column++;
		}
		row++;
	}
	return csv;
}

char *FlxTilemap_imageToCSV(BitmapData *ImageFile, bool Invert, uint32_t Scale)
{
	return FlxTilemap_bitmapToCSV(ImageFile, Invert, Scale, NULL, 0);
}
