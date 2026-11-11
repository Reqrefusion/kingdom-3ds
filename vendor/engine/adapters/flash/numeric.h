#ifndef ENGINE_FLASH_NUMERIC_H
#define ENGINE_FLASH_NUMERIC_H
#include <stdint.h>
#include <math.h>
/* AS3 Number -> uint/int coercion and 32-bit shift semantics. No undefined
   C casts for NaN, infinity, negative values or out-of-range doubles. */
static inline uint32_t kg_as3_uint(double value){
 if(!isfinite(value)||value==0)return 0;
 /* Common finite positive channel/index path avoids a costly fmod on ARM. */
 if(value>0&&value<4294967296.0)return (uint32_t)value;
 double wrapped=fmod(trunc(value),4294967296.0);
 if(wrapped<0)wrapped+=4294967296.0;return (uint32_t)wrapped;
}
static inline int32_t kg_as3_int_bits(uint32_t value){
 return value<0x80000000u?(int32_t)value:(int32_t)((int64_t)value-INT64_C(4294967296));
}
static inline int32_t kg_as3_int(double value){return kg_as3_int_bits(kg_as3_uint(value));}
static inline uint32_t kg_as3_shr(uint32_t value,unsigned count){return value>>(count&31u);}
static inline int32_t kg_as3_sar(uint32_t value,unsigned count){
 count&=31u;if(!count)return kg_as3_int_bits(value);
 uint32_t shifted=value>>count;
 if(value&0x80000000u)shifted|=UINT32_MAX<<(32-count);
 return kg_as3_int_bits(shifted);
}
/* Utils.interpolateColor uses Number products followed by uint assignment
   per channel, rather than a float a+(b-a)*f expression. */
static inline uint32_t kg_as3_color_lerp(uint32_t a,uint32_t b,double f){
 uint32_t color=0;double fi=1-f;
 for(unsigned bit=0;bit<32;bit+=8){
  uint32_t channel=kg_as3_uint(fi*((a>>bit)&255u)+f*((b>>bit)&255u));
  color|=channel<<bit;
 }
 return color;
}
#endif
