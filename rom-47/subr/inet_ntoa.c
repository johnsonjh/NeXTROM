/*	@(#)inet_ntoa.c	1.0	10/06/86	(c) 1986 NeXT	*/

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	from	inet_ntoa.c	5.2 (Berkeley) 3/9/86
 */

/*
 * Convert network-format internet address
 * to base 256 d.d.d.d representation.
 */
#include <sys/types.h>
#include <mon/monparam.h>
#include <mon/global.h>

char *
inet_ntoa (in)
	struct in_addr in;
{
	struct mon_global *mg = (struct mon_global*) restore_mg();
	char *bp = mg->mg_inetntoa;
	register char *p;

	p = (char *)&in;
	if (in.s_addr == 0xffffffff)
		sprintf (bp, "bcast");
	else
#define	UC(b)	(((int)b)&0xff)
		sprintf(bp, "%d.%d.%d.%d",
			UC(p[0]), UC(p[1]), UC(p[2]), UC(p[3]));
	return (bp);
}
