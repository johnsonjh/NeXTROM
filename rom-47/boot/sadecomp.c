#include <sys/types.h>
#include <mon/monparam.h>
#include <mon/sio.h>
#include <mon/global.h>
#include <mon/nvram.h>

int comp_open(), comp_close(), comp_boot();
struct protocol proto_comp = { comp_open, comp_close, comp_boot };

#if	FIXME
#define	getc(d)		((char) (si->si_dev->d_read) (1 | RAW_IO))
#define	putc(d, c)	(si->si_dev->d_write) (1 | RAW_IO, (c))
#else	FIXME
#define	getc(d)		((char) (si->si_dev->d_read) (0 | RAW_IO))
#define	putc(d, c)	(si->si_dev->d_write) (0 | RAW_IO, (c))
#endif	FIXME

#ifndef SACREDMEM
#define SACREDMEM	0
#endif

# ifndef USERMEM
#  define USERMEM 750000	/* default user memory */
# endif

#ifdef USERMEM
# if USERMEM >= (2621440+SACREDMEM)
#  if USERMEM >= (4718592+SACREDMEM)
#   define FBITS		13
#   define PBITS	16
#  else 2.5M <= USERMEM < 4.5M
#   define FBITS		12
#   define PBITS	16
#  endif USERMEM <=> 4.5M
# else USERMEM < 2.5M
#  if USERMEM >= (1572864+SACREDMEM)
#   define FBITS		11
#   define PBITS	16
#  else USERMEM < 1.5M
#   if USERMEM >= (1048576+SACREDMEM)
#    define FBITS		10
#    define PBITS	16
#   else USERMEM < 1M
#    if USERMEM >= (631808+SACREDMEM)
#     define PBITS	16
#    else
#     if USERMEM >= (329728+SACREDMEM)
#      define PBITS	15
#     else
#      if USERMEM >= (178176+SACREDMEM)
#       define PBITS	14
#      else
#       if USERMEM >= (99328+SACREDMEM)
#        define PBITS	13
#       else
#        define PBITS	12
#       endif
#      endif
#     endif
#    endif
#    undef USERMEM
#   endif USERMEM <=> 1M
#  endif USERMEM <=> 1.5M
# endif USERMEM <=> 2.5M
#endif USERMEM

#ifdef PBITS		/* Preferred BITS for this memory size */
# ifndef BITS
#  define BITS PBITS
# endif BITS
#endif PBITS

#if BITS == 16
# define HSIZE	69001		/* 95% occupancy */
#endif
#if BITS == 15
# define HSIZE	35023		/* 94% occupancy */
#endif
#if BITS == 14
# define HSIZE	18013		/* 91% occupancy */
#endif
#if BITS == 13
# define HSIZE	9001		/* 91% occupancy */
#endif
#if BITS == 12
# define HSIZE	5003		/* 80% occupancy */
#endif
#if BITS == 11
# define HSIZE	2591		/* 79% occupancy */
#endif
#if BITS == 10
# define HSIZE	1291		/* 79% occupancy */
#endif
#if BITS == 9
# define HSIZE	691		/* 74% occupancy */
#endif
/* BITS < 9 will cause an error */

/*
 * a code_int must be able to hold 2**BITS values of type int, and also -1
 */
#if BITS > 15
typedef long int	code_int;
#else
typedef int		code_int;
#endif

typedef long int	  count_int;

typedef	unsigned char	char_type;
char_type magic_header[] = { "\037\235" };	/* 1F 9D */

/* Defines for third byte of header */
#define BIT_MASK	0x1f
#define BLOCK_MASK	0x80
/* Masks 0x40 and 0x20 are free.  I think 0x20 should mean that there is
   a fourth header byte (for expansion).
*/
#define INIT_BITS 9			/* initial number of bits/code */

#define ARGVAL() (*++(*argv) || (--argc && *++argv))

struct comp_softc {

char *output;
char msgbuf[1130];

int n_bits;				/* number of bits/code */
int maxbits;				/* user settable max # bits/code */
code_int maxcode;			/* maximum code, given n_bits */
code_int maxmaxcode;			/* should NEVER generate this code */
# define MAXCODE(n_bits)	((1 << (n_bits)) - 1)

/*
 * One code could conceivably represent (1<<BITS) characters, but
 * to get a code of length N requires an input string of at least
 * N*(N-1)/2 characters.  With 5000 chars in the stack, an input
 * file would have to contain a 25Mb string of a single character.
 * This seems unlikely.
 */
# define MAXSTACK    8000		/* size of output stack */
char stack[MAXSTACK];

unsigned short codetab [HSIZE];
code_int hsize;				/* for dynamic table sizing */
count_int fsize;

#define tab_prefix	s->codetab	/* prefix code for this entry */
char_type  	tab_suffix[1<<BITS];	/* last char in this entry */

#ifdef USERMEM
short ftable [(1 << FBITS) * 256];
count_int fcodemem [1 << FBITS];
#endif USERMEM

code_int free_ent;			/* first unused entry */


/*
 * block compression parameters -- after all codes are used up,
 * and compression rate changes, start over.
 */
int block_compress;
int clear_flg;
/*
 * the next two codes should not be changed lightly, as they must not
 * lie within the contiguous general code space.
 */ 
#define FIRST	257	/* first free entry */
#define	CLEAR	256	/* table clear output code */

int ateof;

int gc_offset, gc_size;
int gc_pad[10];
char_type gc_buf[BITS+100];

unsigned char cbuf[2000];	/* was 1124 */
unsigned char *cbufptr;
int cbufsize;

char *loadpt;

int sadecomp;
};	/* end of comp_softc */

code_int getcode();

comp_open (mg, si)
	register struct mon_global *mg;
	register struct sio *si;
{
	register struct comp_softc *s;

	si->si_sadmem = mon_alloc (sizeof (struct comp_softc));
	s = (struct comp_softc*) si->si_sadmem;
	s->maxbits = BITS;
	s->maxmaxcode = 1 << BITS;
	s->hsize = HSIZE;
	s->free_ent = 0;
	s->block_compress = BLOCK_MASK;
	s->clear_flg = 0;
	s->ateof = 0;
if (si->si_dev->d_io_type == D_IOT_PACKET) {
	s->sadecomp = 1;
	tftp_open (mg, si);
	return (0);
}
	return (si->si_dev->d_open(si));
}

comp_close (mg, si)
	register struct mon_global *mg;
	register struct sio *si;
{
	return (si->si_dev->d_close (si));
}

comp_boot (mg, si)
	struct mon_global *mg;
	struct sio *si;
{
    register struct comp_softc *s = (struct comp_softc*) si->si_sadmem;
    char *p;
    int c1,c2;
    extern char edata, end;

    initoutfile(si);
    if(s->maxbits < INIT_BITS) s->maxbits = INIT_BITS;
    if (s->maxbits > BITS) s->maxbits = BITS;
    s->maxmaxcode = 1 << s->maxbits;

    /* Check the magic number */
    c1 = getc8(si);
    c2 = getc8(si);
    if ((c1!=(magic_header[0] & 0xFF))
     || (c2!=(magic_header[1] & 0xFF))) {
	printf("not in compressed format\n");
	return (0);
    }
    s->maxbits = getc8(si);	/* set -b from file */
    s->block_compress = s->maxbits & BLOCK_MASK;
    s->maxbits &= BIT_MASK;
    s->maxmaxcode = 1 << s->maxbits;
    s->fsize = 100000;		/* assume stdin large for USERMEM */
    if(s->maxbits > BITS) {
	printf("compressed with %d bits, can only handle %d bits\n", s->maxbits, 
	    BITS);
    } else
        decompress(si);
    closeoutfile(si);
    if ((int) s->loadpt == 0x04000000)	/* boot it */
	return ((int) s->loadpt);
    printf("end of file\n");
    return (0);
}

char_type lmask[9] = {0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80, 0x00};
char_type rmask[9] = {0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff};

decompress(si)
	struct sio *si;
{
    register struct comp_softc *s = (struct comp_softc*) si->si_sadmem;
    register int stack_top = MAXSTACK;
    register code_int code, oldcode, incode;
    register int finchar;

    /*
     * As above, initialize the first 256 entries in the table.
     */
    s->maxcode = MAXCODE(s->n_bits = INIT_BITS);
    for ( code = 255; code >= 0; code-- ) {
	tab_prefix[code] = 0;
	s->tab_suffix[code] = (char_type)code;
    }
    s->free_ent = ((s->block_compress) ? FIRST : 256 );

    finchar = oldcode = getcode(si);
    putc8 (si,  (char)finchar );	/* first code must be 8 bits = char */

    while ( (code = getcode(si)) != -1 ) {
	if ( (code == CLEAR) && s->block_compress ) {
	    for ( code = 255; code > 0; code -= 4 ) {
		tab_prefix [code-3] = 0;
		tab_prefix [code-2] = 0;
		tab_prefix [code-1] = 0;
		tab_prefix [code] = 0;
	    }
	    s->clear_flg = 1;
	    s->free_ent = FIRST - 1;
	    if ( (code = getcode (si)) == -1 )	/* O, untimely death! */
		break;
	}
	incode = code;
	/*
	 * Special case for KwKwK string.
	 */
	if ( code >= s->free_ent ) {
	    s->stack[--stack_top] = finchar;
	    code = oldcode;
	}

	/*
	 * Generate output characters in reverse order
	 */
	while ( code >= 256 ) {
	    s->stack[--stack_top] = s->tab_suffix[code];
	    code = tab_prefix[code];
	}
	s->stack[--stack_top] = finchar = s->tab_suffix[code];

	/*
	 * And put them out in forward order
	 */
	for ( ; stack_top < MAXSTACK; stack_top++ ) {
		/* INLINE expansion of: putc8(si, s->stack[stack_top]); */
		*s->output = s->stack[stack_top];
		s->output++;
	}
	stack_top = MAXSTACK;

	/*
	 * Generate the new entry.
	 */
	if ( (code=s->free_ent) < s->maxmaxcode ) {
	    tab_prefix[code] = (unsigned short)oldcode;
	    s->tab_suffix[code] = finchar;
	    s->free_ent = code+1;
	} 
	/*
	 * Remember previous code.
	 */
	oldcode = incode;
    }
}

code_int
getcode(si)
	struct sio *si;
{
    register struct comp_softc *s = (struct comp_softc*) si->si_sadmem;
    register code_int code;
    register int r_off, bits;
    register char_type *bp = s->gc_buf;

    if ( s->clear_flg > 0 || s->gc_offset >= s->gc_size || s->free_ent > s->maxcode ) {
	/*
	 * If the next entry will be too big for the current code
	 * size, then we must increase the size.  This implies reading
	 * a new buffer full, too.
	 */
	if ( s->free_ent > s->maxcode ) {
	    s->n_bits++;
	    if ( s->n_bits == s->maxbits )
		s->maxcode = s->maxmaxcode;	/* won't get any bigger now */
	    else
		s->maxcode = MAXCODE(s->n_bits);
	}
	if ( s->clear_flg > 0) {
    	    s->maxcode = MAXCODE (s->n_bits = INIT_BITS);
	    s->clear_flg = 0;
	}
	/* INLINE of :s->gc_size = read8( s->gc_buf, s->n_bits, si ); */
{
	char *bp = (char*) s->gc_buf;
	int i = s->n_bits;
	int c;

	if (s->ateof) {
		s->gc_size = -1;
		goto rtn2;
	}
	while(i--) {
		/* INLINE of: c = getc8(si); */

		if (!s->cbufptr || s->cbufptr >= &s->cbuf[s->cbufsize]) {
			if ((s->cbufsize = getmsg(si)) == 0) {
				c = -1;
				goto rtn;
			}
			bcopy(&s->msgbuf[2], s->cbuf, s->cbufsize);
			s->cbufptr = s->cbuf;
		}
		if ((si->si_dev->d_io_type == D_IOT_CHAR) &&
		    (s->cbufsize < 1024) &&
		    (s->cbufptr - s->cbuf) >= s->cbufsize) {
			c = -1;
		} else
			c = (*s->cbufptr++)&0xff;
rtn:
		if (c == -1) {
			s->ateof = 1;
			s->gc_size = s->n_bits-i;
			goto rtn2;
		}
		*bp++ = (int)c;
	}
	s->gc_size = s->n_bits;
rtn2:
	;
}

	if ( s->gc_size <= 0 )
	    return -1;			/* end of file */
	s->gc_offset = 0;
	/* Round size down to integral number of codes */
	s->gc_size = (s->gc_size << 3) - (s->n_bits - 1);
    }
    r_off = s->gc_offset;
    bits = s->n_bits;
	/*
	 * Get to the first byte.
	 */
	bp += (r_off >> 3);
	r_off &= 7;
	/* Get first part (low order bits) */
	code = (*bp++ >> r_off);
	bits -= (8 - r_off);
	r_off = 8 - r_off;		/* now, offset into code word */
	/* Get any 8 bit parts in the middle (<=1 for up to 16 bits). */
	if ( bits >= 8 ) {
	    code |= *bp++ << r_off;
	    r_off += 8;
	    bits -= 8;
	}
	/* high order bits. */
	code |= (*bp & rmask[bits]) << r_off;
    s->gc_offset += s->n_bits;

    return code;
}

initoutfile(si)
	struct sio *si;
{
	register struct comp_softc *s = (struct comp_softc*) si->si_sadmem;
	int i, c;

	if (si->si_dev->d_io_type == D_IOT_CHAR) {
		/* Open UART at EXTB */
		while((c = getc(0)) != 2)
			;
		i = getmsg(si);
		s->output = *(char **)&s->msgbuf[2];
	} else {
		s->output = (char*) 0x06000000;
	}
	s->loadpt = s->output;
	printf("Loading file at 0x%x\n", s->output);
}

closeoutfile (si)
	struct sio *si;
{
	if (si->si_dev->d_io_type == D_IOT_CHAR)
		putc(0, 0xaa);
}

getc8 (si)
	struct sio *si;
{
	register struct comp_softc *s = (struct comp_softc*) si->si_sadmem;

	if (!s->cbufptr || s->cbufptr >= &s->cbuf[s->cbufsize]) {
		if ((s->cbufsize = getmsg(si)) == 0) {
			return (-1);
		}
		bcopy(&s->msgbuf[2], s->cbuf, s->cbufsize);
		s->cbufptr = s->cbuf;
	}
	if ((si->si_dev->d_io_type == D_IOT_CHAR) && (s->cbufsize < 1024) &&
	    (s->cbufptr - s->cbuf) >= s->cbufsize)
		return(-1);
	return((*s->cbufptr++)&0xff);
}

bcopy(from, to, count)
char *from, *to;
{
	while(count--)
		*to++ = *from++;
}

read8(buf, nchars, si)
char *buf;
	struct sio *si;
{
	register struct comp_softc *s = (struct comp_softc*) si->si_sadmem;

	int i = nchars;
	int c;

	if (s->ateof) {
		return(-1);
	}
	while(i--) {
		c = getc8(si);
		if (c == -1) {
			s->ateof = 1;
			return(nchars-i);
		}
		*buf++ = c;
	}
	return(nchars);
}

putc8(si, c)
	struct sio *si;
{
	register struct comp_softc *s = (struct comp_softc*) si->si_sadmem;

	*s->output = c;
	s->output++;
}


exit()
{
	for(;;);
}

getmsg (si)
	struct sio *si;
{
	register struct comp_softc *s = (struct comp_softc*) si->si_sadmem;
	short size,i;
	register char *p;
	register short csum;
	char *linep = "root";
	int oack;
	register struct mon_global *mg = restore_mg();

	if (si->si_dev->d_io_type == D_IOT_PACKET) {
		size = tftp_boot (mg, si, &linep, s->sadecomp, &s->msgbuf[2]);
		s->sadecomp = 2;
	} else {
		putc(0, 0xaa);	/* send an ack */
		oack = 0;
		/* Get the message size */
		while (!oack) {
			s->msgbuf[0] = getc(0);
			s->msgbuf[1] = getc(0);
			size = *(short *)s->msgbuf;
			p = &s->msgbuf[2];
			for(i=0;i<size+2;i++) {
				*p++ = getc(0);
			}
			csum = 0;
			for(i=0;i<size+2;i++)
				csum += s->msgbuf[i];
			if (csum != *(short *)&s->msgbuf[size+2]) {
				printf("getmsg: bad checksum\n");
				putc(0, 0xff);	/* nack */
			} else
				oack = 1;	/* Ok, wait to send ack */
		}
	}
	return(size);
}
