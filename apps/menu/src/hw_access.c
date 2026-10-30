#include "xparameters.h"
#include "xil_io.h"
#include "xgpiops.h"
#include "xiicps.h"
#include "xstatus.h"
#include "xtime_l.h"
#include "hw_access.h"

void axil_platform_init(){}

void axil_platform_close(){}

hw_u32_t axil_platform_status(){ return HW_SUCCESS; }

void axil_platform_clear_status() {}

hw_val_t axil_read_register  (hw_addr_t offset){
  return Xil_In32(AXIL_REGISTERS_BASEADDR+offset);
}

void     axil_write_register (hw_addr_t offset, hw_val_t value){
  Xil_Out32(AXIL_REGISTERS_BASEADDR+offset, value);
}

//
// DMA Interface:
//

void dma_platform_init(){}

hw_u32_t dma_platform_status(){ return HW_SUCCESS; }

void dma_platform_clear_status() {}

hw_val_t dma_read_register  (hw_addr_t offset){
  return Xil_In32(DMA_REGISTERS_BASEADDR+offset);
}

void     dma_write_register (hw_addr_t offset, hw_val_t value){
  Xil_Out32(DMA_REGISTERS_BASEADDR+offset, value);
}

void dma_platform_init_buffer(hw_addr_t baseaddr, hw_addr_t size){
}

hw_ptr_t dma_ptr(hw_addr_t addr){
  return (hw_ptr_t) addr;
}

hw_ptr_t dma_safe_buffer(hw_addr_t addr, hw_addr_t size){
  return (hw_ptr_t) addr;
}

//
// I2C Interface:
//

// I2C device parameters
#define IIC_DEVICE_ID       XPAR_XIICPS_0_DEVICE_ID
#define IIC_SCLK_RATE       200000
#define IIC_BUS_TIMEOUT_US  10000   // Longest wait for the bus to go idle
#define IIC_SCL_MIO         10      // I2C0 SCL and SDA (MIO 10 and 11)
#define IIC_SDA_MIO         11

// Status flags
#define IIC_STATUS_INIT     0x01
#define IIC_STATUS_WRITE    0x02
#define IIC_STATUS_READ     0x04
#define IIC_STATUS_TIMEOUT  0x08

static XIicPs iicps;
static int iicps_initialized = 0;
static hw_u32_t G_IIC_STATUS = 0;
static hw_u8_t G_IIC_ADDR = 0;
static hw_u8_t G_IIC_REG = 0;
static XGpioPs gpiops;   // PS GPIO (defined below), for bus recovery

// Record a failure.  The first one keeps its device and register.
static void iic_fail(hw_u32_t flag, hw_u8_t addr, hw_u8_t reg) {
  if (G_IIC_STATUS == 0) {
    G_IIC_ADDR = addr;
    G_IIC_REG = reg;
  }
  G_IIC_STATUS |= flag;
}

// Initialize the I2C interface
void iic_platform_init() {
  // do nothing if already initialized
  if (iicps_initialized)
    return;

  printf("Initializing I2C interface...\r\n");

  XIicPs_Config *cfg = XIicPs_LookupConfig(IIC_DEVICE_ID);
  if (cfg == NULL) {
    printf("FAILED: No config found.\r\n");
    iic_fail(IIC_STATUS_INIT, 0, 0);
    return;
  }

  int status = XIicPs_CfgInitialize(&iicps, cfg, cfg->BaseAddress);
  if (status != XST_SUCCESS) {
    printf("FAILED: Config initialize.\r\n");
    iic_fail(IIC_STATUS_INIT, 0, 0);
    return;
  }

  printf("Performing I2C self-test...\r\n");
  status = XIicPs_SelfTest(&iicps);
  if (status != XST_SUCCESS) {
    printf("FAILED: Self-test.\r\n");
    iic_fail(IIC_STATUS_INIT, 0, 0);
    return;
  }

  XIicPs_SetSClk(&iicps, IIC_SCLK_RATE);
  iicps_initialized = 1;
  printf("I2C interface initialized successfully.\r\n");

}

void iic_platform_close() {
  // noting to release, just mark as not inialized:
  iicps_initialized = 0;
}

hw_u32_t iic_platform_status() {
  return G_IIC_STATUS;
}

// SLCR registers for the MIO pin multiplexer
#define SLCR_LOCK         0xF8000004
#define SLCR_UNLOCK       0xF8000008
#define SLCR_LOCK_KEY     0x767B
#define SLCR_UNLOCK_KEY   0xDF0D
#define SLCR_MIO_PIN(n)   (0xF8000700 + 4*(n))
#define SLCR_MIO_SEL      0x000000FE   // L0-L3 select bits, all zero for GPIO

// Drive an MIO pin low (low) or release it to the pull-up (!low)
static void iic_mio_drive(hw_u32_t pin, int low) {
  if (low) {
    XGpioPs_WritePin(&gpiops, pin, 0);
    XGpioPs_SetDirectionPin(&gpiops, pin, 1);
    XGpioPs_SetOutputEnablePin(&gpiops, pin, 1);
  } else {
    XGpioPs_SetOutputEnablePin(&gpiops, pin, 0);
    XGpioPs_SetDirectionPin(&gpiops, pin, 0);
  }
  usleep(5);
}

// Free a bus held by a device, by clocking SCL (as GPIO) until SDA is released, then a stop.
static void iic_recover_bus() {
  gpio_platform_init();
  iic_mio_drive(IIC_SCL_MIO, 0);
  iic_mio_drive(IIC_SDA_MIO, 0);

  hw_u32_t scl_pin = Xil_In32(SLCR_MIO_PIN(IIC_SCL_MIO));
  hw_u32_t sda_pin = Xil_In32(SLCR_MIO_PIN(IIC_SDA_MIO));
  Xil_Out32(SLCR_UNLOCK, SLCR_UNLOCK_KEY);
  Xil_Out32(SLCR_MIO_PIN(IIC_SCL_MIO), scl_pin & ~SLCR_MIO_SEL);
  Xil_Out32(SLCR_MIO_PIN(IIC_SDA_MIO), sda_pin & ~SLCR_MIO_SEL);

  for (int i = 0; i < 9 && XGpioPs_ReadPin(&gpiops, IIC_SDA_MIO) == 0; i++) {
    iic_mio_drive(IIC_SCL_MIO, 1);
    iic_mio_drive(IIC_SCL_MIO, 0);
  }
  iic_mio_drive(IIC_SCL_MIO, 1);
  iic_mio_drive(IIC_SDA_MIO, 1);
  iic_mio_drive(IIC_SCL_MIO, 0);
  iic_mio_drive(IIC_SDA_MIO, 0);

  Xil_Out32(SLCR_MIO_PIN(IIC_SCL_MIO), scl_pin);
  Xil_Out32(SLCR_MIO_PIN(IIC_SDA_MIO), sda_pin);
  Xil_Out32(SLCR_LOCK, SLCR_LOCK_KEY);
}

void iic_platform_reset() {
  iic_recover_bus();
  G_IIC_STATUS = 0;
  G_IIC_ADDR = 0;
  G_IIC_REG = 0;
  if (iicps_initialized) {
    XIicPs_Reset(&iicps);
    XIicPs_SetSClk(&iicps, IIC_SCLK_RATE);
  } else {
    iic_platform_init();
  }
}

void iic_platform_print_status() {
  if (G_IIC_STATUS == 0) {
    printf("I2C status:  OK\r\n");
    return;
  }
  printf("I2C status:  0x%x (%s%s%s%s) at device 0x%02x register 0x%02x\r\n",
         (unsigned) G_IIC_STATUS,
         (G_IIC_STATUS & IIC_STATUS_INIT)    ? " init"    : "",
         (G_IIC_STATUS & IIC_STATUS_WRITE)   ? " write"   : "",
         (G_IIC_STATUS & IIC_STATUS_READ)    ? " read"    : "",
         (G_IIC_STATUS & IIC_STATUS_TIMEOUT) ? " timeout" : "",
         G_IIC_ADDR, G_IIC_REG);
}

// Wait for the bus to go idle, at most IIC_BUS_TIMEOUT_US.  Returns 0 if idle.
static int iic_wait_bus(hw_u8_t addr, hw_u8_t reg) {
  XTime start, now;
  XTime_GetTime(&start);
  while (XIicPs_BusIsBusy(&iicps)) {
    XTime_GetTime(&now);
    if ((now - start) > (XTime) COUNTS_PER_SECOND / 1000000 * IIC_BUS_TIMEOUT_US) {
      printf("ERROR: I2C bus busy timeout at addr 0x%x reg 0x%x\r\n", addr, reg);
      iic_fail(IIC_STATUS_TIMEOUT, addr, reg);
      return -1;
    }
  }
  return 0;
}

void iic_write(hw_u8_t addr, hw_u8_t reg, const hw_u8_t *data, hw_u32_t len) {
  if (G_IIC_STATUS)
    return;
  if (!iicps_initialized) {
    iic_fail(IIC_STATUS_INIT, 0, 0);
    return;
  }

  hw_u32_t buf_len = 1 + len;
  hw_u8_t buf[buf_len];

  buf[0] = reg;
  for (hw_u32_t i = 0; i < len; i++) {
    buf[1 + i] = data[i];
  }

  int status = XIicPs_MasterSendPolled(&iicps, buf, buf_len, addr);
  if (status != XST_SUCCESS) {
    printf("I2C write failed to addr 0x%x reg 0x%x\r\n", addr, reg);
    iic_fail(IIC_STATUS_WRITE, addr, reg);
  }

  iic_wait_bus(addr, reg);
}

void iic_read(hw_u8_t addr, hw_u8_t reg, hw_u8_t *data, hw_u32_t len,  bool use_repeated_read) {
  // On any failure the caller gets zeros:
  for (hw_u32_t i = 0; i < len; i++)
    data[i] = 0;

  if (G_IIC_STATUS)
    return;
  if (!iicps_initialized) {
    iic_fail(IIC_STATUS_INIT, 0, 0);
    return;
  }

  // Repeated start, as in the Xilinx example (xiicps_repeated_start_example.c):  the option
  // holds the bus after the register is sent, so the bus never goes idle before the receive;
  // clearing it before the receive releases the bus (stop) when the receive ends.
  if (use_repeated_read)
    XIicPs_SetOptions(&iicps, XIICPS_REP_START_OPTION);

  // Send register first
  hw_u8_t reg_buf = reg;
  int status = XIicPs_MasterSendPolled(&iicps, &reg_buf, 1, addr);

  if (use_repeated_read)
    XIicPs_ClearOptions(&iicps, XIICPS_REP_START_OPTION);

  if (status != XST_SUCCESS) {
    printf("I2C read: failed to send register 0x%x\r\n", reg);
    iic_fail(IIC_STATUS_READ, addr, reg);
    iic_wait_bus(addr, reg);
    return;
  }

  if (!use_repeated_read) {
    if (iic_wait_bus(addr, reg))
      return;
  }

  // Read data
  status = XIicPs_MasterRecvPolled(&iicps, data, len, addr);
  if (status != XST_SUCCESS) {
    printf("I2C read failed from addr 0x%x reg 0x%x\r\n", addr, reg);
    iic_fail(IIC_STATUS_READ, addr, reg);
    for (hw_u32_t i = 0; i < len; i++)
      data[i] = 0;
  }

  iic_wait_bus(addr, reg);
}

//
// PS GPIO (MIO) Interface:
//

#define GPIOPS_DEVICE_ID XPAR_XGPIOPS_0_DEVICE_ID
static XGpioPs gpiops;
static int gpio_initialized = 0;

void gpio_platform_init(){
  // do nothing if already initialized
  if (gpio_initialized)
    return;

  printf("initializing PS GPIO interface (MIO)...");
  XGpioPs_Config *cfg = XGpioPs_LookupConfig(GPIOPS_DEVICE_ID);
  if (cfg == NULL) {
    printf("FAILED.\r\n");
    return;
  }
  int status = XGpioPs_CfgInitialize(&gpiops, cfg, cfg->BaseAddr);
  if (status != XST_SUCCESS) {
    printf("FAILED.\r\n");
    return;
  }
  printf("SUCCESS.\r\n");
  gpio_initialized = 1;
}

void gpio_platform_close(){
  // nothing to release, just mark as not initialized:
  gpio_initialized = 0;
}

hw_u32_t gpio_platform_status(){ return HW_SUCCESS; }

void gpio_platform_clear_status(){}

void gpio_platform_configure_pin(hw_u32_t pin, gpio_dir_t dir, hw_u32_t value){
  XGpioPs_SetDirectionPin(&gpiops, pin, (dir==GPIO_DIR_OUTPUT));
  XGpioPs_SetOutputEnablePin(&gpiops, pin, (dir==GPIO_DIR_OUTPUT));
  if (dir==GPIO_DIR_OUTPUT)
    XGpioPs_WritePin(&gpiops, pin, value);
}

void gpio_write_pin(hw_u32_t pin, hw_u32_t value){
  XGpioPs_WritePin(&gpiops, pin, value);
}

hw_u32_t gpio_read_pin(hw_u32_t pin){
  return XGpioPs_ReadPin(&gpiops, pin);
}

//
// BRAM Interface:  (Currently Unused and Untested)
//

void        bram_platform_init(){}

void        bram_platform_close(){}

hw_u32_t    bram_platform_status(){ return HW_SUCCESS; }

void        bram_platform_clear_status(){}

void        bram_platform_write(hw_u32_t addr, hw_u32_t value){
  Xil_Out32(AXIL_REGISTERS_BASEADDR+addr, value);
}

hw_u32_t    bram_platform_read(hw_u32_t addr){
  return Xil_In32(AXIL_REGISTERS_BASEADDR+addr);
}





static XTime G_START_TIME;
static XTime G_STOP_TIME;

void start_hw_timer(){
  XTime_GetTime(&G_START_TIME);
}
void stop_hw_timer(){
  XTime_GetTime(&G_STOP_TIME);
}

hw_u32_t hw_timer_elapsed_us(){
  XTime elapsed_us = ((G_STOP_TIME - G_START_TIME) * 1000000ULL) / COUNTS_PER_SECOND ;
  return (unsigned) elapsed_us;
}



char input_choice(void) {
  char c = 0, last = 0;
  print("Enter choice and press return: ");

  while (1) {
    c = inbyte();   // blocking read from UART

    if (c == '\r' || c == '\n') {
      return last;
    }

    // echo so user sees what they typed
    outbyte(c);
    last = c;
  }
}
