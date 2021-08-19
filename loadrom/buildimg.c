/*	buildimg.c - Edit 1.3			*/

/*	LoadROM Version 2.0 - March 1988	*/
/*	Copyright (C) 1988 Grammar Engine, Inc.	*/

#include "loadrom.h"

static	short et_checktable[256];

long	buildimg()
	{
	long files();
	long binary();
	long mhex();
	long ihex();
	long thex();
	long getrec();
	void ckintek();

	maxloc = pblock.roms * pblock.rom;
	if (files())
		return(result = OPEN);
	if (pblock.flags & IMAG)
		return(binary());
	else
		{
		ckintek();
		while (!getrec())
			{
			switch (line[0])
				{
				case 's':
				case 'S':
					mhex();
					break;
				case ':':
					ihex();
					break;
				case '%':
					thex();
					break;
				default:
					result  = DATA;
					pblock.bad = line;
					break;
				}
			if (result)
				return(result);
			}
		return(result);
		}
	}

/* `binary` - process a binary (process) image */

static	long binary()
	{
	extern long lrread();
	extern void lrclose();
	long putbin();
	long loc;

	loc = 0;
	bufx = pblock.file[0].offset;
	while(!lrread())
		{
		if (!readc)
			break;
		for (; bufx<readc; bufx++)
			if (putbin(loc++,buf[bufx]))
				break;
		if (result)
			{
			lrclose();
			return(result);
			}
		bufx = 0;
		}
	lrclose();
	return(result);
	}

/* `mhex ` - process Motorola hex record */

static	long mhex()
	{
	long getnx();
	long puthex();
	short checkmi();
	long loc;
	short count;
	char *hex;

	count = getnx(&line[2], 2);
	if (checkmi(&line[2], (count+1)*2) != 0xFF)
		{
		if (!(pblock.flags & XFLAG))
			{
			pblock.bad = line;
			return(result = CKSM);
			}
		}
	switch(line[1])
		{
		case '0':
			count = 0;
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
			break;
		case '8':
			count = 0;
			break;
		case '9':
			count = 0;
			break;
		default:
			count = 0;
			result = DATA;
			pblock.bad = line;
			break;
		}
	return(puthex(loc, count, hex));
	}

/* `ihex` - process Intel hex record */

static	long ihex()
	{
	long getnx();
	long puthex();
	short checkmi();
	long loc;
	short count;
	char *hex;
	static long ibase = 0;

	count = getnx(&line[1], 2);
	if (checkmi(&line[1],count*2+8) != (short)getnx(&line[9+count*2],2))
		{
		if (!(pblock.flags & XFLAG))
			{
			if (getnx(&line[7], 2) != 1)
				{
				pblock.bad = line;
				return(result = CKSM);
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
	return(puthex(loc, count, hex));
	}

/* `thex` - process Tektronix hex record */

static	long thex()
	{
	long getnx();
	long puthex();
	short checktek();
	long loc;
	short count, tmp, tmp1, si;
	char *hex;
	static long tbase = 0;

	count = getnx(&line[1], 2);
	if (checktek(line, count) != (short)getnx(&line[4], 2))
		{
		if (!(pblock.flags & XFLAG))
			{
			pblock.bad = line;
			return(result = CKSM);
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
			break;
		default:
			result = DATA;
			pblock.bad = line;
			count = 0;
			break;
		}
	return(puthex(loc, count, hex));
	}

/* `puthex` - process a hex string */

static	long puthex(loc, count, hex)
	long loc;
	short count;
	char *hex;
	{
	long getnx();
	long putbin();
	short di;
	char c;

	if (count)
		{
		loc = loc - romoff + pblock.file[filex].offset;
		if (loc > maxloc || loc < 0)
			{
			pblock.bad = line;
			return(result = ADDR);
			}
		for (di=0; di<count; di++)
			{
			c = (char)getnx(hex++,2);
			hex++;
			if (putbin(loc++,c))
				break;
			}
		}
	return(result);
	}

/* `putbin` - store a binary character into the rommap */

ENTRY	long putbin(loc,c)
	long loc;
	char c;
	{
	extern long image_loc();
	extern long image_write();
	long getspace();
	long si;
	short ri;
	static long lastloc = -1;

    	for (ri=0; ri<NMAP; ri++)
		{
		if (rommap[ri].madr == (char *)0)
			{
			if (result = getspace(ri))
				return(result);
			}
		if (loc > rommap[ri].end)
			continue;

		loc -= rommap[ri].start;
		if (!rommap[ri].flag)
			{
			*(rommap[ri].madr + loc) = c;
			break;
			}

		if (++lastloc == loc)
			{
			result = image_write(c);
			break;
			}

		lastloc = loc;
		if (rommap[ri].marker < loc)
			{
			if (result = image_loc(0L,2))
				return(IOER);
			
			for (si=rommap[ri].marker+1; si<loc; si++)
				if (result = image_write(pblock.fill))
					return(IOER);
			result = image_write(c);
			break;
			}
		if (result = image_loc(loc,0))
			break;
		result = image_write(c);
		break;
		}
	if (ri == NMAP)
		return(result = ENDF);
	if (rommap[ri].marker < loc)
		rommap[ri].marker = loc;
	return(result);
	}

/* 'getspace' - get some memory or file space for ROM image */

static	long getspace(ri)
	short ri;
	{
#ifdef	MSDOS
	extern char huge *lrmalloc();
	char huge *hex;
#else
	extern char *lrmalloc();
	char *hex;
#endif
	extern long image_open();
	long di;

	if ((rommap[ri].madr=lrmalloc(pblock.rom)) != (char *)0)
		{
		if (!ri)
			{
			rommap[ri].start = 0;
			rommap[ri].end = pblock.rom - 1;
			}
		else
			{
			rommap[ri].start = rommap[ri-1].end + 1;
			rommap[ri].end = rommap[ri].start + pblock.rom - 1;
			rommap[ri-1].marker =
				rommap[ri-1].end - rommap[ri-1].start;
			}
		hex = rommap[ri].madr;
		if (pblock.flags & FILC)
			for (di=0; di<pblock.rom; di++)
				*hex++ = (char)pblock.fill;
		rommap[ri].flag = 0;
		}
	else
		{
		if (!image_open())
			{
			if (!ri)
				{
				rommap[ri].start = 0;
				}
			else
				{
				rommap[ri].start = rommap[ri-1].end + 1;
				rommap[ri-1].marker =
					rommap[ri-1].end - rommap[ri-1].start;
				}
			rommap[ri].end= (pblock.roms * pblock.rom) - 1;
			rommap[ri].flag = 1;
			}
		else
			{
			return(OPEN);
			}
		rommap[ri].madr = (char *)1;
		}
	rommap[ri].marker = -1;
	if (ri < (NMAP-1))
		rommap[ri+1].madr = (char *)0;
	return(OKAY);
	}

/* verify and open all files */

static	long files()
	{
	extern long lropen();

	for (filex = 0; filex < NFILE; filex++)
		{
		if (pblock.file[filex].name == (char *)0)
			break;
		if (lropen())
			{
			pblock.bad = pblock.file[filex].name;
			return(OPEN);
			}
		}
	filex = 0;
	return(OKAY);
	}

/* get a line from input stream */

static	long getrec()
	{
	char getbyte();

	for (linex=0; linex < LINSIZ; linex++)
		{
		if ((line[linex] = getbyte()) == (char)ENDF)
			return(ENDF);
		if (line[linex] == '\n')
			break;
		}
	line[linex] = '\0';
	return(OKAY);
	}

/* `getbyte` - get one byte from current open file */

static	char getbyte()
	{
	extern void lrclose();

	if (bufx >= readc)
		{
		bufx = 0;
		if ((result = lrread()) < 0)
			{
			pblock.bad = pblock.file[filex].name;
			return(DATA);
			}
		if (readc == 0)
			{
			lrclose();
			filex++;
			if (pblock.file[filex].name == (char *)0)
				return(ENDF);
			if ((result = lrread()) < 0)
				{
				pblock.bad = pblock.file[filex].name;
				return(DATA);
				}
			}
		}
	return(buf[bufx++]);
	}

/* `getnx` - get hex data worth `n` chars from a string */

static	long getnx(str,n)
	char *str;
	short n;
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

/* 'check' - compute checksum on a hex record */

static	short checkmi(data, length)
	char *data;
	short length;
	{
	short index;
	unsigned long checksum;

	checksum = 0;

	for (index = 0; index < length; index += 2, data++)
		checksum += getnx(data++,2);

	if (line[0] == ':')
		return((short)((~checksum + 1) & 0xFF));
	return(checksum & 0xff);
	}
static	short checktek(buffer, length)
	char *buffer;
	short length;
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

static	void ckintek()
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
