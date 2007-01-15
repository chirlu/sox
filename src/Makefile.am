## Process this file with automake to produce Makefile.in

SUBDIRS = libgsm

AM_CFLAGS = $(LUA_INCLUDES)
AM_LDFLAGS = @SAMPLERATE_LIBS@

# Pass flags from --enable-silent-libtool
LIBTOOL = @LIBTOOL@ @LIBTOOLFLAGS@

bin_PROGRAMS = sox
bin_SCRIPTS = libst-config
lib_LTLIBRARIES = libst.la
include_HEADERS = st.h stconfig.h ststdint.h

formats = 8svx.c adpcm.c adpcm.h adpcms.c adpcms.h aiff.c au.c auto.c avr.c \
	  cdr.c cvsd.c cvsdfilt.h dat.c flac.c g711.c g711.h g721.c \
	  g723_16.c g723_24.c g723_40.c g72x.c g72x.h gsm.c hcom.c \
	  ima_rw.c ima_rw.h maud.c mp3.c nulfile.c prc.c \
	  raw.c sf.c sfircam.h skelform.c smp.c sndrtool.c sphere.c tx16w.c voc.c \
	  vorbis.c vox.c wav.c wav.h wve.c xa.c

effects = avg.c band.c bandpass.c biquad.c biquad.h breject.c btrworth.c \
	  btrworth.h chorus.c compand.c dcshift.c deemphas.c earwax.c \
	  echo.c echos.c equalizer.c fade.c FFT.c FFT.h filter.c flanger.c \
	  highp.c highpass.c lowp.c lowpass.c luaeff.c luaform.c \
	  lintlib.c mask.c mcompand.c noiseprof.c noisered.c noisered.h \
	  pad.c pan.c phaser.c pitch.c polyphas.c rabbit.c rate.c repeat.c \
	  resample.c reverb.c reverse.c silence.c skeleff.c speed.c stat.c \
	  stretch.c swap.c synth.c tone.c trim.c vibro.c vol.c

libst_la_SOURCES = $(formats) $(effects) alsa.c oss.c sunaudio.c handlers.c misc.c \
	  st_i.h stio.c stlua.c util.c xmalloc.c xmalloc.h getopt.c getopt1.c getopt.h

sox_SOURCES = sox.c
sox_LDADD = libst.la libgsm/libgsm.la $(LUA_LIBS)

all: sox play rec

libgsm/libgsm.la:
	$(MAKE) -C libgsm

play rec: sox
	sox > /dev/null 2>&1
	ln -sf .libs/lt-sox rec
	ln -sf .libs/lt-sox play

install-exec-hook:
	cd $(DESTDIR)$(bindir) && $(RM) play && $(LN_S) sox play
	cd $(DESTDIR)$(bindir) && $(RM) rec && $(LN_S) sox rec

distclean-local:
	$(RM) *.raw *.sf ststdint.h
