/*	@(#)getput.c	1.0	10/10/86	(c) 1986 NeXT	*/

#include <sys/types.h>
#include <mon/global.h>

mon_getc () {
	register struct mon_global *mg = restore_mg();

	switch (mg->mg_console_i) {
		case CONS_I_KBD:
			return (kmgetc (0));
		case CONS_I_SCC_A:
			return (getc (0));
		case CONS_I_SCC_B:
			return (getc (1));
		case CONS_I_NET:
			return;
	}
}

mon_try_getc () {
	register struct mon_global *mg = restore_mg();

	switch (mg->mg_console_i) {
		case CONS_I_KBD:
			return (kmis_char (0));
		case CONS_I_SCC_A:
			return (is_char (0));
		case CONS_I_SCC_B:
			return (is_char (1));
		case CONS_I_NET:
			return;
	}
}

mon_putc (c)
	char c;
{
	register struct mon_global *mg = restore_mg();

again:
	switch (mg->mg_console_o) {
		case CONS_O_BITMAP:
			if (kmputc (0, c) == 0)
				goto again;	/* console changed */
			break;
		case CONS_O_SCC_A:
			putc (0, c);
			break;
		case CONS_O_SCC_B:
			putc (1, c);
			break;
		case CONS_O_NET:
			break;
	}
}
