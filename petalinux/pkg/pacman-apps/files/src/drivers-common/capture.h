#ifndef CAPTURE_H
#define CAPTURE_H

#include <stdint.h>
#include "hw_access.h"

#ifdef __cplusplus
extern "C" {
#endif

// The ADC and Logic Analyzer use the same capture mechanism, using registers at the same offset in
// both scopes (ADC and LA).

// Samples are captured into a cicular buffer, with one trigger per ARM.  The trigger is the edge of
// a one sample bit, a forced trigger, or the other unit's cross trigger.

// MODE bit 1 can be used to turn off acquisition, independently of the trigger which can still produce cross-triggers.

#define C_ADDR_CAP_PERIOD    0x020   // RW  sample every PERIOD clocks (0:  no samples)
#define C_ADDR_CAP_MODE      0x028   // RW  bit 1:  acquisition enable (reset 1);  LA bit 0:  BAUD locked
#define C_ADDR_CAP_TRIG      0x02C   // RW  bits 4-0 trigger sample bit, 9-8 edge, 10 cross trigger enable
#define C_ADDR_CAP_POST      0x030   // RW  samples after the triggering one
#define C_ADDR_CAP_ARM       0x040   // W   start a capture
#define C_ADDR_CAP_FORCE     0x044   // W   trigger now
#define C_ADDR_CAP_START     0x048   // W   readout:  point at the oldest sample
#define C_ADDR_CAP_NEXT      0x04C   // W   readout:  next sample
#define C_ADDR_CAP_STATUS    0x050   // R   bit 0 armed, 1 triggered, 2 done
#define C_ADDR_CAP_TRIG_POS  0x054   // R   position of the triggering sample (0 = oldest)
#define C_ADDR_CAP_COUNT     0x058   // R   samples in the buffer
#define C_ADDR_CAP_DEPTH     0x05C   // R   buffer depth
#define C_ADDR_CAP_TS_LOWER  0x060   // R   timestamp of the trigger, bits 31-0
#define C_ADDR_CAP_TS_UPPER  0x064   // R   bits 63-32
#define C_ADDR_CAP_DATA      0x070   // R   the sample pointed at

#define CAP_MODE_ACQUIRE     0x2
#define CAP_TRIG_CROSS       0x400

#define CAP_EDGE_RISE        0
#define CAP_EDGE_FALL        1
#define CAP_EDGE_EITHER      2

#define CAP_ARMED            0x1
#define CAP_TRIGGERED        0x2
#define CAP_DONE             0x4

typedef struct {
  hw_u32_t period;         // clocks between samples (0:  none)
  hw_u32_t trig_bit;       // sample bit 0-31 (a bit beyond the sample never triggers)
  hw_u32_t trig_edge;      // CAP_EDGE_*
  hw_u32_t cross;          // 1:  the other unit's trigger triggers this capture
  hw_u32_t post;           // samples after the triggering one
  hw_u32_t acquire;        // 1:  acquisition enabled (0:  trigger only)
} capture_config_t;

// every clock, trigger on a rising edge of bit 31 (choose a constant bit for FORCE or cross trigger only), no
// cross trigger, no post samples, acquisition enabled
void capture_default_config(capture_config_t * c);
// period, trigger, post and MODE (acquisition enable | mode_bits) of the capture in scope
void capture_configure(hw_addr_t scope, const capture_config_t * c, hw_u32_t mode_bits);
void capture_arm(hw_addr_t scope);
void capture_force(hw_addr_t scope);
hw_val_t capture_status(hw_addr_t scope);
hw_u32_t capture_depth(hw_addr_t scope);
uint64_t capture_trig_ts(hw_addr_t scope);
// wait for the capture to finish, up to timeout_us;  0 when done
int capture_wait_done(hw_addr_t scope, hw_u32_t timeout_us);
// read the capture in time order (START, then DATA and NEXT):  up to max samples into buf;  returns the number read
hw_u32_t capture_read(hw_addr_t scope, hw_u32_t * buf, hw_u32_t max, hw_u32_t * trig_pos, uint64_t * trig_ts);

#ifdef __cplusplus
}
#endif

#endif // CAPTURE_H
