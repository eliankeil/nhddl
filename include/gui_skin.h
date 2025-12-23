#ifndef _GUI_SKIN_H_
#define _GUI_SKIN_H_
#include <stdint.h>

typedef struct {
    uint64_t background;
    uint64_t listText;
    uint64_t selectedText;
    uint64_t headerText;
    uint64_t iconEnabled;
    uint64_t coverFrame;
    uint64_t warnText;
    uint64_t errorText;
} ThemeColors;

extern ThemeColors currentTheme;

void setDefaultSkin(void);
int  loadSkin(const char *path);
int  saveSkin(const char *path);

#endif
