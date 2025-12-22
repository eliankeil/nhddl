#include "gui_skin.h"
#include "gui_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ThemeColors currentTheme;

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
