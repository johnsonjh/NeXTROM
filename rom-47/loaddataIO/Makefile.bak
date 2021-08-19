CFILES=	main.c 
OBJS=	main.o
CFLAGS= -O -g

all:	loaddataIO

loaddataIO:    .depend_done ${OBJS}
	$(CC) $(CFLAGS) -o loaddataIO ${OBJS}
	rm -f ../conf/loaddataIO
	install -s loaddataIO ../conf

clean:
	rm -f $(OBJS) loaddataIO .depend_done
		
install: loaddataIO
	
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

