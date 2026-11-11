/* org/flixel/FlxU.as, FlxPoint.as, FlxRect.as, system/FlxAnim.as, system/FlxList.as */
#include "flixel.h"

/* ---- FlxU ---------------------------------------------------------------- */

Number FlxU_abs(Number Value)
{
	return (Value > 0) ? Value : -Value;
}

Number FlxU_floor(Number Value)
{
	Number number = AS3_INT(Value);
	return (Value > 0) ? (number) : ((number != Value) ? (number - 1) : (number));
}

Number FlxU_ceil(Number Value)
{
	Number number = AS3_INT(Value);
	return (Value > 0) ? ((number != Value) ? (number + 1) : (number)) : (number);
}

Number FlxU_round(Number Value)
{
	Number number = AS3_INT(Value + 0.5);
	return (Value > 0) ? (number) : ((number != Value) ? (number - 1) : (number));
}

Number FlxU_min(Number Number1, Number Number2)
{
	return (Number1 <= Number2) ? Number1 : Number2;
}

Number FlxU_max(Number Number1, Number Number2)
{
	return (Number1 >= Number2) ? Number1 : Number2;
}

Number FlxU_bound(Number Value, Number Min, Number Max)
{
	Number lowerBound = (Value < Min) ? Min : Value;
	return (lowerBound > Max) ? Max : lowerBound;
}

Number FlxU_srand(Number Seed)
{
	return as3_mod(69621 * (Number)AS3_INT(Seed * 0x7FFFFFFF), 0x7FFFFFFF) / 0x7FFFFFFF;
}

Number FlxU_computeVelocity(Number Velocity, Number Acceleration, Number Drag, Number Max)
{
	if (Acceleration != 0)
		Velocity += Acceleration * FlxG.elapsed;
	else if (Drag != 0) {
		Number drag = Drag * FlxG.elapsed;
		if (Velocity - drag > 0)
			Velocity = Velocity - drag;
		else if (Velocity + drag < 0)
			Velocity += drag;
		else
			Velocity = 0;
	}
	if ((Velocity != 0) && (Max != 10000)) {
		if (Velocity > Max)
			Velocity = Max;
		else if (Velocity < -Max)
			Velocity = -Max;
	}
	return Velocity;
}

FlxPoint *FlxU_rotatePoint(Number X, Number Y, Number PivotX, Number PivotY, Number Angle, FlxPoint *Point)
{
	Number sin = 0;
	Number cos = 0;
	Number radians = Angle * -0.017453293;
	while (radians < -3.14159265)
		radians += 6.28318531;
	while (radians > 3.14159265)
		radians = radians - 6.28318531;

	if (radians < 0) {
		sin = 1.27323954 * radians + .405284735 * radians * radians;
		if (sin < 0)
			sin = .225 * (sin * -sin - sin) + sin;
		else
			sin = .225 * (sin * sin - sin) + sin;
	} else {
		sin = 1.27323954 * radians - 0.405284735 * radians * radians;
		if (sin < 0)
			sin = .225 * (sin * -sin - sin) + sin;
		else
			sin = .225 * (sin * sin - sin) + sin;
	}

	radians += 1.57079632;
	if (radians > 3.14159265)
		radians = radians - 6.28318531;
	if (radians < 0) {
		cos = 1.27323954 * radians + 0.405284735 * radians * radians;
		if (cos < 0)
			cos = .225 * (cos * -cos - cos) + cos;
		else
			cos = .225 * (cos * cos - cos) + cos;
	} else {
		cos = 1.27323954 * radians - 0.405284735 * radians * radians;
		if (cos < 0)
			cos = .225 * (cos * -cos - cos) + cos;
		else
			cos = .225 * (cos * cos - cos) + cos;
	}

	Number dx = X - PivotX;
	Number dy = PivotY + Y; /* Y axis is inverted in flash */
	if (Point == NULL)
		Point = FlxPoint_alloc(0, 0);
	Point->x = PivotX + cos * dx - sin * dy;
	Point->y = PivotY - sin * dx - cos * dy;
	return Point;
}

Number FlxU_getAngle(FlxPoint *Point1, FlxPoint *Point2)
{
	Number x = Point2->x - Point1->x;
	Number y = Point2->y - Point1->y;
	if ((x == 0) && (y == 0))
		return 0;
	Number c1 = 3.14159265 * 0.25;
	Number c2 = 3 * c1;
	Number ay = (y < 0) ? -y : y;
	Number angle = 0;
	if (x >= 0)
		angle = c1 - c1 * ((x - ay) / (x + ay));
	else
		angle = c2 - c1 * ((x + ay) / (ay - x));
	angle = ((y < 0) ? -angle : angle) * 57.2957796;
	if (angle > 90)
		angle = angle - 270;
	else
		angle += 90;
	return angle;
}

Number FlxU_getDistance(FlxPoint *Point1, FlxPoint *Point2)
{
	Number dx = Point1->x - Point2->x;
	Number dy = Point1->y - Point2->y;
	return Math_sqrt(dx * dx + dy * dy);
}

Number *FlxU_getRGBA(uint32_t Color, Number *Results)
{
	if (Results == NULL)
		Results = as3_alloc(4 * sizeof(Number));
	Results[0] = AS3_SAR(Color, 16) & 0xFF;
	Results[1] = AS3_SAR(Color, 8) & 0xFF;
	Results[2] = Color & 0xFF;
	Results[3] = (Number)(AS3_SAR(Color, 24) & 0xFF) / 255;
	return Results;
}

Number *FlxU_getHSB(uint32_t Color, Number *Results)
{
	if (Results == NULL)
		Results = as3_alloc(4 * sizeof(Number));

	Number red = (Number)(AS3_SAR(Color, 16) & 0xFF) / 255;
	Number green = (Number)(AS3_SAR(Color, 8) & 0xFF) / 255;
	Number blue = (Number)((Color) & 0xFF) / 255;

	Number m = (red > green) ? red : green;
	Number dmax = (m > blue) ? m : blue;
	m = (red > green) ? green : red;
	Number dmin = (m > blue) ? blue : m;
	Number range = dmax - dmin;

	Results[2] = dmax;
	Results[1] = 0;
	Results[0] = 0;

	if (dmax != 0)
		Results[1] = range / dmax;
	if (Results[1] != 0) {
		if (red == dmax)
			Results[0] = (green - blue) / range;
		else if (green == dmax)
			Results[0] = 2 + (blue - red) / range;
		else if (blue == dmax)
			Results[0] = 4 + (red - green) / range;
		Results[0] *= 60;
		if (Results[0] < 0)
			Results[0] += 360;
	}

	Results[3] = (Number)(AS3_SAR(Color, 24) & 0xFF) / 255;
	return Results;
}

uint32_t FlxU_makeColor(uint32_t Red, uint32_t Green, uint32_t Blue, Number Alpha)
{
	return (uint32_t)((AS3_INT((Alpha > 1) ? Alpha : (Alpha * 255)) & 0xFF) << 24 | (Red & 0xFF) << 16 | (Green & 0xFF) << 8 | (Blue & 0xFF));
}

uint32_t FlxU_makeColorFromHSB(Number Hue, Number Saturation, Number Brightness, Number Alpha)
{
	Number red;
	Number green;
	Number blue;
	if (Saturation == 0.0) {
		red = Brightness;
		green = Brightness;
		blue = Brightness;
	} else {
		if (Hue == 360)
			Hue = 0;
		int32_t slice = AS3_INT(Hue / 60);
		Number hf = Hue / 60 - slice;
		Number aa = Brightness * (1 - Saturation);
		Number bb = Brightness * (1 - Saturation * hf);
		Number cc = Brightness * (1 - Saturation * (1.0 - hf));
		switch (slice) {
		case 0: red = Brightness; green = cc; blue = aa; break;
		case 1: red = bb; green = Brightness; blue = aa; break;
		case 2: red = aa; green = Brightness; blue = cc; break;
		case 3: red = aa; green = bb; blue = Brightness; break;
		case 4: red = cc; green = aa; blue = Brightness; break;
		case 5: red = Brightness; green = aa; blue = bb; break;
		default: red = 0; green = 0; blue = 0; break;
		}
	}

	return (uint32_t)((AS3_INT((Alpha > 1) ? Alpha : (Alpha * 255)) & 0xFF) << 24 | AS3_UINT(red * 255) << 16 | AS3_UINT(green * 255) << 8 | AS3_UINT(blue * 255));
}

char *FlxU_formatTime(Number Seconds, bool ShowMS)
{
	char *timeString = AS3_CAT(AS3_STR(AS3_INT(Seconds / 60)), ":");
	int32_t timeStringHelper = AS3_INT(Seconds) % 60;
	if (timeStringHelper < 10)
		timeString = AS3_CAT(timeString, "0");
	timeString = AS3_CAT(timeString, AS3_STR(timeStringHelper));
	if (ShowMS) {
		timeString = AS3_CAT(timeString, ".");
		timeStringHelper = AS3_INT((Seconds - AS3_INT(Seconds)) * 100);
		if (timeStringHelper < 10)
			timeString = AS3_CAT(timeString, "0");
		timeString = AS3_CAT(timeString, AS3_STR(timeStringHelper));
	}
	return timeString;
}

char *FlxU_formatTicks(uint32_t StartTicks, uint32_t EndTicks)
{
	char number[32];
	snprintf(number, sizeof number, "%g", (Number)(uint32_t)(EndTicks - StartTicks) / 1000);
	return AS3_CAT(number, "s");
}

char *FlxU_formatMoney(Number Amount, bool ShowDecimal, bool EnglishStyle)
{
	int32_t helper;
	int32_t amount = AS3_INT(Amount);
	char *string = "";
	char *comma = "";
	char *zeroes = "";
	while (amount > 0) {
		if ((strlen(string) > 0) && strlen(comma) <= 0) {
			if (EnglishStyle)
				comma = ",";
			else
				comma = ".";
		}
		zeroes = "";
		helper = amount - AS3_INT(amount / 1000) * 1000;
		amount = AS3_INT(amount / 1000.0);
		if (amount > 0) {
			if (helper < 100)
				zeroes = AS3_CAT(zeroes, "0");
			if (helper < 10)
				zeroes = AS3_CAT(zeroes, "0");
		}
		string = AS3_CAT(zeroes, AS3_STR(helper), comma, string);
	}
	if (ShowDecimal) {
		amount = AS3_INT(Amount * 100) - (AS3_INT(Amount) * 100);
		string = AS3_CAT(string, (EnglishStyle ? "." : ","), AS3_STR(amount));
		if (amount < 10)
			string = AS3_CAT(string, "0");
	}
	return string;
}

void *FlxU_getRandom(const As3Array *Objects, uint32_t StartIndex, uint32_t Length)
{
	if (Objects != NULL) {
		uint32_t l = Length;
		if ((l == 0) || (l > Objects->length - StartIndex))
			l = Objects->length - StartIndex;
		if (l > 0)
			return As3Array_get(Objects, StartIndex + AS3_UINT(Math_random() * l));
	}
	return NULL;
}

As3Array *FlxU_shuffle(As3Array *Objects, uint32_t HowManyTimes)
{
	uint32_t i = 0;
	uint32_t index1;
	uint32_t index2;
	void *object;
	while (i < HowManyTimes) {
		index1 = AS3_UINT(Math_random() * Objects->length);
		index2 = AS3_UINT(Math_random() * Objects->length);
		object = As3Array_get(Objects, index2);
		As3Array_set(Objects, index2, As3Array_get(Objects, index1));
		As3Array_set(Objects, index1, object);
		i++;
	}
	return Objects;
}

uint32_t FlxU_getTicks(void)
{
	return getTimer();
}

/* ---- FlxPoint ---------------------------------------------------------- */

FlxPoint *FlxPoint_alloc(Number X, Number Y)
{
	FlxPoint *p = as3_alloc(sizeof *p);
	p->x = X;
	p->y = Y;
	return p;
}

FlxPoint *FlxPoint_make(FlxPoint *self, Number X, Number Y)
{
	self->x = X;
	self->y = Y;
	return self;
}

FlxPoint *FlxPoint_copyFrom(FlxPoint *self, const FlxPoint *Point)
{
	self->x = Point->x;
	self->y = Point->y;
	return self;
}

FlxPoint *FlxPoint_copyTo(const FlxPoint *self, FlxPoint *Point)
{
	Point->x = self->x;
	Point->y = self->y;
	return Point;
}

Number FlxRect_get_left(const FlxRect *self)
{
	return self->x;
}

Number FlxRect_get_right(const FlxRect *self)
{
	return self->x + self->width;
}

Number FlxRect_get_top(const FlxRect *self)
{
	return self->y;
}

Number FlxRect_get_bottom(const FlxRect *self)
{
	return self->y + self->height;
}

/* ---- FlxRect ----------------------------------------------------------- */

FlxRect *FlxRect_alloc(Number X, Number Y, Number Width, Number Height)
{
	FlxRect *r = as3_alloc(sizeof *r);
	return FlxRect_make(r, X, Y, Width, Height);
}

FlxRect *FlxRect_make(FlxRect *self, Number X, Number Y, Number Width, Number Height)
{
	self->x = X;
	self->y = Y;
	self->width = Width;
	self->height = Height;
	return self;
}

FlxRect *FlxRect_copyFrom(FlxRect *self, const FlxRect *Rect)
{
	self->x = Rect->x;
	self->y = Rect->y;
	self->width = Rect->width;
	self->height = Rect->height;
	return self;
}

bool FlxRect_overlaps(const FlxRect *self, const FlxRect *Rect)
{
	return (Rect->x + Rect->width > self->x) && (Rect->x < self->x + self->width) &&
		(Rect->y + Rect->height > self->y) && (Rect->y < self->y + self->height);
}

/* ---- FlxAnim ------------------------------------------------------------ */

FlxAnim *FlxAnim_new(const char *Name, const int32_t *Frames, uint32_t FramesLength, Number FrameRate, bool Looped)
{
	FlxAnim *self = as3_alloc(sizeof *self);
	self->name = Name;
	self->delay = 0;
	if (FrameRate > 0)
		self->delay = 1.0 / FrameRate;
	self->frames = as3_alloc((FramesLength ? FramesLength : 1) * sizeof(int32_t));
	memcpy(self->frames, Frames, FramesLength * sizeof(int32_t));
	self->frames_length = FramesLength;
	self->looped = Looped;
	return self;
}

void FlxAnim_destroy(FlxAnim *self)
{
	free(self->frames);
	self->frames = NULL;
}

/* ---- FlxList ------------------------------------------------------------ */

/* ADAPTATION: the quadtree builds ~900 list nodes per frame; freed nodes are
   kept on a free list instead of going back to malloc (allocation only). */
static FlxList *FlxList__pool;

FlxList *FlxList_new(void)
{
	FlxList *self = FlxList__pool;
	if (self != NULL)
		FlxList__pool = self->next;
	else
		self = as3_alloc(sizeof *self);
	self->object = NULL;
	self->next = NULL;
	return self;
}

/* AS3 relies on the garbage collector after destroy() unlinks the chain;
   C frees the nodes here instead. */
void FlxList_destroy(FlxList *self)
{
	while (self != NULL) {
		FlxList *next = self->next;
		self->object = NULL;
		self->next = FlxList__pool;
		FlxList__pool = self;
		self = next;
	}
}

/* ---- FlxPoint / FlxRect: the members the game never calls ------------------ */

void FlxPoint_copyFromFlash(FlxPoint *self, const Point *FlashPoint)
{
	self->x = FlashPoint->x;
	self->y = FlashPoint->y;
}

Point *FlxPoint_copyToFlash(const FlxPoint *self, Point *FlashPoint)
{
	FlashPoint->x = self->x;
	FlashPoint->y = self->y;
	return FlashPoint;
}

FlxRect *FlxRect_copyTo(const FlxRect *self, FlxRect *Rect)
{
	Rect->x = self->x;
	Rect->y = self->y;
	Rect->width = self->width;
	Rect->height = self->height;
	return Rect;
}

FlxRect *FlxRect_copyFromFlash(FlxRect *self, const Rectangle *FlashRect)
{
	self->x = FlashRect->x;
	self->y = FlashRect->y;
	self->width = FlashRect->width;
	self->height = FlashRect->height;
	return self;
}

Rectangle *FlxRect_copyToFlash(const FlxRect *self, Rectangle *FlashRect)
{
	FlashRect->x = self->x;
	FlashRect->y = self->y;
	FlashRect->width = self->width;
	FlashRect->height = self->height;
	return FlashRect;
}

/* ---- FlxU: reflection helpers ------------------------------------------------
 * ADAPTATION: flash.utils.getQualifiedClassName / getDefinitionByName have no C
 * equivalent; the class table (game/class_table.c) lists every translated class. */

const char *FlxU_getClassName(const void *Obj, bool Simple)
{
	(void)Simple; /* class names here are already unqualified */
	return ((const As3Object *)Obj)->klass->name;
}

bool FlxU_compareClassNames(const void *Obj1, const void *Obj2)
{
	return as3_streq(FlxU_getClassName(Obj1, false), FlxU_getClassName(Obj2, false));
}

const As3Class *FlxU_getClass(const char *Name)
{
	extern const As3Class *const As3_classTable[];
	for (int i = 0; As3_classTable[i] != NULL; i++)
		if (as3_streq(As3_classTable[i]->name, Name))
			return As3_classTable[i];
	return NULL;
}

char *FlxU_formatArray(const As3Array *AnyArray)
{
	if ((AnyArray == NULL) || (AnyArray->length <= 0))
		return "";
	/* items are Strings (AnyArray[i].toString()) */
	char *string = as3_strdup((const char *)As3Array_get(AnyArray, 0));
	uint32_t i = 1;
	uint32_t l = AnyArray->length;
	while (i < l)
		string = AS3_CAT(string, ", ", (const char *)As3Array_get(AnyArray, i++));
	return string;
}

void FlxU_openURL(const char *URL, const char *Window)
{
	(void)URL;
	(void)Window; /* navigateToURL(new URLRequest(URL), Window): there is no browser on the console */
}
