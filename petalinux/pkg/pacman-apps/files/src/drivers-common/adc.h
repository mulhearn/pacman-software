#ifndef ADC_H
#define ADC_H

#include <stdint.h>
#include "hw_access.h"
#include "capture.h"

#ifdef __cplusplus
extern "C" {
#endif

// GLOBAL REGISTERS:

#define SCOPE_ADC 0xD000
#define C_ADDR_ADC_STATUS  0x000
#define C_ADDR_ADC_CONFIG  0x004
#define C_ADDR_ADC_LOOK    0x010

// LOOK:  the ADC data as selected by the mux, bits 11-0 data, bit 12 overflow (DOF);  STATUS reads ADC_STATUS_VALUE;
// CONFIG bit 0:  ADC_EN (ADC input enabled)
#define ADC_LOOK_DATA_MASK 0xFFF
#define ADC_LOOK_DOF       0x1000
#define ADC_STATUS_VALUE   0x00000ADC
#define ADC_CONFIG_ENABLE  0x1

// Emulator, mux, comparator (firmware:  adc_emul.vhd, adc_mux.vhd, adc_cmp.vhd;  addresses:  register_map.vhd)
#define C_ADDR_ADC_EMUL_MIN     0x080   // RW  bits 11-0:  ramp start and minimum
#define C_ADDR_ADC_EMUL_MAX     0x084   // RW  bits 11-0:  ramp maximum
#define C_ADDR_ADC_EMUL_STEP    0x088   // RW  bits 12-0:  change per step (signed)
#define C_ADDR_ADC_EMUL_PERIOD  0x08C   // RW  bits 15-0:  one step every PERIOD clocks (0:  hold)
#define C_ADDR_ADC_MUX_SELECT   0x090   // RW  bit 0:  0 the ADC pins, 1 the emulator (LOOK, comparator, capture)
#define C_ADDR_ADC_CMP_THRESH   0x0A0   // RW  bits 11-0:  threshold
#define C_ADDR_ADC_CMP_SWING    0x0A4   // RW  bits 11-0:  hysteresis (high at THRESH+SWING, low at THRESH-SWING)
#define C_ADDR_ADC_CMP_LEVEL    0x0A8   // R   bit 0:  comparator level

#define ADC_MUX_PINS      0
#define ADC_MUX_EMULATOR  1

// the capture (capture.h, scope SCOPE_ADC):  samples of 14 bits
#define ADC_CAP_DATA_MASK  0xFFF
#define ADC_CAP_DOF        0x1000
#define ADC_CAP_LEVEL      0x2000
#define ADC_CAP_LEVEL_BIT  13

// the emulator ramp:  from min, step (negative:  ramps down) every period clocks, wrapping within min-max
void adc_set_emulator(hw_u32_t min, hw_u32_t max, int step, hw_u32_t period);
// ADC_MUX_PINS or ADC_MUX_EMULATOR (selecting the emulator restarts its ramp at min)
void adc_select(hw_u32_t source);
void adc_set_comparator(hw_u32_t thresh, hw_u32_t swing);

// ADC driver initialization (the ADC is left asleep):
void adc_init();
// ADC sleep pin:  1 asleep, 0 awake
void adc_set_sleep(hw_val_t sleep);


// menu style hooks to ADC features:
void adc_read_registers();
void adc_toggle_sleep();
void adc_toggle_config();

#ifdef __cplusplus
}
#endif

#endif // ADC_H


