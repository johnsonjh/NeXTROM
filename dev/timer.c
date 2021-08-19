/*	@(#)timer.c	1.0	5/20/87	(c) 1987 NeXT	*/

#include <sys/types.h>
#include <mon/global.h>

#define	EVENTC_LATCH	(volatile u_char*) (P_EVENTC + 0)
#define	EVENTC_H	(volatile u_char*) (P_EVENTC + 1)
#define	EVENTC_M	(volatile u_char*) (P_EVENTC + 2)
#define	EVENTC_L	(volatile u_char*) (P_EVENTC + 3)
#define EVENT_HIGHBIT 0x80000
#define EVENT_MASK 0xfffff

/* Return 32 bit representation of event counter */
unsigned int event_get(void)
{
	register struct mon_global *mg = restore_mg();
	u_int high, low;

	high = mg->event_high;
	low = *EVENTC_LATCH;	/* load the latch from the event counter */
	low = (*EVENTC_H << 16) | (*EVENTC_M << 8) | *EVENTC_L;
	low &= EVENT_MASK;
	if ((high ^ low) & EVENT_HIGHBIT)
		high += EVENT_HIGHBIT;
	mg->event_high = high;
	return (high | low);
}

/*
 * Returns the value of a free running, millisecond resolution clock.
 */
mon_time() {
	return (event_get() / 1000);
}

#if 0
/*
 *	Delay for the specified number of microseconds.
 */
delay(n)
	int	n;
{
	register int d = event_get();

	while (event_get() - d < (n+1))
		continue;
}
#endif

