/*	@(#)odvar.h	1.0	08/12/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 * HISTORY
 * 12-Aug-87  John Seamons (jks) at NeXT
 *	Created.
 *
 **********************************************************************
 */

/* error control */
/* FIXME: decide on correct action/retry/rtz strategy for each error */
struct od_err {
	u_char	e_action : 3,		/* error action */
#define	EA_ADV		0		/* advisory only */
#define	EA_RETRY	1		/* just retry previous operation */
#define	EA_RTZ		2		/* must RTZ immediately */
#define	EA_EJECT	3		/* eject disk immediately */
#define	EA_RESPIN	4		/* spin down and up again */
		e_retry : 3,		/* number of retries to attempt */
		e_rtz : 2;		/* number of RTZs to attempt */
	char	*e_msg;
} od_err[] = {
#define	E_NO_ERROR	0
	EA_RETRY, 3, 1, 0,
#define	E_TIMING	1
	EA_RETRY, 3, 1, 0,
#define	E_CMP		2
	EA_RETRY, 3, 1, 0,
#define	E_ECC		3
	EA_RETRY, 2, 1, "uncorrectable ECC error",
#define	E_TIMEOUT	4
	EA_RETRY, 3, 1, "sector timeout",
#define	E_FAULT		5
	EA_RETRY, 3, 1, 0,
#define	E_D_PARITY	6
	EA_RETRY, 3, 1, 0,

#define	E_RESV_1	7
#define	E_STATUS_BASE	E_RESV_1
	EA_RETRY, 3, 1, 0,
#define	E_INSERT	8
	EA_RETRY, 3, 1, 0,
#define	E_RESET		9
	EA_ADV,	  3, 1, 0,
#define	E_SEEK		10
	EA_RTZ,   3, 0, 0,
#define	E_CMD		11
	EA_RESPIN,0, 0, 0,
#define	E_INTERFACE	12
	EA_RETRY, 3, 1, 0,
#define	E_I_PARITY	13
	EA_RETRY, 3, 1, 0,
#define	E_RESV_2	14
	EA_RETRY, 3, 1, 0,
#define	E_STOPPED	15
	EA_ADV,	  3, 1, 0,
#define	E_SIDE		16
	EA_EJECT, 0, 0, "media upside down",
#define	E_SERVO		17
	EA_ADV,	  3, 1, 0,
#define	E_POWER		18
	EA_RETRY, 3, 1, 0,
#define	E_WP		19
	EA_RETRY, 3, 1, 0,
#define	E_EMPTY		20
	EA_RETRY, 0, 0, "no disk inserted",
#define	E_BUSY		21
	EA_RETRY, 3, 1, 0,

#define	E_RF		22
#define	E_EXTENDED_BASE	E_RF
	EA_RETRY, 3, 1, 0,
#define	E_RESV_3	23
	EA_RETRY, 3, 1, 0,
#define	E_RESV_4	24
	EA_RETRY, 3, 1, 0,
#define	E_WRITE		25
	EA_RETRY, 3, 1, 0,
#define	E_COARSE	26
	EA_RETRY, 0, 1, 0,
#define	E_TEST		27
	EA_RETRY, 0, 0, 0,
#define	E_SLEEP		28
	EA_RETRY, 3, 1, 0,
#define	E_LENS		29
	EA_RTZ,   3, 0, 0,
#define	E_TRACKING	30
	EA_RTZ,   3, 0, 0,
#define	E_PLL		31
	EA_RETRY, 3, 1, "PLL failed",
#define	E_FOCUS		32
	EA_RTZ,   3, 0, 0,
#define	E_SPEED		33
	EA_RTZ,   3, 0, 0,
#define	E_STUCK		34
	EA_EJECT, 0, 0, 0,
#define	E_ENCODER	35
	EA_RTZ,   3, 0, 0,
#define	E_LOST		36
	EA_RTZ,   3, 0, 0,

#define	E_RESV_5	37
#define	E_HARDWARE_BASE	E_RESV_5
	EA_RETRY, 3, 1, 0,
#define	E_RESV_6	38
	EA_RETRY, 3, 1, 0,
#define	E_RESV_7	39
	EA_RETRY, 3, 1, 0,
#define	E_RESV_8	40
	EA_RETRY, 3, 1, 0,
#define	E_RESV_9	41
	EA_RETRY, 3, 1, 0,
#define	E_RESV_10	42
	EA_RETRY, 3, 1, 0,
#define	E_LASER		43
	EA_RESPIN,0, 0, 0,
#define	E_INIT		44
	EA_RETRY, 3, 1, 0,
#define	E_TEMP		45
	EA_EJECT, 0, 0, 0,
#define	E_CLAMP		46
	EA_EJECT, 0, 0, 0,
#define	E_STOP		47
	EA_EJECT, 0, 0, 0,
#define	E_TEMP_SENS	48
	EA_EJECT, 0, 0, 0,
#define	E_LENS_POS	49
	EA_RESPIN,0, 0, 0,
#define	E_SERVO_CMD	50
	EA_RESPIN,0, 0, 0,
#define	E_SERVO_TIMEOUT	51
	EA_RESPIN,0, 0, 0,
#define	E_HEAD		52
	EA_RESPIN,0, 0, 0,

#define	E_SOFT_TIMEOUT	53
	EA_RESPIN,0, 0, 0,
#define	E_BAD_BAD	54
	EA_RETRY, 0, 0, 0,
#define	E_BAD_REMAP	55
	EA_RETRY, 0, 0, 0,
#define	E_STARVE	56
	EA_RETRY, 3, 1, 0,
#define	E_DMA_ERROR	57
	EA_RETRY, 3, 1, 0,
#define	E_BUSY_TIMEOUT1	58
	EA_RETRY, 0, 0, 0,
#define	E_BUSY_TIMEOUT2	59
	EA_RETRY, 0, 0, 0,
};

/* drive commands */
#define	OMD1_SEK	0x0000
#define	OMD1_HOS	0xa000
#define	OMD1_REC	0x1000
#define	OMD1_RDS	0x2000
#define	OMD1_RCA	0x2200
#define	OMD1_RES	0x2800
#define	OMD1_RHS	0x2a00
#define	OMD1_RGC	0x3000
#define	OMD1_RVI	0x3f00
#define	OMD1_SRH	0x4100
#define	OMD1_SVH	0x4200
#define	OMD1_SWH	0x4300
#define	OMD1_SEH	0x4400
#define	OMD1_SFH	0x4500
#define	OMD1_RID	0x5000
#define	OMD1_SPM	0x5200
#define	OMD1_STM	0x5300
#define	OMD1_LC		0x5400
#define	OMD1_ULC	0x5500
#define	OMD1_EC		0x5600
#define	OMD1_SOO	0x5900
#define	OMD1_SOF	0x5a00
#define	OMD1_RSD	0x8000	
#define	OMD1_SD		0xb000
#define	OMD1_RJ		0x5100

/* formatter commands */
#define	OMD_ECC_READ	0x80
#define	OMD_ECC_WRITE	0x40
#define	OMD_RD_STAT	0x20
#define	OMD_ID_READ	0x10		/* must reset bit manually */
#define	OMD_VERIFY	0x08
#define	OMD_ERASE	0x04
#define	OMD_READ	0x02
#define	OMD_WRITE	0x01
#define	OMD_SPINUP	0xf0
#define	OMD_EJECT	0xf1
#define	OMD_SEEK	0xf2
#define	OMD_SPIRAL_OFF	0xf3
#define	OMD_RESPIN	0xf4

/*
 * DKIOCREQ mappings
 */
struct dr_cmdmap {
	char	dc_cmd;
	daddr_t	dc_blkno;
	u_short	dc_flags;
#define	DRF_SPEC_RETRY	0x0001		/* user specified retry/rtz values */
	char	dc_retry;		/* # of retries before aborting */
	char	dc_rtz;			/* # of restores before aborting */
	u_short	dc_wait;		/* wait time after cmd */
};

struct dr_errmap {
	char	de_err;
	u_int	de_ecc_count;		/* # of ECCs required */
};

struct od_stats {
	int	s_vfy_retry;
	int	s_vfy_failed;
};
