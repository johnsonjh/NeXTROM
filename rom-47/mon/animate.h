/*	@(#)animate.h	1.0	9/26/88	(c) 1988 NeXT	*/

#ifndef	_ANIMATE_
#define	_ANIMATE_

/* display geometry */
#define	LOGIN_X		340
#define	LOGIN_Y		308
#define	TEXT_X		550
#define	TEXT_Y2		390
#define	TEXT_Y3		380
#define	TEST_X		640
#define	TEST_Y		353

#ifndef	ASSEMBLER
struct	animation {
	short	x, y;
	char	*icon;
	char	next;
	char	anim_time;
};
#endif	ASSEMBLER

#endif	_ANIMATE_
