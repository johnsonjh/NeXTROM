#include <sys/types.h>
#include <sys/param.h>
#include <mon/global.h>
#include <mon/tftp.h>
#include "netinet/ip_icmp.h"
#include <arpa/tftp.h>

ppkt (bp)
	struct bootp_packet *bp;
{
	register struct mon_global *mg = restore_mg();
	struct ether_header *eh = &bp->bp_ether;
	struct ip *ip = &bp->bp_ip;
	struct udphdr *uh = &bp->bp_udp;
	struct ether_arp *ar = (struct ether_arp*) &bp->bp_ip;
	struct tftphdr *th = (struct tftphdr*) ((int)uh +
		sizeof (struct udphdr));
	u_char *e;
	struct icmp *ic;
	extern struct u_char etherbcastaddr[];

	printf ("d ");
	if (bcmp (eh->ether_dhost, etherbcastaddr, 6) == 0)
		printf ("bcast ");
	else {
		e = (u_char*) eh->ether_dhost;
		printf ("%02x%02x%02x%02x%02x%02x ",
			e[0], e[1], e[2], e[3], e[4], e[5]);
	}
	printf ("s ");
	if (bcmp (eh->ether_shost, etherbcastaddr, 6) == 0)
		printf ("bcast ");
	else {
		e = (u_char*) eh->ether_shost;
		printf ("%02x%02x%02x%02x%02x%02x ",
			e[0], e[1], e[2], e[3], e[4], e[5]);
	}

	switch (eh->ether_type) {

	case ETHERTYPE_IP:
#if	1
		printf ("IP ");
#else
		printf ("IP s %s ", inet_ntoa (ip->ip_src));
		printf ("d %s ", inet_ntoa (ip->ip_dst));
#endif
		if (ip->ip_p == IPPROTO_UDP) {
			uh = &bp->bp_udp;
			printf ("UDP sp %d dp %d ",
				uh->uh_sport, uh->uh_dport);
			if (uh->uh_sport == 69 || uh->uh_dport == 69)
				printf ("TFTP %d ", th->th_block);
		} else
		if (ip->ip_p == IPPROTO_ICMP) {
			ic = (struct icmp*) &bp->bp_udp;
			printf ("ICMP t %d c %d\n",
				ic->icmp_type, ic->icmp_code);
		} else
		if (ip->ip_p == IPPROTO_RAW) {
			printf ("RAW");
		} else
			printf ("?proto %d? ", ip->ip_p);
		break;

	case ETHERTYPE_ARP:
		printf ("ARP %s ", ar->ea_hdr.ar_op == ARPOP_REQUEST?
			"req" : "rep");
		e = (u_char*) ar->arp_sha;
		printf ("sh %02x%02x%02x%02x%02x%02x sp %s ",
			e[0], e[1], e[2], e[3], e[4], e[5],
			inet_ntoa (ar->arp_spa));
		e = (u_char*) ar->arp_tha;
		printf ("th %02x%02x%02x%02x%02x%02x tp %s ",
			e[0], e[1], e[2], e[3], e[4], e[5],
			inet_ntoa (ar->arp_tpa));

		break;

	default:
		printf ("type 0x%x ", eh->ether_type);
		break;
	}
	printf ("\n");
}
