# Makefile for Open Watcom make utility and compiler.
# Doesn't include the GSM library, or other format libraries
# Also need to create the stconfig.h file (from stconfig.h.in or ../win32/
# stconfig.h), and ststdint.h which can simply include <stdint.h>.

#GSM_SUPPORT = 1
#VORBIS = 1

!ifdef __LOADDLL__
!  loaddll wcc386 wccd386.dll
!  loaddll wlink wlink.dll
!  loaddll wlib wlibd.dll
!endif

CC = wcc386
CFLAGS = -zq -wx -6r -fp6 -fpi87 -oikl+nrt

FOBJ = 8svx.obj adpcm.obj aiff.obj alsa.obj au.obj auto.obj avr.obj cdr.obj &
cvsd.obj dat.obj g711.obj g721.obj g723_16.obj g723_24.obj g723_40.obj &
g72x.obj gsm.obj hcom.obj ima_rw.obj maud.obj mp3.obj nulfile.obj oss.obj &
prc.obj raw.obj sf.obj smp.obj sndrtool.obj sphere.obj sunaudio.obj &
tx16w.obj voc.obj vorbis.obj vox.obj wav.obj wve.obj

EOBJ = avg.obj band.obj bandpass.obj breject.obj btrworth.obj chorus.obj &
compand.obj copy.obj dcshift.obj deemphas.obj earwax.obj echo.obj echos.obj &
biquad.obj tone.obj &
fade.obj FFT.obj filter.obj flanger.obj highp.obj highpass.obj lowp.obj &
lowpass.obj mask.obj mcompand.obj noiseprof.obj noisered.obj phaser.obj &
pitch.obj pan.obj polyphas.obj rate.obj repeat.obj resample.obj reverb.obj &
reverse.obj silence.obj speed.obj stat.obj stretch.obj swap.obj synth.obj &
trim.obj vibro.obj vol.obj

LIBOBJS = $(FOBJ) $(EOBJ) handlers.obj misc.obj stio.obj getopt.obj util.obj
#VORBISLIB = -logg -lvorbis -lvorbisfile -lvorbisenc

all: .SYMBOLIC sox.exe soxmix.exe

LINK_CMD = wlink SYS NT F $[@ L st.lib N $@
sox.exe: sox.obj st.lib
	$(LINK_CMD)
soxmix.exe: soxmix.obj st.lib
	$(LINK_CMD)

sox.obj: sox.c
	$(CC) $[@ -bc -bt=NT $(CFLAGS) -fo=$@
soxmix.obj: sox.c
	$(CC) $[@ -bc -bt=NT -dSOXMIX $(CFLAGS) -fo=$@

st.lib: $(LIBOBJS)
	%create st.lbc
	FOR %o IN ($(LIBOBJS)) DO @%append st.lbc +%o
	wlib -n -b -c $@ @st.lbc

C_CMD = $(CC) $[@ -bt=NT $(CFLAGS) -fo=$@
.c.obj:
	$(C_CMD)

clean: .PROCEDURE
	del *.obj
	del *.err
	del st.lib
