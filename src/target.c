#include "target.h"
#include "common.h"
#include "devices.h"
#include <errno.h>
#include <fcntl.h>
#include <ps2sdkapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Completely frees TargetList. Passed pointer will not be valid after this function executes
void freeTargetList(TargetList *result) {
  Target *target = result->first;
  while (target != NULL) {
    target = freeTarget(result, target);
  }
  result->first = NULL;
  result->last = NULL;
  result->total = 0;
  free(result);
}

// Finds target with given index in the list and returns a pointer to it
Target *getTargetByIdx(TargetList *targets, int idx) {
  Target *current = targets->first;
  while (current != NULL) {
    if (current->idx == idx) {
      return current;
    }
    current = current->next;
  }
  return NULL;
}

// Makes and returns a deep copy of src without prev/next pointers.
Target *copyTarget(Target *src) {
  Target *copy = calloc(sizeof(Target), 1);
  copy->idx = src->idx;
  copy->isFavorite = src->isFavorite; // copiar estado de favorito

  copy->fullPath = strdup(src->fullPath);
  copy->name = strdup(src->name);
  copy->id = strdup(src->id);
  copy->device = src->device;

  return copy;
}

// Converts lowercase ASCII string into uppercase
void toUppercase(char *str) {
  for (int i = 0; i <= strlen(str); i++)
    if (str[i] >= 0x61 && str[i] <= 0x7A) {
      str[i] -= 32;
    }
}

// Inserts title in the list while keeping the alphabetical order
void insertIntoTargetList(TargetList *result, Target *title) {
  // Traverse the list in reverse
  Target *curTitle = result->last;

  // Covert title name to uppercase
  char *curUppercase = strdup(title->name);
  toUppercase(curUppercase);

  // Overall, title name should not exceed PATH_MAX
  char lastUppercase[PATH_MAX];

  while (curTitle != NULL) {
    // Reset string buffer
    lastUppercase[0] = '\0';
    // Convert name of the last title to uppercase
    strlcpy(lastUppercase, curTitle->name, PATH_MAX);
    toUppercase(lastUppercase);

    // Compare new title name and the current title name
    if (strcmp(curUppercase, lastUppercase) >= 0) {
      // Insert after current
      if (curTitle->next != NULL) {
        curTitle->next->prev = title;
        title->next = curTitle->next;
      } else {
        result->last = title;
      }
      title->prev = curTitle;
      curTitle->next = title;
      break;
    }

    if (curTitle->prev == NULL) {
      // Insert at beginning
      curTitle->prev = title;
      title->next = curTitle;
      result->first = title;
      break;
    }

    curTitle = curTitle->prev;
  }
  free(curUppercase);
}

// Completely frees Target and returns pointer to the next target in the list
Target *freeTarget(TargetList *targetList, Target *target) {
  if (targetList->first == target) {
    targetList->first = target->next;
  }
  if (targetList->last == target) {
    targetList->last = target->prev;
  }

  Target *next = NULL;
  if (target->next != NULL) {
    next = target->next;
    if (target->prev != NULL) {
      next->prev = target->prev;
      target->prev->next = next;
    } else {
      next->prev = NULL;
    }
  } else if (target->prev != NULL) {
    target->prev->next = NULL;
  }

  free(target->fullPath);
  free(target->name);
  if (target->id != NULL)
    free(target->id);

  free(target);
  return next;
}

// Mueve un Target al inicio de la lista (favoritos)
void moveTargetToTop(TargetList *list, Target *target) {
    if (!list || !target) return;
    if (list->first == target) return;

    // Desenganchar
    if (target->prev) target->prev->next = target->next;
    if (target->next) target->next->prev = target->prev;
    if (list->last == target) list->last = target->prev;

    // Insertar al inicio
    target->prev = NULL;
    target->next = list->first;
    if (list->first) list->first->prev = target;
    list->first = target;

    if (!list->last) list->last = target;
}

// Reubica un Target en su posición normal (orden alfabético)
void moveTargetToNormalPosition(TargetList *list, Target *target) {
    if (!list || !target) return;

    // Desenganchar
    if (target->prev) target->prev->next = target->next;
    if (target->next) target->next->prev = target->prev;
    if (list->first == target) list->first = target->next;
    if (list->last == target) list->last = target->prev;

    target->prev = NULL;
    target->next = NULL;

    // Reinsertar en orden alfabético
    insertIntoTargetList(list, target);
}
