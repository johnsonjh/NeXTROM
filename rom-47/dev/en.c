/* Standalone PROM monitor driver for the NeXT on-board ethernet controller */

#define	DBUG		0
#define	RX		1
#define	CHAIN_TX	0
#define	CHAIN_RX	1
#define	ODD		0
#define	WRAP		8
#define	TXOFF		0
#define	CRC		0
#define	PROMISC		0

#include <sys/types.h>
#include <sys/param.h>
#include <next/scb.h>
#include <mon/global.h>
#include <mon/sio.h>
#include <mon/tftp.h>
#include <mon/nvram.h>
#include <mon/monparam.h>
#include <nextdev/dma.h>
#include <dev/enreg.h>
#include <next/cpu.h>
#include <next/bmap.h>

#define	DBUG_TR		0x01
#define	DBUG_RX		0x02
#define	DBUG_TX		0x04
#define	DBUG_ZRX	0x08
#define	DBUG_RPPKT	0x10
#define	DBUG_RDUMP	0x20
#define	DBUG_TPPKT	0x40
#define	DBUG_TDUMP	0x80
#define	DBUG_SEQ	0x100

#if	DBUG
#define	dbug_tr(a)	if (si->si_unit & DBUG_TR) printf a
#define	dbug_tx(a)	if (si->si_unit & DBUG_TX) printf a
#define	dbug_rppkt(a)	if (si->si_unit & DBUG_RPPKT) printf a
#else	DBUG
#define	dbug_tr(a)
#define	dbug_tx(a)
#define	dbug_rppkt(a)
#endif	DBUG

int	en_open(), en_close(), en_poll(), en_write(), en_null();
struct device enet_en =
	{en_open, en_close, en_poll, en_write, en_null, D_IOT_PACKET};
extern struct u_char etherbcastaddr[];

#define	NRBUF	32

struct rbuf {
	char	*b_bp;
	u_int b_dmacsr;
	volatile u_char	b_done, b_devcsr;
	volatile int	b_lastbyte;
	char	b_data[ETHERMTU+100];
};

/*
 * Data structure for PROM driver.
 */
struct en_softc {
	int			sc_initialized;
	volatile struct	en_regs	*sc_addr;		/* Controller regs */
	volatile struct	dma_dev		*sc_tdma;	/* Tx DMA regs */
	volatile struct	dma_dev		*sc_rdma;	/* Rx DMA regs */
	char			sc_enetaddr[6];		/* Physical address */
	volatile short		sc_wb;			/* write buffer */
	volatile short		sc_rb;			/* read buffer */
	struct	rbuf		sc_rbuf[NRBUF];		/* receive buffers */
	char			*sc_tbuf;		/* Aligned Tx buf */
	char			*sc_tdata[ETHERMTU+50];	/* xmit buffer */
	volatile u_char		sc_double_dma, sc_xdone;
};

en_null()
{
}

en_open(si)
register struct sio *si;
{
	register struct mon_global *mg = restore_mg();
	register struct nvram_info *ni = &mg->mg_nvram;
	register struct en_softc *sc;
	register volatile struct en_regs *en;
	register volatile struct dma_dev *rdma;
	register int i, j;
	char *nb;
	struct rbuf *bp;
	int en_intr_handler(), en_intr_handler3();
	struct scb *get_vbr(), *vbr;
	extern u_char no_addr[];
	volatile struct bmap_chip *bm = (volatile struct bmap_chip*) P_BMAP;

	if (!si->si_devmem) {
	    si->si_devmem = (caddr_t) mon_alloc (sizeof (struct en_softc));
	}
	mg->mg_si = si;
	sc = (struct en_softc*) si->si_devmem;
	if (sc->sc_initialized)
		return((int)sc->sc_enetaddr);	/* Success */
	
	/* Set controller register base address */
	sc->sc_addr = (struct en_regs *) P_ENET;
	sc->sc_tdma = (struct dma_dev *) P_ENETX_CSR;
	sc->sc_rdma = (struct dma_dev *) P_ENETR_CSR;

	/* Align rbuf/tbuf */
	for (i = 0; i < NRBUF; i++) {
		bp = &sc->sc_rbuf[i];
		nb = bp->b_bp = (char*)(((int)bp->b_data & ~0xf) + 0x10);
#if	DBUG
		for (j = 0; j < ETHERMTU; j++)
			nb[j] = 0x55;
#endif	DBUG
		bp->b_done = 0;
		bp->b_dmacsr = bp->b_devcsr = 0;
	}
	sc->sc_tbuf = (char *)(((int)sc->sc_tdata & ~0xf) + 0x10);
	en = sc->sc_addr;

#if	MC68040
	/* start with TPE if requested */
	if (strcmp (mg->mg_boot_dev, "tp") == 0) {
		bm->bm_drw |= BMAP_TPE;
		en->en_txmode &= ~EN_TXMODE_NO_LBC;
		mg->mg_boot_dev = "en";
	}
#endif	MC68040
	
	en->en_reset = EN_RESET;
	en->en_txmask = 0;
#if	CRC
	en->en_rxmask = EN_RXMASK_CRCERRIE;
#else	CRC
	en->en_rxmask = 0;
#endif	CRC
	en->en_txstat = EN_TXSTAT_CLEAR;
	en->en_rxstat = EN_RXSTAT_CLEAR;
	en->en_rxmode = EN_RXMODE_OFF;	/* Rx enabled when buffer posted */
	if (bcmp (clientetheraddr+3, no_addr, 3))
		bcopy (clientetheraddr, sc->sc_enetaddr, 6);
	else
		bcopy (ni->ni_enetaddr, sc->sc_enetaddr, 6);
	bcopy (sc->sc_enetaddr, en->en_enetaddr, 6);
	rdma = sc->sc_rdma;
	sc->sc_wb = sc->sc_rb = 0;
	nb = sc->sc_rbuf[0].b_bp;
	rdma->dd_csr = DMACSR_INITBUF | DMACSR_RESET | DMACSR_READ;
	rdma->dd_next = nb;
	rdma->dd_limit = nb + ((ETHERMTU + 15) &  ~0xf);
#if	RX
#if	CHAIN_RX
	rxbufsetup(si, 1);
#else	CHAIN_RX
	rdma->dd_csr = DMACSR_READ | DMACSR_SETENABLE;
	en->en_rxmode = EN_RXMODE_NORMAL;
#endif	CHAIN_RX
#endif	RX
	sc->sc_initialized = 1;
	*mg->mg_intrmask |= I_BIT(I_ENETR_DMA);
	vbr = get_vbr();
	vbr->scb_ipl[6-1] = en_intr_handler;
#ifdef	notdef
	vbr->scb_ipl[3-1] = en_intr_handler3;
#endif
	en->en_reset = 0;
	if (curipl() >= 6)
		asm ("movw #0x2500,sr");
	return((int)sc->sc_enetaddr);	/* Success */
}

en_intsetup() {
	asm (".globl _en_intr_handler");
	asm ("_en_intr_handler:");
	asm ("moveml #0xc0c0,sp@-");
	asm ("jsr _en_intr");
	asm ("moveml sp@+,#0x0303");
	asm ("rte");
#ifdef	notdef
	asm (".globl _en_intr_handler3");
	asm ("_en_intr_handler3:");
	asm ("moveml #0xc0c0,sp@-");
	asm ("jsr _en_intr3");
	asm ("moveml sp@+,#0x0303");
	asm ("rte");
#endif
}

/* en_poll: returns 0 if a receive has not yet occurred or is not complete.
 * Returns the packet size and copies (to "p") a correct packet.
 */
en_poll(si, p, ts, maxsize)
register struct sio *si;
caddr_t p;
struct tftp_softc *ts;
{
	register struct mon_global *mg = restore_mg();
	register struct en_softc *sc =
		(struct en_softc*) si->si_devmem;
	register volatile struct en_regs *en = sc->sc_addr;
	register int len, i;
	struct rbuf *bp;
	int off;

	bp = &sc->sc_rbuf[sc->sc_rb];
	if (sc->sc_double_dma) {
		dbug_tr (("!"));
		sc->sc_double_dma = 0;
	}
	if (sc->sc_xdone) {
		printf ("X");
		sc->sc_xdone = 0;
	}
	if (bp->b_done) {
		off = 0;
		len = (bp->b_lastbyte & 0x3fffffff) - (int)bp->b_bp;
		bp->b_done = 0;
#if	DBUG
		if (si->si_unit & DBUG_SEQ)
			printf ("<%x>", bp->b_devcsr);
#endif	DBUG
		bp->b_dmacsr = bp->b_devcsr = 0;
		sc->sc_rb = (sc->sc_rb + 1) % NRBUF;
		if (len < 0 || len > (ETHERMTU + 18)) {
			printf ("L");
			return (0);
		}
#if	DBUG
		if (si->si_unit & DBUG_SEQ)
			printf ("%d:%d ", len, sc->sc_rb);
		if (si->si_unit & DBUG_RPPKT) {
			printf ("*R* len %d ", len);
			ppkt (bp->b_bp+off);
		}
#endif	DBUG
		bcopy(bp->b_bp + off, p, MIN (len, maxsize));
#if	DBUG
		if (si->si_unit & DBUG_RDUMP) {
			for (i = 0; i < 0x1000; i += 4) {
				if (((i/4) % WRAP) == 0)
					printf ("\n%08x: ",
						(int)bp->b_bp + off + i);
				printf ("%08x ",
					*(int*)((int)bp->b_bp + off + i));
				if (*(int*)((int)bp->b_bp + off + i) ==
				    0x55555555)
					break;
			}
			printf ("\n\n");
		}
		if (si->si_unit & (DBUG_RDUMP | DBUG_ZRX)) {
			for (i = 0; i < ETHERMTU; i++)
				bp->b_bp[i] = 0x55;
		}
#endif	DBUG
		return(len);
	}
	return (0);
}

en_txstat() {
}

#ifdef	notdef
en_intr3()
{
	register struct mon_global *mg = restore_mg();
	register struct sio *si = mg->mg_si;
	register struct en_softc *sc =
		(struct en_softc*) si->si_devmem;
	register volatile struct en_regs *en = sc->sc_addr;

	printf ("rxstat 0x%x ", en->en_rxstat);
	en->en_rxstat = EN_RXSTAT_CLEAR;
}
#endif

en_write(si, p, len, timeout)
register struct sio *si;
register caddr_t p;
{
	register struct mon_global *mg = restore_mg();
	register struct en_softc *sc =
		(struct en_softc*) si->si_devmem;
	register volatile struct en_regs *en = sc->sc_addr;
	register volatile struct dma_dev *tdma = sc->sc_tdma;
	register lim, i;
	int odd = ODD;
	volatile struct bmap_chip *bm = (volatile struct bmap_chip*) P_BMAP;

	/*
	 *  Switch to thin on a higher-level timeout.
	 *  Can't reasonably poll for heartbeat on every TPE transmit.
	 */
#if	MC68040
	if (timeout) {
		bm->bm_drw &= ~BMAP_TPE;
		en->en_txmode |= EN_TXMODE_NO_LBC;
	}
#endif	MC68040
		
	/* Wait till device is ready */
retrans:
	while (!(en->en_txstat & EN_TXSTAT_READY))
		printf ("en_write: tx not ready\n");
	en->en_txstat = EN_TXSTAT_CLEAR;
	tdma->dd_csr = DMACSR_INITBUF | DMACSR_RESET | DMACSR_CLRCOMPLETE;
	tdma->dd_csr = 0;
	bcopy(p, sc->sc_tbuf + odd + TXOFF, len);
#if	DBUG
	if (si->si_unit & DBUG_TPPKT) {
		printf ("*T* len %d txstat 0x%x ", len, en->en_txstat);
		ppkt (p);
		en->en_txstat = EN_TXSTAT_CLEAR;
	}
	if (si->si_unit & DBUG_TDUMP) {
		for (i = 0; i < len; i += 4) {
			if (((i/4) % WRAP) == 0)
				printf ("\n%04x: ",
					(int)sc->sc_tbuf + odd + i + TXOFF);
			printf ("%08x ",
				*(int*)((int)sc->sc_tbuf + odd + i + TXOFF));
		}
		printf ("\n\n");
	}
#endif	DBUG
	*(int*)((int)&tdma->dd_next + 0x200) = (int)sc->sc_tbuf;
	tdma->dd_saved_next = (char*) sc->sc_tbuf;
#if	CHAIN_TX
	tdma->dd_limit = (char*) ((int) sc->sc_tbuf + 16);
	tdma->dd_saved_limit = (char*) ((int) sc->sc_tbuf + 16);
	tdma->dd_start = (char*) ((int) sc->sc_tbuf + 16);
	tdma->dd_savestart = (char*) ((int) sc->sc_tbuf + 16);
#endif	CHAIN_TX
	lim = (int) sc->sc_tbuf + odd + len;
	lim |= ENTX_EOP;		/* Signify last (only) buffer */
	lim += 15;
	dbug_tx (("next 0x%x limit 0x%x\n", sc->sc_tbuf, lim));
#if	CHAIN_TX
	tdma->dd_stop = (char*) lim;
	tdma->dd_savestop = (char*) lim;
#else	CHAIN_TX
	tdma->dd_limit = (char*) lim;
	tdma->dd_saved_limit = (char*) lim;
#endif	CHAIN_TX
#if	CHAIN_TX
	if (odd)
		tdma->dd_csr = DMACSR_READ | DMACSR_SETENABLE |
			DMACSR_SETSUPDATE;
	else
		tdma->dd_csr = DMACSR_SETENABLE | DMACSR_SETSUPDATE;
#else	CHAIN_TX
	if (odd)
		tdma->dd_csr = DMACSR_READ | DMACSR_SETENABLE;
	else
		tdma->dd_csr = DMACSR_SETENABLE;
#endif	CHAIN_TX

#if	MC68040
#define	EN_TX_TIMEOUT	1000000
	if (!(bm->bm_drw & BMAP_TPE_RXSEL)) {
		for (i = 0; i < EN_TX_TIMEOUT && !(tdma->dd_csr & DMACSR_COMPLETE);)
			i++;
		if ((en->en_txstat & EN_TXSTAT_COLLERR16) || i == EN_TX_TIMEOUT) {
			for (i = 0; i < EN_TX_TIMEOUT &&
			    (bm->bm_drw & BMAP_HEARTBEAT);)
				i++;
			if (i < EN_TX_TIMEOUT) {
				bm->bm_drw |= BMAP_TPE;
				en->en_txmode &= ~EN_TXMODE_NO_LBC;
			}
			goto retrans;
		}
	}
#endif	MC68040
	return (0);
}

en_close(si)
register struct sio *si;
{
	register struct en_softc *sc =
		(struct en_softc*) si->si_devmem;
	register volatile struct dma_dev *tdma = sc->sc_tdma;
	register volatile struct dma_dev *rdma = sc->sc_rdma;
	register volatile struct en_regs *en = sc->sc_addr;

	tdma->dd_csr = DMACSR_INITBUF | DMACSR_RESET;
	rdma->dd_csr = DMACSR_INITBUF | DMACSR_RESET;
	en->en_reset = EN_RESET;
	return (0);
}

en_intr() {
	register struct mon_global *mg = restore_mg();
	register struct sio *si = mg->mg_si;
	register struct en_softc *sc =
		(struct en_softc*) si->si_devmem;
	register volatile struct dma_dev *rdma = sc->sc_rdma;
	register volatile struct en_regs *en = sc->sc_addr;
	struct rbuf *nb;
	int tries;

	nb = &sc->sc_rbuf[sc->sc_wb];
	tries = 0;
	while (((nb->b_dmacsr = rdma->dd_csr & 0x0f000000) == 0) && (++tries < 32))
		;
	nb->b_devcsr = en->en_rxstat;
	nb->b_lastbyte = (int) rdma->dd_saved_limit;
	nb->b_done = 1;
	en->en_rxstat = EN_RXSTAT_CLEAR;
	sc->sc_wb = (sc->sc_wb + 1) % NRBUF;
	rxbufsetup (si, 0);
}

/* Enable receives, post buffer */
rxbufsetup(si, first)
	register struct sio *si;
{
	register struct en_softc *sc =
		(struct en_softc*) si->si_devmem;
	register volatile struct en_regs *en = sc->sc_addr;
	register volatile struct dma_dev *rdma = sc->sc_rdma;
	register u_int dmacsr, tmp;
	register int i;

	struct rbuf *nb;

#if	CHAIN_RX
	/* check for both chains being empty */
	if (!first) {
		while ((dmacsr = rdma->dd_csr & 0x0f000000) == 0)
			;
		if ((dmacsr & DMACSR_ENABLE) == 0) {
		nb = &sc->sc_rbuf[sc->sc_wb];
		nb->b_dmacsr = dmacsr;
		nb->b_devcsr = 0x7f;
		nb->b_done = 1;
		en->en_rxstat = EN_RXSTAT_CLEAR;
		sc->sc_wb = (sc->sc_wb + 1) % NRBUF;
		nb = &sc->sc_rbuf[sc->sc_wb];
		rdma->dd_next = nb->b_bp;
		rdma->dd_limit = nb->b_bp + ((ETHERMTU + 15) &  ~0xf);
		nb = &sc->sc_rbuf[(sc->sc_wb+1) % NRBUF];
		rdma->dd_start = nb->b_bp;
		rdma->dd_stop = nb->b_bp + ((ETHERMTU + 15) &  ~0xf);
		tmp = DMACSR_READ | DMACSR_SETSUPDATE | DMACSR_CLRCOMPLETE |
			DMACSR_SETENABLE;
		sc->sc_double_dma = 1;
		} else
			goto foo;
	} else {
foo:
		/* Enable channel */
 		nb = &sc->sc_rbuf[(sc->sc_wb+1) % NRBUF];
		rdma->dd_start = nb->b_bp;
		rdma->dd_stop = nb->b_bp + ((ETHERMTU + 15) &  ~0xf);
		tmp = DMACSR_READ | DMACSR_SETSUPDATE | DMACSR_CLRCOMPLETE;
		if (first)
			tmp |= DMACSR_SETENABLE;
	}
#else	CHAIN_RX
		nb = &sc->sc_rbuf[sc->sc_wb];
		rdma->dd_next = nb->b_bp;
		rdma->dd_limit = nb->b_bp + ((ETHERMTU + 15) &  ~0xf);
		tmp = DMACSR_READ | DMACSR_SETENABLE | DMACSR_CLRCOMPLETE;
#endif	CHAIN_RX
	rdma->dd_csr = tmp;
#if	PROMISC
	en->en_rxmode = EN_RXMODE_PROMISCUOUS;
#else
	en->en_rxmode = EN_RXMODE_NORMAL;
#endif
	while ((dmacsr = rdma->dd_csr & 0x0f000000) == 0)
		;
	if ((dmacsr & DMACSR_ENABLE) == 0)
		sc->sc_xdone = 1;
}
