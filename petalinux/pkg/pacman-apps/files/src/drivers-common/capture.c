#include <stdint.h>

#include "hw_access.h"
#include "capture.h"

void capture_default_config(capture_config_t * c){
  c->period = 1;
  c->trig_bit = 31;
  c->trig_edge = CAP_EDGE_RISE;
  c->cross = 0;
  c->post = 0;
  c->acquire = 1;
}

void capture_configure(hw_addr_t scope, const capture_config_t * c, hw_u32_t mode_bits){
  axil_write_register(scope + C_ADDR_CAP_PERIOD, c->period);
  axil_write_register(scope + C_ADDR_CAP_MODE, (c->acquire ? CAP_MODE_ACQUIRE : 0) | mode_bits);
  axil_write_register(scope + C_ADDR_CAP_TRIG, (c->cross ? CAP_TRIG_CROSS : 0) | ((c->trig_edge & 0x3) << 8)
                      | (c->trig_bit & 0x1F));
  axil_write_register(scope + C_ADDR_CAP_POST, c->post);
}

void capture_arm(hw_addr_t scope){
  axil_write_register(scope + C_ADDR_CAP_ARM, 0);
}

void capture_force(hw_addr_t scope){
  axil_write_register(scope + C_ADDR_CAP_FORCE, 0);
}

hw_val_t capture_status(hw_addr_t scope){
  return axil_read_register(scope + C_ADDR_CAP_STATUS);
}

hw_u32_t capture_depth(hw_addr_t scope){
  return axil_read_register(scope + C_ADDR_CAP_DEPTH);
}

uint64_t capture_trig_ts(hw_addr_t scope){
  return ((uint64_t) axil_read_register(scope + C_ADDR_CAP_TS_UPPER) << 32)
         | axil_read_register(scope + C_ADDR_CAP_TS_LOWER);
}

int capture_wait_done(hw_addr_t scope, hw_u32_t timeout_us){
  for (hw_u32_t t = 0; t <= timeout_us; t += 100){
    if (capture_status(scope) & CAP_DONE) return 0;
    usleep(100);
  }
  return -1;
}

hw_u32_t capture_read(hw_addr_t scope, hw_u32_t * buf, hw_u32_t max, hw_u32_t * trig_pos, uint64_t * trig_ts){
  hw_u32_t count = axil_read_register(scope + C_ADDR_CAP_COUNT);
  if (trig_pos) *trig_pos = axil_read_register(scope + C_ADDR_CAP_TRIG_POS);
  if (trig_ts) *trig_ts = capture_trig_ts(scope);
  if (count > max) count = max;
  axil_write_register(scope + C_ADDR_CAP_START, 0);
  for (hw_u32_t i = 0; i < count; i++){
    buf[i] = axil_read_register(scope + C_ADDR_CAP_DATA);
    axil_write_register(scope + C_ADDR_CAP_NEXT, 0);
  }
  return count;
}
