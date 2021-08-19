/* @(#)expand.h	1.0	08/15/88	(c) 1988 NeXT Inc.	 */
/*
 * *************************************************************************
 *
 *
 *
 *
 * expand.h -- expands compressed pictures. ROM version 
 *
 * boot rom video compression and expansion.  Assumes 2 bit graphics, and the
 * compressed runs break at each line, so that a run will never be part of
 * 2 lines 
 *
 *
 * HISTORY 
 *
 * 11-August-88 Joe Becker (jbecker) at NeXT Created. 
 *
****************************************************************** 
 *
 *
 */

#include <sys/types.h>

struct compressed_group {
	u_char          count[4];
	u_char          value;	/* four 2 bit pixels packed into a byte */
};

/* FIXME: sizeof macro returns word aligned, but ROM data is packed */
#define TRUE_SIZE_OF_GROUP 5

/* FIXME?  Is there a clever and clear way to indicate the data is packed
as tight as possible after the number of rows? */

struct compressed_picture {
	char            type, spare[3];
	int             row_length, num_rows;
};
