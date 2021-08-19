/*	@(#)diskproto.c	1.0	09/10/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 *
 * diskproto.c -- generic routines for booting from disk drives
 * PROM MONITOR VERSION
 *
 * HISTORY
 * 09-Apr-90	Doug Mitchell at NeXT
 *	Allowed multi-sector I/Os in disk_read().
 *
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
#define BLOCKS_PER_IO	16 

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
	unsigned dg_valid;		/* # of valid bytes in dg_buf */
	char dg_bootfile[MAXBOOTFILE];
};

diskopen(mg, si, p1, p2, p3)
struct mon_global *mg;
struct sio *si;
{
	struct disk_global *dgp;
	u_int buflen;
	int e;

	dgp = (struct disk_global *)mon_alloc(sizeof(*dgp));
	mg->mg_dgp = (char *)dgp;

	if (e = (*si->si_dev->d_open)(mg, si, p1, p2, p3))
		return (e);	/* failure */
	dgp->dg_lbn = -1;
	dgp->dg_valid = 0;
	dgp->dg_off = 0;
	buflen = (si->si_blklen * BLOCKS_PER_IO) + DMA_ENDALIGNMENT;
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
		if(offset < 0)
			continue;	/* this boot block not present */
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
	u_short checksum, cksum(), *dl_cksum, size;

	if (dlp->dl_version == DL_V1 || dlp->dl_version == DL_V2) {
		size = sizeof (struct disk_label);
		dl_cksum = &dlp->dl_checksum;
	} else
	if (dlp->dl_version == DL_V3) {
		size = sizeof (struct disk_label) - sizeof (dlp->dl_un);
		dl_cksum = &dlp->dl_v3_checksum;
	} else {
		printf("Bad version 0x%x\n", dlp->dl_version);
		return (0);
	}
	if (dlp->dl_label_blkno != blkno) {
		printf("Bad blkno\n");
		return (0);	/* label not where it's supposed to be */
	}
	checksum = *dl_cksum;
	*dl_cksum = 0;
	if (cksum(dlp, size) != checksum) {
		printf("Bad cksum\n");
		return (0);
	}
	*dl_cksum = checksum;
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
	char x[1024];
	char *ma;
	int i;

	disk_seek(mg, si, offset);
	i = disk_read(mg, si, x, sizeof(x));
	if (i != sizeof(x))
		goto shread;
	if ((size = loader (x, &i, &entry, &ma)) == 0)
		return (0);
#ifdef	DP_DEBUG
	printf("disk_load(): loader returned %d\n", size);
#endif	DP_DEBUG
	i = disk_read(mg, si, ma, size);
	if (i != size)
		goto shread;

	/*
	 * Transfer to the block 0 boot, it will return with the entry
	 * point of the desired program or zero if it couldn't be loaded.
	 */
	if (si->si_args == SIO_SPECIFIED)
		sprintf(bfp, "%s(%d,%d,%d)%s", mg->mg_boot_dev,
		    si->si_ctrl, si->si_unit, si->si_part, mg->mg_boot_file);
	else
		sprintf(bfp, "%s()%s", mg->mg_boot_dev, mg->mg_boot_file);
#ifdef	DP_DEBUG
	printf("disk_load(): transferring to boot block; entry = 0x%x\n", 
		entry);
#endif	DP_DEBUG
	entry = (* (unsigned long (*)()) entry)(bfp);
#ifdef	DP_DEBUG
		printf("disk_load(): returning from boot block; entry = "
			"0x%x\n", entry);
#endif	DP_DEBUG
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
	int rcc;		/* requested byte count per I/O */
	int bufsize = si->si_blklen * BLOCKS_PER_IO;
	
#ifdef 	DP_DEBUG
	printf("disk_read: dg_buf=0x%x dg_lbn=%d dg_off=0x%x\n",
		    dgp->dg_buf, dgp->dg_lbn, dgp->dg_off);
#endif	DP_DEBUG
	while (cc > 0) {
		lbn = dgp->dg_off / si->si_blklen;
		/* 
		 * see if we already have requested data 
		 */
		rcc = bufsize;
		if(rcc > cc)
			rcc = cc;
		if(rcc & (si->si_blklen - 1)) {
			/* round up to an even sector */
			rcc = (((rcc + si->si_blklen) / si->si_blklen) * 
			      si->si_blklen);
		}
		if((lbn < dgp->dg_lbn) || 
		   ((lbn * si->si_blklen + rcc) > 
		    (dgp->dg_lbn * si->si_blklen + dgp->dg_valid))) {
#ifdef DP_DEBUG
			printf("disk_read: lbn=%d buf=0x%x rcc=%d\n",
		    		lbn, buf, rcc);
#endif DP_DEBUG
			i=(*si->si_dev->d_read)(mg, si, lbn, dgp->dg_buf, rcc);
			if (i != rcc) {
				return(-1);
			}
			dgp->dg_lbn = lbn;
			dgp->dg_valid = rcc;
		}
		bp = &dgp->dg_buf[dgp->dg_off % si->si_blklen];
		while (bp < &dgp->dg_buf[bufsize] && cc > 0) {
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







