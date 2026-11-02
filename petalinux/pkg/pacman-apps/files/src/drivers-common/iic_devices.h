#ifndef IIC_DEVICES_H
#define IIC_DEVICES_H

// iic_devices.h
//
// Device specific I2C drivers.

#include "hw_access.h"

#ifdef __cplusplus
extern "C" {
#endif

// AD5677 DAC (16 channel)
// addr: I2C address of device
// chan[3,0]: DAC channel
// val: digital value to write
void ad5677_set_voltage(hw_u8_t addr, hw_u8_t chan, hw_val_t val);
// Read back one channel's input register
hw_val_t ad5677_read(hw_u8_t addr, hw_u8_t chan);

// PAC1944 power monitor (4 channel), single shot
// addr: I2C address of device
// chan: 0-3
hw_val_t pac1944_v_dn(hw_u8_t addr, hw_val_t chan);
hw_val_t pac1944_i_dn(hw_u8_t addr, hw_val_t chan);
// Manufacturer ID (register 0xFE)
hw_u8_t  pac1944_id(hw_u8_t addr);

// MAX14661 MUX (16 switches, two commons:  0 = COMA, 1 = COMB;  switch n is bit n-1 of a mask)
#define MAX14661_CMD_OPEN   0x10  // all switches of the common open
#define MAX14661_CMD_COPY   0x11  // copy the common's shadow registers
#define MAX14661_CMD_KEEP   0x12  // no change
// Command to COMA and COMB, CMD_A then CMD_B (applied once both are written).
void max14661_cmd(hw_u8_t addr, hw_u8_t cmd_a, hw_u8_t cmd_b);
// Write the shadow registers of one common.
void max14661_write_shadow(hw_u8_t addr, hw_val_t common, hw_val_t mask);
// Read the switch state of one common, as a mask.
hw_val_t max14661_read(hw_u8_t addr, hw_val_t common);
// Read the shadow registers of one common, as a mask.
hw_val_t max14661_read_shadow(hw_u8_t addr, hw_val_t common);
// raw register write (development only)
void max14661_write_reg(hw_u8_t addr, hw_u8_t reg, hw_u8_t value);

// Legacy devices (not present in latest HW version, but still supported in legacy HW)

// ADS1219 ADC
// addr: I2C address of device
// mux: mux[2:0] selection (see datasheet)
hw_val_t ads1219_adc_dn(hw_u8_t addr, hw_u8_t mux);
// Configuration register (RREG, repeated start)
hw_u8_t  ads1219_config(hw_u8_t addr);

// INA220 ADC
// addr: I2C address of device
hw_val_t ina220_v_adc_dn(hw_u8_t addr);
hw_val_t ina220_i_adc_dn(hw_u8_t addr);
// Configuration register (0x00)
hw_val_t ina220_config(hw_u8_t addr);

#ifdef __cplusplus
}
#endif


#endif // IIC_DEVICES_H
