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

// Dimensiones de las Artes
#define COVER_ART_RES_W 120
#define COVER_ART_RES_H 200
#define LOGO_ART_RES_W 150
#define LOGO_ART_RES_H 62
#define DISC_ART_RES_W 128
#define DISC_ART_RES_H 128
#define SPINE_ART_RES_W 10.8
#define SPINE_ART_RES_H 200

// ************************************************
// FUNCIÓN DE CORRECCIÓN DE ALPHA
// ************************************************

// Corrige la inversión del canal Alpha en la memoria RAM de la textura
// (PSMCT32).
void correctAlpha(GSTEXTURE *tex) {
  if (tex->PSM != GS_PSM_CT32 || tex->Mem == NULL) {
    return;
  }

  u32 num_pixels = tex->Width * tex->Height;
  u32 *pixels = (u32 *)tex->Mem;

  for (u32 i = 0; i < num_pixels; i++) {
    u32 pixel_value = pixels[i];
    u8 alpha = (u8)((pixel_value >> 24) & 0xFF);
    u8 new_alpha = 0x80 - alpha;
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
static GSTEXTURE *logoTexture;
static GSTEXTURE *discTexture; // Nuevo: Disc Texture
static GSTEXTURE *spineTexture;
static char lineBuffer[255];
static int lastTitleIdx = -1;
static int frameCounter = 0;

// Estructura para manejar el estado de una animación de un solo eje (X)
typedef struct {
  float startPosition;  // Posición X inicial (X1 base)
  float targetDistance; // Distancia total a recorrer (ej: -40.0f o 64.0f)
  float currentOffset;  // Desplazamiento actual (dx)
  int delayFrames;      // Frames de pausa inicial (30)
  bool isFinished;      // true si la animación ha terminado
} AnimationState;

static AnimationState logoAnimState;
static AnimationState discAnimState;
// Path relative to storage device mountpoint.
static const char artPath[] = "/ART";

// Coordenadas de Cover Art
static int coverArtX2;
static int coverArtY2;
static int coverArtX1;
static int coverArtY1;

// Coordenadas de Logo Art
static int logoArtX1;
static int logoArtY1;
static int logoArtX2;
static int logoArtY2;

// Nuevo: Coordenadas de Disc Art
static int discArtX1;
static int discArtY1;
static int discArtX2;
static int discArtY2;

// NUEVO: Coordenadas de Spine Art
static int spineArtX1;
static int spineArtY1;
static int spineArtX2;
static int spineArtY2;

// NUEVO: Coordenadas de Box3D Art (Overlay)
static int box3dArtX1;
static int box3dArtY1;
static int box3dArtX2;
static int box3dArtY2;

// NUEVO: Coordenadas de Screen Art (Overlay)
static int screenArtX1;
static int screenArtY1;
static int screenArtX2;
static int screenArtY2;

// 4 Puntos para Cover Art: TopLeft(TL), TopRight(TR), BottomRight(BR),
// BottomLeft(BL)
static struct {
  float xTL, yTL;
  float xTR, yTR;
  float xBR, yBR;
  float xBL, yBL;
} coverArtQuad;

// 4 Puntos para Spine Art: TopLeft(TL), TopRight(TR), BottomRight(BR),
// BottomLeft(BL)
static struct {
  float xTL, yTL;
  float xTR, yTR;
  float xBR, yBR;
  float xBL, yBL;
} spineArtQuad;

const int keepoutArea = 20;
const int headerHeight = 20 + keepoutArea;
const int footerHeight = 30 + keepoutArea;

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
  gsGlobal->PSM = GS_PSM_CT24;
  gsGlobal->PSMZ = GS_PSMZ_16S;
  gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
  gsGlobal->DoubleBuffering = GS_SETTING_ON;
  gsGlobal->Test->ATST = 7;
  gsGlobal->Test->AREF = 0x00;
  gsGlobal->Test->AFAIL = 0;

  dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
              D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

  int res;
  if ((res = dmaKit_chan_init(DMA_CHANNEL_GIF))) {
    printf("ERROR: Failed to initlize DMAC: %d\n", res);
    return res;
  }

  gsKit_clear(gsGlobal, currentTheme.background);

  gsKit_vram_clear(gsGlobal);
  gsKit_init_screen(gsGlobal);
  gsKit_display_buffer(gsGlobal);
  gsKit_TexManager_init(gsGlobal);
  gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
  gsKit_set_test(gsGlobal, GS_ATEST_ON);
  gsKit_mode_switch(gsGlobal, GS_ONESHOT);
  gsKit_clear(gsGlobal, currentTheme.background);

  if (initGraphics()) {
    printf("ERROR: Failed to initialize font\n");
    return -1;
  };

  // Init box3d overlay
  int box3dWidth = getBox3dWidth();
  int box3dHeight = getBox3dHeight();
  const int box3dMargin = 30;

  box3dArtX2 = gsGlobal->Width - box3dMargin;
  box3dArtX1 = box3dArtX2 - box3dWidth;
  // Centrado vertical: Y1 = (H / 2) - (box3d_H / 2)
  box3dArtY1 = (gsGlobal->Height / 2) - (box3dHeight / 2);
  box3dArtY2 = box3dArtY1 + box3dHeight;

    // Init screen overlay
  int screenWidth = getScreenWidth();
  int screenHeight = getScreenHeight();
  const int screenMargin = 30;

  screenArtX2 = coverArtX1 + screenMargin;
  screenArtX1 = screenArtX2 - screenWidth;
  // Centrado vertical: Y1 = (H / 2) - (screen_H / 2)
  screenArtY1 = (gsGlobal->Height / 2) - (screenHeight / 2);
  screenArtY2 = screenArtY1 + screenHeight;

  // Init cover texture
  coverTexture = calloc(sizeof(GSTEXTURE), 1);
  coverTexture->PSM = GS_PSM_CT32;
  coverTexture->Delayed = 1;

  coverArtX2 = (gsGlobal->Width - keepoutArea - 12.8);
  coverArtY2 = (gsGlobal->Height / 2) + (COVER_ART_RES_H / 2);
  coverArtX1 = coverArtX2 - COVER_ART_RES_W;
  coverArtY1 = coverArtY2 - COVER_ART_RES_H;

  // --- COVER ART (Frontal) ---
  // Se mueve la esquina superior derecha (TR) hacia arriba y la inferior
  // derecha (BR) hacia abajo. Esto crea un efecto de "libro abierto" o sesgo
  // hacia el frente/lista.

  coverArtQuad.xTL = (float)coverArtX1;
  coverArtQuad.yTL = (float)coverArtY1; // TL sin sesgo

  coverArtQuad.xTR = (float)coverArtX2;
  coverArtQuad.yTR = (float)coverArtY1 + 6.3f; // TR (Sesgo arriba)

  coverArtQuad.xBR = (float)coverArtX2;
  coverArtQuad.yBR = (float)coverArtY2 - 17.0; // BR (Sesgo abajo)

  coverArtQuad.xBL = (float)coverArtX1;
  coverArtQuad.yBL = (float)coverArtY2; // BL sin sesgo

  // LOGO ART: Inicialización y Coordenadas (Centrado en Cover Area)
  logoTexture = calloc(sizeof(GSTEXTURE), 1);
  logoTexture->PSM = GS_PSM_CT32;
  logoTexture->Delayed = 1;

  logoArtX2 = (gsGlobal->Width - 10);
  logoArtY2 = coverArtY1 + 10;
  logoArtX1 = logoArtX2 - LOGO_ART_RES_W;
  logoArtY1 = logoArtY2 - LOGO_ART_RES_H;

  // DISC ART: Inicialización y Coordenadas (Centrado en Cover Area)
  discTexture = calloc(sizeof(GSTEXTURE), 1);
  discTexture->PSM = GS_PSM_CT32;
  discTexture->Delayed = 1;

  discArtX2 = (gsGlobal->Width - keepoutArea - 20 - COVER_ART_RES_W);
  discArtY2 = (gsGlobal->Height / 2) + (COVER_ART_RES_H / 2) + 40;
  discArtX1 = discArtX2 - DISC_ART_RES_W;
  discArtY1 = discArtY2 - DISC_ART_RES_H;

  // Init spineArt
  spineTexture = calloc(sizeof(GSTEXTURE), 1);
  spineTexture->PSM = GS_PSM_CT32;
  spineTexture->Delayed = 1;

  // Se alinea verticalmente con coverArt (mismo Y1, Y2)
  spineArtY1 = coverArtY1;
  spineArtY2 = coverArtY2;

  // Spine Art se toca el lado derecho (X2) contra el lado izquierdo (X1) de
  // coverArt.
  spineArtX2 = coverArtX1;
  spineArtX1 = spineArtX2 - SPINE_ART_RES_W;

  // --- SPINE ART (Lomo) ---
  // El Spine debe tener sesgo opuesto al Cover,
  // para que el lomo parezca conectarse correctamente a la caja.

  spineArtQuad.xTL = (float)spineArtX1;
  spineArtQuad.yTL = (float)spineArtY1 + 0.4f; // TL (Sesgo arriba, menos intenso)

  spineArtQuad.xTR = (float)spineArtX2;
  spineArtQuad.yTR = (float)coverArtY1; // TR (Se conecta al TL del Cover, sin sesgo)

  spineArtQuad.xBR = (float)spineArtX2;
  spineArtQuad.yBR = (float)coverArtY2; // BR (Se conecta al BL del Cover, sin sesgo)

  spineArtQuad.xBL = (float)spineArtX1;
  spineArtQuad.yBL = (float)spineArtY2 - 3.0f; // BL (Sesgo abajo, menos intenso)

  return 0;
}

// loadArt: Unificada para LOGO, DISC y COVER
int loadArt(struct DeviceMapEntry *device, char *titleID) {
  if (device->metadev) {
    device = device->metadev;
  }

  // --- 1. LÓGICA DE CARGA DE LOGO ART (*_LGO.png) ---
  gsKit_TexManager_invalidate(gsGlobal, logoTexture);
  if (logoTexture->Mem != NULL) {
    free(logoTexture->Mem);
    logoTexture->Mem = NULL;
  }
  logoTexture->Vram = 0;
  logoTexture->VramClut = 0;

  snprintf(lineBuffer, 255, "%s%s/%s_LGO.png", device->mountpoint, artPath,
           titleID);

  int logoLoaded = 0;
  if (gsKit_texture_png(gsGlobal, logoTexture, lineBuffer) == 0) {
    if (logoTexture->Mem != NULL) {
      correctAlpha(logoTexture);
      gsKit_TexManager_bind(gsGlobal, logoTexture);
      logoLoaded = 1;
    }
  }

  // --- 2. LÓGICA DE CARGA DE DISC ART (*_ICO.png) ---
  gsKit_TexManager_invalidate(gsGlobal, discTexture);
  if (discTexture->Mem != NULL) {
    free(discTexture->Mem);
    discTexture->Mem = NULL;
  }
  discTexture->Vram = 0;
  discTexture->VramClut = 0;

  snprintf(lineBuffer, 255, "%s%s/%s_ICO.png", device->mountpoint, artPath,
           titleID);

  int discLoaded = 0;
  if (gsKit_texture_png(gsGlobal, discTexture, lineBuffer) == 0) {
    if (discTexture->Mem != NULL) {
      correctAlpha(discTexture);
      gsKit_TexManager_bind(gsGlobal, discTexture);
      discTexture->Filter = GS_FILTER_LINEAR;
      discLoaded = 1;
    }
  }

  // NUEVO: 3. LÓGICA DE CARGA DE SPINE ART (*_LAB.png)
  // ************************************************
  gsKit_TexManager_invalidate(gsGlobal, spineTexture);
  if (spineTexture->Mem != NULL) {
    free(spineTexture->Mem);
    spineTexture->Mem = NULL;
  }
  spineTexture->Vram = 0;
  spineTexture->VramClut = 0;

  snprintf(lineBuffer, 255, "%s%s/%s_LAB.png", device->mountpoint, artPath,
           titleID);

  int spineLoaded = 0;
  if (gsKit_texture_png(gsGlobal, spineTexture, lineBuffer) == 0) {
    if (spineTexture->Mem != NULL) {
      // NOTA: Aunque el spine es un lomo de 11x200,
      // la función gsKit_texture_png lo habrá cargado con el ancho/alto real
      // del PNG. Para Spine/Cover, asumimos que coinciden en altura.
      correctAlpha(spineTexture);
      gsKit_TexManager_bind(gsGlobal, spineTexture);
      spineTexture->Filter = GS_FILTER_LINEAR;
      spineLoaded = 1;
    }
  }

  // --- 3. LÓGICA DE CARGA DE COVER ART (*_COV.png) ---
  gsKit_TexManager_invalidate(gsGlobal, coverTexture);
  if (coverTexture->Mem != NULL) {
    free(coverTexture->Mem);
    coverTexture->Mem = NULL;
  }
  coverTexture->Vram = 0;
  coverTexture->VramClut = 0;

  snprintf(lineBuffer, 255, "%s%s/%s_COV.png", device->mountpoint, artPath,
           titleID);

  int coverLoaded = 0;
  if (gsKit_texture_png(gsGlobal, coverTexture, lineBuffer) == 0) {
    if (coverTexture->Mem != NULL) {
      correctAlpha(coverTexture);
      gsKit_TexManager_bind(gsGlobal, coverTexture);
      coverTexture->Filter = GS_FILTER_LINEAR;
      coverLoaded = 1;
    }
  }

  // ************************************************
  // LIBERACIÓN DE MEMORIA RAM (CLEANUP FINAL)
  // ************************************************

  if (logoLoaded) {
    free(logoTexture->Mem);
    logoTexture->Mem = NULL;
  }
  if (discLoaded) {
    free(discTexture->Mem);
    discTexture->Mem = NULL;
  }
  // NUEVO: Liberar memoria de Spine Art
  if (spineLoaded) {
    free(spineTexture->Mem);
    spineTexture->Mem = NULL;
  }
  if (coverLoaded) {
    free(coverTexture->Mem);
    coverTexture->Mem = NULL;
  }

  return coverLoaded ? 0 : -1;
}

// Frees textures and deinits gsKit
void closeUI() {
  gsKit_vram_clear(gsGlobal);
  closeFont();
  free(coverTexture);
  free(logoTexture);
  free(discTexture); // Liberar DiscTexture
  free(spineTexture);
  gsKit_deinit_global(gsGlobal);
}

// Función de Easing (Deceleración) - Quartic Ease Out
// t: tiempo/progreso actual (de 0 a 1)
float easeOutQuart(float t) {
  // 1 - (1 - t)^4
  t = 1.0f - t;
  return 1.0f - (t * t * t * t);
}

// Lógica para avanzar la animación de un solo objeto.
void animateSprite(AnimationState *state, int totalFrames) {
  if (state->isFinished) {
    return;
  }

  if (frameCounter < state->delayFrames) {
    // Pausa inicial
    return;
  }

  // Calcular el tiempo transcurrido desde el fin de la pausa
  float elapsedFrames = (float)(frameCounter - state->delayFrames);

  // Progreso (t) de 0.0 a 1.0
  float progress = elapsedFrames / (float)totalFrames;

  if (progress >= 1.0f) {
    // Fin del movimiento
    state->currentOffset = state->targetDistance;
    state->isFinished = true;
    return;
  }

  // Aplicar la curva de easing para frenar suavemente
  float easedProgress = easeOutQuart(progress);

  // Calcular el desplazamiento (Offset) actual
  state->currentOffset = state->targetDistance * easedProgress;
}

// Main UI loop. Displays the target list.
int uiLoop(TargetList *titles) {

  if ((LAUNCHER_OPTIONS.vmode != VMODE_NONE) &&
      (gsGlobal->Mode != LAUNCHER_OPTIONS.vmode)) {
    uiInit();
  }

  int res = 0;
  if ((gsGlobal == NULL) && (res = uiInit())) {
    printf("ERROR: Failed to init UI: %d\n", res);
    goto exit;
  }
  initPad();

  int isCoverUninitialized = 1;
  int selectedTitleIdx = 0;
  int maxTitlesPerPage =
      (gsGlobal->Height - (headerHeight + footerHeight)) / getFontLineHeight() -
      1;
  Target *curTarget = titles->first;

  char *lastTitle = calloc(sizeof(char), PATH_MAX + 1);
  if (!getLastLaunchedTitle(lastTitle)) {
    int mountpointLen;
    while (curTarget != NULL) {
      mountpointLen = getRelativePathIdx(curTarget->fullPath);
      if (mountpointLen == -1)
        mountpointLen = 0;

      if (!strcmp(lastTitle, &curTarget->fullPath[mountpointLen])) {
        selectedTitleIdx = curTarget->idx;
        break;
      }
      curTarget = curTarget->next;
    }
    if (curTarget == NULL) {
      curTarget = titles->first;
    }
  }
  free(lastTitle);

  isCoverUninitialized = loadArt(curTarget->device, curTarget->id);

  int frameCount = 0;
  int prevInput = 0;
  int input = 0;
  int repeatCounter = 0;
  const int repeatDelay = 20;
  const int repeatSpeed = 2;

  while (1) {
    gsKit_clear(gsGlobal, currentTheme.background);
    gsKit_TexManager_nextFrame(gsGlobal);

    if (curTarget->idx != selectedTitleIdx) {
      curTarget = getTargetByIdx(titles, selectedTitleIdx);
      isCoverUninitialized = loadArt(curTarget->device, curTarget->id);
      // ************************************************
      // LÓGICA DE ABORTO/REINICIO DE ANIMACIÓN
      // ************************************************
      frameCounter = 0;

      // Logo Art (Mueve 40px a la izquierda)
      logoAnimState.startPosition = (float)logoArtX1;
      logoAnimState.targetDistance = -90.0f;
      logoAnimState.currentOffset = 0.0f;
      logoAnimState.delayFrames = 25;
      logoAnimState.isFinished = false;

      // Disc Art (Mueve 64px a la derecha)
      discAnimState.startPosition = (float)discArtX1;
      discAnimState.targetDistance = 94.0f;
      discAnimState.currentOffset = 0.0f;
      discAnimState.delayFrames = 35;
      discAnimState.isFinished = false;
    }

    if (!isCoverUninitialized)
      drawTitleList(titles, selectedTitleIdx, maxTitlesPerPage, coverTexture);
    else
      drawTitleList(titles, selectedTitleIdx, maxTitlesPerPage, NULL);

    gsKit_queue_exec(gsGlobal);
    gsKit_finish();
    gsKit_sync_flip(gsGlobal);

    if (input == -1)
      input = waitForInput(-1);
    else
      input = pollInput();

    if (gsGlobal->Mode == GS_MODE_PAL)
      frameCount = (frameCount + 1) % 8;
    else
      frameCount = (frameCount + 1) % 10;

    if (frameCount && (input == prevInput) && !(input & (PAD_UP | PAD_DOWN)))
      continue;

    int pressed = input & ~prevInput;

    if (input & (PAD_CROSS | PAD_CIRCLE)) {
      Target *target = copyTarget(curTarget);
      freeTargetList(titles);
      uiLaunchTitle(target, NULL);
      return -1;
    } else if (input & (PAD_UP | PAD_DOWN)) {
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
      if (selectedTitleIdx == titles->total - 1) {
        selectedTitleIdx = 0;
      } else {
        selectedTitleIdx += maxTitlesPerPage;
        if (selectedTitleIdx >= titles->total)
          selectedTitleIdx = titles->total - 1;
      }
      repeatCounter = 0;
    } else if (pressed & PAD_L1) {
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

  drawIconWindow(curX, baseY, 0, gsGlobal->Height, 0, currentTheme.iconCircle,
                 ALIGN_CENTER, ICON_CIRCLE);
  curX += getIconWidth(ICON_CIRCLE);
  drawIconWindow(curX, baseY, 0, gsGlobal->Height, 0, currentTheme.iconCross,
                 ALIGN_CENTER, ICON_CROSS);
  curX += getIconWidth(ICON_CROSS) + 5;
  drawTextWindow(curX, baseY, 0, gsGlobal->Height - 1, 0,
                 currentTheme.headerText, ALIGN_VCENTER, "Launch title");

  int centerTotal = getIconWidth(ICON_SELECT) + getLineWidth("Skin") + 40 +
                    getIconWidth(ICON_START) + getLineWidth("Exit") + 40 +
                    getIconWidth(ICON_UPDOWN) + getLineWidth("Navigate");
  int centerX = (gsGlobal->Width - centerTotal) / 2;

  drawIconWindow(centerX, baseY, 0, gsGlobal->Height, 0, currentTheme.iconStart,
                 ALIGN_CENTER, ICON_SELECT);
  drawTextWindow(centerX + getIconWidth(ICON_SELECT) + 5, baseY, 0,
                 gsGlobal->Height - 1, 0, currentTheme.headerText,
                 ALIGN_VCENTER, "Skin");
  centerX += getIconWidth(ICON_SELECT) + getLineWidth("Skin") + 40;

  drawIconWindow(centerX, baseY, 0, gsGlobal->Height, 0, currentTheme.iconStart,
                 ALIGN_CENTER, ICON_START);
  drawTextWindow(centerX + getIconWidth(ICON_START) + 5, baseY, 0,
                 gsGlobal->Height - 1, 0, currentTheme.headerText,
                 ALIGN_VCENTER, "Exit");
  centerX += getIconWidth(ICON_START) + getLineWidth("Exit") + 40;

  drawIconWindow(centerX, baseY, 0, gsGlobal->Height, 0, currentTheme.iconPad,
                 ALIGN_CENTER, ICON_UPDOWN);
  drawTextWindow(centerX + getIconWidth(ICON_UPDOWN) + 5, baseY, 0,
                 gsGlobal->Height - 1, 0, currentTheme.headerText,
                 ALIGN_VCENTER, "Navigate");

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

  int blockHeight = maxTitlesPerPage * getFontLineHeight();

  int offsetLeft = -4;
  int offsetRight = -190;
  int offsetTop = +2;
  int offsetBottom = +14;

  int listX1 = keepoutArea + offsetLeft;
  int listX2 = coverArtX1 + offsetRight;
  int listY1 = headerHeight + getFontLineHeight() / 2 + offsetTop;
  int listY2 = listY1 + blockHeight + offsetBottom;

  drawRoundedFrame(listX1, listY1, listX2, listY2, 1);

  Target *curTitle = titles->first;

  int marginLeft = 0;
  int marginRight = 0;
  int marginTop = 8;
  int marginBottom = 8;

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

    int cutLeft = textX1 + 11;
    int cutRight = textX2 - 20;

    static float scrollOffset = 0.0f;
    static int scrollState = 0;
    static int pauseCounter = 0;
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
      case 0:
        holdCounter++;
        drawStartX = cutLeft;
        if (holdCounter >= HOLD_FRAMES) {
          scrollState = 1;
          scrollOffset = 0.0f;
        }
        break;

      case 1: {
        float speed = SCROLL_SPEED + (scrollOffset * 0.01f);
        scrollOffset += speed;

        drawStartX = cutLeft - (int)scrollOffset;

        if (drawStartX + textWidth <= cutRight) {
          drawStartX = cutRight - textWidth;
          scrollState = 2;
          pauseCounter = 0;
        }
      } break;

      case 2:
        pauseCounter++;
        if (pauseCounter >= PAUSE_FRAMES) {
          scrollState = 3;
        }
        drawStartX = cutLeft - (int)scrollOffset;
        break;

      case 3: {
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

  // --- Dibujar íconos de scroll ---

  int scrollMargin = 1;
  int iconWidth = 16;
  int iconHeight = 16;

  int offsetScrollX = +3;
  int offsetScrollY = 0;

  int offsetBarX = +2;
  int offsetBarY = -4;

  int scrollUpX = listX2 - iconWidth - scrollMargin + 1 + offsetScrollX;
  int scrollUpY = listY1 + scrollMargin + 9 + offsetScrollY;

  int scrollDownX = listX2 - iconWidth - scrollMargin + 1 + offsetScrollX;
  int scrollDownY = listY2 - iconHeight - scrollMargin + 3 + offsetScrollY;

  int scrollRangeTop = scrollUpY + iconHeight;
  int scrollRangeBottom = scrollDownY - iconHeight;
  int scrollRangeHeight = scrollRangeBottom - scrollRangeTop;

  int totalTitles = titles->total;
  int firstIdx = 0;
  int lastIdx = totalTitles - 1;

  if (selectedTitleIdx > lastIdx) {
    selectedTitleIdx = firstIdx;
  } else if (selectedTitleIdx < firstIdx) {
    selectedTitleIdx = lastIdx;
  }

  float ratio = 0.0f;
  if (lastIdx > firstIdx) {
    ratio = (float)(selectedTitleIdx - firstIdx) / (float)(lastIdx - firstIdx);
  }

  int scrollBarX =
      listX2 - iconWidth - scrollMargin + offsetScrollX + offsetBarX;
  int scrollBarY = scrollRangeTop + (int)(ratio * scrollRangeHeight) +
                   offsetScrollY + offsetBarY;

  struct padButtonStatus buttons;
  padRead(0, 0, &buttons);
  u32 btns = ~buttons.btns;

  bool padUpPressed = (btns & PAD_UP);
  bool padDownPressed = (btns & PAD_DOWN);

  u64 colorUp = padUpPressed ? currentTheme.iconEnabled : currentTheme.listText;
  u64 colorDown =
      padDownPressed ? currentTheme.iconEnabled : currentTheme.listText;
  u64 colorBar = currentTheme.iconEnabled;

  drawIcon((float)scrollUpX, (float)scrollUpY, 0, colorUp, ICON_SCROLLUP);
  drawIcon((float)scrollDownX, (float)scrollDownY, 0, colorDown,
           ICON_SCROLLDOWN);
  drawIcon((float)scrollBarX, (float)scrollBarY, 0, colorBar, ICON_SCROLLBAR);

  // Draw cover art placeholder/frame
  gsKit_prim_sprite(gsGlobal, coverArtX1 - 2, coverArtY1 - 2, coverArtX2 + 2,
                    coverArtY2 + 2, 1, currentTheme.background);

  // Duración del movimiento después de la pausa (ajustable)
  const int LOGO_MOVE_FRAMES = 180;

  // ************************************************
  // 1. DIBUJADO DE LOGO ART (PRIORIDAD 3)
  // ************************************************
  if (logoTexture != NULL && logoTexture->Mem == NULL) {

    animateSprite(&logoAnimState, LOGO_MOVE_FRAMES);

    float finalLogoArtX1 = (float)logoArtX1 + logoAnimState.currentOffset;
    float finalLogoArtX2 = (float)logoArtX2 + logoAnimState.currentOffset;

    gsKit_prim_sprite_texture(gsGlobal, logoTexture, finalLogoArtX1,
                              (float)logoArtY1, 0.0f, 0.0f, finalLogoArtX2,
                              (float)logoArtY2, (float)logoTexture->Width,
                              (float)logoTexture->Height, 1, FontMainColor);
  }

  // Duración del movimiento después de la pausa (ajustable)
  const int DISC_MOVE_FRAMES = 140;

  // ************************************************
  // 2. DIBUJADO DE DISC ART (PRIORIDAD 2)
  // ************************************************
  if (discTexture != NULL && discTexture->Mem == NULL) {

    animateSprite(&discAnimState, DISC_MOVE_FRAMES);

    float finalDiscArtX1 = (float)discArtX1 + discAnimState.currentOffset;
    float finalDiscArtX2 = (float)discArtX2 + discAnimState.currentOffset;

    gsKit_prim_sprite_texture(gsGlobal, discTexture, finalDiscArtX1,
                              (float)discArtY1, 0.0f, 0.0f, finalDiscArtX2,
                              (float)discArtY2, (float)discTexture->Width,
                              (float)discTexture->Height, 1, FontMainColor);
  }

// ************************************************
  // NUEVO: 3. DIBUJADO DE SPINE ART (PRIORIDAD 2, antes de Cover)
  // Solución: Dibujado como 2 triángulos para evitar culling (descarte).
  // ************************************************
  if (spineTexture != NULL && spineTexture->Mem == NULL) {

    // La textura debe coincidir con el tamaño real cargado
    float texWidth = (float)spineTexture->Width;
    float texHeight = (float)spineTexture->Height;

    // --- Triángulo 1: Parte Superior (TL, TR, BL) ---
    gsKit_prim_triangle_texture(
        gsGlobal, spineTexture,
        // Vértice 1: TL (x, y, u, v)
        spineArtQuad.xTL, spineArtQuad.yTL, 0.0f, 0.0f,
        // Vértice 2: TR
        spineArtQuad.xTR, spineArtQuad.yTR, texWidth, 0.0f,
        // Vértice 3: BL
        spineArtQuad.xBL, spineArtQuad.yBL, 0.0f, texHeight,
        2, FontMainColor
    );

    // --- Triángulo 2: Parte Inferior (TR, BR, BL) ---
    gsKit_prim_triangle_texture(
        gsGlobal, spineTexture,
        // Vértice 1: TR
        spineArtQuad.xTR, spineArtQuad.yTR, texWidth, 0.0f,
        // Vértice 2: BR
        spineArtQuad.xBR, spineArtQuad.yBR, texWidth, texHeight,
        // Vértice 3: BL
        spineArtQuad.xBL, spineArtQuad.yBL, 0.0f, texHeight,
        2, FontMainColor
    );
  }

// ************************************************
// 3. DIBUJADO DE COVER ART (PRIORIDAD 1 / CIERRE)
// SOLUCIÓN: Dibujado como 2 triángulos para evitar culling.
// ************************************************
if (selectedTitleCover != NULL && selectedTitleCover->Mem == NULL) {

    // --- Triángulo 1: Parte Superior (TL, TR, BL) ---
    gsKit_prim_triangle_texture(
        gsGlobal, selectedTitleCover,
        // Vértice 1: TL (x, y, u, v)
        coverArtQuad.xTL, coverArtQuad.yTL, 0.0f, 0.0f,
        // Vértice 2: TR
        coverArtQuad.xTR, coverArtQuad.yTR, (float)selectedTitleCover->Width, 0.0f,
        // Vértice 3: BL
        coverArtQuad.xBL, coverArtQuad.yBL, 0.0f, (float)selectedTitleCover->Height,
        2, FontMainColor
    );

    // --- Triángulo 2: Parte Inferior (TR, BR, BL) ---
    gsKit_prim_triangle_texture(
        gsGlobal, selectedTitleCover,
        // Vértice 1: TR
        coverArtQuad.xTR, coverArtQuad.yTR, (float)selectedTitleCover->Width, 0.0f,
        // Vértice 2: BR
        coverArtQuad.xBR, coverArtQuad.yBR, (float)selectedTitleCover->Width, (float)selectedTitleCover->Height,
        // Vértice 3: BL
        coverArtQuad.xBL, coverArtQuad.yBL, 0.0f, (float)selectedTitleCover->Height,
        2, FontMainColor
    );
}
  // ************************************************
  // 5. DIBUJADO DE BOX3D (PRIORIDAD MÁXIMA / CAPA SUPERIOR)
  // ************************************************

  // El color (0xFFFFFFFF con alpha a 0x80) se usa para teñir la textura
  // (Asumiendo que drawBox3d acepta el parámetro 'color' como se sugirió).
  uint64_t box_color = GS_SETREG_RGBA(0x80, 0x80, 0x80, 0x80);

  // Z = 5 para que esté por encima de todas las demás capas (Cover es Z=2,
  // Logo/Disc/Spine son Z=3)
  drawBox3d((float)box3dArtX1, (float)box3dArtY1, 5, box_color);

    // ************************************************
  // 5. DIBUJADO DE SCREEN (PRIORIDAD MÁXIMA / CAPA SUPERIOR)
  // ************************************************

  uint64_t screen_color = GS_SETREG_RGBA(0x80, 0x80, 0x80, 0x80);

  // Z = 5 para que esté por encima de todas las demás capas (Cover es Z=2,
  // Logo/Disc/Spine son Z=3)
  drawScreen((float)screenArtX1, (float)screenArtY1, 1, screen_color);

  // Incrementar el contador de frames para la animación
  frameCounter++;
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