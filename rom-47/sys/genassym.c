/*	@(#)genassym.c	1.0	10/12/86	(c) 1986 NeXT	*/

#include <sys/types.h>
#include <mon/monparam.h>
#include <mon/global.h>
#include <mon/nvram.h>
#include <next/reg.h>

main() {
	register struct regs *r = 0;
	register struct nvram_info *n = 0;
	register struct mon_global *mg = 0;

	printf ("#define R_d0 0x%x\n", &r->r_dreg[0]);
	printf ("#define R_d1 0x%x\n", &r->r_dreg[1]);
	printf ("#define R_d2 0x%x\n", &r->r_dreg[2]);
	printf ("#define R_d3 0x%x\n", &r->r_dreg[3]);
	printf ("#define R_d4 0x%x\n", &r->r_dreg[4]);
	printf ("#define R_d5 0x%x\n", &r->r_dreg[5]);
	printf ("#define R_d6 0x%x\n", &r->r_dreg[6]);
	printf ("#define R_d7 0x%x\n", &r->r_dreg[7]);
	printf ("#define R_a0 0x%x\n", &r->r_areg[0]);
	printf ("#define R_a1 0x%x\n", &r->r_areg[1]);
	printf ("#define R_a2 0x%x\n", &r->r_areg[2]);
	printf ("#define R_a3 0x%x\n", &r->r_areg[3]);
	printf ("#define R_a4 0x%x\n", &r->r_areg[4]);
	printf ("#define R_a5 0x%x\n", &r->r_areg[5]);
	printf ("#define R_a6 0x%x\n", &r->r_areg[6]);
	printf ("#define R_a7 0x%x\n", &r->r_areg[7]);
	printf ("#define R_sp 0x%x\n", &r->r_areg[7]);
	printf ("#define R_usp 0x%x\n", &r->r_usp);
	printf ("#define R_isp 0x%x\n", &r->r_isp);
	printf ("#define R_msp 0x%x\n", &r->r_msp);
	printf ("#define R_sfc 0x%x\n", &r->r_sfc);
	printf ("#define R_dfc 0x%x\n", &r->r_dfc);
	printf ("#define R_vbr 0x%x\n", &r->r_vbr);
	printf ("#define R_caar 0x%x\n", &r->r_caar);
	printf ("#define R_cacr 0x%x\n", &r->r_cacr);
	printf ("#define R_crph 0x%x\n", &r->r_crph);
	printf ("#define R_crpl 0x%x\n", &r->r_crpl);
	printf ("#define R_srph 0x%x\n", &r->r_srph);
	printf ("#define R_srpl 0x%x\n", &r->r_srpl);
	printf ("#define R_tc 0x%x\n", &r->r_tc);
	printf ("#define R_tt0 0x%x\n", &r->r_tt0);
	printf ("#define R_tt1 0x%x\n", &r->r_tt1);
	printf ("#define R_ts 0x%x\n", &r->r_ts);
	printf ("#define R_sr 0x%x\n", &r->r_sr);
	printf ("#define R_pc 0x%x\n", &r->r_pc);
	printf ("#define R_fmt 0x%x\n", ((int) &r->r_pc) + sizeof (int));
	printf ("#define R_vor 0x%x\n", ((int) &r->r_pc) + sizeof (int));
	printf ("#define MG_sid 0x%x\n", &mg->mg_sid);
	printf ("#define MG_nofault 0x%x\n", &mg->mg_nofault);
	printf ("#define SIZEOFNVRAM %d\n", sizeof (struct nvram_info));
	printf ("#define N_bootcmd 0x%x\n", n->ni_bootcmd);
	printf ("#define N_potcontrol 0x%x\n", n->ni_pot);
}
