#include "gui_skin.h"
#include "gui_graphics.h"   // para BGColor, FontMainColor, etc.
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


// TODO: implementar loadSkin() y saveSkin() con parseo simple de skin.yaml
