/*	@(#)reglist.h	1.0	10/21/86	(c) 1986 NeXT	*/

#ifndef _REGLIST_
#define _REGLIST_

#define	PRE_CHECK	0
#define	POST_CHECK	1

struct reglist {
	char *rl_name;
	int rl_off;
	int (*rl_check)();
	char *rl_p1;
	int rl_p2;
};

int check_bits(), check_string(), check_enetaddr(), check_pot(), check_opt();

#endif
