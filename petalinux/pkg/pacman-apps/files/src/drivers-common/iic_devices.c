// iic_devices.c
//
// I2C devices (see iic_devices.h)
//

#include <assert.h>
#include "hw_access.h"
#include "iic_devices.h"

#define AD5677_BASE_REG   0x30
#define AD5677_NUM_CHAN   16

#define PAC1944_SINGLE_SHOT_CFG 0x85
#define PAC1944_REG_VOLT_BASE   0x07  // VBUS channels 1-4
#define PAC1944_REG_CURR_BASE   0x0B  // VSENSE channels 1-4

void ad5677_set_voltage(hw_u8_t addr, hw_u8_t chan, hw_val_t val) {
  assert(chan < AD5677_NUM_CHAN); // 16-channel DAC:  channels 0-15

  hw_u8_t reg = AD5677_BASE_REG + chan;
  hw_u8_t buf[2];
  buf[0] = (val >> 8) & 0xFF;
  buf[1] = val & 0xFF;

  iic_write(addr, reg, buf, 2);
}

static hw_val_t pac1944_adc_dn(hw_u8_t addr, hw_u8_t reg){
  hw_u8_t buf[2];

  // Configure single-shot mode
  hw_u8_t cfg[2] = { PAC1944_SINGLE_SHOT_CFG, 0x00 };
  iic_write(addr, 1, cfg, 2);

  // Refresh twice
  iic_write(addr, 0, NULL, 0);
  usleep(5000);
  iic_write(addr, 0, NULL, 0);
  usleep(5000);

  // Read the output register
  iic_read(addr, reg, buf, 2, false);

  uint32_t val = (buf[0] << 8) | buf[1];
  return val;
}

hw_val_t pac1944_v_dn(hw_u8_t addr, hw_val_t chan){
  return (chan < 4) ? pac1944_adc_dn(addr, PAC1944_REG_VOLT_BASE + chan) : 0;
}

hw_val_t pac1944_i_dn(hw_u8_t addr, hw_val_t chan){
  return (chan < 4) ? pac1944_adc_dn(addr, PAC1944_REG_CURR_BASE + chan) : 0;
}

#define MAX14661_REG_DIR0   0x00  // switch state:  DIR0-1 COMA, DIR2-3 COMB
#define MAX14661_REG_SHDW0  0x10  // shadow:  same layout
#define MAX14661_REG_CMD_A  0x14  // CMD_B follows

void max14661_cmd(hw_u8_t addr, hw_u8_t cmd_a, hw_u8_t cmd_b){
  iic_write(addr, MAX14661_REG_CMD_A,     &cmd_a, 1);
  iic_write(addr, MAX14661_REG_CMD_A + 1, &cmd_b, 1);
}

void max14661_write_shadow(hw_u8_t addr, hw_val_t common, hw_val_t mask){
  hw_u8_t lo = mask & 0xFF, hi = (mask >> 8) & 0xFF;
  iic_write(addr, MAX14661_REG_SHDW0 + 2*common,     &lo, 1);
  iic_write(addr, MAX14661_REG_SHDW0 + 2*common + 1, &hi, 1);
}

hw_val_t max14661_read(hw_u8_t addr, hw_val_t common){
  hw_u8_t lo, hi;
  iic_read(addr, MAX14661_REG_DIR0 + 2*common,     &lo, 1, false);
  iic_read(addr, MAX14661_REG_DIR0 + 2*common + 1, &hi, 1, false);
  return ((hw_val_t) hi << 8) | lo;
}

void max14661_write_reg(hw_u8_t addr, hw_u8_t reg, hw_u8_t value) {
  iic_write(addr, reg, &value, 1);
}

#define INA220_REG_I       0x1        // Shunt Voltage Register
#define INA220_REG_V       0x2        // BUS Voltage Register


hw_val_t ina220_v_adc_dn(hw_u8_t addr){
  hw_u8_t buf[2];

  iic_read(addr, INA220_REG_V, buf, 2, true);

  hw_val_t reg = (((hw_val_t) buf[0]) << 8) | buf[1];
  hw_val_t dn = (reg >> 3); // D0 is at bit 3

  return dn;
}

hw_val_t ina220_i_adc_dn(hw_u8_t addr){
  hw_u8_t buf[2];

  iic_read(addr, INA220_REG_I, buf, 2, true);

  hw_val_t reg = (((hw_val_t) buf[0]) << 8) | buf[1];
  hw_val_t dn  = reg;

  return dn;
}


#define ADS1219_REG_RESET      0x06
#define ADS1219_REG_CFG        0x40
#define ADS1219_REG_START      0x08
#define ADS1219_REG_READY      0x24
#define ADS1219_REG_RDATA      0x10
#define ADS1219_MASK_READY     0x80
#define ADS1219_SHIFT_MUX      5

hw_val_t ads1219_adc_dn(hw_u8_t addr, hw_u8_t mux){
  int ready = 0;
  hw_u8_t buf[4];

  if (mux > 0x7)
    return 0;
  hw_u8_t cfg = (mux << ADS1219_SHIFT_MUX);

  //printf("INFO:  ads1219_adc_dn:  addr: 0x%x cfg: 0x%x \r\n", addr, cfg);
  iic_write(addr, ADS1219_REG_RESET, NULL, 0);
  buf[0] = cfg;
  iic_write(addr, ADS1219_REG_CFG, buf, 1);
  iic_write(addr, ADS1219_REG_START, NULL, 0);
  for (int i = 0; i < 100 && !iic_platform_status(); i++){
    iic_read(addr, ADS1219_REG_READY, buf, 1, true);
    if (buf[0] & ADS1219_MASK_READY) {
      ready = 1;
      break;
    }
    usleep(1000);
  }
  if (!ready) {
    if (!iic_platform_status())
      printf("ERROR: timeout waiting on ADS1219 ready bit (addr 0x%x)\r\n", addr);
    return 0;
  }

  iic_read(addr, ADS1219_REG_RDATA, buf, 3, true);

  uint32_t val = (buf[0] << 16) | (buf[1] << 8) | buf[2];
  return val;
}



