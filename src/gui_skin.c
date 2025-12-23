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
    "Enabled Icon",
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
            char buf[64];
            snprintf(buf, sizeof(buf), "%s: 0x%08X",
                     fields[i], (unsigned int)(*values[i]));
            y = drawText(keepoutArea + 10, y, 0, 0, 0,
                         (i == selectedIdx) ? currentTheme.selectedText : currentTheme.listText,
                         buf);
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
        if (input & PAD_UP) {
            selectedIdx = (selectedIdx - 1 + totalFields) % totalFields;
        } else if (input & PAD_DOWN) {
            selectedIdx = (selectedIdx + 1) % totalFields;
        } else if (input & (PAD_CROSS | PAD_CIRCLE)) {
            // Entrar al submenú de edición por canal
            uint64_t *colorPtr = values[selectedIdx];
            int channel = 0; // 0=A, 1=B, 2=G, 3=R
            static int repeatCounter = 0;
            static const int repeatDelay = 8; // frames antes de empezar a repetir
            static const int repeatSpeed = 2; // cada 2 frames repite

            while (1) {
                gsKit_clear(gsGlobal, currentTheme.background);

                char buf[64];
                snprintf(buf, sizeof(buf), "Edit %s: 0x%08X", fields[selectedIdx], (unsigned int)(*colorPtr));
                drawTextWindow(0, headerHeight, gsGlobal->Width, 0, 0,
                               currentTheme.headerText, ALIGN_HCENTER, buf);

                // Mostrar canal activo
                const char *channels[] = {"Alpha", "Blue", "Green", "Red"};
                snprintf(buf, sizeof(buf), "Canal: %s", channels[channel]);
                drawTextWindow(0, headerHeight + 2*getFontLineHeight(),
                               gsGlobal->Width, 0, 0,
                               currentTheme.listText, ALIGN_HCENTER, buf);

                // Preview del color centrado horizontalmente
                int previewWidth  = 200;   // ancho del cuadro
                int previewHeight = 70;   // alto del cuadro
                int previewY      = 100;   // posición vertical fija

                // cálculo centrado
                int previewX = (gsGlobal->Width - previewWidth) / 2;

                // dibujar sprite centrado
                gsKit_prim_sprite(gsGlobal,
                                  previewX, previewY,                       // esquina superior izquierda
                                  previewX + previewWidth, previewY + previewHeight, // esquina inferior derecha
                                  0, *colorPtr);


                // Footer con íconos en el submenú
                int subBaseY = gsGlobal->Height - footerHeight;
                int subBaseX = keepoutArea + 10;

                // L1/R1 → Cambiar canal
                drawIconWindow(subBaseX, subBaseY, 0, gsGlobal->Height, 0,
                               FontMainColor, ALIGN_CENTER, ICON_L1);
                drawIconWindow(subBaseX + getIconWidth(ICON_L1) + 10, subBaseY, 0, gsGlobal->Height, 0,
                               FontMainColor, ALIGN_CENTER, ICON_R1);
                drawTextWindow(subBaseX + getIconWidth(ICON_L1) + getIconWidth(ICON_R1) + 20, subBaseY,
                               0, gsGlobal->Height - 1, 0,
                               currentTheme.headerText, ALIGN_VCENTER, "Cambiar canal");

                // LEFT/RIGHT → Ajustar valor
                drawIconWindow(subBaseX + 250, subBaseY, 0, gsGlobal->Height, 0,
                               FontMainColor, ALIGN_CENTER, ICON_LEFT);
                drawIconWindow(subBaseX + 250 + getIconWidth(ICON_LEFT) + 10, subBaseY, 0, gsGlobal->Height, 0,
                               FontMainColor, ALIGN_CENTER, ICON_RIGHT);
                drawTextWindow(subBaseX + 250 + getIconWidth(ICON_LEFT) + getIconWidth(ICON_RIGHT) + 20, subBaseY,
                               0, gsGlobal->Height - 1, 0,
                               currentTheme.headerText, ALIGN_VCENTER, "Ajustar valor");

                // START → OK
                drawIconWindow(subBaseX + 500, subBaseY, 0, gsGlobal->Height, 0,
                               FontMainColor, ALIGN_CENTER, ICON_START);
                drawTextWindow(subBaseX + 500 + getIconWidth(ICON_START) + 5, subBaseY,
                               0, gsGlobal->Height - 1, 0,
                               currentTheme.headerText, ALIGN_VCENTER, "OK");

                // TRIANGLE → Cancelar
                drawIconWindow(subBaseX + 700, subBaseY, 0, gsGlobal->Height, 0,
                               FontMainColor, ALIGN_CENTER, ICON_TRIANGLE);
                drawTextWindow(subBaseX + 700 + getIconWidth(ICON_TRIANGLE) + 5, subBaseY,
                               0, gsGlobal->Height - 1, 0,
                               currentTheme.headerText, ALIGN_VCENTER, "Cancelar");

                gsKit_queue_exec(gsGlobal);
                gsKit_finish();
                gsKit_sync_flip(gsGlobal);

                // Usamos pollInput() para permitir repetición al mantener presionado
                static int prevInput = 0; // estado anterior del mando
                int editInput = pollInput();

                static int repeatCounter = 0;
                static const int repeatDelay = 30; // ~1 segundo a 60fps
                static const int repeatSpeed = 2;  // cada 2 frames después del delay

                // --- L1/R1: solo una vez por pulsación ---
                if ((editInput & PAD_L1) && !(prevInput & PAD_L1)) {
                    channel = (channel - 1 + 4) % 4;
                }
                else if ((editInput & PAD_R1) && !(prevInput & PAD_R1)) {
                    channel = (channel + 1) % 4;
                }

                // --- LEFT/RIGHT: repetición controlada ---
                else if (editInput & (PAD_LEFT | PAD_RIGHT)) {
                    uint8_t *bytes = (uint8_t*)colorPtr;
                    const int channelMap[4] = {3, 2, 1, 0}; // 0=A, 1=B, 2=G, 3=R
                    int idx = channelMap[channel];

                    // Primer ajuste inmediato (solo una vez al presionar)
                    if (repeatCounter == 0) {
                        if ((editInput & PAD_LEFT) && bytes[idx] > 0) bytes[idx] -= 1;
                        if ((editInput & PAD_RIGHT) && bytes[idx] < 255) bytes[idx] += 1;
                    }

                    // Incrementar contador mientras se mantiene presionado
                    repeatCounter++;

                    // Después del delay, aplicar repetición acelerada
                    if (repeatCounter > repeatDelay && (repeatCounter % repeatSpeed == 0)) {
                        if ((editInput & PAD_LEFT) && bytes[idx] > 0) bytes[idx] -= 1;
                        if ((editInput & PAD_RIGHT) && bytes[idx] < 255) bytes[idx] += 1;
                    }
                }
                else {
                    // Resetear contador si no hay tecla presionada
                    repeatCounter = 0;
                }

                // --- START/TRIANGLE: salir ---
                if (editInput & PAD_START) {
                    break; // aplicar cambios
                }
                else if (editInput & PAD_TRIANGLE) {
                    break; // salir sin cambios adicionales
                }

                // actualizar estado anterior
                prevInput = editInput;
                        }
                    } else if (input & PAD_SQUARE) {
                        setDefaultSkin(); // Reset a valores por defecto
                    } else if (input & PAD_START) {
                        saveSkin("mc0:/APP_NHDDL/skin.yaml");
                        break; // salir guardando
                    } else if (input & PAD_TRIANGLE) {
                        break; // salir sin guardar
                    }
                }
return res;
}


