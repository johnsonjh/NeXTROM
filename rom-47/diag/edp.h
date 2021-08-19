/*	@(#)edp.h	1.0	10/06/86	(c) 1986 NeXT	*/

#ifndef _EDP_
#define _EDP_

#include <mon/sio.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

extern	u_char clientetheraddr[];

struct	edp_hdr {
	int	th_seq;
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

/* func */
#define	EDP_ECHO		1

/* error */
#define	EDP_MAXERR	8

#define	EDP_BSIZE	1500

struct edp_packet {
	struct	ether_header tp_ether;
	struct	ip tp_ip;
	struct	edp_hdr tp_hdr;
	u_char	tp_buf[EDP_BSIZE];
};

#define	EDP_HDRSIZE \
	(sizeof (struct ether_header) + sizeof (struct ip) + \
	sizeof (struct edp_hdr))

struct edp_softc {
	struct	sio ts_si;
	struct	edp_packet ts_xmit, ts_recv;
	struct	in_addr ts_clientipaddr;
	u_char	ts_clientetheraddr[6];
	u_char	ts_serveretheraddr[6];
	int	ts_xid;
};
#endif	_EDP_
