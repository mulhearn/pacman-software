#ifndef ATC_MENU_H
#define ATC_MENU_H

#include "hw_access.h"
#include "atc.h"

#ifdef __cplusplus
extern "C" {
#endif

// menu interface to ASIC driver:
void atc_menu();

// menu options:

// Check each tile's SYNC (G) and TRIG (H) through the ATC test card to its PISO I0 and I1 (tiles
// in tile_mask, one at a time).
void atc_check_outputs(hw_u32_t tile_mask);

// Check the rates on the TRIG (LEMO A) and PPS (LEMO B) inputs:  a 1 kHz TTL on both.
void atc_check_lemo_inputs();

#ifdef __cplusplus
}
#endif

#endif // ASIC_MENU_H
