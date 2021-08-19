/*	@(#)tftp.c	1.0	10/06/86	(c) 1986 NeXT	*/

#define	LEVEL2_ENET_BOOT	1
#define	TFTP_DEBUG		0

#if	TFTP_DEBUG
#define	dbug1(a)	if (si->si_unit & 1) printf a
#define	dbug2(a)	if (si->si_unit & 2) printf a
#define	dbug4(a)	if (si->si_unit & 4) printf a
#define	dbug8(a)	if (si->si_unit & 8) printf a
#else	TFTP_DEBUG
#define	dbug1(a)
#define	dbug2(a)
#define	dbug4(a)
#define	dbug8(a)
#endif	TFTP_DEBUG

#include <sys/types.h>
#include <mon/tftp.h>
#include <mon/monparam.h>
#include <mon/sio.h>
#include <mon/global.h>
#include <mon/nvram.h>
#include <arpa/tftp.h>

u_char etherbcastaddr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

int tftp_open(), tftp_close(), tftp_boot();
struct protocol proto_tftp = { tftp_open, tftp_close, tftp_boot };

tftp_open (mg, si)
	register struct mon_global *mg;
	register struct sio *si;
{
	register struct tftp_softc *ts;
	register u_char *e;
	register struct nvram_info *ni = &mg->mg_nvram;

	si->si_protomem = mon_alloc (sizeof (struct tftp_softc));
	ts = (struct tftp_softc*) si->si_protomem;
	bcopy (etherbcastaddr, ts->ts_serveretheraddr, 6);
	e = (u_char*) si->si_dev->d_open (si);
	if (!e)
		return (BE_INIT);
	bcopy (e, ts->ts_clientetheraddr, 6);
	return (0);
}

tftp_close (mg, si)
	register struct mon_global *mg;
	register struct sio *si;
{
	return (si->si_dev->d_close (si));
}

tftp_boot (mg, si, linep)
	register struct mon_global *mg;
	register struct sio *si;
	char **linep;
{
	register struct tftp_softc *ts = (struct tftp_softc*) si->si_protomem;
	register struct tftp_packet *tx = &ts->ts_xmit;
	register struct tftp_packet *tr = &ts->ts_recv;
	register struct bootp_packet *tb = &ts->ts_bootpr;
	char *bp, *lp = *linep;
	register i;
	register int len;
	int variation = 0, p_d = 0;
	int warned = 0, resid, entry, size, skip, remain, xfer;
	char *index();
	struct udpiphdr *ui;

	if (bootp_query(mg, si, linep) == 0)
		return (0);

	tx->tp_ip.ip_v = IPVERSION;
	tx->tp_ip.ip_hl = sizeof (struct ip) >> 2;
	tx->tp_ip.ip_ttl = MAXTTL;
	tx->tp_ip.ip_p = IPPROTO_UDP;
	tx->tp_ip.ip_src = ts->ts_clientipaddr;
	tx->tp_ip.ip_dst = tb->bp_bootp.bp_siaddr;
	printf ("Booting %s from %s\n", tb->bp_bootp.bp_file,
		tb->bp_bootp.bp_sname);
	tx->tp_udp.uh_sport = IPPORT_RESERVED +
		(mon_time() & (IPPORT_RESERVED-1));
	tx->tp_udp.uh_dport = IPPORT_TFTP;
	ts->ts_bno = 1;
	tx->tp_hdr.th_func = RRQ;

	/* Copy filename given by bootp server here */
	bp = tx->tp_hdr.th_buf;
	lp = (char *) tb->bp_bootp.bp_file;
	while (*lp != 0)
		*bp++ = *lp++;
	*bp++ = 0;
	strcpy (bp, "octet");
	bp += 6;
	tx->tp_udp.uh_ulen = sizeof (struct udphdr) +
		sizeof (tx->tp_hdr.th_func) + (bp - tx->tp_hdr.th_buf);
	tx->tp_ip.ip_len = sizeof (struct ip) + tx->tp_udp.uh_ulen;
	tx->tp_udp.uh_sum = 0;	/* FIXME: figure out proper checksum? */

	/*
 	 * timeout/retry policy:
	 * Two different strategies are used.  Before a server has been
	 * locked to we retransmit at a random time in a binary window
	 * which is doubled each transmission (up to a maximum).  This
	 * avoids flooding the net after, say, a power failure when all
	 * machines are trying to reboot simultaneously.  After a connection
	 * is established the binary window is reset and only doubled after
	 * a number of retries fail at each window value.
	 */
	ts->timemask = TFTP_MIN_BACKOFF;
	ts->timeout = 0;
	ts->lockon = 0;
	ts->time = 0;	/* force immediate transmission */

	for (ts->retry = 0, ts->xcount = 0; ts->xcount < TFTP_XRETRY;) {
		if (mon_time() - ts->time >= ts->timeout) {
retrans:
			ts->time = mon_time();
			dbug4 (("T%d ", tx->tp_hdr.th_bno));
			tftp_ipoutput (mg, si, ts, IPIO_TFTP, ts->retry);
			if (ts->lockon == 0 || ts->retry > TFTP_RETRY) {
				ts->xcount++;
				if (ts->timemask < TFTP_MAX_BACKOFF)
					ts->timemask = (ts->timemask<<1) | 1;
				ts->timeout = ts->timemask & event_get();
			} else 
				ts->retry++;
		}
		len = tftp_ipinput (mg, si, ts, IPIO_TFTP);
		if (len == 0)
			continue;
		ts->timemask = TFTP_MIN_BACKOFF;
		if (len < TFTP_HDRSIZE) {
#if	TFTP_DEBUG
			printf ("?");
			if (si->si_unit & 8) {
				printf ("et 0x%x ",
					tr->tp_ether.ether_type);
			}

			if (si->si_unit & 16) {
				for (i = 0; i < 100; i += 4) {
					if (((i/4) % 12) == 0)
						printf ("\n%08x: ", (int)tr+i);
					printf ("%08x ", *(int*)((int)tr+i));
				}
				en_txstat(si);
			}
#endif	TFTP_DEBUG
			continue;
		}
		if (tr->tp_ip.ip_p != IPPROTO_UDP ||
		    tr->tp_udp.uh_dport != tx->tp_udp.uh_sport) {
#if	TFTP_DEBUG
			if (si->si_unit & 8) {
				ppkt (tr);
				for (i = 0; i < 0x100; i += 4) {
					if (((i/4) % 12) == 0)
						printf ("\n%08x: ",
							(int)tr+i);
					printf ("%08x ", *(int*)((int)tr+i));
				}
				en_txstat(si);
			}
#endif	TFTP_DEBUG
			continue;
		}
		if (ts->lockon &&
		    (tx->tp_ip.ip_dst.s_addr != tr->tp_ip.ip_src.s_addr)) {
			continue;
		}
		if (tr->tp_hdr.th_func == ERROR) {
			printf ("\ntftp: %s\n", &tr->tp_hdr.th_error + 1);
			return (0);
		}
		if (tr->tp_hdr.th_func != DATA) {
			continue;
		}
		if (tr->tp_hdr.th_bno != ts->ts_bno) {
			tx->tp_hdr.th_bno = ts->ts_bno - 1;
			ts->time = 0;
#if	TFTP_DEBUG
			if (si->si_unit & 8) {
				ppkt (tr);
				for (i = 0; i < 0x100; i += 4) {
					if (((i/4) % 12) == 0)
						printf ("\n%08x: ", (int)tr+i);
					printf ("%08x ", *(int*)((int)tr+i));
				}
				en_txstat(si);
			}
#endif	TFTP_DEBUG
			/* drain delayed packets */
			for (i = 0; i < 1000; i++)
				tftp_ipinput (mg, si, ts, IPIO_TFTP);
			continue;
		}
#ifdef	notdef
		if (tr->tp_udp.uh_sum) {
			ui = (struct udpiphdr*) &tr->tp_ip;
			ui->ui_next = ui->ui_prev = 0;
			ui->ui_x1 = 0;
			ui->ui_len = ui->ui_ulen;
			if (cksum (ui, ui->ui_ulen + sizeof (struct ip))) {
#if	1
				printf ("C");
#endif
				/* flush input packets before retrans */
				do {
					if (remain = tftp_ipinput (mg, si, ts,
					    IPIO_TFTP))
						printf ("$");
				} while (remain);

				goto retrans;
			}
		} else
		if (warned == 0) {
			printf ("Warning: server not generating UDP checksums\n");
			warned = 1;
		}
#endif	notdef
		printf (".");
		dbug4 (("R%d", tr->tp_hdr.th_bno));
		ts->xcount = 0;
		ts->retry = 0;

		/* we have an in-sequence DATA packet */
		if (ts->ts_bno == 1) {	/* lock on to server and port */
			tx->tp_udp.uh_dport = tr->tp_udp.uh_sport;

			/* revert to original backoff once locked on */
			ts->timemask = TFTP_MIN_BACKOFF;
			ts->lockon = 1;
		}
		len = tr->tp_udp.uh_ulen - (sizeof (struct udphdr) +
			sizeof (struct tftp_hdr));
		if (ts->ts_bno == 1) {
			resid = len;
			size = loader (tr->tp_buf, &resid, &entry, &ts->ts_loadp);
			if (size == 0)
				return (0);
		} else {
			bp = tr->tp_buf;
			xfer = len;
			if (resid < 0) {
				skip = len < -resid? len : -resid;
				resid += skip;
				xfer -= skip;
				bp += skip;
			}
			if (xfer > 0) {
				bcopy (bp, ts->ts_loadp, xfer);
				ts->ts_loadp += xfer;
				size -= xfer;
			}
		}

		/* flush input packets before ACK */
		do {
			if (remain = tftp_ipinput (mg, si, ts, IPIO_TFTP))
				dbug2 (("#"));
		} while (remain);

		/* send ACK */
		ts->xcount = 0;
		ts->retry = 0;
		tx->tp_hdr.th_func = ACK;
		tx->tp_hdr.th_bno = ts->ts_bno++;
		tx->tp_udp.uh_ulen = sizeof (struct udphdr) +
			sizeof (struct tftp_hdr);
		tx->tp_ip.ip_len = sizeof (struct ip) + tx->tp_udp.uh_ulen;
		ts->time = mon_time();
		dbug2 (("A"));
		dbug4 (("%d", tx->tp_hdr.th_bno));
		tftp_ipoutput (mg, si, ts, IPIO_TFTP, 0);
		if ((len < TFTP_BSIZE || size <= 0) && ts->ts_bno != 2) {
#if	LEVEL2_ENET_BOOT
			/* call level 2 Ethernet boot */
			char boot_arg[128];
			
			if (si->si_args == SIO_SPECIFIED)
				sprintf(boot_arg, "%s(%d,%d,%d)%s",
					mg->mg_boot_dev,
					si->si_ctrl, si->si_unit, si->si_part,
					mg->mg_boot_file);
			else
				sprintf(boot_arg, "%s()%s", 
					mg->mg_boot_dev, mg->mg_boot_file);
			printf ("\n");
			entry = (* (unsigned long (*)()) entry) (boot_arg);
#endif	LEVEL2_ENET_BOOT
			return (entry);
		}
	}
	printf ("tftp: timeout\n");
	return (0);
}

bootp_query (mg, si, linep)
	register struct mon_global *mg;
	register struct sio *si;
	char **linep;
{
	register struct tftp_softc *ts = (struct tftp_softc*) si->si_protomem;
	register struct bootp_packet *tb = &ts->ts_bootpx;
	register struct bootp_packet *tr = &ts->ts_bootpr;
	register struct nvram_info *ni = &mg->mg_nvram;
	char *bp, *lp = *linep;
	int time, retry, variation = 0, len, timeout, timemask, promisc_time,
		echo, promisc;
	extern char *index();
	struct nextvend *nv_s = (struct nextvend *)&tb->bp_bootp.bp_vend;
	struct nextvend *nv_r = (struct nextvend *)&tr->bp_bootp.bp_vend;

	/* Choose a transaction id based on hardware address and clock */
	tb->bp_ip.ip_v = IPVERSION;
	tb->bp_ip.ip_hl = sizeof (struct ip) >> 2;
	tb->bp_ip.ip_id++;
	tb->bp_ip.ip_ttl = MAXTTL;
	tb->bp_ip.ip_p = IPPROTO_UDP;
	tb->bp_ip.ip_src.s_addr = 0;
	tb->bp_ip.ip_dst.s_addr = INADDR_BROADCAST;
	tb->bp_udp.uh_sport = htons(IPPORT_BOOTPC);
	tb->bp_udp.uh_dport = htons(IPPORT_BOOTPS);
	tb->bp_udp.uh_sum = 0;
	bzero(&tb->bp_bootp, sizeof tb->bp_bootp);
	tb->bp_bootp.bp_op = BOOTREQUEST;
	tb->bp_bootp.bp_htype = ARPHRD_ETHER;
	tb->bp_bootp.bp_hlen = 6;
	tb->bp_bootp.bp_ciaddr.s_addr = 0;
	printf("Requesting BOOTP information");
	if (bp = index (lp, ':')) {
		*bp = 0;
		strncpy (tb->bp_bootp.bp_sname, lp, 64);
		printf ("from %s", lp);
		lp = bp + 1;
	}
	bcopy((caddr_t)ts->ts_clientetheraddr, tb->bp_bootp.bp_chaddr, 6);
	bp = (char *) tb->bp_bootp.bp_file;
	while (*lp != 0 && *lp != '-' && *lp != ' ')
		*bp++ = *lp++;
	*bp++ = 0;
	*linep = lp;
#if	LEVEL2_ENET_BOOT
	bp = (char *) tb->bp_bootp.bp_file;
	strcpy (bp, "boot");
#endif	LEVEL2_ENET_BOOT

	/* setup for sexy net init */
	bcopy(VM_NEXT, &nv_s->nv_magic, 4);
	nv_s->nv_version = 1;
	nv_s->nv_opcode = BPOP_OK;
	tb->bp_udp.uh_ulen = htons(sizeof tb->bp_udp + sizeof tb->bp_bootp);
	tb->bp_ip.ip_len = sizeof (struct ip) + tb->bp_udp.uh_ulen;

	/*
 	 * timeout/retry policy:
	 * Before a server has responded we retransmit at a random time
	 * in a binary window which is doubled each transmission.  This
	 * avoids flooding the net after, say, a power failure when all
	 * machines are trying to reboot simultaneously.
	 */
	promisc = 1;
resend:
	timemask = BOOTP_MIN_BACKOFF;
	timeout = 0;
	time = 0;	/* force immediate transmission */
	promisc_time = mon_time();
	tb->bp_bootp.bp_xid = ts->ts_clientetheraddr[5] ^ mon_time();
	for (retry = 0; retry < BOOTP_RETRY;) {
	if (mon_time() - time >= timeout) {
		time = mon_time();
		tftp_ipoutput (mg, si, ts, IPIO_BOOTP, retry);
		if (promisc)
			printf (".");
		if (timemask < BOOTP_MAX_BACKOFF)
			timemask = (timemask << 1) | 1;
		timeout = timemask & event_get();
		retry++;
	}
	len = tftp_ipinput (mg, si, ts, IPIO_BOOTP);
	if (len == 0 || len < BOOTP_PKTSIZE)
		continue;
	if (tr->bp_ip.ip_p != IPPROTO_UDP ||
	    tr->bp_udp.uh_dport != tb->bp_udp.uh_sport)
		continue;
	if (tr->bp_udp.uh_dport != htons(IPPORT_BOOTPC))
	  continue;
	if (tr->bp_bootp.bp_xid != tb->bp_bootp.bp_xid || 
	    tr->bp_bootp.bp_op != BOOTREPLY ||
	    bcmp(tr->bp_bootp.bp_chaddr, ts->ts_clientetheraddr,
		 sizeof ts->ts_clientetheraddr) != 0)
	  continue;
	
	/* it's for us! */
	if (nv_r->nv_opcode == BPOP_OK) {
		alert_done();
		printf(" [OK]\n");
		bcopy (tr->bp_ether.ether_shost,
		    ts->ts_serveretheraddr, 6);
		ts->ts_clientipaddr = tr->bp_bootp.bp_yiaddr;
#if	LEVEL2_ENET_BOOT
#else	LEVEL2_ENET_BOOT
		/* pass along the filename we're booting from */
		mg->mg_boot_file = (char*) tr->bp_bootp.bp_file;
#endif	LEVEL2_ENET_BOOT
		return(1);
	}
	
	/* ignore promiscuous replies for the first 10 seconds */
	if (promisc && mon_time() - promisc_time < 10 * 1000) {
		continue;
	}
	promisc = 0;
	
	/* enforce max length of text string */
	if (strlen (nv_r->nv_text) > NVMAXTEXT)
		nv_r->nv_null = '\0';
	
	/* display the message */
	alert_popup (60, 8, 0, nv_r->nv_text);
	echo = 1;
	
	switch (nv_r->nv_opcode) {
	case BPOP_QUERY_NE:
		echo = 0;
		/* fall through ... */
		
	case BPOP_QUERY:
		gets (nv_s->nv_text, NVMAXTEXT, echo);
		if (!echo)
			printf ("\n");

		/* return to sender */
		tb->bp_ip.ip_dst = *(struct in_addr *)&tr->bp_bootp.bp_siaddr;
		nv_s->nv_opcode = nv_r->nv_opcode;
		nv_s->nv_xid = nv_r->nv_xid;
		break;

	case BPOP_ERROR:
		/* Reset the destination, opcode and xid */
		tb->bp_ip.ip_dst.s_addr = INADDR_BROADCAST;
		nv_s->nv_opcode = BPOP_OK;
		nv_s->nv_xid = 0;
		break;
	}
	goto resend;
	}
	alert_done();
	printf(" [timeout]\n");
	return(0);
}

arp_input (mg, si, ts, packet)
     register struct mon_global *mg;
     register struct sio *si;
     register struct tftp_softc *ts;
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
	    return (si->si_dev->d_write) (si, (caddr_t)eh, len, 0);
	}
    return(-1);
}

tftp_ipoutput (mg, si, ts, type, timeout)
	register struct mon_global *mg;
	register struct sio *si;
	register struct tftp_softc *ts;
	int type;
{
	register struct tftp_packet *tp = &ts->ts_xmit;
	register struct bootp_packet *bp = &ts->ts_bootpx;
	register struct ether_header *eh;
	register struct ip *ip;
	register short len;
	caddr_t pp;

	switch (type) {
	  case IPIO_TFTP:
	    eh = &tp->tp_ether;
	    ip = &tp->tp_ip;
	    len = tp->tp_ip.ip_len + sizeof (struct ether_header);
	    pp = (caddr_t) tp;
	    break;
	  case IPIO_BOOTP:
	    eh = &bp->bp_ether;
	    ip = &bp->bp_ip;
	    len = bp->bp_ip.ip_len + sizeof (struct ether_header);
	    pp = (caddr_t) bp;
	    break;
	  default:
	    return(-1);
	}

	eh->ether_type = ETHERTYPE_IP;
	bcopy (ts->ts_clientetheraddr, eh->ether_shost, 6);
	bcopy (ts->ts_serveretheraddr, eh->ether_dhost, 6);
	ip->ip_sum = 0;
	ip->ip_sum = ~cksum ((caddr_t)ip, sizeof (struct ip));
	if (len < ETHERMIN + sizeof (struct ether_header))
		len = ETHERMIN + sizeof (struct ether_header);
	return (si->si_dev->d_write) (si, pp, len, timeout);
}

tftp_ipinput (mg, si, ts, type)
     register struct mon_global *mg;
     register struct sio *si;
     register struct tftp_softc *ts;
     int type;
{
    register short len;
    register struct tftp_packet *tp = &ts->ts_recv;
    register struct bootp_packet *bp = &ts->ts_bootpr;
    register struct ether_header *eh;
    register struct ip *ip;
    struct ether_arp *ap;

    switch (type) {
      case IPIO_TFTP:
	eh = &tp->tp_ether;
	ip = &tp->tp_ip;
	len = (si->si_dev->d_read) (si, (caddr_t)tp, ts, sizeof (*tp));
	break;
      case IPIO_BOOTP:
	eh = &bp->bp_ether;
	ip = &bp->bp_ip;
	len = (si->si_dev->d_read) (si, (caddr_t)bp, ts, sizeof (*bp));
	break;

      default:
	return(0);
    }

    if (len == 0)
	return (0);
    if (ts->ts_clientipaddr.s_addr) { /* We know our address */
	if (bcmp (eh->ether_dhost, etherbcastaddr, 6) == 0)
		return (0);
	/* First check for incoming ARP request's */
	if (eh->ether_type == ETHERTYPE_ARP) {
	    arp_input(mg, si, ts, eh);
	    return(0);	/* We don't want to see this higher up */
	}
	/* Not ARP, is it IP for us? */
	if (len >= sizeof (struct ether_header) + sizeof (struct ip) &&
	    eh->ether_type == ETHERTYPE_IP &&
	    ip->ip_dst.s_addr == ts->ts_clientipaddr.s_addr) {
	  return (len);
	}
	return (1);
    }

    /* We don't know our IP address yet, so accept packets aimed at us */
    if (len >= sizeof (struct ether_header) + sizeof (struct ip) &&
	eh->ether_type == ETHERTYPE_IP &&
	! bcmp(eh->ether_dhost, ts->ts_clientetheraddr, 6))
	    return(len);
    return (0);
}
