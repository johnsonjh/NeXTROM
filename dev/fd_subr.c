/*	@(#)fd_subr.c	2.0	01/29/90	(c) 1990 NeXT	*/

/* 
 * fd_subr.c -- Support routines for Floppy Disk Driver
 * STANDALONE BOOT VERSION
 *
 * HISTORY
 * 03-Apr-90	Doug Mitchell at NeXT
 *	Created.
 */ 

void delay(int n);

#import <sys/types.h>
#import <sys/param.h>
#import <nextdev/disk.h>
#import <next/psl.h>
#import <nextdev/dma.h>
#import <mon/sio.h>
#import	<dev/fd_extern.h>
#import	<dev/fd_vars.h> 

/*
 * prototypes of private functions
 */
static void fd_log_2_phys(fd_volume_t fvp,
	u_int sector,
	struct fd_rw_cmd *cmdp);
static void fd_setup_rw_req(fd_volume_t fvp);
static void fd_gen_rw_cmd(fd_volume_t fvp,
	fd_ioreq_t fdiop,
	u_int block,
	u_int byte_count,
	caddr_t dma_addrs,
	int read);
static void fd_gen_recal(fd_volume_t fvp, fd_ioreq_t fdiop);
static void fd_log_error(fd_volume_t fvp, char *err_string);

fd_return_t fd_start(fd_volume_t fvp)
{
	/* 
	 * Execute one of two types of commands. Normal reads and writes are
	 * specified in fvp->{start_sect, bytes_to_go, start_addrs}.
	 * Special commands (FVIOF_MAP) are in fvp->dev_ioreq.
	 */
	struct fd_format_info *fip = &fvp->format_info;
	fd_return_t rtn;
	
#ifdef	FD_DEBUG
	printf("fd_start: volume %d drive %d\n", fvp->volume_num,
		fvp->drive_num);
#endif	FD_DEBUG
	if(fvp->io_flags & FVIOF_SPEC) {
		/* special command; easy case... */
		fvp->bytes_to_go = 0;
	}
	else {
		fd_setup_rw_req(fvp);
		fvp->inner_retry_cnt = FV_INNER_RETRY;
		fvp->outer_retry_cnt = FV_OUTER_RETRY;
	} /* normal I/O request */
	fvp->state = FVSTATE_EXECUTING;
	/*
	 * We keep on calling the controller I/O module until the command
	 * is complete. Reasons for reiteration are retries are track-crossing
	 * mapping.
	 */
	while(fvp->state != FVSTATE_IDLE) {
		fc_docmd(fvp);
	}
#ifdef	FD_DEBUG
	printf("fd_start: returning %d\n", fvp->dev_ioreq.status);
#endif	FD_DEBUG
	return(fvp->dev_ioreq.status);
err:
	/*
	 * bogus system request. Abort.
	 */
	fvp->dev_ioreq.status = FDR_REJECT;
	return(FDR_REJECT);
} /* fd_start() */

void fd_intr(fd_volume_t fvp)
{
	int device;
	fd_ioreq_t fdiop = &fvp->dev_ioreq;
	struct buf *bp;
	u_char opcode;
	
	/*
	 * controller level has done everything it could to process the 
	 * command in fvp->dev_ioreq. If error or more data is yet to be 
	 * transferred in this I/O (because of track crossings, etc.),
	 * set up another I/O here; fd_start will restart the I/O. If
	 * I/O complete or retries exhausted, set fvp->state = FVSTATE_IDLE.
	 */
#ifdef	notdef
	printf("fd_intr: volume %d status %d\n", fvp->volume_num,
		fvp->dev_ioreq.status);
#endif	notdef
	opcode = fdiop->cmd_blk[0] & FCCMD_OPCODE_MASK;
	
	/*
	 * no retries on special commands.
	 */
	if(fvp->io_flags & FVIOF_SPEC) {
		fvp->state = FVSTATE_IDLE;
		return;
	}
	else {
	    /* 
	     * note any data transferred regardless of status.
	     */
	    if(fdiop->bytes_xfr) {
	    	    int sect_size = fvp->format_info.sect_size;
		    
		    /*
		     * OK: if doing a read or write command, we can only move
		     * an integral number of sectors, even though the hardware
		     * might tell us differently (on writes with poorly aligned
		     * end address, the hardware will tell us we moved up to
		     * 0x1C extra bytes). 
		     */
		    if((opcode == FCCMD_READ) || (opcode == FCCMD_WRITE))
			fdiop->bytes_xfr &= ~(sect_size - 1);

		    /* 
		     * FIXME: I think the 82077 passes us a whole sector's
		     * worth of data before detecting this. To indicate which 
		     * sector was bad (and to avoid passing bad data), back off
		     * on the "bytes transferred" by one sector.
		     */
		    if((fdiop->status == FDR_DATACRC) && (fdiop->bytes_xfr))
		    	fdiop->bytes_xfr -= sect_size;

		    fvp->bytes_to_go -= fdiop->bytes_xfr;
		    fvp->start_addrs += fdiop->bytes_xfr;
		    if(sect_size) {
		    	/* maybe a non-disk I/O...*/
			fvp->start_sect += (fdiop->bytes_xfr / sect_size);
		    }
		    /* 
		     * In case of error, fvp->start sect should be the sector
		     * in error (the sector to be retried).
		     */
	    }

	    switch (fdiop->status) {
		case FDR_SUCCESS:
		    switch(fvp->state) {
			case FVSTATE_RECALIBRATING:
			    /* 
			     * now we go back and try some more I/O. Go back
			     * to retry mode.
			     */
			    fvp->state = FVSTATE_RETRYING;
			    goto do_some_more;
			    
			case FVSTATE_RETRYING:
			    /*
			     * hey, the retry worked. Exit this mode.
			     */
			    fvp->state = FVSTATE_EXECUTING;
			    /* drop thru... */
			    
			case FVSTATE_EXECUTING:
do_some_more:
			    /*
			     * if any more data is yet to be transferred, set
			     * up another I/O request. No remapping on read
			     * here, since we're reading this data for the
			     * first time.
			     */
			    if(fvp->bytes_to_go) {
				fd_setup_rw_req(fvp);
				break;
			    }
			    else {
				/*
				 * Normal I/O completion.
				 */
				fvp->state = FVSTATE_IDLE;
				break;
			    }
			default:
			    printf("fd_intr: BOGUS fvp->state");
			    fdiop->status = FDR_REJECT;
			    fvp->state = FVSTATE_IDLE;
			    break;
		    } /* switch state */
		    break; 			/* from case FDR_SUCCESS */
		    
		case FDR_TIMEOUT:
		case FDR_DATACRC:
		case FDR_HDRCRC:
		case FDR_MEDIA:
		case FDR_SEEK:
		case FDR_DRIVE_FAIL:
		case FDR_NOHDR:
		case FDR_NO_ADDRS_MK:
		case FDR_CNTRL_MK:
		case FDR_NO_DATA_MK:
		    /* 
		     * these errors are retriable.
		     */
		    if(fvp->state == FVSTATE_RETRYING) {
			if (--fvp->inner_retry_cnt <= 0) {
			    if(fvp->outer_retry_cnt-- <= 0) {
				/*
				 * we shot our wad. Give up.
				 */
				fd_log_error(fvp, "FATAL");
				fvp->state = FVSTATE_IDLE;
				break;
			    }
			    else {
				/*
				 * Well, let's recalibrate the drive
				 * and try some more.
				 */
				fd_log_error(fvp, "RECALIBRATE");
				fvp->inner_retry_cnt = FV_INNER_RETRY;
				fd_gen_recal(fvp, &fvp->dev_ioreq);
				fvp->state = FVSTATE_RECALIBRATING;
				break;
			    }
			} /* inner retries exhausted */
			else {
			    /* 
			     * OK, try again. No need to remap; we doing 
			     * the same I/O as last time (or at least part of 
			     * it).
			     */
			    fd_log_error(fvp, "RETRY");
			    fd_setup_rw_req(fvp);
			    break;
			}
		    } /* already retrying */
		    else {
			/*
			 * First error for this I/O
			 */
			fd_setup_rw_req(fvp);
			fvp->state = FVSTATE_RETRYING;
			if(fvp->inner_retry_cnt == 0) {
			    /* 
			     * retries disabled. give up. 
			     */
			    fd_log_error(fvp, "FATAL");
			    fvp->state = FVSTATE_IDLE;
			    break;
			}
			fd_log_error(fvp, "RETRY");
			break;
		    } /* starting retry sequence */
		    break;			/* from retryable error case */
		    
		default:			/* non-retriable errors */
		    fd_log_error(fvp, "FATAL");
		    break;
	    } /* switch fdiop->status */
	} /* normal I/O request */ 
#ifdef	FD_DEBUG
	printf("fd_intr: returning; status = %d, state = %d\n", fdiop->status,
		fvp->state);
#endif	FD_DEBUG
	return;
		
} /* fd_intr() */

static void fd_log_error(fd_volume_t fvp, char *err_string)
{
	printf("fd%d: Sector %d(d) cmd = %s; status = %d: %s\n", 
		fvp->volume_num, fvp->start_sect, 
		fvp->io_flags & FVIOF_READ ? "Read" : "Write",
		fvp->dev_ioreq.status, err_string);
}
	
static void fd_setup_rw_req(fd_volume_t fvp)
{
	/* input:  fvp->start_sect
	 *	   fvp->bytes_to_go
	 *	   fvp->start_addrs
	 *	   fvp->io_flags
	 *	   fvp->format_info
	 * output: fvp->current_sect
	 *	   fvp->current_byte_cnt
	 *	   fvp->current_addrs
	 *	   fvp->dev_ioreq
	 * 
	 * This routine determines the largest "safe" I/O which can be
	 * performed starting at fvp->start_sect, up to a max of 
	 * fvp->bytes_to_go bytes.
	 *
	 * No I/O is allowed to go past a track boundary.
	 *
	 * Parameters describing the resulting "safe" I/O are placed in
	 * fvp->current_sect and fvp->current_byte_cnt. An fd_rw_cmd is then
	 * generated and placed in fvp->dev_ioreq.cmd_blk[].
	 */
	struct fd_rw_cmd start_rw_cmd;		/* first sector, phys params */
	struct fd_format_info *fip = &fvp->format_info;
	int num_sects = howmany(fvp->bytes_to_go, fip->sect_size);
	int bytes_to_xfr;
	
#ifdef	FD_DEBUG
	printf("fd_setup_rw_req: start_sect %d bytes_to_go 0x%x\n",
		fvp->start_sect, fvp->bytes_to_go);
#endif	FD_DEBUG
	/*
	 * See if we'll wrap past a track boundary. Max sector number on
	 * a track is sects_per_trk, NOT sects_per_trk-1...
	 */
	fd_log_2_phys(fvp, fvp->start_sect, &start_rw_cmd);
	if((start_rw_cmd.sector + num_sects) > (fip->sects_per_trk+1)) {
		num_sects = fip->sects_per_trk - start_rw_cmd.sector + 1;
		bytes_to_xfr = num_sects * fip->sect_size;
	}
	else {
		/*
		 * The whole I/O fits on one track.
		 */
		bytes_to_xfr = fvp->bytes_to_go;
	}
	fvp->current_sect     = fvp->start_sect;
	fvp->current_byte_cnt = bytes_to_xfr;
	fvp->current_addrs    = fvp->start_addrs;
	fd_gen_rw_cmd(fvp,
		&fvp->dev_ioreq,
		fvp->current_sect,
		fvp->current_byte_cnt,
		fvp->current_addrs,
		fvp->io_flags & FVIOF_READ);	
} /* fd_setup_rw_req() */

/*
 * generate a write or read command in *fdiop (No I/O).
 */
static void fd_gen_rw_cmd(fd_volume_t fvp,
	fd_ioreq_t fdiop,
	u_int sector,
	u_int byte_count,
	caddr_t dma_addrs,
	int read)		/* non-zero ==> read */
{
	struct fd_rw_cmd *cmdp = (struct fd_rw_cmd *)fdiop->cmd_blk;
	struct fd_format_info *fip = &fvp->format_info;
	int ss, n;
	
	bzero(cmdp, SIZEOF_RW_CMD);
	fd_log_2_phys(fvp, sector, cmdp);	/* assign track, head, sect */
	cmdp->mt = 0;				/* multitrack - always false */
	cmdp->mfm = fip->density_info.mfm;
	cmdp->opcode = read ? FCCMD_READ : FCCMD_WRITE;
	cmdp->hds = cmdp->head;
	/*
	 * controller thread writes drive_sel.
	 *
	 * sector size = (2 ** (n-1)) * 256
	 */
	ss = 256;
	n=1;
	while(ss != fip->sect_size) {
		n++;
		ss <<= 1;
	}
	cmdp->sector_size = n;
	/*
	 * eot = the number of the LAST sector to be read/written.
	 */
	cmdp->eot = cmdp->sector + howmany(byte_count, fip->sect_size) - 1;
	cmdp->gap_length = fip->density_info.gap_length;
	cmdp->dtl = 0xff;
	
	fdiop->density = fip->density_info.density;
	fdiop->timeout = TO_RW;
	fdiop->command = FDCMD_CMD_XFR;
	fdiop->num_cmd_bytes = SIZEOF_RW_CMD;
	fdiop->addrs = dma_addrs;
	fdiop->byte_count = byte_count;
	fdiop->num_stat_bytes = SIZEOF_RW_STAT;
	if(read)
		fdiop->flags |= FD_IOF_DMA_RD;
	else
		fdiop->flags &= ~FD_IOF_DMA_RD;
	
} /* fd_gen_rw_cmd() */

/*******************************************
 *	Standard I/O command routines      *
 *******************************************/
 
fd_return_t fd_live_rw(fd_volume_t fvp,
	int sector,			/* raw sector # */
	int sect_count,
	caddr_t addrs,			/* src/dest of data */
	boolean_t read)
{
	int rtn;
	
#ifdef	FD_DEBUG
	printf("fd_live_rw: vol %d sect %d sect_count %d\n", fvp->volume_num,
		sector, sect_count);
#endif	FD_DEBUG
	fvp->start_sect = sector;
	fvp->start_addrs = addrs;	
	fvp->bytes_to_go = sect_count * fvp->format_info.sect_size;
	if(read)
		fvp->io_flags |= FVIOF_READ;
	else
		fvp->io_flags &= ~FVIOF_READ;
	fvp->io_flags &= ~FVIOF_SPEC;
	rtn = fd_start(fvp);
#ifdef	FD_DEBUG
	printf("fd_live_rw: returning %d\n", rtn);
#endif	FD_DEBUG
	return(rtn);
} /* fd_live_rw() */

/*
 * Execute a read or write avoiding retries, remapping,
 * and error logging on console.
 */
 
int fd_raw_rw(fd_volume_t fvp,
	int sector,			/* raw sector # */
	int sect_count,
	caddr_t addrs,			/* src/dest of data */
	boolean_t read)
{
	struct fd_ioreq ioreq;

	bzero(&ioreq, sizeof(struct fd_ioreq));
	fd_gen_rw_cmd(fvp,
		&ioreq,
		sector,
		sect_count * fvp->format_info.sect_size,
		addrs,
		read);
	return(fd_command(fvp, &ioreq));	
} /* fd_raw_rw() */

/*
 * execute a FDCMD_GET_STATUS command. Status returned in *dstatp.
 */
int fd_get_status(fd_volume_t fvp, struct fd_drive_stat *dstatp)
{
	struct fd_ioreq ioreq;
	int rtn;
	
	bzero(&ioreq, sizeof(ioreq));
	ioreq.command = FDCMD_GET_STATUS;
	ioreq.timeout = TO_SIMPLE;
	rtn = fd_command(fvp, &ioreq);
	*dstatp = ioreq.drive_stat;
	return(rtn);
}

/*
 * execute recalibrate command.
 */
int fd_recal(fd_volume_t fvp)
{
	struct fd_ioreq ioreq;
	int rtn;
	
	fd_gen_recal(fvp, &ioreq);
	rtn = fd_command(fvp, &ioreq);
	return(rtn);
}

/*
 * execute a simple command (like FDCMD_MOTOR_OFF).
 */
int fd_basic_cmd(fd_volume_t fvp, int command)
{
	struct fd_ioreq ioreq;
	int rtn;
	
	bzero(&ioreq, sizeof(ioreq));
	ioreq.command = command;
	ioreq.timeout = TO_SIMPLE;
	rtn = fd_command(fvp, &ioreq);
	if(rtn == FDR_SUCCESS)
		rtn = ioreq.status;
	return(rtn);

} /* fd_basic_cmd() */


/*
 * generate a recalibrate command in *fdiop.
 */
static void fd_gen_recal(fd_volume_t fvp, fd_ioreq_t fdiop)
{
	struct fd_recal_cmd *cmdp = (struct fd_recal_cmd *)fdiop->cmd_blk;
	
	cmdp->opcode = FCCMD_RECAL;
	cmdp->rsvd1 = 0;
	cmdp->drive_sel = 0;			/* to be changed by controller
						 * code */
	fdiop->density = fvp->format_info.density_info.density;
	fdiop->timeout = TO_RECAL;
	fdiop->command = FDCMD_CMD_XFR;
	fdiop->num_cmd_bytes = sizeof(struct fd_recal_cmd);
	fdiop->addrs = 0;
	fdiop->byte_count = 0;
	fdiop->num_stat_bytes = sizeof(struct fd_int_stat);
} /* fd_gen_recal() */

/*****************************************
 * 	miscellaneous subroutines        *
 *****************************************/
/*
 * convert logical sector # into cylinder, head, sector. No range checking.
 * Physical parameters are obtained from *fvp->labelp and fvp->sects_per_track.
 *
 * First sector on a track is sector 1.
 */
static void fd_log_2_phys(fd_volume_t fvp,
	u_int sector,
	struct fd_rw_cmd *cmdp)
{
	u_int track;
	struct fd_format_info *fip = &fvp->format_info;
	
	track = sector / fip->sects_per_trk;
	cmdp->cylinder = track / fip->disk_info.tracks_per_cyl;
	cmdp->head     = track % fip->disk_info.tracks_per_cyl;
	cmdp->sector   = sector % fip->sects_per_trk + 1;
}

/*
 * create a new volume struct for the specified volume #.
 */
void fd_new_fv(fd_volume_t fvp, int vol_num)
{
	bzero(fvp, sizeof(struct fd_volume));
	fvp->drive_num = DRIVE_UNMOUNTED;	/* i.e., not inserted */
	fvp->volume_num = vol_num;
	fvp->format_info.density_info.density = FD_DENS_NONE;
	fvp->format_info.flags = 0;
	fvp->flags = FVF_VALID;
	fvp->io_flags = 0;
	return;
} /* fd_new_fv() */

void fd_set_density_info(fd_volume_t fvp, int density)
{
	/*
	 * Set up fvp->format_info.density_info per density. Density = 
	 * FD_DENS_NONE marks the disk as unformatted.
	 */
	struct fd_density_info *deip;
	
	for(deip = fd_density_info; 
	    deip->density != FD_DENS_NONE;
	    deip++) {
	    	if(density == deip->density) 
			break;
	}
	fvp->format_info.density_info = *deip;
	if(density == FD_DENS_NONE) {
		fvp->format_info.flags &= ~FFI_FORMATTED;
	}
}
			
int fd_set_sector_size(fd_volume_t fvp, int sector_size)
{
	struct fd_format_info *fip = &fvp->format_info;
	int good_size;
	int ok=0;
	
	fip->sect_size = sector_size;
	fip->sects_per_trk = (fip->density_info.capacity / 
			(fip->disk_info.tracks_per_cyl * 
			 fip->disk_info.num_cylinders)) / fip->sect_size;
	fip->total_sects = fip->sects_per_trk *
			    fip->disk_info.tracks_per_cyl * 
			    fip->disk_info.num_cylinders;
	fip->flags |= FFI_FORMATTED;
	fip->flags &= ~FFI_LABELVALID;
	return(0);
}

/* end of fd_subr.c */






