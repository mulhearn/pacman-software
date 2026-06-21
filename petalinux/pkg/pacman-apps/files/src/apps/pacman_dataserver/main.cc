#include <atomic>
#include <chrono>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <zmq.h>
#include <sys/time.h>

#include "rx_buffer.hh"
#include "pacman.hh"
#include "pacman_message.hh"

// -----------------------------
// Configuration constants
// -----------------------------
#define PUB_SOCKET_BINDING "tcp://*:5556"
//#define MAX_BATCH 16000       // max words per message
//#define MIN_BATCH 4000        // min words to trigger send
#define MAX_BATCH 64000        // max words per message
#define MIN_BATCH 16000        // min words to trigger send

#define BATCH_TIMEOUT_MS 1    // flush timeout in milliseconds

const int PUB_HWM     = 100;    // high-water mark
const int PUB_LINGER  = 0;      // drop unsent messages at close
const int PUB_SNDTIMEO = 1000;  // send timeout in ms

std::atomic<bool> msg_ready = true;

// ZMQ free callback
void clear_msg(void*, void*) {
    msg_ready = true;
}

int main(int argc, char* argv[]) {
    printf("INFO:  Starting pacman-dataserver...\n");
    printf("INFO:  Minimum words per packet:  %d\n", MIN_BATCH);
    printf("INFO:  Maximum words per packet:  %d\n", MAX_BATCH);
    printf("INFO:  Initializing RX buffer.\n");
    rx_buffer_init();

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
    static char msg_buffer[HEADER_BYTES + MAX_BATCH*WORD_BYTES];
    zmq_msg_t pub_msg;

    auto batch_start_time = std::chrono::steady_clock::now();

    // -----------------------------
    // Periodic reporting
    // -----------------------------
#define REPORT_INTERVAL_MS 10000
    struct timeval report_start, window_start, now_tv;
    gettimeofday(&report_start, NULL);
    gettimeofday(&window_start, NULL);

    uint64_t total_packets_rx  = 0;
    uint64_t total_errors      = 0;
    uint64_t total_proc        = 0;
    uint64_t total_messages    = 0;
    uint64_t window_packets_rx = 0;
    uint64_t window_proc       = 0;
    uint64_t window_messages   = 0;

    while (1) {
      gettimeofday(&now_tv, NULL);
      double window_ms = 1000.0*(now_tv.tv_sec - window_start.tv_sec) + (now_tv.tv_usec - window_start.tv_usec)/1000.0;
      if (window_ms >= REPORT_INTERVAL_MS) {
	double cumulative_ms = 1000.0*(now_tv.tv_sec - report_start.tv_sec) + (now_tv.tv_usec - report_start.tv_usec)/1000.0;
	window_packets_rx = pacman_packet_count_rx(1);
	total_packets_rx += window_packets_rx;


	printf("INFO:  --- Report @ cumulative: %.1f ms  current interval: %.1f ms ---\n", cumulative_ms, window_ms);
	printf(" errors:      (total:  %12llu) \n",  total_errors);
	printf(" max rx pending:  %u \n",  pacman_max_rx_pending());
	printf(" dma packets: %12llu (total:  %12llu) ---> rate: %12.4f \n",
	       window_packets_rx, total_packets_rx, window_packets_rx/window_ms);
	printf(" processed:   %12llu (total:  %12llu) ---> rate: %12.4f \n",
	       window_proc, total_proc, window_proc/window_ms);
	printf(" messages:    %12lu (total:  %12lu) \n", window_messages, total_messages);
	window_messages = 0;
	window_proc = 0;
	gettimeofday(&window_start, NULL);
      }

      // Poll RX buffer continuously
      pacman_poll_rx();
      std::this_thread::sleep_for(std::chrono::microseconds(200));

      uint32_t available = rx_buffer_count();

      // Accumulate words in batch counter
      if (available > MAX_BATCH) available = MAX_BATCH;

      auto now = std::chrono::steady_clock::now();
      auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - batch_start_time).count();

      int processed = 0;
      // Check if we should flush
      if (msg_ready && ((available >= MIN_BATCH) || ((available > 0) && (elapsed_ms >= BATCH_TIMEOUT_MS)))){

	// Copy words from RX buffer into message buffer
	for (uint32_t i = 0; i < available; i++) {
	  pacman_word_t* w = (pacman_word_t*)(msg_buffer + HEADER_BYTES + i*WORD_BYTES);
	  if (rx_buffer_out((uint32_t*)w)) {
	    processed++;
	  } else {
	    perror("ERROR: rx_buffer_out failed unexpectedly");
	    break;
	  }
	}

	total_proc += processed;
	window_proc += processed;

	/*
	// Initialize header
	write_header_data((pacman_header_t*)msg_buffer, processed*WORD_BYTES);

	// Send message with zero-copy
	msg_ready = false;

	if (zmq_msg_init_data(&pub_msg, msg_buffer,
			      HEADER_BYTES + processed*WORD_BYTES,
			      clear_msg, NULL) != 0) {
	  perror("ERROR: zmq_msg_init_data failed");
	  msg_ready = true;
	  batch_start_time = std::chrono::steady_clock::now();
	  continue;
	}

	if (zmq_msg_send(&pub_msg, pub_socket, 0) < 0) {
	  perror("ERROR: zmq_msg_send failed");
	  zmq_msg_close(&pub_msg);
	} else {
	  total_messages++;
	  window_messages++;
	}
	*/

	batch_start_time = std::chrono::steady_clock::now();
      } else {
	//std::this_thread::sleep_for(std::chrono::microseconds(100));
      }
    }

    zmq_close(pub_socket);
    zmq_ctx_term(ctx);
    return EXIT_SUCCESS;
}

