/*	@(#)cursor.c	1.0	10/06/86	(c) 1986 NeXT	*/

#include <mon/cursor.h>

cursor (shape, variation) {
	char    *ind = "-\\|/";

	if (shape == CURSOR_NETXFER)
		printf ("%c\b", ind[variation]);
	if (shape == CURSOR_NETCOLL)
		printf ("X\b");
}
