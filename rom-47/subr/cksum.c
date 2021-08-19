/*	@(#)cksum.c	1.0	10/12/86	(c) 1986 NeXT	*/

#include <sys/types.h>

cksum (cp, bytes)
	caddr_t	cp;
	register u_short bytes;
{
	register u_short *sp = (u_short*) cp;
	register u_long sum = 0;
	register u_long oneword = 0x00010000;

	bytes >>= 1;
	while (bytes--) {
		sum += *sp++;
		if (sum >= oneword) {	/* wrap carries into low bit */
			sum -= oneword;
			sum++;
		}
	}
	return (sum & 0xffff);
}

