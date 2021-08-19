/*	@(#)tftp.h	1.0	10/06/86	(c) 1986 NeXT	*/

#ifndef _TFTP_
#define _TFTP_

#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <rpc/types.h>
#include <nfs/nfs.h>
#include "bootp.h"

#define	ntohl(x)	(x)
#define	ntohs(x)	(x)
#define	htonl(x)	(x)
#define	htons(x)	(x)

#define	IPPORT_TFTP	69

extern	u_char clientetheraddr[];

struct	tftp_hdr {
	short	th_func;
	union {
		short	th_Error;
		short	th_Bno;
		char	th_Buf[1];
	} th_un;
};

#define	th_error	th_un.th_Error
#define	th_bno		th_un.th_Bno
#define	th_buf		th_un.th_Buf

#ifdef notdef
/* func */
#define	RRQ		1
#define	WRQ		2
#define	DATA		3
#define	ACK		4
#define	ERROR		5
#endif notdef

/* error */
#define	EUNDEF		0
#define	ENOTFOUND	1
#define	EACCESS		2
#define	ENOSPACE	3
#define	EBADOP		4
#define	EBADID		5
#define	EEXISTS		6
#define	ENOUSER		7
#define	TFTP_MAXERR	8

#define	TFTP_BSIZE	512

struct tftp_packet {
	struct ether_header tp_ether;
	struct ip tp_ip;
	struct udphdr tp_udp;
	struct tftp_hdr tp_hdr;
	char tp_buf[TFTP_BSIZE];
};

#define	TFTP_HDRSIZE \
	(sizeof (struct ether_header) + sizeof (struct ip) + \
	sizeof (struct udphdr) + sizeof (struct tftp_hdr))

/* backoffs must be masks */
#define	TFTP_MIN_BACKOFF	0x7ff		/* 2.048 sec */
#define	TFTP_MAX_BACKOFF	0xffff		/* 65.535 sec */
#define	TFTP_XRETRY		50		/* # backoff intervals */
#define	TFTP_RETRY		80		/* # retries after lockon FIXME */

struct tftp_softc {
	int time, timemask, xcount, retry, lockon, timeout;
	struct in_addr ts_clientipaddr;
	u_char ts_clientetheraddr[6];
	u_char ts_serveretheraddr[6];
	int ts_bno;
	int ts_xid;
	char *ts_loadp;
	struct bootp_packet ts_bootpx;
	struct bootp_packet ts_bootpr;
	struct tftp_packet ts_xmit;
	struct tftp_packet ts_recv;
};

/* Types for ip input/output */
#define IPIO_TFTP	0
#define IPIO_BOOTP	1

#endif	_TFTP_
