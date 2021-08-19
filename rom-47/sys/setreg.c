/*	@(#)setreg.c	1.0	10/21/86	(c) 1986 NeXT	*/

#include <sys/types.h>
#include <mon/reglist.h>
#include <mon/nvram.h>
#include <mon/tftp.h>

#define	IMAX	128

extern char *scann();

setreg (lp, rlist, r, prefix)
	register char *lp;
	struct reglist *rlist;
	register int r;
	char *prefix;
{
	register struct reglist *rl;
	int modified = 0, val, i;
	char *skipwhite();
	char input[IMAX], *ip = input;

	lp = skipwhite (lp);
	for (rl = rlist; rl->rl_name; rl++) {
		if (*lp == 0 || strcmp (lp, rl->rl_name) == 0) {
			if (rl->rl_name[0])
				printf ("%s%s: ", prefix, rl->rl_name);
			if (rl->rl_check == 0)
				printf ("%08x? ",
					*(int*)((int) r + rl->rl_off));
			else
				if ((*rl->rl_check) (PRE_CHECK, r, rl))
					continue;
			gets (ip, IMAX, 1);
			if (*ip == 0)
				continue;
			if (rl->rl_check == 0) {
				if (*ip == '.')
					break;
				if (login(0) == 0)
					break;
				if (scann (ip, &val, 16) != 0)
					*(int*) ((int) r + rl->rl_off) = val;
				else
					break;
			} else {

				/*
				 * POST_CHECK routines return:
				 *  0: keep going
				 * -1: stop
				 *  1: stop but modified
				 */
				i = (*rl->rl_check) (POST_CHECK, r, rl, ip);
				if (i == -1)
					break;
				modified = 1;
				if (i == 1)
					break;
			}
			modified = 1;
		}
	}
	return (modified);
}

check_bits (check, r, rl, ip)
	register int r;
	register struct reglist *rl;
	register char *ip;
{
	int val;

	switch (check) {

	case PRE_CHECK:
		printf ("%b? ", *(int*) ((int) r + rl->rl_off), rl->rl_p1);
		return (0);

	case POST_CHECK:
		if (scann (ip, &val, 16) == 0)
			return (-1);
		if (login(0) == 0)
			return (-1);
		*(int*) ((int) r + rl->rl_off) = val;
		return (0);
	}
}

check_string (check, r, rl, ip)
	register int r;
	register struct reglist *rl;
	register char *ip;
{
	switch (check) {

	case PRE_CHECK:
		printf ("%s? ", (char*) ((int) r + rl->rl_off));
		return (0);

	case POST_CHECK:
		if (login(0) == 0)
			return (-1);
		if (*ip == '.') {
			*(char*) ((int) r + rl->rl_off) = 0;
			return (1);
		}
		if (strlen (ip) >= (int) rl->rl_p1) {
			printf ("must be < %d chars long\n",
				(int) rl->rl_p1);
			return (-1);
		}
		strcpy ((char*) ((int) r + rl->rl_off), ip);
		return (0);
	}
}

#if	DEBUG
check_enetaddr (check, r, rl, ip)
	register int r;
	register struct reglist *rl;
	register char *ip;
{
	register int i;
	register struct nvram_info *ni = (struct nvram_info*) r;
	int val;
	extern u_char no_addr[];

	switch (check) {

#define	EA	ni->ni_enetaddr

	case PRE_CHECK:
		printf (": %x:%x:%x:%x:%x:%x? ",
			EA[0], EA[1], EA[2], EA[3], EA[4], EA[5]);
		return (0);

	case POST_CHECK:
		if (*ip == '.')
			return (-1);
		if (login(0) == 0)
			return (-1);
		for (i = 0; i < 6; i++) {
			if ((ip = scann (ip, &val, 16)) == 0)
				goto fail;
			if (*ip && *ip != ':' && (*ip < '0' || *ip > 'f'))
				goto fail;
			EA[i] = val;
			if (*ip == ':')
				ip++;
		}
		ni->ni_hw_pwd = 0;	/* because they share the NVRAM */
		return (0);
	}
fail:
	type_help();
	return (-1);
}
#endif	DEBUG

check_pot (check, r, rl, ip)
	register int r;
	register struct reglist *rl;
	register char *ip;
{
	register struct nvram_info *ni = (struct nvram_info*) r;

	switch (check) {

	case PRE_CHECK:
		printf (" %s? ", ni->ni_pot[0] & rl->rl_off? "yes" : "no");
		return(0);

	case POST_CHECK:
		if (*ip == '.')
			return (-1);
		if (login(0) == 0)
			return (-1);
		if (*ip == 'y' || *ip == 'Y') {
			ni->ni_pot[0] |= rl->rl_off;
			return (0);
		} else
		if (*ip == 'n' || *ip == 'N') {
			ni->ni_pot[0] &=~rl->rl_off;
			return (0);
		} else
		return (-1);
	}		

}

check_opt (check, r, rl, ip)
	register int r;
	register struct reglist *rl;
	register char *ip;
{
	u_int *opt = (u_int*) r;

	switch (check) {

	case PRE_CHECK:
		printf (" %s? ", *opt & rl->rl_off? "yes" : "no");
		return(0);

	case POST_CHECK:
		if (*ip == '.')
			return (-1);
		if (login(1) == 0)
			return (-1);
		if (*ip == 'y' || *ip == 'Y') {
			*opt |= rl->rl_off;
			return (0);
		} else
		if (*ip == 'n' || *ip == 'N') {
#if	ODBOOT
			if (rl->rl_off == ALLOW_EJECT && od_inserted() == 0) {
				printf ("There must be a disk inserted in "
					"drive #0 before you can set this "
					"option\n");
				return (-1);
			}
#endif	ODBOOT
			*opt &= ~rl->rl_off;
			return (0);
		} else
		return (-1);
	}		

}
