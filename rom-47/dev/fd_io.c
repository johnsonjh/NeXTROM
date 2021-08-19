/*	@(#)fd_io.c	2.0	01/24/90	(c) 1990 NeXT	*/

/* 
 * fd_io.c -- I/O routines for Floppy Disk driver 
 * PROM MONITOR VERSION
 *
 * HISTORY
 * 03-Apr-90	Doug Mitchell at NeXT
 *	Created.
 *
 */ 
 
#import <next/cpu.h>
#import <sys/time.h>
#import <next/psl.h>
#import <nextdev/dma.h>
#import <mon/sio.h>
#import <dev/fd_extern.h>
#import	<nextdev/fd_reg.h>
#import	<dev/fd_vars.h>
#import <nextdev/odreg.h>

/*
 * prototypes of local functions
 */
static fd_return_t fc_wait_pio(fd_controller_t fcp, u_char poll_bits);
static void fc_update_stat(fd_controller_t fcp, fd_ioreq_t fdiop);

/* 	
 *	initialization
 */
int fc_init(struct fd_global *fdgp, caddr_t hw_reg_ptr)
{
	fd_controller_t fcp = &fdgp->fd_controller;
	fd_cntrl_regs_t fcrp;
	u_char drive_sel;
	int i;
	int rtn;
	
	/* 
	 * init fd_controller struct for this chip
	 */
#ifdef	FD_DEBUG
	printf("fc_init\n");
#endif	FD_DEBUG
	fcrp = fcp->fcrp = (fd_cntrl_regs_t)hw_reg_ptr;
	fcp->flags = 0;
	fcp->flpctl_sh = fcp->fcrp->flpctl = FLC_82077_SEL;
	fcp->last_dens = FD_DENS_NONE;
	fdgp->fd_volume.flags &= ~FVF_VALID;

	rtn = fc_82077_reset(fcp, NULL);
	
	if(rtn) 
		return(rtn);
	/*
	 * initialize dma_chan and interrupt linkage
	 *
	 * Note; SCSI has already init'd the hardware; it's going to happen 
	 * again in dma_init...no way to avoid it, but there's no problem
	 * since we own the hardware at this point.
	 */
	fcp->dma_chan.dc_handler = NULL;
	fcp->dma_chan.dc_hndlrarg = (int)fcp;
	fcp->dma_chan.dc_hndlrpri = CALLOUT_PRI_SOFTINT1;
	fcp->dma_chan.dc_flags = 0;
#ifdef	DMA_PAD
	fcp->dma_chan.dc_flags |= DMACHAN_PAD;
#endif	DMA_PAD
	fcp->dma_chan.dc_ddp = (struct dma_dev *)P_SCSI_CSR;
	dma_init(&fcp->dma_chan, I_SCSI_DMA);
	fcp->flags |= FCF_INITD;
	fcp->sczctl = (u_char *)SDC_ADDRS;
	fcp->sczfst = (u_char *)SFS_ADDRS;

	return 0;
	
} /* fc_probe() */

void fc_docmd(fd_volume_t fvp)
{
	fd_controller_t fcp = fvp->fcp;
	int unit;
	int i;
	fd_ioreq_t fdiop;
	int device;
	int s;
	u_char motor_bit;
	
#ifdef	FD_DEBUG
	printf("fc_docmd: vol %d cmd %d opcode %d\n", fvp->volume_num,
		fvp->dev_ioreq.command, fvp->dev_ioreq.cmd_blk[0]);
#endif	FD_DEBUG
	fdiop = fcp->fdiop = &fvp->dev_ioreq;
	fdiop->unit = fvp->unit;
		
	/*
	 * initialize return parameters and flags.
	 */
	fdiop->status         = -1;	/* must be written by
					    * command handler */
	fdiop->cmd_bytes_xfr  = 0;
	fdiop->bytes_xfr      = 0;
	fdiop->stat_bytes_xfr = 0;			
	fcp->flags &= ~(FCF_TIMEOUT | FCF_DMAERROR | FCF_DMAING );
		
	/*
	 * if the controller chip got into serious trouble last 
	 * time thru, reset it.
	 */
	if(fcp->flags & FCF_NEEDSINIT) {
		fdiop->status = fc_82077_reset(fcp, NULL);
		if(fdiop->status) {
			goto hw_done;
		}
	}
	/*
	 * select drive.  
	 */
	if(fdiop->unit >= NUM_UNITS) {
		fdiop->status = FDR_BADDRV;
		goto abort;
	}
	fcp->fcrp->dor |= ((fcp->fcrp->dor & ~DOR_DRVSEL_MASK) | 
			    fdiop->unit);			
	/*
	 * dispatch to the appropriate command routine. 
	 */
	switch(fcp->fdiop->command) {
	    case FDCMD_CMD_XFR:
		fc_cmd_xfr(fcp, fvp);
		break;
#ifdef	INCLUDE_EJECT
	    case FDCMD_EJECT:
		fc_eject(fcp);
		fvp->drive_num = DRIVE_UNMOUNTED;
		break;
#endif	INCLUDE_EJECT
	    case FDCMD_MOTOR_ON:
		fc_motor_on(fcp);
		fcp->fdiop->status = FDR_SUCCESS;
		break;
	    case FDCMD_MOTOR_OFF:
		fc_motor_off(fcp);
		fcp->fdiop->status = FDR_SUCCESS;
		break;
	    case FDCMD_GET_STATUS:
		/*
		 * nothing to do; we'll pick up the status below.
		 */
		fdiop->status = FDR_SUCCESS;
		break;
	    default:
		fdiop->status = FDR_REJECT;
		break;
	}
	/*
	 * Update media_id and motor_on.
	 */
	fc_update_stat(fcp, fcp->fdiop);
	motor_bit = DOR_MOTEN0 << fcp->fdiop->unit;
	if(fcp->fcrp->dor & motor_bit)
		fvp->drive_flags |= FDF_MOTOR_ON;
	else
		fvp->drive_flags &= ~FDF_MOTOR_ON;
	
	/*
	 * command complete (or aborted).
	 */
abort:
	/*
	 * Reset hardware if we timed out or got into serious
	 * trouble.
	 */
	if(fdiop->status == FDR_TIMEOUT) {
		/*
		 * Timeouts can happen as a result of reading at the wrong 
		 * density...
		 */
#ifdef	FD_DEBUG
		fc_82077_reset(fcp, "Command Timeout");
		printf("Command 0x%x  Opcode 0x%x\n", fdiop->command,
			fdiop->cmd_blk[0]);
#else	FD_DEBUG
		fc_82077_reset(fcp, NULL);
#endif	FD_DEBUG
	}
	if(fdiop->status == FDR_BADPHASE) 
		fc_82077_reset(fcp, "Bad Controller Phase");
	if(fcp->flags & FCF_NEEDSINIT)
		fc_82077_reset(fcp, "Controller hang");
hw_done:
	fd_intr(fvp);
#ifdef	FD_DEBUG
	printf("fc_docmd: returning; status =  %d\n", fdiop->status);
#endif	FD_DEBUG
	return;
} /* fc_docmd() */

void fc_intr(fd_controller_t fcp)
{	
	/*
	 * hardware-level interrupt handler. Called by fc_send_cmd().
	 */
	fd_return_t rtn;
	fd_cntrl_regs_t fcrp = fcp->fcrp;
	int i;
		
	/*
	 * First, if we are DMAing, flush the DMA fifo and get hardware 
	 * back to PIO mode.
	 */
	if(fcp->flags & FCF_DMAING) {
		for(i=0; i<8; i++) {	/* fixme: how many flushes? */
			*fcp->sczctl |= SDC_FLUSH;
			delay(5);
			*fcp->sczctl &= ~SDC_FLUSH;
			delay(5);
		}
		*fcp->sczctl &= ~SDC_DMAMODE;
	}
		
	/*
	 * Wait a nominal length of time for RQM
	 */
	for(i=0; i<INTR_RQM_LOOPS; i++) {
		if(fcrp->msr & MSR_RQM) 
			break;
	}
	if(i == INTR_RQM_LOOPS) {
		goto badint;
	}
	if((fcrp->msr & MSR_DIO) == DIO_WRITE) {
		/*
		 * A Sense Interrupt Status command is expected.
		 */
		rtn = fc_send_byte(fcp, FCCMD_INTSTAT);
		if(rtn) {
			goto badint;
		}
	}
	else {
		/*
		 * First status byte is available.
		 */
		fcp->fdiop_i->stat_blk[0] = fcrp->fifo;
		fcp->fdiop_i->stat_bytes_xfr++;
	}
	return;
	
badint:
	fcp->flags |= FCF_NEEDSINIT;
	return;

	
} /* fc_intr() */

fd_return_t fc_82077_reset(fd_controller_t fcp, char *error_str)
{
	/*
	 * Initialize the 82077.
	 * We must own hardware at this time.
	 */
	fd_return_t rtn;
	fd_cntrl_regs_t fcrp = fcp->fcrp;
	
	if(error_str != NULL)
		printf("fc: Controller Reset: %s\n", error_str);
#ifdef	FD_DEBUG
	else
		printf("fc_82077_reset\n");
#endif	FD_DEBUG
			
	/*
	 * reset 82077. We must get from here to the configure command in
	 * 250 us to avoid the interrupt and 4 "sense interrupt status" 
	 * commmands resulting from polling emulation.
	 */
	fcrp->dor = 0;			/* reset true */
	delay(5 * FC_RESET_HOLD);
	fcrp->dor = DOR_RESET_NOT;	/* reset false */
	
	/* 
	 * Initialize 82077 registers.
	 */
	fcrp->dsr = 0;			/* default precomp; lowest data rate */
	fcrp->ccr = 0;			/* lowest data rate */
	fcrp->flpctl = fcp->flpctl_sh = FLC_82077_SEL;
	fcp->last_dens = FD_DENS_NONE; 	/* force specify command on next 
					 * I/O */
	fcp->dma_chan.dc_flags &= ~DMACHAN_ERROR;

	/*
	 * send a configure command, and a specify (w/default maximum data
	 * rate).
	 */
	rtn = fc_configure(fcp, FD_DENS_4);
	if(rtn == 0) {
		rtn = fc_specify(fcp, FD_DENS_4, &fd_drive_info[0]);
	}
	if(rtn == FDR_SUCCESS) {
		fcp->flags &= ~FCF_NEEDSINIT;
	}
	else {
		fcp->flags |= FCF_NEEDSINIT;
	}
	return(rtn);
} /* fc_82077_reset() */

fd_return_t fc_send_byte(fd_controller_t fcp, u_char byte) {
	/*
	 * send one command/parameter byte. Returns FDR_SUCCESS,
	 * FDR_TIMEOUT, or FDR_BADPHASE.
	 */
	fd_return_t rtn;
	
	if((rtn = fc_wait_pio(fcp, DIO_WRITE)) == FDR_SUCCESS)
		fcp->fcrp->fifo = byte;
	return(rtn);
}

fd_return_t fc_get_byte(fd_controller_t fcp, u_char *bp) {
	/*
	 * get one status byte. Returns FDR_SUCCESS, FDR_TIMEOUT, or
	 * FDR_BADPHASE.
	 */
	fd_return_t rtn;
	
	if((rtn = fc_wait_pio(fcp, DIO_READ)) == FDR_SUCCESS)
		*bp = fcp->fcrp->fifo;
	return(rtn);
}

static fd_return_t fc_wait_pio(fd_controller_t fcp, u_char poll_bits)
{
	/* 
	 * common routine for polling MSR_POLL_BITS in msr. Timer assumed 
	 * to be running.
	 */

	fd_return_t rtn = FDR_SUCCESS;
	register fd_cntrl_regs_t fcrp = fcp->fcrp;
	int loop;
	
	for(loop=0; loop<(5 * FIFO_RW_LOOP); loop++) {
		if(fcrp->msr & MSR_RQM) {
			if((fcrp->msr & MSR_POLL_BITS) == 
			    (poll_bits | MSR_RQM)) {
				break;
			}
			else {
				return(FDR_BADPHASE);
			}
		}
	}
	if(loop == FIFO_RW_LOOP) {
		rtn = FDR_TIMEOUT;
	}
	return(rtn);
} /* fc_wait_pio() */
		
void fc_flpctl_bset(fd_controller_t fcp, u_char bits)
{
	/*
	 * set bit(s) in flpctl and its shadow.
	 */
	fcp->flpctl_sh |= bits;
	fcp->fcrp->flpctl = fcp->flpctl_sh;
}

void fc_flpctl_bclr(fd_controller_t fcp, u_char bits)
{
	/*
	 * clear bit(s) in flpctl and its shadow.
	 */
	fcp->flpctl_sh &= ~bits;
	fcp->fcrp->flpctl = fcp->flpctl_sh;
}

/*
 * Update motor_on, media_id, and write protect.
 */
static void fc_update_stat(fd_controller_t fcp, fd_ioreq_t fdiop)
{
	u_char motor_bit;
	fd_cntrl_regs_t fcrp = fcp->fcrp;
	
	motor_bit = DOR_MOTEN0 << fcp->fdiop->unit;
	if(fcrp->dor & motor_bit)
		fdiop->drive_stat.motor_on = 1;
	else
		fdiop->drive_stat.motor_on = 0;
	fdiop->drive_stat.media_id = fcrp->flpctl & FLC_MID_MASK;
}

fd_return_t fc_configure(fd_controller_t fcp, u_char density)
{
	struct fd_ioreq ioreq;
	struct fd_configure_cmd *fccp;
	fd_return_t rtn;
	
	/*
	 * build a configure command 
	 */
	bzero(&ioreq, sizeof(struct fd_ioreq));
	fccp = (struct fd_configure_cmd *)ioreq.cmd_blk;
	fccp->opcode = FCCMD_CONFIGURE;
	fccp->rsvd1 = 0;
	/*
	 * Enable implied seek and fifo, disable drive polling
	 */
	fccp->conf_2 = CF2_EIS | CF2_DPOLL | CF2_FIFO_DEFAULT;
	fccp->pretrk = CF_PRETRACK;
	ioreq.timeout = TO_SIMPLE;
	ioreq.command = FDCMD_CMD_XFR;
	ioreq.num_cmd_bytes = sizeof(struct fd_configure_cmd);
	rtn = fc_send_cmd(fcp, &ioreq);
	return(rtn);
} /* fc_configure() */


#define HULT(time, max_time, mult) \
	(((time) >= (max_time)) ? 0 : ((time) / (mult)))

/*
 * Select specified density. This involves setting the data rate select bits
 * in dsr and ccr as well as sending a specify command.
 */
fd_return_t fc_specify (fd_controller_t fcp,
	int density,			/* FD_DENS_4, etc. */
	struct fd_drive_info *fdip)
{
	struct fd_ioreq ioreq;
	struct fd_specify_cmd *fcsp;
	fd_return_t rtn;
	register fd_cntrl_regs_t fcrp = fcp->fcrp;
	int data_rate;
		
	bzero(&ioreq, sizeof(struct fd_ioreq));
	fcsp = (struct fd_specify_cmd *)ioreq.cmd_blk;
	fcsp->opcode = FCCMD_SPECIFY;
	switch(density) {
	    case FD_DENS_1:
		data_rate = DRATE_MFM_250;
		/* specify cmd block...*/
	    	fcsp->srt = 16 - ((fdip->seek_rate + 1) / 2);	/* round up */	
		fcsp->hlt = HULT(fdip->head_settle_time, 512, 4);
		fcsp->hut = HULT(fdip->head_unload_time, 512, 32);
		break;
	    case FD_DENS_2:
		data_rate = DRATE_MFM_500;
	    	fcsp->srt = 16 - fdip->seek_rate;
		fcsp->hlt = HULT(fdip->head_settle_time, 256, 2);
		fcsp->hut = HULT(fdip->head_unload_time, 256, 16);
		break;
	    case FD_DENS_4:
		data_rate = DRATE_MFM_1000;
	    	fcsp->srt = 16 - 2 * fdip->seek_rate;
		fcsp->hlt = HULT(fdip->head_settle_time, 128, 1);
		fcsp->hut = HULT(fdip->head_unload_time, 128, 8);
		break;
	    default:
	    	printf("fd: Bogus density (%d) in fc_specify()\n", density);
		return(FDR_REJECT);
	}

	fcrp->dsr = PRECOMP_DEFAULT | data_rate;
	fcrp->ccr = data_rate;
	ioreq.timeout = TO_SIMPLE;
	ioreq.command = FDCMD_CMD_XFR;
	ioreq.num_cmd_bytes = SIZEOF_SPECIFY_CMD;
	rtn = fc_send_cmd(fcp, &ioreq);
	if(rtn == FDR_SUCCESS)
		fcp->last_dens = density;
	return(rtn);

} /* fc_specify() */

/* end of fd_io.c */







