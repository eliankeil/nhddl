/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# (c) 2020 Francisco Javier Trujillo Mata <fjtrujy@gmail.com>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# Corregido para handoff limpio tipo uLaunchELF (reset IOP, servicios mínimos)
*/

#include <kernel.h>
#include <sifrpc.h>
#include <iopcontrol.h>
#include <loadfile.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

//--------------------------------------------------------------
// Redefinition of init/deinit libc:
//--------------------------------------------------------------
// DON'T REMOVE is for reducing binary size.
// These functions are defined as weak in /libc/src/init.c
//--------------------------------------------------------------
void _libcglue_init() {}
void _libcglue_deinit() {}
void _libcglue_args_parse(int argc, char **argv) {}

DISABLE_PATCHED_FUNCTIONS();
DISABLE_EXTRA_TIMERS_FUNCTIONS();
PS2_DISABLE_AUTOSTART_PTHREAD();

//--------------------------------------------------------------
// Clear user memory
// PS2Link (C) 2003 Tord Lindstrom (pukko@home.se)
//         (C) 2003 adresd (adresd_ps2dev@yahoo.com)
//--------------------------------------------------------------
static void wipeUserMem(void) {
  int i;
  for (i = 0x100000; i < GetMemorySize(); i += 64) {
    asm volatile(
      "\tsq $0, 0(%0) \n"
      "\tsq $0, 16(%0) \n"
      "\tsq $0, 32(%0) \n"
      "\tsq $0, 48(%0) \n"
      :: "r"(i));
  }
}

int main(int argc, char *argv[]) {
  // Esperamos argv[1] = ruta absoluta del ELF a ejecutar (POPStarter/app)
  if (argc < 2 || argv[1] == NULL || argv[1][0] == '\0') {
    printf("Loader: no ELF path provided in argv[1]\n");
    return -EINVAL;
  }

  const char *elfPath = argv[1];

  // Cambiar CWD al directorio del ELF (ayuda con rutas relativas)
  char pathBuf[512];
  snprintf(pathBuf, sizeof(pathBuf), "%s", elfPath);
  char *slash = strrchr(pathBuf, '/');
  if (slash) {
    *slash = '\0';
    chdir(pathBuf);
  }

  // Reset IOP y sincronización → entorno limpio como uLaunchELF
  SifIopReset(NULL, 0);
  while (!SifIopSync()) { /* esperar */ }

  // Re-init RPC y servicio de loadfile para SifLoadElf
  SifInitRpc(0);
  SifLoadFileInit();

  // Opcional: limpiar memoria de usuario para evitar residuos
  wipeUserMem();

  // Cargar ELF objetivo
  t_ExecData elfdata;
  memset(&elfdata, 0, sizeof(elfdata));

  int ret = SifLoadElf(elfPath, &elfdata);
  if (ret != 0 || elfdata.epc == 0) {
    printf("Loader: SifLoadElf failed (%d) for %s\n", ret, elfPath);
    // Cerrar RPC antes de salir
    SifExitRpc();
    return ret ? ret : -ENOENT;
  }

  // Hand-off limpio justo antes del salto
  SifExitRpc();
  FlushCache(0);
  FlushCache(2);
  DI(); // desactivar interrupciones del EE para evitar callbacks activos

  // Ejecutar ELF (sin argumentos; POPStarter no los necesita)
  // No debería volver.
  return ExecPS2((void *)elfdata.epc, (void *)elfdata.gp, 0, NULL);
}
