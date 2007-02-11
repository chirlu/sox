dnl Process this file with autoconf to produce a configure script.

AC_INIT(SoX, 13.0.0, sox-devel@lists.sourceforge.net)
AM_INIT_AUTOMAKE

dnl create a config.h file (Automake will add -DHAVE_CONFIG_H)
AM_CONFIG_HEADER(src/stconfig.h)

AC_CONFIG_SRCDIR(sox.1)

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)
m4_ifdef([PKG_PROG_PKG_CONFIG], [PKG_PROG_PKG_CONFIG])

dnl Debugging
AC_MSG_CHECKING([if we want a debug build])
AC_ARG_ENABLE(debug,
AC_HELP_STRING([--enable-debug], [make a debug build]),
[
if test $enableval = yes; then
        CFLAGS="-g"
        if test "$GCC" = "yes"; then
          CFLAGS="$CFLAGS -ggdb"
        fi
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi
], [
AC_MSG_RESULT(no)
])

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
AC_CHECKING(if math library is required during link)
AC_CHECK_FUNC(pow)
if test "$ac_cv_func_pow" = no; then
  AC_CHECK_LIB(m, pow)
fi

dnl Large File Support
AC_SYS_LARGEFILE
AC_FUNC_FSEEKO

dnl Allow libtool to be silenced
AC_ARG_ENABLE(silent-libtool,
    AC_HELP_STRING([--enable-silent-libtool],
        [Pass --silent to libtool.]),
	LIBTOOLFLAGS=--silent)
AC_SUBST(LIBTOOLFLAGS)

dnl Check for ALSA audio
AC_CHECK_HEADERS(alsa/asoundlib.h,
    [with_alsa=yes
    AC_CHECK_LIB(asound, snd_pcm_open,, with_alsa=no)],
    with_alsa=no)
if test "$with_alsa" = yes; then
   AC_DEFINE(HAVE_ALSA, 1, [Define to 1 if you have ALSA.])
fi

dnl Check for OSS audio
with_oss=no
AC_CHECK_HEADERS(sys/soundcard.h, with_oss=yes)
AC_CHECK_HEADERS(machine/soundcard.h, with_oss=yes)
if test "$with_oss" = yes; then
   AC_DEFINE(HAVE_OSS, 1, [Define to 1 if you have OSS.])
fi

dnl Check for Sun audio
with_sun_audio=no
AC_CHECK_HEADERS(sys/audioio.h, with_sun_audio=yes)
AC_CHECK_HEADERS(sun/audioio.h, with_sun_audio=yes)
if test "$with_sun_audio" = yes; then
   AC_DEFINE(HAVE_SUN_AUDIO, 1, [Define to 1 if you have Sun /dev/audio.])
fi

dnl Check for libgsm
AC_CHECK_HEADERS(gsm/gsm.h, found_libgsm=yes)
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
    AC_HELP_STRING([--with-flac],
        [Use FLAC if present (default is yes)]),
        [with_flac=$withval],
        [with_flac=yes])
if test "$with_flac" = yes; then
    AC_CHECK_HEADER(FLAC/file_encoder.h,
        [AC_CHECK_LIB(FLAC, FLAC__file_encoder_new,,with_flac=no)],
        with_flac=no)
fi

dnl Check for MAD libraries
AC_ARG_WITH(mad,
    AC_HELP_STRING([--with-mad],
        [Use MAD (MP3 Audio Decoder) if present (default is yes)]),
        [with_mad=$withval],
        [with_mad=yes])
if test "$with_mad" = yes; then
    AC_CHECK_HEADER(mad.h,
        [AC_CHECK_LIB(mad, mad_stream_buffer,,with_mad=no)],
        with_mad=no)
fi

dnl Test for LAME library.
AC_ARG_WITH(lame,
    AC_HELP_STRING([--with-lame],
        [Use LAME (LAME Ain't an MP3 Encoder) if present (default is yes)]),
        [with_lame=$withval],
        [with_lame=yes])
if test "$with_lame" = yes; then
    AC_CHECK_HEADER(lame/lame.h,
        [AC_CHECK_LIB(mp3lame, lame_init,,with_lame=no)],
        with_lame=no)
fi

dnl Test for libsamplerate.
AC_ARG_WITH(samplerate,
    AC_HELP_STRING([--with-samplerate],
        [Use libsamplerate (aka Secret Rabbit Code) if present (default is yes)]),
        [with_samplerate=$withval],
        [with_samplerate=yes])
if test "$with_samplerate" = yes; then
  SOX_PATH_SAMPLERATE(, 
                      [with_samplerate=no
                       AC_MSG_NOTICE([Could not find a usable libsamplerate])])
fi
AC_SUBST(SAMPLERATE_CFLAGS)
AC_SUBST(SAMPLERATE_LIBS)

dnl Generate output files.
AX_CREATE_STDINT_H(src/ststdint.h)
AC_CONFIG_FILES(Makefile src/Makefile src/libgsm/Makefile src/libst-config)
AC_OUTPUT

if test "$found_libgsm" = "yes"; then
   gsm_option="external"
else
   gsm_option="in-tree"
fi

dnl Report configuration.
echo
echo "ALSA driver....................... $with_alsa"
echo "OSS driver........................ $with_oss"
echo "SUN audio driver.................. $with_sun_audio"
echo "libgsm............................ $gsm_option"
echo "libsndfile formats................ $with_sndfile"
echo "Ogg Vorbis format................. $with_ogg_vorbis"
echo "FLAC format....................... $with_flac"
echo "MAD MP3 reader.................... $with_mad"
echo "LAME MP3 writer................... $with_lame"
echo "Secret Rabbit Code resampling..... $with_samplerate"
echo
echo "Configure finished.  Do 'make && make install' to compile and install SoX."
echo
