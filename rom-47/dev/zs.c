/*	@(#)zs.c	1.0	6/1/87	(c) 1987 NeXT	*/

/* FIXME: use of mg_flags should be partitioned per channel */

#include <sys/types.h>
#include <next/cpu.h>
#include <mon/global.h>
#include <mon/sio.h>
#include <dev/zsreg.h>

#define	ZSADDR_A	((struct zsdevice*) P_SCC+1)
#define	ZSADDR_B	((struct zsdevice*) P_SCC)

#ifndef CTRL
#define	CTRL(s)		((s)&0x1f)
#endif CTRL

void zschan_init();

void
zs_init()
{
	register struct mon_global *mg = restore_mg();

	ZSWRITE_A(9, WR9_RESETHARD);
	delay(10);
	zschan_init(P_SCC+1, 9600);
	zschan_init(P_SCC, 9600);
	mg->mg_flags &= ~(MGF_UART_STOP | MGF_UART_TYPE_AHEAD);
}

void
zschan_init(zsaddr, speed)
volatile register struct zsdevice *zsaddr;
{
	int tmp;

	ZSWRITE(zsaddr, 9, WR9_NV);
	ZSWRITE(zsaddr, 11, WR11_TXCLKBRGEN|WR11_RXCLKBRGEN);
#if	NeXT
	zsaddr->zs_ctrl = WR0_RESET;
	zsaddr->zs_ctrl = WR0_RESET_STAT;
#else	NeXT
	ZSWRITE(zsaddr, 0, WR0_RESET);
	ZSWRITE(zsaddr, 0, WR0_RESET_STAT);
#endif	NeXT
	ZSWRITE(zsaddr, 4, WR4_X16CLOCK|WR4_STOP1);
	ZSWRITE(zsaddr, 3, WR3_RX8);
	ZSWRITE(zsaddr, 5, WR5_TX8);
	/*
	 * baud rate generator has to be disabled while switching
	 * time constants and clock sources
	 */
	ZSWRITE(zsaddr, 14, 0);		/* Baud rate generator disabled */
	tmp = zs_tc(speed, 16);
	ZSWRITE(zsaddr, 12, tmp);
	ZSWRITE(zsaddr, 13, tmp>>8);
	ZSWRITE(zsaddr, 14, tmp>>16);
	delay(10);
	ZSWRITE(zsaddr, 14, WR14_BRENABLE|(tmp>>16));
	ZSWRITE(zsaddr, 3, WR3_RX8|WR3_RXENABLE);
	ZSWRITE(zsaddr, 5, WR5_TX8|WR5_RTS|WR5_DTR|WR5_TXENABLE);
}

static int zsfreq[] = { PCLK_HZ, RTXC_HZ };

/*
 * NOTE: FIXSCALE * max_clk_hz must be < 2^31
 */
#define	FIXSCALE	256

int
zs_tc(baudrate, clkx)
{
	int i, br, f;
	int rem[2], tc[2];

	u_char *scc_clk = (u_char*) P_SCC_CLK;

	/* setup scc clock select register */
	*scc_clk = PCLK_3684_MHZ | SCLKB_4_MHZ | SCLKA_4_MHZ;

	/*
	 * This routine takes the requested baudrate and calculates
	 * the proper clock input and time constant which will program
	 * the baudrate generator with the minimum error
	 *
	 * BUG: doesn't handle 134.5 -- so what!
	 */
	for (i = 0; i < 2; i++) {
		tc[i] = ((zsfreq[i]+(baudrate*clkx))/(2*baudrate*clkx)) - 2;
		/* kinda fixpt calculation */
		br = (FIXSCALE*zsfreq[i]) / (2*(tc[i]+2)*clkx);
		rem[i] = abs(br - (FIXSCALE*baudrate));
	}
	if (rem[1] <= rem[0])
		return(tc[1] & 0xffff);	/* RTxC is better clock choice */
	/* PCLK is better clock choice */
	return((tc[0] & 0xffff) | (WR14_BRPCLK<<16));
}

int
zs_getc(zsaddr)
volatile register struct zsdevice *zsaddr;
{
	while ((zsaddr->zs_ctrl & RR0_RXAVAIL) == 0)
		continue;
	return(zsaddr->zs_data);
}

void
zs_putc(zsaddr, c)
volatile register struct zsdevice *zsaddr;
{
	while ((zsaddr->zs_ctrl & RR0_TXEMPTY) == 0)
		continue;
	zsaddr->zs_data = c;
}

int
zs_ischar(zsaddr)
volatile register struct zsdevice *zsaddr;
{
	return(zsaddr->zs_ctrl & RR0_RXAVAIL);
}

void
zs_scan(zsaddr)
volatile register struct zsdevice *zsaddr;
{
	register struct mon_global *mg = restore_mg();

	if (zs_ischar(zsaddr)) {
		switch (zs_getc(zsaddr) & 0x7f) {
		case CTRL('S'):
			mg->mg_flags |= MGF_UART_STOP;
			break;
		default:
			mg->mg_flags |= MGF_UART_TYPE_AHEAD;
		case CTRL('Q'):
			mg->mg_flags &= ~MGF_UART_STOP;
			break;
		}
	}
}

int
getc (dev)
{
	int c;
	register struct mon_global *mg = restore_mg();
	volatile register struct zsdevice *zsaddr =
		(struct zsdevice*) (P_SCC + ((dev & ~RAW_IO)? 0 : 1));

	if ((mg->mg_flags & MGF_UART_SETUP) == 0) {
		zs_init();
		mg->mg_flags |= MGF_UART_SETUP;
	}
	if (dev & RAW_IO) {
		c = zs_getc(zsaddr);
		return (c);
	}
	do {
		/*
		 * clear parity bit
		 */
		c = zs_getc(zsaddr) & 0x7f;
	} while (c == CTRL('S') || c == CTRL('Q'));
	mg->mg_flags &= ~MGF_UART_TYPE_AHEAD;
	return(c);
}

int
is_char(dev)
{
	register int ahead;
	register struct mon_global *mg = restore_mg();
	volatile register struct zsdevice *zsaddr =
		(struct zsdevice*) (P_SCC + ((dev & ~RAW_IO)? 0 : 1));

	ahead = mg->mg_flags & MGF_UART_TYPE_AHEAD;
	mg->mg_flags &= ~MGF_UART_TYPE_AHEAD;
	return (ahead);
}

putc(dev, c)
{
	register struct mon_global *mg = restore_mg();
	volatile register struct zsdevice *zsaddr =
		(struct zsdevice*) (P_SCC + ((dev & ~RAW_IO)? 0 : 1));

	if ((mg->mg_flags & MGF_UART_SETUP) == 0) {
		zs_init();
		mg->mg_flags |= MGF_UART_SETUP;
	}
	if (dev & RAW_IO) {
		zs_putc(zsaddr, c);
		return;
	}
	do {
		zs_scan(zsaddr);
	} while (mg->mg_flags & MGF_UART_STOP);
	zs_putc(zsaddr, c);
}

zs_null()
{
	return (0);
}

struct	device serial_zs = { zs_null, zs_null, getc, putc, D_IOT_CHAR };
