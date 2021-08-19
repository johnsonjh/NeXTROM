/*	sysdep.c - Edit 0.1				*/

/*	LoadICE Version 1.4b	- June 1990			*/
/*	Copyright (C) 1989, 90 Grammar Engine, Inc.	*/

/* System Dependent Functions -				*/
/*	lropen - open the file				*/
/*	lrread - read from the file			*/
/*	lrwrite - write to the file			*/
/*	lrclose - close the file			*/
/*	lrioctl - perform control functions		*/

#include "loadice.h"
#include <stdio.h>

#ifdef	UNIX
/* #include <sys/termio.h> */
#define B19200	EXTA
#define NOTEMPNAME	/* No DOS style tempname() function */
#include <sys/ioctl.h>
#include <fcntl.h>
#endif

#ifdef	MSDOS
#include <malloc.h>
#include <conio.h>
#endif
#ifdef	MAC
#include <CursorCtl.h>
#include <Serial.h>
#define NOTEMPNAME	/* The MAC does not provide a tempname() function */
#define	AinRefNum	-6
#define	AoutRefNum	-7
#define	BinRefNum	-8
#define	BoutRefNum	-9
/* SCC Serial Chip Addresses - from SysEqu.a in MPW Alincludes */
#define SCCRd	0x1D8	 /* SCC base read address [pointer] */
#define	SCCWr	0x1DC	 /* SCC base write address [pointer] */
pascal void Debugger() extern 0xA9FF;
#endif

#ifndef	VMS
#include <fcntl.h>
#ifndef MAC
#include <sys/types.h>
#include <sys/stat.h>
#endif
#endif

#ifdef	MSDOS
#include <string.h>
#include <io.h>
#include <time.h>
#endif

#ifdef	UNIX
#include <sgtty.h>
#endif

#ifdef	VMS
#include	<iodef.h>
#include	<ssdef.h>
#include	<ttdef.h>
#include	<tt2def.h>
#include	<file.h>
#include	<stat.h>
#define NOTEMPNAME	/* The VAXC lib does not provide a tempname() function */
#endif

#include "proto.h"

#ifndef MAC
#ifdef ANSI
void spincw(short *pcount)
#else
void spincw(pcount)
short *pcount;
#endif
	{
	switch ((*pcount)++ % 4)
		{
		case 0:
			printf("|\b");
			break;
		case 1:
			printf("/\b");
			break;
		case 2:
			printf("-\b");
			break;
		case 3:
			printf("\\\b");
			break;
		}
	}
#ifdef ANSI
void spinccw(short *pcount)
#else
void spinccw(pcount)
short *pcount;
#endif
	{
	switch ((*pcount)++ % 4)
		{
		case 0:
			printf("|\b");
			break;
		case 1:
			printf("\\\b");
			break;
		case 2:
			printf("-\b");
			break;
		case 3:
			printf("/\b");
			break;
		}
	}
#else /* IF MAC */
#ifdef ANSI
void spincw(short *pcount)
#else
void spincw(pcount)
short *pcount;
#endif
	{
	Show_Cursor(HIDDEN_CURSOR);
	SpinCursor(-1);
	}
#ifdef ANSI
void spinccw(short *pcount)
#else
void spinccw(pcount)
short *pcount;
#endif
	{
	static short icurse=0;
	
	if (!icurse)
		{
		InitCursorCtl(NULL);
		icurse = 1;
		}
	Show_Cursor(HIDDEN_CURSOR);
	SpinCursor(1);
	}
#endif	/* nMAC */

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

#ifdef MAC
int portRefNum, vrfyRefNum;
#endif

ENTRY	long lropen()
	{
	int ref;
#ifdef	MSDOS
	if (pblock.file[filex].code)
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
	(void)close(pblock.file[filex].refnum);
	}

ENTRY	long lrread()
	{
	if ((readc = read(pblock.file[filex].refnum, &buf[0], TBSIZ)) < 0)
		{
		pblock.bad = pblock.file[filex].name;
		return(result = IOER);
		}
	else
		return(OKAY);
	}

#ifdef	MSDOS
ENTRY	char _huge *lrmalloc(long num)
	{
	if (forced) return(0);
	return(halloc(num, 1));
	}
#else
#ifdef ANSI
ENTRY	char *lrmalloc(long num)
#else
ENTRY	char *lrmalloc(num)
long num;
#endif
	{
	char *malloc();

	return(malloc(num));
	}
#endif
#ifdef ANSI
ENTRY	long image_open(char id)
#else
ENTRY	long image_open(id)
char id;
#endif
	{
#ifdef NOTEMPNAME
	rommap[id].imagename = "loadice.tmp ";
	rommap[id].imagename[11] = id + (char)0x30;
#else
	if ((rommap[id].imagename = tempnam("d:","temp"/*rommap[id].imagename*/)) == (char *)0)
		return(OPEN);
#endif
	printf("Temp file = %s\n",rommap[id].imagename);
	if ((rommap[id].image =
#ifdef	MSDOS
	 open(rommap[id].imagename,O_CREAT|O_TRUNC|O_RDWR|O_BINARY,S_IWRITE)) < 0)
#else
	 open(rommap[id].imagename,O_CREAT|O_TRUNC|O_RDWR,0)) < 0)
#endif
		{
		pblock.bad = rommap[id].imagename;
		return(OPEN);
		}
	else
		return(OKAY);
	}

#ifdef ANSI
ENTRY	long image_loc(char id, long loc,int origin)
#else
ENTRY	long image_loc(id, loc, origin)
char id;
long loc;
int origin;
#endif
	{
	if (lseek(rommap[id].image, loc, origin) < 0)
		return(IOER);
	else
		return(OKAY);
	}

#ifdef ANSI
ENTRY	long image_write(char id, char c)
#else
ENTRY	long image_write(id, c)
char id,c;
#endif
	{
	if (write(rommap[id].image, &c, 1) < 0)
		return(IOER);
	else
		return(OKAY);
	}

#ifdef ANSI
ENTRY	long image_wblock(char id, char *ibuf, short c)
#else
ENTRY	long image_wblock(id, ibuf, c)
char id;
char *ibuf;
short c;
#endif
	{
	long	dsize;

	if ((dsize = write(rommap[id].image, ibuf, c)) < 0)
		return(IOER);
	else
		return(dsize);
	}

#ifdef ANSI
ENTRY	long image_read(char id, char *cp)
#else
ENTRY	long image_read(id, cp)
char id;
char *cp;
#endif
	{
	if (read(rommap[id].image,cp,1) <= 0)
		return(IOER);
	else
		return(OKAY);
	}

#ifdef ANSI
ENTRY	long image_bread(char id, char *cp, int ccnt)
#else
ENTRY	long image_bread(id, cp, ccnt)
char id;
char *cp;
int ccnt;
#endif
	{
	if ((ccnt = read(rommap[id].image,cp,ccnt)) <= 0)
		return(IOER);
	else
		return(ccnt);
	}

#ifdef ANSI
ENTRY	void image_delete(char id)
#else
ENTRY	void image_delete(id)
char id;
#endif
	{
#ifdef	MSDOS
	remove(rommap[id].imagename);
#else
#ifdef	VMS
	delete(rommap[id].imagename);
#else
	unlink(rommap[id].imagename);
#endif
#endif
	rommap[id].imagename = (char *)0;
	}

#ifdef	MSDOS

ENTRY	long rawmode()
	{
	char c, mono;

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
	outp(DLLSB, (int)(0xFF & CDIV));
	outp(DLMSB, (int)((0xFF00 & CDIV) >> 8));
	outp(LCREG, 3);   /* MUST BE TWO STOP BITS FOR DAISY CHAIN USE */
	mctl = 8;
	if (parallel)
		{
		pblock.ppdat = mono = 0;
		outp(MPDAT, 0xAA);
		if((c = (char)inp(MPDAT)) == (char)0xAA)
			{
			outp(MPDAT, 0x55);
			if((c = (char)inp(MPDAT)) == (char)0x55)
				mono = 1;
			}
		if (parallel == 1 && mono)
			{		/* IBM Monochrome Display & Printer Adapter */
			pblock.ppin = MPIN;
			pblock.ppout = MPOUT;
			pblock.ppdat = MPDAT;
			outp(pblock.ppdat, (char)254);
			outp(pblock.ppout, STROFF);
			return(OKAY);
			}
		if ((parallel == 1 && !mono) || (parallel == 2 && mono))
			{
			outp(PPDAT, 0xAA);
			if((c = (char)inp(PPDAT)) != (char)0xAA)
				printf("Parallel Adapter not responding! \n");
			pblock.ppin = PPIN;
			pblock.ppout = PPOUT;
			pblock.ppdat = PPDAT;
			outp(pblock.ppdat, (char)254);
			outp(pblock.ppout, STROFF);
			return(OKAY);
			}
		if ((parallel == 2 && !mono) || (parallel == 3))
			{
			outp(PP2DAT, 0xAA);
			if((c = (char)inp(PP2DAT)) != (char)0xAA)
				printf("Parallel Adapter number 2 not responding! \n");
			pblock.ppin = PP2IN;
			pblock.ppout = PP2OUT;
			pblock.ppdat = PP2DAT;
			outp(pblock.ppdat, (char)254);
			outp(pblock.ppout, STROFF);
			}
		}
	return(OKAY);
	}

ENTRY	void rawoff()
	{
	}

#ifdef ANSI
ENTRY	void putpar(char byte)
#else
ENTRY	void putpar(byte)
char byte;
#endif
	{
	while(!((char)inp(pblock.ppin) & BUSY));
	outp(pblock.ppdat, byte);
	outp(pblock.ppout, STRON);
	outp(pblock.ppout, STROFF);
	}

 /* use Autofeed as strobe and Out of paper as busy */

#ifdef ANSI
ENTRY	void putpar2(char byte)
#else
ENTRY	void putpar2(byte)
char byte;
#endif
	{
	while((char)inp(pblock.ppin) & PAPER);
	outp(pblock.ppdat, byte);
	outp(pblock.ppout, AUTOON);
	outp(pblock.ppout, AUTOOFF);
	}

#ifdef ANSI
ENTRY	void putcom(char byte)
#else
ENTRY	void putcom(byte)
char byte;
#endif
	{
	while (!(inp(LSREG) & BUFMT));
	outp(TXBUF, byte);
	}


#ifdef ANSI
ENTRY	long getcom(char *cp)
#else
ENTRY	long getcom(cp)
char *cp;
#endif
	{
	if (inp(LSREG) & RDA)
		{
		*cp = (char)inp(RXBUF);
		return(OKAY);
		}
/*
	else
		{
		if (kbhit())
			getchar();
*/		return(ENDF);
/*		}
*/	}

#ifdef ANSI
void	sleep(short time)		/* time is in tenths of seconds */
#else
void	sleep(time)				/* time is in tenths of seconds */
short time;
#endif
	{
	extern int errno;
	clock_t ci,wi;
	clock_t clock(void);

	if ((ci =clock()) == (clock_t)-1 )
		{
		printf("Clock not readable");
		for (ci=time * 250; ci > 0; ci--)
			for (wi = 0; wi < 200;wi++);
		}
	else
		{
		wi = time + ci/(CLK_TCK/10);
		while (ci/(CLK_TCK/10) <= wi)
			{
			ci=clock();
			}
		}
	errno = 0;
	}
#endif
#ifndef MAC
void beep()
	{
	printf("\07");
	}
#endif
#ifdef	MAC
void beep()
	{
	}
long rawmode()
	{
	int result,baud;
	unsigned char serial_port;
	char **serial_chip;
	
	if (!strcmp(pblock.portname, "modem"))
		serial_port = sPortA;
	else
		if (!strcmp(pblock.portname, "printer"))
			serial_port = sPortB;
		else
			{
			pblock.bad = pblock.portname;
			return(BARG);
			}

	if (result = RAMSDOpen(serial_port))
		{
		pblock.bad = pblock.portname;
		return(OPEN);
		}
	
	switch (pblock.baud)
		{
		case 1200: 
			baud = baud1200 | stop20 | noParity | data8;
			break;
		case 2400:
			baud = baud2400 | stop20 | noParity | data8;
			break;
		case 4800:
			baud = baud4800 | stop20 | noParity | data8;
			break;
		case 9600:
			baud = baud9600 | stop20 | noParity | data8;
			break;
		case 19200:
			baud = baud19200 | stop20 | noParity | data8;
			break;
		case 57600:
			baud = baud57600 | stop20 | noParity | data8;
			break;
		}

	if (serial_port == sPortA)
		portRefNum = AoutRefNum;
	else
		portRefNum = BoutRefNum;
		
	if (serial_port == sPortA)
		vrfyRefNum = AinRefNum;
	else
		vrfyRefNum = BinRefNum;
		
	if (result = SerReset(portRefNum,baud))
		{
		printf("LoadICE: can't set baud rate\n");
		return(IOER);
		}
	if ((vrfyRefNum) && (result = SerReset(vrfyRefNum,baud)))
		return(IOER);

	SerSetBuf(vrfyRefNum,(char *)0,0); /* dump data in buffer */
	return(OKAY);
	}
	
void rawoff()
	{
	if (portRefNum)
		RAMSDClose(portRefNum);
	if (vrfyRefNum)
		RAMSDClose(vrfyRefNum);
	}

#define MacTicker	(*(long*)0x16A)

#ifdef ANSI
void sleep(int secs)
#else
void sleep(secs)
int secs;
#endif
	{
	register long beginticks,endticks;

	beginticks = MacTicker;
	endticks = beginticks + secs * 10;
	while (MacTicker < endticks)
		;
	}

#ifdef ANSI
void putcom(char data)
#else
void putcom(data)
char data;
#endif
	{
	extern long		result;
	static short	index;
	long			count=1;
	
/*	outbuf[index++] = c;
	if (index == 127)
		{
		PBwrite();
		index = 0;
		}
	}  this is a more efficient method, when it works! */
	
	result = FSWrite(portRefNum,&count,&data);
	}

#ifdef ANSI
long getcom(char *c)
#else
long getcom(c)
char *c;
#endif
	{
	long count = 0;
		
	SerGetBuf(vrfyRefNum,&count);
	if (count)
		{
		count = 1;
		if(FSRead(vrfyRefNum,&count,c))
			return(IOER);
		else
			return(OKAY);
		}
	else
		return(ENDF);
	}
#ifdef ANSI
ENTRY void perror(char *errstr)
#else
ENTRY void perror(errstr)
char *errstr;
#endif
	{
	extern int errno;
	
	printf("%s - Error code (errno) = %d\n",errstr, errno);
	}
#endif

#ifdef	VMS
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

#ifdef ANSI
void putcom(char byte)
#else
void putcom(byte)
char byte;
#endif
	{
	sys$qiow(0,term_chan,IO$_WRITEVBLK | IO$M_NOFORMAT,
		iosb,0,0,&byte,1,0,0,0,0);
	}

#ifdef ANSI
ENTRY	long getcom(char *cp)
#else
ENTRY	long getcom(cp)
char *cp;
#endif
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

#ifdef ANSI
ENTRY	long filelength(int fdes)
#else
ENTRY	long filelength(fdes)
int fdes;
#endif
	{
	struct stat sbuf;

	fstat(fdes, &sbuf);
	return((long)sbuf.st_size);
	}
#endif

#ifdef	UNIX
#ifdef	ATNT
ENTRY	long rawmode()
	{
	struct termio arg;
	char baud;
	
	switch (pblock.baud)
		{
		case 1200: 
			baud = B1200;
			break;
		case 2400:
			baud = B2400;
			break;
		case 4800:
			baud = B4800;
			break;
		case 9600:
			baud = B9600;
			break;
		case 19200:
			baud = B19200;
			break;
		default:
			pblock.baud=B19200;
			baud = B19200;
			break;
		}

	printf("Opening %s\n",pblock.portname);
	if ((pblock.port = open(pblock.portname,O_RDWR | O_NDELAY)) < 0)
		{
		pblock.bad = pblock.portname;
		return(IOER);
		}
	if (ioctl(pblock.port, TCGETA, &arg) < 0)
		{
		perror("Error getting terminal status");
		return(IOER);
		}
	arg.c_iflag = 0;
	arg.c_oflag = 0;
	arg.c_cflag = baud | CSTOPB | CREAD | CS8 | CLOCAL;
	arg.c_lflag = 0;
	arg.c_cc[VMIN]= 1;
	arg.c_cc[VTIME]=1;
	if (ioctl(pblock.port, TCSETA, &arg) < 0)
		{
		perror("Error setting terminal status");
		return(IOER);
		}
	return(OKAY);
	}
#else
ENTRY	long rawmode()
	{
	struct sgttyb arg;
	int flags, baud;

	printf("Opening %s\n",pblock.portname);
	if ((pblock.port = open(pblock.portname,O_RDWR)) < 0)
		{
		pblock.bad = pblock.portname;
		return(IOER);
		}
	if ((flags = fcntl(pblock.port, F_GETFL, 0)) == -1)
		{
		perror("Error getting file (device) mode");
		return(IOER);
		}
	if (fcntl(pblock.port, F_SETFL, flags | O_NDELAY) == -1)
		{
		perror("Error setting no-delay mode");
		return(IOER);
		}
	if (ioctl(pblock.port, TIOCGETP, &arg) == -1)
		{
		perror("Error getting terminal status");
		return(IOER);
		}

	/* set baud rate */
	switch (pblock.baud)
		{
		case 1200: 
			baud = B1200;
			break;
		case 2400:
			baud = B2400;
			break;
		case 4800:
			baud = B4800;
			break;
		case 9600:
			baud = B9600;
			break;
		case 19200:
			baud = B19200;
			break;
		default:
			pblock.baud=B19200;
			baud = B19200;
			break;
		}

	arg.sg_ispeed = baud;
	arg.sg_ospeed = baud;

	arg.sg_flags = RAW;
	if (ioctl(pblock.port, TIOCSETP, &arg) == -1)
		{
		perror("Error setting terminal status");
		return(IOER);
		}
	return(OKAY);
	}
#endif

ENTRY	void rawoff()
	{
	extern int close();

	(void)close(pblock.port);
	}

#ifdef ANSI
ENTRY	void putcom(char byte)
#else
ENTRY	void putcom(byte)
char byte;
#endif
	{
#ifdef ATNT
	write(pblock.port,&byte,1);
#else
	while (write(pblock.port,&byte,1)<0);
#endif
	}

#ifdef ANSI
ENTRY	long getcom(char *cp)
#else
ENTRY	long getcom(cp)
char *cp;
#endif
	{
	if (read(pblock.port,cp,1) == 1)
		{
		return(OKAY);
		}
	else
		{
		return(ENDF);
		}
	}

#ifdef PI_BLOCK
void wrblock(tid, ccnt, ibuf)
char tid;
short ccnt;
char *ibuf;
	{
	static char bufwrt[300];
	int n_write;
	int i;
	int cnt, offset;

	bufwrt[0]=tid;
	bufwrt[1]=PIWR|NORESP;
	bufwrt[2]=(char)ccnt;
	for (i=0; i<ccnt; i++)
		bufwrt[i+3] = *ibuf++;

	cnt = ccnt + 3;
	offset = 0;
	while ((n_write = write(pblock.port, bufwrt + offset, cnt)) != cnt) {
	    if (n_write > 0) {
		offset += n_write;
		cnt -= n_write;
	    }
	}
#ifdef OLD_BAD_CODE
#ifdef ATNT
	if (write(pblock.port,bufwrt,ccnt+3)<0)
		{
		perror("WRBLOCK-writefailed");
		}
#else
	while (write(pblock.port,bufwrt,ccnt+3)<0);
#endif
#endif /* OLD_BAD_CODE */
	}
void rdblock(tid,ccnt,obuf)
char tid;
short ccnt;
char *obuf;
	{
	static char bufred[300];
	int i,j,k;

	bufred[0]=tid;
	bufred[1]=PIRD;
	bufred[2]=1;
	bufred[3]=(char)ccnt;
#ifdef ATNT
	if (write(pblock.port,bufred,4)<0)
		{
		perror("RDBLOCK-writefailed");
		return;
		}
#else
	while (write(pblock.port,bufred,4)<0);
#endif
	i=j=0;
	k=ccnt+3;
	while(k)
		{
		while ((i=read(pblock.port,&bufred[j],k)) < 0);
		j+=i;
		k-=i;
		}
	for (i=0; i<ccnt; i++)
		*obuf++ = bufred[i+3];
	}
#endif /*PI_BLOCK*/

#ifdef ANSI
ENTRY	long filelength(int fdes)
#else
ENTRY	long filelength(fdes)
int fdes;
#endif
	{
	struct stat sbuf;

	fstat(fdes, &sbuf);
	return((long)sbuf.st_size);
	}
#endif /*UNIX*/

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
			str = "Unrecognized Data";
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
		case MISC:
			str = "General command failure";
			break;
		case COMM:
			str = "Promice Communication link failed";
			break;
		case CONN:
			str = "Failed to connect to Promice. Cycle power on Promice";
			break;
		case BADU:
			str = "Check file specs for consistency against word size";
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
		{
		printf(" Location -> %s\n", pblock.bad);
		pblock.bad[0] = NULL;
		}
	else
		printf("\n");
	result = 0;
	}
