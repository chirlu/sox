## Process this file with automake to produce Makefile.in

RM = rm -f 

AM_CPPFLAGS = -DLADSPA_PATH="\"@LADSPA_PATH@\""
AM_CFLAGS = @WARN_CFLAGS@ #-Werror

if HAVE_LIBLTDL
# This is being used as a short cut to turn off versioning of ALL dynamic
# fmt libraries.  If any fmt ever needs to add a specific LDFLAGS
# then it will need to also add -avoid-version because AM_LDFLAGS
# is ignored when you specify a more specific one.
# We want to version libsox and we are OK because they
# have a more specific LDFLAGS that includes -version-info.
AM_LDFLAGS = -avoid-version -module
AM_CPPFLAGS += -DPKGLIBDIR="\"$(pkglibdir)\""
endif
if HAVE_WIN32_LTDL
# Must match line above.
AM_LDFLAGS = -avoid-version -module
AM_CPPFLAGS += -DPKGLIBDIR="\"$(pkglibdir)\""
endif

# Pass flags from --enable-silent-libtool
LIBTOOL = @LIBTOOL@ @LIBTOOLFLAGS@

if HAVE_DISTRO
AM_CPPFLAGS += -DDISTRO="\"@DISTRO@\""
endif



#########################
# SoX - the application #
#########################

bin_PROGRAMS = sox
EXTRA_PROGRAMS = example0 example1 example2 example3 example4 example5 sox_sample_test
lib_LTLIBRARIES = libsox.la
include_HEADERS = sox.h
sox_SOURCES = sox.c
if HAVE_WIN32_GLOB
sox_SOURCES += win32-glob.c win32-glob.h
endif
sox_LDADD = libsox.la
example0_SOURCES = example0.c
example1_SOURCES = example1.c
example2_SOURCES = example2.c
example3_SOURCES = example3.c
example4_SOURCES = example4.c
example5_SOURCES = example5.c
sox_sample_test_SOURCES = sox_sample_test.c sox_sample_test.h



######################################################
# libsox - file format, effects, and utility library #
######################################################

# Format handlers and utils source
libsox_la_SOURCES = adpcms.c adpcms.h aiff.c aiff.h cvsd.c cvsd.h cvsdfilt.h \
	  g711.c g711.h g721.c g723_24.c g723_40.c g72x.c g72x.h vox.c vox.h \
	  raw.c raw.h formats.c formats.h formats_i.c sox_i.h skelform.c \
	  xmalloc.c xmalloc.h getopt.c \
	  util.c util.h libsox.c libsox_i.c sox-fmt.c soxomp.h

# Effects source
libsox_la_SOURCES += \
	band.h bend.c biquad.c biquad.h biquads.c chorus.c compand.c \
	compandt.c compandt.h contrast.c dcshift.c delay.c dft_filter.c \
	dft_filter.h dither.c dither.h divide.c downsample.c earwax.c \
	echo.c echos.c effects.c effects.h effects_i.c effects_i_dsp.c \
	fade.c fft4g.c fft4g.h fifo.h fir.c firfit.c flanger.c gain.c \
	hilbert.c input.c ladspa.h ladspa.c loudness.c mcompand.c \
	mcompand_xover.h mixer.c noiseprof.c noisered.c \
	noisered.h output.c overdrive.c pad.c phaser.c rate.c \
	rate_filters.h rate_half_fir.h rate_poly_fir0.h rate_poly_fir.h \
	remix.c repeat.c reverb.c reverse.c silence.c sinc.c skeleff.c \
	speed.c splice.c stat.c stats.c stretch.c swap.c \
	synth.c tempo.c tremolo.c trim.c upsample.c vad.c vol.c \
	ignore-warning.h
if HAVE_PNG
    libsox_la_SOURCES += spectrogram.c
endif

# Libraries required by libsox for file handlers, effects, or utils;
# regardless if libltdl is used or not.
libsox_la_LIBADD = @PNG_LIBS@
if HAVE_MAGIC
libsox_la_LIBADD += @MAGIC_LIBS@
endif

if HAVE_OPENMP
libsox_la_LIBADD += @GOMP_LIBS@
endif

libsox_la_CFLAGS = @WARN_CFLAGS@
libsox_la_LDFLAGS = @APP_LDFLAGS@ -version-info @SHLIB_VERSION@

if HAVE_WIN32_LTDL
  libsox_la_SOURCES += win32-ltdl.c win32-ltdl.h
endif

if HAVE_LIBLTDL
  libsox_la_CFLAGS += $(LTDLINCL) 
  libsox_la_LDFLAGS += $(LIBLTDL) 
endif



#########################
# libsox - File Formats #
#########################

# Uncomment for bit-rot detection on linux
#libsox_la_SOURCES += coreaudio.c sndio.c sunaudio.c
#libsox_la_CFLAGS += -Ibit-rot

libsox_la_SOURCES += raw-fmt.c s1-fmt.c s2-fmt.c s3-fmt.c \
  s4-fmt.c u1-fmt.c u2-fmt.c u3-fmt.c u4-fmt.c al-fmt.c la-fmt.c ul-fmt.c \
  lu-fmt.c 8svx.c aiff-fmt.c aifc-fmt.c au.c avr.c cdr.c cvsd-fmt.c \
  dvms-fmt.c dat.c hcom.c htk.c maud.c prc.c sf.c smp.c \
  sounder.c soundtool.c sphere.c tx16w.c voc.c vox-fmt.c ima-fmt.c adpcm.c adpcm.h \
  ima_rw.c ima_rw.h wav.c wve.c xa.c nulfile.c f4-fmt.c f8-fmt.c gsrt.c

libsox_la_LIBADD += @GSM_LIBS@ @LIBGSM_LIBADD@
libsox_la_LIBADD += @LPC10_LIBS@ @LIBLPC10_LIBADD@

pkglib_LTLIBRARIES =

include optional-fmts.am



# example programs will need same link options as sox does.
example0_LDADD = ${sox_LDADD}
example1_LDADD = ${sox_LDADD}
example2_LDADD = ${sox_LDADD}
example3_LDADD = ${sox_LDADD}
example4_LDADD = ${sox_LDADD}
example5_LDADD = ${sox_LDADD}

EXTRA_DIST = monkey.wav optional-fmts.am \
	     CMakeLists.txt soxconfig.h.cmake \
	     tests.sh testall.sh tests.bat testall.bat test-comments

all: sox$(EXEEXT) play rec soxi sox_sample_test$(EXEEXT) example0$(EXEEXT) example1$(EXEEXT) example2$(EXEEXT) example3$(EXEEXT) example4$(EXEEXT) example5$(EXEEXT)

play rec: sox$(EXEEXT)
	if test "$(PLAYRECLINKS)" = "yes"; then	\
		test -f sox$(EXEEXT) && ( $(RM) $@ && $(LN_S) sox$(EXEEXT) $@ ) || $(LN_S) sox $@; \
	fi

soxi: sox$(EXEEXT)
	if test "$(SYMLINKS)" = "yes"; then	\
		test -f sox$(EXEEXT) && ( $(RM) $@ && $(LN_S) sox$(EXEEXT) $@ ) || $(LN_S) sox $@; \
	fi

install-exec-hook:
	if test "$(PLAYRECLINKS)" = "yes"; then	\
		cd $(DESTDIR)$(bindir); $(RM) play rec; $(LN_S) sox play; $(LN_S) sox rec; \
	fi
	if test "$(SYMLINKS)" = "yes"; then	\
		cd $(DESTDIR)$(bindir); $(RM) soxi; $(LN_S) sox soxi; \
	fi

uninstall-hook:
	if test "$(PLAYRECLINKS)" = "yes"; then	\
		cd $(DESTDIR)$(bindir); $(RM) play rec; \
	fi
	if test "$(SYMLINKS)" = "yes"; then	\
		cd $(DESTDIR)$(bindir); $(RM) soxi; \
	fi

clean-local:
	$(RM) play rec soxi
	$(RM) sox_sample_test$(EXEEXT)
	$(RM) example0$(EXEEXT) example1$(EXEEXT) example2$(EXEEXT) example3$(EXEEXT) example4$(EXEEXT) example5$(EXEEXT)

distclean-local:

loc:
	sloccount \
	$(include_HEADERS) \
	$(sox_SOURCES) \
	$(example0_SOURCES) \
	$(example1_SOURCES) \
	$(example2_SOURCES) \
	$(example3_SOURCES) \
	$(example4_SOURCES) \
	$(example5_SOURCES) \
	$(sox_sample_test_SOURCES) \
	$(libsox_la_SOURCES)


# Ideally we would use the "check" target so that "make distcheck"
# would run the test suite, but an uninstalled libltdl build cannot
# currently load its formats and effects, so the checks would fail.
installcheck:
	$(srcdir)/tests.sh --bindir=${bindir} --builddir=${builddir} --srcdir=${srcdir}
	$(srcdir)/testall.sh --bindir=${bindir} --srcdir=${srcdir}

