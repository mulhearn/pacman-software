// la_menu.c
//
// Logic analyzer test setups (see la_menu.h)

#include <stdio.h>
#include "hw_access.h"
#include "la.h"
#include "la_menu.h"

#define LA_SETUP_NONE   0
#define LA_SETUP_READ   1
#define LA_SETUP_POKE   2
#define LA_NSETUP       3
#define LA_BUF_MAX      65536

static const char * const LA_SETUP_NAME[LA_NSETUP] = {"none", "ASIC read", "poke alignment"};

// Sample bit names of each setup (bit 0 first;  NULL:  not shown)
static const char * const LA_BITS_READ[12] = {
  "POSI 0", "POSI 1", "POSI 2", "POSI 3", "PISO 0", "PISO 1", "PISO 2", "PISO 3",
  "UCLK", "BAUD", "LEMO A", "LEMO B"};
static const char * const LA_BITS_POKE[12] = {
  "G", "H", NULL, NULL, "UCLK", "BAUD", "LEMO A", "LEMO B", "M0", "M1", "M2", "M3"};

static int G_LA_SETUP = LA_SETUP_NONE;
static int G_LA_TILE = 0;                     // 0-9:  TILE 1-10
static hw_u32_t G_LA_BUF[LA_BUF_MAX];

// Configure the current setup (none:  nothing).
static void la_apply_setup(){
  if (G_LA_SETUP == LA_SETUP_NONE)
    return;
  la_config_t c;
  la_default_config(&c);
  c.tile[0] = G_LA_TILE;
  c.cap.period = 1;
  c.cap.post = capture_depth(SCOPE_LA) / 2;
  if (G_LA_SETUP == LA_SETUP_READ){
    c.lane[0] = LA_LANE_TILE_POSI(0);
    c.lane[1] = LA_LANE_TILE_PISO(0);
    c.lane[2] = LA_LANE_ATC_STATUS_LOWER;
    c.cap.trig_bit = 0;
    c.cap.trig_edge = CAP_EDGE_EITHER;
  } else {
    c.lane[0] = LA_LANE_TILE_AB_GH;
    c.lane[1] = LA_LANE_ATC_STATUS_LOWER;
    c.lane[2] = LA_LANE_ATC_STATUS_UPPER;
    c.cap.trig_bit = 8;
    c.cap.trig_edge = CAP_EDGE_RISE;
  }
  la_configure(&c);
}

static void la_print_setup(){
  printf("LA setup:  %s", LA_SETUP_NAME[G_LA_SETUP]);
  if (G_LA_SETUP != LA_SETUP_NONE) printf(",  TILE %d", G_LA_TILE + 1);
  printf("\r\n");
}

void toggle_logic_analyzer_setup(){
  G_LA_SETUP = (G_LA_SETUP + 1) % LA_NSETUP;
  la_apply_setup();
  la_print_setup();
}

void toggle_logic_analyzer_tile(){
  G_LA_TILE = (G_LA_TILE + 1) % 10;
  la_apply_setup();
  la_print_setup();
}

void la_arm(){
  capture_arm(SCOPE_LA);
  printf("LA armed (%s)\r\n", LA_SETUP_NAME[G_LA_SETUP]);
}

void la_force(){
  capture_force(SCOPE_LA);
  printf("LA forced\r\n");
}

void show_la_status(){
  hw_val_t st = capture_status(SCOPE_LA);
  printf("LA status 0x%x:  %s%s%s\r\n", (unsigned) st, (st & CAP_ARMED) ? "armed " : "",
         (st & CAP_TRIGGERED) ? "triggered " : "", (st & CAP_DONE) ? "done" : "");
  la_print_setup();
  if (!(st & CAP_DONE))
    return;
  hw_u32_t pos;
  uint64_t ts;
  hw_u32_t n = capture_read(SCOPE_LA, G_LA_BUF, LA_BUF_MAX, &pos, &ts);
  printf("%u samples, trigger at %u, timestamp %llu\r\n", (unsigned) n, (unsigned) pos,
         (unsigned long long) ts);
  const char * const * names = G_LA_SETUP == LA_SETUP_READ ? LA_BITS_READ :
                               G_LA_SETUP == LA_SETUP_POKE ? LA_BITS_POKE : NULL;
  int nbits = names ? 12 : 32;
  for (int b = 0; b < nbits; b++){
    if (names && !names[b]) continue;
    unsigned edges = 0;
    long first = 0;
    for (hw_u32_t i = 1; i < n; i++){
      if (((G_LA_BUF[i] ^ G_LA_BUF[i-1]) >> b) & 1){
        if (!edges) first = (long) i - (long) pos;
        edges++;
      }
    }
    if (!names && !edges) continue;
    char label[16];
    if (!names) snprintf(label, sizeof(label), "bit %d", b);
    printf("  %-8s  %6u edges", names ? names[b] : label, edges);
    if (edges) printf(",  first at %+ld", first);
    printf("\r\n");
  }
}

void la_menu(){
  while(1){
    printf("LOGIC ANALYZER MENU:  choose an option:\r\n");
    printf("(x) main menu (s) toggle setup (t) toggle tile (a) arm (f) force (w) show status \r\n");
    int input = input_choice();
    if (input == INPUT_END)
      return;
    printf("INFO: selected %c\r\n", input);

    switch(input){
    case 'x':
      return;
    case 's':
      toggle_logic_analyzer_setup();
      break;
    case 't':
      toggle_logic_analyzer_tile();
      break;
    case 'a':
      la_arm();
      break;
    case 'f':
      la_force();
      break;
    case 'w':
      show_la_status();
      break;
    default:
      printf("invalid selection...\r\n");
    }
  }
}
