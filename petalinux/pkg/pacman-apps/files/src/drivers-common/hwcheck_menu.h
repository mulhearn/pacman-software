#ifndef HWCHECK_MENU_H
#define HWCHECK_MENU_H

// hwcheck_menu.h
//
// The pacman_hwcheck tests as a submenu of both main menus (Linux and bare metal):  one entry per
// test, printing the test's result lines.  The tests run on the menu's tile mask;  with the I2C
// hardware version set to disabled (I2C menu), the I2C tests SKIP.  With J5 installed, only the
// read-only report runs.

#include <stdbool.h>
#include "hw_access.h"

#ifdef __cplusplus
extern "C" {
#endif

// Tile mask presets (bit t = tile t+1), in the order the toggle steps through them:
#define HWCHECK_TILE_MASK_ALL       0x3FF  // every tile (the default)
#define HWCHECK_TILE_MASK_TILE_1    0x001  // tile 1 only
#define HWCHECK_TILE_MASK_HW1V4     0x0FF  // tiles 1-8 (hw1v4)
#define HWCHECK_TILE_MASK_LOOPBACK  0x003  // the loopback test card (bench 1v5)
#define HWCHECK_TILE_MASK_ATC       0x018  // the ATC test card (bench 1v5)

// menu interface to the hardware tests:
void hwcheck_menu();

// menu options:

// Step the menu's tile mask to the next preset.
void hwcheck_toggle_tile_mask();

// Report the firmware and board status:  J5, versions and build, data path, and (unless no_iic)
// the board and tile monitors.  Read only:  allowed with J5 installed.
void hwcheck_read_only(bool no_iic);

#ifdef __cplusplus
}
#endif

#endif // HWCHECK_MENU_H
