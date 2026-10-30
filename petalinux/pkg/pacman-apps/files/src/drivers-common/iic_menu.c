#include <stdlib.h>
#include <stdio.h>

#include "hw_access.h"
#include "iic.h"
#include "iic_menu.h"
#include "global.h"
#include "check.h"


void iic_show_status(){
  printf("IIC status menu\n");
  printf("HW enum:  %u\n", (unsigned int) iic_get_hw_version());
  iic_platform_print_status();
}

void iic_reset_bus(){
  printf("INFO: resetting the I2C bus and clearing the I2C status\r\n");
  iic_platform_reset();
  iic_platform_print_status();
}


void iic_toggle_hardware_version(){
  static int mode = 0;
  mode = (mode + 1) % 5;

  if (mode==0){
    printf("INFO: setting I2C HW version to latest (PACMAN 1V5)\r\n");
    iic_set_hw_version(1,5,0);
  } else if (mode==1){
    printf("INFO: setting I2C HW version to PACMAN 1V4 \r\n");
    iic_set_hw_version(1,4,0);
  } else if (mode==2){
    printf("INFO: setting I2C HW version to PACMAN 1V3 \r\n");
    iic_set_hw_version(1,3,0);
  } else if (mode==3){
    printf("INFO: setting I2C HW version to disabled \r\n");
    iic_set_hw_version(0,0,0);
  } else {
    printf("INFO: setting I2C to the simulated board (no hardware) \r\n");
    iic_set_simulated();
  }
}

void iic_toggle_power(){
  unsigned vddd[] = {0x00, 0x2000, 0x4000, 0x8000, 0xFFFF};
  unsigned vdda[] = {0x00, 0x2000, 0x4000, 0x8000, 0xFFFF};
  static int mode = 0;
  mode = (mode + 1) % 5;

  printf("INFO: setting VDDD to 0x%x \r\n", vddd[mode]);
  for (int i=0; i<10; i++){
    iic_set_vddd_dn(i, vddd[mode]);
  }

  printf("INFO: setting VDDA to 0x%x \r\n", vdda[mode]);
  for (int i=0; i<10; i++){
    iic_set_vdda_dn(i, vdda[mode]);
  }
}

void iic_monitor_tile_power(int i){
  printf("TILE %2d POWER SUMMARY:\r\n", i+1);
  unsigned vdda = iic_mon_vdda_mv(i);
  unsigned vddd = iic_mon_vddd_mv(i);
  unsigned idda = iic_mon_idda_ma(i);
  unsigned iddd = iic_mon_iddd_ma(i);

  printf("VDDA:  voltage:  %5d mV current: %5d mA\r\n", vdda, idda);
  printf("VDDD:  voltage:  %5d mV current: %5d mA\r\n", vddd, iddd);
}

void iic_monitor_power(){
  for (int i=0; i<10; i++){
    iic_monitor_tile_power(i);
  }

  printf("BOARD POWER SUMMARY:\r\n");

  unsigned vba = iic_mon_vboard_mv(0);
  unsigned vbb = iic_mon_vboard_mv(1);
  unsigned vbc = iic_mon_vboard_mv(2);
  unsigned vbd = iic_mon_vboard_mv(3);

  unsigned iba = iic_mon_iboard_ma(0);
  unsigned ibb = iic_mon_iboard_ma(1);
  unsigned ibc = iic_mon_iboard_ma(2);
  unsigned prb = iic_mon_probe_dn();
  printf("Board 3V6:  voltage:  %5d mV  current:  %5d mA\r\n",  vba, iba);
  printf("Board 3V3:  voltage:  %5d mV  current:  %5d mA\r\n",  vbb, ibb);
  printf("Board 3V0:  voltage:  %5d mV  current:  %5d mA\r\n",  vbc, ibc);
  printf("RTD Probe:  3V3:      %5d mV  current:  0x%04X DN\r\n",   vbd, prb);
}

void iic_toggle_test_dac(){
  // VDDA DAC is used for postive  end of differential test DAC output
  // VDDD DAC is used for negative end of differntial test DAC output

  unsigned vp[] = {0x00, 0x1000, 0x0000, 0x2000, 0x0000};
  unsigned vn[] = {0x00, 0x0000, 0x1000, 0x0000, 0x2000};
  static int mode = 0;
  mode = (mode + 1) % 5;

  printf("INFO: setting DAC test VP to 0x%x \r\n", vp[mode]);
  printf("INFO: setting DAC test VN to 0x%x \r\n", vn[mode]);
  iic_set_test_dac_dn(vp[mode], vn[mode]);

}

void iic_get_iv_curve(hw_val_t itile, iic_iv_point_t iv[IIC_IV_POINTS]){
  for (unsigned k=0; k<IIC_IV_POINTS; k++){
    hw_val_t vset = k * 0x1000;
    iic_set_vdda_dn(itile, vset);
    iic_set_vddd_dn(itile, vset);
    usleep(10);
    iv[k].dac_dn  = vset;
    iv[k].vdda_mv = iic_mon_vdda_mv(itile);
    iv[k].idda_ma = iic_mon_idda_ma(itile);
    iv[k].vddd_mv = iic_mon_vddd_mv(itile);
    iv[k].iddd_ma = iic_mon_iddd_ma(itile);
  }
  iic_set_vdda_dn(itile, 0);
  iic_set_vddd_dn(itile, 0);
}

void iic_record_iv_curves(){
  iic_iv_point_t iv[IIC_IV_POINTS];

  printf("INFO: first setting all voltages to zero.\r\n");
  for (int i=0; i<10; i++){
    iic_set_vdda_dn(i, 0);
    iic_set_vddd_dn(i, 0);
  }

  for (int i=0; i<10; i++){
    printf("INFO:  VDDD/VDDA IV curves for Tile %d\r\n", i+1);
    iic_get_iv_curve(i, iv);
    for (unsigned k=0; k<IIC_IV_POINTS; k++)
      printf("INFO:  vset: 0x%04x vdda: %7d idda: %7d vddd: %7d iddd: %7d\r\n", (unsigned) iv[k].dac_dn,
             (int) iv[k].vdda_mv, (int) iv[k].idda_ma, (int) iv[k].vddd_mv, (int) iv[k].iddd_ma);
  }
}

// *** Checks ***

static const char *   BOARD_RAIL[3]       = {"3V6", "3V3", "3V0"};
static const unsigned BOARD_NOMINAL_MV[3] = {3600, 3300, 3000};
#define BOARD_TOLERANCE_PCT 5

// tile rails (first limits, from the PACMAN 1v5 bench 2026-10-16; no ASICs):
#define TILE_OFF_MAX_MV       50   // a rail switched off, or another tile's rail while one tile is driven
#define TILE_SWING_MIN_MV   1000   // rise of a rail over the DAC sweep
#define TILE_MONOTONIC_MV     30   // largest allowed drop from one sweep point to the next
#define TILE_LOAD_MIN_MA       5   // current at the top of the sweep:  a load is present
#define TILE_GAIN_MV         157   // rail gain per 0x1000 DAC codes, above the regulator floor (0x3000-0xF000)
#define TILE_GAIN_PCT          5   // tolerance on the gain
#define TILE_GAIN_FIRST        3   // first sweep point of the gain fit (0x3000)

static bool within_pct(int v, int ref, int pct){
  return (100*v >= ref*(100-pct)) && (100*v <= ref*(100+pct));
}

static void set_all_tile_dacs(hw_val_t dn){
  for (int t = 0; t < 10; t++){
    iic_set_vdda_dn(t, dn);
    iic_set_vddd_dn(t, dn);
  }
}

// largest rail voltage over the tiles, except tile skip (-1: none); *which = its tile
static unsigned max_tile_mv(int skip, int * which){
  unsigned vmax = 0;
  *which = -1;
  for (int t = 0; t < 10; t++){
    if (t == skip) continue;
    unsigned a = iic_mon_vdda_mv(t), d = iic_mon_vddd_mv(t);
    if (a > vmax) { vmax = a; *which = t; }
    if (d > vmax) { vmax = d; *which = t; }
  }
  return vmax;
}

// one rail of a swept tile:  rises steadily over the sweep, into a load; load resistance fitted through 0
static void tile_rail(int t, const char * rail, const unsigned mv[], const unsigned ma[]){
  char name[32];
  unsigned dip = 0;
  double svv = 0, svi = 0;
  for (int k = 0; k < IIC_IV_POINTS; k++){
    if (k > 0 && mv[k] + TILE_MONOTONIC_MV < mv[k-1] && mv[k-1] - mv[k] > dip) dip = mv[k-1] - mv[k];
    svv += (double) mv[k] * mv[k];
    svi += (double) mv[k] * ma[k];
  }
  unsigned swing = (mv[IIC_IV_POINTS-1] > mv[0]) ? mv[IIC_IV_POINTS-1] - mv[0] : 0;
  unsigned top_ma = ma[IIC_IV_POINTS-1];
  int gain = ((int) mv[IIC_IV_POINTS-1] - (int) mv[TILE_GAIN_FIRST]) / (IIC_IV_POINTS-1 - TILE_GAIN_FIRST);
  bool ok = (swing >= TILE_SWING_MIN_MV) && (dip == 0) && (top_ma >= TILE_LOAD_MIN_MA)
         && within_pct(gain, TILE_GAIN_MV, TILE_GAIN_PCT);
  char load[32] = "no load";
  if (svi > 0) snprintf(load, sizeof(load), "load %.1f ohm", svv / svi);
  snprintf(name, sizeof(name), "tile_%02d_%s", t+1, rail);
  check_result(ok ? "PASS" : "FAIL", name, "%u-%u mV, gain %d mV/0x1000, %u mA at top, %s%s", mv[0],
               mv[IIC_IV_POINTS-1], gain, top_ma, load, dip ? ", NOT MONOTONIC" : "");
}

void iic_check_power(hw_u32_t tile_mask){
  char name[32];
  unsigned monitor_mv[3];

  global_status_t gs;
  get_global_status(&gs);
  hw_val_t enables = gs.enables;

  // everything off:  board rails, and every tile rail near zero
  set_global_enables(0);
  set_all_tile_dacs(0);
  usleep(100000);

  for (int i = 0; i < 3; i++)
    monitor_mv[i] = iic_mon_vboard_mv(i);
  if (iic_platform_status() != 0){
    check_result("FAIL", "power", "no response from the power monitors (I2C status 0x%x)",
                 (unsigned) iic_platform_status());
    set_global_enables(enables);
    return;
  }
  for (int i = 0; i < 2; i++){
    snprintf(name, sizeof(name), "board_%s", BOARD_RAIL[i]);
    check_result(within_pct(monitor_mv[i], BOARD_NOMINAL_MV[i], BOARD_TOLERANCE_PCT) ? "PASS" : "FAIL", name,
                 "%u mV (nominal %u +/- %d%%)", monitor_mv[i], BOARD_NOMINAL_MV[i], BOARD_TOLERANCE_PCT);
  }
  check_result((monitor_mv[2] <= TILE_OFF_MAX_MV) ? "PASS" : "FAIL", "board_3V0_off",
               "%u mV with tile power off (limit %u)", monitor_mv[2], (unsigned) TILE_OFF_MAX_MV);
  int which;
  unsigned off_max = max_tile_mv(-1, &which);
  check_result((off_max <= TILE_OFF_MAX_MV) ? "PASS" : "FAIL", "tiles_off", "max %u mV, all tiles (limit %u)",
               off_max, (unsigned) TILE_OFF_MAX_MV);

  // main tile power:  3V0 comes up
  set_global_enables(GLOBAL_ENABLE_TILE_POWER);
  usleep(100000);
  unsigned v3v0 = iic_mon_vboard_mv(2);
  check_result(within_pct(v3v0, BOARD_NOMINAL_MV[2], BOARD_TOLERANCE_PCT) ? "PASS" : "FAIL", "board_3V0",
               "%u mV (nominal %u +/- %d%%)", v3v0, BOARD_NOMINAL_MV[2], BOARD_TOLERANCE_PCT);

  // one tile at a time:  main power + that tile's enable, both DACs swept over the range (iic_get_iv_curve);
  // then the tile held at the top while every other tile's rails are read
  for (int t = 0; t < 10; t++){
    snprintf(name, sizeof(name), "tile_%02d_power", t+1);
    if (!((tile_mask >> t) & 1)){
      check_result("SKIP", name, "not in the tile mask 0x%03x", (unsigned) tile_mask);
      continue;
    }
    set_global_enables(GLOBAL_ENABLE_TILE_POWER | (1u << t));
    usleep(100000);
    iic_iv_point_t iv[IIC_IV_POINTS];
    iic_get_iv_curve(t, iv);
    unsigned amv[IIC_IV_POINTS], ama[IIC_IV_POINTS], dmv[IIC_IV_POINTS], dma[IIC_IV_POINTS];
    for (int k = 0; k < IIC_IV_POINTS; k++){
      amv[k] = iv[k].vdda_mv; ama[k] = iv[k].idda_ma;
      dmv[k] = iv[k].vddd_mv; dma[k] = iv[k].iddd_ma;
      printf("DATA  tile_%02d dac 0x%04x  vdda %5u mV %5u mA  vddd %5u mV %5u mA\r\n", t+1,
             (unsigned) iv[k].dac_dn, amv[k], ama[k], dmv[k], dma[k]);
    }
    iic_set_vdda_dn(t, iv[IIC_IV_POINTS-1].dac_dn);
    iic_set_vddd_dn(t, iv[IIC_IV_POINTS-1].dac_dn);
    usleep(10000);
    unsigned other = max_tile_mv(t, &which);
    iic_set_vdda_dn(t, 0);
    iic_set_vddd_dn(t, 0);
    if (iic_platform_status() != 0){
      check_result("FAIL", name, "I2C error during the sweep (status 0x%x)", (unsigned) iic_platform_status());
      continue;
    }
    tile_rail(t, "vdda", amv, ama);
    tile_rail(t, "vddd", dmv, dma);
    snprintf(name, sizeof(name), "tile_%02d_isolation", t+1);
    char where[24] = "";
    if (which >= 0) snprintf(where, sizeof(where), " on tile %d", which + 1);
    check_result((other <= TILE_OFF_MAX_MV) ? "PASS" : "FAIL", name, "other tiles max %u mV%s (limit %u)",
                 other, where, (unsigned) TILE_OFF_MAX_MV);
  }

  // back off, enables as found
  set_all_tile_dacs(0);
  set_global_enables(enables);
}

void iic_check_sma(bool pause){
  if (iic_set_mux_loop(IIC_MUX_OPEN, IIC_MUX_LOOP_FRONT_PANEL_R28) || iic_platform_status()){
    check_result("FAIL", "sma_r28", "MUX refused or I2C error (status 0x%x)",
                 (unsigned) iic_platform_status());
    iic_set_mux_open();
    return;
  }
  if (pause)
    check_pause("R28 is across the SMA jacks J1001 (P) and J14 (N):  100 ohm +/- 10% on a DMM.");
  iic_set_mux_open();
  if (iic_platform_status())
    check_result("FAIL", "sma_r28", "I2C error (status 0x%x)", (unsigned) iic_platform_status());
  else
    check_result("INFO", "sma_r28", pause ? "R28 set, then MUX open"
                                          : "R28 set and MUX open (no pause)");
}

void iic_menu(){
  printf("I2C menu:  \r\n");
  while(1){
    printf("choose an option:\r\n");
    printf("(x) Exit I2C menu (s) I2C status (r) reset I2C (v) toggle I2C HW version (c) check I2C (p) toggle power (m) monitor power \r\n");
    printf("(k) check power (a) check SMA \r\n");
    char input = input_choice();
    printf("INFO: selected %c\r\n", input);

    switch(input){
    case 'x':
      return;
    case 's':
      iic_show_status();
      break;
    case 'r':
      iic_reset_bus();
      break;
    case 'k':
      iic_check_power(0x3FF);
      break;
    case 'a':
      iic_check_sma(true);
      break;
    case 'v':
      iic_toggle_hardware_version();
      break;
    case 'p':
      iic_toggle_power();
      break;
    case 'm':
      iic_monitor_power();
      break;
    default:
      printf("invalid selection...\r\n");
    }
  }

}
