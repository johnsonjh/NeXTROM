/*	@(#)loader.c	1.0	05/09/89	(c) 1989 NeXT	*/

#define	A_OUT_COMPAT	1

#include <sys/types.h>
#include <sys/loader.h>
#if	A_OUT_COMPAT
#include <mon/exec.h>
#endif	A_OUT_COMPAT

/* loads both object file formats: a.out & mach.o */

loader (bp, len, entry, loadp)
	int *len;
	char *bp;
	char **entry, **loadp;
{
#if	A_OUT_COMPAT
	struct exec *ap = (struct exec*) bp;
#endif	A_OUT_COMPAT
	struct mach_header *mh = (struct mach_header*) bp;
	struct segment_command *sc;
	int total_size, cmd, first_seg = 0;

#if	A_OUT_COMPAT
	if (ap->a_magic == OMAGIC) {
		total_size = ap->a_text + ap->a_data;
		*loadp = *entry = (char*) ap->a_entry;
		bp += sizeof (struct exec);
		*len -= sizeof (struct exec);
	} else
#endif	A_OUT_COMPAT
	if (mh->magic == MH_MAGIC && mh->filetype == MH_PRELOAD) {
		sc = (struct segment_command*)
			(bp + sizeof (struct mach_header));
		first_seg = 1;
		total_size = 0;
		
		/*
		 *  Only look at the first two segments (assumes that text and data
		 *  sections will reside there).  Can't look at all of them because
		 *  the header probably exceeds the buffer size.
		 */
		for (cmd = 0; cmd < 2; cmd++) {
			switch (sc->cmd) {
			
			case LC_SEGMENT:
				if (first_seg) {
					*entry = *loadp = (char*) sc->vmaddr;
					bp += sc->fileoff;
					*len -= sc->fileoff;
					first_seg = 0;
				}
				total_size += sc->filesize;
				break;
			}
			sc = (struct segment_command*)
				((int)sc + sc->cmdsize);
		}
	} else
		goto bad;
	if (*len > 0) {
		bcopy (bp, *loadp, *len);
		*loadp += *len;
		total_size -= *len;
	}
	return (total_size);
bad:
	printf ("unknown binary format\n");
	return (0);
}


