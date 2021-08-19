/*	@(#)boot.c	1.0	10/16/86	(c) 1986 NeXT	*/

/*
 * History
 *
 * 09-Apr-90	Doug Mitchell at NeXT
 *	Added floppy support.
 */
#include <sys/types.h>
#include <mon/global.h>
#include <mon/sio.h>
#include <mon/nvram.h>
#include <mon/monparam.h>
#include <next/scb.h>
#include <nextdev/dma.h>
#include <nextdev/video.h>

extern char od_icon[], od_icon2[], od_icon3[],
	od_arrow1[], od_arrow2[], od_arrow3[], od_flip[], od_bad[],
	sd_icon[], sd_icon2[], sd_icon3[],
	en_icon[], en_icon2[], en_icon3[];

struct animation animation[] = {

#define	OD_SPIN		0
/* 0 */		{ TEXT_X, TEXT_Y3, "Loading\nfrom\ndisk ...", 1, 0 },
/* 1 */		{ 640, 395+84, od_icon, 2, 68/12 },
/* 2 */		{ 640, 423+41, od_icon2, 3, 68/12 },
/* 3 */		{ 640, 423+41, od_icon3, 1, 68/12 },

#define	OD_INSERT	4
/* 4 */		{ TEXT_X, TEXT_Y3, "Please\ninsert\ndisk", 5, 0 },
/* 5 */		{ 640, 395+84, od_icon, 6, 1 },
/* 6 */		{ 672, 431+17, od_arrow1, 7, 68/2 },
/* 7 */		{ 640, 395+84, od_icon, 8, 1 },
/* 8 */		{ 672, 439+17, od_arrow2, 9, 68/2 },
/* 9 */		{ 640, 395+84, od_icon, 10, 1 },
/* 10 */	{ 672, 447+17, od_arrow3, 5, 68/2 },

#define	OD_FLIP		11
/* 11 */	{ TEXT_X, TEXT_Y3, "Please\nflip\ndisk", 12, 0 },
/* 12 */	{ 640, 395+84, od_icon, 13, 68/2 },
/* 13 */	{ 664, 437+20, od_flip, 12, 68/2 },

#define	OD_BAD		14
/* 14 */	{ TEXT_X, TEXT_Y2, "Bad\ndisk", 15, 0 },
/* 15 */	{ 640, 395+84, od_icon, 16, 68/2 },
/* 16 */	{ 640, 423+41, od_bad, 15, 68/2 },

#define	SD_SPIN		17
/* 17 */	{ TEXT_X, TEXT_Y3, "Loading\nfrom\ndisk ...", 18, 0 },
/* 18 */	{ 640, 390+95, sd_icon, 19, 68/12 },
/* 19 */	{ 640, 415+65, sd_icon2, 20, 68/12 },
/* 20 */	{ 640, 415+65, sd_icon3, 18, 68/12 },

#define	SD_BAD		21
/* 21 */	{ TEXT_X, TEXT_Y2, "SCSI\nerror", 22, 0 },
/* 22 */	{ 640, 390+95, sd_icon, 22, 68/1 },

#define	EN_SPIN		23
/* 23 */	{ TEXT_X, TEXT_Y3, "Loading\nfrom\nnetwork ...", 24, 0 },
/* 24 */	{ 640, 395+84, en_icon, 25, 68/12 },
/* 25 */	{ 640, 416+63, en_icon2, 26, 68/12 },
/* 26 */	{ 640, 416+63, en_icon3, 24, 68/12 },

#define	EN_BAD		27
/* 27 */	{ TEXT_X, TEXT_Y2, "Bad\nnetwork", 28, 0 },
/* 28 */	{ 640, 395+84, en_icon, 28, 68/1 },

#define	FD_SPIN		29
/* FIXME : get real FD bitmaps */
/* 29 */	{ TEXT_X, TEXT_Y3, "Loading\nfrom\nfloppy ...", 30, 68/1 },
/* 30 */	{ 640, 395+84, od_icon, 31, 68/12 },
/* 31 */	{ 640, 423+41, od_icon2, 32, 68/12 },
/* 32 */	{ 640, 423+41, od_icon3, 30, 68/12 },

#define	FD_BAD		33
/* 33 */	{ TEXT_X, TEXT_Y2, "Bad\ndisk", 34, 0 },
/* 34 */	{ 640, 395+84, od_icon, 35, 68/2 },
/* 35 */	{ 640, 423+41, od_bad, 34, 68/2 },

#define	FD_INSERT	36
/* 36 */	{ TEXT_X, TEXT_Y3, "Please\ninsert\ndisk", 37, 0 },
/* 37 */	{ 640, 395+84, od_icon, 38, 1 },
/* 38 */	{ 672, 431+17, od_arrow1, 39, 68/2 },
/* 39 */	{ 640, 395+84, od_icon, 40, 1 },
/* 40 */	{ 672, 439+17, od_arrow2, 41, 68/2 },
/* 41 */	{ 640, 395+84, od_icon, 42, 1 },
/* 42 */	{ 672, 447+17, od_arrow3, 37, 68/2 },
};

extern struct protocol proto_srec, proto_comp, proto_tftp, proto_disk;

extern struct device enet_ex;
extern struct device serial_mk;
extern struct device enet_en;
extern struct device serial_zs;
extern struct device scsi_disk;
extern struct device optical_disk;
extern struct device floppy_disk;

struct bootdev {
	char *b_name;
	struct protocol *b_proto;
	struct device *b_dev;
	char *b_desc;
	char b_boot, b_bad, b_insert, b_flip;
} bootdev[] = {

#if	NETBOOT
	{ "en", &proto_tftp, &enet_en, "Ethernet (try thin interface first)",
	EN_SPIN, EN_BAD, -1, -1 },
	{ "tp", &proto_tftp, &enet_en, "Ethernet (try twisted pair interface first)",
	EN_SPIN, EN_BAD, -1, -1 },
#endif	NETBOOT

#if	SCSIBOOT
	{ "sd", &proto_disk, &scsi_disk, "SCSI disk",
	SD_SPIN, SD_BAD, -1, -1 },
#endif	SCSIBOOT

#if	ODBOOT
	{ "od", &proto_disk, &optical_disk, "Optical disk",
	OD_SPIN, OD_BAD, OD_INSERT, OD_FLIP },
#endif	ODBOOT

#if	FDBOOT
	{ "fd", &proto_disk, &floppy_disk, "Floppy disk",
	/* FIXME - get real icons */
	FD_SPIN, FD_BAD, FD_INSERT, -1 },
#endif	FDBOOT

	0,
};

boot (mg, how)
	struct mon_global *mg;
	enum SIO_ARGS how;
{
	struct sio sio;
	char *lp, *save_lp;
	register struct sio *si = &sio;
	register struct nvram_info *ni = &mg->mg_nvram;
	register struct bootdev *b;
	int entryaddr, n1 = 0, n2 = 0, e = 0, last_e;
	char dev[3], *cp, save[128], *tp;
	extern char *skipwhite(), *scann(), loginwindow[];
	struct dma_dev *vddp = (struct dma_dev*) P_VIDEO_CSR;
again:
	
	lp = mg->mg_boot_arg;
	bzero (si, sizeof (*si));
	si->si_args = how;
	lp = skipwhite (lp);
	if (*lp == '?')
		goto usage;
	if (!(dev[0] = *lp) || !(dev[1] = *(++lp))) {
		if (ni->ni_bootcmd[0] == 0) {
			see_msg ("No default boot command.\n");
			return (0);
		}
		strcpy (mg->mg_boot_arg, ni->ni_bootcmd);
		lp = mg->mg_boot_arg;
		printf ("Boot command: %s\n", lp);
		cp = skipwhite (lp);
		for (b = bootdev; b->b_name; b++) {
			if (strncmp (b->b_name, cp, 2) == 0)
			break;
		}
		if (b->b_name == 0) {
			see_msg ("Default boot device not found.\n");
			return (0);
		}
		strncpy (dev, b->b_name, 2);
		lp++;
	}
	lp++;
	dev[2] = 0;
	lp = skipwhite (lp);
	if (*lp == 0 || *lp != '(')
		goto lookupdev;
	if (*++lp == ')') {
		lp++;
		goto lookupdev;
	}
	if ((tp = scann (lp, &n1, 10)) == 0)
		goto usage;
	lp = tp;
	si->si_args = SIO_SPECIFIED;
	if (*lp == ',') {
		if ((tp = scann (++lp, &n2, 10)) == 0)
			goto usage;
		lp = tp;
		if (*lp == ',') {
			if ((tp = scann (++lp, &si->si_part, 10)) == 0)
				goto usage;
			lp = tp;
			si->si_unit = n2;
			si->si_ctrl = n1;
		} else {
			si->si_part = n2;
			si->si_unit = n1;
		}
	} else {
		si->si_part = n1;
	}
	if (*lp == ')')
		lp++;
lookupdev:
	for (b = bootdev; b->b_name; b++) {
		if (strncmp (dev, b->b_name, 2) == 0)
		break;
	}
	if (b->b_name == 0)
		goto usage;
bootfound:
	si->si_dev = b->b_dev;
	lp = skipwhite (lp);

	/* set name of boot file -- can be overridden by boot routines */
	mg->mg_boot_dev = b->b_name;
	mg->mg_boot_info = mg->mg_bootfile;
	sprintf (mg->mg_boot_info, "(%d,%d,%d)",
		si->si_ctrl, si->si_unit, si->si_part);
	mg->mg_boot_file = mg->mg_bootfile + strlen (mg->mg_bootfile) + 1;
	*mg->mg_boot_file = 0;
	if (*lp && *lp != '-') {
		strncpy (mg->mg_boot_file, lp, NBOOTFILE);
		cp = mg->mg_boot_file;
		while (*cp && *cp != '-' && *cp != ' ')
			cp++;
		*cp = 0;
	}
	printf ("boot %s%s%s\n", mg->mg_boot_dev, mg->mg_boot_info, lp);
	save_lp = lp;
	last_e = -1;

	/* catch interrupts */
	if ((mg->km.flags & KMF_SEE_MSGS) == 0 &&
	    (*mg->mg_intrmask & I_BIT(I_VIDEO)) == 0) {
		struct scb *get_vbr(), *vbr;
		int anim_handler();

		mg->mg_animate = 0;
		mg->mg_flags |= MGF_ANIM_RUN;
		vbr = get_vbr();
		vbr->scb_ipl[3-1] = anim_handler;
		*mg->mg_intrmask |= I_BIT(I_VIDEO);
		vddp->dd_limit = (char*) RETRACE_LIMIT;
	} else
		mg->mg_flags &= ~MGF_ANIM_RUN;

	if ((mg->km.flags & KMF_SEE_MSGS) == 0) {
		drawCompressedPicture (LOGIN_X, LOGIN_Y, loginwindow);
		animate (b->b_boot);
		anim_run();	/* get first image on screen */
		anim_run();
	}
open:
	/* reset allocatable memory */
	mg->mg_alloc_brk = mg->mg_alloc_base;
	asm ("movw #0x2700,sr");
	e = b->b_proto->p_open (mg, si, 0, 0, 0);
	
	/*
	 *  Don't actually drop IPL and start animation until the boot device
	 *  open routine has a chance to clear device interrupts, etc.
	 */
	if ((mg->km.flags & KMF_SEE_MSGS) == 0) {
		asm ("movw #0x2200,sr");
		if (e != last_e) {
			last_e = e;

			switch (e) {

			case 0:
				animate (b->b_boot);
				break;

			case BE_FLIP:
				animate (b->b_flip);
				last_e = BE_INSERT;
				break;

			case BE_INSERT:
				animate (b->b_insert);
				break;

			case BE_NODEV:
#if	ODBOOT
				/* switch to Ethernet for diskless machines */

				if (b->b_dev == &optical_disk) {
					strcpy (mg->mg_boot_arg, "en");
					goto again;
				}
#endif	ODBOOT
				/* fall through ... */
				
			case BE_INIT:
			default:
				animate (b->b_bad);
#if	ODBOOT
				if (b->b_dev == &optical_disk)
					od_eject (si->si_unit, 0);
#endif	ODBOOT
				delay (3000000);
				last_e = -1;
				goto open;
			}
		}
		if (e) {
			delay (3000000);
			goto open;
		}
	} else
	if (e)
		return (0);
	lp = save_lp;
	entryaddr = b->b_proto->p_boot (mg, si, &lp, 0, 0);
	if (entryaddr == 0 && (mg->km.flags & KMF_SEE_MSGS) == 0) {
	
		/* if loading extended diagnostics fails, just boot kernel */
		if (strcmp (mg->mg_boot_file, "diagnostics") == 0) {
			strcpy (mg->mg_boot_arg, ni->ni_bootcmd);
			goto again;
		}
		animate (b->b_bad);
#if	ODBOOT
		if (b->b_dev == &optical_disk)
			od_eject (si->si_unit, 0);
#endif	ODBOOT
		delay (3000000);
		last_e = -1;
		goto open;
	}
	b->b_proto->p_close (mg, si);
	mg->mg_boot_arg = skipwhite (lp);
	return (entryaddr);
usage:
	see_msg ("Usage: b [device[(ctrl,unit,part)] [filename] [flags]]\n");
	printf ("boot devices:\n");
	for (b = bootdev; b->b_name; b++)
		printf ("\t%s: %s.\n", b->b_name, b->b_desc);
	return (0);
}

anim_retrace() {
	register struct mon_global *mg = restore_mg();
	struct dma_dev *vddp = (struct dma_dev*) P_VIDEO_CSR;

	/* have to catch SCSI interrupts on this same ipl */

	if (*mg->mg_intrstat & I_BIT(I_SCSI)) {
		 if (mg->mg_scsi_intr)
			(*mg->mg_scsi_intr) (mg->mg_scsi_intrarg);
		mg->mg_flags |= MGF_SCSI_INTR;
	}

	if (*mg->mg_intrstat & I_BIT(I_VIDEO)) {
		vddp->dd_csr = DMACMD_RESET;
		anim_run();
	}
}

anim_run() {
	register struct mon_global *mg = restore_mg();
	struct animation *ap;

	if ((mg->mg_flags & MGF_ANIM_RUN) &&
	    (ap = mg->mg_animate) && --mg->mg_anim_time <= 0) {
		if (ap->anim_time) {
			drawCompressedPicture (ap->x, VIDEO_H - ap->y, 
				ap->icon);
		} else {
			pprintf (ap->x, ap->y, 1, ap->icon);
		}
		mg->mg_anim_time = ap->anim_time;
		mg->mg_animate = &animation[ap->next];
	}
	km_power_check();
	return (mg->mg_flags & MGF_ANIM_RUN);
}

anim_intsetup() {
	asm (".globl _anim_handler");
	asm ("_anim_handler:");
	asm ("moveml #0xc0c0,sp@-");
	asm ("jsr _anim_retrace");
	asm ("movml sp@+,#0x0303");
	asm ("rte");
}

animate (an)
{
	register struct mon_global *mg = restore_mg();
	register struct animation *ap = &animation[an];

	if (an == -1)
		return;
	mg->mg_animate = 0;
	mg->mg_animate = ap;
	mg->mg_anim_time = 0;
	mg->mg_anim_run = &anim_run;
}




