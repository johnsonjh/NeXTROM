/* @(#)pics.c	1.0	08/15/88	(c) 1988 NeXT Inc.	 */

/*
 * picsp.c -- Compressed pictures.  ROM VERSION
 *
 * HISTORY
 * 28-Sep-88  John Seamons (jks) at NeXT
 *	Added new icons.
 *
 * 15-Aug-88  Joe Becker (jbecker) at NeXT
 *	Created.
 *
 */

char test[] = {
#if	NO_SPACE
#else	NO_SPACE
#include "../bitmaps/cube88x88x2.squ"
#endif	NO_SPACE
};

char font[] = {
#if	NO_SPACE
#else	NO_SPACE
#include "../bitmaps/bootfont256x50x2.squ"
#endif	NO_SPACE
};

char loginwindow[] = {
#if	NO_SPACE
#else	NO_SPACE
#include "../bitmaps/nextbox440x176x2.squ"
#endif	NO_SPACE
};

char od_icon[] = {
#if	defined(NO_SPACE) || !defined(ODBOOT)
#else	NO_SPACE
#include "../bitmaps/grayoptical180x84x2.squ"
#endif	NO_SPACE
};

char od_icon2[] = {
#if	defined(NO_SPACE) || !defined(ODBOOT)
#else	NO_SPACE
#include "../bitmaps/grayoptical272x41x2.squ"
#endif	NO_SPACE
};

char od_icon3[] = {
#if	defined(NO_SPACE) || !defined(ODBOOT)
#else	NO_SPACE
#include "../bitmaps/grayoptical372x41x2.squ"
#endif	NO_SPACE
};

char od_arrow1[] = {
#if	defined(NO_SPACE) || !defined(ODBOOT)
#else	NO_SPACE
#include "../bitmaps/diskarrow116x17x2.squ"
#endif	NO_SPACE
};

char od_arrow2[] = {
#if	defined(NO_SPACE) || !defined(ODBOOT)
#else	NO_SPACE
#include "../bitmaps/diskarrow216x17x2.squ"
#endif	NO_SPACE
};

char od_arrow3[] = {
#if	defined(NO_SPACE) || !defined(ODBOOT)
#else	NO_SPACE
#include "../bitmaps/diskarrow316x17x2.squ"
#endif	NO_SPACE
};

char od_flip[] = {
#if	defined(NO_SPACE) || !defined(ODBOOT)
#else	NO_SPACE
#include "../bitmaps/flipdisk32x20x2.squ"
#endif	NO_SPACE
};

char od_bad[] = {
#if	defined(NO_SPACE) || !defined(ODBOOT)
#else	NO_SPACE
#include "../bitmaps/badgraydisk80x41x2.squ"
#endif	NO_SPACE
};

char en_icon[] = {
#if	defined(NO_SPACE) || !defined(NETBOOT)
#else	NO_SPACE
#include "../bitmaps/grayethernet188x84x2.squ"
#endif	NO_SPACE
};

char en_icon2[] = {
#if	defined(NO_SPACE) || !defined(NETBOOT)
#else	NO_SPACE
#include "../bitmaps/grayethernet288x63x2.squ"
#endif	NO_SPACE
};

char en_icon3[] = {
#if	defined(NO_SPACE) || !defined(NETBOOT)
#else	NO_SPACE
#include "../bitmaps/grayethernet388x63x2.squ"
#endif	NO_SPACE
};

char sd_icon[] = {
#if	defined(NO_SPACE) || !defined(SCSIBOOT)
#else	NO_SPACE
#include "../bitmaps/grayscsi180x95x2.squ"
#endif	NO_SPACE
};

char sd_icon2[] = {
#if	defined(NO_SPACE) || !defined(SCSIBOOT)
#else	NO_SPACE
#include "../bitmaps/grayscsi272x65x2.squ"
#endif	NO_SPACE
};

char sd_icon3[] = {
#if	defined(NO_SPACE) || !defined(SCSIBOOT)
#else	NO_SPACE
#include "../bitmaps/grayscsi372x65x2.squ"
#endif	NO_SPACE
};

#ifdef	notdef
char testing[] = {
#include "testings80x40x2.squ"
};

char please[] = {
#include "please64x20x2.squ"
};

char insert[] = {
#include "insert56x20x2.squ"
};

char flip[] = {
#include "flip32x20x2.squ"
};

char bad[] = {
#include "bad40x20x2.squ"
};

char loading[] = {
#include "loading72x20x2.squ"
};

char from[] = {
#include "from48x20x2.squ"
};

char disk[] = {
#include "disk48x20x2.squ"
};

char network[] = {
#include "network80x20x2.squ"
};

char dots[] = {
#include "dots16x8x2.squ"
};

char digits[10][32] = {
{
#include "graydigit08x10x2.squ"
},
{
#include "graydigit18x10x2.squ"
},
{
#include "graydigit28x10x2.squ"
},
{
#include "graydigit38x10x2.squ"
},
{
#include "graydigit48x10x2.squ"
},
{
#include "graydigit58x10x2.squ"
},
{
#include "graydigit68x10x2.squ"
},
{
#include "graydigit78x10x2.squ"
},
{
#include "graydigit88x10x2.squ"
},
{
#include "graydigit98x10x2.squ"
},
};

char logoIcon[] = {
#include "logo.squ"
};

char simmIcon256KB[] = {
#include "simmIcon256KB.squ"
};

char simmIcon1MB[] = {
#include "simmIcon1MB.squ"
};

char simmIcon4MB[] = {
#include "simmIcon4MB.squ"
};
#endif	notdef
