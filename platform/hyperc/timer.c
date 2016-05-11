#include "timer_interface.h"
#include <hcos/tmr.h>

extern unsigned _HZ;

bool has_timer_expired(Timer* timer) {
	return timer->expire > tmr_ticks;
}

void countdown_ms(Timer* timer, uint32_t timeout) {
	timer->expire = tmr_ticks + (timeout*_HZ)/1000;
}

void countdown_sec(Timer* timer, uint32_t timeout) {
	timer->expire = tmr_ticks + (timeout*_HZ);
}

uint32_t left_ms(Timer* timer) {
	return ((timer->expire - tmr_ticks) * 1000)/_HZ;
}

void init_timer(Timer* timer) 
{
	timer->expire = 0;
}
