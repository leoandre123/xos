#pragma once
#include "battery_info.h" // shared struct (battery_info)
#include "types.h"

// Initialise the battery subsystem.  Scans DSDT/SSDTs for battery devices.
void battery_init(void);

uint battery_count(void);

// Fill *out with current battery status.  Returns false if idx is out of
// range or no battery device was found.
bool battery_get(uint idx, battery_info *out);
