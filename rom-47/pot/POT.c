/* @(#)POT.c	1.0	04/01/88	(c) 1988 NeXT Inc.	 */

/*
 * POT.c -- Power on self test routines, except for memory tests. ROM VERSION 
 */

/*
 *  HISTORY
 *  19-Oct-90  Bassanio Law (blaw) at NeXT
 *	Put enet transceiver in external mode once to clear
 *	a weird and intermitten NetBusy condition.  This is done
 *	at the begining of enet selftest.
 *  30-Apr-90  Bassanio Law (blaw) at NeXT
 *	Fixed a bug in eventcounter test that did not handle
 *	   rollover correctly.
 *	It is possible (very rare) for a bad bank of SIMM to be misclaisified 
 *	    as empty in set_SCR2_memType.  The SIMM must failed in a certain 
 *	    mode such that it passes burst_mode_ok check but fails 
 *	    get_page_mode_size.  This problem is fixed. 
*  26-Feb-90  John Seamons (jks) at NeXT
 *	Changed MODriveTest() to fix bug noticed by blaw.
 *   9-Sep-89  John Seamons (jks) at NeXT
 *	#ifdef'd out TBG test due to lack of ROM space.
 *  19-Jun-89  Bassanio (blaw) at NeXT
 *	Fixed and cleaned up codes for 1.0 ROM release
 *	Added FPU and TBG tests.
 *	Fixed timers and delay routines
 *	Rewrite memory test to fix bug and run faster
 *	Rewrite code to determine SIMM's type and size
 *	Check mixed mode SIMM
 *	Compress error codes from 4 to 1 byte
 *	Save last two error codes in nvRAM
 *	Changed SIMM counting start from 0 to be consistent with jks
 *  09-Feb-89  Bassanio (blaw) at NeXT
 *	Changed NVRAMTest to test all 32 bytes for all patterns.
 *	Also not to have legal checksum during testing in case
 *	..of power failure (it actually happens in production process).
 *	Fixed "printSIMMNumber" to use "value' from argument list instead of
 *	..hardwired "0xaa".
 *	Corrected "printSIMMNumber" to compute the proper bad SIMM number.
 *	Fixed 70005 (ethernet error) problem per J. Wiseman.
 *	The bug was caused by reading a non-readable hardware register.
 *
 *  11-August-88 Joe Becker (jbecker) at NeXT Changes to support DMA
 *	chip 313 and sound out underrun. 
 *
 *  1-Apr-88  Joe Becker (jbecker) at NeXT
 *	Created. 
 */

#include <pot/POT.h>
#include <pot/expand.h>
#include <mon/monparam.h>
#include <next/cpu.h>
#include <next/mmu.h>
#include <next/bmap.h>
#include <next/trap.h>
#include <next/mem.h>
#include <nextdev/dma.h>
#include <nextdev/fd_reg.h>

#define SCR2_DRAM_MASK 0x00110000L
#define SCR2_DRAM_4M 0x00000000L
#define SCR2_DRAM_256K 0x00100000L
#define	TP1	0x12345678
#define	TP2	0x89abcdef
#define	TP3	0xabcdef01

#define CONST_16MB 0x1000000
#define CONST_4MB 0x400000
#define CONST_1MB 0x100000
#define	CACR_WA		0x2000	/* cache write allocate policy */
#define	CACR_DBE	0x1000	/* d-cache burst enable */
#define	CACR_CD		0x0800	/* clear data cache */
#define	CACR_CED	0x0400	/* clear data entry */
#define	CACR_FD		0x0200	/* freeze data cache */
#define	CACR_ED		0x0100	/* enable data cache */
#define	CACR_IBE	0x0010	/* i-cache burst enable */
#define	CACR_CI		0x0008	/* clear instruction cache */
#define	CACR_CEI	0x0004	/* clear instruction entry */
#define	CACR_FI		0x0002	/* freeze instruction cache */
#define	CACR_EI		0x0001	/* enable instruction cache */

/* This routine is defined to save ROM space */
/* There are many references to this routine */
/* It saves close to 200 bytes with 22 references */
static void delay10()
{
                delay(9);  /* compensate for the extra overhead */
    return;
}

static void printSIMMNumber(u_int adr, u_char value, u_char * data, char type)
{
    int         cnt, bank = ((adr >> 24) & 0x0f) - 4;
    tprintf("\nDRAM error type %d\n", type);
    tprintf("Check socket (0 is the first socket): ");
/****
    socket's are oganized as:
    socket 0 ---> bit 0 to 7
    socket 1 ---> bit 8 - 15
    socket 2 ---> bit 16 - 23
    socket 3 ---> bit 24 - 31
      
    Memory is organized as:
    byte 0 ---> bit 24 -31
    byte 1 ---> bit 16 - 23
    byte 2 ---> bit 8 - 15
    byte 3 ---> bit 0 to 7
****/
    for (cnt = 0; cnt < 4; cnt++) {
	if (data[cnt] != value)
	    tprintf("%d ", ((4 * bank) + 3 - cnt));
    }
    tprintf("\n\n");
    return;
}


static void print_memory_test_result(volatile u_int * adr)
{
    volatile u_int result;
    if (!adr)
	return;

    tprintf("\nMemory error at location: %x\n", adr);
    tprintf("Value at time of failure: %x\n", *adr);

    *adr = 0x55555555;
    *++adr = 0xaaaaaaaa;
    result = *--adr;
    if (result != 0x55555555) {
	printSIMMNumber((u_int) adr, 0x55, (u_char *) & result, -1);
	return;
    }
    *adr = 0xaaaaaaaa;
    *++adr = 0x55555555;
    result = *--adr;
    if (result != 0xaaaaaaaa) {
	printSIMMNumber((u_int) adr, 0xaa, (u_char *) & result, -2);
	return;
    }
    tprintf("Coupling dependent memory fault!\n");
    tprintf("One or more SIMM at memory bank %d is bad\n",
	    (((int)adr >> 24) & 0x0f) - 4);
    tprintf("Note: bank 0 is the first bank\n");

    return;
}

#if 0			   /* Use only for debugging */
static void serialPrint(char portID, char *stringPtr)
/* Simple routine to print debug messages out through a serial port */

{

    char       *portPtr = (portID == 'a') ? SCC_PORT_A : SCC_PORT_B;
    char        data;
    int         cnt;
    data = *portPtr;	   /* make sure SCC is looking for a command */
    do {
	do
	    *portPtr = 0;  /* check status register */
	while (!(*portPtr & 4));	/* wait for Tx buffer empty */
	*(portPtr + 2) = *stringPtr++;	/* send next character */
    } while (*stringPtr);
    return;
}
#endif


static void intrSetUp()
{			   /* create some asm for intrs */
                asm(".globl _SCSIIntrHandler");
    asm("_SCSIIntrHandler:");
    asm("moveml	#0x7ffe,sp@-");
    asm("jsr	_SCSIIntr");
    asm("moveml	sp@+,#0x7ffe");
    asm("rte");

    asm(".globl _SCCIntrHandler");
    asm("_SCCIntrHandler:");
    asm("moveml	#0x7ffe,sp@-");
    asm("jsr	_SCCIntr");
    asm("moveml	sp@+,#0x7ffe");
    asm("rte");

    asm(".globl _SystemTimerIntrHandler");
    asm("_SystemTimerIntrHandler:");
    asm("moveml	#0x7ffe,sp@-");
    asm("jsr	_SystemTimerIntr");
    asm("moveml	sp@+,#0x7ffe");
    asm("rte");

    asm(".globl _SoundOutIntrHandler");
    asm("_SoundOutIntrHandler:");
    asm("moveml	#0x7ffe,sp@-");
    asm("jsr	_SoundOutIntr");
    asm("moveml	sp@+,#0x7ffe");
    asm("rte");

    asm(".globl _SoundOutOverRunIntrHandler");
    asm("_SoundOutOverRunIntrHandler:");
    asm("moveml	#0x7ffe,sp@-");
    asm("jsr	_SoundOutOverRunIntr");
    asm("moveml	sp@+,#0x7ffe");
    asm("rte");
}

static long FPUTest()
{
    if (_fpu_reg())
        return (1);
    if (_fpu_mov())
	return (2);
    if (_fpu_ops())
	return (3);
    return (0);
}


static void SCCIntr()
{			   /* interrupt handler for SCC DMA */

    /* FIXME: Need to do something better here? ... */
    *(u_int*) P_SCC_CSR = DMACSR_RESET | DMACSR_INITBUF | DMACSR_WRITE;
}



static long SCCTest()
/*
 * This test sets the SCC chip up for LOCAL LOOPBACK, 9600, no parity, 1 stop
 * bit, and checks if a character can be written and read back. 
 *
 * Remember that "internal" loop back prints out the test character. 
 *
 */

{
/*
 * following array is a list of SCC reg id, and parm pairs. Register 4 is
 * loaded with 0x44, then reg 11 with 0x50... 
 */

    static char initializationSequence[] =
    {4, 0x44, 11, 0x50, 14, 0x10, 12, 0x0a,
     13, 0x00, 14, 0x11, 10, 0x00, 3, 0xc1,
     5, 0xea, 15, 0x00};

    register char *seq = initializationSequence;
    volatile register struct scc_dev *scc = (struct scc_dev *) P_SCC;
    register char cnt, data;

    int         SCCIntrHandler();

    struct mon_global *mg = restore_mg();
    struct scc_tc *sc;
    struct dma_buf *bp;
    volatile struct dma_channel *dma;
    struct scb *get_vbr(), *vbr;
    u_char *nb;
    int         (*old_interrupt_handler) (), old_interrupt_mask;
    int         dmacsr, i;
/* Use DMA to check port A */

    sc = (struct scc_tc *) mon_alloc(sizeof(struct scc_tc));
    sc->dev_regs = (struct scc_dev *) P_SCC;	/* point to SCC chip regs */
    sc->dmacsr = (u_int *)P_SCC_CSR;	/* ptr to DMA csr */
    sc->dma_regs = (struct dma_channel *) (P_SCC_CSR + 0x4000);	/* ptr to DMA regs */


/* force a hardware reset with port A, reg 9 */

    data = scc->a_reg0;	   /* make sure SCC is looking for a command */
    delay10();
    scc->a_reg0 = 9;	   /* use port A, reg 9 to reset chip */
    delay10();
    scc->a_reg0 = 0xca;	   /* reset ports A and B, no interrupt vector */


/* down load the SCC port A control registers */

    for (cnt = 0; cnt < sizeof(initializationSequence); cnt++) {
	delay10();
	scc->a_reg0 = *seq++;
    }


/* down load the SCC port B control registers */

    delay10();
    data = scc->b_reg0;	   /* is SCC looking for cmd? */
    seq = initializationSequence;
    for (cnt = 0; cnt < sizeof(initializationSequence); cnt++) {
	delay10();
	scc->b_reg0 = *seq++;
    }


/* Enable SCC DMA interrupts */

    vbr = get_vbr();
    old_interrupt_handler = vbr->scb_ipl[6 - 1];
    old_interrupt_mask = *mg->mg_intrmask;
    vbr->scb_ipl[6 - 1] = SCCIntrHandler;
/* FIXME: use the "official name for the bits in the intrmask */
    *mg->mg_intrmask = 0x00200000;
    asm("movw #0x2500,sr");


/*
 * Keep in mind that the SCC supports xmit OR recv DMA, but not both 
 */


/* Set up the SCC port A for transmit DMA with W/REQ */

    *(sc->dmacsr) = DMACSR_RESET | DMACSR_INITBUF | DMACSR_WRITE;	/* set up DMA csr */
    *(sc->dmacsr) = 0;
    *(sc->dmacsr) = DMACSR_WRITE;	/* set dir for mem to dev */

    nb = (u_char *)sc->tdata;
    *nb = SCC_TEST_CHAR;

    dma = sc->dma_regs;	   /* make code cleaner?!? */
    dma->next = nb;
    dma->limit = ++nb;

    data = scc->a_reg0;	   /* is SCC looking for cmd? */
    delay10();
    scc->a_reg0 = 1;	   /* port A, reg 1 */
    delay10();
    scc->a_reg0 = 0x40;	   /* want request for transmit DMA */
    *(sc->dmacsr) = DMACSR_WRITE | DMACSR_SETENABLE;

/* Transmit a char */

    delay10();
    data = scc->a_reg0;	   /* is SCC looking for cmd? */
    delay10();
    scc->a_reg0 = 1;	   /* port A, reg 1 */
    delay10();
    scc->a_reg0 = 0xc0;	   /* enable transmit DMA */

/* Wait for the transfer to complete */

    delay(100000);	   /* is it going to happen */
    if (*(sc->dmacsr) & DMACSR_ENABLE)
	return (1);

/* Check if we recieved the correct character */

    i = 0;
    do {
	delay10();
	scc->a_reg0 = 0;   /* check status register */
	delay10();
    } while (!(scc->a_reg0 & 1) && (i++ < 5000));	/* wait for char */
    if (i >= 5000)	   /* (20us * 5000) + overhead > 0.1 second */
	return (2);

    if (scc->a_data != SCC_TEST_CHAR)	/* char doesn't match */
	return (3);

    data = scc->a_reg0;	   /* is SCC looking for cmd? */
    delay10();
    scc->a_reg0 = 1;	   /* port A, reg 1 */
    delay10();
    scc->a_reg0 = 0x00;	   /* disable DMA */

/* Clean up the interrupt system */

    asm("movw #0x2700,sr");/* turn off interrupt */
    vbr->scb_ipl[6 - 1] = old_interrupt_handler;	/* restore handler */
    *mg->mg_intrmask = old_interrupt_mask;	/* restore mask */

    *(sc->dmacsr) = DMACSR_RESET | DMACSR_INITBUF | DMACSR_WRITE;	/* turn off DMA */

    delay10();
    data = scc->a_reg0;	   /* is SCC looking for cmd? */
    delay10();
    scc->a_reg0 = 1;	   /* port A, reg 1 */
    delay10();
    scc->a_reg0 = 0x00;	   /* disable DMA */


/* Check port B */

    delay10();
    data = scc->b_reg0;	   /* make sure SCC wants a cmd */

    i = 0;
    do {
	delay10();
	scc->b_reg0 = 0;   /* check status register */
	delay10();
    } while (!(scc->b_reg0 & 4) && (i++ < 5000));	/* wait for Tx buffer
							 * empty */
    if (i >= 5000)	   /* (20us * 5000) + overhead > 0.1 second */
	return (5);
    scc->b_data = SCC_TEST_CHAR;	/* send test character */

    i = 0;
    do {
	delay10();
	scc->b_reg0 = 0;   /* check status register */
	delay10();
    } while (!(scc->b_reg0 & 1) && (i++ < 5000));	/* wait for char */
    if (i >= 5000)	   /* (20us * 5000) + overhead > 0.1 second */
	return (6);

    if (scc->b_data != SCC_TEST_CHAR)	/* char doesn't match */
	return (7);


/* both ports seem to work. */

    return (0);
}



static void SCSIIntr()
{
                tprintf("SCSI DMA intr?\n");
}



static long SCSITest()
/*
 * Checks if the 53c90 chip can be addressed, and if it can pass the suite of
 * self tests proposed by the NCR "53c90 Users Guide". 
 */

{
    volatile struct scsi_5390_regs *s5rp = (struct scsi_5390_regs *) P_SCSI;
    char        reg;
    int         counter;
    long        cnt;
#define S5RDMAC_NOINTRBITS	S5RDMAC_20MHZ

    ((struct fd_cntrl_regs*)P_FLOPPY)->flpctl &= ~FLC_82077_SEL;

/* reset the 53c90 */

    s5rp->s5r_dmactrl = S5RDMAC_NOINTRBITS | S5RDMAC_RESET;
    delay(10);
    s5rp->s5r_dmactrl = S5RDMAC_NOINTRBITS;
    delay(10);
    s5rp->s5r_cmd = S5RCMD_RESETCHIP;
    s5rp->s5r_cmd = S5RCMD_NOP;


/* check if the fifo works */

    s5rp->s5r_fifo = 0;
    s5rp->s5r_fifo = 1;
    s5rp->s5r_fifo = 2;
    s5rp->s5r_fifo = 3;
    s5rp->s5r_fifo = 4;
    reg = s5rp->s5r_fifoflags & S5RFLG_FIFOLEVELMASK;
    if (reg != 5)
	return (1);	   /* check if 5 bytes made it to chip */

    reg = s5rp->s5r_fifo;
    if (reg != 0)
	return (2);	   /* make sure we get the correct data */
    reg = s5rp->s5r_fifo;
    if (reg != 1)
	return (2);	   /* ditto.. */

    reg = s5rp->s5r_fifoflags & S5RFLG_FIFOLEVELMASK;
    if (reg != 3)
	return (3);	   /* check if 3 bytes are still in FIFO */
    return (0);


}



static long ExtendedSCSITest()
/*
 * Checks if the 53c90 chip can be addressed, and if it can pass the suite of
 * self tests proposed by the NCR "53c90 Users Guide". 
 */

{
    volatile struct scsi_5390_regs *s5rp = (struct scsi_5390_regs *) P_SCSI;
    char        reg;
    int         counter;
    long        cnt;
#define S5RDMAC_NOINTRBITS	S5RDMAC_20MHZ

/* reset the 53c90 */

    s5rp->s5r_dmactrl = S5RDMAC_NOINTRBITS | S5RDMAC_RESET;
    delay(10);
    s5rp->s5r_dmactrl = S5RDMAC_NOINTRBITS;
    delay(10);
    s5rp->s5r_cmd = S5RCMD_RESETCHIP;
    s5rp->s5r_cmd = S5RCMD_NOP;
/*
 * FIXME: Chip's cmd reg doesn't work if SCSI bus is unterminated, so test
 * will fail on early rev boards that don't have the SCSI bus terminated... 
 */

/* FIXME: Should check if the SCSI DMA channel is working */
/* to see if cmd reg works, check if flush FIFO cmd works */
/*
 * FIXME: 53c90 will not accept commands if the bus is unterminated 
 */
    s5rp->s5r_cmd = S5RCMD_FLUSHFIFO;
    delay(10);
    reg = s5rp->s5r_fifoflags & S5RFLG_FIFOLEVELMASK;
    if (reg != 0)
	return (4);


/* check if the counter works */

    s5rp->s5r_cntlsb = (char)0x55;
    s5rp->s5r_cntmsb = (char)0x55;
    s5rp->s5r_cmd = S5RCMD_ENABLEDMA | S5RCMD_NOP;	/* move data to cntr */
    counter = s5rp->s5r_cntmsb << 8 | s5rp->s5r_cntlsb;
    if (counter != 0x5555)
	return (5);

    s5rp->s5r_cntlsb = (char)0xaa;
    s5rp->s5r_cntmsb = (char)0xaa;
    s5rp->s5r_cmd = S5RCMD_ENABLEDMA | S5RCMD_NOP;	/* move data to cntr */
    counter = s5rp->s5r_cntmsb << 8 | s5rp->s5r_cntlsb;
    if (counter != 0xaaaa)
	return (6);


/* check if the configuration register works */

    for (cnt = 0; cnt <= 255; cnt++) {	/* go wild! check every value... */
	s5rp->s5r_config = cnt;
	if (s5rp->s5r_config != cnt)
	    return (7);
    }
    s5rp->s5r_config = 0;  /* return to a wholesome configuration */


/* check if the "interrupts" work */
/*
 * FIXME: Really ought to enable an interrupt and field it. Right now only
 * the interrupt register is checked... 
 */

    reg = s5rp->s5r_intrstatus;	/* clear left over interrupts */
    s5rp->s5r_cmd = 0xff;  /* give an illegal command */
    delay(10);
    reg = s5rp->s5r_intrstatus;
    if (~reg & S5RINT_ILLEGALCMD)
	return (8);	   /* illegal bit wasn't on */


/* chip's ok, clean up it's state */

    s5rp->s5r_cmd = S5RCMD_RESETCHIP;
    s5rp->s5r_cmd = S5RCMD_NOP;

    return (0);
}



static long EtherNetTest()
/*
 * Checks if the board can transmit and receive, (in loopback mode), a
 * physical and broadcast packet and reject a physical packet with the
 * incorrect address. 
 */

{
    caddr_t     mon_alloc();

    extern struct device enet_en;
    u_char etherbcastaddr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

    struct sio  sio;
    struct tftp_softc *ts;

    struct sio *si = &sio;

    u_char     *etheradr;
    static u_char destadr[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    int         len, packetSize;
    long        cnt;
    caddr_t     p, packet, testPacket;
    volatile struct bmap_chip *bm = (volatile struct bmap_chip*) P_BMAP;

#define	MAX_PACKET_SIZE	1500
#define POLL_CNT_LIMIT	1000

    bzero(si, sizeof(*si));/* zero the SIO struct */
    si->si_unit = 0x00;	   /* set debug level */
    si->si_args = SIO_ANY; /* Set type of SIO desired */
    si->si_part = 0;
    si->si_dev = &enet_en; /* Want to use the enet driver */
    si->si_protomem = mon_alloc(sizeof(struct tftp_softc));	/* Alloc bufs */
    ts = (struct tftp_softc *) si->si_protomem;
    bcopy(etherbcastaddr, ts->ts_serveretheraddr, 6);	/* Listen for bcasts */

    etheradr = (u_char *) en_open(si);	/* Should return our enet adr */
    if (!etheradr)
	return (1);	   /* 0!?!, must not have worked */
    bcopy(etheradr, ts->ts_clientetheraddr, 6);	/* set our physical adr */

    packetSize = MAX_PACKET_SIZE;
    p = packet = (caddr_t) mon_alloc(packetSize);
    bzero(packet, packetSize);

/* enter loopback mode */

/*
 * it is necessary to first set it in external mode to avoid a weird
 * Netbusy condition 
 */
     
    ((volatile struct en_regs *) P_ENET)->en_txmode = EN_TXMODE_NO_LBC;
    delay10(); /* wait just in case */
    ((volatile struct en_regs *) P_ENET)->en_txmode = 0;

#if	MC68040
    bm->bm_ddir = BMAP_TPE;
    bm->bm_drw = ~BMAP_TPE;
#endif	MC68040

/* clear rx_buf of leftovers */

    for (cnt = 0; cnt < POLL_CNT_LIMIT; cnt++)
	if (len = en_poll(si, packet, ts, packetSize))
	    cnt = 0;

    if (len) {		   /* couldn't drain rx_buf */
	en_close(si);	   /* all done with enet */
	return (2);
    }
/* send ourselves a packet */

    p = testPacket = (caddr_t) mon_alloc(packetSize);
    bcopy(etheradr, p, 6);
    p += 6;		   /* set dest = ourselves */
    bcopy(etheradr, p, 6);
    p += 6;		   /* "" for source */
    *p++ = 0x55;
    *p++ = 0xaa;	   /* set type to 0x55aa */
    for (cnt = 0; cnt < packetSize - 6 - 6 - 2; cnt++)
	*p++ = cnt;	   /* "message" */

    en_write(si, testPacket, packetSize, 0);

/* did our message get received? */

    packet = (caddr_t) mon_alloc(packetSize);
    for (cnt = 0; cnt < POLL_CNT_LIMIT; cnt++)
	if (len = en_poll(si, packet, ts, packetSize))
	    break;

    if (cnt >= POLL_CNT_LIMIT) {	/* didn't receive the packet */
	en_close(si);	   /* all done with enet */
	return (3);
    }
    if (bcmp(packet, testPacket, packetSize)) {	/* packet was garbled */
	en_close(si);	   /* all done with enet */
	return (4);
    }
/* hack our address and see if packet is received */

    ((struct en_regs *) P_ENET)->en_enetaddr[5] = etheradr[5] + 1;
    en_write(si, testPacket, packetSize, 0);
    bzero(packet, packetSize);
    for (cnt = 0; cnt < POLL_CNT_LIMIT; cnt++)
	if (len = en_poll(si, packet, ts, packetSize))
	    break;

    if (cnt != POLL_CNT_LIMIT) {	/* received a packet in error */
	en_close(si);	   /* all done with enet */
	return (5);
    }
/* use bcast address and see if packet is received */

    for (cnt = 0; cnt < 6; testPacket[cnt++] = 0xff);
    en_write(si, testPacket, packetSize, 0);
    bzero(packet, packetSize);
    for (cnt = 0; cnt < POLL_CNT_LIMIT; cnt++)
	if (len = en_poll(si, packet, ts, packetSize))
	    break;

    if (cnt >= POLL_CNT_LIMIT) {	/* didn't receive bcast pkt */
	en_close(si);	   /* all done with enet */
	return (6);
    }
    if (bcmp(packet, testPacket, packetSize)) {	/* bcast pkt was garbled */
	en_close(si);	   /* all done with enet */
	return (7);
    }
/* exit loopback mode */

    ((volatile struct en_regs *) P_ENET)->en_txmode = EN_TXMODE_NO_LBC;
    en_close(si);	   /* all done with enet */
    return (0);
}



static long MODriveTest()
{
    u_char     *input_block, *coded_block, *result_block;
    volatile struct disk_tc *sc;
    volatile struct disk_dev *disk;
    volatile struct dma_channel *dma;

    u_char      temp, *ptr;
    int         cnt;
/* Use DMA to check MO Disk ECC */

    sc = (struct disk_tc *) mon_alloc(sizeof(struct disk_tc));
    sc->dev_regs = (struct disk_dev *) P_DISK;	/* point to DISK chip regs */
    sc->dmacsr = (u_int *)P_DISK_CSR;	/* ptr to DMA csr */
    sc->dma_regs = (struct dma_channel *) (P_DISK_CSR + 0x4000);	/* ptr to DMA regs */

    disk = sc->dev_regs;
    dma = sc->dma_regs;

/* allocate a buffer of data to be encoded */

    input_block = (u_char *) mon_alloc(UNCODED_DISK_BLOCK_SIZE + CHIP_SLOP);
    coded_block = (u_char *) mon_alloc(CODED_DISK_BLOCK_SIZE + CHIP_SLOP);
    result_block = (u_char *) mon_alloc(UNCODED_DISK_BLOCK_SIZE + CHIP_SLOP);

/* initialize the buffers */

    input_block = (u_char *) (((int)input_block & ~0xf) + 0x10);
    coded_block = (u_char *) (((int)coded_block & ~0xf) + 0x10);
    result_block = (u_char *) (((int)result_block & ~0xf) + 0x10);

    for (cnt = 0; cnt < UNCODED_DISK_BLOCK_SIZE; input_block[cnt++] = cnt);
    bzero(coded_block, CODED_DISK_BLOCK_SIZE);
    bzero(result_block, UNCODED_DISK_BLOCK_SIZE);

/* Set up the disk chip */

    disk->flag_strategy[0] = 0x35;
    disk->flag_strategy[1] = 0;
    disk->flag_strategy[2] = 0;
    disk->flag_strategy[3] = 0x33;
    disk->flag_strategy[4] = 0x44;
    disk->flag_strategy[5] = 0x35;
    disk->flag_strategy[6] = 0x33;

    disk->control_2 = 0;   /* init the control reg */
    disk->interrupt_status = DISK_STATUS_CLEAR;	/* clear the status reg */
    disk->interrupt_mask = 0x00;	/* punt interrupts */
    disk->control_2 = DISK_ECC_SELECT | DISK_ECC_ENCODE;

/* Set up the DMA */

    *(sc->dmacsr) = DMACSR_RESET | DMACSR_INITBUF | DMACSR_WRITE;	/* set DMA csr up */
    *(sc->dmacsr) = 0;
    *(sc->dmacsr) = DMACSR_WRITE;	/* set dir for mem to dev */
    dma->next = input_block;
    dma->limit = input_block + UNCODED_DISK_BLOCK_SIZE;

/* Transfer the data */
/* Note that it is necessary to enable ECC prior to DMA here */
/* This has the effect of flushing the internal DMA bus which might have garbage */
/* ..remained during power up */
    disk->control_1 = DISK_ECC_WRITE;
    *(sc->dmacsr) = DMACSR_WRITE | DMACSR_SETENABLE;
    cnt = 0;
    while (!(disk->interrupt_status & 0x08)) {
	delay10();
	if (++cnt > 200000)
	    return (1);
    }

/* Use DMA to read the encoded data back */

    *(sc->dmacsr) = DMACSR_RESET | DMACSR_INITBUF | DMACSR_READ;	/* set the DMA csr up */
    *(sc->dmacsr) = 0;
    *(sc->dmacsr) = DMACSR_READ;	/* set dir for dev to mem */
    dma->next = coded_block;
    dma->limit = coded_block + CODED_DISK_BLOCK_SIZE;
    *(sc->dmacsr) = DMACSR_READ | DMACSR_SETENABLE;

    disk->interrupt_status = DISK_STATUS_CLEAR;	/* clear the status reg */
    disk->control_1 = DISK_ECC_READ;
    cnt = 0;
    while (!(disk->interrupt_status & 0x08)) {
	delay10();
	if (++cnt > 200000)
	    return (2);
    }


/* Mangle encoded data */

    coded_block[1] = ~coded_block[1];
    coded_block[50] = coded_block[50] + 1;
    coded_block[100] = ~coded_block[100] - 100;
    coded_block[200] = coded_block[200] + 255;
    coded_block[777] = ~coded_block[777];
    coded_block[890] = coded_block[890] + 23;
    coded_block[1111] = coded_block[1110] + 39;
    coded_block[1290] = coded_block[1290] + 22;
    for (cnt = 0; cnt < 32; cnt++)
	coded_block[cnt + 533] = (~coded_block[cnt + 533] % 47) + 37;

/* Send the mangled data to ECC for correction */

    *(sc->dmacsr) = DMACSR_RESET | DMACSR_INITBUF | DMACSR_WRITE;	/* set DMA csr up */
    *(sc->dmacsr) = 0;
    *(sc->dmacsr) = DMACSR_WRITE;	/* set dir for mem to dev */
    dma->next = coded_block;
    dma->limit = coded_block + CODED_DISK_BLOCK_SIZE;
    *(sc->dmacsr) = DMACSR_WRITE | DMACSR_SETENABLE;

    disk->interrupt_status = DISK_STATUS_CLEAR;	/* clear the status reg */
    disk->control_2 = DISK_ECC_SELECT | DISK_ECC_DECODE;
    disk->control_1 = DISK_ECC_WRITE;
    cnt = 0;
    while (!(disk->interrupt_status & 0x08)) {
	delay10();
	if (++cnt > 200000)
	    return (3);
    }

/*
 * FIXME: The ECC counts the 32 contigous errors as 28, so total is 36, not
 * 40 
 */
    if (disk->correction_count != 36)
	return (4);

/* Use DMA to read the corrected data back */

    *(sc->dmacsr) = DMACSR_RESET | DMACSR_INITBUF | DMACSR_READ;	/* set the DMA csr up */
    *(sc->dmacsr) = 0;
    *(sc->dmacsr) = DMACSR_READ;	/* set dir for dev to mem */
    dma->next = result_block;
    dma->limit = result_block + UNCODED_DISK_BLOCK_SIZE;
    *(sc->dmacsr) = DMACSR_READ | DMACSR_SETENABLE;

    disk->interrupt_status = DISK_STATUS_CLEAR;	/* clear the status reg */
    disk->control_1 = DISK_ECC_READ;
    cnt = 0;
    while (!(disk->interrupt_status & 0x08)) {
	delay10();
	if (++cnt > 200000)
	    return (5);
    }


/* check the corrected data */

    if (bcmp(input_block, result_block, UNCODED_DISK_BLOCK_SIZE))
	return (6);

/* disk chip seems to work, reset the DMA */

    *(sc->dmacsr) = DMACSR_RESET | DMACSR_INITBUF | DMACSR_WRITE;
    *(sc->dmacsr) = DMACSR_RESET | DMACSR_INITBUF | DMACSR_READ;

    return (0);
}



static long RTCTest()
{
    int         seconds, cnt = 0;

    rtc_init();
    seconds = rtc_read(RTC_SEC);
    while (seconds == rtc_read(RTC_SEC)) {	/* wait for clock to tick */
	delay(1000);	   /* wait 1 ms */
	if (++cnt > 1100)  /* with overhead and 10% margin */
	    return (1);
    }

    return (0);
/* FIXME: check if the alarm works and the interrupt is ok */
}

/*
 * Byte offsets - normal main memory addresses 
 * from TBG addresses 
 */

#define MM_TBG_AB_OFF 		0x0C000000
#define MM_TBG_clApB_OFF 	0x10000000
#define MM_TBG_1mAB_OFF 	0x14000000
#define MM_TBG_ApBmAB_OFF 	0x18000000

#define VM_TBG_AB_OFF 		0x01000000
#define VM_TBG_clApB_OFF 	0x02000000
#define VM_TBG_1mAB_OFF 	0x03000000
#define VM_TBG_ApBmAB_OFF 	0x04000000

#if 0
static long TBGTest()
{

    /* 00 00 00 00  01 01 01 01  10 10 10 10  11 11 11 11 */
    const u_int pattern_A = 0x0055aaffL;
    /* 00 01 10 11  00 01 10 11  00 01 10 11  00 01 10 11 */
    const u_int pattern_B = 0x1b1b1b1bL;

    volatile u_int *mmemptr = (void *) P_MAINMEM;
    volatile u_int *vmemptr = (void *) P_VIDEOMEM;
    u_int       result[8];
    const u_int expected[8] = {0x5161B, 0x1B6FBFFF, 0x5094E4, 0x1B6BAfff, \
			       0x5161B, 0x1B6FBFFF, 0x5094E4, 0x1B6BAfff};


    u_int       temp, i;

    {
	temp = *mmemptr;   /* saves the original content */

	*mmemptr = pattern_A;
	*(mmemptr + MM_TBG_AB_OFF / sizeof(u_int)) = pattern_B;
	result[0] = *mmemptr;

	*mmemptr = pattern_A;
	*(mmemptr + MM_TBG_clApB_OFF / sizeof(u_int)) = pattern_B;
	result[1] = *mmemptr;

	*mmemptr = pattern_A;
	*(mmemptr + MM_TBG_1mAB_OFF / sizeof(u_int)) = pattern_B;
	result[2] = *mmemptr;

	*mmemptr = pattern_A;
	*(mmemptr + MM_TBG_ApBmAB_OFF / sizeof(u_int)) = pattern_B;
	result[3] = *mmemptr;

	*mmemptr = temp;   /* restore the old content */
    }

    {
	temp = *vmemptr;   /* saves the original content */

	*vmemptr = pattern_A;
	*(vmemptr + VM_TBG_AB_OFF / sizeof(u_int)) = pattern_B;
	result[4] = *vmemptr;

	*vmemptr = pattern_A;
	*(vmemptr + VM_TBG_clApB_OFF / sizeof(u_int)) = pattern_B;
	result[5] = *vmemptr;

	*vmemptr = pattern_A;
	*(vmemptr + VM_TBG_1mAB_OFF / sizeof(u_int)) = pattern_B;
	result[6] = *vmemptr;

	*vmemptr = pattern_A;
	*(vmemptr + VM_TBG_ApBmAB_OFF / sizeof(u_int)) = pattern_B;
	result[7] = *vmemptr;

	*vmemptr = temp;   /* restore the old content */
    }

    for (i = 0; i < 8; i++) {
	if (result[i] != expected[i])
	    return (i + 1);
    }

    return (0);
}
#endif

static void SystemTimerIntr()
{			   /* interrupt handler for system timer */
    char *timer_csr = (char*) P_TIMER_CSR;
    
    volatile char temp;
    if (temp = *timer_csr);	/* clear interrupt with a read */
    *timer_csr = 0;
}



static long SystemTimerTest()
{

    int         SystemTimerIntrHandler();

    struct scb *get_vbr(), *vbr;
    int         (*old_interrupt_handler) (), old_interrupt_mask;

    struct mon_global *mg = restore_mg();
    volatile int *scr2 = (int*) P_SCR2;
    volatile u_char temp, *timer_csr = (u_char *) P_TIMER_CSR;
    volatile register u_char *timer_hi = (u_char *) P_TIMER;
    volatile register u_char *timer_lo = (u_char *) P_TIMER + 1;
    register u_short timer;
    register int cnt = 0;
#define timer_set(t)  \
	(*timer_lo = 0xff, *timer_hi = (t)>>8, *timer_lo = (t)&0xff)

    *scr2 &= ~SCR2_TIMERIPL7;

    *timer_csr = TIMER_ENABLE;
    *timer_csr = 0;
    for (cnt = 0; cnt < 0x10000; cnt++) {
	*timer_lo = 0xff & cnt;
	*timer_hi = 0xff & (cnt >> 8);
	*timer_lo = 0xff & cnt;
	*timer_csr = TIMER_UPDATE;
#if	MC68040
	asm("nop");	/* flush bus writes */
#endif	MC68040
	timer = (*timer_hi << 8) + *timer_lo;
	if (timer != cnt)
	    return (2);	   /* latch is broken */
    }

    vbr = get_vbr();
    old_interrupt_handler = vbr->scb_ipl[6 - 1];
    old_interrupt_mask = *mg->mg_intrmask;
    vbr->scb_ipl[6 - 1] = SystemTimerIntrHandler;
/* FIXME: use the "official name for the bits in the intrmask */
    *mg->mg_intrmask = 0x20000000;
    temp = *timer_csr;	   /* clear leftover intrs */
    asm("movw #0x2500,sr");

    *timer_csr = 0;	   /* set timer for 1000 us = 1ms */
    timer_set(1000);
    *timer_csr = TIMER_ENABLE | TIMER_UPDATE;
    delay(1100);	   /* give 10% tolerance because of software
			    * approximation */
    if (*timer_csr & TIMER_ENABLE)
	return (3);

    *timer_csr = 0;
    asm("movw #0x2700,sr");/* turn off ints */
    vbr->scb_ipl[6 - 1] = old_interrupt_handler;	/* restore handler */
    *mg->mg_intrmask = old_interrupt_mask;	/* restore mask */

    return (0);
}


static long EventCounterTest()
{
#define	NUMBER_OF_EVENT_COUNTER_SAMPLES 100

    char        rollover = 0;
    register u_int eventCnt1, eventCnt2;
    u_int       value[100];
    volatile register u_int *ptr = value, ec;
    volatile register u_char *eventc_latch = (u_char*) P_EVENTC,
	*eventc_h = (u_char*) (P_EVENTC+1),
	*eventc_m = (u_char*) (P_EVENTC+2),
	*eventc_l = (u_char*) (P_EVENTC+3);
    int         cnt;
    int         diff, average;

    cnt = 0;		   /* does eventc move? */
    ec = *eventc_latch;
    ec = (*eventc_h << 16) | (*eventc_m << 8) | *eventc_l;
    eventCnt2 = 0xfffff & ec;
    delay(1);		   /* a very short delay (a few us) */
    /* counter ticks every 1 us, with delay and overhead it should tick */
    ec = *eventc_latch;
    ec = (*eventc_h << 16) | (*eventc_m << 8) | *eventc_l;
    if (eventCnt2 == (0xfffff & ec))	/* it didn't tick */
	return (1);

    ec = *eventc_latch;
    ec = (*eventc_h << 16) | (*eventc_m << 8) | *eventc_l;
    eventCnt1 = ec;   /* read it very fast */
    delay(1000);
    ec = *eventc_latch;
    ec = (*eventc_h << 16) | (*eventc_m << 8) | *eventc_l;
    eventCnt2 = ec;   /* read it very fast again */
    eventCnt1 &= 0xfffff;
    eventCnt2 &= 0xfffff;
    diff = eventCnt2 - eventCnt1;
    if (diff < 0)
	diff = (0x100000 + diff);	/* rolled over */
    if ((900 > diff) || (diff > 1100))	/* actual is about 1028 */
	return (2);	   /* counter is not accurate */

/*
 * have to capture values and then check them since eventc is so fast 
 */

    ptr = value;
    for (cnt = 0; cnt < NUMBER_OF_EVENT_COUNTER_SAMPLES; cnt++) {
	ec = *eventc_latch;
	ec = (*eventc_h << 16) | (*eventc_m << 8) | *eventc_l;
	*ptr++ = 0xfffff & ec;
    }

    for (average = 0, cnt = 1; cnt < NUMBER_OF_EVENT_COUNTER_SAMPLES; cnt++) {
	diff = value[cnt] - value[cnt - 1]; /* note diff is signed, value is not */
	if (diff >= 0)
	    value[cnt - 1] = diff;
	else	/* rolled over */
	    value[cnt - 1] = (0x100000 + diff);
	average += value[cnt - 1];
    }
    average /= (NUMBER_OF_EVENT_COUNTER_SAMPLES - 1);

/* First samples is asynchronous to the rest... */

    /* Note that value[0] is not a good sample since the sample */
    /* loop was not fully cached in the CPU */
    for (cnt = 1; cnt < NUMBER_OF_EVENT_COUNTER_SAMPLES - 1; cnt++) {
	if (abs(value[cnt] - average) > 3)
	    return (3);
    }

    return (0);
}


static void SoundOutOverRunIntr()
{			   /* interrupt handler for Sound out over */
                tprintf("Sound Out Over Run Interrupt.\n");
}




static void mon_send_nodma(int mon_cmd, int data)
{
    volatile struct monitor *mon = (struct monitor *) P_MON;
/*
 * mon_send waits for one complete dma cycle if dmaen is on but must have
 * dmaen on to avoid underrun when sending a sound out cmd, hence this code
 * stolen from sound.c, written by Gregg Kellog. 
 *
 * Wait for any previous command to complete transmission 
 */

    while (mon->mon_csr.ctx);
    mon->mon_csr.cmd = mon_cmd;
    mon->mon_data = data;
/*
 * FIXME: this delay value is ad hock; what is the "correct" value? 
 */
    delay(200);
}


#define FIFTY_MICROSEC 50
static void SoundOutIntr()
{			   /* interrupt handler for Sound out DMA */

    /* clear interupt */
    *(u_int *)P_SOUNDOUT_CSR = DMACSR_RESET | DMACSR_INITBUF | DMACSR_WRITE;

    mon_send_nodma(MON_SNDOUT_CTRL(0), 0);	/* turn off monitor */
    delay(FIFTY_MICROSEC);
}


static void setVol(channel, flags, vol)
    int         channel, flags, vol;
{
    char        i;
    int         bits = 0x700 | channel | vol;
    mon_send(MON_GP_OUT, flags);
    delay(200);

    for (i = 10; i >= 0; i--) {
	mon_send(MON_GP_OUT, flags
		 | ((bits & (1 << i)) ? (SGP_VSCD << 24) : 0));
	delay(200);
	mon_send(MON_GP_OUT, flags | (SGP_VSCCK << 24)
		 | ((bits & (1 << i)) ? (SGP_VSCD << 24) : 0));
	delay(200);
    }
    mon_send(MON_GP_OUT, flags);
    delay(200);
    mon_send(MON_GP_OUT, flags | (SGP_VSCS << 24));
    delay(200);
    mon_send(MON_GP_OUT, flags);
    delay(200);
}

extern int  SoundOutTest()
{
    short      *SineForSoundOut();
    int         SoundOutIntrHandler();
    void        mon_send_nodma();
    void        setVol();

    register struct mon_global *mg = restore_mg();
    struct nvram_info *ni = &mg->mg_nvram;
    struct scb *get_vbr(), *vbr;
    int         (*old_interrupt_handler) (), old_interrupt_mask;

    short      *dacData;
    volatile u_int *dmacsr = (u_int *)P_SOUNDOUT_CSR;
    volatile int *dmaNext = (int *)(P_SOUNDOUT_CSR + 0x4000);
    volatile int *dmaLimit = (int *)(P_SOUNDOUT_CSR + 0x4000 + 0x4);
    volatile int *dmaStart = (int *)(P_SOUNDOUT_CSR + 0x4000 + 0x8);
    volatile int *dmaStop = (int *)(P_SOUNDOUT_CSR + 0x4000 + 0xc);
    volatile struct monitor *mon = (struct monitor *) P_MON;
#define MAX_RETRY 8

    int         i, retry = MAX_RETRY, result = 0;
    register int gpFlags, vol_r, vol_l;


/* dma must end on nibble */
/*
 * FIXME: sineforsound routine calculates 64K of sound, only 8k used 
 */
    dacData = (short *)mon_alloc(0x10000 + CHIP_SLOP);
    dacData = (short *)(((int)dacData & ~0xf) + 0x10);


/* enable sound out DMA interrupts */

    vbr = get_vbr();
/*
 * FIXME: Ignore sound out dma underrun ?  yes, a failure is a failure 
 */
    old_interrupt_handler = vbr->scb_ipl[6 - 1];
    vbr->scb_ipl[6 - 1] = SoundOutIntrHandler;
    old_interrupt_mask = *mg->mg_intrmask;
/* FIXME: use the "official name for the bits in the intrmask */
    *mg->mg_intrmask = 0x00800000;
    asm("movw #0x2500,sr");

/* Set up monitor control register */

    mon->mon_csr.dmaout_dmaen = 0;	/* disable sound out */
    mon_send_nodma(MON_SNDOUT_CTRL(0), 0); /* sound off cmd */
    delay(FIFTY_MICROSEC);
    clr_so_urun();	   /* reset overrun error bit */

/* set the volume using values stored in NVRAM */

    gpFlags = ni->ni_spkren ? SGP_SPKREN << 24 : 0;
    gpFlags |= ni->ni_lowpass ? SGP_LOWPASS << 24 : 0;
    setVol(VOLUME_LCHAN, gpFlags, VOL_NOM);
    setVol(VOLUME_RCHAN, gpFlags, VOL_NOM);

/* Set up the DMA channel */

    *dmacsr = DMACSR_RESET | DMACSR_INITBUF | DMACSR_WRITE;
    *dmacsr = 0x0;
    *dmacsr = DMACSR_WRITE;
    *dmaNext = (int)dacData;
    *dmaLimit = 0x02000 + (int)dacData;
    *dmaStart = (int)dacData;
    *dmaStop = 0x02000 + (int)dacData;
    *dmacsr = DMACSR_WRITE | DMACSR_SETENABLE;

/* load up the sine table */

    SineForSoundOut(dacData);

/* Send the data and wait for the interrupt */

    mon_send_nodma(MON_SNDOUT_CTRL(1), 0);	/* sound on cmd */
    mon->mon_csr.dmaout_dmaen = 1;	/* enable sound out */
    while (!mon->mon_csr.dmaout_dmaen);	/* wait for bit to settle */
    clr_so_urun();	   /* reset overrun error bit */

#define MAX_OVERRUN_RESET 50
    while (retry > 0) {	   /* try to start sound */
	i = 0;
	if (++i < MAX_OVERRUN_RESET) {
	    if (*dmaNext != *dmaStart) {	/* normal case */
		clr_so_urun();	/* reset overrun error bit */
		break;
	    }
	    if (mon->mon_csr.dmaout_ovr) {
		if (*dmaNext == *dmaStart) {	/* wedged */
		    mon_send_nodma(0xc7, *dacData);
		    clr_so_urun();	/* reset overrun error bit */
		    retry--;
		    continue;
		} else {   /* spurious overrun */
		    clr_so_urun();	/* reset overrun error bit */
		    break;
		}
	    }
	}
	if (i >= MAX_OVERRUN_RESET) {
	    result = 1;	   /* didn't start or underrun, fatal error */
	    tprintf("\nSound Out DMA error!\n");
	    break;
	}
    }

/* whew!, got the sound out started, but does it work? */

    if (result == 0) {	   /* it started */
	/*
	 * FIXME: put in a decent call to a timer long enough for sound to
	 * finish 
	 */
	delay(200000);
	if (*dmacsr & DMACSR_ENABLE) {
	    result = 2;	   /* test failed, no intr occured */
	}
    }
/* Clean up the interrupt system */

    asm("movw #0x2700,sr");/* turn off intrs */
    vbr->scb_ipl[6 - 1] = old_interrupt_handler;	/* restore handler */
    *mg->mg_intrmask = old_interrupt_mask;	/* restore mask */
    mon_send_nodma(MON_SNDOUT_CTRL(0), 0);	/* send sound off cmd */
    delay(FIFTY_MICROSEC);
    mon->mon_csr.dmaout_dmaen = 0;	/* disable sound out */
    mon->mon_csr.dmaout_ovr = 1;
    setVol(VOLUME_LCHAN, gpFlags, ni->ni_vol_l);	/* restore volume */
    setVol(VOLUME_RCHAN, gpFlags, ni->ni_vol_r);

    return (result);
}

static u_int *test_SIMM(volatile u_int * baseptr, int size)
{
    u_int       limit;
    register u_int *memptr;
    /*
     * Note that memptr is not declared volatile to speed up the read back
     * portion of the test. The compiler generates faster code this way because
     * it compare directly from the memory instead of first loading it 
     */

    register u_int value1, value2, value3;
    int         test;

    /* turn on both i and d cache for max speed */
#ifdef	MC68030
    set_cacr(CACR_CI + CACR_EI + CACR_CD + CACR_ED + CACR_DBE);
#endif	MC68030
#ifdef	MC68040
    set_cacr(CACR_040_DE + CACR_040_IE);
#endif	MC68040

    limit = (u_int) baseptr + size;
    value1 = 0xdb6db6dbL;
    value2 = value1 << 1;
    value3 = value1 << 2;

    for (test = 1; test <= 2; test++) {
	memptr = (u_int *) baseptr;
	for (; (u_int) memptr < (limit - 8);) {
	    *memptr++ = value1;
	    *memptr++ = value2;
	    *memptr++ = value3;
	}
	if ((u_int) memptr < limit)
	    *memptr++ = value1;
	if ((u_int) memptr < limit)
	    *memptr++ = value2;

	/* flush the d cache, actually not needed because the way */
	/* we roll over the cache, but I am paranoid */
	clear_d_cache();

	memptr = (u_int *) baseptr;
	for (; (u_int) memptr < (limit - 8);) {
	    if (*memptr++ != value1)
		return (--memptr);
	    if (*memptr++ != value2)
		return (--memptr);
	    if (*memptr++ != value3)
		return (--memptr);
	}
	if ((u_int) memptr < limit)
	    if (*memptr++ != value1)
		return (--memptr);
	if ((u_int) memptr < limit)
	    if (*memptr++ != value2)
		return (--memptr);

	value1 = ~value1;
	value2 = ~value2;
	value3 = ~value3;
    }

    {
	volatile char *charptr = (char *)baseptr;
	int         i;

#ifdef	MC68030
	set_cacr(CACR_CI + CACR_EI);	/* leave d cache off */
#endif	MC68030
#ifdef	MC68040
	set_cacr(CACR_040_IE);
#endif	MC68040
	for (i = 1; i <= 4; i++)
	    *charptr++ = i;

	charptr = (char *)baseptr;
	for (i = 1; i <= 4; i++)
	    if (*charptr++ != i)
		return ((u_int *) baseptr);

    }

    return (0);
}



static int  is_empty(volatile u_int * basePtr)
{
    int         i;

    for (i = 0; i < 8; i++) {
	*basePtr = 0;
#if	MC68040
	asm(".word 0xf4f8 | cpusha	ic,dc");
	asm("nop");		/* allow aliased write to complete! */
#endif	MC68040
	if (*basePtr != (int)basePtr)	/* should read address as data */
	    if (*basePtr != (int)basePtr)	/* try again */
		return (0);/* error, bank is not empty */
	basePtr++;
    }

    return (1);		   /* the bank is empty */

}


static char get_page_mode_size(volatile u_int * baseAdr)
{
    /* pointer to first rollover word if SIMM is 1Mb */
    register volatile u_int *memptr1 = baseAdr + 0x400000 / sizeof(int);
    /* pointer to first rollover word if SIMM is 256Kb */
    register volatile u_int *memptr2 = baseAdr + 0x100000 / sizeof(int);

    *baseAdr = TP1;
    *memptr1 = TP2;
    *memptr2 = TP3;
#if	MC68040
    asm(".word 0xf4f8 | cpusha	ic,dc");
    asm("nop");		/* allow aliased write to complete! */
#endif	MC68040
    switch (*baseAdr) {
    case TP1:
	return (SIMM_16MB + SIMM_PAGE_MODE);
    case TP2:
	return (SIMM_4MB + SIMM_PAGE_MODE);
    case TP3:
	return (SIMM_1MB + SIMM_PAGE_MODE);
    default:
	return (SIMM_EMPTY);
    }
}

static int  is_nibble(volatile u_int * baseAdr)
/*NOTE one must try the hardware from 4Mb, 1Mb, then to 256MB 
/*NOTE and properly set the SCR2 prior to calling */

{
               *baseAdr = TP1;
    *++baseAdr = ~TP1;
#if	MC68040
    asm(".word 0xf4f8 | cpusha	ic,dc");
    asm("nop");		/* allow aliased write to complete! */
#endif	MC68040
    return ((*--baseAdr == TP1));
}


extern int  set_SCR2_memType(register struct mon_global * mg)
/* Note that data cache is assumed to be off */
/* Note that the SIMM tpye and size is determined by "try an error"
   and the exact sequence of tests is critical.  I wonder if a better
   way exsists given the hardware design....
*/
{
    int         bank, i, parity, no_parity;
    int         error = 0;

    unsigned    maskbits;
    /* declared volatile since they should not be optimized */
    volatile u_int *scr2;
    volatile u_int *memptr;
    volatile struct bmap_chip *bm = (volatile struct bmap_chip*) P_BMAP;

    scr2 = (u_int *) P_SCR2;	/* pointer to SCR2, slot relative */

    for (bank = 0; bank < N_SIMM; bank++) {
	/* pointer to first word in memory bank, slot relative */
	memptr = (u_int *) (P_MAINMEM + 0x1000000 * bank);
	maskbits = ~(SCR2_DRAM_MASK << bank);

	/* tell SCR2 to expect page mode SIMM's at this bank */
	/* then check for empty bank or page mode */
	*scr2 = (*scr2 & maskbits) | (SCR2_PAGE_MODE << bank);
	if (is_empty(memptr)) {
	    mg->mg_simm[bank] = SIMM_EMPTY;
	    continue;
	}
	if (burst_mode_ok(memptr)) {	/* confirm our guess */
	    mg->mg_simm[bank] = get_page_mode_size(memptr);
	    if (mg->mg_simm[bank] == SIMM_EMPTY) {	/* hey, it is bad */
		print_memory_test_result(memptr);	/* find out the bad
							 * socket */
		error = 1;
	    }
	    continue;	   /* move on to next bank */
	}
	/* tell SCR2 to expect 4Mb nibble mode SIMM's at this bank */
	*scr2 = (*scr2 & maskbits) | (SCR2_DRAM_4M << bank);
	if (is_nibble(memptr)) {	/* confirm our guess */
	    mg->mg_simm[bank] = SIMM_16MB;
	    continue;	   /* move on to next bank */
	}
	/* tell SCR2 to expect 1Mb nibble mode SIMM's at this bank */
	*scr2 = (*scr2 & maskbits) | (SCR2_DRAM_1M << bank);
	if (is_nibble(memptr)) {	/* confirm our guess */
	    mg->mg_simm[bank] = SIMM_4MB;
	    continue;	   /* move on to next bank */
	}
	/* tell SCR2 to expect 256Kb nibble mode SIMM's at this bank */
	*scr2 = (*scr2 & maskbits) | (SCR2_DRAM_256K << bank);
	if (is_nibble(memptr)) {	/* confirm our guess */
	    mg->mg_simm[bank] = SIMM_1MB;
	    continue;	   /* move on to next bank */
	}
	/* We have a problem, the SIMM's are sick */
	/* tell SCR2 to expect page mode SIMM's at this bank */
	/* since it is universal for all SIMM's as long as we don't */
	/* use burst mode. */
	*scr2 = (*scr2 & maskbits) | (SCR2_PAGE_MODE << bank);
	mg->mg_simm[bank] = SIMM_EMPTY;	/* consider this bank empty */
	print_memory_test_result(memptr);	/* find out the bad socket */
	error = 1;

    }

    for (bank = 0; bank < N_SIMM; bank++)	/* check for mixed mode SIMM's */
	if (mg->mg_simm[bank] != SIMM_EMPTY) {

		/* pointer to first word in memory bank, slot relative */
		memptr = (volatile u_int *) (P_MAINMEM + 0x1000000 * bank);
		if (!burst_mode_ok(memptr)) {
		    tprintf("Bank %d has mixed mode SIMM's\n", bank);
		    mg->mg_simm[bank] = SIMM_EMPTY;	/* consider it is empty */
		    error = 1;
		}
	}

#if	MC68040
	/* check for parity SIMM's */
	parity = no_parity = 0;
	for (bank = 0; bank < N_SIMM; bank++) {
		if (mg->mg_simm[bank] != SIMM_EMPTY) {
			int *vbr, old_buserr, parity_error();
			extern int simm_size[];
	
			memptr = (volatile u_int *) (P_MAINMEM + 0x1000000 * bank);
			vbr = (int*) get_vbr();
			old_buserr = vbr[T_BUSERR >> 2];
			vbr[T_BUSERR >> 2] = (int)&parity_error;
			bm->bm_paren = 1;
			*memptr = 0;
			*(memptr+1) = 0;
			*(memptr+2) = 0;
			*(memptr+3) = 0;
			clear_d_cache();
			i = *memptr;
			*memptr = 0x01010101;
			*(memptr+1) = 0x01010101;
			*(memptr+2) = 0x01010101;
			*(memptr+3) = 0x01010101;
			clear_d_cache();
			i = *memptr;
			vbr[T_BUSERR >> 2] = old_buserr;
			if (bm->bm_paren == 0) {
				no_parity = 1;
				continue;
			}
			
			/* establish good parity at each RAM location */
			bzero (memptr, simm_size[mg->mg_simm[bank]]);
			clear_d_cache();
			mg->mg_simm[bank] |= SIMM_PARITY;
			parity = 1;
		}
	}
	if (parity && no_parity) {
		tprintf ("All of the SIMMs must be parity SIMMs if you want parity to work.\n");
		error = 1;
	}
	if (no_parity) {

		/* revert to non-parity memory timing */
		extern char mem_timing[][32][NMTREG];
		volatile struct scr1 scr1;
		
		scr1 = *(struct scr1*) P_SCR1;
		for (i = 0; i < NMTREG; i++) {
		    ((char*)P_MEMTIMING)[i] = mem_timing[mg->mg_machine_type]
			    [(scr1.s_cpu_clock << 2) + scr1.s_mem_speed][i];
		}
	} else {
		mg->mg_flags2 |= MGF2_PARITY;
	}
#endif	MC68040
    return (error);
}


/**************  EXTERNAL PROCEDURES  *****************/

extern unsigned char poweron_test()
{

    register struct mon_global *mg = restore_mg();
    struct nvram_info *ni = &mg->mg_nvram;
    char        on, extended, soundout, loop, verbose, abort;
    long        result, ctr;
    on = ni->ni_pot[0] & POT_ON;
    verbose = ni->ni_pot[0] & VERBOSE_POT;
    loop = ni->ni_pot[0] & LOOP_POT;
    extended = ni->ni_pot[0] & EXTENDED_POT;
    soundout = ni->ni_pot[0] & TEST_MONITOR_POT;

    if (!on)
	return (0);

    if (verbose)
	printf("Testing the FPU");
    if (result = FPUTest())
	return (FPU_TEST_FAILED << 4 | (result & 0x0f));

    if (verbose)
	printf(", SCC");
    if (result = SCCTest())
	return (SCC_CONTROLLER_TEST_FAILED << 4 | (result & 0x0f));

/*
 * serialPrint('a',"\r\nPort A:\r\n"); serialPrint('b',"\r\nPort B:\r\n"); 
 */

    if (verbose)
	printf(", SCSI");
    if (result = SCSITest())
	return (SCSI_CONTROLLER_TEST_FAILED << 4 | (result & 0x0f));

    if (verbose)
	printf(", Enet");
    if (result = EtherNetTest())
	return (ETHERNET_CONTROLLER_TEST_FAILED << 4 | (result & 0x0f));

    if (mg->mg_machine_type == NeXT_CUBE || mg->mg_machine_type == NeXT_X15) {
	if (verbose)
	    printf(", ECC");
	if (result = MODriveTest())
	    return (MO_CONTROLLER_TEST_FAILED << 4 | (result & 0x0f));
    }

    if (verbose)
	printf(", RTC");
    if (result = RTCTest())
	return (RTC_TEST_FAILED << 4 | (result & 0x0f));
#if 0
    if (verbose)
	printf(", TBG");
    if (result = TBGTest())
	return (TBG_TEST_FAILED << 4 | (result & 0x0f));
#endif
    if (verbose)
	printf(", Timer");
    if (result = SystemTimerTest())
	return (SYSTEM_TIMER_TEST_FAILED << 4 | (result & 0x0f));

    if (verbose)
	printf(", Event Counter");
    if (result = EventCounterTest())
	return (EVENT_COUNTER_TEST_FAILED << 4 | (result & 0x0f));

    if (soundout) {
	if (verbose)
	    printf(", Sound Out");

	/* for some reason, we need to do this twice! */
	SoundOutTest();
	delay(100000);
	if (result = SoundOutTest())
	    return (SOUND_OUT_TEST_FAILED << 4 | (result & 0x0f));
    }
    
    if (extended) {
	printf("\n\nStarting Extended Self Test...\n");
	printf("Extended SCSI Test");
	if (result = ExtendedSCSITest())
	    return (SCSI_CONTROLLER_TEST_FAILED << 4 | (result & 0x0f));
    }
    if (loop) {
	printf("\n\nPress and hold any key to exit self test");
	for (ctr = 0; ctr < 10; ctr++)
	    if (abort = (u_char) mon_try_getc())
		return (0);
	    else {
		delay(400000);
		printf("%s", ".");
	    }
	entry();
    }
    return (POT_PASSED);
}



extern int  NVRAMTest()
{
    register struct nvram_info *ni;
    char        nv_data[sizeof(*ni)];
    long        index;
    int         nvbyte, pattern;

    ni = (struct nvram_info *) nv_data;
    if (!nvram_check(ni))	/* nvram ok, are POT & DRAM bits on? */
	return (ni->ni_pot[0] & TEST_DRAM_POT);

    /* Test all 32 nvRAM bytes */
    for (nvbyte = 0; nvbyte < 32; nvbyte++)
	for (pattern = 1; pattern < 0x100; pattern = pattern << 1) {
	    rtc_write(RTC_RAM | nvbyte, pattern);
	    /* if nvram can't be set, blink forever */
	    if (rtc_read(RTC_RAM | nvbyte) != pattern)
		blinkLed(BLINK_NVRAM);
	}

    /* test DRAM if NVRAM is bogus */
    return (1);
}


extern int  mainRAMTest(int sid, register struct mon_global * mg)
{
    int         bank;
    volatile u_int *memptr;
    volatile u_int *badmemptr;
    int         error = 0;
    u_int       oldcacr;

/* example code to enable printing for debugging 
    mg->km.flags &= ~KMF_INIT;
    mg->km.flags |= KMF_SEE_MSGS;
    printf("Hello world\n");
*/
    led_On(sid);
    /* saves the old cacr since it will get mess up */
    /* set it to 0 just to get the oldcacr */

    oldcacr = set_cacr(0);

    for (bank = 0; bank < 4; bank++) {
	memptr = (u_int *) (sid + P_MAINMEM + CONST_16MB * bank);
	badmemptr = 0;
	switch (mg->mg_simm[bank]) {
	case SIMM_16MB:
	case SIMM_16MB + SIMM_PAGE_MODE:
	    badmemptr = test_SIMM(memptr, CONST_16MB);
	    break;
	case SIMM_4MB:
	case SIMM_4MB + SIMM_PAGE_MODE:
	    badmemptr = test_SIMM(memptr, CONST_4MB);
	    break;
	case SIMM_1MB:
	case SIMM_1MB + SIMM_PAGE_MODE:
	    badmemptr = test_SIMM(memptr, CONST_1MB);
	    break;
	}
	if (badmemptr != 0) {
	    error = 1;
	    set_cacr(0);   /* must make sure that at least d cache is not on */
	    print_memory_test_result(badmemptr);
	    /* test_SIMM will reset the cacr to whatever it wants */
	}
    }

    set_cacr(oldcacr);
    led_Off(sid);
    return (error);
}

