/*	loadice.c - Edit 0.2	BAJ				*/

/*	LoadICE Version 1.4b - June	1990			*/
/*	Copyright (C) 1990 Grammar Engine, Inc.	*/

/* LoadICE shell -							*/
/*	Parase command line.					*/
/*	Build parameterblock `paramb`			*/
/*	Call image builder `buildimg()`			*/
/*	Check return code, process errors		*/
/*	Allow interactive editing and loading	*/

#define claimglobals
#include "loadice.h"
#include <stdio.h>

#ifndef	MAC
#include <errno.h>
#ifndef BSD
#include <stdlib.h>
#endif
#endif

#ifdef	MSDOS
#include <io.h>
#include <string.h>
#include <conio.h>
#endif

#ifdef	VMS
#include <processes.h>
#endif

#include "proto.h"
#ifdef ANSI
ENTRY  void main(int argc,char **argv)
#else
ENTRY  void main(argc,argv)
	   int argc;
	   char **argv;
#endif
		{
	extern int errno;
	char tid;

	setbuf(stdout, NULL);
	printf("LoadICE V 1.4b\nCopyright (C) 1990 Grammar Engine, Inc.\n");
	errno = 0;
	if (result = initpb())
		{
		error();
		byebye();
		}
	if (parsec(argc,argv))
		{
		error();
		byebye();
		}
	if (result = rawmode())
		{
		error();
		portopen = 0;
		rawoff();
		byebye();
		}
	portopen = 1;
	if (picon())
		{
		error();
		byebye();
		}
	if (units == 0)
		{
		result = CONN;
		error();
		byebye();
		}
	sizer();
	if (result)
		{
		error();
		byebye();
		}
	pblock.roms = units;
	if (units*8 < pblock.mode)
		pblock.mode = units * (char)8;
	printf("Operating Mode %d bits\n", (short)pblock.mode);

	for(maxloc = 0, tid = 0; tid < units; tid++)
		{
		if (result = getspace(tid))
			{
			error();
			byebye();
			}
		maxloc += rommap[tid].rom;
		}
	if (filex)
		{
		printf("Building ROM image\n");
		if (buildimg())
			{
			error();
			byebye();
			}
		if (pblock.flags & LOAD)
			{
			if (loadimg((char)0))
				error();
			}
		else
			command();
		}
	else
		{
		command();
		}
	if(result)
		error();
	for (tid = 0;tid < units; tid++)
		{
		if (rommap[tid].imagename != NULL)
			image_delete(tid);
		pimode(tid,(char)0);
		}
	byebye();
	}

/* `byebye` - reset all Promice units */

void byebye()
	{
	char 	tid;
	if(portopen)
		{
		for (tid = units - (char)1; tid >= 0; --tid)
			{
			picmd(0,tid,PIBR,1,(char)0xff,0,0);
			}
		portopen = 0;
		rawoff();
		}
	exit(SUCCESS);
	}
#ifdef ANSI
void sizer(void)
#else
void sizer()
#endif
	{
	char	tid,c,rsize;
	long 	l;

	for (tid = 0;tid < units; tid++)
		{
		for(l = rommap[tid].rom , esize = 0 ; l >= 2048 ; esize++)
			l /=2;
		rsize = pisize(tid);
		if (result)
			return;
		if ((rsize & 0x0F) < esize)
			{
			esize = (rsize & (char)0x0f);
			pimode(tid,(char)0);
			for (rommap[tid].rom = 1024, c = esize; c > 0; c--)
				rommap[tid].rom *=2;
			}
		rommap[tid].model = 1;
		rommap[tid].model = rommap[tid].model << ((rsize&0x0f) + 3);
		if (rommap[tid].model == 1024)
			rommap[tid].model = 10;
		else if (rommap[tid].model == 2048)
			rommap[tid].model = 20;
		else if (rommap[tid].model == 4096)
			rommap[tid].model = 40;
		else if (rommap[tid].model == 8192)
			rommap[tid].model = 80;
		printf("Module #%d Model %03d Emulating %ld bytes\n"
		,tid,rommap[tid].model,rommap[tid].rom);
		}
	}

/* `parsec` - parse command line */

#ifdef ANSI
long parsec(int argc, char **argv)
#else
long parsec(argc, argv)
int argc;
char **argv;
#endif
	{
	short ai;

	for (ai=1; ai<argc; ai++)
		{
		if (argv[ai][0] == '?')
			{
			help2();
			byebye();
			}
		if (argv[ai][0] == '-')
			{
			if ((result=parsesw(argv,&ai)) < 0)
				return(result);
			else
				continue;
			}
		pblock.file[filex].name = argv[ai];
		printf("filename = %s  ",pblock.file[filex].name);
		if (pblock.file[filex].skip)
			printf("skip = %lx  ",pblock.file[filex].skip);
		if (pblock.file[filex].offset < (long)0)
			printf("offset = - %lx\n",(0 - pblock.file[filex].offset));
		else
			printf("offset = %lx\n",pblock.file[filex].offset);
		filex +=1;
		if (filex == NFILE)
			{
			printf("Too many files %d\n",filex);
			pblock.bad = argv[ai];
			result = BARG;
			return(result);
			}
		pblock.file[filex].name = (char *)0;
		}
	return(result);
	}

/* `parsesw` - parse command line switch specifications */

#ifdef ANSI
long parsesw(char **argv, short *ndxh)
#else
long parsesw(argv, ndxh)
char **argv;
short *ndxh;
#endif
	{
	long value;
	char flag;
	char tid;

	switch(argv[*ndxh][1])
		{
		case 'b':
		case 'B':
			pblock.baud = value = getns(argv[++*ndxh]);
			if (value != 1200 && value != 2400 && value != 4800
			  && value != 9600 && value != 19200 && value != 57600)
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
		case 'h':
		case 'H':
			fastresp = 1;
			break;				
		case 'i':
		case 'I':
			pblock.file[filex].skip = getnum(argv[++*ndxh]);
			pblock.file[filex].code = IMAG;
			break;
		case 'k':
		case 'K':
			pblock.cka1 = gethex(argv[++*ndxh]);
			pblock.cka2 = gethex(argv[++*ndxh]);
			pblock.cka3 = gethex(argv[++*ndxh]);
			pblock.flags |= CHEK;
			break;
		case 'l':
		case 'L':
			pblock.flags |= LOAD;
			break;
		case 'm':
		case 'M':
			if(*(argv[*ndxh+1]+1) == ':')
				{
				pblock.ltid = (char)gethex(argv[++*ndxh]);
				pblock.lstart = gethex(argv[*ndxh]+2);
				pblock.lend = gethex(argv[++*ndxh])+1;
				if(pblock.lend < pblock.lstart)
					{
					pblock.bad = argv[--*ndxh];
					return(BARG);
					}
				pblock.partial = 1;
				}
			break;
		case 'n':
		case 'N':
			break;
		case 'o':
		case 'O':
			pblock.portname = argv[++*ndxh];
			break;
		case 'p':
		case 'P':
			parallel = 1;
			break;
		case 'r':
		case 'R':
			flag = tid = 0;
			if(*(argv[*ndxh+1]+1) == ':')
				{
				flag = 1;
				tid = (char)gethex(argv[++*ndxh]);
				value = getns(argv[*ndxh]+2);
				}
			else
				value = getns(argv[++*ndxh]);
			value = romvalue(value);
			if (!value)
				return(BARG);
			if (flag)
				rommap[tid].rom = value;
			else
				{
				for (tid = 0; tid < NMAP; tid++)
					rommap[tid].rom = value;
				}
			break;
		case 's':
		case 'S':
			value = getnum(argv[++*ndxh]);
			pblock.file[filex].offset = value;
			break;
		case 'u':
		case 'U':
			value = getnum(argv[++*ndxh]);
			pblock.file[filex].tid = (char)value;
			if (pblock.file[filex].tid >= NMAP)
				{
				printf("Unit id too high\n");
				pblock.bad = argv[*ndxh];
				return(BARG);
				}
			break;
		case 'v':
		case 'V':
			verify = 0;
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
		case 'z':
		case 'Z':
			ignorebadadd=1;
			break;
		case '0':
			forced = 1;
			break;
		default:
			pblock.bad = argv[*ndxh];
			return(BARG);
			break;
		}
	return(OKAY);
	}

/* `loadimg` - load the ROMimage to the Promice */

#ifdef ANSI
long loadimg(char compare)
#else
long loadimg(compare)
char compare;
#endif
	{
#ifdef	MSDOS
	char _huge *hex;
#else
	char *hex;
#endif
	long size, tcount, vcount, fi, di, dcnt, ccount;
	char ti, tid, ri;
	char exadd, hiadd, loadd;
	long ptradd, rc;
	short pcount;
	unsigned char df[256],cf[64];
	char ptrflag = 1;
  tcount = ccount = 0;
  pcount = 0;
  if(!compare)
    {
	if (pblock.flags & CHEK)
		checksum();
	if (parallel)
		printf("Loading on parallel port\n");
	else
		printf("Loading @ baudrate = %ld\n",pblock.baud);
	tcount=0;
	for (ri=0; ri < units; ri++)
		{
		tid = ri;
		vcount = 0;
		if (hex = rommap[ri].madr)
			{
			size = rommap[ri].marker + 1;
			if (rommap[ri].flag)
				(void)image_loc(ri, (long)0,0);
			if(pblock.partial)
				{
				size = pblock.lend - pblock.lstart;
				vcount = pblock.lstart;
				tid = pblock.ltid;
				hex = rommap[tid].madr + pblock.lstart;
				if (rommap[ri].flag)
					(void)image_loc(ri, pblock.lstart,0);
				ri = units;
				}
			while(size && !result)
				{
				if (!(vcount & 0xffff) || ptrflag)
					{
					ptrflag = 0;
					ptradd = 0x100000 - rommap[tid].rom + vcount;
					loadd = (char)(ptradd & 0xff);
					hiadd = (char)((ptradd >> 8) & 0xff);
					exadd = (char)((ptradd >>16) & 0xff);
					pimode(tid,(char)1);
					picmd(1,tid,PILP,3,exadd,hiadd,loadd);
					}
				spinccw(&pcount);
				if (size >= 256)
					{
					size -= 256;
					di = 256;
					}
				else
					{
					di = size;
					size = 0;
					}
				if (rommap[ri].flag)
					{
					rc = image_bread(ri, &df[0],(int)di);
					if (rc != di)
						{
						di = rc;
						result = ENDF;
						break;
						}
					}
				else
					{
					for (dcnt=0;dcnt <di;dcnt++)
						df[dcnt] = *hex++;
					}
				wrblock(tid,(short)di,df);
				tcount += di;
				vcount += di;
				}
			}
		else
			break;
		}
	if (result)
		{
		error();
		byebye();
		}
	else
		printf("Loaded\n");
	}	/* End of if !compare*/

	if(verify || compare)
		{
		for (fi=0, ri=0; ri < units; ri++)
			{
			tid = (char)ri;
			vcount = 0;
			if (hex = rommap[ri].madr)
				{
				size = rommap[ri].marker + 1;
				if (rommap[ri].flag)
					(void)image_loc(ri, 0L,0);
				if(pblock.partial)
					{
					size = pblock.lend - pblock.lstart;
					vcount = pblock.lstart;
					tid = pblock.ltid;
					hex = rommap[tid].madr + pblock.lstart;
					if (rommap[ri].flag)
						(void)image_loc(ri, pblock.lstart,0);
					ptrflag = 1;
					ri = units;
					}
				while(size)				
					{
					if (!(vcount & 0xffff) || ptrflag)
						{
						ptrflag = 0;
						ptradd = 0x100000 - rommap[tid].rom + vcount;
						loadd = (char)(ptradd & 0xff);
						hiadd = (char)((ptradd >> 8) & 0xff);
						exadd = (char)((ptradd >>16) & 0xff);
						pimode(tid,(char)1);
						picmd(0,tid,PILP|NORESP,3,exadd,hiadd,loadd);
						}
					if (size >= 64)
						{
						size -= 64;
						di = 64;
						}
					else
						{
						di = (char)size;
						size = 0;
						}
					rdblock(tid,(short)di,df);
					if (result)
						return(result);
					if (rommap[ri].flag)
						{
						rc = image_bread(ri, &cf[0],(int)di);
						if (rc != di)
							{
							di = rc;
							result = ENDF;
							return(result);
							}
						}
					else
						{
						for (ti=0; ti< (char)di; ti++)
							{
							cf[ti] = *hex++;
							}
						}
					for (ti=0; ti< (char)di; ti++)
						{
						if (cf[ti] != df[ti])
							{
							printf("(%x:%05lx I=%02x ",tid,vcount,cf[ti]&0xff);
							printf("P=%02x)", df[ti]&0xff);
							if (++fi >= 4)
								{
								printf("\n");
								fi=0;
								}
							ccount += 1;
							if (ccount == 12)
								{
								printf("Do you want to continue displaying errors?");
								gets(line);
#ifdef	MAC
		if (strlen(line) > 59)
			strcpy(line,&line[60]);
#endif
								if (line[0] != 'Y' && line[0] != 'y')
								goto STOPCOMP;
								}
							}
						vcount += 1;
						}
					spincw(&pcount);
					}
				}
			else
				break;
			}
STOPCOMP:
		printf("Verified\n");
		}   /* END OF IF(VERIFY || COMPARE) */
	pblock.partial = 0;
	if (!compare)
		printf("Loading complete - %ld data bytes transfered\n", tcount);
		beep();
		for (tid = 0;tid < units; tid++)
		pimode(tid,(char)0);
	(void)pireset(pblock.rtime);
	return(result);
	}

/* `setptr` - set the pointer in the Promice */

#ifdef ANSI
void setptr(char tid, long addr)
#else
void setptr(tid, addr)
char tid;
long addr;
#endif
	{
	char exadd,hiadd,loadd;
	long ptradd;

	ptradd = 0x100000 - rommap[tid].rom + addr;
	loadd = (char)(ptradd & 0xff);
	hiadd = (char)((ptradd >> 8) & 0xff);
	exadd = (char)((ptradd >>16) & 0xff);
	picmd(1,tid,(PILP),3,exadd,hiadd,loadd);
	}

/* `wrblock` - write a block of data to the Promice */

#ifndef PI_BLOCK
#ifdef ANSI
void wrblock(char tid, short ccnt, char *dptr)
#else
void wrblock(tid, ccnt, dptr)
char tid;
short ccnt;
char *dptr;
#endif
	{
	char	c;
#ifdef	MSDOS
	extern void putpar();
	extern void putpar2();

	if (!parallel || ccnt == 1)			/* SERIAL PORT */
		{
#endif
		putcom(tid);
		putcom(PIWR | NORESP);
		putcom((char)ccnt);
		for (; ccnt != 0; --ccnt)
			{
			c = *dptr++;
			putcom(c);
			}
#ifdef	MSDOS
		}
	else					/* PARALLEL PORT */
		{
		if (tid == 0 || tid == 1)
			{
			putpar(tid);
			putpar(PIWR | NORESP);
			putpar((char)ccnt);
			for (; ccnt != 0; --ccnt)
				{
				c = *dptr++;
				putpar(c);
				}
			}
		else
			{
			putpar2(tid);
			putpar2(PIWR | NORESP);
			putpar2((char)ccnt);
			for (; ccnt != 0; --ccnt)
				{
				c = *dptr++;
				putpar2(c);
				}
			}
		}
#endif
	}		

/* `rdblock` - read a block of data from the Promice */

#ifdef ANSI
void rdblock(char tid, short ccnt, char *ibuf)
#else
void rdblock(tid, ccnt, ibuf)
char tid;
short ccnt;
char *ibuf;
#endif
	{
	char	a;
	unsigned char	b,d,fi;
	short	lsreg,rxbuf;
#ifndef MSDOS
	char c;
#endif
	d = 0;
	picmd(0,tid,PIRD,1,(char)ccnt,0,0);
#ifdef MSDOS
	lsreg = LSREG;
	rxbuf = RXBUF;
	while (!(inp(lsreg) & RDA));
	a = (char)inp(rxbuf);
	while (!(inp(lsreg) & RDA));
	b = (char)inp(rxbuf);
	while (!(inp(lsreg) & RDA));
	d = (char)inp(rxbuf);
	for (fi = 0; fi < d;)
		{
		while (!(inp(lsreg) & RDA));
		ibuf[fi++] = (char)inp(rxbuf);
		}
#else
	while(getcom(&a));
	while(getcom(&b));
	while(getcom(&d));
	for (fi = 0; fi < d;)
		{
		while(getcom(&c));
		ibuf[fi++]=c;
		}
#endif
	if(a != tid)
		result = COMM;
	if(b != (unsigned char)(PIRD | 0x80))
		result = COMM;
	if(d != (unsigned char)ccnt)
		result = COMM;
	}
#endif /*PI_BLOCK*/

/* `pimode` - send mode command to Promice with no response */

#ifdef ANSI
void pimode(char tid, char mode)
#else
void pimode(tid, mode)
char tid;
char mode;
#endif
	{
	putcom(tid);
	putcom(PIMO | NORESP);
	putcom((char)1);
	if(fastresp)
		putcom((char)(mode | FAST));
	else
		putcom(mode);
	return;
	}

/* `pisize` - send mode command to Promice and return size */

#ifdef ANSI
char pisize(char tid)
#else
char pisize(tid)
char tid;
#endif
	{
	char c[4];
	char ccnt = 0;

	putcom(tid);
	putcom((char)PIMO);
	putcom((char)1);
	putcom((char)0x0);
	do
		{
		while(getcom(&c[ccnt]));
		}
	while (ccnt++ < 3);
	if (c[0] != tid)
		return((char)(result = MISC));
	if (c[1] != (char)(PIMO | 0x80))
		return((char)(result = MISC));
	if (c[2] != (char)1)
		return((char)(result = MISC));
	return(c[3]);
	}

/* `pireset` - send reset target command to Promice */

#ifdef ANSI
char pireset(char time)
#else
char pireset(time)
char time;
#endif
	{
	char c[4];
	char ccnt = 0;
	if (!time)
		return(0);
	putcom((char)0);
	putcom((char)PIRT);
	putcom((char)1);
	putcom(time);
	do
		{
		while(getcom(&c[ccnt]));
		}
	while (ccnt++ < 3);
	if (c[0] != (char)0)
		return((char)(result = COMM));
	if (c[1] != (char)(PIRT | 0x80))
		return((char)(result = COMM));
	if (c[2] != (char)1)
		return((char)(result = COMM));
	return(c[3]);
	}

/* `pitest` - send test RAM command to Promice */

#ifdef ANSI
char pitest(char tid, char pc)
#else
char pitest(tid, pc)
char tid;
char pc;
#endif
	{
	char ibuf[5], a, b, c, d, fi;

	putcom(tid);
	putcom(PITS);
	putcom((char)1);
	putcom(pc); 
	while(getcom(&a));
	while(getcom(&b));
	while(getcom(&d));
	if (d > 5)
	    d = 5;
	for (fi = 0; fi < d;)
		{
		while(getcom(&c));
		ibuf[fi++]=c;
		}
	if (a != tid)
		{
		return((char)(result = COMM));
		}
	if (b != (char)(PITS | 0x80))
		{
		return((char)(result = COMM));
		}
	if (d != (char)1)
		{
		printf("Memory failed at %02x%02x%02x\n",ibuf[0]&0xff,ibuf[1]&0xff,ibuf[2]&0xff);
		return(MISC);
		}
	return(ibuf[0]);
	}

/* `piversion` - find version of Promice firmare */

void piversion()
	{
	char ibuf[5], a, b, c, d, tid; 
	char ccnt = 0;
	long fi;

	if (line[1] == '\000' | line[2] == '?')
		{
		printf("Correct usage - V unit\n");
		return;
		}
	tid = (char)gethex(&line[2]);
	if (tid >= units)
		{
		printf("Unit id too high\n");
		return;
		}
	putcom(tid);
	putcom(PIVS);
	putcom((char)1);
	putcom((char)0); 
	while(getcom(&a));
	while(getcom(&b));
	while(getcom(&d));
	if (d > 4)
	    d = 4;
	for (fi = 0; fi < d;)
		{
		while(getcom(&c));
		ibuf[fi++]=c;
		}
	ibuf[fi] = 0;
	if (a != tid)
		{
		result = COMM;
		return;
		}
	if (b != (char)(PIVS | 0x80))
		{
		result = COMM;
		return;
		}
	if (d != (char)4)
		{
		result = COMM;
		return;
		}
	printf("Promice Version %s\n",ibuf);
	}

/* `picon` - establish link with Promice */

long picon()
	{
	char c = 0;
	char d, tid;

	printf("Establishing connection with Promice");
	for(tid = units-(char)1;tid >= 0;--tid)
		picmd(0,tid,PIBR,1,(char)0xff,0,0);
	sleep(1);
	putcom(PIBR);
	sleep(1);
	putcom(PIBR);
	sleep(1);
	do
		{
		while (getcom(&c))
			{
			putcom(PIBR);
			sleep(1);
			}
		}
	while (c != PIBR);
	printf("\n");
	putcom(PID);
	putcom((char)0);
	while (getcom(&c));
	while (c == PIBR)
		{
		while (getcom(&c));
		}
	while(getcom(&d));
	units = d;
	if (c != PID)
		{
		result = CONN;
		return(result);
		}
	return(OKAY);
	}

/* `picmd` - send command to Promice */

#ifdef ANSI
void picmd(char response, char uid, char command,
	 char count, char data, char aux1, char aux2)
#else
void picmd(response, uid, command, count, data, aux1, aux2)
char response,uid,command,count,data,aux1,aux2;
#endif
	{
	putcom(uid);
	putcom(command);
	putcom(count);
	putcom(data);
	if (--count)
		{
		putcom(aux1);
		if (--count)
			putcom(aux2);
		}
	if (response)
		pirsp(response);
	}

#ifdef ANSI
void pirsp(char code)
#else
void pirsp(code)
char code;
#endif
	{
	short index,len,ci;
	char c, stuff[256];

	for (ci=index=0; index<3; index++)
		{
		while(getcom(&c));
		stuff[ci++]=c;
		}
	len = (short)c;
	if (code < 2)
		for (index=0; index<len; index++)
			{		
			while(getcom(&c));
			stuff[ci++]=c;
			}
		globc=c;
	}
/* `checksum` - compute and store checksum */

void checksum()
	{
	char ck;
	long i;

	printf("Computing Checksum \n");
	for (ck=0,i=pblock.cka1; i<=pblock.cka2; i++)
		ck += getbin(pblock.file[0].tid,i);

	(void)putbin(pblock.file[0].tid,pblock.cka3,(char)(-ck));
	printf("Checksum = %x Stored at %x:%lx\n",
			(unsigned char)(-ck),pblock.file[0].tid,pblock.cka3);
	if(result)
		error();
	}

/* `command` - operate command level interactive */

void command()
	{
	for (;;)
		{
		if (result)
			error();
		printf("LoadICE: ");
		gets(line);
#ifdef	MAC
		if (strlen(line) > 8)
			strcpy(line,&line[9]);
#endif
		switch (line[0])
			{
			case 'a':
			case 'A':
				switch (line[1])
					{
					case 'w':
					case 'W':
						aiwrite();
						break;
					case 'b':
					case 'B':
						aibitch();
						break;
					case 'r':
					case 'R':
						airead();
						break;
					default:
						break;
					}
				break;
			case 'b':
			case 'B':
				if (busreqmode)
					{
					printf("Bus Request Mode is off\n");
					busreqmode = 0;
					}
				else
					{
					printf("Bus Request Mode is on\n");
					busreqmode = 1;
					}
				break;
			case 'c':
			case 'C':
				compare();
				break;
			case 'd':
			case 'D':
				dump();
				break;
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
			case 'f':
			case 'F':
				fill();
				break;
			case 'm':
			case 'M':
				move();
				break;
			case 'p':
			case 'P':
				pmove();
				break;
			case 's':
			case 'S':
				save();
				break;
			case 'h':
			case 'H':
				hexload(1);
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
				printf("System function not enabled under VMS\n");
#else
#ifndef MAC
				(void)system(&line[1]);
#endif
#endif
				break;
			case '?':
				help();
				break;
			case 'x':
			case 'X':
				return;
				break;
			case 'r':
			case 'R':
				resetpi();
				break;
			case 'v':
			case 'V':
				piversion();
				break;
			case 'w':
			case 'W':
				writepi();
				break;
			case 't':
			case 'T':
				testpi();
				break;
			default:
				printf("Unknown command\n");
				break;
			}
		}
	}
/* `airead` - send read AI command to Promice */

void airead()
	{
	char ibuf[4], a, b, c, d, fi;

	putcom((char)0);
	putcom(AIRD);
	putcom((char)1);
	putcom((char)gethex(&line[3]));
	while(getcom(&a));
	while(getcom(&b));
	while(getcom(&d));
	for (fi = 0; fi < d;)
		{
		while(getcom(&c));
		ibuf[fi++]=c;
		}
	if (a != 0)
		{
		result = COMM;
		return;
		}
	if (b != (char)(AIRD | 0x80))
		{
		result = COMM;
		return;
		}
	if (d != (char)1)
		{
		result = COMM;
		return;
		}
	printf("The AI returned %02x\n",(ibuf[0] & 0xff));
	return;
	}
/* `aiwrite` - send write AI command to Promice */

void aiwrite()
	{
	char ibuf[4], a, b, c, d, fi;

	putcom((char)0);
	putcom(AIWR);
	putcom((char)2);
	putcom((char)gethex(&line[3])); 
	putcom((char)gethex(&line[6]));
	while(getcom(&a));
	while(getcom(&b));
	while(getcom(&d));
	for (fi = 0; fi < d;)
		{
		while(getcom(&c));
		ibuf[fi++]=c;
		}
	if (a != 0)
		{
		result = COMM;
		return;
		}
	if (b != (char)(AIWR | 0x80))
		{
		result = COMM;
		return;
		}
	if (d != (char)1)
		{
		result = COMM;
		return;
		}
	printf("The AI returned %02x\n",(ibuf[0] & 0xff));
	return;
	}
/* `aibitch` - send AI bit change command to Promice */

void aibitch()
	{
	char ibuf[4], a, b, c, d, fi;

	putcom((char)0);
	putcom(AIBC);
	putcom((char)2);
	putcom((char)gethex(&line[3])); 
	putcom((char)gethex(&line[6]));
	while(getcom(&a));
	while(getcom(&b));
	while(getcom(&d));
	for (fi = 0; fi < d;)
		{
		while(getcom(&c));
		ibuf[fi++]=c;
		}
	if (a != 0)
		{
		result = COMM;
		return;
		}
	if (b != (char)(AIBC | 0x80))
		{
		result = COMM;
		return;
		}
	if (d != (char)1)
		{
		result = COMM;
		return;
		}
	printf("The AI returned %02x\n",(ibuf[0] & 0xff));
	return;
	}

/* `testpi` - test the Promice */

void testpi()
	{
	char tid, c;

	if (line[1] == '\000' | line[2] == '?')
		{
		printf("Correct usage - T unit\n");
		return;
		}
	tid = (char)gethex(&line[2]);
	if (tid >= units)
		{
		printf("Unit id too high\n");
		return;
		}
	pimode(tid,(char)1);
	if (c = pitest(tid,(char)1))
		printf("\nUnit number %d failed the memory test\n",tid);
	else
		printf("\nUnit number %d passed the memory test\n",tid);
	}

/* `resetpi` - reset target */

void resetpi()
	{
	unsigned char time;

	if (line[1] == '\000' | line[2] == '?')
		{
		printf("Correct usage - R time (in milliseconds)\n");
		return;
		}
	time = (unsigned char)(getnum(&line[2]) / 9 + 1);
	if (time < 1)
		time = 1;
	if (time > 255)
		{
		printf("Max time is 2280 milliseconds\n");
		time = 255;
		return;
		}
	(void)pireset(time);
	}

/* `help` - help the user */

void help()
	{
	FILE *hf;
	char hstr[100];
#ifdef	MSDOS
	char pn[40];
	char *path, *str1, *str2;
#endif

	if ((hf = fopen("loadice.hlp", "r")) == NULL)
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
				(void)strcat(pn,"\\loadice.hlp");
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
/* `help2` - help the user */

void help2()
	{
	FILE *hf;
	char hstr[100];
#ifdef	MSDOS
	char pn[40];
	char *path, *str1, *str2;
#endif

	if ((hf = fopen("loadice2.hlp", "r")) == NULL)
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
				(void)strcat(pn,"\\loadice.hlp");
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

void readpi()
	{
	long loc;
	short	ccnt;
	char tid;
	unsigned char c;

	if (line[1] == '\000' | line[2] == '?')
		{
		printf("Correct usage - R unit:address\n");
		return;
		}
	for (ccnt = 0; line[ccnt]!= ':' && line[ccnt]!=0; ccnt++);
	if (line[ccnt] == ':')
		{
		tid = (char)gethex(&line[2]);
		for (ccnt = 0;line[ccnt]!= ':' && line[ccnt]!=0 ; ccnt++);
		loc = gethex(&line[++ccnt]);
		}
	else
		{
		tid = 0;
		loc = gethex(&line[2]);
		}
	if (tid >= units)
		{
		printf("Unit id too high\n");
		return;
		}
	printf("%02x:%05lx ",tid,loc);
	pimode(tid,(char)1);
	setptr(tid,loc);
	rdblock(tid,1,&c);
	for (tid = 0;tid < units; tid++)
		{
		pimode(tid,(char)0);
		}
	printf("%02x ",c&0xff);
	printf("\n");
	}

void writepi()
	{
	long loc;
	char tid;
	unsigned char c,ccnt;

	if (line[1] == '\000' | line[2] == '?')
		{
		printf("Correct usage - W unit:address,byte\n");
		return;
		}
	for (ccnt = 0; line[ccnt]!= ':' && line[ccnt]!=0; ccnt++);
	if (line[ccnt] == ':')
		{
		tid = (char)gethex(&line[2]);
		for (ccnt = 0;line[ccnt]!= ':' && line[ccnt]!=0 ; ccnt++);
		loc = gethex(&line[++ccnt]);
		}
	else
		{
		tid = 0;
		loc = gethex(&line[2]);
		}
	if (tid >= units)
		{
		printf("Unit id too high\n");
		return;
		}
	for (ccnt=0;line[ccnt]!= ',' && line[ccnt]!=0 ; ccnt++);
	c = (char)gethex(&line[++ccnt]);
	printf("%02x:%05lx %02x ",tid,loc,c&0xff);
	setptr(tid,loc);
	picmd(1,tid,PIMB,1,c,0,0);
	printf("\n");
	picmd(1,tid,PIMB|0x10,1,0,0,0);
	printf("its now %02x\n",globc&0xff);
	}

/* `dump` - dump 16 bytes from the image file */

void dump()
	{
	long loc;
	char tid;
	unsigned char c,ccnt;

	filex = 0;
	if (line[1] == '\000' | line[2] == '?')
		{
		printf("Correct usage - D unit:address\n");
		return;
		}
	for (ccnt = 0; line[ccnt]!= ':' && line[ccnt]!=0; ccnt++);
	if (line[ccnt] == ':')
		{
		tid = (char)gethex(&line[2]);
		for (ccnt = 0;line[ccnt]!= ':' && line[ccnt]!=0 ; ccnt++);
		loc = gethex(&line[++ccnt]);
		}
	else
		{
		tid = 0;
		loc = gethex(&line[2]);
		}
	if (tid >= units)
		{
		printf("Unit id too high\n");
		return;
		}
	if (loc >= rommap[tid].rom)
		{
		printf("Invalid Address %2x:%lx\n",tid,loc);
		return;
		}
	printf("%02x:%05lx ",tid,loc);
	if (!busreqmode)
		{
		pimode(tid,(char)1);
		setptr(tid,loc);
		for (ccnt = 0;ccnt < 16;ccnt++)
			{
			if (loc + ccnt >= rommap[tid].rom)
				{
				printf("\n");
				return;
				}
			rdblock(tid,1,&c);
			printf("%02x ",c&0xff);
			}
		for (tid = 0;tid < units; tid++)
			{
			pimode(tid,(char)0);
			}
		}
	else
		{
		for (ccnt = 0;ccnt < 16;ccnt++)
			{
			setptr(tid,loc++);
			if (loc >= rommap[tid].rom)
				{
				printf("\n");
				return;
				}
			picmd(1,tid,PIMB|0x10,1,0,0,0);
			printf("%02x ",globc&0xff);
			}
		}
	printf("\n");
	}

/* `edit` - edit the image file */

void edit()
	{
	long loc, ccnt;
	unsigned char c, ic;
	char tid;

	filex = 0;

	if (line[1] == '\000' | line[2] == '?')
		{
		printf("Correct usage - E unit:address\n");
		return;
		}
	for (ccnt = 0; line[ccnt]!= ':' && line[ccnt]!=0; ccnt++);
	if (line[ccnt] == ':')
		{
		tid = (char)gethex(&line[2]);
		for (ccnt = 0;line[ccnt]!= ':' && line[ccnt]!=0 ; ccnt++);
		loc = gethex(&line[++ccnt]);
		}
	else
		{
		tid = 0;
		loc = gethex(&line[2]);
		}
	if (tid >= units)
		{
		printf("Unit id too high\n");
		return;
		}
	for (;;)
		{
		if(!busreqmode)
			pimode(tid,(char)1);
		setptr(tid,loc);
		if(!busreqmode)
			rdblock(tid,1,&c);
		else
			{
			picmd(1,tid,PIMB|0x10,1,0,0,0);
			c = globc;
			}
		if (loc >= rommap[tid].rom)
			{
			printf("Invalid Address %2x:%lx\n",tid,loc);
			return;
			}
		if (rommap[tid].marker >= loc)
			ic = (char)getbin(tid,loc);
		else
			ic = 0xff;
		printf("%02x:%05lx %02x  [%02x]",tid,loc,c&0xff,ic&0xff);
		if (gets(line) == NULL)
			return;
#ifdef	MAC
		if (strlen(line) > 16)
			strcpy(line,&line[17]);
#endif
		if (line[0] == 'x' | line[0] == 'X' | line[0] == '.')
			{
			if (!busreqmode)
				{
				for (tid = 0;tid < units; tid++)
					{
					pimode(tid,(char)0);
					}
				}
			return;
			}
		if (line[0] == '\000')
			{
			loc++;
			if (loc >= rommap[tid].rom)
				loc--;
			continue;
			}
		if (line[0] == '^')
			{
			loc--;
			if (loc < 0) 
				loc = 0;
			continue;
			}
		c = (char)gethex(line);
		setptr(tid,loc);
		if (!busreqmode)
			wrblock(tid,1,&c);
		else
			picmd(1,tid,PIMB,1,c,0,0);

		if (verify)
			{
			if (!busreqmode)
				{
				setptr(tid,loc);
				rdblock(tid,1,&ic);
				}
			else
				{
				picmd(1,tid,PIMB|0x10,1,0,0,0);
				ic = globc;
				}
			if (c != ic)
				printf("Verify failed");
			}
		loc++;
		if (loc >= rommap[tid].rom)
			loc--;
		}
	}

/* `fill` - fill some area of ROM image */

void fill()
	{
	long saddr, eaddr, value, cnt, di;
	char tid;

	saddr = eaddr = value = 0;
	if (line[3] == ':')
		{
		if (sscanf(&line[1], "%x:%lx %lx %lx", &di, &saddr, &eaddr, &value) != 4)
			{
			printf("Usage:  F Unit:StartAddress EndAddress HexValue\n");
			return;
			}
		tid = (char)di;
		}
	else
		{
		tid = 0;
		if (sscanf(&line[1], "%lx %lx %lx", &saddr, &eaddr, &value) != 3)
			{
			printf("Usage: F Unit:StartAddress EndAddress HexValue\n");
			return;
			}
		}
	if (saddr > eaddr)
		{
		printf("StartAddress can not be greater than EndAddress\n");
		printf("StartAddress = %lx   EndAddress = %lx\n",saddr,eaddr);
		return;
		}
	if (eaddr >= rommap[tid].rom)
		{
		result = ADDR;
		return;
		}
	if (tid >= units)
		{
		printf("Unit number too high\n");
		return;
		}
	if (rommap[tid].flag)
		{
		if (!(result = image_loc(tid, saddr,0)))
			{
			while(saddr < eaddr)
				{
				if ((long)(eaddr - saddr) < (long)512)
					di = eaddr - saddr;
				else
					di = (long)512;
				for (cnt = 0; cnt < di; cnt++)
					buf[cnt]=(unsigned char)value;
				cnt = image_wblock(tid, buf, (short)di);
				if (cnt != di)
					{
					result = MUCH;
					return;
					}
				saddr += di;
				}
			if (rommap[tid].marker < eaddr)
				rommap[tid].marker = eaddr;
			}
		}
	while (saddr <= eaddr && result == OKAY)
		{
		putbin(tid,saddr++,(unsigned char)value);
		}
	}

/* `move` - move some area of ROM image */

void move()
	{
	long saddr, eaddr, daddr, si, di;
	char stid, dtid;

	saddr = eaddr = daddr = 0;
	if (sscanf(&line[1], "%X:%X %X %X:%X",&si, &saddr, &eaddr, &di, &daddr) != 5)
		{
		printf("Usage: M SourceUnit:StartAddress EndAddress DestUnit:DestAddr\n");
		return;
		}
	stid = (char)si;
	dtid = (char)di;
	if (daddr>saddr && daddr<eaddr && stid == dtid)
		{
		printf("invalid destination address\n");
		return;
		}
	if (saddr > eaddr)
		{
		printf("StartAddress can not be greater than EndAddr\n");
		return;
		}
	if (stid >= units || dtid >= units)
		{
		printf("Unit number too high\n");
		return;
		}
	while (saddr <= eaddr && result == OKAY)
		putbin(dtid,daddr++,(char)getbin(stid,saddr++));
	}

/* `pmove` - move some area of ROM image, allows overlapped move */

void pmove()
	{
	long saddr, eaddr, daddr, si, di;
	char stid, dtid;

	saddr = eaddr = daddr = 0;
	if (sscanf(&line[1], "%X:%X %X %X:%X",&si, &saddr, &eaddr, &di, &daddr) != 5)
		{
		printf("Usage: P SourceUnit:StartAddress EndAddress DestUnit:DestAddr\n");
		return;
		}
	stid = (char)si;
	dtid = (char)di;
	if (saddr > eaddr)
		{
		printf("StartAddress can not be greater than EndAddr\n");
		return;
		}
	if (stid >= units || dtid >= units)
		{
		printf("Unit number too high\n");
		return;
		}
	while (saddr <= eaddr && result == OKAY)
		putbin(dtid,daddr++,(char)getbin(stid,saddr++));
	}

/* `save` - save the ROM image file */

void save()
	{
	char *fname, *get_name(), ibuf[64];
	FILE *sf, *fopen();
#ifdef	MSDOS
	char _huge *hex;
#else
	char *hex;
#endif
	long size, di;
	char ri;
	char c, ccnt, tid;

	if ((fname = get_name()) == NULL)
		{
		printf("No filename specified\n");
		return;
		}
	if ((sf = fopen(fname, "wb")) == NULL)
		{
		perror("Save file");
		return;
		}
	printf("Would you like the Image or Promice contents? ");
	if (gets(line) == NULL)
		return;
#ifdef	MAC
	if (strlen(line) > 45)
		strcpy(line,&line[46]);
#endif
	if (line[0] == 'i' || line[0] == 'I')
		{
		printf("Saving Image to file\n");
		for (ri=0; ri < units; ri++)
			{
			if (hex = rommap[ri].madr)
				{
				size = rommap[ri].marker + 1;
				if (rommap[ri].flag)
					{
					if(image_loc(ri, 0L,0))
						{
						result = IOER;
						return;
						}
					}
				for (di=0; di < size; di++)
					{
					if (rommap[ri].flag)
						{
						if (image_read(ri, &c))
							{
							result = IOER;
							return;
							}
						}
					else
						c = *hex++;
					fputc(c, sf);
					}
				if (size)
					for (di=size; di < rommap[ri].rom; di++)
						fputc(pblock.fill,sf);
				}
			else
				break;
			}
		}
	else
		{
		printf("Which unit would you like to save? ");
		if (gets(line) == NULL)
			return;
#ifdef	MAC
		if (strlen(line) > 34)
			strcpy(line,&line[35]);
#endif
		tid = (char)gethex(line);
		if (tid >= units)
			{
			printf("Unit number too high\n");
			return;
			}
		size = rommap[tid].rom;
		pimode(tid,(char)1);
		for (di=0; di < size && !result;)
			{
			if (size - di < 64)
				c = (char)(size - di);
			else
				c = 64;
			if (!(di & 0xffff))
				{
				setptr(tid,di);
				}
			rdblock(tid,c,ibuf);
			di += c;
			for (ccnt = 0 ; ccnt < c ; ccnt++)
				{
				fputc(ibuf[ccnt], sf);
				}
			}
		for (tid = 0;tid < units; tid++)
			{
			pimode(tid,(char)0);
			}
		}
	fflush(sf);
#ifndef MAC
	size = filelength(fileno(sf));
	printf("File size = %ld (0x%lx)\n",size, size);
#endif
	fclose(sf);
	}

/* `compare` - compare image with Promice */

void compare()
	{
	if (loadimg((char)1))
		error();
	}

/* `load` - load a given file in to the Promice */

#ifdef ANSI
void load(short n)
#else
void load(n)
short n;

#endif
	{
	char *fname, *get_name();
	long di;
	if (sscanf(&line[1], "%x:%lx %lx", &di, &pblock.lstart, &pblock.lend) == 3)
		{
		pblock.ltid = (char)di;
		pblock.lend += 1;
		printf("Loading unit %x from %lx to %lx\n",pblock.ltid,pblock.lstart,pblock.lend-1);
		if (pblock.ltid >= units)
			{
			printf("Unit id too high\n");
			return;
			}
		if (pblock.lend <= pblock.lstart)
			{
			printf("End address must be greater than start address\n");
			return;
			}
		if (pblock.lend > rommap[pblock.ltid].marker+1)
			{
			printf("The image is only valid up to %lx\n",rommap[pblock.ltid].marker+1);
			return;
			}
		pblock.partial = 1;
		}
	else if (sscanf(&line[1], " %lx %lx", &pblock.lstart, &pblock.lend) == 2)
		{
		pblock.ltid =0;
		pblock.lend +=1;
		printf("Loading unit %x from %lx to %lx\n",pblock.ltid,pblock.lstart,pblock.lend-1);
		if (pblock.lend <= pblock.lstart)
			{
			printf("End address must be greater than start address\n");
			return;
			}
		if (pblock.lend > rommap[pblock.ltid].marker+1)
			{
			printf("The image is only valid up to %lx\n",rommap[pblock.ltid].marker+1);
			return;
			}
		pblock.partial = 1;
		}
	else if ((fname = get_name()) != NULL)
		{
		filex = 0;
		pblock.file[0].name = fname;
		pblock.file[0].offset = 0;
		pblock.file[1].name = NULL;
		if (n == 2)
			pblock.file[0].code = HEXF;
		else
			pblock.file[0].code = IMAG;
		printf("Building ROM image\n");
		if (buildimg())
			{
			error();
			return;
			}
		}
	if (n)
		{
		if (loadimg((char)0))
			error();
		}
	}

/* `hexload` - load a hex file in to the Promice */

#ifdef ANSI
void hexload(short n)
#else
void hexload(n)
short n;
#endif
	{
	char *fname,*get_name();
	char lline[LINSIZ+1];

	filex = 0;
	if ((fname = get_name()) != NULL)
		{
		strcpy(finame[filex],fname);
		printf("Do you want to clear the current image and load a hex file? ");
		gets(line);
#ifdef	MAC
		if (strlen(line) > 59)
			strcpy(line,&line[60]);
#endif
		if (line[0] == 'Y' || line[0] == 'y')
			image_clear();
		else
			return;
		printf("Is the data 8, 16, or 32 bit format? ");
		gets(lline);
#ifdef	MAC
		if (strlen(lline) > 36)
			strcpy(lline,&lline[37]);
#endif
		switch(lline[0])
			{
			case '8':
				pblock.mode = 8;
				break;
			case '1':
				pblock.mode = 16;
				break;
			case '3':
				pblock.mode = 32;
				break;
			default:
				return;
				break;
			}
		while (line[0] == 'Y' || line[0] == 'y')
			{
			pblock.file[filex].name = finame[filex];
			pblock.file[filex].code = HEXF;
			pblock.file[filex + 1].name = (char *)0;
			
			printf("Which Unit would you like to load?(default = 0) ");
			if (gets(lline) == NULL)
				pblock.file[filex].tid = 0;
			else
				{
#ifdef	MAC
				if (strlen(lline) > 47)
					strcpy(lline,&lline[48]);
#endif
				pblock.file[filex].tid = (char)gethex(lline);
				}
			if (pblock.file[filex].tid >= units)
				{
				printf("Unit id too high\n");
				return;
				}
			printf("What offset would you like?(default = 0) ");
			if (gets(lline) == NULL)
				pblock.file[filex].offset = 0;
			else
				{
#ifdef	MAC
				if (strlen(lline) > 40)
					strcpy(lline,&lline[41]);
#endif
				pblock.file[filex].offset = gethex(lline);
				}
			printf("\nDo you want to load another hex file? ");
			gets(line);
#ifdef	MAC
			if (strlen(line) > 37)
				strcpy(line,&line[38]);
#endif
			if (line[0] == 'Y' || line[0] == 'y')
				{
				printf("Enter the name of next hex file ");
#ifdef	MAC
				gets(line);
				if (strlen(line) > 31)
					strcpy(line,&line[32]);
				strcpy(finame[++filex],line);
#else
				gets(finame[++filex]);
#endif
				}
			}
		finame[++filex][0] = (char)0;
		printf("Building ROM image\n");
		if (buildimg())
			{
			error();
			return;
			}
		}
	else
		{
		printf("Correct usage - H filename\n");
		return;
		}
	if (n)
		{
		if (loadimg((char)0))
			error();
		}
	}

/* `image_clear` - clear all existing images */

void image_clear()
	{
	short ri;

	for (ri=0; ri<NMAP; ri++)
		{
		if (rommap[ri].madr == NULL)
			break;
		rommap[ri].marker = -1;
		}
	filex = 0;
	pblock.file[0].name = NULL;
	pblock.file[0].offset = 0;
	}

/* `get_name` - get a token from a string */

char *get_name()
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

#ifdef ANSI
char getbin(char tid, long loc)
#else
char getbin(tid, loc)
char tid;
long loc;
#endif
	{
	char c;

	if (loc <= rommap[tid].rom && loc >= 0)
		{
		if (rommap[tid].flag)
			{
			if (image_loc(tid, loc,0) < 0)
				{
				return(0);
				}
			if (image_read(tid, &c) < 0)
				{
				return(0);
				}
			return(c);
			}
		else
			return(*(rommap[tid].madr + loc));
		}
	printf("Address out of range\n");
	return(0);
	}

/* `getns` - read a number from a string */

#ifdef ANSI
long getns(char *str)
#else
long getns(str)
char *str;
#endif
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

#ifdef ANSI
long gethex(char *str)
#else
long gethex(str)
char *str;
#endif
	{
	short si;
	long value;
	char negative = 0;
	
	for (si=0, value=0; str[si]; si++)
		{
		if (str[si] == '-')
			{
			negative = 1;
			continue;
			}
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
	if (negative)
		return(-value);
	return(value);
	}

/* `getnum` - read a string as a number (hex or decimal) */

#ifdef ANSI
long getnum(char *str)
#else
long getnum(str)
char *str;
#endif
	{
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

long initpb()
	{
	FILE *inf;
	long value, c, offset;
	short ccnt, icnt;
	char tid, flag;
		
	initglobal();
	if ((inf = fopen("loadice.ini", "r")) == NULL)
		return(OKAY);

	printf("Initializing defaults from file: loadice.ini\n");
	while (fgets(line,100,inf) != NULL)
		{
		if (strlen(line) < 2)
			continue;
		if (line[0] == '*')
			continue;
		if (!strncmp(line,"ROM",3) ||
		 !strncmp(line,"Rom",3) ||
		 !strncmp(line,"rom",3))
			{
			flag = 0;
			if(line[5] == ':')
				{
				flag = 1;
				tid = (char)gethex(&line[4]);
				value = getns(&line[6]);
				}
			else
				value = getns(&line[4]);
			value = romvalue(value);
			if (!value)
				return(BARG);
			if (flag)
				rommap[tid].rom = value;
			else
				{
				for (tid = 0; tid < NMAP; tid++)
					rommap[tid].rom = value;
				}
			continue;
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
		if (!strncmp(line,"PARALLEL",8) ||
		 !strncmp(line,"Parallel",8) ||
		 !strncmp(line,"parallel",8))
			{
			parallel = 1;
			if (!strncmp(&line[9],"LPT2",4) || !strncmp(&line[9],"lpt2",4))
				{
				parallel = 2;
				}
			if (!strncmp(&line[9],"LPT3",4) || !strncmp(&line[9],"lpt3",4))
				{
				parallel = 3;
				}
			continue;
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
		if (!strncmp(line,"FILE",4) ||
		 !strncmp(line,"File",4) ||
		 !strncmp(line,"file",4))
			{
			if (line[4] == '=')
				{
				for (icnt = 0,ccnt = 5; line[ccnt]!= ' ' && line[ccnt]!=0;)
					{
					finame[filex][icnt++] = line[ccnt++];
					}
				}
			else 
				{
				pblock.bad = line;
				return(BARG);
				}
			pblock.file[filex].code = HEXF;
			pblock.file[filex].name = finame[filex];
			c = gethex(&line[++ccnt]);
			for (; line[ccnt]!= ':' && line[ccnt]!=0; ccnt++);
			tid = (char)gethex(&line[ccnt-1]);
			offset = gethex(&line[ccnt+1]);
			pblock.file[filex].tid = tid;
			pblock.file[filex].offset = offset - c;
			printf("filename = %s  ",pblock.file[filex].name);
			if (pblock.file[filex].offset < (long)0)
				printf("offset = - %lx\n",(0 - pblock.file[filex].offset));
			else
				printf("offset = %lx\n",pblock.file[filex].offset);
			filex +=1;
			if (filex == NFILE)
				{
				printf("Too many files %d\n",filex);
				pblock.bad = line;
				return(BARG);
				}
			pblock.file[filex].name = (char *)0;
			continue;
			}
		if (!strncmp(line,"IMAGE",5) ||
		 !strncmp(line,"Image",5) ||
		 !strncmp(line,"image",5))
			{
			if (line[5] == '=')
				{
				for (icnt = 0,ccnt = 6; line[ccnt]!= ' ' && line[ccnt]!=0;)
					{
					finame[filex][icnt++] = line[ccnt++];
					}
				}
			else
				{
				pblock.bad = line;
				return(BARG);
				}
			pblock.file[filex].code = IMAG;
			pblock.file[filex].name = finame[filex];
			pblock.file[filex].skip = gethex(&line[++ccnt]);
			for (; line[ccnt]!= ':' && line[ccnt]!=0; ccnt++);
			pblock.file[filex].tid = (char)gethex(&line[ccnt-1]);
			pblock.file[filex].offset = gethex(&line[ccnt+1]);
			printf("filename = %s  ",pblock.file[filex].name);
			printf("skip = %lx  ",pblock.file[filex].skip);
			if (pblock.file[filex].offset < (long)0)
				printf("offset = - %lx\n",(0 - pblock.file[filex].offset));
			else
				printf("offset = %lx\n",pblock.file[filex].offset);
			filex +=1;
			pblock.file[filex].name = (char *)0;
			continue;
			}
		if (!strncmp(line,"FILL",4) ||
		 !strncmp(line,"Fill",4) ||
		 !strncmp(line,"fill",4))
			{
			value = gethex(&line[5]);
			pblock.fill = (char)value;
			pblock.flags |= FILC;
			continue;
			}
		if (!strncmp(line,"FFILL",5) ||
		 !strncmp(line,"Ffill",5) ||
		 !strncmp(line,"ffill",5))
			{
			value = gethex(&line[6]);
			pblock.fill = (char)value;
			pblock.flags |= FILC | FFIL;
			printf("Force fill with %x \n",pblock.fill);
			continue;
			}
		if (!strncmp(line,"BAUD",4) ||
		 !strncmp(line,"Baud",4) ||
		 !strncmp(line,"baud",4))
			{
			value = getns(&line[5]);
			if (value != 1200 && value != 2400 && value != 4800
			 && value != 9600 && value != 19200 && value != 57600)
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
		if (!strncmp(line,"NOVERIFY",8) ||
		 !strncmp(line,"Noverify",8) ||
		 !strncmp(line,"noverify",8))
			{
			verify = 0;
			continue;
			}
		if (!strncmp(line,"HIGH",4) ||
		 !strncmp(line,"High",4) ||
		 !strncmp(line,"high",4))
			{
			fastresp = 1;
			continue;
			}
		if (!strncmp(line,"RESET",5) ||
		 !strncmp(line,"Reset",5) ||
		 !strncmp(line,"reset",5))
			{
			value = getnum(&line[6]);
			if (value < 0 || value > 2286)
				value = 2286;
			if (!value)
				value = 500;
			value = value/9 + 1;
			pblock.rtime = (char)value;
			continue;
			}
		if (!strncmp(line,"VERIFY",6) ||
		 !strncmp(line,"Verify",6) ||
		 !strncmp(line,"verify",6))
			{
			verify = 1;
			continue;
			}
		if (!strncmp(line,"MODEL",5) ||
		 !strncmp(line,"Model",5) ||
		 !strncmp(line,"model",5))
			{
			continue;
			}
		if (!strncmp(line,"NUMBER",6) ||
		 !strncmp(line,"Number",6) ||
		 !strncmp(line,"number",6))
			{
			units = (char)gethex(&line[7]);
			continue;
			}

		pblock.bad = line;
		return(BARG);
		}
	(void)fclose(inf);
	return(OKAY);
	}

#ifdef ANSI
long romvalue(long value)
#else
long romvalue(value)
long value;
#endif
	{
	if (value == 2716 || value == 2 || value == 2048)
		{
		value = 2048;
		}
	else if (value == 2732 || value == 4 || value == 4096)
		{
		value = 4096;
		}
	else if (value == 2764 || value == 8 || value == 8192)
		{
		value = 8192;
		}
	else if (value == 27128 || value == 16 || value == 16384)
		{
		value = 16384;
		}
	else if (value == 27256 || value == 32 || value == 32768)
		{
		value = 32768;
		}
	else if (value == 27512 || value == 64 || value == 65536)
		{
		value = 65536;
		}
	else if (value == 27010 || value == 128 || value == 131072)
		{
		value = 131072;
		}
	else if (value == 27020 || value == 256 || value == 262144)
		{
		value = 262144;
		}
	else if (value == 27040 || value == 512 || value == 524288)
		{
		value = 524288;
		}
	else if (value == 27080 || value == 1024 || value == 1048576)
		{
		value = 1048576;
		}
	else
		{
		pblock.bad = line;
		value = 0;
		}
	return(value);
	}


void initglobal()
	{
	char tid;
	int i;

	portopen = 0;
	pblock.partial = 0;
	pblock.mode = MODE;
	pblock.baud = BAUD;
	pblock.portname = PORT;
	parallel = 0;
	pblock.flags = LOAD;
	pblock.bad = NULL;
	for (tid = 0; tid < NMAP; tid++)
		{
		rommap[tid].rom = MAXROM;
		rommap[tid].imagename = NULL;
		rommap[tid].bi = 0;
		}
	filex = 0;
	verify = 1;				/*default verify on*/
	fastresp = 0;			/*default slow response*/
	ignorebadadd = 0;		/*default crash on bad hex address */
	forced = 0; 			/*don't force image on disk */
#ifdef	VERIFY
	pblock.flags |= VRFY;
#endif
#ifdef	FILL
	pblock.fill = FILL;
	pblock.flags |= FILC;
#endif
	for (i=0; i<NFILE; i++)
		{
		pblock.file[i].name = NULL;
		pblock.file[i].offset = STARTING;
		pblock.file[i].skip = 0;
		pblock.file[i].code = 0;
		pblock.file[i].tid = 0;
		}
	rommap[0].madr = NULL;
	}
