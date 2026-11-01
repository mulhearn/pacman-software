#ifndef CHECK_H
#define CHECK_H

// check.h
//
// Hardware checks, shared by pacman_hwcheck and the menus:  one line per result, counted.  Each
// area's checks are in its menu file (<area>_check_<what>).  Every check leaves the board in its
// original state:  enables restored, DACs 0, MUX open.

#include <stdbool.h>
#include "hw_access.h"

#ifdef __cplusplus
extern "C" {
#endif

// Print one result line:  verdict PASS, FAIL, SKIP or INFO.
void check_result(const char * verdict, const char * name, const char * fmt, ...);

// Zero the counts.
void check_clear_counts();

// The counts since the last clear.
void check_counts(int * npass, int * nfail, int * nskip);

// Pause for the operator:  print prompt, wait for Enter (or the end of input).
void check_pause(const char * prompt);

#ifdef __cplusplus
}
#endif

#endif // CHECK_H
