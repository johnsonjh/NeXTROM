/*	@(#)srec.c	1.0	9/29/87		(c) 1986 NeXT	*/

#define	DEBUG	1

#if	DEBUG
#define	dbug	if (si->si_unit & 1) printf
#define	dbug1	if (si->si_unit & 2) printf
#define	dbug2	if (si->si_unit & 4) printf
#endif	DEBUG

#include <sys/types.h>
#include <mon/monparam.h>
#include <mon/cursor.h>
#include <mon/sio.h>
#include <mon/global.h>
#include <mon/nvram.h>

int srec_open(), srec_close(), srec_boot();
struct protocol proto_srec = { srec_open, srec_close, srec_boot };

#if	FIXME
#define	getch()		((u_char) (si->si_dev->d_read) (1))
#else	FIXME
#define	getch()		((u_char) (si->si_dev->d_read) (0))
#endif	FIXME

srec_open (mg, si)
	register struct mon_global *mg;
	register struct sio *si;
{
	return (si->si_dev->d_open(si));
}

srec_close (mg, si)
	register struct mon_global *mg;
	register struct sio *si;
{
	return (si->si_dev->d_close (si));
}

srec_boot (mg, si, linep)
	struct mon_global *mg;
	struct sio *si;
	char **linep;
{
	register u_char *bp;
	register int i, cnt = 0;
	register u_int len, addr, csum;
	u_char getbyte(), c;

	while (1) {
		while ((c = getch()) != 'S')
			;
		if ((c = getch()) == '7') {
			len = getbyte(si);
			addr = 0;
			for (i=0; i<4; i++)
				addr = (addr << 8) | getbyte(si);
			printf ("\nstarting at 0x%x\n", addr);
			return (addr);
		} else
		if (c == '0')
			continue;
		else
		if (c != '3') {
			printf ("\n%d: unknown type: S%c\n", cnt, c);
			continue;
		}
		len = getbyte(si);
		csum = len;
		addr = 0;
		for (i=0; i<4; i++) {
			c = getbyte(si);
			addr = (addr << 8) | c;
			csum += c;
		}
		bp = (u_char*) addr;
		len -= 5;
		for (i=0; i<len; i++) {
			c = getbyte(si);
			*bp++ = c;
			csum += c;
		}
		c = getbyte(si);
		csum = ~csum & 0xff;
		if (csum != c)
			printf ("\n%d: checksum error, expected 0x%x got 0x%x\n",
				cnt, csum, c);
		/* printf ("%d\r", cnt++); */
	}
}

u_char val[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 0, 10, 11, 12, 13, 14, 15
/*  0  1  2  3  4  5  6  7  8  9  :  ;  <  =  >  ?  @   A   B   C   D   E   F
 */
};

u_char
getbyte(si)
	register struct sio *si;
{
	u_char v;

	v = val[getch() - '0'] << 4;
	v |= val[getch() - '0'];
	return (v);
}
