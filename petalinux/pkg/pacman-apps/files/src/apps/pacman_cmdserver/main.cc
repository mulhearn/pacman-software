#include <stdlib.h>
#include <stdio.h>
#include <zmq.h>
#include <unistd.h>

#include "tx_buffer.hh"
#include "pacman.hh"
#include "pacman_message.hh"
#include "pacman_highlevel_interface.hh"

#define REP_SOCKET_BINDING "tcp://*:5555"
#define ECHO_SOCKET_BINDING "tcp://*:5554"
#define TX_WAIT_US          100000   // Longest wait for TX buffer space, once per request
#define TX_DRAIN_POLL_MS    1        // Request poll while the TX buffer still holds words
#define ERR_CODE            0xEEEE   // ERR word code:  message rejected

static volatile bool msg_ready = true;
static bool tx_stalled = false;  // TX buffer space timed out in this request:  refuse the rest

// ZMQ free callback
static void clear_msg(void*, void*) {
  msg_ready = true;
}

void handle_ping(pacman_word_t * req_word, pacman_word_t * rep_word){
  printf("INFO:  handling ping request\n");
  write_word_ping(rep_word);
}

void handle_read(pacman_word_t * req_word, pacman_word_t * rep_word){
  uint32_t addr = req_word->read.addr;
  printf("INFO:  handling read request for address 0x%08x\n", addr);
  uint32_t value = pacman_read(addr);
  uint8_t pacman_id = get_pacman_id();
  write_word_read(rep_word, pacman_id, addr, value);
}

void handle_write(pacman_word_t * req_word, pacman_word_t * rep_word){
  uint32_t addr  = req_word->write.addr;
  uint32_t value = req_word->write.value;
  printf("INFO:  handling write request for address 0x%08x and value 0x%08x\n", addr, value);
  pacman_write(addr, value);
  uint8_t pacman_id = get_pacman_id();
  write_word_write(rep_word, pacman_id, addr, value);
}

void handle_data(pacman_word_t * req_word, pacman_word_t * rep_word){
  uint16_t chan       = req_word->data.chan;
  uint32_t payload_lo = req_word->data.payload_lo;
  uint32_t payload_hi = req_word->data.payload_hi;

  //printf("INFO:  handling TX request for chan %5d payload 0x%08x%08x \n", chan, payload_hi, payload_lo);
  uint8_t pacman_id = get_pacman_id();
  if ((chan > 0) && (chan <= TX_BUFFER_CHAN)) {
    // Wait for space, moving words to the DMA meanwhile
    unsigned waited = 0;
    while ((tx_buffer_free(chan-1) == 0) && !tx_stalled) {
      pacman_poll_tx();
      usleep(10);
      waited += 10;
      if (waited >= TX_WAIT_US) {
        printf("ERROR:  no TX space on channel %d;  rest of request refused\n", chan);
        tx_stalled = true;
      }
    }
    if (tx_buffer_in(chan-1, payload_hi, payload_lo)) {
      write_word_data(rep_word, pacman_id, chan, 0, 0, payload_hi, payload_lo);
      return;
    }
  } else {
    printf("ERROR:  TX channel %d out of range, refused\n", chan);
  }
  write_word_err(rep_word, pacman_id, 0, 0, ERR_CODE);
}

// tx_pending:  true while the TX buffer holds words not yet given to the DMA
static bool tx_pending(void){
  for (unsigned c=0; c<TX_BUFFER_CHAN; c++)
    if (tx_buffer_count(c) > 0)
      return true;
  return false;
}

// send_reply:  send msg on sock, waiting until ZMQ has released it
static void send_reply(void* sock, pacman_msg_t* msg){
  zmq_msg_t zmq_msg;
  msg_ready = false;
  zmq_msg_init_data(&zmq_msg, msg, total_message_size(msg), clear_msg, NULL);
  zmq_msg_send(&zmq_msg, sock, 0);
  while (!msg_ready) { usleep(1000); }
  zmq_msg_close(&zmq_msg);
}

void handle_unknown(pacman_word_t * req_word, pacman_word_t * rep_word){
  printf("ERROR:  invalid request received, replying with error \n");
  write_word_err(rep_word, 0, 0, 0, ERR_CODE);
}


int main(int argc, char* argv[]){
  printf("INFO:  Starting pacman_cmdserver...\n");

  printf("INFO:  Initializing TX buffer.\n");
  tx_buffer_init();

  printf("INFO:  Initializing ZMQ sockets.\n");
  void* ctx = zmq_ctx_new();
  void* rep_socket = zmq_socket(ctx, ZMQ_REP);
  void* echo_socket = zmq_socket(ctx, ZMQ_PUB);

  int timeo = 7000;
  zmq_setsockopt(rep_socket, ZMQ_SNDTIMEO, &timeo, sizeof(timeo));
  zmq_setsockopt(echo_socket, ZMQ_SNDTIMEO, &timeo, sizeof(timeo));

  if (zmq_bind(rep_socket, REP_SOCKET_BINDING) != 0) {
    printf("ERROR:  Failed to bind socket (%s)!\n", REP_SOCKET_BINDING);
    return 1;
  }
  printf("INFO:  ZMQ REP socket created successfully.\n");

  if (zmq_bind(echo_socket, ECHO_SOCKET_BINDING) != 0) {
    printf("ERROR:  Failed to bind socket (%s)!\n", ECHO_SOCKET_BINDING);
    return 1;
  }
  printf("INFO:  ZMQ PUB socket (for echo) created successfully.\n");

  printf("INFO:  Initializing PACMAN hardware drivers\n");
  if (pacman_init(1) == EXIT_FAILURE){
    printf("ERROR:  Failed to initialize PACMAN hardware drivers\n");
    return 1;
  }
  if (pacman_init_tx(1) == EXIT_FAILURE){
    printf("ERROR:  Failed to initialize PACMAN TX driver\n");
    return 1;
  }
  printf("INFO:  PACMAN HW driver initialization was successful.\n");

  // ZMQ messages:
  static zmq_msg_t zmq_req;
  static zmq_msg_t zmq_echo;
  static zmq_msg_t zmq_rep;


  // pacman messages:
  static pacman_msg_t rep_msg;
  static pacman_msg_t req_msg;

  while (1) {
    pacman_poll_tx();

    // Drain the TX buffer between requests
    if (tx_pending()) {
      zmq_pollitem_t item = { rep_socket, 0, ZMQ_POLLIN, 0 };
      if (zmq_poll(&item, 1, TX_DRAIN_POLL_MS) <= 0)
        continue;
    }

    //printf("INFO:  Waiting for new message...\n");
    zmq_msg_init(&zmq_req);
    int rc = zmq_msg_recv(&zmq_req, rep_socket, 0);
    if (rc < 0) {
      zmq_msg_close(&zmq_req);
      return 0;  // exit on error
    }
    size_t nbytes_recv = rc;
    //printf("INFO:  Message received with %zu bytes\n", nbytes_recv);


    // A request too short, too large, or not the size its header gives:  one ERR word in reply
    if ((nbytes_recv < HEADER_BYTES) || (nbytes_recv > sizeof(req_msg))
        || (nbytes_recv != HEADER_BYTES + ((pacman_header_t*) zmq_msg_data(&zmq_req))->n_bytes)) {
      printf("ERROR:  malformed request of %zu bytes, refused\n", nbytes_recv);
      zmq_msg_close(&zmq_req);
      write_header_rep(&rep_msg.header, WORD_BYTES);
      write_word_err(&rep_msg.words[0], get_pacman_id(), 0, 0, ERR_CODE);
      send_reply(rep_socket, &rep_msg);
      continue;
    }
    memcpy(&req_msg, zmq_msg_data(&zmq_req), nbytes_recv);
    //print_msg(&req_msg, "INFO:  ");

    if (is_string_msg(&req_msg)){
      const char* req_str = reinterpret_cast<const char*>(req_msg.raw);
      size_t req_len = req_msg.header.n_bytes;
      size_t rep_len = pacman_highlevel_command
        (req_str, req_len, reinterpret_cast<char*>(rep_msg.raw), sizeof(rep_msg.raw));

      // fill header
      memset(&rep_msg.header, 0, sizeof(rep_msg.header));
      rep_msg.header.msg_type      = MSG_TYPE_STRING;
      rep_msg.header.pacman        = 0;
      rep_msg.header.version_major = MSG_VERSION_MAJOR;
      rep_msg.header.version_minor = MSG_VERSION_MINOR;
      rep_msg.header.n_bytes       = rep_len;
      rep_msg.header.timestamp_lo  = req_msg.header.timestamp_lo;
      rep_msg.header.timestamp_hi  = req_msg.header.timestamp_hi;

      // send reply:
      msg_ready = false;
      static zmq_msg_t zmq_rep;
      zmq_msg_init_data(&zmq_rep, &rep_msg, total_message_size(&req_msg), clear_msg, NULL);
      zmq_msg_send(&zmq_rep, rep_socket, 0);
      while (!msg_ready) { usleep(1000); }
      zmq_msg_close(&zmq_rep);

      //close the request message
      zmq_msg_close(&zmq_req);

      continue;
    }

    // Start a reply message with the same number of words as the request:
    uint32_t n_bytes = req_msg.header.n_bytes;
    uint32_t n_words = n_bytes/WORD_BYTES;
    write_header_rep(&rep_msg.header, n_bytes, req_msg.header.timestamp_hi, req_msg.header.timestamp_lo);

    // Handle each word in the request one at a time:
    tx_stalled = false;
    for (uint32_t i=0; i<n_words; i++){
      char wt = req_msg.words[i].raw[0];
      switch (wt) {
      case WORD_TYPE_PING:
        handle_ping(&req_msg.words[i], &rep_msg.words[i]);
        break;
      case WORD_TYPE_READ:
        handle_read(&req_msg.words[i], &rep_msg.words[i]);
        break;
      case WORD_TYPE_WRITE:
        handle_write(&req_msg.words[i], &rep_msg.words[i]);
        break;
      case WORD_TYPE_DATA:
        handle_data(&req_msg.words[i], &rep_msg.words[i]);
        break;
      default:
        handle_unknown(&req_msg.words[i], &rep_msg.words[i]);
        break;
      }
    }

    // echo the request:
    //printf("INFO:  echoing request\n");
    msg_ready = false;
    zmq_msg_init_data(&zmq_echo, &req_msg, total_message_size(&req_msg), clear_msg, NULL);
    zmq_msg_send(&zmq_echo, echo_socket, 0);
    while (!msg_ready) { usleep(1000); }
    zmq_msg_close(&zmq_echo);

    // close the request message once we have finished handling the requests
    zmq_msg_close(&zmq_req);

    // echo our reply:
    //printf("INFO:  echoing reply\n");
    msg_ready = false;
    zmq_msg_init_data(&zmq_echo, &rep_msg, total_message_size(&rep_msg), clear_msg, NULL);
    zmq_msg_send(&zmq_echo, echo_socket, 0);
    while (!msg_ready) { usleep(1000); }
    zmq_msg_close(&zmq_echo);

    // sending our reply message:
    //printf("INFO:  sending reply message\n");
    msg_ready = false;
    zmq_msg_init_data(&zmq_rep, &rep_msg, total_message_size(&rep_msg), clear_msg, NULL);
    zmq_msg_send(&zmq_rep, rep_socket, 0);
    while (!msg_ready) { usleep(1000); }
    zmq_msg_close(&zmq_rep);
  }

  return 0;
}
