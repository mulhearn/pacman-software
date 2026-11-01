#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "hw_access.h"
#include "dma.h"
#include "rxtx.h"
#include "rxtx_menu.h"
#include "global.h"

// -----------------------------------------------------------
// RX/TX menu layer:  test configurations built on the driver
// -----------------------------------------------------------

void toggle_rx_config(void){
  static const struct {
    unsigned period, flags, mode, input, phase;
    const char * name;
  } modes[] = {
    {0x0A, 0, RX_MODE_EDGE,  RX_INPUT_PIN,      4, "UART input."},
    {0x0A, 0, RX_MODE_EDGE,  RX_INPUT_LOOPBACK, 4, "Full internal loopback."},
    {0x0A, 0, RX_MODE_EDGE,  RX_INPUT_PATTERN,  4, "Test pattern mode."},
    {0x0A, 0, RX_MODE_FIXED, RX_INPUT_LOOPBACK, 1, "Internal loopback, edge detect disabled."},
    {0x0A, 1, RX_MODE_EDGE,  RX_INPUT_LOOPBACK, 8, "Internal loopback, intentional noise."},
    {0x0A, 1, RX_MODE_EDGE,  RX_INPUT_LOOPBACK, 9, "Internal loopback, intentional failures."},
  };
  static int mode = 0;
  mode = (mode + 1) % 6;
  printf("INFO: %s  Broadcasting rx config write 0x%08x \r\n", modes[mode].name,
         (unsigned int) rx_config(modes[mode].period, modes[mode].flags, modes[mode].mode,
                                  modes[mode].input, modes[mode].phase));
  rx_set_config_options_all(modes[mode].period, modes[mode].flags, modes[mode].mode,
                            modes[mode].input, modes[mode].phase);
}

// *** Settings ***

static unsigned G_RING_BDS = RXTX_RING_BDS;
static unsigned G_LOOPBACK_PACKETS = RXTX_LOOPBACK_PACKETS;

unsigned rxtx_menu_ring_bds(void){
  return G_RING_BDS;
}

void toggle_rxtx_ring_bds(void){
  G_RING_BDS = (G_RING_BDS >= RXTX_RING_BDS) ? 16 : 2 * G_RING_BDS;
  printf("INFO:  BD ring size %u (set up with init descriptor ring mode)\r\n", G_RING_BDS);
}

unsigned rxtx_loopback_packets(void){
  return G_LOOPBACK_PACKETS;
}

void rxtx_set_loopback_packets(unsigned packets){
  G_LOOPBACK_PACKETS = packets;
}

void rxtx_toggle_loopback_packets(void){
  static const unsigned presets[] = {RXTX_LOOPBACK_PACKETS, 100000, 1000, 100};
  static int mode = 0;
  mode = (mode + 1) % (int) (sizeof(presets) / sizeof(presets[0]));
  G_LOOPBACK_PACKETS = presets[mode];
  printf("INFO:  loopback length %u packets\r\n", G_LOOPBACK_PACKETS);
}

void rxtx_toggle_uart_mask(void){
  static const struct { hw_u32_t tiles;  uint64_t extra;  const char * name; } presets[] = {
    {0x3FF, 0,   "every UART"},
    {0,     0,   "none"},
    {0,     0x1, "UART 1"},
    {0x001, 0,   "tile 1 (UARTs 1-4)"},
    {0x006, 0,   "tiles 2 and 3 (UARTs 5-12)"},
  };
  static int mode = 0;
  mode = (mode + 1) % (int) (sizeof(presets) / sizeof(presets[0]));
  rxtx_set_uart_mask(rxtx_tile_uarts(presets[mode].tiles) | presets[mode].extra);
  uint64_t mask = rxtx_uart_mask();
  printf("INFO:  RXTX UART mask 0x%02x%08x:  %s\r\n", (unsigned) (mask >> 32), (unsigned) mask,
         presets[mode].name);
}

// *** Benchmarks and BER ***

void rxtx_benchmark_tx(void){
  const unsigned packets    = rxtx_loopback_packets();
  const unsigned uarts      = __builtin_popcountll(rxtx_uart_mask());
  const unsigned batch_size = 100;
  const uint64_t mask = rxtx_uart_mask();

  unsigned tx_sent = 0;

  start_hw_timer();
  while (tx_sent < packets) {
    unsigned batch_count = 0;
    hw_addr_t nxta = 0;
    while((batch_count < batch_size) && (batch_count < (packets - tx_sent)) && (dma_next_available_tx_bd(&nxta))){
      hw_ptr_t tx_buf = dma_get_buffer(nxta);

      tx_buf[0]= (hw_u32_t) mask;
      tx_buf[1]= (hw_u32_t) (mask >> 32);

      for (int i=0; i<TX_PAYLOAD_U32_WORDS; i++)
        tx_buf[i+TX_HEADER_U32_WORDS] = rand();

      HW_FLUSH_DCACHE(tx_buf, TX_PACKET_BYTES);

      dma_add_tx_bd(nxta);
      batch_count++;
    }
    if (batch_count > 0){
      tx_sent += batch_count;
      dma_tx_batch();
    }
  }
  dma_wait_tx_idle(100000);
  stop_hw_timer();

  unsigned elapsed_us = hw_timer_elapsed_us();

  printf("INFO:  elapsed microseconds:    %d (0x%x)\r\n", elapsed_us, (unsigned int) elapsed_us);
  printf("INFO:  tx payloads per packet:  %d\r\n", uarts);
  printf("INFO:  packets:                 %d\r\n", packets);

  if (elapsed_us == 0)
    return;

  unsigned a = (unsigned) (1000ULL * uarts * packets / elapsed_us);
  unsigned m = uarts*10000/66;
  unsigned p = uarts*10000/67;

  printf("INFO:  achieved throughput:     %d tx uart packets per ms\r\n", a);
  printf("INFO:  maximum tx rate:         %d tx uart packets (64-bit+2 @ 10 MHz) per ms\r\n", m);
  printf("INFO:  practical max:           %d tx uart packets (64-bit+3 @ 10 MHz) per ms\r\n", p);
}

void rxtx_benchmark_loopback(void){
  rxtx_check_t r;
  const unsigned uarts = __builtin_popcountll(rxtx_uart_mask());
  rxtx_loopback(rxtx_uart_mask(), rxtx_loopback_packets(), false, 0, &r);

  printf("INFO:  tx packets: %u  words sent: %u  received: %u  stray: %u  bad packets: %u\r\n",
         r.tx_packets, r.words_sent, r.words_rcvd, r.stray_words, r.bad_packets);
  if (r.tx_timeout || r.timeout)
    printf("ERROR: a timeout occurred during the RX/TX benchmark (tx %u, time limit %u)\r\n",
           r.tx_timeout, r.timeout);
  printf("INFO:  elapsed microseconds:    %u\r\n", r.elapsed_us);
  if (r.elapsed_us == 0)
    return;

  unsigned a = (unsigned) (1000ULL * r.words_rcvd / r.elapsed_us);
  unsigned m = uarts*10000/66;
  unsigned p = uarts*10000/67;

  printf("INFO:  achieved throughput:     %u rx uart words per ms\r\n", a);
  printf("INFO:  maximum rate:            %u uart words (64-bit+2 @ 10 MHz) per ms\r\n", m);
  printf("INFO:  practical max:           %u uart words (64-bit+3 @ 10 MHz) per ms\r\n", p);
}

void rxtx_ber_loopback(void){
  rxtx_check_t r;
  rxtx_loopback(rxtx_uart_mask(), rxtx_loopback_packets(), true, 1, &r);

  unsigned long long bits = 64ULL * r.words_rcvd;
  printf("INFO:  tx packets:     %u \r\n", r.tx_packets);
  printf("INFO:  words sent:     %u \r\n", r.words_sent);
  printf("INFO:  words received: %u (missing: %d) \r\n", r.words_rcvd, (int) (r.words_sent - r.words_rcvd));
  printf("INFO:  words in error: %u \r\n", r.words_bad);
  if (r.bit_errors)
    printf("INFO:  bit errors:     %u in %llu bits:  BER %.2e \r\n", r.bit_errors, bits,
           (double) r.bit_errors / (double) bits);
  else if (bits)
    printf("INFO:  bit errors:     0 in %llu bits:  BER < %.1e (95%% CL) \r\n", bits,
           3.0 / (double) bits);
  else
    printf("INFO:  bit errors:     no words received \r\n");
  printf("INFO:  stale words:    %u   other words: %u   stray words: %u   bad packets: %u   tx timeout: %u \r\n",
         r.stale_words, r.other_words, r.stray_words, r.bad_packets, r.tx_timeout);
  printf("INFO:  time limit:     %s   elapsed: %u us \r\n",
         r.timeout == 1 ? "drain" : r.timeout == 2 ? "check" : "none", r.elapsed_us);
}

void rxtx_menu(void){
  while(1){
    printf("RX/TX MENU:  choose an option:\r\n");
    printf("(x) exit RX/TX menu \r\n");
    printf("(a) read TX status (b) read RX status (c) read TX look (d) read RX look \r\n");
    printf("(e) toggle TX UART configs (f) toggle RX UART configs (g) zero counts \r\n");
    printf("(h) toggle RX buffer config (i) toggle RX enables (j) toggle RXTX UART mask \r\n");
    printf("(k) toggle test patterns (l) toggle delay \r\n");
    printf("(F) toggle BD ring size (m) init descriptor ring mode \r\n");
    printf("(n) show BDs (o) show head/tail \r\n");
    printf("(p) clear IOC flags (q) single TX (r) single RX (s) batch TX (t) batch RX \r\n");
    printf("(u) show TX buffer (v) show RX buffer (w) show RX transferred \r\n");
    printf("(A) reset TX DMA (B) TX DMA status (C) reset RX DMA \r\n");
    printf("(D) RX DMA status (E) long DMA status \r\n");
    printf("(G) toggle loopback length (y) benchmark TX (z) benchmark RX/TX loopback \r\n");
    printf("(L) RX/TX loopback bit error rate \r\n");
    int input = input_choice();
    if (input == INPUT_END)
      return;
    printf("INFO: selected %c\r\n", input);

    switch(input){
    case 'x':
      return;
    case 'a':
      read_tx_status();
      break;
    case 'b':
      read_rx_status();
      break;
    case 'c':
      read_tx_look();
      break;
    case 'd':
      read_rx_look();
      break;
    case 'e':
      toggle_tx_config();
      break;
    case 'f':
      toggle_rx_config();
      break;
    case 'g':
      zero_rxtx_counts();
      break;
    case 'h':
      toggle_rx_buffer_config();
      break;
    case 'i':
      toggle_rx_buffer_enables();
      break;
    case 'j':
      rxtx_toggle_uart_mask();
      break;
    case 'k':
      toggle_rx_test_patterns();
      break;
    case 'l':
      toggle_rx_test_pattern_delay();
      break;
    case 'm':
      init_rxtx_descriptor_ring_mode(rxtx_menu_ring_bds());
      break;
    case 'n':
      show_rxtx_bds();
      break;
    case 'o':
      show_rxtx_head_tail();
      break;
    case 'p':
      clear_rxtx_ioc();
      break;
    case 'q':
      single_tx();
      break;
    case 'r':
      single_rx();
      break;
    case 's':
      batch_tx();
      break;
    case 't':
      batch_rx();
      break;
    case 'u':
      show_tx_buffer();
      break;
    case 'v':
      show_rx_buffer();
      break;
    case 'w':
      show_rx_transferred();
      break;
    case 'A':
      dma_reset_tx(DMA_TIMEOUT);
      break;
    case 'B':
      dma_show_tx_status();
      break;
    case 'C':
      dma_reset_rx(DMA_TIMEOUT);
      break;
    case 'D':
      dma_show_rx_status();
      break;
    case 'E':
      dma_show_long_status();
      break;
    case 'F':
      toggle_rxtx_ring_bds();
      break;
    case 'G':
      rxtx_toggle_loopback_packets();
      break;
    case 'y':
      rxtx_benchmark_tx();
      break;
    case 'z':
      rxtx_benchmark_loopback();
      break;
    case 'L':
      rxtx_ber_loopback();
      break;
    default:
      printf("invalid selection...\r\n");
    }
  }
}
