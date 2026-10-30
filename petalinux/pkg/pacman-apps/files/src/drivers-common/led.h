#ifndef __LED_H__
#define __LED_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void init_led();
void blink_red_led();
void blink_pacman_leds();

// Blink the Trenz and PACMAN LEDs:  with pause, until the operator presses Enter;  without, a fixed
// time.
void led_check_blink(bool pause);

#ifdef __cplusplus
}
#endif


#endif // __LED_H__
