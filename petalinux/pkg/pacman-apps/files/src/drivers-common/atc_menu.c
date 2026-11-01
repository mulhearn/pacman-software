#include <stdlib.h>

#include "hw_access.h"
#include "atc.h"
#include "atc_menu.h"
#include <stdio.h>
#include "global.h"
#include "rxtx.h"
#include "rxtx_menu.h"
#include "check.h"

// *** Checks ***

// Rates on the isolated front-panel LEMO inputs, from the ATC input counters (rising edges,
// debounced, 16 bits, saturating):  LEMO A = TRIG_ISO (J18), LEMO B = SYNC_ISO (K18, the PPS
// input).  A 1 kHz TTL on both:  LEMO_EXPECT_HZ, a link-alive test, +/- LEMO_TOL_PCT.

#define LEMO_GATE_US   1000000
#define LEMO_COUNT_MAX 0xFFFF
#define LEMO_EXPECT_HZ 1000
#define LEMO_TOL_PCT   20

static const char * LEMO_NET[2] = {"TRIG_ISO, LEMO A", "SYNC_ISO, LEMO B"};

// both input counters over one gate:  rates in Hz (saturated counts flagged)
static void lemo_rates(double hz[2], bool sat[2]){
  axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_COUNT_REQ, 0x10);   // clear
  axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_COUNT_REQ, 0x20);   // start
  start_hw_timer();
  usleep(LEMO_GATE_US);
  axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_COUNT_REQ, 0x30);   // stop
  stop_hw_timer();
  double gate = hw_timer_elapsed_us() / 1e6;
  for (int i = 0; i < 2; i++){
    axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_COUNT_REQ, 0x70 + i);   // read input counter i
    unsigned count = axil_read_register(C_SCOPE_ATC + C_ADDR_ATC_COUNT) & 0xFFFF;
    sat[i] = count >= LEMO_COUNT_MAX;
    hz[i] = count / gate;
  }
  axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_COUNT_REQ, 0x20);   // counters run, as after reset
}

static bool lemo_expected(double hz, bool sat){
  return !sat && hz >= LEMO_EXPECT_HZ * (100 - LEMO_TOL_PCT) / 100.0
              && hz <= LEMO_EXPECT_HZ * (100 + LEMO_TOL_PCT) / 100.0;
}

void atc_check_lemo_inputs(){
  static const char * NAME[2] = {"timing_trig_rate", "timing_pps_rate"};
  hw_val_t invert = axil_read_register(C_SCOPE_ATC + C_ADDR_ATC_CONFIG_INPUT);
  double hz[2];
  bool sat[2];
  lemo_rates(hz, sat);
  for (int i = 0; i < 2; i++){
    const char * inv = ((invert >> i) & 1) ? ", input inverted (ATC CONFIG_INPUT)" : "";
    check_result(lemo_expected(hz[i], sat[i]) ? "PASS" : "FAIL", NAME[i], "%s%.1f Hz (expected %d +/- %d%%) (%s%s)",
                 sat[i] ? "saturated, > " : "", hz[i], LEMO_EXPECT_HZ, LEMO_TOL_PCT, LEMO_NET[i], inv);
  }
}

// ATC outputs tested through the ATC test card, one tile at a time.  The tile's SYNC_O (ATC G) and
// TRIG_O (ATC H) come back on its PISO I0, I1.  Counts from the RX counters (STARTS, FRAME_ERR) and
// the last word from LOOK, without using the DMA stream.
#define ATC_POKES      100
#define ATC_POKE_US    100                   // between pokes:  back-to-back pokes merge into one pulse
#define ATC_PULSE      50                    // clocks (5 bit periods at RX 10 clocks per bit)
#define ATC_DST(duration, tiles, h, g)  (((duration) << 16) | ((tiles) << 6) | ((h) << 1) | (g))

static uint64_t rx_look(unsigned u){
  axil_write_register(SCOPE_RX + UART_GLOBAL + C_ADDR_RX_LOOK_SELECT, u);
  return ((uint64_t) axil_read_register(SCOPE_RX + UART_GLOBAL + C_ADDR_RX_LOOK_UPPER) << 32)
       | axil_read_register(SCOPE_RX + UART_GLOBAL + C_ADDR_RX_LOOK_LOWER);
}

static unsigned atc_output_count(unsigned cmd){   // 0x50 + tile (G) or 0x60 + tile (H)
  axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_COUNT_REQ, cmd);
  return axil_read_register(C_SCOPE_ATC + C_ADDR_ATC_COUNT) & 0xFFFF;
}

static void atc_outputs_zero(){
  zero_rxtx_counts();
  axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_COUNT_REQ, 0x10);   // clear
  axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_COUNT_REQ, 0x20);   // start
}

static void atc_outputs_pulse(int t, int h){   // h = 0:  sync (G, I0);  h = 1:  trig (H, I1)
  char name[32];
  unsigned u0 = 4*t, u = u0 + h;
  rx_uart_status_t s[4];

  snprintf(name, sizeof(name), "tile_%02d_%s", t+1, h ? "trig" : "sync");
  atc_outputs_zero();
  axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_DST_POKE_A, ATC_DST(ATC_PULSE, 1u << t, h, !h));
  for (int i = 0; i < ATC_POKES; i++){
    send_poke_a(1u << t);
    usleep(ATC_POKE_US);
  }
  usleep(10000);
  for (int k = 0; k < 4; k++) get_rx_uart_status(u0 + k, &s[k]);
  uint64_t word = rx_look(u);
  unsigned atc = atc_output_count((h ? 0x60 : 0x50) + t);
  uint64_t low = ~word + 1;                    // the word is ones above k zero bits:  ~word + 1 = 2^k
  int zeros = (word && !(low & (low - 1))) ? __builtin_ctzll(word) : -1;
  bool ok = s[h].starts == ATC_POKES && s[h].frame_err == 0 && zeros >= 3 && zeros <= 7
         && s[1-h].starts == 0 && atc == ATC_POKES;
  check_result(ok ? "PASS" : "FAIL", name, "%u of %u words on UART %u (%s %u, I3 %u), frame errors %u, last "
               "0x%016llx, ATC %u", s[h].starts, ATC_POKES, u+1, h ? "I0" : "I1", s[1-h].starts, s[3].starts,
               s[h].frame_err, (unsigned long long) word, atc);

  snprintf(name, sizeof(name), "tile_%02d_%s_mask", t+1, h ? "trig" : "sync");
  atc_outputs_zero();
  axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_DST_POKE_A, ATC_DST(ATC_PULSE, 0x3FFu, h, !h));
  for (int i = 0; i < ATC_POKES; i++){
    send_poke_a(0x3FFu & ~(1u << t));
    usleep(ATC_POKE_US);
  }
  usleep(10000);
  get_rx_uart_status(u, &s[h]);
  atc = atc_output_count((h ? 0x60 : 0x50) + t);
  check_result((s[h].starts == 0 && atc == 0) ? "PASS" : "FAIL", name,
               "%u words on UART %u, ATC %u, poked every other tile", s[h].starts, u+1, atc);
}

void atc_check_outputs(hw_u32_t tile_mask){
  static const hw_addr_t LEMO[2] = {C_ADDR_ATC_DST_LEMO_A, C_ADDR_ATC_DST_LEMO_B};
  global_status_t gs;
  get_global_status(&gs);
  hw_val_t enables = gs.enables;
  hw_val_t buffer_enables = axil_read_register(SCOPE_RX + UART_GLOBAL + C_ADDR_RX_BUFFER_ENABLES);
  hw_val_t lemo[2], poke_a = axil_read_register(C_SCOPE_ATC + C_ADDR_ATC_DST_POKE_A);
  for (int i = 0; i < 2; i++){
    lemo[i] = axil_read_register(C_SCOPE_ATC + LEMO[i]);
    axil_write_register(C_SCOPE_ATC + LEMO[i], 0);
  }
  axil_write_register(SCOPE_RX + UART_GLOBAL + C_ADDR_RX_BUFFER_ENABLES, 0);

  for (int t = 0; t < 10; t++){
    if (!((tile_mask >> t) & 1)){
      char name[32];
      snprintf(name, sizeof(name), "tile_%02d_timing_outputs", t+1);
      check_result("SKIP", name, "not in the tile mask 0x%03x", (unsigned) tile_mask);
      continue;
    }
    set_global_enables(1u << t);
    rx_set_config_all(RX_CONFIG_DEFAULT);
    for (unsigned u = 0; u < 40; u++)
      if (u < 4u*t || u >= 4u*t + 4) rx_disable_uart(u);
    usleep(10000);
    atc_outputs_pulse(t, 0);
    atc_outputs_pulse(t, 1);
  }

  rx_set_config_all(RX_CONFIG_DEFAULT);
  set_global_enables(enables);
  axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_DST_POKE_A, poke_a);
  for (int i = 0; i < 2; i++)
    axil_write_register(C_SCOPE_ATC + LEMO[i], lemo[i]);
  axil_write_register(SCOPE_RX + UART_GLOBAL + C_ADDR_RX_BUFFER_ENABLES, buffer_enables);
}

void atc_menu(){
  printf("ASIC timing and control (ATC) signal menu:  \r\n");
  while(1){
    printf("choose an option:\r\n");
    printf("(x) Exit timing menu (f) set ATC default configuration\r\n");
    printf("(r) read ATC registers (n) read ATC counts (t) toggle ATC destinations (s) toggle ATC run \r\n");
    printf("(a) poke A (b) poke B (c) poke C (d) poke D \r\n");
    printf("(o) check ATC outputs (i) check LEMO inputs \r\n");
    int input = input_choice();
    if (input == INPUT_END)
      return;
    printf("INFO: selected %c\r\n", input);

    switch(input){
    case 'x':
      return;
    case 'f':
      set_atc_default_config();
      break;
    case 'r':
      read_atc_registers();
      break;
    case 'n':
      read_atc_counts();
      break;
    case 't':
      toggle_atc_destinations();
      break;
    case 's':
      toggle_atc_run();
      break;
    case 'a':
      send_poke_a(0x3FF);
      break;
    case 'b':
      send_poke_b(0x3FF);
      break;
    case 'c':
      send_poke_c(0x3FF);
      break;
    case 'd':
      send_poke_d(0x3FF);
      break;
    case 'o':
      atc_check_outputs(0x3FF);
      break;
    case 'i':
      atc_check_lemo_inputs();
      break;
    default:
      printf("invalid selection...\r\n");
    }
  }
}
