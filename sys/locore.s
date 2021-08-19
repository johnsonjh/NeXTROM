//	@(#)locore.s	1.0	10/17/86	(c) 1986 NeXT

#define	ICACHE		1
#define ASSEMBLER	1

#import <assym.h>
#import <mon/monparam.h>
#import <pot/POT.h>
#import <next/cframe.h>
#import <next/psl.h>
#import <next/mmu.h>
#import <next/mem.h>
#import <next/trap.h>
#import <next/cpu.h>
#import <next/scr.h>
#import <next/bmap.h>
#import <nextdev/dma.h>
#import <nextdev/video.h>

// first thing in the PROM
	.text
monitor:
	.long	0			// initial SP
	.long	_entry			// initial PC
	.globl	_clientetheraddr
_clientetheraddr:
	.byte	0,0,0xf,0xff,0xff,0xff	// Ethernet address
	.long	0xffffffff,0xffffffff	// reserved for future use
_enet_crc:
#if	DEBUG
	.long	0x6aab57ca		// CRC for lower memory chunk
					// ..that matches the dummy enet adr
#else	DEBUG
	.long	0xffffffff		// CRC for lower memory chunk
					// ..to be recorded in factory
#endif	DEBUG
	.globl	_monitor_crc
_monitor_crc:
	.long	0x18c25014

// We get here when someone yanks on the -reset line of the processor
// as a result of power-on or double bus fault.  The processor will set:
//
//	status (sr): t0/t1=0, s=1, m=0, il=7	super state, interrupts off
//	vector base (vbr): =0			vectors start at location 0
//	cache control (cacr): =0		data & inst caches disabled
//	translation control (tc): e=0		address translation disabled
//	transparent translation (tt0&1): e=0	transparent xlation disabled
//	stack pointer (sp): =0			fetched from loc vbr+0
//	program counter (pc): =_entry		fetched from loc vbr+4
//
// All other registers in the processor are unchanged at this point.
// -reset also clears system control register 2 (scr2) which enables
// ROM overlay mode that maps the monitor EPROM to absolute location zero
// until we turn it off.

#define	sid	a5			/* slot identifier */
	.globl	_entry
_entry:
#ifdef	MC68030
	movl	#CACR_CLR_ENA_I,a0
#endif	MC68030
#ifdef	MC68040
	// do double reset to fix 040 bug
	cmpl	#0xa5005aff,d1		// look for unique pattern
	beq	2f			// done reset before
	movl	#0xa5005aff,d1		// save the unique pattern
	movl	#0,(P_BMAP+BM_DRW)	// reset data reg
	movl	#BMAP_RESET,(P_BMAP+BM_DDIR)	// enable GPIO(2) for RESET
	movl	#0x0000ffff,d2		// set delay value
1:	
	dbra	d2,1b			// wait for reset
2:
	movl	#0,(P_BMAP+BM_DDIR)	// reset output

//	cinva	ic,dc
	.word	0xf4d8
#if	ICACHE
	movl	#(CACR_040_IE),a0
#else	ICACHE
	movl	#0,a0
#endif	ICACHE
#endif	MC68040
	movc	a0,cacr

#ifdef	MC68040
	 // Set "local only" mode, ROM local and 7 ROM waits states
	movl	#(BMAP_LO+BMAP_RML+0x07000000),(P_BMAP+BM_RCNTL)
#if	BMAP_EMUL
	movl	#0x02,(P_BMAP+BM_ASEN)
	orl	#SCR2_OVERLAY,P_SCR2
#else	BMAP_EMUL

	// do AS delay tuning
	lea	1f,a5
	jmp	as_tune
1:
	movl	#BMAP_BWE,(P_BMAP+BM_BURWREN)
	movl	#0xe1000000,(P_BMAP+BM_AMR)
#endif	BMAP_EMUL
#endif	MC68040

	movl	P_SID,d0		// get our slot id bits
	andl	#0xf0000000,d0
	movl	d0,sid
#ifdef	MC68030
	lea	sid@(cont:l),a0		// continue at slot-relative address
	jmp	a0@
cont:
	orl	#0x00001000,sid@(P_SCR2) // 7 wait states = 280 nS @ 25 MHz
#endif	MC68030
	orl	#SCR2_OVERLAY,sid@(P_SCR2) // turn off overlay mode
#ifdef	MC68040
	movl	d0,sid@(P_BMAP+BM_SID)		// set BMAP SID register
#endif	MC68040
#if	MEGABIT
	orl	#SCR2_ROM_1M,sid@(P_SCR2) // using a 1Mb EPROM
#endif	MEGABIT

	// set memory timing registers
	// note that constant is in EPROM and will not access RAM prematurely
	movl	sid@(P_SCR1),d0
	movl	d0,d1
	andl	#0x00000003,d1		// scr1.s_cpu_clock
	asll	#2,d1
	movl	d0,d2
	andl	#0x00000030,d2		// scr1.s_mem_speed
	asrl	#4,d2
	orl	d2,d1
	movl	d0,d2
	andl	#0x0000f000,d2		// scr1.s_cpu_rev (machine_type)
	asrl	#7,d2
	orl	d2,d1
	orl	#0x00000010,d1		// start with parity timing
	mulu	#NMTREG,d1
	movl	#_mem_timing:l,a0
	addl	d1,a0
	lea	sid@(P_MEMTIMING:l),a1
	movl	#NMTREG-1,d0
1:	movb	a0@+,a1@+
	dbra	d0,1b

#if	AS_MEAS || AS_MEAS1
	fmovel	fp0,0x0b03b000
	fmovel	fp1,0x0b03b004
	fmovel	fp2,0x0b03b008
	fmovel	fp3,0x0b03b00c
	fmovel	fp4,0x0b03b010
	fmovel	fp5,0x0b03b014
	fmovel	fp6,0x0b03b018
	fmovel	fp7,0x0b03b01c
#endif	AS_MEAS || AS_MEAS1
	// disable interrupts
	clrl	sid@(P_INTRMASK)

	// setup video DMA
	movl	#RETRACE_START,d0
	movl	d0,sid@(P_VIDEO_SPAD+8)	// start

	// verify ROM checksum
#if	DEBUG
#else	DEBUG
	movl	#(_monitor_crc+4),a1	// check local ROM from ...
					// ... end of crc to end of ROM
#if	MEGABIT
	movl	#(monitor+0x20000),d3	// 128KB = 1Mbit
#else	MEGABIT
	movl	#(monitor+0x10000),d3	// 64KB  = 512Kbit
#endif	MEGABIT
	subl	#(_monitor_crc+4),d3
	lea	0f,a0
	bra	calculateCRC
0:
	movw	#BLINK_CRC1,a0		// set blink count for blinkLed
	cmpl	_monitor_crc,d0
	bne	blinkLed

	movl	#(monitor),a1	// check local ROM from base 0
	movl	#(_monitor_crc - 4),d3	// ..till 4 bytes before monitor CRC
	subl	#(monitor),d3

	lea	1f,a0
	bra	calculateCRC
1:
	movw	#BLINK_CRC2,a0		// set blink count for blinkLed
	cmpl	_enet_crc,d0
	bne	blinkLed
#endif	DEBUG

#if	AS_MEAS || AS_MEAS1
#else	AS_MEAS || AS_MEAS1
	// test video RAM
	lea	2f,a7
	bra	NoStackVideoRAMTest
2:
#endif	AS_MEAS || AS_MEAS1

	//
	//  Place stack, mon_globals & vectors in off screen VRAM
	//  (in case main RAM is bad).
	//  They will get moved to main RAM later, at boot time.
	//
	lea	sid@(P_VIDEOMEM+0x40000-0x400-0x400),sp
	clrb	sp@(4)			// zero mg_flags
	lea	sp@(0x400),a0		// setup vbr
	movc	a0,vbr
	movl	sid,sp@(MG_sid)
	movl	sp,sp@-			// setup pointer to struct mg
 	jsr	_save_mg
	addl	#4,sp

	// test NVRAM
#if	NCC
	clrl	d2
#else	NCC
	jbsr	_NVRAMTest
	movl	d0,d2			// returns RAM test flag
#endif	NCC
	
	// catch exceptions: This trashes the store used by save_mg
	lea	_mon_trap,a0		// default exception handler
	addl	sid,a0
	movc	vbr,a1			// vector base
	movl	#256-1,d1		// number of vectors
1:	movl	a0,a1@+
	dbra	d1,1b

	/* enable MMU here so device accesses in power-on test will be serialized */
#if	MC68040
//	cinva	bc			// flush cache
	.word	0xf4d8
	movl	#0x00ffc000,d0		// except devices: cache enabled, writethrough
//	movc	d0,itt1
	.long	0x4e7b0005
//	movc	d0,dtt1
	.long	0x4e7b0007
	movl	#0x0200c040,d0		// devices: cache inhibited, serialized
//	movc	d0,itt0
	.long	0x4e7b0004
//	movc	d0,dtt0
	.long	0x4e7b0006
//	pflusha				// flush TLB
	.word	0xf518
	movl	#0xc000,d0		// enable MMU
//	movc	d0,tc
	.long	0x4e7b0003
#endif	MC68040

	// Find a console bitmap device to use for "testing" message
	movl	sp,sp@-			// setup pointer to struct mg
 	jsr	_save_mg		// restore global pointer to struct mg
	jsr	_km_select_console	// Get console dev, using struct mg pointer
	addl	#4,sp			// Pop the pointer off the stack

	// put up "testing" message
	movl	sp,a2			// point at mon_global area
	movl	a2,sp@-			// push initial mon_global area
	movl	sid,sp@-		// push slot id
	jsr	_mon_test_msg
	addl	#8,sp

	// setup to enter monitor
	movb	d0,sp@(4)		// set mg_flags to mon_test_msg ret val
	movl	sp,a0
	lea	exit_to_mon,a1		// where to go if user code does an rts
	movl	a1,sp@-
	movl	d2,sp@-			// RAM test flag
	movl	a0,sp@-			// initial mon_global area
	movl	sid,sp@-		// slot id
	movw	#T_RESET,sp@-		// reset pseudo-vector
	pea	sid@(USERENTRY)		// default user code entry address
	movw	sr,sp@-			// save sr
	
_mon_trap:
	subw	#R_sr,sp		// backup stack to start of reg frame
	moveml	#0xffff,sp@(R_d0)	// save all
	addl	#R_sr,sp@(R_sp)		// point saved ssp at exception frame
	movc	usp,a0
	movl	a0,sp@(R_usp)
	movc	sfc,a0
	movl	a0,sp@(R_sfc)
	movc	dfc,a0
	movl	a0,sp@(R_dfc)
	movc	vbr,a0
	movl	a0,sp@(R_vbr)
#ifdef	MC68030
	movc	caar,a0
	movl	a0,sp@(R_caar)
#endif	MC68030
	movc	cacr,a0
	movl	a0,sp@(R_cacr)
	movw	#0x2700,sr		// spl(7)
#ifdef	MC68030
	movl	#CACR_CLR_ENA_I,a0
#endif	MC68030
#ifdef	MC68040
//	cpusha	ic,dc
	.word	0xf4f8
#if	ICACHE
	movl	#(CACR_040_IE),a0
#else	ICACHE
	movl	#0,a0
#endif	ICACHE
#endif	MC68040
	movc	a0,cacr
	jbsr	_main			// run monitor
	movl	sp@(R_usp),a0
	movc	a0,usp
	movl	sp@(R_sfc),a0
	movc	a0,sfc
	movl	sp@(R_dfc),a0
	movc	a0,dfc
	movl	sp@(R_vbr),a0
	movc	a0,vbr
#ifdef	MC68030
	movl	sp@(R_caar),a0
	movc	a0,caar
#endif	MC68030
	movl	sp@(R_cacr),a0
	movc	a0,cacr
	movl	sp@(R_sp),a0		// has ssp been modified by mon cmd?
	lea	sp@(R_sr),a1
	cmpl	a0,a1
	jeq	1f			// no
	movw	sp@(R_sr),a0@		// yes, build resume exception frame
	movl	sp@(R_pc),a0@(2)
	clrw	a0@(6)			// dummy fmt/vor
	moveml	sp@(R_d0),#0xffff	// restore all (will use new ssp)
	rte
1:
	moveml	sp@(R_d0),#0x7fff	// restore all (except ssp)
	addw	#R_sr,sp		// pop register frame
	rte

	.globl	_parity_error
_parity_error:
	movl	#0,(P_BMAP+BM_BUSERR)
	rte

#ifdef	MC68030
	.data
zero:	.long	0
	.text
#endif	MC68030

	.globl	_t_boot
_t_boot:
#ifdef	MC68030
//	pmove	zero,tc			// disable MMU
	.long	0xf0394000
	.long	zero
#endif	MC68030
#if	MC68040
//	cinva	bc			// flush cache
	.word	0xf4d8
	movl	#0x00ffc000,d0		// except devices: cache enabled, writethrough
//	movc	d0,itt1
	.long	0x4e7b0005
//	movc	d0,dtt1
	.long	0x4e7b0007
	movl	#0x0200c040,d0		// devices: cache inhibited, serialized
//	movc	d0,itt0
	.long	0x4e7b0004
//	movc	d0,dtt0
	.long	0x4e7b0006
//	pflusha				// flush TLB
	.word	0xf518
	movl	#0xc000,d0		// enable MMU
//	movc	d0,tc
	.long	0x4e7b0003
#endif	MC68040
	movl	a_p0,d0			// restore our vbr
	movc	d0,vbr
	movw	#T_BOOT,sp@-		// boot pseudo-vector
	pea	sid@(USERENTRY)		// default user code entry address
	movw	sr,sp@-			// save sr
	jra	_mon_trap		// enter monitor

// We need a place to save the monitor global pointer that is slot independent.
// The master stack pointer is used by the kernel so we stash mg
// in the first entry of the vector table (T_STRAY).  The vector table
// is unique per single board computer -- any kernel we boot must respect
// this convention.
// FIXME: if T_STRAY (offset 0) is used instead of T_BADTRAP (offset 4)
// bit 0x10000 seems to get set when the kernel exits causing mg to be wrong!
	.globl _save_mg, _restore_mg
_save_mg:
	movc	vbr,a0
	movl	a_p0,a0@(T_BADTRAP)
	rts

_restore_mg:
	movc	vbr,a0
	movl	a0@(T_BADTRAP),d0
	rts

// Routines to access the vector base register
	.globl	_get_vbr
_get_vbr:
	movc	vbr,d0
	rts

	.globl	_set_vbr
_set_vbr:
	movl	a_p0,d0
	movc	d0,vbr
	rts

// return current ipl
	.globl	_curipl
_curipl:
	movw	sr,d0
	lsrl	#8,d0
	andl	#7,d0
	rts

// switch to a new stack and new pc while passing some arguments
	.globl	_switch_stack_and_go
_switch_stack_and_go:
	movl	a_p0,a0			// base of new stack
	movl	a_p1,a1			// go address
	movl	a_p16,a0@-		// push args on new stack
	movl	a_p15,a0@-
	movl	a_p14,a0@-
	movl	a_p13,a0@-
	movl	a_p12,a0@-
	movl	a_p11,a0@-
	movl	a_p10,a0@-
	movl	a_p9,a0@-
	movl	a_p8,a0@-
	movl	a_p7,a0@-
	movl	a_p6,a0@-
	movl	a_p5,a0@-
	movl	a_p4,a0@-
	movl	a_p3,a0@-
	movl	a_p2,a0@-
	movl	a0,sp			// switch to new stack..
	jsr	a1@			// ..and go
	// fall into ...

// If the user code returns with an "rts" we will end up here.
// Simulate a trap to get into the monitor.
	.globl	exit_to_mon
exit_to_mon:
	pea	exit_to_mon		// restack another pointer to here
exit_to_mon2:
	movw	#T_USEREXIT,sp@-	// build an exception frame
	pea	exit_to_mon2		// where to go if we just get continued
	orw	#0x2700,sp@-
	jra	_mon_trap

// Routines for accessing virtual spaces.
// get{lwb} (addr, virtual, fcode)
// put{lwb} (addr, virtual, fcode, value)
// Bus and address errors are caught by the standard monitor routines.
	.globl	_getl, _getw, _getb, _putl, _putw, _putb
_getl:
	movc	sfc,d1			// save sfc
	movl	a_p2,a0			// fcode to use
	movc	a0,sfc			// set sfc
	movl	a_p0,a0			// address to get
	movsl	a0@,d0			// get it (virtual)
	movc	d1,sfc			// restore sfc
	rts
_getw:
	movc	sfc,d1			// save sfc
	movl	a_p2,a0			// fcode to use
	movc	a0,sfc			// set sfc
	movl	a_p0,a0			// address to get
	subl	d0,d0			// zero high word of result
	movw	a0@,d0			// get it (virtual)
	movc	d1,sfc			// restore sfc
	rts
_getb:
	movc	sfc,d1			// save sfc
	movl	a_p2,a0			// fcode to use
	movc	a0,sfc			// set sfc
	movl	a_p0,a0			// address to get
	subl	d0,d0			// zero high 3 bytes of result
	movsb	a0@,d0			// get it (virtual)
	movc	d1,sfc			// restore sfc
	rts
_putl:
	movc	dfc,d1			// save dfc
	movl	a_p2,a0			// fcode to use
	movc	a0,dfc			// set dfc
	movl	a_p0,a0			// address to put
	movl	a_p3,d0			// value to put
	movsl	d0,a0@			// put it
	movc	d1,dfc			// restore dfc
	rts
_putw:
	movc	dfc,d1			// save dfc
	movl	a_p2,a0			// fcode to use
	movc	a0,dfc			// set dfc
	movl	a_p0,a0			// address to put
	movl	a_p3,d0			// value to put
	movsw	d0,a0@			// put it
	movc	d1,dfc			// restore dfc
	rts
_putb:
	movc	dfc,d1			// save dfc
	movl	a_p2,a0			// fcode to use
	movc	a0,dfc			// set dfc
	movl	a_p0,a0			// address to put
	movl	a_p3,d0			// value to put
	movsb	d0,a0@			// put it
	movc	d1,dfc			// restore dfc
	rts

	.globl	f68881_used
f68881_used:
	rts

// extern int burst_mode_ok(int* baseAdr);
	.globl _burst_mode_ok
_burst_mode_ok:

#define	TP1	0x01234567
#define	TP2	0x89abcdef
#define	TP3	0x092b4d6f
#define	TP4	0x81a3c5e7

	clrl	d0			// assume not page mode

	movel 	sp@(4),a0		// move mem ptr to register

	movl	#TP1,a0@+		// write test pattern
	movl	#TP2,a0@+
	movl	#TP3,a0@+
	movl	#TP4,a0@+
#ifdef	MC68030
	movl	#CACR_CLR_ENA,a0	// flush & enable D-cache
	movc	a0,cacr
#endif	MC68030
#ifdef	MC68040
//	cpusha	ic,dc
	.word	0xf4f8
#if	ICACHE
	movl	#(CACR_040_DE + CACR_040_IE),a0
#else	ICACHE
	movl	#(CACR_040_DE),a0
#endif	ICACHE
	movc	a0,cacr
	nop				// flush bus writes
#endif	MC68040

	movel 	sp@(4),a0		// move mem ptr to register
	
	tstb    a0@(4)                  // a kludge to flush the DMA
			                // buffer residue
			                // it is necessary since
	                                // the DMA buffer can have garbage
					// left over from previous test
					// or PROM reset that matches
					// this pattern.
	
	cmpl	#TP1,a0@+		// burst read worked?
	bne	burst_fail		// no, burst mode fails
	cmpl	#TP2,a0@+            	
	bne	burst_fail
	cmpl	#TP3,a0@+
	bne	burst_fail
	cmpl	#TP4,a0@+
	bne	burst_fail

	moveq 	#1,d0			// it is page mode
burst_fail:
#ifdef	MC68030
	movl	#CACR_CLR_ENA_I,a0	// disable D-cache
	movc	a0,cacr
#endif	MC68030
#ifdef	MC68040
//	cpusha	ic,dc
	.word	0xf4f8
#if	ICACHE
	movl	#CACR_040_IE,a0
#else	ICACHE
	movl	#0,a0
#endif	ICACHE
	movc	a0,cacr
#endif	MC68040
	rts

_BlinkDeath:	.globl _BlinkDeath
	movl	#BLINK_CRC2,sp@+		// set blink count for blinkLed
	jbsr	_blinkLed
	
	.globl	_bfins, _bfextu
_bfins:
	movl	a_p0,d0
	movl	a_p1,a0
	movl	a_p2,d1
	movl	a_p3,a1
	movl	d2,sp@-
	movl	a1,d2
	bfins	d0,a0@{d1:d2}
	movl	sp@+,d2
	rts

_bfextu:
	movl	a_p0,a0
	movl	a_p1,d0
	movl	a_p2,d1
	bfextu	a0@{d0:d1},d0
	rts

/* AS tuning routine */
#define	MINDELTA	5  // data is valid if it's same on next 5 taps
#define	TYPE_WINDOW	0
#define	TYPE_TRANS	1
#define	POL_ZERO	0
#define	POL_ONE		0xffffffff
#define	CLK_OFFSET	5

#define	d_pol		d2
#define	d_centerndx	d3
#define	d_testndx	d4
#define	d_i		d5
#define	d_bitmask	d6
#define	d_fdelta	d7
#define	a_found		a0
#define	a_type		a1
#define	a_swin		a2
#define	a_ewin		a3
#define	a_rts2		a4
#define	a_rts1		a5
#define	a_v0		a6
#define	a_v1		a7

scan_timing:
	clrl	d_fdelta
while1:
	cmpl	#MINDELTA,d_fdelta
	bge	out2			// found valid first transition?
	cmpl	#33,d_i
	bge	out1			// end of tstatus0 reg?
	movl	a_v0,d0
	andl	d_bitmask,d0
	movl	d_pol,d1
	andl	d_bitmask,d1
	cmpl	d0,d1
	beq	1f			// data bit match polarity?
	clrl	d_fdelta		// no, either glitched or not matched
	bra	2f
1:
	tstl	d_fdelta		
	bne	3f			// saw transition before?
	movl	d_i,a_found		// no, mark the position
3:
	addl	#1,d_fdelta		// increment de-glitch counter
2:
	addl	#1,d_i			// increment bit index
	lsll	#1,d_bitmask
	bra	while1
out1:
	movl	#1,d_bitmask		// continue searching with tstatus1 reg
while2:
	cmpl	#MINDELTA,d_fdelta
	bge	out2			// found valid first transition?
	cmpl	#65,d_i			
	bge	out2			// end of tstatus1 reg?
	movl	a_v1,d0
	andl	d_bitmask,d0
	movl	d_pol,d1
	andl	d_bitmask,d1
	cmpl	d0,d1
	beq	1f			// data bit match polarity?
	clrl	d_fdelta		// no, either glitched or not matched
	bra	2f
1:
	tstl	d_fdelta
	bne	3f			// saw transition before?
	movl	d_i,a_found		// no, mark the position
3:
	addl	#1,d_fdelta		// increment de-glitch counter
2:
	addl	#1,d_i			// increment bit index
	lsll	#1,d_bitmask
	bra	while2
out2:
	cmpl	#TYPE_TRANS,a_type
	bne	1f
	movl	a_found,d0
	jmp	a4@
1:
	movl	a_found,d_fdelta		// save ewin
	
	// search for opposite transition
	movl	#0,a_found
	eorl	#0xffffffff,d_pol		// flip polarity
while3:
	cmpl	#33,d_i
	bge	out3			// end of tstatus0 reg?
	movl	a_v0,d0
	andl	d_bitmask,d0
	movl	d_pol,d1
	andl	d_bitmask,d1
	cmpl	d0,d1
	bne	1f			// data bit not match polarity?
	movl	d_i,a_found		// no, then no need to search further 
	bra	out4			// de-glitching is not needed on second transition	
1:
	addl	#1,d_i
	lsll	#1,d_bitmask
	bra	while3
out3:
	movl	#1,d_bitmask
while4:
	cmpl	#65,d_i
	bge	out4			// end of tstatus1 reg?
	movl	a_v1,d0
	andl	d_bitmask,d0
	movl	d_pol,d1
	andl	d_bitmask,d1
	cmpl	d0,d1
	bne	1f			// data bit not match polarity?
	movl	d_i,a_found		// no, then no need to search further
	bra	out4			// de-glitching is not need on second transition	
1:
	addl	#1,d_i
	lsll	#1,d_bitmask
	bra	while4
out4:
	movl	d_fdelta,d0			// restore ewin
	movl	a_found,d1			// save swin
	jmp	a4@

	.globl	_as_tune
_as_tune:
	moveml	#0xfffe,sp@-
	movl	(P_BMAP+BM_RCNTL),sp@-		// save state of local mode
	movc	vbr,a0
	movl	sp,a0@				// save stack pointer
//	movc	tc,d0				// save MMU enable state
	.long	0x4e7a0003
	movl	d0,a0@(T_MMU_CONFIG)
	movc	cacr,d0				// save cache enable state
	movl	d0,a0@(T_MMU_ILL)
//	cpusha	ic,dc				// flush cache
	.word	0xf4f8
	clrl	d0				// disable MMU
//	movc	d0,tc
	.long	0x4e7b0003
	movl	#(CACR_040_IE),d0		// enable I-cache only
	movc	d0,cacr
	lea	1f,a5
	jmp	as_tune
1:
	movc	vbr,a0
	movl	a0@(T_MMU_ILL),d0		// restore cache enable state
	movc	d0,cacr
	movl	a0@(T_MMU_CONFIG),d0		// restore MMU enable state
//	movc	d0,tc
	.long	0x4e7b0003
	movl	a0@,sp				// restore stack pointer
	movl	sp@+,(P_BMAP+BM_RCNTL)
	moveml	sp@+,#0x7fff
	rts

as_tune:
	// must be in local mode
	orl	#BMAP_LO,(P_BMAP+BM_RCNTL)
	
	// AD measurement, setup time
	movl	#(BMAP_TC_AD31+BMAP_SCYC),(P_BMAP+BM_ASEN)
	
	// measure AD31 0 -> 1 -> 0
	movl	#as_one,a0
	movl	#(P_BMAP+BM_STSAMPLE),a1
	nop
//	move16	a0@+,a1@+
	.long	0xf6209000
	nop
	movl	(P_BMAP+BM_TSTATUS0),a_v0	// capture timing status
	movl	(P_BMAP+BM_TSTATUS1),a_v1
#if	AS_MEAS
	movl	a_v1,d0
	fmovel	d0,fp0
	movl	a_v0,d0
	fmovel	d0,fp1
#endif	AS_MEAS
	movl	#TYPE_WINDOW,a_type
	movl	#POL_ONE,d_pol
	movl	#1,d_bitmask
	movl	#1,d_i
	lea	1f,a4
	jmp	scan_timing
1:
	movl	d0,a_ewin
	movl	d1,a_swin

	// measure AD31 1 -> 0 -> 1
	movl	#as_zero,a0
	movl	#(P_BMAP+BM_STSAMPLE+0x80000000),a1
	nop
//	move16	a0@+,a1@+
	.long	0xf6209000
	nop
	movl	(P_BMAP+BM_TSTATUS0),a_v0	// capture timing status
	movl	(P_BMAP+BM_TSTATUS1),a_v1
#if	AS_MEAS
	movl	a_v1,d0
	fmovel	d0,fp2
	movl	a_v0,d0
	fmovel	d0,fp3
#endif	AS_MEAS
	movl	#TYPE_WINDOW,a_type
	movl	#POL_ZERO,d_pol
	movl	#1,d_bitmask
	movl	#1,d_i
	lea	1f,a4
	jmp	scan_timing
1:
	// use smallest window
	cmpl	a_ewin,d0		// pick biggest of two for ewin
	ble	1f
	movl	d0,a_ewin
1:
	cmpl	a_swin,d1		// pick smallest of two for swin
	bge	1f
	movl	d1,a_swin
1:
	movl	a_swin,d_centerndx		// centerndx = (swin + ewin) / 2
	addl	a_ewin,d_centerndx
	lsrl	#1,d_centerndx

	// clock measurement
	movl	#(BMAP_TC_CLK+BMAP_SCYC),(P_BMAP+BM_ASEN)
	movl	#as_zero,a0
	movl	#(P_BMAP+BM_STSAMPLE),a1
	nop
//	move16	a0@+,a1@+
	.long	0xf6209000
	nop
	movl	(P_BMAP+BM_TSTATUS0),a_v0	// capture timing status
	movl	(P_BMAP+BM_TSTATUS1),a_v1
#if	AS_MEAS1
	movl	a_v1,d0
	fmovel	d0,fp0
	movl	a_v0,d0
	fmovel	d0,fp1
#endif	AS_MEAS1
	movl	#TYPE_WINDOW,a_type
	movl	#POL_ONE,d_pol
	movl	#8,d_bitmask		// skip the last rising edge of clock
	movl	#4,d_i
	lea	1f,a4
	jmp	scan_timing
1:
//	subl	#CLK_OFFSET,d1
#if	AS_MEAS1
	fmovel	d1,fp2
	fmovel	d_centerndx,fp3
#endif	AS_MEAS1
	cmpl	d1,d_centerndx
	bge	ast_meas

	// fall into new AS tuning code ...

#define MAX_DELAY	0xb0000000	// shifted max AS delay taps
#define ONE_DELAY	0x08000000	// shifted 1 delay

#define	d_done		d2	// done flag
#define	d_delay		d3	// AS delay counter
#define	d_bit0		d4	// bit[0] of tstatus

	// AST measurement
	movl	#0, d_delay			// clear delay counter
	movl	#0, d_done			// clear done flag
	movl	#(BMAP_TC_AST),(P_BMAP+BM_ASEN)
	// add delay until finding 1
scan1:
	movl	d_delay,(P_BMAP+BM_ASCNTL)	// set strobe delay
	movl	#0,(P_BMAP+BM_STSAMPLE)		// start sample
	movl	(P_BMAP+BM_TSTATUS0),d_bit0	// capture timing status
	andl	#1, d_bit0			// mask off tstatus[31:1]
	tstl	d_bit0				// bit0 = 0?
	bne	scan0				// no, then goto scan0
	cmpl	#MAX_DELAY, d_delay		// yes, then reach max delay 
						//    and cannot find 1 yet?
	beq	done				// yes, then done
	addl	#ONE_DELAY, d_delay		// no, then add 1 delay
	bra	scan1

	// subtract delay until finding 0
scan0:
	tstl	d_delay				// reach min. delay?	
	beq	done				// yes, then done
	subl	#ONE_DELAY, d_delay		// no, then subtract 1 delay
	movl	d_delay,(P_BMAP+BM_ASCNTL)	// set strobe delay
	movl	#0,(P_BMAP+BM_STSAMPLE)		// start sample
	movl	(P_BMAP+BM_TSTATUS0),d_bit0	// capture timing status
	andl	#1, d_bit0			// mask off tstatus[31:1]
	tstl	d_bit0				// bit0 = 0?
	bne	scan0				// no, then keep subtracting
done:
	tstl	d_done				// yes, then it's done
	bne	1f				// done	with AS?
	movl	#1, d_done			// no, then set done flag

	// do AS measurement
	movl	#(BMAP_ASEN+BMAP_TC_AS),(P_BMAP+BM_ASEN)
	bra scan1

1:
	// park in AST to save power
	movl	#(BMAP_ASEN+BMAP_TC_AST),(P_BMAP+BM_ASEN)
	addl	#ONE_DELAY,d_delay		// add 1 delay to final result
	movl	d_delay,(P_BMAP+BM_ASCNTL)	// set strobe delay
	jmp	a5@

ast_meas:
	// AST measurement
	movl	#(BMAP_TC_AST+BMAP_SCYC),(P_BMAP+BM_ASEN)
	movl	#0,(P_BMAP+BM_ASCNTL)		// min strobe delay
	movl	#as_zero,a0
	movl	#(P_BMAP+BM_STSAMPLE),a1
	nop
//	move16	a0@+,a1@+
	.long	0xf6209000
	nop
	movl	(P_BMAP+BM_TSTATUS0),a_v0	// capture timing status
	movl	(P_BMAP+BM_TSTATUS1),a_v1
#if	AS_MEAS
	movl	a_v1,d0
	fmovel	d0,fp4
	movl	a_v0,d0
	fmovel	d0,fp5
#endif	AS_MEAS
	movl	#POL_ONE,d_pol
	movl	#TYPE_TRANS,a_type
	movl	#1,d_bitmask
	movl	#1,d_i
	lea	1f,a4
	jmp	scan_timing
1:
#if	AS_MEAS1
	fmovel	d0,fp4
#endif	AS_MEAS1
	clrl	d_testndx			// testndx = asfall - centerndx
	cmpl	d0,d_centerndx
	bge	1f
	subl	d_centerndx,d0
	movl	d0,d_testndx
1:
	movl	d_testndx,d0
#if	AS_MEAS1
	fmovel	d_testndx,fp5
#endif	AS_MEAS1
	rorl	#5,d0
	movl	d0,(P_BMAP+BM_ASCNTL)		// set strobe delay

	// AS measurement
	movl	#(BMAP_ASEN+BMAP_TC_AS+BMAP_SCYC),(P_BMAP+BM_ASEN)
	movl	#as_zero,a0
	movl	#(P_BMAP+BM_STSAMPLE),a1
	nop
//	move16	a0@+,a1@+
	.long	0xf6209000
	nop
	movl	(P_BMAP+BM_TSTATUS0),a_v0	// capture timing status
	movl	(P_BMAP+BM_TSTATUS1),a_v1
#if	AS_MEAS
	movl	a_v1,d0
	fmovel	d0,fp6
	movl	a_v0,d0
	fmovel	d0,fp7
#endif	AS_MEAS
	movl	#POL_ONE,d_pol
	movl	#TYPE_TRANS,a_type
	movl	#1,d_bitmask
	movl	#1,d_i
	lea	1f,a4
	jmp	scan_timing
1:
#if	AS_MEAS1
	fmovel	d0,fp6
#endif	AS_MEAS1
	movl	d0,d1				// testndx += asfall - centerndx
	subl	d_centerndx,d1
	addl	d1,d_testndx
	bge	1f
	clrl	d_testndx
1:
	rorl	#5,d_testndx			// place in upper 5 bits
#if	AS_MEAS1
	fmovel	d_testndx,fp7
#endif	AS_MEAS1
#if	AS_MEAS || AS_MEAS1
	movl	#0x00000000,d_testndx
#endif	AS_MEAS || AS_MEAS1
	addl	#ONE_DELAY,d_testndx		// add 1 delay to final result
	movl	d_testndx,(P_BMAP+BM_ASCNTL)	// set strobe delay

	// park in AST to save power
	movl	#(BMAP_ASEN+BMAP_TC_AST+BMAP_SCYC),(P_BMAP+BM_ASEN)	
	jmp	a5@

	.long	0xffffffff
	.long	0xffffffff
	.long	0xffffffff
	.long	0xffffffff
as_one:
	.long	0xffffffff
	.long	0xffffffff
	.long	0xffffffff
	.long	0xffffffff

	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
as_zero:
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	
#if	DBUG_DMA
	.data
	.long	0x74562934
	.long	0x75927444
	.long	0x00484004
	.long	0x87346332
	.long	0x09238787
	.long	0x23778211
	.long	0x11174726
	.long	0x98237857
pattern:
	.long	0x12345678
	.long	0xabcdef01
	.long	0x17263415
	.long	0xab1635df
	.long	0xab716258
	.long	0x71928abe
	.long	0x81726355
	.long	0x12121244
	.text

	.globl	_mtest	
_mtest:
	movl	#pattern,d0 
	andl	#0xfffffff0,d0
	movl	d0,a0
	movl	#0x04000000,a1
#if 0
	nop
//	move16	a0@+,a1@+
	.long	0xf6209000
#else
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+
#endif
	
	movl	#0x04000100,a0
	clrl	a0@+
	clrl	a0@+
	clrl	a0@+
	clrl	a0@+
	clrl	a0@+
	clrl	a0@+
	clrl	a0@+
	clrl	a0@+
	
	movl	#0x04000100,d1
	movl	d1,a1
	movl	#0x04000000,a0
	nop
//	move16	a0@+,a1@+
	.long	0xf6209000
	movl	#pattern,d0
	andl	#0xfffffff0,d0
	addl	#0xc,d0
	movl	d0,a0
	movl	#0x04000100,d1
	addl	#0xc,d1
	movl	d1,a1
	movl	a0@,d0
	cmpl	a1@,d0
	beq	ok
	movl	a0@,0x04000000
	movl	a1@,0x04000004
	movl	a0,0x04000008
	movl	a1,0x0400000c
	movl	#0,d0
	rts
ok:	movl	#1,d0
	rts
#endif	DBUG_DMA

