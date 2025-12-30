#include "gui.h"
#include "common.h"
#include "gui_args.h"
#include "gui_frames.h"
#include "gui_graphics.h"
#include "gui_icons.h"
#include "gui_skin.h"
#include "launcher.h"
#include "options.h"
#include "pad.h"
#include "skin_layout.h"
#include <dmaKit.h>
#include <gsKit.h>
#include <gsToolkit.h>
#include <kernel.h>
#include <libpad.h>
#include <malloc.h>
#include <ps2sdkapi.h>
#include <stdint.h>
#include <stdio.h>

#define DIV_ROUND(n, d) (n + (d - 1)) / d

// Assuming 140x200 cover art
#define COVER_ART_RES_W 140
#define COVER_ART_RES_H 200

void closeUI();
int uiLoop(TargetList *titles);
int uiTitleOptionsLoop(Target *title);
int uiArgumentListLoop(Target *target, ArgumentList *titleArguments);
void drawTitleList(TargetList *titles, int selectedTitleIdx,
                   int maxTitlesPerPage, GSTEXTURE *selectedTitleCover);
void uiLaunchTitle(Target *target, ArgumentList *arguments);
void drawGameID(const char *game_id);
int createSplashThread();
void uiSplashThread();
void closeUISplashThread();

GSGLOBAL *gsGlobal;
static GSTEXTURE *coverTexture;
static GSTEXTURE *icoTexture;
static char lineBuffer[255];

// Path relative to storage device mountpoint.
// Used to load cover art
static const char artPath[] = "/ART";

// Cover art sprite coordinates
// Initialized during uiInit from screen width and height
static int coverArtX2;
static int coverArtY2;
static int coverArtX1;
static int coverArtY1;

const int keepoutArea = 20;
const int headerHeight = 20 + keepoutArea;
const int footerHeight = 40 + keepoutArea;

void initVMode(GSGLOBAL *gsGlobal) {
  switch (LAUNCHER_OPTIONS.vmode) {
  case GS_MODE_NTSC:
    printf("Forcing NTSC mode\n");
    gsGlobal->Mode = GS_MODE_NTSC;
    gsGlobal->Interlace = GS_INTERLACED;
    gsGlobal->Field = GS_FIELD;
    gsGlobal->Width = 640;
    gsGlobal->Height = 448;
    break;
  case GS_MODE_PAL:
    printf("Forcing PAL mode\n");
    gsGlobal->Mode = GS_MODE_PAL;
    gsGlobal->Interlace = GS_INTERLACED;
    gsGlobal->Field = GS_FIELD;
    gsGlobal->Width = 640;
    gsGlobal->Height = 512;
    break;
  case GS_MODE_DTV_480P:
    printf("Forcing 480p mode\n");
    gsGlobal->Mode = GS_MODE_DTV_480P;
    gsGlobal->Interlace = GS_NONINTERLACED;
    gsGlobal->Field = GS_FRAME;
    gsGlobal->Width = 640;
    gsGlobal->Height = 448;
    break;
  default:
  }
}

int uiInit() {
  setDefaultSkin();
  if (loadSkin("mc0:/APP_NHDDL/skin.yaml") < 0) {
    saveSkin("mc0:/APP_NHDDL/skin.yaml");
  }
  if (gsGlobal != NULL) {
    printf("Reinitializing UI\n");
    closeUI();
  }
  gsGlobal = gsKit_init_global();
  initVMode(gsGlobal);
  gsGlobal->PSM = GS_PSM_CT32; // Set color depth to avoid PAL VRAM issues
  gsGlobal->PSMZ = GS_PSMZ_16S;
  gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
  gsGlobal->DoubleBuffering = GS_SETTING_ON;
  // Setup TEST register to ignore fully transparent pixels
  gsGlobal->Test->ATST = 7; // Set alpha test method to NOTEQUAL (pixels with A
                            // not equal to AREF pass)
  gsGlobal->Test->AREF = 0x00; // Set reference value to 0x00 (transparent)
  gsGlobal->Test->AFAIL = 0;   // Don't update buffers when test fails

  dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
              D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

  // Initialize the DMAC
  int res;
  if ((res = dmaKit_chan_init(DMA_CHANNEL_GIF))) {
    printf("ERROR: Failed to initlize DMAC: %d\n", res);
    return res;
  }

  gsKit_clear(gsGlobal, currentTheme.background);

  // Init screen
  gsKit_vram_clear(gsGlobal);
  gsKit_init_screen(gsGlobal);
  gsKit_display_buffer(
      gsGlobal); // Switch display buffer to avoid garbage appearing on screen
  gsKit_TexManager_init(gsGlobal);
  // Set alpha and mode, clear active buffer
  gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
  gsKit_set_test(gsGlobal, GS_ATEST_ON);
  gsKit_mode_switch(gsGlobal, GS_ONESHOT);
  gsKit_clear(gsGlobal, currentTheme.background);

  // Initialize resources
  if (initGraphics()) {
    printf("ERROR: Failed to initialize font\n");
    return -1;
  };
gsKit_TexManager_bind(gsGlobal, icoTexture);

  // Init cover texture
  coverTexture = calloc(sizeof(GSTEXTURE), 1);
  coverArtX2 = (gsGlobal->Width - keepoutArea - 10);
  coverArtY2 = (gsGlobal->Height / 2) + (COVER_ART_RES_H / 2);
  coverArtX1 = coverArtX2 - COVER_ART_RES_W;
  coverArtY1 = coverArtY2 - COVER_ART_RES_H;
  coverTexture->Delayed = 1;
icoTexture = calloc(sizeof(GSTEXTURE), 1);
icoTexture->Delayed = 1;

  return 0;
}

int loadCoverArt(struct DeviceMapEntry *device, char *titleID) {
    if (device->metadev) {
        device = device->metadev;
    }

    // Cover art
    snprintf(lineBuffer, 255, "%s%s/%s_COV.png", device->mountpoint, artPath, titleID);
    gsKit_TexManager_invalidate(gsGlobal, coverTexture);
    if (gsKit_texture_png(gsGlobal, coverTexture, lineBuffer)) {
        return -1;
    }
    gsKit_TexManager_bind(gsGlobal, coverTexture);
    //free(coverTexture->Mem);
    //coverTexture->Mem = NULL;

    // 🔹 ICO art
    snprintf(lineBuffer, 255, "%s%s/%s_ICO.png", device->mountpoint, artPath, titleID);
    gsKit_TexManager_invalidate(gsGlobal, icoTexture);
    if (gsKit_texture_png(gsGlobal, icoTexture, lineBuffer)) {
        return -1;
    }
    gsKit_TexManager_bind(gsGlobal, icoTexture);
    // 🔹 NO liberar Mem aquí, porque necesitamos el canal alfa intacto
    // icoTexture->Delayed = 1; // opcional, igual que coverTexture

    return 0;
}


// Frees textures and deinits gsKit
void closeUI() {
  gsKit_vram_clear(gsGlobal);
  closeFont();
  free(coverTexture);
  gsKit_deinit_global(gsGlobal);
}

// Main UI loop. Displays the target list.
int uiLoop(TargetList *titles) {
  // Reinitialize UI if video mode doesn't match
  if ((LAUNCHER_OPTIONS.vmode != VMODE_NONE) &&
      (gsGlobal->Mode != LAUNCHER_OPTIONS.vmode)) {
    uiInit();
  }

  int res = 0;
  if ((gsGlobal == NULL) && (res = uiInit())) {
    printf("ERROR: Failed to init UI: %d\n", res);
    goto exit;
  }
  // Init gamepad inputs
  initPad();

  int isCoverUninitialized = 1;
  int selectedTitleIdx = 0;
  int maxTitlesPerPage =
      (gsGlobal->Height - (headerHeight + footerHeight)) / getFontLineHeight() -
      1;
  Target *curTarget = titles->first;

  // Get last launched title and find it in the target list
  char *lastTitle = calloc(sizeof(char), PATH_MAX + 1);
  if (!getLastLaunchedTitle(lastTitle)) {
    int mountpointLen;
    while (curTarget != NULL) {
      // Compare paths without the mountpoint
      mountpointLen = getRelativePathIdx(curTarget->fullPath);
      if (mountpointLen == -1)
        mountpointLen = 0;

      if (!strcmp(lastTitle, &curTarget->fullPath[mountpointLen])) {
        selectedTitleIdx = curTarget->idx;
        break;
      }
      curTarget = curTarget->next;
    }
    // Reinitialize target if last launched title couldn't be loaded
    if (curTarget == NULL) {
      curTarget = titles->first;
    }
  }
  free(lastTitle);

  // Load cover art
  isCoverUninitialized = loadCoverArt(curTarget->device, curTarget->id);

  // Main UI loop
  int frameCount = 0;
  int prevInput = 0;
  int input = 0;
  int repeatCounter = 0;
  const int repeatDelay = 20; // ~1s a 60fps
  const int repeatSpeed = 2;  // cada 2 frames después del delay

  while (1) {
    gsKit_clear(gsGlobal, currentTheme.background);
    gsKit_TexManager_nextFrame(gsGlobal);

    // Reload target if index has changed
    if (curTarget->idx != selectedTitleIdx) {
      curTarget = getTargetByIdx(titles, selectedTitleIdx);
      isCoverUninitialized = loadCoverArt(curTarget->device, curTarget->id);
    }

    // Draw title list
    if (!isCoverUninitialized)
      drawTitleList(titles, selectedTitleIdx, maxTitlesPerPage, coverTexture);
    else
      drawTitleList(titles, selectedTitleIdx, maxTitlesPerPage, NULL);

    gsKit_queue_exec(gsGlobal);
    gsKit_finish();
    gsKit_sync_flip(gsGlobal);

    // Process user inputs:
    if (input == -1)            // If input is -1, block until input changes
      input = waitForInput(-1); // Usado al volver de opciones
    else
      input = pollInput(); // Estado sostenido de botones

    if (gsGlobal->Mode == GS_MODE_PAL)
      frameCount = (frameCount + 1) % 8;
    else
      frameCount = (frameCount + 1) % 10;

    // Permitir acumulación cuando UP/DOWN están sostenidos; filtrar el resto
    if (frameCount && (input == prevInput) && !(input & (PAD_UP | PAD_DOWN)))
      continue;

    // Flancos para botones que no deben repetir
    int pressed = input & ~prevInput;

    if (input & (PAD_CROSS | PAD_CIRCLE)) {
      Target *target = copyTarget(curTarget);
      freeTargetList(titles);
      uiLaunchTitle(target, NULL);
      return -1;
    } else if (input & (PAD_UP | PAD_DOWN)) {
      // --- Navegación con aceleración ---
      int upEdge = (input & PAD_UP) && !(prevInput & PAD_UP);
      int downEdge = (input & PAD_DOWN) && !(prevInput & PAD_DOWN);

      if (upEdge) {
        selectedTitleIdx =
            ((selectedTitleIdx - 1) + titles->total) % titles->total;
        repeatCounter = 0;
      }
      if (downEdge) {
        selectedTitleIdx = (selectedTitleIdx + 1) % titles->total;
        repeatCounter = 0;
      }

      if ((input & PAD_UP) || (input & PAD_DOWN)) {
        if (upEdge || downEdge) {
          repeatCounter = 0;
        } else {
          repeatCounter++;
        }

        if (repeatCounter > repeatDelay && (repeatCounter % repeatSpeed == 0)) {
          if (input & PAD_UP) {
            selectedTitleIdx =
                ((selectedTitleIdx - 1) + titles->total) % titles->total;
          }
          if (input & PAD_DOWN) {
            selectedTitleIdx = (selectedTitleIdx + 1) % titles->total;
          }
        }
      } else {
        repeatCounter = 0;
      }
    } else if (pressed & PAD_R1) {
      // Switch to the next page (solo flanco)
      if (selectedTitleIdx == titles->total - 1) {
        selectedTitleIdx = 0;
      } else {
        selectedTitleIdx += maxTitlesPerPage;
        if (selectedTitleIdx >= titles->total)
          selectedTitleIdx = titles->total - 1;
      }
      repeatCounter = 0;
    } else if (pressed & PAD_L1) {
      // Switch to the previous page (solo flanco)
      if (selectedTitleIdx == 0) {
        selectedTitleIdx = titles->total - 1;
      } else {
        selectedTitleIdx -= maxTitlesPerPage;
        if (selectedTitleIdx < 0)
          selectedTitleIdx = 0;
      }
      repeatCounter = 0;
    } else if (input & PAD_TRIANGLE) {
      input = -1;
      prevInput = 0;
      repeatCounter = 0;
      if ((res = uiTitleOptionsLoop(curTarget)) < 0) {
        return -1;
      }
    } else if (input & PAD_START) {
      repeatCounter = 0;
      break;
    } else if (input & PAD_SELECT) {
      ExitCode skinExit = uiSkinOptionsLoop();
      input = -1;
      prevInput = 0;
      repeatCounter = 0;
      continue;
    } else {
      repeatCounter = 0;
    }

    // Actualizar estado anterior al final de la iteración
    frameCount = 0;
    prevInput = input;
  }

exit:
  closePad();
  closeUI();
  return res;
}

void drawTitleListFooter(int baseX) {
  int baseY = gsGlobal->Height - footerHeight;
  int curX = baseX;

  // CIRCLE + CROSS → Launch title (izquierda)
  drawIconWindow(curX, baseY, 0, gsGlobal->Height, 0, currentTheme.iconCircle,
                 ALIGN_CENTER, ICON_CIRCLE);
  curX += getIconWidth(ICON_CIRCLE);
  drawIconWindow(curX, baseY, 0, gsGlobal->Height, 0, currentTheme.iconCross,
                 ALIGN_CENTER, ICON_CROSS);
  curX += getIconWidth(ICON_CROSS) + 5;
  drawTextWindow(curX, baseY, 0, gsGlobal->Height - 1, 0,
                 currentTheme.headerText, ALIGN_VCENTER, "Launch title");

  // Bloques centrados: SKIN + EXIT + NAVIGATE
  int centerTotal = getIconWidth(ICON_SELECT) + getLineWidth("Skin") + 40 +
                    getIconWidth(ICON_START) + getLineWidth("Exit") + 40 +
                    getIconWidth(ICON_UPDOWN) + getLineWidth("Navigate");
  int centerX = (gsGlobal->Width - centerTotal) / 2;

  // SELECT → Skin
  drawIconWindow(centerX, baseY, 0, gsGlobal->Height, 0, currentTheme.iconStart,
                 ALIGN_CENTER, ICON_SELECT);
  drawTextWindow(centerX + getIconWidth(ICON_SELECT) + 5, baseY, 0,
                 gsGlobal->Height - 1, 0, currentTheme.headerText,
                 ALIGN_VCENTER, "Skin");
  centerX += getIconWidth(ICON_SELECT) + getLineWidth("Skin") + 40;

  // START → Exit
  drawIconWindow(centerX, baseY, 0, gsGlobal->Height, 0, currentTheme.iconStart,
                 ALIGN_CENTER, ICON_START);
  drawTextWindow(centerX + getIconWidth(ICON_START) + 5, baseY, 0,
                 gsGlobal->Height - 1, 0, currentTheme.headerText,
                 ALIGN_VCENTER, "Exit");
  centerX += getIconWidth(ICON_START) + getLineWidth("Exit") + 40;

  // PAD → Navigate
  drawIconWindow(centerX, baseY, 0, gsGlobal->Height, 0, currentTheme.iconPad,
                 ALIGN_CENTER, ICON_UPDOWN);
  drawTextWindow(centerX + getIconWidth(ICON_UPDOWN) + 5, baseY, 0,
                 gsGlobal->Height - 1, 0, currentTheme.headerText,
                 ALIGN_VCENTER, "Navigate");

  // TRIANGLE → Title options (derecha)
  int rightX = gsGlobal->Width - baseX - 5 - getIconWidth(ICON_TRIANGLE) -
               getLineWidth("Title options");
  drawIconWindow(rightX, baseY, 0, gsGlobal->Height, 0,
                 currentTheme.iconTriangle, ALIGN_CENTER, ICON_TRIANGLE);
  drawTextWindow(0, gsGlobal->Height - 1 - footerHeight,
                 gsGlobal->Width - baseX, gsGlobal->Height, 0,
                 currentTheme.headerText, ALIGN_VCENTER | ALIGN_RIGHT,
                 "Title options");
}

// Draws title list
void drawTitleList(TargetList *titles, int selectedTitleIdx,
                   int maxTitlesPerPage, GSTEXTURE *selectedTitleCover) {
  int curPage = selectedTitleIdx / maxTitlesPerPage;

  // Draw header and footer
  int baseX = keepoutArea + 10;
  drawTextWindow(baseX, headerHeight - getFontLineHeight(),
                 gsGlobal->Width - baseX, 0, 0, currentTheme.headerText,
                 ALIGN_HCENTER, "Title List");
  snprintf(lineBuffer, 255, "Page %d/%d\nTitle %d/%d", curPage + 1,
           DIV_ROUND(titles->total, maxTitlesPerPage), selectedTitleIdx + 1,
           titles->total);
  drawTextWindow(baseX, headerHeight - getFontLineHeight(),
                 gsGlobal->Width - baseX, 0, 0, currentTheme.headerText,
                 ALIGN_RIGHT, lineBuffer);

  drawTitleListFooter(baseX);

  // --- Marco fijo alrededor de la lista ---
  int blockHeight = maxTitlesPerPage * getFontLineHeight();

  // offsets manuales para cada lado (ajustables)
  int offsetLeft = -4;    // mover borde izquierdo
  int offsetRight = -190; // mover borde derecho (ejemplo: hacia carátula)
  int offsetTop = +2;     // mover borde superior
  int offsetBottom = +14; // mover borde inferior

  // coordenadas del marco (fijas + offsets)
  int listX1 = keepoutArea + offsetLeft;
  int listX2 = coverArtX1 + offsetRight;
  int listY1 = headerHeight + getFontLineHeight() / 2 + offsetTop;
  int listY2 = listY1 + blockHeight + offsetBottom;

  drawRoundedFrame(listX1, listY1, listX2, listY2, 1);

  // Draw title list
  Target *curTitle = titles->first;

  // márgenes internos respecto al frame
  int marginLeft = 0;  // espacio entre texto y borde izquierdo
  int marginRight = 0; // espacio entre texto y borde derecho
  int marginTop =
      8; // espacio entre el borde superior del frame y el primer título
  int marginBottom =
      8; // espacio entre el borde inferior del frame y el último título

  // el primer título arranca dentro del frame, pegado al borde superior +
  // margen
  int titleY = listY1 + marginTop;

  while (curTitle != NULL) {
    if (curTitle->idx < maxTitlesPerPage * curPage)
      goto next;
    if (curTitle->idx >= maxTitlesPerPage * (curPage + 1))
      break;
    if (titleY >= listY2 - marginBottom)
      break;

    if (selectedTitleIdx == curTitle->idx) {
      drawTextWindow(coverArtX1,
                     drawTextWindow(coverArtX1, coverArtY2 + 5, coverArtX2, 0,
                                    0, currentTheme.listText, ALIGN_HCENTER,
                                    curTitle->id),
                     coverArtX2, 0, 0, currentTheme.listText, ALIGN_HCENTER,
                     modeToString(curTitle->device->mode));
    }

    int textX1 = listX1 + marginLeft;
    int textX2 = listX2 + marginRight;
    int textWidth = getLineWidth(curTitle->name);

    // límites de corte reales
    int cutLeft = textX1 + 11;  // margen real
    int cutRight = textX2 - 20; // margen derecho

    static float scrollOffset = 0.0f;
    static int scrollState = 0; // 0=START_DELAY, 1=LEFT, 2=PAUSE, 3=RIGHT
    static int pauseCounter = 0;
    static int lastTitleIdx = -1;
    static int scrollFinished = 0;
    static int holdCounter = 0;

    const int HOLD_FRAMES = 45;
    const float SCROLL_SPEED = 0.5f;
    const int PAUSE_FRAMES = 60;

    if (selectedTitleIdx != lastTitleIdx) {
      scrollOffset = 0.0f;
      scrollState = 0;
      pauseCounter = 0;
      holdCounter = 0;
      scrollFinished = 0;
      lastTitleIdx = selectedTitleIdx;
    }

    int drawStartX = cutLeft;

    if (textWidth > (cutRight - cutLeft) && selectedTitleIdx == curTitle->idx &&
        !scrollFinished) {

      switch (scrollState) {
      case 0: // START_DELAY
        holdCounter++;
        drawStartX = cutLeft;
        if (holdCounter >= HOLD_FRAMES) {
          scrollState = 1;
          scrollOffset = 0.0f;
        }
        break;

      case 1: // LEFT (ease-in)
      {
        float speed = SCROLL_SPEED + (scrollOffset * 0.01f);
        scrollOffset += speed;

        drawStartX = cutLeft - (int)scrollOffset;

        // stop cuando el texto completo entró en la ventana
        if (drawStartX + textWidth <= cutRight) {
          drawStartX = cutRight - textWidth;
          scrollState = 2;
          pauseCounter = 0;
        }
      } break;

      case 2: // PAUSE
        pauseCounter++;
        if (pauseCounter >= PAUSE_FRAMES) {
          scrollState = 3;
        }
        drawStartX = cutLeft - (int)scrollOffset;
        break;

      case 3: // RIGHT (ease-out)
      {
        float speed = SCROLL_SPEED + ((textWidth - scrollOffset) * 0.01f);
        scrollOffset -= speed;

        drawStartX = cutLeft - (int)scrollOffset;

        if (drawStartX >= cutLeft) {
          drawStartX = cutLeft;
          scrollOffset = 0.0f;
          scrollState = 0;
          holdCounter = 0;
          scrollFinished = 1;
        }
      } break;
      }
    } else {
      if (selectedTitleIdx == curTitle->idx) {
        holdCounter = 0;
      }
      drawStartX = cutLeft;
    }

    // clamp final
    int minStartX = cutRight - textWidth;
    int maxStartX = cutLeft;
    if (drawStartX < minStartX)
      drawStartX = minStartX;
    if (drawStartX > maxStartX)
      drawStartX = maxStartX;

    titleY = drawTextClipped(drawStartX, titleY, cutLeft, cutRight, 0,
                             ((selectedTitleIdx == curTitle->idx)
                                  ? currentTheme.selectedText
                                  : currentTheme.listText),
                             curTitle->name);

  next:
    curTitle = curTitle->next;
  }

  // --- Dibujar íconos de scroll magnetizados al frame ---
  // Se coloca al final de drawTitleList, después del bucle de títulos.

  int scrollMargin = 1; // separación respecto al borde del frame
  int iconWidth = 16;   // ancho fijo del PNG del ícono
  int iconHeight = 16;  // alto fijo del PNG del ícono

  // offsets globales (mueven todo el bloque UP/DOWN/BAR)
  int offsetScrollX = +3;
  int offsetScrollY = 0;

  // offsets específicos del scrollbar (solo afectan al BAR)
  int offsetBarX = +2; // mover un pelín a la derecha
  int offsetBarY = -4; // mover verticalmente si lo necesitás

  // Posición del ícono de scroll UP
  int scrollUpX = listX2 - iconWidth - scrollMargin + 1 + offsetScrollX;
  int scrollUpY = listY1 + scrollMargin + 9 + offsetScrollY;

  // Posición del ícono de scroll DOWN
  int scrollDownX = listX2 - iconWidth - scrollMargin + 1 + offsetScrollX;
  int scrollDownY = listY2 - iconHeight - scrollMargin + 3 + offsetScrollY;

  // Rango vertical disponible para el scrollbar
  int scrollRangeTop = scrollUpY + iconHeight;
  int scrollRangeBottom = scrollDownY - iconHeight;
  int scrollRangeHeight = scrollRangeBottom - scrollRangeTop;

  // Total de títulos
  int totalTitles = titles->total;
  int firstIdx = 0;
  int lastIdx = totalTitles - 1;

  // Wrap-around explícito
  if (selectedTitleIdx > lastIdx) {
    selectedTitleIdx = firstIdx;
  } else if (selectedTitleIdx < firstIdx) {
    selectedTitleIdx = lastIdx;
  }

  // ratio = posición relativa
  float ratio = 0.0f;
  if (lastIdx > firstIdx) {
    ratio = (float)(selectedTitleIdx - firstIdx) / (float)(lastIdx - firstIdx);
  }

  // Posición del scrollbar con offset independiente
  int scrollBarX =
      listX2 - iconWidth - scrollMargin + offsetScrollX + offsetBarX;
  int scrollBarY = scrollRangeTop + (int)(ratio * scrollRangeHeight) +
                   offsetScrollY + offsetBarY;

  // --- Lectura de botones con libpad ---
  struct padButtonStatus buttons;
  padRead(0, 0, &buttons);  // puerto 0, slot 0
  u32 btns = ~buttons.btns; // invertir: 0=presionado, 1=liberado

  bool padUpPressed = (btns & PAD_UP);
  bool padDownPressed = (btns & PAD_DOWN);

  // Colores sólidos
  u64 colorUp =
      padUpPressed ? currentTheme.iconEnabled : currentTheme.listText;
  u64 colorDown =
      padDownPressed ? currentTheme.iconEnabled : currentTheme.listText;
  u64 colorBar = currentTheme.iconEnabled;

  // Dibujar íconos
  drawIcon((float)scrollUpX, (float)scrollUpY, 0, colorUp, ICON_SCROLLUP);
  drawIcon((float)scrollDownX, (float)scrollDownY, 0, colorDown,
           ICON_SCROLLDOWN);
  drawIcon((float)scrollBarX, (float)scrollBarY, 0, colorBar, ICON_SCROLLBAR);

  // Draw cover art placeholder/frame
  gsKit_prim_sprite(gsGlobal, coverArtX1 - 2, coverArtY1 - 2, coverArtX2 + 2,
                    coverArtY2 + 2, 1, currentTheme.coverFrame);

  // Draw cover art if it exists
  if (selectedTitleCover != NULL) {
    // Temporaily disable alpha blending
    // Some PNGs require inverted alpha channel value to display properly
    // Since cover art has nothing to blend, we can bypass the issue altogether
    gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;
    gsKit_prim_sprite_texture(gsGlobal, selectedTitleCover, coverArtX1,
                              coverArtY1, 0.0f, 0.0f, coverArtX2, coverArtY2,
                              selectedTitleCover->Width,
                              selectedTitleCover->Height, 2, FontMainColor);
    gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
  } else {
    gsKit_prim_sprite(gsGlobal, coverArtX1, coverArtY1, coverArtX2, coverArtY2,
                      1, currentTheme.background);
    drawTextWindow(coverArtX1, coverArtY1, coverArtX2, coverArtY2, 1,
                   currentTheme.coverFrame, ALIGN_CENTER, "No cover art");
  }
// 🔹 Draw ico art (con blending activo)
if (icoTexture != NULL) {
    int icoX1 = coverArtX1 - COVER_ART_RES_W; // a la izquierda de coverArt
    int icoY1 = coverArtY1;
    int icoX2 = icoX1 + icoTexture->Width;
    int icoY2 = icoY1 + icoTexture->Height;

    gsKit_prim_sprite_texture(gsGlobal, icoTexture,
                              icoX1, icoY1,
                              0.0f, 0.0f,
                              icoX2, icoY2,
                              icoTexture->Width,
                              icoTexture->Height,
                              2, GS_SETREG_RGBA(0x80,0xFF,0xFF,0x80));
}

}

void drawTitleOptionsFooter(int baseX) {
  int baseY = gsGlobal->Height - footerHeight;

  // CIRCLE + CROSS → Toggle (izquierda)
  drawIconWindow(baseX, baseY, 0, gsGlobal->Height, 0, currentTheme.iconCircle,
                 ALIGN_CENTER, ICON_CIRCLE);
  drawIconWindow(baseX + getIconWidth(ICON_CIRCLE), baseY, 0, gsGlobal->Height,
                 0, currentTheme.iconCross, ALIGN_CENTER, ICON_CROSS);
  drawTextWindow(baseX + 5 + getIconWidth(ICON_CIRCLE) +
                     getIconWidth(ICON_CROSS),
                 gsGlobal->Height - 1 - footerHeight, 0, gsGlobal->Height, 0,
                 currentTheme.headerText, ALIGN_VCENTER, "Toggle");

  // Bloques centrados: TEST + SAVE + NAVIGATE
  int centerTotal = getIconWidth(ICON_SQUARE) + getLineWidth("Test") + 40 +
                    getIconWidth(ICON_START) + getLineWidth("Save") + 40 +
                    getIconWidth(ICON_UPDOWN) + getLineWidth("Navigate");
  int centerX = (gsGlobal->Width - centerTotal) / 2;

  // SQUARE → Test
  drawIconWindow(centerX, baseY, 0, gsGlobal->Height, 0,
                 currentTheme.iconSquare, ALIGN_CENTER, ICON_SQUARE);
  drawTextWindow(centerX + getIconWidth(ICON_SQUARE) + 5, baseY, 0,
                 gsGlobal->Height - 1, 0, currentTheme.headerText,
                 ALIGN_VCENTER, "Test");
  centerX += getIconWidth(ICON_SQUARE) + getLineWidth("Test") + 40;

  // START → Save
  drawIconWindow(centerX, baseY, 0, gsGlobal->Height, 0, currentTheme.iconStart,
                 ALIGN_CENTER, ICON_START);
  drawTextWindow(centerX + getIconWidth(ICON_START) + 5, baseY, 0,
                 gsGlobal->Height - 1, 0, currentTheme.headerText,
                 ALIGN_VCENTER, "Save");
  centerX += getIconWidth(ICON_START) + getLineWidth("Save") + 40;

  // PAD → Navigate
  drawIconWindow(centerX, baseY, 0, gsGlobal->Height, 0, currentTheme.iconPad,
                 ALIGN_CENTER, ICON_UPDOWN);
  drawTextWindow(centerX + getIconWidth(ICON_UPDOWN) + 5, baseY, 0,
                 gsGlobal->Height - 1, 0, currentTheme.headerText,
                 ALIGN_VCENTER, "Navigate");

  // TRIANGLE → Cancel (derecha)
  int rightX = gsGlobal->Width - baseX - 5 - getIconWidth(ICON_TRIANGLE) -
               getLineWidth("Cancel");
  drawIconWindow(rightX, baseY, 0, gsGlobal->Height, 0,
                 currentTheme.iconTriangle, ALIGN_VCENTER | ALIGN_LEFT,
                 ICON_TRIANGLE);
  drawTextWindow(0, gsGlobal->Height - 1 - footerHeight,
                 gsGlobal->Width - baseX, gsGlobal->Height, 0,
                 currentTheme.headerText, ALIGN_VCENTER | ALIGN_RIGHT,
                 "Cancel");

  // L1/R1 → Switch views (arriba)
  drawTextWindow(0,
                 gsGlobal->Height - 1 - footerHeight - getFontLineHeight() / 2,
                 gsGlobal->Width, gsGlobal->Height, 0, currentTheme.headerText,
                 ALIGN_TOP | ALIGN_HCENTER, "Switch views");
  drawIconWindow(0, gsGlobal->Height - footerHeight - getFontLineHeight() / 2,
                 (gsGlobal->Width - getLineWidth("Switch views")) / 2 - 5,
                 gsGlobal->Height, 0, currentTheme.iconStart, ALIGN_TOP | ALIGN_RIGHT,
                 ICON_L1);
  drawIconWindow((gsGlobal->Width + getLineWidth("Switch views")) / 2 + 5,
                 gsGlobal->Height - footerHeight - getFontLineHeight() / 2,
                 gsGlobal->Width, gsGlobal->Height, 0, currentTheme.iconStart,
                 ALIGN_TOP | ALIGN_LEFT, ICON_R1);
}

// Draws well-known Neutrino arguments
// Returns -1 if error occurs
int uiTitleOptionsLoop(Target *target) {
  int res = 0;

  // Load arguments from config files
  ArgumentList *titleArguments = loadLaunchArgumentLists(target);
  int input = 0;
  int activeArgumentIdx = 0;

  // Parse arguments
  for (int i = 0; i < (uiArgumentsTotal); i++)
    uiArguments[i].parse(&uiArguments[i], titleArguments);

  int baseX = keepoutArea + 10;
  int i = 0;
  while (1) {
    gsKit_clear(gsGlobal, currentTheme.background);

    // Draw header
    snprintf(lineBuffer, 255, "%s\n%s", target->name, target->id);
    drawTextWindow(baseX, headerHeight - getFontLineHeight(),
                   gsGlobal->Width - baseX, 0, 0, currentTheme.headerText,
                   ALIGN_HCENTER, lineBuffer);

    int startY = headerHeight + 1.5 * getFontLineHeight();
    for (i = 0; i < uiArgumentsTotal; i++) {
      startY =
          getFontLineHeight() / 2 +
          uiArguments[i].draw(&uiArguments[i], (i == activeArgumentIdx) ? 1 : 0,
                              baseX, startY, 0, gsGlobal->Width - baseX, 0);
    }

    // Draw footer
    drawTitleOptionsFooter(baseX);

    gsKit_queue_exec(gsGlobal);
    gsKit_finish();
    gsKit_sync_flip(gsGlobal);

    // Process user inputs
    input = waitForInput(-1);
    if (input & (PAD_L1 | PAD_R1)) {
      // Show full argument list
      if ((res = uiArgumentListLoop(target, titleArguments)))
        goto exit;

      // Re-parse arguments
      activeArgumentIdx = 0;
      for (i = 0; i < uiArgumentsTotal; i++)
        uiArguments[i].parse(&uiArguments[i], titleArguments);
    } else if (input & PAD_SQUARE) {
      // Launch title without saving arguments
      uiLaunchTitle(target, titleArguments);
      res = -1; // If this was somehow reached, something went terribly wrong
      goto exit;
    } else if (input & PAD_START) {
      updateTitleLaunchArguments(target, titleArguments);
      goto exit;
    } else if (input & PAD_TRIANGLE) {
      // Quit to title list
      goto exit;
    } else {
      switch (uiArguments[activeArgumentIdx].handleInput(
          &uiArguments[activeArgumentIdx], input)) {
      case ACTION_CHANGED:
        uiArguments[activeArgumentIdx].marshal(&uiArguments[activeArgumentIdx],
                                               titleArguments);
        break;
      case ACTION_NEXT_ARGUMENT:
        if (activeArgumentIdx < uiArgumentsTotal - 1)
          activeArgumentIdx++;
        break;
      case ACTION_PREV_ARGUMENT:
        if (activeArgumentIdx > 0)
          activeArgumentIdx--;
        break;
      default:
      }
    }
  }
exit:
  freeArgumentList(titleArguments);
  return res;
}

// Handles all arguments in arugment list
// Returns -1 if error occurs, 1 if parent needs to exit to title list
int uiArgumentListLoop(Target *target, ArgumentList *titleArguments) {
  int selectedArgIdx = 0;
  int input = 0;

  Argument *curArgument = titleArguments->first;
  while (1) {
    gsKit_clear(gsGlobal, currentTheme.background);
    int baseX = keepoutArea + 10;

    // Draw header
    snprintf(lineBuffer, 255, "%s\n%s", target->name, target->id);
    drawTextWindow(baseX, headerHeight - getFontLineHeight(),
                   gsGlobal->Width - baseX, 0, 0, currentTheme.headerText,
                   ALIGN_HCENTER, lineBuffer);
    drawTextWindow(baseX, headerHeight + 1.5 * getFontLineHeight(),
                   gsGlobal->Width - baseX, 0, 0, currentTheme.listText,
                   ALIGN_HCENTER, "Launch arguments");

    // Draw footer
    drawTitleOptionsFooter(baseX);

    int startY = headerHeight + 2.5 * getFontLineHeight();
    int idx = 0;

    // Set number of elements per page according to line height and available
    // screen height
    int maxArguments =
        (gsGlobal->Height - startY - footerHeight - getFontLineHeight() / 2) /
        getFontLineHeight();
    int curPage = selectedArgIdx / maxArguments;

    snprintf(lineBuffer, 255, "Page %d/%d", curPage + 1,
             (!titleArguments->total)
                 ? 1
                 : DIV_ROUND(titleArguments->total, maxArguments));
    startY = drawTextWindow(baseX, startY - getFontLineHeight(),
                            gsGlobal->Width - baseX, 0, 0,
                            currentTheme.headerText, ALIGN_RIGHT, lineBuffer);

    Argument *argument = titleArguments->first;
    while (argument != NULL) {
      // Do not display arguments before the current page
      if (idx < maxArguments * curPage) {
        idx++;
        goto next;
      }
      // Do not display arguments beyond the current page
      if (idx >= maxArguments * (curPage + 1)) {
        break;
      }

      // Draw argument
      if (!argument->isDisabled)
        drawIconWindow(baseX, startY, 20, startY + getFontLineHeight(), 0,
                       currentTheme.iconEnabled, ALIGN_CENTER, ICON_ENABLED);

      snprintf(lineBuffer, 255, "%s%s%s %s",
               ((argument->isGlobal) ? "[G] " : ""), argument->arg,
               (!strlen(argument->value)) ? "" : ":", argument->value);
      startY = drawText(baseX + getIconWidth(ICON_ENABLED), startY, 0, 0, 0,
                        ((selectedArgIdx == idx) ? currentTheme.iconEnabled
                                                 : currentTheme.listText),
                        lineBuffer);

      idx++;
    next:
      argument = argument->next;
    }

    gsKit_queue_exec(gsGlobal);
    gsKit_finish();
    gsKit_sync_flip(gsGlobal);

    // Process user inputs
    input = waitForInput(-1);
    if (input & (PAD_L1 | PAD_R1)) {
      return 0;
    } else if (input & PAD_SQUARE) {
      // Launch title without saving arguments
      uiLaunchTitle(target, titleArguments);
      return -1; // If this was somehow reached, something went terribly wrong
    } else if (input & PAD_START) {
      updateTitleLaunchArguments(target, titleArguments);
      return 1;
    } else if (input & PAD_TRIANGLE) {
      return 1;
    }

    // Ignore inputs when the argument is not initialized
    if (!curArgument)
      continue;

    if (input & (PAD_CROSS | PAD_CIRCLE)) {
      // Toggle argument
      curArgument->isDisabled = !curArgument->isDisabled;
      // If the argument was disabled, reset global flag
      if (curArgument->isDisabled)
        curArgument->isGlobal = 0;
    } else if (input & PAD_UP) {
      // Point to the previous argument
      selectedArgIdx =
          (selectedArgIdx - 1 + titleArguments->total) % titleArguments->total;
      curArgument =
          (curArgument->prev) ? curArgument->prev : titleArguments->last;
    } else if (input & PAD_DOWN) {
      // Advance to the next argument
      selectedArgIdx = (selectedArgIdx + 1) % titleArguments->total;
      curArgument =
          (curArgument->next) ? curArgument->next : titleArguments->first;
    }
  }
}

// Displays Game ID and launches the title
void uiLaunchTitle(Target *target, ArgumentList *arguments) {
  // Initialize arugments if not set
  if (arguments == NULL) {
    arguments = loadLaunchArgumentLists(target);
  }

  gsKit_clear(gsGlobal, currentTheme.background);

  // Draw screen with GameID and title parameters
  snprintf(lineBuffer, 255, "Launching\n%s\n%s\n\n%s", target->name, target->id,
           target->fullPath);
  drawTextWindow(0, 0, gsGlobal->Width, gsGlobal->Height, 0,
                 currentTheme.listText, ALIGN_CENTER, lineBuffer);
  drawGameID(target->id);

  gsKit_queue_exec(gsGlobal);
  gsKit_finish();
  gsKit_sync_flip(gsGlobal);

  // Cleanup the UI and launch title
  closePad();
  closeUI();
  launchTitle(target, arguments);
}

//
// GameID code based on
// https://github.com/CosmicScale/Retro-GEM-PS2-Disc-Launcher
//

static uint8_t calculateCRC(const uint8_t *data, int len) {
  uint8_t crc = 0x00;
  for (int i = 0; i < len; i++) {
    crc += data[i];
  }
  return 0x100 - crc;
}

void drawGameID(const char *gameID) {
  uint8_t data[64] = {0};
  int gidlen =
      strnlen(gameID, 11); // Ensure the length does not exceed 11 characters

  int dpos = 0;
  data[dpos++] = 0xA5; // detect word
  data[dpos++] = 0x00; // address offset
  dpos++;
  data[dpos++] = gidlen;

  memcpy(&data[dpos], gameID, gidlen);
  dpos += gidlen;

  data[dpos++] = 0x00;
  data[dpos++] = 0xD5; // end word
  data[dpos++] = 0x00; // padding

  int data_len = dpos;
  data[2] = calculateCRC(&data[3], data_len - 3);

  int xstart = (gsGlobal->Width / 2) - (data_len * 8);
  int ystart = gsGlobal->Height - (((gsGlobal->Height / 8) * 2) + 20);
  int height = 2;

  for (int i = 0; i < data_len; i++) {
    for (int j = 7; j >= 0; j--) {
      int x = xstart + (i * 16 + ((7 - j) * 2));
      int x1 = x + 1;
      gsKit_prim_sprite(gsGlobal, x, ystart, x1, ystart + height, 0,
                        GS_SETREG_RGBA(0xFF, 0x00, 0xFF, 0x80));

      uint32_t color = (data[i] >> j) & 1
                           ? GS_SETREG_RGBA(0x00, 0xFF, 0xFF, 0x80)
                           : GS_SETREG_RGBA(0xFF, 0xFF, 0x00, 0x80);
      gsKit_prim_sprite(gsGlobal, x1, ystart, x1 + 1, ystart + height, 0,
                        color);
    }
  }
}

//
// Splash screen functions
//

struct {
  int32_t doneSema;      // Used to signal UI splash thread to exit
  int32_t newStringSema; // Used to signal UI splash thread that a new string is
                         // ready
  int32_t drawnSema;     // Used to signal that UI splash thread has finished
                         // drawing or closed
  UILogLevelType level;  // Log level
  char neutrinoVersion[100]; // Neutrino version string
  char buf[255];             // String buffer. String must be null-terminated
} logBuffer = {};
#define THREAD_STACK_SIZE 0x1000
static uint8_t threadStack[THREAD_STACK_SIZE] __attribute__((aligned(16)));

// Initializes and starts UI splash thread
int startSplashScreen() {
  printf("Starting UI splash thread\n");
  // Initialize splash semaphores
  ee_sema_t semaphore;
  semaphore.init_count = 0;
  semaphore.max_count = 1;
  semaphore.option = 0;
  logBuffer.drawnSema = CreateSema(&semaphore);
  logBuffer.newStringSema = CreateSema(&semaphore);
  logBuffer.doneSema = CreateSema(&semaphore);

  // Initialize thread
  ee_thread_t thread;
  thread.func = uiSplashThread;
  thread.stack = threadStack;
  thread.stack_size = THREAD_STACK_SIZE;
  thread.gp_reg = &_gp;
  thread.initial_priority = 0x2;
  thread.attr = thread.option = 0;

  // Start thread
  int32_t threadID;
  if ((threadID = CreateThread(&thread)) >= 0) {
    if (StartThread(threadID, NULL) < 0) {
      DeleteThread(threadID);
      threadID = -1;
    }
  }

  return threadID;
}

// Draws loading splash screen in a separate thread
void uiSplashThread() {
  // Draw logo and version
  gsKit_mode_switch(gsGlobal, GS_PERSISTENT);
  gsKit_TexManager_nextFrame(gsGlobal);
  gsKit_clear(gsGlobal, currentTheme.background);
  drawLogo((gsGlobal->Width - getLogoWidth()) / 2, gsGlobal->Height / 4, 2);
  drawTextWindow(0, (gsGlobal->Height / 4 + getLogoHeight() + 10),
                 gsGlobal->Width, 0, 0, GS_SETREG_RGBA(0x40, 0x40, 0x40, 0x80),
                 ALIGN_HCENTER, SKIN_EDITION);
  gsKit_mode_switch(gsGlobal, GS_ONESHOT);

  drawGameID("NHDDL");

  uint64_t color = currentTheme.headerText;
  int logStartY = gsGlobal->Height - footerHeight - getFontLineHeight() * 3;
  // Loop until something sends a signal
  while (PollSema(logBuffer.doneSema) != logBuffer.doneSema) {
    gsKit_queue_exec(gsGlobal);
    gsKit_finish();
    gsKit_sync_flip(gsGlobal);
    // Wait until a new string is written to buffer
    WaitSema(logBuffer.newStringSema);
    gsKit_TexManager_nextFrame(gsGlobal);
    switch (logBuffer.level) {
    case LEVEL_INFO_NODELAY:
    case LEVEL_INFO:
      color = currentTheme.headerText;
      break;
    case LEVEL_WARN:
      color = WarnTextColor;
      break;
    case LEVEL_ERROR:
      color = ErrorTextColor;
      break;
    }
    drawTextWindow(0, logStartY, gsGlobal->Width,
                   gsGlobal->Height - footerHeight, 0, color, ALIGN_CENTER,
                   logBuffer.buf);
    if (logBuffer.neutrinoVersion[0] != '\0')
      drawTextWindow(
          0,
          (gsGlobal->Height / 4 + getLogoHeight() + getFontLineHeight() + 10),
          gsGlobal->Width, 0, 0, GS_SETREG_RGBA(0x40, 0x40, 0x40, 0x80),
          ALIGN_HCENTER, logBuffer.neutrinoVersion);
    SignalSema(logBuffer.drawnSema);
  }
  gsKit_queue_reset(gsGlobal->Per_Queue);
  DeleteSema(logBuffer.doneSema);
  DeleteSema(logBuffer.newStringSema);
  SignalSema(logBuffer.drawnSema);
  ExitDeleteThread();
}

// Stops UI splash thread
void stopUISplashThread() {
  SignalSema(logBuffer.doneSema);
  SignalSema(logBuffer.newStringSema);
  WaitSema(logBuffer.drawnSema);
  DeleteSema(logBuffer.drawnSema);
}

// Logs to splash screen and debug console in a thread-safe way
void uiSplashLogString(UILogLevelType level, const char *str, ...) {
  va_list args;
  va_start(args, str);

  logBuffer.level = level;
  vsnprintf(logBuffer.buf, 255, str, args);
  va_end(args);
  printf(logBuffer.buf);

  if (!gsGlobal)
    return;

  SignalSema(logBuffer.newStringSema);
  WaitSema(logBuffer.drawnSema);

  switch (level) {
  case LEVEL_INFO_NODELAY:
    return;
  case LEVEL_INFO:
    sleep(1);
    return;
  case LEVEL_WARN:
  case LEVEL_ERROR:
    sleep(2);
    return;
  }
}

// Sets Neutrino version on the splash screen
void uiSplashSetNeutrinoVersion(const char *str) {
  if (!gsGlobal)
    return;

  if (str[0] == '\0')
    return;

  strcpy(logBuffer.neutrinoVersion, "Neutrino");
  strncat(logBuffer.neutrinoVersion, str, 100 - 10);

  SignalSema(logBuffer.newStringSema);
  WaitSema(logBuffer.drawnSema);
}
