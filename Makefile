SUBDIR=	config loadice loadrom srec srec2crc loaddataIO conf
SRCFILES= Makefile README bitmaps boot conf config dev diag loadice \
	loadrom mon pot srec srec2crc subr sys loaddataIO

all clean depend:
	 @for i in ${SUBDIR}; \
	 do \
		echo ================= make $@ for $$i =================; \
		(cd $$i; ${MAKE} $@) || exit $?; \
	 done

install:	DSTROOT
	@CWD=`pwd`; cd ${DSTROOT}; DSTROOT=`pwd`; cd $$CWD; \
	for i in ${SUBDIR}; \
	do \
		echo ================= make $@ for $$i =================; \
		(cd $$i; ${MAKE} DSTROOT=$$DSTROOT $@) || exit $?; \
	done

installsrc:	SRCROOT
	tar cf - ${SRCFILES} | (cd ${SRCROOT}; tar xfBp -)

SRCROOT DSTROOT:
	@if [ -n "${$@}" ]; \
	then \
		exit 0; \
	else \
		echo Must define $@; \
		exit 1; \
	fi
