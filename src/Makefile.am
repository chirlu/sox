## Process this file with automake to produce Makefile.in

RM = rm -f 

AM_CPPFLAGS = -DLADSPA_PATH="\"@LADSPA_PATH@\"" -DPKGLIBDIR="\"$(pkglibdir)\""
AM_CFLAGS = $(LTDLINCL) -Wconversion
AM_LDFLAGS = $(LIBLTDL)

# Pass flags from --enable-silent-libtool
LIBTOOL = @LIBTOOL@ @LIBTOOLFLAGS@

bin_PROGRAMS = sox
lib_LTLIBRARIES = libsox.la
include_HEADERS = sox.h soxstdint.h

# Format modules; listed first so optional ones can be added later
pkglib_LTLIBRARIES = libsox_fmt_auto.la libsox_fmt_raw.la libsox_fmt_s1.la \
	  libsox_fmt_s2.la libsox_fmt_s3.la libsox_fmt_s4.la libsox_fmt_u1.la	\
	  libsox_fmt_u2.la libsox_fmt_u3.la libsox_fmt_u4.la		\
	  libsox_fmt_al.la libsox_fmt_la.la libsox_fmt_ul.la		\
	  libsox_fmt_lu.la libsox_fmt_svx.la libsox_fmt_aiff.la		\
	  libsox_fmt_aifc.la libsox_fmt_au.la libsox_fmt_avr.la		\
	  libsox_fmt_cdr.la libsox_fmt_cvsd.la libsox_fmt_dvms.la	\
	  libsox_fmt_dat.la libsox_fmt_gsm.la libsox_fmt_hcom.la	\
	  libsox_fmt_lpc10.la libsox_fmt_maud.la libsox_fmt_prc.la	\
	  libsox_fmt_sf.la libsox_fmt_smp.la libsox_fmt_sndrtool.la	\
	  libsox_fmt_sphere.la libsox_fmt_txw.la libsox_fmt_voc.la	\
	  libsox_fmt_vox.la libsox_fmt_ima.la libsox_fmt_wav.la		\
	  libsox_fmt_wve.la libsox_fmt_xa.la libsox_fmt_nul.la

# File format detection
libsox_fmt_auto_la_SOURCES = auto.c
libsox_fmt_auto_la_LIBADD = libsox.la

# File formats
libsox_fmt_raw_la_SOURCES = raw-fmt.c
libsox_fmt_raw_la_LIBADD = libsox.la
libsox_fmt_s1_la_SOURCES = s1-fmt.c
libsox_fmt_s1_la_LIBADD = libsox.la
libsox_fmt_s2_la_SOURCES = s2-fmt.c
libsox_fmt_s2_la_LIBADD = libsox.la
libsox_fmt_s3_la_SOURCES = s3-fmt.c
libsox_fmt_s3_la_LIBADD = libsox.la
libsox_fmt_s4_la_SOURCES = s4-fmt.c
libsox_fmt_s4_la_LIBADD = libsox.la
libsox_fmt_u1_la_SOURCES = u1-fmt.c
libsox_fmt_u1_la_LIBADD = libsox.la
libsox_fmt_u2_la_SOURCES = u2-fmt.c
libsox_fmt_u2_la_LIBADD = libsox.la
libsox_fmt_u3_la_SOURCES = u3-fmt.c
libsox_fmt_u3_la_LIBADD = libsox.la
libsox_fmt_u4_la_SOURCES = u4-fmt.c
libsox_fmt_u4_la_LIBADD = libsox.la
libsox_fmt_al_la_SOURCES = al-fmt.c
libsox_fmt_al_la_LIBADD = libsox.la
libsox_fmt_la_la_SOURCES = la-fmt.c
libsox_fmt_la_la_LIBADD = libsox.la
libsox_fmt_ul_la_SOURCES = ul-fmt.c
libsox_fmt_ul_la_LIBADD = libsox.la
libsox_fmt_lu_la_SOURCES = lu-fmt.c
libsox_fmt_lu_la_LIBADD = libsox.la
libsox_fmt_svx_la_SOURCES = 8svx.c
libsox_fmt_svx_la_LIBADD = libsox.la
libsox_fmt_aiff_la_SOURCES = aiff-fmt.c
libsox_fmt_aiff_la_LIBADD = libsox.la
libsox_fmt_aifc_la_SOURCES = aifc-fmt.c
libsox_fmt_aifc_la_LIBADD = libsox.la
libsox_fmt_au_la_SOURCES = au.c
libsox_fmt_au_la_LIBADD = libsox.la
libsox_fmt_avr_la_SOURCES = avr.c
libsox_fmt_avr_la_LIBADD = libsox.la
libsox_fmt_cdr_la_SOURCES = cdr.c
libsox_fmt_cdr_la_LIBADD = libsox.la
libsox_fmt_cvsd_la_SOURCES = cvsd-fmt.c
libsox_fmt_cvsd_la_LIBADD = libsox.la
libsox_fmt_dvms_la_SOURCES = dvms-fmt.c
libsox_fmt_dvms_la_LIBADD = libsox.la
libsox_fmt_dat_la_SOURCES = dat.c
libsox_fmt_dat_la_LIBADD = libsox.la
libsox_fmt_gsm_la_SOURCES = gsm.c
libsox_fmt_gsm_la_LIBADD = libsox.la @GSM_LIBS@
libsox_fmt_hcom_la_SOURCES = hcom.c
libsox_fmt_hcom_la_LIBADD = libsox.la
libsox_fmt_lpc10_la_SOURCES = lpc10.c
libsox_fmt_lpc10_la_LIBADD = ../lpc10/liblpc10.la libsox.la
libsox_fmt_maud_la_SOURCES = maud.c
libsox_fmt_maud_la_LIBADD = libsox.la
libsox_fmt_prc_la_SOURCES = prc.c
libsox_fmt_prc_la_LIBADD = libsox.la
libsox_fmt_sf_la_SOURCES = sf.c sfircam.h
libsox_fmt_sf_la_LIBADD = libsox.la
libsox_fmt_smp_la_SOURCES = smp.c
libsox_fmt_smp_la_LIBADD = libsox.la
libsox_fmt_sndrtool_la_SOURCES = sndrtool.c
libsox_fmt_sndrtool_la_LIBADD = libsox.la
libsox_fmt_sphere_la_SOURCES = sphere.c
libsox_fmt_sphere_la_LIBADD = libsox.la
libsox_fmt_txw_la_SOURCES = tx16w.c
libsox_fmt_txw_la_LIBADD = libsox.la
libsox_fmt_voc_la_SOURCES = voc.c
libsox_fmt_voc_la_LIBADD = libsox.la
libsox_fmt_vox_la_SOURCES = vox-fmt.c
libsox_fmt_vox_la_LIBADD = libsox.la
libsox_fmt_ima_la_SOURCES = ima-fmt.c
libsox_fmt_ima_la_LIBADD = libsox.la
libsox_fmt_wav_la_SOURCES = adpcm.c adpcm.h ima_rw.c ima_rw.h wav.c wav.h
libsox_fmt_wav_la_LIBADD = libsox.la @GSM_LIBS@
libsox_fmt_wve_la_SOURCES = wve.c
libsox_fmt_wve_la_LIBADD = libsox.la
libsox_fmt_xa_la_SOURCES = xa.c
libsox_fmt_xa_la_LIBADD = libsox.la
if HAVE_FLAC
libsox_fmt_flac_la_SOURCES = flac.c
libsox_fmt_flac_la_LIBADD = libsox.la @FLAC_LIBS@
pkglib_LTLIBRARIES += libsox_fmt_flac.la
endif
if HAVE_FFMPEG
libsox_fmt_ffmpeg_la_SOURCES = ffmpeg.c
libsox_fmt_ffmpeg_la_LIBADD = libsox.la @FFMPEG_LIBS@
pkglib_LTLIBRARIES += libsox_fmt_ffmpeg.la
endif
if HAVE_SNDFILE
libsox_fmt_sndfile_la_SOURCES = sndfile.c
libsox_fmt_sndfile_la_CFLAGS = @SNDFILE_CFLAGS@
libsox_fmt_sndfile_la_LIBADD = libsox.la @SNDFILE_LIBS@
pkglib_LTLIBRARIES += libsox_fmt_sndfile.la
endif
if HAVE_AMR_WB
libsox_fmt_amr_wb_la_SOURCES = amr-wb.c
libsox_fmt_amr_wb_la_LIBADD = libsox.la @AMR_WB_LIBS@
pkglib_LTLIBRARIES += libsox_fmt_amr_wb.la
endif
if HAVE_AMR_NB
libsox_fmt_amr_nb_la_SOURCES = amr-nb.c
libsox_fmt_amr_nb_la_LIBADD = libsox.la @AMR_NB_LIBS@
pkglib_LTLIBRARIES += libsox_fmt_amr_nb.la
endif
if HAVE_MP3
libsox_fmt_mp3_la_SOURCES = mp3.c
libsox_fmt_mp3_la_LIBADD = libsox.la @MP3_LIBS@
pkglib_LTLIBRARIES += libsox_fmt_mp3.la
endif
if HAVE_OGG_VORBIS
libsox_fmt_vorbis_la_SOURCES = vorbis.c
libsox_fmt_vorbis_la_LIBADD = libsox.la @OGG_LIBS@
pkglib_LTLIBRARIES += libsox_fmt_vorbis.la
endif

# I/O devices
libsox_fmt_nul_la_SOURCES = nulfile.c
libsox_fmt_nul_la_LIBADD = libsox.la
if HAVE_ALSA
libsox_fmt_alsa_la_SOURCES = alsa.c
libsox_fmt_alsa_la_LIBADD = libsox.la @ALSA_LIBS@
pkglib_LTLIBRARIES += libsox_fmt_alsa.la
endif
if HAVE_LIBAO
libsox_fmt_ao_la_SOURCES = ao.c
libsox_fmt_ao_la_LIBADD = libsox.la @AO_LIBS@
pkglib_LTLIBRARIES += libsox_fmt_ao.la
endif
if HAVE_OSS
libsox_fmt_oss_la_SOURCES = oss.c
libsox_fmt_oss_la_LIBADD = libsox.la @OSS_LIBS@
pkglib_LTLIBRARIES += libsox_fmt_oss.la
endif
if HAVE_SUN_AUDIO
libsox_fmt_sunau_la_SOURCES = sunaudio.c
libsox_fmt_sunau_la_LIBADD = libsox.la
pkglib_LTLIBRARIES += libsox_fmt_sunau.la
endif

effects = band.h biquad.c biquad.h biquads.c chorus.c compand.c	\
	  compandt.c compandt.h dcshift.c dither.c earwax.c echo.c echos.c	\
	  fade.c FFT.c FFT.h filter.c flanger.c ladspa.c mcompand.c	\
	  mixer.c noiseprof.c noisered.c noisered.h pad.c pan.c		\
	  phaser.c pitch.c polyphas.c rabbit.c rate.c repeat.c		\
	  resample.c reverb.c reverse.c silence.c skeleff.c speed.c	\
	  stat.c stretch.c swap.c synth.c synth.h tremolo.c trim.c	\
	  vibro.c vol.c

libsox_la_SOURCES = $(effects) adpcms.c adpcms.h aiff.c aiff.h cvsd.c cvsd.h cvsdfilt.h \
	  g711.c g711.h g721.c g723_24.c g723_40.c g72x.c g72x.h vox.c vox.h	\
	  raw.c raw.h handlers.c misc.c sox_i.h skelform.c soxio.c	\
	  util.c xmalloc.c xmalloc.h getopt.c getopt1.c getopt.h	\
	  soxconfig.h
libsox_la_CFLAGS = @SAMPLERATE_CFLAGS@
libsox_la_LIBADD = @LIBGSM_LIBADD@ @SAMPLERATE_LIBS@

sox_SOURCES = sox.c
sox_LDADD = libsox.la

EXTRA_DIST = tests.sh testall.sh tests.bat testall.bat monkey.au monkey.wav

all: sox$(EXEEXT) play rec

libgsm/libgsm.la:
	$(MAKE) -C libgsm

play rec: sox$(EXEEXT)
	./sox --help > /dev/null
	test -f .libs/lt-sox && ( $(RM) $@ && $(LN_S) .libs/lt-sox $@ ) || $(LN_S) sox$(EXEEXT) $@

install-exec-hook:
	if test "$(PLAYRECLINKS)" = "yes"; then	\
		cd $(DESTDIR)$(bindir); $(RM) play rec; $(LN_S) sox play; $(LN_S) sox rec; \
	fi

clean-local:
	$(RM) play rec

distclean-local:
	$(RM) soxstdint.h
