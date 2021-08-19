//////////////////////////////////////////////////////////////////////////////
// 									    //
//			      F P U _ U T I L S . S  			    //
// 									    //
//////////////////////////////////////////////////////////////////////////////
//
//	Revision History
//	-----------------
//
//    Date     Initials      Change
//  --------   --------     -----------------
//  06/21/89	blaw	
//	Port over for ROM selftest
//	Shortened the loops for speed
//  02/07/89	blaw	
//	Save scratch registers in all routines
//	Fixed bug in fpu_reg: "andl" --> "andl #"
//	Commented out fsave & frestore operations
//	Added comment block to all routines
//	Changed __fpu_move to use the stack instead of local variable
//	..so that cache will work and subroutine is reentrant.
//	Decreased the loop count from 0xffff to 0x1000 in some tests.
//  01/18/89     JVW	    Creation
//

	.text
	.globl	__fpu_reg
	.globl	__fpu_mov
	.globl	__fpu_ops
	
__fpu_reg:
//
// This routine test all 8 FPU registers with various data patterns.
// Return 0 iff no error found
//
// Note that FPU register are 80 bits long.
//
	movml	d1-d7/a0-a5,sp@-	//Save scratch registers

	moveq	#0,d0		//Initialize return status register d0
	moveq	#0,d1		//Marching 0's
	bsrs	0f		//Go check'em	
	moveq	#-1,d1		//Marching 1's
	bsrs	0f		//Go check'em	
	moveq	#1,d1		//Marching 1
	bsrs	0f		//Go check'em	
	moveq	#1,d1		//Marching 0
	negl	d1
	bsrs	0f		//Go check'em	
	movl	#0x55555555,d1	//Marching 010101010101....
	bsrs	0f		//Go check'em	
	movl	#0x99999999,d1	//Marching 100110011001...
	bsrs	0f		//Go check'em	
	movl	#0x88888888,d1	//Marching 100010001000...
	bsrs	0f		//Go check'em	
	movl	#0x77777777,d1	//Marching 011101110111...
	bsrs	0f		//Go check'em	
	
	movml	sp@+,d1-d7/a0-a5	//Restore scratch registers
	rts
	
0:
	movl	d1,d2		//Copy d1 to d2 and d3
	movl	d1,d3
	movw	#256-1,d4	//# rotations to do
1:
	moveq	#8-1,d5		//Eight sets of registers to push
2:
	movel	d1,sp@-		//Push FPU register image
	movel	d2,sp@-
	movel	d3,sp@-
	dbf	d5,2b
	
	fmovemx	sp@+,fp0-fp7	//Pop to all FPU registers
	fmovemx	fp0-fp7,sp@-	//Push from all FPU registers
	moveq	#8-1,d5		//Eight sets of registers to pop and compare
2:
	movel	sp@+,d6		//Compare d[1-3] with each fp[0-7]
	eorl	d3,d6		//Special masked compare for MSBs
	andl	#0xffff0000,d6	
	sne	d6		//Set d6 on error
	orb	d6,d0		//Accumulate
	cmpl	sp@+,d2
	sne	d6
	orb	d6,d0
	cmpl	sp@+,d1
	sne	d6
	orb	d6,d0
	dbf	d5,2b
	
	lsll	#1,d1		//Left shift d1
	roxll	#1,d2		//Left shift X into d2
	roxll	#1,d3		//Left shift X into d3
	bccs	4f
	orl	#1,d1		//Set d1's LSB to match d3's orginal MSB	
4:
	dbf	d4,1b
	
	rts
	
__fpu_mov:
//
// This routine test the FPU by moving data back and forth in
// different format.
//
// Returns 0 iff no error
//
	movml	d1-d7/a0-a5,sp@-	//Save scratch registers

	moveq	#0,d0			//Initialize return status register d0
	movel	#0x8000,d3		//Get biggest 16-bit number
	movew   #0x400-1,d2		//Load loop counter
0:
	fmovel d3,fp1			//Get the number
	fmovel fp1,sp@-			//Convert to long
	fmovel sp@+,fp1
	fmoves fp1,sp@-			//Convert to single
	fmoves sp@+,fp1
	fmoved fp1,sp@-			//Convert to double
	fmoved sp@+,fp1
	fmovex fp1,sp@-			//Convert to extended
	fmovex sp@+,fp1
#if	MC68030
	fmovep fp1,sp@-{#0}		//Convert to packed
	fmovep sp@+,fp1
#endif	MC68030
	fmovel fp1,d4			//Convert back to long
	cmpl	d4,d3
	sne	d1
	orb	d1,d0
	addl	#1,d3
	dbra	d2,0b
	
	movml	sp@+,d1-d7/a0-a5	//Restore scratch registers
	rts
	
__fpu_ops:
//
// This routine test some basic (+ - * / sqrt) arithemetic functions
// of the FPU.
//
// Returns 0 iff no error.
//
	movml	d1-d7/a0-a5,sp@-	//Save scratch registers

	moveq	#0,d0			//Initialize return status register d0
	fmovel	#0x8000,fp0		//Get biggest 16-bit number
	movew #0x400-1,d2		//Load loop counter
0:
	fmovex fp0,fp1
	fadds d2,fp1
	faddb #5,fp1
	faddw #7,fp1
	faddl #9,fp1
//NOTE: can not execute fsave & frestore in user mode
	fnop				//Make sure it gets done
	fsave sp@-			//Check idle state save/restore
	frestore sp@+
	fsubl #9,fp1
	fsubw #7,fp1
	fsubb #5,fp1
	fsubs d2,fp1
	fmovex fp1,fp2
	fmulx fp2,fp2
	fdivx fp1,fp2
	fmulx fp2,fp2
	fsqrtx fp2
	fcmpx fp0,fp2
	nop
	fnop
	fsne	d1
	orb	d1,d0
	faddb #1,fp0
	dbra	d2,0b
	
	movml	sp@+,d1-d7/a0-a5	//Restore scratch registers
	rts
	

