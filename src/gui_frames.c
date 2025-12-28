#include "gui_frames.h"
#include "gui_graphics.h"   
#include "gui_icons.h"      
#include "gui_skin.h"

// Tabla de partes del marco (coordenadas UV dentro del atlas ICONS_PNG)
const FrameIcon FRAME_PARTS[] = {
    {102, 14, 9, 9},   // FRAME_CORNER_TL
    {117, 14, 9, 9},   // FRAME_CORNER_TR
    {102, 29, 9, 9},  // FRAME_CORNER_BL
    {117, 29, 9, 9},  // FRAME_CORNER_BR
    {111, 14, 6, 3},   // FRAME_EDGE_TOP
    {111, 35, 6, 3},  // FRAME_EDGE_BOTTOM
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
    gsKit_prim_sprite_texture(gsGlobal,
        icons, // usar la textura global del atlas cargada en initGraphics()
        x, y, f->x, f->y,
        x + f->w, y + f->h, f->x + f->w, f->y + f->h,
        z, color);
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
    gsKit_prim_sprite_texture(gsGlobal, icons,
        x1 + getFramePartWidth(FRAME_CORNER_TL), y1,
        FRAME_PARTS[FRAME_EDGE_TOP].x, FRAME_PARTS[FRAME_EDGE_TOP].y,
        x2 - getFramePartWidth(FRAME_CORNER_TR), y1 + FRAME_PARTS[FRAME_EDGE_TOP].h,
        FRAME_PARTS[FRAME_EDGE_TOP].x + FRAME_PARTS[FRAME_EDGE_TOP].w, FRAME_PARTS[FRAME_EDGE_TOP].y + FRAME_PARTS[FRAME_EDGE_TOP].h,
        z, 0x80808080);

    // Bottom
    gsKit_prim_sprite_texture(gsGlobal, icons,
        x1 + getFramePartWidth(FRAME_CORNER_BL), y2 - FRAME_PARTS[FRAME_EDGE_BOTTOM].h,
        FRAME_PARTS[FRAME_EDGE_BOTTOM].x, FRAME_PARTS[FRAME_EDGE_BOTTOM].y,
        x2 - getFramePartWidth(FRAME_CORNER_BR), y2,
        FRAME_PARTS[FRAME_EDGE_BOTTOM].x + FRAME_PARTS[FRAME_EDGE_BOTTOM].w, FRAME_PARTS[FRAME_EDGE_BOTTOM].y + FRAME_PARTS[FRAME_EDGE_BOTTOM].h,
        z, 0x80808080);

    // Left
    gsKit_prim_sprite_texture(gsGlobal, icons,
        x1, y1 + getFramePartHeight(FRAME_CORNER_TL),
        FRAME_PARTS[FRAME_EDGE_LEFT].x, FRAME_PARTS[FRAME_EDGE_LEFT].y,
        x1 + FRAME_PARTS[FRAME_EDGE_LEFT].w, y2 - getFramePartHeight(FRAME_CORNER_BL),
        FRAME_PARTS[FRAME_EDGE_LEFT].x + FRAME_PARTS[FRAME_EDGE_LEFT].w, FRAME_PARTS[FRAME_EDGE_LEFT].y + FRAME_PARTS[FRAME_EDGE_LEFT].h,
        z, 0x80808080);

    // Right
    gsKit_prim_sprite_texture(gsGlobal, icons,
        x2 - FRAME_PARTS[FRAME_EDGE_RIGHT].w, y1 + getFramePartHeight(FRAME_CORNER_TR),
        FRAME_PARTS[FRAME_EDGE_RIGHT].x, FRAME_PARTS[FRAME_EDGE_RIGHT].y,
        x2, y2 - getFramePartHeight(FRAME_CORNER_BR),
        FRAME_PARTS[FRAME_EDGE_RIGHT].x + FRAME_PARTS[FRAME_EDGE_RIGHT].w, FRAME_PARTS[FRAME_EDGE_RIGHT].y + FRAME_PARTS[FRAME_EDGE_RIGHT].h,
        z, 0x80808080);
}
