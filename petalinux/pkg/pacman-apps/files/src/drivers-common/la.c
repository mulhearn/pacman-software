#include <stdint.h>

#include "hw_access.h"
#include "la.h"

void la_default_config(la_config_t * c){
  capture_default_config(&c->cap);
  c->phase = 0;
  c->baud_locked = 0;
  for (int x = 0; x < 4; x++) c->tile[x] = x;
  for (int n = 0; n < 8; n++) c->lane[n] = LA_LANE_ZERO;
}

void la_configure(const la_config_t * c){
  capture_configure(SCOPE_LA, &c->cap, c->baud_locked ? LA_MODE_BAUD_LOCK : 0);
  axil_write_register(SCOPE_LA + C_ADDR_LA_PHASE, c->phase);
  for (int x = 0; x < 4; x++) axil_write_register(SCOPE_LA + C_ADDR_LA_TILE_A + 4*x, c->tile[x] & 0xF);
  for (int n = 0; n < 8; n++) axil_write_register(SCOPE_LA + C_ADDR_LA_MUX_SELECT_A + 4*n, c->lane[n] & 0x1F);
}
