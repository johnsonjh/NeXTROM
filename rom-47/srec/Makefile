CFILES=	srec.c
OBJS=	srec.o
CFLAGS= -O

all:	srec

srec:	.depend_done ${OBJS}
	$(CC) -o srec ${OBJS}
	rm -f ../conf/srec
	install -s srec ../conf

clean:
	rm -f ${OBJS} srec .depend_done

install: srec

depend .depend_done:
	grep '^#include' ${CFILES} | grep -v '<' | \
	sed -e 's/:[^"]*"\([^"]*\)".*/: \1/' \
	    -e 's/\.c/.o/' \
	    -e 's,../[a-zA-Z]*/,,' | \
	awk ' { if ($$1 != prev) { print rec; rec = $$0; prev = $$1; } \
		else { if (length(rec $$2) > 78) { print rec; rec = $$0; } \
		       else rec = rec " " $$2 } } \
	      END { print rec } ' > makedep
	echo '$$r makedep' >>eddep
	echo '/^# DO NOT DELETE THIS LINE/+1,$$d' >eddep
	echo '$$r makedep' >>eddep
	echo 'w' >>eddep
	rm -f Makefile.bak
	cp Makefile Makefile.bak
	chmod +w Makefile
	ed - Makefile < eddep
	rm eddep makedep
	touch .depend_done

# DO NOT DELETE THIS LINE -- make depend uses it

#include "exec.h"
