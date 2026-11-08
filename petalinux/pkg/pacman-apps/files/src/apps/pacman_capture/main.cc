// pacman_capture:  arm the scope (ADC) or logic analyzer, and save its capture to a file (Linux).
// Uses the configuration already in the hardware (set by pacman_hwcheck, pacman_menu, ...);
// configures nothing.
//
//   pacman_capture [--arm-scope|--arm-logic] [--dump-scope|--dump-logic] [--nsamples N]
//                  [--offset K] [-w SECONDS] [-o FILE]
//
//   --arm-scope    arm the ADC capture with its current configuration
//   --arm-logic    arm the logic analyzer capture with its current configuration
//   --dump-scope   save the ADC capture
//   --dump-logic   save the logic analyzer capture
//   -w SECONDS     with arm and dump together:  wait up to SECONDS for the capture to finish, then
//                  save it (default 10;  0:  no timeout);  arm alone returns at once
//   --nsamples N   save N samples around the trigger (default:  the whole buffer)
//   --offset K     the trigger K samples from the window's centre (+:  later in the window, more
//                  samples before it);  the window is trimmed at the buffer's ends, with a note
//   -o FILE        output file (default:  capture_scope.txt or capture_logic.txt)
//
// File:  one header line,
//   # pacman_capture <scope|logic>  status <armed triggered done>  period <clocks>
//     trig_bit <bit> edge <rise|fall|either> cross <0|1>  post <samples>  depth <samples>
//     trig_pos <sample in buffer>  trig_ts <timestamp>  samples <n>  first <buffer position>
//     trig_row <row of the trigger in this file>
// for the logic analyzer also the tiles of lanes A-D and the lanes of nibbles A-H;  then one
// sample per line, oldest first:  hex, 8 digits (logic analyzer:  nibbles H to A) or 4 digits
// (scope:  bits 11-0 data, 12 overflow, 13 comparator level).

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include "hw_access.h"
#include "capture.h"
#include "adc.h"
#include "la.h"

static hw_u32_t G_BUF[65536];

static const char * edge_name(hw_u32_t e){
  return e == CAP_EDGE_RISE ? "rise" : e == CAP_EDGE_FALL ? "fall" : "either";
}

// Save the capture of scope to path:  nsamples around the trigger (0:  all), offset as in the
// usage.
static int dump(hw_addr_t scope, const char * path, long nsamples, long offset){
  bool logic = scope == SCOPE_LA;
  hw_val_t st = capture_status(scope);
  capture_config_t c;
  capture_read_config(scope, &c);
  hw_u32_t depth = capture_depth(scope), pos = 0;
  uint64_t ts = 0;
  hw_u32_t n = capture_read(scope, G_BUF, sizeof(G_BUF) / sizeof(G_BUF[0]), &pos, &ts);
  long first = 0, count = n;
  bool trimmed = false;
  if (nsamples > 0 && nsamples < (long) n){
    count = nsamples;
    first = (long) pos - (nsamples / 2 + offset);
    if (first < 0){ first = 0; trimmed = true; }
    if (first + count > (long) n){ first = n - count; trimmed = true; }
  }
  FILE * f = fopen(path, "w");
  if (!f){ perror(path); return 1; }
  char status[32] = "";
  if (st & CAP_ARMED) strcat(status, "armed,");
  if (st & CAP_TRIGGERED) strcat(status, "triggered,");
  if (st & CAP_DONE) strcat(status, "done,");
  if (status[0]) status[strlen(status) - 1] = 0; else strcpy(status, "none");
  fprintf(f, "# pacman_capture %s  status %s  period %u  trig_bit %u edge %s cross %u  post %u",
          logic ? "logic" : "scope", status, (unsigned) c.period, (unsigned) c.trig_bit,
          edge_name(c.trig_edge), (unsigned) c.cross, (unsigned) c.post);
  fprintf(f, "  depth %u  trig_pos %u  trig_ts %llu", (unsigned) depth, (unsigned) pos,
          (unsigned long long) ts);
  fprintf(f, "  samples %ld  first %ld  trig_row %ld", count, first, (long) pos - first);
  if (logic){
    fprintf(f, "  tiles");
    for (int x = 0; x < 4; x++)
      fprintf(f, "%c%u", x ? ',' : ' ',
              (unsigned) axil_read_register(SCOPE_LA + C_ADDR_LA_TILE_A + 4*x));
    fprintf(f, "  lanes");
    for (int k = 0; k < 8; k++)
      fprintf(f, "%c%u", k ? ',' : ' ',
              (unsigned) axil_read_register(SCOPE_LA + C_ADDR_LA_MUX_SELECT_A + 4*k));
  }
  fprintf(f, "\n");
  for (long i = first; i < first + count; i++)
    fprintf(f, logic ? "%08x\n" : "%04x\n", (unsigned) (logic ? G_BUF[i] : G_BUF[i] & 0x3FFF));
  fclose(f);
  printf("%s:  %s capture (%s), %ld of %u samples, trigger at row %ld%s\n", path,
         logic ? "logic" : "scope", status, count, (unsigned) n, (long) pos - first,
         trimmed ? " (window trimmed at the buffer's end)" : "");
  return (st & CAP_DONE) ? 0 : 1;
}

int main(int argc, char * argv[]){
  int arm = -1, dmp = -1;                     // 0 scope, 1 logic
  long nsamples = 0, offset = 0, wait_s = 10;
  const char * out = NULL;
  static const struct option opts[] = {
    {"arm-scope", no_argument, NULL, 'A'}, {"arm-logic", no_argument, NULL, 'L'},
    {"dump-scope", no_argument, NULL, 'a'}, {"dump-logic", no_argument, NULL, 'l'},
    {"nsamples", required_argument, NULL, 'n'}, {"offset", required_argument, NULL, 'k'},
    {NULL, 0, NULL, 0}};
  int opt;
  while ((opt = getopt_long(argc, argv, "w:o:", opts, NULL)) != -1){
    switch (opt){
    case 'A': arm = 0; break;
    case 'L': arm = 1; break;
    case 'a': dmp = 0; break;
    case 'l': dmp = 1; break;
    case 'n': nsamples = strtol(optarg, NULL, 0); break;
    case 'k': offset = strtol(optarg, NULL, 0); break;
    case 'w': wait_s = strtol(optarg, NULL, 0); break;
    case 'o': out = optarg; break;
    default:
      fprintf(stderr, "usage:  %s [--arm-scope|--arm-logic] [--dump-scope|--dump-logic]\n"
              "         [--nsamples N] [--offset K] [-w SECONDS] [-o FILE]\n", argv[0]);
      return 3;
    }
  }
  if (arm < 0 && dmp < 0){
    fprintf(stderr, "nothing to do:  give --arm-scope, --arm-logic, --dump-scope or "
            "--dump-logic\n");
    return 3;
  }
  if (arm >= 0 && dmp >= 0 && arm != dmp){
    fprintf(stderr, "arm and dump the same unit\n");
    return 3;
  }
  axil_platform_init();
  if (arm >= 0){
    hw_addr_t scope = arm ? SCOPE_LA : SCOPE_ADC;
    capture_arm(scope);
    printf("%s armed\n", arm ? "logic" : "scope");
    if (dmp >= 0){
      bool done = false;
      if (wait_s == 0)
        while (!(capture_status(scope) & CAP_DONE)) usleep(1000);
      else
        done = capture_wait_done(scope, (hw_u32_t) (wait_s * 1000000)) == 0;
      if (wait_s && !done) printf("not done after %ld s\n", wait_s);
    }
  }
  if (dmp < 0)
    return 0;
  if (!out) out = dmp ? "capture_logic.txt" : "capture_scope.txt";
  return dump(dmp ? SCOPE_LA : SCOPE_ADC, out, nsamples, offset);
}
