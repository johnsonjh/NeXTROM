/*	@(#)screg.h	1.0	09/10/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 * screg.h -- register definitions for NCR53C90 specific scsi routines
 * PROM MONITOR VERSION
 *
 * HISTORY
 * 10-Sept-87  Mike DeMoney (mike) at NeXT
 *	Created.
 *
 **********************************************************************
 */ 

#ifndef _SCREG_
#define	_SCREG_

struct scsi_5390_regs {
	u_char	s5r_cntlsb;		/* 0x0: lsb of transfer count */
	u_char	s5r_cntmsb;		/* 0x1: msb of transfer count */
	u_char	s5r_fifo;		/* 0x2: data fifo */
	u_char	s5r_cmd;		/* 0x3: command register */
	u_char	s5r_status;		/* 0x4: READ: status register */
#define	s5r_select	s5r_status	/* 0x4: WRITE: selection id */
	u_char	s5r_intrstatus;		/* 0x5: READ: interrupt status */
#define	s5r_seltimeout	s5r_intrstatus	/* 0x5: WRITE: select timeout */
	u_char	s5r_seqstep;		/* 0x6: READ: sequence step */
#define	s5r_syncperiod	s5r_seqstep	/* 0x6: WRITE: sync period */
	u_char	s5r_fifoflags;		/* 0x7: READ: fifo flags */
#define	s5r_syncoffset	s5r_fifoflags	/* 0x7: WRITE: sync offset */
	u_char	s5r_config;		/* 0x8: configuration register */
	u_char	s5r_clkconv;		/* 0x9: WRITEONLY: clock conv. factor */
	u_char	s5r_testmode;		/* 0xa: WRITEONLY: test mode */
	u_char	s5r_pad[0x15];		/* 0xb - 0x1f: not used */
	u_char	s5r_dmactrl;		/* 0x20: dma control */
	u_char	s5r_dmastatus;		/* 0x21: dma status */
};

/*
 * 5390 commands
 * (target commands not shown)
 */

/* Miscellaneous commands */
#define	S5RCMD_NOP		0x00		/* no operation */
#define	S5RCMD_FLUSHFIFO	0x01		/* flush 5390 fifo */
#define	S5RCMD_RESETCHIP	0x02		/* hard reset 5390 */
#define	S5RCMD_RESETBUS		0x03		/* reset SCSI bus */

/* disconnect mode commands */
#define	S5RCMD_SELECT		0x41		/* select w/o ATN */
#define	S5RCMD_SELECTATN	0x42		/* select w/ ATN */
						/* sends IDENTIFY msg & cmd */
#define	S5RCMD_SELECTATNSTOP	0x43		/* select w/ ATN & STOP */
						/* sends IDENTIFY then stops */
#define	S5RCMD_ENABLESELECT	0x44		/* enable select/reselect */
#define	S5RCMD_DISABLESELECT	0x45		/* disable select/reselect */

/* initiator mode commands */
#define	S5RCMD_TRANSFERINFO	0x10		/* transfer info */
#define	S5RCMD_INITCMDCMPLT	0x11		/* get status & msg */
#define	S5RCMD_MSGACCEPTED	0x12		/* msg ok, release ACK */
#define	S5RCMD_TRANSFERPAD	0x18		/* transfer null data */
#define	S5RCMD_SETATN		0x1a		/* assert ATN */

/* target mode commands */
#define	S5RCMD_DISCONNECT	0x27		/* release BSY */

/* OR this with command to enable DMA */
#define	S5RCMD_ENABLEDMA	0x80		/* enable dma */

/*
 * status register
 */
#define	S5RSTS_GROSSERROR	0x40		/* nasty error */
#define	S5RSTS_PARITYERROR	0x20		/* SCSI bus parity error */
#define	S5RSTS_COUNTZERO	0x10		/* transfer count == 0 */
#define	S5RSTS_TRANSFERCMPLT	0x08		/* command received cmplt */
#define	S5RSTS_PHASEMASK	0x07		/* SCSI bus phase mask */

/*
 * interrupt status register
 */
#define	S5RINT_SCSIRESET	0x80		/* SCSI bus reset detected */
#define	S5RINT_ILLEGALCMD	0x40		/* illegal cmd issued */
#define	S5RINT_DISCONNECT	0x20		/* target disconnected */
#define	S5RINT_BUSSERVICE	0x10		/* no need for MSGACCEPTED */
#define	S5RINT_FUNCCMPLT	0x08		/* discon't cmd cmplt */
#define	S5RINT_RESELECTED	0x04		/* reselected as initiator */
#define	S5RINT_SELECTEDATN	0x02		/* selected with ATN */
#define	S5RINT_SELECTED		0x01		/* just selected */

/*
 * configuration register
 */
#define	S5RCNF_SLOWCABLE	0x80		/* incr data setup time */
#define	S5RCNF_DISABLERESETINTR	0x40		/* disable reset intr */
#define	S5RCNF_PARITYTEST	0x20		/* parity test mode */
#define	S5RCNF_ENABLEPARITY	0x10		/* enable parity check */
#define	S5RCNF_BUSIDMASK	0x07		/* mask for our SCSI bus id */

/*
 * fifo flags register
 */
#define	S5RFLG_FIFOLEVELMASK	0x1f		/* fifo slots filled */

/*
 * sequence register
 */
#define	S5RSEQ_SEQMASK		0x07		/* sequence step */

/*
 * clock conversion factor calculation
 * clkMHz is 5390 clock frequency in MHz (20 => 20MHz clock)
 */
#define	S5RCLKCONV_FACTOR(clkMHz)	( (clkMHz) <= 10 ? 2 \
					:((clkMHz) <= 15 ? 3 \
					:((clkMHz) <= 20 ? 4 \
					: 5)))

/*
 * select timeout calculation
 * clkMHz is 5390 clock frequency in MHz (20 => 20MHz clock)
 * TOms is desired selection timeout in milliseconds (250 => 250ms timeout)
 */
#define	S5RSELECT_TIMEOUT(clkMHz,TOms)	\
	((((clkMHz)*(TOms)*1000)+(S5RCLKCONV_FACTOR(clkMHz)*8192)-1) \
	/ (S5RCLKCONV_FACTOR(clkMHz)*8192))

/*
 * dma control register
 */
#define	S5RDMAC_CLKMASK		0xc0	/* clock selection bits */
#define	S5RDMAC_10MHZ		0x00	/* 10 MHz clock */
#define	S5RDMAC_12MHZ		0x40	/* 12.5 MHz clock */
#define	S5RDMAC_16MHZ		0xc0	/* 16.6 MHz clock */
#define	S5RDMAC_20MHZ		0x80	/* 20 MHz clock */
#define	S5RDMAC_INTENABLE	0x20	/* interrupt enable */
#define	S5RDMAC_DMAMODE		0x10	/* 1 => dma, 0 => pio */
#define	S5RDMAC_DMAREAD		0x08	/* 1 => dma from scsi to mem */
#define	S5RDMAC_FLUSH		0x04	/* flush fifo */
#define	S5RDMAC_RESET		0x02	/* reset scsi chip */
#define	S5RDMAC_WD33C92		0x01	/* 1 => WD33C92, 0 => NCR 53[89]0 */

/*
 * dma fifo status register
 */
#define	S5RDMAS_STATE		0xc0	/* DMA/SCSI bank state */
#define	S5RDMAS_D0S0		0x00	/* DMA rdy for bank 0, SCSI in bank 0 */
#define	S5RDMAS_D0S1		0x40	/* DMA req for bank 0, SCSI in bank 1 */
#define	S5RDMAS_D1S1		0x80	/* DMA rdy for bank 1, SCSI in bank 1 */
#define	S5RDMAS_D1S0		0xc0	/* DMA req for bank 1, SCSI in bank 0 */
#define	S5RDMAS_OUTFIFOMASK	0x38	/* output fifo byte (INVERTED) */
#define	S5RDMAS_INFIFOMASK	0x07	/* input fifo byte (INVERTED) */

#define	S5RDMA_FIFOALIGNMENT	4	/* mass storage chip buffer size */

#endif _SCREG_
