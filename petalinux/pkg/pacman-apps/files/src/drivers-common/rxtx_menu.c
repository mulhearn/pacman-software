#include <stdint.h>
#include <string.h>
#include "hw_access.h"
#include "dma.h"
#include "rxtx.h"
#include "rxtx_menu.h"
#include "global.h"
#include "iic.h"
#include "check.h"

// -----------------------------------------------------------
// RX/TX menu layer:  test configurations built on the driver
// -----------------------------------------------------------

void toggle_rx_config(void){
  static const struct { hw_u32_t config; const char * name; } modes[] = {
    {RX_TEST_PIN,            " UART input."},
    {RX_TEST_LOOPBACK,       "Full internal loopback."},
    {RX_TEST_PATTERN,        "Test pattern mode."},
    {RX_TEST_LOOPBACK_FIXED, "Full internal loopback, with edge detect disabled."},
    {RX_TEST_LOOPBACK_NOISE, "Full internal loopback, with intentional noise."},
    {RX_TEST_LOOPBACK_FAIL,  "Full internal loopback, with intentional failures."},
  };
  static int mode = 0;
  mode = (mode + 1) % 6;
  printf("INFO: %s  Broadcasting rx config write 0x%08x \r\n", modes[mode].name, (unsigned int) modes[mode].config);
  rx_set_config_all(modes[mode].config);
}

// expected word k on a UART:  splitmix64 of (seed, uart, k)
static uint64_t check_word(hw_u32_t seed, unsigned uart, unsigned k){
  uint64_t z = ((uint64_t) seed << 32) ^ ((uint64_t) uart << 24) ^ (uint64_t) k;
  z += 0x9E3779B97F4A7C15ULL;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

// time limits:
#define CHECK_DRAIN_US          1000000
#define CHECK_BASE_US           1000000
#define CHECK_PACKET_NS         264000     // 10 x 26.4 us

static hw_u32_t check_elapsed_us(void){
  stop_hw_timer();
  return hw_timer_elapsed_us();
}

void rxtx_loopback_run(unsigned tx_packets, hw_u32_t seed, uint64_t uart_mask, rxtx_check_t * r){
  const unsigned uarts      = 40;
  const unsigned batch_size = 100;
  const unsigned timeout    = 10000;
  unsigned tx_timeout = timeout;
  unsigned rx_timeout = timeout;
  unsigned tx_k = 0;            // words sent so far on each UART
  unsigned rx_k[40];            // words received so far on each UART
  hw_val_t chan_id[40];         // channel id in the RX word header, per UART

  memset(r, 0, sizeof(*r));
  if (rxtx_ring_size() == 0){
    printf("ERROR:  loopback check:  no DMA ring set up (init_rxtx_descriptor_ring_mode).\r\n");
    return;
  }
  for (unsigned u=0; u<uarts; u++){
    rx_uart_status_t us;
    get_rx_uart_status(u, &us);
    chan_id[u] = us.chan & 0xFFFF;
    rx_k[u] = 0;
  }

  hw_val_t buffer_config = rx_get_buffer_config();
  rx_set_buffer_config(RX_BUFFER_CONFIG_DAQ);

  // discard words left over from earlier activity (they would shift the expected sequence):
  start_hw_timer();
  for (unsigned idle = 0; idle < 100000; idle++){
    if (check_elapsed_us() > CHECK_DRAIN_US){
      r->timeout = 1;
      rx_set_buffer_config(buffer_config);
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

  const unsigned nmasked = __builtin_popcountll(uart_mask & RXTX_ALL_UARTS);
  const unsigned words_expected = nmasked * tx_packets;
  const hw_u32_t limit_us = CHECK_BASE_US + (hw_u32_t) (((uint64_t) tx_packets * CHECK_PACKET_NS) / 1000);
  start_hw_timer();
  while (tx_timeout && rx_timeout && (r->words_rcvd < words_expected)){
    if (check_elapsed_us() > limit_us){
      r->timeout = 2;
      break;
    }
    if (r->tx_packets < tx_packets) {
      tx_timeout--;
      unsigned batch_count = 0;
      hw_addr_t nxta = 0;
      while((batch_count < batch_size) && (batch_count < (tx_packets - r->tx_packets)) && (dma_next_available_tx_bd(&nxta))){
        hw_ptr_t tx_buf = dma_get_buffer(nxta);
        tx_buf[0] = (hw_u32_t) uart_mask;           // UARTs 0-31
        tx_buf[1] = (hw_u32_t) (uart_mask >> 32);   // UARTs 32-39
        for (unsigned u=0; u<uarts; u++){
          uint64_t w = check_word(seed, u, tx_k);
          tx_buf[TX_HEADER_U32_WORDS + 2*u]     = (hw_u32_t) w;
          tx_buf[TX_HEADER_U32_WORDS + 2*u + 1] = (hw_u32_t) (w >> 32);
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
            uint64_t data = ((uint64_t) word[5] << 32) | word[4];   // fragments: header, timestamp, data
            uint64_t diff = data ^ check_word(seed, u, rx_k[u]++);
            r->words_rcvd++;
            if (diff){
              r->words_bad++;
              r->bit_errors += __builtin_popcountll(diff);
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
  r->elapsed_us = check_elapsed_us();
  r->words_sent = nmasked * r->tx_packets;
  r->tx_timeout = (tx_timeout == 0);
  rx_set_buffer_config(buffer_config);
}

void print_rxtx_check(const rxtx_check_t * r){
  printf("INFO:  tx packets:     %u \r\n", r->tx_packets);
  printf("INFO:  words sent:     %u \r\n", r->words_sent);
  printf("INFO:  words received: %u (missing: %d) \r\n", r->words_rcvd, (int) (r->words_sent - r->words_rcvd));
  printf("INFO:  words in error: %u \r\n", r->words_bad);
  printf("INFO:  bit errors:     %u in %llu bits \r\n", r->bit_errors, 64ULL * r->words_rcvd);
  printf("INFO:  stale words:    %u   other words: %u   stray words: %u   bad packets: %u   tx timeout: %u \r\n",
         r->stale_words, r->other_words, r->stray_words, r->bad_packets, r->tx_timeout);
  printf("INFO:  time limit:     %s   elapsed: %u us \r\n",
         r->timeout == 1 ? "drain" : r->timeout == 2 ? "check" : "none", r->elapsed_us);
}

void menu_check_rxtx_loopback(void){
  rxtx_check_t r;
  if (rxtx_ring_size() == 0){
    printf("INFO:  no DMA ring set up yet:  initialize the BD ring first (RX/TX menu item 20).\r\n");
    return;
  }
  printf("INFO:  loopback check, random data, 10000 packets, current RX config... \r\n");
  rxtx_loopback_run(10000, 1, RXTX_ALL_UARTS, &r);
  print_rxtx_check(&r);
}

// *** Check ***

static void loopback_counters(const char * name, uint64_t uart_mask){
  rx_uart_status_t us;
  unsigned frame = 0, noise = 0, lost = 0;
  for (unsigned u = 0; u < 40; u++){
    if (!((uart_mask >> u) & 1)) continue;
    get_rx_uart_status(u, &us);
    frame += us.frame_err;
    noise += us.noise;
    lost  += us.lost;
  }
  check_result("INFO", name, "frame errors %u, noise %u, lost %u", frame, noise, lost);
}

static const char * loopback_limit(const rxtx_check_t * r){
  return r->timeout == 1 ? ", TIME LIMIT (drain)" : r->timeout == 2 ? ", TIME LIMIT" : "";
}

// RX and TX tested through the loopback card (TX N -> RX N), one tile at a time.
void rxtx_check_loopback(hw_u32_t tile_mask, hw_u32_t ber_bits){
  rxtx_check_t r;
  char name[32];
  unsigned packets = ber_bits ? (ber_bits + 4*64 - 1) / (4*64) : RXTX_LOOPBACK_PACKETS;
  unsigned stray = 0;

  global_status_t gs;
  get_global_status(&gs);
  hw_val_t enables = gs.enables;
  hw_val_t buffer_enables = axil_read_register(SCOPE_RX + UART_GLOBAL + C_ADDR_RX_BUFFER_ENABLES);
  axil_write_register(SCOPE_RX + UART_GLOBAL + C_ADDR_RX_BUFFER_ENABLES, 0);
  init_rxtx();
  init_rxtx_descriptor_ring_mode(128);
  for (int t = 0; t < 10; t++){
    iic_set_vdda_dn(t, 0);
    iic_set_vddd_dn(t, 0);
  }

  for (int t = 0; t < 10; t++){
    snprintf(name, sizeof(name), "tile_%02d_loopback", t+1);
    if (!((tile_mask >> t) & 1)){
      check_result("SKIP", name, "not in the tile mask 0x%03x", (unsigned) tile_mask);
      continue;
    }
    uint64_t uarts = 0xFULL << (4*t);
    set_global_enables(GLOBAL_ENABLE_TILE_POWER | (1u << t));
    rx_set_config_all(RX_TEST_PIN);
    for (unsigned u = 0; u < 40; u++)
      if (!((uarts >> u) & 1)) rx_disable_uart(u);
    usleep(10000);
    zero_rxtx_counts();
    rxtx_loopback_run(packets, 1 + t, uarts, &r);

    bool words_ok = (r.words_sent > 0) && (r.words_rcvd == r.words_sent) && !r.tx_timeout && !r.timeout
                    && !r.bad_packets;
    unsigned u0 = 4*t;
    check_result(words_ok ? "PASS" : "FAIL", name, "%u of %u received in %.2f s (UARTs %u-%u: %u %u %u %u%s%s)",
                 r.words_rcvd, r.words_sent, r.elapsed_us / 1e6, u0+1, u0+4, r.rcvd[u0], r.rcvd[u0+1],
                 r.rcvd[u0+2], r.rcvd[u0+3], r.tx_timeout ? ", TX TIMEOUT" : "", loopback_limit(&r));
    snprintf(name, sizeof(name), "tile_%02d_bit_errors", t+1);
    check_result((r.bit_errors == 0 && r.words_rcvd > 0) ? "PASS" : "FAIL", name, "%u in %llu bits (%u words)",
                 r.bit_errors, 64ULL * r.words_rcvd, r.words_bad);
    snprintf(name, sizeof(name), "tile_%02d_rx_counters", t+1);
    loopback_counters(name, uarts);
    if (r.stray_words){
      char list[128] = "";
      for (unsigned u = 0; u < 40; u++){
        if (((uarts >> u) & 1) || !r.rcvd[u]) continue;
        size_t n = strlen(list);
        snprintf(list + n, sizeof(list) - n, " UART %u: %u", u+1, r.rcvd[u]);
      }
      snprintf(name, sizeof(name), "tile_%02d_stray", t+1);
      check_result("FAIL", name, "%u words on disabled UARTs:%s", r.stray_words, list);
      stray += r.stray_words;
    }
  }
  rx_set_config_all(RX_CONFIG_DEFAULT);
  set_global_enables(enables);
  axil_write_register(SCOPE_RX + UART_GLOBAL + C_ADDR_RX_BUFFER_ENABLES, buffer_enables);
  if (tile_mask)
    check_result(stray ? "FAIL" : "PASS", "loopback_stray", "%u words on disabled UARTs", stray);
}
