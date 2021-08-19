/*	@(#)diskproto.c	1.0	09/10/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 *
 * diskproto.c -- generic routines for booting from disk drives
 * PROM MONITOR VERSION
 *
 * HISTORY
 *  9-May-88  John Seamons (jks) at NeXT
 *	Made generic instead of SCSI specific to support optical disks.
 *
 * 25-Feb-88  Mike DeMoney (mike) at NeXT
 *	Added stuff to deal with disk labels.
 *	Added stuff to dynamically size buffer based on logical block size.
 *
 * 10-Sep-87  Mike DeMoney (mike) at NeXT
 *	Created.
 *
 **********************************************************************
 */ 

#include "sys/param.h"
#include "mon/sio.h"
#include "mon/global.h"
#include <nextdev/dma.h>

#define	MAXBOOTFILE	64		/* max len of "xx(0,0,0)mach" */
/* #define	DP_DEBUG	1		/* */

int diskopen(), diskclose(), diskboot();
static unsigned long disk_load();
static int disk_getlabel();
static int disk_checklabel();

struct protocol proto_disk = { diskopen, diskclose, diskboot };

struct disk_global {
	char *dg_buf;
	struct disk_label dg_dl;
	unsigned dg_lbn;
	unsigned dg_off;
	char dg_bootfile[MAXBOOTFILE];
};

diskopen(mg, si)
struct mon_global *mg;
struct sio *si;
{
	struct disk_global *dgp;
	u_int buflen;
	int e;

	dgp = (struct disk_global *)mon_alloc(sizeof(*dgp));
	mg->mg_dgp = (char *)dgp;

	if (e = (*si->si_dev->d_open)(mg, si, 0))
		return (e);	/* failure */
	dgp->dg_lbn = -1;
	dgp->dg_off = 0;
	buflen = si->si_blklen + DMA_ENDALIGNMENT;
	dgp->dg_buf = mon_alloc(buflen);
	dgp->dg_buf = (char *)(((int)dgp->dg_buf + (DMA_ENDALIGNMENT - 1)) /
		DMA_ENDALIGNMENT * DMA_ENDALIGNMENT);
	return(0); /* success */
}

diskboot(mg, si, linep, x1, x2)
struct mon_global *mg;
struct sio *si;
char **linep;
{
	struct disk_global *dgp = (struct disk_global *)mg->mg_dgp;
	unsigned long entry;
	int tries, offset;
	struct disk_label *dlp;
	char *lp;

	/*
	 * Skip file name
	 */
	lp = *linep;
	if (*lp != '-') {
		while (*lp && *lp != ' ')
			lp++;
		*linep = lp;
	}
	if (! disk_getlabel(mg, si)) {
		printf("Bad label\n");
		return(0);
	}
	dlp = &dgp->dg_dl;

	/* allow kernel to look at disk label info to get hostname */
	mg->mg_boot_info = (char*) dlp;

	if (*mg->mg_boot_file == '\0') {
		if (*dlp->dl_bootfile)
			mg->mg_boot_file = dlp->dl_bootfile;
		else {
			printf("No bootfile in label\n");
			return(0);
		}
	}
	if (dlp->dl_secsize < si->si_blklen
	    || (dlp->dl_secsize % si->si_blklen) != 0) {
		printf("dev blk len %d, fs sect %d\n",
		    si->si_blklen, dlp->dl_secsize);
		return(0);
	}
	for (tries = 0; tries < NBOOTS; tries++) {
		offset = dlp->dl_boot0_blkno[tries] * dlp->dl_secsize;
		if (entry = disk_load(mg, si, offset))
			return(entry);
	}
	printf("Can't load blk0 boot\n");
	return(0);
}

static int
disk_getlabel(mg, si)
struct mon_global *mg;
struct sio *si;
{
	struct disk_global *dgp = (struct disk_global *)mg->mg_dgp;
	struct disk_label *dlp;
	int tries, blkno, i;

#ifdef DP_DEBUG
	printf("getting label\n");
#endif DP_DEBUG
	dlp = &dgp->dg_dl;
	for (tries = 0; tries < NLABELS; tries++) {
		blkno = (*si->si_dev->d_label_blkno)
			(mg, si, sizeof (*dlp), tries);
		disk_seek(mg, si, blkno * si->si_blklen);
		bzero(dlp, sizeof(*dlp));
		i = disk_read(mg, si, dlp, sizeof(*dlp));
#ifdef DP_DEBUG
		printf("disk_read returns %d\n", i);
#endif DP_DEBUG
		if (i == sizeof(*dlp) && disk_checklabel(dlp, blkno))
			return(1);
	}
#ifdef DP_DEBUG
	printf("getlabel failed\n");
#endif DP_DEBUG
	return(0);
}

static int
disk_checklabel(dlp, blkno)
struct disk_label *dlp;
int blkno;
{
	u_short xx, checksum, cksum();

	if (dlp->dl_version != DL_V1 && dlp->dl_version != DL_V2) {
		printf("Bad version 0x%x\n", dlp->dl_version);
		return (0);
	}
	if (dlp->dl_label_blkno != blkno) {
		printf("Bad blkno\n");
		return (0);	/* label not where it's supposed to be */
	}
	checksum = dlp->dl_checksum;
	dlp->dl_checksum = 0;
	if ((xx = cksum(dlp, sizeof (struct disk_label))) != checksum) {
		printf("Bad cksum\n");
		return (0);
	}
	dlp->dl_checksum = checksum;
	return (1);
}
			
static unsigned long
disk_load(mg, si, offset)
struct mon_global *mg;
struct sio *si;
int offset;
{
	struct disk_global *dgp = (struct disk_global *)mg->mg_dgp;
	char *bfp = dgp->dg_bootfile;
	unsigned long entry, size;
	char x[32];
	char *ma;
	int i;

	disk_seek(mg, si, offset);
	i = disk_read(mg, si, x, sizeof(x));
	if (i != sizeof(x))
		goto shread;
	if ((size = loader (x, sizeof (x), &entry, &ma)) == 0)
		return (0);
printf ("size %d entry 0x%x ma 0x%x\n", size, entry, ma);
	i = disk_read(mg, si, ma, size);
	if (i != size)
		goto shread;
printf ("size %d entry 0x%x ma 0x%x\n", size, entry, ma);

	/*
	 * Transfer to the block 0 boot, it will return with the entry
	 * point of the desired program or zero if it couldn't be loaded.
	 */
	if (si->si_args == SIO_SPECIFIED)
		sprintf(bfp, "%s(%d,%d,%d)%s", mg->mg_boot_dev,
		    si->si_ctrl, si->si_unit, si->si_part, mg->mg_boot_file);
	else
		sprintf(bfp, "%s()%s", mg->mg_boot_dev, mg->mg_boot_file);
printf ("0x%x 0x%x\n", *(int*)0x4100000, *(int*)0x41061a8);
	entry = (* (unsigned long (*)()) entry)(bfp,
	    /* FIXME: nuke this when backward compatibility not required */
	    mg->mg_nvram.ni_console_i, mg->mg_nvram.ni_console_o);
printf ("entry 0x%x\n", entry);
	return(entry);
shread:
	printf("short read\n");
	return(0);
}

disk_seek(mg, si, off)
struct mon_global *mg;
struct sio *si;
{
	struct disk_global *dgp = (struct disk_global *)mg->mg_dgp;

#ifdef DP_DEBUG
	printf("seeking to 0x%x\n", off);
#endif DP_DEBUG
	dgp->dg_off = off;
}

disk_read(mg, si, buf, cc)
struct mon_global *mg;
struct sio *si;
char *buf;
int cc;
{
	struct disk_global *dgp = (struct disk_global *)mg->mg_dgp;
	unsigned lbn;
	char *bp;
	int i;
	int occ = cc;

	while (cc > 0) {
		lbn = dgp->dg_off / si->si_blklen;
#ifdef DP_DEBUG
		printf("disk_read: dg_buf=0x%x dg_lbn=0x%x dg_off=0x%x\n",
		    dgp->dg_buf, dgp->dg_lbn, dgp->dg_off);
		printf("disk_read: lbn=%d buf=0x%x cc=%d\n",
		    lbn, buf, cc);
#endif DP_DEBUG
		if (lbn != dgp->dg_lbn) {
			i=(*si->si_dev->d_read)(mg, si, lbn, dgp->dg_buf,
			    si->si_blklen);
			if (i != si->si_blklen) {
				return(-1);
			}
			dgp->dg_lbn = lbn;
		}
		bp = &dgp->dg_buf[dgp->dg_off % si->si_blklen];
		while (bp < &dgp->dg_buf[si->si_blklen] && cc > 0) {
			*buf++ = *bp++;
			dgp->dg_off++;
			cc--;
		}
	}
	return(occ);
}

diskclose(mg, si)
struct mon_global *mg;
struct sio *si;
{
	(*si->si_dev->d_close)(mg, si);
}
