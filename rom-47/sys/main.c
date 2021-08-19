/*	@(#)main.c	1.0	10/10/86	(c) 1986 NeXT	*/

/*
 *  HISTORY
 *  30-Apr-90  Bassanio Law (blaw) at NeXT
 *	Does not enable monitor sound out by default anymore in mon_setup_comm.
 *	This is in anticipate of the new automated test process in the factory
 *	which freshly assembled board will not be hooked up to a monitor.
 */
 
#include <sys/types.h>
#include <mon/monparam.h>
#include <mon/global.h>
#include <mon/sio.h>
#include <mon/nvram.h>
#include <mon/reglist.h>
#include <dev/enreg.h>
#include <assym.h>
#include <next/mmu.h>
#include <next/mem.h>
#include <next/psl.h>
#include <next/reg.h>
#include <next/cpu.h>
#include <next/trap.h>
#include <next/scr.h>
#include <next/bmap.h>
#include <nextdev/video.h>
#include <nextdev/snd_dspreg.h>
#include <nextdev/odreg.h>
#include <nextdev/screg.h>

#if SIZEOFNVRAM != 32
sizeof struct nvram != 32!
#endif

#define	CB		check_bits

struct reglist dataregs[] = {
	{"0",	R_d0},
	{"1",	R_d1},
	{"2",	R_d2},
	{"3",	R_d3},
	{"4",	R_d4},
	{"5",	R_d5},
	{"6",	R_d6},
	{"7",	R_d7},
	0,
};

struct reglist addrregs[] = {
	{"0",	R_a0},
	{"1",	R_a1},
	{"2",	R_a2},
	{"3",	R_a3},
	{"4",	R_a4},
	{"5",	R_a5},
	{"6",	R_a6},
	{"7",	R_a7},
	0,
};

struct reglist cpuregs[] = {
	{"pc",	R_pc},
	{"sr", 	R_sr - 2, CB,
"\20\317\2trace\16s\216u\15m\215i\311\3ipl\5x\4n\3z\2v\1c"},
	{"usp",	R_usp},
	{"isp",	R_isp},
	{"msp",	R_msp},
	{"vbr",	R_vbr},
	{"sfc",	R_sfc},
	{"dfc",	R_dfc},
#ifdef	MC68030
	{"cacr",R_cacr,	CB, "\20\14cd\13ced\12fd\11ed\4ci\3cei\2fi\1ei"},
	{"caar",R_caar,	CB, "\20\311\30cfa\303\6index"},
#endif	MC68030
#ifdef	MC68040
	{"cacr",R_cacr,	CB, "\20\40de\20ie"},
#endif	MC68040
	0,
};

struct reglist systemregs[] = {
	{"intrstat", P_INTRSTAT_CON, CB,
"\20\40NMI\37pwrfail\36systimer\35enetTXDMA\34enetRXDMA\33scsiDMA\32opticalDMA\31printerDMA\30soundoutDMA\27soundinDMA\26sccDMA\25dspDMA\24m2rDMA\23r2mDMA\22scc\21rpi\20bus\17rtc\16optical\15scsi\14printer\13enetTX\12enetRX\11soundrun\10phone\7dsp\6video\5monitor\4kybd/mouse\3power\2softint2\1softint1"},
	{"intrmask", P_INTRMASK_CON, CB,
"\20\40NMI\37pwrfail\36systimer\35enetTXDMA\34enetRXDMA\33scsiDMA\32opticalDMA\31printerDMA\30soundoutDMA\27soundinDMA\26sccDMA\25dspDMA\24m2rDMA\23r2mDMA\22scc\21rpi\20bus\17rtc\16optical\15scsi\14printer\13enetTX\12enetRX\11soundrun\10phone\7dsp\6video\5monitor\4kybd/mouse\3power\2softint2\1softint1"},
	{"scr1", P_SCR1_CON, CB,
"\20\335\4sid\321\10DMArev\311\10CPUrev\307\2vmss\305\2mmss\301\2ccms"},
	{"scr2", P_SCR2_CON, CB,
"\20\240DSPreset\37DSPblock\36DSPunpk\35DSPb\34DSPa\33rpi\32softint2\31softint1\325\4mem256K/4M\321\4mem1M/4M\20timeripl7\315\3ROMwait\13rtdata\12rtclk\11rtce\210ROMovly\1ekgLED"},
	0,
};
#undef CB

struct reglist nvram[] = {
	{"boot command", N_bootcmd, check_string, (char*) NVRAM_BOOTCMD},
#if	DEBUG
	{"Ethernet address", 0, check_enetaddr},
#endif	DEBUG
	{"DRAM tests", TEST_DRAM_POT, check_pot},
	{"perform power-on system test", POT_ON, check_pot},
	{"	sound out tests", TEST_MONITOR_POT, check_pot},
	{"	SCSI tests", EXTENDED_POT, check_pot},
	{"	loop until keypress", LOOP_POT, check_pot},
	{"	verbose test mode", VERBOSE_POT, check_pot},
	{"boot extended diagnostics", BOOT_POT, check_pot},
	{"serial port A is alternate console", SCC_ALT_CONS, check_opt},
	{"allow any ROM command even if password protected",
		ANY_CMD, check_opt},
	{"allow boot from any device even if password protected",
		BOOT_ANY, check_opt},
	{"allow optical drive #0 eject even if password protected",
		ALLOW_EJECT, check_opt},
	0,
};

char *space[] = {
	"0", "UD", "UP", "3", "4", "SD",
	"SP", "cpu"
};

char *simm_str[] = {
	"no", "16MB nibble mode", "4MB nibble mode", "1MB nibble mode",
	"illegal", "16MB page mode", "4MB page mode", "1MB page mode (illegal)",
	"illegal", "16MB parity nibble mode", "4MB parity nibble mode",
	"1MB parity nibble mode", "illegal", "16MB parity page mode",
	"4MB parity page mode", "1MB parity page mode (illegal)",
};

#define	MB	(1024*1024)

int simm_size[] = {
	0, 16*MB, 4*MB, 1*MB, 0, 16*MB, 4*MB, 1*MB,
	0, 16*MB, 4*MB, 1*MB, 0, 16*MB, 4*MB, 1*MB,
};

u_char speed_cpu[] = {
	16, 20, 25, 33,
};

u_char speed_mem[] = {
	100, 100, 80, 60,
};

/*
 *  Standard memory timing configurations (per Digital guys):
 *
 *  Note that as of 6/89, all CPU's are coded for 120ns but
 *  actually requires 100ns SIMM's.
 *
 *  25MHz no parity 
 *	5-2-2-2  CPU Main Burst
 *	6-4-4-4  CPU Video Burst
 *	6 CPU Main&Video NonBurst
 *	4-1-1-1  DMA Main Busrt
 *	4-2-2-2  DMA Video Burst
 *	4 DMA Main&Video NonBurst
 *
 *  25MHz parity
 *	6-3-3-3  CPU Main Burst
 *	7-4-4-4  CPU Video Burst
 *	7 CPU Main&Video NonBurst
 *	5-2-2-2  DMA Main Busrt
 *	5-2-2-2  DMA Video Burst
 *	5 DMA Main&Video NonBurst
 *
 *  20MHz no parity 
 *	5-2-2-2  CPU Main Burst
 *	5-4-4-4  CPU Video Burst
 *	5 CPU Main&Video NonBurst
 *	4-1-1-1  DMA Main Busrt
 *	4-2-2-2  DMA Video Burst
 *	4 DMA Main&Video NonBurst
 *
 *  20MHz parity
 *	6-3-3-3  CPU Main Burst
 *	6-4-4-4  CPU Video Burst
 *	6 CPU Main&Video NonBurst
 *	5-2-2-2  DMA Main Busrt
 *	5-2-2-2  DMA Video Burst
 *	5 DMA Main&Video NonBurst
 */
char mem_timing[][32][NMTREG] = {
{	/* NeXT_CUBE */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 16 MHz cpu, 120 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 16 MHz cpu, 100 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 16 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 16 MHz cpu, 60 ns RAM */
	{0x19, 0x28, 0x08, 0x10, 0x00},	/* 20 MHz cpu, 120 RAS, 30 CAS */
	{0x31, 0x38, 0x68, 0x0d, 0x00},	/* 20 MHz cpu, 100 RAS, 25 CAS */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 20 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 20 MHz cpu, 60 ns RAM */
	{0x1a, 0x28, 0x00, 0x10, 0x00},	/* 25 MHz cpu, 120 RAS, 30 CAS */
	{0x3e, 0x78, 0x00, 0x0c, 0x00},	/* 25 MHz cpu, 100 RAS, 25 CAS */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 25 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 25 MHz cpu, 60 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 33 MHz cpu, 120 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 33 MHz cpu, 100 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 33 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 33 MHz cpu, 60 ns RAM */
	
	/* same as above -- no parity on NeXT_CUBE */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 16 MHz cpu, 120 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 16 MHz cpu, 100 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 16 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 16 MHz cpu, 60 ns RAM */
	{0x19, 0x28, 0x08, 0x10, 0x00},	/* 20 MHz cpu, 120 RAS, 30 CAS */
	{0x31, 0x38, 0x68, 0x0d, 0x00},	/* 20 MHz cpu, 100 RAS, 25 CAS */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 20 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 20 MHz cpu, 60 ns RAM */
	{0x1a, 0x28, 0x00, 0x10, 0x00},	/* 25 MHz cpu, 120 RAS, 30 CAS */
	{0x3e, 0x78, 0x00, 0x0c, 0x00},	/* 25 MHz cpu, 100 RAS, 25 CAS */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 25 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 25 MHz cpu, 60 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 33 MHz cpu, 120 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 33 MHz cpu, 100 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 33 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 33 MHz cpu, 60 ns RAM */
},
{	/* NeXT_WARP9, no parity */
	{0x02, 0x28, 0x08, 0x10, 0x00},	/* 16 MHz cpu, 120 RAS, 30 CAS */
	{0x02, 0x28, 0x08, 0x10, 0x00},	/* 16 MHz cpu, 100 RAS, 25 CAS */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 16 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 16 MHz cpu, 60 ns RAM */
	{0x19, 0x38, 0x68, 0x10, 0x00},	/* 20 MHz cpu, 120 RAS, 30 CAS */
	{0x19, 0x38, 0x68, 0x10, 0x00},	/* 20 MHz cpu, 100 RAS, 25 CAS */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 20 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 20 MHz cpu, 60 ns RAM */
	{0x1a, 0x78, 0x60, 0x10, 0x00},	/* 25 MHz cpu, 120 RAS, 30 CAS */
	{0x1a, 0x78, 0x60, 0x10, 0x00},	/* 25 MHz cpu, 100 RAS, 25 CAS */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 25 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 25 MHz cpu, 60 ns RAM */
	{0x02, 0x28, 0x08, 0x10, 0x00},	/* 33 MHz cpu, 120 RAS, 30 CAS */
	{0x02, 0x28, 0x08, 0x10, 0x00},	/* 33 MHz cpu, 100 RAS, 25 CAS */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 33 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 33 MHz cpu, 60 ns RAM */
	
	/* NeXT_WARP9, parity */
	{0x02, 0x28, 0x08, 0x10, 0x00},	/* 16 MHz cpu, 120 RAS, 30 CAS */
	{0x02, 0x28, 0x08, 0x10, 0x00},	/* 16 MHz cpu, 100 RAS, 25 CAS */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 16 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 16 MHz cpu, 60 ns RAM */
	{0x06, 0x24, 0x68, 0x10, 0x00},	/* 20 MHz cpu, 120 RAS, 30 CAS */
	{0x06, 0x24, 0x68, 0x10, 0x00},	/* 20 MHz cpu, 100 RAS, 25 CAS */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 20 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 20 MHz cpu, 60 ns RAM */
	{0x07, 0x64, 0x60, 0x10, 0x00},	/* 25 MHz cpu, 120 RAS, 30 CAS */
	{0x07, 0x64, 0x60, 0x10, 0x00},	/* 25 MHz cpu, 100 RAS, 25 CAS */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 25 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 25 MHz cpu, 60 ns RAM */
	{0x02, 0x28, 0x08, 0x10, 0x00},	/* 33 MHz cpu, 120 RAS, 30 CAS */
	{0x02, 0x28, 0x08, 0x10, 0x00},	/* 33 MHz cpu, 100 RAS, 25 CAS */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 33 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 33 MHz cpu, 60 ns RAM */
},
{	/* NeXT_X15, no parity */
	{0x02, 0x28, 0x08, 0x10, 0x00},	/* 16 MHz cpu, 120 RAS, 30 CAS */
	{0x02, 0x28, 0x08, 0x10, 0x00},	/* 16 MHz cpu, 100 RAS, 25 CAS */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 16 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 16 MHz cpu, 60 ns RAM */
	{0x19, 0x38, 0x68, 0x10, 0x00},	/* 20 MHz cpu, 120 RAS, 30 CAS */
	{0x19, 0x38, 0x68, 0x10, 0x00},	/* 20 MHz cpu, 100 RAS, 25 CAS */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 20 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 20 MHz cpu, 60 ns RAM */
	{0x1a, 0x78, 0x60, 0x10, 0x00},	/* 25 MHz cpu, 120 RAS, 30 CAS */
	{0x1a, 0x78, 0x60, 0x10, 0x00},	/* 25 MHz cpu, 100 RAS, 25 CAS */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 25 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 25 MHz cpu, 60 ns RAM */
	{0x02, 0x28, 0x08, 0x10, 0x00},	/* 33 MHz cpu, 120 RAS, 30 CAS */
	{0x02, 0x28, 0x08, 0x10, 0x00},	/* 33 MHz cpu, 100 RAS, 25 CAS */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 33 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 33 MHz cpu, 60 ns RAM */
	
	/* NeXT_X15, parity */
	{0x02, 0x28, 0x08, 0x10, 0x00},	/* 16 MHz cpu, 120 RAS, 30 CAS */
	{0x02, 0x28, 0x08, 0x10, 0x00},	/* 16 MHz cpu, 100 RAS, 25 CAS */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 16 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 16 MHz cpu, 60 ns RAM */
	{0x06, 0x24, 0x68, 0x10, 0x00},	/* 20 MHz cpu, 120 RAS, 30 CAS */
	{0x06, 0x24, 0x68, 0x10, 0x00},	/* 20 MHz cpu, 100 RAS, 25 CAS */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 20 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 20 MHz cpu, 60 ns RAM */
	{0x07, 0x64, 0x60, 0x10, 0x00},	/* 25 MHz cpu, 120 RAS, 30 CAS */
	{0x07, 0x64, 0x60, 0x10, 0x00},	/* 25 MHz cpu, 100 RAS, 25 CAS */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 25 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 25 MHz cpu, 60 ns RAM */
	{0x02, 0x28, 0x08, 0x10, 0x00},	/* 33 MHz cpu, 120 RAS, 30 CAS */
	{0x02, 0x28, 0x08, 0x10, 0x00},	/* 33 MHz cpu, 100 RAS, 25 CAS */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 33 MHz cpu, 80 ns RAM */
	{0x00, 0x00, 0x48, 0x00, 0x00},	/* 33 MHz cpu, 60 ns RAM */
},
};

u_char no_addr[3] = { 0xff, 0xff, 0xff };
extern char loginwindow[];

mon_setup_comm (sid, mg)
	register struct mon_global *mg;
{
	register struct nvram_info *ni = &mg->mg_nvram;
	int nvram_bad;

	bzero (&mg->km, sizeof (mg->km));
	bzero (mg->addr, sizeof (int) * NADDR);
	mg->na = 0;
	mg->fmt = 'l';
	mg->mg_radix = 16;
	mg->mg_sid = sid;
	mg->mg_console_i = CONS_I_KBD;
	mg->mg_console_o = CONS_O_BITMAP;

	nvram_bad = nvram_check (ni);
	if (nvram_bad || ni->ni_reset != NI_RESET) {
		bzero (ni, sizeof (*ni));
		ni->ni_reset = NI_RESET;
		ni->ni_brightness = BRIGHT_MAX;
		ni->ni_pot[0] = POT_ON | TEST_DRAM_POT;
		strcpy (ni->ni_bootcmd, "od");
		ni->ni_vol_l = ni->ni_vol_r = VOL_NOM;
		ni->ni_allow_eject = 1;
	}
	return (nvram_bad);
}

mon_test_msg (sid, mg)
	register struct mon_global *mg;
{
	register struct nvram_info *ni = &mg->mg_nvram;
	extern char bright_table[], test[];

	mon_setup_comm (sid, mg);

#if	NCC || NO_SPACE
#else	NCC || NO_SPACE
	if (ni->ni_pot[0] & (POT_ON | TEST_DRAM_POT)) {
		drawCompressedPicture (LOGIN_X, LOGIN_Y, loginwindow);
		pprintf (TEXT_X, TEXT_Y2, 1, "Testing\nsystem ...");
		drawCompressedPicture (TEST_X, TEST_Y, test);
		return (MGF_LOGINWINDOW);
	}
#endif	NCC || NO_SPACE
	return (0);
}

mon_setup (sid, mg, nmi, memtest)
	register struct mon_global *mg;
	int memtest;				/* flag to test main memory */
{
	register struct nvram_info *ni = &mg->mg_nvram;
	register int i;
	register u_char *cp;
	char *c, *test_msg;
	volatile struct scr1 scr1;
	int nvram_bad = 0, mem_size, config, top_mem, vbr, top_region,
		nvram_rewrite = 0;
	extern char *skipwhite();
	u_char errorCode, poweron_test();
	int mon_getc(), mon_try_getc(), mon_putc(), alert(), as_tune();

	if (!nmi) {
		save_mg (mg);
		nvram_bad = mon_setup_comm (sid, mg);
		mg->mg_sid = sid;
		mg->mg_getc = &mon_getc;
		mg->mg_try_getc = &mon_try_getc;
		mg->mg_putc = &mon_putc;
		mg->mg_alert = &alert;
		mg->mg_alloc = &mon_alloc;
		mg->mg_major = MAJOR;
		mg->mg_minor = MINOR;
		mg->mg_seq = SEQ;
		mg->mg_clientetheraddr = (char*) clientetheraddr;
		mg->mg_pagesize = 8192;
		mg->test_msg = (char*) (P_VIDEOMEM + P_VIDEOSIZE);
		mg->eventc_latch = (volatile u_char*) P_EVENTC;
		mg->event_high = 0;
		mg->mg_as_tune = &as_tune;
		mg->mg_flags2 = 0;
	} else {
		bzero (&mg->km, sizeof (mg->km));
		mg->km.flags |= KMF_NMI;
		mg->mg_flags &= ~MGF_ANIM_RUN;
	}

	scr1 = *(struct scr1*) P_SCR1;
	mg->mg_dmachip = 313;
	mg->mg_diskchip = 310;
	mg->mg_intrmask = (int*) P_INTRMASK;
	mg->mg_intrstat = (int*) P_INTRSTAT;
	mg->mg_machine_type = MACHINE_TYPE(scr1.s_cpu_rev);
	mg->mg_board_rev = BOARD_REV(scr1.s_cpu_rev);

	/* see messages if not autobooting or NMI or verbose mode */
#if	NO_SPACE || NCC
	if (1 || ni->ni_bootcmd[0] == 0 || nmi ||
#else	NO_SPACE || NCC
	if (ni->ni_bootcmd[0] == 0 || nmi ||
#endif	NO_SPACE || NCC
	    ((ni->ni_pot[0] & POT_ON) && (ni->ni_pot[0] & VERBOSE_POT)) ||
	    ((ni->ni_pot[0] & POT_ON) && (ni->ni_pot[0] & LOOP_POT)))
		mg->km.flags |= KMF_SEE_MSGS;

        if (!nmi && set_SCR2_memType(mg))
	{
	    tprintf("Main Memory Configuration Test Failed\n\n");
	    selftest_fail (!(mg->km.flags & KMF_SEE_MSGS));
	}

#if	NCC
#else	NCC
        if (memtest)  
	    if (mainRAMTest(sid, mg))
	    {
		tprintf("Main Memory Test Failed\n\n");
	    	selftest_fail (!(mg->km.flags & KMF_SEE_MSGS));
	    }
#endif	NCC
	
#ifdef	MC68030
	printf ("CPU MC68030 ");
#endif	MC68030
#ifdef	MC68040
	printf ("CPU MC68040 ");
#endif	MC68040
	if (bcmp (clientetheraddr+3, no_addr, 3))
		cp = clientetheraddr;
	else
		cp = ni->ni_enetaddr;
	printf ("%d MHz, memory %d nS\nBackplane slot #%d\n"
		"Ethernet address: %x:%x:%x:%x:%x:%x\n",
		speed_cpu[scr1.s_cpu_clock], speed_mem[scr1.s_mem_speed],
		(mg->mg_sid >> 28) / 2,
		cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]);
#if 0
	printf ("DMA chip %d, disk chip %d\n",
		mg->mg_dmachip, mg->mg_diskchip);
	for (i = 0; i < NMTREG; i++)
		printf ("0x%x ", mem_timing[(scr1.s_cpu_clock << 2) +
			scr1.s_mem_speed][i]);
	printf ("\n");
#endif
	if (nvram_bad)
		printf ("Warning: non-volatile memory is uninitialized.\n");
	test_msg = (char*) (P_VIDEOMEM + P_VIDEOSIZE);
	if (!nmi || (nmi && mg->test_msg != test_msg))
		mem_probe (mg);

	top_region = -1;
	for (mem_size = 0, i = 0; i < N_SIMM; i++) {
		mem_size += simm_size[mg->mg_simm[i]];
		if (mg->mg_simm[i] != SIMM_EMPTY)
			top_region = i;
			
		/* keep SIMM_PARITY bit in high 4 bits of NVRAM for backward compat. */
		if (nvram_bad) {
			ni->ni_simm |= (mg->mg_simm[i] & 7) << (i*3);
			ni->ni_simm |= (mg->mg_simm[i] & 8) << (i+9);
		} else {
			config = (ni->ni_simm >> (i*3)) & 7;
			config |= (ni->ni_simm >> (i+9)) & 8;
			if (mg->mg_simm[i] != config) {
printf ("Memory sockets %d-%d configured for %s SIMMs but have %s SIMMs installed.\n",
					i*4, i*4 + 3, simm_str[config],
					simm_str[mg->mg_simm[i]]);
				/* FIXME: do something smarter here? */
				ni->ni_simm &= ~(7 << (i*3));
				ni->ni_simm &= ~(8 << (i+9));
				ni->ni_simm |= (mg->mg_simm[i] & 7) << (i*3);
				ni->ni_simm |= (mg->mg_simm[i] & 8) << (i+9);
				nvram_rewrite = 1;
			}
		}
	}
	printf ("Memory size %dMB", mem_size/MB);
	if (mg->mg_flags2 & MGF2_PARITY)
		printf(", parity enabled");
	printf ("\n");
	if (nvram_bad || nvram_rewrite)
		nvram_set (ni);

	/*
	 *  Return after normal NMIs because the stack etc. has
	 *  already been moved to main memory.  Don't return
	 *  after an NMI following a self test failure because
	 *  the stack etc. needs to be setup.  If there is no
	 *  main memory that passed self test then we can't go on.
	 */
	if (nmi) {
		printf ("\n");
		
		/* display self test error messages */
		test_msg = (char*) (P_VIDEOMEM + P_VIDEOSIZE);
		if (mg->test_msg != test_msg)
			for (c = test_msg; c < mg->test_msg; c++)
				putchar (*c);
		if (top_region == -1) {
no_mem:			printf ("can't continue without "
				"some working memory\n");
			while (1)
				km_power_check();
		}
		if (mg->test_msg != test_msg) {
			mg->test_msg = test_msg;
		} else 
			return;
	} else
	if (top_region == -1)
		goto no_mem;

	/* relocate monitor globals to top of memory */
	top_mem = mg->mg_region[top_region].last_phys_addr;
	top_mem -= sizeof (struct mon_global);
	bcopy (mg, top_mem, sizeof (struct mon_global));
	mg = (struct mon_global*) top_mem;

	/* relocate vectors */
	top_mem -= 256*4;
	vbr = get_vbr();
	bcopy (vbr, top_mem, 256*4);
	set_vbr (top_mem);
	save_mg ((struct mon_global*) mg);
	mg->mg_vbr = top_mem;
	
	/* refresh ni pointer since mg is moved */
	ni = &mg->mg_nvram;

	/* define new monitor stack (actually setup when booting) */
	mg->mg_mon_stack = top_mem;
	top_mem -= STACK_SIZE;
	mg->mg_alloc_base = (caddr_t) top_mem;
	mg->mg_alloc_brk = mg->mg_alloc_base;
	top_mem = top_mem & ~(mg->mg_pagesize - 1);
	mg->mg_region[top_region].last_phys_addr = top_mem;

#if	NCC
#else	NCC
	if (!nmi && (ni->ni_pot[0] & POT_ON)) {
		if (errorCode = poweron_test()) {
			if (ni->ni_pot[2] != errorCode) {
				ni->ni_pot[1] = ni->ni_pot[2];
				ni->ni_pot[2] = errorCode;
			}
			nvram_set (ni);
			if (mg->km.flags & KMF_SEE_MSGS) {
				tprintf ("\n\n");
			}
			tprintf("System test failed.  Error code %x.\n\n",
				errorCode);
			selftest_fail (!(mg->km.flags & KMF_SEE_MSGS));
		} else if (ni->ni_pot[0] & POT_ON)
			printf("\nSystem test passed.\n");
	}
#endif	NCC
	printf ("\n");
}

main (r, sid, mg, memtest)
	struct regs r;
	int sid;				/* passed by T_RESET only */
	register struct mon_global *mg;		/* passed by T_RESET only */
	int memtest;				/* flag to test main memory */
{
	register struct nvram_info *ni;
#define	PMAX	16
	char *lp, *ap, pwd[PMAX];
	int val, go, fcode, *poll, i;
	extern char *skipwhite(), *scann();
	extern t_boot();
	extern char bright_table[];
	u_char errorCode, poweron_test();
	u_char next_char;
	volatile struct bmap_chip *bm = (volatile struct bmap_chip*) P_BMAP;

	if (r.r_vector == T_RESET) {
#if	ODBOOT
		od_reset();
#endif	ODBOOT
		mon_setup (sid, mg, 0, memtest);
	}

	mg = restore_mg();
	sid = mg->mg_sid;
	ni = &mg->mg_nvram;
	lp = mg->mg_inputline;

	switch (r.r_vector) {

	case T_RESET:
#if	NO_SPACE
		if (0 && ni->ni_bootcmd[0] != 0) {
#else	NO_SPACE
		if (ni->ni_bootcmd[0] != 0) {
#endif	NO_SPACE
			if ((mg->mg_flags & MGF_LOGINWINDOW) == 0 &&
			    (mg->km.flags & KMF_SEE_MSGS) == 0)
				drawCompressedPicture (LOGIN_X, LOGIN_Y, 
					loginwindow);
#if	0 && ODBOOT
			if (km_prim_ischar(0) && km_prim_getc(0) == -1)
				od_eject (0, 0);
#endif	ODBOOT

			/* boot extended diagnostics if requested */
			if (ni->ni_pot[0] & BOOT_POT) {
				*lp++ = ni->ni_bootcmd[0];
				*lp++ = ni->ni_bootcmd[1];
				strcpy (lp, "diagnostics");
			} else
				*lp = 0;
			mg->mg_boot_arg = mg->mg_inputline;
			mg->mg_boot_how = SIO_AUTOBOOT;
			switch_stack_and_go (mg->mg_mon_stack, t_boot,
				mg->mg_vbr);
	 		/* NOTREACHED */
		}
		break;

	case T_BOOT:
		/*
		 * Because the monitor may be running off a kernel stack
		 * during a boot request, we must switch to the monitor
		 * stack (mg_mon_stack) before doing the boot.  This is
		 * done by doing a switch_stack_and_go() to t_boot()
		 * anytime a boot is required.
		 */

		/* reset devices */
		*mg->mg_intrmask = 0;
		zs_init();
		((struct od_regs*) P_DISK)->r_disr = OMD_CLR_INT;
		((struct od_regs*) P_DISK)->r_dimr = 0;
		((struct od_regs*) P_DISK)->r_cntrl1 = 0;
		((struct scsi_5390_regs*) P_SCSI)->s5r_cmd =
			S5RCMD_RESETBUS;
		*(int*)P_SCR2 &= ~DSP_RESET;
		rtc_reset();
		mg->mg_boot_info = 0;

		/*
		 * When we get here, we are either booting from a RESET
		 * or we are rebooting from the Mach kernel and
		 * have passed through t_boot().  In either case, the MMU is
		 * off, and we may have to replace virt addresses in the
		 * console config. with correct physical addresses.
		 */
		km_get_config( mg, mg->km_coni.slot_num, mg->km_coni.fb_num );

		/*
		 *  Always setup thin/TPE selection here so higher level software
		 *  doesn't have to worry about it and can retain any previous
		 *  interface selection at a lower level.  Select thin by default.
		 */
		((struct en_regs*) P_ENET)->en_reset = EN_RESET;
		((struct en_regs*) P_ENET)->en_txmode = EN_TXMODE_NO_LBC;
#if	MC68040
		bm->bm_ddir = BMAP_TPE;
		bm->bm_drw = ~BMAP_TPE;
#endif	MC68040

		if ((go = boot (mg, mg->mg_boot_how)) == 0) {
			break;
		}

		mg->km.flags &= ~KMF_ALERT;
		asm ("movw #0x2700,sr");
		switch_stack_and_go (mg->mg_mon_stack, go, mg,
			mg->mg_console_i, mg->mg_console_o,
			mg->mg_boot_dev, mg->mg_boot_arg, mg->mg_boot_info,
			mg->mg_sid, mg->mg_pagesize, N_SIMM, mg->mg_region,
			bcmp (clientetheraddr+3, no_addr, 3)?
			clientetheraddr : ni->ni_enetaddr, mg->mg_boot_file);
		/* NOTREACHED */
		break;

	case T_MON_BOOT:
		/* take over vectors again */
		set_vbr (mg->mg_vbr);

		/* kernel may have left the UART in a wierd state */
		mg->mg_flags &= ~MGF_UART_SETUP;
		mg->mg_boot_arg = (char*) r.r_dreg[0];
		nvram_check (ni);

		/* don't reboot -- just halt */
		if (strcmp (mg->mg_boot_arg, "-h") == 0) {
		
			/*
			 *  If text on screen from previous alert() call
			 *  then don't clear the screen.
			 */
			if ((mg->km.flags & KMF_ALERT) == 0) {
				km_clear_screen();
				mg->km.flags &= ~KMF_INIT;
				mg->km.flags |= KMF_SEE_MSGS;
			}
			break;
		}
		/*
		 * Note that the MMU is still active, with frame buffer
		 * mappings as set by the kernel.  We don't have to 
		 * reset the console config. data structures until
		 * after t_boot() turns off the MMU.
		 */
		km_clear_screen();

		/* if there was a popup last time then do it again */
		if (mg->km.flags & KMF_SEE_MSGS)
			mg->km.flags &= ~KMF_INIT;
		else
			drawCompressedPicture (LOGIN_X, LOGIN_Y, loginwindow);

		/* if just flags, use default boot command */
		if (*mg->mg_boot_arg == '-') {
			if (ni->ni_bootcmd[0] == 0) {
				mg->km.flags &= ~KMF_INIT;
				mg->km.flags |= KMF_SEE_MSGS;
				printf ("No default boot command.\n");
				break;
			}
			strcpy (lp, ni->ni_bootcmd);
			strcat (lp, " ");
			strcat (lp, mg->mg_boot_arg);
		} else
			strcpy (lp, mg->mg_boot_arg);
		mg->mg_boot_arg = lp;
		mg->mg_boot_how = SIO_ANY;
		switch_stack_and_go (mg->mg_mon_stack, t_boot, mg->mg_vbr);
		/* NOTREACHED */
		break;

	case T_LEVEL7:
		if (*mg->mg_intrstat & I_BIT (I_NMI)) {
			km_clear_nmi();
			mon_setup (sid, mg, 1, 0);
			mg = restore_mg();
			break;
		}
		km_clear_screen();
		mg->km.flags &= ~KMF_INIT;
		mg->km.flags |= KMF_SEE_MSGS;
		break;

	default:
		mg->km.flags &= ~KMF_INIT;
		mg->km.flags |= KMF_SEE_MSGS;
		if (mg->mg_nofault) {
			/*
			 * Convert to normal 4 word frame so we can
			 * rte to nofault routine
			 */
			switch (r.r_format & 0xF) {/* Sign extension in bitfields! */
			case EF_NORMAL4:
			case EF_THROWAWAY:
				break;
			case EF_NORMAL6:
				r.r_sp += sizeof(struct excp_normal6);
				break;
#ifdef	MC68030
			case EF_COPROC:
				r.r_sp += sizeof(struct excp_coproc);
				break;
			case EF_SHORTBUS:
				r.r_sp += sizeof(struct excp_shortbus);
				break;
			case EF_LONGBUS:
				r.r_sp += sizeof(struct excp_longbus);
				break;
#endif	MC68030
#ifdef	MC68040
			case EF_FLOATPT:
				r.r_sp += sizeof(struct excp_floatpt);
				break;
			case EF_ACCESS:
				r.r_sp += sizeof(struct excp_access);
				break;
#endif	MC68040
			default:
				printf("bogus stack frame\n");
				mg->mg_nofault = 0;
				goto bad_exception;
			}
			r.r_pc = (int)mg->mg_nofault;
			mg->mg_nofault = 0;
			return;
		}
bad_exception:
		printf ("Exception #%d (0x%x) at 0x%x\n",
			r.r_vector >> 2, r.r_vector, r.r_pc);
#if	DEBUG
		printf ("sp 0x%x fa 0x%x\n", r.r_sp,
			((struct excp_access*)((int)&r + sizeof(r)))->e_faultaddr);
#endif	DEBUG
		break;
	}

#ifdef	notdef
	if (sid != 0) {		/* slave CPU */
		printf ("Waiting for slot #0 to boot\n");
		poll = (int*) (mg->mg_sid + P_MAINMEM);	/* FIXME: where? */
		*poll = 0;
		while (*poll == 0)
			;
		printf ("Starting execution at 0x%x\n", *poll);
		go = *poll;
		mg->mg_boot_how = SIO_ANY;
		switch_stack_and_go (mg->mg_mon_stack, go
			/* FIXME: pass which boot args here? */);
	}
#endif	notdef

	fcode = r.r_sr & SR_SUPER ? FC_SUPERD : FC_USERD;
	mg->mg_flags &= ~MGF_LOGINWINDOW;

	while (printf ("NeXT>"), 1) {
	lp = mg->mg_inputline;
#if	MON_MOUSE
	while (gets (lp, LMAX, 1) < 0) {

		/* mouse down */
		for (i = 0; i <= DEV_N; i++) {
			struct bitmap *b;

			b = &bm[i];
			if (mg->mx > b->bm_x && mg->mx < b->bm_x + b->bm_w &&
			    mg->my > b->bm_y && mg->my < b->bm_y + b->bm_h) {
				strcpy (lp, b->bm_action);
				if (i == BM_LOGO) {
					mg->km.flags &= ~KMF_INIT;
					mg->km.flags |= KMF_SEE_MSGS;
				} else
					show_bitmap (b, BM_HILITE);
				goto hit;
			}
		}
	}
hit:
#else	MON_MOUSE
	gets (lp, LMAX, 1);
#endif	MON_MOUSE
	lp = skipwhite (lp);
	if (*lp == 0)
		continue;
		
	if ((mg->mg_flags & MGF_LOGINWINDOW) || /* already login */
	    (ni->ni_hw_pwd != HW_PWD) || 	/* hardware password not set */
	    (*lp == 'b' && *(lp + 1) == 0) || 	/* boot default */
	    (*lp == 'h') ||
	    (*lp == '?') ||
	    (*lp == 'c') ||
	    (*lp == 'm') ||
	    (*lp == 'e' && *(lp + 1) == 'c') ||
	    (*lp == 'p') || 			/* already protected */
	    (*lp == 'b' && *(lp + 1) == '?') ||	
	    (*lp == 'e' && *(lp + 1) == 'j' && ni->ni_allow_eject) ||
	    (*lp == 'e' && *(lp + 1) == 'o' && ni->ni_allow_eject) ||
	    (*lp == 'b' && ni->ni_boot_any) ||
	    (*lp != 'P' && ni->ni_any_cmd)); 	/* if P call login(1) */
	else if (login(1) == 0)
	    continue; /* go back to the top and don't honor the cmd */

	switch (*lp++) {
	case 'P':
		printf ("New password: ");
		bzero (pwd, PMAX);
		gets (pwd, PMAX, 0);
		printf ("\n");
		nvram_check (ni);
		if (pwd[0] == 0) {
			ni->ni_hw_pwd = 0;
		} else {
			printf ("Retype new password: ");
			bzero (lp, PMAX);
			gets (lp, PMAX, 0);
			printf ("\n");
			if (strncmp (pwd, lp, PMAX)) {
				printf ("Mismatch - password unchanged\n");
			} else {
				for (i = 0; i < NVRAM_HW_PASSWD; i++) {
					ni->ni_hw_passwd[i] = pwd[i] ^ 'N';
				}
				ni->ni_hw_pwd = HW_PWD;
			}
		}
		nvram_set (ni);
		bzero (pwd, PMAX);
		bzero (lp, PMAX);
		break;
#ifdef	notdef
	case 't':	/* FIXME: need this case to debug power on test */
		if(errorCode = poweron_test())
		  printf("\n\nSystem test failed.  Error code %x.\n\n",
			errorCode);
		else 
		  printf("\nSystem test passed.\n");
		break;
#endif
	case 'b':
		mg->mg_boot_arg = lp;
		mg->mg_boot_how = SIO_ANY;
		switch_stack_and_go (mg->mg_mon_stack, t_boot, mg->mg_vbr);
		/* NOTREACHED */
		break;
	case 'a':
		setreg (lp, addrregs, &r, "a");
		break;
	case 'd':
		setreg (lp, dataregs, &r, "d");
		break;
	case 'r':
		setreg (lp, cpuregs, &r, "");
		break;
	case 'm':
		for (i = 0; i < N_SIMM; i++)
	printf ("Memory sockets %d-%d have %s SIMMs installed (0x%x-0x%x)\n",
			i*4, i*4 + 3, simm_str[mg->mg_simm[i]],
			mg->mg_region[i].first_phys_addr,
			mg->mg_region[i].last_phys_addr);
		break;
	case 'p':
		if (setreg (lp, nvram, ni, ""))
			nvram_set (ni);
		break;
	case 's':
		setreg (lp, systemregs, mg->mg_sid, "");
		break;
	case 'e':
		next_char = *lp;
		switch(next_char) {
		    case 'c':
			/* print recorded self test error codes */
			printf("Old error code: %x\nLast error code: %x\n", 
				ni->ni_pot[1], ni->ni_pot[2]);
			break;
			
#if	ODBOOT
		    case 'j':	/* for compatibility */
		    case 'o':	/* optical */
#endif	ODBOOT
#if	FDBOOT
		    case 'f':	/* floppy */
#endif	FDBOOT
#if	defined(ODBOOT) || defined(FDBOOT)
			i = 0;
			while (*(++lp))
				if (*lp == '0' || *lp == '1') {
					i = *lp - '0';
					break;
				}
#endif	defined(ODBOOT) || defined(FDBOOT)
#if	FDBOOT
			if(next_char == 'f') {
				fd_eject(i);
				break;
			}
#endif	FDBOOT
#if	ODBOOT
			if((next_char == 'j') || (next_char == 'o'))
				od_eject(i, 0);
				break;
#endif	ODBOOT
			/* else drop thru */
		    default:
			examine (lp, fcode);
			break;
		}
		break;
#if 0
	case 'f':
		fill (lp, fcode);
		break;
	case 'v':
		view (lp, fcode);
		break;
#endif
	case 'c':
		return;
#if	0
	case 'g':
		ap = lp;
		if (ap = scann (ap, &val, 0))
			go = val;
		else
			go = r.r_pc;
		mg->mg_boot_arg = ap;
		mg->mg_boot_dev = 0;
		mg->mg_boot_info = 0;
		switch_stack_and_go (mg->mg_mon_stack, go, mg,
			mg->mg_console_i, mg->mg_console_o,
			mg->mg_boot_dev, mg->mg_boot_arg, mg->mg_boot_info,
			mg->mg_sid, mg->mg_pagesize, N_SIMM, mg->mg_region,
			bcmp (clientetheraddr+3, no_addr, 3)?
			clientetheraddr : ni->ni_enetaddr, mg->mg_boot_file);
		/* NOTREACHED */
		break;
#endif
	case 'S':
		if (scann (lp, &val, 10))
			fcode = val & 7;
		printf ("Function code %d (%s)\n", fcode, space[fcode]);
		break;
	case 'R':
		if (scann (lp, &val, 10))
			mg->mg_radix = val;
		printf ("default input radix %d\n", mg->mg_radix);
		break;
#if	ENETDIAG
	case 'E':
		enet();
		break;
#endif	ENETDIAG
#if	DBUG_DMA
	case 'D':
		dbug_dma(2);
		break;
#endif	DBUG_DMA
#if 0
	case 'T':
		while (1) {
			led_On(0);
			delay(1000000);
			led_Off(0);
			delay(1000000);
		}
		break;
#endif
#if	NO_SPACE
#else	NO_SPACE
	case 'h':
	case '?':
		help();
		break;
#endif	NO_SPACE
	default:
		printf ("Huh?\n");
		break;
	}
	}
}

mem_probe (mg)
	register struct mon_global *mg;
{
	register int i;
	register struct mon_region *rp;

	/* FIXME: actually count up memory and compare against SIMM size */
	for (i = 0; i < N_SIMM; i++) {
		rp = &mg->mg_region[i];
		if (mg->mg_simm[i] != SIMM_EMPTY) {
			rp->first_phys_addr = P_MAINMEM +
				i * (P_MEMSIZE / N_SIMM);
			rp->last_phys_addr = rp->first_phys_addr +
				simm_size[mg->mg_simm[i]];
		} else {
			rp->first_phys_addr = 0;
			rp->last_phys_addr = 0;
		}
	}
}

probe_rb(mg, addr)
	register struct mon_global *mg;
	volatile char *addr;
{
	int junk;
	void probe_recover();

	/*
	 * This test is just a gross hack to avoid a compiler
	 * warning about the probe_recover code being unreachable
	 */
	if (mg == (struct mon_global *)0)
		goto avoid_unreachable_error;
	mg->mg_nofault = probe_recover;
#if	MC68040
	asm("nop");	/* Sync insn pipeline and avoid insn overlap! */
#endif	MC68040
	junk = *addr;
#if	MC68040
	asm("nop");	/* allow bus write to complete! */
#endif	MC68040
	mg->mg_nofault = (void (*)())0;
	return(1);

avoid_unreachable_error:
	asm("	.globl	_probe_recover");
	asm("_probe_recover:");
	mg->mg_nofault = (void (*)())0;
	return(0);
}

/*
 * When probing NextBus hardware, be aware that some things may only
 * respond to 32 bit read cycles!  This code uses the probe_recover()
 * entry point defined above.
 */
probe_rw(mg, addr)
	register struct mon_global *mg;
	volatile int *addr;
{
	int junk;
	void probe_recover();


	mg->mg_nofault = probe_recover;
#if	MC68040
	asm("nop");	/* Sync insn pipeline and avoid insn overlap! */
#endif	MC68040
	junk = *addr;
#if	MC68040
	asm("nop");	/* allow bus write to complete! */
#endif	MC68040
	mg->mg_nofault = (void (*)())0;
	return(1);
}

see_msg (s)
	char *s;
{
	register struct mon_global *mg = restore_mg();

	if ((mg->km.flags & KMF_SEE_MSGS) == 0) {
		mg->km.flags &= ~KMF_INIT;
		mg->km.flags |= KMF_SEE_MSGS;
	}
	printf (s);
}

selftest_fail (loop)
{
	extern char test[];

	if (loop) {
		drawCompressedPicture (LOGIN_X, LOGIN_Y, loginwindow);
		drawCompressedPicture (TEST_X, TEST_Y, test);
	}
	pprintf (TEXT_X, TEXT_Y3, 1, "System\ntest\nfailed");
	while (loop)
		km_power_check();
}

login (always_ask)
{
	struct mon_global *mg = restore_mg();
	struct nvram_info *ni = &mg->mg_nvram;
	int i;
	char pwd[PMAX];
	
	if (ni->ni_hw_pwd != HW_PWD || (mg->mg_flags & MGF_LOGINWINDOW) ||
	    (always_ask == 0 && ni->ni_any_cmd))
		return (1);
	printf ("Password: ");
	bzero (pwd, PMAX);
	gets (pwd, PMAX, 0);
	printf ("\n");
	for (i = 0; i < NVRAM_HW_PASSWD; i++) {
		if ((pwd[i] ^ 'N') != ni->ni_hw_passwd[i]) {
			printf ("Sorry\n");
			break;
		}
	}
	bzero (pwd, PMAX);
	if (i < NVRAM_HW_PASSWD)
		return (0);
	mg->mg_flags |= MGF_LOGINWINDOW;
	return (1);
}

type_help()
{
	printf ("usage error, type \"?\" for help\n");
}
