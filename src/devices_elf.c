// devices_elf.c
// Scans /APPS/*/XX.*.ELF on file-based devices (BDM, MMCE, etc.)
#include "common.h"
#include "devices.h"
#include "gui.h"
#include "options.h"
#include <errno.h>
#include <fcntl.h>
#include <ps2sdkapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

static int ends_with_elf(const char *name) {
  size_t n = strlen(name);
  if (n < 4) return 0;
  const char *ext = name + (n - 4);
  return (!strcasecmp(ext, ".elf"));
}

static int matches_poppattern(const char *name) {
  // XX.*.ELF (mínimo "XX.A.ELF")
  if (!ends_with_elf(name)) return 0;
  if (strlen(name) < 7) return 0;
  return (name[0] == 'X' && name[1] == 'X' && name[2] == '.');
}

static void trim_ext(const char *filename, char *out, size_t outsz) {
  const char *dot = strrchr(filename, '.');
  size_t len = dot ? (size_t)(dot - filename) : strlen(filename);
  if (len >= outsz) len = outsz - 1;
  memcpy(out, filename, len);
  out[len] = '\0';
}

int findELF(TargetList *result, struct DeviceMapEntry *device) {
  if (device->mode == MODE_NONE || device->mountpoint == NULL)
    return -ENODEV;

  // Ruta base /APPS en el dispositivo
  char apps_root[PATH_MAX + 1];
  snprintf(apps_root, sizeof(apps_root), "%s/APPS", device->mountpoint);

  DIR *root = opendir(apps_root);
  if (!root) {
    // No existe /APPS en este dispositivo; no es error fatal
    return -ENOENT;
  }

  struct dirent *de;
  while ((de = readdir(root)) != NULL) {
    if (de->d_name[0] == '.') continue;
    if (de->d_type != DT_DIR) continue;

    // Subcarpeta dentro de /APPS
    char subdir[PATH_MAX + 1];
    snprintf(subdir, sizeof(subdir), "%s/%s", apps_root, de->d_name);

    DIR *sd = opendir(subdir);
    if (!sd) continue;

    struct dirent *f;
    while ((f = readdir(sd)) != NULL) {
      if (f->d_name[0] == '.') continue;
      if (f->d_type == DT_DIR) continue;
      if (!matches_poppattern(f->d_name)) continue;

      // Ruta absoluta al ELF
      char fullpath[PATH_MAX + 1];
      snprintf(fullpath, sizeof(fullpath), "%s/%s", subdir, f->d_name);

      // Crear Target
      Target *title = calloc(1, sizeof(Target));
      title->prev = NULL;
      title->next = NULL;
      title->fullPath = strdup(fullpath);
      title->device = device;
      title->id = NULL;     // ELFs no tienen TitleID
      title->isElf = 1;     // Marca como ELF

      // Label: preferir nombre de carpeta; si falla, filename sin extensión
      title->name = strdup(de->d_name);
      if (!title->name || title->name[0] == '\0') {
        char tmp[256];
        trim_ext(f->d_name, tmp, sizeof(tmp));
        if (title->name) free(title->name);
        title->name = strdup(tmp);
      }

      // Insertar en lista (orden alfabético)
      result->total++;
      if (result->first == NULL) {
        result->first = title;
        result->last = title;
      } else {
        insertIntoTargetList(result, title);
      }
    }
    closedir(sd);
  }
  closedir(root);

  // Indexar
  int idx = 0;
  Target *cur = result->first;
  while (cur) {
    cur->idx = idx++;
    cur = cur->next;
  }

  return (idx > 0) ? 0 : -ENOENT;
}
