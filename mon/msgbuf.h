/*	@(#)msgbuf.h	1.0	10/06/86	(c) 1986 NeXT	*/

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	from msgbuf.h	7.1 (Berkeley) 6/4/86
 */

#ifndef _MSGBUF_
#define _MSGBUF_

#define	MSG_MAGIC	0x063061
#define	MSG_BSIZE	(4096 - 3 * sizeof (long))
struct	msgbuf {
	long	msg_magic;
	long	msg_bufx;
	long	msg_bufr;
	char	msg_bufc[MSG_BSIZE];
};

#endif
