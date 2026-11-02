#ifndef IIC_H
#define IIC_H

// iic.h
//
// I2C features for setting output voltages, monitoring power, and
// configuring MUX.
//
// This driver supports multiple HW versions, as selected via
// iic_set_hw_version (see below).  Unsupported features in legacy
// hardware generally do nothing and return 0, with no error.
//
// The TILE cards are labeled TILE 1- TILE 10 in PACMAN schematics.
// Here we use itile to denote the zero referenced tile index
// (e.g. TILE 1 has itile=0)
//

#include "hw_access.h"

#ifdef __cplusplus
extern "C" {
#endif

// confirm that I2C is up and running using NO-OPs
void check_iic();

// set the PACMAN HW version to use for I2C
void iic_set_hw_version(hw_val_t major, hw_val_t minor, hw_val_t patch);

// use the simulated board (iic_sim.c) instead of hardware:
void iic_set_simulated();

// report IIC version currently in use:
hw_val_t iic_get_hw_version();

// set VDDA and VDDD of channel <itile> to value <val>
void iic_set_vdda_dn(hw_val_t itile, hw_val_t val);
void iic_set_vddd_dn(hw_val_t itile, hw_val_t val);

// set the differential test DAC:  <vp> on the positive side, <vn> on the negative side
// (PACMAN 1V5:  DAC channel 10 of the VDDA and VDDD DACs; also reachable as itile 10 above)
void iic_set_test_dac_dn(hw_val_t vp, hw_val_t vn);

// Get monitored value of VDDA and VDDD for TILE <chan>+1 in mV
hw_val_t iic_mon_vdda_mv(hw_val_t itile);
hw_val_t iic_mon_vddd_mv(hw_val_t itile);

// Get monitored value of IDDA and IDDD for TILE <chan>+1 in mA
hw_val_t iic_mon_idda_ma(hw_val_t itile);
hw_val_t iic_mon_iddd_ma(hw_val_t itile);

// Gen monitored value of board voltage for chan <chan>
// chan:  0= 3V6, 1=3V3, 2=3V0, 3=3V3 (Probe)
hw_val_t iic_mon_vboard_mv(hw_val_t chan);

// Gen monitored value of board voltage for chan <chan>
// chan:  0= 3V6, 1=3V3, 2=3V0
hw_val_t iic_mon_iboard_ma(hw_val_t chan);

// Get monitored value of votage drop across probe in raw counts:
hw_val_t iic_mon_probe_dn();

// Open every switch on both chips.
void iic_set_mux_open();

// mux sources:
#define IIC_MUX_OPEN         0   // sources:  nothing
#define IIC_MUX_TILE_1       1   //           TILE 1 ... TILE 10 = 1 ... 10 (ANA_TEST)
#define IIC_MUX_TILE_10     10
#define IIC_MUX_DAC         11   //           the test DAC

// MUX commands below:  return 0 when done;  a request that is not allowed is refused, changes
// nothing, and returns non-zero.

// front-panel SMA jacks (COMA) mux configuration by  source and 100 ohm termination option
int iic_set_mux_to_front_panel(hw_val_t source, hw_val_t term);

// onboard ADC (COMB) mux configuration by  source and 100 ohm termination option
int iic_set_mux_to_adc(hw_val_t source, hw_val_t term);

// For loop testing, source (TILE 1-10 or the DAC) on the front panel, through the 100 ohm loop
// resistors (R29, R28) into the ADC (PACMAN 1V5 only); opens everything first.  Pattern
// FRONT_PANEL_R28 requires source IIC_MUX_OPEN.
#define IIC_MUX_LOOP_SERIES       0   // R29 COMA_P-COMB_N, R28 COMA_N-COMB_P:  ADC sees -(P - N)
#define IIC_MUX_LOOP_SERIES_SWAP  1   // R28 COMA_P-COMB_N, R29 COMA_N-COMB_P:  ADC sees -(P - N)
#define IIC_MUX_LOOP_DIVIDER_P    2   // P through R29, R28 to ground:  ADC sees -P/2
#define IIC_MUX_LOOP_DIVIDER_N    3   // N through R29, R28 to ground:  ADC sees +N/2
#define IIC_MUX_LOOP_FRONT_PANEL_R28  4   // R28 alone across front panel (J1001 P, J14 N)
int iic_set_mux_loop(hw_val_t source, hw_val_t pattern);

// commons (board files)
#define IIC_MUX_FRONT_PANEL  0   // COMA
#define IIC_MUX_ADC          1   // COMB
// raw register write to one front-panel MUX chip, side 0: P, side 1: N
// (PACMAN 1V5 MAX14661: 0x10-0x13 shadow switch registers, 0x14 CMDA (COMA), 0x15 CMDB (COMB)).
// (unsafe: allows configurations that could damage the hardware)
void iic_write_mux_reg(hw_val_t side, hw_val_t reg, hw_val_t value);

#ifdef __cplusplus
}
#endif

#endif // IIC_H
