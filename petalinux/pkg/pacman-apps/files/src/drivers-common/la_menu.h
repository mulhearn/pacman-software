#ifndef LA_MENU_H
#define LA_MENU_H

#include "hw_access.h"
#include "la.h"

#ifdef __cplusplus
extern "C" {
#endif

// Menu interface to the logic analyzer
void la_menu();


// Cycle the LA setup
void toggle_logic_analyzer_setup();

// Cycle the tile of the setup's tile lanes (1-10).
void toggle_logic_analyzer_tile();

// Arm the capture with the current setup (returns at once).
void la_arm();

// Trigger now.
void la_force();

// Capture status (armed, triggered, done), trigger position and timestamp;  when done, per lane of
// the setup:  edge count and the first edge relative to the trigger.
void show_la_status();

#ifdef __cplusplus
}
#endif

#endif // LA_MENU_H
