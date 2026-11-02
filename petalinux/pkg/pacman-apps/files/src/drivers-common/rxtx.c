#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "hw_access.h"
#include "dma.h"
#include "global.h"
#include "asic.h"
#include "rxtx.h"

#define ALL_UARTS 0xFFFFFFFFFFULL   // UARTs 1-40

// RX UART config fields:
#define RX_CONFIG_PERIOD_SHIFT 16          // [23:16] clocks per bit
#define RX_CONFIG_PERIOD_MASK  0x00FF0000
#define RX_CONFIG_FLAGS_SHIFT  14          // [15:14] bit 14: start a byte despite vote dissent
#define RX_CONFIG_FLAGS_MASK   0x0000C000
#define RX_CONFIG_MODE_SHIFT   12          // [13:12] sampling mode
#define RX_CONFIG_MODE_MASK    0x00003000
#define RX_CONFIG_INPUT_SHIFT  8           // [11:8]  input select
#define RX_CONFIG_INPUT_MASK   0x00000F00
#define RX_CONFIG_PHASE_SHIFT  0           // [7:0]   sample point
#define RX_CONFIG_PHASE_MASK   0x000000FF

// RX buffer config fields, 0 = no condition:
#define RX_BUFFER_MAX_WORDS_SHIFT 16          // [31:16] max 192-bit words per DMA packet
#define RX_BUFFER_MAX_WORDS_MASK  0xFFFF0000
#define RX_BUFFER_TIMEOUT_SHIFT   0           // [15:0]  packet timeout, in units of 256 clocks
#define RX_BUFFER_TIMEOUT_MASK    0x0000FFFF

static uint64_t G_UART_MASK = ALL_UARTS;

static unsigned tx_counter = 0;
static unsigned tx_ring_size = 0;
static unsigned rx_ring_size = 0;

void init_rxtx(void){
  dma_platform_init();
  dma_platform_init_buffer(DMA_BUFFER_BASEADDR, DMA_BUFFER_SIZE);
  dma_init_batch_tx_taildesc(TX_BATCH_NEXTDESC_ADDR);
  dma_init_batch_rx_taildesc(RX_BATCH_NEXTDESC_ADDR);
}

void init_tx_descriptor_ring_mode(int ring_size){
  printf("INFO:  initializing TX BD ring:\r\n");

  tx_ring_size = ring_size;

  dma_init_bd_ring(TX_BD_BASEADDR, ring_size, TX_PACKET_BYTES, DMA_BD_CONTROL_SOF | DMA_BD_CONTROL_EOF, DMA_BD_STATUS_COMPLETE);

  dma_write_tx_curdesc(TX_BD_BASEADDR);
  dma_write_tx_taildesc(TX_BD_BASEADDR);

  dma_write_batch_tx_taildesc(TX_BD_BASEADDR);

  dma_run_tx(DMA_TIMEOUT);

  // send initial empty TX
  dma_clear_bd_status(TX_BD_BASEADDR);
  dma_write_tx_taildesc(TX_BD_BASEADDR);
}

void init_rx_descriptor_ring_mode(int ring_size){
  printf("INFO:  initializing RX BD ring:\r\n");

  rx_ring_size = ring_size;

  dma_init_bd_ring(RX_BD_BASEADDR, ring_size, RX_BUF_BYTES, 0, 0);

  dma_write_rx_curdesc(dma_get_next_bd_addr(RX_BD_BASEADDR));
  dma_write_rx_taildesc(RX_BD_BASEADDR);

  dma_write_batch_rx_taildesc(RX_BD_BASEADDR);

  dma_run_rx(DMA_TIMEOUT);

  dma_write_rx_taildesc(RX_BD_BASEADDR);
}

// size of the TX/RX BD rings set up by init_rxtx_descriptor_ring_mode in this process (0: none yet)
unsigned rxtx_ring_size(void){
  return (tx_ring_size == rx_ring_size) ? tx_ring_size : 0;
}

void init_rxtx_descriptor_ring_mode(int ring_size){
  dma_reset_tx(DMA_TIMEOUT);
  dma_reset_rx(DMA_TIMEOUT);
  init_tx_descriptor_ring_mode(ring_size);
  init_rx_descriptor_ring_mode(ring_size);
}

void show_rxtx_bds(void){
  printf("INFO:  TX BD:\r\n");
  dma_show_bd_ring(TX_BD_BASEADDR);
  printf("INFO:  RX BD:\r\n");
  dma_show_bd_ring(RX_BD_BASEADDR);
}

void show_rxtx_head_tail(void){
  dma_show_tx_current_tail_addrs();
  dma_show_rx_current_tail_addrs();
}

unsigned rx_pending(void) {
  hw_addr_t current = dma_read_rx_curdesc();
  hw_addr_t tail    = dma_read_rx_taildesc();
  if (current >= tail)
    return (current - tail) / DMA_BD_BYTES;
  else
    return (current - tail + rx_ring_size * DMA_BD_BYTES) / DMA_BD_BYTES;
}

void clear_rxtx_ioc(void){
  printf("INFO:  clearing DMA TX IOC flag.\r\n");
  dma_clear_tx_ioc();
  printf("INFO:  clearing DMA RX IOC flag\r\n");
  dma_clear_rx_ioc();
}

void show_tx_buffer(void){
  printf("INFO:  TX Buffer:\r\n");
  dma_show_buffer_ring(TX_BD_BASEADDR, 6, 1000);
}

void show_rx_buffer(void){
  printf("INFO:  RX Buffer:\r\n");
  dma_show_buffer_ring(RX_BD_BASEADDR, 6, 1000);
}

void show_rx_transferred(void){
  printf("INFO:  RX Buffer:\r\n");
  dma_show_transferred_ring(RX_BD_BASEADDR, 6, 1000);
}

void single_tx(void){
  // TX buffer is a 128 bit header plus 40 uarts allocated 64 bits each.
  // This is a total of 84 32-bit words (4 header words, 80 uart words)
  // The resulting AXI stream is 128 bits times 21 beats.

  hw_addr_t nxta = dma_get_next_bd_addr(dma_read_tx_taildesc());
  // keep batch tail synced even when doing single buffers:
  dma_write_batch_tx_taildesc(nxta);

  hw_ptr_t tx_buf = dma_get_buffer(nxta);

  tx_buf[0]= (hw_u32_t) G_UART_MASK;
  tx_buf[1]= (hw_u32_t) (G_UART_MASK >> 32);

  for (int i=0; i<TX_PAYLOAD_U32_WORDS; i++)
    tx_buf[i+TX_HEADER_U32_WORDS] = 0xB000F000 + i + (tx_counter<<16);
  tx_counter++;

  HW_FLUSH_DCACHE(tx_buf, TX_PACKET_BYTES);
  dma_clear_tx_ioc();
  dma_clear_bd_status(nxta);
  dma_write_tx_taildesc(nxta);

  if (dma_wait_tx_ioc(DMA_TIMEOUT) > 0){
    printf("INFO: single TX yielded TX IOC flag high (SUCCESS)\r\n");
  }
}

void single_rx(void){
  hw_addr_t nxta = dma_get_next_bd_addr(dma_read_rx_taildesc());

  if (dma_read_bd_status(nxta) & DMA_BD_STATUS_COMPLETE){
    printf("INFO:  RX success.\r\n");
    // keep batch tail synced even when doing single buffers:
    dma_write_batch_rx_taildesc(nxta);
    dma_clear_bd_status(nxta);
    dma_write_rx_taildesc(nxta);
  } else {
    printf("INFO:  nothing RXed.\r\n");
  }
}

void batch_tx(void){
  unsigned count = 0;
  hw_addr_t nxta;

  while((count < 10) && (dma_next_available_tx_bd(&nxta))){
    printf("INFO:  working on buffer %d at HW addr 0x%08X \r\n", count, (unsigned int) nxta);
    hw_ptr_t tx_buf = dma_get_buffer(nxta);

    tx_buf[0]= (hw_u32_t) G_UART_MASK;
    tx_buf[1]= (hw_u32_t) (G_UART_MASK >> 32);

    for (int i=0; i<TX_PAYLOAD_U32_WORDS; i++)
      tx_buf[i+TX_HEADER_U32_WORDS] = 0xB000F000 + i + (tx_counter<<16);
    HW_FLUSH_DCACHE(tx_buf, TX_PACKET_BYTES);

    dma_add_tx_bd(nxta);
    count++;
    tx_counter++;
  }

  dma_clear_tx_ioc();
  printf("INFO:  sending batch of %d TX buffers \r\n", count);
  dma_tx_batch();

  // NOTE: the IOC fires on the first complete transfer, so this only confirms one buffer was sent
  if (dma_wait_tx_ioc(DMA_TIMEOUT) > 0){
    printf("INFO:  batch TX yielded TX IOC flag high (SUCCESS)\r\n");
  }
}

void batch_rx(void){
  unsigned count = 0;
  hw_addr_t nxta;

  while((count < 10) && dma_next_available_rx_bd(&nxta)){
    // do work on buffer ...
    count++;
    dma_add_rx_bd(nxta);
  }

  printf("INFO:  sending batch of %d RX buffers \r\n", count);
  dma_rx_batch();
}


void benchmark_dma_tx();
void benchmark_dma_rxtx_loopback();

void toggle_tx_config(void){
  static int mode = 0;
  mode = (mode + 1) % 12;
  if (mode==0){
    unsigned config = 0x00C60006;
    printf("INFO: Broadcasting tx config write 0x%08x \r\n", (unsigned int) config);
    axil_write_register(SCOPE_TX+UART_BROADCAST+C_ADDR_TX_UART_CONFIG, config);
  } else if (mode==1){
    unsigned config = 0x00000006;
    printf("INFO: Broadcasting tx config write 0x%08x \r\n", (unsigned int) config);
    axil_write_register(SCOPE_TX+UART_BROADCAST+C_ADDR_TX_UART_CONFIG, config);
  } else if (mode==2) {
    unsigned config = 0x00C60001;
    printf("INFO: Broadcasting tx config write 0x%08x \r\n", (unsigned int) config);
    axil_write_register(SCOPE_TX+UART_BROADCAST+C_ADDR_TX_UART_CONFIG, config);
  } else if (mode==3) {
    unsigned config = 0x00000001;
    printf("INFO: Broadcasting tx config write 0x%08x \r\n", (unsigned int) config);
    axil_write_register(SCOPE_TX+UART_BROADCAST+C_ADDR_TX_UART_CONFIG, config);
  } else if (mode==4) {
    unsigned config = 0x00C60002;
    printf("INFO: Broadcasting tx config write 0x%08x \r\n", (unsigned int) config);
    axil_write_register(SCOPE_TX+UART_BROADCAST+C_ADDR_TX_UART_CONFIG, config);
  } else if (mode==5) {
    unsigned config = 0x00C60003;
    printf("INFO: Broadcasting tx config write 0x%08x \r\n", (unsigned int) config);
    axil_write_register(SCOPE_TX+UART_BROADCAST+C_ADDR_TX_UART_CONFIG, config);
  } else if (mode==6) {
    unsigned config = 0x00C60004;
    printf("INFO: Broadcasting tx config write 0x%08x \r\n", (unsigned int) config);
    axil_write_register(SCOPE_TX+UART_BROADCAST+C_ADDR_TX_UART_CONFIG, config);
  } else if (mode==7) {
    unsigned config = 0x00C60005;
    printf("INFO: Broadcasting tx config write 0x%08x \r\n", (unsigned int) config);
    axil_write_register(SCOPE_TX+UART_BROADCAST+C_ADDR_TX_UART_CONFIG, config);
  } else if (mode==8) {
    unsigned config = 0x00C60006;
    printf("INFO: Broadcasting tx config write 0x%08x \r\n", (unsigned int) config);
    axil_write_register(SCOPE_TX+UART_BROADCAST+C_ADDR_TX_UART_CONFIG, config);
  } else if (mode==9) {
    unsigned config = 0x00C60007;
    printf("INFO: Broadcasting tx config write 0x%08x \r\n", (unsigned int) config);
    axil_write_register(SCOPE_TX+UART_BROADCAST+C_ADDR_TX_UART_CONFIG, config);
  } else if (mode==10) {
    unsigned config = 0x00C60008;
    printf("INFO: Broadcasting tx config write 0x%08x \r\n", (unsigned int) config);
    axil_write_register(SCOPE_TX+UART_BROADCAST+C_ADDR_TX_UART_CONFIG, config);
  } else if (mode==11) {
    unsigned config = 0x00C60009;
    printf("INFO: Broadcasting tx config write 0x%08x \r\n", (unsigned int) config);
    axil_write_register(SCOPE_TX+UART_BROADCAST+C_ADDR_TX_UART_CONFIG, config);
  }
}

static void rx_set_input(unsigned chan, unsigned input){
  if (chan < 40){
    hw_u32_t cfg = axil_read_register(SCOPE_RX+(chan<<8)+C_ADDR_RX_UART_CONFIG);
    cfg &= ~RX_CONFIG_INPUT_MASK;
    cfg |= (input << RX_CONFIG_INPUT_SHIFT) & RX_CONFIG_INPUT_MASK;
    axil_write_register(SCOPE_RX+(chan<<8)+C_ADDR_RX_UART_CONFIG, cfg);
  }
}

hw_u32_t rx_config(unsigned period, unsigned flags, unsigned mode, unsigned input, unsigned phase){
  return ((period << RX_CONFIG_PERIOD_SHIFT) & RX_CONFIG_PERIOD_MASK)
       | ((flags  << RX_CONFIG_FLAGS_SHIFT)  & RX_CONFIG_FLAGS_MASK)
       | ((mode   << RX_CONFIG_MODE_SHIFT)   & RX_CONFIG_MODE_MASK)
       | ((input  << RX_CONFIG_INPUT_SHIFT)  & RX_CONFIG_INPUT_MASK)
       | ((phase  << RX_CONFIG_PHASE_SHIFT)  & RX_CONFIG_PHASE_MASK);
}

hw_u32_t rx_buffer_config(unsigned max_words, unsigned timeout){
  return ((max_words << RX_BUFFER_MAX_WORDS_SHIFT) & RX_BUFFER_MAX_WORDS_MASK)
       | ((timeout   << RX_BUFFER_TIMEOUT_SHIFT)   & RX_BUFFER_TIMEOUT_MASK);
}

void rx_set_config_all(hw_u32_t config){
  axil_write_register(SCOPE_RX+UART_BROADCAST+C_ADDR_RX_UART_CONFIG, config);
}

void rx_set_config(unsigned chan, hw_u32_t config){
  if (chan < 40)
    axil_write_register(SCOPE_RX+(chan<<8)+C_ADDR_RX_UART_CONFIG, config);
}

void rx_set_config_options(unsigned chan, unsigned period, unsigned flags, unsigned mode,
                           unsigned input, unsigned phase){
  rx_set_config(chan, rx_config(period, flags, mode, input, phase));
}

void rx_set_config_options_all(unsigned period, unsigned flags, unsigned mode, unsigned input,
                               unsigned phase){
  rx_set_config_all(rx_config(period, flags, mode, input, phase));
}

hw_val_t rx_get_buffer_config(void){
  return axil_read_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_BUFFER_CONFIG);
}

void rx_set_buffer_config(hw_val_t config){
  axil_write_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_BUFFER_CONFIG, config);
}

void rx_disable_uart(unsigned chan){
  rx_set_input(chan, RX_INPUT_OFF);
}

void rx_enable_uart(unsigned chan){
  rx_set_input(chan, RX_INPUT_PIN);
}

bool rx_uart_is_enabled(unsigned chan){
  if (chan < 40){
    hw_u32_t cfg = axil_read_register(SCOPE_RX+(chan<<8)+C_ADDR_RX_UART_CONFIG);
    if ((cfg & RX_CONFIG_INPUT_MASK) == (RX_INPUT_PIN << RX_CONFIG_INPUT_SHIFT)) {
      return true;
    }
  }
  return false;
}

void toggle_rx_test_patterns(void){
  static int mode = 0;
  mode = (mode + 1) % 3;
  if (mode==0){
    unsigned config = 0x00000000;
    unsigned delay  = 0x00000000;
    printf("INFO:  Test patterns disabled with config: 0x%08x delay 0x%08x\r\n", (unsigned int) config, (unsigned int) delay);
    axil_write_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_PATTERN_CONFIG, config);
    axil_write_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_PATTERN_DELAY, delay);
    axil_write_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_PATTERN_LOWER, 0);
    axil_write_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_PATTERN_UPPER, 0);
  } else if (mode==1) {
    unsigned config = 0x00000101;
    unsigned delay  = 0x00004000;
    printf("INFO:  Test patterns enabled  with config: 0x%08x delay 0x%08x\r\n", (unsigned int) config, (unsigned int) delay);
    axil_write_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_PATTERN_CONFIG, config & 0xFF);  // phase first (pattern.vhd start-up)
    axil_write_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_PATTERN_CONFIG, config);
    axil_write_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_PATTERN_DELAY, delay);
    axil_write_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_PATTERN_LOWER, 0xAAAAAAAA);
    axil_write_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_PATTERN_UPPER, 0xBBBBBBBB);
  } else if (mode==2) {
    unsigned config = 0x00000101;
    unsigned delay  = 0x00004000;
    printf("INFO:  Test patterns enabled  with config: 0x%08x delay 0x%08x\r\n", (unsigned int) config, (unsigned int) delay);
    axil_write_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_PATTERN_CONFIG, config);
    axil_write_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_PATTERN_DELAY, delay);
    axil_write_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_PATTERN_LOWER, 0x01234567);
    axil_write_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_PATTERN_UPPER, 0x89ABDCDF);
  }
}

void toggle_rx_test_pattern_delay(void){
  static int mode = 0;
  mode = (mode + 1) % 6;
  unsigned delay[] = {0x4000, 0x1000, 0x400, 0xc6, 0x42, 0x00};

  printf("INFO:  setting test pattern delay to: 0x%08x\r\n", (unsigned int) delay[mode]);
  axil_write_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_PATTERN_DELAY, delay[mode]);
}


void toggle_rx_buffer_config(void){
  static int mode = 0;
  mode = (mode + 1) % 4;

  unsigned config[] = {0x00000000, 0x00000001, 0x00000010, 0x00010000, 0x00100000};
  printf("INFO: Setting RX buffer config to 0x%08X \r\n", config[mode]);
  rx_set_buffer_config(config[mode]);
}

void toggle_rx_buffer_enables(void){
  static int mode = 0;
  mode = (mode + 1) % 5;

  unsigned config[] = {0x7, 0x0, 0x1, 0x2, 0x4};
  printf("INFO: Setting RX buffer enables to 0x%08X \r\n", config[mode]);
  axil_write_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_BUFFER_ENABLES, config[mode]);
}

void get_rx_uart_status(unsigned chan, rx_uart_status_t * us){
  unsigned cshift = (chan<<8);
  us->status    = axil_read_register(SCOPE_RX+cshift+C_ADDR_RX_UART_STATUS);
  us->config    = axil_read_register(SCOPE_RX+cshift+C_ADDR_RX_UART_CONFIG);
  us->chan      = axil_read_register(SCOPE_RX+cshift+C_ADDR_RX_UART_CHAN);
  us->starts    = axil_read_register(SCOPE_RX+cshift+C_ADDR_RX_UART_STARTS);
  us->beats     = axil_read_register(SCOPE_RX+cshift+C_ADDR_RX_UART_BEATS);
  us->updates   = axil_read_register(SCOPE_RX+cshift+C_ADDR_RX_UART_UPDATES);
  us->lost      = axil_read_register(SCOPE_RX+cshift+C_ADDR_RX_UART_LOST);
  us->frame_err = axil_read_register(SCOPE_RX+cshift+C_ADDR_RX_UART_FRAME_ERR);
  us->noise     = axil_read_register(SCOPE_RX+cshift+C_ADDR_RX_UART_NOISE);
}

void get_tx_uart_status(unsigned chan, tx_uart_status_t * us){
  unsigned cshift = (chan<<8);
  us->status = axil_read_register(SCOPE_TX+cshift+C_ADDR_TX_UART_STATUS);
  us->config = axil_read_register(SCOPE_TX+cshift+C_ADDR_TX_UART_CONFIG);
  us->starts = axil_read_register(SCOPE_TX+cshift+C_ADDR_TX_UART_STARTS);
  us->beats  = axil_read_register(SCOPE_TX+cshift+C_ADDR_TX_UART_BEATS);
}

void read_rx_status(void){
  rx_uart_status_t us;
  for (int i=0; i<40; i++){
    get_rx_uart_status(i, &us);
    printf("%2d: ch: %2d cfg: 0x%08x status: 0x%08x s: %d b: %d u: %d l: %d f:%d n:%d\r\n",i,
           (unsigned int) us.chan, (unsigned int) us.config, (unsigned int) us.status,
           (unsigned int) us.starts, (unsigned int) us.beats, (unsigned int) us.updates,
           (unsigned int) us.lost, (unsigned int) us.frame_err, (unsigned int) us.noise);
  }
  printf("rx buffer status------------0x%x    \r\n", (unsigned int) axil_read_register(SCOPE_RX+0x3F00+C_ADDR_RX_BUFFER_STATUS));
  printf("rx buffer config------------0x%x    \r\n", (unsigned int) axil_read_register(SCOPE_RX+0x3F00+C_ADDR_RX_BUFFER_CONFIG));
  printf("rx buffer enables-----------0x%x    \r\n", (unsigned int) axil_read_register(SCOPE_RX+0x3F00+C_ADDR_RX_BUFFER_ENABLES));
  printf("rx pacman id----------------0x%x    \r\n", (unsigned int) axil_read_register(SCOPE_RX+0x3F00+C_ADDR_RX_PACMAN));
  printf("FIFO count------------------%d      \r\n", (unsigned int) axil_read_register(SCOPE_RX+0x3F00+C_ADDR_RX_FIFO_CNT));
  printf("FIFO max--------------------%d      \r\n", (unsigned int) axil_read_register(SCOPE_RX+0x3F00+C_ADDR_RX_FIFO_MAX));
  printf("heartbeat config------------0x%x    \r\n", (unsigned int) axil_read_register(SCOPE_RX+0x3F00+C_ADDR_RX_HEARTBEAT_CONFIG));
  printf("sync config-----------------0x%x    \r\n", (unsigned int) axil_read_register(SCOPE_RX+0x3F00+C_ADDR_RX_ROLLOVER_CONFIG));
  printf("word_type_lut---------------0x%x    \r\n", (unsigned int) axil_read_register(SCOPE_RX+0x3F00+C_ADDR_RX_WORD_TYPE_LUT));

  printf("header a--------------------0x%x    \r\n", (unsigned int) axil_read_register(SCOPE_RX+0x3F00+C_ADDR_RX_HEADER_A));
  printf("header b--------------------0x%x    \r\n", (unsigned int) axil_read_register(SCOPE_RX+0x3F00+C_ADDR_RX_HEADER_B));
  printf("header c--------------------0x%x    \r\n", (unsigned int) axil_read_register(SCOPE_RX+0x3F00+C_ADDR_RX_HEADER_C));
  printf("header d--------------------0x%x    \r\n", (unsigned int) axil_read_register(SCOPE_RX+0x3F00+C_ADDR_RX_HEADER_D));
  printf("end of packet header--------0x%x    \r\n", (unsigned int) axil_read_register(SCOPE_RX+0x3F00+C_ADDR_RX_EOP_HEADER));

  printf("pattern config--------------0x%x    \r\n", (unsigned int) axil_read_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_PATTERN_CONFIG));
  printf("pattern delay---------------0x%x    \r\n", (unsigned int) axil_read_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_PATTERN_DELAY));
  printf("payload---------------------0x%x %x    \r\n",
	 (unsigned int) axil_read_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_PATTERN_UPPER),
	 (unsigned int) axil_read_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_PATTERN_LOWER));
  printf("pattern status--------------0x%x    \r\n", (unsigned int) axil_read_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_PATTERN_STATUS));
  printf("pattern starts--------------0x%x    \r\n", (unsigned int) axil_read_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_PATTERN_STARTS));
  printf("pattern stops---------------0x%x    \r\n", (unsigned int) axil_read_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_PATTERN_STOPS));
}

void read_rx_look(void){
  hw_u32_t udata[2];
  for (int i=0; i<40; i++){
    axil_write_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_LOOK_SELECT, i);
    udata[0] = axil_read_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_LOOK_LOWER);
    udata[1] = axil_read_register(SCOPE_RX+UART_GLOBAL+C_ADDR_RX_LOOK_UPPER);
    printf("Channel %2d Look:  0x%08x %08x ", i, (unsigned int) udata[1], (unsigned int) udata[0]);
    asic_print_packet_summary(udata);
  }
}

void read_tx_status(void){
  tx_uart_status_t us;
  for (int i=0; i<40; i++){
    get_tx_uart_status(i, &us);
    printf("%2d: config: 0x%08x status: 0x%08x starts: %d beats: %d\r\n",i,
	   (unsigned int) us.config, (unsigned int) us.status, (unsigned int) us.starts, (unsigned int) us.beats);
  }
  printf("rx buffer status----------- 0x%x    \r\n", (unsigned int) axil_read_register(SCOPE_TX+0x3F00+C_ADDR_TX_BUFFER_STATUS));
}

void read_tx_look(void){
  hw_u32_t udata[2];
  for (int i=0; i<40; i++){
    axil_write_register(SCOPE_TX+UART_GLOBAL+C_ADDR_TX_LOOK_SELECT, i);
    udata[0] = axil_read_register(SCOPE_TX+UART_GLOBAL+C_ADDR_TX_LOOK_LOWER);
    udata[1] = axil_read_register(SCOPE_TX+UART_GLOBAL+C_ADDR_TX_LOOK_UPPER);
    printf("Channel %2d Look:  0x%08x %08x ", i, (unsigned int) udata[1], (unsigned int) udata[0]);
    asic_print_packet_summary(udata);
  }
}

uint64_t rxtx_uart_mask(void){
  return G_UART_MASK;
}

void rxtx_set_uart_mask(uint64_t mask){
  G_UART_MASK = mask & ALL_UARTS;
}

uint64_t rxtx_tile_uarts(hw_u32_t tile_mask){
  uint64_t mask = 0;
  for (unsigned t = 0; t < 10; t++)
    if ((tile_mask >> t) & 1) mask |= 0xFULL << (4*t);
  return mask;
}

void zero_rxtx_counts(void){
  axil_write_register(SCOPE_TX+0x3F00+C_ADDR_TX_ZERO_CNTS, 0x0);
  axil_write_register(SCOPE_RX+0x3F00+C_ADDR_RX_ZERO_CNTS, 0x0);
}


//
// Loopback test:
//

// expected word k on a UART:  splitmix64 of (seed, uart, k)
static uint64_t loopback_word(hw_u32_t seed, unsigned uart, unsigned k){
  uint64_t z = ((uint64_t) seed << 32) ^ ((uint64_t) uart << 24) ^ (uint64_t) k;
  z += 0x9E3779B97F4A7C15ULL;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

// time limits:
#define LOOPBACK_DRAIN_US       1000000
#define LOOPBACK_BASE_US        1000000
#define LOOPBACK_PACKET_NS      264000     // 10 x 26.4 us

static hw_u32_t loopback_elapsed_us(void){
  stop_hw_timer();
  return hw_timer_elapsed_us();
}

void rxtx_loopback(uint64_t uart_mask, unsigned packets,
                   bool ber_test, hw_u32_t seed, rxtx_check_t * r){
  const unsigned uarts      = 40;
  const unsigned batch_size = 100;
  const unsigned timeout    = 10000;
  unsigned tx_timeout = timeout;
  unsigned rx_timeout = timeout;
  unsigned tx_k = 0;            // words sent so far on each UART
  unsigned rx_k[40];            // words received so far on each UART
  hw_val_t chan_id[40];         // channel id in the RX word header, per UART

  memset(r, 0, sizeof(*r));
  uart_mask &= ALL_UARTS;
  for (unsigned u=0; u<uarts; u++){
    rx_uart_status_t us;
    get_rx_uart_status(u, &us);
    chan_id[u] = us.chan & 0xFFFF;
    rx_k[u] = 0;
  }

  // discard words left over from earlier activity (they would shift the expected sequence):
  start_hw_timer();
  for (unsigned idle = 0; idle < 100000; idle++){
    if (loopback_elapsed_us() > LOOPBACK_DRAIN_US){
      r->timeout = 1;
      return;
    }
    hw_addr_t nxta = 0;
    while (dma_next_available_rx_bd(&nxta)){
      unsigned xbytes = dma_poll_bd_transferred(nxta);
      if (xbytes >= RX_TRAILER_BYTES)
        r->stale_words += (xbytes - RX_TRAILER_BYTES) / RX_WORD_BYTES;
      dma_add_rx_bd(nxta);
      dma_rx_batch();
      idle = 0;
    }
  }

  const unsigned nmasked = __builtin_popcountll(uart_mask);
  const unsigned words_expected = nmasked * packets;
  const hw_u32_t limit_us = LOOPBACK_BASE_US + (hw_u32_t) (((uint64_t) packets * LOOPBACK_PACKET_NS) / 1000);
  start_hw_timer();
  while (tx_timeout && rx_timeout && (r->words_rcvd < words_expected)){
    if (loopback_elapsed_us() > limit_us){
      r->timeout = 2;
      break;
    }
    if (r->tx_packets < packets) {
      tx_timeout--;
      unsigned batch_count = 0;
      hw_addr_t nxta = 0;
      while((batch_count < batch_size) && (batch_count < (packets - r->tx_packets)) && (dma_next_available_tx_bd(&nxta))){
        hw_ptr_t tx_buf = dma_get_buffer(nxta);
        tx_buf[0] = (hw_u32_t) uart_mask;           // UARTs 1-32
        tx_buf[1] = (hw_u32_t) (uart_mask >> 32);   // UARTs 33-40
        if (ber_test){
          for (unsigned u=0; u<uarts; u++){
            uint64_t w = loopback_word(seed, u, tx_k);
            tx_buf[TX_HEADER_U32_WORDS + 2*u]     = (hw_u32_t) w;
            tx_buf[TX_HEADER_U32_WORDS + 2*u + 1] = (hw_u32_t) (w >> 32);
          }
        }
        tx_k++;
        HW_FLUSH_DCACHE(tx_buf, TX_PACKET_BYTES);
        dma_add_tx_bd(nxta);
        batch_count++;
      }
      if (batch_count > 0){
        tx_timeout = timeout;
        r->tx_packets += batch_count;
        dma_tx_batch();
      }
    }
    {
      rx_timeout--;
      unsigned batch_count = 0;
      hw_addr_t nxta = 0;
      while((batch_count < batch_size) && (dma_next_available_rx_bd(&nxta))){
        unsigned xbytes = dma_poll_bd_transferred(nxta);
        if ((xbytes < RX_TRAILER_BYTES) || ((xbytes - RX_TRAILER_BYTES) % RX_WORD_BYTES)){
          r->bad_packets++;
        } else {
          hw_ptr_t rx_buf = dma_get_buffer(nxta);
          HW_INVALIDATE_DCACHE(rx_buf, xbytes);
          unsigned nwords = (xbytes - RX_TRAILER_BYTES) / RX_WORD_BYTES;
          for (unsigned i=0; i<nwords; i++){
            hw_ptr_t word = rx_buf + i * (RX_WORD_BYTES/4);
            hw_val_t chan = (word[0] >> 16) & 0xFFFF;   // header: [7:0] type, [15:8] pacman id, [31:16] channel
            unsigned u = 0;
            while ((u < uarts) && (chan_id[u] != chan)) u++;
            if (u == uarts){
              r->other_words++;
              continue;
            }
            r->rcvd[u]++;
            if (!((uart_mask >> u) & 1)){
              r->stray_words++;
              continue;
            }
            r->words_rcvd++;
            if (ber_test){
              uint64_t data = ((uint64_t) word[5] << 32) | word[4];   // fragments: header, timestamp, data
              uint64_t diff = data ^ loopback_word(seed, u, rx_k[u]++);
              if (diff){
                r->words_bad++;
                r->bit_errors += __builtin_popcountll(diff);
              }
            }
          }
        }
        batch_count++;
        dma_add_rx_bd(nxta);
      }
      if (batch_count > 0){
        rx_timeout = timeout;
        dma_rx_batch();
      }
    }
  }
  r->elapsed_us = loopback_elapsed_us();
  r->words_sent = nmasked * r->tx_packets;
  r->tx_timeout = (tx_timeout == 0);
}
