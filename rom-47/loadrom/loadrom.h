/******* Customization History

88.12.20 blaw@NeXT (Bassanio Law)
    Port code to NeXT computer from IBM PC diskette
    Change default settings to suit NeXT's 512Kb 8-bit wide EPROM
    .. and use "/dev/ttya" as output port.  Note that these settings
    .. can be overriden at run time via command line flags or the file
    .. "loadrom.ini".

*************************************/

/*	loadrom.h - Edit 1.2			*/

/*	LoadROM Version 2.0 - March 1988	*/
/*	Copyright (C) 1988 Grammar Engine, Inc.	*/

/* These defines may be changed by the user or they may be overridden	*/
/* by the initialization file `loadrom.ini` or the command line input.	*/

#define	ROM		65536		/* default emulation ROM size */
#define	MODEL		512		/* default model number */
#define	NUMBER		1		/* default module count */
#define	WORD		8		/* default word size (8, 16, 32) */
#define	STARTING	0x0		/* ROM space start address */
#define	OUTPUT		"/dev/ttya"	/* stick default portname here */
#define	BAUD		19200		/* default baudrate */

/* The following two defines if present will enforce filling and/or	*/
/* verification of data - they can always be specified in the command	*/
/* line or in the the initialization file `loadrom.ini` for activation.	*/

/*#define	FILL	0xFF		/* fill character */
/*#define	VERIFY	10		/* retry count */

/******* TRY NOT TO CHANGE ANY OF THE STUFF BELOW THIS LINE ************/

#define	ROMS	NUMBER			/* number of ROMS */
#define	MODE	WORD			/* mode of operation */
#define	PORT	OUTPUT			/* putput port */
#define	NFILE	15			/* number of input files */
#define	NMAP	9			/* number of memory maps */
#ifdef	VERIFY
#define	NTRY	VERIFY			/* number of tries to load */
#else
#define	NTRY	1			/* default value */
#endif
#define	OPSIZ	44			/* for port name from loadrom.ini */
#define	VBSIZ	20			/* verify buffer ring size */
#define	TBSIZ	1024			/* size of text buffer */
#define	LINSIZ	255			/* size of input record */
#define	ENTRY				/* null */

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
#define	BADL	-9			/* verify failed */

#ifdef	MSDOS				/* for MSDOS - IBM/PC systems */
#define	COM1	(0x3F8)			/* address of COM port 1 */
#define	COM2	(0x2F8)			/* address of COM port 2 */
#define	COM3	(0x3E8)			/* address of COM port 3 */
#define	COM4	(0x2E8)			/* address of COM port 4 */
#endif

/* flags used by LoadROM */

#define	LOAD	0x01			/* load image after build */
#define	EDIT	0x02			/* edit image before load */
#define	IMAG	0x04			/* first file is image file */
#define	FILC	0x08			/* fill character specified */
#define	FFIL	0x10			/* fill whole roms */
#define	CHEK	0x20			/* compute checksum */
#define	VRFY	0x40			/* do loop back verification */
#define	XFLAG	0x80			/* ignore checksum errors */

/* structures for storing user input and image information */

struct	file				/* file specification */
	{
	char	*name;			/* filename */
	long	offset;			/* file offset in ROMulator space */
	int	refnum;			/* refnum, file pointer from open */
	char	mult;			/* address multiplier (1,2,3,4) */
	char	code;			/* return code from buildimg */
	};

struct	pblock				/* parameter block */
	{
	short	model;			/* model number */
	long	rom;			/* emulation rom size */
	long	baud;			/* com port baud rate */
	char	mode;			/* mode of operation */
	char	roms;			/* number of modules */
	char	fill;			/* fill character */
	char	flags;			/* various flag bits */
	int	image;			/* image file pointer */
	char	*imagename;		/* image file name */
	int	port;			/* output port reference num */
	char	*portname;		/* output port name */
	char	*bad;			/* possible return error pointer */
	long	cka1;			/* checksum from here */
	long	cka2;			/*  to here inclusive */
	long	cka3;			/* store result here */
	struct	file file[NFILE];	/* place to store file info */
	};

struct	rommap
	{
#ifdef	MSDOS
	char	huge *madr;		/* memory address from `malloc` */
#else
	char	*madr;			/* memory address from `malloc` */
#endif
	long	start;			/* ROM space start address */
	long	end;			/* ROM space end address */
	long	marker;			/* last place loaded */
	char	flag;			/* memory or file? */
	};

/* GLOBAL VARIABLES */

struct	pblock pblock;			/* the parameter block */
struct	rommap rommap[NMAP];		/* memory map of ROMimage */
long	romoff;				/* ROM space offset */
long	maxloc;				/* max possible value of loc */
long	result;				/* error code stored here */
long	rtswitch;			/* when to turn RTS off */
short	readc;				/* how much read by last read */
short	filex;				/* index into file specs */
short	nvtry;				/* number of tries on reload */
short	linex;				/* index into line buffer */
short	bufx;				/* buffer index */
short	vin;				/* verify in pointer */
short	vout;				/* verify out pointer */
char	oport[OPSIZ];			/* a place for portname */
char	buf[TBSIZ];			/* input buffer for read */
char	vbuf[VBSIZ];			/* verify buffer */
char	line[LINSIZ+1];			/* input line of hex data */

#ifdef	MSDOS
short	mctl;				/* modem control value */
#endif

/*	E	n	d	of definitions */
