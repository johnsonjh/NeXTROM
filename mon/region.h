/*	@(#)region.h	1.0	03/03/87	(c) 1987 NeXT	*/

#ifndef	_REGION_
#define	_REGION_

/*
 *	Structure describing each region of contiguous physical memory.
 *	This information is provided by the monitor at boot time.
 */

typedef struct mon_region {
	long	first_phys_addr;
	long	last_phys_addr;
} *mon_region_t;

#endif	_REGION_
