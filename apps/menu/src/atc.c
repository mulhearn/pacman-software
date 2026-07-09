#include <stdlib.h>

#include "hw_access.h"
#include "atc.h"


void set_atc_default_config(){

  // uart and baud periods both 10 clock cycles:
  axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_CONFIG_UART,  0xA);
  axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_CONFIG_BAUD,  0xA);
  axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_CONFIG_INPUT, 0x0);
  // invert all:
  axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_CONFIG_G,     0x3FF00);
  axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_CONFIG_H,     0x3FF00);

  //destination configurations:

  //LEMO A destination configuration:  This is a SYNC pulse, H+T, duration 5
  axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_DST_LEMO_A, 0x0032FFC6);

  //LEMO B destination configuration:  This is a SYNC pulse, H+T, duration 5
  axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_DST_LEMO_B, 0x0032FFC6);

  //POKE A destination configuration:  This is an INTERNAL_RESET pulse, G, duration 24
  axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_DST_POKE_A, 0x00F0FFC1);

  //POKE B destination configuration:  This is a FULL_RESET pulse, G, duration 1024
  axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_DST_POKE_B, 0x2800FFC1);

  //POKE C destination configuration:  This is a SYNC pulse, H+T, duration 5
  axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_DST_POKE_C, 0x0032FFC6);

  //POKE D destination configuration:  This is a SYNC pulse, H+T, duration 5
  axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_DST_POKE_D, 0x0032FFC6);

}

void read_atc_registers(){
  printf("timestamp-------------------0x%x \r\n", (unsigned int) axil_read_register(C_SCOPE_ATC + C_ADDR_ATC_TIMESTAMP));
  printf("uart period-----------------0x%x \r\n", (unsigned int) axil_read_register(C_SCOPE_ATC + C_ADDR_ATC_CONFIG_UART));
  printf("baud period-----------------0x%x \r\n", (unsigned int) axil_read_register(C_SCOPE_ATC + C_ADDR_ATC_CONFIG_BAUD));
  printf("input config----------------0x%x \r\n", (unsigned int) axil_read_register(C_SCOPE_ATC + C_ADDR_ATC_CONFIG_INPUT));
  printf("G output config-------------0x%x \r\n", (unsigned int) axil_read_register(C_SCOPE_ATC + C_ADDR_ATC_CONFIG_G));
  printf("H output config-------------0x%x \r\n", (unsigned int) axil_read_register(C_SCOPE_ATC + C_ADDR_ATC_CONFIG_H));
  
  printf("destination LEMO A----------0x%x \r\n", (unsigned int) axil_read_register(C_SCOPE_ATC + C_ADDR_ATC_DST_LEMO_A));
  printf("destination LEMO B----------0x%x \r\n", (unsigned int) axil_read_register(C_SCOPE_ATC + C_ADDR_ATC_DST_LEMO_B));
  printf("destination poke A----------0x%x \r\n", (unsigned int) axil_read_register(C_SCOPE_ATC + C_ADDR_ATC_DST_POKE_A));
  printf("destination poke B----------0x%x \r\n", (unsigned int) axil_read_register(C_SCOPE_ATC + C_ADDR_ATC_DST_POKE_B));  
  printf("destination poke C----------0x%x \r\n", (unsigned int) axil_read_register(C_SCOPE_ATC + C_ADDR_ATC_DST_POKE_C));
  printf("destination poke D----------0x%x \r\n", (unsigned int) axil_read_register(C_SCOPE_ATC + C_ADDR_ATC_DST_POKE_D));
  printf("destination logic A---------0x%x \r\n", (unsigned int) axil_read_register(C_SCOPE_ATC + C_ADDR_ATC_DST_LOGIC_A));
  printf("destination logic B---------0x%x \r\n", (unsigned int) axil_read_register(C_SCOPE_ATC + C_ADDR_ATC_DST_LOGIC_B));
}

void read_atc_counts(){
  unsigned int integral, count;

  axil_write_register(C_SCOPE_ATC+C_ADDR_ATC_COUNT_REQ, 0x70);    
  count = axil_read_register(C_SCOPE_ATC+C_ADDR_ATC_COUNT);
  integral = (count >> 16) & 0xFFFF;
  count = count & 0xFFFF;  
  printf("LEMO A------count: %4d (0x%04x) integral: %4d (0x%04x) \r\n", count, count, integral, integral);

  axil_write_register(C_SCOPE_ATC+C_ADDR_ATC_COUNT_REQ, 0x71);    
  count = axil_read_register(C_SCOPE_ATC+C_ADDR_ATC_COUNT);
  integral = (count >> 16) & 0xFFFF;
  count = count & 0xFFFF;  
  printf("LEMO B------count: %4d (0x%04x) integral: %4d (0x%04x) \r\n", count, count, integral, integral);

  axil_write_register(C_SCOPE_ATC+C_ADDR_ATC_COUNT_REQ, 0x72);    
  count = axil_read_register(C_SCOPE_ATC+C_ADDR_ATC_COUNT);
  integral = (count >> 16) & 0xFFFF;
  count = count & 0xFFFF;  
  printf("POKE A------count: %4d (0x%04x) integral: %4d (0x%04x) \r\n", count, count, integral, integral);

  axil_write_register(C_SCOPE_ATC+C_ADDR_ATC_COUNT_REQ, 0x73);    
  count = axil_read_register(C_SCOPE_ATC+C_ADDR_ATC_COUNT);
  integral = (count >> 16) & 0xFFFF;
  count = count & 0xFFFF;  
  printf("POKE B------count: %4d (0x%04x) integral: %4d (0x%04x) \r\n", count, count, integral, integral);

  axil_write_register(C_SCOPE_ATC+C_ADDR_ATC_COUNT_REQ, 0x74);    
  count = axil_read_register(C_SCOPE_ATC+C_ADDR_ATC_COUNT);
  integral = (count >> 16) & 0xFFFF;
  count = count & 0xFFFF;  
  printf("POKE C------count: %4d (0x%04x) integral: %4d (0x%04x) \r\n", count, count, integral, integral);

  axil_write_register(C_SCOPE_ATC+C_ADDR_ATC_COUNT_REQ, 0x75);    
  count = axil_read_register(C_SCOPE_ATC+C_ADDR_ATC_COUNT);
  integral = (count >> 16) & 0xFFFF;
  count = count & 0xFFFF;  
  printf("POKE D------count: %4d (0x%04x) integral: %4d (0x%x) \r\n", count, count, integral, integral);
  
  for (int i=0; i<10; i++){
    axil_write_register(C_SCOPE_ATC+C_ADDR_ATC_COUNT_REQ, 0x50 + i);
    count = axil_read_register(C_SCOPE_ATC+C_ADDR_ATC_COUNT);
    integral = (count >> 16) & 0xFFFF;
    count = count & 0xFFFF;  
    printf("OUTPUT G----count: %4d (0x%04x) integral: %4d (0x%04x) \r\n", count, count, integral, integral);
  }

  for (int i=0; i<10; i++){
    axil_write_register(C_SCOPE_ATC+C_ADDR_ATC_COUNT_REQ, 0x60 + i);
    count = axil_read_register(C_SCOPE_ATC+C_ADDR_ATC_COUNT);
    integral = (count >> 16) & 0xFFFF;
    count = count & 0xFFFF;  
    printf("OUTPUT H----count: %4d (0x%04x) integral: %4d (0x%04x) \r\n", count, count, integral, integral);
  }
  
}

void toggle_atc_destinations(){

  static int mode = 0;
  mode = (mode + 1) % 2;
  if (mode == 0) {
    printf("INFO:  setting all destinations to zero (no output) \r\n");
    axil_write_register(C_SCOPE_ATC+C_ADDR_ATC_DST_LEMO_A,  0x0);
    axil_write_register(C_SCOPE_ATC+C_ADDR_ATC_DST_LEMO_B,  0x0);
    axil_write_register(C_SCOPE_ATC+C_ADDR_ATC_DST_POKE_C,  0x0);
    axil_write_register(C_SCOPE_ATC+C_ADDR_ATC_DST_POKE_D,  0x0);
    axil_write_register(C_SCOPE_ATC+C_ADDR_ATC_DST_LOGIC_A, 0x0);
    axil_write_register(C_SCOPE_ATC+C_ADDR_ATC_DST_LOGIC_B, 0x0);
  } else if (mode == 1) {
    printf("INFO:  setting all destinations to default configuration \r\n");
    axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_DST_LEMO_A, 0x0032FFC6);
    axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_DST_LEMO_B, 0x0032FFC6);
    axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_DST_POKE_A, 0x00F0FFC1);
    axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_DST_POKE_B, 0x2080FFC1);
    axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_DST_POKE_C, 0x0032FFC6);
    axil_write_register(C_SCOPE_ATC + C_ADDR_ATC_DST_POKE_D, 0x0032FFC6);
    axil_write_register(C_SCOPE_ATC+C_ADDR_ATC_DST_LOGIC_A, 0x0);
    axil_write_register(C_SCOPE_ATC+C_ADDR_ATC_DST_LOGIC_B, 0x0);
  }
}


void toggle_atc_run(){

  static int mode = 0;
  mode = (mode + 1) % 2;
  if (mode == 0) {
    printf("INFO:  clearing then starting ATC counters \r\n");
    axil_write_register(C_SCOPE_ATC+C_ADDR_ATC_COUNT_REQ, 0x10);
    axil_write_register(C_SCOPE_ATC+C_ADDR_ATC_COUNT_REQ, 0x20);    
  } else if (mode == 1) {
    printf("INFO:  stopping ATC counters \r\n");
    axil_write_register(C_SCOPE_ATC+C_ADDR_ATC_COUNT_REQ, 0x30);
  }
}

void send_poke_a(unsigned mask){
  axil_write_register(C_SCOPE_ATC+C_ADDR_ATC_POKE_A,mask);
}

void send_poke_b(unsigned mask){
  axil_write_register(C_SCOPE_ATC+C_ADDR_ATC_POKE_B,mask);
}


void send_poke_c(unsigned mask){
  axil_write_register(C_SCOPE_ATC+C_ADDR_ATC_POKE_C,mask);
}

void send_poke_d(unsigned mask){
  axil_write_register(C_SCOPE_ATC+C_ADDR_ATC_POKE_D,mask);
}
