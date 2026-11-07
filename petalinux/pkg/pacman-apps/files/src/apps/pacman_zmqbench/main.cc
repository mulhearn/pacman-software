// pacman_zmqbench:  ZMQ-only throughput benchmark.  Publishes prebuilt data messages
// (no DMA, no copy) on the data server's port, for pacman_counter (on the board, or remote
// with -i) to receive and count.
#include <atomic>
#include <chrono>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <zmq.h>

#include "pacman_message.hh"

#define PUB_SOCKET_BINDING "tcp://*:5556"
#define MAX_PAYLOAD 64000  // max words in a message payload
#define MAX_DEPTH 16       // max messages in flight
#define STALL_LIMIT_MS 1000
#define IO_CPU   1  // ZMQ's I/O thread (the TCP send) alone on one core,
#define MAIN_CPU 0  // this thread with the network interrupts on the other

const int PUB_HWM      = 1000;
const int PUB_LINGER   = 0;
const int PUB_SNDTIMEO = 0;

static char * G_BUFFER[MAX_DEPTH];
static std::atomic<bool> G_FREE[MAX_DEPTH];
static volatile bool G_RUNNING = true;

// free_buffer:  ZMQ's free callback;  hint is the buffer's index
static void free_buffer(void *, void * hint){ G_FREE[(long) hint] = true; }

static void signal_handler(int){ G_RUNNING = false; }

// pin_this_thread:  run the calling thread on cpu only
static void pin_this_thread(int cpu){
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);
  pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
}

// eth_interrupts:  the Ethernet interrupts so far on cpu0 and cpu1 (0 if not found)
static void eth_interrupts(unsigned long long * cpu0, unsigned long long * cpu1){
  *cpu0 = *cpu1 = 0;
  FILE * f = fopen("/proc/interrupts", "r");
  if (f == NULL) return;
  char line[512];
  while (fgets(line, sizeof(line), f)){
    if (strstr(line, "eth0") != NULL){
      sscanf(line, "%*s %llu %llu", cpu0, cpu1);
      break;
    }
  }
  fclose(f);
}

static double ms_since(const struct timeval * t0){
  struct timeval now;
  gettimeofday(&now, NULL);
  return 1000.0*(now.tv_sec - t0->tv_sec) + (now.tv_usec - t0->tv_usec)/1000.0;
}

int main(int argc, char * argv[]){
  unsigned nwords = MAX_PAYLOAD;
  unsigned secs   = 10;
  unsigned depth  = 1;
  bool pinned     = true;
  int opt;
  while ((opt = getopt(argc, argv, "w:t:q:u")) != -1){
    switch(opt){
    case 'w': nwords = atoi(optarg); break;
    case 't': secs   = atoi(optarg); break;
    case 'q': depth  = atoi(optarg); break;
    case 'u': pinned = false; break;
    default:  nwords = 0; break;
    }
  }
  if ((nwords < 1) || (nwords > MAX_PAYLOAD) || (depth < 1) || (depth > MAX_DEPTH) || (secs < 1)){
    printf("Usage: %s [-w words per message, 1-%d (%d)] [-t seconds (10)]"
           " [-q messages in flight, 1-%d (1)] [-u unpinned]\n", argv[0], MAX_PAYLOAD, MAX_PAYLOAD,
           MAX_DEPTH);
    return 1;
  }
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  void * ctx = zmq_ctx_new();
  if (pinned) pin_this_thread(IO_CPU);  // ZMQ's threads, started by the first socket, inherit this core
  void * pub = zmq_socket(ctx, ZMQ_XPUB);
  if (pinned) pin_this_thread(MAIN_CPU);
  zmq_setsockopt(pub, ZMQ_SNDHWM, &PUB_HWM, sizeof(PUB_HWM));
  zmq_setsockopt(pub, ZMQ_LINGER, &PUB_LINGER, sizeof(PUB_LINGER));
  zmq_setsockopt(pub, ZMQ_SNDTIMEO, &PUB_SNDTIMEO, sizeof(PUB_SNDTIMEO));
  if (zmq_bind(pub, PUB_SOCKET_BINDING) != 0){
    printf("ERROR:  cannot bind %s (%s):  stop pacman_dataserver first.\n", PUB_SOCKET_BINDING,
           zmq_strerror(zmq_errno()));
    zmq_close(pub);
    zmq_ctx_term(ctx);
    return 1;
  }

  uint32_t session = 0;
  while (session == 0){
    if (getrandom(&session, sizeof(session), 0) != sizeof(session)){
      perror("ERROR:  getrandom");
      return 1;
    }
  }

  // the messages:  data words on channels 1-40, built once
  unsigned nbytes = nwords * WORD_BYTES;
  for (unsigned b=0; b<depth; b++){
    G_BUFFER[b] = (char *) malloc(HEADER_BYTES + nbytes);
    if (G_BUFFER[b] == NULL){
      printf("ERROR:  out of memory\n");
      return 1;
    }
    pacman_word_t * words = (pacman_word_t *) (G_BUFFER[b] + HEADER_BYTES);
    for (unsigned i=0; i<nwords; i++){
      write_word_data(&words[i], 0, 1 + i%40, 0, i, rand(), rand());
    }
    G_FREE[b] = true;
  }

  printf("INFO:  pacman_zmqbench:  %u words per message (%.2f MB), %u in flight, %u s\n", nwords,
         (HEADER_BYTES + nbytes)/1e6, depth, secs);
  printf("INFO:  waiting for a subscriber on %s ...\n", PUB_SOCKET_BINDING);
  char sub[256];
  while (G_RUNNING && (zmq_recv(pub, sub, sizeof(sub), 0) < 0)){}
  if (!G_RUNNING) return 0;
  printf("INFO:  subscriber connected;  sending.\n");
  unsigned long long irq0_start, irq1_start;
  eth_interrupts(&irq0_start, &irq1_start);

  uint32_t seq = 0;
  uint64_t messages = 0;
  unsigned next = 0;
  struct timeval start, stall;
  gettimeofday(&start, NULL);
  while (G_RUNNING && (ms_since(&start) < 1000.0*secs)){
    // an unsubscribe (first byte 0):  with no subscriber, messages would be dropped, not sent
    if ((zmq_recv(pub, sub, sizeof(sub), ZMQ_DONTWAIT) > 0) && (sub[0] == 0)){
      printf("WARNING:  the subscriber left:  run ended early.\n");
      break;
    }
    if (!G_FREE[next]){
      gettimeofday(&stall, NULL);
      while (!G_FREE[next] && (ms_since(&stall) < STALL_LIMIT_MS)){
        std::this_thread::sleep_for(std::chrono::microseconds(100));
      }
      if (!G_FREE[next]){
        printf("ERROR:  a message not sent within %d ms:  stalled subscriber.\n", STALL_LIMIT_MS);
        break;
      }
    }
    write_header_data((pacman_header_t *) G_BUFFER[next], nbytes);
    ((pacman_header_t *) G_BUFFER[next])->seq     = seq++;
    ((pacman_header_t *) G_BUFFER[next])->session = session;
    G_FREE[next] = false;
    zmq_msg_t msg;
    zmq_msg_init_data(&msg, G_BUFFER[next], HEADER_BYTES + nbytes, free_buffer,
                      (void *) (long) next);
    if (zmq_msg_send(&msg, pub, 0) < 0){
      printf("ERROR:  zmq_msg_send failed (%s)\n", zmq_strerror(zmq_errno()));
      break;
    }
    messages++;
    next = (next + 1) % depth;
  }
  double ms = ms_since(&start);

  // wait for the messages in flight, so the counts are of messages sent
  gettimeofday(&stall, NULL);
  for (unsigned b=0; b<depth; b++){
    while (!G_FREE[b] && (ms_since(&stall) < STALL_LIMIT_MS)){
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
  }
  double all_ms = ms_since(&start);
  unsigned long long irq0, irq1;
  eth_interrupts(&irq0, &irq1);
  irq0 -= irq0_start;
  irq1 -= irq1_start;
  printf("INFO:  sent %llu messages (%llu packets) in %.1f s (%.1f s until the last left)\n",
         (unsigned long long) messages, (unsigned long long) (messages * nwords), ms/1000,
         all_ms/1000);
  printf("INFO:  %.1f packets/ms, %.1f MB/s\n", messages * nwords / all_ms,
         messages * (HEADER_BYTES + nbytes) / all_ms / 1000);
  printf("INFO:  session 0x%08x\n", session);
  if (pinned){
    printf("INFO:  ZMQ I/O thread on cpu%d, main thread on cpu%d;  Ethernet interrupts:  "
           "cpu0 %llu, cpu1 %llu\n", IO_CPU, MAIN_CPU, irq0, irq1);
    unsigned long long off_main = (MAIN_CPU == 0) ? irq1 : irq0;
    if (off_main*10 > irq0 + irq1){
      printf("WARNING:  Ethernet interrupts off cpu%d:  the I/O thread shared its core;  "
             "results not comparable.\n", MAIN_CPU);
    }
  } else {
    printf("INFO:  unpinned;  Ethernet interrupts:  cpu0 %llu, cpu1 %llu\n", irq0, irq1);
  }

  zmq_close(pub);
  zmq_ctx_term(ctx);
  for (unsigned b=0; b<depth; b++) free(G_BUFFER[b]);
  return 0;
}
