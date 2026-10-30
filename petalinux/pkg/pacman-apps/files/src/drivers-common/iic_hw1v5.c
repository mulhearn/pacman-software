// iic_hw1v5.c
//
// I2C implementation specific to PACMAN 1V5
//

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include "hw_access.h"
#include "iic_devices.h"
#include "iic.h"

// I2C Address Space - PACMAN 1V5
//0001100   AD5677        16-chan. 16-bit DAC for VDDA setup
//0001101   AD5677        16-chan. 16-bit DAC for VDDD setup
//0010000   PAC1944       4-chan. Power Monitor VDDA+VDDD Tile1 + Tile2
//0010001   PAC1944       4-chan. Power Monitor VDDA+VDDD Tile3 + Tile4
//0010010   PAC1944       4-chan. Power Monitor VDDA+VDDD Tile5 + Tile6
//0010011   PAC1944       4-chan. Power Monitor VDDA+VDDD Tile7 + Tile8
//0010100   PAC1944       4-chan. Power Monitor VDDA+VDDD Tile9 + Tile10
//0010101   PAC1944       4-chan. Power Monitor T3V0 + D3V6 + D3V3
//1001100   MAX14661      16:2 Positive-Side MUX
//1001101   MAX14661      16:2 Negative-Side MUX
//1010000   SFP           SFP Module for Timing (primary addr.)
//1010001   SFP           SFP Module for Timing (secondary addr.)
//1100000   ADN2814       Clock & Data Recovery (CDR) for Timing
//

#define MAX_TILE          10

#define ADDR_BAD          0b0001110  // Non-existent address
#define ADDR_DAC_VDDA     0b0001100  // AD5677 DAC for VDDA TILES 1-10
#define ADDR_DAC_VDDD     0b0001101  // AD5677 DAC for VDDD TILES 1-10
#define ADDR_ADC_TILES    0b0010000  // PAC1944 for Tiles 1+2 (ADDR+0), Tiles 3+4 (ADDR+1), ...
#define ADDR_ADC_BOARD    0b0010101  // PAC 1944 for Board Power and Temp
#define ADDR_MUX_P        0b1001100  // MAX14661 for TILES 1-10
#define ADDR_MUX_N        0b1001101  // MAX14661 for TILES 1-10

// Each PAC1944 has four channels, covering TILE N and N+1
// CHAN 0: VDDA N
// CHAN 1: VDDD N
// CHAN 2: VDDA N+1
// CHAN 3: VDDD N+1

#define PAC1944_OFFSET_VDDA     0
#define PAC1944_OFFSET_VDDD     1

#define FULLSCALE_VSENSE_MV          9000   // 9 V full scale for TILE voltage sense
#define FULLSCALE_TILE_MA            5000   // 5 A full scale for TIle current sense (100 mV / 20 mR)

static hw_val_t get_mv(hw_val_t dn){
  return FULLSCALE_VSENSE_MV * dn / 0xFFFF;
}

static hw_val_t get_ma(hw_val_t dn){
  return FULLSCALE_TILE_MA * dn / 0xFFFF;
}

void iic_hw1v5_set_vdda_dn(hw_val_t itile, hw_val_t val) {
  if (itile > MAX_TILE) return; // 10 tile channels, plus the test DAC (VOUT10, SET_VDDA_TEST)

  ad5677_set_voltage(ADDR_DAC_VDDA, itile, val);
}

void iic_hw1v5_set_vddd_dn(hw_val_t itile, hw_val_t val) {
  if (itile > MAX_TILE) return; // 10 tile channels, plus the test DAC (VOUT10, SET_VDDD_TEST)

  ad5677_set_voltage(ADDR_DAC_VDDD, itile, val);
}

void iic_hw1v5_set_vplus_dn( hw_val_t val) {
  ad5677_set_voltage(ADDR_DAC_VDDA, MAX_TILE, val);
}

void iic_hw1v5_set_vminus_dn( hw_val_t val) {
  ad5677_set_voltage(ADDR_DAC_VDDD, MAX_TILE, val);
}

hw_val_t iic_hw1v5_mon_vdda_mv(hw_val_t itile) {
  if (itile >= MAX_TILE) return 0;

  hw_u8_t addr = ADDR_ADC_TILES + (itile/2);
  return get_mv(pac1944_v_dn(addr, 2*(itile%2)+PAC1944_OFFSET_VDDA));
}

hw_val_t iic_hw1v5_mon_vddd_mv(hw_val_t itile) {
  if (itile >= MAX_TILE) return 0;

  hw_u8_t addr = ADDR_ADC_TILES + (itile/2);
  return get_mv(pac1944_v_dn(addr, 2*(itile%2)+PAC1944_OFFSET_VDDD));
}

hw_val_t iic_hw1v5_mon_idda_ma(hw_val_t itile) {
  if (itile >= MAX_TILE) return 0;

  hw_u8_t addr = ADDR_ADC_TILES + (itile/2);
  return get_ma(pac1944_i_dn(addr, 2*(itile%2)+PAC1944_OFFSET_VDDA));
}

hw_val_t iic_hw1v5_mon_iddd_ma(hw_val_t itile) {
  if (itile >= MAX_TILE) return 0;

  hw_u8_t addr = ADDR_ADC_TILES + (itile/2);
  return get_ma(pac1944_i_dn(addr, 2*(itile%2)+PAC1944_OFFSET_VDDD));
}

hw_val_t iic_hw1v5_mon_vboard_mv(hw_val_t chan) {
  if (chan >= 4) return 0; // 4 channels for board power voltage monitoring

  return get_mv(pac1944_v_dn(ADDR_ADC_BOARD, chan));
}

hw_val_t iic_hw1v5_mon_iboard_ma(hw_val_t chan) {
  hw_val_t iscale[] = {20/5,20/5, 50/5};

  if (chan >= 3) return 0; // 3 channels for board power current monitoring

  return iscale[chan] * get_ma(pac1944_i_dn(ADDR_ADC_BOARD, chan));
}

hw_val_t iic_hw1v5_mon_probe_dn() {
  return pac1944_i_dn(ADDR_ADC_BOARD, 3);
}

// *** Front-panel / ADC MUX (two MAX14661:  P side ADDR_MUX_P, N side ADDR_MUX_N) ***
#define MUX_SW_DAC          12
#define MUX_SW_TERM_A       14
#define MUX_SW_TERM_B       15
#define MUX_SW_LOOP1        14   // R29 to switch 14 of the other chip (both on one common:  TERM_A or TERM_B)
#define MUX_SW_LOOP2        15   // R28 to switch 15 of the other chip
#define MUX_SW_GND          16
#define MUX_BIT(sw)         (1u << ((sw) - 1))

static const hw_u8_t MUX_CHIP[2] = { ADDR_MUX_P, ADDR_MUX_N };

// switch mask for (common, source, term), or -1 if that is not an allowed state
static int mux_mask(hw_val_t common, hw_val_t source, hw_val_t term){
  hw_val_t mask = 0;
  if (common > IIC_MUX_ADC) return -1;
  if (source >= IIC_MUX_TILE_1 && source <= IIC_MUX_TILE_10) mask = MUX_BIT(source - IIC_MUX_TILE_1 + 1);
  else if (source == IIC_MUX_DAC)                             mask = MUX_BIT(MUX_SW_DAC);
  else if (source != IIC_MUX_OPEN)                            return -1;
  if (term) mask |= MUX_BIT(common == IIC_MUX_FRONT_PANEL ? MUX_SW_TERM_A : MUX_SW_TERM_B);
  return (int) mask;
}

void iic_hw1v5_mux_open(){
  for (int c = 0; c < 2; c++)
    max14661_cmd(MUX_CHIP[c], MAX14661_CMD_OPEN, MAX14661_CMD_OPEN);
}

int iic_hw1v5_mux_select(hw_val_t common, hw_val_t source, hw_val_t term){
  int m = mux_mask(common, source, term);
  if (m < 0){
    printf("**ERROR** iic_mux_select:  not an allowed state (common %u, source %u, term %u)\n",
           (unsigned) common, (unsigned) source, (unsigned) term);
    return -1;
  }
  hw_val_t mask = (hw_val_t) m;
  // refuse a second termination on one source:  the other common carrying the same source, terminated
  hw_val_t other = 1 - common;
  hw_val_t src = mask & ~MUX_BIT(common == IIC_MUX_FRONT_PANEL ? MUX_SW_TERM_A : MUX_SW_TERM_B);
  hw_val_t other_term = MUX_BIT(other == IIC_MUX_FRONT_PANEL ? MUX_SW_TERM_A : MUX_SW_TERM_B);
  for (int c = 0; c < 2 && term && src; c++){
    hw_val_t o = max14661_read(MUX_CHIP[c], other);
    if ((o & src) && (o & other_term)){
      printf("**ERROR** iic_mux_select:  source %u is terminated on the other common:  refused\n",
             (unsigned) source);
      return -1;
    }
  }
  for (int c = 0; c < 2; c++){
    max14661_write_shadow(MUX_CHIP[c], common, mask);
    // copy this common's shadow, leave the other bank as it is
    max14661_cmd(MUX_CHIP[c], common == 0 ? MAX14661_CMD_COPY : MAX14661_CMD_KEEP,
                              common == 1 ? MAX14661_CMD_COPY : MAX14661_CMD_KEEP);
  }
  return 0;
}

// Masks per pattern [chip P, N][COMA, COMB];  SRC stands for the source's switch.
#define SRC 0x10000u
#define L1  MUX_BIT(MUX_SW_LOOP1)
#define L2  MUX_BIT(MUX_SW_LOOP2)
#define GND MUX_BIT(MUX_SW_GND)
static const hw_val_t MUX_LOOP_MASK[5][2][2] = {
  {{SRC | L1, L2},       {SRC | L2, L1}},         // series
  {{SRC | L2, L1},       {SRC | L1, L2}},         // series, swapped
  {{SRC | L1, L2 | GND}, {0, L1 | L2}},           // divider P
  {{0, L1 | L2},         {SRC | L1, L2 | GND}},   // divider N
  {{L2, 0},              {L2, 0}},                // R28 alone across the front panel
};
#undef L1
#undef L2
#undef GND

int iic_hw1v5_mux_loop(hw_val_t source, hw_val_t pattern){
  hw_val_t sw;
  if (source >= IIC_MUX_TILE_1 && source <= IIC_MUX_TILE_10) sw = MUX_BIT(source - IIC_MUX_TILE_1 + 1);
  else if (source == IIC_MUX_DAC)                             sw = MUX_BIT(MUX_SW_DAC);
  else sw = 0;
  bool no_source = pattern == IIC_MUX_LOOP_FRONT_PANEL_R28;
  if ((no_source ? source != IIC_MUX_OPEN : !sw) || pattern > IIC_MUX_LOOP_FRONT_PANEL_R28){
    printf("**ERROR** iic_set_mux_loop:  not an allowed state (source %u, pattern %u)\n", (unsigned) source, (unsigned) pattern);
    return -1;
  }
  // from all open, either chip alone leaves the far ends of the resistors open:  no intermediate state joins two nodes
  iic_hw1v5_mux_open();
  for (int c = 0; c < 2; c++){
    for (int k = 0; k < 2; k++){
      hw_val_t m = MUX_LOOP_MASK[pattern][c][k];
      max14661_write_shadow(MUX_CHIP[c], k, (m & SRC) ? (m & ~SRC) | sw : m);
    }
    max14661_cmd(MUX_CHIP[c], MAX14661_CMD_COPY, MAX14661_CMD_COPY);
  }
  return 0;
}
#undef SRC

// raw register write to one front-panel MUX chip (side 0: P, U13; side 1: N, U8):  development only, bypasses the
// safe MUX layer;  not reachable from pacman_server.
void iic_hw1v5_write_mux_reg(hw_val_t side, hw_val_t reg, hw_val_t value){
  max14661_write_reg(side ? ADDR_MUX_N : ADDR_MUX_P, reg, value);
}




