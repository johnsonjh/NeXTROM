/*	@(#)sd.c	1.0	09/10/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 *
 * sd.c -- implementation of disk specific scsi routines
 * PROM MONITOR VERSION
 *
 * Supports scsi disks meeting mandatory requirements of
 * ANSI X3.131.
 *
 * HISTORY
 * 24-Feb-88   Mike DeMoney (mike) at NeXT
 *	Added stuff to support read capacity, start command, and
 *	test unit ready.  Also added retry for sdread() and allowed
 *	for explicit statement of desired target.
 * 10-Sept-87  Mike DeMoney (mike) at NeXT
 *	Created.
 *
 **********************************************************************
 */ 

#include "sys/param.h"
#include <mon/global.h>
#include <mon/sio.h>
#include <nextdev/dma.h> 
#include "scsireg.h"
#include "scsivar.h"

/* #define	SD_DEBUG	1		/* */
/* #define 	SD_DEBUG1 	1		/* */
int sdopen();
int sdclose();
int sdread();
int sdwrite();
int sdlabel_blkno();

#define	MAX_SDTRIES	3
#define SDOPEN_TRIES	10

struct device scsi_disk =
	{ sdopen, sdclose, sdread, sdwrite, sdlabel_blkno, D_IOT_PACKET };

/*
 * private per-device sd info
 * this struct parallels scsi_device and bus_device structs
 */
struct scsi_disk_device {
	struct	scsi_device sdd_sd;	/* parallel scsi_device */
	union {
		char Sdd_irbuf[sizeof(struct inquiry_reply)
			       + DMA_BEGINALIGNMENT - 1];
		char Sdd_crbuf[sizeof(struct capacity_reply)
			       + DMA_BEGINALIGNMENT - 1];
		char Sdd_erbuf[sizeof(struct esense_reply)
			       + DMA_BEGINALIGNMENT - 1];
	} sdd_replybuf;
};

static int sdfind();
static int sdslave();
static struct capacity_reply *sdblklen();
static void sdinit();
static void sdreqsense();
static int sdcmd();
static void sdfail();

sdopen(mg, si)
struct mon_global *mg;
struct sio *si;
{
	struct scsi_disk_device *sddp;
	int target, lun, tries, ctrl;
	struct capacity_reply *crp;

	sddp = (struct scsi_disk_device *)
	    mon_alloc(sizeof(*sddp));
	mg->mg_sddp = (char *)sddp;
	scsetup(mg, si);
	tries = 0;
	if (si->si_args == SIO_SPECIFIED) {
		ctrl = si->si_ctrl;
		lun = si->si_unit;
	} else {
		ctrl = 0;
		lun = 0;
	}
	do {
		target = sdfind(mg, si, ctrl, lun);
		km_power_check();
	} while ((target < 0) && 
	         (++tries < SDOPEN_TRIES) &&
		 (sddp->sdd_sd.sd_state != SDSTATE_RESET));
	if (target < 0) {
		if(sddp->sdd_sd.sd_state == SDSTATE_RESET)
			printf("SCSI Bus Hung\n");
		else
			printf("no SCSI disk\n");
		sdclose(mg, si);
		return(BE_NODEV);
	} else
		printf("booting SCSI target %d, lun %d\n", target, lun);
	sddp->sdd_sd.sd_target = target;
	sddp->sdd_sd.sd_lun = lun;

	sdinit(mg, si);
	crp = sdblklen(mg, si);
	if (crp == 0 || crp->cr_blklen == 0) {
		printf("dev blk len?\n");
		sdclose(mg, si);
		return(BE_NODEV);
	}
	si->si_blklen = crp->cr_blklen;
	si->si_lastlba = crp->cr_lastlba;
#ifdef notdef
	printf("%d byte sectors\n", si->si_blklen);
#endif notdef
	return(0);
}

sdclose(mg, si)
struct mon_global *mg;
struct sio *si;
{
}

static int
sdfind(mg, si, ctrl, lun)
struct mon_global *mg;
struct sio *si;
{
	int target, logical_target = -1;
	struct scsi_disk_device *sddp = (struct scsi_disk_device *)mg->mg_sddp;

	for (target = 0; target < 7; target++) {
		if (sdslave(mg, si, target, lun)) 
			logical_target++;
		else {
			/* if bus is hung, give up now */
			if(sddp->sdd_sd.sd_state == SDSTATE_RESET)
				return(-1);
		}
		if (logical_target == ctrl)
			return(target);
	}
	return(-1);
}

static int
sdslave(mg, si, target, lun)
struct mon_global *mg;
struct sio *si;
int target, lun;
{
	struct scsi_disk_device *sddp = (struct scsi_disk_device *)mg->mg_sddp;
	struct scsi_device *sdp = &sddp->sdd_sd;
	struct cdb_6 *c6p = &sdp->sd_cdb.cdb_c6;
	struct inquiry_reply *irp;
	int i;
#ifdef	SD_DEBUG1
	printf("sdslave(): target %d\n",target);
#endif	SD_DEBUG1
	irp = DMA_ALIGN(struct inquiry_reply *, &sddp->sdd_replybuf);

	for (i = 0; i < MAX_SDTRIES; i++) {
		/* Do an INQUIRE cmd */
		bzero(c6p, sizeof(*c6p));
		c6p->c6_opcode = C6OP_INQUIRY;
		c6p->c6_lun = lun;
		c6p->c6_len = sizeof(struct inquiry_reply);
		c6p->c6_ctrl = CTRL_NOLINK;
		sdp->sd_target = target;
		sdp->sd_lun = lun;
		sdp->sd_addr = (caddr_t)irp;
		sdp->sd_bcount = sizeof(struct inquiry_reply);
		sdp->sd_read = 1;
		if (sdcmd(mg, si, sdp) && irp->ir_devicetype == DEVTYPE_DISK)
			return(1);
		/*
		 * if bus is hung, give up now 
		 */
		if(sdp->sd_state == SDSTATE_RESET) {
#ifdef	SD_DEBUG1	
			printf("sdslave detected reset\n");
#endif	SD_DEBUG1
			return(0);
		}
	}
	return(0);
}

static struct capacity_reply
*sdblklen(mg, si)
struct mon_global *mg;
struct sio *si;
{
	struct scsi_disk_device *sddp = (struct scsi_disk_device *)mg->mg_sddp;
	struct scsi_device *sdp = &sddp->sdd_sd;
	struct cdb_10 *c10p = &sdp->sd_cdb.cdb_c10;
	struct capacity_reply *crp;
	int i;

	crp = DMA_ALIGN(struct capacity_reply *, &sddp->sdd_replybuf);

	for (i = 0; i < MAX_SDTRIES; i++) {
		/* Do a READ CAPACITY cmd */
		bzero(c10p, sizeof(*c10p));
		c10p->c10_opcode = C10OP_READCAPACITY;
		c10p->c10_lun = sdp->sd_lun;
		c10p->c10_ctrl = CTRL_NOLINK;
		sdp->sd_addr = (caddr_t)crp;
		sdp->sd_bcount = sizeof(struct capacity_reply);
		sdp->sd_read = 1;
		if (sdcmd(mg, si, sdp))
			return(crp);
	}
	sdfail(mg, si, sdp, "READ CAPACITY");
	return(0);
}

static void
sdreqsense(mg, si)
struct mon_global *mg;
struct sio *si;
{
	struct scsi_disk_device *sddp = (struct scsi_disk_device *)mg->mg_sddp;
	struct scsi_device *sdp = &sddp->sdd_sd;
	struct cdb_6 *c6p = &sdp->sd_cdb.cdb_c6;
	struct esense_reply *erp;
	int i;

	erp = DMA_ALIGN(struct esense_reply *, &sddp->sdd_replybuf);

	for (i = 0; i < MAX_SDTRIES; i++) {
		/* Do a REQUEST SENSE cmd */
		bzero(c6p, sizeof(*c6p));
		c6p->c6_opcode = C6OP_REQSENSE;
		c6p->c6_lun = sdp->sd_lun;
		c6p->c6_len = sizeof(*erp);
		c6p->c6_ctrl = CTRL_NOLINK;
		sdp->sd_addr = (caddr_t)erp;
		sdp->sd_bcount = sizeof(struct esense_reply);
		sdp->sd_read = 1;
		if (scpollcmd(mg, si, sdp))
			return;
		if (sdp->sd_state == SDSTATE_COMPLETED
		    && sdp->sd_status == STAT_BUSY)
			delay(1000000);		/* 1 sec */
	}
	sdfail(mg, si, sdp, "REQ SENSE");
}

static void
sdinit(mg, si)
struct mon_global *mg;
struct sio *si;
{
	struct scsi_disk_device *sddp = (struct scsi_disk_device *)mg->mg_sddp;
	struct scsi_device *sdp = &sddp->sdd_sd;
	struct cdb_6 *c6p = &sdp->sd_cdb.cdb_c6;
	struct cdb_10 *c10p = &sdp->sd_cdb.cdb_c10;
	int tries, i;

	for (i = 0; i < MAX_SDTRIES; i++) {
		/* Do a START UNIT cmd */
		bzero(c6p, sizeof(*c6p));
		c6p->c6_opcode = C6OP_STARTSTOP;
		c6p->c6_lun = sdp->sd_lun;
		c6p->c6_lba = (1<<16);	/* IMMEDIATE bit */
		c6p->c6_len = 1;	/* START bit */
		c6p->c6_ctrl = CTRL_NOLINK;
		sdp->sd_bcount = 0;
		if (sdcmd(mg, si, sdp))
			break;
	}


	/* Do TEST UNIT READY cmds until success */
	tries = 0;
	do {
		if (tries) {
			if (tries < 0) {
				printf("waiting for drive to come ready");
				tries = 1;
			} else {
				printf(".");
				tries++;
			}
			delay(1000000);		/* 1 sec */
		} else
			tries = -1;
		bzero(c6p, sizeof(*c6p));
		c6p->c6_opcode = C6OP_TESTRDY;
		c6p->c6_lun = sdp->sd_lun;
		c6p->c6_ctrl = CTRL_NOLINK;
		sdp->sd_bcount = 0;
		km_power_check();
	} while (!sdcmd(mg, si, sdp));
	if (tries > 0)
		printf("\n");
}

int
sdread(mg, si, lbn, ma, cc)
struct mon_global *mg;
struct sio *si;
char *ma;
{
	struct scsi_disk_device *sddp = (struct scsi_disk_device *)mg->mg_sddp;
	struct scsi_device *sdp = &sddp->sdd_sd;
	struct cdb_6 *c6p = &sdp->sd_cdb.cdb_c6;
	int nblk, i;

	/*
	 * We assume that the caller will always do reads in multiples
	 * of our block size
	 */
#ifdef SD_DEBUG
	printf("sdread: lbn=%d, ma=0x%x, cc=%d\n", lbn, ma, cc);
#endif SD_DEBUG
	nblk = howmany(cc, si->si_blklen);
	if (nblk * si->si_blklen != cc) {
		printf("bad dev blk size %d\n", si->si_blklen);
		return(-1);
	}
	for (i = 0; i < MAX_SDTRIES; i++) {
		bzero(c6p, sizeof(*c6p));
		c6p->c6_opcode = C6OP_READ;
		c6p->c6_lun = sdp->sd_lun;
		c6p->c6_lba = lbn;
		c6p->c6_len = (nblk == 256) ? 0 : nblk;
		c6p->c6_ctrl = CTRL_NOLINK;
		sdp->sd_bcount = cc;
		sdp->sd_addr = ma;
		sdp->sd_read = 1;
		if (sdcmd(mg, si, sdp)) {
#ifdef SD_DEBUG
			printf("sdread: ma0-3=%x%x%x%x\n", ma[0], ma[1],
			    ma[2], ma[3]);
#endif SD_DEBUG
			return(cc - sdp->sd_resid);
		}
	};
	sdfail(mg, si, sdp, "READ");
	return(-1);
}

sdwrite(mg, si, lbn, ma, cc)
struct mon_global *mg;
struct sio *si;
{
}

sdlabel_blkno (mg, si, size, i)
struct mon_global *mg;
struct sio *si;
{
	return (i * howmany (size, si->si_blklen));
}

static int
sdcmd(mg, si, sdp)
struct mon_global *mg;
struct sio *si;
struct scsi_device *sdp;
{
	struct scsi_disk_device *sddp = (struct scsi_disk_device *)mg->mg_sddp;

	if (scpollcmd(mg, si, sdp))
		return(1);
	switch (sdp->sd_state) {
	case SDSTATE_RESET:
#ifdef	SD_DEBUG1
	    printf("sdcmd detected reset\n");
	    break;
#endif	SD_DEBUG1
	case SDSTATE_SELTIMEOUT:
	case SDSTATE_ABORTED:
	case SDSTATE_UNIMPLEMENTED:
		break;
	case SDSTATE_COMPLETED:
		switch (sdp->sd_status) {
		case STAT_CHECK:
			sdreqsense(mg, si);
			break;
		case STAT_BUSY:
			delay(1000000);		/* 1 sec */
			break;
		}
		break;
	default:
		printf("sdcmd bad state: %d\n", sdp->sd_state);
		break;
	}
	return(0);
}

static void
sdfail(mg, si, sdp, msg)
struct mon_global *mg;
struct sio *si;
struct scsi_device *sdp;
char *msg;
{
	struct scsi_disk_device *sddp = (struct scsi_disk_device *)mg->mg_sddp;
	struct	esense_reply *erp;

	erp = DMA_ALIGN(struct esense_reply *, &sddp->sdd_replybuf);

	printf("%s: ", msg);
	switch (sdp->sd_state) {
	case SDSTATE_SELTIMEOUT:
		printf("Selection timeout on target\n");
		break;
	case SDSTATE_COMPLETED:
		switch (sdp->sd_status) {
		case STAT_CHECK:
			printf("Failed, sense key: 0x%x\n", erp->er_sensekey);
			break;
		case STAT_BUSY:
			printf("Target busy\n");
			break;
		default:
			printf("sdcmd bad state: %d\n",
			    sdp->sd_status);
			break;
		}
		break;
	case SDSTATE_ABORTED:
		printf("Target disconnected\n");
		break;
	case SDSTATE_UNIMPLEMENTED:
		printf("Driver refused command\n");
		break;
	default:
		printf("sdfail bad state: %d\n", sdp->sd_state);
		break;
	}
}
