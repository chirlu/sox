dnl Process this file with autoconf to produce a configure script.

AC_INIT(SoX, 12.18.3, cbagwell@users.sourceforge.net)
AM_INIT_AUTOMAKE

dnl create a config.h file (Automake will add -DHAVE_CONFIG_H)
AM_CONFIG_HEADER(src/stconfig.h)

AC_CONFIG_SRCDIR(sox.1)

dnl Parameters to configure

AC_ARG_ENABLE(gsm,
	[  --disable-external-gsm  Disable detection of external GSM library],
	enable_external_gsm=$enableval, enable_external_gsm=yes)

AC_ARG_ENABLE(alsa_dsp,
	[  --disable-alsa-dsp      Disable detection of ALSA],
	[enable_alsa_dsp=$enableval],
	[enable_alsa_dsp=yes])

AC_ARG_ENABLE(oss_dsp,
	[  --disable-oss-dsp       Disable detection of OSS],
	[enable_oss_dsp=$enableval],
	[enable_oss_dsp=yes])

AC_ARG_ENABLE(sun_audio,
	[  --disable-sun-audio     Disable detection of SUN-style audio],
	[enable_sun_audio=$enableval],
	[enable_sun_audio=yes])

AC_ARG_ENABLE(ogg_vorbis,
	[  --disable-ogg-vorbis    Disable detection of Ogg Vorbis],
	[enable_ogg_vorbis=$enableval],
	[enable_ogg_vorbis=yes])

AC_ARG_ENABLE(flac,
	[  --disable-flac          Disable detection of FLAC],
	[enable_flac=$enableval],
	[enable_flac=yes])

AC_ARG_ENABLE(mad,
	[  --disable-mad           Disable detection of MAD (MP3 Audio Decoder)],
	[enable_mad=$enableval],
	[enable_mad=yes])

AC_ARG_ENABLE(lame,
	[  --disable-lame          Disable detection of LAME (LAME Ain't an MP3 Encoder)],
	[enable_lame=$enableval],
	[enable_lame=yes])

AC_ARG_ENABLE(samplerate,
        [  --disable-samplerate    Detection of libsamplerate (aka Secret Rabbit Code)],
        [enable_samplerate=$enableval],
        [enable_samplerate=yes])

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

dnl Check if math library is needed.

AC_CHECKING(if math library is required during link)
AC_CHECK_FUNC(pow)
if test "$ac_cv_func_pow" = no
then
  AC_CHECK_LIB(m, pow)
fi

dnl Large File Support
AC_SYS_LARGEFILE
AC_FUNC_FSEEKO
      
dnl Initial values for exported symbols.

PLAY_SUPPORT=0

dnl Test for Ogg Vorbis libraries.

if test "$enable_ogg_vorbis" = yes
then
    AC_CHECK_HEADER(vorbis/codec.h, found_ogg_vorbis=yes, enable_ogg_vorbis=no)
    if test "$found_ogg_vorbis" = yes
    then
        AC_CHECK_LIB(vorbis, vorbis_analysis_init,
                     LIBS="-lvorbisfile -lvorbisenc -lvorbis -logg $LIBS"
                     AC_DEFINE([HAVE_LIBVORBIS], 1, 
		               [Define if you have Ogg Vorbis Library installed]),
	             enable_ogg_vorbis=no, -logg)
    fi
fi

dnl Test for FLAC libraries.

if test "$enable_flac" = yes
then
    AC_CHECK_HEADER(FLAC/file_encoder.h, found_flac=yes, enable_flac=no)
    if test "$found_flac" = yes
    then
        AC_CHECK_LIB(FLAC, FLAC__file_encoder_new,
                     LIBS="-lFLAC $LIBS"
                     AC_DEFINE([HAVE_LIBFLAC], 1, 
		               [Define if you have FLAC Library installed]),
	             enable_flac=no)
    fi
fi

dnl Test for MAD libraries.

if test "$enable_mad" = yes
then
    AC_CHECK_HEADER(mad.h, found_mad=yes, enable_mad=no)
    if test "$found_mad" = yes
    then
        AC_CHECK_LIB(mad, mad_stream_buffer,
                     LIBS="$LIBS -lmad" 
		     AC_DEFINE([HAVE_LIBMAD], 1, 
		               [Define if you have MAD (MP3 Audio Decoder) Library installed]),
		     enable_mad=no)
     fi
fi

dnl Test for LAME library.

if test "$enable_lame" = yes
then
    AC_CHECK_HEADER(lame/lame.h, found_lame=yes, enable_lame=no)
    if test "$found_lame" = yes
    then
        AC_CHECK_LIB(mp3lame, lame_init,
                     LIBS="$LIBS -lmp3lame"
                     AC_DEFINE([HAVE_LAME], 1, 
		               [Define if you have LAME (LAME Ain't an MP3 Encoder) library installed]),
		     enable_lame=no)
    fi
fi

dnl Test for libsamplerate.

if test "$enable_samplerate" = yes
then
    PKG_CHECK_MODULES(SAMPLERATE, samplerate >= 0.0.15,
        AC_DEFINE_UNQUOTED([HAVE_SAMPLERATE],1,
            [Set to 1 if you have libsamplerate.]),
        enable_samplerate=no)
fi
AC_SUBST(SAMPLERATE_CFLAGS)
AC_SUBST(SAMPLERATE_LIBS)

dnl Checks for ALSA.

if test "$enable_alsa_dsp" = yes
then
    AC_CHECK_HEADERS(alsa/asoundlib.h, found_alsa_dsp=yes)

    if test "$found_alsa_dsp" = yes
    then
        AC_CHECK_LIB(asound, snd_pcm_open,
                     LIBS="$LIBS -lasound"
                     PLAY_SUPPORT=1
                     AC_DEFINE([HAVE_ALSA], 1, 
		               [Define if you have ALSA library installed]),
		     enable_lame=no)
    else
        enable_alsa_dsp=no
    fi
fi
AM_CONDITIONAL(ALSA, test x$enable_alsa_dsp = xyes)

if test "$enable_oss_dsp" = yes
then
    AC_CHECK_HEADERS(sys/soundcard.h machine/soundcard.h, found_oss_dsp=yes)

    if test "$found_oss_dsp" = yes
    then
        AC_DEFINE([HAVE_OSS], 1, 
                  [Define if you have OSS installed])
	PLAY_SUPPORT=1
    else
        enable_oss_dsp=no
    fi
fi
AM_CONDITIONAL(OSS, test x$enable_oss_dsp = xyes)

if test "$enable_sun_audio" = yes
then
    AC_CHECK_HEADERS(sys/audioio.h sun/audioio.h, found_sun_audio=yes)

    if test "$found_sun_audio" = yes
    then
        AC_DEFINE([HAVE_SUNAUDIO], 1, 
                  [Define if you have Sun /dev/audio installed])
	PLAY_SUPPORT=1
    else
        enable_sun_audio=no
    fi
fi
AM_CONDITIONAL(SUNAU, test x$enable_sun_audio = xyes)

if test "$enable_external_gsm" = yes
then
    AC_CHECK_HEADERS(gsm/gsm.h, found_libgsm=yes)

    if test "$found_libgsm" = yes
    then
        AC_CHECK_LIB(gsm, gsm_create, LIBS="$LIBS -lgsm")
        AC_DEFINE([EXTERNAL_GSM], 1, [Define if you are using an external GSM library])
    fi
fi

AC_SUBST(PLAY_SUPPORT)

dnl Generate output files...

AC_CONFIG_FILES([Makefile src/libst-config src/Makefile src/libgsm/Makefile])
AC_CONFIG_FILES([src/play], [chmod +x src/play])

AC_OUTPUT()

AX_CREATE_STDINT_H(src/ststdint.h)

if test ! -f src/tests.sh; then cp ${srcdir}/src/tests.sh src/tests.sh; fi
if test ! -f src/testall.sh; then cp ${srcdir}/src/testall.sh src/testall.sh; fi
if test ! -f src/monkey.au; then cp ${srcdir}/src/monkey.au src/monkey.au; fi
if test ! -f src/monkey.wav; then cp ${srcdir}/src/monkey.wav src/monkey.wav; fi

echo
echo "ALSA Driver.......................   $enable_alsa_dsp"
echo "OSS Driver........................   $enable_oss_dsp"
echo "SUN /dev/audio....................   $enable_sun_audio"
echo "Ogg Vorbis support................   $enable_ogg_vorbis"
echo "FLAC support......................   $enable_flac"
echo "MAD MP3 Decoder...................   $enable_mad"
echo "LAME MP3 Encoder..................   $enable_lame"
echo "Secret Rabbit Code resampling.....   $enable_samplerate"
echo
echo "Configure finished.  Do 'make; make install' to compile and install SoX."
echo

dnl Local Variables:
dnl comment-start: "dnl "
dnl comment-end: ""
dnl comment-start-skip: "\\bdnl\\b\\s *"
dnl compile-command: "autoconf"
dnl End:
