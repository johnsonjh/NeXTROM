/*	LoadICE.h - Edit 0.1			*/

/*	LoadICE Version 1.4b -	June 1990		*/
/*	Copyright (C) 1990 Grammar Engine, Inc.	*/

#ifdef	ansi
#define ANSI
#endif

#ifdef claimglobals
#define extern
#endif

#ifdef TURBOC
#define outp OUTPORTB
#define inp INPORTB
#endif

#define	MAXROM		1048576	/* default emulation ROM size */
#define	NUMBER		8		/* default module count */
#define	WORD		16		/* default word size (8, 16, 32) */
#define	STARTING	0x0		/* ROM space start address */
#define	OUTPUT		"com1"		/* stick default portname here */
#define	BAUD		9600		/* default baudrate */

#define	ROMS	NUMBER			/* number of ROMS */
#define	MODE	WORD			/* mode of operation */
#define	PORT	OUTPUT			/* output port */
#define	NFILE	55			/* number of input files + 1 */
#define	NMAP	9			/* number of memory maps */
#define	OPSIZ	44			/* for port name from loadice.ini */
#define	TBSIZ	1024			/* size of text buffer */
#define	LINSIZ	255			/* size of input record */
#define	ENTRY				/* null */

/* Define commands etc for PROMICE */

#define	PID 	0x00			/* establish unit ID */
#define	PIMB	0x07			/* modify a byte */
#define	PIWR	0x01			/* write data to promice */
#define	PIRD	0x02			/* read data from promice */
#define	PIBR	0x03			/* used for establishing baudrate */
#define	PILP	0x00			/* load data pointer */
#define	PIMO	0x04			/* process mode */
#define	PITS	0x05			/* test ram */
#define	PIRT	0x06			/* reset target */
#define	PIVS	0x0F			/* read version */
#define	AIWR	0x0C			/* AI write */
#define	AIRD	0x0D			/* AI read */
#define	AIBC	0x0A			/* AI change bit */
#define NORESP	0x20			/* No response bit for all commands */
#define NOTIME	0x80			/* disable timer for command */
#define	FAST	0x80			/* go fast - set it in mode data */

/* Define exit codes */

#ifndef VMS
#define SUCCESS 0			/* Most people return a zero */
#else
#define SUCCESS 1			/* Except VAX/VMS */
#endif

/* Define codes returned in the long variable `result` */

#define	OKAY	0			/* everything went fine */
#define	ENDF	-1			/* end of file */
#define	OPEN	-2			/* failed to open some file */
#define	DATA	-3			/* bad data in some file */
#define	ADDR	-4			/* bad address in some file */
#define	CKSM	-5			/* bad checksum in some file */
#define	BARG	-6			/* bad command line argument */
#define	IOER	-7			/* I/O error */
#define	MUCH	-8			/* too much data */
#define	MISC	-9			/* miscelleneous command failure */
#define CONN	-10 		/* failed to connect to promice */
#define BADU	-11 		/* bad unit # */
#define COMM	-12 		/* Promice Comm link failure */

#ifdef	MSDOS				/* for MSDOS - IBM/PC systems */
#define PPIN	(0x379) 		/* address of parallel port status in */
#define PPOUT   (0x37A)         /* address of parallel port status out */
#define PPDAT   (0x378)         /* address of parallel port data out */
#define PP2IN	(0x279) 		/* address of parallel port2 status in */
#define PP2OUT	 (0x27A)		 /* address of parallel port2 status out */
#define PP2DAT	 (0x278)		 /* address of parallel port2 data out */
#define MPIN    (0x3BD)         /* address of IBM MD&PA port status in */
#define MPOUT   (0x3BE)         /* address of IBM MD&PA port status out */
#define MPDAT   (0x3BC)         /* address of IBM MD&PA port data out */

#define BUSY    (0x80)          /* busy bit on parallel port */
#define PAPER   (0X20)          /* busy bit on second parallel port */
#define ACK     (0x40)          /* ack bit on parallel port */
#define STRON   (0x0d)          /* strobe bit on for parallel port */
#define STROFF  (0x0c)          /* strobe bit off for parallel port */
#define AUTOON  (0x0e)          /* strobe bit on for second parallel port */
#define AUTOOFF (0x0c)          /* strobe bit off for second parallel port */

#define	COM1	(0x3F8)			/* address of COM port 1 */
#define	COM2	(0x2F8)			/* address of COM port 2 */
#define	COM3	(0x3E8)			/* address of COM port 3 */
#define	COM4	(0x2E8)			/* address of COM port 4 */
extern int mstat;				/* modem status 					*/
extern int lstat;			/* line status						*/
extern int comerror;			/* error number 					*/
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
#define	CDIV	(115200/pblock.baud)
#endif

/* flags used by LoadICE */

#define	LOAD	0x01			/* load image after build */
#define	EDIT	0x02			/* edit image before load */
#define	IMAG	0x04			/* indicates binary file */
#define HEXF	0x00			/* indicates hex file */
#define	FILC	0x08			/* fill character specified */
#define	FFIL	0x10			/* fill whole roms */
#define	CHEK	0x20			/* compute checksum */
#define	VRFY	0x40			/* do loop back verification */
#define	XFLAG	0x80			/* ignore checksum errors */

/* structures for storing user input and image information */

struct	file				/* file specification */
	{
	char	*name;			/* filename */
	char	tid;			/* unit number for file */
	long	offset;			/* file offset in ROM space */
	long	skip;			/* bytes to skip in binary files */
	int		refnum;			/* refnum, file pointer from open */
	char	code;			/* if IMAG then binary */
	};

struct	pblock				/* parameter block */
	{
	long	baud;			/* com port baud rate */
	short	ppin;			/* parallel port input port */
	short	ppout;			/* parallel port output port */
	short	ppdat;			/* parallel port data port */
	char	mode;			/* mode of operation */
	char	roms;			/* number of modules */
	char	fill;			/* fill character */
	char	flags;			/* various flag bits */
	int	port;			/* output port reference num */
	char	*portname;		/* output port name */
	char	*bad;			/* possible return error pointer */
	long	cka1;			/* checksum from here */
	long	cka2;			/*  to here inclusive */
	long	cka3;			/* store result here */
	struct	file file[NFILE];	/* place to store file info */
	char	ltid;
	char	partial;
	char	rtime;			/* reset time in millisec/9 */
	char	bogus;			/* alignment */
	long	lstart;
	long 	lend;
	};

struct	rommap
	{
#ifdef	MSDOS
	char	huge *madr;		/* memory address from `malloc` */
#else
	char	*madr;			/* memory address from `malloc` */
#endif
	long	rom;			/* image and emulation rom size */
	long	start;			/* starting of good data in this map */
	long	marker;			/* last place loaded */
	short	model;			/* model number */
	char	flag;			/* memory or file? */
	char	bogus;			/* alignment */
	char	ibuf[256];		/* temp image buffer */
	short	bi; 			/* buffer index for above */
	long	iloc;			/* location in image */
	int 	image;			/* image file pointer */
	char	*imagename;		/* image file name */
 };

/* GLOBAL VARIABLES */

extern struct	pblock pblock;			/* the parameter block */
extern struct	rommap rommap[NMAP];	/* memory map of ROMimage */
extern char	finame[NFILE][LINSIZ];	/* array of filenames */
extern long	maxloc;				/* max possible value of loc */
extern long	result;				/* error code stored here */
extern long	readc;				/* how much read by last read */
extern short	filex;				/* index into file specs */
extern short	linex;				/* index into line buffer */
extern long	bufx;				/* buffer index */
extern char	oport[OPSIZ];		/* a place for portname */
extern char	buf[TBSIZ];			/* input buffer for read */
extern char	line[LINSIZ+1];		/* input line of hex data */
extern char	rca;				/* RCA record format */
extern char	units;				/* total number of units */
extern char	esize;				/* ROM emulation size */
extern char	verify;				/* verify option is default on */
extern char	fastresp;			/* fast response is default off */
extern char	parallel;			/* parallel port enable flag */
extern char	ignorebadadd;		/* ignore out of range address flag */
extern char	busreqmode;			/* if set use modify byte commands */
extern char	globc;				/* global character holder */
extern long portopen;			/* set if serial port is open */
extern long forced;				/* FORCE image on Disk */
#ifdef	MSDOS
extern short	mctl;				/* modem control value */
#endif

#undef extern
/*	E	n	d	of definitions */
