#include "gui_skin.h"
#include "gui_graphics.h" 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpad.h>
#include "skin_layout.h"
#include "pad.h"
#include "gui_icons.h"

extern GSGLOBAL *gsGlobal;

ThemeColors currentTheme;

// Inicializa los colores por defecto en currentTheme
void setDefaultSkin() {
    currentTheme.background   = BGColor;
    currentTheme.listText     = FontMainColor;
    currentTheme.selectedText = ColorSelected;
    currentTheme.headerText   = HeaderTextColor;
    currentTheme.iconEnabled    = ColorGrey;
    currentTheme.coverFrame   = ColorGrey;
    currentTheme.warnText     = WarnTextColor;
    currentTheme.errorText    = ErrorTextColor;
}

// Genera skin.yaml con los valores por defecto y comentarios
int saveSkin(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) {
        printf("ERROR: No se pudo crear skin.yaml en %s\n", path);
        return -1;
    }

    fprintf(f, "# Neutrino Skin Configuration\n");
    fprintf(f, "# Colores en formato ABGR (0xAABBGGRR)\n\n");

    fprintf(f, "background:   0x%08X # Original: 0x%08X\n", (unsigned int)BGColor, (unsigned int)BGColor);
    fprintf(f, "listText:     0x%08X # Original: 0x%08X\n", (unsigned int)FontMainColor, (unsigned int)FontMainColor);
    fprintf(f, "selectedText: 0x%08X # Original: 0x%08X\n", (unsigned int)ColorSelected, (unsigned int)ColorSelected);
    fprintf(f, "headerText:   0x%08X # Original: 0x%08X\n", (unsigned int)HeaderTextColor, (unsigned int)HeaderTextColor);
    fprintf(f, "iconEnabled:    0x%08X # Original: 0x%08X\n", (unsigned int)ColorGrey, (unsigned int)ColorGrey);
    fprintf(f, "coverFrame:   0x%08X # Original: 0x%08X\n", (unsigned int)ColorGrey, (unsigned int)ColorGrey);
    fprintf(f, "warnText:     0x%08X # Original: 0x%08X\n", (unsigned int)WarnTextColor, (unsigned int)WarnTextColor);
    fprintf(f, "errorText:    0x%08X # Original: 0x%08X\n", (unsigned int)ErrorTextColor, (unsigned int)ErrorTextColor);

    fclose(f);
    return 0;
}

int loadSkin(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("WARN: No se pudo abrir skin.yaml en %s\n", path);
        return -1;
    }

    char line[256];
    char key[64];
    unsigned int value;

    while (fgets(line, sizeof(line), f)) {
        // Saltar comentarios y líneas vacías
        if (line[0] == '#' || line[0] == '\n')
            continue;

        // Parsear "clave:" y "0xHEX" ignorando todo lo posterior
        if (sscanf(line, "%63[^:]: 0x%08X", key, &value) == 2) {
            if      (strcmp(key, "background")   == 0) currentTheme.background   = value;
            else if (strcmp(key, "listText")     == 0) currentTheme.listText     = value;
            else if (strcmp(key, "selectedText") == 0) currentTheme.selectedText = value;
            else if (strcmp(key, "headerText")   == 0) currentTheme.headerText   = value;
            else if (strcmp(key, "iconEnabled")    == 0) currentTheme.iconEnabled    = value;
            else if (strcmp(key, "coverFrame")   == 0) currentTheme.coverFrame   = value;
            else if (strcmp(key, "warnText")     == 0) currentTheme.warnText     = value;
            else if (strcmp(key, "errorText")    == 0) currentTheme.errorText    = value;
        }
    }

    fclose(f);
    return 0;
}

// Campos del skin (labels visibles en pantalla)
const char *fields[] = {
    "Background",
    "Titles|Options Text",
    "Selected Text",
    "Secondary Text",
    "Enable Icon",
    "Cover Art Border",
    "Warning Text",
    "Error Text"
};

uint64_t *values[] = {
    &currentTheme.background,
    &currentTheme.listText,
    &currentTheme.selectedText,
    &currentTheme.headerText,
    &currentTheme.iconEnabled,
    &currentTheme.coverFrame,
    &currentTheme.warnText,
    &currentTheme.errorText
};

int totalFields = sizeof(fields) / sizeof(fields[0]);
int uiSkinOptionsLoop() {
  int res = 0; 
  int input = 0;
  int selectedIdx = 0;
  int editing = 0;       // 0 = navegando, 1 = editando parámetro
  int editChannel = 0;   // 0=A, 1=B, 2=G, 3=R
  int repeatCounter = 0;
  const int repeatDelay = 20; // ~1s a 60fps
  const int repeatSpeed = 2;  // cada 2 frames después del delay

    while (1) {
        gsKit_clear(gsGlobal, currentTheme.background);

        // Header
        drawTextWindow(0, headerHeight - getFontLineHeight(),
                       gsGlobal->Width, 0, 0,
                       currentTheme.headerText, ALIGN_HCENTER,
                       "Skin Configuration");

        // Lista de parámetros
        int y = headerHeight + getFontLineHeight();
        for (int i = 0; i < totalFields; i++) {
            // Label del parámetro
            uint64_t nameColor = (i == selectedIdx) ? currentTheme.selectedText : currentTheme.listText;
            y = drawText(keepoutArea + 10, y, 0, 0, 0, nameColor, fields[i]);

            // Descomponer color en ABGR
            uint32_t color32 = (uint32_t)(*values[i]);
            uint8_t a = (color32 >> 24) & 0xFF;
            uint8_t b = (color32 >> 16) & 0xFF;
            uint8_t g = (color32 >> 8)  & 0xFF;
            uint8_t r = (color32)       & 0xFF;

            // Armar cadena completa
            char buf[32];
            snprintf(buf, sizeof(buf), "%02X : %02X : %02X : %02X", a, b, g, r);

            // Calcular ancho y posición centrada
            int lineWidth = (int)getLineWidth(buf);
            int centerX   = (gsGlobal->Width - lineWidth) / 2;

            // Colores de cada canal
            uint64_t chColorAA = currentTheme.listText;
            uint64_t chColorBB = currentTheme.listText;
            uint64_t chColorGG = currentTheme.listText;
            uint64_t chColorRR = currentTheme.listText;

            if (editing && i == selectedIdx) {
                // Mostrar iconEnabled a la izquierda
                drawIconWindow(keepoutArea - getIconWidth(ICON_ENABLED) - 5,
                               y - getFontLineHeight(), 0, y, 0,
                               currentTheme.iconEnabled, ALIGN_VCENTER, ICON_ENABLED);

                // Resaltar canal activo
                if (editChannel == 0) chColorAA = currentTheme.selectedText;
                if (editChannel == 1) chColorBB = currentTheme.selectedText;
                if (editChannel == 2) chColorGG = currentTheme.selectedText;
                if (editChannel == 3) chColorRR = currentTheme.selectedText;
            }

            // Dibujar valores centrados
            y = drawText(centerX, y, 0, 0, 0,
                         (i == selectedIdx) ? currentTheme.selectedText : currentTheme.listText,
                         buf);

            // Preview a la derecha de la cadena
            int prevW = 140;
            int prevH = getFontLineHeight();
            gsKit_prim_sprite(gsGlobal,
                              centerX + lineWidth + 20, y - getFontLineHeight(),
                              centerX + lineWidth + 20 + prevW, y - getFontLineHeight() + prevH,
                              0, (uint64_t)(*values[i]));
        }
      
        // Footer con acciones (pantalla principal del editor)
        int baseY = gsGlobal->Height - footerHeight;
        int curX  = keepoutArea + 10;

        // CROSS → Editar
        drawIconWindow(curX, baseY, 0, gsGlobal->Height, 0,
                       FontMainColor, ALIGN_CENTER, ICON_CROSS);
        curX += getIconWidth(ICON_CROSS) + 5;
        drawTextWindow(curX, baseY, 0, gsGlobal->Height - 1, 0,
                       currentTheme.headerText, ALIGN_VCENTER, "Editar");
        curX += getLineWidth("Editar") + 40; // espacio extra

        // START → Guardar
        drawIconWindow(curX, baseY, 0, gsGlobal->Height, 0,
                       FontMainColor, ALIGN_CENTER, ICON_START);
        curX += getIconWidth(ICON_START) + 5;
        drawTextWindow(curX, baseY, 0, gsGlobal->Height - 1, 0,
                       currentTheme.headerText, ALIGN_VCENTER, "Guardar");
        curX += getLineWidth("Guardar") + 40;

        // TRIANGLE → Cancelar
        drawIconWindow(curX, baseY, 0, gsGlobal->Height, 0,
                       FontMainColor, ALIGN_CENTER, ICON_TRIANGLE);
        curX += getIconWidth(ICON_TRIANGLE) + 5;
        drawTextWindow(curX, baseY, 0, gsGlobal->Height - 1, 0,
                       currentTheme.headerText, ALIGN_VCENTER, "Cancelar");
        curX += getLineWidth("Cancelar") + 40;

        // SQUARE → Reset
        drawIconWindow(curX, baseY, 0, gsGlobal->Height, 0,
                       FontMainColor, ALIGN_CENTER, ICON_SQUARE);
        curX += getIconWidth(ICON_SQUARE) + 5;
        drawTextWindow(curX, baseY, 0, gsGlobal->Height - 1, 0,
                       currentTheme.headerText, ALIGN_VCENTER, "Reset");
        curX += getLineWidth("Reset") + 40;

        // UP/DOWN → Navegar
        drawIconWindow(curX, baseY, 0, gsGlobal->Height, 0,
                       FontMainColor, ALIGN_CENTER, ICON_UP);
        curX += getIconWidth(ICON_UP) + 10;
        drawIconWindow(curX, baseY, 0, gsGlobal->Height, 0,
                       FontMainColor, ALIGN_CENTER, ICON_DOWN);
        curX += getIconWidth(ICON_DOWN) + 5;
        drawTextWindow(curX, baseY, 0, gsGlobal->Height - 1, 0,
               currentTheme.headerText, ALIGN_VCENTER, "Navegar");
        curX += getLineWidth("Navegar") + 40;

        // LEFT/RIGHT → Cambiar opción
        drawIconWindow(curX, baseY, 0, gsGlobal->Height, 0,
                       FontMainColor, ALIGN_CENTER, ICON_LEFT);
        curX += getIconWidth(ICON_LEFT) + 10;
        drawIconWindow(curX, baseY, 0, gsGlobal->Height, 0,
                       FontMainColor, ALIGN_CENTER, ICON_RIGHT);
        curX += getIconWidth(ICON_RIGHT) + 5;
        drawTextWindow(curX, baseY, 0, gsGlobal->Height - 1, 0,
                       currentTheme.headerText, ALIGN_VCENTER, "Cambiar opción");


        gsKit_queue_exec(gsGlobal);
        gsKit_finish();
        gsKit_sync_flip(gsGlobal);

        // Procesar entradas
        input = waitForInput(-1);

        // CROSS: alterna edición/confirmación del parámetro seleccionado
        if (input & PAD_CROSS) {
            if (!editing) {
                editing = 1;
                editChannel = 0;   // empieza en Alpha
                repeatCounter = 0;
            } else {
                editing = 0;       // confirma y sale de edición
                repeatCounter = 0;
            }
        }
        // START: guardar y salir
        else if (input & PAD_START) {
            saveSkin("mc0:/APP_NHDDL/skin.yaml");
            break;
        }
        // TRIANGLE: salir sin guardar
        else if (input & PAD_TRIANGLE) {
            break;
        }
        // SQUARE: reset a valores por defecto
        else if (input & PAD_SQUARE) {
            setDefaultSkin();
        }
        // Navegación / edición
        else if (!editing) {
            // Mover entre parámetros
            if (input & PAD_UP) {
                selectedIdx = (selectedIdx - 1 + totalFields) % totalFields;
            } else if (input & PAD_DOWN) {
                selectedIdx = (selectedIdx + 1) % totalFields;
            }
            repeatCounter = 0;
        } else {
            // Cambiar canal con L1/R1
            if (input & PAD_L1) {
                editChannel = (editChannel - 1 + 4) % 4;
                repeatCounter = 0;
            } else if (input & PAD_R1) {
                editChannel = (editChannel + 1) % 4;
                repeatCounter = 0;
            }
            // Ajustar valor con Left/Right y aceleración
            else if (input & (PAD_LEFT | PAD_RIGHT)) {
                uint8_t *bytes = (uint8_t*)values[selectedIdx];
                const int channelMap[4] = {3, 2, 1, 0}; // 0=A, 1=B, 2=G, 3=R
                int idx = channelMap[editChannel];

                if (repeatCounter == 0) {
                    if ((input & PAD_LEFT)  && bytes[idx] > 0)   bytes[idx] -= 1;
                    if ((input & PAD_RIGHT) && bytes[idx] < 255) bytes[idx] += 1;
                }
                repeatCounter++;
                if (repeatCounter > repeatDelay && (repeatCounter % repeatSpeed == 0)) {
                    if ((input & PAD_LEFT)  && bytes[idx] > 0)   bytes[idx] -= 1;
                    if ((input & PAD_RIGHT) && bytes[idx] < 255) bytes[idx] += 1;
                }
            } else {
                repeatCounter = 0;
            }
        }
    }
    return res;
}



