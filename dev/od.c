/*	@(#)od.c	1.0	08/12/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 * HISTORY
 *  8-May-88  John Seamons (jks) at NeXT
 *	Standalone version.
 *
 * 12-Aug-87  John Seamons (jks) at NeXT
 *	Created.
 *
 **********************************************************************
 */ 

#define	NOD		1
#define	MULTI_SURFACE	0

#include "sys/types.h"
#include "sys/param.h"
#ifdef SUN_NFS
#include "sys/time.h"
#include "sys/vnode.h"
#include "ufs/inode.h"
#include "ufs/fs.h"
#include "ufs/fsdir.h"
#else !SUN_NFS
#include "sys/inode.h"
#include "sys/fs.h"
#include "sys/dir.h"
#endif !SUN_NFS
#include "sys/buf.h"
#include "next/psl.h"
#include "next/cpu.h"
#include "next/scr.h"
#include "next/pmap.h"
#include "next/printf.h"
#include "nextdev/busvar.h"
#include "nextdev/odreg.h"
#include <dev/odvar.h>
#include "mon/global.h"

#if	STANDALONE
#include <nextdev/dma.h>
#include "saio.h"
#else	STANDALONE
#include "mon/sio.h"
#include <nextdev/dma.h>
#endif	STANDALONE

/*
 *	A number of "controllers" (NOC) can control NUNIT "units" per
 *	controller.  A "drive" (NOD) is an arbitrary controller/unit pair.
 *	NOD and NOC are determined from the kernel configuration file.
 *	Each drive has a number of logical partitions (NPART).
 *	The minor device number encodes the drive and partition to use:
 *
 *	minor(dev) = vvvddppp
 *	v = volume 0-7
 *	d = drive 0-3
 *	p = partition 0-7
 */
#define	NDRIVE	4
#define	OD_VOL(dev)		(minor(dev) >> 5)
#define OD_DRIVE(dev)		((minor(dev) >> 3) & 3)
#define OD_PART(dev)		(minor(dev) & 7)
#define	OD_DEV(drive, part)	(((drive) << 3) | (part))
#define	NUNIT	2			/* number of units per ctrl */

#define	NDRIVES		1		/* number of supported drive types */
struct drive_info od_drive_info[NDRIVES] = {
#define	DRIVE_TYPE_OMD1	0
	{				/* OMD-1 drive */
		"Canon OMD-1",			/* disk name */
		{ 0*16, 15430*16, -1, -1 },	/* label locations */
		1024,				/* device sector size */
		256 * 1024,			/* max xfer bcount */
	},
};

/* additional info not kept in standard device_info struct */
struct	more_info {			/* info about media */
	int	mi_reserved_cyls;	/* # cyls to skip at front of media */
	short	mi_nsect;		/* # sectors / track */
} od_more_info[NDRIVES] = {
	{ 4149, 16 },			/* OMD-1 drive FIXME: 4149 -> 4096 */
};

#if	STANDALONE
#else	STANDALONE
int odopen(), odclose(), odread(), odwrite(), odlabel_blkno();
struct device optical_disk = {
	odopen, odclose, odread, odwrite, odlabel_blkno, D_IOT_PACKET
};
#endif	STANDALONE

/*
 * These two structures mirror bus_ctrl and bus_device
 * but contain more specific information for the driver.
 */
struct ctrl {				/* per controller info */
	struct	dma_chan c_dma_chan;	/* DMA info */
	dma_list_t c_dma_list;		/* DMA hdr list */
	volatile struct	od_regs *c_od_regs; /* controller registers */
	caddr_t	c_va;			/* virt addr of buffer */
	struct pmap *c_pmap;		/* physical map of buffer */
	daddr_t	c_bno;			/* requested block number */
	int	c_flags;
#define	CF_CUR_POS	0x80000000	/* current head position known */
#define	CF_ISSUE_D_CMD	0x40000000	/* issue drive command */
#define	CF_ISSUE_F_CMD	0x20000000	/* issue formatter command */
#define	CF_DMAINT_EXP	0x10000000	/* DMA interrupt expected */
#define	CF_DEVINT_EXP	0x08000000	/* device interrupt expected */
#define	CF_ATTN_INPROG	0x04000000	/* attn in progress, no new cmds */
#define	CF_ATTN_WAKEUP	0x02000000	/* wake me up when attn finishes */
#define	CF_ATTN_ASYNC	0x01000000	/* attn was asynchronous */
#define	CF_ERR_INPROG	0x00800000	/* error recovery in progress */
#define	CF_ONLINE	0x00400000	/* controller is online */
#define	CF_SOFT_TIMEOUT	0x00200000	/* soft timeout has occured */
#define	CF_NEED_DMA	0x00100000	/* DMA needed for this operation */
#define	CF_PHYS		0x00080000	/* physical disk I/O */
#define	CF_TESTING	0x00040000	/* testing in progress */
#define	CF_ASYNC_INPROG	0x00020000	/* async processing in progress */
#define	CF_DMA_ERROR	0x00010000	/* DMA error has occured */
#define	CF_CANT_SLEEP	0x00008000	/* interrupts can't sleep */
#define	CF_ERR_VALID	0x00004000	/* c_err is valid */
#define	CF_50_50_SEEK	0x00002000	/* first try at a 50/50 seek chance */
#define	CF_FAIL_IO	0x00001000	/* fail this I/O operation */
	int	*c_tbuf;		/* aligned pointer to test buffer */
#define	MAXBUF		8192		/* test & copy buffer size */
	int	c_tbuffer[MAXBUF/4];	/* test buffer (worst case size) */
	char	c_align1[16];		/* slop for DMA alignment */
	int	*c_cbuf;		/* aligned pointer to copy buffer */
	int	c_ccount;		/* copy count */
	struct pmap *c_cmap;		/* copy map */
	int	c_cbuffer[MAXBUF/4];	/* copy buffer (worst case size) */
	char	c_align2[16];		/* slop for DMA alignment */
	short	c_track;		/* requested track */
	short	c_seek_track;		/* seek track (may not be same) */
	int	c_blkno;		/* block number to transfer */
	int	c_test_blkno;		/* block number during testing */
	int	c_pblk;			/* physical block number */
	short	c_nsect;		/* number of sectors to xfer */
	short	c_xfer;			/* number actually xfer'd */
	u_short	c_cmd;			/* transfer command */
	u_short	c_rds;			/* drive status */
	u_short	c_res;			/* extended status */
	u_short	c_rhs;			/* hardware status */
	short	c_offset;		/* offset into status bitmap */
	char	c_shift;		/* shift into status bitmap */
	short	c_cur_track;		/* current track under head */
	char	c_cur_sect;		/* current sector under head */
#if	MULTI_SURFACE
	char	c_cur_surface;		/* current surface under head */
#endif	MULTI_SURFACE
	char	c_err;			/* error code */
	u_short	c_dcmd;			/* current drive command */
	u_short	c_last_dcmd;		/* last issued drive command */
	u_char	c_fcmd;			/* current formatter command */
	char	c_test_pass;		/* pass number during testing */
	u_char	c_retry;		/* number of retries attempted */
	u_char	c_rtz;			/* number of RTZs performed */
	char	c_timeout;		/* ticks since command started */
	char	c_update;		/* label/bitmap update interval */
	u_short	c_head;			/* requested head */
	char	c_sect;			/* requested sector */
#if	MULTI_SURFACE
	char	c_surface;		/* requested surface */
#endif	MULTI_SURFACE
	char	c_fsm;			/* transfer finite state machine */
#define	FSM_START	1		/* start a transfer */
#define	FSM_SEEK	2
#define	FSM_WRITE	3
#define	FSM_VERIFY	4
#define	FSM_WORKED	5
#define	FSM_FAILED	6
#define	FSM_EJECT	7
#define	FSM_ERR_DONE	8
#define	FSM_ATTN_RDS	9
#define	FSM_ATTN_RES	10
#define	FSM_ATTN_RHS	11
#define	FSM_ATTN_RID	12
#define	FSM_INSERT_RID	13
#define	FSM_INSERT_STM	14
#define	FSM_TEST_WRITE	15
#define	FSM_TEST_ERASE	16
#define	FSM_TEST_DONE	17
#define	FSM_WRITE_RETRY	18
#define	FSM_RESPIN_RST	19
#define	FSM_RESPIN_STM	20
	char	c_next_fsm;
	char	c_retry_fsm;
	char	c_fsm_test;
	char	c_err_fsm_save;
	char	c_respin_fsm_save;
	char	c_attn_fsm_save;
	char	c_attn_next_fsm_save;
};

struct drive {				/* per drive info */
	struct	ctrl *d_ctrl;		/* controller info */
	struct	bus_ctrl *d_bc;		/* bus controller */
	struct	bus_device *d_bd;	/* bus device */
	struct	disk_label *d_label;	/* aligned pointer to disk label */
	union {
		struct	disk_label d_lab;	/* disk label */
		char	d_sect_pad[MAXBUF];	/* FIXME? */
	} d_un;
	char	d_align1[16];		/* slop for DMA alignment */
	struct	drive_info *d_di;	/* drive info */
	struct	buf d_queue;		/* buffer queue */
	struct	buf d_rbuf;		/* buffer for raw I/O */
	struct	buf d_cbuf;		/* buffer for special commands */
	struct	proc *d_insert_proc;	/* proc to notify on insert event */
	int	d_drive;		/* drive number */
	int	d_base;			/* base sector number of media area */
	int	d_size;			/* size of media area (sectors) */
	int	d_insert_sig;		/* signal to send on insert event */
	short	d_flags;
#define	DF_ONLINE		0x8000	/* drive is online */
#define	DF_SPINNING		0x4000	/* disk is inserted */
#define	DF_UPDATE		0x2000	/* update label & bitmap on disk */
#define	DF_SPIRALING		0x1000	/* drive is spiraling */
#define	DF_SPEC_RETRY		0x0800	/* user specified retry/rtz values */
#define	DF_IGNORE_BITMAP	0x0400	/* ignore bitmap information */
	u_short	d_cmd;			/* special drive command */
	u_char	d_retry;		/* user specified # of retries */
	u_char	d_rtz;			/* user specified # of restores */
	char	d_drive_type;		/* type of drive */
	char	d_spiral_time;		/* idle time spent spiraling */
};

/* tuned constants */
/* FIXME: are these right? */
#define	ISSUE_LATENCY	1		/* sect latency to issue new cmd */
#define	JUMP_LATENCY	2		/* sect latency to jump 1 track */

/* flags */
#define	SPIN		0x01
#define	DONT_SPIN	0x02
#define	INTR		0x04
#define	DONT_INTR	0x08

/* return codes */
#define	OD_BUSY		1
#define	OD_DONE		0
#define	OD_ERR		-1

/* error message filter */
#define	EF_FAILED	3
#define	EF_RESPIN	2
#define	EF_RTZ		1
#define	EF_RETRY	0
int	od_errmsg_filter = EF_RESPIN;		/* patchable */
char *od_errtype[] = {
	"retry", "restore", "re-spin", "failed",
};

/* flag strategy info */
u_char	od_flgstr[OMD_NFLAG] = { 0x35, 0x00, 0x00, 0x33, 0x44, 0x35, 0x33 };
int	od_frmr = 0x26;

/* misc */
int	od_land = 1;			/* distance to land seek before op */
#define	ALIGN(type, addr) \
	((type)(((unsigned)(addr)+DMA_ENDALIGNMENT-1)&~(DMA_ENDALIGNMENT-1)))

/* debug */
#define	od_dbug		0x0
#if	0
#define	dbug_fsm	if (od_dbug & 0x001) printf
#define	dbug_xfer	if (od_dbug & 0x002) printf
#define	DBUG_STATUS	0x004
#define	dbug_status	if (od_dbug & DBUG_STATUS) printf
#define	dbug_dma	if (od_dbug & 0x008) printf
#define	dbug_intr	if (od_dbug & 0x010) printf
#define	dbug_timer	if (od_dbug & 0x020) printf
#define	dbug_spiral	if (od_dbug & 0x040) printf
#define	dbug_bitmap	if (od_dbug & 0x080) printf
#define	dbug_where	if (od_dbug & 0x100) printf
#define	dbug_flags	if (od_dbug & 0x200) printf
#define	DBUG_CMD	0x400
#define	dbug_cmd	if (od_dbug & DBUG_CMD) printf
#define	dbug_attn	if (od_dbug & 0x800) printf
#define	DBUG_REMAP	0x1000
#define	dbug_remap	if (od_dbug & DBUG_REMAP) printf
#define	dbug_stats	if (od_dbug & 0x2000) printf
#define	dbug_flgstr	if (od_dbug & 0x4000) printf
#define	dbug_istat	if (od_dbug & 0x8000) printf
#define	dbug_update	if (od_dbug & 0x10000) printf
#define	DBUG_TIMEOUT	0x20000
#define	dbug_timeout	if (od_dbug & DBUG_TIMEOUT) printf
#define	dbug_rj		if (od_dbug & 0x40000) printf
#define	dbug_valid	if (od_dbug & 0x80000) printf
#define	DBUG_READ_LABEL	0x01000000
#define	DBUG_IGNORE	0x02000000
#define	DBUG_NOTEST	0x04000000
#define	DBUG_CURPOS	0x08000000
#define	DBUG_DAEMON	0x10000000
#define	DBUG_50_50	0x20000000
#define	DBUG_RJ		0x40000000
#define	DBUG_ATTN	0x80000000
#else
#define	dbug_fsm
#define	dbug_xfer
#define	DBUG_STATUS	0x004
#define	dbug_status
#define	dbug_dma
#define	dbug_intr
#define	dbug_timer
#define	dbug_spiral
#define	dbug_bitmap
#define	dbug_where
#define	dbug_flags
#define	DBUG_CMD	0x400
#define	dbug_cmd
#define	dbug_attn
#define	DBUG_REMAP	0x1000
#define	dbug_remap
#define	dbug_stats
#define	dbug_flgstr
#define	dbug_istat
#define	dbug_update
#define	DBUG_TIMEOUT	0x20000
#define	dbug_timeout
#define	dbug_rj
#define	dbug_valid
#define	DBUG_READ_LABEL	0x01000000
#define	DBUG_IGNORE	0x02000000
#define	DBUG_NOTEST	0x04000000
#define	DBUG_CURPOS	0x08000000
#define	DBUG_DAEMON	0x10000000
#define	DBUG_50_50	0x20000000
#define	DBUG_RJ		0x40000000
#define	DBUG_ATTN	0x80000000
#endif

odinit (reg, ctrl, silent, no_wait)
	register caddr_t reg;
{
	struct mon_global *mg = restore_mg();
	register struct ctrl *c;
	volatile register struct od_regs *r = (struct od_regs*) reg;
	register struct dma_chan *dcp;
	register int i;
	int od_dma_intr(), initr;
	volatile struct scr1 scr1;


	/* wait for drive to power up */
	r->r_cntrl1 = 0;
	r->r_cntrl2 = 0;
	if (no_wait && (r->r_disr & OMD_CMD_COMPL) == 0)
		return (BE_NODEV);
#define	TOUT	3000000		/* includes time for od_reset() */
	for (i = 0; i < TOUT && (r->r_disr & OMD_CMD_COMPL) == 0; i++)
		;
	if (i == TOUT) {
		if (!silent)
			printf ("no optical disk\n");
		return (BE_NODEV);
	}
#undef	TOUT
	c = (struct ctrl*) mg->mg_alloc (sizeof (*c));
	mg->mg_odc = (char*) c;
	dcp = &c->c_dma_chan;
	bzero (c, sizeof (struct ctrl));	/* init ctrl info */
	c->c_od_regs = r;
	c->c_flags |= CF_ONLINE;

	/* clear and disable interrupts */
	r->r_disr = OMD_CLR_INT;
	r->r_dimr = 0;

	/* set various flags */
	initr = OMD_SEC_GREATER | OMD_ID_1234 | OMD_ECC_STV_DIS;
	scr1 = *(struct scr1*) P_SCR1;
	if (scr1.s_cpu_clock >= CPU_25MHz)
		initr |= OMD_25_MHZ;
	r->r_initr = initr;
	r->r_frmr = (u_char) od_frmr;
	r->r_dmark = 0x01;
	for (i = 0; i < OMD_NFLAG; i++) {
		r->r_flgstr[i] = od_flgstr[i];
	}

	/* initialize DMA */
	dcp->dc_ddp = (struct dma_dev*) P_DISK_CSR;
	dcp->dc_queue.dq_head = c->c_dma_list;
	dma_init (dcp);

	return (0);
}

#if	STANDALONE
odattach (iop)
	struct iob *iop;
{
	struct mon_global *mg = restore_mg();
	register struct ctrl *c = (struct ctrl*) mg->mg_odc;
	register struct drive *d;
	register struct drive_info *di;
	register struct more_info *mi;
	int err;

	/* initialize drive info */
	d = (struct drive*) mg->mg_alloc (sizeof (*d));
	mg->mg_odd = (char*) d;
	bzero (d, sizeof (struct drive));
	d->d_drive = iop->i_unit;
	d->d_label = ALIGN(struct disk_label*, &d->d_un.d_lab);

	/* link everything together */
	d->d_ctrl = c;

	/* FIXME: somehow, determine drive type */
	d->d_drive_type = DRIVE_TYPE_OMD1;

	/* read label */
	if (err = od_read_label (d)) {
		if (d->d_flags & DF_SPINNING)
			printf ("no valid disk label found\n");
		return (err);
	}
	d->d_flags |= DF_ONLINE;
	iop->i_secsize = d->d_label->dl_secsize;
	return (0);
}
#else	STANDALONE

odattach (si, no_spin)
	struct sio *si;
{
	struct mon_global *mg = restore_mg();
	register struct ctrl *c = (struct ctrl*) mg->mg_odc;
	register struct drive *d;
	register struct drive_info *di;
	register struct more_info *mi;
	int err;

	/* initialize drive info */
	d = (struct drive*) mg->mg_alloc (sizeof (*d));
	mg->mg_odd = (char*) d;
	bzero (d, sizeof (struct drive));
	d->d_drive = si->si_unit;
	d->d_label = ALIGN(struct disk_label*, &d->d_un.d_lab);

	/* link everything together */
	d->d_ctrl = c;

	/* FIXME: somehow, determine drive type */
	d->d_drive_type = DRIVE_TYPE_OMD1;

	if (no_spin) {
		d->d_label->dl_secsize = 1;
		d->d_label->dl_nsect = 2;
		d->d_di = &od_drive_info[d->d_drive_type];
	} else {

		/* read label */
		if (err = od_read_label (d)) {
			if (d->d_flags & DF_SPINNING)
				printf ("no valid disk label found\n");
			return (err);
		}
	}

	d->d_flags |= DF_ONLINE;
	si->si_blklen = d->d_label->dl_secsize;
	return (0);
}
#endif	STANDALONE

odlabel_blkno (mg, si, size, i)
	struct mon_global *mg;
	struct sio *si;
{
	return (od_drive_info[0].di_label_blkno[i]);
}

#if	STANDALONE
odopen (iop)
	struct iob *iop;
{
	if (iop->i_unit < 0)
		iop->i_unit = 0;
	if (iop->i_part < 0)
		iop->i_part = 0;
	if (iop->i_part >= NPART)
		_stop("bad partition number");
	if (iop->i_ctrl < 0) {
		iop->i_ctrl = 0;
		iop->i_unit = 0;
	} else {
		if (iop->i_ctrl >= NOD)
			_stop("illegal od drive number");
	}
	if (odinit (P_DISK, iop->i_ctrl))
		return (-1);
	if (odattach (iop))
		return (-1);
	return (0);
}
#else	STANDALONE

odopen (mg, si, no_spin, silent, no_wait)
	struct mon_global *mg;
	struct sio *si;
{
	int e;

	if (si->si_ctrl != 0 || si->si_unit > 1) {
		printf ("bad ctrl or unit number\n");
		return (BE_INIT);
	}
	if (e = odinit (P_DISK, si->si_ctrl, silent, no_wait))
		return (e);
	if (e = odattach (si, no_spin))
		return (e);
	return (0);
}
#endif	STANDALONE

odclose()
{
}

#if	STANDALONE
#else	STANDALONE
odread (mg, si, lbn, ma, cc)
	struct mon_global *mg;
	struct sio *si;
	char *ma;
{
	od_cmd (OMD_READ, lbn, ma, cc, 0, 0, 0, 0, CF_PHYS);
	return (cc);
}

odwrite()
{
}
#endif	STANDALONE

#if	STANDALONE
odstrategy (iop, func)
	struct iob *iop;
{
	register struct mon_global *mg = restore_mg();
	register struct ctrl *c = (struct ctrl*) mg->mg_odc;
	register struct drive *d = (struct drive*) mg->mg_odd;
	int bno, ret;

	bno = iop->i_bn + d->d_label->dl_part[iop->i_part].p_base;
	bno *= iop->i_secsize / d->d_label->dl_secsize;

	if (func == READ) {
		if (od_cmd (OMD_READ, bno, iop->i_ma, iop->i_cc,
			0, 0, 0, 0, 0))
			return (-1);
		return (iop->i_cc);
	} else
		_stop ("odstrategy: illegal func");
	return (-1);
}
#endif	STANDALONE

od_strategy (bp, phys)
	register struct buf *bp;
{
	struct mon_global *mg = restore_mg();
	register struct ctrl *c = (struct ctrl*) mg->mg_odc;
	register struct drive *d = (struct drive*) mg->mg_odd;
	register struct disk_label *l;

	l = d->d_label;
	if (bp->b_bcount % l->dl_secsize) {
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR | B_DONE;
		return;
	}
	od_setup (c, d, bp, phys);
}

/* setup transfer parameters in od_ctrl from info in bp */
od_setup (c, d, bp, phys)
	register struct ctrl *c;
	register struct drive *d;
	register struct buf *bp;
{
	register struct disk_label *l;
	register nblk;

	l = d->d_label;
	c->c_blkno = bp->b_blkno;
	c->c_va = bp->b_un.b_addr;
	nblk = howmany (bp->b_bcount, l->dl_secsize);
	c->c_cmd = d->d_cmd;
	c->c_nsect = nblk;
	c->c_flags &= ~CF_PHYS;
	c->c_flags |= phys;
	bp->b_resid = bp->b_bcount - c->c_nsect * l->dl_secsize;
	c->c_retry = c->c_rtz = 0;
	od_fsm (c, d, FSM_START, bp);
}

char od_next_state[9] = {
	/* next */	/* fcmd */
	0,		/* 0 */
	FSM_VERIFY,	/* OMD_WRITE */
	FSM_WORKED,	/* OMD_READ */
	0,		/* 3 */
	FSM_WRITE,	/* OMD_ERASE */
	0,		/* 4 */
	0,		/* 5 */
	0,		/* 6 */
	FSM_WORKED,	/* OMD_VERIFY */
};

/*
 * Start or continue a transfer via finite state machine.
 *
 * Confusing terminology: because current optical drives
 * only have one media surface the term "track" is used instead
 * of "cylinder".  Also, "head" means which head mode is in effect
 * instead of which surface is being selected.
 *
 * The flag CF_CUR_POS is set in strategic places to indicate what disk
 * location is currently under the head.  It is reset on disk error or
 * any other action that allows the disk to spiral forward to an
 * unpredictable spot.  Be very careful how this flag is manipulated to
 * prevent redundant seeks or unnecessary sector timeouts.
 */
od_fsm (c, d, state, bp)
	register struct ctrl *c;
	register struct drive *d;
	register int state;
	struct buf *bp;
{
	struct mon_global *mg = restore_mg();
	struct bus_device *bd = d->d_bd;
	register struct disk_label *l = d->d_label;
	volatile register struct od_regs *r = c->c_od_regs;
	register struct od_err *e;
	register int htrack, dirty = 0, ht, ignore_bitmap;
	int track, rel_trk, max_ht, off, limit, i, pass, ag, status, rj_head;
	int use_rj;
	int norm_sect;				/* normalized sector # */
	int spht;
	int htpag;
	int g_usable;

	spht = l->dl_nsect >> 1;		/* sectors per half-track */
	htpag = l->dl_ag_alts / spht;      /* half-tracks per alt group */
	g_usable = l->dl_ag_size - l->dl_ag_alts;
	dbug_fsm ("[%d] ", state);
	c->c_fsm = state;
	c->c_flags &= ~(CF_ISSUE_D_CMD|CF_ISSUE_F_CMD);

	switch (state) {

	case FSM_START:
		c->c_retry_fsm = FSM_START;
		i = d->d_di->di_maxbcount / d->d_di->di_devblklen;
		c->c_xfer = MIN(i, c->c_nsect);

		/* if not phys I/O, skip over alternates */
		if ((c->c_flags & CF_PHYS) == 0) {
			off = c->c_blkno % g_usable;
			if (off >= l->dl_ag_off)
				off += l->dl_ag_alts;
			if (off < l->dl_ag_off)
				limit = l->dl_ag_off - off;
			else
				limit = l->dl_ag_size - off + l->dl_ag_off;
			c->c_xfer = MIN(c->c_xfer, limit);
			c->c_pblk = c->c_blkno/g_usable*l->dl_ag_size + off +
				l->dl_front;
		} else {
			c->c_pblk = c->c_blkno;
		}
forward:
		c->c_pblk += d->d_base;	/* skip reserved area */
		c->c_sect = c->c_pblk % l->dl_nsect;
		norm_sect = c->c_sect % spht;
#if	MULTI_SURFACE
		c->c_surface = c->c_pblk / l->dl_nsect;
		c->c_track = c->c_surface / l->dl_ntrack;    /* really cyl # */
		c->c_surface %= l->dl_ntrack;
		htrack = ((c->c_track - d->d_base / l->dl_nsect) /
			l->dl_ntrack) << 1;
#else	MULTI_SURFACE
		c->c_track = c->c_pblk / l->dl_nsect;	/* really cyl # */
		htrack = (c->c_track - d->d_base / l->dl_nsect) << 1;
#endif	MULTI_SURFACE
		if (c->c_sect >= spht)
			htrack |= 1;
		c->c_fcmd = c->c_cmd;

		ignore_bitmap = 1;
		dirty = 1;	/* can't tell if pre-erased */
cont:
		/* if any sector was previously written do an erase pass */
		/* FIXME: really erase a large, multi half-track request? */
		if (c->c_fcmd == OMD_WRITE && dirty)
			c->c_fcmd = OMD_ERASE;

		switch (c->c_fcmd) {

		case OMD_READ:
			c->c_head = OMD1_SRH;
			rj_head = RJ_READ;
			break;
#if 0
		case OMD_WRITE:
			c->c_head = OMD1_SWH;
			rj_head = RJ_WRITE;
			break;

		case OMD_ERASE:
			c->c_head = OMD1_SEH;
			rj_head = RJ_ERASE;
			break;

		case OMD_VERIFY:
			c->c_head = OMD1_SVH;
			rj_head = RJ_VERIFY;
			break;
#endif
		case OMD_SPINUP:
			if ((i = od_status (c, d, r, OMD1_RDS,
			    SPIN | DONT_INTR)) == -1)
				goto failed;
			if ((i & (1 << (E_EMPTY - E_STATUS_BASE + 1))) != 0) {
				c->c_err = E_EMPTY;
				c->c_flags |= CF_ERR_VALID;
				goto failed;
			}
			c->c_dcmd = OMD1_RID;
			c->c_flags |= CF_ISSUE_D_CMD;
			od_issue_cmd (c, d);
			c->c_next_fsm = FSM_INSERT_RID;
			odintr (c, d, bp);
			return;

		case OMD_EJECT:
			od_status (c, d, r, OMD1_RDS, SPIN | DONT_INTR);
			od_status (c, d, r, OMD1_RID, SPIN | DONT_INTR);
			c->c_dcmd = OMD1_EC;
			c->c_flags |= CF_ISSUE_D_CMD;
			od_issue_cmd (c, d);
			c->c_next_fsm = FSM_EJECT;
			odintr (c, d, bp);
			return;

		default:
			c->c_err = E_CMD;
			c->c_flags |= CF_ERR_VALID;
			goto failed;
		}

		/* make sure we are spiraling */
		if ((d->d_flags & DF_SPIRALING) == 0) {
			c->c_dcmd = OMD1_SOO;
			c->c_flags |= CF_ISSUE_D_CMD;
			c->c_flags &= ~CF_ISSUE_F_CMD;

			/* resume at current state */
			c->c_next_fsm = c->c_fsm;
			d->d_flags |= DF_SPIRALING;
			break;
		}

		/* select head mode & seek to track */
		track = c->c_track;
		use_rj = 0;
		if (1) {
			track -= 1;	/* land before first half-track */
			if (c->c_flags & CF_50_50_SEEK) {
				use_rj = 1;
			}
			c->c_flags &= ~CF_50_50_SEEK;
		} else {
			c->c_flags |= CF_50_50_SEEK;
		}

			/* HOS should complete almost immediately */
			dbug_xfer ("HOS %d c_track %d ",
				track>>12, c->c_track);
			od_drive_cmd (c, d, OMD1_HOS | (track >> 12),
				SPIN|DONT_INTR);

			/* select head before seeking */
			c->c_seek_track = track;
			c->c_dcmd = c->c_head;
			c->c_flags |= CF_ISSUE_D_CMD;
			c->c_flags &= ~CF_ISSUE_F_CMD;
			c->c_next_fsm = FSM_SEEK;
		break;

	case FSM_SEEK:
		c->c_dcmd = OMD1_SEK | (c->c_seek_track & 0xfff);
		c->c_flags |= CF_ISSUE_D_CMD | CF_ISSUE_F_CMD;
		c->c_next_fsm = od_next_state[c->c_fcmd];
		break;
#if 0
	case FSM_WRITE:
		/* detect erase-only requests */
		if (c->c_cmd == OMD_ERASE)
			goto worked;
		c->c_fcmd = OMD_WRITE;
		c->c_flags |= CF_ISSUE_F_CMD;
		c->c_next_fsm = od_next_state[c->c_fcmd];
		c->c_retry_fsm = FSM_WRITE_RETRY;
		goto cont;

	case FSM_VERIFY:
		c->c_fcmd = OMD_VERIFY;
		c->c_flags |= CF_ISSUE_F_CMD;
		c->c_next_fsm = od_next_state[c->c_fcmd];
		c->c_retry_fsm = FSM_WRITE_RETRY;
		goto cont;

	case FSM_WRITE_RETRY:
		c->c_fcmd = OMD_WRITE;
		c->c_flags |= CF_ISSUE_F_CMD;
		c->c_next_fsm = od_next_state[c->c_fcmd];
		c->c_retry_fsm = FSM_WRITE_RETRY;
		dirty = 1;
		goto cont;
#endif
	case FSM_ERR_DONE:
		c->c_flags &= ~CF_ERR_INPROG;
		od_fsm (c, d, c->c_err_fsm_save, bp);
		return;

	case FSM_ATTN_RID:
		return (OD_ERR);

	case FSM_INSERT_RID:
		i = od_status (c, d, r, OMD1_RDS, SPIN | DONT_INTR);
		dbug_status ("{0x%x} ", i);
		if ((i & (1 << (E_STOPPED - E_STATUS_BASE + 1))) == 0)
			goto spinning;		/* already spinning */
		od_drive_cmd (c, d, OMD1_STM, SPIN | DONT_INTR);
		c->c_next_fsm = FSM_INSERT_STM;
		odintr (c, d, bp);
		return (OD_BUSY);

	case FSM_INSERT_STM:
spinning:
		/* can be called from od_cmd also */
		if ((c->c_flags & CF_ATTN_ASYNC) == 0)
			goto worked;	/* FIXME: what if it failed? */

		/* attach will happen on first open */
		c->c_flags &= ~(CF_ATTN_ASYNC | CF_ASYNC_INPROG);
		bp->b_flags |= B_DONE;
		return (OD_DONE);

	case FSM_EJECT:
		d->d_flags &= ~(DF_ONLINE | DF_SPINNING);
		c->c_flags &= ~CF_ERR_INPROG;
		if (c->c_flags & CF_FAIL_IO) {
			c->c_flags &= ~CF_FAIL_IO;
			goto failed;
		}
		goto worked;

	case FSM_WORKED:
worked:
		c->c_nsect -= c->c_xfer;

		/* restart fragmented requests (crossing alternates, etc.) */
		if (c->c_nsect > 0) {
			c->c_va += c->c_xfer * l->dl_secsize;
			c->c_blkno += c->c_xfer;
			od_fsm (c, d, FSM_START, bp);
			return;
		}
		bp->b_flags |= B_DONE;
		return;

	case FSM_FAILED:
failed:
		od_perror (c, EF_FAILED);
		bp->b_error = c->c_err;
		bp->b_flags |= B_ERROR | B_DONE;
		return;
	}

	/* setup sector info */
	r->r_track_h = (c->c_track >> 8) & 0xff;
	r->r_track_l = c->c_track & 0xff;

	/* sector increment = 1 */
	r->r_incr_sect = (1 << 4) | c->c_sect;

	/* c_xfer = 256 will set r_seccnt = 0 */
	r->r_seccnt = c->c_xfer;
	dbug_where ("<%d:%d:%d> ",
		c->c_track, c->c_sect, c->c_xfer);

	od_issue_cmd (c, d);	/* will spin until complete */
	odintr (c, d, bp);
}

odintr (c, d, bp)
	register struct ctrl *c;
	register struct drive *d;
	register struct buf *bp;
{
	register struct od_err *e;
	volatile register struct od_regs *r = c->c_od_regs;
	register int i, disr, desr, bno, err;

	disr = r->r_disr;
	desr = r->r_desr;
	r->r_disr = OMD_CLR_INT;

	/*
	 * Determine error code.  Since all error bits are presented
	 * in parallel we scan them in priority order and report the
	 * first error encountered.  FIXME: is there a better way?
	 * FIXME: this doesn't work at all if there are coupled error bits!
	 */
	err = 0;
	if (c->c_flags & CF_SOFT_TIMEOUT) {
		err = E_SOFT_TIMEOUT;
		c->c_flags &= ~CF_SOFT_TIMEOUT;
	} else
	if (c->c_flags & CF_DMA_ERROR) {
		err = E_DMA_ERROR;
		c->c_flags &= ~CF_DMA_ERROR;
	} else
	if (disr & OMD_DATA_ERR) {
		if (desr & OMD_ERR_STARVE)
			err = E_STARVE;
		else
		if (desr & OMD_ERR_TIMING)
			err = E_TIMING;
		else
		if (desr & OMD_ERR_CMP)
			err = E_CMP;
		else
		if (desr & OMD_ERR_ECC)
			err = E_ECC;
	} else
	if (disr & OMD_TIMEOUT) {

		/* FIXME: spiraling may have been lost */
		d->d_flags &= ~DF_SPIRALING;
		err = E_TIMEOUT;
		dbug_timeout ("T/O @ %d:%d wanted %d:%d ",
			(((u_short)r->r_track_h) << 8) | r->r_track_l,
			 r->r_incr_sect, c->c_track, c->c_sect);
	} else
	if (disr & OMD_READ_FAULT)
		err = E_FAULT;
	else
	if (disr & OMD_PARITY_ERR)
		err = E_I_PARITY;

	if (err && (c->c_flags & CF_ERR_VALID) == 0) {
		c->c_err = err;
		c->c_flags |= CF_ERR_VALID;
	}

#if 1
	/* clear formatter (only really needed on ECC errors) */
	r->r_cntrl1 = 0;
#endif

	od_attn (c, d, r, disr, bp);
	
	/* certain errors take precedence over others */
	if (c->c_rds & (1 << (E_SIDE - E_STATUS_BASE + 1)))
		c->c_err = E_SIDE;

	/* resume an error recovery operation */
	/* FIXME: what do we do about errors during error recovery? */
	if (c->c_flags & (CF_ERR_INPROG | CF_ATTN_INPROG)) {
		od_fsm (c, d, c->c_next_fsm, bp);
		goto out;
	}

	if ((c->c_flags & CF_ERR_VALID) == 0) {

		/* command completed successfully -- advance state machine */
		c->c_flags &= ~CF_NEED_DMA;
		od_fsm (c, d, c->c_next_fsm, bp);
		goto out;
	}

	/* process errors */
	dbug_intr ("*FAILED* %d ", c->c_err);

	/* abort the DMA if there was an error */
	if (c->c_flags & CF_NEED_DMA) {
		dma_abort (&c->c_dma_chan);
		c->c_flags &= ~(CF_DMAINT_EXP|CF_NEED_DMA);
	}

	c->c_flags &= ~CF_ERR_VALID;
	switch ((e = &od_err[c->c_err])->e_action) {

	case EA_RETRY:

		/* abort a testing operation */
		/* FIXME: ignore first sector timeout for now */
		if (c->c_flags & CF_TESTING &&
		    (c->c_err != E_TIMEOUT || c->c_retry)) {
			od_perror (c, EF_FAILED);
			c->c_flags |= CF_ERR_VALID;
			od_fsm (c, d, c->c_fsm_test, bp);
			goto out;
		}
		if (c->c_retry++ >= (d->d_flags & DF_SPEC_RETRY?
		    d->d_retry : e->e_retry)) {
			c->c_retry = 0;
			if (c->c_rtz++ >= (d->d_flags & DF_SPEC_RETRY?
			    d->d_rtz : e->e_rtz)) {
				od_fsm (c, d, FSM_FAILED, bp);
				/* FIXME: go offline if unit not ready? */
				/* or is this obviated by eject capability? */
				break;
			} else {
				/* issue recalibrate */
				c->c_flags |= CF_ERR_INPROG;
				od_perror (c, EF_RTZ);
				od_drive_cmd (c, d, OMD1_REC, DONT_INTR|SPIN);
				c->c_err_fsm_save = c->c_retry_fsm;
				c->c_next_fsm = FSM_ERR_DONE;
				d->d_flags &= ~DF_SPIRALING;
				od_fsm (c, d, c->c_next_fsm, bp);
				goto out;
			}
		}

		/* retry the original command */
		if (c->c_err != E_TIMEOUT ||
		    (c->c_flags & CF_50_50_SEEK) == 0)
			od_perror (c, EF_RETRY);
		od_fsm (c, d, c->c_retry_fsm, bp);
		break;

	case EA_RTZ:

		/* must RTZ immediately otherwise subsequent cmds will fail */
		c->c_flags |= CF_ERR_INPROG;
		od_perror (c, EF_RTZ);
		od_drive_cmd (c, d, OMD1_REC, DONT_INTR|SPIN);
		d->d_flags &= ~DF_SPIRALING;
		if (c->c_retry > e->e_retry) {
			c->c_err_fsm_save = FSM_FAILED;
		} else
			c->c_err_fsm_save = c->c_retry_fsm;
		c->c_retry++;
		c->c_next_fsm = FSM_ERR_DONE;
		break;

	case EA_EJECT:

		/* nothing we can do but eject the disk and go offline */
		c->c_flags |= CF_ERR_INPROG;
		od_drive_cmd (c, d, OMD1_EC, DONT_INTR|SPIN);
		c->c_flags |= CF_FAIL_IO;
		c->c_next_fsm = FSM_EJECT;
		od_fsm (c, d, c->c_next_fsm, bp);
		break;

	default:
		bp->b_flags |= B_DONE | B_ERROR;
	}
out:
	c->c_flags &= ~CF_CANT_SLEEP;
}

od_issue_cmd (c, d)
	register struct ctrl *c;
	register struct drive *d;
{
	struct mon_global *mg = restore_mg();
	volatile register struct od_regs *r = c->c_od_regs;
	register struct dma_chan *dcp = &c->c_dma_chan;
	register int direction, i;

	dbug_flags ("issue: c_flags 0x%x ", c->c_flags);
	if (c->c_flags & CF_ISSUE_F_CMD) {
		if (c->c_fcmd & (OMD_ECC_READ | OMD_ECC_WRITE |
		    OMD_READ | OMD_WRITE)) {
			c->c_flags |= CF_NEED_DMA;
			if (c->c_fcmd & (OMD_ECC_READ|OMD_READ))
				direction = DMACSR_READ;
			else
				direction = DMACSR_WRITE;

			/* start DMA */
			dbug_dma ("DMA va 0x%x size %d ",
				c->c_va, c->c_xfer*d->d_label->dl_secsize);
			dma_list (dcp, c->c_dma_list, c->c_va, c->c_xfer *
				d->d_label->dl_secsize, direction);

			/* FIXME: set tail pointer in dcp? (unused by dma) */
			dma_enable (dcp, direction);
		}
	}

	if (c->c_flags & CF_ISSUE_D_CMD) {
		if (od_drive_cmd (c, d, c->c_dcmd, DONT_INTR |
		    (c->c_flags & CF_ISSUE_F_CMD? DONT_SPIN : SPIN)) < 0)
			return (-1);
	} else {
		/* select drive */
		r->r_cntrl2 = OMD_SECT_TIMER | d->d_drive;
	}

	/* formatter commands can be issued after drive command issues */
	if (c->c_flags & CF_ISSUE_F_CMD) {
		r->r_cntrl1 = 0;	/* reset formatter first */
		r->r_cntrl1 |= c->c_fcmd;
		if (c->c_fcmd & (OMD_READ | OMD_ECC_READ)) {

			/* use DMA complete on reads */
			while ((dcp->dc_ddp->dd_csr & DMACSR_COMPLETE) == 0) {

				/* detect drive errors */
				if (r->r_disr & OMD_ENA_INT)
					return (-1);
				km_power_check();
			}
		} else {

			/* else use operation complete */
			while ((r->r_dimr & OMD_OPER_COMPL) == 0)
				km_power_check();
		}
		
	}
	return (0);
}

od_drive_cmd (c, d, cmd, flags)
	register struct ctrl *c;
	register struct drive *d;
{
	struct mon_global *mg = restore_mg();
	volatile register struct od_regs *r = c->c_od_regs;
	register int i, j, s;

	/* select drive */
	r->r_cntrl2 = OMD_SECT_TIMER | d->d_drive;

#define	TOUT	(8*1024*1024)
	for (i = 1, j = 1; i <= TOUT && (r->r_disr & OMD_CMD_COMPL) == 0; i++)
		if ((i % j) == 0) {
#ifdef	notdef
			printf ("od_drive_cmd: %d busy1? disr 0x%x c1 0x%x\n",
				i, r->r_disr, r->r_cntrl1);
#endif
			r->r_cntrl1 = 0;
			j <<= 1;
		}
	if (i > TOUT) {
		c->c_err = E_BUSY_TIMEOUT1;
		c->c_flags |= CF_ERR_VALID;
		c->c_timeout = 0;
		return (-1);
	}
	c->c_last_dcmd = cmd;
	r->r_csr_h = cmd >> 8;
	r->r_csr_l = cmd & 0xff;	/* triggers on write of low byte */
	for (i = 1, j = 1; i <= TOUT && (r->r_disr & OMD_CMD_COMPL) == 1; i++)
		if ((i % j) == 0) {
#ifdef	notdef
			printf ("od_drive_cmd: %d busy2? disr 0x%x c1 0x%x\n",
				j, r->r_disr, r->r_cntrl1);
#endif
			r->r_cntrl1 = 0;
			j <<= 1;
		}
	if (i > TOUT) {
		c->c_err = E_BUSY_TIMEOUT2;
		c->c_flags |= CF_ERR_VALID;
		c->c_timeout = 0;
		return (-1);
	}

	while (flags & INTR)
		;

	if (flags & SPIN)
		while ((r->r_disr & OMD_CMD_COMPL) == 0)
			km_power_check();

	return (0);
}

od_perror (c, errtype)
	register struct ctrl *c;
	register int errtype;
{
	register struct od_err *e = &od_err[c->c_err];
	register int i;

	if (errtype >= od_errmsg_filter) {
		printf ("od%d%c: %s %s ",
			OD_DRIVE(0), OD_PART(0) + 'a',
			c->c_cmd == OMD_READ? "read" :
			c->c_cmd == OMD_WRITE? "write" :
			c->c_cmd == OMD_ERASE? "erase" : "command",
			od_errtype[errtype]);
		if (e->e_msg)
			printf ("(%s)", e->e_msg);
		else
			printf ("(error #%d)", c->c_err);
		printf (" %d:0:%d\n", c->c_track, c->c_sect);
	}
}

/*
 * Check for attention condition.
 * It will take another interrupt or two to get the drive status.
 * Try a "read drive status" then "read extended status"
 * and finally "read hardware status" to find the error.
 * Return value:
 *	OD_BUSY = attention pending, more status info on next interrupt
 *	OD_DONE = no attention pending
 *	OD_ERR = error found
 * FIXME: do we handle errors in the attention gathering commands properly?
 */
od_attn (c, d, r, disr, flag, bp)
	register struct ctrl *c;
	register struct drive *d;
	volatile register struct od_regs *r;
	register int disr;
{
	register int i;
	register struct od_err *e;

	/* ATTN forces spiraling off */
	if (disr & OMD_ATTN)
		d->d_flags &= ~DF_SPIRALING;

	if ((disr & OMD_ATTN) == 0) {
		c->c_rds = c->c_res = c->c_rhs = 0;
		return (OD_DONE);
	}

	c->c_res = c->c_rhs = 0;
	c->c_rds = od_status (c, d, r, OMD1_RDS, SPIN | DONT_INTR);
	if ((c->c_flags & CF_ERR_VALID) == 0 && (c->c_rds & 0xfffe)) {
		for (i = 15; i > 0; i--)
			if (c->c_rds & (1 << i)) {
				e = &od_err[E_STATUS_BASE+i-1];
				if (e->e_action != EA_ADV) {
					c->c_err = E_STATUS_BASE +
						i - 1;
					c->c_flags |= CF_ERR_VALID;
					dbug_status ("ERR %d ",
						c->c_err);
					break;
				}
			}
	}
	if ((c->c_rds & 1) == 0)
		goto attn_rid;
	c->c_res = od_status (c, d, r, OMD1_RES, SPIN | DONT_INTR);
	if ((c->c_flags & CF_ERR_VALID) == 0 && (c->c_res & 0xfffe)) {
		for (i = 15; i > 0; i--)
			if (c->c_res & (1 << i)) {
				e = &od_err[E_EXTENDED_BASE+i-1];
				if (e->e_action != EA_ADV) {
					c->c_err = E_EXTENDED_BASE +
						i - 1;
					c->c_flags |= CF_ERR_VALID;
					dbug_status ("-ERR- %d ",
						c->c_err);
					break;
				}
			}
	}
	if ((c->c_res & 1) == 0)
		goto attn_rid;
	c->c_rhs = od_status (c, d, r, OMD1_RHS, SPIN | DONT_INTR);
		if ((c->c_flags & CF_ERR_VALID) == 0 && c->c_rhs) {
			for (i = 15; i >= 0; i--)
				if (c->c_rhs & (1 << i)) {
					e = &od_err[E_HARDWARE_BASE+i];
					if (e->e_action != EA_ADV) {
						c->c_err = E_HARDWARE_BASE+i;
						c->c_flags |= CF_ERR_VALID;
						dbug_status ("-ERR- %d ",
							c->c_err);
						break;
					}
				}
		}
attn_rid:
	/* RID must be issued to reset status bits and ATTN signal */
	od_drive_cmd (c, d, OMD1_RID, SPIN | DONT_INTR);
	return (OD_DONE);
}

od_status (c, d, r, cmd, flags)
	register struct ctrl *c;
	register struct drive *d;
	volatile register struct od_regs *r;
{
	register int s;

	od_drive_cmd (c, d, cmd, DONT_SPIN | (flags & (INTR | DONT_INTR)));
	delay (40);
	r->r_cntrl1 = 0;	/* reset formatter first */
	r->r_cntrl1 |= OMD_RD_STAT;
	if (flags & SPIN) {
		for (s = 1; s <= TOUT && (r->r_disr & OMD_CMD_COMPL) == 0;)
			s++;
		if (s > TOUT) {
			c->c_err = E_BUSY_TIMEOUT1;
			c->c_flags |= CF_ERR_VALID;
			c->c_timeout = 0;
			return (-1);
		}
		return ((r->r_csr_h << 8) | r->r_csr_l);
	}
#undef	TOUT
}

od_cmd (cmd, bno, bufp, bytes, err, dc, de, use_bitmap, phys)
	u_short cmd;
	char *bufp;
	char *err;
	struct dr_cmdmap *dc;
	struct dr_errmap *de;
{
	struct mon_global *mg = restore_mg();
	register struct drive *d = (struct drive*) mg->mg_odd;
	register struct disk_label *l = d->d_label;
	register struct buf *bp = &d->d_cbuf;
	register int s, bcount;

	bp->b_flags = B_BUSY | B_READ;
	d->d_cmd = cmd;
	if (use_bitmap == 0)
		d->d_flags |= DF_IGNORE_BITMAP;
	bp->b_blkno = bno;
	bcount = howmany (bytes, l->dl_secsize) * l->dl_secsize;
	bp->b_bcount = bcount;
	bp->b_un.b_addr = bufp;
	od_strategy (bp, phys);

	while ((bp->b_flags & B_DONE) == 0)
		km_power_check();
	d->d_flags &= ~(DF_SPEC_RETRY | DF_IGNORE_BITMAP);
	bp->b_flags &= ~B_BUSY;
	if (bp->b_flags & B_ERROR) {
		if (err)
			*err = bp->b_error;
		return (EIO);
	}
	return (0);
}

od_read_label (d)
	register struct drive *d;
{
	register struct mon_global *mg = restore_mg();
	register int i, blkno, dk, bytes;
	register struct disk_label *l = d->d_label;
	register struct drive_info *di;
	register struct more_info *mi;
	int e;
	char err;

	/* search for label */
	d->d_di = di = &od_drive_info[d->d_drive_type];
	mi = &od_more_info[d->d_drive_type];
	for (i = 0; i < NLABELS; i++) {
		if ((blkno = di->di_label_blkno[i]) == -1)
			continue;

		/* fake parameters actually contained in the label */
		bzero (l, sizeof (struct disk_label));
		d->d_base = mi->mi_reserved_cyls * mi->mi_nsect;
		l->dl_secsize = di->di_devblklen;
		l->dl_nsect = mi->mi_nsect;
		l->dl_ntrack = 1;

		/* spin up drive if necessary */
		if ((d->d_flags & DF_SPINNING) == 0) {
			e = od_cmd (OMD_SPINUP,
				0, 0, 0, &err, 0, 0, 0, CF_PHYS);
			if (e == EIO && (err == E_BUSY_TIMEOUT1 ||
			    err == E_BUSY_TIMEOUT2))
				return (BE_NODEV);
			if (e == EIO && err == E_SIDE)
				return (BE_FLIP);
			if (e == EIO && err == E_EMPTY)
				return (BE_INSERT);
			if (e)
				continue;
		}
		d->d_flags |= DF_SPINNING;
		e = od_cmd (OMD_READ, blkno, l, sizeof *l, &err,
			0, 0, 0, CF_PHYS);
		if (e == EIO && err == E_SIDE)
			return (BE_FLIP);
		if (e == EIO && err == E_EMPTY)
			return (BE_INSERT);
		if (e)
			continue;
		if (od_validate_label (l, blkno))
			continue;
		return (0);
	}
	return (BE_INIT);
}

od_validate_label (l, blkno)
	register struct disk_label *l;
{
	u_short ccksum, cksum(), ck, *dl_cksum, size;

	if (l->dl_version == DL_V1 || l->dl_version == DL_V2) {
		size = sizeof (struct disk_label);
		dl_cksum = &l->dl_checksum;
	} else
	if (l->dl_version == DL_V3) {
		size = sizeof (struct disk_label) - sizeof (l->dl_un);
		dl_cksum = &l->dl_v3_checksum;
	} else {
		dbug_valid ("bad version code 0x%x\n", l->dl_version);
		return (-1);
	}
	if (l->dl_label_blkno != blkno) {
		dbug_valid ("blkno 0x%x sb 0x%x\n", l->dl_label_blkno, blkno);
		return (-1);	/* label not where it's supposed to be */
	}
	ccksum = *dl_cksum;
	*dl_cksum = 0;
	if ((ck = cksum (l, size)) != ccksum) {
		dbug_valid ("cksum 0x%x sb 0x%x blk %d\n", ck, ccksum, blkno);
		return (-1);
	}
	return (0);
}

od_eject (drive, no_wait)
{
	struct mon_global *mg = restore_mg();
	struct sio sio, *si = &sio;
	struct nvram_info *ni = &mg->mg_nvram;

	if (mg->mg_machine_type != NeXT_CUBE && mg->mg_machine_type != NeXT_X15)
		return (-1);
	if (ni->ni_hw_pwd == HW_PWD && ni->ni_allow_eject == 0 && ni->ni_any_cmd == 0)
		return (-1);
	si->si_ctrl = 0;
	si->si_unit = drive;
	if (odopen (mg, si, 1, 0, no_wait))
		return (-1);
	od_cmd (OMD_EJECT, 0, 0, 0, 0, 0, 0, 0, CF_PHYS);
	return (0);
}

od_reset()
{
	struct mon_global *mg = restore_mg();
	volatile register struct od_regs *r = (struct od_regs*) P_DISK;

	if (mg->mg_machine_type != NeXT_CUBE && mg->mg_machine_type != NeXT_X15)
		return;
	r->r_disr = OMD_RESET;
	delay (100);
	r->r_disr = 0;
}

od_inserted()
{
	struct mon_global *mg = restore_mg();
	struct sio sio, *si = &sio;
	register struct ctrl *c;
	register struct drive *d;
	volatile register struct od_regs *r;
	int i;

	if (mg->mg_machine_type != NeXT_CUBE && mg->mg_machine_type != NeXT_X15)
		return (0);
	si->si_ctrl = 0;
	si->si_unit = 0;
	if (odopen (mg, si, 1, 1, 1))
		return (0);
	c = (struct ctrl*) mg->mg_odc;
	d = (struct drive*) mg->mg_odd;
	r = c->c_od_regs;
	if ((i = od_status (c, d, r, OMD1_RDS, SPIN | DONT_INTR)) == -1)
		return (0);
	if ((i & (1 << (E_EMPTY - E_STATUS_BASE + 1))) != 0)
		return (0);
	return (1);
}


