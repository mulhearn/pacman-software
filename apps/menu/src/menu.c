#include <stdio.h>
#include <stdlib.h>
#include "xparameters.h"
#include "xtime_l.h"
#include "xil_io.h"
#include "xaxidma.h"
#include "xstatus.h"
#include "xil_printf.h"
#include "sleep.h"
#include "xemacps.h"

#include "hw_access.h"
#include "dma.h"
#include "global.h"
#include "iic.h"
#include "rxtx.h"
#include "rxtx_menu.h"
#include "atc.h"
#include "adc.h"
#include "asic.h"
#include "led.h"

#include "iic_menu.h"
#include "asic_menu.h"
#include "atc_menu.h"
#include "adc_menu.h"
#include "hwcheck_menu.h"

#define EMAC_DEVICE_ID      XPAR_XEMACPS_0_DEVICE_ID
#define PHY_ADDRESS         0x1A    // Your CPLD PHY address

XEmacPs EmacPs;

void mdio_init()
{
    XEmacPs_Config *Config;
    Config = XEmacPs_LookupConfig(EMAC_DEVICE_ID);
    XEmacPs_CfgInitialize(&EmacPs, Config, Config->BaseAddress);
}

void read_mac_from_cpld(){
  xil_printf("INFO: reading MAC address from CPLD \r\n");

  u16 a,b,c,d,e,f;
  long stat = 0;

  stat |= XEmacPs_PhyRead(&EmacPs, PHY_ADDRESS, 0x9, &a);
  stat |= XEmacPs_PhyRead(&EmacPs, PHY_ADDRESS, 0xA, &c);
  stat |= XEmacPs_PhyRead(&EmacPs, PHY_ADDRESS, 0xB, &e);

  if (stat != XST_SUCCESS) {
    return;
  }

  b = a&0xFF;
  a = (a>>8)&0xFF;
  d = c&0xFF;
  c = (c>>8)&0xFF;
  f = e&0xFF;
  e = (e>>8)&0xFF;

  xil_printf("INFO: success reading MAC address from CPLD: %02x:%02x:%02x:%02x:%02x:%02x \r\n",
	     a,b,c,d,e,f);
}

void toggle_cpld(){
  u16 cr;
  long stat = 0;

  stat |= XEmacPs_PhyRead(&EmacPs, PHY_ADDRESS, 0x5, &cr);

  if (stat != XST_SUCCESS) {
    return;
  }
  xil_printf("INFO: success reading CR1 (LEDs) from CPLD: 0x%04x \r\n", cr);


  static int mode = 0;
  mode = (mode + 1) % 5;
  if (mode == 0) {
    xil_printf("Setting CPLD LED configuration to default (RED: slow blink for SD card bood, GREEN: MIO \r\n");
    XEmacPs_PhyWrite(&EmacPs, PHY_ADDRESS, 0x5, 0x00);
  } else if (mode == 1) {
    xil_printf("Setting CPLD LED configuration to RED: MIO:  Green: On \r\n");
    XEmacPs_PhyWrite(&EmacPs, PHY_ADDRESS, 0x5, 0x47);
  } else if (mode == 2) {
    xil_printf("Setting CPLD LED configuration to RED: MIO:  Green: Off \r\n");
    XEmacPs_PhyWrite(&EmacPs, PHY_ADDRESS, 0x5, 0x46);
  } else if (mode == 3) {
    xil_printf("Setting CPLD LED configuration to RED: OFF:  Green: MIO \r\n");
    XEmacPs_PhyWrite(&EmacPs, PHY_ADDRESS, 0x5, 0x64);
  } else if (mode == 4) {
    xil_printf("Setting CPLD LED configuration to RED: MIO:  Green: MIO \r\n");
    XEmacPs_PhyWrite(&EmacPs, PHY_ADDRESS, 0x5, 0x44);
  }
}

void toggle_dcache(){
  static int mode = 0;
  mode = (mode + 1) % 2;

  if (mode == 0) {
    xil_printf("enabling dcache\r\n");
    Xil_DCacheEnable();
  } else {
    xil_printf("disabling dcache\r\n");
    Xil_DCacheDisable();
  }
}


void main_menu(){
  while(1){
    printf("MAIN MENU:  choose an option:\r\n");
    printf("(l) blink LEDs (g) read global status (s) toggle scratch (n) toggle enables \r\n");
    printf("(t) toggle dcache (m) read MAC from CPLD (k) toggle CPLD config \r\n");
    printf("(a) I2C menu (b) RX/TX menu (c) ATC menu (d) ADC menu \r\n");
    printf("(e) single ASIC menu (f) hardware check menu \r\n");
    int input = input_choice();
    if (input == INPUT_END)
      return;
    printf("INFO: selected %c\r\n", input);

    switch(input){
    case 'l':
      blink_leds();
      break;
    case 'g':
      read_global_status();
      break;
    case 's':
      toggle_global_scratch();
      break;
    case 'n':
      toggle_global_enables();
      break;
    case 't':
      toggle_dcache();
      break;
    case 'm':
      read_mac_from_cpld();
      break;
    case 'k':
      toggle_cpld();
      break;
    case 'a':
      iic_menu();
      break;
    case 'b':
      rxtx_menu();
      break;
    case 'c':
      atc_menu();
      break;
    case 'd':
      adc_menu();
      break;
    case 'e':
      asic_menu();
      break;
    case 'f':
      hwcheck_menu();
      break;
    default:
      printf("invalid selection...\r\n");
    }
  }
}

int main(){
  printf("Menu-Driver Demonstration Driver For PACMAN\r\n");
  printf("Sanity number:  1\r\n");
  printf("Random Max:  0x%x Random Number:  0x%x \r\n", RAND_MAX, rand());

  int status = 0;
  axil_platform_init();
  print_global_version();
  iic_platform_init();
  // I2C for the hardware version the firmware was built for (the I2C menu can override):
  global_status_t gs;
  get_global_status(&gs);
  iic_set_hw_version(gs.hardware_major, gs.hardware_minor, gs.hardware_patch);
  gpio_platform_init();

  mdio_init();
  init_rxtx();
  init_led();
  if (status != XST_SUCCESS) {
    printf("Hardware initialization has FAILED.\r\n");
    return 0;
  }

  printf("Setting up DMA circular buffers (n=%d):\r\n", RXTX_RING_BDS);
  init_rxtx_descriptor_ring_mode(RXTX_RING_BDS);

  printf("Setting ATC default configuration:\r\n");
  set_atc_default_config();

  main_menu();
  return 0;
}

