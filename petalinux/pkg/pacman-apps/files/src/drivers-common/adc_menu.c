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
