/******* Customization History

88.12.20 blaw@NeXT (Bassanio Law)
    Port code to NeXT computer from IBM PC diskette
    Add baud rate selection support
    Turn off ECHO mode
    
Bug Report:
88.12.20 blaw@NeXT (Bassanio Law)
    An extraneous character is generated at the begining while
    .. the port is flushed.  Turning off echo mode will solve
    .. the problem at least in the second round.

    ioctl (TIOCFLUSH) call returns -1 when called and trigger
    .. an error message.  But everything seems okay.  Will investigate
    .. later.

*************************************/



/*	sysdep.c - Edit 1.3			*/

/*	LoadROM Version 2.0 - March 1988	*/
/*	Copyright (C) 1988 Grammar Engine, Inc.	*/

/* System Dependent Functions -			*/
/*	lropen - open the file			*/
/*	lrread - read from the file		*/
/*	lrwrite - write to the file		*/
/*	lrclose - close the file		*/
/*	lrioctl - perform control functions	*/

#include <stdio.h>

#ifndef	VMS
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#ifdef	MSDOS
#include <string.h>
#include <io.h>
#endif

#ifdef	UNIX			/* AT&T UNIX systems */
#include <sgtty.h>
#endif

#ifdef	BSD			/* BSD derivatives */
#include <sys/ioctl.h>
#endif

#ifdef	VMS
#include	<iodef.h>
#include	<ssdef.h>
#include	<ttdef.h>
#include	<tt2def.h>
#include	<file.h>
#include	<stat.h>
#endif

#include "loadrom.h"

#ifdef	MSDOS
#define	TXBUF	(pblock.port)
#define	RXBUF	(pblock.port)
#define	DLLSB	(pblock.port)
#define	DLMSB	(pblock.port+1)
#define	IEREG	(pblock.port+1)
#define	LCREG	(pblock.port+3)
#define	MCREG	(pblock.port+4)
#define	LSREG	(pblock.port+5)
#define	MCDTR	(0x1)
#define	MCRTS	(0x2)
#define	RDA	1
#define	BUFMT	32
#define	CDIV	((38400/pblock.baud)*3)
#endif

#ifdef	VMS
struct	vax_string
	{
	long	v_string_length;
	char	*v_string_pointer;
	} term_buf;

short	term_chan;
short	iosb[4];
short	iosb2[4];
long	term_sense[3];
long	rom_sense[3];
long	p1_block[2];
long	tmask[2];
#endif

ENTRY	long lropen()
	{
	extern int open();
	int ref;
#ifdef	MSDOS
	if (pblock.flags & IMAG)
		ref = open(pblock.file[filex].name, O_RDONLY|O_BINARY);
	else
#endif
		ref = open(pblock.file[filex].name, O_RDONLY);
	if (ref <  0)
		return(OPEN);
	else
		pblock.file[filex].refnum = ref;
	return(OKAY);
	}

ENTRY	void lrclose()
	{
	extern int close();

	(void)close(pblock.file[filex].refnum);
	}

ENTRY	long lrread()
	{
	extern int read();

	if ((readc = read(pblock.file[filex].refnum, &buf[0], TBSIZ)) < 0)
		return(IOER);
	else
		return(OKAY);
	}

#ifdef	MSDOS
ENTRY	char huge *lrmalloc(size)
	long size;
	{
	static int memcnt = 0;
	extern char huge *halloc();

	return(halloc(size, 1));
	}
#else
ENTRY	char *lrmalloc(size)
	long size;
	{
	static int memcnt = 0;
	extern char *malloc();

	return(malloc(size));
	}
#endif

ENTRY	long image_open()
	{
	extern char *tmpnam();

	if ((pblock.imagename = tmpnam(pblock.imagename)) == (char *)0)
		return(IOER);
	if ((pblock.image =
#ifdef	MSDOS
	 open(pblock.imagename,O_CREAT|O_TRUNC|O_RDWR|O_BINARY,S_IWRITE)) < 0)
#else
	 open(pblock.imagename,O_CREAT|O_TRUNC|O_RDWR,0)) < 0)
#endif
		return(OPEN);
	else
		return(OKAY);
	}

ENTRY	long image_loc(loc,origin)
	long loc;
	int origin;
	{
	if (lseek(pblock.image, loc, origin) < 0)
		return(IOER);
	else
		return(OKAY);
	}

ENTRY	long image_write(c)
	char c;
	{
	if (write(pblock.image, &c, 1) < 0)
		return(IOER);
	else
		return(OKAY);
	}

ENTRY	long image_read(cp)
	char *cp;
	{
	if (read(pblock.image,cp,1) < 0)
		return(IOER);
	else
		return(OKAY);
	}

ENTRY	void image_delete()
	{
#ifdef	MSDOS
	extern int remove();

	remove(pblock.imagename);
#else
#ifdef	VMS
	extern int delete();

	delete(pblock.imagename);
#else
	extern int unlink();

	unlink(pblock.imagename);
#endif
#endif
	pblock.imagename = (char *)0;
	}

#ifdef	MSDOS
ENTRY	void rtson()
	{
	mctl |= MCRTS;
	outp(MCREG, mctl);
	}

ENTRY	void rtsoff()
	{
	mctl &= ~MCRTS;
	outp(MCREG, mctl);
	}

ENTRY	void dtron()
	{
	mctl |= MCDTR;
	outp(MCREG, mctl);
	}

ENTRY	void dtroff()
	{
	mctl &= ~MCDTR;
	outp(MCREG, mctl);
	}

ENTRY	long rawmode()
	{
	switch (pblock.portname[3])
		{
		case '1':
			pblock.port = COM1;
			break;
		case '2':
			pblock.port = COM2;
			break;
		case '3':
			pblock.port = COM3;
			break;
		case '4':
			pblock.port = COM4;
			break;
		default:
			pblock.bad = pblock.portname;
			return(BARG);
		}
	outp(IEREG, 0);
	outp(LCREG, 128);
	outp(DLLSB, 0xFF & CDIV);
	outp(DLMSB, (0xFF00 & CDIV) >> 8);
	outp(LCREG, 3);
	mctl = 8;
	return(OKAY);
	}

ENTRY	void rawoff()
	{
	void dtroff();

	dtroff();
	}

ENTRY	void putcom(byte)
	char byte;
	{
	while (!(inp(LSREG) & BUFMT));
	outp(TXBUF, byte);
	}

ENTRY	long getcom(cp)
	char *cp;
	{
	if (inp(LSREG) & RDA)
		{
		*cp = (char)inp(RXBUF);
		return(OKAY);
		}
	else
		return(ENDF);
	}

ENTRY	sleep(time)
	short time;
	{
	short ci,wi;

	for (ci=time * 3500; ci > 0; ci--)
		wi = rand() * ci;
	}
#endif

#ifdef	VMS
ENTRY	void rtson()
	{
	p1_block[0] = ((long)TT$M_DS_RTS) << 16;
	sys$qiow(0,term_chan,IO$_SETMODE|IO$M_SET_MODEM|IO$M_MAINT,
		iosb,0,0,p1_block,8,0,0,0,0);
	}

ENTRY	void rtsoff()
	{
	p1_block[0] = ((long)TT$M_DS_RTS) << 24;
	sys$qiow(0,term_chan,IO$_SETMODE|IO$M_SET_MODEM|IO$M_MAINT,
		iosb,0,0,p1_block,8,0,0,0,0);
	}

ENTRY	void dtron()
	{
	p1_block[0] = ((long)TT$M_DS_DTR) << 16;
	sys$qiow(0,term_chan,IO$_SETMODE|IO$M_SET_MODEM|IO$M_MAINT,
		iosb,0,0,p1_block,8,0,0,0,0);
	}

ENTRY	void dtroff()
	{
	sys$cancel(term_chan);
	p1_block[0] = ((long)TT$M_DS_DTR) << 24;
	sys$qiow(0,term_chan,IO$_SETMODE|IO$M_SET_MODEM|IO$M_MAINT,
		iosb,0,0,p1_block,8,0,0,0,0);
	}

ENTRY	long rawmode()
	{
	iosb2[1] = -1;
	tmask[0] = tmask[1] = 0;
	term_buf.v_string_pointer = pblock.portname;
	term_buf.v_string_length = (long)strlen(pblock.portname);
	if (!(sys$assign(&term_buf,&term_chan,0,0) & 1))
		{
		pblock.bad = pblock.portname;
		return(OPEN);
		}
	if (!(sys$qiow(0,term_chan,IO$_SENSEMODE,iosb,0,0,term_sense,
		12,0,0,0,0) & 1))
		{
		pblock.bad = pblock.portname;
		return(IOER);
		}
	rom_sense[0] = term_sense[0];
	rom_sense[1] = term_sense[1] | (TT$M_EIGHTBIT | TT$M_MECHFORM |
		TT$M_MECHTAB | TT$M_NOBRDCST | TT$M_NOECHO);
	rom_sense[2] = term_sense[2] | (TT2$M_PASTHRU);
	rom_sense[1] &= ~(TT$M_CRFILL | TT$M_ESCAPE | TT$M_HALFDUP |
		TT$M_HOSTSYNC | TT$M_LFFILL | TT$M_READSYNC | TT$M_TTSYNC |
		TT$M_WRAP);
	rom_sense[2] &= ~(TT2$M_LOCALECHO);
	if (!(sys$qiow(0,term_chan,IO$_SETMODE,iosb,0,0,rom_sense,
		12,0,0,0,0) & 1))
		{
		pblock.bad = pblock.portname;
		return(IOER);
		}
	return(OKAY);
	}

ENTRY	void rawoff()
	{
	sys$cancel(term_chan);
	sys$qiow(0,term_chan,IO$_SETMODE,iosb,0,0,term_sense,12,0,0,0,0);
	sys$dassgn(term_chan);
	}

ENTRY	putcom(byte)
	char byte;
	{
	sys$qiow(0,term_chan,IO$_WRITEVBLK | IO$M_NOFORMAT,
		iosb,0,0,&byte,1,0,0,0,0);
	}

ENTRY	long getcom(cp)
	char *cp;
	{
	static char cc;

	if (iosb2[1] == 1)
		{
		*cp = cc;
		iosb2[1] = 0;
		sys$qio(0,term_chan,IO$_READVBLK | IO$M_NOFILTR | IO$M_NOECHO,
		 iosb2,0,0,&cc,1,0,tmask,0,0);
		return(OKAY);
		}
	if (iosb2[1] == -1)
		{
		iosb2[1] = 0;
		sys$qio(0,term_chan,IO$_READVBLK | IO$M_NOFILTR | IO$M_NOECHO,
		 iosb2,0,0,&cc,1,0,tmask,0,0);
		return(OKAY);
		}
	return(ENDF);
	}

ENTRY	long filelength(fdes)
	int fdes;
	{
	struct stat sbuf;

	fstat(fdes, &sbuf);
	return((long)sbuf.st_size);
	}
#endif

#ifdef	UNIX			/* AT&T UNIX systems */
ENTRY	void rtson()
	{
	}

ENTRY	void rtsoff()
	{
	}

ENTRY	void dtron()
	{
	}

ENTRY	void dtroff()
	{
	}

ENTRY	long rawmode()
	{
	struct sgttyb arg;

	pblock.port = open(pblock.portname,O_RDWR);
	close(pblock.port);
	sleep(1);
	pblock.port = open(pblock.portname,O_RDWR | O_NDELAY);
	if (ioctl(pblock.port, TIOCGETP, &arg) == -1)
		{
		perror("Error getting terminal status");
		return(IOER);
		}
#ifdef	SYSV9600
	/* see tty(4) in UNIX System V manual: */
	arg.sg_ispeed = 015;
	arg.sg_ospeed = 015;
#endif
	arg.sg_flags |= RAW;
	if (ioctl(pblock.port, TIOCSETP, &arg) == -1)
		{
		perror("Error setting terminal status");
		return(IOER);
		}
	return(OKAY);
	}

ENTRY	void rawoff()
	{
	extern int close();

	(void)close(pblock.port);
	}

ENTRY	putcom(byte)
	char byte;
	{
	write(pblock.port,&byte,1);
	}

ENTRY	long getcom(cp)
	char *cp;
	{
	if (read(pblock.port,cp,1) == 1)
		return(OKAY);
	else
		return(ENDF);
	}

ENTRY	long filelength(fdes)
	int fdes;
	{
	struct stat sbuf;

	fstat(fdes, &sbuf);
	return((long)sbuf.st_size);
	}
#endif

#ifdef	BSD			/* BSD 4.? system */
ENTRY	void rtson()
	{
	}

ENTRY	void rtsoff()
	{
	}

ENTRY	void dtron()
	{
	if (ioctl(pblock.port, TIOCSDTR, 0) == -1)
		perror("Error setting DTR");
	}

ENTRY	void dtroff()
	{
	int mode = 0;

	if (ioctl(pblock.port, TIOCFLUSH, &mode) == -1)
		perror("Error flushing via ioctl");
	if (ioctl(pblock.port, TIOCCDTR, 0) == -1)
		perror("Error clearing DTR");
	}

ENTRY	long rawmode()
	{
	struct sgttyb arg;

	pblock.port = open(pblock.portname,O_RDWR | O_NDELAY);
	if (ioctl(pblock.port, TIOCGETP, &arg) == -1)
		{
		perror("Error getting terminal status");
		return(IOER);
		}
	{ /** added on for NeXT by blaw 88.12.20 **/
	    int speedcode;
	    switch (pblock.baud) {
	    case 300: speedcode = B300; break;
	    case 600: speedcode = B600; break;
	    case 1200: speedcode = B1200; break;
	    case 2400: speedcode = B2400; break;
	    case 4800: speedcode = B4800; break;
	    case 9600: speedcode = B9600; break;
	    case 19200: speedcode = EXTA; break;
	    default: 
		fprintf(stderr, "Unsupported baud selection: %d bps\n", pblock.baud); 
		exit(-1);
	    }
	    arg.sg_ispeed = speedcode;
	    arg.sg_ospeed = speedcode;
	}
	arg.sg_flags |= RAW;
	arg.sg_flags &= ~ECHO;
	if (ioctl(pblock.port, TIOCSETP, &arg) == -1)
		{
		perror("Error setting terminal status");
		return(IOER);
		}
	return(OKAY);
	}

ENTRY	void rawoff()
	{
	extern int close();

	(void)close(pblock.port);
	}

ENTRY	putcom(byte)
	char byte;
	{
	write(pblock.port,&byte,1);
	}

ENTRY	long getcom(cp)
	char *cp;
	{
	if (read(pblock.port,cp,1) == 1)
		return(OKAY);
	else
		return(ENDF);
	}

ENTRY	long filelength(fdes)
	int fdes;
	{
	struct stat sbuf;

	fstat(fdes, &sbuf);
	return((long)sbuf.st_size);
	}
#endif

ENTRY	void error()
	{
	extern long result;
	extern int errno;
	char *str;

#ifdef MSDOS
	switch((int)result)
#else
	switch(result)
#endif
		{
		case ENDF:
			str = "End-O-File";
			break;
		case OPEN:
			str = "Open failed";
			break;
		case DATA:
			str = "Bad data";
			break;
		case ADDR:
			str = "Address out of range";
			break;
		case CKSM:
			str = "Checksum error";
			break;
		case BARG:
			str = "Bad argument";
			break;
		case IOER:
			str = "I/O error";
			break;
		case MUCH:
			str = "Too much data";
			break;
		case BADL:
			str = "Failed to verify data";
			break;
		default:
			str = "Unknown error code";
			break;
		}
	if (errno)
		{
		perror("System error message");
		errno = 0;
		}
	printf("ERROR - %s\n", str);
	if (pblock.bad)
		printf(" Location -> %s\n", pblock.bad);
	else
		printf("\n");
	}
