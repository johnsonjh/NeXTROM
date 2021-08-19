/*	@(#)kmreg.h	1.0	10/12/87	(c) 1987 NeXT	*/

/* 
 * HISTORY
 * 12-Oct-87  John Seamons (jks) at NeXT
 *	Created.
 */

#ifndef KMREG_MON_H
#define KMREG_MON_H

#import "sys/types.h"
#import "sys/ioctl.h"
#import "sys/tty.h"
#import "nextdev/kmreg.h"
#import "mon/nvram.h"

struct km_mon {
	union	kybd_event kybd_event;
	union	kybd_event autorepeat_event;
	short	x, y;
	short	nc_tm, nc_lm, nc_w, nc_h;
	int	store;
	int	fg, bg;
	short	ansi;
	short	*cp;
#define	KM_NP		3
	short	p[KM_NP];
	volatile short flags;
#define	KMF_INIT	0x0001
#define	KMF_AUTOREPEAT	0x0002
#define	KMF_STOP	0x0004
#define	KMF_SEE_MSGS	0x0008
#define	KMF_PERMANENT	0x0010
#define	KMF_ALERT	0x0020
#define	KMF_HW_INIT	0x0040
#define	KMF_MON_INIT	0x0080

#if defined(MONITOR)
#define	KMF_CURSOR	0x0100
#define	KMF_NMI		0x0200
#else	/* KERNEL */
/* Assorted stuff for kernel console support */
#define	KMF_ALERT_KEY	0x0100
#define	KMF_CURSOR	0x0200
#define	KMF_ANIM_RUN	0x0400
#endif	/* KERNEL */
};

#endif KMREG_MON_H




