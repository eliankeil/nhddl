#ifndef _GUI_ICONS_H_
#define _GUI_ICONS_H_

// All icons except "Enabled" taken from OPL and modified to fit NHDDL

#include <stdint.h>

typedef struct Icon {
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
} Icon;

// Icon types, must match ICONS array index
typedef enum {
  ICON_CIRCLE,
  ICON_CROSS,
  ICON_SQUARE,
  ICON_TRIANGLE,
  ICON_L1,
  ICON_R1,
  ICON_SELECT,
  ICON_START,
  ICON_ENABLED,
  ICON_UPDOWN,
  ICON_LEFTRIGHT
} IconType;

const Icon ICONS[] = {
    {0, 0, 25, 25},   // Circle
    {25, 0, 25, 25},  // Cross
    {50, 0, 25, 25},  // Square
    {75, 0, 25, 25},  // Triangle
    {0, 25, 25, 15},  // L1
    {25, 25, 25, 15}, // R1
    {0, 40, 21, 11},  // Select
    {21, 40, 21, 11}, // Start
    {100, 0, 12, 12},  // Enabled
    {50, 25, 25, 25}, // UpDown
    {75, 25, 25, 25} // LeftRight
};

#endif
