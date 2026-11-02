#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include "hw_access.h"
#include "assert.h"


// move to header?
#define DMA_REGISTERS_LEN      0x00010000
#define AXIL_REGISTERS_LEN     0x00010000

//
// AXI-Lite Registers:
//

static volatile uint32_t * G_AXIL  = NULL;

void axil_platform_init(){
  // do nothing if already initialized:
  if (G_AXIL != NULL)
    return;

  axil_platform_clear_status();

  printf("INFO:  Opening /dev/mem.\n");
  int dh = open("/dev/mem", O_RDWR|O_SYNC);
  if (dh < 0) {
    printf("ERROR:  Failed to open /dev/mem\r\n");
    return;
  }

  printf("INFO:  Initializing PACMAN AXI-Lite interface of size %d at 0x%X\n", AXIL_REGISTERS_LEN, AXIL_REGISTERS_BASEADDR);
  G_AXIL = (uint32_t*) mmap(NULL, AXIL_REGISTERS_LEN, PROT_READ|PROT_WRITE, MAP_SHARED, dh, AXIL_REGISTERS_BASEADDR);
  if (G_AXIL == MAP_FAILED) {
    printf("ERROR:  mmap failed");
    close(dh);
    return;
  }
  close(dh);
}

void axil_platform_close(){ }

hw_u32_t axil_platform_status(){ return HW_SUCCESS; }

void axil_platform_clear_status() {}

hw_val_t axil_read_register  (hw_addr_t offset){
  return G_AXIL[offset>>2];
}

void     axil_write_register (hw_addr_t offset, hw_val_t value){
  G_AXIL[offset>>2] = value;
}

//
// Timing Registers:
//

// placeholder values until new firmware
#define TIMING_REGISTERS_BASEADDR AXIL_REGISTERS_BASEADDR
#define TIMING_REGISTERS_LEN AXIL_REGISTERS_LEN

static volatile uint32_t * G_TIMING  = NULL;

void timing_platform_init(){
  // do nothing if already initialized:
  if (G_TIMING != NULL)
    return;

  timing_platform_clear_status();

  printf("INFO:  Opening /dev/mem.\n");
  int dh = open("/dev/mem", O_RDWR|O_SYNC);
  if (dh < 0) {
    printf("ERROR:  Failed to open /dev/mem\r\n");
    return;
  }

  printf("INFO:  Initializing PACMAN AXI-Lite interface of size %d at 0x%X\n", TIMING_REGISTERS_LEN, TIMING_REGISTERS_BASEADDR);
  G_TIMING = (uint32_t*) mmap(NULL, TIMING_REGISTERS_LEN, PROT_READ|PROT_WRITE, MAP_SHARED, dh, TIMING_REGISTERS_BASEADDR);
  if (G_TIMING == MAP_FAILED) {
    printf("ERROR:  mmap failed");
    close(dh);
    return;
  }
  close(dh);
}

void timing_platform_close(){ }

hw_u32_t timing_platform_status(){ return HW_SUCCESS; }

void timing_platform_clear_status() {}

hw_val_t timing_read_register  (hw_addr_t offset){
  return G_TIMING[offset>>2];
}

void     timing_write_register (hw_addr_t offset, hw_val_t value){
  G_TIMING[offset>>2] = value;
}

//
// DMA Registers (AXI-Lite Interface):
//

static volatile uint32_t * G_DMA  = NULL;

void dma_platform_init(){
  // do nothing if already initialized:
  if (G_DMA != NULL)
    return;

  dma_platform_clear_status();


  printf("INFO:  Opening /dev/mem.\n");
  int dh = open("/dev/mem", O_RDWR|O_SYNC);
  if (dh < 0) {
    printf("ERROR:  Failed to open /dev/mem\r\n");
    return;
  }

  printf("INFO:  Initializing DMA AXI-Lite interface of size %d at 0x%X\n", DMA_REGISTERS_LEN, DMA_REGISTERS_BASEADDR);
  G_DMA = (uint32_t*)mmap(NULL, DMA_REGISTERS_LEN, PROT_READ|PROT_WRITE, MAP_SHARED, dh, DMA_REGISTERS_BASEADDR);
  if (G_DMA == MAP_FAILED) {
    printf("ERROR:  mmap failed");
    close(dh);
    return;
  }

  close(dh);
}

void dma_platform_close(){
}

hw_u32_t dma_platform_status(){ return HW_SUCCESS; }

void dma_platform_clear_status() {}

hw_val_t dma_read_register  (hw_addr_t offset){
  return G_DMA[offset>>2];
}

void     dma_write_register (hw_addr_t offset, hw_val_t value){
  G_DMA[offset>>2] = value;
}

//
// DMA buffer:
//

static volatile uint32_t * G_BUF  = NULL;
static hw_addr_t G_BUF_BASEADDR = 0;
static hw_addr_t G_BUF_SIZE = 0;

void dma_platform_init_buffer(hw_addr_t baseaddr, hw_addr_t size){
  G_BUF_BASEADDR = baseaddr;
  G_BUF_SIZE     = size;

  printf("INFO:  Opening /dev/mem.\n");
  int dh = open("/dev/mem", O_RDWR|O_SYNC);
    if (dh < 0) {
    printf("ERROR:  Failed to open /dev/mem\r\n");
    return;
  }

  printf("INFO:  Initializing DMA buffer of size %d at 0x%X\n", G_BUF_SIZE, G_BUF_BASEADDR);
  G_BUF = (uint32_t*) mmap(NULL, G_BUF_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, dh, G_BUF_BASEADDR);
  if (G_BUF == MAP_FAILED) {
    printf("ERROR:  mmap failed");
    close(dh);
    return;
  }
  close(dh);
}


hw_ptr_t dma_ptr(hw_addr_t addr){
    assert(addr >= G_BUF_BASEADDR);
    hw_addr_t offset = addr - G_BUF_BASEADDR;
    assert(offset < G_BUF_SIZE);
    return &G_BUF[offset>>2];
}

hw_ptr_t dma_safe_buffer(hw_addr_t addr, hw_addr_t size){
  assert(addr >= G_BUF_BASEADDR);
  hw_addr_t offset = addr - G_BUF_BASEADDR;
  assert(size <= G_BUF_SIZE);
  assert(offset <= G_BUF_SIZE - size);
  return &G_BUF[offset>>2];
}

//
// I2C Interface:
//

#define I2C_DEV "/dev/i2c-0"
#define I2C_DEBUG false
#define I2C_TIMEOUT_10MS 5   // Kernel transfer timeout, in 10 ms units

// Status flags
#define IIC_STATUS_INIT     0x01
#define IIC_STATUS_WRITE    0x02
#define IIC_STATUS_READ     0x04
#define IIC_STATUS_TIMEOUT  0x08

static int G_IIC_FH = -1;
static hw_u32_t G_IIC_STATUS = 0;
static hw_u8_t G_IIC_ADDR = 0;
static hw_u8_t G_IIC_REG = 0;

// Record a failure.  The first one keeps its device and register.
static void iic_fail(hw_u32_t flag, hw_u8_t addr, hw_u8_t reg) {
  if (G_IIC_STATUS == 0) {
    G_IIC_ADDR = addr;
    G_IIC_REG = reg;
  }
  G_IIC_STATUS |= flag;
}

void iic_platform_init() {
  // do nothing if already initialized:
  if (G_IIC_FH >= 0) {
    return;
  }

  G_IIC_FH = open(I2C_DEV, O_RDWR);
  if (G_IIC_FH < 0) {
    printf("**ERROR** Failed to open I2C device\n");
    iic_fail(IIC_STATUS_INIT, 0, 0);
    return;
  }
  ioctl(G_IIC_FH, I2C_TIMEOUT, I2C_TIMEOUT_10MS);
}


void iic_platform_close() {
}

hw_u32_t iic_platform_status(){
  return G_IIC_STATUS;
}

// Kind of a failed transfer, a timeout if the kernel timed out
#define IIC_KIND(kind) (errno == ETIMEDOUT ? IIC_STATUS_TIMEOUT : (kind))

// The kernel driver recovers a stuck bus itself (scl-gpios and sda-gpios in the device tree), so
// the device is only reopened here.
void iic_platform_reset(){
  G_IIC_STATUS = 0;
  G_IIC_ADDR = 0;
  G_IIC_REG = 0;
  if (G_IIC_FH >= 0)
    close(G_IIC_FH);
  G_IIC_FH = -1;
  iic_platform_init();
}

void iic_platform_print_status(){
  if (G_IIC_STATUS == 0) {
    printf("I2C status:  OK\n");
    return;
  }
  printf("I2C status:  0x%x (%s%s%s%s) at device 0x%02x register 0x%02x\n", (unsigned) G_IIC_STATUS,
         (G_IIC_STATUS & IIC_STATUS_INIT)    ? " init"    : "",
         (G_IIC_STATUS & IIC_STATUS_WRITE)   ? " write"   : "",
         (G_IIC_STATUS & IIC_STATUS_READ)    ? " read"    : "",
         (G_IIC_STATUS & IIC_STATUS_TIMEOUT) ? " timeout" : "",
         G_IIC_ADDR, G_IIC_REG);
}

// Write a sequence of bytes to an I2C device
void iic_write(hw_u8_t addr, hw_u8_t reg, const hw_u8_t *data, hw_u32_t len) {
    if (G_IIC_STATUS)
        return;
    if (G_IIC_FH < 0) {
        printf("**ERROR** iic_write: I2C not initialized\n");
        iic_fail(IIC_STATUS_INIT, 0, 0);
        return;
    }

    if (ioctl(G_IIC_FH, I2C_SLAVE, addr) < 0) {
        printf("**ERROR** iic_write: Failed to set I2C address 0x%02x\n", addr);
        iic_fail(IIC_STATUS_WRITE, addr, reg);
        return;
    }

    hw_u8_t buf[len + 1];
    buf[0] = reg;
    for (hw_u32_t i = 0; i < len; i++)
        buf[i + 1] = data[i];

    errno = 0;
    ssize_t wrote = write(G_IIC_FH, buf, len + 1);
    if (wrote != (ssize_t)(len + 1)) {
      printf("**ERROR** iic_write: Failed to write %u bytes to 0x%02x (return value:  %zd\n", len+1, addr, wrote);
      iic_fail(IIC_KIND(IIC_STATUS_WRITE), addr, reg);
    }

#if I2C_DEBUG
    printf("iic_write: addr 0x%02x reg 0x%02x data:", addr, reg);
    for (hw_u32_t i = 0; i < len; i++) printf(" 0x%02x", data[i]);
    printf("\n");
#endif
}

// Read a sequence of bytes from an I2C device
void iic_read(hw_u8_t addr, hw_u8_t reg, hw_u8_t *data, hw_u32_t len, bool use_repeated_read)
{
    if (G_IIC_STATUS) {
        memset(data, 0, len);
        return;
    }
    if (G_IIC_FH < 0) {
        printf("**ERROR** iic_read: I2C not initialized\n");
        iic_fail(IIC_STATUS_INIT, 0, 0);
        memset(data, 0, len);
        return;
    }

    if (!use_repeated_read) {
        // fallback: simple write() + read()
        if (ioctl(G_IIC_FH, I2C_SLAVE, addr) < 0) {
            printf("**ERROR** iic_read: Failed to set I2C address 0x%02x\n", addr);
            iic_fail(IIC_STATUS_READ, addr, reg);
            memset(data, 0, len);
            return;
        }
        errno = 0;
        if (write(G_IIC_FH, &reg, 1) != 1) {
            printf("**ERROR** iic_read: Failed to write register 0x%02x to 0x%02x\n", reg, addr);
            iic_fail(IIC_KIND(IIC_STATUS_READ), addr, reg);
            memset(data, 0, len);
            return;
        }
        errno = 0;
        if (read(G_IIC_FH, data, len) != (ssize_t)len) {
            printf("**ERROR** iic_read: Failed to read %u bytes from 0x%02x\n", len, addr);
            iic_fail(IIC_KIND(IIC_STATUS_READ), addr, reg);
            memset(data, 0, len);
        }
        return;
    }

    // repeated read via I2C_RDWR
    struct i2c_rdwr_ioctl_data msgset;
    struct i2c_msg msgs[2];

    msgs[0].addr  = addr;
    msgs[0].flags = 0;          // write
    msgs[0].len   = 1;
    msgs[0].buf   = &reg;

    msgs[1].addr  = addr;
    msgs[1].flags = I2C_M_RD;   // read
    msgs[1].len   = len;
    msgs[1].buf   = data;

    msgset.msgs  = msgs;
    msgset.nmsgs = 2;

    errno = 0;
    if (ioctl(G_IIC_FH, I2C_RDWR, &msgset) < 0) {
        printf("**ERROR** iic_read: repeated read failed from 0x%02x reg 0x%02x\n", addr, reg);
        iic_fail(IIC_KIND(IIC_STATUS_READ), addr, reg);
        memset(data, 0, len);
    }

#if I2C_DEBUG
    printf("iic_read: addr 0x%02x reg 0x%02x data:", addr, reg);
    for (hw_u32_t i = 0; i < len; i++) printf(" 0x%02x", data[i]);
    printf("\n");
#endif
}



// Write wdata, then read rdata (see iic_read)
void iic_write_read(hw_u8_t addr, const hw_u8_t *wdata, hw_u32_t wlen, hw_u8_t *rdata,
                    hw_u32_t rlen, bool use_repeated_read)
{
    hw_u8_t reg = wlen ? wdata[0] : 0;
    if (G_IIC_STATUS) {
        memset(rdata, 0, rlen);
        return;
    }
    if (G_IIC_FH < 0) {
        printf("**ERROR** iic_write_read: I2C not initialized\n");
        iic_fail(IIC_STATUS_INIT, 0, 0);
        memset(rdata, 0, rlen);
        return;
    }

    hw_u8_t wbuf[wlen];
    memcpy(wbuf, wdata, wlen);
    if (use_repeated_read) {
        struct i2c_rdwr_ioctl_data msgset;
        struct i2c_msg msgs[2] = {
            { addr, 0,        (__u16) wlen, wbuf },
            { addr, I2C_M_RD, (__u16) rlen, rdata },
        };
        msgset.msgs  = msgs;
        msgset.nmsgs = 2;
        errno = 0;
        if (ioctl(G_IIC_FH, I2C_RDWR, &msgset) < 0) {
            printf("**ERROR** iic_write_read: repeated read failed from 0x%02x reg 0x%02x\n", addr,
                   reg);
            iic_fail(IIC_KIND(IIC_STATUS_READ), addr, reg);
            memset(rdata, 0, rlen);
        }
        return;
    }

    if (ioctl(G_IIC_FH, I2C_SLAVE, addr) < 0) {
        printf("**ERROR** iic_write_read: Failed to set I2C address 0x%02x\n", addr);
        iic_fail(IIC_STATUS_READ, addr, reg);
        memset(rdata, 0, rlen);
        return;
    }
    errno = 0;
    if (write(G_IIC_FH, wbuf, wlen) != (ssize_t) wlen) {
        printf("**ERROR** iic_write_read: Failed to write %u bytes to 0x%02x\n", wlen, addr);
        iic_fail(IIC_KIND(IIC_STATUS_READ), addr, reg);
        memset(rdata, 0, rlen);
        return;
    }
    errno = 0;
    if (read(G_IIC_FH, rdata, rlen) != (ssize_t) rlen) {
        printf("**ERROR** iic_write_read: Failed to read %u bytes from 0x%02x\n", rlen, addr);
        iic_fail(IIC_KIND(IIC_STATUS_READ), addr, reg);
        memset(rdata, 0, rlen);
    }
}

// The device tree's clock-frequency for I2C0 (big-endian);  0 if it cannot be read
hw_u32_t iic_platform_clock_hz(){
    static const char * PATH[] = {
        "/sys/firmware/devicetree/base/axi/i2c@e0004000/clock-frequency",
        "/sys/firmware/devicetree/base/amba/i2c@e0004000/clock-frequency"};
    for (int i = 0; i < 2; i++) {
        hw_u8_t b[4];
        int fd = open(PATH[i], O_RDONLY);
        if (fd < 0) continue;
        ssize_t n = read(fd, b, 4);
        close(fd);
        if (n == 4) return ((hw_u32_t) b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
    }
    return 0;
}


//
// GPIO
//

// Status flags
static hw_u32_t G_MIO_STATUS = 0;

// First MIO pin index (platform-dependent)
static const hw_u32_t G_MIO_FIRST_PIN = 906;

void gpio_platform_init() {
    G_MIO_STATUS = 0;
    // No extra initialization needed for sysfs GPIO
}

void gpio_platform_close() {
    // No cleanup needed for sysfs GPIO
}

hw_u32_t gpio_platform_status() {
    return G_MIO_STATUS;
}

void gpio_platform_clear_status() {
    G_MIO_STATUS = 0;
}

void gpio_platform_configure_pin(hw_u32_t pin, gpio_dir_t dir, hw_u32_t value) {
    char buf[64];
    int fd;

    // 1. Export the pin (ignore errors if already exported)
    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd >= 0) {
        snprintf(buf, sizeof(buf), "%u", G_MIO_FIRST_PIN + pin);
	ssize_t n;
        n=write(fd, buf, strlen(buf));
	(void) n;
        close(fd);
    }

    // 2. Set direction
    snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%u/direction", G_MIO_FIRST_PIN + pin);
    fd = open(buf, O_WRONLY);
    if (fd < 0) {
        G_MIO_STATUS |= 1;  // direction open failed
        return;
    }
    const char *dir_str = (dir == GPIO_DIR_OUTPUT) ? "out" : "in";
    if (write(fd, dir_str, strlen(dir_str)) != (ssize_t)strlen(dir_str)) {
        G_MIO_STATUS |= 2;  // direction write failed
        close(fd);
        return;
    }
    close(fd);

    // 3. Set initial value if output
    if (dir == GPIO_DIR_OUTPUT) {
        snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%u/value", G_MIO_FIRST_PIN + pin);
        fd = open(buf, O_WRONLY);
        if (fd < 0) {
            G_MIO_STATUS |= 4;  // value open failed
            return;
        }
        char vbuf[2];
        snprintf(vbuf, sizeof(vbuf), "%u", value ? 1 : 0);
        if (write(fd, vbuf, 1) != 1) {
            G_MIO_STATUS |= 8;  // value write failed
        }
        close(fd);
    }
}

void gpio_write_pin(hw_u32_t pin, hw_u32_t value) {
    char fbuf[64];
    snprintf(fbuf, sizeof(fbuf), "/sys/class/gpio/gpio%u/value", G_MIO_FIRST_PIN + pin);
    int fd = open(fbuf, O_WRONLY);
    if (fd < 0) {
        G_MIO_STATUS |= 16;
        return;
    }
    char vbuf[2];
    snprintf(vbuf, sizeof(vbuf), "%u", value);
    if (write(fd, vbuf, 1) != 1) {
        G_MIO_STATUS |= 32;
    }
    close(fd);
}

hw_u32_t gpio_read_pin(hw_u32_t pin) {
    char fbuf[64];
    snprintf(fbuf, sizeof(fbuf), "/sys/class/gpio/gpio%u/value", G_MIO_FIRST_PIN + pin);
    int fd = open(fbuf, O_RDONLY);
    if (fd < 0) {
        G_MIO_STATUS |= 64;
        return 0;
    }
    char vbuf[2];
    if (read(fd, vbuf, 1) != 1) {
        G_MIO_STATUS |= 128;
        close(fd);
        return 0;
    }
    close(fd);
    return (vbuf[0] == '0') ? 0 : 1;
}

//
// BRAM:
//

// PACMAN AXI-Lite interface to BRAM
#define PACMAN_BRAM_ADDR 0x42000000
#define PACMAN_BRAM_HIGH 0x42001FFF
#define PACMAN_BRAM_LEN  (PACMAN_BRAM_HIGH - PACMAN_BRAM_ADDR + 1)

static uint32_t G_BRAM_STATUS = 0;
static volatile uint32_t *G_BRAM = NULL;
static int g_mem_fd = -1;

uint32_t bram_platform_status() {
    return G_BRAM_STATUS;
}

void bram_platform_clear_status() {
    G_BRAM_STATUS = 0;
}

void bram_platform_init() {
    if (G_BRAM != NULL) return; // already initialized

    G_BRAM_STATUS = 0;

    g_mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (g_mem_fd < 0) {
        perror("ERROR: opening /dev/mem for BRAM");
        G_BRAM_STATUS |= 1;
        return;
    }

    G_BRAM = (volatile uint32_t *)mmap(NULL, PACMAN_BRAM_LEN,
                                       PROT_READ | PROT_WRITE,
                                       MAP_SHARED, g_mem_fd,
                                       PACMAN_BRAM_ADDR);
    if (G_BRAM == MAP_FAILED) {
        perror("ERROR: mmap BRAM");
        G_BRAM = NULL;
        G_BRAM_STATUS |= 2;
        close(g_mem_fd);
        g_mem_fd = -1;
        return;
    }

    printf("INFO: BRAM platform initialized (0x%X bytes at 0x%X)\n", PACMAN_BRAM_LEN, PACMAN_BRAM_ADDR);
}

void bram_platform_close() {
    if (G_BRAM != NULL) {
        munmap((void *)G_BRAM, PACMAN_BRAM_LEN);
        G_BRAM = NULL;
    }
    if (g_mem_fd >= 0) {
        close(g_mem_fd);
        g_mem_fd = -1;
    }
}

void bram_platform_write(uint32_t addr, uint32_t value) {
    if (!G_BRAM || addr > PACMAN_BRAM_LEN) {
        G_BRAM_STATUS |= 4; // invalid write
        return;
    }
    G_BRAM[addr >> 2] = value;
}

uint32_t bram_platform_read(uint32_t addr) {
    if (!G_BRAM || addr > PACMAN_BRAM_LEN) {
        G_BRAM_STATUS |= 8; // invalid read
        return 0xDEADBEEF;
    }
    return G_BRAM[addr >> 2];
}

//
// Timer:
//

static struct timespec start;
static struct timespec stop;


void start_hw_timer(){
  clock_gettime(CLOCK_MONOTONIC, &start);
}
void stop_hw_timer(){
  clock_gettime(CLOCK_MONOTONIC, &stop);
}

hw_u32_t hw_timer_elapsed_us(){
  long seconds        = stop.tv_sec  - start.tv_sec;
  long nanoseconds    = stop.tv_nsec - start.tv_nsec;
  hw_u32_t elapsed_us = seconds * 1000000 + nanoseconds / 1000;
  return elapsed_us;
}

int input_choice(){
  char line[64];
  int c, n = 0;

  printf("> ");
  fflush(stdout);
  while ((c = getchar()) != EOF && c != '\n'){
    if (c != '\r' && n < (int) sizeof(line) - 1)
      line[n++] = c;
  }
  line[n] = 0;
  if (c == EOF && n == 0){
    printf("\r\n");
    return INPUT_END;
  }
  // A terminal echoes the line itself;  piped input is echoed here:
  if (!isatty(STDIN_FILENO))
    printf("%s\r\n", line);
  return line[0];
}
