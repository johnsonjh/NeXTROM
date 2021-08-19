#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/loader.h>
#include "exec.h"

/* #define DEBUG	1 */

char *progname;
char hex[] = "0123456789ABCDEF";
unsigned maxdata = 64/2;

char *atob();
void srec();
void error();
void warning();

main(argc, argv)
int argc;
char **argv;
{
	FILE *obj;
	char buf[1024];
	struct exec *e = (struct exec*) &buf, a;
	struct mach_header *mh = (struct mach_header*) &buf;
	int address_len = 4; /* address length in bytes */
	unsigned text_base, data_base;
	int text_set, data_set, off;

	/*
	 * srec -[2|3|4] -Taddr -Daddr -llen OBJ
	 *
	 *   Converts OBJ to Moto S-record format.
	 *
	 *   -a specifies s-record address length.  Addresses can
	 *   be either 2, 3, or 4 bytes long.  A warning is given
	 *   if OBJ addresses won't fit in the given address size.
	 *
	 *   If -T is specified, srec addresses for text
	 *   start at addr rather than the a.out entry point,
	 *   If addr is not specified, 0 is used.
	 *
	 *   If -D is given, the data is placed at addr rather
	 *   than immediately after the text,  if addr
	 *   is not specified, 0 is used.  A warning is
	 *   printed if text and data overlap.
	 */

	progname = *argv++;
	argc--;

	for (; argc > 1 && **argv == '-'; argv++, argc--) {

		switch ((*argv)[1]) {

		case '2':
			address_len = 2;
			break;

		case '3':
			address_len = 3;
			break;

		case '4':
			address_len = 4;
			break;

		case 'T':
			if (*atob(&(*argv)[2], &text_base, 16))
				error("bad -T spec: %s", *argv);
			text_set++;
			break;

		case 'D':
			if (*atob(&(*argv)[2], &data_base, 16))
				error("bad -D spec: %s", *argv);
			data_set++;
			break;

		case 'l':
			if (*atob(&(*argv)[2], &maxdata, 10) || maxdata <= 0
			    || maxdata > 60)
				error("bad -l spec: %s", *argv);
			break;


		default:
			error("unknown arg: %s", *argv);
		}
	}
	if (argc != 1)
		error("Usage: %s [-(2|3|4)] [-T[addr]] [-D[addr]] OBJ",
		    progname);
	
	if ((obj = fopen(*argv, "r")) == NULL)
		error("Can't open %s", *argv);
	fread(buf, sizeof(buf), 1, obj);
	if (e->a_magic == OMAGIC) {
		bcopy (e, a, sizeof (struct exec));
		off = sizeof (struct exec);
	} else
	if (mh->magic == MH_MAGIC && mh->filetype == MH_PRELOAD) {
		struct segment_command *sc;
		int seg, cmd;
		
		sc = (struct segment_command*)
			(buf + sizeof (struct mach_header));
		seg = 0;
		for (cmd = 0; cmd < mh->ncmds; cmd++) {
			switch (sc->cmd) {
			
			case LC_SEGMENT:
				if (seg == 0) {
					a.a_entry = sc->vmaddr;
					off = sc->fileoff;
					a.a_text = sc->filesize;
					seg = 1;
				} else
				if (seg == 1) {
					a.a_data = sc->filesize;
					seg = 2;
				}
				break;
			}
			sc = (struct segment_command*)
				((int)sc + sc->cmdsize);
		}
	} else
		error("Bad object module: %s", *argv);

	if (! text_set)
		text_base = a.a_entry;
	if (! data_set)
		data_base = a.a_entry + a.a_text;
	if (text_set && ! data_set)
		data_base = text_base + a.a_text;
	if ((text_base + a.a_text > data_base && text_base <= data_base)
	    || (data_base + a.a_data > text_base && data_base <= text_base))
		warning("text and data overlap");

	if (fseek(obj, off, 0) == -1)
		error("Truncated object file: %s", *argv);
	/*
	 * Something to fill the sign-on srec
	 */
	hdrsrec(address_len, 0, "(c) NeXT, Inc., 1987");
	srec(obj, address_len, a.a_text, text_base, *argv);
	srec(obj, address_len, a.a_data, data_base, *argv);
	eofsrec(address_len, a.a_entry);
	exit(0);
}

hdrsrec(address_len, addr_field, str)
unsigned addr_field;
char *str;
{
	unsigned csum;
	int byte;
	char *cp;
#ifdef DEBUG
	int i;
#endif DEBUG
	unsigned hexint();

	csum = 0;

	putchar('S');
	/* record type */
	putchar(hex[0]);
#ifdef DEBUG
	putchar(' ');
#endif DEBUG
	/* record length (address_len + data_len + csum_byte) */
	csum += hexint(address_len + strlen(str) + 1, 1);
#ifdef DEBUG
	putchar(' ');
#endif DEBUG
	/* address */
	csum += hexint(addr_field, address_len);
#ifdef DEBUG
	putchar(' ');
	i = 0;
#endif DEBUG

	for (cp = str; *cp; cp++) {
		csum += hexint(*cp, 1);
#ifdef DEBUG
		if ((++i & 3) == 0)
			putchar(' ');
#endif DEBUG
	}
#ifdef DEBUG
	putchar(' ');
#endif DEBUG
	hexint(~csum, 1);
	putchar('\n');
}

void
srec(obj, address_len, data_len, data_base, filename)
FILE *obj;
int address_len;
unsigned data_len;
unsigned data_base;
char *filename;
{
	unsigned csum;
	unsigned len;
	int byte;
#ifdef DEBUG
	int i;
#endif DEBUG
	unsigned hexint();

	if (data_base + data_len < data_base)
		warning("address range exceeds 32 bits");
	while (data_len) {
		len = data_len > maxdata ? maxdata : data_len;
		data_len -= len;

		csum = 0;

		putchar('S');
		/* record type */
		putchar(hex[address_len-1]);
#ifdef DEBUG
		putchar(' ');
#endif DEBUG
		/* record length (address_len + data_len + csum_byte) */
		csum += hexint(address_len + len + 1, 1);
#ifdef DEBUG
		putchar(' ');
#endif DEBUG
		/* address */
		csum += hexint(data_base, address_len);
		data_base += len;
#ifdef DEBUG
		putchar(' ');
		i = 0;
#endif DEBUG
		while (len--) {
			if ((byte = getc(obj)) == EOF)
				error("Truncated object module: %s",
				    filename);
			csum += hexint(byte, 1);
#ifdef DEBUG
			if ((++i & 3) == 0)
				putchar(' ');
#endif DEBUG
		}
#ifdef DEBUG
		putchar(' ');
#endif DEBUG
		hexint(~csum, 1);
		putchar('\n');
	}
	if (address_len != 4 && data_base > (1 << (address_len * 8)))
		warning("address exceeds range of srec address field");
}

eofsrec(address_len, entrypt)
int address_len;
unsigned entrypt;
{
	unsigned csum;
	unsigned hexint();

	csum = 0;

	putchar('S');
	/* record type */
	putchar(hex[9 - (address_len - 2)]);
#ifdef DEBUG
	putchar(' ');
#endif DEBUG
	/* record length (address_len + csum_byte) */
	csum += hexint(address_len + 1, 1);
#ifdef DEBUG
	putchar(' ');
#endif DEBUG
	/* address */
	csum += hexint(entrypt, address_len);
#ifdef DEBUG
	putchar(' ');
	putchar(' ');
#endif DEBUG
	hexint(~csum, 1);
	putchar('\n');
}

unsigned
hexint(val, nbytes)
unsigned val;
int nbytes;
{
	unsigned csum = 0;
	unsigned byte;
	int i;

	for (i = nbytes - 1; i >= 0; i--) {
		byte = (val >> (i * 8)) & 0xff;
		csum += byte;
		putchar(hex[byte >> 4]);
		putchar(hex[byte & 0xf]);
	}
	return(csum);
}

void
error(msg, arg)
char *msg;
char *arg;
{
	fprintf(stderr, "%s: ERROR: ", progname);
	fprintf(stderr, msg, arg);
	putc('\n', stderr);
	exit(1);
}

void
warning(msg, arg)
char *msg;
char *arg;
{
	fprintf(stderr, "%s: WARNING: ", progname);
	fprintf(stderr, msg, arg);
	putc('\n', stderr);
}

unsigned digit();

/*
 * atob -- convert ascii to binary.  Accepts all C numeric formats.
 */
char *
atob(cp, iptr, base)
char *cp;
int *iptr;
unsigned base;
{
	int minus = 0;
	register int value = 0;
	unsigned d;

	*iptr = 0;
	if (!cp)
		return(0);

	while (isspace(*cp))
		cp++;

#ifdef notdef
	while (*cp == '-') {
		cp++;
		minus = !minus;
	}

	/*
	 * Determine base by looking at first 2 characters
	 */
	if (*cp == '0') {
		switch (*++cp) {
		case 'X':
		case 'x':
			base = 16;
			cp++;
			break;

		case 'B':	/* a frill: allow binary base */
		case 'b':
			base = 2;
			cp++;
			break;
		
		default:
			base = 8;
			break;
		}
	}
#else
	base = 16;
#endif

	while ((d = digit(*cp)) < base) {
		value *= base;
		value += d;
		cp++;
	}

	if (minus)
		value = -value;

	*iptr = value;
	return(cp);
}

/*
 * digit -- convert the ascii representation of a digit to its
 * binary representation
 */
unsigned
digit(c)
char c;
{
	unsigned d;

	if (isdigit(c))
		d = c - '0';
	else if (isalpha(c)) {
		if (isupper(c))
			c = tolower(c);
		d = c - 'a' + 10;
	} else
		d = 999999; /* larger than any base to break callers loop */

	return(d);
}
