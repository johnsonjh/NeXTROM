/*	@(#)fd_cmds.c	2.0	02/23/90	(c) 1990 NeXT	*/

/* 
 * fd_cmds.c -- I/O commands for Floppy Disk driver
 * PROM MONITOR VERSION
 *
 * HISTORY
 * 03-Apr-90	Doug Mitchell at NeXT 
 *	Created.
 */

#import <sys/types.h>
#import <sys/param.h>
#import <next/psl.h>
#import <next/cpu.h>
#import <nextdev/dma.h>
#import <mon/sio.h>
#import <mon/global.h>
#import <dev/fd_extern.h>
#import	<nextdev/fd_reg.h>
#import	<dev/fd_vars.h>

/*
 * prototypes of local functions:
 */
static fd_return_t fc_wait_intr(fd_controller_t fcp, int us_time);
static int dma_bytes_moved(struct dma_chan *dcp, dma_list_t dhp);
static void fc_dma_init(fd_controller_t fcp);

/*
 * Standard controller command routines. Command to be executed is in
 * *fcp->fdiop.
 */

void fc_cmd_xfr(fd_controller_t fcp, fd_volume_t fvp)
{
	register fd_cntrl_regs_t fcrp = fcp->fcrp;
	fd_ioreq_t fdiop = fcp->fdiop;
	fd_return_t rtn;
	u_char motor_bit;
	u_char opcode = fdiop->cmd_blk[0] & FCCMD_OPCODE_MASK;
				
	/* 
 	 * select drive (in the command block to be sent). Why does the
	 * controller make us do this here and in fcrp->dor?
	 */
#ifdef	FD_DEBUG
	printf("fc_cmd_xfr: opcode %d\n", opcode);
#endif	FD_DEBUG
	switch(opcode) {
	    case FCCMD_VERSION:
	    case FCCMD_SPECIFY:
	    case FCCMD_CONFIGURE:
	    	break;
	    default:
	    	fdiop->cmd_blk[1] |= fcp->fdiop->unit;
		break;
	}
	
	/*
	 * Select Density. Skip if density hasn't changed since the last time 
	 * we came this way.
	 */
	if(fdiop->density != fcp->last_dens) {		
	   	if(rtn = fc_configure(fcp, fdiop->density)) { 
			fdiop->status = rtn;
			return;
		}
		if(rtn = fc_specify(fcp,
				fdiop->density,
				&fd_drive_info[fvp->drive_type])) {
			fdiop->status = rtn;
			return;
		}
	}
	/*
	 * start motor if necessary. Skip if motor is already on or we're
	 * executing a command which does not require the disk to be spinning.
	 */
	motor_bit = DOR_MOTEN0 << fdiop->unit;
	if(!(fcrp->dor & motor_bit)) {
		switch(opcode) {
		    case FCCMD_READ:
 		    case FCCMD_READ_DELETE:
		    case FCCMD_WRITE:
		    case FCCMD_WRITE_DELETE:
		    case FCCMD_READ_TRACK:
		    case FCCMD_VERIFY:
		    case FCCMD_FORMAT:
  		    case FCCMD_RECAL:
		    case FCCMD_READID:
		    case FCCMD_SEEK:
			fc_motor_on(fcp);
			/*
			 * Now that we know the motor is on, check for 
			 * Drive present.
			 */
			if(fcp->fcrp->flpctl & FLC_DRIVEID) {	/* true low */
				fdiop->status = FDR_BADDRV;
#ifdef	FD_DEBUG
				printf("fc_cmd_xfr: DRIVE NOT PRESENT\n");
#endif	FD_DEBUG
				return;
			}
			break;
		    default:
		    	break;
		}
	}
	/*
	 * go for it.
	 */
	fdiop->status = fc_send_cmd(fcp, fdiop);
#ifdef	FD_DEBUG
	printf("fc_cmd_xfr: done: status = %d\n", fdiop->status);
#endif	FD_DEBUG
} /* fc_cmd_xfr() */

#ifdef	INCLUDE_EJECT
void fc_eject(fd_controller_t fcp)
{
	struct fd_ioreq ioreq;
	fd_return_t rtn;
	u_char motor_bit;
	
	/*
	 * First turn the motor on if it's off.
	 */
	motor_bit = DOR_MOTEN0 << fcp->fdiop->unit;
	if(!(fcp->fcrp->dor & motor_bit))
		fc_motor_on(fcp);

	/*
	 * Seek to the parking track.
	 */
	bzero(&ioreq, sizeof(ioreq));
	fd_gen_seek(fcp->last_dens, &ioreq, FD_PARK_TRACK, 0);
	if(rtn = fc_send_cmd(fcp, &ioreq)) {
		fcp->fdiop->status = rtn;
		return;
	}
	
	/*
	 * Eject the disk.
	 */
	fc_flpctl_bset(fcp, FLC_EJECT);
	delay(TO_EJECT_PULSE);
	fc_flpctl_bclr(fcp, FLC_EJECT);
	fc_wait_intr(fcp, TO_EJECT*1000);	
					/* just delay; no interrupt */
	/*
	 * Turn motor off.
	 */
	fc_motor_off(fcp);
	fcp->fdiop->status = FDR_SUCCESS;
} /* fc_eject() */

/*
 * generate a seek command to specified track in *fdiop.
 */
void fd_gen_seek(int density, fd_ioreq_t fdiop, int cyl, int head)
{
	struct fd_seek_cmd *cmdp = 
		(struct fd_seek_cmd *)fdiop->cmd_blk;
	
	cmdp->opcode = FCCMD_SEEK;
	cmdp->relative = 0;
	cmdp->dir = 0;
	cmdp->rsvd1 = 0;		/* cheaper than a bzero */
	cmdp->hds = head;
	cmdp->cyl = cyl;
	fdiop->density = density;
	fdiop->timeout = TO_SEEK;
	fdiop->command = FDCMD_CMD_XFR;
	fdiop->num_cmd_bytes = SIZEOF_SEEK_CMD;
	fdiop->addrs = 0;
	fdiop->byte_count = 0;
	fdiop->num_stat_bytes = sizeof(struct fd_int_stat);

} /* fd_gen_seek() */

#endif	INCLUDE_EJECT


void fc_motor_on(fd_controller_t fcp) 
{
	u_char motor_bit;
	
	motor_bit = DOR_MOTEN0 << fcp->fdiop->unit;
	if(fcp->fcrp->dor & motor_bit)
		return;
	fcp->fcrp->dor |= motor_bit;
	fc_wait_intr(fcp, TO_MOTORON*1000);	
					/* just delay; no interrupt */
} /* fc_motor_on() */

void fc_motor_off(fd_controller_t fcp) 
{
	u_char motor_bit;
	
	motor_bit = DOR_MOTEN0 << fcp->fdiop->unit;
	fcp->fcrp->dor &= ~motor_bit;
} /* fc_motor_off() */

/*
 * 	Common I/O subroutines
 */
/* #define DUMP_DATA 1	/* */
/* #define DMA_DUMP  1	/* */

fd_return_t fc_send_cmd(fd_controller_t fcp, fd_ioreq_t fdiop) 
{
	/*
	 * send command in fdiop->cmd_blk; DMA to/from fdiop->addrs;
	 * place status (from result phase) in fdiop->stat_blk[]. Good
	 * status is verified (for all the commands we know about at least).
	 */
	int i;
	fd_return_t rtn = FDR_SUCCESS;
	u_char *csp;
	u_char opcode;
	char *dma_start_addrs;
	boolean_t no_interrupt=FALSE;
	boolean_t dma_enable = FALSE;
	int requested_byte_count = fdiop->byte_count;
	int direction;
	int do_pad = 0;
	int dma_after=0;
	struct fd_rw_stat *rw_statp = (struct fd_rw_stat *)fdiop->stat_blk;
	
	opcode = fdiop->cmd_blk[0] & FCCMD_OPCODE_MASK;
#ifdef	FD_DEBUG
	printf("fc_send_cmd: opcode %d\n", opcode);
#endif	FD_DEBUG
	fdiop->status = 0xffffffff;		/* Invalid */
	fdiop->cmd_bytes_xfr = 0;
	fdiop->bytes_xfr = 0;
	fdiop->stat_bytes_xfr = 0;
	
	/*
	 * Set up DMA if enabled.
	 */	
	if(requested_byte_count > 0) {
		
		/*
		 * We require well-aligned transfers since we have no way 
		 * of detecting the transfer count at the device level - we
		 * have to rely on the DMA engine.
		 *
		 * Since some commands (particularly format) can't live with
		 * this restriction, we'll only enforce it for reads and 
		 * writes.
		 *
		 * Warning: short writes do not work. Higher level code must
		 * provide full sector buffers.
		 */
		if((opcode == FCCMD_READ) || (opcode == FCCMD_WRITE)) {
			if((! DMA_BEGINALIGNED(fdiop->addrs)) ||
			   (! DMA_ENDALIGNED(requested_byte_count))) {
				rtn = FDR_REJECT;
				goto out;
			}
		}
		if(requested_byte_count > 0x100000) {
			rtn = FDR_REJECT;
			goto out;
		}
		fc_dma_init(fcp);
		direction = (fdiop->flags & FD_IOF_DMA_DIR) == FD_IOF_DMA_RD ? 
				DMACSR_READ : DMACSR_WRITE;
		/*
		 * Kludge: writing at 250 KHz (i.e., FD_DENS_1) requires the
		 * use of the FIFO to avoid DMA hangs resulting from defects
		 * in the 82077. If we use the FIFO, we have to allow for
		 * more data to be transferred to the 82077 than we really 
		 * want - enough to fill the FIFO after the actual data
		 * has moved. For now, just add on I82077_FIFO_SIZE bytes
		 * to the DMA if we're at the low data rate. For writes and 
		 * reads, we'll truncate to a sector size.
		 */
		dma_list(&fcp->dma_chan, 
			fcp->dma_list, 
			fdiop->addrs,
		    	requested_byte_count,
			direction);
#ifdef	DMA_DUMP
		printf("fc_send_cmd: dh_start 0x%x dh_stop 0x%x dc_tail "
			" 0x%x fdiop 0x%x\n",
			fcp->dma_list[0].dh_start, fcp->dma_list[0].dh_stop,
			fcp->dma_chan.dc_tail, fdiop);
#endif	DMA_DUMP
		dma_start(&fcp->dma_chan, 
			fcp->dma_list, 
			direction);
		dma_enable = TRUE;
	}
	
	/*
	 * enable interrupt mechanism.
	 */
	fcp->fdiop_i = fdiop;
		
	/*
	 * send command bytes. FIXME: add loop counter.
	 */
	csp = fdiop->cmd_blk;
	for(i=0; i<fdiop->num_cmd_bytes; i++) {
		rtn = fc_send_byte(fcp, *csp++);
		switch(rtn) {
		    case FDR_SUCCESS:
			fdiop->cmd_bytes_xfr++;
			break;				/* go for another */
		    case FDR_BADPHASE:
		    	/*
			 * We're in status phase...abort. Controller needs
			 * a reset. (FIXME: detect illegal command, or just
			 * blow it off?)
			 *
			 * We'd kind of like to do a reset right now, but 
			 * fc_82077_reset does a configure command, which
			 * calls us...if things are really screwed up, 
			 * we'll call fc_82077_reset forever, recursively.
			 * Just flag the bad hardware state.
			 */
			fcp->flags |= FCF_NEEDSINIT;
			goto out;
		    default:
		    	printf("fc_send_cmd: Error sending command bytes "
				" (%d)\n", rtn);
			fcp->flags |= FCF_NEEDSINIT;
			goto out;			/* no hope */
		}
	}

	/*
	 * Switch hardware to DMA mode if appropriate.
	 */
	if(requested_byte_count > 0) {
		fcp->flags |= FCF_DMAING;
		if((fdiop->flags & FD_IOF_DMA_DIR) == FD_IOF_DMA_RD) {
			*fcp->sczctl |= SDC_DMAREAD;
			fcp->flags |= FCF_DMAREAD;
		}
		else {
			*fcp->sczctl &= ~SDC_DMAREAD;
			fcp->flags &= ~FCF_DMAREAD;
		}
		*fcp->sczctl |= SDC_DMAMODE;
	}
	else
		fcp->flags &= ~FCF_DMAING;
	
	/*
	 * For most commands, we wait for result phase.
	 *  
	 * Note that for commands which require a Sense
	 * Interrupt Status command at interrupt, the command is sent by 
	 * the interrupt handler. For all other commands, the interrupt handler
	 * moved the first status byte to fdiop->stat_blk[] upon interrupt.
	 */
	switch(opcode) {
	    case FCCMD_INTSTAT:		/* sense interrupt status */
	    case FCCMD_SPECIFY:
	    case FCCMD_DRIVE_STATUS:
	    case FCCMD_CONFIGURE:
	    case FCCMD_VERSION:
	    case FCCMD_DUMPREG:
	    case FCCMD_PERPENDICULAR:
	    	break;			/* no interrupt for these */
	    default:
		rtn = fc_wait_intr(fcp, fdiop->timeout * 1000);
		break;
	}
	/*
	 * IF DMAing, flush DMA fifo and get back to PIO mode
	 */
	if(fcp->flags & FCF_DMAING) {
		
		int s;
		
		/*
		 * Flush the DMA fifo (if fc_intr hasn't already done so).
		 */
		if(*fcp->sczctl & SDC_DMAMODE) {
			for(i=0; i<8; i++) {
				*fcp->sczctl |= SDC_FLUSH;
				delay(5);
				*fcp->sczctl &= ~SDC_FLUSH;
				delay(5);
			}
			*fcp->sczctl &= ~SDC_DMAMODE;
		}
	}

	switch(rtn) {
	    case FDR_SUCCESS:
	    	break;
	    case FDR_TIMEOUT: 
	    	no_interrupt = TRUE;	/* proceed to get status */
		break;
	    default:
		goto out;
	}
	
	/*
	 * Attempt to move specified number of status bytes. (We may be 
	 * starting at stat_blk[1]...). Stop if the controller goes into
	 * "command" phase as well...
	 */
	csp = &fdiop->stat_blk[fdiop->stat_bytes_xfr];
	for(i=fdiop->stat_bytes_xfr; i<fdiop->num_stat_bytes; i++) {
		rtn = fc_get_byte(fcp, csp++);
		switch(rtn) {
		    case FDR_SUCCESS:
			fdiop->stat_bytes_xfr++;	
			break;				/* go for another */
		    case FDR_BADPHASE:
		    	if(fdiop->stat_bytes_xfr)
				goto parse_status;	/* make some sense
							 * out of what we 
							 * have */
			else 
				goto out;		/* no hope */
				
		    default:
		    	printf("fc_send_cmd: Error getting status bytes\n");
			goto out;			/* no hope */
		}
	}
parse_status:
	/*
	 * Handle DMA stuff:
	 *	-- check for DMA errors
	 *	-- get bytes transferred from DMA engine
	 */
	if(fcp->flags & FCF_DMAING) {
		
		fcp->flags &= ~FCF_DMAING;
		/*
		 * If this was a successful write command, dma_bytes_moved
		 * doesn't work if the write succeeded because of the 'after' 
		 * mechanism used to fill the FIFO. 
		 */
		if(((opcode == FCCMD_WRITE) || 
		    (opcode == FCCMD_WRITE_DELETE)) &&
		   (rw_statp->stat1 == SR1_EOCYL)) {
		   	fdiop->bytes_xfr = fdiop->byte_count;
		}
		else {
			fdiop->bytes_xfr = dma_bytes_moved(&fcp->dma_chan,
			 		                   fcp->dma_list);
		}
		if(fdiop->bytes_xfr > fdiop->byte_count) {
			fdiop->bytes_xfr = fdiop->byte_count;
		}
#ifdef	DMA_DUMP
		printf("fc_send_cmd: bytes_xfr = 0x%x byte_count = 0x%x\n",
			fdiop->bytes_xfr, fdiop->byte_count);
#endif	DMA_DUMP
		if(fcp->flags & FCF_DMAERROR) {
			rtn = FDR_MEMFAIL;
		}
		dma_abort(&fcp->dma_chan);
		dma_enable = FALSE;
		fc_dma_init(fcp);	/* clean up so SCSI doesn't have to do
					 * this bullshit. */
#ifdef	DUMP_DATA
		if(fdiop->bytes_xfr && (opcode == FCCMD_READ)) {
			int i;
			
			printf("Read Data: ");
			for(i=0; i<0x10; i++)
				printf("%x ", ((u_char *)fdiop->addrs)[i]);
			printf("\n");
		}			 
#endif	DUMP_DATA
	}

	/*
	 * OK, parse returned status. 
	 */
	if(rtn == FDR_SUCCESS) {
		switch(opcode) {
		    /* 
		     * handle all of the standard read/write commands.
		     */
		    case FCCMD_READ:
		    case FCCMD_WRITE:
		    case FCCMD_READID:
		    {
			u_char sbyte;
			
			if(fdiop->stat_bytes_xfr == 0) {
				/*
				 * Insufficient status info. Punt.
				 */
				rtn = FDR_CNTRLR;
				break;
			}
			sbyte = rw_statp->stat0 & SR0_INTCODE;
			if(sbyte != INTCODE_COMPLETE) {
				/* end of cylinder is normal I/O complete
				 * for read and write.
				 */
				if(rw_statp->stat1 == SR1_EOCYL)
					break;	
			    	/*
			     	 * Special case for read and write - if we
				 * were trying to move a partial sector, 
				 * INTCODE_ABNORMAL is expected. We ignore
				 * this status if we moved all of the data
				 * we tried to and the controller is reporting
				 * over/underrun. Kludge. 
				 */
				if((fdiop->byte_count != 0) &&
				   (sbyte == INTCODE_ABNORMAL) &&
				   (fdiop->byte_count == fdiop->bytes_xfr) &&
				   (rw_statp->stat1 & SR1_OURUN))
				   	break;
					
				rtn = FDR_MEDIA;
				break;
			}
			break;
		    }
#ifdef	INCLUDE_EJECT
		    case FCCMD_SEEK:
		    {
		     	struct fd_seek_cmd *cmdp = 
				(struct fd_seek_cmd *)fdiop->cmd_blk;
		    	struct fd_int_stat *statp =
				(struct fd_int_stat *)fdiop->stat_blk;

			/*
			 * verify Seek End.
			 */
			if((statp->stat0 & SR0_SEEKEND) == 0) {

				/* No Seek Complete */
				rtn = FDR_SEEK;
				break;
			}
		    	/*
			 * verify we're on the right track. We got the current
			 * cylinder # from the Sense Interrupt Status command.
			 */
			cmdp = (struct fd_seek_cmd *)fdiop->cmd_blk;
			statp = (struct fd_int_stat *)fdiop->stat_blk; 
			if(cmdp->relative)
				break;		/* can't verify relative seek's
						 * destination */
			else if(statp->pcn != cmdp->cyl) {
				rtn = FDR_SEEK;
			}
			break;
			
		    } /* case FCCMD_SEEK */
#endif	INCLUDE_EJECT

		    case FCCMD_RECAL:
		    {
		     	struct fd_seek_cmd *cmdp = 
				(struct fd_seek_cmd *)fdiop->cmd_blk;
		    	struct fd_int_stat *statp =
				(struct fd_int_stat *)fdiop->stat_blk;

		    	/*
			 * verify we're on track 0. We got the current
			 * cylinder # from the Sense Interrupt Status command.
			 */
			statp = (struct fd_int_stat *)fdiop->stat_blk; 
			/*
			 * verify Seek End, and controller and drive both 
			 * saying we're on track 0.
			 */
			if((statp->stat0 & SR0_SEEKEND) == 0) {

				/* No Seek Complete */
				rtn = FDR_SEEK;
				break;
			}
			if(statp->pcn != 0) {
				rtn = FDR_SEEK;
				break;
			}
			if(fcp->fcrp->sra & SRA_TRK0_NOT) {
			 	rtn = FDR_SEEK;
			}
			break;
			
		    } /* case FCCMD_RECAL */
		    
		    /* 
		     * Now the others with no interesting status:
		     */
		    default:
			 break;
		
		} /* switch command */
	}
out:
	/*
	 * Cleanup controller state.
	 */
	if(dma_enable)
		dma_abort(&fcp->dma_chan);
	if(no_interrupt)
		rtn = FDR_TIMEOUT;
#ifdef	FD_DEBUG
	printf("fc_send_cmd: returning %d\n", rtn);
#endif	FD_DEBUG
	return(rtn);
} /* fc_send_cmd() */

static void fc_dma_init(fd_controller_t fcp) 
{
	/*
	 * King Kludge: inititialize DMA channel. Per K. Grundy's
	 * suggestion, we have to enable a short DMA transfer from
	 * device to memory, then disable it. To be clean about
	 * the dma_chan struct and DMa interrupts, for now we'll
	 * actually do dma_list, etc...FIXME. optimize so this 
	 * isn't so much of a performance hit!
	 */
	char dummy_array[0x20];
	char *dummy_p;
	
	dummy_p = DMA_ENDALIGN(char *, dummy_array);
	dma_list(&fcp->dma_chan, 
		fcp->dma_list, 
		(caddr_t)dummy_p,
		0x10,
		DMACSR_READ);
	dma_start(&fcp->dma_chan, 
		fcp->dma_list, 
		DMACSR_READ);
	delay(10);
	dma_abort(&fcp->dma_chan);
}

static int dma_bytes_moved(struct dma_chan *dcp, dma_list_t dhp)
{
	/*
	 * Determine how many bytes actually transferred during a DMA using
	 * *dcp and dma_list. It is assumed that the current value of next
	 * in hardware points to (last address transferred + 1) and that
	 * dma_list has not been disturbed since the DMA.
	 *
	 * FIXME: this is not bulletproof. Here's a case where it will fail:
	 *	first dma_hdr.dh_start = 1000
	 *	             .dh_stop  = 2000
	 *      ...
	 *      nth   dma_hdr.dh_start = 2000
	 *	             .dh_stop  = 3000
	 *	...
	 *	At end of DMA, dd_next = 2000. 
	 *
	 *	Did the DMA end at the end of the first buffer or the 
	 *	start of the nth buffer? The code below will claim that the 
	 *	DMA ended at the end of the first buffer...
	 */
	int hdr_num;
	int bytes_xfr=0;
	char *end_addrs = dcp->dc_ddp->dd_next;
	
#ifdef	DMA_DUMP
	printf("dma_bytes_moved: dd_next 0x%x dd_limit 0x%x\n",
		end_addrs, dcp->dc_ddp->dd_limit);
#endif	DMA_DUMP
	for(hdr_num=0; hdr_num<NDMAHDR; hdr_num++) {
		if((end_addrs < dhp->dh_start) ||
		   (end_addrs > dhp->dh_stop)) {
		   	/* 
			 * end not in this buf; must have moved the whole
			 * thing.
			 */
#ifdef	DMA_DUMP
			printf("dma_bytes_moved: whole buf dh_start 0x%x "
				"dh_stop 0x%x\n", dhp->dh_start, dhp->dh_stop);
#endif	DMA_DUMP
			goto next_buf;
		}
		else if((dcp->dc_current == dhp) ||
		        ((end_addrs > dhp->dh_start) && 
		        (end_addrs < dhp->dh_stop))) {
			/*
			 * case 1: dcp->dc_current == dhp. 
			 * This is where we stopped in the case of 
			 * an aborted DMA - dc_current non-NULL always implies
			 * "no DMA complete".
			 *
			 * Case 2 - unamibiguous termination in this buf.
			 *
			 * In either case, this is where we stopped.
			 */
#ifdef	DMA_DUMP
			printf("dma_bytes_moved: last buf dh_start 0x%x "
				"dh_stop 0x%x\n", dhp->dh_start, dhp->dh_stop);
#endif	DMA_DUMP
			bytes_xfr += end_addrs - dhp->dh_start;
			return(bytes_xfr);
		}
		else if(end_addrs == dhp->dh_stop) {
			/*
			 * The h/w next pointer points to the end of this 
			 * buf. We moved the entire buf. We're done if 
			 * no other bufs were enabled.
			 */
			if((dhp->dh_state != DMASTATE_1STDONE) ||
			   (dhp->dh_link == NULL)) {
				/*
				 * this is where we stopped.
				 */
				bytes_xfr += end_addrs - dhp->dh_start;
#ifdef	DMA_DUMP
				printf("dma_bytes_moved: end buf dh_start "
					"0x%x dh_stop 0x%x\n", dhp->dh_start,
					dhp->dh_stop);
#endif	DMA_DUMP
				return(bytes_xfr);
			}
		}
next_buf:
		/*
		 * This buf is not the end.
		 */ 
#ifdef	DMA_DUMP
		printf("dma_bytes_moved: more dh_start 0x%x dh_stop 0x%x\n",
			dhp->dh_start, dhp->dh_stop);
#endif	DMA_DUMP
		bytes_xfr += dhp->dh_stop - dhp->dh_start;
		dhp++;
	}
	printf("dma_bytes_moved: DMA buf overflow");
} /* dma_bytes_moved() */

static fd_return_t fc_wait_intr(fd_controller_t fcp, int us_time)
{
	/*
	 * Sets up timeout of us_time microseconds; then waits for
	 * interrupt or timeout. Returns FDR_SUCCESS or FDR_TIMEOUT.
	 *
	 * fc_enable_intr() must be called before the earliest possible time 
	 * an interrupt could occur (like before sending command bytes).
	 */
	int s;
	fd_return_t rtn = FDR_SUCCESS;	
	int loop;
	struct mon_global *mg = restore_mg();
	volatile int *intrstatus = mg->mg_intrstat;
	int loop_count = (us_time / 1000) + 1;
	
	/*
	 * no timeouts or threads - count loops.
	 */
	fcp->flags &= ~FCF_TIMEOUT;
	for(loop=0; loop<loop_count; loop++) {
		if (*intrstatus & I_BIT(I_PHONE)) {
			fc_intr(fcp);
			break;
		}
		delay(1000);
	}
	if(loop == loop_count) {
		fcp->flags |= FCF_TIMEOUT;
#ifdef	FD_DEBUG
		printf("fc_wait_intr: Interrupt Timeout %d uS\n", us_time);
#endif	FD_DEBUG
	}
	/*
	 * disable further interrupts
	 */
	if(fcp->flags & FCF_TIMEOUT) {
		rtn = FDR_TIMEOUT;
	}
#ifdef	FD_DEBUG
	printf("fc_wait_intr: %d uS\n", us_time);
#endif	FD_DEBUG
	return(rtn);
	
} /* fc_wait_intr() */

/* end of fd_cmds.c */





