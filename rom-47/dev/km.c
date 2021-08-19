/*	@(#)km.c	1.0	10/12/87	(c) 1987 NeXT	*/

/* 
 **********************************************************************
 * HISTORY
 * 09-Apr-90  Doug Mitchell at NeXT
 *	Added floppy eject on power off.
 *
 *  4-Apr-90  Mike Paquette (mpaque) at NeXT
 *	Revised for generic frame buffer support. Added the p-code
 *	interpreter and bus probing code.
 *
 * 12-Oct-87  John Seamons (jks) at NeXT
 *	Created.
 *
 **********************************************************************
 */ 
#include "sys/types.h"
#include "sys/param.h"
#include "next/cpu.h"
#include "next/scr.h"
#include "next/bmap.h"
#include "next/clock.h"
#include "nextdev/monreg.h"
#include "nextdev/video.h"
#include "nextdev/keycodes.h"
#include "nextdev/ohlfs12.h"
#include "nextdev/td.h"
#include "mon/global.h"
#include "mon/monparam.h"

#define	TAB_SIZE	8
#define	NC_W_BIG	120
#define	NC_H_BIG	60
#define	X_OVHD		3		/* = 2 for font8x10 */
#define	Y_OVHD		26
#define	Y_BOT		2
#define	TITLEBAR_OFF	-17
#define	NC_TM		((KM_VIDEO_H - (mg->km.nc_h * CHAR_H - Y_OVHD)) / 2 \
				/ CHAR_H)
#define	NC_LM		((KM_VIDEO_W - (mg->km.nc_w * CHAR_W)) / 2 / CHAR_W)

#ifndef CTRL
#define	CTRL(s)		((s)&0x1f)
#endif CTRL

#if	MON_MOUSE
#define	SAVE		0
#define	RESTORE		1
#define	CURSOR_W	16
#define	CURSOR_H	16

#define	MOUSE_THRESH	5
short	km_mouse_thresholds[MOUSE_THRESH] = { 2, 3, 4, 5, 6 };
short	km_mouse_factors[MOUSE_THRESH] = { 2, 4, 6, 8, 10 };

u_int arrow_data[CURSOR_H] = {
	0x00000000, 0x30000000, 0x3c000000, 0x3f000000,
	0x3fc00000, 0x3ff00000, 0x3ffc0000, 0x3fff0000,
	0x3fffc000, 0x3ff00000, 0x3cf00000, 0x303c0000,
	0x003c0000, 0x000f0000, 0x000f0000, 0x00000000
};

u_int arrow_mask[CURSOR_H] = {
	0xf0000000, 0xfc000000, 0xff000000, 0xffc00000,
	0xfff00000, 0xfffc0000, 0xffff0000, 0xffffc000,
	0xfffff000, 0xfffffc00, 0xfffc0000, 0xfcff0000,
	0xf0ff0000, 0xc03fc000, 0x003fc000, 0x000fc000
};
#endif	MON_MOUSE

kminit()
{
	register struct mon_global *mg = restore_mg();

	if ((mg->km.flags & KMF_HW_INIT) == 0) {

		/* allow hardware to settle */
		delay(150000);
		
		/* switch console to SCC if monitor is not responding */
		if ((km_send (MON_KM_USRREQ, KM_SET_ADRS(0)) & MON_NO_RESP) &&
		    mg->mg_nvram.ni_alt_cons) {
			mg->mg_console_i = CONS_I_SCC_A;
			mg->mg_console_o = CONS_O_SCC_A;
			return (0);
		}
		
		/* blink keyboard LEDs during selftest */
		if ((mg->mg_nvram.ni_pot[0] & POT_ON) &&
		    !(mg->km.flags & KMF_NMI)) {
			km_send (MON_KM_USRREQ, KM_SET_LEDS(0, 3));
			delay (250000);
		}
		km_send (MON_KM_USRREQ, KM_SET_LEDS(0, 0));

		/* for now, hardwire one keyboard and mouse */
		mon_send (MON_KM_POLL, 0x01fffff6);
		
		/* wait for power on key to go up */
		while (*mg->mg_intrstat & I_BIT(I_POWER))
			rtc_intr();
		mg->km.flags |= KMF_HW_INIT;
	}

	kminit2();
	mg->km.flags |= KMF_INIT;
	mg->km.nc_w = NC_W_BIG;
	mg->km.nc_h = NC_H_BIG;
	mg->km.nc_lm = NC_LM;
	mg->km.nc_tm = NC_TM;
	if (mg->km.flags & KMF_SEE_MSGS)
		km_flip_cursor();
	return (1);
}

kminit2()
{
	register struct mon_global *mg = restore_mg();
	register int i;

	mg->km.bg = KM_WHITE;
	mg->km.fg = KM_BLACK;
	mg->km.x = 0;
	mg->km.y = 0;
	mg->km.cp = &mg->km.p[1];
	for (i = 0; i < KM_NP; i++)
		mg->km.p[i] = 0;
	mg->mx = 0;
}

kmpopup (title, w, h)
	char *title;
{
	struct mon_global *mg = restore_mg();
	register int vmem, sp, x, y, dp;
	int unit, bg, bg2, bg3, error, i;
	char c, *mp, ROM_title[64];

	if (title == 0) {
		sprintf (ROM_title, "NeXT ROM Monitor %d.%d (v%d Warp9/X15 beta)",
			MAJOR, MINOR, SEQ);
		title = ROM_title;
	}
	mg->km.flags |= KMF_SEE_MSGS;
	/* create new window */
	mg->km.nc_w = w? w : NC_W_BIG;
	mg->km.nc_h = h? h : NC_H_BIG;
	mg->km.nc_lm = NC_LM;
	mg->km.nc_tm = NC_TM;
	vmem = KM_P_VIDEOMEM + (mg->km.nc_lm * CHAR_W * sizeof(int) / KM_NPPW) - 
	       (X_OVHD * CHAR_W * sizeof(int) / (KM_NPPW * 2) );
	if (w || h)
		mg->km.store = 0x04100000;	/* 1 Meg into main memory! */
	else
		mg->km.store = 0;
	dp = mg->km.store;
	km_begin_access();
	for (y = 0; y < Y_OVHD + mg->km.nc_h * CHAR_H; y++) {
		if (y == 0 || y == 22 ||
		    y == Y_OVHD + mg->km.nc_h * CHAR_H - 1)
			bg = bg2 = bg3 = KM_BLACK;
		else
		if (y >= 2 && y <= 20)
			bg = KM_BLACK, bg2 = KM_LT_GRAY, bg3 = KM_DK_GRAY;
		else
		if (y == 1)
			bg = bg2 = bg3 = KM_LT_GRAY;
		else
		if (y == 21)
			bg = bg2 = bg3 = KM_DK_GRAY;
		else
			bg = bg2 = bg3 = KM_WHITE;
		sp = vmem + (mg->km.nc_tm * CHAR_H + y - Y_OVHD +
			Y_BOT) * KM_VIDEO_NBPL;
		/*
		 * Fill in a scanline.  Skip the first 6 pixels, set the 7th
		 * pixel to black, the 8th pixel to bg2, and most of the rest
		 * to bg.  Set the 2nd to last pixel to bg3, and the last to black.
		 *
		 * This is pretty horrible code.
		 */
		switch ( KM_NPPW )	/* Switch on the number of pixels per word */
		{
		    case 16:	/* 2 bit pixels */
			if (dp)
				*((int*)dp)++ = *(int*)sp;
			*(int*)sp =  *(int*)sp & 0xfff00000 | (0x000c0000 & KM_BLACK) |
				(bg & 0x0000ffff) | (bg2 & 0x00030000);
			((int*)sp)++;
			for (x = 1; x < (((mg->km.nc_w + X_OVHD) >> 1) +
					 (X_OVHD & 1)) - 1; x++) {
				if (dp)
					*((int*)dp)++ = *(int*)sp;
				*((int*)sp)++ = bg;
			}
			if (dp)
				*((int*)dp)++ = *(int*)sp;
#if	(X_OVHD & 1)
			*(int*)sp =  *(int*)sp & 0x0fffffff | (0x30000000 & KM_BLACK) |
				(bg3 & 0xc0000000);
#else
			*(int*)sp =  *(int*)sp & 0x00000fff | (0x00003000 & KM_BLACK) |
				(bg & 0xffff0000) | (bg3 & 0x0000c000);
#endif
			((int*)sp)++;
			break;
		    case 4:	/* Byte size pixels */
		    	((char*)sp) += 7;	/* Bump into our X_OVHD margin */
		    	if ( dp ){
				*((char*)dp)++ = *((char*)sp);
				*((char*)sp)++ = KM_BLACK & 0xFF; /* Pixel 7 */
				*((char*)dp)++ = *((char*)sp);
				*((char*)sp)++ = bg2 & 0xFF;	  /* Pixel 8 */
				for(i = 9; i < ((mg->km.nc_w + X_OVHD)*CHAR_W)-9; ++i){
					*((char*)dp)++ = *(char*)sp;
					*((char*)sp)++ = bg & 0xFF;
				}
				*((char*)dp)++ = *((char*)sp);
				*((char*)sp)++ = bg3 & 0xFF; 	 /* 2nd to last */
				*((char*)dp)++ = *((char*)sp);
				*((char*)sp)++ = KM_BLACK & 0xFF; /* last pixel */
			} else {
				*((char*)sp)++ = KM_BLACK & 0xFF; /* Pixel 7 */
				*((char*)sp)++ = bg2 & 0xFF;	  /* Pixel 8 */
				for (i = 9; i < ((mg->km.nc_w + X_OVHD)*CHAR_W)-9; ++i)
					*((char*)sp)++ = bg & 0xFF;
				*((char*)sp)++ = bg3 & 0xFF;	 /* 2nd to last */
				*((char*)sp)++ = KM_BLACK & 0xFF; /* last pixel */
			}
			break;
		    case 2:	/* Word size pixels */
		    	((short*)sp) += 7;	/* Bump into our X_OVHD margin */
		    	if ( dp ){
				*((short*)dp)++ = *((short*)sp);
				*((short*)sp)++ = KM_BLACK & 0xFFFF; /* Pixel 7 */
				*((short*)dp)++ = *((short*)sp);
				*((short*)sp)++ = bg2 & 0xFFFF;	  /* Pixel 8 */
				for(i = 9; i < ((mg->km.nc_w + X_OVHD)*CHAR_W)-9; ++i){
					*((short*)dp)++ = *(short*)sp;
					*((short*)sp)++ = bg & 0xFFFF;
				}
				*((short*)dp)++ = *((short*)sp);
				*((short*)sp)++ = bg3 & 0xFFFF;      /* 2nd to last */
				*((short*)dp)++ = *((short*)sp);
				*((short*)sp)++ = KM_BLACK & 0xFFFF; /* last pixel */
			} else {
				*((short*)sp)++ = KM_BLACK & 0xFFFF; /* Pixel 7 */
				*((short*)sp)++ = bg2 & 0xFFFF;	     /* Pixel 8 */
				for (i = 9; i < ((mg->km.nc_w + X_OVHD)*CHAR_W)-9; ++i)
					*((short*)sp)++ = bg & 0xFFFF;
				*((short*)sp)++ = bg3 & 0xFFFF;	     /* 2nd to last */
				*((short*)sp)++ = KM_BLACK & 0xFFFF; /* last pixel */
			}
			break;
		    case 1:	/* longword sized pixels */
		    	((int*)sp) += 7;	/* Bump into our X_OVHD margin */
		    	if ( dp ){
				*((int*)dp)++ = *((int*)sp);
				*((int*)sp)++ = KM_BLACK;	/* Pixel 7 */
				*((int*)dp)++ = *((int*)sp);
				*((int*)sp)++ = bg2;		/* Pixel 8 */
				for(i = 9; i < ((mg->km.nc_w + X_OVHD)*CHAR_W)-9; ++i){
					*((int*)dp)++ = *(int*)sp;
					*((int*)sp)++ = bg;
				}
				*((int*)dp)++ = *((int*)sp);
				*((int*)sp)++ = bg3;		/* 2nd to last */
				*((int*)dp)++ = *((int*)sp);
				*((int*)sp)++ = KM_BLACK;	/* last pixel */
			} else {
				*((int*)sp)++ = KM_BLACK;	/* Pixel 7 */
				*((int*)sp)++ = bg2;		/* Pixel 8 */
				for (i = 9; i < ((mg->km.nc_w + X_OVHD)*CHAR_W)-9; ++i)
					*((int*)sp)++ = bg;
				*((int*)sp)++ = bg3;		/* 2nd to last */
				*((int*)sp)++ = KM_BLACK;	/* last pixel */
			}
		    	break;
		}
	}
	mg->km.bg = KM_BLACK;
	mg->km.fg = KM_WHITE;
	mg->km.x = mg->km.nc_w / 2 - strlen (title) / 2;
	mg->km.y = TITLEBAR_OFF;
	for (i = 0; c = ((char*)title)[i]; i++)	
		kmpaint (c);
	km_flip_cursor();
	km_end_access();
	kminit2();
	mg->km.flags &= ~KMF_CURSOR;
}

kmrestore()
{
	register int vmem, dp, sp, x, y;
	struct mon_global *mg = restore_mg();

	/* restore obscured bits */
	if (mg->km.store) {
		mg->km.flags &= ~KMF_SEE_MSGS;
		vmem = KM_P_VIDEOMEM + (mg->km.nc_lm * CHAR_W * sizeof(int)/KM_NPPW) - 
		       (X_OVHD * CHAR_W * sizeof(int) / (KM_NPPW * 2) );
		sp = mg->km.store;
		km_begin_access();
		for (y = 0; y < Y_OVHD + mg->km.nc_h * CHAR_H; y++) {
			/* Set start of scanline. */
			dp = vmem + (mg->km.nc_tm * CHAR_H + y - Y_OVHD + Y_BOT) *
				KM_VIDEO_NBPL;
			switch( KM_NPPW )  /* Switch on number of pixels per word. */
			{
			    case 16:	/* 2 bit pixels */
				for (x = 0; x < (((mg->km.nc_w + X_OVHD) >> 1) +
						 (X_OVHD & 1)); x++)
					*((int*)dp)++ = *((int*)sp)++;
				break;
			    case 4:	/* 8 bit pixels */
			    	((char*)dp) += 7;    /* Bump into our X_OVHD margin */
				for(x = 7; x < ((mg->km.nc_w + X_OVHD)*CHAR_W)-7; ++x)
					*((char*)dp)++ = *((char*)sp)++;
			    	break;
			    case 2:	/* 16 bit pixels */
			    	((short*)dp) += 7;    /* Bump into our X_OVHD margin */
				for (x = 7; x < ((mg->km.nc_w + X_OVHD)*CHAR_W)-7; ++x)
					*((short*)dp)++ = *((short*)sp)++;
			    	break;
			    case 1:	/* 32 bit pixels */
			    	((int*)dp) += 7;    /* Bump into our X_OVHD margin */
				for (x = 7; x < ((mg->km.nc_w + X_OVHD)*CHAR_W)-7; ++x)
					*((int*)dp)++ = *((int*)sp)++;
			        break;
			}
		}
		km_end_access();
		mg->km.store = 0;
	}
}

km_clear_nmi()
{
	struct mon_global *mg = restore_mg();
	register volatile struct monitor *mon = (struct monitor*) P_MON;

	mon->mon_csr.km_clr_nmi = 1;
}

km_prim_ischar (dev)
	dev_t dev;
{
	register struct mon_global *mg = restore_mg();
	register volatile struct monitor *mon = (struct monitor*) P_MON;
	struct mon_status csr;
	register int mouse;

	csr = mon->mon_csr;
	if (csr.km_dav || csr.km_ovr) {
		if (csr.km_ovr)
			mon->mon_csr.km_ovr = 1;
		mg->km.kybd_event.data = mon->mon_km_data;
		mouse = MON_DEV_ADRS (mg->km.kybd_event.data);
		if (mg->km.kybd_event.k.up_down == 0 || mouse) {
			mg->mg_flags |= MGF_KM_EVENT;
			return (1);
		}
	}
	return (0);
}

kmis_char (dev)
	dev_t dev;
{
	register struct mon_global *mg = restore_mg();

	if (mg->mg_flags & MGF_KM_TYPE_AHEAD) {
		mg->mg_flags &= ~MGF_KM_TYPE_AHEAD;
		return (1);
	}
	return (0);
}

kmscan (dev)
	dev_t dev;
{
	register struct mon_global *mg = restore_mg();

	if (!km_prim_ischar (dev))
		return;
	switch (km_prim_getc (dev)) {

	case inv:
		break;

	case CTRL('S'):
		mg->km.flags |= KMF_STOP;
		break;

	default:
		mg->mg_flags |= MGF_KM_TYPE_AHEAD;
		/* fall through ... */

	case CTRL('Q'):
		mg->km.flags &= ~KMF_STOP;
		break;

	}
}

kmputc (dev, c)
	dev_t dev;
	register int c;
{
	register struct mon_global *mg = restore_mg();

	if ((mg->km.flags & KMF_INIT) == 0) {
		if (kminit() == 0)
			return (0);
		if (mg->km.flags & KMF_SEE_MSGS) {
			kmpopup (0, 0, 0);				
		}
	}
	if ((mg->km.flags & KMF_SEE_MSGS) == 0)
		return (1);
	do {
		kmscan (dev);
	} while (mg->km.flags & KMF_STOP);
	if (c == '\n')
		kmpaint ('\r');
	kmpaint (c);
	return (1);
}

km_power_check()
{
	register struct mon_global *mg = restore_mg();
	register volatile struct monitor *mon = (struct monitor*) P_MON;
	struct mon_status csr;
	int c;

	if ((*mg->mg_intrstat & I_BIT(I_POWER)) && rtc_intr()) {
		if (mg->km.flags & KMF_SEE_MSGS) {
			printf ("\nreally power down? ");
			do {
				do {
					csr = mon->mon_csr;
				} while (csr.km_dav == 0);
				mg->km.kybd_event.data =
					mon->mon_km_data;
				c = kybd_process (&mg->km.kybd_event);
			} while (c == inv);
		} else
			c = 'y';
		if (c == 'y') {
			while (*mg->mg_intrstat & I_BIT(I_POWER))
				rtc_intr();
#if	ODBOOT
			od_eject (0, 1);
#endif	ODBOOT
#if	FDBOOT
			fd_eject (0);
#endif	FDBOOT
			rtc_power_down();
			while (1) ;
		}
		kmpaint (c);
		mg->mg_flags &= ~MGF_KM_TYPE_AHEAD;
		while (*mg->mg_intrstat & I_BIT(I_POWER))
			rtc_intr();
		return ('\n');
	}
	return (0);
}

km_prim_getc (dev)
	dev_t dev;
{
	register struct mon_global *mg = restore_mg();
	register volatile struct monitor *mon = (struct monitor*) P_MON;
	struct mon_status csr;
	register int c, mouse;
	union mouse_event me;

	/* FIXME: why does a test w/ mon->mon_csr.km_dav fail? */
	do {
		if (c = km_power_check())
			return (c);
		
		/* repoll monitor in case it has just been plugged back in */
		mon_send (MON_KM_POLL, 0x01fffff6);

		csr = mon->mon_csr;
	} while (csr.km_dav == 0 && (mg->mg_flags & MGF_KM_EVENT) == 0);
	if (mg->mg_flags & MGF_KM_EVENT)
		mg->mg_flags &= ~MGF_KM_EVENT;
	else
		mg->km.kybd_event.data = mon->mon_km_data;

	/* reenable keyboard if it is not responding */
	if ((mg->km.kybd_event.data & MON_NO_RESP) &&
	    (mg->km.kybd_event.data & MON_MASTER) == 0) {
		km_send (MON_KM_USRREQ, KM_SET_ADRS(0));
	}

	mouse = MON_DEV_ADRS (mg->km.kybd_event.data) == 1;
	if (mouse) {
#if	MON_MOUSE
		if (mg->km.flags & KMF_SEE_MSGS)
			return (inv);
		me.data = mg->km.kybd_event.data;
		process_mouse (mg, &me);
		if (me.m.button_left == 0 || me.m.button_right == 0)
			return (-1);
		else
#endif	MON_MOUSE
			return (inv);
	}
	mg->mg_flags &= ~MGF_KM_TYPE_AHEAD;
	return (kybd_process (&mg->km.kybd_event));
}

kmgetc (dev)
	dev_t dev;
{
	register struct mon_global *mg = restore_mg();
	register int c;

	do {
		c = km_prim_getc (dev);
	} while (c == inv || c == CTRL('S') || c == CTRL('Q'));
	mg->mg_flags &= ~MGF_KM_TYPE_AHEAD;
	if (c == '\r')
		c = '\n';
	return (c);
}

char	bright_table[BRIGHT_MAX+1] = {
	0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3e, 0x3d, 0x3b, 0x37, 0x2f,
	0x1e, 0x3c, 0x39, 0x33, 0x27, 0x0e, 0x1d, 0x3a, 0x35, 0x2b, 0x16,
	0x2c, 0x18, 0x30, 0x21, 0x02, 0x05, 0x0b, 0x17, 0x2e, 0x1c, 0x38,
	0x31, 0x23, 0x06, 0x0d, 0x1b, 0x36, 0x2d, 0x1a, 0x34, 0x29, 0x12,
	0x24, 0x08, 0x11, 0x22, 0x04, 0x09, 0x13, 0x26, 0x0c, 0x19, 0x32,
	0x25, 0x0a, 0x15, 0x2a, 0x14, 0x28, 0x10
};

kybd_process (ke)
	register union kybd_event *ke;
{
	register struct mon_global *mg = restore_mg();
	register struct nvram_info *ni = &mg->mg_nvram;
	u_char *brightness = (u_char*) P_BRIGHTNESS;
	register int shift, control, c;
	int level;

	/* FIXME: handle up/down events in RAW mode? */

	/* ignore invalid or up events */
	if (ke->k.valid == 0 || ke->k.up_down == 1)
		return (inv);
	shift = (ke->k.shift_right || ke->k.shift_left)? 1 : 0;
	control = ke->k.control? 162 : 0;
	c = ascii[((ke->k.key_code << 1) | shift) + control];

	switch (c) {

	case dim:
	case bright:
		nvram_check (ni);
		level = ni->ni_brightness + (c == dim? -1 : 1);
		level = MAX (level, BRIGHT_MIN);
		level = MIN (level, BRIGHT_MAX);
		ni->ni_brightness = level;
		nvram_set (ni);
		level = bright_table[level];
		*brightness = level | BRIGHT_ENABLE;
		break;

	case loud:
		break;

	case quiet:
		break;

	case up:
		break;

	case down:
		break;

	case left:
		break;

	case right:
		break;

	default:
		return (c);
	}

	return (inv);
}

/* send a command to the keyboard/mouse bus and wait for reply packet */
km_send (mon_cmd, cmd)
{
	register volatile struct monitor *mon = (struct monitor*) P_MON;
	register int i = 100000;

	mon_send (mon_cmd, cmd);

	/* wait for reply packet */
	while (i && mon->mon_csr.km_dav == 0)
		i--;
	if (i == 0)
		return (MON_NO_RESP);
	return (0);
}

/* send a command to the monitor gate array */
mon_send (mon_cmd, data)
{
	register struct mon_global *mg = restore_mg();
	register volatile struct monitor *mon = (struct monitor*) P_MON;
	struct mon_status csr;
	register int i;

	if ((mg->km.flags & KMF_MON_INIT) == 0) {
		mon->mon_csr.reset = 1;
		mg->km.flags |= KMF_MON_INIT;
	}
	mon->mon_csr.cmd = mon_cmd;
	mon->mon_data = data;
	delay (100);
}

km_clear_win()
{
	register struct mon_global *mg = restore_mg();
	register int x, y, endy, vmem;

	/* clear rectangle within screen */
	endy = CHAR_H * mg->km.nc_tm + CHAR_H * mg->km.nc_h;
	km_begin_access();
	for (y = CHAR_H * mg->km.nc_tm; y < endy; y++) {
		/* Place vmem at left end of scanline to be cleared. */
		vmem = KM_P_VIDEOMEM + (y * KM_VIDEO_NBPL) + 
			((mg->km.nc_lm + mg->km.x) * CHAR_W * sizeof (int))/KM_NPPW;
		for(x = (mg->km.nc_lm + mg->km.x) * CHAR_W;
		    x < ((mg->km.nc_lm + mg->km.nc_w) * CHAR_W);
		    x += KM_NPPW ) {
			*((int *)vmem)++ = mg->km.bg;
		}
	}
	km_end_access();
}

km_clear_screen()
{
	register struct mon_global *mg = restore_mg();
	register int i;
	register int * vmem;
	register int size;

	km_begin_access();
	vmem = (int *)KM_P_VIDEOMEM;
	size = (KM_VIDEO_NBPL * KM_VIDEO_H) / sizeof(int);
	for ( i = 0; i < size; ++i )
		*vmem++ = KM_DK_GRAY;
	km_end_access();
}

kmpaint (c)
	register char c;
{
	register struct mon_global *mg = restore_mg();
	register int i, j, y, vp, vmem;
	int longs, repeat, pattern, bpl, chunk;
	register int *s, *d;
	
	km_begin_access();
	km_flip_cursor();

	switch (c) {

	case '\r':
		if ((mg->km.flags & KMF_CURSOR) == 0) {
			mg->km.flags |= KMF_CURSOR;
			km_flip_cursor();
		}
		mg->km.x = 0;
		break;

	case '\n':
		mg->km.y++;
		break;

	case '\b':
		if (mg->km.x == 0)
			break;
		mg->km.x--;
		break;

	case '\t':
		j = mg->km.x;
		for (i = 0; i < TAB_SIZE - (j % TAB_SIZE); i++) {
			km_flip_cursor();
			kmpaint (' ');
		}
		km_flip_cursor();
		break;

	case np:
		mg->km.x = mg->km.y = 0;
		km_clear_win();
		break;
		
	case '\007':
		SoundOutTest();
		break;

	default:
		if (c < ' ')		/* skip other control chars */
			break;
		if ((mg->km.flags & KMF_CURSOR) == 0)
			mg->km.flags |= KMF_CURSOR;
		c -= ' ';	/* font tables start at space char */
		/* Set vmem at upper left corner of char to be rendered. */
		if (mg->km.x >= 0){
		    vmem = KM_P_VIDEOMEM +
			(((mg->km.nc_lm + mg->km.x) * CHAR_W * sizeof (int))/KM_NPPW) +
			    (mg->km.nc_tm * CHAR_H * KM_VIDEO_NBPL);
		} else {	/* Treat km.x as a pixel offset into frame buffer? */
		    vmem = KM_P_VIDEOMEM + (((-mg->km.x) * sizeof(int)) / KM_NPPW);
		}
		/*
		 * Render each 8 pixel scanline fragment from the font.
		 */
		for (y = 0; y < CHAR_H; y++) {
			if (mg->km.y >= 0)
				vp = mg->km.y * CHAR_H;
			else
			if (mg->km.x >= 0)
				vp = mg->km.y;
			else
				vp = -mg->km.y;
			vp = vmem + (vp + y) * KM_VIDEO_NBPL;
			i = ohlfs12[c][y];	/* i gets 1 scanline frag from font. */
			pattern = 0;
			switch ( KM_NPPW )
			{
			    case 16:	/* 2 bit pixels */
				for (j = 0; j < CHAR_W; ++j)
					pattern |= ((((i >> j) & 1)?
					    mg->km.fg : mg->km.bg) & 3) << (j << 1);
				*(short*) vp = pattern;
				break;
			    case 4:	/* 8 bit pixels */
			        for ( j = CHAR_W - 1; j >= 0; --j )
				   *((char*)vp)++ =
				   	(((i >> j)&1)?mg->km.fg:mg->km.bg) & 0xFF;
				break;
			    case 2:	/* 16 bit pixels */
			        for ( j = CHAR_W - 1; j >= 0; --j )
				   *((short*)vp)++ =
				       (((i >> j)&1)?mg->km.fg:mg->km.bg)&0xFFFF;
				break;
			    case 1:	/* 32 bit pixels */
			        for ( j = CHAR_W - 1; j >= 0; --j )
				   *((int*)vp)++ = (((i >> j)&1)?mg->km.fg:mg->km.bg);
				break;
			}
		}
		if (mg->km.x >= 0)
			mg->km.x++;
		else
			mg->km.x -= CHAR_W;
		break;
	}

	if (mg->km.x >= mg->km.nc_w)		/* wrap */
		mg->km.x = 0,  mg->km.y++;
	if (mg->km.y >= mg->km.nc_h) {		/* scroll screen */
		mg->km.y = mg->km.nc_h - 1;
		bpl = KM_VIDEO_NBPL;
		/* Indent to left margin, scanline 0 */
		vmem = KM_P_VIDEOMEM + (mg->km.nc_lm * CHAR_W * sizeof(int) / KM_NPPW);
		chunk = CHAR_H * bpl;	/* Bytes in a full row of characters */
		longs = (mg->km.nc_w * CHAR_W) / KM_NPPW; /* longs per win scanline */
		for (y = CHAR_H * mg->km.nc_tm; y < CHAR_H * mg->km.nc_tm +
		    CHAR_H * (mg->km.nc_h - 1); y++) {
			d = (int*) (vmem + bpl*y);
			s = (int*) ((int) d + chunk);
			LOOP32 (longs, *d++ = *s++);
		}
		km_clear_eol();

	}
	km_flip_cursor();
	km_end_access();
}

km_flip_cursor()
{
	register struct mon_global *mg = restore_mg();
	register int vmem, vp, y, x;

	if (mg->km.x < 0)
		return;

	km_begin_access();
	/* Set vmem to upper left corner of cursor rect, on 1st line in window. */
	vmem = KM_P_VIDEOMEM +
		(((mg->km.nc_lm + mg->km.x) * CHAR_W * sizeof (int))/KM_NPPW) +
		(mg->km.nc_tm * CHAR_H * KM_VIDEO_NBPL);
	for (y = 0; y < CHAR_H; y++) {
		if (mg->km.y >= 0)		/* Bump down to current text line */
			vp = mg->km.y * CHAR_H;
		else
			vp = mg->km.y;
		vp = vmem + (vp + y) * KM_VIDEO_NBPL;	/* Bump to next scanline */
		/* Zap 8 pixels along the scanline */
		if ( KM_NPPW == 16 ) {
			*(short*) vp = ~*(short*) vp;
		} else {
			for ( x = 0; x < 8; x += KM_NPPW ) {
				*((int*)vp) = ~*((int*)vp);
				++((int*)vp);
			}
		}
	}
	km_end_access();

}

km_clear_eol()
{
	register struct mon_global *mg = restore_mg();
	register int x, y, vmem, nchars;

	km_begin_access();
	nchars = mg->km.nc_w - mg->km.x;	/* Number of characters to clear. */
	for (y = 0; y < CHAR_H; y++) {
		vmem = KM_P_VIDEOMEM
			+ (((mg->km.nc_tm + mg->km.y) * CHAR_H + y) * KM_VIDEO_NBPL)
			+ ((mg->km.nc_lm + mg->km.x) * CHAR_W * sizeof (int))/KM_NPPW;
		if ( KM_NPPW == 16 ) {	/* 2 bit display */
		    /* Hidden assumption: CHAR_W == 8 */
		    for (x = 0; x < nchars; x++)
			    *((short*)vmem)++ = mg->km.bg;
		} else {
		    /* Use x as a word index into the scanline */
		    for ( x = 0; x < (nchars * CHAR_W) / KM_NPPW; ++x )
		    	*((int*)vmem)++ = mg->km.bg;
		}
	}
	km_end_access();

}

#if	MON_MOUSE
process_mouse (mg, me)
	register struct mon_global *mg;
	register union mouse_event *me;
{
	register int d, x, y, sum, i;

	if (me->m.delta_x || me->m.delta_y) {
		mouse_cursor (mg, mg->mx, mg->my, RESTORE);
		x = me->m.delta_x & 0x3f;
		if (me->m.delta_x & 0x40)
			x |= 0xffffffc0;
		sum = x < 0? -x : x;
		y = me->m.delta_y & 0x3f;
		if (me->m.delta_y & 0x40)
			y |= 0xffffffc0;
		sum += y < 0? -y : y;
		if (sum > km_mouse_thresholds[0]) {
			for (i = 1; i < MOUSE_THRESH; i++)
				if (sum < km_mouse_thresholds[i])
					break;
			i--;
			x *= km_mouse_factors[i];
			y *= km_mouse_factors[i];
		}
		mg->mx -= x;
		mg->mx = MAX (mg->mx, 0);
		mg->mx = MIN (mg->mx, VIDEO_W - CURSOR_W);
		mg->my -= y;
		mg->my = MAX (mg->my, 0);
		mg->my = MIN (mg->my, VIDEO_H - CURSOR_H);
		mouse_cursor (mg, mg->mx, mg->my, SAVE);
	}
}

mouse_cursor (mg, mx, my, mode)
	register struct mon_global *mg;
	register int mx, my;
{
	register u_int *p, lsft, rsft, lmask, rmask, y;

	p = (u_int*) (P_VIDEOMEM + ((mx >> 4) << 2) + my * VIDEO_NBPL);
	for (y = 0; y < CURSOR_H; y++) {
		if (mode == RESTORE) {
			*p = mg->cursor_save[0][y];
			*(p + 1) = mg->cursor_save[1][y];
		} else {
			mg->cursor_save[0][y] = *p;
			mg->cursor_save[1][y] = *(p + 1);
		}
		p += VIDEO_NBPL >> 2;
	}
	if (mode == RESTORE)
		return;

	p = (u_int*) (P_VIDEOMEM + ((mx >> 4) << 2) + my * VIDEO_NBPL);
	lsft = mx % 16;
	rsft = 16 - lsft;
	lmask = ~((1 << (2 * rsft)) - 1);
	rmask = ~lmask;
	lsft <<= 1;
	rsft <<= 1;
	for (y = 0; y < CURSOR_H; y++) {
		*p = (*p & lmask) | (*p & (~arrow_mask[y] >> lsft)) |
			(arrow_data[y] >> lsft);
		*(p + 1) = (*(p + 1) & rmask) |
			(*(p + 1) & (~arrow_mask[y] << rsft)) |
			(arrow_data[y] << rsft);
		p += VIDEO_NBPL >> 2;
	}
}
#endif	MON_MOUSE

alert_popup (w, h, title, msg, p1, p2, p3)
{
	struct mon_global *mg = restore_mg();

	if ((mg->km.flags & KMF_SEE_MSGS) == 0) {
		mg->mg_flags &= ~MGF_ANIM_RUN;
		kmpopup (title, w, h);
	}
	mg->km.flags |= KMF_ALERT;
	printf (msg, p1, p2, p3);
}

alert (msg, p1, p2, p3)
{
	alert_popup (0, 0, "Error during boot", msg, p1, p2, p3);
}

alert_done()
{
	struct mon_global *mg = restore_mg();

	if ((mg->km.flags & KMF_ALERT) == 0)
		return;
	kmrestore();
	mg->mg_flags |= MGF_ANIM_RUN;
	mg->km.flags &= ~KMF_ALERT;
}

#include "mon/km_pcode.h"
#import <nextdev/nbicreg.h>

/*
 * Read the requested word from the p-code table.
 */
 static unsigned int
km_read_pcode( item )
    int item;
{
	struct mon_global *mg = restore_mg();
	register unsigned int *ip;
	register unsigned int result;
	
	ip = (unsigned int *) KM_PCODE_TABLE + 
		(item * (int)mg->km_coni.byte_lane_id);
	
	/* Only values of 1, 4, or 8 are valid here. */
	switch( (int)mg->km_coni.byte_lane_id )
	{
		case ID_ALL:
			result = *ip;
			break;
		case ID_MSB32:
			result = *ip++ & 0xFF000000;
			result |= (*ip++ & 0xFF000000) >> 8;
			result |= (*ip++ & 0xFF000000) >> 16;
			result |= (*ip & 0xFF000000) >> 24;
			break;
		case ID_MSB64:
			result = *ip & 0xFF000000; ip += 2;
			result |= (*ip & 0xFF000000) >> 8; ip += 2;
			result |= (*ip & 0xFF000000) >> 16; ip += 2;
			result |= (*ip & 0xFF000000) >> 24;
			break;
		default:
			return 0;	/* Bogus byte lane ID value. */
	}
	return result;
}
/*
 * Select the console display device.
 * Default to the standard 2 bit display.
 */

km_select_console( mg )
    struct mon_global *mg;
{
	int slot;
	int console_slot, fb_num;
	int prefer_console_slot = -1;
	int prefer_fb_num;
	int	save_flags;
	register struct nvram_info *ni;
	volatile int *scr2 = (volatile int *)P_SCR2;
	volatile struct scr1 scr1 = *(struct scr1*) P_SCR1;
	int machine_type;
	
	save_flags = mg->km.flags;		/* Device faults change the flags! */
	mon_setup_comm (mg->mg_sid, mg);	/* Make sure the NV RAM is sane */
	/*
	 * Initialize all of the attached console devices.
	 * First, take care of the on-board device.
	 * Then, scan the NextBus (if present) for devices and initialize them.
	 */
	machine_type = MACHINE_TYPE(scr1.s_cpu_rev);
	console_slot = mg->mg_sid;
	fb_num = 0;
	km_config_default(mg);
	km_clear_screen();
	km_unblank_screen(mg);

	/*
	 * It's only safe to zap the overlay bits if we are in slot 0.
	 * Also, the only machines that could have a NextBus are the 
	 * X15 and the 030 cubes.
	 */
	if ( console_slot == 0 &&
	    (machine_type == NeXT_CUBE || machine_type == NeXT_X15) )
	{
	    /*
	     * Get the preferred console slot from the non-volatile RAM.
	     */
	    ni = &mg->mg_nvram;
	    if ( ni->ni_use_console_slot )	/* Bit indicates user set the slot. */
		    prefer_console_slot = (ni->ni_console_slot & 3) << 1;
	    *scr2 |= SCR2_OVERLAY;		/* Gain access to NextBus */
	    
	    if (machine_type == NeXT_X15)	/* Kick the BMAP chip, too */
	    {
		volatile struct bmap_chip *bmap_chip = (struct bmap_chip *)P_BMAP;
		bmap_chip->bm_lo = 0;
	    }
	    /*
	     * If an NBIC is present, scan and configure frame buffers
	     * on the NextBus.
	     */
	    if ( probe_rw( mg, NBIC_CR ) ){ /* Is an NBIC installed? */
		*(volatile int *)NBIC_IDR = NBIC_IDR_VALID;
		*(volatile int *)NBIC_CR = NBIC_CR_STFWD;	   /* Configure NBIC */
		
		/* Delay just over 200 msec to allow frame buffers
		 * on the bus to settle
		 */
		delay( 210000 );
		/*
		 * Scan and configure each slot on the NextBus.
		 * The CPU slot is assumed to hold the default device,
		 * and so is skipped.
		 *
		 * Console_slot will hold the slot for the highest available 
		 * console device when we are done. This will become the console,
		 * unless a match was found for the preferred console as
		 * specified in NVRAM.
		 */
		for ( slot = 0; slot <= KM_HIGH_SLOT; slot += 2 ) {
		    if ( slot == mg->mg_sid )
			    continue;
		    if ( km_init_slot(mg, slot) == 1 ) {
			    console_slot = slot;
			    fb_num = mg->km_coni.fb_num;
			    if ( slot == prefer_console_slot )
				    prefer_fb_num = fb_num;
		    } else {
		    /* If preferred console isn't available, invalidate it. */
			    if ( slot == prefer_console_slot )
				    prefer_console_slot = -1;
		    }
		}
	    }
	}

	/*
	 * Everything is initialized. Load up the configuration info for the last
	 * frame buffer we saw.
	 */
	if ( prefer_console_slot != -1 ) {
		console_slot = prefer_console_slot;
		fb_num = prefer_fb_num;
	}
	if ( console_slot == mg->mg_sid )
		km_config_default(mg);
	else
		km_get_config( mg, console_slot, fb_num );
	mg->km.flags = 	save_flags;
}

/*
 * Check out the slot specified by the ROM monitor, and load in the 
 * needed configuration tables.  Run the initialization code for each 
 * frame buffer on the board until one indicates ready.  Then clear the 
 * frame buffer and run the (optional) unblank function to enable video 
 * output.
 *
 * Values read are sanity checked here.  All code run later assumes that the
 * configuration tables are correct.
 */
km_init_slot( mg, slot )
    int slot;
    struct mon_global *mg;
{
	unsigned int * addr;
	unsigned int tmp;
	int nfb, pc, fbpc, i;

	addr = (unsigned int *) KM_SLOT_ID_REG(slot);
	/*
	 * First, see if there is even an NBIC in the selected slot.
	 */
	if ( probe_rw( mg, addr ) == 0 )	/* Probe using 32 bit bus cycle */
	{
		return 0;
	}
	/* Is the NBIC ID that of a console device? */
	if ( (*addr & KM_SLOT_CONSOLE_BITS) != KM_SLOT_CONSOLE_BITS )
		return 0;
	/*
	 * At this point we have a board which claims to be a console device.
	 * Proceed with caution, as a missing ROM could fault on this next access.
	 */
	addr = (unsigned int *)KM_CONVERT_ADDR(slot,KM_SIGNATURE_ADDR);
	if ( probe_rw( mg, addr ) == 0 )	/* Probe using 32 bit bus cycle */
	{
		return 0;
	}
	if ( (*addr & KM_SIGNATURE_MASK) != KM_SIGNATURE_BITS )
		return 0;		/* Nope. Bad signature byte */

	/* We have a frame buffer.  Try and configure it for use. */		
	bzero( &mg->km_coni, sizeof (struct km_console_info) );
	mg->km_coni.slot_num = slot;
	mg->km_coni.fb_num = 0;
	
	addr = (unsigned int *)KM_CONVERT_ADDR(slot, KM_BYTE_LANE_ID_ADDR);
	mg->km_coni.byte_lane_id = *addr >> 24;
	switch ( mg->km_coni.byte_lane_id )
	{
		case ID_ALL:
		case ID_MSB32:
		case ID_MSB64:
			break;		/* valid values */
		default:
			return 0;	/* Bogus byte lane ID value. */
	}
	
	tmp = KM_READ_SLOT_BYTE(slot, KM_PCODE_ADDR3) << 24;
	tmp |= KM_READ_SLOT_BYTE(slot, KM_PCODE_ADDR2) << 16;
	tmp |= KM_READ_SLOT_BYTE(slot, KM_PCODE_ADDR1) << 8;
	tmp |= KM_READ_SLOT_BYTE(slot, KM_PCODE_ADDR0);
	/* The address as read should be in the form 0xF0xxxxxx or 0x0xxxxxxx */
	if ( (tmp & 0xFF000000) != 0xF0000000 && (tmp & 0xF0000000) != 0 )
		return 0;
	mg->km_coni.map_addr[KM_CON_PCODE].virt_addr = KM_CONVERT_ADDR(slot,tmp);
	mg->km_coni.map_addr[KM_CON_PCODE].phys_addr = KM_CONVERT_ADDR(slot,tmp);
	
	/* Now read in the 'number_of_frame_buffers' value. */
	nfb = km_read_pcode( KMPC_NUM_FBS );
	if ( nfb == 0 )
		return 0;
	/*
	 * Run the initialization pseudo-code for each frame buffer on
	 * the board, until we exhaust the list or get a ready indication.
	 */
	pc = KMPC_TABLE_START;
	while ( pc < (KMPC_TABLE_START + (nfb * KMPC_TABLE_INCR)) ) {
	    if ( km_run_pcode( km_read_pcode(pc + KMPC_INIT_OFF) ) == 1 ) {
	    	/* A console device in a ready to run state has been detected. */
		km_get_config( mg, slot, (int)mg->km_coni.fb_num );
		/* Clear and unblank the display. */
		km_clear_screen();
		km_unblank_screen(mg);
		return 1;
	    }
	    pc += KMPC_TABLE_INCR;
	    ++(mg->km_coni.fb_num);
	}
	return 0;
}

km_config_default( mg )
    struct mon_global *mg;
{
	bzero( &mg->km_coni, sizeof (struct km_console_info) );
	KM_NPPW = NPPB * sizeof (int);
	KM_VIDEO_NBPL = VIDEO_NBPL;
	KM_VIDEO_W = VIDEO_W;
	KM_VIDEO_MW = VIDEO_MW;
	KM_VIDEO_H = VIDEO_H;
	KM_WHITE = WHITE;
	KM_LT_GRAY = LT_GRAY;
	KM_DK_GRAY = DK_GRAY;
	KM_BLACK = BLACK;
	mg->km_coni.map_addr[KM_CON_FRAMEBUFFER].virt_addr = P_VIDEOMEM;
	mg->km_coni.map_addr[KM_CON_FRAMEBUFFER].phys_addr = P_VIDEOMEM;
	mg->km_coni.map_addr[KM_CON_FRAMEBUFFER].size = VIDEO_H * VIDEO_NBPL;
	mg->km_coni.map_addr[KM_CON_BACKINGSTORE].virt_addr =
					P_VIDEOMEM + (VIDEO_H * VIDEO_NBPL);
	mg->km_coni.map_addr[KM_CON_BACKINGSTORE].phys_addr =
					P_VIDEOMEM + (VIDEO_H * VIDEO_NBPL);
	mg->km_coni.map_addr[KM_CON_BACKINGSTORE].size =
					(VIDEO_MH - VIDEO_H) * VIDEO_NBPL;
	mg->km_coni.slot_num = mg->mg_sid;
	mg->km_coni.fb_num = 0;
}

km_unblank_screen( mg )
    struct mon_global *mg;
{
	int pc;
	register struct nvram_info *ni = &mg->mg_nvram;
	extern char bright_table[];
	unsigned level;


	if ( mg->mg_sid != mg->km_coni.slot_num ) {
		pc = KMPC_TABLE_START + (mg->km_coni.fb_num * KMPC_TABLE_INCR);
		km_run_pcode( km_read_pcode(pc + KMPC_UNBLANK_SCREEN_OFF) );
		return;
	}
	/* Turn on the on-board display. Set the brightness from the NV RAM */
	level = ni->ni_brightness;
	if ( level < BRIGHT_NOM )
		level = BRIGHT_NOM;
	*(u_char*) P_BRIGHTNESS = BRIGHT_ENABLE | bright_table[level];
}

km_get_config( mg, slot, fb_num )
    struct mon_global *mg;
    int slot;
    int fb_num;
{
	unsigned int tmp;
	int i;
	unsigned int *ip;
	int pc;
	
	if ( slot == mg->mg_sid ) {
		km_config_default( mg );
		return;
	}
	bzero( &mg->km_coni, sizeof (struct km_console_info) );
	mg->km_coni.slot_num = slot;
	mg->km_coni.fb_num = fb_num;

	ip = (unsigned int *)KM_CONVERT_ADDR(slot, KM_BYTE_LANE_ID_ADDR);
	mg->km_coni.byte_lane_id = *ip >> 24;

	tmp = KM_READ_SLOT_BYTE(slot, KM_PCODE_ADDR3) << 24;
	tmp |= KM_READ_SLOT_BYTE(slot, KM_PCODE_ADDR2) << 16;
	tmp |= KM_READ_SLOT_BYTE(slot, KM_PCODE_ADDR1) << 8;
	tmp |= KM_READ_SLOT_BYTE(slot, KM_PCODE_ADDR0);
	mg->km_coni.map_addr[KM_CON_PCODE].virt_addr = KM_CONVERT_ADDR(slot,tmp);
	mg->km_coni.map_addr[KM_CON_PCODE].phys_addr = KM_CONVERT_ADDR(slot,tmp);

	/* Read the first 32 bits from the P-code ROM.  These give the size */
	/* Convert the p-code size into the number of bytes we need to map. */
	mg->km_coni.map_addr[KM_CON_PCODE].size =
			km_read_pcode(KMPC_SIZE) * 4 * (int)mg->km_coni.byte_lane_id;
	pc = KMPC_TABLE_START + (fb_num * KMPC_TABLE_INCR);
	mg->km_coni.start_access_pfunc =
			km_read_pcode(pc + KMPC_START_ACCESS_OFF );
	mg->km_coni.end_access_pfunc =
			km_read_pcode(pc + KMPC_END_ACCESS_OFF );
	pc = km_read_pcode(pc + KMPC_FB_LAYOUT_OFF);
	
	/* Read in the frame buffer layout structure */
	mg->km_coni.pixels_per_word = km_read_pcode( pc++ );
	mg->km_coni.bytes_per_scanline = km_read_pcode( pc++ );
	mg->km_coni.dspy_w = km_read_pcode( pc++ );
	mg->km_coni.dspy_max_w = km_read_pcode( pc++ );
	mg->km_coni.dspy_h = km_read_pcode( pc++ );
	mg->km_coni.flag_bits = km_read_pcode( pc++ );
	mg->km_coni.color[0] = km_read_pcode( pc++ );
	mg->km_coni.color[1] = km_read_pcode( pc++ );
	mg->km_coni.color[2] = km_read_pcode( pc++ );
	mg->km_coni.color[3] = km_read_pcode( pc++ );
	
	/* Read in the 5 map_addr structures. */
	for ( i = KM_CON_FRAMEBUFFER; i < KM_CON_MAP_ENTRIES; ++i ) {
	    tmp = km_read_pcode( pc++ );
	    mg->km_coni.map_addr[i].phys_addr = KM_CONVERT_ADDR(slot,tmp);
	    mg->km_coni.map_addr[i].virt_addr = KM_CONVERT_ADDR(slot,tmp);
	    mg->km_coni.map_addr[i].size = km_read_pcode( pc++ );
	}
	
	mg->km_coni.flag_bits |= KM_CON_ON_NEXTBUS;
	
	/* Deal with the case where the FB doesn't supply off-screen memory. */
	if ( KM_BACKING_SIZE == 0 ) {
	    KM_BACKING_STORE = KM_DEFAULT_STORE; /* In on-board mem */
	    KM_BACKING_SIZE = KM_DEFAULT_STORE_SIZE;
	}
}
/*
 * km_begin_access does any pre-conditioning that may be needed for a particular
 * console frame buffer.
 */
km_begin_access()
{
	struct mon_global *mg = restore_mg();

	if ( mg->km_coni.access_stack++ )
		return;
	if ( mg->km_coni.start_access_pfunc )
		km_run_pcode( mg->km_coni.start_access_pfunc );
}

/*
 * km_end_access does any post-access conditioning that may be needed for a particular
 * console frame buffer.
 */
km_end_access()
{
	struct mon_global *mg = restore_mg();
	if ( --mg->km_coni.access_stack > 0 )
		return;
	mg->km_coni.access_stack = 0;	/* Avoid underflow errors */
	
	if ( mg->km_coni.end_access_pfunc )
		km_run_pcode( mg->km_coni.end_access_pfunc );
}

/*
 * validate and convert the specified physical address into the correct 
 * virtual address.  This is support glue for the p-code machine.
 *
 * One could ask, "Why do this in the ROM code?"  Well, if one hits the NMI 
 * monitor from Mach, and then types the 'mon' command, one can then use the
 * ROM monitor while VM is active!  Yeech...
 */
 vm_offset_t
km_convert_addr( addr )
    vm_offset_t addr;
{
	int i;
	int delta;
	struct mon_global *mg = restore_mg();
	struct km_console_info *kip = &mg->km_coni;

	for ( i = 0; i < KM_CON_MAP_ENTRIES; ++i ) {
	    if ( kip->map_addr[i].size == 0 )	/* empty entry */
		    continue;
	    if ( addr >= kip->map_addr[i].phys_addr
		&& addr < (kip->map_addr[i].phys_addr + kip->map_addr[i].size) ){
		    delta = addr - kip->map_addr[i].phys_addr;
		    return( (vm_offset_t)(delta + kip->map_addr[i].virt_addr) );
	    }
	}
	return( addr );
}
/*
 * This function implements the actual p-machine which interprets the 
 * pseudocode in the extended ROM.
 *
 * The p-machine is called with the requested operation, which is really
 * the starting PC for the p-machine for the desired operation.  A value of zero
 * means that no p-machine operation is needed, and the p-machine should return
 * a value of 0.
 *
 * Once a starting PC has been determined, the execution loop is entered.
 * The byte lane format in use is examined, and an instruction is fetched.
 * The opcode is extracted.  If the opcode indicates that an immediate value
 * is needed, than that is also fetched. The pc is incremented.
 * The extracted opcode is processed through a case switch to the code
 * which implements the specified operation.
 *
 * An END opcode causes the p-machine to stop execution, and the p_machine
 * procedure returns the value in reg[0] to the caller.
 */
km_run_pcode( pc )
    int pc;
{
	int32 reg[P_NREGS];	/* P-machine register set */
	int32 immediate;	/* 32 bit immediate value for some insts. */
	unsigned int32 inst;	/* The current instruction */
	unsigned int opcode;	/* The OPCODE field from the inst. */
	int zero, positive, negative;	/* The condition codes... */
	vm_offset_t	addr;	/* An address, as found in the p-code. */
	struct mon_global *mg = restore_mg();
	int slot = mg->km_coni.slot_num;
	
	if ( pc == 0 )	/* No procedure for this op? */
		return( 0 );
		
	for(;;)
	{
		inst = km_read_pcode( pc++ );
		if ( IMMEDIATE_FOLLOWS_OPCODE( inst ) )
			immediate = km_read_pcode( pc++ );
		switch( OPCODE( inst ) )
		{
			case LOAD_CR:
			    reg[REG2(inst)] = immediate;
			    break;
				
			case LOAD_AR:
			    addr = (vm_offset_t)KM_CONVERT_ADDR(slot,immediate);
			    reg[REG2(inst)] = *((int *)addr);
			    break;
				
			case LOAD_IR:
			    addr = (vm_offset_t)KM_CONVERT_ADDR(slot,reg[REG1(inst)]);
			    addr = km_convert_addr(addr);
			    reg[REG2(inst)]=*((int *)addr);
			    break;
				
			case STORE_RA:
			    addr = (vm_offset_t)KM_CONVERT_ADDR(slot,immediate);
			    addr = km_convert_addr(addr);
			    *((int *)addr) = reg[REG1(inst)]; 
			    break;
							    
			case STORE_RI:
			    addr = (vm_offset_t)KM_CONVERT_ADDR(slot,reg[REG2(inst)]);
			    addr = km_convert_addr(addr);
			    *((int *)addr) = reg[REG1(inst)]; 
			    break;
			    
			case STOREV:
				{
				    int32 addr;
				    int32 counter = BRANCH_DEST(inst);
				    
				    do
				    {
					    immediate = km_read_pcode(pc++);
					    addr = (vm_offset_t)km_read_pcode(pc++);
					    addr =
					km_convert_addr(KM_CONVERT_ADDR(slot,addr));
					    *((int *)addr) = immediate;
				    } while( --counter );
				}
				break;
			   
			case ADD_CRR:
			    reg[REG2(inst)] = immediate + reg[REG1(inst)];
			    break;
			    
			case ADD_RRR:
			    reg[REG3(inst)]=reg[REG1(inst)]+reg[REG2(inst)];
			    break;
			    
			case SUB_RCR:
			    reg[REG2(inst)]=reg[REG1(inst)]-immediate;
			    break;
			    
			case SUB_CRR:
			    reg[REG2(inst)]=immediate - reg[REG1(inst)];
			    break;

			case SUB_RRR:
			    reg[REG3(inst)]=reg[REG1(inst)] - reg[REG2(inst)];
			    break;
			    
			case AND_CRR:
			    reg[REG2(inst)] = immediate & reg[REG1(inst)];
			    break;
			    
			case AND_RRR:
			    reg[REG3(inst)]=reg[REG1(inst)] & reg[REG2(inst)];
			    break;
			    
			case OR_CRR:
			    reg[REG2(inst)] = immediate | reg[REG1(inst)];
			    break;
			    
			case OR_RRR:
			    reg[REG3(inst)]=reg[REG1(inst)] | reg[REG2(inst)];
			    break;
			
			case XOR_CRR:
			    reg[REG2(inst)] = immediate ^ reg[REG1(inst)];
			    break;
			    
			case XOR_RRR:
			    reg[REG3(inst)]=reg[REG1(inst)] ^ reg[REG2(inst)];
			    break;

			case ASL_RCR:
			    reg[REG3(inst)] = reg[REG1(inst)] << FIELD2(inst);
			    break;

			case ASR_RCR:
			    reg[REG3(inst)] = reg[REG1(inst)] >> FIELD2(inst);
			    break;

			case MOVE_RR:
			    reg[REG2(inst)] = reg[REG1(inst)];
			    break;
			    
			case TEST_R:
				zero = 0;
				positive = 0;
				negative = 0;
				
				immediate = reg[REG1(inst)];
				if ( immediate == 0 )
				{
					zero = 1;
					break;
				}
				if ( immediate > 0 )
				{
					positive = 1;
					break;
				}
				negative = 1;
				break;
				
			case BR:
				pc = BRANCH_DEST( inst );
				break;
				
			case BPOS:
				if ( positive )
					pc = BRANCH_DEST( inst );
				break;
				
			case BNEG:
				if ( negative )
					pc = BRANCH_DEST( inst );
				break;

			case BZERO:
				if ( zero )
					pc = BRANCH_DEST( inst );
				break;

			case BNPOS:
				if ( ! positive )
					pc = BRANCH_DEST( inst );
				break;

			case BNNEG:
				if ( ! negative )
					pc = BRANCH_DEST( inst );
				break;

			case BNZERO:
				if ( ! zero )
					pc = BRANCH_DEST( inst );
				break;

			case END:
				return( reg[0] );
		}
	}
}

