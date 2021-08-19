/* @(#)expand.c	1.0	08/15/88	(c) 1988 NeXT Inc.	 */

/*
 * expand.c -- expands compressed pictures.  ROM version
 *
 * boot rom video compression and expansion.  Assumes 2 bit graphics, and the
 * compressed runs break at each line, so that a run will never be part of
 * 2 lines.
 *
 * HISTORY
 * 29-March-90  Mike Paquette (mpaque) at NeXT
 *	Hacked and slashed to add support for multiple frame buffers.
 *
 * 29-May-89  John Seamons (jks) at NeXT
 *	Added sparse compression mode.
 *
 * 11-Aug-88  Joe Becker (jbecker) at NeXT
 *	Created. 
 */

#include "pot/POT.h"
#include "pot/expand.h"
#include "mon/global.h"
#include <next/spl.h>

/*
 * The following functions do all of the drawing operations into the frame buffer,
 * except for raw format bitmaps.  These are passed as function pointers to
 * the decompression routines.
 */
/*
 * 2 bit pixel draw code.
 */
setBytes(pixel, count, value)
    register int  *pixel;
    register u_char count, value;
{
    struct mon_global *mg = restore_mg();
    register u_char *fb = (u_char *)KM_P_VIDEOMEM;
    u_char	final_value = 0;
    int		*table= &mg->km_coni.color[0];
    int		i;

/* ASSUMES the first pixel was byte aligned */
    fb += (*pixel >> 2);
    *pixel += (count << 2);
    for ( i = 0; i < 8; i += 2 ) {
    	final_value |= ((table[(value >> i) & 3] & 3) << i);
    }
    while( count-- ) {
	*fb++ = final_value;
    }
}

/*
 * 8 bit draw code.
 */
set8Bits(pixel, count, value)
    register int  *pixel;
    register u_char count, value;
{
    struct mon_global *mg = restore_mg();
    register u_char *fb = (u_char *)KM_P_VIDEOMEM;
    int		*table= &mg->km_coni.color[0];

/* ASSUMES the first pixel was byte aligned */
    fb += *pixel;		/* Bump FB down to the first pixel */
    *pixel += (count << 2);	/* 4 pixels per 'count' */
    while( count-- ) {		/* 4 pixels are encoded within each source count */
	*fb++ = table[(value >> 6) & 3];
	*fb++ = table[(value >> 4) & 3];
	*fb++ = table[(value >> 2) & 3];
	*fb++ = table[value & 3];
    }
}

/*
 * 16 bit draw code.
 */
set16Bits(pixel, count, value)
    register int  *pixel;
    register u_char count, value;
{
    struct mon_global *mg = restore_mg();
    register u_short *fb = (u_short *)KM_P_VIDEOMEM;
    int		*table= &mg->km_coni.color[0];

/* ASSUMES the first pixel was byte aligned */
    fb += *pixel;		/* Bump FB down to the first pixel */
    *pixel += (count << 2);	/* 4 pixels per 'count' */
    while( count-- ) {		/* 4 pixels are encoded within each source count */
	*fb++ = table[(value >> 6) & 3];
	*fb++ = table[(value >> 4) & 3];
	*fb++ = table[(value >> 2) & 3];
	*fb++ = table[value & 3];
    }
}

/*
 * 32 bit draw code.
 */
set32Bits(pixel, count, value)
    register int  *pixel;
    register u_char count, value;
{
    struct mon_global *mg = restore_mg();
    register u_int *fb = (u_int *)KM_P_VIDEOMEM;
    int		*table= &mg->km_coni.color[0];

/* ASSUMES the first pixel was byte aligned */
    fb += *pixel;		/* Bump FB down to the first pixel */
    *pixel += (count << 2);	/* 4 pixels per 'count' */
    while( count-- ) {		/* 4 pixels are encoded within each source count */
	*fb++ = table[(value >> 6) & 3];
	*fb++ = table[(value >> 4) & 3];
	*fb++ = table[(value >> 2) & 3];
	*fb++ = table[value & 3];
    }
}

/*
 * This is called to unpack the Helvetica font into off-screen memory.
 * It isn't pretty, but it saves considerable stack and code space when 
 * we need it most.
 */
drawOffScreenCompressedPicture( x0, y0, picture )
	int             x0, y0;
	struct compressed_picture *picture;
{
	struct mon_global *mg = restore_mg();
	int save_fb = KM_P_VIDEOMEM;
	int s;
	
	s = spl7();
	KM_P_VIDEOMEM = KM_BACKING_STORE;
	drawCompressedPicture( x0, y0, picture );
	KM_P_VIDEOMEM = save_fb;
	splx(s);
}
/*
 * This is called to do all of the animation image drawing.
 */
drawCompressedPicture(x0, y0, picture)
	int             x0, y0;
	struct compressed_picture *picture;
{
	struct mon_global *mg = restore_mg();
	int             left, right;
	int             row_length, num_rows;
	int		(*draw_func)();
	/* Find left edge of picture and right edge of screen */

	left = x0 + y0 * KM_VIDEO_MW;
	/* right = KM_VIDEO_W + y0 * KM_VIDEO_MW; */

	/*
	 * FIXME: this is for the right edge, need to fiddle pixel in
	 * addition to getting the limits right !! 
	 */

	/* Make sure we don't draw past the visible region */

	row_length = picture->row_length;
#if 0
	if (row_length + x0 > KM_VIDEO_W) {
		row_length = KM_VIDEO_W - x0;
		if (row_length <= 0)
			return (-1);
	}
#endif
	num_rows = picture->num_rows;
#if 0
	if (num_rows + y0 > KM_VIDEO_H) {
		num_rows = KM_VIDEO_H - y0;
		if (num_rows <= 0)
			return (-1);
	}
#endif
/* printf("length %d, rows %d, , left %x, picture %x\n",row_length,num_rows,left,picture); */

	switch( KM_NPPW )
	{
		case 16:	/* 2 bit */
			draw_func = setBytes;
			break;
		case 4:		/* 8 bit pixels */
			draw_func = set8Bits;
			break;
		case 2:		/* 16 bit pixels */
			draw_func = set16Bits;
			break;
		case 1:		/* 32 bit pixels */
			draw_func = set32Bits;
	}
#if 0
	if (*(char *)picture == 'P')
		drawPixels(row_length, num_rows, left, ++picture);
	else
#endif
	km_begin_access();
	if (*(char *)picture == 'B')
		drawBytes(row_length, num_rows, left, ++picture, draw_func);
	else if (*(char *)picture == 'U')
		drawRaw(row_length, num_rows, left, ++picture, draw_func);
	else if (*(char *)picture == 'S')
		drawSparse(row_length, num_rows, left, ++picture, draw_func);
	km_end_access();
	return (0);
}

#if 0
drawPixels(row_length, num_rows, left, group)
	int             row_length, num_rows, left;
	struct compressed_group *group;

{
	u_char          count, value, element;
	u_int		row, col;
	int		pixel = left;

	/* draw each row till out of rows... */
	row = col = element = 0;

	while (row < num_rows) {
		while (col < row_length) {
			value = (group->value >> (6 - 2 * element)) & 3;
			count = group->count[element++];
			if (element > 3) {
				element = 0;
				group = (struct compressed_group *)
					((int)group + TRUE_SIZE_OF_GROUP);
			}
			setPixels(&pixel, count, value);
			col += count;
		}
		row++;
		col = 0;
		pixel = left += VIDEO_MW;
	}
	return(0);
}

setPixels(pixel, count, value)
	register int   *pixel;
	register u_char count, value;
{
	/*
	 * Sleazy routine to expand a pixel compressed picture; should 
	 *
	 * 1) draw the 0-3 pixels to a byte boundary in VRAM 2) draw a byte
	 * of values till <4 pixels remain 3) draw the 0-3 remaining
	 * pixels... 
	 *
	 * I'll fix all that once we get the basics working... 
	 *
	 * Routine uses the fact that if there are 4 pixels per byte, pixel/4
	 * is byte offset from start of VRAM, and the 2 bit field starts
	 * 2*(pixel %4) bits right from the byte boundary. 
	 *
	 * Don't forget that pixel INCLUDES NON_VISIBLE PIXELS. 
	 */

	register u_char *adr, offset;
	register u_int  orValue = value << 8, temp;

	for (; count > 0; count--) {
		adr = (u_char *) (P_VIDEOMEM + (*pixel >> 2));
		offset = (1 + (*pixel & 0x3)) << 1;
		*adr = (((*adr << offset) & ~0x300) | orValue) >> offset;
		++*pixel;
	}

}
#endif


drawBytes(row_length, num_rows, left, data, draw_func)
	int             row_length, num_rows, left;
	char		*data;
	int		(*draw_func)();
{
	struct mon_global *mg = restore_mg();
	u_char          count, value;
	u_int		row, col;
	int		pixel;

	/* draw each row till out of rows... */
	row = col = 0;
	pixel = left;
	while (row < num_rows) {
		while (col < row_length) {
			count = *data++;
			value = *data++;
			(draw_func)(&pixel, count, value);
			col += count<<2;
		}
		row++;
		col = 0;
		pixel = left += KM_VIDEO_MW;
	}
	return(0);
}

drawRaw(row_length, num_rows, left, data, draw_func)
	int             row_length, num_rows, left;
	char		*data;
	int		(*draw_func)();
{
	struct mon_global *mg = restore_mg();
	u_int		row, col;
	int		*table= &mg->km_coni.color[0];

	switch( KM_NPPW )
	{
	    case 16:	/* 2 bit pixels */
		/* ASSUMES the first pixel was byte aligned */
		/* draw each row till out of rows... */
		for ( row = 0; row < num_rows; ++row ) {
			u_char *fb = (u_char *)(KM_P_VIDEOMEM + (left >> 2));
			if (  KM_BLACK != BLACK ) {
			    u_char in, out;
			    int i;
			    for ( col = row_length; col > 0; col -= 4 ) {
				in = *data++;
				out = 0;
				for ( i = 0; i < 8; i += 2 )
				    out |= ((table[(in>>i)&3] & 3) << i);
				*fb++ = out;
			    }
			} else {
			    for ( col = row_length; col > 0; col -= 4 )
				    *fb++ = *data++;
			}
			left += KM_VIDEO_MW;
		}
		break;
	    case 4:	/* 8 bit pixels */
		for ( row = 0; row < num_rows; ++row ) {
			u_char *fb = ((u_char *)KM_P_VIDEOMEM) + left;
			for ( col = row_length; col > 0; col -= 4 ) {
			    *fb++ = table[(*data >> 6) & 3];
			    *fb++ = table[(*data >> 4) & 3];
			    *fb++ = table[(*data >> 2) & 3];
			    *fb++ = table[*data & 3];
			    ++data;
			}
			left += KM_VIDEO_MW;
		}
		break;
	    case 2:	/* 16 bit pixels */
		for ( row = 0; row < num_rows; ++row ) {
			u_short *fb = ((u_short *)KM_P_VIDEOMEM) + left;
			for ( col = row_length; col > 0; col -= 4 ) {
			    *fb++ = table[(*data >> 6) & 3];
			    *fb++ = table[(*data >> 4) & 3];
			    *fb++ = table[(*data >> 2) & 3];
			    *fb++ = table[*data & 3];
			    ++data;
			}
			left += KM_VIDEO_MW;
		}
		break;
	    case 1:	/* 32 bit pixels */
		for ( row = 0; row < num_rows; ++row ) {
			u_int *fb = ((u_int *)KM_P_VIDEOMEM) + left;
			for ( col = row_length; col > 0; col -= 4 ) {
			    *fb++ = table[(*data >> 6) & 3];
			    *fb++ = table[(*data >> 4) & 3];
			    *fb++ = table[(*data >> 2) & 3];
			    *fb++ = table[*data & 3];
			    ++data;
			}
			left += KM_VIDEO_MW;
		}
		break;
	}
}

drawSparse(row_length, num_rows, left, data, draw_func)
	int             row_length, num_rows, left;
	u_char		*data;
	int		(*draw_func)();
{
	struct mon_global *mg = restore_mg();
	u_char          count, value;
	u_int		row, col;
	int		pixel, tabsize, i;
	u_char		values[256], runlen[256];
	
	tabsize = *data++;
	for (i = 0; i < tabsize; i++)
		values[i] = *data++,  runlen[i] = *data++;

	row = col = 0;
	/* draw each row till out of rows... */
	row = col = 0;
	pixel = left;
	while (row < num_rows) {
		while (col < row_length) {
			i = *data++;
			count = runlen[i];
			value = values[i];
			(draw_func)(&pixel, count, value);
			col += count<<2;
		}
		row++;
		col = 0;
		pixel = left += KM_VIDEO_MW;
	}
	return(0);
}




