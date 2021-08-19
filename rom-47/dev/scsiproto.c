/*	@(#)scsiproto.c	1.0	09/10/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 *
 * sd.c -- implementation of disk specific scsi routines
 * PROM MONITOR VERSION
 *
 * Supports scsi disks meeting mandatory requirements of
 * ANSI X3.131.
 *
 * FIXME:
 *	deal with dk_busy stuff
 *
 * HISTORY
 * 25-Feb-88   Mike DeMoney (mike) at NeXT
 *	Added stuff to deal with disk labels.
 *	Added stuff to dynamically size buffer based on logical block size.
 * 10-Sept-87  Mike DeMoney (mike) at NeXT
 *	Created.
 *
 **********************************************************************
 */ 

#include "../sys/param.h"
#include "../mon/sio.h"
#include "../mon/global.h"
#include "../nextdev/disk.h"
#include <a.out.h>
#include <nextdev/dma.h>

int scsiopen(), scsiclose(), scsiboot();
static unsigned long scsi_load();
static int scsi_getlabel();
static int scsi_checklabel();

struct protocol proto_scsi = { scsiopen, scsiclose, scsiboot };

struct scsi_global {
	char *sg_buf;
	struct disk_label sg_dl;
	unsigned sg_lbn;
	unsigned sg_off;
};

scsiopen(mg, si)
struct mon_global *mg;
struct sio *si;
{
	struct scsi_global *sgp;
	u_int buflen;

	sgp = (struct scsi_global *)mon_alloc(sizeof(*sgp));
	mg->mg_sgp = (char *)sgp;

	if ((*si->si_dev->d_open)(mg, si))
		return(-1);	/* failure */
	sgp->sg_lbn = -1;
	sgp->sg_off = 0;
	buflen = si->si_blklen + DMA_BEGINALIGNMENT;
	sgp->sg_buf = DMA_ALIGN(char *, mon_alloc(buflen));
	return(0); /* success */
}

scsiboot(mg, si, linep, x1, x2)
struct mon_global *mg;
struct sio *si;
char **linep;
{
	struct scsi_global *sgp = (struct scsi_global *)mg->mg_sgp;
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
	if (! scsi_getlabel(mg, si)) {
		printf("Can't read disk label\n");
		return(0);
	}
	dlp = &sgp->sg_dl;
	if (*mg->mg_boot_file == '\0') {
		if (*dlp->dl_bootfile)
			mg->mg_boot_file = dlp->dl_bootfile;
		else {
			printf("No default bootfile specified in disk label\n");
			return(0);
		}
	}
	if (dlp->dl_secsize < si->si_blklen
	    || (dlp->dl_secsize % si->si_blklen) != 0) {
printf("Format error: device block length %d, file system sector %d\n",
		    si->si_blklen, dlp->dl_secsize);
		return(0);
	}
	for (tries = 0; tries < NBOOTS; tries++) {
		offset = dlp->dl_boot0_blkno[tries] * dlp->dl_secsize;
		if (entry = scsi_load(mg, si, offset))
			return(entry);
	}
	printf("Can't load blk0 boot\n");
	return(0);
}

static int
scsi_getlabel(mg, si)
struct mon_global *mg;
struct sio *si;
{
	struct scsi_global *sgp = (struct scsi_global *)mg->mg_sgp;
	struct disk_label *dlp;
	int tries, blkno, i;

	dlp = &sgp->sg_dl;
	for (tries = 0; tries < NLABELS; tries++) {
		blkno = tries * howmany(sizeof(*dlp), si->si_blklen);
		scsi_seek(mg, si, blkno * si->si_blklen);
		i = scsi_read(mg, si, dlp, sizeof(*dlp));
		if (i == sizeof(*dlp) && scsi_checklabel(dlp, blkno))
			return(1);
	}
	return(0);
}

static int
scsi_checklabel(dlp, blkno)
struct disk_label *dlp;
int blkno;
{
	u_short xx, checksum, cksum();

	if (dlp->dl_version != DL_V1 && dlp->dl_version != DL_V2) {
		printf("bad version code 0x%x\n", dlp->dl_version);
		return (0);
	}
	if (dlp->dl_label_blkno != blkno) {
		printf("bad blkno\n");
		return (0);	/* label not where it's supposed to be */
	}
	dlp->dl_label_blkno = 0;
	checksum = dlp->dl_checksum;
	dlp->dl_checksum = 0;
	if ((xx = ~cksum(dlp, sizeof (struct disk_label))) != checksum) {
		printf("cksum calc 0x%x, label 0x%x\n", xx, checksum);
		return (0);
	}
	dlp->dl_checksum = checksum;
	return (1);
}
			
static unsigned long
scsi_load(mg, si, offset)
struct mon_global *mg;
struct sio *si;
int offset;
{
	unsigned long entry;
	struct exec x;
	char bootfile[64];
	char *ma;
	int i;

	scsi_seek(mg, si, offset);
	i = scsi_read(mg, si, &x, sizeof(x));
	if (i != sizeof(x))
		goto shread;
	if (x.a_magic != 0407) {
		printf("Bad boot block\n");
		return;
	}
	ma = (char *)x.a_entry;
	printf("Loading boot at 0x%x: ", ma);

	printf("%d", x.a_text);
	i = scsi_read(mg, si, ma, x.a_text);
	if (i != x.a_text)
		goto shread;
	ma += x.a_text;

	printf("+%d", x.a_data);
	i = scsi_read(mg, si, ma, x.a_data);
	if (i != x.a_data)
		goto shread;
	ma += x.a_data;

	printf("+%d", x.a_bss);
	x.a_bss += 128*512;	/* slop */
	for (i = 0; i < x.a_bss; i++)
		*ma++ = 0;
	/*
	 * Transfer to the block 0 boot, it will return with the entry
	 * point of the desired program or zero if it couldn't be loaded.
	 */
	if (si->si_args == SIO_SPECIFIED)
		sprintf(bootfile, "%s(%d,%d,%d)%s", mg->mg_boot_dev,
		    si->si_ctrl, si->si_unit, si->si_part, mg->mg_boot_file);
	else
		sprintf(bootfile, "%s()%s", mg->mg_boot_dev, mg->mg_boot_file);
	entry = (* (unsigned long (*)()) x.a_entry)(bootfile,
	    mg->mg_nvram.ni_console_i, mg->mg_nvram.ni_console_o);
	printf("\nBlock 0 boot returned, transfering to kernel at 0x%x\n",
	    entry);
	asm("movl	sp,sp@-");
	printf("sp = 0x%x\n");
	asm("addql	#4,sp");
	return(entry);
shread:
	printf("short read\n");
	return(0);
}

scsi_seek(mg, si, off)
struct mon_global *mg;
struct sio *si;
{
	struct scsi_global *sgp = (struct scsi_global *)mg->mg_sgp;

	sgp->sg_off = off;
}

scsi_read(mg, si, buf, cc)
struct mon_global *mg;
struct sio *si;
char *buf;
int cc;
{
	struct scsi_global *sgp = (struct scsi_global *)mg->mg_sgp;
	unsigned lbn;
	char *bp;
	int i;
	int occ = cc;

	while (cc > 0) {
		lbn = sgp->sg_off / si->si_blklen;
		if (lbn != sgp->sg_lbn) {
			i=(*si->si_dev->d_read)(mg,si,lbn,sgp->sg_buf,si->si_blklen);
			if (i != si->si_blklen) {
				printf("scsiread got %d expected %d\n",
				    i, si->si_blklen);
				return(-1);
			}
			sgp->sg_lbn = lbn;
		}
		bp = &sgp->sg_buf[sgp->sg_off % si->si_blklen];
		while (bp < &sgp->sg_buf[si->si_blklen] && cc > 0) {
			*buf++ = *bp++;
			sgp->sg_off++;
			cc--;
		}
	}
	return(occ);
}

scsiclose(mg, si)
struct mon_global *mg;
struct sio *si;
{
	(*si->si_dev->d_close)(mg, si);
}
