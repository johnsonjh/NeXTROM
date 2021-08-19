/* POTCodes.h		4/1/88

	Result codes from the POT
*/

#ifndef	_POT_
#define	_POT_


#define	POT_PASSED			0
/* #define	PRAM_TEST_FAILED		1   not used */
/* #define	PROM_TEST_FAILED		2   not used */
/* #define	MAIN_RAM_TEST_FAILED		3   not used */
/* #define VIDEO_RAM_TEST_FAILED		    not assigned */
#define	FPU_TEST_FAILED			4
#define SCC_CONTROLLER_TEST_FAILED	5
#define	SCSI_CONTROLLER_TEST_FAILED	6
#define ETHERNET_CONTROLLER_TEST_FAILED	7
#define MO_CONTROLLER_TEST_FAILED	8
#define RTC_TEST_FAILED			9
#define	TBG_TEST_FAILED			10

/* #define DSP_TEST_FAILED			11   not used */

#define	SYSTEM_TIMER_TEST_FAILED	12
#define EVENT_COUNTER_TEST_FAILED	13
#define SOUND_OUT_TEST_FAILED		14

#define NO_SUCH_TEST			-1

/* EKG LED blink counts */
#define	BLINK_CRC1	1
#define	BLINK_CRC2	2
#define	BLINK_VRAM	3
#define	BLINK_NVRAM	4

#ifndef	ASSEMBLER

#include <sys/types.h>
#include <next/cpu.h>
#include <next/scb.h>
#include <next/scr.h>
#include <next/machparam.h>
#include <next/clock.h>
#include <nextdev/screg.h>
#include <nextdev/monreg.h>
#include <nextdev/video.h>
#include <mon/global.h>
#include <mon/nvram.h>
#include <mon/sio.h>
#include <mon/tftp.h>
#include <arpa/tftp.h>
#include <dev/enreg.h>
/*
	Constants used by the tests
*/

#define	SCC_TEST_CHAR	'@'
#define	SCC_PORT_A	(char *)(P_SCC+1)
#define	SCC_PORT_B	(char *)P_SCC

#define	SCSI_BASE	(char *)P_SCSI
#define SCSI_DMA_CTRL	0x20


/*
	Structures used by the tests
*/

#define	MAX_DMA_BUFFER_SIZE	16
#define	CHIP_SLOP		16
#define	NUMBER_OF_READ_BUFFERS	4


struct dma_buf {
	u_char	*bufptr;
	u_int	dmacsr;
	u_char	done, devcsr;
	u_char	data[MAX_DMA_BUFFER_SIZE+CHIP_SLOP];
};

struct	dma_channel {
	u_char	*next;
	u_char	*limit;
	u_char	*start;
	u_char	*stop;
};

struct	scc_dev {
	char	b_reg0;
	char	a_reg0;
	char	b_data;
	char	a_data;
};

struct	scc_tc {
	struct	scc_dev	*dev_regs;
	u_int	*dmacsr;
	struct	dma_channel *dma_regs;
	short	wb;
	short	rb;
	struct	dma_buf	rbuf[NUMBER_OF_READ_BUFFERS];
	u_char	*tbuf;
	u_char	tdata[MAX_DMA_BUFFER_SIZE+CHIP_SLOP];
	u_char	xdone;
};

#define	UNCODED_DISK_BLOCK_SIZE	1024
#define	CODED_DISK_BLOCK_SIZE	1296

struct	disk_dev {
	u_char	sector_id[3];
	u_char	sector_cnt;
	u_char	interrupt_status;
	u_char	interrupt_mask;
	u_char	control_2;
	u_char	control_1;
	u_short	csr;
	u_char	data_error_status;
	u_char	correction_count;
	u_char	initialization;
	u_char	format;
	u_char	data_mark;
	u_char	not_used;
	u_char	flag_strategy[7];
};

struct	disk_tc {
	struct	disk_dev *dev_regs;
	u_int	*dmacsr;
	struct	dma_channel *dma_regs;
	short	wb;
	short	rb;
	struct	dma_buf	rbuf[NUMBER_OF_READ_BUFFERS];
	u_char	*tbuf;
	u_char	tdata[MAX_DMA_BUFFER_SIZE+CHIP_SLOP];
	u_char	xdone;
};

#define	DISK_STATUS_CLEAR	0xfc
#define	DISK_ECC_SELECT		0x40
#define	DISK_ECC_DECODE		0x20
#define	DISK_ECC_ENCODE		0
#define	DISK_ECC_WRITE		0x40
#define	DISK_ECC_READ		0x80

#define	SGP_SPKREN		0x10
#define SGP_LOWPASS		0x08
#define SGP_VSCCK		0x04
#define SGP_VSCD		0x02
#define SGP_VSCS		0x01

#define VOLUME_RCHAN		0x80
#define VOLUME_LCHAN		0x40

#endif	ASSEMBLER
#endif	_POT_
