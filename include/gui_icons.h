#ifndef _GUI_ICONS_H_
#define _GUI_ICONS_H_

#include <stdint.h>

typedef struct Icon {
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
} Icon;

// Solo declaraciones
extern const Icon ICONS[];
extern unsigned int SIZE_ICONS_PNG;
extern unsigned char ICONS_PNG[] __attribute__((aligned(16)));
extern unsigned int SIZE_LOGO_PNG;
extern unsigned char LOGO_PNG[] __attribute__((aligned(16)));
extern unsigned int SIZE_BOX3D_PNG;
extern unsigned char BOX3D_PNG[] __attribute__((aligned(16)));
extern unsigned int SIZE_CRT_PNG;
extern unsigned char CRT_PNG[] __attribute__((aligned(16)));

#endif
