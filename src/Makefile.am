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

# Pass flags from --enable-silent-libtool
LIBTOOL = @LIBTOOL@ @LIBTOOLFLAGS@

if HAVE_DISTRO
AM_CPPFLAGS += -DDISTRO="\"@DISTRO@\""
endif

#########################
# SoX - the application #
#########################

bin_PROGRAMS = sox
EXTRA_PROGRAMS = example0 example1 example2 sox_sample_test
lib_LTLIBRARIES = libsox.la
include_HEADERS = sox.h
nodist_include_HEADERS = soxstdint.h
sox_SOURCES = sox.c
sox_LDADD = libsox.la
example0_SOURCES = example0.c
example1_SOURCES = example1.c
example2_SOURCES = example2.c
sox_sample_test_SOURCES = sox_sample_test.c sox_sample_test.h

###############################################
# libsox - Dynamic Libraries for File Formats #
###############################################

if HAVE_LIBLTDL

# Format modules; listed first so optional ones can be added later
pkglib_LTLIBRARIES = libsox_fmt_raw.la libsox_fmt_s1.la \
	  libsox_fmt_s2.la libsox_fmt_s3.la libsox_fmt_s4.la libsox_fmt_u1.la \
	  libsox_fmt_u2.la libsox_fmt_u3.la libsox_fmt_u4.la		\
	  libsox_fmt_al.la libsox_fmt_la.la libsox_fmt_ul.la		\
	  libsox_fmt_lu.la libsox_fmt_svx.la libsox_fmt_aiff.la		\
	  libsox_fmt_aifc.la libsox_fmt_au.la libsox_fmt_avr.la		\
	  libsox_fmt_cdr.la libsox_fmt_cvsd.la libsox_fmt_dvms.la	\
	  libsox_fmt_dat.la libsox_fmt_gsm.la libsox_fmt_hcom.la	\
	  libsox_fmt_lpc10.la libsox_fmt_maud.la libsox_fmt_prc.la	\
	  libsox_fmt_sf.la libsox_fmt_smp.la	\
	  libsox_fmt_sphere.la libsox_fmt_txw.la libsox_fmt_voc.la	\
	  libsox_fmt_vox.la libsox_fmt_ima.la libsox_fmt_wav.la		\
	  libsox_fmt_wve.la libsox_fmt_xa.la libsox_fmt_nul.la \
	  libsox_fmt_htk.la libsox_fmt_sounder.la libsox_fmt_soundtool.la \
	  libsox_fmt_f4.la libsox_fmt_f8.la

# File formats
libsox_fmt_raw_la_SOURCES = raw-fmt.c
libsox_fmt_raw_la_LIBADD = libsox.la
libsox_fmt_f4_la_SOURCES = f4-fmt.c
libsox_fmt_f4_la_LIBADD = libsox.la
libsox_fmt_f8_la_SOURCES = f8-fmt.c
libsox_fmt_f8_la_LIBADD = libsox.la
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
if HAVE_COREAUDIO
libsox_fmt_coreaudio_la_SOURCES = coreaudio.c
libsox_fmt_coreaudio_la_LIBADD = libsox.la @COREAUDIO_LIBS@
pkglib_LTLIBRARIES += libsox_fmt_coreaudio.la
endif
libsox_fmt_cvsd_la_SOURCES = cvsd-fmt.c
libsox_fmt_cvsd_la_LIBADD = libsox.la
libsox_fmt_dvms_la_SOURCES = dvms-fmt.c
libsox_fmt_dvms_la_LIBADD = libsox.la
libsox_fmt_dat_la_SOURCES = dat.c
libsox_fmt_dat_la_LIBADD = libsox.la
libsox_fmt_gsm_la_SOURCES = gsm.c
libsox_fmt_gsm_la_LIBADD = libsox.la @GSM_LIBS@ @LIBGSM_LIBADD@
libsox_fmt_hcom_la_SOURCES = hcom.c
libsox_fmt_hcom_la_LIBADD = libsox.la
libsox_fmt_htk_la_SOURCES = htk.c
libsox_fmt_htk_la_LIBADD = libsox.la
libsox_fmt_lpc10_la_SOURCES = lpc10.c
libsox_fmt_lpc10_la_LIBADD = libsox.la @LPC10_LIBS@ @LIBLPC10_LIBADD@
libsox_fmt_maud_la_SOURCES = maud.c
libsox_fmt_maud_la_LIBADD = libsox.la
libsox_fmt_prc_la_SOURCES = prc.c
libsox_fmt_prc_la_LIBADD = libsox.la
libsox_fmt_sf_la_SOURCES = sf.c
libsox_fmt_sf_la_LIBADD = libsox.la
libsox_fmt_smp_la_SOURCES = smp.c
libsox_fmt_smp_la_LIBADD = libsox.la
libsox_fmt_sounder_la_SOURCES = sounder.c
libsox_fmt_sounder_la_LIBADD = libsox.la
libsox_fmt_soundtool_la_SOURCES = soundtool.c
libsox_fmt_soundtool_la_LIBADD = libsox.la
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
libsox_fmt_wav_la_SOURCES = adpcm.c adpcm.h ima_rw.c ima_rw.h wav.c
libsox_fmt_wav_la_LIBADD = libsox.la @GSM_LIBS@ @LIBGSM_LIBADD@
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
libsox_fmt_sndfile_la_CFLAGS = @WARN_CFLAGS@ @SNDFILE_CFLAGS@
libsox_fmt_sndfile_la_LIBADD = libsox.la @SNDFILE_LIBS@
libsox_fmt_caf_la_SOURCES = caf.c sndfile.c
libsox_fmt_caf_la_CFLAGS = @WARN_CFLAGS@ @SNDFILE_CFLAGS@
libsox_fmt_caf_la_LIBADD = libsox.la @SNDFILE_LIBS@
libsox_fmt_mat4_la_SOURCES = mat4.c sndfile.c
libsox_fmt_mat4_la_CFLAGS = @WARN_CFLAGS@ @SNDFILE_CFLAGS@
libsox_fmt_mat4_la_LIBADD = libsox.la @SNDFILE_LIBS@
libsox_fmt_mat5_la_SOURCES = mat5.c sndfile.c
libsox_fmt_mat5_la_CFLAGS = @WARN_CFLAGS@ @SNDFILE_CFLAGS@
libsox_fmt_mat5_la_LIBADD = libsox.la @SNDFILE_LIBS@
libsox_fmt_paf_la_SOURCES = paf.c sndfile.c
libsox_fmt_paf_la_CFLAGS = @WARN_CFLAGS@ @SNDFILE_CFLAGS@
libsox_fmt_paf_la_LIBADD = libsox.la  @SNDFILE_LIBS@
libsox_fmt_fap_la_SOURCES = fap.c sndfile.c
libsox_fmt_fap_la_CFLAGS = @WARN_CFLAGS@ @SNDFILE_CFLAGS@
libsox_fmt_fap_la_LIBADD = libsox.la @SNDFILE_LIBS@
libsox_fmt_w64_la_SOURCES = w64.c sndfile.c
libsox_fmt_w64_la_CFLAGS = @WARN_CFLAGS@ @SNDFILE_CFLAGS@
libsox_fmt_w64_la_LIBADD = libsox.la @SNDFILE_LIBS@
libsox_fmt_xi_la_SOURCES = xi.c sndfile.c
libsox_fmt_wi_la_CFLAGS = @WARN_CFLAGS@ @SNDFILE_CFLAGS@
libsox_fmt_xi_la_LIBADD = libsox.la @SNDFILE_LIBS@
libsox_fmt_pvf_la_SOURCES = pvf.c sndfile.c
libsox_fmt_pvf_CFLAGS = @WARN_CFLAGS@ @SNDFILE_CFLAGS@
libsox_fmt_pvf_la_LIBADD = libsox.la @SNDFILE_LIBS@
libsox_fmt_sd2_la_SOURCES = sd2.c sndfile.c
libsox_fmt_sd2_CFLAGS = @WARN_CFLAGS@ @SNDFILE_CFLAGS@
libsox_fmt_sd2_la_LIBADD = libsox.la @SNDFILE_LIBS@
pkglib_LTLIBRARIES += libsox_fmt_sndfile.la libsox_fmt_caf.la libsox_fmt_mat4.la libsox_fmt_mat5.la libsox_fmt_paf.la libsox_fmt_fap.la libsox_fmt_w64.la libsox_fmt_xi.la libsox_fmt_pvf.la libsox_fmt_sd2.la
endif
if HAVE_AMRWB
libsox_fmt_amr_wb_la_SOURCES = amr-wb.c amr.h amr1.h amr2.h
libsox_fmt_amr_wb_la_LIBADD = libsox.la @AMR_WB_LIBS@
pkglib_LTLIBRARIES += libsox_fmt_amr_wb.la
endif
if HAVE_AMRNB
libsox_fmt_amr_nb_la_SOURCES = amr-nb.c amr.h amr1.h amr2.h
libsox_fmt_amr_nb_la_LIBADD = libsox.la @AMR_NB_LIBS@
pkglib_LTLIBRARIES += libsox_fmt_amr_nb.la
endif
if HAVE_WAVPACK
libsox_fmt_wavpack_la_SOURCES = wavpack.c
libsox_fmt_wavpack_la_LIBADD = libsox.la @WAVPACK_LIBS@
pkglib_LTLIBRARIES += libsox_fmt_wavpack.la
endif
if HAVE_MP3
libsox_fmt_mp3_la_SOURCES = mp3.c mp3-duration.h
libsox_fmt_mp3_la_LIBADD = libsox.la @MP3_LIBS@
pkglib_LTLIBRARIES += libsox_fmt_mp3.la
endif
if HAVE_OGG_VORBIS
libsox_fmt_vorbis_la_SOURCES = vorbis.c vorbis1.h
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
endif

######################################################
# libsox - file format, effects, and utility library #
######################################################

# Format handlers and utils source
libsox_la_SOURCES = adpcms.c adpcms.h aiff.c aiff.h cvsd.c cvsd.h cvsdfilt.h \
	  g711.c g711.h g721.c g723_24.c g723_40.c g72x.c g72x.h vox.c vox.h \
	  raw.c raw.h formats.c formats.h formats_i.c sox_i.h skelform.c \
	  xmalloc.c xmalloc.h getopt.c getopt1.c getopt.h \
	  util.c util.h libsox.c libsox_i.c sox-fmt.c

# Effects source
libsox_la_SOURCES += \
	band.h bend.c biquad.c biquad.h biquads.c chorus.c compand.c \
	compandt.c compandt.h contrast.c dcshift.c delay.c dither.c \
	dither_fir.h dither_iir.h earwax.c echo.c echos.c effects.c effects.h \
	effects_i.c fade.c fft4g.c fft4g.h fifo.h filter.c flanger.c input.c \
	ladspa.c loudness.c mcompand.c mcompand_xover.h mixer.c noiseprof.c \
	noisered.c noisered.h normalise.c output.c pad.c pan.c phaser.c \
	polyphas.c rabbit.c rate.c rate_filters.h rate_half_fir.h \
	rate_poly_fir0.h rate_poly_fir.h remix.c repeat.c resample.c reverb.c \
	reverse.c silence.c skeleff.c speed.c splice.c stat.c swap.c stretch.c \
	synth.c tempo.c tremolo.c trim.c vol.c
if HAVE_PNG
    libsox_la_SOURCES += spectrogram.c
endif

# Libraries required by libsox for file handlers, effects, or utils;
# regardless if libltdl is used or not.
libsox_la_LIBADD = @PNG_LIBS@ @SAMPLERATE_LIBS@
if HAVE_MAGIC
libsox_la_LIBADD += @MAGIC_LIBS@
endif

libsox_la_CFLAGS = @WARN_CFLAGS@  @SAMPLERATE_CFLAGS@
libsox_la_LDFLAGS = @APP_LDFLAGS@ -version-info @SHLIB_VERSION@

if HAVE_LIBLTDL
  libsox_la_CFLAGS += $(LTDLINCL) 
  libsox_la_LDFLAGS += $(LIBLTDL) 
else
  libsox_la_SOURCES += raw-fmt.c s1-fmt.c s2-fmt.c s3-fmt.c \
    s4-fmt.c u1-fmt.c u2-fmt.c u3-fmt.c u4-fmt.c al-fmt.c la-fmt.c ul-fmt.c \
    lu-fmt.c 8svx.c aiff-fmt.c aifc-fmt.c au.c avr.c cdr.c cvsd-fmt.c \
    dvms-fmt.c dat.c gsm.c hcom.c htk.c lpc10.c maud.c prc.c sf.c smp.c \
    sounder.c soundtool.c sphere.c tx16w.c voc.c vox-fmt.c ima-fmt.c adpcm.c adpcm.h \
    ima_rw.c ima_rw.h wav.c wve.c xa.c nulfile.c f4-fmt.c f8-fmt.c
libsox_la_LIBADD += @GSM_LIBS@ @LIBGSM_LIBADD@
libsox_la_LIBADD += @LPC10_LIBS@ @LIBLPC10_LIBADD@


sox_LDADD += @GSM_LIBS@ @LIBGSM_LIBADD@
sox_LDADD += @LPC10_LIBS@ @LIBLPC10_LIBADD@

if HAVE_FLAC
    libsox_la_SOURCES += flac.c
    libsox_la_LIBADD += @FLAC_LIBS@
    sox_LDADD += @FLAC_LIBS@
endif
if HAVE_FFMPEG
    libsox_la_SOURCES += ffmpeg.h ffmpeg.c
    libsox_la_LIBADD += @FFMPEG_LIBS@
    sox_LDADD += @FFMPEG_LIBS@
endif
if HAVE_SNDFILE
    libsox_la_SOURCES += sndfile.c caf.c mat4.c mat5.c paf.c fap.c w64.c xi.c pvf.c sd2.c
    libsox_la_CFLAGS += @SNDFILE_CFLAGS@
    libsox_la_LIBADD += @SNDFILE_LIBS@
    sox_LDADD += @SNDFILE_LIBS@
endif
if HAVE_AMRWB
    libsox_la_SOURCES += amr-wb.c amr.h amr1.h amr2.h
    libsox_la_LIBADD += @AMR_WB_LIBS@
    sox_LDADD += @AMR_WB_LIBS@
endif
if HAVE_AMRNB
    libsox_la_SOURCES += amr-nb.c amr.h amr1.h amr2.h
    libsox_la_LIBADD += @AMR_NB_LIBS@
    sox_LDADD += @AMR_NB_LIBS@
endif
if HAVE_COREAUDIO
    libsox_la_SOURCES += coreaudio.c
    libsox_la_LIBADD += @COREAUDIO_LIBS@
    sox_LDADD += @COREAUDIO_LIBS@
endif
if HAVE_WAVPACK
    libsox_la_SOURCES += wavpack.c
    libsox_la_LIBADD += @WAVPACK_LIBS@
    sox_LDADD += @WAVPACK_LIBS@
endif
if HAVE_MP3
    libsox_la_SOURCES += mp3.c mp3-duration.h
    libsox_la_LIBADD += @MP3_LIBS@
    sox_LDADD += @MP3_LIBS@
endif
if HAVE_OGG_VORBIS
    libsox_la_SOURCES += vorbis.c vorbis1.h
    libsox_la_LIBADD += @OGG_LIBS@
    sox_LDADD += @OGG_LIBS@
endif
if HAVE_ALSA
    libsox_la_SOURCES += alsa.c
    libsox_la_LIBADD += @ALSA_LIBS@
    sox_LDADD += @ALSA_LIBS@
endif
if HAVE_LIBAO
    libsox_la_SOURCES += ao.c
    libsox_la_LIBADD += @AO_LIBS@
    sox_LDADD += @AO_LIBS@
endif
if HAVE_OSS
    libsox_la_SOURCES += oss.c
    libsox_la_LIBADD += @OSS_LIBS@
    sox_LDADD += @OSS_LIBS@
endif
if HAVE_SUN_AUDIO
    libsox_la_SOURCES += sunaudio.c
endif

#end !HAVE_LIBLTDL
endif

# example programs will need same link options as sox does.
example0_LDADD = ${sox_LDADD}
example1_LDADD = ${sox_LDADD}
example2_LDADD = ${sox_LDADD}

EXTRA_DIST = monkey.au monkey.wav \
	     CMakeLists.txt soxstdint.h.cmake soxconfig.h.cmake \
	     tests.sh testall.sh tests.bat testall.bat test-comments

all: sox$(EXEEXT) play rec soxi sox_sample_test$(EXEEXT) example0$(EXEEXT) example1$(EXEEXT) example2$(EXEEXT)

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
	$(RM) example0$(EXEEXT) example1$(EXEEXT) example2$(EXEEXT)

distclean-local:
	$(RM) soxstdint.h

# Ideally we would use the "check" target so that "make distcheck"
# would run the test suite, but an uninstalled libltdl build cannot
# currently load its formats and effects, so the checks would fail.
installcheck:
	$(srcdir)/tests.sh --bindir=${bindir} --builddir=${builddir} --srcdir=${srcdir}
	$(srcdir)/testall.sh --bindir=${bindir} --srcdir=${srcdir}

