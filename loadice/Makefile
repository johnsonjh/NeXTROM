# Customization History
#
# 10-1-90 mpaque@NeXT (Mike Paquette)
#     Port code to NeXT computer from IBM PC diskette
#     Change compiler flag to -DBSD -bsd (gcc compatability mode)
#
# 10-16-90 blaw@NeXT (Bassanio Law)
#     Fixed bugs in non-blocking I/O routines in sysdep.c
#

#   @(#)makefile    1.0
#                   Copyright (c) 1990
#   Grammar Engine, Incorporated,  Columbus, Ohio  USA
#
# Note: To compile and link programs on systems without a program maintenance
#       utility, type the commands below after making the following
#       substitutions:  change the string "$(CC)" to "cc"; change the string
#       "$(FLAGS)" to "-O -DUNIX". etc.
#
#       For BSD dertivatives replace -DATNT with -DBSD in the FLAGS
#
CFILES=	loadice.c buildimg.c sysdep.c
OBJS=	loadice.o buildimg.o sysdep.o
CC=cc
FLAGS= -O -g -bsd -DUNIX -DPI_BLOCK -DBSD

all: loadice

loadice:    loadice.o buildimg.o sysdep.o
	$(CC) $(FLAGS) $(OBJS) -o loadice
	rm -f ../conf/loadice
	install -s loadice ../conf

clean:
	rm -f ${OBJS} loadice .depend_done

install: loadice
	
loadice.o: loadice.c loadice.h
	$(CC) $(FLAGS) loadice.c -c

buildimg.o: buildimg.c loadice.h
	$(CC) $(FLAGS) buildimg.c -c

sysdep.o: sysdep.c loadice.h
	$(CC) $(FLAGS) sysdep.c -c

	
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
