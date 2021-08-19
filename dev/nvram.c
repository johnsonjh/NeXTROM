/*	@(#)nvram.c	1.0	10/12/86	(c) 1986 NeXT	*/

#include <sys/types.h>
#include <mon/monparam.h>
#include <mon/global.h>
#include <mon/nvram.h>
#include <next/clock.h>
#include <next/cpu.h>
#include <next/scr.h>

delay1()
{
	delay(1);
}

rtc_set_clr (addr, val, mode)
	u_char addr, val;
{
	rtc_write (addr, (rtc_read (addr) & ~val) | (val & mode));
}

rtc_intr()
{
	struct mon_global *mg = restore_mg();
	int p_down = 0, status;
	
	while (*mg->mg_intrstat & I_BIT(I_POWER)) {
		if ((status = rtc_read (RTC_STATUS)) & RTC_NEW_CLOCK) {
			if (status & RTC_FTU)
				rtc_set_clr (RTC_CONTROL, RTC_FTUC, RTC_SET);
			if (status & RTC_LOW_BATT)
				rtc_set_clr (RTC_CONTROL, RTC_LBE, RTC_CLR);
			if (status & RTC_ALARM)
				rtc_set_clr (RTC_CONTROL, RTC_AC, RTC_SET);
			if (status & RTC_RPD) {
				rtc_set_clr (RTC_CONTROL, RTC_RPDC, RTC_SET);
				p_down = 1;
			}
		} else {
			return (1);
		}
	}
	return (p_down);
}

rtc_power_down()
{
	int addr;
	
	addr = (rtc_read (RTC_STATUS) & RTC_NEW_CLOCK)? RTC_CONTROL : RTC_INTRCTL;
	rtc_set_clr (addr, RTC_PDOWN, RTC_SET);
	while (1)
		;
	/* NOT_REACHED */
}

rtc_reset()
{
	if ((rtc_read (RTC_STATUS) & RTC_NEW_CLOCK) == 0)
		rtc_write (RTC_INTRCTL, 0);
	rtc_intr();
}

rtc_init()
{
	rtc_reset();
	if (rtc_read (RTC_STATUS) & RTC_NEW_CLOCK)
		rtc_set_clr (RTC_CONTROL, RTC_START, RTC_SET);
	else
		rtc_write (RTC_CONTROL, RTC_START | RTC_XTAL);
}

nvram_check (ni)
	register struct nvram_info *ni;
{
	register u_short verify, computed;
	register int i;
	register char *cp;

	for (i = 0, cp = (char*) ni; i < sizeof (*ni); i++)
		*cp++ = rtc_read (RTC_RAM | i);
	verify = ni->ni_cksum;
	ni->ni_cksum = 0;
	if ((computed = ~cksum ((caddr_t) ni, sizeof (*ni))) == 0 ||
	    computed != verify)
		return (-1);
	return (0);
}

nvram_set (ni)
	register struct nvram_info *ni;
{
	struct nvram_info verify;
	register int i;
	register char *cp;

	ni->ni_cksum = 0;
	ni->ni_cksum = ~cksum ((caddr_t) ni, sizeof (*ni));
	for (i = 0, cp = (char*) ni; i < sizeof (*ni); i++)
		rtc_write (RTC_RAM | i, *cp++);
	bcopy ((caddr_t) ni, (caddr_t) &verify, sizeof (*ni));
	nvram_check (ni);
	if (strcmp ((caddr_t) &verify, (caddr_t) ni, sizeof (*ni)) != 0)
		return(-1);
	return(0);
}

rtc_write (addr, val)
	u_char addr, val;
{
	register int i, v, def;
	register struct mon_global *mg = restore_mg();
	volatile register int *scr2 = (int*) P_SCR2;

#if	NCC
#else	NCC
	addr |= RTC_WRITE;
#endif	NCC
	def = *scr2 & ~(SCR2_RTDATA | SCR2_RTCE | SCR2_RTCLK);
	*scr2 = def | SCR2_RTCE;
	delay1();
	for (i = 0;  i < 8;  i++) {
		v = def | SCR2_RTCE;
		if (addr & 0x80)
			v |= SCR2_RTDATA;
		*scr2 = v;
		delay1();
		v |= SCR2_RTCLK;
		*scr2 = v;
		delay1();
		v &= ~SCR2_RTCLK;
		*scr2 = v;
		addr <<= 1;
		delay1();
	}
	for (i = 0;  i < 8;  i++) {
		v = def | SCR2_RTCE;
		if (val & 0x80)
			v |= SCR2_RTDATA;
		*scr2 = v;
		delay1();
		v |= SCR2_RTCLK;
		*scr2 = v;
		delay1();
		v &= ~SCR2_RTCLK;
		*scr2 = v;
		val <<= 1;
		delay1();
	}
	*scr2 = def;
}

rtc_read (addr)
	u_char addr;
{
	u_char val;
	
	val = rtc_real_read (addr);
#if	NCC
	rtc_real_read (RTC_STATUS);	/* MCS1850 bug: park address to reduce Ibatt */
#endif	NCC
	return (val);
}

rtc_real_read (addr)
	u_char addr;
{
	register int i, v, def;
	register struct mon_global *mg = restore_mg();
	volatile register int *scr2 = (int*) P_SCR2;
	register u_char val = 0;

#if	NCC
	addr |= RTC_WRITE;
#endif	NCC
	def = *scr2 & ~(SCR2_RTDATA | SCR2_RTCE | SCR2_RTCLK);
	*scr2 = def | SCR2_RTCE;
	delay1();
	for (i = 0;  i < 8;  i++) {
		v = def | SCR2_RTCE;
		if (addr & 0x80)
			v |= SCR2_RTDATA;
		*scr2 = v;
		delay1();
		v |= SCR2_RTCLK;
		*scr2 = v;
		delay1();
		v &= ~SCR2_RTCLK;
		*scr2 = v;
		addr <<= 1;
		delay1();
	}
	for (i = 0;  i < 8;  i++) {
		*scr2 = def | SCR2_RTCE | SCR2_RTCLK;
		delay1();
		val <<= 1;
		*scr2 = def | SCR2_RTCE;
		delay1();
		val |= (*scr2 & SCR2_RTDATA)? 1 : 0;
		delay1();
	}
	*scr2 = def;
	return (val);
}
