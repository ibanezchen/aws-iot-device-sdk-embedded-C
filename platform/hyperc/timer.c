#include "timer_interface.h"
#include <hcos/tmr.h>
#include "plt.h"

bool has_timer_expired(Timer* timer) {
	return timer->expire < tmr_ticks;
}

void countdown_ms(Timer* timer, uint32_t timeout) {
	timer->expire = tmr_ticks + (timeout*PLT_HZ)/1000;
}

void countdown_sec(Timer* timer, uint32_t timeout) {
	timer->expire = tmr_ticks + (timeout*PLT_HZ);
}

uint32_t left_ms(Timer* timer) {
	return ((timer->expire - tmr_ticks) * 1000)/PLT_HZ;
}

void init_timer(Timer* timer) 
{
	timer->expire = 0;
}
