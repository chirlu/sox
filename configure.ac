dnl Process this file with autoconf to produce a configure script.

AC_INIT(SoX, 14.0.0, sox-devel@lists.sourceforge.net)
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
AC_LIBLTDL_INSTALLABLE
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL
AC_CONFIG_SUBDIRS(libltdl)
AC_SUBST(LIBTOOL_DEPS)
AC_SUBST(LIBLTDL)
AC_SUBST(LTDLINCL)
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
fi

dnl Check for system dependent features.
AC_C_BIGENDIAN
AC_C_INLINE

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h unistd.h byteswap.h)

dnl Checks for library functions.
AC_CHECK_FUNCS(strcasecmp strdup)
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
        AC_CHECK_LIB(asound, snd_pcm_open,, enable_alsa=no),
        enable_alsa=no)
fi
if test "$enable_alsa" = yes; then
   AC_DEFINE(HAVE_ALSA, 1, [Define to 1 if you have ALSA.])
fi

dnl Check for libao
AC_MSG_CHECKING([whether to try building libao sound driver])
AC_ARG_ENABLE(libao,
    AC_HELP_STRING([--disable-libao], [Don't build libao sound driver.]),,enable_libao=yes)
AC_MSG_RESULT($enable_libao)
if test "$enable_libao" = "yes"; then
    AC_CHECK_HEADERS(ao/ao.h,
        AC_CHECK_LIB(ao, ao_play,, enable_libao=no),
        enable_libao=no)
fi

dnl Check for OSS
AC_MSG_CHECKING([whether to try building OSS sound driver])
AC_ARG_ENABLE(oss,
    AC_HELP_STRING([--disable-oss], [Build OSS sound driver.]),,enable_oss=yes)
AC_MSG_RESULT($enable_oss)
if test "$enable_oss" = "yes"; then
    AC_CHECK_HEADERS(sys/soundcard.h,,
        [AC_CHECK_HEADERS(machine/soundcard.h,
            [AC_CHECK_LIB(ossaudio, _oss_ioctl, LIBS="$LIBS -lossaudio")],
            enable_oss=no)])
fi
if test "$enable_oss" = yes; then
   AC_DEFINE(HAVE_OSS, 1, [Define to 1 if you have OSS.])
fi

dnl Check for Sun audio
AC_MSG_CHECKING([whether to try building Sun audio driver])
AC_ARG_ENABLE(sun-audio,
    AC_HELP_STRING([--disble-sun-audio], [Build Sun audio driver.]),,enable_sun_audio=yes)
AC_MSG_RESULT($enable_sun_audio)
if test "$enable_sun_audio" = "yes"; then
    AC_CHECK_HEADERS(sys/audioio.h,,
        [AC_CHECK_HEADERS(sun/audioio.h,, enable_sun_audio=no)])
fi
if test "$enable_sun_audio" = yes; then
   AC_DEFINE(HAVE_SUN_AUDIO, 1, [Define to 1 if you have Sun audio.])
fi

dnl Check for libgsm
AC_CHECK_HEADERS(gsm/gsm.h, found_libgsm=yes,
    [AC_CHECK_HEADERS(gsm.h, found_libgsm=yes)])
if test "$found_libgsm" = yes; then
    AC_CHECK_LIB(gsm, gsm_create, LIBS="$LIBS -lgsm")
    AC_DEFINE(EXTERNAL_GSM, 1, [Define if you are using an external GSM library])
else
    LIBGSM_SUBDIR=src/libgsm
    LIBGSM_LIBADD=libgsm/libgsm.la
fi
AC_SUBST(LIBGSM_SUBDIR)
AC_SUBST(LIBGSM_LIBADD)
     
dnl Check for libsndfile
AC_ARG_WITH(sndfile, AC_HELP_STRING([--with-sndfile],
    [Use libsndfile if present (default is yes)]),
    [with_sndfile=$withval],
    [with_sndfile=yes])
if test "$with_sndfile" = yes; then
  SOX_PATH_SNDFILE(, [with_sndfile=no
                       AC_MSG_NOTICE([Could not find a usable libsndfile])])
fi
AC_SUBST(SNDFILE_CFLAGS)
AC_SUBST(SNDFILE_LIBS)
        
dnl Check for Ogg Vorbis libraries
AC_CHECK_HEADER(vorbis/codec.h,
    [with_ogg_vorbis=yes
    AC_CHECK_LIB(ogg, ogg_packet_clear,, with_ogg_vorbis=no)
    AC_CHECK_LIB(vorbisfile, ov_clear,, with_ogg_vorbis=no)
    AC_CHECK_LIB(vorbisenc, vorbis_encode_init_vbr,, with_ogg_vorbis=no)],
    with_ogg_vorbis=no)

dnl Check for FLAC libraries
AC_ARG_WITH(flac,
    AC_HELP_STRING([--without-flac],
        [Don't try to use FLAC]),
        [with_flac=$withval])
if test "$with_flac" != "no"; then
    using_flac=yes
    AC_CHECK_HEADER(FLAC/all.h,
        [AC_CHECK_LIB(FLAC, FLAC__stream_encoder_new,,using_flac=no)],
        using_flac=no)
    if test "$with_flac" = "yes" -a "$using_flac" = "no"; then
        AC_MSG_FAILURE([cannot find FLAC])
    fi
fi

dnl Check for ffmpeg libraries
AC_CHECK_HEADER(ffmpeg/avformat.h,
    [with_ffmpeg=yes
    AC_CHECK_LIB(avformat, av_open_input_file,, with_ffmpeg=no)],
    with_ffmpeg=no)

dnl Check for MAD libraries
AC_ARG_WITH(mad,
    AC_HELP_STRING([--without-mad],
        [Don't try to use MAD (MP3 Audio Decoder)]),
        [with_mad=$withval])
if test "$with_mad" != "no"; then
    using_mad=yes
    AC_CHECK_HEADER(mad.h,
        [AC_CHECK_LIB(mad, mad_stream_buffer,,using_mad=no)],
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
if test "$with_lame" != "no"; then
    using_lame=yes
    AC_CHECK_HEADER(lame/lame.h,
        [AC_CHECK_LIB(mp3lame, lame_init,,using_lame=no)],
        using_lame=no)
    if test "$with_lame" = "yes" -a "$using_lame" = "no"; then
        AC_MSG_FAILURE([cannot find LAME])
    fi
fi

dnl Test for libsamplerate.
AC_ARG_WITH(samplerate,
    AC_HELP_STRING([--without-samplerate],
        [Don't try to use libsamplerate (aka Secret Rabbit Code)]),
        [with_samplerate=$withval])
if test "$with_samplerate" != "no"; then
    using_samplerate=yes
    SOX_PATH_SAMPLERATE(, using_samplerate=no)
    if test "$with_samplerate" = "yes" -a "$using_samplerate" = "no"; then
        AC_MSG_FAILURE([cannot find libsamplerate])
    fi
fi
AC_SUBST(SAMPLERATE_CFLAGS)
AC_SUBST(SAMPLERATE_LIBS)

dnl Generate output files.
AX_CREATE_STDINT_H(src/soxstdint.h)
AC_CONFIG_FILES(Makefile src/Makefile src/libgsm/Makefile amr-wb/Makefile lpc10/Makefile)
AC_OUTPUT

if test "$found_libgsm" = "yes"; then
   gsm_option="external"
else
   gsm_option="in-tree"
fi

dnl Report configuration.
echo
echo "Debugging build................... $enable_debug"
echo "ALSA driver....................... $enable_alsa"
echo "libao driver...................... $enable_libao"
echo "OSS driver........................ $enable_oss"
echo "SUN audio driver.................. $enable_sun_audio"
echo "play and rec symlinks............. $enable_playrec_symlinks"
echo "libgsm............................ $gsm_option"
echo "libsndfile formats................ $with_sndfile"
echo "Ogg Vorbis format................. $with_ogg_vorbis"
echo "FLAC format....................... $using_flac"
echo "ffmpeg formats.................... $with_ffmpeg"
echo "MAD MP3 reader.................... $using_mad"
echo "LAME MP3 writer................... $using_lame"
echo "Secret Rabbit Code resampling..... $using_samplerate"
echo
echo "Configure finished.  Do 'make && make install' to compile and install SoX."
echo
