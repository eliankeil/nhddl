// include/launcher.h
#ifndef _LAUNCHER_H_
#define _LAUNCHER_H_

#include "target.h"
#include "options.h"

// Ejecuta un ISO a través de Neutrino con argumentos
void launchTitle(Target *target, ArgumentList *arguments);

// Ejecuta un ELF directamente (POPStarter, apps)
void launchElfTarget(Target *target);

// Carga y ejecuta el ELF embebido (loader.elf) con argv preparado
int LoadELFFromFile(int argc, char *argv[]);

#endif
