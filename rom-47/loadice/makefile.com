$ set verify
$ ! Makefile for VAX/VMS system
$ ! Copyright 1990
$ ! Grammar Engine, Inc, Columbus, Ohio , USA
$ CC/DEFINE=VMS LOADICE
$ CC/DEFINE=VMS BUILDIMG
$ CC/DEFINE=VMS SYSDEP
$ ! Now link the stuff
$ DEFINE LNK$LIBRARY SYS$LIBRARY:VAXCRTL
$ LINK LOADICE,BUILDIMG,SYSDEP
$ ! End of Makefile
