/*	@(#)help.c	1.0	10/27/86	(c) 1986 NeXT	*/

#if	NO_SPACE
#else	NO_SPACE
char help_msg[] = {
"NeXT ROM monitor commands:\n"
"\tp  inspect/modify configuration parameters\n"
"\ta [n]  open address register\n"
"\tm  print memory configuration\n"
"\td [n]  open data register\n"
"\tr [regname]  open processor register\n"
"\ts [systemreg]  open system register\n"
"\te [lwb] [alist] [format]  examine memory location addr\n"
"\tec  print recorded system error codes\n"
#if	ODBOOT
"\tej [drive #]  eject optical disk (default = 0)\n"
"\teo  (same as above)\n"
#endif	ODBOOT
#if	FDBOOT
"\tef [drive #]  eject floppy disk (default = 0)\n"
#endif	FDBOOT
"\tc  continue execution at last pc location\n"
#if 0
"\tg [addr]  go at specified address (default = pc)\n"
#endif
"\tb [device[(ctrl,unit,part)] [filename] [flags]]  boot from device\n"
"\tS [fcode]  open function code (address space)\n"
#if 0
"\tv [lwb]  start stop  view memory from start to stop\n"
"\tf [lwb]  start stop patt [inc] [mod]  fill memory with pattern\n"
#endif
#if	SAVE_SPACE
"\tt [rw] ea level [fcode]  MMU translate virtual address\n"
#endif	SAVE_SPACE
"\tR [radix]  set input radix\n"
#if	ENETDIAG
"\tE  enter Ethernet diagnostic submenu\n"
#endif	ENETDIAG
"Notes:\n"
"\t[lwb] select long/word/byte length (default = long).\n"
"\t[alist] is starting address or list of addresses to cyclically examine.\n"
"\tExamine command, with no arguments, uses last [alist].\n"
"Copyright (c) 1988-1990 NeXT Inc.\n\n"
};

help()
{
	printf ("%s", help_msg);
}
#endif	NO_SPACE
