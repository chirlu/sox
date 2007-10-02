dnl Process this file with autoconf to produce a configure script.

AC_INIT(SoX, 14.0.1, sox-devel@lists.sourceforge.net)

dnl Find target architecture
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE

dnl create a config.h file (Automake will add -DHAVE_CONFIG_H)
AM_CONFIG_HEADER(src/soxconfig.h)

dnl Check we have the right srcdir
AC_CONFIG_SRCDIR(sox.1)

dnl Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LN_S

AC_ARG_WITH(libltdl,
    AC_HELP_STRING([--without-libltdl],
        [Don't try to use libltdl for external dynamic library support]),
        [with_libltdl=$withval])

using_libltdl=no
if test "$with_libltdl" != "no"; then
    dnl Disable libltdl support when building only static libraries
    if test "$enable_shared" != "no"; then
        using_libltdl=yes
    fi
    dnl Force off using libltdl on targets that are know to have
    dnl problems.
    case $target in
      *cygwin*)
        using_libltdl=no
        ;;
    esac
fi

if test "$using_libltdl" != "no"; then
  AC_LIBLTDL_INSTALLABLE
  AC_LIBTOOL_DLOPEN
fi
AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)
AC_CONFIG_SUBDIRS(libltdl)
if test "$using_libltdl" != "no"; then
  AC_SUBST(LIBLTDL)
  AC_SUBST(LTDLINCL)
  AC_CHECK_HEADERS(ltdl.h, AC_DEFINE([HAVE_LIBLTDL], 1, [Define to 1 if you have libltdl]), [using_libltdl=no])
fi
AM_CONDITIONAL(HAVE_LIBLTDL, test x$using_libltdl = xyes)

m4_ifdef([PKG_PROG_PKG_CONFIG], [PKG_PROG_PKG_CONFIG])

dnl Debugging
AC_MSG_CHECKING([whether to make a debug build])
AC_ARG_ENABLE(debug, AC_HELP_STRING([--enable-debug], [make a debug build]))
AC_MSG_RESULT($enable_debug)
if test "$enable_debug" = "yes"; then
    CFLAGS="-g"
    if test "$GCC" = "yes"; then
        CFLAGS="$CFLAGS -ggdb"
    fi
else
    enable_debug=no
fi

dnl Extra CFLAGS if we have gcc
if test "$GCC" = yes; then
    CFLAGS="$CFLAGS -Wall -W -Wmissing-prototypes -Wstrict-prototypes -pedantic"
    dnl Define stricter policy on GNU/Linux, all symbols must be resolved
    case $target in
      *linux*)
        LDFLAGS="$LDFLAGS -Wl,-z,defs"
        ;;
    esac
fi

dnl Check for system dependent features.
AC_C_BIGENDIAN
AC_C_INLINE

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h unistd.h byteswap.h sys/time.h sys/timeb.h)

dnl Checks for library functions.
AC_CHECK_FUNCS(strcasecmp strdup popen vsnprintf gettimeofday)
AC_REPLACE_FUNCS(getopt_long)
AM_CONDITIONAL(GETOPT_LONG, test x$getopt_long = xtrue)

dnl Check if math library is needed.
AC_CHECK_FUNC(pow)
if test "$ac_cv_func_pow" = no; then
  AC_CHECK_LIB(m, pow)
fi

dnl Large File Support
AC_SYS_LARGEFILE
AC_FUNC_FSEEKO

dnl Allow libtool to be silenced
AC_MSG_CHECKING([whether libtool should be silenced])
AC_ARG_ENABLE(silent-libtool,
    AC_HELP_STRING([--enable-silent-libtool], [Pass --silent to libtool.]),
    [if test "$enable_silent_libtool" = "yes"; then
	LIBTOOLFLAGS=--silent
    fi])
AC_MSG_RESULT($enable_silent_libtool)
AC_SUBST(LIBTOOLFLAGS)

dnl Check if we want play/rec links
AC_MSG_CHECKING([whether to make play and rec symlinks])
AC_ARG_ENABLE(playrec-symlinks,
    AC_HELP_STRING([--disable-playrec-symlinks], [Don't make play and rec symlinks to sox.]),,enable_playrec_symlinks=yes)
AC_MSG_RESULT($enable_playrec_symlinks)
if test "$enable_playrec_symlinks" = "yes"; then
    PLAYRECLINKS=yes
fi
AC_SUBST(PLAYRECLINKS)
     
dnl Check for ALSA
AC_MSG_CHECKING([whether to try building ALSA sound driver])
AC_ARG_ENABLE(alsa,
    AC_HELP_STRING([--disable-alsa], [Don't build ALSA sound driver.]),,enable_alsa=yes)
AC_MSG_RESULT($enable_alsa)
if test "$enable_alsa" = "yes"; then
    AC_CHECK_HEADERS(alsa/asoundlib.h,
        AC_CHECK_LIB(asound, snd_pcm_open, ALSA_LIBS="$ALSA_LIBS -lasound", enable_alsa=no),
        enable_alsa=no)
fi
if test "$enable_alsa" = yes; then
   AC_DEFINE(HAVE_ALSA, 1, [Define to 1 if you have ALSA.])
fi
AM_CONDITIONAL(HAVE_ALSA, test x$enable_alsa = xyes)
AC_SUBST(ALSA_LIBS)

dnl Check for libao
AC_MSG_CHECKING([whether to try building libao sound driver])
AC_ARG_ENABLE(libao,
    AC_HELP_STRING([--disable-libao], [Don't build libao sound driver.]),,enable_libao=yes)
AC_MSG_RESULT($enable_libao)
if test "$enable_libao" = "yes"; then
    AC_CHECK_HEADERS(ao/ao.h,
        AC_CHECK_LIB(ao, ao_play, AO_LIBS="$AO_LIBS -lao", enable_libao=no),
        enable_libao=no)
fi
if test "$enable_libao" = yes; then
   AC_DEFINE(HAVE_LIBAO, 1, [Define to 1 if you have libao.])
fi
AM_CONDITIONAL(HAVE_LIBAO, test x$enable_libao = xyes)
AC_SUBST(AO_LIBS)

dnl Check for OSS
AC_MSG_CHECKING([whether to try building OSS sound driver])
AC_ARG_ENABLE(oss,
    AC_HELP_STRING([--disable-oss], [Don't build OSS sound driver.]),,enable_oss=yes)
AC_MSG_RESULT($enable_oss)
if test "$enable_oss" = "yes"; then
    AC_CHECK_HEADERS(sys/soundcard.h,,
        [AC_CHECK_HEADERS(machine/soundcard.h,
            [AC_CHECK_LIB(ossaudio, _oss_ioctl, OSS_LIBS="$OSS_LIBS -lossaudio")],
            enable_oss=no)])
fi
if test "$enable_oss" = yes; then
   AC_DEFINE(HAVE_OSS, 1, [Define to 1 if you have OSS.])
fi
AM_CONDITIONAL(HAVE_OSS, test x$enable_oss = xyes)
AC_SUBST(OSS_LIBS)

dnl Check for Sun audio
AC_MSG_CHECKING([whether to try building Sun audio driver])
AC_ARG_ENABLE(sun-audio,
    AC_HELP_STRING([--disable-sun-audio], [Don't build Sun audio driver.]),,enable_sun_audio=yes)
AC_MSG_RESULT($enable_sun_audio)
if test "$enable_sun_audio" = "yes"; then
    AC_CHECK_HEADERS(sys/audioio.h,,
        [AC_CHECK_HEADERS(sun/audioio.h,, enable_sun_audio=no)])
fi
if test "$enable_sun_audio" = yes; then
   AC_DEFINE(HAVE_SUN_AUDIO, 1, [Define to 1 if you have Sun audio.])
fi
AM_CONDITIONAL(HAVE_SUN_AUDIO, test x$enable_sun_audio = xyes)

dnl Check for libgsm
AC_CHECK_HEADERS(gsm/gsm.h, found_libgsm=yes,
    [AC_CHECK_HEADERS(gsm.h, found_libgsm=yes)])
if test "$found_libgsm" = yes; then
    AC_CHECK_LIB(gsm, gsm_create, GSM_LIBS="$GSM_LIBS -lgsm")
    AC_DEFINE(EXTERNAL_GSM, 1, [Define if you are using an external GSM library])
else
    LIBGSM_LIBADD=../libgsm/libgsm.la
fi
AC_SUBST(LIBGSM_LIBADD)
AC_SUBST(GSM_LIBS)
AM_CONDITIONAL(EXTERNAL_GSM, test x$found_libgsm = xyes)

dnl Check for libsndfile
AC_ARG_WITH(sndfile,
    AC_HELP_STRING([--without-sndfile],
    [Don't try to use libsndfile]),
    [with_sndfile=$withval])
using_sndfile=no
if test "$with_sndfile" != "no"; then
    using_sndfile=yes
    SOX_PATH_SNDFILE(, using_sndfile=no)
    if test "$with_sndfile" = "yes" -a "$using_sndfile" = "no"; then
        AC_MSG_FAILURE([cannot find libsndfile])
    fi
fi
AM_CONDITIONAL(HAVE_SNDFILE, test x$using_sndfile = xyes)
AC_SUBST(SNDFILE_CFLAGS)
AC_SUBST(SNDFILE_LIBS)
        
dnl Check for Ogg Vorbis libraries
AC_ARG_WITH(ogg,
    AC_HELP_STRING([--without-ogg], 
	[Don't try to use Ogg Vorbis]),
        [with_ogg=$withval])
using_ogg=no
if test "$with_ogg" != "no"; then
    using_ogg=yes
    AC_CHECK_HEADER(vorbis/codec.h,
        [AC_CHECK_LIB(ogg, ogg_packet_clear, OGG_LIBS="$OGG_LIBS -logg", using_ogg=no)
        AC_CHECK_LIB(vorbis, vorbis_analysis_headerout, OGG_LIBS="$OGG_LIBS -lvorbis", using_ogg=no)
        AC_CHECK_LIB(vorbisfile, ov_clear, OGG_LIBS="$OGG_LIBS -lvorbisfile", using_ogg=no)
        AC_CHECK_LIB(vorbisenc, vorbis_encode_init_vbr, OGG_LIBS="$OGG_LIBS -lvorbisenc", using_ogg=no)],
        using_ogg=no)
    if test "$with_ogg" = "yes" -a "$using_ogg" = "no"; then
        AC_MSG_FAILURE([cannot find Ogg Vorbis])
    fi
fi
if test "$using_ogg" = yes; then
   AC_DEFINE(HAVE_OGG_VORBIS, 1, [Define to 1 if you have Ogg Vorbis.])
fi
AM_CONDITIONAL(HAVE_OGG_VORBIS, test x$using_ogg = xyes)
AC_SUBST(OGG_LIBS)

dnl Check for FLAC libraries
AC_ARG_WITH(flac,
    AC_HELP_STRING([--without-flac],
        [Don't try to use FLAC]),
        [with_flac=$withval])
using_flac=no
if test "$with_flac" != "no"; then
    using_flac=yes
    AC_CHECK_HEADER(FLAC/all.h,
        [AC_CHECK_LIB(FLAC, FLAC__stream_encoder_new, FLAC_LIBS="$FLAC_LIBS -lFLAC",using_flac=no)],
        using_flac=no)
    if test "$with_flac" = "yes" -a "$using_flac" = "no"; then
        AC_MSG_FAILURE([cannot find FLAC])
    fi
fi
if test "$using_flac" = yes; then
   AC_DEFINE(HAVE_FLAC, 1, [Define to 1 if you have FLAC.])
fi
AM_CONDITIONAL(HAVE_FLAC, test x$using_flac = xyes)
AC_SUBST(FLAC_LIBS)

dnl Check for ffmpeg libraries
AC_ARG_WITH(ffmpeg,
    AC_HELP_STRING([--without-ffmpeg],
        [Don't try to use ffmpeg]),
        [with_ffmpeg=$withval])
using_ffmpeg=no
if test "$with_ffmpeg" != "no"; then
    using_ffmpeg=yes
    AC_CHECK_HEADER(ffmpeg/avformat.h,
        [AC_CHECK_LIB(avformat, av_open_input_file, FFMPEG_LIBS="$FFMPEG_LIBS -lavformat", using_ffmpeg=no)
        AC_CHECK_LIB(avutil, av_rescale_q, FFMPEG_LIBS="$FFMPEG_LIBS -lavutil", using_ffmpeg=no)
        AC_CHECK_LIB(avcodec, avcodec_encode_audio, FFMPEG_LIBS="$FFMPEG_LIBS -lavcodec", using_ffmpeg=no)],
        using_ffmpeg=no)
    if test "$with_ffmpeg" = "yes" -a "$using_ffmpeg" = "no"; then
        AC_MSG_FAILURE([cannot find ffmpeg])
    fi
fi
if test "$using_ffmpeg" = yes; then
   AC_DEFINE(HAVE_FFMPEG, 1, [Define to 1 if you have ffmpeg.])
fi
AM_CONDITIONAL(HAVE_FFMPEG, test x$using_ffmpeg = xyes)
AC_SUBST(FFMPEG_LIBS)

dnl Check for MAD libraries
AC_ARG_WITH(mad,
    AC_HELP_STRING([--without-mad],
        [Don't try to use MAD (MP3 Audio Decoder)]),
        [with_mad=$withval])
using_mad=no
if test "$with_mad" != "no"; then
    using_mad=yes
    AC_CHECK_HEADERS(mad.h,
        [AC_CHECK_LIB(mad, mad_stream_buffer, MP3_LIBS="$MP3_LIBS -lmad",using_mad=no)],
        with_mad=no)
    if test "$with_mad" = "yes" -a "$using_mad" = "no"; then
        AC_MSG_FAILURE([cannot find libmad])
    fi
fi

dnl Test for LAME library.
AC_ARG_WITH(lame,
    AC_HELP_STRING([--without-lame],
        [Don't try to use LAME (LAME Ain't an MP3 Encoder)]),
        [with_lame=$withval])
using_lame=no
if test "$with_lame" != "no"; then
    using_lame=yes
    AC_CHECK_HEADERS(lame/lame.h,,
        [AC_CHECK_HEADERS(lame.h,, using_lame=no)])
    AC_CHECK_LIB(mp3lame, lame_init, MP3_LIBS="$MP3_LIBS -lmp3lame",using_lame=no)
    if test "$with_lame" = "yes" -a "$using_lame" = "no"; then
        AC_MSG_FAILURE([cannot find LAME])
    fi
fi

dnl MP3 library depends on libmad || LAME
AM_CONDITIONAL(HAVE_MP3, test "x$using_mad" = "xyes" -o "x$using_lame" = "xyes")
AC_SUBST(MP3_LIBS)

dnl Check for amr-wb libraries
AC_ARG_WITH(amr-wb,
    AC_HELP_STRING([--without-amr-wb],
        [Don't try to use amr-wb]),
        [with_amr_wb=$withval])
using_amr_wb=no
if test "$with_amr_wb" != "no"; then
    using_amr_wb=yes
    AC_CHECK_HEADER(amrwb/dec.h,
        [AC_CHECK_LIB(amrwb, D_IF_init, AMR_WB_LIBS="$AMR_WB_LIBS -lamrwb",using_amr_wb=no)],
        using_amr_wb=no)
    if test "$with_amr_wb" = "yes" -a "$using_amr_wb" = "no"; then
        AC_MSG_FAILURE([cannot find amr-wb])
    fi
fi
if test "$using_amr_wb" = yes; then
   AC_DEFINE(HAVE_AMR_WB, 1, [Define to 1 if you have AMR-WB.])
fi
AM_CONDITIONAL(HAVE_AMR_WB, test x$using_amr_wb = xyes)
AC_SUBST(AMR_WB_LIBS)

dnl Check for amr-nb libraries
AC_ARG_WITH(amr-nb,
    AC_HELP_STRING([--without-amr-nb],
        [Don't try to use amr-nb]),
        [with_amr_nb=$withval])
using_amr_nb=no
if test "$with_amr_nb" != "no"; then
    using_amr_nb=yes
    AC_CHECK_HEADER(amrnb/sp_dec.h,
        [AC_CHECK_LIB(amrnb, Decoder_Interface_init, AMR_NB_LIBS="$AMR_NB_LIBS -lamrnb" ,using_amr_nb=no)],
        using_amr_nb=no)
    if test "$with_amr_nb" = "yes" -a "$using_amr_nb" = "no"; then
        AC_MSG_FAILURE([cannot find amr-nb])
    fi
fi
if test "$using_amr_nb" = yes; then
   AC_DEFINE(HAVE_AMR_NB, 1, [Define to 1 if you have AMR-NB.])
fi
AM_CONDITIONAL(HAVE_AMR_NB, test x$using_amr_nb = xyes)
AC_SUBST(AMR_NB_LIBS)

dnl Test for libsamplerate.
AC_ARG_WITH(samplerate,
    AC_HELP_STRING([--without-samplerate],
        [Don't try to use libsamplerate (aka Secret Rabbit Code)]),
        [with_samplerate=$withval])
using_samplerate=no
if test "$with_samplerate" != "no"; then
    using_samplerate=yes
    SOX_PATH_SAMPLERATE(, using_samplerate=no)
    if test "$with_samplerate" = "yes" -a "$using_samplerate" = "no"; then
        AC_MSG_FAILURE([cannot find libsamplerate])
    fi
fi
AC_SUBST(SAMPLERATE_CFLAGS)
AC_SUBST(SAMPLERATE_LIBS)

dnl Test for LADSPA
AC_ARG_WITH(ladspa,
    AC_HELP_STRING([--without-ladspa], [Don't try to use LADSPA]), [with_ladspa=$withval])
using_ladspa=no
if test "$with_ladspa" != "no" -a "$using_libltdl" = "yes"; then
    using_ladspa=yes
    AC_CHECK_HEADERS(ladspa.h,, using_ladspa=no)
    if test "$with_ladspa" = "yes" -a "$using_ladspa" = "no"; then
        AC_MSG_FAILURE([cannot find ladspa.h])
    fi
fi
LADSPA_PATH=${libdir}/ladspa
AC_ARG_WITH(ladspa-path,
    AC_HELP_STRING([--with-ladspa-path], [Default search path for LADSPA plugins]), [LADSPA_PATH=$withval])
AC_SUBST(LADSPA_PATH)

dnl Generate output files.
AX_CREATE_STDINT_H(src/soxstdint.h)
AC_CONFIG_FILES(Makefile src/Makefile libgsm/Makefile lpc10/Makefile)
AC_OUTPUT

if test "$found_libgsm" = "yes"; then
   gsm_option="external"
else
   gsm_option="in-tree"
fi

dnl Report configuration.
echo
echo "Debugging build................... $enable_debug"
echo "External module support........... $using_libltdl"
echo "ALSA driver....................... $enable_alsa"
echo "libao driver...................... $enable_libao"
echo "OSS driver........................ $enable_oss"
echo "SUN audio driver.................. $enable_sun_audio"
echo "play and rec symlinks............. $enable_playrec_symlinks"
echo "libgsm............................ $gsm_option"
echo "libsndfile formats................ $using_sndfile"
echo "Ogg Vorbis format................. $using_ogg"
echo "FLAC format....................... $using_flac"
echo "ffmpeg formats.................... $using_ffmpeg"
echo "MAD MP3 reader.................... $using_mad"
echo "LAME MP3 writer................... $using_lame"
echo "AMR-WB format..................... $using_amr_wb"
echo "AMR-NB format..................... $using_amr_nb"
echo "LADSPA effects.................... $using_ladspa"
echo "Secret Rabbit Code resampling..... $using_samplerate"
echo
echo "Configure finished.  Do 'make && make install' to compile and install SoX."
echo
