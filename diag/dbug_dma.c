#include <sys/types.h>
#include <mon/monparam.h>
#include <mon/global.h>
#include <mon/sio.h>
#include <mon/nvram.h>
#include <assym.h>
#include <next/mmu.h>
#include <next/mem.h>
#include <next/psl.h>
#include <next/reg.h>
#include <next/cpu.h>
#include <next/trap.h>
#include <next/scr.h>
#include <next/scb.h>
#include <nextdev/dma.h>
#include <nextdev/monreg.h>
#include <nextdev/odreg.h>

#define	OMD_ECC_READ	0x80
#define	OMD_ECC_WRITE	0x40

#define	SOUND		1
#define	ESDI		2
#define	SCSI		4

#define	M_SOUND_S	(char*) 0x04010000
#define	M_SOUND_E	(char*) 0x04020000

#define	M_ESDI_SIZE	1296			/* read size */
#define	M_ESDI_S	(char*) 0x04000ff0
#define	M_ESDI_E	(char*) (0x04000ff0 + M_ESDI_SIZE)

dbug_dma (options)
{
	int i;
	int *intrmask, *intrstat;
	struct dma_dev *sound, *esdi, *scsi;
	struct od_regs *esdi_r;
	int intr_handler();
	struct scb *get_vbr(), *vbr;
	struct mon_global *mg = restore_mg();
	char line[4];

#if 1
	while (1) {
		if (mtest() == 0)
			printf ("@ 0x%x/0x%x was 0x%x sb 0x%x\n",
				*(int*)0x0400000c, *(int*)0x04000008,
				*(int*)0x04000004, *(int*)0x04000000);
		else
			printf (".");
	}
#endif
	intrmask = (int*) P_INTRMASK;
	intrstat = (int*) P_INTRSTAT;
	*intrmask = 0;		/* disable interrupts */
	vbr = get_vbr();
	vbr->scb_ipl[6-1] = intr_handler;
	asm ("movw #0x2500,sr");
	sound = (struct dma_dev*) P_SOUNDOUT_CSR;
	esdi = (struct dma_dev*) P_DISK_CSR;
	esdi_r = (struct od_regs*) P_DISK;
	scsi = (struct dma_dev*) P_SCSI_CSR;
#if 1
esdi_r->r_disr = 0xfc;
esdi_r->r_disr = 0xfc | OMD_SDFDA;
if (mtest() == 0)
	printf ("fail ");
#endif

	if (options & SOUND)
		printf ("sound ");
	if (options & ESDI)
		printf ("esdi ");
	if (options & SCSI)
		printf ("scsi ");
	printf ("\n");

	if (options & SOUND) {
		for (i=(int)M_SOUND_S; i<(int)M_SOUND_E; i+=2)
			*(short*)i = i & 0xff;		/* triangle wave */
		mon_send (MON_SNDOUT_CTRL(0x1), 0);	/* enable sound */
		mon_send (MON_GP_OUT, 0xff000000);	/* disable speaker */
		sound->dd_csr = DMACMD_RESET | DMACSR_INITBUF;
		sound->dd_next = M_SOUND_S;
		sound->dd_limit = M_SOUND_E;
		sound->dd_start = M_SOUND_S;
		sound->dd_stop = M_SOUND_E;
		sound->dd_csr = DMACMD_STARTCHAIN;
		*(int*)0x200e000 = 0x80000200;		/* enable sound DMA */
		*intrmask |= I_BIT (I_SOUND_OUT_DMA);
	}

	if (options & ESDI) {
		for (i=(int)M_ESDI_S; i<(int)M_ESDI_E; i+=4)
			*(int*)i = i;
		mg->mg_flags |= 8;

		printf ("w"); /* */
		esdi_r->r_initr = OMD_25_MHZ;
		esdi_r->r_cntrl2 = OMD_ECC_DIS;
		esdi->dd_csr = DMACMD_RESET | DMACSR_INITBUF;
		esdi->dd_next = M_ESDI_S;
		esdi->dd_limit = (char*) (M_ESDI_S + 1024);
		/* *(volatile char*)0x2012004 = 2; */
		esdi->dd_csr = DMACMD_START;
		esdi_r->r_disr = 0xfc;
		esdi_r->r_cntrl1 = OMD_ECC_WRITE;
		for (i=0; i<100; i++)
			;
		/* *(volatile char*)0x2012004 = 0; */

		*intrmask |= I_BIT (I_DISK_DMA);
	}

	if (options & SCSI) {
#if 0
		if (diskopen (mg, si) < 0)
			panic ("diskopen");
		if (disk_read (mg, si, b, cc) != cc)
			panic ("disk_read");
#endif
	}

	while (1) {
#if 0
		/* unaligned memory access */
		volatile int foo;
		
		*(volatile int*)0x04100000 = 0xa5a5a5a5;
		*(volatile int*)0x04100004 = 0xa5a5a5a5;
		foo = 0;
	asm ("movw #0x2700,sr");
		esdi_r->r_disr = 0xfc | OMD_SDFDA;
		*(volatile int*)0x04100002 = 0x12345678;
		foo = *(volatile int*)0x04100002;
		esdi_r->r_disr = 0xfc;
	asm ("movw #0x2500,sr");
		if (foo != 0x12345678)
			printf ("foo = 0x%x ", foo);
#else
#if 1
		/* burst access */
		if (mtest() == 0)
			printf ("@ 0x%x/0x%x was 0x%x sb 0x%x\n",
				*(int*)0x0400000c, *(int*)0x04000008,
				*(int*)0x04000004, *(int*)0x04000000);
#else
		/* TBG access */
		volatile int foo;
		
	asm ("movw #0x2700,sr");
		esdi_r->r_disr = 0xfc | OMD_SDFDA;
		*(volatile int*)0x04000000 = 0x1b1b1b1b;
		*(volatile int*)0x14000000 = 0x1b6cb1c6;
		foo = *(volatile int*)0x04000000;
		esdi_r->r_disr = 0xfc;
	asm ("movw #0x2500,sr");
		if (foo != 0x2f7fbbdf)
			printf ("foo = 0x%x ", foo);
#endif
#endif
	}
}

intr()
{
	int *intrmask, *intrstat;
	struct dma_dev *sound, *esdi, *scsi;
	struct od_regs *esdi_r;
	struct mon_global *mg = restore_mg();

	intrmask = (int*) P_INTRMASK;
	intrstat = (int*) P_INTRSTAT;
	sound = (struct dma_dev*) P_SOUNDOUT_CSR;
	esdi = (struct dma_dev*) P_DISK_CSR;
	esdi_r = (struct od_regs*) P_DISK;
	scsi = (struct dma_dev*) P_SCSI_CSR;

	if (*intrstat & I_BIT (I_SOUND_OUT_DMA)) {
		if (sound->dd_csr & DMACSR_ENABLE) {
			sound->dd_start = M_SOUND_S;
			sound->dd_stop = M_SOUND_E;
			sound->dd_csr = DMACMD_CHAIN;
		} else {
			sound->dd_csr = DMACMD_RESET | DMACSR_INITBUF;
			sound->dd_next = M_SOUND_S;
			sound->dd_limit = M_SOUND_E;
			sound->dd_start = M_SOUND_S;
			sound->dd_stop = M_SOUND_E;
			sound->dd_csr = DMACMD_STARTCHAIN;
			*(int*)0x200e000 = 0xa0000200;
		}
	}

	if (*intrstat & I_BIT (I_DISK_DMA)) {
		volatile int d, i, a, o, A, D;
		char line[8];

#if 0
		i = 0;
		while (esdi_r->r_cntrl1) {
			if (*intrstat & I_BIT (I_SOUND_OUT_DMA)) {
				sound->dd_start = M_SOUND_S;
				sound->dd_stop = M_SOUND_E;
				sound->dd_csr = DMACMD_CHAIN;
			}
			if (i == 32) {
#if 1
				printf ("R"); /* */
				esdi_r->r_initr = OMD_25_MHZ;
				esdi_r->r_cntrl2 = OMD_ECC_DIS;
				esdi->dd_csr = DMACMD_RESET | DMACSR_INITBUF;
				esdi->dd_next = M_ESDI_S;
				esdi->dd_limit = (char*) (M_ESDI_S + 16);
				esdi->dd_csr = DMACMD_START;
				/* printf ("r2 0x%x ", esdi_r->r_cntrl1); */
				/* esdi_r->r_disr = 0xfc; */
				/* esdi_r->r_cntrl1 = OMD_ECC_WRITE; */
				return;
#else
/*				printf ("R"); /* */
				esdi_r->r_cntrl1 = 0;
#endif
			}
			i++;
		}
#endif

		/* toggle r/w flag */
		if (mg->mg_flags & 8)
			mg->mg_flags &= ~8;
		else
			mg->mg_flags |= 8;
		if (mg->mg_flags & 8) {
#if 0
			/* check previous read */
			d = (int)M_ESDI_S;
			o = 0;
			for (i = 0; i < 256; i++) {
				a = (int)M_ESDI_S + i*4;
				if ((i % 9) == 8)
					continue;
				if (*(int*)a != d && o < 8*4) {
/*				printf ("0x%x: was 0x%x sb 0x%x @ 0x%x\n",
						o, *(int*)a, d, a); /* */
					o += 4;
					if (o == 4) {
						D = d;  A = a;
					}
				}
				d += 4;
			}
			if (o == 4)
				printf ("SL 0x%x: was 0x%x sb 0x%x @ 0x%x\n",
						o, *(int*)A, D, A); /* */
#endif
			/* restore pattern before write */
			for (i=(int)M_ESDI_S; i<(int)M_ESDI_E; i+=4)
				*(int*)i = i;

			/* write */
#if 0
			for (i=0; i<800000; i++)
				if (*intrstat & I_BIT (I_SOUND_OUT_DMA)) {
					sound->dd_start = M_SOUND_S;
					sound->dd_stop = M_SOUND_E;
					sound->dd_csr = DMACMD_CHAIN;
				}
			printf ("w");
#endif

			esdi_r->r_initr = OMD_25_MHZ;
			esdi_r->r_cntrl2 = OMD_ECC_DIS;
			esdi->dd_csr = DMACMD_RESET | DMACSR_INITBUF;
			esdi->dd_next = M_ESDI_S;
			esdi->dd_limit = (char*) (M_ESDI_S + 1024);
			esdi->dd_csr = DMACMD_START;
			/*esdi_r->r_disr = 0xfc;*/
			esdi_r->r_cntrl1 = OMD_ECC_WRITE;
		} else {
			/* read */
#if 0
			for (i=0; i<800000; i++)
				if (*intrstat & I_BIT (I_SOUND_OUT_DMA)) {
					sound->dd_start = M_SOUND_S;
					sound->dd_stop = M_SOUND_E;
					sound->dd_csr = DMACMD_CHAIN;
				}
			printf ("r");
#endif
#if 1
delay(100);
#endif
			esdi_r->r_initr = OMD_25_MHZ;
			esdi_r->r_cntrl2 = OMD_ECC_DIS;
			esdi->dd_csr = DMACMD_RESET | DMACSR_INITBUF |
				DMACSR_READ;
			esdi->dd_next = M_ESDI_S;
			esdi->dd_limit = M_ESDI_E;
			esdi->dd_csr = DMACMD_START | DMACSR_READ;
			/*esdi_r->r_disr = 0xfc;*/
			esdi_r->r_cntrl1 = OMD_ECC_READ;
			if (esdi->dd_next == esdi->dd_limit)
				printf ("!");
		}
	}
}

intsetup()
{
	asm (".globl _intr_handler");
	asm ("_intr_handler:");
	asm ("moveml #0xc0c0,sp@-");
	asm ("jsr _intr");
	asm ("moveml sp@+,#0x0303");
	asm ("rte");
}

panic (s)
{
	printf ("%s\n", s);
	while (1)
		;
}
