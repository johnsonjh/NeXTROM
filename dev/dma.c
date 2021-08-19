/*	@(#)dma.c	1.0	08/12/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 * HISTORY
 *
 * 12-Aug-87 Mike DeMoney (mike) at NeXT
 *	Created
 *
 **********************************************************************
 */ 

#include <sys/param.h>
#include <next/cpu.h>
#include <nextdev/dma.h>
#include <mon/global.h>

#ifndef NULL
#define	NULL		0
#endif

/*
 * dma.c -- support routines for NeXT DMA hardware
 */

/*
 * dma_init -- initialize dma channel hardware
 * dma_chan struct must be initialized before calling
 */
void
dma_init(dcp)
register struct dma_chan *dcp;
{
	volatile register struct dma_dev *ddp;
	caddr_t cp;

	dcp->dc_flags &=~ (DMACHAN_BUSY | DMACHAN_CHAINING | DMACHAN_ERROR);
	ddp = dcp->dc_ddp;
	ddp->dd_csr = DMACMD_RESET;
	dcp->dc_tail = (char *)roundup((u_int)dcp->dc_tailbuf,
				       DMA_ENDALIGNMENT);
}

/*
 * Given a virtual address and count, costruct a DMA chain list by
 * looking for contiguous groups of physical pages.  No chain is
 * made larger than the DMA hardware can handle.
 *
 * SHOULD ONLY BE USED FOR LASER PRINTER, DISK, SCSI, and DSP DMA
 */
void
dma_list(dcp, fdhp, vaddr, bcount, direction)
struct dma_chan *dcp;
dma_list_t fdhp;
caddr_t vaddr;
long bcount;
int direction;
{
	struct dma_hdr *dhp;
	caddr_t paddr, start, stop;
	unsigned offset;
	long rem;

	if (! DMA_BEGINALIGNED(vaddr))
		printf("dma_list: bad alignment");
	dhp = fdhp;
	dhp->dh_start = vaddr;
	dhp->dh_stop = vaddr + bcount;
	dhp->dh_link = NULL;
	dhp->dh_state = 0;
	dhp->dh_flags = 0;
	dcp->dc_taillen = 0;
	if (direction == DMACSR_READ) { /* FROM DEVICE TO MEM */
		if ((rem = dhp->dh_stop - dhp->dh_start) < DMA_MAXTAIL) {
			dcp->dc_taillen = rem;
			dcp->dc_tailstart = dhp->dh_start;
			dhp->dh_start = (caddr_t)dcp->dc_tail;
			dhp->dh_stop = (char *)roundup((u_int)dcp->dc_tail+rem,
			    DMA_ENDALIGNMENT);
		} else {
			dcp->dc_tailstart = (char *)
			  ((u_int)dhp->dh_stop &~ (DMA_ENDALIGNMENT-1));
			dcp->dc_taillen = dhp->dh_stop - dcp->dc_tailstart;
			rem = 2 * DMA_ENDALIGNMENT;
			dhp->dh_stop = dcp->dc_tailstart;
			dhp->dh_link = dhp + 1;
			dhp->dh_state = 0;
			dhp->dh_flags = 0;
			dhp++;
			dhp->dh_start = (caddr_t)dcp->dc_tail;
			dhp->dh_stop = dcp->dc_tail + rem;
		}
		dhp->dh_link = NULL;
		dhp->dh_state = 0;
	} else {			/* FROM MEM TO DEVICE */
		dhp->dh_stop = (char *)
		    roundup((u_int)dhp->dh_stop, DMA_ENDALIGNMENT);
		dhp->dh_link = dhp + 1;
		dhp->dh_state = 0;
		dhp->dh_flags = 0;
		/* guarantee that pre-fetches don't exhaust dma */
		dhp++;
		dhp->dh_start = (caddr_t)dcp->dc_tail;
		dhp->dh_stop = dhp->dh_start + 3*DMA_ENDALIGNMENT;
		dhp->dh_link = NULL;
		dhp->dh_state = 0;
		dhp->dh_flags = 0;
	}
	fdhp->dh_flags = DMADH_INITBUF;
}

void
dma_cleanup(dcp, resid)
struct dma_chan *dcp;
long resid;
{
	/*
	 * Before calling, driver must have flushed tail of 
	 * transfer.  We're called with what the driver believes is the
	 * remaining byte count for the transfer (resid).  We have
	 * to transfer any part of the tail that come before that
	 * residual.  It should be sitting in dc_tail and should be
	 * copied to physical addresses starting at dc_tailstart.
	 */
	dma_abort(dcp);
	if (resid < 0)
		printf("dma_cleanup: negative resid");
	if (resid < dcp->dc_taillen)
		bcopy(dcp->dc_tail, dcp->dc_tailstart, dcp->dc_taillen-resid);
}

/*
 * dma_start -- start dma at a particular dma header
 */
void
dma_start(dcp, dhp, direction)
register struct dma_chan *dcp;
register struct dma_hdr *dhp;
register int direction;
{
	volatile register struct dma_dev *ddp;
	register int s, resetbits;
	struct mon_global *mg = restore_mg();

	dcp->dc_direction = direction;
	ddp = dcp->dc_ddp;

	/* direction must be set before next & limit written */
	resetbits = DMACMD_RESET | direction;

	if (dhp->dh_flags & DMADH_INITBUF) {
		ddp->dd_csr = resetbits;
		ddp->dd_next_initbuf = dhp->dh_start;
	} else {
		ddp->dd_csr = resetbits;
		ddp->dd_next = dhp->dh_start;
	}
	ddp->dd_limit = dhp->dh_stop;
		
	if ((dhp = dhp->dh_link) != NULL) {
		ddp->dd_start = dhp->dh_start;
		ddp->dd_stop = dhp->dh_stop;
		ddp->dd_csr = DMACMD_STARTCHAIN | direction;
		dcp->dc_flags |= DMACHAN_CHAINING;
	} else {
		ddp->dd_csr = DMACMD_START | direction;
	}
	dcp->dc_flags |= DMACHAN_BUSY;
}

/*
 * dma_abort -- shutdown dma on channel
 * SIDE-EFFECT: saves next and dma state in dma_chan struct.
 */
void
dma_abort(dcp)
register struct dma_chan *dcp;
{
	volatile register struct dma_dev *ddp;
	register int s;

	ddp = dcp->dc_ddp;
	dcp->dc_state = ddp->dd_csr & DMASTATE_MASK;
	ddp->dd_csr = DMACMD_RESET;
	dcp->dc_next = ddp->dd_next;
	dcp->dc_flags &=~ (DMACHAN_BUSY | DMACHAN_CHAINING
			   | DMACHAN_ERROR);
}
