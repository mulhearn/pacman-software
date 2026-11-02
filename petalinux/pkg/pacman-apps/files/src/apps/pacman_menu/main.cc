#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <sys/time.h>

#include "hw_access.h"
#include "global.h"
#include "dma.h"
#include "led.h"
#include "iic.h"
#include "rxtx.h"
#include "rxtx_menu.h"
#include "atc.h"
#include "adc.h"
#include "asic.h"
#include "jumper.h"

#include "asic_menu.h"
#include "atc_menu.h"
#include "iic_menu.h"
#include "adc_menu.h"
#include "hwcheck_menu.h"

// *** LED ***


// *** GLOBAL UNIT ***


// *** MENUS ***


void main_menu(){
  while(1){
    printf("MAIN MENU:  choose an option:\r\n");
    printf("(x) exit (l) blink LEDs (g) global registers (s) toggle scratch registers \r\n");
    printf("(a) I2C menu (b) RX/TX menu (c) ATC menu (d) ADC menu \r\n");
    printf("(e) ASIC menu (f) hardware check menu \r\n");
    int input = input_choice();
    if (input == INPUT_END)
      return;
    printf("INFO: selected %c\r\n", input);

    switch(input){
    case 'x':
      return;
    case 'l':
      blink_leds();
      break;
    case 'g':
      read_global_status();
      read_jumper_status();
      break;
    case 's':
      toggle_global_scratch();
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
  printf("pacman_menu:  PACMAN Linux driver access via menu, for diagnostics and hardware checkout.\r\n");
  //printf("Random Max:  0x%x Random Number:  0x%x \r\n", RAND_MAX, rand());

  axil_platform_init();
  print_global_version();
  iic_platform_init();
  // I2C for the hardware version the firmware was built for (the I2C menu can override):
  global_status_t gs;
  get_global_status(&gs);
  iic_set_hw_version(gs.hardware_major, gs.hardware_minor, gs.hardware_patch);
  bram_platform_init();
  init_rxtx();
  init_led();
  init_jumper();

  main_menu();
}
