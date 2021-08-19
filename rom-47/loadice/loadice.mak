PROJ = LOADICE
PROJFILE = LOADICE.MAK
DEBUG = 0

PWBRMAKE  = pwbrmake
NMAKEBSC1  = set
NMAKEBSC2  = nmake
CC	= cl
CFLAGS_G  = /W4 /DANSI /BATCH
CFLAGS_D  = /Od /Zi /Gs
CFLAGS_R  = /Ot /Ol /Og /Oe /Oi /Ow /Gs
ASM  = masm
AFLAGS_G  = /Mx /T
AFLAGS_D  = /Zi
LINKER	= link
ILINK  = ilink
LRF  = echo > NUL
BIND  = bind
RC	= rc
IMPLIB	= implib
LFLAGS_G  = /NOI  /BATCH
LFLAGS_D  = /CO /NOF /NOP
LFLAGS_R  = /E /F /PACKC
MAPFILE_D  = NUL
MAPFILE_R  = NUL
BRFLAGS  = /o $(PROJ).bsc
BROWSE	= 0
RUNFLAGS  = -d -r 27040 -v -x 
CVFLAGS  = /25 /F /R

OBJS  = BUILDIMG.obj LOADICE.obj SYSDEP.obj
SBRS  = BUILDIMG.sbr LOADICE.sbr SYSDEP.sbr

all: $(PROJ).exe

.SUFFIXES: .c .obj .sbr

BUILDIMG.obj : BUILDIMG.C loadice.h proto.h c:\include\string.h\
		c:\include\stdio.h

BUILDIMG.sbr : BUILDIMG.C loadice.h proto.h c:\include\string.h\
		c:\include\stdio.h

LOADICE.obj : LOADICE.C c:\include\stdio.h c:\include\io.h c:\include\errno.h\
		c:\include\stdlib.h c:\include\string.h c:\include\conio.h loadice.h\
		proto.h

LOADICE.sbr : LOADICE.C c:\include\stdio.h c:\include\io.h c:\include\errno.h\
		c:\include\stdlib.h c:\include\string.h c:\include\conio.h loadice.h\
		proto.h

SYSDEP.obj : SYSDEP.C c:\include\stdio.h c:\include\malloc.h\
		c:\include\conio.h c:\include\fcntl.h c:\include\sys\types.h\
		c:\include\sys\stat.h c:\include\string.h c:\include\io.h\
		c:\include\time.h loadice.h proto.h

SYSDEP.sbr : SYSDEP.C c:\include\stdio.h c:\include\malloc.h\
		c:\include\conio.h c:\include\fcntl.h c:\include\sys\types.h\
		c:\include\sys\stat.h c:\include\string.h c:\include\io.h\
		c:\include\time.h loadice.h proto.h


$(PROJ).bsc : $(SBRS)
		$(PWBRMAKE) @<<
$(BRFLAGS) $(SBRS)
<<

$(PROJ).exe : $(OBJS)
!IF $(DEBUG)
		$(LRF) @<<$(PROJ).lrf
$(RT_OBJS: = +^
) $(OBJS: = +^
)
$@
$(MAPFILE_D)
$(LLIBS_G: = +^
) +
$(LLIBS_D: = +^
) +
$(LIBS: = +^
)
$(DEF_FILE) $(LFLAGS_G) $(LFLAGS_D);
<<
!ELSE
		$(LRF) @<<$(PROJ).lrf
$(RT_OBJS: = +^
) $(OBJS: = +^
)
$@
$(MAPFILE_R)
$(LLIBS_G: = +^
) +
$(LLIBS_R: = +^
) +
$(LIBS: = +^
)
$(DEF_FILE) $(LFLAGS_G) $(LFLAGS_R);
<<
!ENDIF
!IF $(DEBUG)
		$(LINKER) @$(PROJ).lrf
!ELSE
		$(LINKER) @$(PROJ).lrf
!ENDIF


.c.obj :
!IF $(DEBUG)
		$(CC) /c $(CFLAGS_G) $(CFLAGS_D) /Fo$@ $<
!ELSE
		$(CC) /c $(CFLAGS_G) $(CFLAGS_R) /Fo$@ $<
!ENDIF

.c.sbr :
!IF $(DEBUG)
		$(CC) /Zs $(CFLAGS_G) $(CFLAGS_D) /FR$@ $<
!ELSE
		$(CC) /Zs $(CFLAGS_G) $(CFLAGS_R) /FR$@ $<
!ENDIF


run: $(PROJ).exe
		$(PROJ).exe $(RUNFLAGS)

debug: $(PROJ).exe
		CV $(CVFLAGS) $(PROJ).exe $(RUNFLAGS)
