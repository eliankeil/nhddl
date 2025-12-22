#include "gui_skin.h"
#include "gui_graphics.h"   // para BGColor, FontMainColor, etc.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ThemeColors currentTheme;

void setDefaultSkin(void) {
    currentTheme.background   = BGColor;
    currentTheme.headerText   = HeaderTextColor;
    currentTheme.listText     = FontMainColor;
    currentTheme.selectedText = ColorSelected;
    currentTheme.optionText   = FontMainColor;
    currentTheme.warnText     = WarnTextColor;
    currentTheme.errorText    = ErrorTextColor;
    currentTheme.coverFrame   = FontMainColor;
    currentTheme.iconFrame    = FontMainColor;
}

// TODO: implementar loadSkin() y saveSkin() con parseo simple de skin.yaml
