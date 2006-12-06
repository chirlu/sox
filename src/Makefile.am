SUBDIRS = libgsm

# Objects.

bin_PROGRAMS = sox
lib_LIBRARIES = libst.a

formats = 8svx.c adpcm.c aiff.c au.c auto.c avr.c cdr.c \
	  cvsd.c dat.c flac.c g711.c g721.c g723_16.c g723_24.c g723_40.c \
	  g72x.c gsm.c hcom.c ima_rw.c maud.c mp3.c nulfile.c prc.c \
	  raw.c sf.c smp.c sndrtool.c sphere.c tx16w.c voc.c vorbis.c \
	  vox.c wav.c wve.c xa.c

effects = avg.c band.c bandpass.c biquad.c breject.c btrworth.c chorus.c \
	  compand.c copy.c dcshift.c deemphas.c earwax.c echo.c echos.c \
	  equalizer.c fade.c FFT.c filter.c flanger.c highp.c highpass.c lowp.c \
	  lowpass.c mask.c mcompand.c noiseprof.c noisered.c pan.c \
	  phaser.c pitch.c polyphas.c rabbit.c rate.c repeat.c resample.c \
	  reverb.c reverse.c silence.c speed.c stat.c \
	  stretch.c swap.c synth.c tone.c trim.c vibro.c vol.c

PLAY_0    =
PLAY_1    = play

libst_a_SOURCES = $(formats) $(effects) handlers.c misc.c stio.c util.c
if OSS
libst_a_SOURCES += oss.c
endif
if ALSA
libst_a_SOURCES += alsa.c
endif
if SUNAU
libst_a_SOURCES += sunaudio.c
endif

sox_SOURCES = sox.c
sox_LDADD = libst.a libgsm/libgsm.a
sox_LDFLAGS = @SAMPLERATE_LIBS@

all: sox soxmix $(PLAY_$(PLAY_SUPPORT))

soxmix: sox
	$(RM) soxmix
	$(LN_S) sox soxmix

libgsm/libgsm.a:
	cd libgsm && $(MAKE)

PLAY_INSTALL_0    =
PLAY_INSTALL_1    = install-play

install: sox $(PLAY_INSTALL_$(PLAY_SUPPORT))
	$(top_srcdir)/mkinstalldirs $(bindir)
	$(INSTALL) -c -m 755 sox $(bindir)
	$(RM) $(bindir)/soxmix
	cd $(bindir) && $(LN_S) sox soxmix

install-play:
	if [ -f $(bindir)/rec ] ; then $(RM) $(bindir)/rec; fi
	$(top_srcdir)/mkinstalldirs $(bindir)
	$(INSTALL) -c -m 755 play $(bindir)
	cd $(bindir) && $(LN_S) play rec

install-lib: libst.a
	$(top_srcdir)/mkinstalldirs $(libdir)
	$(top_srcdir)/mkinstalldirs $(includedir)
	$(INSTALL) -c -m 755 libst-config $(bindir)
	$(INSTALL) -c -m 644 libst.a $(libdir)
	$(RANLIB) $(libdir)/libst.a
	$(INSTALL) -c -m 644 $(srcdir)/st.h $(includedir)
	$(INSTALL) -c -m 644 ststdint.h $(includedir)

PLAY_UNINSTALL_0    =
PLAY_UNINSTALL_1    = uninstall-play

uninstall: $(PLAY_UNINSTALL_$(PLAY_SUPPORT))
	$(RM) $(bindir)/sox
	$(RM) $(bindir)/soxmix

uninstall-play:
	$(RM) $(bindir)/play
	$(RM) $(bindir)/rec

uninstall-lib:
	$(RM) $(libdir)/libst.a
	$(RM) $(includedir)/st.h
	$(RM) $(includedir)/ststdint.h

clean:
	$(RM) *.o libst.a

distclean-local:
	$(RM) *.raw *.sf libst-config stconfig.h ststdint.h
