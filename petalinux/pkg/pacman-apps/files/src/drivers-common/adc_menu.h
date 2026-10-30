#ifndef ADC_MENU_H
#define ADC_MENU_H

#include "hw_access.h"
#include "adc.h"

#ifdef __cplusplus
extern "C" {
#endif

// Check the ADC registers, then the test DAC into the ADC through the MUX (no card):  direct, and
// through both series loops.
void adc_check();

// Check each tile's CLK on its analog monitor (tiles in tile_mask, one at a time;  the ATC test
// card):  into the ADC (100 ohm), then through the front panel and the loop resistors.
void adc_check_ana_mon(hw_u32_t tile_mask);

#ifdef __cplusplus
}
#endif

#endif // ADC_MENU_H
