// @(#)MemoryTest.s	1.0	08/15/88	(c) 1988 NeXT Inc.	 */

//
// Change History:
// 89.06.10 blaw	Remove main Memory Tests since new tests are written
//


// assembly language routines used to perform the Boot ROM Power On Test

// This define tells the cpu.h to only include items relevent to assembler...
#define	ASSEMBLER	1

// Slot ID is contained in a5.  On board memory is slot, relative...

#define	sid	a5
#define	RTC_WRITE 0x80

#include <pot/POT.h>
#include <next/cpu.h>
#include <next/mmu.h>
#include <next/scr.h>
#include <nextdev/video.h>
#include <mon/monparam.h>
#include <mon/nvram.h>
#include <mon/animate.h>
#include <assym.h>

// ConstantRAMTest.  Check RAM that a2 points to with constant in d1 till 
// post incremented scan pointer == a3. d0 contains the test result: 0 if 
// no problems, address of the bad location otherwise. a4 is the return
// address since we can't use the stack...
//
// uses:	 a1
//
// input:	a2		starting adr
//		a3		ending adr
//		a4		return adr
//
// output:	d0		result

ConstantRAMTest:
	clrl	d0		// assume test will pass
	movl	a2,a1		// a1 is the scan ptr
3:
	movl	d1,a1@+		// load memory
	cmpl	a1,a3
	bnes	3b

	movl	a2,a1		// check memory
4:	cmpl	a1@+,d1
	bnes	6f		// does the data match? it should	
	cmpl	a1,a3
	bnes	4b		// has all of memory been checked?
	bra	ConstantRAMTestExit	// all of memory passed
6:
	movl	a1,d0
	subql	#4,d0		// return address of first bad cell
	
ConstantRAMTestExit:
	jmp	a4@


// VideoRamTest.  If an error is found, the long address of the
// bad location is returned.  If everything is fine, 0 is returned.
// Return address on stack for C calls, no stack entry is used at
// power up and a0 must contain return address. 
//
// Since the hardware bus errors aren't supported, there is NOT a handler.
//
// Uses:	d1-d2,d6-d7/a1-a4
//
// input:	a0		return adr
//
// output:	d0		result

	.globl _VideoRAMTest
	.globl NoStackVideoRAMTest

_VideoRAMTest:
	link	a6,#0
	moveml	#0x7ffe,sp@-
	lea	VideoRAMTestExit,a7
NoStackVideoRAMTest:
	movl	sid@(P_SCR2),d1
	orl	#1,d1
	movl	d1,sid@(P_SCR2)	// turn on LED
	movc	cacr,d2		// turn on i-cache for max speed
#ifdef	MC68030
	movl	#CACR_EI+CACR_IBE,d0
#endif	MC68030
#ifdef	MC68040
//	cpusha	ic,dc
	.word	0xf4f8
	movl	#CACR_040_IE,d0
#endif	MC68040
	movc	d0,cacr

// Load the pointer to start and end of Video RAM. 

	lea	sid@(P_VIDEOMEM),a2
// FIXME: screen size (P_VIDEOSIZE) is only 3a800, VRAM is 40000 long
 	lea	a2@(0x40000),a3
	movl	#0xaaaaaaaa,d1
	lea	1f,a4
	bra	ConstantRAMTest
1:	tstl	d0
	bne	NoStackVideoRAMTestExit

	movl	#0x5555555,d1
	lea	2f,a4
	bra	ConstantRAMTest
2:	tstl	d0
	bne	NoStackVideoRAMTestExit
	clrl	d1
	movl	a2,a1		// a1 is the scan ptr
3:	movl	d1,a1@+		// load memory
	addql	#1,d1
	cmpl	a1,a3
	bnes	3b
	clrl	d1
	movl	a2,a1		// check upper bits of AD bus
4:	cmpl	a1@+,d1
	bnes	5f		// does the data match? it should	
 	addql	#1,d1
	cmpl	a1,a3
	bnes	4b		// has all of memory been checked?
	bras	NoStackVideoRAMTestExit
5:	movl	a1,d0

NoStackVideoRAMTestExit:
	movc	d2,cacr		// restore the cache control register
	movw	#BLINK_VRAM,a0	// set the blink count for blinkLed
	tstl	d0
#if	NO_VTEST
#else	NO_VTEST
	bne	blinkLed	// test failed blink LED
#endif	NO_VTEST
	movl	sid@(P_SCR2),d1
	andl	#0xfffffffe,d1
	movl	d1,sid@(P_SCR2)	// test passed turn off LED
	jmp	a7@		// all done, result code is in d0
VideoRAMTestExit:
	moveml	sp@+,#0x7ffe
	unlk	a6
	rts
	
	.globl	calculateCRC

//-----------------------------------------------------------------------
//	Method:		see "Error-Correcting Codes" W.W. Peterson and 
//			E.J. Weldon, Jr. 2nd Ed. MIT Press ) 1972, (printed 1986)
//			especially chapters 7 (pp 170-178) and 8 (pp 224-228).
//			also see std IEEE 802.3-1985 sec 3.2.8
//
//	Input:		a1 points to first byte
//			d3 contains the number of bytes
//			a0 is return adr
//
//	Output:		d0 contains the 32 bit, mod 2, remainder of input div
//			crc_poly. Result is ones complemented and bit
//			reversed ala 802.3 std.
//
//	Uses:		a1/d0-d4
//----------------------------------------------------------------------

#define	crc_poly	0x04c11db7

calculateCRC:
	movl	sid@(P_SCR2),d1
	orl	#1,d1
	movl	d1,sid@(P_SCR2)	// turn on LED
	
	movl	#-1,d0		// same as complementing first 32 bits of data
1:	movb	a1@+,d1		// get next byte
	moveq	#7,d2		// ls bit of byte is sent first
2:	lsll	#1,d0		// carry bit = next d4 bit = CRC ms bit
	roxrb	#1,d4		// ms bit of quotient = ms bit of d0 
	rorb	#1,d1		// ms bit of dataByte = next data bit
	eorb	d1,d4		// ms bit of quotient = "carry" = next quotient bit
	bpls	3f		// if carry bit then subtract poly from remainder
	eorl	#crc_poly,d0	// mod 2 subtract = xor = mod 2 add
3:	dbra	d2,2b		// processed all 8 bits?
	subql	#1,d3
	bne	1b		// processed all bytes?

	notl	d0 		// this is almost the CRC
	movw	#31,d2		// 802.3 CRC uses reverse bit order	
5:	roxll	#1,d0
	roxrl	#1,d1
	dbra	d2,5b
	movl	d1,d0		// have the 802.3 CRC now
	movl	sid@(P_SCR2),d1
	andl	#0xfffffffe,d1
	movl	d1,sid@(P_SCR2)	// test passed turn off LED
	jmp	a0@
	
	.globl	_calculateCRC
_calculateCRC:			// C interface to register only code
	link	a6,#0
	moveml	#0x7ffe,sp@-
	movl	a6@(8),a1
	movl	a6@(12),d3
	lea	1f,a0
	bra     calculateCRC
1:	moveml	sp@+,#0x7ffe
	unlk	a6
	rts	
