/*	@(#)prf.c	1.0	10/06/86	(c) 1986 NeXT	*/

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	from subr_prf.c	7.1 (Berkeley) 6/5/86
 */

#include <sys/types.h>
#include <nextdev/video.h>
#include <mon/global.h>
#include "varargs.h"

#define	PPRINTF		(char*) 1
#define	TPRINTF		(char*) 2

#define	FONT_W		256
#define	FONT_H		50
#define	FONT_X_INC	2
#define	FONT_Y_INC	20

struct bbox {
	u_char	x, y, w, h;
	char	oy;
} bbox[128] = {
#include "helvetica.h"
};

#ifdef gcc
#define	void	int
#endif gcc

/*
 * Print a character on console.
 */
/*ARGSUSED*/
putchar (c)
	register char c;
{
	if (c == '\n')
		mon_putc('\r');
	if (c != '\0')
		mon_putc(c);
}

putchar_panel (c)
	register u_char c;
{
	register int y, x;
	struct mon_global *mg = restore_mg();
	struct bbox *bb = &bbox[c - ' '];
	char *s, *d, *sstart, *dstart;

	if (c == '\n') {
		mg->my += FONT_Y_INC;
		mg->mx = mg->mx >> 16;
		mg->mx = mg->mx | (mg->mx << 16);
	} else {
		km_begin_access();
		sstart = (char*) KM_BACKING_STORE +
			(10 + FONT_H - bb->y - bb->h) * KM_VIDEO_NBPL +
			(bb->x * sizeof(int))/ KM_NPPW;
		dstart = (char*) KM_P_VIDEOMEM +
			(mg->my - bb->h + bb->oy) * KM_VIDEO_NBPL +
			((mg->mx & 0xffff) * sizeof(int)) / KM_NPPW;
		for (y = 0; y < bb->h; y++) {
			s = sstart;
			d = dstart;
			switch( KM_NPPW )
			{
			    case 16:	/* 2 bit pixels */
				bfins (bfextu (s, (bb->x % 4) * 2, bb->w * 2),
					d, ((mg->mx & 0xffff) % 4) * 2, bb->w * 2);
				break;
			    case 4:
			    	for ( x = 0; x < bb->w; ++x )
					*d++ = *s++;
				break;
			    case 2:
			    	for ( x = 0; x < bb->w; ++x )
					*((short *)d)++ = *((short *)s)++;
				break;
			    case 1:
			    	for ( x = 0; x < bb->w; ++x )
					*((int *)d)++ = *((int *)s)++;
				break;
			}
			sstart += KM_VIDEO_NBPL;
			dstart += KM_VIDEO_NBPL;
		}
		mg->mx += bb->w + FONT_X_INC;
		km_end_access();
	}
}

static char *
pc (c, bp)
	register char c;
	register char *bp;
{
	struct mon_global *mg = restore_mg();
	
	if (bp == PPRINTF)
		putchar_panel (c);
	else
	if (bp == TPRINTF) {
		if ((mg->km.flags & KMF_SEE_MSGS) == 0)
			*mg->test_msg++ = c;
		putchar (c);
	} else
	if (bp)
		*bp++ = c;
	else
		putchar (c);
	return (bp);
}

/*
 * Printn prints a number n in base b.
 * We don't use recursion to avoid deep kernel stacks.
 */
char *
printn(n, b, lz, fw, bp)
	u_long n;
	register char *bp;
{
	register lfw = fw;
	char prbuf[11];
	register char *cp;

	if (b == 10 && (int)n < 0) {
		bp = pc('-', bp);
		n = (unsigned)(-(int)n);
	}
	cp = prbuf;
	do {
		*cp++ = "0123456789abcdef"[n%b];
		n /= b;
	} while (n);
	for (lfw = fw - (cp-prbuf); lfw > 0; lfw--)
		bp = pc (lz? '0' : ' ', bp);
	do
		bp = pc(*--cp, bp);
	while (cp > prbuf);
	return (bp);
}

/*
 * Scaled down version of C Library printf.
 * Used to print diagnostic information directly on console tty.
 * Since it is not interrupt driven, all system activities are
 * suspended.  Printf should not be used for chit-chat.
 *
 * One additional format: %b is supported to decode error registers.
 * Usage is:
 *	printf("reg=%b\n", regval, "<base><arg>*");
 * Where <base> is the output base expressed as a control character,
 * e.g. \10 gives octal; \20 gives hex.  Each arg is a sequence of
 * characters, the first of which gives the bit number to be inspected
 * (origin 1), and the next characters (up to a control character, i.e.
 * a character <= 32), give the name of the register.  If the high octal
 * digit in the "bit number to be inspected" character = 2 then negative
 * logic is used (the name will be printed if the bit is zero).  Thus
 *	printf("reg=%b\n", 2, "\10\2BITTWO\201BITONE\n");
 * would produce output:
 *	reg=3<BITTWO,BITONE>
 *
 * Also supports printing fields within error registers.
 * If the "bit number to be inspected" character has high bits 0b110
 * then the following character specifies a field width (in bits)
 * starting at the LSB specified in the low order 5 bits of the
 * bit number character (origin 1).  For example,
 *	printf("sr=%b\n", 0x17, "\20\6S\5M\301\3IPL\n");
 * produces:
 *	sr=27<M,IPL=7>
 * If the "bit number to be inspected" character has high bits 0b111
 * then the following character specifies a field with (in bits)
 * starting at the LSB specified in the low order 5 bits of the
 * bit number character (origin 1) and the character after that
 * specifies a value to match against.  For example,
 *	printf("cr=%b\n", 0x7, "\20\341\3\0IDLE\341\3\7BUSY\n");
 * produces:
 *	cr=7<BUSY>
 */
/*VARARGS1*/
printf(fmt, va_alist)
	char *fmt;
	va_dcl
{
	va_list ap;

	va_start(ap);
	prf(fmt, 0, ap);
	va_end(ap);
}

/*VARARGS1*/
sprintf(bp, fmt, va_alist)
	char *bp, *fmt;
	va_dcl
{
	va_list ap;

	va_start(ap);
	prf(fmt, bp, ap);
	va_end(ap);
}

/* used to place text in animating panel */
/*VARARGS1*/
pprintf(x, y, clear, fmt, va_alist)
	char *fmt;
	va_dcl
{
	va_list ap;
	extern char font[];
	struct mon_global *mg = restore_mg();
	register int tx, ty, p;

	if (mg->km.flags & KMF_SEE_MSGS)
		return;
	/* Unpack a fresh copy of the Helvetica font in offscreen memory */
	drawOffScreenCompressedPicture (0, 10, font);
	
	/* clear text background */
	if (clear) {
		for (ty = 340; ty <= 440; ty++) {
			p = KM_P_VIDEOMEM + (ty * KM_VIDEO_NBPL) +
				(TEXT_X * sizeof(int))/KM_NPPW;
			for (tx = TEXT_X; tx < (TEXT_X + 90 - KM_NPPW); tx += KM_NPPW)
				*((int*)p)++ = KM_LT_GRAY;
		}
	}
	mg->mx = x | (x << 16);
	mg->my = y;
	va_start(ap);
	prf(fmt, PPRINTF, ap);
	va_end(ap);
}

/* used by self test routines so text will be saved for later */
/*VARARGS1*/
tprintf(fmt, va_alist)
	char *fmt;
	va_dcl
{
	va_list ap;

	va_start(ap);
	prf(fmt, TPRINTF, ap);
	va_end(ap);
}

prf(fmt, bp, adx)
	register char *fmt, *bp;
	va_list adx;
{
	int b, c, i, base, w, v;
	int val;
	u_char *s;
	int any, leadingzeros, fieldwidth;

loop:
	while ((c = *fmt++) != '%') {
		if (c == '\0') {
			if (bp > (char*) 256)	/* check for char* pointer */
				*bp++ = 0;
			return;
		}
		bp = pc(c, bp);
	}
	leadingzeros = fieldwidth = 0;
again:
	c = *fmt++;
	switch (c) {
	case '0':
		leadingzeros = 1;
		goto again;
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		fieldwidth = c - '0';
		goto again;
	case 'l':
		goto again;
	case 'x': case 'X':
		b = 16;
		goto number;
	case 'd': case 'D':
	case 'u':		/* what a joke */
		b = 10;
		goto number;
	case 'o': case 'O':
		b = 8;
number:
		val = va_arg(adx, int);
		bp = printn((u_long)val, b, leadingzeros, fieldwidth, bp);
		break;
	case 'c':
		b = va_arg(adx, int);
		for (i = 24; i >= 0; i -= 8)
			if (c = (b >> i) & 0x7f)
				bp = pc(c, bp);
		break;
	case 'b':
		b = va_arg(adx, int);
		s = va_arg(adx, u_char*);
		bp = printn((u_long)b, base = *s++, 1, 8, bp);
		any = 0;
		while (i = *s++) {
			if ((i&0340) == 0340) {		/* field match */
				w = *s++;
				v = *s++;
				if (((b >> ((i&037)-1)) & ((1<<w)-1)) == v) {
					bp = pc(any? ',' : '<', bp);
					any = 1;
					for (; (c = *s) > 32 &&
					    ((c&0200) == 0); s++);
						bp = pc(c, bp);
				} else
					for (; (c = *s) > 32 &&
					    ((c&0200) == 0); s++);
			} else
			if ((i&0340) == 0300) {		/* field = xxx */
				w = *s++;
				bp = pc(any? ',' : '<', bp);
				any = 1;
				for (; (c = *s) > 32 && ((c&0200) == 0); s++)
					bp = pc(c, bp);
				bp = pc('=', bp);
				bp = printn((b>>((i&037)-1)) & ((1<<w)-1),
					base, 0, 0, bp);
				} else
			if ((((i&0300) == 0) && (b & (1 << (i-1)))) ||
			    (((i&0300) == 0200) &&
			    ((b & (1<<((i&077)-1))) == 0))) {
				bp = pc(any? ',' : '<', bp);
				any = 1;
				for (; (c = *s) > 32 && ((c&0200) == 0); s++)
					bp = pc(c, bp);
			} else
				for (; (c = *s) > 32 && ((c&0200) == 0); s++);
		}
		if (any)
			bp = pc('>', bp);
		break;

	case 's':
		s = va_arg(adx, u_char *);
		while (c = *s++)
			bp = pc(c, bp);
		break;

	case '%':
		bp = pc('%', bp);
		break;
	}
	goto loop;
}




