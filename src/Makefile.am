## Process this file with automake to produce Makefile.in

SUBDIRS = libgsm

bin_PROGRAMS = sox
bin_SCRIPTS = play libst-config
lib_LIBRARIES = libst.a
include_HEADERS = st.h stconfig.h ststdint.h

formats = 8svx.c adpcm.c adpcm.h aiff.c au.c auto.c avr.c cdr.c \
	  cvsd.c cvsdfilt.h dat.c flac.c g711.c g711.h g721.c \
	  g723_16.c g723_24.c g723_40.c g72x.c g72x.h gsm.c hcom.c \
	  ima_rw.c ima_rw.h maud.c mp3.c nulfile.c prc.c raw.c \
	  sf.c sfircam.h smp.c sndrtool.c sphere.c tx16w.c voc.c vorbis.c \
	  vox.c wav.c wav.h wve.c xa.c

effects = avg.c band.c bandpass.c biquad.c biquad.h breject.c btrworth.c \
	  btrworth.h chorus.c compand.c copy.c dcshift.c deemphas.c earwax.c \
	  echo.c echos.c equalizer.c fade.c FFT.c FFT.h filter.c flanger.c \
	  highp.c highpass.c lowp.c lowpass.c lua.c lintlib.c mask.c \
	  mcompand.c noiseprof.c noisered.c noisered.h pan.c \
	  phaser.c pitch.c polyphas.c rabbit.c rate.c repeat.c resample.c \
	  reverb.c reverse.c silence.c speed.c stat.c \
	  stretch.c swap.c synth.c tone.c trim.c vibro.c vol.c

libst_a_SOURCES = $(formats) $(effects) alsa.c oss.c sunaudio.c handlers.c misc.c \
	  st_i.h stio.c util.c xmalloc.c xmalloc.h getopt.c getopt1.c getopt.h

AM_CFLAGS = $(LUA_INCLUDES)
AM_LDFLAGS = $(LUA_LIBS) @SAMPLERATE_LIBS@

sox_SOURCES = sox.c
sox_LDADD = libst.a libgsm/libgsm.a

all: sox soxmix play rec

libgsm/libgsm.a:
	$(MAKE) -C libgsm

soxmix: sox
	$(RM) soxmix && $(LN_S) sox soxmix

rec: play
	$(RM) play && $(LN_S) play rec

install-exec-hook:
	cd $(DESTDIR)$(bindir) && \
	$(RM) soxmix && $(LN_S) sox soxmix && \
	$(RM) rec && $(LN_S) play rec

distclean-local:
	$(RM) *.raw *.sf ststdint.h
