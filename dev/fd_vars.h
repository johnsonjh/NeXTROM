/*	@(#)fd_vars.h	2.0	01/24/90	(c) 1990 NeXT	
 *
 * fd_vars.h -- Internally used data structures and constants for Floppy 
 *		Disk driver
 *
 * PROM MONITOR VERSION
 *
 * HISTORY
 * 03-Apr-90	Doug Mitchell at NeXT
 *	Created.
 *
 */

#ifndef	_FDVARS_
#define _FDVARS_

#import <sys/types.h>
#import <next/printf.h>
#import <nextdev/dma.h>
#import <nextdev/disk.h>
#import <nextdev/busvar.h>
#import	<nextdev/fd_reg.h>
#import <nextdev/fd_extern.h>
#import <nextdev/sf_access.h>

/* #define 	FD_DEBUG 	1	/* */
#define 	INCLUDE_EJECT	1	/* implement eject command */

#define FD_MAX_DMA		0x10000
#define NFC			1		/* # controllers */
#define NFD			2		/* # drives */
#define NFV			NFD		/* # volumes */
#define NFP			1		/* # partitions */

#ifndef	TRUE
#define TRUE	1
#endif	TRUE
#ifndef	FALSE
#define FALSE	0
#endif	FALSE

/*
 * one per controller. Statically allocated. 
 */
struct fd_controller {
	volatile struct fd_cntrl_regs *fcrp;	/* hardware registers */
	volatile int		flags;		/* see FCF_xxx, below. */
	fd_ioreq_t		fdiop;		/* ptr to current I/O 
						 * request (from driver
						 * level) */
	fd_ioreq_t		fdiop_i;	/* ptr to current I/O 
						 * request (interrupt level) */
	u_char			flpctl_sh;	/* shadow of FLPCTL register */
	struct	dma_chan 	dma_chan;	/* DMA info */
	dma_list_t		dma_list;	/* DMA segment info */
	u_char			*sczfst;	/* SCSI FIFO status */
	u_char			*sczctl;	/* SCSI dma control */
	int 			last_dens;	/* last value of density 
						 * used */

};

typedef struct fd_controller *fd_controller_t;

/*
 * fd_controller.flags
 */
#define FCF_COMMAND	0x00000001		/* new work to do in fc_req_q
						 */		
#define FCF_TIMEOUT	0x00000008		/* timer expired */
#define FCF_NEEDSINIT	0x00000400		/* 82077 needs reset/init */
#define FCF_DMAING	0x00004000		/* DMA in progress */
#define FCF_DMAREAD	0x00008000		/* DMA read (device to mem) */
#define FCF_DMAERROR	0x00010000		/* DMA error detected */
#define FCF_INITD	0x00020000		/* DMA channel initialized */

/*
 * fd_volume.sect_size values
 */
#define FD_SECTSIZE_MIN		512
#define FD_SECTSIZE_MAX		1024

/*
 * Round a byte count up to the next sector-aligned boundary.
 */
#define	FD_SECTALIGN(count, sect_size)	\
	((((unsigned)(count) + sect_size - 1) &~ (sect_size - 1)))

/*
 * one per volume/drive.
 */
struct fd_volume {
	u_int			drive_num;	/* logical drive # this volume
						 * is in. DRIVE_UNMOUNTED means
						 * not inserted. */
	int 			volume_num;	/* from minor number */
	struct fd_ioreq		dev_ioreq;	/* every I/O request is
						 * placed here when command is 
						 * passed to controller
						 * thread */
	u_int 			flags;		/* see FDF_xxx, below */
	int 			state;		/* see FVSTATE_xxx, below */

	/*
	 * drive info 
	 */
	int 			unit;		/* controller-relative drive
						 * number */
	fd_controller_t		fcp;		/* controller to which this 
						 * drive is attached */
	int			drive_flags;	/* see FDF_xxx, below */
	int 			drive_type;	/* DRIVE_TYPE_FD288, etc. 
						 * index into fd_drive_info[])
						 */
	
	/*
	 * the following are used for reads and writes only. Start_sect
	 * is the current logical sector of the transfer. Bytes_to_go is the
	 * total byte count yet to be transferred. The current request is
	 * physically at current_sect for current_byte_count. (This may be
	 * in the spare area.)
	 */
	u_int 			start_sect;
	u_int			bytes_to_go;
	caddr_t			start_addrs;
	u_int			current_sect;
	u_int			current_byte_cnt;
	caddr_t			current_addrs;
	int			inner_retry_cnt;
	int			outer_retry_cnt;
	int			io_flags;	/* see FVIOF_xxx, below */
	
	/*
	 * physical device info.
	 */
	struct fd_format_info	format_info;	/* density, #sects, etc. */
};

typedef	struct fd_volume *fd_volume_t;

/*
 * fd_volume.flags
 */
#define FVF_VALID	0x00000001

/*
 * fd_volume.io_flags
 */
#define FVIOF_READ	0x00000001		/* 1 = read  0 = write */
#define FVIOF_SPEC	0x00000002		/* 1 = command in 
						 * fvp->dev_ioreq; else r/w
						 * using fvp->{start_sect, 
						 * bytes_to_go, start_addrs}.
						 */
						 
#define DRIVE_UNMOUNTED NFD			/* this volume currently is not
						 * present in a drive. */
#define VALID_DRIVE(drive_num) 	(drive_num < NFD) 					 
	
/*
 * fd_volume.state values 
 */
#define FVSTATE_IDLE		0
#define FVSTATE_EXECUTING	1
#define FVSTATE_RETRYING	2
#define FVSTATE_RECALIBRATING	3

/*
 * retry counts
 */
#define FV_INNER_RETRY		2  		/* retries between recals */
#define FV_OUTER_RETRY		1		/* # of recalibrates */
#define FV_ATTACH_TRIES		2		/* # of attempts during 
						 * fd_attach_com I/O */

/*
 * fd_volume.drive_flags values
 */
#define FDF_PRESENT		0x00000001	/* drive is physically 
						 * present */
#define FDF_MOTOR_ON		0x00000002	/* motor is currently on */

/*
 * fd_volume.drive_type values
 */
#define	DRIVE_TYPE_FD288	0

struct fd_global {
	struct fd_controller	fd_controller;
	struct bus_ctrl 	*fc_bcp;
	struct fd_volume	fd_volume;
	u_char 			sector_buf[FD_SECTSIZE_MAX + DMA_ENDALIGNMENT];
	u_char 			*sbp;
	int			silent;		/* inhibit error reporting */
};

/*
 * One per drive_type. Constants, in fd_driver.c.
 */
struct fd_drive_info {
	struct drive_info	gen_drive_info;		/* name, block length,
							 * etc. */
	int			seek_rate;		/* in ms */
	int 			head_settle_time;	/* ms */
	int 			head_unload_time;	/* ms */
};

/*
 * standard timeouts, in milliseconds
 */
#define TO_EJECT	2000
#define TO_MOTORON	500			/* just a delay */
#define TO_MOTOROFF	10			/* just a delay */
#define TO_SIMPLE	10			/* get status, etc. */
#define TO_IDLE		2000			/* max motor on idle time */
#define TO_RW		1000			/* r/w timeout */
#define TO_RECAL	1000			/* recal */
#define TO_SEEK		400			/* seek */
#define TO_SFA_ARB	10000			/* wait 10 seconds (??) for 
						 * sfa_arbitrate() to succeed 
						 */

/*
 * low-level timeouts in microseconds
 */
#define TO_EJECT_PULSE	3			/* width of eject pulse */
#define TO_FIFO_RW	10000			/* max time to wait to r/w 
						 * cmd/status bytes from/to
						 * FIFO. This timeout covers
						 * all 'n' command bytes or
						 * all 'n' status bytes. A 
						 * value of 2000 sometimes 
						 * times out in 500 us! */

/*
 * timeouts in loops for polling mode
 */
#define LOOPS_PER_MS 	80			/* for fc_wait_intr */
#define FIFO_RW_LOOP	(200 * LOOPS_PER_MS)
#define EJECT_LOOP	(2000 * LOOPS_PER_MS)
#define MOTOR_ON_LOOP	(500 * LOOPS_PER_MS)
#define MOTOR_OFF_LOOP	(50 * LOOPS_PER_MS)
#define INTR_RQM_LOOPS	100			/* loops to wait in fc_intr
					 	 * for RQM */

/*
 * misc. constants
 */
#ifndef	NULL
#define NULL 0
#endif	NULL

/*
 * 	prototypes for functions globally visible
 *
 * in fd_driver.c:
 */
int fd_command(fd_volume_t fvp,
	fd_ioreq_t fdiop);
/*
 * in fd_subr.c:
 */
fd_return_t fd_start(fd_volume_t fvp);
void fd_intr(fd_volume_t fvp);
void fd_done(fd_volume_t fvp);
int fd_get_label(fd_volume_t fvp);
int fd_get_status(fd_volume_t fvp, struct fd_drive_stat *dstatp);
int fd_recal(fd_volume_t fvp);
void fd_gen_write_cmd(fd_volume_t fvp,
	struct fd_ioreq *fdiop,
	u_int block,
	u_int byte_count,
	caddr_t dma_addrs,
	int timeout);
void fd_new_fv(fd_volume_t fvp, int vol_num);
void fd_free_fv(fd_volume_t fvp);
int fd_basic_cmd(fd_volume_t fvp, int command);
int fd_live_rw(fd_volume_t fvp,
	int fs_sector,	
	int byte_count,
	caddr_t addrs,	
	boolean_t read);
void fd_set_density_info(fd_volume_t fvp, int density);
int fd_set_sector_size(fd_volume_t fvp, int sector_size);
int fd_raw_rw(fd_volume_t fvp,
	int sector,	
	int sect_count,
	caddr_t addrs,	
	boolean_t read);

/*
 * in fd_io.c:
 */
int fc_init(struct fd_global *fdgp, caddr_t hw_reg_ptr);
void fc_docmd(fd_volume_t fvp);
fd_return_t fc_send_byte(fd_controller_t fcp, u_char byte);
fd_return_t fc_get_byte(fd_controller_t fcp, u_char *bp);
void fc_flpctl_bset(fd_controller_t fcp, u_char bits);
void fc_flpctl_bclr(fd_controller_t fcp, u_char bits);
fd_return_t fc_specify (fd_controller_t fcp,
	int density,
	struct fd_drive_info *fdip);
fd_return_t fc_82077_reset(fd_controller_t fcp, char *error_str);
fd_return_t fc_configure(fd_controller_t fcp, u_char density);
void fc_intr(fd_controller_t fcp);

/*
 * in fd_cmds.c:
 */
void fc_cmd_xfr(fd_controller_t fcp, fd_volume_t fvp);
#ifdef	INCLUDE_EJECT
void fc_eject(fd_controller_t fcp);
void fd_gen_seek(int density, fd_ioreq_t fdiop, int cyl, int head);
#endif	INCLUDE_EJECT
void fc_motor_on(fd_controller_t fcp);
void fc_motor_off(fd_controller_t fcp);
fd_return_t fc_send_cmd(fd_controller_t fcp, fd_ioreq_t fdiop);

extern struct fd_density_info fd_density_info[];
extern struct fd_disk_info fd_disk_info[];
extern struct fd_drive_info fd_drive_info[];
#endif	_FDVARS_ 







