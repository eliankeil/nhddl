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
#define ICO_ART_RES 128

// ************************************************
// 1. FUNCIÓN DE CORRECCIÓN DE ALPHA AÑADIDA
// ************************************************

// Corrige la inversión del canal Alpha en la memoria RAM de la textura
// (PSMCT32). Asume el formato AARRGGBB, donde el Alpha es el byte más alto.
void correctAlpha(GSTEXTURE *tex) {
    // La corrección solo es necesaria si la textura es de 32 bits y tiene datos.
    if (tex->PSM != GS_PSM_CT32 || tex->Mem == NULL) {
        return;
    }

    u32 num_pixels = tex->Width * tex->Height;
    u32 *pixels = (u32 *)tex->Mem;

    for (u32 i = 0; i < num_pixels; i++) {
        u32 pixel_value = pixels[i];

        // Extraer el Alpha (bits 24-31, asumiendo AARRGGBB o similar)
        u8 alpha = (u8)((pixel_value >> 24) & 0xFF);

        // Invertir el canal Alpha: Alpha_Nuevo = 0x80 - Alpha_Original (o 255 - alpha, usando 0x80 por el contexto)
        u8 new_alpha = 0x80 - alpha;

        // Recomponer el píxel: Alpha Nuevo << 24 | (RGB original)
        pixels[i] = (pixel_value & 0x00FFFFFF) | (new_alpha << 24);
    }
}

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
static GSTEXTURE *icoTexture; // <--- Declaración Global
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

// ************************************************
// VARIABLES ICO ART (Inicializadas a 0 o se configuran en loadCoverArt)
// ************************************************
static int icoArtFinalX;   
static int icoArtY;        
static float icoArtScrollX; 
static int icoArtAnimationState = 0; 
static int icoArtFrameCounter = 0;
// ************************************************

const int keepoutArea = 20;
const int headerHeight = 20 + keepoutArea;
const int footerHeight = 40 + keepoutArea;

void initVMode(GSGLOBAL *gsGlobal) {
    // ... (sin cambios)
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

// ************************************************
// FUNCIÓN UIINIT CORREGIDA (AÑADE ICOTEXTURE INIT)
// ************************************************
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
    gsGlobal->PSM = GS_PSM_CT24; // Set color depth to avoid PAL VRAM issues
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

    // Init cover texture
    coverTexture = calloc(sizeof(GSTEXTURE), 1);
    coverArtX2 = (gsGlobal->Width - keepoutArea - 10);
    coverArtY2 = (gsGlobal->Height / 2) + (COVER_ART_RES_H / 2);
    coverArtX1 = coverArtX2 - COVER_ART_RES_W;
    coverArtY1 = coverArtY2 - COVER_ART_RES_H;

    // PSM debe ser CT32 para que el canal Alpha del PNG funcione.
    coverTexture->PSM = GS_PSM_CT32;
    coverTexture->Delayed = 1;

    // --- AÑADIR: Inicialización de icoTexture ---
    icoTexture = calloc(sizeof(GSTEXTURE), 1);
    icoTexture->PSM = GS_PSM_CT32;
    icoTexture->Delayed = 1;
    // ---------------------------------------------

    return 0;
}


// ************************************************
// FUNCIÓN LOADCOVERART CORREGIDA (AÑADE CARGA DE ICOART Y SETUP DE ANIMACIÓN)
// ************************************************
// Invalidates currently loaded texture and loads a new one
int loadCoverArt(struct DeviceMapEntry *device, char *titleID) {
    if (device->metadev) { // Fallback to metadata device
        device = device->metadev;
    }

    // --- 1. LÓGICA DE CARGA DE COVER ART (COV.png) ---
    snprintf(lineBuffer, 255, "%s%s/%s_COV.png", device->mountpoint, artPath,
             titleID);
    gsKit_TexManager_invalidate(gsGlobal, coverTexture);

    int coverLoaded = 0;
    if (gsKit_texture_png(gsGlobal, coverTexture, lineBuffer) == 0) {
        if (coverTexture->Mem != NULL) {
            correctAlpha(coverTexture);
        }
        gsKit_TexManager_bind(gsGlobal, coverTexture);
        free(coverTexture->Mem);
        coverTexture->Mem = NULL;
        coverLoaded = 1;
    }


    // --- 2. LÓGICA DE CARGA DE ICO ART (ICO.png) ---
    // Invalidar la textura antigua de ICO
    gsKit_TexManager_invalidate(gsGlobal, icoTexture);
    icoTexture->Mem = (void *)1; // Temporalmente marcado como "no cargado/no disponible"

    // Construir la ruta a ICO.png
    snprintf(lineBuffer, 255, "%s%s/%s_ICO.png", device->mountpoint, artPath,
             titleID);

    // Cargar la nueva textura
    if (gsKit_texture_png(gsGlobal, icoTexture, lineBuffer) == 0) {
        // Éxito en la carga
        if (icoTexture->Mem != NULL) {
            correctAlpha(icoTexture);
            gsKit_TexManager_bind(gsGlobal, icoTexture);
            free(icoTexture->Mem);
            icoTexture->Mem = NULL; // Mem == NULL ahora significa que está en VRAM
        }
    
        // 3. Inicializar la posición de la animación de ICO ART
        icoArtY = coverArtY1 + (COVER_ART_RES_H / 2) - (ICO_ART_RES / 2);

        // Posición X final: solapa 64px (la mitad del ícono) en la izquierda de coverArtX1
        const int overlap = ICO_ART_RES / 2; // 64
        icoArtFinalX = coverArtX1 - overlap;

        // Posición X inicial (fuera de pantalla, a la izquierda)
        icoArtScrollX = (float)(-ICO_ART_RES - 10);
        
        // Resetear el estado de la animación
        icoArtAnimationState = 0;
        icoArtFrameCounter = 0;
    } else {
        // Falló la carga de ICO.png. icoTexture->Mem ya está marcado como (void*)1 (no cargado).
        // Se asegura que icoTexture->Mem no sea NULL para distinguirlo de "Cargado en VRAM".
    }

    // Retornar 0 si al menos la carátula principal se cargó, -1 si falló todo.
    return coverLoaded ? 0 : -1;
}

// ************************************************
// FUNCIÓN CLOSEUI CORREGIDA (AÑADE LIBERACIÓN DE ICOTEXTURE)
// ************************************************
// Frees textures and deinits gsKit
void closeUI() {
    gsKit_vram_clear(gsGlobal);
    closeFont();
    // Liberación de las dos texturas
    free(coverTexture);
    free(icoTexture);
    gsKit_deinit_global(gsGlobal);
}


// Main UI loop. Displays the target list.
int uiLoop(TargetList *titles) {
    // ... (sin cambios)
// ... (código completo de uiLoop)
// ...

// ************************************************
// FUNCIÓN DRAWTITLELIST CORREGIDA (CORRECCIÓN DE GS_PRIM_SPRITE_TEXTURE)
// ************************************************
// Draws title list
void drawTitleList(TargetList *titles, int selectedTitleIdx,
                           int maxTitlesPerPage, GSTEXTURE *selectedTitleCover) {

    // ----------------------------------------------------
    // ACCESO A VARIABLES GLOBALES DE ICOART
    // Las variables ya están declaradas como static globales al inicio del archivo.
    // ----------------------------------------------------
    
    // ... (lógica de header, footer, marco, y listado de títulos sin cambios)
    
    // ... (código de scroll, posición de iconos)

    // Dibujar íconos
    drawIcon((float)scrollUpX, (float)scrollUpY, 0, colorUp, ICON_SCROLLUP);
    drawIcon((float)scrollDownX, (float)scrollDownY, 0, colorDown,
                     ICON_SCROLLDOWN);
    drawIcon((float)scrollBarX, (float)scrollBarY, 0, colorBar, ICON_SCROLLBAR);

    // ----------------------------------------------------
    // LÓGICA DE ANIMACIÓN ICOART (NUEVO)
    // ----------------------------------------------------
    if (icoTexture != NULL) {
        const int START_DELAY_FRAMES = 30; // 30 frames de espera
        const float SCROLL_SPEED = 10.0f;  // Pixeles por frame (ajustable)

        // Solo animar si la textura está lista (Mem == NULL)
        if (icoTexture->Mem == NULL) { 
            switch (icoArtAnimationState) {
                case 0: // INICIAL: Espera 30 frames
                    icoArtFrameCounter++;
                    if (icoArtFrameCounter >= START_DELAY_FRAMES) {
                        icoArtAnimationState = 1; // Inicia scroll
                    }
                    break;

                case 1: // SCROLL: Movimiento hacia la derecha
                    // Movimiento (scroll)
                    icoArtScrollX += SCROLL_SPEED; 

                    // Verificar posición de parada:
                    if (icoArtScrollX >= icoArtFinalX) {
                        icoArtScrollX = (float)icoArtFinalX; // Fija la posición
                        icoArtAnimationState = 2; // Stop
                    }
                    break;
                    
                case 2: // DETENIDO (Posición final)
                    // No hacer nada
                    break;
            }
        }
    }
    // ----------------------------------------------------
    
    // Draw cover art placeholder/frame
    gsKit_prim_sprite(gsGlobal, coverArtX1 - 2, coverArtY1 - 2, coverArtX2 + 2,
                               coverArtY2 + 2, 1, currentTheme.coverFrame);

    // Draw cover art if it exists
    if (selectedTitleCover != NULL) {
        // La corrección de Alpha en la carga estandariza la textura.
        gsKit_prim_sprite_texture(gsGlobal, selectedTitleCover, coverArtX1,
                                         coverArtY1, 0.0f, 0.0f, coverArtX2, coverArtY2,
                                         selectedTitleCover->Width,
                                         selectedTitleCover->Height, 2, FontMainColor);
    } else {
        gsKit_prim_sprite(gsGlobal, coverArtX1, coverArtY1, coverArtX2, coverArtY2,
                                     1, currentTheme.background);
        drawTextWindow(coverArtX1, coverArtY1, coverArtX2, coverArtY2, 1,
                                   currentTheme.coverFrame, ALIGN_CENTER, "No cover art");
    }
    
    // ----------------------------------------------------
    // DIBUJADO DE ICO ART (NUEVO)
    // ----------------------------------------------------
    int icoArtDrawX1 = (int)icoArtScrollX;
    int icoArtDrawX2 = icoArtDrawX1 + ICO_ART_RES;
    int icoArtDrawY1 = icoArtY;
    int icoArtDrawY2 = icoArtY + ICO_ART_RES;
    
    if (icoTexture != NULL) {
        // Dibujar un marco para el ícono
        gsKit_prim_sprite(gsGlobal, icoArtDrawX1 - 2, icoArtDrawY1 - 2, icoArtDrawX2 + 2,
                          icoArtDrawY2 + 2, 1, currentTheme.coverFrame);

        // Mem == NULL significa que la textura fue cargada exitosamente a VRAM
        if (icoTexture->Mem == NULL) { 
            // CORRECCIÓN DEL ERROR DE 13 ARGUMENTOS
            gsKit_prim_sprite_texture(gsGlobal, icoTexture, (float)icoArtDrawX1,
                                      (float)icoArtDrawY1, 0.0f, 0.0f, (float)icoArtDrawX2,
                                      (float)icoArtDrawY2, 3, // Z-depth (tercer plano)
                                      icoTexture->Width,
                                      icoTexture->Height, FontMainColor); // Color (12mo argumento)
        } else {
            // Placeholder si falla la carga (icoTexture->Mem != NULL)
            gsKit_prim_sprite(gsGlobal, icoArtDrawX1, icoArtDrawY1, icoArtDrawX2, icoArtDrawY2,
                              1, currentTheme.background);
            drawTextWindow(icoArtDrawX1, icoArtDrawY1, icoArtDrawX2, icoArtDrawY2, 1,
                           currentTheme.coverFrame, ALIGN_CENTER, "No ico art");
        }
    }
    // ----------------------------------------------------
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
                 gsGlobal->Height, 0, currentTheme.iconStart,
                 ALIGN_TOP | ALIGN_RIGHT, ICON_L1);
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