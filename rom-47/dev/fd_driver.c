/*	@(#)fd_driver.c	2.0	01/24/90	(c) 1990 NeXT	*/

/* 
 * fd_driver.c -- Front end for Floppy Disk driver
 * PROM MONITOR VERSION
 *
 * HISTORY
 * 03-Apr-90	Doug Mitchell at NeXT
 *	Created.
 */ 

#import <sys/types.h>
#import <sys/param.h>
#import <nextdev/disk.h>
#import <nextdev/dma.h>
#import <next/psl.h>
#import <next/cpu.h>
#import <sys/conf.h>
#import <mon/sio.h>
#import <mon/global.h>
#import	<dev/fd_extern.h>
#import	<dev/fd_vars.h> 

/* #define FD_DEBUG1	1	/* */

int fd_open();
int fd_close();
int fd_read();
int fd_write();
int fd_label_blkno();

static int fd_attach(struct fd_global *fdgp, int no_config);

struct device floppy_disk =
	{ fd_open, fd_close, fd_read, fd_write, fd_label_blkno, D_IOT_PACKET };

/*
 * ROM'd Constants 
 */
#define	DRIVE_TYPES	1		/* number of supported drive types */

struct fd_drive_info fd_drive_info[] = {
	{
	    {
		"Sony MPX-111N",	/* disk name */
		{ 0, 
		  0, 
		  0,
		  0 }, 			/* label locations - not used */
		1024,			/* device sector size. This is
					 * not fixed. */
		64 * 1024		/* max xfer bcount */
	    },
	    SONY_SEEK_RATE,
	    SONY_HEAD_LOAD,
	    SONY_HEAD_UNLOAD
	}
};

/*
 * Constant tables for mapping media_id and density into physical disk 
 * parameters.
 */
struct fd_disk_info fd_disk_info[] = {
   /* media_id          tracks_per_cyl  num_cylinders   max_density */
    { FD_MID_1MB,	NUM_FD_HEADS, 	NUM_FD_CYL, 	FD_DENS_1    },
    { FD_MID_2MB,	NUM_FD_HEADS, 	NUM_FD_CYL, 	FD_DENS_2    },
    { FD_MID_4MB,	NUM_FD_HEADS, 	NUM_FD_CYL, 	FD_DENS_4    },
    { FD_MID_NONE,	0, 		0, 		FD_DENS_NONE },
};

struct fd_density_info fd_density_info[] = {
    /* density     	capacity    	gap3	mfm         */
    { FD_DENS_1,	720  * 1024,	8,	TRUE	}, /* 720  KB */
    { FD_DENS_2,	1440 * 1024, 	16,	TRUE	}, /* 1.44 MB */
    { FD_DENS_4,	2880 * 1024,	32,	TRUE	}, /* 2.88 MB */
    { FD_DENS_NONE,	720  * 1024,	48,	TRUE	}, /* unformatted */
};

/* #define SHOW_AUTOSIZE	1	/* */

static int fd_attach(struct fd_global *fdgp, int no_config)
{
	fd_volume_t fvp = &fdgp->fd_volume;
	int retry;
	struct fd_rw_stat rw_stat;
	int sect_num;
	struct fd_ioreq ioreq;
	struct fd_disk_info *dip;
	struct fd_format_info *fip = &fvp->format_info;
	struct fd_drive_stat dstat;
	int sect_size;
	int density;
	int rtn;
	
#ifdef	FD_DEBUG
	printf("fd_attach: volume %d drive %d\n", fvp->volume_num,
		fvp->drive_num);
#endif	FD_DEBUG

	/* 
	 * Determine density and try to read disk label.
	 *
	 * First try a recalibrate.
	 */
	fip->density_info.density = FD_DENS_1;	/* necessary for seek rate
						 * programming */
	fip->flags &= ~FFI_FORMATTED;
	for(retry=FV_ATTACH_TRIES; retry; retry--) {
		if(fd_recal(fvp) == 0)
			break;
	} /* retrying */
	if(retry == 0) {
		/* 
		 * Can't even recalibrate; forget it.
		 */
		if(!fdgp->silent)
			printf("fd: RECALIBRATE FAILED\n");
		rtn = BE_NODEV;
		goto attach_fail;
	}
			
	/*
	 * Determine media ID.
	 */
	fip->disk_info.media_id = FD_MID_NONE;
	if(fd_get_status(fvp, &dstat)) {
		if(!fdgp->silent)
			printf("fd: CONTROLLER I/O ERROR\n");
		rtn = BE_NODEV;
		goto attach_fail;
	}
	if(dstat.media_id == FD_MID_NONE) {
		rtn = BE_INSERT;
		goto attach_fail;
	}
	/*
	 * We know the disk; set disk-specific parameters.
	 */
	fip->disk_info.media_id = dstat.media_id;
	for(dip = fd_disk_info; dip->media_id != FD_MID_NONE; dip++) {
		if(dip->media_id == dstat.media_id)
			break;
	}
	fip->disk_info = *dip;
	if(no_config) {
		/* set an arbitraty density and sector size */
		fd_set_density_info(fvp, FD_DENS_4);
		fd_set_sector_size(fvp, FD_SECTSIZE_MIN);
		return(0);
	}
	/*
	 * try reading one sector for each sector size and density
	 */
	fip->flags |= FFI_FORMATTED;		/* invalidate if this part 
						 * fails... */
	fip->density_info.mfm = 1;
	sect_num = 0;
	for(fip->density_info.density=FD_DENS_4; 
	    fip->density_info.density>FD_DENS_NONE; 
	    fip->density_info.density--) {
#ifdef	SHOW_AUTOSIZE
		printf("density = %d\n", fip->density_info.density);
#endif	SHOW_AUTOSIZE
		fd_set_density_info(fvp, fip->density_info.density);
		for(sect_size=FD_SECTSIZE_MIN;
		    sect_size<=FD_SECTSIZE_MAX;
		    sect_size <<= 1) {
			/*
			 * Validate physical disk parameters for this sector
			 * size so we can do I/O.
			 */
#ifdef	SHOW_AUTOSIZE
			printf("sect_size = %d\n", sect_size);
#endif	SHOW_AUTOSIZE
			fd_set_sector_size(fvp, sect_size);
			for(retry=FV_ATTACH_TRIES; retry; retry--) {	
				if(fd_raw_rw(fvp,
				    sect_num,
				    1,			/* sect_count */
				    (caddr_t)fdgp->sbp,
				    TRUE) == 0)		/* read */
					goto disk_formatted;
				/*
				 * Try another sector.
				 */			
				sect_num += (fip->sects_per_trk + 1);
			}
		} /* for each sector size */
		if(fd_recal(fvp)) {
			printf("RECALIBRATE FAILED\n");
			rtn = BE_NODEV;
			goto attach_fail;
		}
	} /* for each density */
	if(retry == 0) {
		/*
		 * Could not read this disk.
		 */
		rtn = BE_INIT;
		goto attach_fail;
	}
disk_formatted:
#ifdef	FD_DEBUG	
	printf("fd_open: RETURNING 0\n");
#endif	FD_DEBUG
	return(0);
	
attach_fail:
	fd_basic_cmd(fvp, FDCMD_MOTOR_OFF);
#ifdef	FD_DEBUG	
	printf("fd_attach: ERROR; RETURNING %d\n", rtn);
#endif	FD_DEBUG
	return(rtn);

} /* fd_attach() */

int fd_open(struct mon_global *mg, 
	struct sio *sip, 
	int no_config,		/* skip density/sector size test */
	int silent)		/* skip error reporting */
{
	fd_volume_t fvp;
	int rtn;
	struct fd_global *fdgp;
	int ctrl, unit;
	
#ifdef	FD_DEBUG
	printf("fd_open: c %d u %d p %d\n", 
		sip->si_ctrl, sip->si_unit, sip->si_part);
#endif	FD_DEBUG
	if (sip->si_args == SIO_SPECIFIED) {
		ctrl = sip->si_ctrl;
		unit = sip->si_unit;
	} else {
		ctrl = 0;
		unit = 0;
	}
	fdgp = (struct fd_global *)mon_alloc(sizeof(*fdgp));
	mg->mg_fdgp = (char *)fdgp;
	fdgp->sbp = DMA_ENDALIGN(u_char *, fdgp->sector_buf);
	fdgp->silent = silent;
	
	fvp = &fdgp->fd_volume;
	fd_new_fv(fvp, unit);
	fvp->drive_num = unit;
	/*
	 * init drive info.
	 */
	fvp->unit = unit;
	fvp->fcp = &fdgp->fd_controller;
	fvp->drive_flags = FDF_PRESENT;	
	/*
	 * Init controller hardware.
	 */
	if(fc_init(fdgp, (caddr_t)P_FLOPPY)) {
#ifdef	FD_DEBUG	
		printf("fd_open: RETURNING -1\n");
#endif	FD_DEBUG
		return(-1);
	}
	fvp->format_info.flags &= ~FFI_FORMATTED;
	rtn = fd_attach(fdgp, no_config);
	if(rtn && silent)
		return(rtn);
	switch(rtn) {
	    case 0:
	    	break;
	    case BE_NODEV:
		printf("No Floppy Disk Drive\n");
		return(rtn);
	    case BE_INSERT:
	    	printf("No Floppy Disk Present\n");
		return(rtn);
	    case BE_INIT:
	    	printf("Floppy Disk not Formatted\n");
		return(rtn);
	    default:
	    	printf("Unknown Floppy Disk error (%d)\n");
		return(rtn);
	}
	if(no_config)
		return(0);
	if ((fvp->format_info.flags & FFI_FORMATTED) == 0) {
	    	printf("Floppy Disk not Initialized\n");
		fd_close(mg, sip);
		return(BE_INIT);
	}
	sip->si_blklen = fvp->format_info.sect_size;
	sip->si_lastlba = fvp->format_info.total_sects;
#ifdef 	FD_DEBUG
	printf("%d byte sectors\n", sip->si_blklen);
#endif	FD_DEBUG

	return(0);	
} /* fd_open() */

int fd_close(struct mon_global *mg, struct sio *sip)
{
}

int fd_read(struct mon_global *mg, 
	struct sio *sip, 
	int sect_num, 
	char *addrs, 
	int byte_count)
{
	int rtn;
	struct fd_global *fdgp = (struct fd_global *)mg->mg_fdgp;
	fd_volume_t fvp = &fdgp->fd_volume;
	
#ifdef	FD_DEBUG1
	printf("fd_read: sect %d byte_count %d\n", sect_num, byte_count);
#endif	FD_DEBUG1
	rtn = fd_live_rw(fvp, 
		sect_num, 
		byte_count / fvp->format_info.sect_size,
		addrs,
		TRUE);			/* read */
	if(rtn)
		rtn = -1;
	else
		rtn = byte_count - fvp->bytes_to_go;
#ifdef	FD_DEBUG
	printf("fd_read: returning %d\n", rtn);
#endif	FD_DEBUG
	return(rtn);
		
} /* fd_read() */

int fd_write(struct mon_global *mg, 
	struct sio *sip, 
	int sect_num, 
	char *addrs, 
	int byte_count)
{

} /* fd_write() */

fd_label_blkno(struct mon_global *mg, 
	struct sio *sip, 
	int label_size,
	int i)
{
	return (i * howmany (label_size, sip->si_blklen));
}

int fd_eject(int drive)
{
	struct mon_global *mg = restore_mg();
	struct sio sio;
	struct fd_global *fdgp;
	
#ifdef	FD_DEBUG
	printf("fd_eject: drive %d\n", drive);
#endif	FD_DEBUG
	sio.si_ctrl = 0;
	sio.si_unit = drive;
	sio.si_args = SIO_SPECIFIED;
	if(fd_open(mg, &sio, 1, 1))	/* no_config, silent */
		return (-1);
	fdgp = (struct fd_global *)mg->mg_fdgp;
	fd_basic_cmd(&fdgp->fd_volume, FDCMD_EJECT);
	return (0);
}

int fd_command(fd_volume_t fvp,	fd_ioreq_t fdiop)
{
	/* 
	 * returns an errno.
	 * Parameters are passed from the user (and to fd_start()) in a 
	 * fd_ioreq struct.
	 */
	 
	int rtn = 0;
	
#ifdef	FD_DEBUG
	printf("fd_command: volume %d cmd %d\n", fvp->volume_num,
		fvp->dev_ioreq.command);
#endif	FD_DEBUG
	fvp->dev_ioreq = *fdiop;
	fvp->io_flags |= FVIOF_SPEC;
	rtn = fd_start(fvp);	
	if(rtn)
		rtn = -1;		/* don't set errno of -1! */
	if((rtn == 0) && (fvp->dev_ioreq.status))
		rtn = -1;
	/*
	 * replace caller's ioreq with our local ioreq
	 */
	*fdiop = fvp->dev_ioreq;
#ifdef	FD_DEBUG
	printf("fd_command: returning %d\n", rtn);
#endif	FD_DEBUG
	return(rtn);
	
} /* fd_command() */

/* end of fd_driver.c */






