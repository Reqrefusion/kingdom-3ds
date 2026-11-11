/* flash.text.TextField / TextFormat subset used by FlxText.
 * Glyphs come from fonts rasterised at build time (tools/convert_assets.py)
 * from the same TTF files the SWF embeds. ADAPTATION: Flash's own text
 * rasteriser and exact line metrics are approximated by the TTF metrics. */
#include "text.h"

const char *TextField_fontRoot = "translit_assets/fonts";

typedef struct Glyph {
	uint8_t advance, w, h;
	int8_t xoff, yoff;
	uint8_t *alpha;
} Glyph;

typedef struct Font {
	char name[32];
	int size;
	int ascent, descent, lineHeight;
	Glyph glyphs[96];
	struct Font *next;
} Font;

static Font *fonts;

static const char *font_file(const char *name)
{
	/* "system" is Flixel's embedded nokiafc22 font. */
	if (name == NULL || name[0] == 0 || as3_streq(name, "system"))
		return "system";
	return name;
}

static Font *font_get(const char *name, int size)
{
	name = font_file(name);
	for (Font *f = fonts; f != NULL; f = f->next)
		if (f->size == size && strcmp(f->name, name) == 0)
			return f;
	Font *f = as3_alloc(sizeof *f);
	snprintf(f->name, sizeof f->name, "%s", name);
	f->size = size;
	char path[512];
	snprintf(path, sizeof path, "%s/%s_%d.font", TextField_fontRoot, name, size);
	FILE *in = fopen(path, "rb");
	if (in != NULL) {
		int32_t header[3];
		if (fread(header, sizeof header, 1, in) == 1) {
			f->ascent = header[0];
			f->descent = header[1];
			f->lineHeight = header[2];
			for (int c = 0; c < 95; c++) {
				uint8_t g[5];
				if (fread(g, 5, 1, in) != 1)
					break;
				Glyph *gl = &f->glyphs[c];
				gl->advance = g[0];
				gl->w = g[1];
				gl->h = g[2];
				gl->xoff = (int8_t)g[3];
				gl->yoff = (int8_t)g[4];
				gl->alpha = as3_alloc((size_t)(gl->w * gl->h) + 1);
				if (gl->w && gl->h && fread(gl->alpha, (size_t)(gl->w * gl->h), 1, in) != 1)
					break;
			}
		}
		fclose(in);
	} else {
		fprintf(stderr, "missing font %s\n", path);
		f->ascent = size;
		f->lineHeight = size + 2;
	}
	f->next = fonts;
	fonts = f;
	return f;
}

TextFormat TextFormat_new(const char *font, Number size, uint32_t color, const char *align)
{
	return (TextFormat){font, size, color, align};
}

TextField *TextField_new(void)
{
	TextField *t = as3_alloc(sizeof *t);
	t->text = strdup("");
	t->defaultTextFormat = TextFormat_new("Times New Roman", 12, 0, "left");
	t->format = t->defaultTextFormat;
	return t;
}

static int text_width(Font *f, const char *s, size_t n)
{
	int w = 0;
	for (size_t i = 0; i < n; i++) {
		unsigned char c = (unsigned char)s[i];
		if (c >= 32 && c < 127)
			w += f->glyphs[c - 32].advance;
	}
	return w;
}

/* Breaks the text into lines (explicit newlines, then word wrap). */
static void layout(TextField *t)
{
	Font *f = font_get(t->format.font, (int)t->format.size);
	t->lineCount = 0;
	const char *s = t->text;
	Number maxWidth = t->width - 4;
	while (t->lineCount < TEXTFIELD_MAX_LINES) {
		size_t len = strcspn(s, "\n\r");
		size_t start = 0;
		do {
			size_t end = len;
			if (t->wordWrap) {
				/* longest prefix of whole words that fits */
				size_t best = 0;
				for (size_t i = start; i <= len; i++)
					if (i == len || s[i] == ' ') {
						if (text_width(f, s + start, i - start) <= maxWidth || best == 0)
							best = i;
						else
							break;
					}
				end = best > start ? best : len;
			}
			TextLine *line = &t->lines[t->lineCount++];
			line->start = (size_t)(s - t->text) + start;
			line->length = end - start;
			line->width = text_width(f, s + start, end - start);
			start = end;
			while (start < len && s[start] == ' ')
				start++;
		} while (start < len && t->lineCount < TEXTFIELD_MAX_LINES);
		if (s[len] == 0)
			break;
		s += len + 1;
	}
	t->lineHeight = f->lineHeight;
}

void TextField_set_text(TextField *t, const char *text)
{
	char *copy = strdup(text ? text : "");
	free(t->text);
	t->text = copy;
}

void TextField_setTextFormat(TextField *t, TextFormat format)
{
	t->format = format;
}

uint32_t TextField_get_numLines(TextField *t)
{
	layout(t);
	return t->lineCount;
}

TextLineMetrics TextField_getLineMetrics(TextField *t, uint32_t lineIndex)
{
	layout(t);
	TextLineMetrics m = {0, 0};
	if (lineIndex < t->lineCount) {
		m.width = t->lines[lineIndex].width;
		m.height = t->lineHeight;
	}
	return m;
}

void BitmapData_drawTextField(BitmapData *self, TextField *t, const Matrix *matrix, const ColorTransform *colorTransform)
{
	self->gen++;
	self->solid = false;
	layout(t);
	Font *f = font_get(t->format.font, (int)t->format.size);
	Matrix m = matrix ? *matrix : Matrix_new();
	uint32_t rgb = t->format.color & 0x00FFFFFFu;
	for (uint32_t li = 0; li < t->lineCount; li++) {
		TextLine *line = &t->lines[li];
		Number x = 2, y = 2 + li * t->lineHeight;
		if (as3_streq(t->format.align, "center"))
			x += floor((t->width - 4 - line->width) / 2);
		else if (as3_streq(t->format.align, "right"))
			x += t->width - 4 - line->width;
		for (size_t i = 0; i < line->length; i++) {
			unsigned char c = (unsigned char)t->text[line->start + i];
			if (c < 32 || c >= 127)
				continue;
			Glyph *g = &f->glyphs[c - 32];
			for (int gy = 0; gy < g->h; gy++)
				for (int gx = 0; gx < g->w; gx++) {
					uint32_t a = g->alpha[gy * g->w + gx];
					if (a == 0)
						continue;
					Number px = x + g->xoff + gx, py = y + f->ascent + g->yoff + gy;
					int32_t dx = (int32_t)floor(m.a * px + m.c * py + m.tx), dy = (int32_t)floor(m.b * px + m.d * py + m.ty);
					if (dx < 0 || dy < 0 || dx >= self->width || dy >= self->height)
						continue;
					uint32_t src = a << 24 | rgb;
					if (colorTransform) {
						Number ca = a * colorTransform->alphaMultiplier + colorTransform->alphaOffset;
						src = (uint32_t)(ca <= 0 ? 0 : ca >= 255 ? 255 : ca) << 24 | rgb;
					}
					uint32_t *d = &self->pixels[dy * self->width + dx];
					*d = BitmapData_blendOver(src, *d, self->transparent);
				}
			x += g->advance;
		}
	}
}
