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

// Menu interface to the hardware tests:
void hwcheck_menu();

// Get, toggle (to the next preset) or set the menu's tile mask (bit t = tile t+1).
unsigned hwcheck_get_tile_mask();
void hwcheck_toggle_tile_mask();
void hwcheck_set_tile_mask(unsigned mask);

// Report the firmware and board status:  J5, versions and build, data path, and (unless no_iic)
// the board and tile monitors.  Read only:  allowed with J5 installed.
void hwcheck_read_only(bool no_iic);

// Run the RX/TX loopback test, using UART RX/TX with a loopback card present, on the menu's tile
// mask.  Each tile from the tile mask one by one, then all at once.
void hwcheck_loopback();

#ifdef __cplusplus
}
#endif

#endif // HWCHECK_MENU_H
