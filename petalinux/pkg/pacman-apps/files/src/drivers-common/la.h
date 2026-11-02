#ifndef LA_H
#define LA_H

#include <stdint.h>
#include "hw_access.h"
#include "capture.h"

#ifdef __cplusplus
extern "C" {
#endif

// Logic analyzer
#define SCOPE_LA 0xC000
#define C_ADDR_LA_PHASE         0x024   // RW  BAUD locked:  first sample PHASE clocks after the BAUD strobe
#define C_ADDR_LA_TILE_A        0x080   // RW  TILE_A-TILE_D (+4 each):  bits 3-0, the tile of lane A-D (0-9)
#define C_ADDR_LA_MUX_SELECT_A  0x100   // RW  MUX_SELECT_A-H (+4 each):  bits 4-0, the lane in nibble A-H

#define LA_MODE_BAUD_LOCK       0x1

// lanes (4 bits each, lowest bit first)
#define LA_LANE_ZERO              0
#define LA_LANE_TILE_PISO(x)      (1 + 2*(x))  // tile lane x = 0-3 (A-D):  PISO 0-3 of its tile
#define LA_LANE_TILE_POSI(x)      (2 + 2*(x))  //   POSI 0-3 of its tile
#define LA_LANE_TILE_AB_GH        9            // G A, H A, G B, H B
#define LA_LANE_TILE_CD_GH        10           // G C, H C, G D, H D
#define LA_LANE_ATC_STATUS_LOWER  11           // UCLK, BAUD, LEMO A, LEMO B
#define LA_LANE_ATC_STATUS_UPPER  12           // markers M0-M3
#define LA_LANE_RX_STATUS_LOWER   13
#define LA_LANE_RX_STATUS_UPPER   14
#define LA_LANE_TX_STATUS_LOWER   15
#define LA_LANE_TX_STATUS_UPPER   16

typedef struct {
  capture_config_t cap;
  hw_u32_t phase;          // BAUD locked:  clocks from the BAUD strobe to the first sample
  hw_u32_t baud_locked;    // 1:  BAUD locked
  hw_u8_t  tile[4];        // tile of lane A-D (0-9)
  hw_u8_t  lane[8];        // lane in nibble A-H
} la_config_t;

// capture_default_config, free running, tiles 0-3, every nibble LA_LANE_ZERO
void la_default_config(la_config_t * c);
void la_configure(const la_config_t * c);

#ifdef __cplusplus
}
#endif

#endif // LA_H
