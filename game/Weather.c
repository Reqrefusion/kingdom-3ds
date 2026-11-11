/* Weather.as */
#include "game.h"

Weather *Weather_new(void)
{
	Weather *self = as3_alloc(sizeof *self);
	/* field initializers */
	self->sky = 0xFF8C8CA6;
	self->horizon = 0xFFCF7968;
	self->haze = 0xAAf3f1e8;
	self->darknessColor = 0x88111114;
	self->darkness = 1.0;
	self->contrast = 0.3;
	self->saturation = 1.0;
	self->ambient = 0x11FF0000;
	self->wind = 0.0;
	self->fog = 0.5;
	self->rain = 0.5;
	self->timeOfDay = 0.5;
	self->sunTint = 0xFFFFFF;
	ColorMatrix_ctor(&self->ambientTransform);
	self->t = 0;
	self->changed = 0;
	self->progress = 0;
	self->ambientAmount = 0;
	self->tweenStart = 0;
	self->tweenDuration = 0.0;
	self->previousState = WeatherPresets_SUNNY;
	self->targetState = WeatherPresets_SUNNY;

	Weather_setVariables(self, WeatherPresets_SUNNY, NULL, 1);
	return self;
}

void Weather_update(Weather *self)
{
	self->t += FlxG.elapsed;
	if (self->t - self->changed > 1.0 / 30) {
		Weather_updateTween(self);
		self->changed = self->t;
	}
}

void Weather_tweenTo(Weather *self, WeatherState *state, Number d)
{
	self->targetState = state;
	if (d == 0) {
		Weather_setVariables(self, state, NULL, 1);
		self->previousState = state;
	} else {
		self->tweenDuration = d;
		self->tweenStart = self->t;
	}
}

void Weather_updateTween(Weather *self)
{
	if (self->targetState == self->previousState)
		return;
	self->progress = (self->t - self->tweenStart) / self->tweenDuration;
	if (self->tweenDuration == 0 || self->progress >= 1) {
		self->previousState = self->targetState;
		self->progress = 1;
	}
	Weather_setVariables(self, self->targetState, self->previousState, self->progress);
}

/* this[v] for the colour (uint) fields */
static uint32_t *weather_color_field(Weather *self, uint32_t bit)
{
	switch (bit) {
	case WEATHER_HAS_sky: return &self->sky;
	case WEATHER_HAS_horizon: return &self->horizon;
	case WEATHER_HAS_haze: return &self->haze;
	case WEATHER_HAS_darknessColor: return &self->darknessColor;
	case WEATHER_HAS_ambient: return &self->ambient;
	case WEATHER_HAS_sunTint: return &self->sunTint;
	default: return NULL;
	}
}

/* this[v] for the Number fields */
static Number *weather_number_field(Weather *self, uint32_t bit)
{
	switch (bit) {
	case WEATHER_HAS_darkness: return &self->darkness;
	case WEATHER_HAS_contrast: return &self->contrast;
	case WEATHER_HAS_saturation: return &self->saturation;
	case WEATHER_HAS_fog: return &self->fog;
	case WEATHER_HAS_rain: return &self->rain;
	case WEATHER_HAS_wind: return &self->wind;
	case WEATHER_HAS_ambientAmount: return &self->ambientAmount;
	case WEATHER_HAS_timeOfDay: return &self->timeOfDay;
	default: return NULL;
	}
}

/* target[v] */
static Number state_value(const WeatherState *s, uint32_t bit)
{
	if (!(s->has & bit))
		return NAN; /* undefined property */
	switch (bit) {
	case WEATHER_HAS_sky: return s->sky;
	case WEATHER_HAS_horizon: return s->horizon;
	case WEATHER_HAS_haze: return s->haze;
	case WEATHER_HAS_darknessColor: return s->darknessColor;
	case WEATHER_HAS_darkness: return s->darkness;
	case WEATHER_HAS_contrast: return s->contrast;
	case WEATHER_HAS_saturation: return s->saturation;
	case WEATHER_HAS_ambient: return s->ambient;
	case WEATHER_HAS_wind: return s->wind;
	case WEATHER_HAS_fog: return s->fog;
	case WEATHER_HAS_rain: return s->rain;
	case WEATHER_HAS_timeOfDay: return s->timeOfDay;
	case WEATHER_HAS_sunTint: return s->sunTint;
	case WEATHER_HAS_ambientAmount: return s->ambientAmount;
	default: return NAN; /* undefined */
	}
}

void Weather_setVariables(Weather *self, WeatherState *target, WeatherState *previous, Number f)
{
	if (!(target->has & WEATHER_HAS_ambientAmount)) {
		target->ambientAmount = (AS3_SAR(AS3_UINT(target->ambient), 24) / (Number)0xFF);
		target->has |= WEATHER_HAS_ambientAmount;
	}
	if (previous == NULL) {
		previous = target;
	}
	Number fi = 1 - f;
	/* for (var v:String in target): each defined property once; the
	   assignments are independent, so visiting order does not matter. */
	for (uint32_t v = 1; v <= WEATHER_HAS_ambientAmount; v <<= 1) {
		if (!(target->has & v))
			continue;
		if (v == WEATHER_HAS_darkness || v == WEATHER_HAS_contrast || v == WEATHER_HAS_saturation || v == WEATHER_HAS_fog || v == WEATHER_HAS_rain || v == WEATHER_HAS_wind || v == WEATHER_HAS_ambientAmount) {
			*weather_number_field(self, v) = (fi * state_value(previous, v)) + (f * state_value(target, v));
		} else if (v == WEATHER_HAS_timeOfDay) {
			if (state_value(target, v) > state_value(previous, v))
				*weather_number_field(self, v) = as3_mod(state_value(previous, v) + (state_value(target, v) - state_value(previous, v)) * f, 1);
			else
				*weather_number_field(self, v) = as3_mod(state_value(previous, v) + (state_value(target, v) + 1 - state_value(previous, v)) * f, 1);
		} else {
			*weather_color_field(self, v) = Utils_interpolateColor(AS3_UINT(state_value(previous, v)), AS3_UINT(state_value(target, v)), f);
		}
	}
	ColorMatrix_reset(&self->ambientTransform);
	ColorMatrix_colorize(&self->ambientTransform, self->ambient, self->ambientAmount);
	ColorMatrix_adjustContrast(&self->ambientTransform, self->contrast, NAN, NAN);
	ColorMatrix_adjustSaturation(&self->ambientTransform, self->saturation);
	self->darknessColor = (self->darknessColor & 0x00FFFFFF) | (AS3_UINT(0xFF * self->darkness) << 24);
}
