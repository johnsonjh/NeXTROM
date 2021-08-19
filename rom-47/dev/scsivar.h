/*
 * scsivar.h -- definitions of generic scsi driver data structures
 * PROM MONITOR VERSION
 *
 * HISTORY
 * 10-Sept-87  Mike DeMoney (mike) at NeXT
 *	Created.
 */

/*
 * scsi_device -- one per scsi device
 */
struct scsi_device {
	u_char	sd_target;		/* scsi target number */
	u_char	sd_lun;			/* lun on target */
	/*
	 * Info filled in by device level and passed to controller level
	 * to start cmd
	 *
	 * NOTE: We don't guarantee that these values will be unchanged
	 * after the command is executed.
	 */
	char	sd_read;		/* i/o from target to initiator */
	union	cdb sd_cdb;		/* current cdb */
	caddr_t	sd_addr;		/* buffer address */
	long	sd_bcount;		/* dma byte count */
	/*
	 * this info is filled in by controller for return to device
	 * level interrupt routine
	 */
	long	sd_resid;		/* # of requested bytes not xfered */
	u_char	sd_status;		/* SCSI status if SDSTATE_COMPLETED */
	u_char	sd_state;		/* device level fsm state */
};

#define	SDSTATE_START		0	/* device yet to be selected */
#define	SDSTATE_SELTIMEOUT	1	/* timeout on target selection */
#define	SDSTATE_COMPLETED	2	/* target sent COMMAND COMPLETE msg */
#define	SDSTATE_ABORTED		3	/* target disconnected unexpectedly */
#define	SDSTATE_UNIMPLEMENTED	4	/* ctrlr can't do what device asked */
#define SDSTATE_RESET		5	/* bus is in reset state - abort */

