// target.h
#ifndef _TARGET_H_
#define _TARGET_H_

#include "common.h"
#include <stdint.h>

// Defined in devices.h
struct DeviceMapEntry;

// An entry in TargetList
typedef struct Target {
  uint16_t idx;                    // ISO/ELF index (monotonically increasing)
  char *fullPath;                  // Full path to ISO/ELF
  char *name;                      // Display name (file or folder-derived)
  char *id;                        // Title ID (ISOs only; NULL for ELFs)
  struct DeviceMapEntry *device;   // Device entry

  int isFavorite;                  // 0 = normal, 1 = favorito
  int isElf;                       // 0 = ISO, 1 = ELF

  struct Target *prev;             // Previous target in the list
  struct Target *next;             // Next target in the list
} Target;

// A linked list of launch candidates
typedef struct {
  int total;     // Total number of targets
  Target *first; // First target
  Target *last;  // Last target
} TargetList;

// Completely frees TargetList. Passed pointer will not be valid after this function executes
void freeTargetList(TargetList *result);

// Finds target with given index in the list and returns a pointer to it
Target *getTargetByIdx(TargetList *targets, int idx);

// Makes and returns a deep copy of src without prev/next pointers.
Target *copyTarget(Target *src);

// Inserts title in the list while keeping the alphabetical order
void insertIntoTargetList(TargetList *result, Target *title);

// Completely frees Target and returns pointer to the next target in the list
Target *freeTarget(TargetList *targetList, Target *target);

#endif
