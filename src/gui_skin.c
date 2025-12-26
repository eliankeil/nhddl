#include "gui_skin.h"
#include "gui.h"
#include "gui_graphics.h"
#include "gui_icons.h"
#include "pad.h"
#include "skin_layout.h"
#include <libpad.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern GSGLOBAL *gsGlobal;

ThemeColors currentTheme;

// Inicializa los colores por defecto en currentTheme
void setDefaultSkin() {
  currentTheme.background = BGColor;
  currentTheme.listText = FontMainColor;
  currentTheme.selectedText = ColorSelected;
  currentTheme.headerText = HeaderTextColor;
  currentTheme.iconEnabled = IconEnabled;
  currentTheme.coverFrame = FontMainColor;
  currentTheme.iconCircle = IconCircle;
  currentTheme.iconCross = IconCross;
  currentTheme.iconSquare = IconSquare;
  currentTheme.iconTriangle = IconTriangle;
  currentTheme.iconPad = IconPad;
}

// Genera skin.yaml con los valores por defecto y comentarios
int saveSkin(const char *path) {
  FILE *f = fopen(path, "w");
  if (!f) {
    printf("ERROR: No se pudo crear skin.yaml en %s\n", path);
    return -1;
  }

  fprintf(f, "# Colores en formato ABGR (0xAABBGGRR)\n\n");

  fprintf(f, "background:   0x%08X\n", (unsigned int)currentTheme.background);
  fprintf(f, "listText:     0x%08X\n", (unsigned int)currentTheme.listText);
  fprintf(f, "selectedText: 0x%08X\n", (unsigned int)currentTheme.selectedText);
  fprintf(f, "headerText:   0x%08X\n", (unsigned int)currentTheme.headerText);
  fprintf(f, "iconEnabled:  0x%08X\n", (unsigned int)currentTheme.iconEnabled);
  fprintf(f, "coverFrame:   0x%08X\n", (unsigned int)currentTheme.coverFrame);
  fprintf(f, "iconCircle:     0x%08X\n", (unsigned int)currentTheme.iconCircle);
  fprintf(f, "iconCross:    0x%08X\n", (unsigned int)currentTheme.iconCross);
  fprintf(f, "iconSquare:    0x%08X\n", (unsigned int)currentTheme.iconSquare);
  fprintf(f, "iconTriangle:    0x%08X\n",
          (unsigned int)currentTheme.iconTriangle);
  fprintf(f, "iconPad:    0x%08X\n", (unsigned int)currentTheme.iconPad);

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
      if (strcmp(key, "background") == 0)
        currentTheme.background = value;
      else if (strcmp(key, "listText") == 0)
        currentTheme.listText = value;
      else if (strcmp(key, "selectedText") == 0)
        currentTheme.selectedText = value;
      else if (strcmp(key, "headerText") == 0)
        currentTheme.headerText = value;
      else if (strcmp(key, "iconEnabled") == 0)
        currentTheme.iconEnabled = value;
      else if (strcmp(key, "coverFrame") == 0)
        currentTheme.coverFrame = value;
      else if (strcmp(key, "iconCircle") == 0)
        currentTheme.iconCircle = value;
      else if (strcmp(key, "iconCross") == 0)
        currentTheme.iconCross = value;
      else if (strcmp(key, "iconSquare") == 0)
        currentTheme.iconSquare = value;
      else if (strcmp(key, "iconTriangle") == 0)
        currentTheme.iconTriangle = value;
      else if (strcmp(key, "iconPad") == 0)
        currentTheme.iconPad = value;
    }
  }

  fclose(f);
  return 0;
}

// Campos del skin (labels visibles en pantalla)
const char *fields[] = {
    "Background",  "Titles|Options Text", "Selected Text", "Secondary Text",
    "Enable Icon", "Cover Art Border",    "Circle Icon",   "Cross Icon",
    "Square Icon", "Triangle Icon",       "Pad Icon"};

uint64_t *values[] = {&currentTheme.background,   &currentTheme.listText,
                      &currentTheme.selectedText, &currentTheme.headerText,
                      &currentTheme.iconEnabled,  &currentTheme.coverFrame,
                      &currentTheme.iconCircle,   &currentTheme.iconCross,
                      &currentTheme.iconSquare,   &currentTheme.iconTriangle,
                      &currentTheme.iconPad};

int totalFields = sizeof(fields) / sizeof(fields[0]);

ExitCode uiSkinOptionsLoop(void) {
  int input = 0;
  int selectedIdx = 0;
  int editing = 0;     // 0 = navegando, 1 = editando parámetro
  int editChannel = 0; // 0=A, 1=B, 2=G, 3=R
  int repeatCounter = 0;
  int prevInput = 0;
  const int repeatDelay = 20; // ~1s a 60fps
  const int repeatSpeed = 2;  // cada 2 frames después del delay

  // … resto del cuerpo de la función …

  // Defaults por índice de parámetro (en orden de fields[])
  const uint64_t defaults[11] = {
      (uint64_t)BGColor,         // background
      (uint64_t)FontMainColor,   // listText
      (uint64_t)ColorSelected,   // selectedText
      (uint64_t)HeaderTextColor, // headerText
      (uint64_t)IconEnabled,     // iconEnabled
      (uint64_t)FontMainColor,   // coverFrame
      (uint64_t)IconCircle,      (uint64_t)IconCross, (uint64_t)IconSquare,
      (uint64_t)IconTriangle,    (uint64_t)IconPad};

  while (1) {
    gsKit_clear(gsGlobal, currentTheme.background);

    // Header fijo
    drawTextWindow(0, headerHeight - getFontLineHeight(), gsGlobal->Width, 0, 0,
                   currentTheme.headerText, ALIGN_HCENTER, "Skin Editor");

    // Lista de parámetros centrados con margen fijo
    int lineSpacing = getFontLineHeight() + 10; // margen fijo entre filas
    int blockHeight =
        (totalFields + 1) * lineSpacing; // +1 por el encabezado ABGR

    // Y donde termina el título en el header
    int topInnerY = headerHeight;

    // Y donde empiezan los íconos en el footer
    int bottomInnerY = gsGlobal->Height - footerHeight;

    // Altura disponible entre título y footer
    int availableHeight = bottomInnerY - topInnerY;

    // Centrar bloque completo en ese espacio
    int startY = topInnerY + (availableHeight - blockHeight) / 2;

    // Encabezado de canales en startY
    int headerY = startY;
    int y = startY + lineSpacing; // parámetros debajo del encabezado

    // Dibujar encabezado de canales ABGR
    const char *channelLabels[4] = {"Alpha", "Blue", "Green", "Red"};
    uint64_t channelColors[4] = {
        0xFF00357D, // Alpha resaltado en gris oscuro
        0xFFFF0000, // Blue resaltado en azul (ABGR)
        0xFF00FF00, // Green resaltado en verde
        0xFF0000FF  // Red resaltado en rojo
    };

    // Calcular posición centrada para el encabezado
    char headerBuf[64];
    snprintf(headerBuf, sizeof(headerBuf), "%s   %s   %s   %s",
             channelLabels[0], channelLabels[1], channelLabels[2],
             channelLabels[3]);

    int headerWidth = (int)getLineWidth(headerBuf);
    int headerX = (gsGlobal->Width - headerWidth) / 2;

    // Dibujar cada palabra con su color, limpiando detrás
    int curXHeader = headerX;
    for (int c = 0; c < 4; c++) {
      uint64_t color = currentTheme.headerText; // por defecto
      if (editing) {
        if (editChannel == c) {
          color = channelColors[c]; // canal activo resaltado
        } else {
          color = 0x80303030; // canales no seleccionados en edición
        }
      }

      int textW = getLineWidth(channelLabels[c]);
      // limpiar área exacta detrás de la palabra
      gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;
      gsKit_prim_sprite(gsGlobal, curXHeader, headerY, curXHeader + textW,
                        headerY + getFontLineHeight(), 0,
                        currentTheme.background);
      gsGlobal->PrimAlphaEnable = GS_SETTING_ON;

      drawText(curXHeader, headerY, 0, 0, 0, color, channelLabels[c]);
      curXHeader += textW + getLineWidth("   ");
    }

    for (int i = 0; i < totalFields; i++) {
      // Descomponer color en ABGR
      uint32_t color32 = (uint32_t)(*values[i]);
      uint8_t a = (color32 >> 24) & 0xFF;
      uint8_t b = (color32 >> 16) & 0xFF;
      uint8_t g = (color32 >> 8) & 0xFF;
      uint8_t r = (color32) & 0xFF;

      // Preparar strings individuales para cada canal
      char chanStr[4][8];
      snprintf(chanStr[0], sizeof(chanStr[0]), "%02X", a);
      snprintf(chanStr[1], sizeof(chanStr[1]), "%02X", b);
      snprintf(chanStr[2], sizeof(chanStr[2]), "%02X", g);
      snprintf(chanStr[3], sizeof(chanStr[3]), "%02X", r);

      // Calcular ancho total y posición centrada
      char buf[32];
      snprintf(buf, sizeof(buf), "%02X   :   %02X   :   %02X   :   %02X", a, b,
               g, r);
      int lineWidth = (int)getLineWidth(buf);
      int centerX = (gsGlobal->Width - lineWidth) / 2;

      // limpiar área de la fila antes de dibujar
      gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;
      gsKit_prim_sprite(gsGlobal, 0, y, gsGlobal->Width, y + lineSpacing, 0,
                        currentTheme.background);
      gsGlobal->PrimAlphaEnable = GS_SETTING_ON;

      // Determinar colores para el título
      uint64_t nameColor;
      if (editing) {
        nameColor = (i == selectedIdx) ? currentTheme.selectedText : 0x80303030;
      } else {
        nameColor = (i == selectedIdx) ? currentTheme.selectedText
                                       : currentTheme.listText;
      }

      // Dibujar título alineado con valores
      drawText(keepoutArea + 10, y, 0, 0, 0, nameColor, fields[i]);

      // Dibujar iconEnabled alineado con el título
      if (editing && i == selectedIdx) {
        drawIconWindow(keepoutArea - getIconWidth(ICON_ENABLED) - 2, y, 0,
                       y + getFontLineHeight(), 0, currentTheme.iconEnabled,
                       ALIGN_VCENTER, ICON_ENABLED);
      }

      // Dibujar valores canal por canal respetando espaciado
      int curX = centerX;
      for (int c = 0; c < 4; c++) {
        uint64_t chanColor;

        if (editing) {
          if (i == selectedIdx && editChannel == c) {
            chanColor =
                channelColors[c]; // canal activo del parámetro seleccionado
          } else if (i == selectedIdx) {
            chanColor =
                currentTheme
                    .headerText; // parámetro seleccionado pero canal no activo
          } else {
            chanColor = 0x80303030; // parámetros no seleccionados
          }
        } else {
          chanColor = (i == selectedIdx) ? currentTheme.selectedText
                                         : currentTheme.listText;
        }

        drawText(curX, y, 0, 0, 0, chanColor, chanStr[c]);
        curX += getLineWidth(chanStr[c]);

        if (c < 3) {
          drawText(curX, y, 0, 0, 0, currentTheme.listText, "   :   ");
          curX += getLineWidth("   :   ");
        }
      }

      // Dibujar preview o ícono alineado con los valores
      int prevW = 140;
      int prevH = getFontLineHeight();
      int prevX = centerX + lineWidth + 20;
      int prevY = y;

      if (editing && i == selectedIdx) {
        // Estamos editando este parámetro → mostrar ícono en lugar del preview
        if (i == 6) { // Circle Icon
          drawIconWindow(prevX, prevY, 0, prevY + prevH, 0,
                         currentTheme.iconCircle, ALIGN_VCENTER, ICON_CIRCLE);
        } else if (i == 7) { // Cross Icon
          drawIconWindow(prevX, prevY, 0, prevY + prevH, 0,
                         currentTheme.iconCross, ALIGN_VCENTER, ICON_CROSS);
        } else if (i == 8) { // Square Icon
          drawIconWindow(prevX, prevY, 0, prevY + prevH, 0,
                         currentTheme.iconSquare, ALIGN_VCENTER, ICON_SQUARE);
        } else if (i == 9) { // Triangle Icon
          drawIconWindow(prevX, prevY, 0, prevY + prevH, 0,
                         currentTheme.iconTriangle, ALIGN_VCENTER,
                         ICON_TRIANGLE);
        } else if (i == 10) { // Pad Icon → dibujar dos íconos
          drawIconWindow(prevX, prevY, 0, prevY + prevH, 0,
                         currentTheme.iconPad, ALIGN_VCENTER, ICON_UPDOWN);
          drawIconWindow(prevX + getIconWidth(ICON_UPDOWN) + 10, prevY, 0,
                         prevY + prevH, 0, currentTheme.iconPad, ALIGN_VCENTER,
                         ICON_LEFTRIGHT);
        } else {
          // otros parámetros → preview normal
          gsKit_prim_sprite(gsGlobal, prevX, prevY, prevX + prevW,
                            prevY + prevH, 0, (uint64_t)(*values[i]));
        }
      } else {
        // No estamos editando → siempre preview normal
        gsKit_prim_sprite(gsGlobal, prevX, prevY, prevX + prevW, prevY + prevH,
                          0, (uint64_t)(*values[i]));
      }

      // Avanzar a la siguiente línea con margen fijo
      y += lineSpacing;
    }

    // Footer dinámico según estado
    int baseY = gsGlobal->Height - footerHeight;

    if (!editing) {
      // Estado navegación
      const char *msgCross = "Edit";
      const char *msgStart = "Save";
      const char *msgTriangle = "Cancel";
      const char *msgSquare = "Reset";
      const char *msgNavigate = "Navigate";

      int totalWidth =
          getIconWidth(ICON_CROSS) + 5 + getLineWidth(msgCross) + 40 +
          getIconWidth(ICON_START) + 5 + getLineWidth(msgStart) + 40 +
          getIconWidth(ICON_TRIANGLE) + 5 + getLineWidth(msgTriangle) + 40 +
          getIconWidth(ICON_SQUARE) + 5 + getLineWidth(msgSquare) + 40 +
          getIconWidth(ICON_UPDOWN) + 5 + getLineWidth(msgNavigate) + 40;

      int curX = (gsGlobal->Width - totalWidth) / 2;

      // CROSS → Edit
      drawIconWindow(curX, baseY, 0, gsGlobal->Height, 0,
                     currentTheme.iconCross, ALIGN_CENTER, ICON_CROSS);
      curX += getIconWidth(ICON_CROSS) + 5;
      drawTextWindow(curX, baseY, 0, gsGlobal->Height - 1, 0,
                     currentTheme.headerText, ALIGN_VCENTER, msgCross);
      curX += getLineWidth(msgCross) + 40;

      // START → Save
      drawIconWindow(curX, baseY, 0, gsGlobal->Height, 0, FontMainColor,
                     ALIGN_CENTER, ICON_START);
      curX += getIconWidth(ICON_START) + 5;
      drawTextWindow(curX, baseY, 0, gsGlobal->Height - 1, 0,
                     currentTheme.headerText, ALIGN_VCENTER, msgStart);
      curX += getLineWidth(msgStart) + 40;

      // TRIANGLE → Cancel
      drawIconWindow(curX, baseY, 0, gsGlobal->Height, 0,
                     currentTheme.iconTriangle, ALIGN_CENTER, ICON_TRIANGLE);
      curX += getIconWidth(ICON_TRIANGLE) + 5;
      drawTextWindow(curX, baseY, 0, gsGlobal->Height - 1, 0,
                     currentTheme.headerText, ALIGN_VCENTER, msgTriangle);
      curX += getLineWidth(msgTriangle) + 40;

      // SQUARE → Reset
      drawIconWindow(curX, baseY, 0, gsGlobal->Height, 0,
                     currentTheme.iconSquare, ALIGN_CENTER, ICON_SQUARE);
      curX += getIconWidth(ICON_SQUARE) + 5;
      drawTextWindow(curX, baseY, 0, gsGlobal->Height - 1, 0,
                     currentTheme.headerText, ALIGN_VCENTER, msgSquare);
      curX += getLineWidth(msgSquare) + 40;

      // UP/DOWN → Navigate
      drawIconWindow(curX, baseY, 0, gsGlobal->Height, 0, currentTheme.iconPad,
                     ALIGN_CENTER, ICON_UPDOWN);
      curX += getIconWidth(ICON_UPDOWN) + 5;
      drawTextWindow(curX, baseY, 0, gsGlobal->Height - 1, 0,
                     currentTheme.headerText, ALIGN_VCENTER, msgNavigate);
      curX += getLineWidth(msgNavigate) + 40;

    } else {
      // Estado edición
      const char *msgCross = "Back";
      const char *msgColor = "Channel"; // LEFT/RIGHT → canal
      const char *msgValue = "Value";   // UP/DOWN   → valor
      const char *msgSquare = "Reset";

      int totalWidth =
          getIconWidth(ICON_CROSS) + 5 + getLineWidth(msgCross) + 40 +
          getIconWidth(ICON_LEFTRIGHT) + 5 + getLineWidth(msgColor) + 40 +
          getIconWidth(ICON_UPDOWN) + 5 + getLineWidth(msgValue) + 40 +
          getIconWidth(ICON_SQUARE) + 5 + getLineWidth(msgSquare) + 40;

      int curX = (gsGlobal->Width - totalWidth) / 2;

      // CROSS → Back
      drawIconWindow(curX, baseY, 0, gsGlobal->Height, 0,
                     currentTheme.iconCross, ALIGN_CENTER, ICON_CROSS);
      curX += getIconWidth(ICON_CROSS) + 5;
      drawTextWindow(curX, baseY, 0, gsGlobal->Height - 1, 0,
                     currentTheme.headerText, ALIGN_VCENTER, msgCross);
      curX += getLineWidth(msgCross) + 40;

      // LEFT/RIGHT → Color
      drawIconWindow(curX, baseY, 0, gsGlobal->Height, 0, currentTheme.iconPad,
                     ALIGN_CENTER, ICON_LEFTRIGHT);
      curX += getIconWidth(ICON_LEFTRIGHT) + 5;
      drawTextWindow(curX, baseY, 0, gsGlobal->Height - 1, 0,
                     currentTheme.headerText, ALIGN_VCENTER, msgColor);
      curX += getLineWidth(msgColor) + 40;

      // UP/DOWN → Value
      drawIconWindow(curX, baseY, 0, gsGlobal->Height, 0, currentTheme.iconPad,
                     ALIGN_CENTER, ICON_UPDOWN);
      curX += getIconWidth(ICON_UPDOWN) + 5;
      drawTextWindow(curX, baseY, 0, gsGlobal->Height - 1, 0,
                     currentTheme.headerText, ALIGN_VCENTER, msgValue);
      curX += getLineWidth(msgValue) + 40;

      // SQUARE → Reset
      drawIconWindow(curX, baseY, 0, gsGlobal->Height, 0,
                     currentTheme.iconSquare, ALIGN_CENTER, ICON_SQUARE);
      curX += getIconWidth(ICON_SQUARE) + 5;
      drawTextWindow(curX, baseY, 0, gsGlobal->Height - 1, 0,
                     currentTheme.headerText, ALIGN_VCENTER, msgSquare);
      curX += getLineWidth(msgSquare) + 40;
    }

    gsKit_queue_exec(gsGlobal);
    gsKit_finish();
    gsKit_sync_flip(gsGlobal);

    // Procesar entradas
    if (!editing) {
      int input = pollInput(); // usar pollInput en lugar de waitForInput

      // calcular flancos
      int pressed = (input & ~prevInput);

      // CROSS: alterna edición/confirmación del parámetro seleccionado
      if (pressed & PAD_CROSS) {
        editing = 1;
        editChannel = 0; // empieza en Alpha
        repeatCounter = 0;
        prevInput = input;
        continue;
      }
      // START: guardar y salir (flanco)
      else if (pressed & PAD_START) {
        saveSkin("mc0:/APP_NHDDL/skin.yaml");
        return EXIT_SAVE;
      }
      // TRIANGLE: salir sin guardar (flanco)
      else if (pressed & PAD_TRIANGLE) {
        return EXIT_CANCEL;
      }
      // SQUARE: reset a todos los valores por defecto (flanco)
      else if (pressed & PAD_SQUARE) {
        setDefaultSkin();
      }
      // Navegación entre parámetros (flanco)
      else if (pressed & PAD_UP) {
        selectedIdx = (selectedIdx - 1 + totalFields) % totalFields;
        repeatCounter = 0;
      } else if (pressed & PAD_DOWN) {
        selectedIdx = (selectedIdx + 1) % totalFields;
        repeatCounter = 0;
      } else {
        repeatCounter = 0;
      }

      // actualizar estado anterior
      prevInput = input;
    } else {
      // EDITANDO: usar pollInput para permitir aceleración continua
      int editInput = pollInput();

      // CROSS: confirmar edición, guardar y salir (flanco)
      if ((editInput & PAD_CROSS) && !(prevInput & PAD_CROSS)) {
        editing = 0; // salir de modo edición
        repeatCounter = 0;
        prevInput = editInput;
        continue;
      }

      // TRIANGLE: salir sin guardar (flanco)
      if ((editInput & PAD_TRIANGLE) && !(prevInput & PAD_TRIANGLE)) {
        editing = 0;
        repeatCounter = 0;
        prevInput = editInput;
        continue;
      }

      // SQUARE: reset SOLO el parámetro seleccionado (flanco)
      if ((editInput & PAD_SQUARE) && !(prevInput & PAD_SQUARE)) {
        *values[selectedIdx] = defaults[selectedIdx];
        repeatCounter = 0;
        prevInput = editInput;
        continue;
      }

      // LEFT/RIGHT: cambiar canal activo (flanco único)
      if ((editInput & PAD_LEFT) && !(prevInput & PAD_LEFT)) {
        editChannel = (editChannel - 1 + 4) % 4;
        repeatCounter = 0;
      } else if ((editInput & PAD_RIGHT) && !(prevInput & PAD_RIGHT)) {
        editChannel = (editChannel + 1) % 4;
        repeatCounter = 0;
      }

      // UP/DOWN: ajustar valor con flanco + aceleración
      if (editInput & (PAD_DOWN | PAD_UP)) {
        int leftEdge = (editInput & PAD_DOWN) && !(prevInput & PAD_DOWN);
        int rightEdge = (editInput & PAD_UP) && !(prevInput & PAD_UP);

        uint8_t *bytes = (uint8_t *)values[selectedIdx];
        const int channelMap[4] = {3, 2, 1, 0}; // 0=A, 1=B, 2=G, 3=R
        int idx = channelMap[editChannel];

        // paso inmediato por flanco
        if (leftEdge && bytes[idx] > 0)
          bytes[idx] -= 1;
        if (rightEdge && bytes[idx] < 255)
          bytes[idx] += 1;

        // acelerar al mantener presionado
        if ((editInput & PAD_DOWN) || (editInput & PAD_UP)) {
          if (leftEdge || rightEdge)
            repeatCounter = 0;
          else
            repeatCounter++;

          if (repeatCounter > repeatDelay &&
              (repeatCounter % repeatSpeed == 0)) {
            if ((editInput & PAD_DOWN) && bytes[idx] > 0)
              bytes[idx] -= 1;
            if ((editInput & PAD_UP) && bytes[idx] < 255)
              bytes[idx] += 1;
          }
        }
      } else {
        repeatCounter = 0;
      }

      // actualizar estado anterior
      prevInput = editInput;
    }
  }
  return EXIT_NONE;
}
