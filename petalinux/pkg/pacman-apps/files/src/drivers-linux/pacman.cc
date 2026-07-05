#ifndef SIMULATED_PACMAN

#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

#include "version.hh"
#include "addr_conf.hh"
#include "pacman.hh"
#include "tx_buffer.hh"

// new common drivers:
#include "hw_access.h"
#include "dma.h"
#include "rxtx.h"
#include "iic.h"

static unsigned count_rx = 0;
static unsigned max_rx_pending = 0;

unsigned pacman_packet_count_rx(int clear){
  unsigned tmp = count_rx;
  if (clear)
    count_rx = 0;
  return tmp;
}

unsigned pacman_max_rx_pending(int clear){
  unsigned tmp = max_rx_pending;
  if (clear)
    max_rx_pending = 0;
  return tmp;
}

volatile uint32_t * G_PACMAN_AXIL = NULL;


int pacman_init(int verbose){
  // initialize axi-lite
  if (verbose){
    printf("INFO:  Initializing PACMAN AXI-Lite interface.\n");
  }

  axil_platform_init();
  timing_platform_init();

  unsigned fwmajor = axil_read_register(0XFF10);
  unsigned fwminor = axil_read_register(0XFF14);
  unsigned fwpatch = axil_read_register(0XFF18);

  if (verbose){
    printf("INFO:  Running pacman-server version %d.%d.%d\n",
	   PACMAN_SERVER_MAJOR_VERSION, PACMAN_SERVER_MINOR_VERSION, PACMAN_SERVER_PATCH_VERSION);
    printf("INFO:  Running pacman firmware version %d.%d.%d\n", fwmajor, fwminor, fwpatch);
  }

  // initialize I2C
  if (verbose){
    printf("INFO:  Initializing PACMAN I2C interface for PACMAN 1v5\n");
  }
  iic_platform_init();
  iic_set_hw_version(1,5,0);

  // DEFAULT parameters
  if (verbose){
    printf("INFO:  Setting minimum DMA packet size to 0x3900 and packet timeout to 1 ms\n");
    printf("INFO:  Enabling Trigger, Sync, and Heartbeat words in the RX unit.\n");
  }
  axil_write_register(0x7FB4, 0x026000C4);
  axil_write_register(0x7FB8, 0x0003);

  if (verbose){
    printf("INFO:  Configuring RX UARTs. \n");
  }
  axil_write_register(0x7B04, 0x00000101);

  if (verbose){
    printf("INFO:  Limiting TX bandwidth\n");
  }
  axil_write_register(0x3B04, 0x07BC0006);

  //polarity configuration: 0xE108
  // 0x0HHHGGGI H=H output mask(10 bits) G=G output mask (10 bits) I = input mask (2 bits)
  axil_write_register(0xE108, 0x03FF3FF0);

  //destination configurations:
  // 0x0MMMDDDO M=tile enables, D=duration O=output enables (1 = G, 2 = H, 4 = T)
  //LEMO A destination configuration:  This is a SYNC pulse, H+T, duration 5
  axil_write_register(0xE110, 0x03FF0056);
  //LEMO B destination configuration:  This is a SYNC pulse, H+T, duration 5
  axil_write_register(0xE114, 0x03FF0056);

  //POKE C destination configuration:  This is an INTERNAL_RESET pulse, G, duration 24
  axil_write_register(0xE118, 0x03FF0181);
  //POKE D destination configuration:  This is a FULL_RESET pulse, G, duration 1024
  axil_write_register(0xE11C, 0x03FF4001);

  //POKE D destination configuration:  This is a SYNC pulse, H+T, duration 2
  //axil_write_register(0xE11C, 0x03FF0056);

  //Request ATC configuration update:
  axil_write_register(0xE100, 0x0);


  return EXIT_SUCCESS;
}

int pacman_init_tx(int verbose, int skip_reset){
  init_rxtx();

  // reset of S2MM halts MM2S in DMA SG mode, so cmdserver (TX) handles both resets:
  dma_reset_tx(DMA_TIMEOUT);
  dma_reset_rx(DMA_TIMEOUT);

  init_tx_descriptor_ring_mode(512);
  return EXIT_SUCCESS;
}

int pacman_init_rx(int verbose, int skip_reset){
  init_rxtx();

  printf("INFO:  Waiting for TX server to initialize first.\r\n");
  // give pacman_cmdserver a head start:
  usleep(100000);

  // confirm it is running:
  unsigned timeout = dma_wait_tx_run(100000);
  if (timeout == 0){
    printf("ERROR:  RX is not running.  Due to single DMA core, must initialize TX (pacman_cmdserver) before starting RX (pacman_dataserver\r\n");
    exit(0);
  }

  printf("INFO:  Initializing RX descriptor ring.\r\n");
  init_rx_descriptor_ring_mode(512);
  return EXIT_SUCCESS;
}

void pacman_poll_rx(pacman_word_t * buffer, unsigned * index, unsigned max){
  const unsigned rx_trailer_bytes = 24; // current firmware, each DMA RX packet has a 192-bit trailer
  hw_addr_t nxta;

  // track maximum of rx buffers pending:
  unsigned pending = rx_pending();
  if (pending > max_rx_pending)
    max_rx_pending = pending;

  while (1) {
    // check that there is enough space for a maximially filled DMA buffer:
    const unsigned max_words_per_bd = (RX_BUF_BYTES - rx_trailer_bytes) / 24;
    if ((max - *index) < max_words_per_bd)
      return;

    if (!dma_next_available_rx_bd(&nxta))
      return;

    unsigned xbytes = dma_poll_bd_transferred(nxta);
    hw_ptr_t rx_buf = dma_get_buffer(nxta);

    assert(xbytes > rx_trailer_bytes);
    assert((xbytes - rx_trailer_bytes) % 24 == 0);
    unsigned packets = (xbytes - rx_trailer_bytes) / 24;
    count_rx += packets;
    memcpy(&buffer[*index], (void*)rx_buf, xbytes - rx_trailer_bytes);
    *index += packets;
    dma_add_rx_bd(nxta);
    dma_rx_batch();
  }
}

int pacman_poll_tx(){
  const unsigned frags = TX_BUFFER_BYTES/4; // number of u32 fragments per buffer
  static uint32_t output[frags];

  const unsigned batch_size = 100;
  unsigned batch_count = 0;
  hw_addr_t nxta;

  while((batch_count < batch_size) && dma_next_available_tx_bd(&nxta)){
    if (tx_buffer_out(output)==1){
      tx_buffer_print_output(output);
      hw_ptr_t tx_buf = dma_get_buffer(nxta);
      for (unsigned i=0; i<frags; i++)
	tx_buf[i] = output[i];
      dma_add_tx_bd(nxta);
      batch_count++;
    } else {
      break;
    }
  }
  if (batch_count > 0){
    printf("INFO:  sending %d TX buffers \r\n", batch_count);
    dma_tx_batch();
  }
  return EXIT_SUCCESS;
}


#endif
