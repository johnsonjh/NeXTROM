// @(#)POTUtils.s	1.0	08/15/88	(c) 1988 NeXT Inc.	 */
// assembly language utilities used by the Boot ROM Power On Test

// This define tells the cpu.h to only include items relevent to assembler...
#define	ASSEMBLER	1

#include <pot/POT.h>
#include <next/cpu.h>
#include <next/mmu.h>
#define	sid	a5

#if 1
// u_int delay(u_int us_count)
	.globl _delay
// delay the number of count in micro seconds
// FIXME: should adjust timing based on CPU clock!  Better yet: use event ctr
_delay:
	subql  #3, sp@(4)       // see if the wait request is small
	ble    1f		// if too small, we should return now
				// otherwise the subtraction compensates
				// for overhead

	movc  cacr, a0  	// saves old cacr
	movel a0, d0		// make a copy
#ifdef	MC68030
	oril  #CACR_EI, d0	// enable I cache
#endif	MC68030
#ifdef	MC68040
	oril	#CACR_040_IE,d0
#endif	MC68040
	movc  d0, cacr

// note: these calculations are only valid for a 25 MHz	clock!
// it takes 6 cycles per dbra 
// --> 0.24 us per dbra for 25MHz CPU
// --> 4.16667 dbra per us
// --> 273066 dbra per 256*256 dbra

	movel sp@(4),d0         // get micro second count
#if	MC68030
	mulul #273066,d1:d0     // now d1 low word has dbra count / 2^16
	                        // and d0 high word has the remainder
#endif	MC68030
#if	MC68040
	mulul #0x64000,d1:d0     // now d1 low word has dbra count / 2^16
	                        // and d0 high word has the remainder
#endif	MC68040
	swap  d0		// swap it to the right position
	
0:	dbra  d0, 0b	
        dbra  d1, 0b		// uses d1 as the outer counter

	movc  a0, cacr		// restore old cacr	
1:	rts
#endif

// u_int set_cacr(u_int cacr)
	.globl _set_cacr
_set_cacr:
#ifdef	MC68040
//	cinva	ic, dc
	.word	0xf4d8
#endif	MC68040
	movc cacr,d0		// get old cacr
	movel sp@(4),a0         // get new cacr
	movc a0,cacr		// store the new one
	rts

	.globl _clear_d_cache
_clear_d_cache:
	movc cacr,d0		// get old cacr
#ifdef	MC68030
	bset #11,d0		// set clear data cache bit
	movc d0,cacr		// do it
#endif	MC68030
#ifdef	MC68040
//	cpusha	dc
	.word	0xf478
#endif	MC68040
	rts


	.globl _led_On		// C routine that turns the led on...
_led_On:
	movel   sp@(4),a0         // get slot offset
	orl	#1,a0@(P_SCR2)
	rts
	
	.globl _led_Off		// ... and turns it off...
_led_Off:
	movel   sp@(4),a0         // get slot offset
	andl	#~1,a0@(P_SCR2)
	rts

	.globl _blinkLed	// blinks LED forever at the specified count
_blinkLed:
	movl	sp@(4),a0	// long on stack sets count
	
	.globl blinkLed		// blink "count" in a0
blinkLed:
0:	movl	a0,d1
	subw	#1,d1
1:	movl	d1,a1
	orl	#1,P_SCR2		// LED on
	movw	#0x8,d0		// wait 250 msec
2:	movw	#~1,d1		// initialize loop count for 2^16
3:	dbeq	d1,3b		// wait till count = 0.
	dbeq	d0,2b
	
	andl	#~1,P_SCR2		// LED off
	movw	#0x8,d0		// wait 250 msec
4:	movw	#~1,d1		// initialize loop count for 2^16
5:	dbeq	d1,5b		// wait till count = 0.
	dbeq	d0,4b
	movl	a1,d1
	dbeq	d1,1b		// do blink count
	
	movw	#0x40,d0	// wait 2 sec
6:	movw	#~1,d1		// initialize loop count for 2^16
7:	dbeq	d1,7b		// wait till count = 0.
	dbeq	d0,6b
	jmp	0b		// loop forever....
	
///////////////////////////////////////////////////////////////////////////////
//
//	Stolen from dmitch (Doug Mitchell) at NeXT.
//
//	This routine clears the OVR bit in the sound out CSR by waiting 
//	for a DMA packet to be sent and clearing OVR immediately afterwards.
//	Straightforeward and/or doesn't work because of DMA chip timing...
//
//	Passed: Nothing
//	returns: Nothing
//
	.globl	_clr_so_urun
_clr_so_urun:
	movw	sr, d1			// disable interrupts
	orw	#0x700, sr
	movl	#P_MON,a0
	btst	#7,a0@
	beq	csu_doit		// if dma out is off then just 
					// do the bit set
	lea	a0@(2),a1		// rtx/ctx/dtx register
	
	// wait for 1 DMA packet to be sent (but don't wait forever...)
	
cdma_wt:
	movew	#100, d0		// only wait for a while
cdw_1:
	btst	#6, a1@			// wait till dma tx in progress
	dbne	d0, cdw_1
cdw_2:	
	btst	#6, a1@			// wait till dma tx not in progress
	bne	cdw_2

csu_doit:
	bset	#5,a0@			// CLEAR UNDERRUN
	movew	d1, sr			// restore interrupt level
	rts

// end of clr_so_urun



// Generates 64kB of a FM chirp, running from 1kHz to 17kHz. 64k bytes
// is 16k samples (2 bytes/channel, left+right channel = 4 bytes/sample),
// so duration of chirp is 16k sampples/44kHz sample rate ~ 1/3 second.
// Using a small part of the buffer generates intersting sounds; the
// first 8k of data sounds like a water drop...

	.globl	_SineForSoundOut
_SineForSoundOut:
	link	a6,#0			// could be in C, but check 68882
	moveml	#0x7ffe,sp@-
	fmovemx	#0xff,sp@-
	movl	a6@(8),a1		// get adr of buffer
	
	movw	#0x3fff,d0		// make 16k samples = 64kB

//
//  FIXME: for some reason, the chirp code doesn't work anymore on an 030!
//
#if	0	
	fmoveb	#0,fp0			// f0 = 0 = theta
	
	fmovecrx #0x00,fp1		// f1 = pi = freq
	fscales	#10,fp1			// f1 = 1024 Hz; need radians/buffer
	fscales	#-15,fp1		// f1 = 1024*2pi/64k = 1024*pi/32k
	
	fmovex	fp1,fp2			// f2 = delta freq
	fscales	#-12,fp2		//    = 16kHz increase/buffer
	
1:	fsinx	fp0,fp3			// f3 = sin theta
	faddx	fp1,fp0			// update theta
	faddx	fp2,fp1			// update freq
	fscales	#11,fp3			// scale sine to 12 bits; loud 'nuff?
	fmovew	fp3,a1@			// store left channel
#endif	MC68030
#if	1
1:	movl	a1,d1			// no fsin on 040, use sawtooth source
	andl	#0xff,d1
	asll	#4,d1	
	movw	d1,a1@			// store left channel
#endif	MC68040
	movw	a1@,a1@(2)		// store right channel
	addl	#4,a1
	dbra	d0,1b			// done?
	
	fmovemx	sp@+,#0xff
	moveml	sp@+,#0x7ffe
	unlk	a6
	rts
