#ifndef ESPERANTO_TIMER_INTERRUPT_H
#define ESPERANTO_TIMER_INTERRUPT_H

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>

class TimerInterrupt {
	public:
		void installTimer(void (*handler)(int));

	private:
		struct sigaction sa;
		struct itimerval timer;
};

#endif
