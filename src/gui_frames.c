#include "gui_frames.h"
#include "gui_graphics.h"   
#include "gui_icons.h"      
#include "gui_skin.h"
#include <gsKit.h>

// Tabla de partes del marco (coordenadas UV dentro del atlas ICONS_PNG)
const FrameIcon FRAME_PARTS[] = {
    {102, 14, 9, 9},   // FRAME_CORNER_TL
    {117, 14, 9, 9},   // FRAME_CORNER_TR
    {102, 29, 9, 9},   // FRAME_CORNER_BL
    {117, 29, 9, 9},   // FRAME_CORNER_BR
    {111, 14, 6, 3},   // FRAME_EDGE_TOP
    {111, 35, 6, 3},   // FRAME_EDGE_BOTTOM
    {102, 23, 3, 6},   // FRAME_EDGE_LEFT
    {123, 23, 3, 6}    // FRAME_EDGE_RIGHT
};

int getFramePartWidth(FramePart part) {
    return FRAME_PARTS[part].w;
}

int getFramePartHeight(FramePart part) {
    return FRAME_PARTS[part].h;
}

void drawFramePart(float x, float y, int z, uint64_t color, FramePart part) {
    const FrameIcon *f = &FRAME_PARTS[part];

    // Configuración de alpha igual que drawIcon
    gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
    gsKit_set_test(gsGlobal, GS_ATEST_OFF);

    gsKit_prim_sprite_texture(gsGlobal,
        icons, // usar la textura global del atlas cargada en initGraphics()
        x, y, f->x, f->y,
        x + f->w, y + f->h, f->x + f->w, f->y + f->h,
        z, color);

    gsKit_set_test(gsGlobal, GS_ATEST_ON);
    gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0,1,0,1,0), 0);
}

// Dibuja un marco completo alrededor del rectángulo [x1,y1]..[x2,y2]
void drawRoundedFrame(int x1, int y1, int x2, int y2, int z) {
    // Esquinas
    drawFramePart(x1, y1, z, 0x80808080, FRAME_CORNER_TL);
    drawFramePart(x2 - getFramePartWidth(FRAME_CORNER_TR), y1, z, 0x80808080, FRAME_CORNER_TR);
    drawFramePart(x1, y2 - getFramePartHeight(FRAME_CORNER_BL), z, 0x80808080, FRAME_CORNER_BL);
    drawFramePart(x2 - getFramePartWidth(FRAME_CORNER_BR), y2 - getFramePartHeight(FRAME_CORNER_BR), z, 0x80808080, FRAME_CORNER_BR);

    // Lados: se estiran entre esquinas
    // Top
    drawFramePart(x1 + getFramePartWidth(FRAME_CORNER_TL), y1, z, 0x80808080, FRAME_EDGE_TOP);

    // Bottom
    drawFramePart(x1 + getFramePartWidth(FRAME_CORNER_BL), y2 - FRAME_PARTS[FRAME_EDGE_BOTTOM].h, z, 0x80808080, FRAME_EDGE_BOTTOM);

    // Left
    drawFramePart(x1, y1 + getFramePartHeight(FRAME_CORNER_TL), z, 0x80808080, FRAME_EDGE_LEFT);

    // Right
    drawFramePart(x2 - FRAME_PARTS[FRAME_EDGE_RIGHT].w, y1 + getFramePartHeight(FRAME_CORNER_TR), z, 0x80808080, FRAME_EDGE_RIGHT);
}
