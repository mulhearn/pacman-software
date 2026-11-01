#include <stdio.h>
#include <string.h>

#include "hw_access.h"
#include "global.h"
#include "iic.h"
#include "jumper.h"
#include "led.h"
#include "rxtx.h"
#include "dma.h"
#include "check.h"
#include "rxtx_menu.h"
#include "iic_menu.h"
#include "atc_menu.h"
#include "adc_menu.h"
#include "hwcheck_menu.h"

static hw_u32_t G_TILE_MASK = 0x3FF;

// *** READ ONLY STATUS REPORT ***

static void read_only_jumper(){
  int j5 = jumper_installed();
  if (j5 == JUMPER_UNREADABLE)
    check_result("FAIL", "j5_jumper", "unreadable (MIO%d)", JUMPER_J5_MIO);
  else
    check_result("INFO", "j5_jumper", (j5 == JUMPER_IN) ? "installed" : "not installed");
}

static void read_only_global(global_status_t * gs){
  char date[32], hash[32];
  get_synthesis_date_string(date, sizeof(date));
  get_git_hash_string(hash, sizeof(hash));

  // Zipline firmware is 3.X; hwcheck v1 is for hardware 1v5:
  check_result((gs->firmware_major == 3) ? "PASS" : "FAIL", "firmware_version", "%u.%u.%u",
               (unsigned) gs->firmware_major, (unsigned) gs->firmware_minor, (unsigned) gs->firmware_patch);
  check_result((gs->hardware_major == 1 && gs->hardware_minor == 5) ? "PASS" : "FAIL", "hardware_version", "%u.%u.%u",
               (unsigned) gs->hardware_major, (unsigned) gs->hardware_minor, (unsigned) gs->hardware_patch);
  check_result("INFO", "synthesis_date", "%s", date);
  check_result("INFO", "git_hash", "%s", hash);
  char build[48];
  get_build_string(gs->build_flags, build, sizeof(build));
  check_result("INFO", "firmware_build", "%s", build);
  check_result("INFO", "vivado_version", "%u.%u", (unsigned) gs->vivado_major, (unsigned) gs->vivado_minor);
  check_result("INFO", "enables", "0x%08x", (unsigned) gs->enables);
  check_result("INFO", "status", "0x%08x", (unsigned) gs->status);
}

static void read_only_dma_status(const char * name, hw_val_t sr){
  const hw_val_t errors = DMASR_DMA_INT_ERR | DMASR_DMA_SEC_ERR | DMASR_DMA_DEC_ERR
                        | DMASR_SG_INT_ERR  | DMASR_SG_SEC_ERR  | DMASR_SG_DEC_ERR;
  check_result((sr & errors) ? "FAIL" : "INFO", name, "0x%08x%s%s", (unsigned) sr,
               (sr & DMASR_HALTED) ? " halted" : "", (sr & errors) ? " ERROR" : "");
}

static void read_only_rxtx(){
  unsigned enabled = 0, frame = 0, noise = 0, lost = 0;
  rx_uart_status_t us;
  for (unsigned i = 0; i < 40; i++){
    get_rx_uart_status(i, &us);
    if (rx_uart_is_enabled(i)) enabled++;
    frame += us.frame_err;
    noise += us.noise;
    lost  += us.lost;
  }
  check_result("INFO", "rx_uarts_enabled", "%u of 40", enabled);
  check_result("INFO", "rx_frame_errors", "%u", frame);
  check_result("INFO", "rx_noise", "%u", noise);
  check_result("INFO", "rx_lost", "%u", lost);

  dma_platform_init();
  read_only_dma_status("dma_tx_status", dma_get_tx_status());
  read_only_dma_status("dma_rx_status", dma_get_rx_status());
}

// one I2C monitor reading; a failed transfer is reported as such, not as a value:
static void iic_reading(const char * name, const char * fmt, unsigned a, unsigned b, hw_u32_t status){
  if (status != 0)
    check_result("FAIL", name, "no response (I2C status 0x%x)", (unsigned) status);
  else
    check_result("INFO", name, fmt, a, b);
}

static void read_only_iic(){
  const char * board[] = {"3V6", "3V3", "3V0"};
  char name[32];
  unsigned a, b;

  for (int i = 0; i < 3; i++){
    snprintf(name, sizeof(name), "board_%s", board[i]);
    a = iic_mon_vboard_mv(i);
    b = iic_mon_iboard_ma(i);
    iic_reading(name, "%5u mV %5u mA", a, b, iic_platform_status());
  }
  a = iic_mon_vboard_mv(3);
  b = iic_mon_probe_dn();
  iic_reading("rtd_probe", "%5u mV 0x%04x DN", a, b, iic_platform_status());

  for (int i = 0; i < 10; i++){
    snprintf(name, sizeof(name), "tile_%02d_vdda", i+1);
    a = iic_mon_vdda_mv(i);
    b = iic_mon_idda_ma(i);
    iic_reading(name, "%5u mV %5u mA", a, b, iic_platform_status());
    snprintf(name, sizeof(name), "tile_%02d_vddd", i+1);
    a = iic_mon_vddd_mv(i);
    b = iic_mon_iddd_ma(i);
    iic_reading(name, "%5u mV %5u mA", a, b, iic_platform_status());
  }
}

void hwcheck_read_only(bool no_iic){
  global_status_t gs;
  get_global_status(&gs);

  read_only_jumper();
  read_only_global(&gs);
  read_only_rxtx();
  if (no_iic)
    check_result("SKIP", "i2c_monitors", "no I2C");
  else
    read_only_iic();
}

// *** LOOPBACK TEST ***

static void loopback_counters(const char * name, uint64_t uart_mask){
  rx_uart_status_t us;
  unsigned frame = 0, noise = 0, lost = 0;
  for (unsigned u = 0; u < 40; u++){
    if (!((uart_mask >> u) & 1)) continue;
    get_rx_uart_status(u, &us);
    frame += us.frame_err;
    noise += us.noise;
    lost  += us.lost;
  }
  check_result("INFO", name, "frame errors %u, noise %u, lost %u", frame, noise, lost);
}

static const char * loopback_limit(const rxtx_check_t * r){
  return r->timeout == 1 ? ", TIME LIMIT (drain)" : r->timeout == 2 ? ", TIME LIMIT" : "";
}

// One loopback run on the tiles in tiles (power on, RX on their UARTs only), with result lines
// under prefix;  returns the stray words.
static unsigned loopback_run(const char * prefix, hw_u32_t tiles, hw_u32_t seed){
  rxtx_check_t r;
  char name[48];
  uint64_t uarts = rxtx_tile_uarts(tiles);

  set_global_enables(GLOBAL_ENABLE_TILE_POWER | tiles);
  rx_set_config_all(RX_CONFIG_DEFAULT);
  for (unsigned u = 0; u < 40; u++)
    if (!((uarts >> u) & 1)) rx_disable_uart(u);
  rxtx_set_uart_mask(uarts);
  usleep(10000);
  zero_rxtx_counts();
  rxtx_loopback(uarts, rxtx_loopback_packets(), true, seed, &r);

  bool words_ok = (r.words_sent > 0) && (r.words_rcvd == r.words_sent) && !r.tx_timeout && !r.timeout
                  && !r.bad_packets;
  snprintf(name, sizeof(name), "%s_loopback", prefix);
  check_result(words_ok ? "PASS" : "FAIL", name, "%u of %u received in %.2f s%s%s", r.words_rcvd,
               r.words_sent, r.elapsed_us / 1e6, r.tx_timeout ? ", TX TIMEOUT" : "",
               loopback_limit(&r));
  unsigned long long bits = 64ULL * r.words_rcvd;
  snprintf(name, sizeof(name), "%s_bit_errors", prefix);
  if (r.bit_errors)
    check_result("FAIL", name, "%u in %llu bits (%u words):  BER %.1e", r.bit_errors, bits,
                 r.words_bad, (double) r.bit_errors / (double) bits);
  else
    check_result(bits ? "PASS" : "FAIL", name, "0 in %llu bits:  BER < %.1e (95%% CL)", bits,
                 bits ? 3.0 / (double) bits : 1.0);
  snprintf(name, sizeof(name), "%s_rx_counters", prefix);
  loopback_counters(name, uarts);
  if (r.stray_words){
    char list[128] = "";
    for (unsigned u = 0; u < 40; u++){
      if (((uarts >> u) & 1) || !r.rcvd[u]) continue;
      size_t n = strlen(list);
      snprintf(list + n, sizeof(list) - n, " UART %u: %u", u+1, r.rcvd[u]);
    }
    snprintf(name, sizeof(name), "%s_stray", prefix);
    check_result("FAIL", name, "%u words on disabled UARTs:%s", r.stray_words, list);
  }
  return r.stray_words;
}

void hwcheck_loopback(){
  hw_u32_t tile_mask = G_TILE_MASK;
  unsigned stray = 0;
  char prefix[16];

  global_status_t gs;
  get_global_status(&gs);
  hw_val_t enables = gs.enables;
  hw_val_t buffer_enables = axil_read_register(SCOPE_RX + UART_GLOBAL + C_ADDR_RX_BUFFER_ENABLES);
  hw_val_t buffer_config = rx_get_buffer_config();
  uint64_t uart_mask = rxtx_uart_mask();
  axil_write_register(SCOPE_RX + UART_GLOBAL + C_ADDR_RX_BUFFER_ENABLES, 0);
  rx_set_buffer_config(RX_BUFFER_CONFIG_DAQ);
  for (int t = 0; t < 10; t++){
    iic_set_vdda_dn(t, 0);
    iic_set_vddd_dn(t, 0);
  }

  for (int t = 0; t < 10; t++){
    snprintf(prefix, sizeof(prefix), "tile_%02d", t+1);
    if (!((tile_mask >> t) & 1)){
      char name[32];
      snprintf(name, sizeof(name), "%s_loopback", prefix);
      check_result("SKIP", name, "not in the tile mask 0x%03x", (unsigned) tile_mask);
      continue;
    }
    stray += loopback_run(prefix, 1u << t, 1 + t);
  }
  if (tile_mask)
    stray += loopback_run("all_tiles", tile_mask, 11);

  rx_set_config_all(RX_CONFIG_DEFAULT);
  rxtx_set_uart_mask(uart_mask);
  set_global_enables(enables);
  rx_set_buffer_config(buffer_config);
  axil_write_register(SCOPE_RX + UART_GLOBAL + C_ADDR_RX_BUFFER_ENABLES, buffer_enables);
  if (tile_mask)
    check_result(stray ? "FAIL" : "PASS", "loopback_stray", "%u words on disabled UARTs", stray);
}

// *** MENU ***

unsigned hwcheck_get_tile_mask(){
  return G_TILE_MASK;
}

void hwcheck_set_tile_mask(unsigned mask){
  G_TILE_MASK = mask & 0x3FF;
}

void hwcheck_toggle_tile_mask(){
  static const hw_u32_t presets[] = {
    0x3FF,   // every tile (the default)
    0x001,   // tile 1 only
    0x0FF,   // tiles 1-8 (hw1v4)
    0x003,   // the loopback test card (bench 1v5)
    0x018,   // the ATC test card (bench 1v5)
  };
  static int mode = 0;
  mode = (mode + 1) % (int) (sizeof(presets) / sizeof(presets[0]));
  G_TILE_MASK = presets[mode];
  printf("INFO: tile mask 0x%03x\r\n", (unsigned) G_TILE_MASK);
}

// The I2C hardware version is disabled (I2C menu):  the I2C tests SKIP.
static bool no_iic(){
  return iic_get_hw_version() == 0;
}

// Run one writing test:  refused with J5 installed;  an I2C test SKIPs without I2C.
static void run_test(const char * name, bool needs_iic, void (*test)(void)){
  if (!jumper_allows_writes())
    return;
  if (needs_iic && no_iic()){
    check_result("SKIP", name, "I2C disabled");
    return;
  }
  test();
}

static void test_power()       { iic_check_power(G_TILE_MASK); }
static void test_loopback()    { hwcheck_loopback(); }
static void test_atc_outputs() { atc_check_outputs(G_TILE_MASK); }
static void test_lemo_inputs() { atc_check_lemo_inputs(); }
static void test_ana_mon()     { adc_check_ana_mon(G_TILE_MASK); }
static void test_adc()         { adc_check(); }
static void test_blink()       { led_check_blink(true); }
static void test_sma()         { iic_check_sma(true); }
static void test_i2c_stress()  { iic_check_stress(IIC_STRESS_SWEEPS); }

void hwcheck_menu(){
  init_jumper();
  printf("Hardware check menu:  \r\n");
  while(1){
    printf("choose an option:  (tile mask 0x%03x)\r\n", (unsigned) G_TILE_MASK);
    printf("(x) Exit hardware check menu (t) toggle tile mask (o) read only \r\n");
    printf("(p) power (l) loopback (a) ATC outputs (i) LEMO inputs (m) analog monitor \r\n");
    printf("(d) ADC (b) blink (s) SMA (z) I2C stress \r\n");
    int input = input_choice();
    if (input == INPUT_END)
      return;
    printf("INFO: selected %c\r\n", input);

    switch(input){
    case 'x':
      return;
    case 't':
      hwcheck_toggle_tile_mask();
      break;
    case 'o':
      hwcheck_read_only(no_iic());
      break;
    case 'p':
      run_test("power", true, test_power);
      break;
    case 'l':
      run_test("loopback", true, test_loopback);
      break;
    case 'a':
      run_test("atc_outputs", false, test_atc_outputs);
      break;
    case 'i':
      run_test("lemo_inputs", false, test_lemo_inputs);
      break;
    case 'm':
      run_test("ana_mon", true, test_ana_mon);
      break;
    case 'd':
      run_test("adc", true, test_adc);
      break;
    case 'b':
      run_test("blink", false, test_blink);
      break;
    case 's':
      run_test("sma", true, test_sma);
      break;
    case 'z':
      run_test("i2c_stress", true, test_i2c_stress);
      break;
    default:
      printf("invalid selection...\r\n");
    }
  }
}
