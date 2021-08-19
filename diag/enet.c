/*	@(#)edp.c	1.0	10/06/86	(c) 1986 NeXT	*/

#include <sys/types.h>
#include <diag/edp.h>
#include <nextdev/monreg.h>

#define	CLIENT_IPADDR	((129<<24) + (18<<16) + (3<<8) + 213)	/* hw-proto9 */
#define	SERVER_IPADDR	((129<<24) + (18<<16) + (3<<8) + 67)	/* hw-proto2 */
#define	SOFTC	(struct edp_softc*) 0x04010000

enet()
{
	int dbug = 0;
	register struct edp_softc *ts = SOFTC;
	register struct edp_packet *tr = &ts->ts_recv;
	register struct edp_packet *tx = &ts->ts_xmit;
	register int len, i;
	int filter = 0, delay = 0, size = 0;
	register u_char *ea;
	int init = 0;
	extern char edata, end;
	char c, *d;
	static char digits[] = "0123456789abcdef";

	bzero (SOFTC, sizeof (struct edp_softc));
	printf ("enet -- Ethernet diagnostic\n");
	ea = (u_char*) en_open(&ts->ts_si);
	ts->ts_clientipaddr.s_addr = CLIENT_IPADDR;
	bcopy (ea, ts->ts_clientetheraddr, 6);
	printf ("our ea %x:%x:%x:%x:%x:%x\n", ea[0], ea[1], ea[2], ea[3], ea[4], ea[5]);
cmd:
	printf ("r=receive packets\n");
	printf ("p=receive & print hex, P=receive & print formatted\n");
	printf ("t=transmit continuously after lockon, T=without lockon \n");
	printf ("e=echo test, E=echo & print test\n");
	printf ("f=filter enet adrs\n");
	printf ("d=delay\n");
	printf ("s=size\n");
	printf ("b=set dbug flags\n");
	printf ("q=quit\n");
	printf (">");
	c = mon_getc() & 0x7f;
	printf ("%c\n", c);
	init = 0;

#if	DBUG_DMA
	if (c == 'Z') {
		int i;

		printf ("ZZZ...\n");
		for (i=0x4000000; i<0x4004000; i+=2)
			*(short*)i = i & 0xff;
		i = 0;
		mon_send (MON_SNDOUT_CTRL(0x1), 0);	/* enable sound */
		mon_send (MON_GP_OUT, 0xff000000);	/* enable speaker */
		*(char*)0x2000040 = 0x30;		/* reset DMA */
		*(int*)0x2004040 = 0x4000000;		/* set next */
		*(int*)0x2004044 = 0x4004000;		/* set limit */
		*(int*)0x2004048 = 0x4000000;		/* set start */
		*(int*)0x200404c = 0x4004000;		/* set stop */
		*(char*)0x2000040 = 0x3;		/* start DMA */
		*(int*)0x200e000 = 0x80000200;		/* enable DMA */
		while (1) {

		if (*(char*)0x2000040 & 8) {
		*(int*)0x2004048 = 0x4000000;		/* set start */
		*(int*)0x200404c = 0x4004000;		/* set stop */
		*(char*)0x2000040 = 0xa;		/* start DMA */
		}
#if 1
		if ((i++ % 5000) == 0) {
		if (size)
			tx->tp_hdr.th_func = size;
		else {
			tx->tp_hdr.th_func++;
			tx->tp_hdr.th_func %= 512;
		}
		for (i=0; i<tx->tp_hdr.th_func; i++)
			tx->tp_buf[i] = i;
		tx->tp_ip.ip_len = sizeof (struct ip) +
			sizeof (struct ether_header) + tx->tp_hdr.th_func;
		edp_ipoutput (ts);
		}
#endif
		}
	}

	if (c == 'V') {
		register volatile int i;

		while (1)
		for (i=0x4100000; i < 0x4300000; i += 4)
			*(volatile int*)i = i;
	}
	if (c == 'v') {
		int v, lv, rn, sn;

		v = *(int*)0x2004180;
		lv = v;
		while (1) {
			v = *(int*)0x2004180;
			*(int*)0x5000000 = v;
			*(int*)0x5000004 = lv;
			/* if (!(((v & ~0xff) == 0xb0000) &&
			    ((lv & ~0xff) == 0xb0000))) { */
			if (v > lv && (v - lv) > 16) {
				rn = *(int*)0x2004150;
				sn = *(int*)0x2004140;
				*(volatile char*)0x2012004 = 2;
				*(volatile char*)0x2012004 = 0;
				printf ("v 0x%x lv 0x%x rn 0x%x sn 0x%x\n",
					v, lv, rn, sn);
				v = *(volatile int*)0x2004180;
			}
			lv = v;
		}
	}
#endif	DBUG_DMA
	if (c == 'q') {
		printf ("quit\n");
		return;
	}
	if (c == 'f') {
		printf ("filter enet adrs (hex)> ");
		filter = 0;
		for (i = 0; i < 8; i++) {
			c = mon_getc() & 0x7f;
			mon_putc (c);
			if (c == '\n')
				break;
			d = (char*) index (digits, c);
			filter = filter*16 + d-digits;
		}
		printf ("\nfilter 0x%x\n", filter);
		goto cmd;
	}
	if (c == 'd') {
		printf ("delay> ");
		delay = 0;
		for (i = 0; i < 8; i++) {
			c = mon_getc() & 0x7f;
			mon_putc (c);
			if (c == '\n')
				break;
			d = (char*) index (digits, c);
			delay = delay*10 + d-digits;
		}
		printf ("\ndelay 0x%x\n", delay);
		goto cmd;
	}
	if (c == 's') {
		printf ("size> ");
		size = 0;
		for (i = 0; i < 8; i++) {
			c = mon_getc() & 0x7f;
			mon_putc (c);
			if (c == '\n')
				break;
			d = (char*) index (digits, c);
			size = size*10 + d-digits;
		}
		printf ("\nsize 0x%x\n", size);
		goto cmd;
	}
	if (c == 'b') {
		printf ("dbug (hex)> ");
		dbug = 0;
		for (i = 0; i < 8; i++) {
			c = mon_getc() & 0x7f;
			mon_putc (c);
			if (c == '\n')
				break;
			d = (char*) index (digits, c);
			dbug = dbug*16 + d-digits;
		}
		printf ("\ndbug 0x%x\n", dbug);
		ts->ts_si.si_unit = dbug;
		goto cmd;
	}

	if (c == 't') {
		printf ("transmit after lockon\n");
	while (1) {
		if (init == 0) {
again:
		len = edp_ipinput (ts);
		if (mon_try_getc())
			goto cmd;
		if (len == 0)
			goto again;
		if (tr->tp_ip.ip_p != IPPROTO_RAW)
			goto again;
		bcopy (tr->tp_ether.ether_shost, ts->ts_serveretheraddr, 6);
		ts->ts_clientipaddr = tr->tp_ip.ip_dst;
		printf ("we are %s\n", inet_ntoa (ts->ts_clientipaddr));
		printf ("locked to %s\n", inet_ntoa (tr->tp_ip.ip_src));
		bcopy (tr, tx, tr->tp_ip.ip_len +
			sizeof (struct ether_header));
		tx->tp_ip.ip_src = tr->tp_ip.ip_dst;
		tx->tp_ip.ip_dst = tr->tp_ip.ip_src;
		tx->tp_hdr.th_seq = 0;
		tx->tp_hdr.th_func = 0;
		init = 1;
		}
		if (size)
			tx->tp_hdr.th_func = size;
		else {
			tx->tp_hdr.th_func++;
			tx->tp_hdr.th_func %= 1400;
		}
		for (i=0; i<tx->tp_hdr.th_func; i++)
			tx->tp_buf[i] = i;
		if (dbug & 1)
			printf ("%d:%d ", tx->tp_hdr.th_seq,
				tx->tp_hdr.th_func);
		tx->tp_ip.ip_len = sizeof (struct ip) +
			5 /*sizeof (tx->tp_hdr.th_seq) + tx->tp_hdr.th_func*/;
		edp_ipoutput (ts);
		printf ("%d ", tx->tp_hdr.th_seq++);
		for (i=0; i<delay; i++)
			;
		if (mon_try_getc())
			goto cmd;
	}
	}

	if (c == 'T') {
		printf ("transmit contin\n");
	ts->ts_clientipaddr.s_addr = CLIENT_IPADDR;
	while (1) {
		if (size)
			tx->tp_hdr.th_func = size;
		else {
			tx->tp_hdr.th_func++;
			tx->tp_hdr.th_func %= 1400;
		}
		for (i=0; i<tx->tp_hdr.th_func; i++)
			tx->tp_buf[i] = i;
		if (dbug & 1)
			printf ("%d:%d ", tx->tp_hdr.th_seq,
				tx->tp_hdr.th_func);
		else
			printf (".");
		tx->tp_ip.ip_len = sizeof (struct ip) +
			sizeof (struct ether_header) + tx->tp_hdr.th_func;
		edp_ipoutput (ts);
		printf ("%d ", tx->tp_hdr.th_seq++);
		for (i=0; i<delay; i++)
			;
		if (mon_try_getc())
			goto cmd;
	}
	}

	if (c == 'p') {
		printf ("print hex dump\n");
	while (1) {
		if (mon_try_getc())
			goto cmd;
		if ((len = edp_ipinput (ts)) == 0)
			continue;
		if (filter && (*(u_short*)((int)tr+4) != filter) &&
		    (*(u_short*)((int)tr+10) != filter)) {
			continue;
		}
		printf ("len %d", len);
		for (i = 0; i < len; i += 4) {
			if (((i/4) % 8) == 0)
				printf ("\n%04x: ", i);
				printf ("%08x ", *(int*)((int)tr + i));
		}
		printf ("\n");
	}
	}

	if (c == 'r') {
		printf ("receive packets\n");
	while (1) {
		if (mon_try_getc())
			goto cmd;
		if ((len = edp_ipinput (ts)) == 0)
			continue;
		if (filter && (*(u_short*)((int)tr+4) != filter) &&
		    (*(u_short*)((int)tr+10) != filter)) {
			continue;
		}
		printf ("%d ", len);
		/* printf ("\n"); */
	}
	}

	if (c == 'P') {
		printf ("print formatted\n");
	while (1) {
		if (mon_try_getc())
			goto cmd;
		if ((len = edp_ipinput (ts)) == 0)
			continue;
		if (filter && (*(u_short*)((int)tr+4) != filter) &&
		    (*(u_short*)((int)tr+10) != filter)) {
			continue;
		}
		ppkt (tr);
		if (mon_try_getc())
			goto cmd;
	}
	}

	if (c == 'e') {
	printf ("echo test\n");
	while (1) {
	len = edp_ipinput (ts);
	if (mon_try_getc())
		goto cmd;
	if (len == 0)
		continue;
	if (tr->tp_ip.ip_p != IPPROTO_RAW)
		continue;
	if (filter && (*(u_short*)((int)tr+4) != filter) &&
	    (*(u_short*)((int)tr+10) != filter))
		continue;
	if (init == 0) {
		printf ("echo test.. (delay=%d)\n", delay);
		bcopy (tr->tp_ether.ether_shost, ts->ts_serveretheraddr, 6);
		ts->ts_clientipaddr = tr->tp_ip.ip_dst;
		printf ("we are %s\n", inet_ntoa (ts->ts_clientipaddr));
		printf ("locked to %s\n", inet_ntoa (tr->tp_ip.ip_src));
		init = 1;
	}
	if (tr->tp_ip.ip_dst.s_addr != ts->ts_clientipaddr.s_addr)
		continue;

	switch (tr->tp_hdr.th_func) {

	case EDP_ECHO:
		/* ppkt (tr); */
		bcopy (tr, tx, tr->tp_ip.ip_len +
			sizeof (struct ether_header));
		tx->tp_ip.ip_src = tr->tp_ip.ip_dst;
		tx->tp_ip.ip_dst = tr->tp_ip.ip_src;
		if (dbug & 1)
			printf ("%d ", tr->tp_hdr.th_seq);
		else
			printf (".");
		for (i = 0; i < delay; i++)
			;
		while (edp_ipoutput (ts)) {
			printf ("collision\n");
		}
		break;

	default:
		printf ("? func %d seq %d len %d\n",
			tr->tp_hdr.th_func, tr->tp_hdr.th_seq,
			tr->tp_ip.ip_len);
		break;
	}
	}
	}

	if (c == 'E') {
	printf ("echo & print test\n");
	while (1) {
	len = edp_ipinput (ts);
	if (mon_try_getc())
		goto cmd;
	if (len == 0)
		continue;
	if (filter && (*(u_short*)((int)tr+4) != filter) &&
	    (*(u_short*)((int)tr+10) != filter))
		continue;
	printf ("len %d", len);
	for (i = 0; i < len+8; i += 4) {
		if (((i/4) % 8) == 0)
			printf ("\n%04x: ", i);
			printf ("%08x ", *(int*)((int)tr + i));
	}
	printf ("\n");
	if (tr->tp_ip.ip_p != IPPROTO_RAW) {
		printf ("not RAW ");
		continue;
	}
	if (init == 0) {
		printf ("echo test.. (delay=%d)\n", delay);
		bcopy (tr->tp_ether.ether_shost, ts->ts_serveretheraddr, 6);
		ts->ts_clientipaddr = tr->tp_ip.ip_dst;
		printf ("we are %s\n", inet_ntoa (ts->ts_clientipaddr));
		printf ("locked to %s\n", inet_ntoa (tr->tp_ip.ip_src));
		init = 1;
	}
	if (tr->tp_ip.ip_dst.s_addr != ts->ts_clientipaddr.s_addr) {
		printf ("wrong ipaddr ");
		continue;
	}

	switch (tr->tp_hdr.th_func) {

	case EDP_ECHO:
		/* ppkt (tr); */
		bcopy (tr, tx, tr->tp_ip.ip_len +
			sizeof (struct ether_header));
		tx->tp_ip.ip_src = tr->tp_ip.ip_dst;
		tx->tp_ip.ip_dst = tr->tp_ip.ip_src;
		for (i = 0; i < delay; i++)
			;
		while (edp_ipoutput (ts)) {
			printf ("collision\n");
		}
		break;

	default:
		printf ("? func %d seq %d len %d\n",
			tr->tp_hdr.th_func, tr->tp_hdr.th_seq,
			tr->tp_ip.ip_len);
		break;
	}
	}
	}
}

edp_ipoutput (ts)
	register struct edp_softc *ts;
{
	register struct edp_packet *tp = &ts->ts_xmit;
	register struct ether_header *eh = &tp->tp_ether;
	register struct ip *ip = &tp->tp_ip;
	register short len;
	caddr_t pp;

	len = tp->tp_ip.ip_len + sizeof (struct ether_header);
	pp = (caddr_t) tp;
	eh->ether_type = ETHERTYPE_IP;
	bcopy (ts->ts_clientetheraddr, eh->ether_shost, 6);
	bcopy (ts->ts_serveretheraddr, eh->ether_dhost, 6);
	ip->ip_sum = 0;
	ip->ip_sum = ~cksum ((caddr_t)ip, sizeof (struct ip));
	if (len < ETHERMIN + sizeof (struct ether_header))
		len = ETHERMIN + sizeof (struct ether_header);
	/* ppkt (tp); */
	return (en_write (&ts->ts_si, pp, len));
}

edp_ipinput (ts)
	register struct edp_softc *ts;
{
	register short len;
	register struct edp_packet *tp = &ts->ts_recv;
	register struct ether_header *eh = &tp->tp_ether;
	register struct ip *ip = &tp->tp_ip;

	len = en_poll (&ts->ts_si, tp, ts, sizeof (*tp));

	/* First check for incoming ARP request's */
	if (len && eh->ether_type == ETHERTYPE_ARP) {
	    edp_arp_input(ts, eh);
	    return(0);	/* We don't want to see this higher up */
	}
	return (len);
}

edp_arp_input (ts, packet)
     register struct edp_softc *ts;
     caddr_t packet;
{
    register struct ether_header *eh = (struct ether_header *) packet;
    register struct ether_arp *arp = (struct ether_arp *) (packet+sizeof(*eh));
    int len;

    if (arp->arp_op == ARPOP_REQUEST &&	arp->arp_hrd == ARPHRD_ETHER &&
	arp->arp_pro == ETHERTYPE_IP &&
	((struct in_addr*)arp->arp_tpa)->s_addr ==
	ts->ts_clientipaddr.s_addr) {
	    /* It's our arp request */
	    bcopy (arp->arp_sha, arp->arp_tha, 6);
	    bcopy (arp->arp_spa, arp->arp_tpa, 4);
	    bcopy (ts->ts_clientetheraddr, arp->arp_sha, 6);
	    *(struct in_addr*) arp->arp_spa = ts->ts_clientipaddr;
	    arp->arp_op = ARPOP_REPLY;
	    bcopy (arp->arp_tha, eh->ether_dhost, 6);
	    bcopy (ts->ts_clientetheraddr, eh->ether_shost, 6);
	    len = sizeof (struct ether_header) + sizeof (struct ether_arp);
	    if (len < ETHERMIN + sizeof (struct ether_header))
	      len = ETHERMIN + sizeof (struct ether_header);
	    return (en_write (&ts->ts_si, (caddr_t)eh, len));
	}
    return(-1);
}

