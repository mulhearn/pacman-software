#ifndef addr_conf_hh
#define addr_conf_hh

//
// PACMAN server address space
//

// The pacman-server read/write register interface is arranged at top
// level as follows:

// 0x00000000 - 0x0000FFFF General Purpose AXI-Lite register
// 0x00100000 - 0x0010FFFF Virtual register
// 0x00200000 - 0x0020FFFF Virtual I2C registers
// 0x00300000 - 0x0030FFFF Timing Endpoint AXI-Lite registers

// The virtual registers are conceptual: they need not correspond to
// actual hardware registers.  For example, writing (any value) to
// virtual register 0x001002F4 enables all tiles, by setting hardware
// register at address 0x4000F010 to value 0x000003FF

// The non-virtual registers refer to the actual hardware registers,
// so setting 0x000F010 to 0x000003FF enables all tiles, by setting
// hardware register at address 0x4000F010 to value 0x000003FF

// Virtual Register Space:
#define PACMAN_VSPACE_REG_START    0x00100000
#define PACMAN_VSPACE_I2C_START    0x00200000
#define PACMAN_VSPACE_TIMING_START 0x00300000
#define PACMAN_MAX_OFFSET          0x0000FFFF

// PACMAN General Purpose AXI-Lite interface HW Address:
#define PACMAN_AXIL_ADDR 0x40000000
#define PACMAN_AXIL_HIGH 0x4000FFFF
#define PACMAN_AXIL_LEN  0x00010000

// PACMAN Timing System AXI-Lite interface HW Address:
// The timing system endpoint will use it's own AXIL interface,
// for now (waiting on firmware) this is just a copy of GP AXIL interface.
#define PACMAN_TIMING_ADDR 0x40000000
#define PACMAN_TIMING_HIGH 0x4000FFFF
#define PACMAN_TIMING_LEN  0x00010000

// PACMAN DMA interface HW Address
#define DMA_ADDR 0x40400000
#define DMA_HIGH 0x4040FFFF
#define DMA_LEN  0x00010000

// HW Address range for TX circular buffer
#define DMA_TX_ADDR   0x30000000
#define DMA_TX_MAXLEN 0x01000000

// HW Address range for RX circular buffer
#define DMA_RX_ADDR   0x31000000
#define DMA_RX_MAXLEN 0x0F000000

// I2C Virtual Registers:
// I2C-1 virtual register space as offsets:
// for example SET_VDDA register for tile 3 is located at I2C_BASE_ADDR + 0x010 + (3-1).
#define I2C_VREG_OFFSET_SET_VDDA   0x010  // set VDDA level (one register per tile + test(0xA) )
#define I2C_VREG_OFFSET_SET_VDDD   0x020  // set VDDD level (one register per tile + test(0xA) )
#define I2C_VREG_OFFSET_MON_VDDA   0x030  // ADC for VDDA voltage (one register per tile, plus two board-level)
#define I2C_VREG_OFFSET_MON_VDDD   0x040  // ADC for VDDD voltage (one register per tile, plus two board-level)
#define I2C_VREG_OFFSET_MON_IDDA   0x050  // ADC for VDDA current (one register per tile, plus two board-level)
#define I2C_VREG_OFFSET_MON_IDDD   0x060  // ADC for VDDA current (one register per tile, plus two board-level)
#define I2C_VREG_OFFSET_SET_MUX_FRONT_PANEL  0x070  // MUX setting for Front Panel  (0-9: ITILE, 10: DAC, otherwise: no connection)
#define I2C_VREG_OFFSET_SET_MUX_ADC          0x080  // MUX setting for ADC          (0-9: ITILE, 10: DAC, otherwise: no connection)

#endif
