/*	@(#)nvram.h	1.0	10/06/86	(c) 1986 NeXT	*/

/*
 *  HISTORY
 *
 * 15-Mar-90  John Seamons (jks) at NeXT
 *	Replaced unused ni_timezone field with bits to indicate auto poweron
 *	mode in new clock chip.  Updated to include ni_reset field used by ROM.
 */
 
#ifndef _NVRAM_
#define _NVRAM_
#ifndef	ASSEMBLER
struct nvram_info {
#define	NI_RESET	9
	u_int	ni_reset : 4,
#define	SCC_ALT_CONS	0x08000000
		ni_alt_cons : 1,
#define	ALLOW_EJECT	0x04000000
		ni_allow_eject : 1,
		ni_vol_r : 6,
		ni_brightness : 6,
#define	HW_PWD	0x6
		ni_hw_pwd : 4,
		ni_vol_l : 6,
		ni_spkren : 1,
		ni_lowpass : 1,
#define	BOOT_ANY	0x00000002
		ni_boot_any : 1,
#define	ANY_CMD		0x00000001
		ni_any_cmd : 1;
#define	NVRAM_HW_PASSWD	6
	u_char ni_ep[NVRAM_HW_PASSWD];
#define	ni_enetaddr	ni_ep
#define	ni_hw_passwd	ni_ep
	u_short ni_simm;		/* 4 SIMMs, 4 bits per SIMM */
	char ni_adobe[2];
	u_char ni_pot[3];
	u_char	ni_new_clock_chip : 1,
		ni_auto_poweron : 1,
		ni_use_console_slot : 1,	/* Console slot was set by user. */
		ni_console_slot : 2,		/* Preferred console dev slot>>1 */
		: 3;
#define	NVRAM_BOOTCMD	12
	char ni_bootcmd[NVRAM_BOOTCMD];
	u_short ni_cksum;
};

#define	N_brightness	0
#define	N_volume_l	1
#define	N_volume_r	2
#endif	ASSEMBLER

/* nominal values during self test */
#define	BRIGHT_NOM	20
#define	VOL_NOM		0

/* bits in ni_pot[0] */
#define POT_ON			0x01
#define EXTENDED_POT		0x02
#define LOOP_POT		0x04
#define	VERBOSE_POT		0x08
#define	TEST_DRAM_POT		0x10
#define	BOOT_POT		0x20
#define	TEST_MONITOR_POT	0x40

/* ni_pot[1] oldest selftest error code */
/* ni_pot[2] most recent selftest error code */

#endif	_NVRAM_

