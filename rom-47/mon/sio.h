/*	@(#)sio.h	1.0	10/06/86	(c) 1986 NeXT	*/

#ifndef _SIO_
#define _SIO_

#import <nextdev/disk.h>

enum SIO_ARGS {
	SIO_AUTOBOOT,
	SIO_SPECIFIED,
	SIO_ANY,
};

struct sio {
	enum SIO_ARGS si_args;
	u_int si_ctrl, si_unit, si_part;
	struct device *si_dev;
	u_int si_blklen;	/* device "addressable block" length */
	u_int si_lastlba;	/* device "last logical blk addr" */
	caddr_t si_sadmem, si_protomem, si_devmem;
};

struct protocol {
	int (*p_open)();
	int (*p_close)();
	int (*p_boot)();
};

/* codes returned by p_open and p_boot */
#define	BE_NODEV	1	/* no device present */
#define	BE_FLIP		2	/* media upside down */
#define	BE_INSERT	3	/* no media present */
#define	BE_INIT		4	/* media not initialized */

struct device {
	int (*d_open)();
	int (*d_close)();
	int (*d_read)();
	int (*d_write)();
	int (*d_label_blkno)();
	int d_io_type;
#define	D_IOT_CHAR	0
#define	D_IOT_PACKET	1
};

#define	RAW_IO	0x80		/* flag to specify raw 8-bit device I/O */

#endif	_SIO_

