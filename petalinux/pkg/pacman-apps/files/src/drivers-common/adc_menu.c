// adc_menu.c
//
// ADC checks (see adc_menu.h)

#include <stdio.h>
#include "hw_access.h"
#include "global.h"
#include "atc.h"
#include "iic.h"
#include "adc.h"
#include "capture.h"
#include "check.h"
#include "adc_menu.h"
#include "iic_menu.h"

#define ADC_READS     100
#define ADC_SETTLE_US 10000
static hw_u32_t CAP_BUF[65536];

static bool capture_present(hw_addr_t scope, hw_u32_t * depth){
  *depth = capture_depth(scope);
  return *depth >= 1024 && *depth <= 65536;
}


// *** The test DAC and the analog monitors (CLK on ana_mon) ***

#define ANA_SWING_MIN   100       // codes:  smallest swing that counts as a signal (DAC-0 noise is ~40 p-p)
#define ANA_LOOP_UART   20
#define ANA_DAC_VP      0x1000
#define ANA_DAC_MIN     200       // codes:  smallest direct DAC step that counts
#define ANA_DAC_LOOP    0.62      // loop step / direct step, inverted
#define ANA_DAC_PCT     10        // tolerance on that
#define ANA_LOOP_PCT    5         // loops:  mean period and edge count
static const unsigned ANA_CLK_UART[2] = {10, 20};
static const char * const ANA_LOOP_NAME[4] = {"series", "series_swap", "divider_p", "divider_n"};

// Capture the ADC with the ATC clock at period want and check for a clock of that period.
static bool ana_clk_capture(const char * name, unsigned want, hw_u32_t depth, bool strict){
  hw_u32_t pos;
  capture_config_t c;
  capture_default_config(&c);           // trigger bit 31:  only FORCE triggers
  capture_configure(SCOPE_ADC, &c, 0);
  axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_CONFIG_UART, want);
  usleep(1000);
  capture_arm(SCOPE_ADC);
  usleep(2000);
  capture_force(SCOPE_ADC);
  int timeout = capture_wait_done(SCOPE_ADC, 100000);
  hw_u32_t n = capture_read(SCOPE_ADC, CAP_BUF, depth, &pos, NULL);
  unsigned lo = ADC_CAP_DATA_MASK, hi = 0, ndof = 0;
  for (hw_u32_t i = 0; i < n; i++){
    unsigned d = CAP_BUF[i] & ADC_CAP_DATA_MASK;
    if (CAP_BUF[i] & ADC_CAP_DOF) ndof++;
    if (d < lo) lo = d;
    if (d > hi) hi = d;
  }
  // rising edges with hysteresis (a quarter of the swing) around the middle
  unsigned mid = (lo + hi) / 2, hyst = (hi - lo) / 4, nedges = 0, smin = 0xFFFFFFFF, smax = 0;
  int state = -1, last = -1;
  double first = 0, final = 0;
  for (hw_u32_t i = 0; n && i < n; i++){
    unsigned d = CAP_BUF[i] & ADC_CAP_DATA_MASK;
    if (d > mid + hyst){
      if (state == 0){
        if (last >= 0){ unsigned s = i - last;  if (s < smin) smin = s;  if (s > smax) smax = s; }
        else first = i;
        last = i; final = i; nedges++;
      }
      state = 1;
    } else if (d + hyst < mid) state = 0;
  }
  double mean = nedges > 1 ? (final - first) / (nedges - 1) : 0;
  double expect = n / (double) want, tol = ANA_LOOP_PCT / 100.0;
  bool ok = !timeout && n == depth && hi - lo >= ANA_SWING_MIN && nedges > 10
            && (strict ? mean > want - 0.05 && mean < want + 0.05 && smin + 1 >= want && smax <= want + 1
                       : mean > want * (1 - tol) && mean < want * (1 + tol) && nedges > expect * (1 - tol)
                         && nedges < expect * (1 + tol));
  check_result(ok ? "PASS" : "FAIL", name, "%u samples%s:  levels 0x%03x-0x%03x (swing %u), %u rising edges, %.2f "
               "clocks apart (%u-%u;  CONFIG_UART %u), DOF %u", (unsigned) n, timeout ? " (capture not done)" : "",
               lo, hi, hi - lo, nedges, mean, nedges > 1 ? smin : 0, smax, want, ndof);
  return ok;
}

static void ana_mon_tile(int t, hw_u32_t depth){
  char name[32];
  iic_set_mux_open();
  if (iic_set_mux_to_adc(IIC_MUX_TILE_1 + t, 1) || iic_platform_status()){
    snprintf(name, sizeof(name), "tile_%02d_ana_clk", t+1);
    check_result("FAIL", name, "MUX refused or I2C error (ADC common on TILE %d with 100 ohm, front panel open)",
                 t+1);
  } else {
    for (int k = 0; k < 2; k++){
      snprintf(name, sizeof(name), "tile_%02d_ana_clk_%u", t+1, ANA_CLK_UART[k]);
      ana_clk_capture(name, ANA_CLK_UART[k], depth, true);
    }
  }
  for (hw_val_t p = IIC_MUX_LOOP_SERIES; p <= IIC_MUX_LOOP_DIVIDER_N; p++){
    snprintf(name, sizeof(name), "tile_%02d_loop_%s", t+1, ANA_LOOP_NAME[p]);
    if (iic_set_mux_loop(IIC_MUX_TILE_1 + t, p) || iic_platform_status())
      check_result("FAIL", name, "MUX refused or I2C error (TILE %d, loop %s)", t+1, ANA_LOOP_NAME[p]);
    else
      ana_clk_capture(name, ANA_LOOP_UART, depth, false);
  }
}

// LOOK mean over ADC_READS reads
static double ana_look_mean(){
  double sum = 0;
  for (int i = 0; i < ADC_READS; i++) sum += axil_read_register(SCOPE_ADC + C_ADDR_ADC_LOOK) & ADC_LOOK_DATA_MASK;
  return sum / ADC_READS;
}

// the step V+ 0 -> ANA_DAC_VP (V- 0) in ADC codes, with the MUX as it is
static double ana_dac_step(){
  iic_set_test_dac_dn(0, 0);
  usleep(ADC_SETTLE_US);
  double zero = ana_look_mean();
  iic_set_test_dac_dn(ANA_DAC_VP, 0);
  usleep(ADC_SETTLE_US);
  double step = ana_look_mean() - zero;
  iic_set_test_dac_dn(0, 0);
  return step;
}

static void ana_mon_dac(){
  iic_set_mux_open();
  if (iic_set_mux_to_adc(IIC_MUX_DAC, 0) || iic_platform_status()){
    check_result("FAIL", "dac_direct", "MUX refused or I2C error (ADC common on the test DAC, front panel open)");
    return;
  }
  double direct = ana_dac_step();
  bool dok = direct >= ANA_DAC_MIN;
  check_result(dok ? "PASS" : "FAIL", "dac_direct", "%+.1f codes (V+ 0 -> 0x%04x, V- 0;  at least %d)", direct,
               ANA_DAC_VP, ANA_DAC_MIN);
  for (hw_val_t p = IIC_MUX_LOOP_SERIES; p <= IIC_MUX_LOOP_SERIES_SWAP; p++){
    char name[32];
    snprintf(name, sizeof(name), "dac_loop_%s", ANA_LOOP_NAME[p]);
    if (iic_set_mux_loop(IIC_MUX_DAC, p) || iic_platform_status()){
      check_result("FAIL", name, "MUX refused or I2C error (DAC, loop %s)", ANA_LOOP_NAME[p]);
      continue;
    }
    double step = ana_dac_step();
    double want = -direct * ANA_DAC_LOOP;
    bool ok = dok && step <= want * (100 - ANA_DAC_PCT) / 100 && step >= want * (100 + ANA_DAC_PCT) / 100;
    check_result(ok ? "PASS" : "FAIL", name, "%+.1f codes (expected %+.1f:  direct %+.1f x -%.2f, +/- %d%%)", step,
                 want, direct, ANA_DAC_LOOP, ANA_DAC_PCT);
  }
}


// *** The test-DAC sawtooth and the ADC setups ***

#define ANA_SAW_STEPS      32
#define ANA_SAW_DAC_MAX    0x6000    // below the ADC's top (it clips near 0x6400)
#define ANA_SAW_NOISE      16        // DN:  a step back of block means smaller than this is noise
#define ANA_SAW_BLOCK      32        // samples per block mean (a DAC step is ~80 samples)
#define ANA_SAW_MIN_RAMPS  3
#define ANA_SAW_MAX_RAMPS  400       // give up after this many ramps without a done capture
#define ADC_SETUP_NONE     0
#define ADC_SETUP_DAC      1
#define ADC_SETUP_MON      2
#define ADC_NSETUP         3

static const char * const ADC_SETUP_NAME[ADC_NSETUP] = {"none", "test DAC", "analog monitor"};
static int G_ADC_SETUP = ADC_SETUP_NONE;
static int G_ADC_TILE = 0;                    // 0-9:  TILE 1-10

// Wake the ADC on its pins.
static void adc_wake(){
  axil_write_register(SCOPE_ADC + C_ADDR_ADC_CONFIG, ADC_CONFIG_ENABLE);
  adc_select(ADC_MUX_PINS);
  adc_init();
  adc_set_sleep(0);
}

// The test-DAC setup;  lo, hi:  LOOK at DAC 0 and ANA_SAW_DAC_MAX.  Non-zero if the MUX refused.
static int adc_setup_test_dac(double * lo, double * hi){
  adc_wake();
  iic_set_mux_open();
  if (iic_set_mux_to_adc(IIC_MUX_DAC, 0) || iic_platform_status())
    return 1;
  iic_set_test_dac_dn(ANA_SAW_DAC_MAX, 0);
  usleep(ADC_SETTLE_US);
  *hi = ana_look_mean();
  iic_set_test_dac_dn(0, 0);
  usleep(ADC_SETTLE_US);
  *lo = ana_look_mean();
  double a = *lo < *hi ? *lo : *hi, b = *lo < *hi ? *hi : *lo;
  adc_set_comparator((hw_u32_t) ((a + b) / 2), (hw_u32_t) ((b - a) / 8));
  capture_config_t c;
  capture_default_config(&c);
  c.period = ADC_SAW_PERIOD;
  c.trig_bit = ADC_CAP_LEVEL_BIT;
  c.trig_edge = *hi > *lo ? CAP_EDGE_FALL : CAP_EDGE_RISE;
  c.post = capture_depth(SCOPE_ADC) / 2;
  capture_configure(SCOPE_ADC, &c, 0);
  return 0;
}

// The analog-monitor setup.  Non-zero if the MUX refused.
static int adc_setup_monitor(){
  adc_wake();
  iic_set_mux_open();
  if (iic_set_mux_to_adc(IIC_MUX_TILE_1 + G_ADC_TILE, 0) || iic_platform_status())
    return 1;
  capture_config_t c;
  capture_default_config(&c);          // every clock, trigger bit 31:  forced only
  c.post = capture_depth(SCOPE_ADC) / 2;
  capture_configure(SCOPE_ADC, &c, 0);
  return 0;
}

// Configure the current setup (none:  nothing).
static void adc_apply_setup(){
  double lo, hi;
  int refused = 0;
  if (G_ADC_SETUP == ADC_SETUP_DAC) refused = adc_setup_test_dac(&lo, &hi);
  if (G_ADC_SETUP == ADC_SETUP_MON) refused = adc_setup_monitor();
  if (refused) printf("MUX refused or I2C error\r\n");
}

static void adc_print_setup(){
  printf("ADC setup:  %s", ADC_SETUP_NAME[G_ADC_SETUP]);
  if (G_ADC_SETUP == ADC_SETUP_MON) printf(",  TILE %d", G_ADC_TILE + 1);
  printf("\r\n");
}

// Least-squares slope and largest residual of y over x, points first to n-1.
static void ana_fit(const double * x, const double * y, int first, int n, double * slope,
                    double * resid){
  double sx = 0, sy = 0, sxx = 0, sxy = 0;
  int m = n - first;
  for (int i = first; i < n; i++){ sx += x[i]; sy += y[i]; sxx += x[i] * x[i]; sxy += x[i] * y[i]; }
  *slope = (m * sxy - sx * sy) / (m * sxx - sx * sx);
  double off = (sy - *slope * sx) / m;
  *resid = 0;
  for (int i = first; i < n; i++){
    double r = y[i] - (*slope * x[i] + off);
    if (r < 0) r = -r;
    if (r > *resid) *resid = r;
  }
}

static void ana_dac_sawtooth(){
  double lo, hi, mv[ANA_SAW_STEPS + 1], dn[ANA_SAW_STEPS + 1];
  if (adc_setup_test_dac(&lo, &hi)){
    check_result("FAIL", "adc_sawtooth", "MUX refused or I2C error (ADC common on the test DAC)");
    return;
  }
  for (int k = 0; k <= ANA_SAW_STEPS; k++){
    hw_val_t d = ANA_SAW_DAC_MAX * k / ANA_SAW_STEPS;
    iic_set_test_dac_dn(d, 0);
    usleep(ADC_SETTLE_US);
    mv[k] = d * 2500.0 / 65536;
    dn[k] = ana_look_mean();
  }
  double gain, resid;
  ana_fit(mv, dn, 1, ANA_SAW_STEPS + 1, &gain, &resid);

  iic_set_test_dac_dn(0, 0);
  usleep(ADC_SETTLE_US);
  capture_arm(SCOPE_ADC);                 // at DAC 0:  the first ramp is recorded whole
  int ramps = 0;
  while (!(capture_status(SCOPE_ADC) & CAP_DONE) && ramps < ANA_SAW_MAX_RAMPS){
    for (int k = 0; k < ANA_SAW_STEPS; k++)
      iic_set_test_dac_dn(ANA_SAW_DAC_MAX * k / ANA_SAW_STEPS, 0);
    ramps++;
  }
  iic_set_test_dac_dn(0, 0);
  if (!(capture_status(SCOPE_ADC) & CAP_DONE)){
    check_result("FAIL", "adc_sawtooth", "no capture after %d ramps", ramps);
    return;
  }
  hw_u32_t n = capture_read(SCOPE_ADC, CAP_BUF, sizeof(CAP_BUF) / sizeof(CAP_BUF[0]), NULL, NULL);
  int resets = 0, back = 0;
  double half = (hi > lo ? hi - lo : lo - hi) / 2, prev = 0;
  bool skip = true;                       // no comparison for the first block and after a reset
  bool lead = false;                      // the buffer starts at the bottom of a ramp
  for (hw_u32_t b = 0; b + ANA_SAW_BLOCK <= n; b += ANA_SAW_BLOCK){
    double sum = 0, mn = ADC_CAP_DATA_MASK, mx = 0;
    for (hw_u32_t i = b; i < b + ANA_SAW_BLOCK; i++){
      double d = CAP_BUF[i] & ADC_CAP_DATA_MASK;
      sum += d;
      if (d < mn) mn = d;
      if (d > mx) mx = d;
    }
    double mean = sum / ANA_SAW_BLOCK;
    if (!b) lead = (hi > lo ? mean - lo : lo - mean) < half / 4;
    if (mx - mn > half){                   // a reset inside the block
      resets++;
      skip = true;
    } else {
      if (!skip && prev - mean >= half) resets++;
      else if (!skip && mean < prev - ANA_SAW_NOISE) back++;
      skip = false;
    }
    prev = mean;
  }
  int full = resets > 0 ? resets - 1 + (lead ? 1 : 0) : 0;
  double dev = 100 * (gain - ADC_SAW_DN_PER_MV) / ADC_SAW_DN_PER_MV;
  bool ok = full >= ANA_SAW_MIN_RAMPS && back == 0 && dev <= ADC_SAW_GAIN_PCT
            && dev >= -ADC_SAW_GAIN_PCT && resid <= ADC_SAW_RESIDUAL_DN;
  check_result(ok ? "PASS" : "FAIL", "adc_sawtooth",
               "%d full ramps (at least %d), %d steps back;  %.3f DN/mV (%.3f +/- %d%%), "
               "residual %.1f DN (at most %d)", full, ANA_SAW_MIN_RAMPS, back, gain,
               ADC_SAW_DN_PER_MV, ADC_SAW_GAIN_PCT, resid, ADC_SAW_RESIDUAL_DN);
}

void toggle_adc_setup(){
  G_ADC_SETUP = (G_ADC_SETUP + 1) % ADC_NSETUP;
  adc_apply_setup();
  adc_print_setup();
}

void toggle_adc_tile(){
  G_ADC_TILE = (G_ADC_TILE + 1) % 10;
  if (G_ADC_SETUP == ADC_SETUP_MON) adc_apply_setup();
  adc_print_setup();
}

void adc_arm(){
  capture_arm(SCOPE_ADC);
  printf("ADC armed (%s)\r\n", ADC_SETUP_NAME[G_ADC_SETUP]);
}

void adc_force(){
  capture_force(SCOPE_ADC);
  printf("ADC forced\r\n");
}

void adc_status(){
  hw_val_t st = capture_status(SCOPE_ADC);
  printf("ADC status 0x%x:  %s%s%s\r\n", (unsigned) st, (st & CAP_ARMED) ? "armed " : "",
         (st & CAP_TRIGGERED) ? "triggered " : "", (st & CAP_DONE) ? "done" : "");
  adc_print_setup();
  if (!(st & CAP_DONE))
    return;
  hw_u32_t pos;
  uint64_t ts;
  hw_u32_t n = capture_read(SCOPE_ADC, CAP_BUF, sizeof(CAP_BUF) / sizeof(CAP_BUF[0]), &pos, &ts);
  unsigned lo = ADC_CAP_DATA_MASK, hi = 0, ndof = 0;
  double sum = 0;
  for (hw_u32_t i = 0; i < n; i++){
    unsigned d = CAP_BUF[i] & ADC_CAP_DATA_MASK;
    if (CAP_BUF[i] & ADC_CAP_DOF) ndof++;
    if (d < lo) lo = d;
    if (d > hi) hi = d;
    sum += d;
  }
  printf("%u samples, trigger at %u, timestamp %llu\r\n", (unsigned) n, (unsigned) pos,
         (unsigned long long) ts);
  if (n) printf("ADC (DN):  min %u, max %u, mean %.1f;  overflow %u\r\n", lo, hi, sum / n, ndof);
}

void adc_check(){
  hw_val_t config0 = axil_read_register(SCOPE_ADC + C_ADDR_ADC_CONFIG);
  hw_val_t status = axil_read_register(SCOPE_ADC + C_ADDR_ADC_STATUS);
  check_result(status == ADC_STATUS_VALUE ? "PASS" : "FAIL", "adc_status", "0x%08x (expected 0x%08x)",
               (unsigned) status, (unsigned) ADC_STATUS_VALUE);
  axil_write_register(SCOPE_ADC + C_ADDR_ADC_CONFIG, ADC_CONFIG_ENABLE);
  hw_val_t config = axil_read_register(SCOPE_ADC + C_ADDR_ADC_CONFIG);
  check_result(config == ADC_CONFIG_ENABLE ? "PASS" : "FAIL", "adc_config", "0x%08x after writing 0x%08x",
               (unsigned) config, (unsigned) ADC_CONFIG_ENABLE);

  hw_u32_t depth;
  bool cap = capture_present(SCOPE_ADC, &depth);
  check_result(cap ? "PASS" : "SKIP", "adc_capture_depth", "%u samples%s", (unsigned) depth,
               cap ? "" : ":  no ADC capture");

  // the test DAC into the ADC through the MUX (ADC common, front panel open)
  adc_select(ADC_MUX_PINS);
  adc_init();
  adc_set_sleep(0);
  iic_set_test_dac_dn(0, 0);
  usleep(ADC_SETTLE_US);
  ana_mon_dac();
  ana_dac_sawtooth();
  iic_set_test_dac_dn(0, 0);
  iic_set_mux_open();
  if (iic_platform_status())
    check_result("FAIL", "adc_mux_open", "I2C error (status 0x%x)", (unsigned) iic_platform_status());
  else
    check_result("PASS", "adc_mux_open", "DAC 0, MUX open");
  axil_write_register(SCOPE_ADC + C_ADDR_ADC_CONFIG, config0);
  adc_set_sleep(1);
}

void adc_check_ana_mon(hw_u32_t tile_mask){
  hw_u32_t depth;
  if (!capture_present(SCOPE_ADC, &depth)){
    check_result("FAIL", "ana_mon", "no ADC capture");
    return;
  }
  global_status_t gs;
  get_global_status(&gs);
  hw_val_t enables = gs.enables, uart = axil_read_register(C_SCOPE_ATC + C_ADDR_ATC_CONFIG_UART);
  hw_val_t config0 = axil_read_register(SCOPE_ADC + C_ADDR_ADC_CONFIG);
  axil_write_register(SCOPE_ADC + C_ADDR_ADC_CONFIG, ADC_CONFIG_ENABLE);
  adc_select(ADC_MUX_PINS);
  adc_init();
  adc_set_sleep(0);
  iic_set_test_dac_dn(0, 0);
  usleep(ADC_SETTLE_US);

  for (int t = 0; t < 10; t++){
    if (!((tile_mask >> t) & 1)){
      char name[32];
      snprintf(name, sizeof(name), "tile_%02d_ana_mon", t+1);
      check_result("SKIP", name, "not in the tile mask 0x%03x", (unsigned) tile_mask);
      continue;
    }
    set_global_enables(1u << t);
    usleep(ADC_SETTLE_US);
    ana_mon_tile(t, depth);
  }
  set_global_enables(enables);
  axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_CONFIG_UART, uart);

  iic_set_test_dac_dn(0, 0);
  iic_set_mux_open();
  if (iic_platform_status())
    check_result("FAIL", "ana_mon_mux_open", "I2C error (status 0x%x)", (unsigned) iic_platform_status());
  else
    check_result("PASS", "ana_mon_mux_open", "DAC 0, MUX open");
  axil_write_register(SCOPE_ADC + C_ADDR_ADC_CONFIG, config0);
  adc_set_sleep(1);
}

void adc_menu(){
  while(1){
    printf("ADC MENU:  choose an option:\r\n");
    printf("(x) main menu (r) read ADC registers (s) toggle ADC sleep (e) toggle ADC enable \r\n");
    printf("(d) toggle DAC (c) check ADC (registers, test DAC) (m) check analog monitors \r\n");
    printf("(u) toggle setup (t) toggle tile (a) arm (f) force (w) status \r\n");
    int input = input_choice();
    if (input == INPUT_END)
      return;
    printf("INFO: selected %c\r\n", input);

    switch(input){
    case 'x':
      return;
    case 'r':
      adc_read_registers();
      break;
    case 's':
      adc_toggle_sleep();
      break;
    case 'e':
      adc_toggle_config();
      break;
    case 'd':
      iic_toggle_test_dac();
      break;
    case 'c':
      adc_check();
      break;
    case 'm':
      adc_check_ana_mon(0x3FF);
      break;
    case 'u':
      toggle_adc_setup();
      break;
    case 't':
      toggle_adc_tile();
      break;
    case 'a':
      adc_arm();
      break;
    case 'f':
      adc_force();
      break;
    case 'w':
      adc_status();
      break;
    default:
      printf("invalid selection...\r\n");
    }
  }
}
