/*
 * Copyright (c) 1987 NeXT, INC
 */

#define	ASSEMBLER

#include "next/cframe.h"

/* abs - int absolute value */

PROCENTRY(abs)
	movl	a_p0,d0
	bpl	1f
	negl	d0
1:
	rts

#define	BCMP_THRESH	64

PROCENTRY(bcmp)
	movl	a_p0,a0
	movl	a_p1,a1
	movl	a_p2,d0
	movl	d2,sp@-
	movl	d0,d2
	cmpl	#BCMP_THRESH,d2
	ble	cklng1		| not worth aligning
	movl	a0,d0
	movl	a1,d1
	eorl	d0,d1
	andl	#3,d1
	bne	cklng1		| cant align
	subl	d0,d1
	andl	#3,d1		| bytes til aligned
	beq	cklng1		| already aligned
	subl	d1,d2		| decr byte count
	subql	#1,d1		| compensate for top entry of dbcc loop
1:	cmpmb	a0@+,a1@+
	dbne	d1,1b
	bne	fail		| last compare failed

cklng1:	movl	d2,d0
	lsrl	#2,d0		| longwords to compare
	beq	cmpb		| byte count less than longword
	subql	#1,d0		| compensate for first pass
	cmpl	#65535,d0
	ble	cmpl		| in range of dbcc loop
	movl	#65535,d0	| knock off 65536 longs
1:	cmpml	a0@+,a1@+
	dbne	d0,1b
	bne	fail		| last compare failed
	subl	#65536*4,d2	| decr byte count appropriately
	bra	cklng1		| see if now in dbcc range

cmpl:	cmpml	a0@+,a1@+
	dbne	d0,cmpl
	bne	fail
	andl	#3,d2		| must be less than a longword to go
	moveq	#0,d0		| preset condition codes
	bra	cmpb

1:	cmpmb	a0@+,a1@+
cmpb:	dbne	d2,1b
	bne	fail
	movl	sp@+,d2
	rts

fail:	moveq	#1,d0
	movl	sp@+,d2
	rts

/* bcopy(s1, s2, n) */

#define	BCOPY_THRESH	64

PROCENTRY(bcopy)
	movl	a_p0,a0
	movl	a_p1,a1
	movl	a_p2,d0
	movl	d2,sp@-
	movl	d0,d2
	cmpl	a0,a1
	bhi	rcopy		| dst > src copy high to low
	beq	done		| src == dst
#if	MC68040
	subql	#1,d2
	bra	copy_bytes
#endif	MC68040
	cmpl	#BCOPY_THRESH,d2
	ble	cklng2		| not worth aligning
	movl	a0,d0
	movl	a1,d1
	eorl	d0,d1
	andl	#3,d1
	bne	cklng2		| cant align
	subl	d0,d1
	andl	#3,d1		| bytes til aligned
	beq	cklng2		| already aligned
	subl	d1,d2		| decr byte count
	subql	#1,d1		| compensate for top entry of dbcc loop
1:	movb	a0@+,a1@+
	dbra	d1,1b

cklng2:	movl	d2,d0
	lsrl	#2,d0		| longwords to compare
	beq	copyb		| byte count less than longword
	subql	#1,d0		| compensate for first pass
	cmpl	#65535,d0
	ble	copyl		| in range of dbcc loop
	movl	#65535,d0	| knock off 65536 longs
1:	movl	a0@+,a1@+
	dbra	d0,1b
	subl	#65536*4,d2	| decr byte count appropriately
	bra	cklng2		| see if now in dbcc range

copyl:	movl	a0@+,a1@+
	dbra	d0,copyl
	andl	#3,d2		| must be less than a longword to go
	bra	copyb

copy_bytes:
1:	movb	a0@+,a1@+
copyb:	dbra	d2,1b
done:	movl	sp@+,d2
	rts

rcopy:	addl	d2,a0		| offset to end of src
	addl	d2,a1		| offset to end of dst
#if	MC68040
	subql	#1,d2
	bra	rcopy_bytes
#endif	MC68040
	cmpl	#BCOPY_THRESH,d2
	ble	rcklng2		| not worth aligning
	movl	a0,d0
	movl	a1,d1
	eorl	d0,d1
	andl	#3,d1
	bne	rcklng2		| cant align
	andl	#3,d0		| bytes til aligned
	beq	rcklng2		| already aligned
	subl	d0,d2		| decr byte count
	subql	#1,d0		| compensate for top entry of dbcc loop
1:	movb	a0@-,a1@-
	dbra	d0,1b

rcklng2:movl	d2,d0
	lsrl	#2,d0		| longwords to compare
	beq	rcopyb		| byte count less than longword
	subql	#1,d0		| compensate for first pass
	cmpl	#65535,d0
	ble	rcopyl		| in range of dbcc loop
	movl	#65535,d0	| knock off 65536 longs
1:	movl	a0@-,a1@-
	dbra	d0,1b
	subl	#65536*4,d2	| decr byte count appropriately
	bra	rcopyl		| see if now in dbcc range

rcopyl:	movl	a0@-,a1@-
	dbra	d0,rcopyl
	andl	#3,d2		| must be less than a longword to go
	bra	rcopyb

rcopy_bytes:
1:	movb	a0@-,a1@-
rcopyb:	dbra	d2,1b
	movl	sp@+,d2
	rts

/* bzero(s1, n) */

#define	BZERO_THRESH	32

PROCENTRY(bzero)
	movl	a_p0,a0
	movl	a_p1,d0
	movl	d2,sp@-
	movl	d0,d2
	cmpl	#BZERO_THRESH,d2
	ble	cklng3		| not worth aligning
	movl	a0,d0
	moveq	#0,d1
	subl	d0,d1
	andl	#3,d1		| bytes til aligned
	beq	cklng3		| already aligned
	subl	d1,d2		| decr byte count
	subql	#1,d1		| compensate for top entry of dbcc loop
1:	clrb	a0@+
	dbra	d1,1b

cklng3:	movl	d2,d0
	lsrl	#2,d0		| longwords to compare
	beq	clrb		| byte count less than longword
	subql	#1,d0		| compensate for first pass
	cmpl	#65535,d0
	ble	clrl		| in range of dbcc loop
	movl	#65535,d0	| knock off 65536 longs
1:	clrl	a0@+
	dbra	d0,1b
	subl	#65536*4,d2	| decr byte count appropriately
	bra	cklng3		| see if now in dbcc range

clrl:	clrl	a0@+
	dbra	d0,clrl
	andl	#3,d2		| must be less than a longword to go
	bra	clrb

1:	clrb	a0@+
clrb:	dbra	d2,1b
	movl	sp@+,d2
	rts

/*
 * Find the first occurence of c in the string cp.
 * Return pointer to match or null pointer.
 *
 * char *
 * index(cp, c)
 *	char *cp, c;
 */

PROCENTRY(index)
	movl	a_p0,a0		| cp
	movl	a_p1,d0		| c
	beq	3f		| c == 0 special cased
1:	movb	a0@+,d1		| *cp++
	beq	2f		| end of string
	cmpb	d1,d0
	bne	1b		| not c
	subw	#1,a0		| undo post-increment
	movl	a0,d0
	rts

2:	moveq	#0,d0		| didnt find c
	rts

3:	tstb	a0@+
	bne	3b
	subw	#1,a0		| undo post-increment
	movl	a0,d0
	rts

/*
 * Copy string s2 over top of string s1.
 * Truncate or null-pad to n bytes.
 *
 * char *
 * strncpy(s1, s2, n)
 *	char *s1, *s2;
 */

PROCENTRY(strncpy)
	movl	a_p0,a0		| dst
	movl	a_p1,a1		| src
	movl	a_p2,d1		| n
	movl	a0,d0
1:	subql	#1,d1
	blt	4f		| n exhausted
	movb	a1@+,a0@+
	bne	1b		| more string to move
	bra	3f		| clear to null until n exhausted

2:	clrb	a0@+
3:	subql	#1,d1
	bge	2b		| n not exhausted
4:	rts

/*
 * Concatenate string s2 to the end of s1
 * and return the base of s1.
 *
 * char *
 * strcat(s1, s2)
 *	char *s1, *s2;
 */

PROCENTRY(strcat)
	movl	a_p0,a0		| s1
	movl	a_p1,a1		| s2
	movl	a0,d0
1:	tstb	a0@+
	bne	1b
	subqw	#1,a0
2:	movb	a1@+,a0@+
	bne	2b
	rts

/*
 * Compare string s1 lexicographically to string s2.
 * Return:
 *	0	s1 == s2
 *	> 0	s1 > s2
 *	< 0	s2 < s2
 *
 * strcmp(s1, s2)
 *	char *s1, *s2;
 */

PROCENTRY(strcmp)
	movl	a_p0,a0
	movl	a_p1,a1
1:	movb	a0@,d0
	cmpb	a1@+,d0
	beq	3f
	extbl	d0
	movb	a1@-,d1
	extbl	d1
	subl	d1,d0
	rts

3:	tstb	a0@+
	bne	1b
	moveq	#0,d0
	rts

/*
 * Copy string s2 over top of s1.
 * Return base of s1.
 *
 * char *
 * strcpy(s1, s2)
 *	char *s1, *s2;
 */

PROCENTRY(strcpy)
	movl	a_p0,a0
	movl	a_p1,a1
	movl	a0,d0
1:	movb	a1@+,a0@+
	bne	1b
	rts

/*
 * Compare at most n characters of string
 * s1 lexicographically to string s2.
 * Return:
 *	0	s1 == s2
 *	> 0	s1 > s2
 *	< 0	s2 < s2
 *
 * strncmp(s1, s2, n)
 *	char *s1, *s2;
 *	int n;
 */

PROCENTRY(strncmp)
	movl	a_p0,a0
	movl	a_p1,a1
	movl	a_p2,d1
1:	subql	#1,d1
	blt	2f
	movb	a0@+,d0
	cmpb	a1@+,d0
	bne	3f
	tstb	d0
	bne	1b
2:	moveq	#0,d0
	rts

3:	extbl	d0
	movb	a1@-,d1
	extbl	d1
	subl	d1,d0
	rts

/*
 * Return the length of cp (not counting '\0').
 *
 * strlen(cp)
 *	char *cp;
 */

PROCENTRY(strlen)
	movl	a_p0,a0
	moveq	#-1,d0
1:	addql	#1,d0
	tstb	a0@+
	bne	1b
	rts

