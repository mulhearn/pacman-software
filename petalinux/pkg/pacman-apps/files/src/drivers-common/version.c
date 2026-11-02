#include <stdio.h>
#include "version.h"

void get_software_version_string(char * buffer, size_t buffer_size){
  snprintf(buffer, buffer_size, "%u.%u.%u %s", PACMAN_SOFTWARE_MAJOR, PACMAN_SOFTWARE_MINOR, PACMAN_SOFTWARE_PATCH,
           PACMAN_SOFTWARE_RELEASE ? "official" : "candidate");
}
