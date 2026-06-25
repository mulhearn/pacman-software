#ifdef SIMULATED_PACMAN

#include <stdlib.h>
#include <stdio.h>
#include <cstring>
#include <unistd.h>
#include <zmq.h>
#include <cassert>

#include "pacman.hh"
#include "pacman_vspace.hh"
#include "pacman_highlevel_interface.hh"
#include "tx_buffer.hh"

#define PUB_SOCKET_BINDING "tcp://*:5557"
#define SUB_SOCKET_BINDING "tcp://localhost:5557"

static void * pub = NULL;
static void * sub = NULL;

#define MAX_PACKETS 1000
#define BYTES_PER_PACKET 24

static uint32_t buffer[MAX_PACKETS*BYTES_PER_PACKET/4];

static volatile bool msg_done = true;

static unsigned count_rx = 0;
unsigned pacman_packet_count_rx(int clear){
  unsigned tmp = count_rx;
  if (clear)
    count_rx = 0;
  return tmp;
}

unsigned pacman_max_rx_pending(int clear){
  return 0;
}

static void clear_msg(void*, void*) {
  msg_done = true;
}

static void send_msg(int count) {
  //printf("DEBUG:  sending message with count %d\n", count);
  zmq_msg_t msg;
  zmq_msg_init_data(&msg, buffer, BYTES_PER_PACKET*count, clear_msg, NULL);
  zmq_msg_send(&msg, pub, 0);
  zmq_msg_close(&msg);
  //printf("DEBUG:  done sending message\n");
}

int pacman_init(int verbose){
  printf("WARNING:  *** Using simulated hardware driver.  ***\n");
  return EXIT_SUCCESS;
}

int pacman_init_rx(int verbose, int skip_reset){
  printf("WARNING:  *** Using simulated hardware driver.  ***\n");

  printf("INFO: opening a loopback ZMQ socket:  \n");
  void* ctx = zmq_ctx_new();
  sub = zmq_socket(ctx, ZMQ_SUB);
  int linger = 0;
  zmq_setsockopt(sub, ZMQ_LINGER, &linger, sizeof(linger));
  int timeo = 0;
  zmq_setsockopt(sub, ZMQ_RCVTIMEO, &timeo, sizeof(timeo));
  timeo = 1000;
  zmq_setsockopt(sub, ZMQ_SNDTIMEO, &timeo, sizeof(timeo));
  zmq_setsockopt(sub, ZMQ_SUBSCRIBE, "", 0);

  if (zmq_connect(sub, SUB_SOCKET_BINDING) !=0 ) {
    printf("ERROR:  Failed to connect socket (%s)!\n", SUB_SOCKET_BINDING);
    return 1;
  }
  printf("INFO:  ZMQ socket created successfully.\n");

  return EXIT_SUCCESS;
}

int pacman_init_tx(int verbose, int skip_reset){
  printf("WARNING:  *** Using simulated hardware driver.  ***\n");

  printf("INFO: opening a loopback ZMQ socket:  \n");
  void* ctx = zmq_ctx_new();
  pub = zmq_socket(ctx, ZMQ_PUB);
  int hwm = 100;
  zmq_setsockopt(pub, ZMQ_SNDHWM, &hwm, sizeof(hwm));
  int linger = 0;
  zmq_setsockopt(pub, ZMQ_LINGER, &linger, sizeof(linger));
  int timeo = 0;
  zmq_setsockopt(pub, ZMQ_SNDTIMEO, &timeo, sizeof(timeo));
  if (zmq_bind(pub, PUB_SOCKET_BINDING) !=0 ) {
    printf("ERROR:  Failed to bind socket (%s)!\n", PUB_SOCKET_BINDING);
    return 1;
  }
  printf("INFO:  ZMQ socket created successfully.\n");


  return EXIT_SUCCESS;
}

void pacman_poll_rx(pacman_word_t * buffer, unsigned * index, unsigned max){
  int rc;

  // keep reading messages until we timeout waiting:
  while(1){

    // check that we have enough space for an entire message:
    if ((max - *index) < MAX_PACKETS)
      return;


    zmq_msg_t msg;
    rc = zmq_msg_init(&msg);
    assert(rc==0);
    rc = zmq_msg_recv(&msg, sub, 0);
    int size = zmq_msg_size(&msg);

    if (size <= 0){
      zmq_msg_close(&msg);
      return;
    }

    if (size % BYTES_PER_PACKET) {
      perror("ERROR: invalid message size received.\n");
      abort();
    }

    int count = size / BYTES_PER_PACKET;
    count_rx += count;

    //printf("DEBUG:  received %d words in buffer of size %d \n", count, size);
    memcpy(&buffer[*index],zmq_msg_data(&msg), size);
    *index += count;

    zmq_msg_close(&msg);

  }
}


int pacman_poll_tx(){
  uint32_t src[TX_BUFFER_BYTES/4];
  int count = 0;
  uint8_t pacman_id = get_pacman_id();
  //printf("DEBUG:  Checking TX buffer...\n");
  while (tx_buffer_out(src)){
    //printf("DEBUG:  Filling loopback buffer...\n");
    for (int i=0; i<TX_BUFFER_CHAN; i++){
      if (((src[i/32]>>(i%32))&1)==1) {
	if (count == 0){
	  // start of new message, wait for previous message to complete:
	  while (! msg_done){ usleep(100); }
	  msg_done = false;
	}
	//printf("DEBUG:  count: %d chan: %3d tx_data: 0x%08x%08x\n", count, i, src[2+2*i+1], src[2+2*i+0]);
	buffer[6*count + 0]=0x0044+(pacman_id<<8)+((i+1)<<16);
	buffer[6*count + 1]=0;
	buffer[6*count + 2]=0x44d50000;  // DEC:  12340000 0000 0000
	buffer[6*count + 3]=0x11200c76;
	buffer[6*count + 4]=src[2+2*i+0];
	buffer[6*count + 5]=src[2+2*i+1];
	count++;
	if (count == MAX_PACKETS){
	  // send message:
	  send_msg(count);
	  count = 0;
	}
      }
    }
  }
  if (count > 0){
    // send message:
    send_msg(count);
    count = 0;
  }

  return EXIT_SUCCESS;
}

#endif
