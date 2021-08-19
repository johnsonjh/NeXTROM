/*
 * scsireg.h -- generic SCSI definitions
 * PROM MONITOR VERSION
 *
 * HISTORY
 * 10-Sept-87  Mike DeMoney (mike) at NeXT
 *	Created.
 */

#ifndef _SCSIREG_
#define	_SCSIREG_

/*
 * status byte definitions
 */
#define	STAT_GOOD		0x00	/* cmd successfully completed */
#define	STAT_CHECK		0x02	/* abnormal condition occurred */
#define	STAT_CONDMET		0x04	/* condition met / good */
#define	STAT_BUSY		0x08	/* target busy */
#define	STAT_INTMGOOD		0x10	/* intermediate / good */
#define	STAT_INTMCONDMET	0x14	/* intermediate / cond met / good */
#define	STAT_RESERVED		0x18	/* reservation conflict */

#define	STAT_MASK		0x1e	/* clears vendor unique bits */

/*
 * message codes
 */
#define	MSG_CMDCMPLT		0x00	/* to host: command complete */
#define	MSG_SAVEPTRS		0x02	/* to host: save data pointers */
#define	MSG_RESTOREPTRS		0x03	/* to host: restore pointers */
#define	MSG_DISCONNECT		0x04	/* to host: disconnect */
#define	MSG_IDETERR		0x05	/* to disk: initiator detected error */
#define	MSG_ABORT		0x06	/* to disk: abort op, go to bus free */
#define	MSG_MSGREJECT		0x07	/* both ways: last msg unimplemented */
#define	MSG_NOP			0x08	/* to disk: no-op message */
#define	MSG_MSGPARERR		0x09	/* to disk: parity error last message */
#define	MSG_LNKCMDCMPLT		0x0a	/* to host: linked command complete */
#define	MSG_LNKCMDCMPLTFLAG	0x0b	/* to host: flagged linked cmd cmplt */
#define	MSG_DEVICERESET		0x0c	/* to disk: reset and go to bus free */

#define	MSG_IDENTIFYMASK	0x80	/* both ways: thread identification */
#define	MSG_ID_DISCONN		0x40	/*	can disconnect/reconnect */
#define	MSG_ID_LUNMASK		0x07	/*	target LUN */

/*
 * SCSI command descriptor blocks
 * (Any device level driver doing fancy things will probably define
 * these locally and cast a pointer on top of the sd_cdb.  We define
 * them here to reserve the appropriate space, driver level routines
 * can use them if they want.)
 *
 * 6 byte command descriptor block
 */
struct cdb_6 {
	u_int	c6_opcode:8,		/* device command */
		c6_lun:3,		/* logical unit */
		c6_lba:21;		/* logical block number */
	u_char	c6_len;			/* transfer length */
	u_char	c6_ctrl;		/* control byte */
};

/*
 * 10 byte command descriptor block
 * BEWARE: this definition is compiler sensitive due to int on
 * short boundry!
 */
struct cdb_10 {
	u_char	c10_opcode;		/* device command */
	u_char	c10_lun:3,		/* logical unit */
		c10_dp0:1,		/* disable page out (cache control) */
		c10_fua:1,		/* force unit access (cache control) */
		c10_mbz1:2,		/* reserved: must be zero */
		c10_reladr:1;		/* addr relative to prev linked cmd */
	u_int	c10_lba;		/* logical block number */
	u_int	c10_mbz2:8,		/* reserved: must be zero */
		c10_len:16,		/* transfer length */
		c10_ctrl:8;		/* control byte */
};

/*
 * 12 byte command descriptor block
 * BEWARE: this definition is compiler sensitive due to int on
 * short boundry!
 */
struct cdb_12 {
	u_char	c12_opcode;		/* device command */
	u_char	c12_lun:3,		/* logical unit */
		c12_dp0:1,		/* disable page out (cache control) */
		c12_fua:1,		/* force unit access (cache control) */
		c12_mbz1:2,		/* reserved: must be zero */
		c12_reladr:1;		/* addr relative to prev linked cmd */
	u_int	c12_lba;		/* logical block number */
	u_char	c12_mbz2;		/* reserved: must be zero */
	u_char	c12_mbz3;		/* reserved: must be zero */
	u_int	c12_mbz4:8,		/* reserved: must be zero */
		c12_len:16,		/* transfer length */
		c12_ctrl:8;		/* control byte */
};
	
union cdb {
	struct	cdb_6	cdb_c6;
	struct	cdb_10	cdb_c10;
	struct	cdb_12	cdb_c12;
};

#define	cdb_opcode	cdb_c6.c6_opcode	/* all opcodes in same place */

/*
 * control byte values
 */
#define	CTRL_LINKFLAG		0x03	/* link and flag bits */
#define	CTRL_LINK		0x01	/* link only */
#define	CTRL_NOLINK		0x00	/* no command linking */

/*
 * six byte cdb opcodes
 * (Optional commands should only be used by formatters)
 */
#define	C6OP_TESTRDY		0x00	/* test unit ready */
#define	C6OP_REQSENSE		0x03	/* request sense */
#define	C6OP_FORMAT		0x04	/* format unit */
#define	C6OP_REASSIGNBLK	0x07	/* OPT: reassign block */
#define	C6OP_READ		0x08	/* read data */
#define	C6OP_WRITE		0x0a	/* write data */
#define	C6OP_INQUIRY		0x12	/* get device specific info */
#define	C6OP_MODESELECT		0x15	/* OPT: set device parameters */
#define	C6OP_MODESENSE		0x1a	/* OPT: get device parameters */
#define	C6OP_STARTSTOP		0x1b	/* OPT: start or stop device */
#define	C6OP_SENDDIAG		0x1d	/* send diagnostic */

/*
 * ten byte cdb opcodes
 */
#define	C10OP_READCAPACITY	0x25	/* read capacity */
#define	C10OP_READEXTENDED	0x28	/* read extended */
#define	C10OP_WRITEEXTENDED	0x2a	/* write extended */
#define	C10OP_READDEFECTDATA	0x37	/* OPT: read media defect info */

/*
 * opcode groups
 */
#define	SCSI_OPGROUP(opcode)	((opcode) & 0xe0)

#define	OPGROUP_0		0x00	/* six byte commands */
#define	OPGROUP_1		0x20	/* ten byte commands */
#define	OPGROUP_2		0x40	/* ten byte commands */
#define	OPGROUP_5		0xa0	/* twelve byte commands */
#define	OPGROUP_6		0xc0	/* six byte, vendor unique commands */
#define	OPGROUP_7		0xe0	/* ten byte, vendor unique commands */

/*
 * extended sense data
 * returned by C6OP_REQSENSE
 */
struct esense_reply {
	u_char	er_ibvalid:1,		/* information bytes valid */
		er_class:3,		/* error class */
		er_code:4;		/* error code */
	u_char	er_segment;		/* segment number for copy cmd */
	u_char	er_filemark:1,		/* file mark */
		er_endofmedium:1,	/* end-of-medium */
		er_badlen:1,		/* incorrect length */
		er_zero1:1,		/* reserved */
		er_sensekey:4;		/* sense key */
	u_char	er_infomsb;		/* MSB of information byte */
	u_int	er_info:24,		/* bits 23 - 0 of info "byte" */
		er_addsenselen:8;	/* additional sense length */
	u_int	er_zero2;		/* copy status (unused) */
	u_char	er_addsensecode;	/* additional sense code */
	/*
	 * technically, there can be additional bytes of sense info
	 * here, but we don't check them, so we don't define them
	 */
};

/*
 * sense keys
 */
#define	SENSE_NOSENSE		0x0	/* no error to report */
#define	SENSE_RECOVERED		0x1	/* recovered error */
#define	SENSE_NOTREADY		0x2	/* target not ready */
#define	SENSE_MEDIA		0x3	/* media flaw */
#define	SENSE_HARDWARE		0x4	/* hardware failure */
#define	SENSE_ILLEGALREQUEST	0x5	/* illegal request */
#define	SENSE_UNITATTENTION	0x6	/* drive attention */
#define	SENSE_DATAPROTECT	0x7	/* drive access protected */
#define	SENSE_ABORTEDCOMMAND	0xb	/* target aborted command */
#define	SENSE_VOLUMEOVERFLOW	0xd	/* eom, some data not transfered */
#define	SENSE_MISCOMPARE	0xe	/* source/media data mismatch */

/*
 * inquiry data
 */
struct inquiry_reply {
	u_char	ir_devicetype;		/* device type, see below */
	u_char	ir_removable:1,		/* removable media */
		ir_typequalifier:7;	/* device type qualifier */
	u_char	ir_zero1:2,		/* reserved */
		ir_ecmaversion:3,	/* ECMA version number */
		ir_ansiversion:3;	/* ANSI version number */
	u_char	ir_zero2:4,		/* reserved */
		ir_rspdatafmt:4;	/* response data format */
	u_char	ir_addlistlen;		/* additional list length */
	u_char	ir_zero3[3];		/* vendor unique field */
	char	ir_vendorid[8];		/* vendor name in ascii */
	char	ir_productid[16];	/* product name in ascii */
	char	ir_revision[32];	/* revision level info in ascii */
	char	ir_endofid[1];		/* just a handle for end of id info */
};

#define	DEVTYPE_DISK		0x00	/* read/write disks */
#define	DEVTYPE_TAPE		0x01	/* tapes and other sequential devices */
#define	DEVTYPE_PRINTER		0x02	/* printers */
#define	DEVTYPE_PROCESSOR	0x03	/* cpu's */
#define	DEVTYPE_WORM		0x04	/* write-once optical disks */
#define	DEVTYPE_READONLY	0x05	/* cd rom's, etc */
#define	DEVTYPE_NOTPRESENT	0x7f	/* logical unit not present */

/*
 * read capacity reply
 */
struct capacity_reply {
	u_int	cr_lastlba;		/* last logical block address */
	u_int	cr_blklen;		/* block length */
};

/*
 * Day-to-day constants in the SCSI world
 */
#define	SCSI_NTARGETS	8		/* 0 - 7 for target numbers */
#define	SCSI_NLUNS	8		/* 0 - 7 luns for each target */

/*
 * scsi bus phases
 */
#define	PHASE_DATAOUT		0x0
#define	PHASE_DATAIN		0x1
#define	PHASE_COMMAND		0x2
#define	PHASE_STATUS		0x3
#define	PHASE_MSGOUT		0x6
#define	PHASE_MSGIN		0x7

/*
 * Defect list header
 * Used by FORMAT and REASSIGN BLOCK commands
 */
struct defect_header {
	u_char	dh_mbz1;
	u_char	dh_fov:1,		/* format options valid */
		dh_dpry:1,		/* disable primary */
		dh_dcrt:1,		/* disable certification */
		dh_stpf:1,		/* stop format */
		dh_mbz2:4;
	u_short	dh_len;			/* items in defect list */
};

#endif _SCSIREG_
