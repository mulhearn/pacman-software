#ifndef PACMAN_HH
#define PACMAN_HH

#include <stdint.h>

#include "pacman_message.hh"

int pacman_init(int verbose=1);

int pacman_init_tx(int verbose=1, int skip_reset=0);

int pacman_init_rx(int verbose=1, int skip_reset=0);

void pacman_poll_rx(pacman_word_t * buffer, unsigned * index, unsigned max);

int pacman_poll_tx();

int pacman_write(uint32_t addr, uint32_t value);

uint32_t pacman_read(uint32_t addr, int * status = NULL);

unsigned pacman_packet_count_rx(int clear = 0);

unsigned pacman_max_rx_pending(int clear = 0);


#endif
