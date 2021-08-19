/*	buildimg.c - Edit 0.1				*/

/*	LoadICE Version 1.4b	- June 1990			*/
/*	Copyright (C) 1989, 90 Grammar Engine, Inc.	*/

#include "loadice.h"
#include "proto.h"
#include <string.h>
#include <stdio.h>
static short et_checktable[256];

long	buildimg()
	{
	char tid;

	rca = 0;
	if (files())
		return(result = OPEN);
	ckintek();
	while (!getrec())
		{
		switch (line[0])
			{
			case 's':
			case 'S':
				mhex();
				break;
			case ';':
				mohex();
				break;
			case ':':
				ihex();
				break;
			case '/':
				thex();
				break;
			case '%':
				ethex();
				break;
			case '_':
				dsphex();
				break;
			case '?':
			case '!':
				rca = 1;
				break;
			default:
				if (rca)
					rhex();
				else
					{
					result  = DATA;
					pblock.bad = line;
					}
				break;
			}
		if (result == ADDR && ignorebadadd)
			{
			error();
			printf("Ignoring above error\n");
			result = 0;
			}
		if (result)
			return(result);
		}
	if (pblock.flags & FFIL)
		{
		for (tid = 0; tid < units; tid++)
			{
			if (rommap[tid].marker < rommap[tid].rom - 1)
				{
				for (;++rommap[tid].marker < rommap[tid].rom;)
					putbin(tid,rommap[tid].marker,(char)pblock.fill);
				rommap[tid].marker = rommap[tid].rom - 1;
				}
			}
		}

	return(result);
	}

/* `binary` - process a binary (process) image */

long binary()
	{
	long loc, si;
	char tid, bank;

	bank = 0;
	tid = pblock.file[filex].tid;
	loc = pblock.file[filex].offset;
	rommap[pblock.file[filex].tid].start = loc;
	bufx = pblock.file[filex].skip;
	while(!lrread())
		{
		if (!readc)
			break;
		if (readc <= bufx)
			{
			bufx -= readc;
			continue;
			}
		while (loc >= rommap[tid].rom * pblock.mode/8)
			{
			loc -= rommap[tid].rom * pblock.mode/8;
			tid += (pblock.mode/8);
			if (tid >= units)
				return(result = MUCH);
			}
		if (rommap[tid].flag && pblock.mode == 8)
			{
			if (rommap[tid].marker < (loc - 1))
				{
				if (result = image_loc(tid, 0L,2))	/* goto end of file*/
					return(result);
				for (si=rommap[tid].marker+1; si<loc; si++)
					if (result = image_write(tid, pblock.fill))
						return(result);
				}
			else if (result = image_loc(tid, loc,0))
				return(result);
			if((si = image_wblock(tid, &buf[bufx],(short)(readc - bufx))) < 0)
				{
				return(result = si);
				}
			loc += si;
			if (si != (readc - bufx))
				return(result = IOER);
			if (rommap[tid].marker < (loc - 1))
				rommap[tid].marker = (loc - 1);
			}
		else
			{
			for (; bufx<readc; bufx++)
				{
				while (loc >= rommap[tid].rom * pblock.mode/8)
					{
					loc -= rommap[tid].rom * pblock.mode/8;
					tid += (pblock.mode/8);
					if (tid >= units)
						return(result = MUCH);
					}
				bank = (char)(loc % (pblock.mode/8));
				if (tid + bank >= units)
					{
					printf("Invalid Unit ID - %d\n",tid);
					pblock.bad = NULL;
					return(result = BADU);
					}
				if (putbin((char)(tid + bank),loc++/(pblock.mode/8),buf[bufx]))
					break;
				}
			}
		if (result)
			return(result);
		bufx = 0;
		}
	return(result);
	}

/* `putbin` - store a binary character into the rommap */

#ifdef ANSI
long putbin(char tid,long loc,char c)
#else
long putbin(tid,loc,c)
char tid;
long loc;
char c;
#endif
	{
	long si;
	static long lastloc = -1;

	if (tid >= units)
		{
		printf("Invalid Unit ID - %d\n",tid);
		pblock.bad = NULL;
		return(result = BADU);
		}
	if (loc >= rommap[tid].rom)
		{
		printf("FILE = %s  address = %lx  unit = %x\n",pblock.file[filex].name,loc,tid);
		pblock.bad = line;
		return(result = ADDR);
		}
	if (!rommap[tid].flag)
		{
		*(rommap[tid].madr + loc) = c;
		}
	else if (++lastloc == loc)
		{
		result = image_write(tid, c);
		}
	else
		{
		lastloc = loc;
		if (rommap[tid].marker < (loc - 1))
			{
			if (result = image_loc(tid, 0L,2))
				return(result);
			for (si=rommap[tid].marker+1; si<loc; si++)
				if (result = image_write(tid, pblock.fill))
					return(result);
			result = image_write(tid, c);
			}
		else if (!(result = image_loc(tid,loc,0)))
			result = image_write(tid, c);
		}
	if (rommap[tid].marker < loc)
		rommap[tid].marker = loc;
	return(result);
	}

/* `mhex ` - process Motorola hex record */

void mhex()
	{
	long loc;
	short count;
	char *hex;


	loc = 0;
	count = (short)getnx(&line[2], 2);
	if (checkmi(&line[2], (count+1)*2) != 0xFF)
		{
		if (!(pblock.flags & XFLAG))
			{
			pblock.bad = line;
			result = CKSM;
			return;
			}
		}
	switch(line[1])
		{
		case '0':
			count = 0;
			loc = 0;
			break;
		case '1':
			loc = getnx(&line[4],4);
			hex = &line[8];
			count -= 3;
			break;
		case '2':
			loc = getnx(&line[4],6);
			hex = &line[10];
			count -= 4;
			break;
		case '3':
			loc = getnx(&line[4],8);
			hex = &line[12];
			count -= 5;
			break;
		case '7':
			count = 0;
			loc = 0;
			break;
		case '8':
			count = 0;
			loc = 0;
			break;
		case '9':
			count = 0;
			loc = 0;
			break;
		default:
			count = 0;
			result = DATA;
			pblock.bad = line;
			break;
		}
	puthex(loc, count, hex);
	}

/* `mohex` - process Mostek hex record */

void mohex()
	{
	long loc;
	short count;
	char *hex;

	count = (short)getnx(&line[1], 2);
	if (checkmi(&line[1],(short)((count*2)+6)) != (short)getnx(&line[(count*2)+7],4))
		{
		if (!(pblock.flags & XFLAG))
			{
			pblock.bad = line;
			result = CKSM;
			return;
			}
		}
	loc = getnx(&line[3], 4);
	hex = &line[7];
	puthex(loc, count, hex);
	}

/* `ihex` - process Intel hex record */

void ihex()
	{
	long loc;
	short count;
	char *hex;
	static long ibase = 0;

	loc = 0;
	count = (short)getnx(&line[1], 2);
	if (checkmi(&line[1],count*2+8) != (short)getnx(&line[9+count*2],2))
		{
		if (!(pblock.flags & XFLAG))
			{
			if (getnx(&line[7], 2) != 1)
				{
				pblock.bad = line;
				result = CKSM;
				return;
				}
			}
		}
	switch((short)getnx(&line[7], 2))
		{
		case 0:
			loc = ibase + getnx(&line[3], 4);
			hex = &line[9];
			break;
		case 1:
			count = 0;
			ibase = 0;
			break;
		case 2:
			ibase = getnx(&line[9], 4) << 4;
			count = 0;
			break;
		case 3:
			count = 0;
			break;
		default:
			result = DATA;
			pblock.bad = line;
			count = 0;
			break;
		}
	puthex(loc, count, hex);
	}

/* `rhex` - process RCA cosmac hex record */

void rhex()
	{
	long loc;
	char *hex;
	short count;
	static long rbase = 0;

	if (rca == 1)
		{
		loc = getnx(&line[0],4);
		hex = &line[4];
		}
	else
		{
		loc = rbase;
		hex = &line[0];
		}

	count = (short)strlen(hex);

	if (*hex == ' ')
		{
		hex++;
		count--;
		}

	switch(*(hex+count))
		{
		case ',':
			rca = 2;
			count--;
			break;

		case ';':
			rca = 1;
			count--;
			break;
		}

	rbase = loc+(count/2);
	puthex(loc, count/2, hex);
	}
	
/* `thex` - process Tektronix hex record */

void thex()
	{
	long loc;
	short count;
	char *hex;

	if (line[1] == '/') /* '//' is an abort record */
		return;
	if (!(count = (short)getnx(&line[5], 2)))
		return;
	if ((checkst(&line[1],6) != (short)getnx(&line[7],2)) ||
	    (checkst(&line[9],count*2)!=(short)getnx(&line[(count*2)+9],2)))
		{
		if (!(pblock.flags & XFLAG))
			{
			pblock.bad = line;
			result = CKSM;
			return;
			}
		}
	loc = getnx(&line[1], 4);
	hex = &line[9];
	puthex(loc, count, hex);
	}

/* `ethex` - process Extended Tektronix hex record */

void ethex()
	{
	long loc;
	short tmp, tmp1, si, count;
	char *hex;
	static long tbase = 0;

	tmp1 = 0;
	loc = 0;
	count = (short)getnx(&line[1], 2);
	if (checktek(line, count) != (short)getnx(&line[4], 2))
		{
		if (!(pblock.flags & XFLAG))
			{
			pblock.bad = line;
			result = CKSM;
			return;
			}
		}
	switch(line[3])
		{
		case '6':
			if (!(tmp = (short)getnx(&line[6], 1)))
				tmp = 16;
			loc = tbase + getnx(&line[7], tmp);
			hex = &line[7 + tmp];
			count -= 6 + tmp;
			break;
		case '3':
			if (!(tmp = (short)getnx(&line[6], 1)))
				tmp = 16;
			si = tmp + 7;
			while (si < count)
				{
				tmp = (short)getnx(&line[si++], 1);
				tmp1 = (short)getnx(&line[si++], 1);
				if (!tmp)
					break;
				si += tmp1;
				}
			if (!tmp)
				tbase = getnx(&line[si], tmp1);
			count = 0;
			break;
		case '8':
			count = 0;
			tbase = 0;
			break;
		default:
			result = DATA;
			pblock.bad = line;
			count = 0;
			break;
		}
	puthex(loc, count, hex);
	}

/* `dsphex` - process Motorola DSP56001 hex record into one ROM */

void dsphex()
	{
	long loc,ccnt;


	while (!getrec())
		{
		while (line[1] == 'D' && line[2] == 'A' && line[3] == 'T' && line[4] == 'A'
		 		&& line[5] == ' ' && line[6] == 'P')       /*data header*/
		 	{
		 	loc = getnx(&line[8],4)+ pblock.file[filex].offset;
			while (!getrec() && (line[0] != '_'))
				{
				ccnt = 0;
				while (ccnt < (long)strlen(line))
					{
					putbin(pblock.file[filex].tid, loc++, (char)getnx(&line[ccnt+4],2));
					putbin(pblock.file[filex].tid, loc++, (char)getnx(&line[ccnt+2],2));
					putbin(pblock.file[filex].tid, loc++, (char)getnx(&line[ccnt],2));
 					ccnt += 7;
					}
				}
			}
		}	
	return;
	}

/* `puthex` - process a hex string */

#ifdef ANSI
void puthex(long loc, short count, char *hex)
#else
void puthex(loc, count, hex)
long loc;
short count;
char *hex;
#endif
	{
	char tid, ri, bank;
	char c;
	long si, di;

	if (count)
		{
		tid = pblock.file[filex].tid;
		loc = loc + pblock.file[filex].offset;
		for (di=  0; di<count; di++)
			{
			c = (char)getnx(hex++,2);
			hex++;
			while (loc >= rommap[tid].rom * pblock.mode/8)
				{
				loc -= rommap[tid].rom * pblock.mode/8;
				tid += (pblock.mode/8);
				if (tid >= units)
					{
					pblock.bad = line;
					result = ADDR;
					return;
					}
				}
			bank = (char)(loc % (pblock.mode/8));
			if (tid + bank >= units)
				{
				printf("Invalid Unit ID - %d\n",tid);
				pblock.bad = NULL;
				result = BADU;
				return;
				}
			if (rommap[tid + bank].flag)
				{
				if (!rommap[tid + bank].bi)
					{
					rommap[tid + bank].iloc = loc/(pblock.mode/8);
					}
				rommap[tid + bank].ibuf[rommap[tid + bank].bi++] = c;
				loc++;
				}
			else
				{
				if (putbin((char)(tid + bank),loc++/(pblock.mode/8),c))
					break;
				}
			}
		for (ri = 0;ri < units;ri++)
			{
			if (rommap[ri].flag && rommap[ri].bi)
				{
				if ((rommap[ri].iloc + rommap[ri].bi) > rommap[ri].rom)
					{
					printf("FILE = %s  address = %lx  unit = %x\n",
						pblock.file[filex].name,rommap[ri].iloc,ri);
					pblock.bad = line;
					result = ADDR;
					return;
					}
				if (rommap[ri].marker < (rommap[ri].iloc - 1))
					{
					if (result = image_loc(ri, 0L,2))
						return;
					for (si=rommap[ri].marker+1; si<rommap[ri].iloc; si++)
						if (result = image_write(ri, pblock.fill))
							return;
					}
				else if (result = image_loc(ri,rommap[ri].iloc,0))
					return;
				if ((si = image_wblock(ri,rommap[ri].ibuf,rommap[ri].bi)) < 0)
					{
					result = si;
					return;
					}
				if (si != rommap[ri].bi)
					{
					result = IOER;
					return;
					}
				if (rommap[ri].marker < (rommap[ri].iloc + rommap[ri].bi - 1))
					rommap[ri].marker = rommap[ri].iloc + rommap[ri].bi - 1;
				rommap[ri].bi = 0;
				}
			}
		}
	}

/* 'getspace' - get some memory or file space for ROM image */

#ifdef ANSI
long getspace(char ri)
#else
long getspace(ri)
char ri;
#endif
	{
#ifdef	MSDOS
	char _huge *lrmalloc();
	char _huge *hex;
#else
	char *lrmalloc();
	char *hex;
#endif
	long di;

	rommap[ri].marker = -1;
	if ((rommap[ri].madr=lrmalloc(rommap[ri].rom)) != (char *)0)
		{
		hex = rommap[ri].madr;
		if (pblock.flags & FILC)
			{
			for (di=0; di<rommap[ri].rom; di++)
				*hex++ = (char)pblock.fill;
			}
		if (pblock.flags & FFIL)
			{
            rommap[ri].marker = rommap[ri].rom -1;
			}
		rommap[ri].flag = 0;
		}
	else
		{
		printf("Opening file for image\n");
		if (!image_open(ri))
			{
			rommap[ri].flag = 1;
			}
		else
			{
			return(OPEN);
			}
		rommap[ri].madr = (char *)1;
		}
	if (ri < (NMAP-1))
		rommap[ri+1].madr = (char *)0;
	return(OKAY);
	}

/* `files` verify all files */

long files()
	{

	for (filex = 0; filex < NFILE; filex++)
		{
		if (pblock.file[filex].name == (char *)0)
			break;
		if (lropen())
			{
			pblock.bad = pblock.file[filex].name;
			return(OPEN);
			}
		lrclose();
		}
	filex = 0;
	readc = 0;
	if (lropen())
		{
		pblock.bad = pblock.file[filex].name;
		return(OPEN);
		}
	return(OKAY);
	}

/* get a line from input stream */

long getrec()
	{

	while (pblock.file[filex].code)
		{
		if (binary())
			return(result);
		lrclose();
		filex++;
		if (pblock.file[filex].name == (char *)0)
			return(ENDF);
		if (result = lropen())
			{
			pblock.bad = pblock.file[filex].name;
			return(result);
			}
		}
	if (pblock.file[filex].name == (char *)0)
		return(ENDF);
	for (linex=0; linex < LINSIZ; linex++)
		{
		if ((line[linex] = getbyte()) < 0)
			return(line[linex]);

		if (line[linex] == '\0')
			{
			linex--;
			continue;
			}

		if ((line[linex] == '\n') || (line[linex] == '\r'))
			{
			if (linex < 2)
				{
				linex = -1;
				continue;
				}
			else
				break;
			}
		}
	line[linex] = '\0';

	if (linex)
		return(OKAY);
	else
		return(getrec());
	}

/* `getbyte` - get one byte from current open file */

char getbyte()
	{

	if (bufx >= readc)
		{
		bufx = 0;
		if ((result = lrread()) < 0)
			{
			pblock.bad = pblock.file[filex].name;
			return((char)(result = IOER));
			}
		if (readc == 0)
			{
			lrclose();
			filex++;
			rca = 0;
			if (pblock.file[filex].name == (char *)0)
				return(ENDF);
			if (result = lropen())
				{
				pblock.bad = pblock.file[filex].name;
				return((char)result);
				}
			if ((result = lrread()) < 0)
				{
				pblock.bad = pblock.file[filex].name;
				return((char)(result = IOER));
				}
			}
		}
	return(buf[bufx++]);
	}

/* `getnx` - get hex data worth `n` chars from a string */

#ifdef ANSI
long getnx(char *str,short n)
#else
long getnx(str,n)
char *str;
short n;
#endif
	{
	long value;
	short si;

	for (si=0, value=0; si < n; si++)
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
			{
			pblock.bad = str;
			return(DATA);
			}
		}
	return(value);
	}

/* 'checkmi' - compute checksum on a hex record */

#ifdef ANSI
short checkmi(char *data, short length)
#else
short checkmi(data, length)
char *data;
short length;
#endif
	{
	short index;
	unsigned long checksum;

	checksum = 0;

	for (index = 0; index < length; index += 2, data++)
		checksum += getnx(data++,2);

	if (line[0] == ':')
		return((short)((~checksum + 1) & 0xFF));
	if (line[0] == ';')
		return((short)(checksum));
	else
		return((short)checksum & 0xff);
	}

/* 'checkst' - compute checksum on standard Tekhex record */

#ifdef ANSI
short checkst(char *data, short length)
#else
short checkst(data, length)
char *data;
short length;
#endif
	{
	short index;
	unsigned long checksum;

	checksum = 0;

	for (index = 0; index < length; index++, data++)
		checksum += getnx(data,1);

	return((short)checksum & 0xff);
	}

/* `checktek` - checksum Extended Tek hex record */

#ifdef ANSI
short checktek(char *buffer, short length)
#else
short checktek(buffer, length)
char *buffer;
short length;
#endif
		{
    	short checksum;
    	short index;

    	checksum = 0;

    	for (index = 1; index <= 3; index++)
        	checksum += et_checktable[buffer[index]];
    	for (index = 6; index <= length; index++)
        	checksum += et_checktable[buffer[index]];
    	return (checksum & 0xff);
    	}

void ckintek()
	{
	et_checktable['0'] = 0;
	et_checktable['1'] = 1;
	et_checktable['2'] = 2;
	et_checktable['3'] = 3;
	et_checktable['4'] = 4;
	et_checktable['5'] = 5;
	et_checktable['6'] = 6;
	et_checktable['7'] = 7;
	et_checktable['8'] = 8;
	et_checktable['9'] = 9;

	et_checktable['A'] = 10;
	et_checktable['B'] = 11;
	et_checktable['C'] = 12;
	et_checktable['D'] = 13;
	et_checktable['E'] = 14;
	et_checktable['F'] = 15;
	et_checktable['G'] = 16;
	et_checktable['H'] = 17;
	et_checktable['I'] = 18;
	et_checktable['J'] = 19;
	et_checktable['K'] = 20;
	et_checktable['L'] = 21;
	et_checktable['M'] = 22;
	et_checktable['N'] = 23;
	et_checktable['O'] = 24;
	et_checktable['P'] = 25;
	et_checktable['Q'] = 26;
	et_checktable['R'] = 27;
	et_checktable['S'] = 28;
	et_checktable['T'] = 29;
	et_checktable['U'] = 30;
	et_checktable['V'] = 31;
	et_checktable['W'] = 32;
	et_checktable['X'] = 33;
	et_checktable['Y'] = 34;
	et_checktable['Z'] = 35;

	et_checktable['$'] = 36;
	et_checktable['%'] = 37;
	et_checktable['.'] = 38;
	et_checktable['_'] = 39;

	et_checktable['a'] = 40;
	et_checktable['b'] = 41;
	et_checktable['c'] = 42;
	et_checktable['d'] = 43;
	et_checktable['e'] = 44;
	et_checktable['f'] = 45;
	et_checktable['g'] = 46;
	et_checktable['h'] = 47;
	et_checktable['i'] = 48;
	et_checktable['j'] = 49;
	et_checktable['k'] = 50;
	et_checktable['l'] = 51;
	et_checktable['m'] = 52;
	et_checktable['n'] = 53;
	et_checktable['o'] = 54;
	et_checktable['p'] = 55;
	et_checktable['q'] = 56;
	et_checktable['r'] = 57;
	et_checktable['s'] = 58;
	et_checktable['t'] = 59;
	et_checktable['u'] = 60;
	et_checktable['v'] = 61;
	et_checktable['w'] = 62;
	et_checktable['x'] = 63;
	et_checktable['y'] = 64;
	et_checktable['z'] = 65;
	}
                                                                       
