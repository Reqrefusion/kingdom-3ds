#ifndef TRANSLIT_FLASH_TEXT_H
#define TRANSLIT_FLASH_TEXT_H
#include "flash.h"

typedef struct TextFormat {
	const char *font;
	Number size;
	uint32_t color;
	const char *align;
} TextFormat;

typedef struct TextLineMetrics {
	Number width;
	Number height;
} TextLineMetrics;

typedef struct TextLine {
	size_t start, length;
	int width;
} TextLine;

#define TEXTFIELD_MAX_LINES 16
typedef struct TextField {
	Number width;
	Number height;
	bool embedFonts, selectable, multiline, wordWrap;
	Number sharpness;
	char *text;
	TextFormat defaultTextFormat;
	TextFormat format; /* format applied to the whole text */
	TextLine lines[TEXTFIELD_MAX_LINES];
	uint32_t lineCount;
	int lineHeight;
} TextField;

extern const char *TextField_fontRoot;
TextFormat TextFormat_new(const char *font, Number size, uint32_t color, const char *align);
TextField *TextField_new(void);
void TextField_set_text(TextField *t, const char *text);
void TextField_setTextFormat(TextField *t, TextFormat format);
uint32_t TextField_get_numLines(TextField *t);
TextLineMetrics TextField_getLineMetrics(TextField *t, uint32_t lineIndex);
void BitmapData_drawTextField(BitmapData *self, TextField *t, const Matrix *matrix, const ColorTransform *colorTransform);

#endif
