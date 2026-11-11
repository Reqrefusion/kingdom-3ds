/* Minimal AS3 runtime used by the line-by-line translation.
 *
 * Conventions (see translit/README.md):
 *   Number           -> double (AS3 Number is an IEEE double)
 *   int / uint       -> int32_t / uint32_t, conversions through AS3_INT/AS3_UINT
 *   obj.method(a)    -> Class_method(obj, a)            (non-virtual / final)
 *   method(a)        -> CALL(self, method, a)           (virtual dispatch)
 *   super.method(a)  -> SUPER(Parent_method(self, a))
 *   x is T           -> as3_is(x, &T_class)
 *   x as T           -> (T *)x  (only after an `is` check or where AS3 casts)
 */
#ifndef TRANSLIT_AS3_H
#define TRANSLIT_AS3_H
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
/* Exact AS3 Number -> int/uint coercion (vendor/engine, found through -Ivendor). */
#include "engine/adapters/flash/numeric.h"

typedef double Number;

#define AS3_INT(v) kg_as3_int((double)(v))
#define AS3_UINT(v) kg_as3_uint((double)(v))
/* AS3 `a >> n` (signed) and `a >>> n` on 32-bit values. */
#define AS3_SAR(v, n) kg_as3_sar((uint32_t)(v), (unsigned)(n))
#define AS3_SHR(v, n) kg_as3_shr((uint32_t)(v), (unsigned)(n))

/* Marks a `super.xxx()` call so the static comparison tool can tell it apart
   from an ordinary call of an inherited method. It has no runtime effect. */
#define SUPER(call) call

/* Array literal with its length, used for addAnimation frame lists. */
#define ARR(...) ((const int32_t[]){__VA_ARGS__}), \
	(uint32_t)(sizeof((const int32_t[]){__VA_ARGS__}) / sizeof(int32_t))

static inline bool as3_streq(const char *a, const char *b)
{
	if (a == b)
		return true;
	if (a == NULL || b == NULL)
		return false;
	return strcmp(a, b) == 0;
}

static inline Number Math_max(Number a, Number b) { return (a > b || isnan(a)) ? a : b; }
static inline Number Math_min(Number a, Number b) { return (a < b || isnan(a)) ? a : b; }
static inline Number Math_abs(Number a) { return fabs(a); }
/* AS3 Math.random(): seeded xorshift, defined in game/Utils.c (ADAPTATION: Flash's
   generator is not documented). Declared here so the Flixel layer can call it. */
Number Math_random(void);
/* AS3 Math.round: halves round up */
static inline Number Math_round(Number a) { return floor(a + 0.5); }
static inline Number Math_sqrt(Number a) { return sqrt(a); }
static inline Number Math_atan2(Number y, Number x) { return atan2(y, x); }
#define Math_PI 3.141592653589793

/* AS3 `%` on Number is fmod. */
static inline Number as3_mod(Number a, Number b) { return fmod(a, b); }

/* ---- Class objects, `new`, `is` --------------------------------------- */

struct FlxBasic;
struct FlxCamera;

/* Every AS3 method that is overridden somewhere in the translated code is
   dispatched through this table. Unused slots stay NULL for classes that do
   not have the method (e.g. reset on FlxGroup). */
typedef struct As3VTable {
	void (*destroy)(struct FlxBasic *self);
	void (*preUpdate)(struct FlxBasic *self);
	void (*update)(struct FlxBasic *self);
	void (*postUpdate)(struct FlxBasic *self);
	void (*draw)(struct FlxBasic *self);
	void (*kill)(struct FlxBasic *self);
	void (*revive)(struct FlxBasic *self);
	void (*reset)(struct FlxBasic *self, Number X, Number Y);
	bool (*onScreen)(struct FlxBasic *self, struct FlxCamera *Camera);
	void (*create)(struct FlxBasic *self); /* FlxState.create */
	void (*calcFrame)(struct FlxBasic *self); /* FlxSprite.calcFrame (FlxText) */
	void (*hurt)(struct FlxBasic *self, Number Damage); /* FlxObject.hurt (Wall) */
} As3VTable;

typedef struct As3Class {
	const char *name;
	const struct As3Class *super;
	/* `new ObjectClass()` for classes with a zero-argument constructor. */
	struct FlxBasic *(*construct)(void);
	As3VTable vt;
} As3Class;

/* First field of every translated object. */
#define AS3_OBJECT_FIELDS const As3Class *klass;

typedef struct As3Object {
	AS3_OBJECT_FIELDS
} As3Object;

static inline bool as3_is(const void *object, const As3Class *klass)
{
	if (object == NULL)
		return false;
	for (const As3Class *k = ((const As3Object *)object)->klass; k != NULL; k = k->super)
		if (k == klass)
			return true;
	return false;
}

/* Virtual call: AS3 `method()` / `obj.method()` where subclasses override. */
#define CALL0(object, method) \
	(((struct FlxBasic *)(object))->klass->vt.method((struct FlxBasic *)(object)))
#define CALL(object, method, ...) \
	(((struct FlxBasic *)(object))->klass->vt.method((struct FlxBasic *)(object), __VA_ARGS__))

static inline void *as3_alloc(size_t size)
{
	void *p = calloc(1, size);
	if (p == NULL)
		abort();
	return p;
}

/* ---- Array of object references (AS3 Array used as a member list) ----- */

typedef struct As3Array {
	void **items;
	uint32_t length;   /* AS3 Array.length */
	uint32_t capacity; /* storage only, not visible to AS3 code */
} As3Array;

static inline void As3Array_set_length(As3Array *a, uint32_t length)
{
	if (length > a->capacity) {
		uint32_t capacity = a->capacity ? a->capacity : 8;
		while (capacity < length)
			capacity *= 2;
		void **items = realloc(a->items, capacity * sizeof(void *));
		if (items == NULL)
			abort();
		memset(items + a->capacity, 0, (capacity - a->capacity) * sizeof(void *));
		a->items = items;
		a->capacity = capacity;
	}
	if (length < a->length)
		memset(a->items + length, 0, (a->length - length) * sizeof(void *));
	a->length = length;
}

/* Reading past the end yields undefined -> null for object references. */
static inline void *As3Array_get(const As3Array *a, uint32_t index)
{
	return index < a->length ? a->items[index] : NULL;
}

/* Writing past the end extends length to index+1, as in AS3. */
static inline void As3Array_set(As3Array *a, uint32_t index, void *value)
{
	if (index >= a->length)
		As3Array_set_length(a, index + 1);
	a->items[index] = value;
}

static inline int32_t As3Array_indexOf(const As3Array *a, const void *value)
{
	for (uint32_t i = 0; i < a->length; i++)
		if (a->items[i] == value)
			return (int32_t)i;
	return -1;
}

static inline void As3Array_splice1(As3Array *a, uint32_t index)
{
	memmove(a->items + index, a->items + index + 1, (a->length - index - 1) * sizeof(void *));
	a->length--;
	a->items[a->length] = NULL;
}

/* ---- String, Array of values, RegExp (PlayState progress strings) ----
 * Spelled so the static comparison reads them as the AS3 expressions:
 *   AS3_CAT(a, b, c)      a + b + c          (String concatenation)
 *   AS3_STR(n)            n                  (int -> String, implicit in AS3)
 *   AS3_BOX(n)            n                  (int stored in an Array)
 *   AS3_ARRAY()           []
 *   AS3_LIST(a, b)        [a, b]             (fixed-size value array)
 *   As3Array_push(&a, v)  a.push(v)
 *   As3Array_join(&a, s)  a.join(s)          (Array of AS3_BOX ints)
 *   AS3_REGEX("D(\\d+)")  /D(\d+)/
 *   As3String_match(s, r) s.match(r)         (Array of captured Strings; empty if none)
 * Strings are heap copies and never freed (a few per in-game day). */
#define AS3_ARRAY() ((As3Array){0})
#define AS3_LIST(...) {__VA_ARGS__} /* [a, b, ...] as a C array initialiser */
#define AS3_BOX(v) ((void *)(intptr_t)(int32_t)(v))
#define AS3_REGEX(pattern) (pattern)

static inline void As3Array_push(As3Array *a, void *value)
{
	As3Array_set(a, a->length, value);
}

static inline char *as3_strdup(const char *s)
{
	size_t n = strlen(s) + 1;
	char *p = as3_alloc(n);
	memcpy(p, s, n);
	return p;
}

/* String(int) */
static inline char *as3_str_int(long v)
{
	char b[24];
	snprintf(b, sizeof b, "%ld", v);
	return as3_strdup(b);
}
#define AS3_STR(v) _Generic((v), char *: as3_strdup, const char *: as3_strdup, default: as3_str_int)(v)

static inline char *as3_cat_n(int n, const char *const *parts)
{
	size_t len = 1;
	for (int i = 0; i < n; i++)
		len += strlen(parts[i]);
	char *p = as3_alloc(len), *q = p;
	for (int i = 0; i < n; i++) {
		size_t k = strlen(parts[i]);
		memcpy(q, parts[i], k);
		q += k;
	}
	*q = 0;
	return p;
}
#define AS3_CAT(...) as3_cat_n((int)(sizeof((const char *[]){__VA_ARGS__}) / sizeof(const char *)), \
	(const char *const[]){__VA_ARGS__})

static inline char *As3Array_join(const As3Array *a, const char *sep)
{
	char *s = as3_strdup("");
	for (uint32_t i = 0; i < a->length; i++)
		s = i ? AS3_CAT(s, sep, AS3_STR((long)(intptr_t)a->items[i])) : AS3_STR((long)(intptr_t)a->items[i]);
	return s;
}

/* parseInt(s): leading decimal integer, NaN when there is none (or s is null) */
static inline Number parseInt(const char *s)
{
	if (s == NULL)
		return NAN;
	while (*s == ' ')
		s++;
	bool neg = *s == '-';
	if (*s == '-' || *s == '+')
		s++;
	if (*s < '0' || *s > '9')
		return NAN;
	Number v = 0;
	for (; *s >= '0' && *s <= '9'; s++)
		v = v * 10 + (*s - '0');
	return neg ? -v : v;
}

/* String.match(RegExp) for the patterns the game uses: literal characters
   and capture groups "(\\d)" / "(\\d+)". Result [0] = whole match, [k] = group
   k, as AS3; an empty Array when nothing matches. */
static inline As3Array *As3String_match(const char *s, const char *re)
{
	As3Array *out = as3_alloc(sizeof *out);
	for (const char *start = s; *start; start++) {
		const char *p = start, *r = re;
		As3Array groups = AS3_ARRAY();
		bool ok = true;
		while (*r && ok) {
			if (strncmp(r, "(\\d+)", 5) == 0 || strncmp(r, "(\\d)", 4) == 0) {
				bool many = r[3] == '+';
				const char *g = p;
				while (*p >= '0' && *p <= '9' && (many || p == g))
					p++;
				ok = p > g;
				char *cap = as3_alloc((size_t)(p - g) + 1);
				memcpy(cap, g, (size_t)(p - g));
				As3Array_push(&groups, cap);
				r += many ? 5 : 4;
			} else
				ok = *p++ == *r++;
		}
		if (ok) {
			char *whole = as3_alloc((size_t)(p - start) + 1);
			memcpy(whole, start, (size_t)(p - start));
			As3Array_push(out, whole);
			for (uint32_t k = 0; k < groups.length; k++)
				As3Array_push(out, groups.items[k]);
			free(groups.items);
			return out;
		}
		free(groups.items);
	}
	return out;
}

#endif
