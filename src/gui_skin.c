#include "gui_skin.h"
#include "gui_graphics.h" 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpad.h>
#include "skin_layout.h"
#include "pad.h"

extern GSGLOBAL *gsGlobal;

ThemeColors currentTheme;

// Inicializa los colores por defecto en currentTheme
void setDefaultSkin() {
    currentTheme.background   = BGColor;
    currentTheme.headerText   = HeaderTextColor;
    currentTheme.listText     = FontMainColor;
    currentTheme.selectedText = ColorSelected;
    currentTheme.warnText     = WarnTextColor;
    currentTheme.errorText    = ErrorTextColor;
    currentTheme.coverFrame   = ColorGrey;
    currentTheme.iconFrame    = ColorGrey;
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
    fprintf(f, "headerText:   0x%08X # Original: 0x%08X\n", (unsigned int)HeaderTextColor, (unsigned int)HeaderTextColor);
    fprintf(f, "listText:     0x%08X # Original: 0x%08X\n", (unsigned int)FontMainColor, (unsigned int)FontMainColor);
    fprintf(f, "selectedText: 0x%08X # Original: 0x%08X\n", (unsigned int)ColorSelected, (unsigned int)ColorSelected);
    fprintf(f, "warnText:     0x%08X # Original: 0x%08X\n", (unsigned int)WarnTextColor, (unsigned int)WarnTextColor);
    fprintf(f, "errorText:    0x%08X # Original: 0x%08X\n", (unsigned int)ErrorTextColor, (unsigned int)ErrorTextColor);
    fprintf(f, "coverFrame:   0x%08X # Original: 0x%08X\n", (unsigned int)ColorGrey, (unsigned int)ColorGrey);
    fprintf(f, "iconFrame:    0x%08X # Original: 0x%08X\n", (unsigned int)ColorGrey, (unsigned int)ColorGrey);

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
            else if (strcmp(key, "headerText")   == 0) currentTheme.headerText   = value;
            else if (strcmp(key, "listText")     == 0) currentTheme.listText     = value;
            else if (strcmp(key, "selectedText") == 0) currentTheme.selectedText = value;
            else if (strcmp(key, "warnText")     == 0) currentTheme.warnText     = value;
            else if (strcmp(key, "errorText")    == 0) currentTheme.errorText    = value;
            else if (strcmp(key, "coverFrame")   == 0) currentTheme.coverFrame   = value;
            else if (strcmp(key, "iconFrame")    == 0) currentTheme.iconFrame    = value;
        }
    }

    fclose(f);
    return 0;
}

// Editor de Skin con edición por canal ABGR
int uiSkinOptionsLoop() {
    int res = 0;
    int input = 0;
    int selectedIdx = 0;

    // Campos del skin
    const char *fields[] = {
        "background", "headerText", "listText",
        "selectedText", "warnText", "errorText",
        "coverFrame", "iconFrame"
    };
    uint64_t *values[] = {
        &currentTheme.background, &currentTheme.headerText, &currentTheme.listText,
        &currentTheme.selectedText, &currentTheme.warnText, &currentTheme.errorText,
        &currentTheme.coverFrame, &currentTheme.iconFrame
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

        // Footer con acciones
        drawTextWindow(0, gsGlobal->Height - footerHeight,
                       gsGlobal->Width, gsGlobal->Height, 0,
                       currentTheme.headerText, ALIGN_CENTER,
                       "[CROSS] Editar   [START] Guardar   [TRIANGLE] Cancelar   [SQUARE] Reset");

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
              
                gsKit_prim_sprite(gsGlobal, 50, 200, 250, 250, 0, *colorPtr);

                drawTextWindow(0, gsGlobal->Height - footerHeight,
                               gsGlobal->Width, gsGlobal->Height, 0,
                               currentTheme.headerText, ALIGN_CENTER,
                               "[L1/R1] Cambiar canal   [LEFT/RIGHT] Ajustar valor   [START] OK   [TRIANGLE] Cancelar");

                gsKit_queue_exec(gsGlobal);
                gsKit_finish();
                gsKit_sync_flip(gsGlobal);

                int editInput = waitForInput(-1);
                if (editInput & PAD_L1) {
                    channel = (channel - 1 + 4) % 4;
                } else if (editInput & PAD_R1) {
                    channel = (channel + 1) % 4;
                } else if (editInput & PAD_LEFT) {
                    uint8_t *bytes = (uint8_t*)colorPtr;
                    if (bytes[channel] > 0) bytes[channel] -= 1;
                } else if (editInput & PAD_RIGHT) {
                    uint8_t *bytes = (uint8_t*)colorPtr;
                    if (bytes[channel] < 255) bytes[channel] += 1;
                } else if (editInput & PAD_START) {
                    break; // aplicar cambios
                } else if (editInput & PAD_TRIANGLE) {
                    break; // salir sin cambios adicionales
                }
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

