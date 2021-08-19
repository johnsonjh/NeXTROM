# Customization History
#
# 88.12.20 blaw@NeXT (Bassanio Law)
#     Port code to NeXT computer from IBM PC diskette
#     Change compiler flag to -DBSD
#
# Note for NeXT computer user from:
# The RS422 is not capable to generated RTS in RS232 form.
# This means we have to set dip swith 5 on and 8 off.  Consult
# .. the ROMulator document for more information.
#


#   @(#)makefile    2.0
#                   Copyright (c) 1988
#   Grammar Engine, Incorporated,  Westerville, Ohio  USA
#
# Note: To compile and link programs on systems without a program maintenance
#       utility, type the commands below after making the following
#       substitutions:  change the string "$(CC)" to "cc"; change the string
#       "$(FLAGS)" to "-O -DUNIX". etc.
#
#       For BSD dertivatives replace -DUNIX with -DBSD in the FLAGS
#
CFILES=	loadrom.c buildimg.c sysdep.c
OBJS=	loadrom.o buildimg.o sysdep.o
CC=cc
FLAGS= -O -DBSD

all: loadrom

loadrom:    .depend_done ${OBJS}
	$(CC) $(FLAGS) -o loadrom ${OBJS}
	rm -f ../conf/loadrom
	install -s loadrom ../conf

clean:
	rm -f ${OBJS} loadrom .depend_done

install: loadrom
	
loadrom.o: loadrom.c loadrom.h
	$(CC) $(FLAGS) loadrom.c -c

buildimg.o: buildimg.c loadrom.h
	$(CC) $(FLAGS) buildimg.c -c

sysdep.o: sysdep.c loadrom.h
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

loadrom.o: loadrom.h
buildimg.o: loadrom.h
sysdep.o: loadrom.h
