#
# MMS description file for SOX/SoundTools (and Gopstein/Harris sound2sun)
#
# Modification History
# 12 Dec 1992, K. S. Kubo, Created
#
# NOTES (todo):
#	* This does not yet provide support for VMS distribution (e.g. shar
#	  target).
#	* It may be nice to link the library as a shareable image.
#	* To do this "right" this should also provide support for sounds
#	  in the DDIF format... someday, maybe.
#
# !!!!!!!! IMPORTANT !!!!!!!!!! This file is outdated.  Please refer
# to Makefile.unx to see which source files need to be compiled
# and update accordingly.  Please send any updates to cbagwell@sprynet.com

.IFDEF DEBUG
DEBUGFLAGS	= /debug/nooptimize
LINKDBGFLAGS	= /nouserlibrary/traceback/debug
.ELSE
DEBUGFLAGS	= /nodebug/optimize
LINKDBGFLAGS	= /nouserlibrary/notraceback/nodebug
.ENDIF

CC		= cc
CFLAGS		= /object=$*.OBJ$(DEBUGFLAGS)
LINK		= link
LINKFLAGS	= /executable=$*.EXE$(LINKDBGFLAGS)


FSRC	= 	raw.c, \
	  	voc.c, \
		au.c, \
		sf.c, \
	  	aiff.c, \
		hcom.c, \
		8svx.c, \
		sndrtool.c, \
		wav.c, \
		sbdsp.c, \
		sunaduo.c, \
		oss.c, \
		smp.c, \
		auto.c

ESRC	=	copy.c, \
		avg.c, \
		stat.c, \
		vibro.c, \
		echo.c, \
		rate.c, \
		band.c, \
		lowp.c, \
		highp.c, \
		reverse.c, \
		dyn.c, \
		cut.c, \
		map.c, \
		split.c, \
		pick.c, \
		mask.c, \
		resample.c

PSRC	=	sox.c

OSRC	=	sound2sun.c

SOURCES = 	$(FSRC),$(ESRC),$(PSRC), \
		handlers.c, libst.c, misc.c, getopt.c, \
		$(OSRC)

HDRS	=	st.h, \
		libst.h, \
		sfheader.h, \
		sfircam.h, \
		patchlevel.h, \
		version.h, \
		wav.h, \
		g72x.h,\
		resdefs.h, \
		resampl.h

TESTS	=	tests.sh, \
		testall.sh, \
		monkey.au, \
		monkey.voc

MISC    = 	readme., readme2., install., todo, tips, cheat, sox.1, \
		sox.txt, libst.3, libst.txt, makefile.unx, makefile.bor, \
		Makefile.b30, Makefile.c70, soxeffect, play, rec

VMS	=	descrip.mms, sox.opt, vms.lis, sound2au.com, sound2sun.opt, \
		sound2sun.c, tests.com

SKEL	  = skel.c skeleff.c

SOUNDLIB  =	soundtools.olb

LIBMODS	= \
    $(SOUNDLIB)(raw) \
    $(SOUNDLIB)(voc) \
    $(SOUNDLIB)(au) \
    $(SOUNDLIB)(sf) \
    $(SOUNDLIB)(aiff) \
    $(SOUNDLIB)(hcom) \
    $(SOUNDLIB)(8svx) \
    $(SOUNDLIB)(sndrtool) \
    $(SOUNDLIB)(wav) \
    $(SOUNDLIB)(sbdsp) \
    $(SOUNDLIB)(sunaudio) \
    $(SOUNDLIB)(oss) \
    $(SOUNDLIB)(smp) \
    $(SOUNDLIB)(auto) \
    $(SOUNDLIB)(copy) \
    $(SOUNDLIB)(avg) \
    $(SOUNDLIB)(stat) \
    $(SOUNDLIB)(vibro) \
    $(SOUNDLIB)(echo) \
    $(SOUNDLIB)(rate) \
    $(SOUNDLIB)(band) \
    $(SOUNDLIB)(lowp) \
    $(SOUNDLIB)(reverse) \
    $(SOUNDLIB)(handlers) \
    $(SOUNDLIB)(libst) \
    $(SOUNDLIB)(misc) \
    $(SOUNDLIB)(getopt)

.FIRST
    @ if F$TrnLnm("VAXC$INCLUDE") .eqs. "" then define VAXC$INCLUDE sys$library
    @ if F$TrnLnm("SYS") .eqs. "" then define SYS sys$library

#
# Actual targets
#
all : sox.exe sound2sun.exe
    @ ! dummy argument

clean :
    - delete *.obj;
    - delete *.raw;
    - delete *.sf;

depend : $(HDRS) $(SOURCES)
    set command/replace clddir:depend
    depend $(SOURCES)
    ! dependencies updated

sox.exe : sox.obj $(SOUNDLIB) descrip.mms sox.opt
    $(LINK) $(LINKFLAGS) sox.obj, sox.opt/options

sound2sun.exe : sound2sun.obj descrip.mms sound2sun.opt
    $(LINK) $(LINKFLAGS) sound2sun.obj, sound2sun.opt/options

$(SOUNDLIB) : $(LIBMODS)
    ! $(SOUNDLIB) updated

#DO NOT DELETE THIS LINE!

raw.obj : libst.h
raw.obj : raw.c
raw.obj : st.h
raw.obj : sys$library:stddef.h
raw.obj : sys$library:stdio.h
voc.obj : st.h
voc.obj : voc.c
voc.obj : sys$library:stddef.h
voc.obj : sys$library:stdio.h
au.obj : au.c
au.obj : st.h
au.obj : sys$library:stddef.h
au.obj : sys$library:stdio.h
sf.obj : sf.c
sf.obj : sfheader.h
sf.obj : st.h
sf.obj : sys$library:stddef.h
sf.obj : sys$library:stdio.h
aiff.obj : aiff.c
aiff.obj : st.h
aiff.obj : sys$library:math.h
aiff.obj : sys$library:stddef.h
aiff.obj : sys$library:stdio.h
hcom.obj : hcom.c
hcom.obj : st.h
hcom.obj : sys$library:stddef.h
hcom.obj : sys$library:stdio.h
8svx.obj : 8svx.c
8svx.obj : st.h
8svx.obj : sys$library:errno.h
8svx.obj : sys$library:math.h
8svx.obj : sys$library:perror.h
8svx.obj : sys$library:stddef.h
8svx.obj : sys$library:stdio.h
8svx.obj : sys:types.h
sndrtool.obj : sndrtool.c
sndrtool.obj : st.h
sndrtool.obj : sys$library:errno.h
sndrtool.obj : sys$library:math.h
sndrtool.obj : sys$library:perror.h
sndrtool.obj : sys$library:stddef.h
sndrtool.obj : sys$library:stdio.h
wav.obj : st.h
wav.obj : wav.c
wav.obj : sys$library:stddef.h
wav.obj : sys$library:stdio.h
sbdsp.obj : sbdsp.c
smp.obj : st.h
smp.obj : smp.c
smp.obj : sys$library:stddef.h
smp.obj : sys$library:stdio.h
smp.obj : sys$library:string.h
auto.obj : st.h
auto.obj : wav.c
auto.obj : sys$library:stddef.h
auto.obj : sys$library:stdio.h
copy.obj : copy.c
copy.obj : st.h
copy.obj : sys$library:stddef.h
copy.obj : sys$library:stdio.h
avg.obj : avg.c
avg.obj : st.h
avg.obj : sys$library:stddef.h
avg.obj : sys$library:stdio.h
stat.obj : st.h
stat.obj : stat.c
stat.obj : sys$library:stddef.h
stat.obj : sys$library:stdio.h
vibro.obj : st.h
vibro.obj : vibro.c
vibro.obj : sys$library:math.h
vibro.obj : sys$library:stddef.h
vibro.obj : sys$library:stdio.h
echo.obj : echo.c
echo.obj : st.h
echo.obj : sys$library:math.h
echo.obj : sys$library:stddef.h
echo.obj : sys$library:stdio.h
rate.obj : rate.c
rate.obj : st.h
rate.obj : sys$library:math.h
rate.obj : sys$library:stddef.h
rate.obj : sys$library:stdio.h
band.obj : band.c
band.obj : st.h
band.obj : sys$library:math.h
band.obj : sys$library:stddef.h
band.obj : sys$library:stdio.h
lowp.obj : lowp.c
lowp.obj : st.h
lowp.obj : sys$library:math.h
lowp.obj : sys$library:stddef.h
lowp.obj : sys$library:stdio.h
reverse.obj : reverse.c
reverse.obj : st.h
reverse.obj : sys$library:math.h
reverse.obj : sys$library:stddef.h
reverse.obj : sys$library:stdio.h
sox.obj : sox.c
sox.obj : st.h
sox.obj : sys$library:errno.h
sox.obj : sys$library:ctype.h
sox.obj : sys$library:perror.h
sox.obj : sys$library:stat.h
sox.obj : sys$library:stddef.h
sox.obj : sys$library:stdio.h
sox.obj : sys$library:string.h
sox.obj : sys$library:varargs.h
sox.obj : sys:types.h
handlers.obj : handlers.c
handlers.obj : st.h
handlers.obj : sys$library:stddef.h
handlers.obj : sys$library:stdio.h
libst.obj : libst.c
misc.obj : misc.c
misc.obj : st.h
misc.obj : sys$library:stddef.h
misc.obj : sys$library:stdio.h
getopt.obj : getopt.c
getopt.obj : st.h
getopt.obj : sys$library:stddef.h
getopt.obj : sys$library:stdio.h
sound2sun.obj : sound2sun.c
sound2sun.obj : sys$library:stddef.h
sound2sun.obj : sys$library:stdio.h
