BUILDDIR= RELEASE040

all depend: ../$(BUILDDIR)/mon
	cd ../$(BUILDDIR); ${MAKE} $@

clean:
	-rm -rf ../$(BUILDDIR)
	-mkdir ../$(BUILDDIR)

../$(BUILDDIR)/Makefile: Makefile Makefile.next files files.next
	-rm -f ../$(BUILDDIR)/Makefile ../$(BUILDDIR)/Makedep
	-touch ../$(BUILDDIR)/Makedep
	./config $(BUILDDIR)

../$(BUILDDIR)/mon: ../$(BUILDDIR)/Makefile
	cd ../$(BUILDDIR); ${MAKE} all

install: ../$(BUILDDIR)/mon
	cd ../$(BUILDDIR); ${MAKE} DSTROOT=$$DSTROOT install

