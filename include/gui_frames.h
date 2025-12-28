#ifndef _GUI_FRAMES_H_
#define _GUI_FRAMES_H_

#include <gsKit.h>
#include <stdint.h>

// Partes del marco (frame)
typedef enum {
    FRAME_CORNER_TL,
    FRAME_CORNER_TR,
    FRAME_CORNER_BL,
    FRAME_CORNER_BR,
    FRAME_EDGE_TOP,
    FRAME_EDGE_BOTTOM,
    FRAME_EDGE_LEFT,
    FRAME_EDGE_RIGHT
} FramePart;

// Metadatos de cada parte del marco
typedef struct {
    int x, y, w, h;
} FrameIcon;

// Tabla de partes del marco (definida en gui_frames.c)
extern const FrameIcon FRAME_PARTS[];

// Helpers
int getFramePartWidth(FramePart part);
int getFramePartHeight(FramePart part);

// Dibuja una parte del marco en coordenadas absolutas
void drawFramePart(float x, float y, int z, uint64_t color, FramePart part);

// Dibuja un marco completo alrededor de un rectángulo
void drawRoundedFrame(int x1, int y1, int x2, int y2, int z);

#endif
