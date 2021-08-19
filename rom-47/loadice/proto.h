/*	Proto.h - Edit 0.1			*/

/*	LoadICE Version 1.4alpha -  May 1990		*/
/*	Copyright (C) 1990 Grammar Engine, Inc.	*/

#ifdef ANSI
long  lropen(void);
void  lrclose(void);
long  lrread(void);
#ifdef MAC
void perror(char *errstr);
#endif
#ifdef	MSDOS
char  _huge *lrmalloc(long	size);
#else
char  *lrmalloc(long size);
#endif
void  spincw(short *pcount);
void  spinccw(short *pcount);
long  image_open(char id);
long  image_loc(char id, long	loc,int	origin);
long  image_write(char id, char	c);
long  image_wblock(char id, char	*ibuf,short	c);
long  image_read(char id, char	*cp);
long  image_bread(char id, char  *cp,int  ccnt);
void  image_delete(char id);
void  beep(void);
long  rawmode(void);
void  rawoff(void);
void  putpar(char  byte);
void  putpar2(char  byte);
void  putcom(char  byte);
long  getcom(char  *cp);
void  sleep(short	time);
void  error(void);
/*******************/
void  main(int	argc,char  * *argv);
void  byebye(void);
void  sizer(void);
long  parsec(int  argc,char  * *argv);
long  parsesw(char  * *argv,short  *ndxh);
long  loadimg(char  compare);
void  setptr(char  tid,long  addr);
void  wrblock(char  tid,short  ccnt,char  *dptr);
void  rdblock(char  tid,short  ccnt,char  *ibuf);
void  pimode(char  tid,char  mode);
char  pisize(char  tid);
char  pireset(char  time);
char  pitest(char  tid,char  pc);
void  piversion(void);
long  picon(void);
void  picmd(char  response,char  uid,char  command,char  count,char  data,char  aux1,char  aux2);
void  pirsp(char  code);
void  checksum(void);
void  command(void);
void  airead(void);
void  aiwrite(void);
void  aibitch(void);
void  testpi(void);
void  resetpi(void);
void  help(void);
void  help2(void);
void  readpi(void);
void  writepi(void);
void  dump(void);
void  edit(void);
void  fill(void);
void  move(void);
void  pmove(void);
void  save(void);
void  compare(void);
void  load(short  n);
void  hexload(short  n);
void  image_clear(void);
char  *get_name(void);
char  getbin(char  tid,long  loc);
long  getns(char  *str);
long  gethex(char  *str);
long  getnum(char  *str);
long  initpb(void);
void  initglobal(void);
long  romvalue(long value);
/*******************/
long  buildimg(void);
long  binary(void);
void  mhex(void);
void  mohex(void);
void  ihex(void);
void  rhex(void);
void  thex(void);
void  ethex(void);
void  dsphex(void);
void  puthex(long	loc,short  count,char  *hex);
long  putbin(char	tid,long	loc,char  c);
long  getspace(char	ri);
long  files(void);
long  getrec(void);
char  getbyte(void);
long  getnx(char  *str,short  n);
short  checkmi(char  *data,short  length);
short  checkst(char  *data,short  length);
short  checktek(char  *buffer,short  length);
void  ckintek(void);

#else		  /* NOT ANSI */

#ifdef	MSDOS
char  _huge *lrmalloc();
#else
char  *lrmalloc();
#endif
#ifndef MAC
void  spincw();
void  spinccw();
#endif
long  image_open();
long  image_loc();
long  image_write();
long  image_wblock();
long  image_read();
long  image_bread();
void  image_delete();
void  beep();
long  rawmode();
void  rawoff();
void  putpar();
void  putpar2();
void  putcom();
long  getcom();
void  sleep();
void  error();
/*******************/
void  main();
void  byebye();
void  sizer();
long  parsec();
long  parsesw();
long  loadimg();
void  setptr();
void  wrblock();
void  rdblock();
void  pimode();
char  pisize();
char  pireset();
char  pitest();
void  piversion();
long  picon();
void  picmd();
void  pirsp();
void  checksum();
void  command();
void  airead();
void  aiwrite();
void  aibitch();
void  testpi();
void  resetpi();
void  help();
void  help2();
void  readpi();
void  writepi();
void  dump();
void  edit();
void  fill();
void  move();
void  pmove();
void  save();
void  compare();
void  load();
void  hexload();
void  image_clear();
char  *get_name();
char  getbin();
long  getns();
long  gethex();
long  getnum();
long  initpb();
void  initglobal();
long  romvalue();
/*******************/
long  buildimg();
long  binary();
void  mhex();
void  mohex();
void  ihex();
void  rhex();
void  thex();
void  ethex();
void  dsphex();
void  puthex();
long  putbin();
long  getspace();
long  files();
long  getrec();
char  getbyte();
long  getnx();
short  checkmi();
short  checkst();
short  checktek();
void  ckintek();
#endif
