// src/launcher.c
#include "common.h"
#include "devices.h"
#include "options.h"
#include "launcher.h"
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>   // t_ExecData, SifLoadElf, ExecPS2
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iopcontrol.h>
#include <iopheap.h>


// Loader ELF embebido
extern uint8_t loader_elf[];
extern int size_loader_elf;

// Argumentos comunes para Neutrino (ISO)
static char isoArgument[] = "dvd";
static char bsdArgument[] = "bsd";
static char bsdfsArgument[] = "bsdfs";

// Valores bsd (Neutrino)
#define BSD_ATA    "ata"
#define BSD_MX4SIO "mx4sio"
#define BSD_UDPBD  "udpbd"
#define BSD_USB    "usb"
#define BSD_ILINK  "ilink"
#define BSD_MMCE   "mmce"

// Valores bsdfs (Neutrino)
#define BSDFS_HDL  "hdl"

// ---------------------------------------------------------
// ELF directo (POPStarter / apps) — secuencia tipo uLE
// ---------------------------------------------------------
void launchElfTarget(Target *target) {
    // Cambiar CWD al directorio del ELF
    char pathBuf[512];
    snprintf(pathBuf, sizeof(pathBuf), "%s", target->fullPath);
    char *lastSlash = strrchr(pathBuf, '/');
    if (lastSlash) {
        *lastSlash = '\0';
        chdir(pathBuf);
    }

    // Resetear IOP como hace uLaunchELF
    SifIopReset(NULL, 0);
    while (!SifIopSync()) { /* esperar */ }

    // Re‑inicializar RPC
    SifInitRpc(0);

    // Cargar ELF
    t_ExecData elfdata;
    memset(&elfdata, 0, sizeof(elfdata));
    int ret = SifLoadElf(target->fullPath, &elfdata);

    if (ret == 0 && elfdata.epc != 0) {
        SifExitRpc();
        FlushCache(0);
        FlushCache(2);
        ExecPS2((void*)elfdata.epc, (void*)elfdata.gp, 0, NULL);
    } else {
        printf("ERROR: no se pudo cargar %s (ret=%d)\n", target->fullPath, ret);
    }
}



// ---------------------------------------------------------
// Helper: construir argv para loader.elf (Neutrino)
// ---------------------------------------------------------
static int assembleArgv(ArgumentList *arguments, char *argv[]) {
  Argument *curArg = arguments->first;
  int argCount = 1; // argv[0] = neutrino.elf
  int argSize = 0;

  argv[0] = NEUTRINO_ELF_PATH;
  while (curArg != NULL) {
    if (!curArg->isDisabled) {
      argSize = (int)strlen(curArg->arg) + (int)strlen(curArg->value) + 3; // \0, '=' y '-'
      char *value = calloc(sizeof(char), argSize);

      if (!strlen(curArg->value))
        snprintf(value, argSize, "-%s", curArg->arg);
      else
        snprintf(value, argSize, "-%s=%s", curArg->arg, curArg->value);

      argv[argCount] = value;
      argCount++;
    }
    curArg = curArg->next;
  }

  return argCount;
}

// ---------------------------------------------------------
// ISO → Neutrino (sin cambios)
// ---------------------------------------------------------
void launchTitle(Target *target, ArgumentList *arguments) {
  // Caso ELF: camino directo y salir
  if (target->isElf) {
    launchElfTarget(target);
    return;
  }

  // ISO → determinar backend bsd/bsdfs
  char *bsdValue;
  switch (target->device->mode) {
    case MODE_ATA:    bsdValue = BSD_ATA;    break;
    case MODE_MX4SIO: bsdValue = BSD_MX4SIO; break;
    case MODE_UDPBD:  bsdValue = BSD_UDPBD;  break;
    case MODE_USB:    bsdValue = BSD_USB;    break;
    case MODE_ILINK:  bsdValue = BSD_ILINK;  break;
    case MODE_MMCE:   bsdValue = BSD_MMCE;   break;
    case MODE_HDL:
      bsdValue = BSD_ATA;
      appendArgument(arguments, newArgument(bsdfsArgument, BSDFS_HDL));
      break;
    default:
      printf("ERROR: Unsupported mode\n");
      return;
  }

  printf("Updating last launched title\n");
  if (updateLastLaunchedTitle(target->device, target->fullPath)) {
    printf("WARN: Failed to update last launched title\n");
  }

  if (target->device->sync)
    target->device->sync();

  // Montaje de VMC en MMCE si aplica
  mmceMountVMC(target->id);

  appendArgument(arguments, newArgument(bsdArgument, bsdValue));
  appendArgument(arguments, newArgument(isoArgument, target->fullPath));
  if (target->device->mode != MODE_HDL)
    appendArgument(arguments, newArgument("qb", ""));

  char **argv = malloc(((arguments->total) + 1) * sizeof(char *));
  int argCount = assembleArgv(arguments, argv);

  printf("Launching %s (%s) with arguments:\n", target->name, target->id);
  for (int i = 0; i < argCount; i++)
    printf("%d: %s\n", i + 1, argv[i]);

  printf("ERROR: failed to load %s: %d\n", NEUTRINO_ELF_PATH,
         LoadELFFromFile(argCount, argv));
}

// ---------------------------------------------------------
// Ejecutar loader.elf embebido con argv (Neutrino ISO path)
// ---------------------------------------------------------
typedef struct {
  uint8_t  ident[16];
  uint16_t type;
  uint16_t machine;
  uint32_t version;
  uint32_t entry;
  uint32_t phoff;
  uint32_t shoff;
  uint32_t flags;
  uint16_t ehsize;
  uint16_t phentsize;
  uint16_t phnum;
  uint16_t shentsize;
  uint16_t shnum;
  uint16_t shstrndx;
} elf_header_t;

typedef struct {
  uint32_t type;
  uint32_t offset;
  void    *vaddr;
  uint32_t paddr;
  uint32_t filesz;
  uint32_t memsz;
  uint32_t flags;
  uint32_t align;
} elf_pheader_t;

#define ELF_MAGIC   0x464c457f
#define ELF_PT_LOAD 1

int LoadELFFromFile(int argc, char *argv[]) {
  uint8_t       *boot_elf;
  elf_header_t  *eh;
  elf_pheader_t *eph;
  void          *pdata;

  // Limpiar región donde se cargará el loader (ver linkfile de loader)
  memset((void *)0x00084000, 0, 0x00100000 - 0x00084000);

  boot_elf = (uint8_t *)loader_elf;
  eh = (elf_header_t *)boot_elf;
  if (_lw((uint32_t)&eh->ident) != ELF_MAGIC)
    __builtin_trap();

  eph = (elf_pheader_t *)(boot_elf + eh->phoff);

  // Copiar las secciones cargables a RAM
  for (int i = 0; i < eh->phnum; i++) {
    if (eph[i].type != ELF_PT_LOAD) continue;
    pdata = (void *)(boot_elf + eph[i].offset);
    memcpy(eph[i].vaddr, pdata, eph[i].filesz);
  }

  // Handoff al loader embebido
  FlushCache(0);
  FlushCache(2);

  return ExecPS2((void *)eh->entry, NULL, argc, argv);
}
