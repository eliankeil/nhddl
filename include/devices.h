// devices.h
#ifndef _DEVICES_H_
#define _DEVICES_H_

#include "common.h"
#include "target.h"

#define MAX_DEVICES 20

// Must scan the device entry for titles and add them to TargetList
typedef int (*titleScanFunc)(TargetList *result, struct DeviceMapEntry *device);
// Must sync the device
typedef void (*syncFunc)();

// Device map entry
struct DeviceMapEntry {
  char *mountpoint;               // Device mountpoint
  syncFunc sync;                  // Must sync the device
  titleScanFunc scan;             // Function used for scanning for ISOs (legacy)
  struct DeviceMapEntry *metadev; // Metadata device override
  ModeType mode;                  // Device driver
  uint8_t index;                  // BDM internal device driver number
};

// Contains all available devices.
extern struct DeviceMapEntry deviceModeMap[];

// Initializes device mode map and returns device count
int initDeviceMap();

// Delays for
void delay(int count);

// Uses MMCE devctl calls to switch memory card to given title ID
void mmceMountVMC(char *titleID);

//
// Device-specific scanning functions
//

// Scans given storage device for ISO files and appends valid launch candidates to TargetList
// Implemented in devices_iso.c
int findISO(TargetList *list, struct DeviceMapEntry *device);

// Scans /APPS/*/XX.*.ELF on the given device and appends ELFs to TargetList
// Implemented in devices_elf.c
int findELF(TargetList *list, struct DeviceMapEntry *device);

// Scans given APA HDD for HDL partitions and appends valid launch candidates to TargetList
// Implemented in devices_hdl.c
int findHDLTargets(TargetList *result, struct DeviceMapEntry *device);

#endif
