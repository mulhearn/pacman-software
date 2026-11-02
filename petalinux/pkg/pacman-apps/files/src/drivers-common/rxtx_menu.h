#ifndef __RXTX_MENU_H_
#define __RXTX_MENU_H_

#include <stdint.h>
#include "rxtx.h"

#ifdef __cplusplus
extern "C" {
#endif

// The loopback check's default length, in packets:  in use until set.
#define RXTX_LOOPBACK_PACKETS 10000

// Get, toggle (to the next of its presets) or set the loopback check's length.
unsigned rxtx_loopback_packets(void);
void rxtx_toggle_loopback_packets(void);
void rxtx_set_loopback_packets(unsigned packets);

// Toggle the RXTX UART mask to the next of its presets (set:  rxtx_set_uart_mask).
void rxtx_toggle_uart_mask(void);

// Menu interface to the RX/TX driver:
void rxtx_menu(void);

// Menu options:

// The BD ring size the RX/TX menu sets up (init descriptor ring mode):  RXTX_RING_BDS until
// changed.
unsigned rxtx_menu_ring_bds(void);

// Step the menu's BD ring size through the powers of two, 16 to RXTX_RING_BDS;  the ring itself
// is not touched until it is set up again.
void toggle_rxtx_ring_bds(void);

void toggle_rx_config(void);

// Speed benchmark, TX only.
void rxtx_benchmark_tx(void);

// Speed benchmark, RX/TX loopback.
void rxtx_benchmark_loopback(void);

// Measure the bit error rate of the RX/TX loopback.
void rxtx_ber_loopback(void);

#ifdef __cplusplus
}
#endif

#endif // __RXTX_MENU_H_
