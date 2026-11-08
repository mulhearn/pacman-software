#ifndef ADC_MENU_H
#define ADC_MENU_H

#include "hw_access.h"
#include "adc.h"

#define ADC_SAW_PERIOD       1000   // clocks between samples (10 us)
#define ADC_SAW_DN_PER_MV    2.048  // nominal:  2 V ADC range, 12 bits
#define ADC_SAW_GAIN_PCT     5      // calibration tolerance (%)
#define ADC_SAW_RESIDUAL_DN  50     // largest fit residual (DN), DAC 0 excluded

#ifdef __cplusplus
extern "C" {
#endif

// Menu interface to the ADC driver:
void adc_menu();

// Menu options:

// Check the ADC / digital scope feature using the test DAC
void adc_check();

// Cycle the ADC setup:
//   none:            nothing configured (the ADC registers as they are)
//   test DAC:        MUX test DAC -> ADC (front panel open), a sample every ADC_SAW_PERIOD clocks,
//                    trigger on the comparator's falling edge at mid-range, trigger at the buffer's
//                    centre
//   analog monitor:  MUX tile -> ADC (no termination), every clock, forced trigger
void toggle_adc_setup();

// Cycle the tile of the analog monitor setup (1-10).
void toggle_adc_tile();

// Arm the capture with the current setup (returns at once).
void adc_arm();

// Trigger now.
void adc_force();

// Capture status (armed, triggered, done), trigger position and timestamp;  when done:  minimum,
// maximum and mean ADC (DN), overflow count.
void adc_status();

// Check each tile's CLK on its analog monitor (tiles in tile_mask, one at a time;  the ATC test
// card):  into the ADC (100 ohm), then through the front panel and the loop resistors.
void adc_check_ana_mon(hw_u32_t tile_mask);

#ifdef __cplusplus
}
#endif

#endif // ADC_MENU_H
