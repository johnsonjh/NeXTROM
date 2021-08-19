/*	@(#)util.c	1.0	10/16/86	(c) 1986 NeXT	*/

#include <sys/types.h>
#include <mon/global.h>

char *
skipwhite (cp)
	register char *cp;
{
	while (*cp && (*cp == ' ' || *cp == '\t' || *cp == '\n' ||
	    *cp == '\r'))
		cp++;
	return (cp);
}

char *
scann (cp, num, pref)
	register char *cp;
	register int *num;
{
	register struct mon_global *mg = restore_mg();
	int sign = 0, not = 0, base, valid = 0;
	register int n, val = 0;
	register char *d;
	static char digits[] = "0123456789abcdef";
	extern char *index();

	base = pref? pref : mg->mg_radix;
	cp = skipwhite (cp);
again:
	if (*cp == '-')
		sign = 1, cp++;
	if (*cp == '~')
		not = 1, cp++;
	if (strncmp (cp, "0t", 2) == 0)
		base = 10, cp += 2;
	else
	if (strncmp (cp, "0x", 2) == 0)
		base = 16, cp += 2;
	n = 0;
	while (*cp && (d = index (digits, *cp)))
		n = n*base + d-digits, cp++, valid = 1;

	/* handle internet "dot" format (e.g. 12.34.56.78) */
	val = (val<<8) + n;
	if (*cp == '.') {
		cp++;
		goto again;
	}
	if (!valid)
		return (0);
	else
		*num = val;
	if (sign)
		*num = -*num;
	if (not)
		*num = ~*num;
	return (skipwhite (cp));
}

caddr_t
mon_alloc (size) {
	register struct mon_global *mg;
	register caddr_t mem;

	mg = restore_mg();
	mem = mg->mg_alloc_brk = mg->mg_alloc_brk - size;
	bzero (mem, size);
	return (mem);
}

