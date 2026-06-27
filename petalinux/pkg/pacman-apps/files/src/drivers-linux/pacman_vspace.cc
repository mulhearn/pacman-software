#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "addr_conf.hh"
#include "pacman.hh"

#include "hw_access.h"
#include "rxtx.h"
#include "iic.h"

//# set voltage dacs  VDDD first
//c.io.set_reg(0x00200020+(PACMAN_TILE-1), VDDD_DAC[PACMAN_TILE], io_group)
//c.io.set_reg(0x00200010+(PACMAN_TILE-1), VDDA_DAC[PACMAN_TILE], io_group)

//vdda=io.get_reg(0x00200030+(i-1), io_group=io_group)
//vddd=io.get_reg(0x00200040+(i-1), io_group=io_group)
//idda=io.get_reg(0x00200050+(i-1), io_group=io_group)
//iddd=io.get_reg(0x00200060+(i-1), io_group=io_group)

void pacman_write(uint32_t addr, uint32_t value){
  unsigned tmp, off;
  printf("DEBUG: pacman write: addr 0x%x value 0x%x \r\n", addr, value);

  // General purpose AXI-Lite hardware registers:
  if (addr < PACMAN_MAX_OFFSET){
    printf("DEBUG: pacman write: non-virtual reg write at offset 0x%x value 0x%x \r\n", addr, value);
    axil_write_register(addr, value);
    return;
  }

  // Virtual registers:
  if ((addr >= PACMAN_VSPACE_REG_START) && (addr <= PACMAN_VSPACE_REG_START+PACMAN_MAX_OFFSET)){
    off = addr - PACMAN_VSPACE_REG_START;
    printf("DEBUG: pacman write: virtual reg write at offset 0x%x value 0x%x \r\n", off, value);

    // ALL CHANNELS ARE ZERO REFERENCED.

    // 0x00100100:  READ_GLOBAL_TILE_POWER      (RO)

    // 0x00100110:  DISABLE_GLOBAL_TILE_POWER   DC/0 (WO)
    // 0x00100114:  ENABLE_GLOBAL_TILE_POWER    DC/0 (WO)

    // 0x00100200:  READ_TILE_ENABLES           (RO)

    // 0x00100210:  DISABLE_SINGLE_TILE <TILE>  (WO)
    // 0x00100214:  ENABLE_SINGLE_TILE  <TILE>  (WO)

    // 0x001002F0:  DISABLE_ALL_TILE    DC/0    (WO)
    // 0x001002F4:  ENABLE_ALL_TILE     DC/0    (WO)

    // 0x00100300:  READ_RX_ENABLES_LOWER    (RO)
    // 0x00100304:  READ_RX_ENABLES_UPPER    (RO)

    // 0x00100310:  DISABLE_SINGLE_UART_RX  <UART>   (WO)
    // 0x00100314:  ENABLE_SINGLE_UART_RX   <UART>   (WO)

    // 0x001003F0:  DISABLE_ALL_UART_RX     DC/0     (WO)
    // 0x001003F4:  ENABLE_ALL_UART_RX      DC/0     (WO)

    // 0x00100410:  SEND_FULL_RESET      <MASK>
    // 0x00100420:  SEND_INTERNAL_RESET  <MASK>

    switch(off){

    case 0x0110:
      // DISABLE_GLOBAL_TILE_POWER
      tmp  = axil_read_register(0xF010);
      tmp &= ~0x00010000;
      axil_write_register(0xF010, tmp);
      return;
    case 0x0114:
      // ENABLE_GLOBAL_TILE_POWER
      tmp = axil_read_register(0xF010);
      tmp |= 0x00010000;
      axil_write_register(0xF010, tmp);
      return;
    case 0x0210:
      // DISABLE_SINGLE_TILE
      if (value >= 10)
	return;
      tmp = axil_read_register(0xF010);
      tmp &= ~(1<<value);
      axil_write_register(0xF010, tmp);
      return;
    case 0x0214:
      // ENABLE_SINGLE_TILE
      if (value >= 10)
	return;
      tmp = axil_read_register(0xF010);
      tmp |= (1<<value);
      axil_write_register(0xF010, tmp);
      return;
    case 0x02F0:
      // DISABLE_ALL_TILE
      tmp = axil_read_register(0xF010);
      tmp &= ~0x3FF;
      axil_write_register(0xF010, tmp);
      return;
    case 0x02F4:
      // ENABLE_ALL_TILE
      tmp = axil_read_register(0xF010);
      tmp |= 0x3FF;
      axil_write_register(0xF010, tmp);
      return;
    case 0x0310:
      // Disable single UART channel
      if (value >= 40)
	return;
      rx_disable_uart(value);
      return;
    case 0x0314:
      // Enable single UART channels
      if (value >= 40)
	return;
      rx_enable_uart(value);
      return;
    case 0x03F0:
      // Disable all UART channels
      for (uint32_t i = 0; i < 40; ++i) {
	rx_disable_uart(i);
      }
      return;
    case 0x03F4:
      // Enable all UART channels
      for (uint32_t i = 0; i < 40; ++i) {
	rx_enable_uart(i);
      }
      return;
    case 0x0410:
      // this is a request to send a internal reset to tiles in mask:
      // poke C is configured for internal reset
      axil_write_register(0xE0C0, value);
      return;
    case 0x0420:
      // this is a request to send a full reset to tiles in mask:
      // poke D is configured for full reset
      axil_write_register(0xE0D0, value);
      return;
      //Legacy interface:
    case 0x0010: // 0x00XX
      tmp = axil_read_register(0xF010);
      tmp &= 0xFFFF0000;
      tmp |= (value & 0x03FF);
      axil_write_register(0xF010, tmp);
      return;
    case 0x0014:
      tmp = axil_read_register(0xF010);
      tmp &= 0xFFF0FFFF;
      if (value&0x1)
	tmp |= 0x00010000;
      axil_write_register(0xF010, tmp);
      return;
    case 0x1010: // 0x10XX
      // this is a request to send a reset pulse:
      if ((value&0x4)!=0){
	// use Poke C register (mapped to G output) and enable all tiles
	axil_write_register(0xE0C0, 0x3FF);
      }
      return;
    case 0x1014:
      // this is a request to set the pulse length of the reset signal
      // Configure POKE C stimulus for G output, all ten tiles enabled, provided (12-bit) pulse length
      tmp = 0x03FF0001 | ((value & 0xFFF)<<4);
      axil_write_register(0xE118, tmp);
      return;
    case 0x201C:
      // RX enables for UARTS 1-32
      for (uint32_t i = 0; i < 32; ++i) {
	if (value & (1u << i))
	  rx_disable_uart(i);
	else
	  rx_enable_uart(i);
      }
      return;
    case 0x2020:
      // RX enables for UARTS 33-40
      for (uint32_t i = 0; i < 8; ++i) {
	if (value & (1u << i))
	  rx_disable_uart(32+i);
	else
	  rx_enable_uart(32+i);
      }
      return;
    }
    return;
  }

  // I2C virtual registers:
  if ((addr >= PACMAN_VSPACE_I2C_START) && (addr <= PACMAN_VSPACE_I2C_START + PACMAN_MAX_OFFSET)){
    off = addr - PACMAN_VSPACE_I2C_START;
    printf("DEBUG: pacman write: I2C virtual reg write at offset 0x%x value 0x%x \r\n", off, value);
    uint32_t upper = 0xFF0 & off;
    uint32_t lower = 0x00F & off;
    (void) lower;

    switch (upper){
    case I2C_VREG_OFFSET_SET_VDDA:
      iic_set_vdda_dn(lower, value);
      return;
    case I2C_VREG_OFFSET_SET_VDDD:
      iic_set_vddd_dn(lower, value);
      return;
    case I2C_VREG_OFFSET_SET_MUX_FRONT_PANEL:
      iic_set_mux_front_panel(value);
      return;
    case I2C_VREG_OFFSET_SET_MUX_ADC:
      iic_set_mux_adc(value);
      return;
    default:
      break;
    }
    return;
  }

  // Timing system AXI-Lite hardware registers:
  if ((addr >= PACMAN_VSPACE_TIMING_START) && (addr <= PACMAN_VSPACE_TIMING_START + PACMAN_MAX_OFFSET)){
    off = addr - PACMAN_VSPACE_TIMING_START;
    printf("DEBUG: pacman write: timging system register write at offset 0x%x value 0x%x \r\n", off, value);
    timing_write_register(off, value);
    return;
  }

  // For now we silently ignore anything not explicitly handled above...

}

uint32_t pacman_read(uint32_t addr){
  unsigned off;

  printf("DEBUG: pacman read addr 0x%x\r\n",addr);

  // General purpose AXI-Lite hardware registers:
  if (addr < PACMAN_MAX_OFFSET){
    printf("DEBUG: pacman read: non-virtual reg read at address 0x%x \r\n", addr);
    return axil_read_register(addr);
  }

  // I2C virtual registers:
  if ((addr >= PACMAN_VSPACE_I2C_START) && (addr <= PACMAN_VSPACE_I2C_START + PACMAN_MAX_OFFSET)){
    off = addr - PACMAN_VSPACE_I2C_START;
    printf("DEBUG: pacman read: I2C virtual reg read at offset 0x%x \r\n", off);

    uint32_t upper = 0xFF0 & off;
    uint32_t lower = 0x00F & off;
    (void) lower;

    switch (upper){
    case I2C_VREG_OFFSET_MON_VDDA:
      return iic_mon_vdda_mv(lower);
    case I2C_VREG_OFFSET_MON_VDDD:
      return iic_mon_vddd_mv(lower);
    case I2C_VREG_OFFSET_MON_IDDA:
      return iic_mon_idda_ma(lower);
    case I2C_VREG_OFFSET_MON_IDDD:
      return iic_mon_iddd_ma(lower);
    default:
      break;
    }
    return 0;
  }

  // Timing system AXI-Lite hardware registers:
  if ((addr >= PACMAN_VSPACE_TIMING_START) && (addr <= PACMAN_VSPACE_TIMING_START + PACMAN_MAX_OFFSET)){
    off = addr - PACMAN_VSPACE_TIMING_START;
    printf("DEBUG: pacman read: timing system register read at offset 0x%x \r\n", off);
    return timing_read_register(off);
  }



  return 0;

}
