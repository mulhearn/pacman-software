#include <stdint.h>

#include "hw_access.h"
#include "adc.h"

#define ADC_SLEEP_PIN 0

void adc_init(){
  // the ADC unit uses the AXI-Lite register and GPIO platforms
  axil_platform_init();
  gpio_platform_init();
  // initialize the ADC sleep pin:
  gpio_platform_configure_pin(ADC_SLEEP_PIN, GPIO_DIR_OUTPUT, 1);
  gpio_write_pin(ADC_SLEEP_PIN, 1);
}

void adc_set_sleep(hw_val_t sleep){
  gpio_write_pin(ADC_SLEEP_PIN, sleep ? 1 : 0);
}

void adc_read_registers(){
  printf("ADC status--------------- 0x%x \r\n", (unsigned int) axil_read_register(SCOPE_ADC+C_ADDR_ADC_STATUS));
  printf("ADC config--------------- 0x%x \r\n", (unsigned int) axil_read_register(SCOPE_ADC+C_ADDR_ADC_CONFIG));
  printf("ADC look----------------- 0x%x \r\n", (unsigned int) axil_read_register(SCOPE_ADC+C_ADDR_ADC_LOOK));
}

void adc_toggle_sleep(){
  static int mode = 0;
  mode = (mode + 1) % 2;
  if (mode == 0) {
    printf("INFO: enabling ADC sleep \r\n");
    gpio_write_pin(ADC_SLEEP_PIN, 1);
  } else {
    printf("INFO: waking ADC (by disabling ADC sleep) \r\n");
    gpio_write_pin(ADC_SLEEP_PIN, 0);
  }
}

void adc_toggle_config(){
  static int mode = 0;
  mode = (mode + 1) % 2;
  if (mode == 0) {
    printf("INFO: disabling ADC input (enabling DEBUG pins)\r\n");
    axil_write_register(SCOPE_ADC+C_ADDR_ADC_CONFIG,0x00000000);
  } else {
    printf("INFO: enabling ADC input (disabling DEBUG pins)\r\n");
    axil_write_register(SCOPE_ADC+C_ADDR_ADC_CONFIG,0x00000001);
  }
}


// *** EMULATOR, MUX, COMPARATOR ***

void adc_set_emulator(hw_u32_t min, hw_u32_t max, int step, hw_u32_t period){
  axil_write_register(SCOPE_ADC + C_ADDR_ADC_EMUL_MIN, min & 0xFFF);
  axil_write_register(SCOPE_ADC + C_ADDR_ADC_EMUL_MAX, max & 0xFFF);
  axil_write_register(SCOPE_ADC + C_ADDR_ADC_EMUL_STEP, (hw_u32_t) step & 0x1FFF);
  axil_write_register(SCOPE_ADC + C_ADDR_ADC_EMUL_PERIOD, period & 0xFFFF);
}

void adc_select(hw_u32_t source){
  axil_write_register(SCOPE_ADC + C_ADDR_ADC_MUX_SELECT, source & 0x1);
}

void adc_set_comparator(hw_u32_t thresh, hw_u32_t swing){
  axil_write_register(SCOPE_ADC + C_ADDR_ADC_CMP_THRESH, thresh & 0xFFF);
  axil_write_register(SCOPE_ADC + C_ADDR_ADC_CMP_SWING, swing & 0xFFF);
}
