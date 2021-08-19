/*	@(#)monparam.h	1.0	10/06/86	(c) 1986 NeXT	*/

#ifndef _MONPARAM_
#define _MONPARAM_

#include <next/cpu.h>
#include <nextdev/video.h>

#define	STACK_SIZE	(8192 - 2048)

#define	N_SIMM		4		/* number of SIMMs in machine */

/* SIMM types */
#define	SIMM_EMPTY	0x0
#define	SIMM_16MB	0x1
#define	SIMM_4MB	0x2
#define	SIMM_1MB	0x3
#define	SIMM_PAGE_MODE	0x4
#define	SIMM_PARITY	0x8

#define	USERENTRY	0x04000000
#define	RAM_MINLOADADDR	0x04000000
#define	RAM_MAXLOADADDR	0x04400000

#endif _MONPARAM_

