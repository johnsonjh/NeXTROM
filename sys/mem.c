/*	@(#)mem.c	1.0	10/27/86	(c) 1986 NeXT	*/

#include <sys/types.h>
#include <mon/global.h>
#include <mon/reglist.h>

#define	IMAX	128
#define	LONG	4
#define	WORD	2
#define	BYTE	1

examine (lp, fcode)
	register char *lp;
{
	register struct mon_global *mg = restore_mg();
	int size = LONG, val, i;
	int getl(), getw(), getb(), (*get)() = getl;
	int putl(), putw(), putb(), (*put)() = putl;
	char *skipwhite(), in_line[IMAX], *ip = in_line, *fmt = "%08x",
		*scann(), *tlp;

	lp = skipwhite (lp);
	if (*lp == 0)
		*lp = mg->fmt, *(lp+1) = 0;
	switch (*lp) {
	case 'l':
		mg->fmt = *lp;
		size = LONG;
		get = getl;  put = putl;
		fmt = "%08x";
		lp++;
		break;
	case 'w':
		mg->fmt = *lp;
		size = WORD;
		get = getw;  put = putw;
		fmt = "%04x";
		lp++;
		break;
	case 'b':
		mg->fmt = *lp;
		size = BYTE;
		get = getb;  put = putb;
		fmt = "%02x";
		lp++;
		break;
	}
	if (*lp) {
		for (mg->na = 0; mg->na < NADDR; mg->na++) {
			if ((tlp = scann (lp, &mg->addr[mg->na], 0)) == 0)
				break;
			lp = tlp;
			mg->addr[mg->na] &= ~ (size - 1);
		}
		if (mg->na == 0) {
fail:
			type_help();
			return;
		}
		lp = skipwhite (lp);
		if (*lp)
			fmt = lp;
	} else
		if (mg->na == 0)
			goto fail;
	i = 0;
	while (1) {
		printf ("%x: ", mg->addr[i]);
		printf (fmt, (*get) (mg->addr[i], 1, fcode));
		printf ("? ");
		gets (ip, IMAX, 1);
		if (*ip) {
			if (scann (ip, &val, 0) == 0)
				return;
			(*put) (mg->addr[i], 1, fcode, val);
		}
		if (mg->na == 1)
			mg->addr[0] += size;
		else
			i = ++i % mg->na;
	}
}

#if 0
view (lp, fcode)
	register char *lp;
{
	register int i, v;
	int size = LONG, addr, stop, val;
	char *skipwhite(), *scann();

	lp = skipwhite (lp);
	switch (*lp) {
	case 'l':
		size = LONG;
		lp++;
		break;
	case 'w':
		size = WORD;
		lp++;
		break;
	case 'b':
		size = BYTE;
		lp++;
		break;
	}
	if ((lp = scann (lp, &addr, 0)) == 0)
		goto usage;
	if ((lp = scann (lp, &stop, 0)) == 0)
		goto usage;
	addr &= ~15;
	for (; addr < (stop&~15); addr += 16) {
		printf ("%08x: ", addr);
		for (i = 0; i < 16; i += size)
			switch (size) {
			case LONG:
				printf ("%08x ",
					getl (addr + i, 1, fcode));
				break;
			case WORD:
				printf ("%04x ",
					getw (addr + i, 1, fcode));
				break;
			case BYTE:
				printf ("%02x ",
					getb (addr + i, 1, fcode));
				break;
			}
		printf ("    ");
		for (i = 0; i < 16; i++) {
			v = getb (addr + i, 1, fcode);
			printf ("%c", (v < ' ' || v > 0x7e) ? '.' : v);
		}
		printf ("\n");
		if (mon_try_getc())
			break;
	}
	return;
usage:
	type_help();
}

fill (lp, fcode)
	register char *lp;
{
	register int i, v;
	int size = LONG, addr, stop, val;
	u_int pattern, incr = 0, modulo = (u_int) -1;
	int putl(), putw(), putb(), (*put)() = putl;
	char *skipwhite(), *scann();

	lp = skipwhite (lp);
	switch (*lp) {
	case 'l':
		size = LONG;
		put = putl;
		lp++;
		break;
	case 'w':
		size = WORD;
		put = putw;
		lp++;
		break;
	case 'b':
		size = BYTE;
		put = putb;
		lp++;
		break;
	}
	if ((lp = scann (lp, &addr, 0)) == 0)
		goto usage;
	if ((lp = scann (lp, &stop, 0)) == 0)
		goto usage;
	if ((lp = scann (lp, &pattern, 0)) == 0)
		goto usage;
	if (lp = scann (lp, &incr, 0))
		scann (lp, &modulo, 0);
	addr &= ~(size - 1);
	stop &= ~(size - 1);
	for (; addr < stop; addr += size) {
		(*put) (addr, 1, fcode, pattern);
		pattern = (pattern + incr) % modulo;
	}
	return;
usage:
	type_help();
}
#endif
