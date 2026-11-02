#ifndef __RXTX_H_
#define __RXTX_H_

#include<stdbool.h>
#include "hw_access.h"

#ifdef __cplusplus
extern "C" {
#endif

// RX/TX REGISTERS:

#define SCOPE_TX       0x0000
#define SCOPE_RX       0x4000
#define UART_GLOBAL    0x3F00
#define UART_BROADCAST 0x3B00

#define C_ADDR_RX_UART_STATUS       0x00
#define C_ADDR_RX_UART_CONFIG       0x04
#define C_ADDR_RX_UART_CHAN         0x08

// RX UART config word from its fields
hw_u32_t rx_config(unsigned period, unsigned flags, unsigned mode, unsigned input, unsigned phase);

#define RX_MODE_OFF           0x0          // no sampling
#define RX_MODE_EDGE          0x1          // sample phase relative to the detected falling edge
#define RX_MODE_FIXED         0x2          // fixed sample phase (no edge detection)

#define RX_INPUT_OFF          0x0
#define RX_INPUT_PIN          0x1          // PISO from the pin
#define RX_INPUT_LOOPBACK     0x2          // own TX (POSI), inside the firmware
#define RX_INPUT_PATTERN      0x4          // test pattern generator
#define RX_INPUT_EMUL         0x8          // ASIC emulation

// RX buffer config word from its fields
hw_u32_t rx_buffer_config(unsigned max_words, unsigned timeout);

// DAQ setting (pacman-server):  0x260 words (0x3900 bytes) per packet, timeout 0x188 units (~1 ms)
#define RX_BUFFER_CONFIG_DAQ  rx_buffer_config(0x260, 0x188)

// operating default:  UART input, 10 clocks per bit, sampled 4 clocks after the falling edge
#define RX_CONFIG_DEFAULT     rx_config(0x0A, 0, RX_MODE_EDGE, RX_INPUT_PIN, 4)

#define C_ADDR_RX_UART_STARTS       0x20
#define C_ADDR_RX_UART_BEATS        0x24
#define C_ADDR_RX_UART_UPDATES      0x28
#define C_ADDR_RX_UART_LOST         0x2C

#define C_ADDR_RX_UART_FRAME_ERR    0x30
#define C_ADDR_RX_UART_NOISE        0x34


#define C_ADDR_RX_LOOK_SELECT       0xA0
#define C_ADDR_RX_LOOK_LOWER        0xA4
#define C_ADDR_RX_LOOK_UPPER        0xA8

#define C_ADDR_RX_BUFFER_STATUS     0xB0
#define C_ADDR_RX_BUFFER_CONFIG     0xB4
#define C_ADDR_RX_BUFFER_ENABLES    0xB8
#define C_ADDR_RX_PACMAN            0xBC

#define C_ADDR_RX_FIFO_CNT          0xF0
#define C_ADDR_RX_FIFO_MAX          0xF4
#define C_ADDR_RX_ZERO_CNTS         0xF8
#define C_ADDR_RX_HEARTBEAT_CONFIG  0xC0
#define C_ADDR_RX_ROLLOVER_CONFIG   0xC4
#define C_ADDR_RX_WORD_TYPE_LUT     0xC8
#define C_ADDR_RX_HEADER_A          0xD0
#define C_ADDR_RX_HEADER_B          0xD4
#define C_ADDR_RX_HEADER_C          0xD8
#define C_ADDR_RX_HEADER_D          0xDC
#define C_ADDR_RX_EOP_HEADER        0xE0

#define C_ADDR_TX_UART_STATUS   0x00
#define C_ADDR_TX_UART_CONFIG   0x04
#define C_ADDR_TX_UART_STARTS   0x20
#define C_ADDR_TX_UART_BEATS    0x24

#define C_ADDR_TX_LOOK_SELECT   0xA0
#define C_ADDR_TX_LOOK_LOWER    0xA4
#define C_ADDR_TX_LOOK_UPPER    0xA8
#define C_ADDR_TX_BUFFER_STATUS 0xB0
#define C_ADDR_TX_ZERO_CNTS     0xF8

#define C_ADDR_RX_PATTERN_LOWER  0x80
#define C_ADDR_RX_PATTERN_UPPER  0x84
#define C_ADDR_RX_PATTERN_DELAY  0x88
#define C_ADDR_RX_PATTERN_CONFIG 0x8C
#define C_ADDR_RX_PATTERN_STATUS 0x90
#define C_ADDR_RX_PATTERN_STARTS 0x94
#define C_ADDR_RX_PATTERN_STOPS  0x98

// this is reserved in system-user.dtsi and located within the HP AXI interface for DMA (0x00000000 - 0x3FFFFFFF):
#define DMA_BUFFER_BASEADDR  0x20000000
#define DMA_BUFFER_SIZE      0x10000000  // 256 MB

#define TX_BD_BASEADDR       0x21000000
#define RX_BD_BASEADDR       0x22000000

// 40 uarts x 64 bits + 1-64 bit header => 328 bits = 0x148
// Note: DMA driver will alighn buffer *spacing* to 0x150
#define TX_PACKET_BYTES 0x148
#define TX_HEADER_BYTES 8
#define TX_PAYLOAD_U32_WORDS ((TX_PACKET_BYTES - TX_HEADER_BYTES)/4)
#define TX_HEADER_U32_WORDS  (TX_HEADER_BYTES/4)

// Buffer sizes:
// 1 single UART        (1+1)*24    =  48  = 0x30   <-- size MMMM=0x0001
// 40 UART              (40+1)*24   = 984  = 0x3D8  <-- typical test pattern size
// 40 UART + 3 Extra    (40+3+1)*24 = 1056 = 0x420  <-- max for CCCC=0x0001
//#define RX_BUF_BYTES 0x800
// Enough for single cycles, max (40 uarts + header + 3 T/S/HB) * 16 bytes = 0x2c0 bytes
// Each uart rx takes 10 cycles, so for 10 cycles, the maximum buffer size is:
//    (40 + 1 + 10*3)*16 = 0x470 (1136) bytes
// So the buffer size below is enough for more than 140 cycles (0x8C) which you should see in settings
// Note:  when switching to 64 bit timestamps, each cycle will take three times as long, and so this becomes:
//    (3*40 + 1 + 30*3)*24 = 5064 (0x13c8)
// and the buffer size below is enough for 32 (0x20)  cycles (about 1/4 of 0x8C)
// More directly, that is large enough for 682 words (0x2aa)
#define RX_BUF_BYTES 0x4000

#define RX_TRAILER_BYTES 24
#define RX_WORD_BYTES    24


#define TX_BATCH_NEXTDESC_ADDR       0x20000000
#define RX_BATCH_NEXTDESC_ADDR       0x20000004

// pacman-server hooks:
void init_rxtx(void);
void init_tx_descriptor_ring_mode(int ring_size);
void init_rx_descriptor_ring_mode(int ring_size);

// Set the config of every RX UART, or of one:  a whole config word (e.g. RX_CONFIG_DEFAULT), or
// its fields (as for rx_config).
void rx_set_config_all(hw_u32_t config);
void rx_set_config(unsigned chan, hw_u32_t config);
void rx_set_config_options(unsigned chan, unsigned period, unsigned flags, unsigned mode,
                           unsigned input, unsigned phase);
void rx_set_config_options_all(unsigned period, unsigned flags, unsigned mode, unsigned input,
                               unsigned phase);

// Get, set the RX buffer config.
void rx_set_buffer_config(hw_val_t config);
hw_val_t rx_get_buffer_config(void);

// Enable, disable RX UART channels.
void rx_disable_uart(unsigned chan);
void rx_enable_uart(unsigned chan);
bool rx_uart_is_enabled(unsigned chan);

// RX BDs filled by the DMA and not yet handed back:  valid only in the process that set up the
// ring (the ring size is this process's).
unsigned rx_pending(void);

// menu hooks:
// per-UART status, read in one call:
typedef struct {
  hw_val_t status;
  hw_val_t config;
  hw_val_t chan;
  hw_val_t starts;
  hw_val_t beats;
  hw_val_t updates;
  hw_val_t lost;
  hw_val_t frame_err;
  hw_val_t noise;
} rx_uart_status_t;

typedef struct {
  hw_val_t status;
  hw_val_t config;
  hw_val_t starts;
  hw_val_t beats;
} tx_uart_status_t;

void get_rx_uart_status(unsigned chan, rx_uart_status_t * us);
void get_tx_uart_status(unsigned chan, tx_uart_status_t * us);

void read_tx_status(void);
void read_tx_look(void);
void toggle_tx_config(void);
// RXTX UART mask (bit u = UART u+1, 40 bits):  the UARTs the RX/TX tools use (single and batch TX,
// the benchmarks, the loopback check).  All 40 by default.
uint64_t rxtx_uart_mask(void);

// The UART mask of the tiles in tile_mask (bit t = tile t+1):  their 4 UARTs each.
uint64_t rxtx_tile_uarts(hw_u32_t tile_mask);

// Set the RXTX UART mask.
void rxtx_set_uart_mask(uint64_t mask);

void read_rx_status(void);
void read_rx_look(void);
void toggle_rx_buffer_config(void);
void toggle_rx_buffer_enables(void);
void toggle_rx_test_patterns(void);
void toggle_rx_test_pattern_delay(void);

void zero_rxtx_counts(void);

#define RXTX_RING_BDS 512   // BDs in each DMA ring (TX and RX) by default, as the servers use

void init_rxtx_descriptor_ring_mode(int ring_size);
unsigned rxtx_ring_size(void);   // 0: no ring set up yet in this process
void show_rxtx_bds(void);
void show_rxtx_head_tail(void);

void clear_rxtx_ioc(void);
void show_tx_buffer(void);
void show_rx_buffer(void);
void show_rx_transferred(void);

void single_tx(void);
void single_rx(void);

void batch_tx(void);
void batch_rx(void);

// Results of an RX/TX loopback test.
typedef struct {
  unsigned tx_packets;    // DMA packets sent
  unsigned words_sent;    // UART words sent (one per masked UART per packet)
  unsigned words_rcvd;    // UART data words received and checked
  unsigned words_bad;     // received words with at least one bit error
  unsigned bit_errors;    // total bit errors in received words
  unsigned stale_words;   // words already waiting before the check started (discarded)
  unsigned other_words;   // received words not from a UART (sync, heartbeat, trigger)
  unsigned stray_words;   // received words from a UART outside the mask
  unsigned rcvd[40];      // words received per UART (stray words included)
  unsigned bad_packets;   // RX DMA packets with an invalid size
  unsigned tx_timeout;    // nonzero if TX stalled
  unsigned timeout;       // nonzero if a time limit ran out:  1 the drain of stale words, 2 the check itself
  unsigned elapsed_us;    // time spent in the check (after the drain)
} rxtx_check_t;

// Run an RX/TX loopback test for <packets> packets on the UARTs in <uart_mask>, with the existing
// configuration:  touches no config, enables or power (all arranged by the caller), and uses the
// DMA ring already set up.  With ber_test, every word is pseudo-random from the seed and compared
// with the one expected next on its UART;  without, the words are only counted and timed.
void rxtx_loopback(uint64_t uart_mask, unsigned packets,
                   bool ber_test, hw_u32_t seed, rxtx_check_t * r);

#ifdef __cplusplus
}
#endif

#endif // __RXTX_H_
