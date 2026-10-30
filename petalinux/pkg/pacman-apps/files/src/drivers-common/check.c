// check.c
//
// Hardware check results (see check.h)

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "check.h"

static int G_NPASS = 0, G_NFAIL = 0, G_NSKIP = 0;

void check_result(const char * verdict, const char * name, const char * fmt, ...){
  char value[160];
  va_list args;
  va_start(args, fmt);
  vsnprintf(value, sizeof(value), fmt, args);
  va_end(args);

  char dashes[32];
  int n = 24 - (int) strlen(name);
  if (n < 2) n = 2;
  memset(dashes, '-', n);
  dashes[n] = '\0';
  printf("%-4s  %s%s %s\r\n", verdict, name, dashes, value);

  if (strcmp(verdict, "PASS") == 0) G_NPASS++;
  if (strcmp(verdict, "FAIL") == 0) G_NFAIL++;
  if (strcmp(verdict, "SKIP") == 0) G_NSKIP++;
}

void check_clear_counts(){
  G_NPASS = G_NFAIL = G_NSKIP = 0;
}

void check_counts(int * npass, int * nfail, int * nskip){
  *npass = G_NPASS;
  *nfail = G_NFAIL;
  *nskip = G_NSKIP;
}

void check_pause(const char * prompt){
  printf("OPERATOR:  %s  Press Enter when done.\r\n", prompt);
  fflush(stdout);
#ifdef _LINUX
  char line[64];
  if (fgets(line, sizeof(line), stdin) == NULL)   // end of input (a scripted run) ends the pause
    printf("\n");
#else
  input_choice();
#endif
}
