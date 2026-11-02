// pacman_hwcheck:  PACMAN hardware test suite.
//
// Tests and modifiers are selected on the command line:  see usage().
// The tests themselves are in common drivers.
//
// Output:  one line per item, "<VERDICT> <name>---- <value>", then a final
// "VERDICT" line.  Exit code:  0 PASS, 1 FAIL, 2 INCOMPLETE, 3 usage or refused.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "hw_access.h"
#include "global.h"
#include "iic.h"
#include "jumper.h"
#include "led.h"
#include "rxtx.h"
#include "dma.h"
#include "check.h"
#include "rxtx_menu.h"
#include "iic_menu.h"
#include "atc_menu.h"
#include "adc_menu.h"
#include "hwcheck_menu.h"

#define EXIT_PASS        0
#define EXIT_FAIL        1
#define EXIT_INCOMPLETE  2
#define EXIT_USAGE       3   // also: refused (J5 installed)

// *** READ ONLY STATUS REPORT ***

static void read_only_linux(){
  struct utsname u;
  if (uname(&u) == 0)
    check_result("INFO", "linux_kernel", "%s", u.release);

  struct ifaddrs * ifs;
  const char * ip = "none";
  char buf[INET_ADDRSTRLEN];
  if (getifaddrs(&ifs) == 0){
    for (struct ifaddrs * i = ifs; i != NULL; i = i->ifa_next){
      if (i->ifa_addr && i->ifa_addr->sa_family == AF_INET && strcmp(i->ifa_name, "eth0") == 0){
        inet_ntop(AF_INET, &((struct sockaddr_in *) i->ifa_addr)->sin_addr, buf, sizeof(buf));
        ip = buf;
      }
    }
    freeifaddrs(ifs);
  }
  check_result("INFO", "eth0_address", "%s", ip);
}

static void read_only(bool no_iic){
  hwcheck_read_only(no_iic);
  read_only_linux();
}

static void usage(){
  printf("usage:  pacman_hwcheck <tests> [modifiers]\n");
  printf("\n");
  printf("tests (combinable;  they run in this order):\n");
  printf("  --read_only    status report (allowed with J5 installed)\n");
  printf("  --power        board rails;  each tile in the mask swept into the card's loads,\n");
  printf("                 the others checked off\n");
  printf("  --loopback     UART loopback through the pins (loopback card), one tile at a time,\n");
  printf("                 then all the masked tiles at once\n");
  printf("  --atc_outputs  each tile's SYNC, TRIG through the ATC test card to its PISO\n");
  printf("  --lemo_inputs  TRIG (LEMO A) and PPS (LEMO B) rates (a 1 kHz TTL on both)\n");
  printf("  --ana_mon      each tile's CLK on its analog monitor (ATC test card), into the ADC\n");
  printf("                 and through the loop resistors\n");
  printf("  --adc          ADC registers;  the test DAC into the ADC through the MUX, direct and\n");
  printf("                 through the series loops (no card)\n");
  printf("  --blink        LED blink (pauses)\n");
  printf("  --sma          R28 across the SMA jacks J1001 (P), J14 (N) (pauses), then open\n");
  printf("  --i2c_stress   sweeps of safe transactions over every I2C device, stopping at the\n");
  printf("                 first error;  reports the I2C clock rate\n");
  printf("modifiers:\n");
  printf("  --tile_mask M  tiles to check (bit t = tile t+1;  default 0x3ff);  others SKIP\n");
  printf("  --ber_bits N   loopback length in bits per tile (default %d packets)\n",
         RXTX_LOOPBACK_PACKETS);
  printf("  --no_i2c       skip everything on I2C\n");
  printf("  --sweeps N     --i2c_stress sweeps (default %d)\n", IIC_STRESS_SWEEPS);
  printf("  --no_pause     no pauses:  --blink blinks a fixed time;  --sma sets the MUX, then\n");
  printf("                 opens it\n");
  printf("  --help         this message\n");
  printf("\n");
  printf("Every test leaves the board as it found it (enables restored, DACs 0, MUX open).\n");
  printf("A run with anything skipped ends INCOMPLETE, never PASS.\n");
  printf("With J5 installed (production), only --read_only runs.\n");
  printf("Exit code:  0 PASS, 1 FAIL, 2 INCOMPLETE, 3 usage or refused.\n");
}

// The tests, in run order;  run with the modifiers
typedef struct {
  hw_u32_t tile_mask, ber_bits, sweeps;
  bool no_iic, pause;
} hwcheck_opts_t;

static void run_read_only(const hwcheck_opts_t * o)   { read_only(o->no_iic); }
static void run_power(const hwcheck_opts_t * o)       { iic_check_power(o->tile_mask); }
static void run_loopback(const hwcheck_opts_t * o)    {
  init_rxtx();
  hwcheck_set_tile_mask(o->tile_mask);
  if (o->ber_bits)
    rxtx_set_loopback_packets((o->ber_bits + 4*64 - 1) / (4*64));   // 4 UARTs x 64 bits per packet
  hwcheck_loopback();
}
static void run_atc_outputs(const hwcheck_opts_t * o) { atc_check_outputs(o->tile_mask); }
static void run_lemo_inputs(const hwcheck_opts_t * o) { (void) o;  atc_check_lemo_inputs(); }
static void run_ana_mon(const hwcheck_opts_t * o)     { adc_check_ana_mon(o->tile_mask); }
static void run_adc(const hwcheck_opts_t * o)         { (void) o;  adc_check(); }
static void run_blink(const hwcheck_opts_t * o)       { led_check_blink(o->pause); }
static void run_sma(const hwcheck_opts_t * o)         { iic_check_sma(o->pause); }
static void run_i2c_stress(const hwcheck_opts_t * o)  { iic_check_stress(o->sweeps); }

static const struct {
  const char * option;
  bool needs_iic;
  void (*run)(const hwcheck_opts_t * o);
} TESTS[] = {
  {"read_only",   false, run_read_only},
  {"power",       true,  run_power},
  {"loopback",    true,  run_loopback},      // every tile rail held at DAC 0
  {"atc_outputs", false, run_atc_outputs},
  {"lemo_inputs", false, run_lemo_inputs},
  {"ana_mon",     true,  run_ana_mon},
  {"adc",         true,  run_adc},
  {"blink",       false, run_blink},
  {"sma",         true,  run_sma},
  {"i2c_stress",  true,  run_i2c_stress},
};
#define NTESTS (int) (sizeof(TESTS) / sizeof(TESTS[0]))

int main(int argc, char * argv[]){
  // OPT_TEST + test index for the tests
  enum { OPT_TILE_MASK = 1, OPT_BER_BITS, OPT_NO_I2C, OPT_NO_PAUSE, OPT_SWEEPS, OPT_HELP,
         OPT_TEST };
  static struct option opts[NTESTS + 7];   // the tests, the 6 modifiers, the end
  static const struct option MODIFIERS[] = {
    {"tile_mask", required_argument, 0, OPT_TILE_MASK},
    {"ber_bits",  required_argument, 0, OPT_BER_BITS},
    {"no_i2c",    no_argument,       0, OPT_NO_I2C},
    {"no_pause",  no_argument,       0, OPT_NO_PAUSE},
    {"sweeps",    required_argument, 0, OPT_SWEEPS},
    {"help",      no_argument,       0, OPT_HELP},
  };
  for (int i = 0; i < NTESTS; i++)
    opts[i] = (struct option) {TESTS[i].option, no_argument, 0, OPT_TEST + i};
  const int NMOD = sizeof(MODIFIERS) / sizeof(MODIFIERS[0]);
  for (int i = 0; i < NMOD; i++)
    opts[NTESTS + i] = MODIFIERS[i];
  opts[NTESTS + NMOD] = (struct option) {0, 0, 0, 0};

  bool selected[NTESTS] = {false};
  hwcheck_opts_t o = {0x3FF, 0, IIC_STRESS_SWEEPS, false, true};
  bool bad = false;
  int c;
  while ((c = getopt_long(argc, argv, "", opts, NULL)) != -1){
    if (c >= OPT_TEST && c < OPT_TEST + NTESTS) { selected[c - OPT_TEST] = true;  continue; }
    switch (c){
    case OPT_TILE_MASK: o.tile_mask = strtoul(optarg, NULL, 0); break;
    case OPT_BER_BITS:
      o.ber_bits = strtoul(optarg, NULL, 0);
      if (o.ber_bits == 0) bad = true;
      break;
    case OPT_NO_I2C:    o.no_iic = true;   break;
    case OPT_NO_PAUSE:  o.pause = false;   break;
    case OPT_SWEEPS:
      o.sweeps = strtoul(optarg, NULL, 0);
      if (o.sweeps == 0) bad = true;
      break;
    case OPT_HELP:      usage();  return EXIT_PASS;
    default:            usage();  return EXIT_USAGE;
    }
  }
  bool any = false, writes = false;
  for (int i = 0; i < NTESTS; i++){
    any = any || selected[i];
    writes = writes || (selected[i] && i > 0);   // every test but --read_only writes
  }
  if (optind < argc || !any || bad || o.tile_mask == 0 || o.tile_mask > 0x3FF){
    usage();
    return EXIT_USAGE;
  }

  printf("pacman_hwcheck:  PACMAN hardware tests.  Tile mask 0x%03x%s.\n", (unsigned) o.tile_mask,
         o.pause ? "" : ", no pauses");
  axil_platform_init();
  init_jumper();

  // J5 = production jumper:  only --read_only runs with it installed
  if (writes && !jumper_allows_writes())
    return EXIT_USAGE;

  global_status_t gs;
  get_global_status(&gs);
  if (!o.no_iic){
    iic_platform_init();
    iic_set_hw_version(gs.hardware_major, gs.hardware_minor, gs.hardware_patch);
  }

  // Unless in read-only mode, zero the RX and TX counters, so this run reports only its own counts:
  if (writes){
    zero_rxtx_counts();
    check_result("INFO", "rxtx_counters", "zeroed at the start of this run");
  }

  for (int i = 0; i < NTESTS; i++){
    if (!selected[i]) continue;
    if (TESTS[i].needs_iic && o.no_iic) check_result("SKIP", TESTS[i].option, "--no_i2c");
    else                                TESTS[i].run(&o);
  }

  int npass, nfail, nskip;
  check_counts(&npass, &nfail, &nskip);
  const char * verdict = nfail ? "FAIL" : (nskip ? "INCOMPLETE" : "PASS");
  printf("VERDICT %s  (pass %d, fail %d, skip %d)\n", verdict, npass, nfail, nskip);
  return nfail ? EXIT_FAIL : (nskip ? EXIT_INCOMPLETE : EXIT_PASS);
}
