#ifndef __ATC_H__
#define __ATC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// ASIC Timing and Control Registers
#define C_SCOPE_ATC 0xE000

#define C_ADDR_ATC_STATUS          0x000 //read only
#define C_ADDR_ATC_TIMESTAMP       0x004 //read only

#define C_ADDR_ATC_POKE_A          0x0A0
#define C_ADDR_ATC_POKE_B          0x0B0
#define C_ADDR_ATC_POKE_C          0x0C0
#define C_ADDR_ATC_POKE_D          0x0D0

#define C_ADDR_ATC_CONFIG_UART     0x100
#define C_ADDR_ATC_CONFIG_BAUD     0x104
#define C_ADDR_ATC_CONFIG_INPUT    0x108
#define C_ADDR_ATC_CONFIG_G        0x110
#define C_ADDR_ATC_CONFIG_H        0x114

#define C_ADDR_ATC_DST_LEMO_A      0x120
#define C_ADDR_ATC_DST_LEMO_B      0x124
#define C_ADDR_ATC_DST_POKE_A      0x128
#define C_ADDR_ATC_DST_POKE_B      0x12C
#define C_ADDR_ATC_DST_POKE_C      0x130
#define C_ADDR_ATC_DST_POKE_D      0x134
#define C_ADDR_ATC_DST_LOGIC_A     0x138
#define C_ADDR_ATC_DST_LOGIC_B     0x13C

#define C_ADDR_ATC_COUNT_REQ       0x200
#define C_ADDR_ATC_COUNT           0x204 //read only

void read_atc_registers();

void set_atc_default_config();

#define C_ATC_BUSY_WAIT 10
int wait_atc_busy(int timeout);

void read_atc_counts();

void toggle_atc_run();

void toggle_atc_destinations();

void send_poke_a(unsigned mask);

void send_poke_b(unsigned mask);

void send_poke_c(unsigned mask);

void send_poke_d(unsigned mask);

#ifdef __cplusplus
}
#endif

#endif // __ATC_H__
