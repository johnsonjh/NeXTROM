/*	loadrom.c - Edit 1.3			*/

/*	LoadROM Version 2.0 - March 1988	*/
/*	Copyright (C) 1988 Grammar Engine, Inc.	*/

/* LoadROM shell - 				*/
/*	Parase command line.			*/
/*	Build parameterblock `paramb`		*/
/*	Call image builder `buildimg()`		*/
/*	Check return code, process errors	*/
/*	Allow interactive editing and loading	*/

#include <stdio.h>

#ifdef	MSDOS
#include <conio.h>
#endif

#ifdef	VMS
#include <processes.h>
#endif

#include "loadrom.h"

ENTRY	main(argc,argv)
	int argc;
	char **argv;
	{
	extern long buildimg();
	extern void error();
	extern void image_delete();
	extern int errno;
	long parsec();
	void command();
	long initpb();
	long loadimg();
	short retry;

	printf("LoadROM V 2.1\nCopyright (C) 1988 Grammar Engine, Inc.\n");
	errno = 0;
	if (result = initpb())
		{
		error();
		exit(result);
		}
	if (parsec(argc,argv))
		{
		error();
		exit(result);
		}
	printf("Model %d - ",pblock.model);
	printf((pblock.roms == 1) ? "%d Module emulating %ld bytes\n" :
		"%d Modules emulating %ld bytes each\n",
				pblock.roms,pblock.rom);
	printf("Operating Mode %d bits\n", (short)pblock.mode);
	if (filex)
		{
		printf("Building ROM image\n");
		if (buildimg())
			{
			error();
			exit(result);
			}
		if (pblock.flags & LOAD)
			{
			for (retry=0; retry<nvtry; retry++)
				{
				if (loadimg())
					{
					error();
					printf("# of retries = %d\n",retry+1);
					}
				else
					break;
				}
			}
		else
			command();
		}
	else
		{
		command();
		}
	if (pblock.imagename != NULL)
		image_delete();
	exit(OKAY);
	}

/* `parsec` - parse command line */

static	long parsec(argc, argv)
	int argc;
	char **argv;
	{
	long parsesw();	
	short ai;

	for (ai=1; ai<argc; ai++)
		{
		if (argv[ai][0] == '-')
			{
			if ((result = parsesw(argv,&ai)) < 0)
				break;
			else
				continue;
			}
		if (pblock.flags & IMAG)
			{
			pblock.bad = argv[ai];
			return(result = BARG);
			}
		pblock.file[filex++].name = argv[ai];
		pblock.file[filex].name = (char *)0;
		}
	if (!(pblock.flags & IMAG))
		{
		romoff = pblock.file[0].offset;
		pblock.file[0].offset = 0;
		}
	return(result);
	}

/* `parsesw` - parse command line switch specifications */

static	long parsesw(argv, ndxh)
	char **argv;
	short *ndxh;
	{
	long value;
	long getns();
	long getnum();

	switch(argv[*ndxh][1])
		{
		case 'b':
		case 'B':
			pblock.baud = value = getns(argv[++*ndxh]);
			if (value != 300 && value != 1200 && value != 2400
			 && value != 4800 && value != 9600 && value != 19200)
				{
				pblock.bad = argv[*ndxh];
				return(BARG);
				}
			break;
		case 'd':
		case 'D':
			pblock.flags &= ~LOAD;
			break;
		case 'f':
		case 'F':
			if (argv[*ndxh][2] == 'f' || argv[*ndxh][2] == 'F')
				pblock.flags |= FFIL;
			pblock.fill = (char)getnum(argv[++*ndxh]);
    			pblock.flags |= FILC;
			break;
		case 'i':
		case 'I':
			if (filex)
				{
				pblock.bad = argv[*ndxh];
				return(BARG);
				}
			pblock.file[filex].offset = getnum(argv[++*ndxh]);
			pblock.file[filex++].name = argv[++*ndxh];
			pblock.file[filex].name = (char *)0;
			pblock.flags |= IMAG;
			break;
		case 'k':
		case 'K':
			pblock.cka1 = getnum(argv[++*ndxh]);
			pblock.cka2 = getnum(argv[++*ndxh]);
			pblock.cka3 = getnum(argv[++*ndxh]);
			pblock.flags |= CHEK;
			break;
		case 'l':
		case 'L':
			pblock.flags |= LOAD;
			break;
		case 'm':
		case 'M':
			pblock.model = (short)(value = getns(argv[++*ndxh]));
			if (value != 256 && value != 512 && value != 10)
				{
				pblock.bad = argv[*ndxh];
				return(BARG);
				}
			break;
		case 'n':
		case 'N':
			pblock.roms = (char)( value = getns(argv[++*ndxh]));
			if (value < 1 || value > 8)
				{
				pblock.bad = argv[*ndxh];
				return(BARG);
				}
			break;
		case 'o':
		case 'O':
			pblock.portname = argv[++*ndxh];
			break;
		case 'r':
		case 'R':
			pblock.rom = value = getns(argv[++*ndxh]);
			if (value == 2716 || value == 2 || value == 2048)
				{
				pblock.rom = 2048;
				break;
				}
			if (value == 2732 || value == 4 || value == 4096)
				{
				pblock.rom = 4096;
				break;
				}
			if (value == 2764 || value == 8 || value == 8192)
				{
				pblock.rom = 8192;
				break;
				}
			if (value == 27128 || value == 16 || value == 16384)
				{
				pblock.rom = 16384;
				break;
				}
			if (value == 27256 || value == 32 || value == 32768)
				{
				pblock.rom = 32768;
				break;
				}
			if (value == 27512 || value == 64 || value == 65536)
				{
				pblock.rom = 65536;
				break;
				}
			if (value == 27010 || value == 128 || value == 131072)
				{
				pblock.rom = 131072;
				break;
				}
			pblock.bad = argv[*ndxh];
			return(BARG);
			break;
		case 's':
		case 'S':
			value = getnum(argv[++*ndxh]);
			pblock.file[filex].offset = value;
			break;
		case 'v':
		case 'V':
			if (nvtry = getnum(argv[++*ndxh]))
				pblock.flags |= VRFY;
			else
				{
				pblock.flags &= ~VRFY;
				nvtry = 1;
				}
			break;
		case 'w':
		case 'W':
			pblock.mode = (char)(value = getns(argv[++*ndxh]));
			if (value != 8 && value != 16 && value != 32)
				{
				pblock.bad = argv[*ndxh];
				return(BARG);
				}
			break;
		case 'x':
		case 'X':
			pblock.flags |= XFLAG;
			break;
		default:
			pblock.bad = argv[*ndxh];
			return(BARG);
			break;
		}
	return(OKAY);
	}

/* `loadimg` - load the ROMimage to the romulator */

static	long loadimg()
	{
	extern long rawmode();
	extern void rawoff();
	extern void dtron();
	extern void dtroff();
	extern void rtson();
	extern void rtsoff();
	extern void sleep();
	extern long image_loc();
	extern long image_read();
	void checksum();
	long verify();
	long veriend();

#ifdef	MSDOS
	char huge *hex;
#else
	char *hex;
#endif
	long size, tcount, tmpcnt, di, fi;
	short ri;
	char c;
	int rc;
	long banksize, loadsize, fillsize;

	if (pblock.flags & CHEK)
		checksum();
	printf("Loading @ baudrate = %ld\n",pblock.baud);

	if (result = rawmode())
		{
		rawoff();
		return(result);
		}
#ifndef	UNIX			/* AT&T UNIX dtr toggle is in rawmode */
	dtron();
	rtson();
	sleep(1);
	dtroff();
	sleep(1);
	dtron();
#endif
	if (pblock.model == 512)
		rtswitch = 65536 * 4;
	else
		rtswitch = -1;
	banksize = pblock.model / 8;
	if (banksize < 2)
		banksize = 128;
	banksize *= 1024 * pblock.mode / 8;
	loadsize = pblock.rom * pblock.mode / 8;
	fillsize = banksize - loadsize;
	vin = vout = tcount = tmpcnt = 0;
	for (ri=0; ri < NMAP; ri++)
		{
		if (hex = rommap[ri].madr)
			{
			size = rommap[ri].marker + 1;
			if (rommap[ri].flag)
				(void)image_loc(0L,0);

			for (di=0; di < size; di++)
				{
				if (rommap[ri].flag)
					{
					rc = image_read(&c);
					if (rc != 1)
						{
						rawoff();
						return(result = ENDF);
						}
					}
				else
					{
					c = *hex++;
					}
				if (verify(c))
					{
					printf("chars sent = %ld\n",tcount);
					rawoff();
					return(result);
					}
				tcount++;
				if (++tmpcnt == loadsize)
					{
					tmpcnt = 0;
					for (fi=0; fi<fillsize; fi++)
						{
						if (verify(pblock.fill))
							{
							rawoff();
							return(result);
							}
						tcount++;
						}
					}
				if (tcount == rtswitch)
					{
					sleep(1);
					rtsoff();
					}
				}
			if (pblock.flags & FFIL)
				{
				for (fi=tmpcnt; fi<loadsize; fi++,tcount++)
					{
					if (verify(pblock.fill))
						{
						rawoff();
						return(result);
						}
					}
				}

			}
		else
			break;
		}
	if (veriend())
		{
		rawoff();
		return(BADL);
		}
	printf("\07Loading complete - %ld bytes transfered\n", tcount);
	sleep(1);
	dtroff();
	rtsoff();
	rawoff();
	return(result);
	}

/* `verify` - load data and verify loaded data via loop back */

static	long verify(c)
	char c;
	{
	extern void putcom();
	extern long getcom();
#ifdef	MSDOS
	extern int kbhit();
	extern int getch();
#endif

	putcom(c);
#ifdef	MSDOS
	if (kbhit())
		getch();
#endif
	if (!(pblock.flags & VRFY))
			return(OKAY);
	vbuf[vout++] = c;
	if (vout >= VBSIZ)
		vout = 0;

	if (vin == vout)
		return(result = BADL);

	if (!getcom(&c))
		{
		if (c != vbuf[vin++])
			return(result = BADL);
		if (vin >= VBSIZ)
			vin = 0;
		}
	return(OKAY);
	}

/* `veriend` - finish trailing end of verify data */

static	long veriend()
	{
	extern long getcom();
	short ci;
	char c;

	while (vin != vout)
		{
		for (ci=0; ci<10000; ci++)
			if (!getcom(&c))
				break;
		if (vbuf[vin++] != c)
			return(result = BADL);
		if (vin >= VBSIZ)
			vin = 0;
		}
	return(OKAY);
	}

/* `checksum` - compute and store checksum */

static	void checksum()
	{
	char ck;
	long i;
	char getbin();
	extern long putbin();

	for (ck=0,i=pblock.cka1; i<=pblock.cka2; i++)
		ck += getbin(i);

	(void)putbin(pblock.cka3,-ck);
	}

/* `command` - operate command level interactive */

static	void command()
	{
	extern int system();
	void edit();
	void save();
	void load();
	void help();

	for (;;)
		{    
		printf("LoadROM: ");
		while (gets(line) == NULL)
			;
		switch (line[0])
			{
			case 'e':
			case 'E':
				switch (line[1])
					{
					case 'x':
					case 'X':
						return;
						break;
					default:
						edit();
						break;
					}
				break;
			case 's':
			case 'S':
				save();
				break;
			case 'l':
			case 'L':
				load(1);
				break;
			case 'g':
			case 'G':
				load(0);
				break;
			case '!':
#ifdef	VMS
				printf("System function not enabled\n");
#else
				(void)system(&line[1]);
#endif
				break;
			case '?':
			case 'h':
			case 'H':
				help();
				break;
			case 'x':
			case 'X':
				return;
				break;
			default:
				printf("Unknown command\n");
				break;
			}
		}
	}

/* `help` - help the user */

static	void help()
	{
	extern FILE *fopen();
	extern int fclose();
	FILE *hf;
	char hstr[100];
#ifdef	MSDOS
	extern char *getenv();
	extern char *strchr();
	extern char *strcat();
	char pn[40];
	char *path, *str1, *str2;
#endif

	if ((hf = fopen("loadrom.hlp", "r")) == NULL)
		{
#ifdef	MSDOS
		if ((path = getenv("PATH")) == NULL)
			{
			perror("Help file");
			return;
			}
		else
			{
			str1 = path;
			do
				{
				pn[0] = '\0';
				str2 = strchr(str1,';');
				if (str2 != NULL)
					*str2 = '\0';
				(void)strcat(pn,str1);
				(void)strcat(pn,"\\loadrom.hlp");
				str1 = str2;
				if ((hf = fopen(pn,"r")) == NULL)
					continue;
				else
					break;
				} while (str2 != NULL);
			}
		if (hf == NULL)
			{
			perror("Help file");
			return;
			}
		}
#else
		perror("Help file");
		return;
		}
#endif
	while (fgets(hstr,100,hf) != NULL)
		printf("%s",hstr);
	fclose(hf);
	}

/* `edit` - edit the image file */

static	void edit()
	{
	extern long putbin();
	long gethex();
	char getbin();
	long loc;
	unsigned char c;

	if (line[1] == '\000')
		loc = 0;
	else
		loc = gethex(&line[2]);
	for (;;)
		{
		c = (char)getbin(loc);
		printf("%08lx %02x ",loc,c);
		if (gets(line) == NULL)
			return;
		if (line[0] == 'x' | line[0] == 'X')
			return;
		if (line[0] == '\000')
			{
			loc++;
			continue;
			}
		c = (char)gethex(line);
		putbin(loc++,c);
		}
	}

/* `save` - save the ROM image file */

static	void save()
	{
	extern long image_loc();
	extern long image_read();
	extern long filelength();
	char *fname, *getname();
	FILE *sf, *fopen();
#ifdef	MSDOS
	char huge *hex;
#else
	char *hex;
#endif
	long size, di;
	short ri;
	char c;
	int rc;

	if ((fname = getname()) == NULL)
		{
		printf("No filename specified\n");
		return;
		}
	if ((sf = fopen(fname, "wb")) == NULL)
		{
		perror("Save file");
		return;
		}
	printf("Saving ROM image to file\n");
	for (ri=0; ri < NMAP; ri++)
		{
		if (hex = rommap[ri].madr)
			{
			size = rommap[ri].marker + 1;
			if (rommap[ri].flag)
				image_loc(0L,0);

			for (di=0; di < size; di++)
				{
				if (rommap[ri].flag)
					{
					rc = image_read(&c);
					if (rc != 1)
						return;
					}
				else
					{
					c = *hex++;
					}
				fputc(c, sf);
				}
			}
		else
			break;
		}
	fflush(sf);
	size = filelength(fileno(sf));
	printf("File size = %ld (0x%lx)\n",size, size);
	fclose(sf);
	}

/* `load` - load a given file in to the ROMulator */

static	void load(n)
	int n;
	{
	extern long filelength();
	extern void error();
	void image_clear();
	char *fname, *getname();
	short retry;

	if ((fname = getname()) != NULL)
		{
		image_clear();
		pblock.file[0].name = fname;
		pblock.flags |= IMAG;
		if (buildimg())
			{
			error();
			return;
			}
		}
	if (n)
		{
		for (retry=0; retry<nvtry; retry++)
			{
			if (loadimg())
				{
				error();
				printf("# of retries = %d\n",retry+1);
				}
			else
				break;
			}
		}
	}

/* `image_clear` - clear any existing image around */

static	void image_clear()
	{
	extern void image_delete();
	extern void lrclose();
	short ri;

	for (ri=0; ri<NMAP; ri++)
		{
		if (rommap[ri].flag)
			{
			image_delete();
			break;
			}
		else
			{
			if (rommap[ri].madr == NULL)
				break;
#ifdef	MSDOS
			hfree(rommap[ri].madr);
#else
			free(rommap[ri].madr);
#endif
			}
		}
	ri = filex;
	for (filex=0; filex<ri; filex++)
		lrclose();
	filex = 0;
	rommap[0].madr = NULL;
	pblock.file[0].name = NULL;
	pblock.file[0].offset = 0;
	}

/* `getname` - get a token from a string */

static	char *getname()
	{
	short si;

	for (si=1; si<LINSIZ; si++)
		{
		if (line[si] == ' ')
			{
			while (line[si++] == ' ');
			return(&line[--si]);
			}
		else
			if (line[si] == '\0')
				return(NULL);
		}
	return(NULL);
	}

/* `getbin` - get a byte from ROM image */

static	char getbin(loc)
	long loc;
	{
	extern long image_loc();
	extern long image_read();
#ifdef	MSDOS
	char huge *hex;
#else
	char *hex;
#endif
	short ri;
	char c;

	for (ri=0; ri<NMAP; ri++)
		{
		if (loc > rommap[ri].end)
			continue;
		loc -= rommap[ri].start;
		if (rommap[ri].flag)
			{
			if (image_loc(loc,0) < 0)
				return(0);
			if (image_read(&c) <= 0)
				return(0);
			return(c);
			}
		else
			return(*(rommap[ri].madr + loc));
		}
	printf("Address out of range\n");
	return(0);
	}

/* `getns` - read a number from a string */

static	long getns(str)
	char *str;
	{
	short si;
	long value;
	for (si=0, value=0; str[si]; si++)
		{
		if (str[si] < '0' || str[si] > '9')
			continue;
		value = value * 10 + (long)(str[si] - '0');
		}
	return(value);
	}

/* `gethex` - read string as hex */

static	long gethex(str)
	char *str;
	{
	short si;
	long value;
	
	for (si=0, value=0; str[si]; si++)
		{
		if (str[si] >= '0' && str[si] <= '9')
			{	
			value = value * 16 + (long)(str[si] - '0');
			continue;
			}
		if (str[si] >= 'A' && str[si] <= 'F')
			{
			value = value * 16 + (long)(str[si] - 'A' + 10);
			continue;
			}
		if (str[si] >= 'a' && str[si] <= 'f')
			{
			value = value * 16 + (long)(str[si] - 'a' + 10);
			continue;
			}
		else
			break;
		}
	return(value);
	}

/* `getnum` - read a string as a number (hex or decimal) */

static	long getnum(str)
	char *str;
	{
	long getns();
	long gethex();
	short si;

	for (si=0; str[si]; si++)
		{
		if (str[si] == '0')
			continue;
		else
			break;
		}
	if (str[si] == '-')
		{
		si++;
		if (str[si] == 'x' || str[si] == 'X')
			return(-gethex(&str[++si]));
		else
			return(-getns(&str[si]));
		}
	else
		{
		if (str[si] == 'x' || str[si] == 'X')
			return(gethex(&str[++si]));
		else
			return(getns(&str[si]));
		}
	}

/* `initpb` - initialize the parameter block from defaults */

static	long initpb()
	{
	extern FILE *fopen();
	extern int fclose();
	FILE *inf;
	long value;
	
	pblock.model = MODEL;
	pblock.roms = ROMS;
	pblock.rom = ROM;
	pblock.mode = MODE;
	pblock.baud = BAUD;
	pblock.portname = PORT;
	pblock.flags = LOAD;
	pblock.imagename = NULL;
	pblock.bad = NULL;
	filex = 0;
#ifdef	VERIFY
	pblock.flags |= VRFY;
#endif
#ifdef	FILL
	pblock.fill = FILL;
	pblock.flags |= FILC;
#endif
	nvtry = NTRY;
	pblock.file[0].name = NULL;
	pblock.file[0].offset = STARTING;
	rommap[0].madr = NULL;

	if ((inf = fopen("loadrom.ini", "r")) == NULL)
		return(OKAY);

	printf("Initializing defaults from file: loadrom.ini\n");
	while (fgets(line,100,inf) != NULL)
		{
		if (!strncmp(line,"MODEL",5) ||
		 !strncmp(line,"Model",5) ||
		 !strncmp(line,"model",5))
			{
			value = getns(&line[6]);
			if (value !=256 && value != 512 && value != 10)
				{
				pblock.bad = line;
				return(BARG);
				}
			else
				{
				pblock.model = (short)value;
				continue;
				}
			}
		if (!strncmp(line,"NUMBER",6) ||
		 !strncmp(line,"Number",6) ||
		 !strncmp(line,"number",6))
			{
			value = getns(&line[7]);
			if (value < 1 || value > 8)
				{
				pblock.bad = line;
				return(BARG);
				}
			else
				{
				pblock.roms = (char)value;
				continue;
				}
			}
		if (!strncmp(line,"ROM",3) ||
		 !strncmp(line,"Rom",3) ||
		 !strncmp(line,"rom",3))
			{
			value = getns(&line[4]);
			if (value == 2716 || value == 2 || value == 2048)
				{
				pblock.rom = 2048;
				continue;
				}
			if (value == 2732 || value == 4 || value == 4096)
				{
				pblock.rom = 4096;
				continue;
				}
			if (value == 2764 || value == 8 || value == 8192)
				{
				pblock.rom = 8192;
				continue;
				}
			if (value == 27128 || value == 16 || value == 16384)
				{
				pblock.rom = 16384;
				continue;
				}
			if (value == 27256 || value == 32 || value == 32768)
				{
				pblock.rom = 32768;
				continue;
				}
			if (value == 27512 || value == 64 || value == 65536)
				{
				pblock.rom = 65536;
				continue;
				}
			if (value == 27010 || value == 128 || value == 131072)
				{
				pblock.rom = 131072;
				continue;
				}
			pblock.bad = line;
			return(BARG);
			}
		if (!strncmp(line,"WORD",4) ||
		 !strncmp(line,"Word",4) ||
		 !strncmp(line,"word",4))
			{
			value = getns(&line[5]);
			if (value != 8 && value != 16 && value != 32)
				{
				pblock.bad = line;
				return(BARG);
				}
			else
				{
				pblock.mode = (char)value;
				continue;
				}
			}
		if (!strncmp(line,"OUTPUT",6) ||
		 !strncmp(line,"Output",6) ||
		 !strncmp(line,"output",6))
			{
			strcpy(oport,&line[7]);
			oport[strlen(oport)-1] = '\0';
			pblock.portname = oport;
			continue;
			}
		if (!strncmp(line,"FILL",4) ||
		 !strncmp(line,"Fill",4) ||
		 !strncmp(line,"fill",4))
			{
			value = getnum(&line[5]);
			pblock.fill = (char)value;
			pblock.flags |= FILC;
			continue;
			}
		if (!strncmp(line,"FFILL",4) ||
		 !strncmp(line,"Ffill",4) ||
		 !strncmp(line,"ffill",4))
			{
			value = getnum(&line[5]);
			pblock.fill = (char)value;
			pblock.flags |= FILC | FFIL;
			continue;
			}
		if (!strncmp(line,"BAUD",4) ||
		 !strncmp(line,"Baud",4) ||
		 !strncmp(line,"baud",4))
			{
			value = getns(&line[5]);
			if (value != 300 && value != 1200 && value != 2400
			 && value != 4800 && value != 9600 && value != 19200)
				{
				pblock.bad = line;
				return(BARG);
				}
			else
				{
				pblock.baud = value;
				continue;
				}
			}
		if (!strncmp(line,"STARTING",8) ||
		 !strncmp(line,"Starting",8) ||
		 !strncmp(line,"starting",8))
			{
			pblock.file[0].offset = getnum(&line[9]);
			continue;
			}
		if (!strncmp(line,"VERIFY",6) ||
		 !strncmp(line,"Verify",6) ||
		 !strncmp(line,"verify",6))
			{
			if (nvtry = getns(&line[7]))
				pblock.flags |= VRFY;
			else
				nvtry = 1;
			continue;
			}

		pblock.bad = line;
		return(BARG);
		}
	(void)fclose(inf);
	return(OKAY);
	}
