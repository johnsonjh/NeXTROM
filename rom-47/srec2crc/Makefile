CFILES=	main.c calculateCRC.s
OBJS=	main.o calculateCRC.o
CFLAGS= -O

all:	srec2crc

srec2crc:    .depend_done ${OBJS}
	$(CC) $(CFLAGS) -o srec2crc ${OBJS}
	rm -f ../conf/srec2crc
	install -s srec2crc ../conf

calculateCRC.o: calculateCRC.s
	-ln -s calculateCRC.s calculateCRC.c
	${CC} -E ${COPTS} calculateCRC.c >calculateCRC.p
	${AS} ${AFLAGS} calculateCRC.p -o calculateCRC.o
	rm -f calculateCRC.p calculateCRC.c
clean:
	rm -f $(OBJS) srec2crc .depend_done
		
install: srec2crc
	
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

