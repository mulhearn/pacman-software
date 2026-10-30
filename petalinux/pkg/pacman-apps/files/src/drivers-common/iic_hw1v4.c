// iic_hw1v4.c
//
// I2C implementation specific to PACMAN 1V4
//

#include <assert.h>
#include "hw_access.h"
#include "iic_devices.h"
#include "iic.h"

// I2C Address Space - PACMAN 1V4
//0001100   AD5677        16-chan. 16-bit DAC for VDDA setup
//1001100   MAX14661      16-chan MUX for TILES 1-8
//1000000   INA220 (x8)   ADC for TILE 1
//1001000   ADS1219 (x4)  ADC for TILES 1+2

#define MAX_TILE          8

#define ADDR_BAD          0b0001110  // Non-existent address
#define ADDR_DAC          0b0001100  // AD5677 DAC for VDDD and VDDA TILES 1-8
#define ADDR_MUX          0b1001100  // MAX14661 for TILES 1-8
#define ADDR_ADC_VDDA     0b1000000  // INA220 for TILE 1
#define ADDR_ADC_VDDD     0b1001000  // ADS1219 for TILES 1+2

// The AD5677 DAC has 16 channels, arranged for PACMAN 1V4 as:
// CHAN 0:   VDDA TILE 1 (itile=0)
// CHAN 1:   VDDD TILE 1 (itile=0)
// CHAN 2:   VDDA TILE 2 (itile=1)
// CHAN 3:   VDDD TILE 2 (itile=1)
// ..
// CHAN 14:  VDDA TILE 8 (itile=7)
// CHAN 15:  VDDD TILE 8 (itile=7)

void iic_hw1v4_set_vdda_dn(hw_val_t itile, hw_val_t val) {
  if (itile >= MAX_TILE) return;

  ad5677_set_voltage(ADDR_DAC, 2*itile, val);
}

void iic_hw1v4_set_vddd_dn(hw_val_t itile, hw_val_t val) {
  if (itile >= MAX_TILE) return;

  ad5677_set_voltage(ADDR_DAC, 2*itile+1, val);
}

hw_val_t iic_hw1v4_mon_vdda_mv(hw_val_t itile) {
  if (itile >= MAX_TILE) return 0;

  hw_u8_t addr = ADDR_ADC_VDDA + itile;

  hw_val_t dn = ina220_v_adc_dn(addr);
  printf("INFO: mon vdda (ina220) dn:  0x%x\n", (unsigned int) dn);

  return 4*dn;
}

hw_val_t iic_hw1v4_mon_idda_ma(hw_val_t itile) {
  if (itile >= MAX_TILE) return 0; // Only 10 tile channels

  hw_u8_t addr = ADDR_ADC_VDDA + itile;

  hw_val_t dn = ina220_i_adc_dn(addr);
  printf("INFO: mon idda (ina220) dn:  0x%x\n", (unsigned int) dn);

  if (dn & 0x8000)
    return 0;
  return 500*dn/1000; // mA from 10uV LSB and 20mR shunt
}

hw_val_t iic_hw1v4_mon_vddd_mv(hw_val_t itile) {
  if (itile >= MAX_TILE) return 0;

  hw_u8_t addr = ADDR_ADC_VDDD + (itile/2);
  hw_u8_t mux  = 4 + 2*(itile%2);

  hw_val_t dn = ads1219_adc_dn(addr, mux);

  if (dn & 0x800000)
    return 0;

  printf("INFO: mon vddd (ads1219) dn:  0x%x\n", (unsigned int) dn);
  return 2*250*(dn>>10)/1000; // mA from 244.14 nV LSB
}

hw_val_t iic_hw1v4_mon_iddd_ma(hw_val_t itile) {
  if (itile >= MAX_TILE) return 0; // Only 10 tile channels

  hw_u8_t addr = ADDR_ADC_VDDD + (itile/2);
  hw_u8_t mux  = 3 + 2*(itile%2);

  hw_val_t dn = ads1219_adc_dn(addr, mux);
  printf("INFO: mon vddd (ads1219) dn:  0x%x\n", (unsigned int) dn);

  if (dn & 0x800000)
    return 0;

  return 250*(dn>>10)/1000; // mA from 244.14 nV LSB
  // And notice that 244.14*1024/1000 = 250

}

// *** Front-panel MUX (one MAX14661, U13) ***
//
// COMA:  ADC_TEST, SMA J1001 "ADC TEST OUTPUT" (out of the PACMAN);
// COMB:  DAC_TEST, SMA J1002 "DAC TEST INPUT" (into the PACMAN).
// Switch 2n-1 is TILEn_ANA_MON, switch 2n TILEn_ADC_TEST (n = 1-8).
// A tile on the front panel is a pair:  its ANA_MON (a tile output) on COMA, its ADC_TEST (a tile
// input) on COMB; nothing else is ever closed (one tile at a time, never one switch on both
// commons).
#define MUX_BIT(sw)         (1u << ((sw) - 1))

void iic_hw1v4_mux_open(){
  max14661_cmd(ADDR_MUX, MAX14661_CMD_OPEN, MAX14661_CMD_OPEN);
}

int iic_hw1v4_mux_select(hw_val_t common, hw_val_t source, hw_val_t term){
  if (common != IIC_MUX_FRONT_PANEL || term
      || !(source == IIC_MUX_OPEN || (source >= IIC_MUX_TILE_1 && source < IIC_MUX_TILE_1 + MAX_TILE))){
    printf("**ERROR** iic_mux_select:  not an allowed state on the 1v4 (common %u, source %u, term %u)\n",
           (unsigned) common, (unsigned) source, (unsigned) term);
    return -1;
  }
  if (source == IIC_MUX_OPEN){
    iic_hw1v4_mux_open();
    return 0;
  }
  hw_val_t n = source - IIC_MUX_TILE_1 + 1;
  max14661_write_shadow(ADDR_MUX, 0, MUX_BIT(2*n - 1));   // COMA:  TILEn_ANA_MON
  max14661_write_shadow(ADDR_MUX, 1, MUX_BIT(2*n));       // COMB:  TILEn_ADC_TEST
  max14661_cmd(ADDR_MUX, MAX14661_CMD_COPY, MAX14661_CMD_COPY);
  return 0;
}
