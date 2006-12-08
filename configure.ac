dnl Process this file with autoconf to produce a configure script.

AC_INIT(SoX, 12.18.3, cbagwell@users.sourceforge.net)
AM_INIT_AUTOMAKE

dnl create a config.h file (Automake will add -DHAVE_CONFIG_H)
AM_CONFIG_HEADER(src/stconfig.h)

AC_CONFIG_SRCDIR(sox.1)

dnl Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB
AC_PROG_INSTALL
AC_PROG_LN_S

dnl Extra CFLAGS if we have gcc
if test "$GCC" = yes; then
    CFLAGS="$CFLAGS -Wall"
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

dnl Large File Support
AC_SYS_LARGEFILE
AC_FUNC_FSEEKO
      
dnl Check for ALSA audio
AC_CHECK_HEADERS(alsa/asoundlib.h,
    [with_alsa=yes
    AC_CHECK_LIB(asound, snd_pcm_open,, with_alsa=no)],
    with_alsa=no)

dnl Check for OSS audio
with_oss=no
AC_CHECK_HEADERS(sys/soundcard.h, with_oss=yes)
AC_CHECK_HEADERS(machine/soundcard.h, with_oss=yes)

dnl Check for Sun-style audio
with_sun_audio=no
AC_CHECK_HEADERS(sys/audioio.h, with_sun_audio=yes)
AC_CHECK_HEADERS(sun/audioio.h, with_sun_audio=yes)

dnl Check for Ogg Vorbis libraries
AC_CHECK_HEADER(vorbis/codec.h,
    [with_ogg_vorbis=yes
    AC_CHECK_LIB(ogg, ogg_packet_clear,, with_ogg_vorbis=no)
    AC_CHECK_LIB(vorbisfile, ov_clear,, with_ogg_vorbis=no)
    AC_CHECK_LIB(vorbisenc, vorbis_encode_init_vbr,, with_ogg_vorbis=no)],
    with_ogg_vorbis=no)

dnl Check for FLAC libraries
with_flac=yes
AC_ARG_WITH(flac,
    AS_HELP_STRING([--with-flac],
        [Use FLAC if present (default is YES)]))
if test "$with_flac" = yes
then
    AC_CHECK_HEADER(FLAC/file_encoder.h,
        [AC_CHECK_LIB(FLAC, FLAC__file_encoder_new,,with_flac=no)],
        with_flac=no)
fi

dnl Check for MAD libraries
with_mad=yes
AC_ARG_WITH(mad,
    AS_HELP_STRING([--with-mad],
        [Use MAD (MP3 Audio Decoder) if present (default is YES)]))
if test "$with_mad" = yes
then
    AC_CHECK_HEADER(mad.h,
        [AC_CHECK_LIB(mad, mad_stream_buffer,,with_mad=no)],
        with_mad=no)
fi

dnl Test for LAME library.
with_lame=yes
AC_ARG_WITH(lame,
    AS_HELP_STRING([--with-lame],
        [Use LAME (LAME Ain't an MP3 Encoder) if present (default is YES)]))
if test "$with_lame" = yes
then
    AC_CHECK_HEADER(lame/lame.h,
        [AC_CHECK_LIB(mp3lame, lame_init,,with_lame=no)],
        with_lame=no)
fi

dnl Test for libsamplerate.
with_samplerate=yes
AC_ARG_WITH(samplerate,
    AS_HELP_STRING([--with-samplerate],
        [Use libsamplerate (aka Secret Rabbit Code) if present (default is YES)]))
if test "$with_samplerate" = yes
then
    PKG_CHECK_MODULES(SAMPLERATE, samplerate >= 0.0.15,
        AC_DEFINE_UNQUOTED([HAVE_SAMPLERATE],1,
            [Set to 1 if you have libsamplerate.]),
        with_samplerate=no)
fi
AC_SUBST(SAMPLERATE_CFLAGS)
AC_SUBST(SAMPLERATE_LIBS)

dnl Generate output files.
AX_CREATE_STDINT_H(src/ststdint.h)
AC_SUBST(with_alsa with_oss) dnl For play.in
AC_CONFIG_FILES(Makefile src/Makefile src/libgsm/Makefile src/libst-config src/play)
AC_OUTPUT

dnl Report configuration.
echo
echo "ALSA Driver....................... $with_alsa"
echo "OSS Driver........................ $with_oss"
echo "SUN /dev/audio.................... $with_sun_audio"
echo "Ogg Vorbis support................ $with_ogg_vorbis"
echo "FLAC support...................... $with_flac"
echo "MAD MP3 Decoder................... $with_mad"
echo "LAME MP3 Encoder.................. $with_lame"
echo "Secret Rabbit Code resampling..... $with_samplerate"
echo
echo "Configure finished.  Do 'make; make install' to compile and install SoX."
echo
