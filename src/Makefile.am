## Process this file with automake to produce Makefile.in

RM = rm -f 

AM_CFLAGS = @SNDFILE_CFLAGS@ @SAMPLERATE_CFLAGS@ -Wconversion
AM_LDFLAGS = @SNDFILE_LIBS@ @SAMPLERATE_LIBS@

# Pass flags from --enable-silent-libtool
LIBTOOL = @LIBTOOL@ @LIBTOOLFLAGS@

bin_PROGRAMS = sox
bin_SCRIPTS = sox-config
lib_LTLIBRARIES = libsox.la
include_HEADERS = sox.h soxstdint.h

formats = 8svx.c adpcm.c adpcm.h adpcms.c adpcms.h aiff.c au.c auto.c avr.c \
	  cdr.c cvsd.c cvsdfilt.h dat.c flac.c g711.c g711.h g721.c g723_24.c \
	  g723_40.c g72x.c g72x.h gsm.c hcom.c ima_rw.c ima_rw.h \
	  maud.c mp3.c nulfile.c prc.c raw.c sf.c sfircam.h skelform.c smp.c \
	  sndfile.c sndrtool.c sphere.c tx16w.c voc.c vorbis.c vox.c wav.c \
	  wav.h wve.c xa.c amr-wb.c

effects = band.h biquad.c biquad.h biquads.c chorus.c compand.c \
	  compandt.c compandt.h dcshift.c\
	  dither.c earwax.c echo.c echos.c fade.c FFT.c FFT.h filter.c\
	  flanger.c mcompand.c mixer.c noiseprof.c \
	  noisered.c noisered.h pad.c pan.c phaser.c pitch.c polyphas.c \
	  rabbit.c rate.c repeat.c resample.c reverb.c reverse.c silence.c \
	  skeleff.c speed.c stat.c stretch.c swap.c synth.c synth.h tremolo.c \
	  trim.c vibro.c vol.c

libsox_la_SOURCES = $(formats) $(effects) alsa.c oss.c sunaudio.c handlers.c \
		   misc.c sox_i.h soxio.c util.c xmalloc.c xmalloc.h \
		   getopt.c getopt1.c getopt.h soxconfig.h
libsox_la_LIBADD = @LIBGSM_LIBADD@

sox_SOURCES = sox.c
sox_LDADD = libsox.la ../amr-wb/libamrwb.la

EXTRA_DIST = tests.sh testall.sh tests.bat testall.bat monkey.au monkey.wav

all: sox$(EXEEXT) play rec

libgsm/libgsm.la:
	$(MAKE) -C libgsm

play rec: sox$(EXEEXT)
	./sox --help > /dev/null
	test -f .libs/lt-sox && ( $(RM) $@ && $(LN_S) .libs/lt-sox $@ ) || $(LN_S) sox$(EXEEXT) $@

install-exec-hook:
	cd $(DESTDIR)$(bindir) && $(RM) play && $(LN_S) sox play
	cd $(DESTDIR)$(bindir) && $(RM) rec && $(LN_S) sox rec

clean-local:
	$(RM) play rec

distclean-local:
	$(RM) *.raw *.sf ststdint.h
