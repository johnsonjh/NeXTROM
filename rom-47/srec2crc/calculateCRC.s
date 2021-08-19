	.globl	calculateCRC
	.even

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

	crc_poly = 0x04c11db7

calculateCRC:	
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
