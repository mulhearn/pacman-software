#ifndef __VERSION_H_
#define __VERSION_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// PACMAN SOFTWARE VERSION:

#define PACMAN_SOFTWARE_MAJOR    3
#define PACMAN_SOFTWARE_MINOR    6
#define PACMAN_SOFTWARE_PATCH    2
#define PACMAN_SOFTWARE_RELEASE  0   // 1 only in a release commit (official);  0 otherwise (candidate)

// The software version, e.g. "3.6.0 official" or "3.6.0 candidate".
void get_software_version_string(char * buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // __VERSION_H_
