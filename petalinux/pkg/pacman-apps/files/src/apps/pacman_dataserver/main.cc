#include <atomic>
#include <chrono>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <zmq.h>
#include <sys/time.h>

#include "pacman.hh"
#include "pacman_message.hh"

// -----------------------------
// Configuration constants
// -----------------------------
#define PUB_SOCKET_BINDING "tcp://*:5556"

// Fire and forget setting:
const int PUB_HWM     = 1;    // high-water mark
const int PUB_LINGER  = 0;    // drop unsent messages at close
const int PUB_SNDTIMEO = 0;   // send timeout in ms

//#define MIN_BATCH 16000       // min words to trigger send
//#define BATCH_TIMEOUT_MS 1    // flush timeout in milliseconds

// Two messages: one filling, one in flight
const int MAX_PAYLOAD = 64000;  // max words in a message payload
static char msg_buffer_a[HEADER_BYTES + MAX_PAYLOAD*WORD_BYTES];
static char msg_buffer_b[HEADER_BYTES + MAX_PAYLOAD*WORD_BYTES];

unsigned fill_index = 0;

// ZMQ free callback
std::atomic<bool> msg_ready = true;
void clear_msg(void*, void*) { msg_ready = true; }

int main(int argc, char* argv[]) {
  printf("INFO:  Starting pacman-dataserver...\n");

  printf("INFO:  Initializing ZMQ socket.\n");
  void* ctx = zmq_ctx_new();
  if (!ctx) { perror("ERROR: zmq_ctx_new failed"); return EXIT_FAILURE; }

  void* pub_socket = zmq_socket(ctx, ZMQ_PUB);
  if (!pub_socket) { perror("ERROR: zmq_socket failed"); zmq_ctx_term(ctx); return EXIT_FAILURE; }

  if (zmq_setsockopt(pub_socket, ZMQ_SNDHWM, &PUB_HWM, sizeof(PUB_HWM)) != 0 ||
      zmq_setsockopt(pub_socket, ZMQ_LINGER, &PUB_LINGER, sizeof(PUB_LINGER)) != 0 ||
      zmq_setsockopt(pub_socket, ZMQ_SNDTIMEO, &PUB_SNDTIMEO, sizeof(PUB_SNDTIMEO)) != 0) {
    perror("ERROR: zmq_setsockopt failed");
    zmq_close(pub_socket);
    zmq_ctx_term(ctx);
    return EXIT_FAILURE;
  }

  if (zmq_bind(pub_socket, PUB_SOCKET_BINDING) != 0) {
    printf("ERROR: Failed to bind socket at %s\n", PUB_SOCKET_BINDING);
    printf("ERROR: (Perhaps pacman_dataserver is already running?)\n");
    zmq_close(pub_socket);
    zmq_ctx_term(ctx);
    return EXIT_FAILURE;
  }
  printf("INFO: ZMQ socket bound successfully at %s\n", PUB_SOCKET_BINDING);

  printf("INFO: Initializing PACMAN RX driver.\n");
  if (pacman_init_rx(1,1) == EXIT_FAILURE) {
    printf("ERROR: Failed to initialize PACMAN RX driver\n");
    zmq_close(pub_socket);
    zmq_ctx_term(ctx);
    return EXIT_FAILURE;
  }
  printf("INFO: PACMAN RX driver initialization was successful.\n");

  printf("INFO: Entering RX loop.\n");

  // -----------------------------
  // Preallocated message buffer
  // -----------------------------
  zmq_msg_t pub_msg;

  // -----------------------------
  // Periodic reporting
  // -----------------------------
  static const int REPORT_INTERVAL_MS = 10000;
  struct timeval report_start, window_start, now_tv;
  gettimeofday(&report_start, NULL);
  gettimeofday(&window_start, NULL);

  uint64_t rcvd_packets_window   = 0;
  uint64_t rcvd_packets_total    = 0;
  uint64_t sent_packets_window   = 0;
  uint64_t sent_packets_total    = 0;
  uint64_t sent_messages_window  = 0;
  uint64_t sent_messages_total   = 0;

  uint64_t idle_time_window      = 0;
  uint64_t idle_time_total       = 0;
  uint64_t stall_time_window     = 0;
  uint64_t stall_time_total      = 0;


  char * fill_buffer = msg_buffer_a;
  while (1) {
    gettimeofday(&now_tv, NULL);
    double window_ms = 1000.0*(now_tv.tv_sec - window_start.tv_sec) + (now_tv.tv_usec - window_start.tv_usec)/1000.0;
    if (window_ms >= REPORT_INTERVAL_MS) {
      double cumulative_ms = 1000.0*(now_tv.tv_sec - report_start.tv_sec) + (now_tv.tv_usec - report_start.tv_usec)/1000.0;
      rcvd_packets_window = pacman_packet_count_rx(1);
      rcvd_packets_total += rcvd_packets_window;

      printf("INFO:  --- Report @ cumulative: %.1f ms  current interval: %.1f ms ---\n", cumulative_ms, window_ms);
      printf(" max rx pending:  %u \n",  pacman_max_rx_pending());
      printf("idle (us):           %12llu (total:  %12llu) ---> frac: %12.4f\n",
             idle_time_window, idle_time_total, idle_time_window/window_ms/1000);
      printf("message stall (us):  %12llu (total:  %12llu) ---> frac: %12.4f\n",
             stall_time_window, stall_time_total, stall_time_window/window_ms/1000);
      printf("received packets:    %12llu (total:  %12llu) ---> rate: %12.4f \n",
             rcvd_packets_window, rcvd_packets_total, rcvd_packets_window/window_ms);
      printf("sent packets:        %12llu (total:  %12llu) ---> rate: %12.4f \n",
             sent_packets_window, sent_packets_total, sent_packets_window/window_ms);
      printf("sent messages:       %12llu (total:  %12llu) \n", sent_messages_window, sent_messages_total);
      stall_time_window     = 0;
      idle_time_window     = 0;
      rcvd_packets_window  = 0;
      sent_packets_window  = 0;
      sent_messages_window = 0;
      gettimeofday(&window_start, NULL);
    }

    pacman_word_t* payload = (pacman_word_t*)(fill_buffer + HEADER_BYTES);
    pacman_poll_rx(payload, &fill_index, MAX_PAYLOAD);

    if (fill_index == 0) {
      static const int IDLE_US = 100;
      idle_time_window += IDLE_US;
      idle_time_total  += IDLE_US;
      std::this_thread::sleep_for(std::chrono::microseconds(IDLE_US));
    }

    static unsigned msg_stall_count = 0;
    if (!msg_ready) {
      msg_stall_count++;
      if (msg_stall_count > 10000) {  // ~1 second at 100us sleep
	printf("ERROR: timeout waiting on msg_ready.\n");
	abort();
      }
      static const int STALL_US = 100;
      stall_time_window += STALL_US;
      stall_time_total  += STALL_US;
      std::this_thread::sleep_for(std::chrono::microseconds(STALL_US));
      continue;
    }
    msg_stall_count = 0;

    if (fill_index > 0){
      char * msg_buffer = fill_buffer;
      unsigned msg_packets = fill_index;

      // setup the new fill buffer:
      if (fill_buffer == msg_buffer_a){
	fill_buffer = msg_buffer_b;
      } else {
	fill_buffer = msg_buffer_a;
      }
      fill_index = 0;

      write_header_data((pacman_header_t*) msg_buffer, msg_packets * WORD_BYTES);

      msg_ready = false;

      bool init_success = (zmq_msg_init_data(&pub_msg, msg_buffer, HEADER_BYTES + msg_packets * WORD_BYTES, clear_msg, NULL) == 0);
      bool send_success = init_success && (zmq_msg_send(&pub_msg, pub_socket, 0) >= 0);

      if (send_success) {
	sent_messages_total++;
	sent_messages_window++;
	sent_packets_total += msg_packets;
	sent_packets_window += msg_packets;
      } else {
	perror("ERROR: initializing or sending ZMQ messages has failed unexpectedly.");
	abort();
	//recovery requires:
	//msg_ready = true;
	//if (init_success) zmq_msg_close(&pub_msg);
	//continue;
      }
    }
  }
  zmq_close(pub_socket);
  zmq_ctx_term(ctx);
  return EXIT_SUCCESS;
}
