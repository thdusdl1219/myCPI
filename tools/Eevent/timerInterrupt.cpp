#include "timerInterrupt.h"

void TimerInterrupt::installTimer(void (*handler)(int)) {
	/* Install timer_handler as the signal handler for SIGVTALRM. */
	memset (&sa, 0, sizeof (sa));
	sa.sa_handler = handler;
	sigaction (SIGVTALRM, &sa, NULL);

	/* Configure the timer to expire after 500 msec... */
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 500000;

	/* ... and every 500 msec after that. */
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 500000;

	/* Start a virtual timer. It counts down whenever this process is executing. */
	setitimer (ITIMER_VIRTUAL, &timer, NULL);
}
