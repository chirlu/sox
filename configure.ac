dnl Process this file with autoconf to produce a configure script.

AC_INIT(SoX, 14.3.0alpha, sox-devel@lists.sourceforge.net)

m4_ifdef([AC_CONFIG_MACRO_DIR], [AC_CONFIG_MACRO_DIR([m4])])

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

dnl Increase version when binary compatibility with previous version is broken
SHLIB_VERSION=1:0:0
AC_SUBST(SHLIB_VERSION)

AC_ARG_WITH(libltdl,
    AC_HELP_STRING([--without-libltdl],
        [Don't try to use libltdl for external dynamic library support]))

using_libltdl=no
if test "$with_libltdl" != "no"; then
    dnl Disable libltdl support when building only static libraries
    if test "$enable_shared" != "no"; then
        using_libltdl=yes
    fi
    dnl Force off using libltdl on targets that are know to have
    dnl problems.
    case $target in
      *cygwin* | *mingw* | *os2*)
        using_libltdl=no
        ;;
    esac
fi

if test "$using_libltdl" != "no"; then
  AC_CHECK_HEADERS(ltdl.h,
                   AC_CHECK_LIB(ltdl, lt_dlinit, LIBLTDL="$LIBLTDL -lltdl",
                                using_libltdl=no), using_libltdl=no)
fi
if test "$using_libltdl" = yes; then
   AC_DEFINE([HAVE_LIBLTDL], 1, [Define to 1 if you have libltdl])
fi
AM_CONDITIONAL(HAVE_LIBLTDL, test x$using_libltdl = xyes)
AC_SUBST(LIBLTDL)

AC_LIBTOOL_DLOPEN
case $target in
  *cygwin* | *mingw*)
  AC_LIBTOOL_WIN32_DLL
  LDFLAGS="$LDFLAGS -no-undefined"
   ;;
esac
AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

AC_ARG_WITH(pkgconfigdir,
    AC_HELP_STRING([--with-pkgconfigdir],
                   [location to install .pc files or "no" to disable (default=$(libdir)/pkgconfig)]))

m4_ifndef([PKG_PROG_PKG_CONFIG], with_pkgconfigdir="no")

using_pkgconfig=no
if test "$with_pkgconfigdir" != "no"; then
    if test "$with_pkgconfigdir" = ""; then
        with_pkgconfigdir="\$(libdir)/pkgconfig"
    fi
    using_pkgconfig="yes"
    PKG_PROG_PKG_CONFIG
fi
AM_CONDITIONAL(HAVE_PKGCONFIG, test x$using_pkgconfig = xyes)
AC_SUBST(PKGCONFIGDIR, $with_pkgconfigdir)

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

    dnl Detect gcc version.  Convert GCC version number to
    dnl something easier to compare.
    AC_MSG_CHECKING([gcc version])
    gccver=`$CC -dumpversion`
    gccnum1=`echo $gccver | cut -d . -f1`
    gccnum2=`echo $gccver | cut -d . -f2`
    gccver=`(expr $gccnum1 "*" 100 + $gccnum2) 2>/dev/null`
    AC_MSG_RESULT($gccver)

    CFLAGS="$CFLAGS -D_FORTIFY_SOURCE=2 -Wall -W -Wmissing-prototypes -Wstrict-prototypes -pedantic"
    if test "$gccver" -lt 403; then
        WARN_CFLAGS="-Wconversion"
    else
        WARN_CFLAGS="-Wtraditional-conversion"
    fi
    dnl Define stricter policy on GNU/Linux, all symbols must be resolved
    case $target in
      *linux* | *solaris*)
        LDFLAGS="$LDFLAGS -Wl,-z,defs"
        ;;
      *os2*)
        APP_LDFLAGS="-no-undefined"
        ;;
    esac
fi
AC_SUBST(APP_LDFLAGS)
AC_SUBST(WARN_CFLAGS)

AC_ARG_WITH(distro, AC_HELP_STRING([--with-distro=distro], [Provide distribution name]))
if test "x$with_distro" != "x"; then
  DISTRO="$with_distro"
  AC_SUBST(DISTRO)
  AC_DEFINE(HAVE_DISTRO, 1, [1 if DISTRO is defined])
  have_distro=yes
else
  DISTRO="not specified!"
fi
AM_CONDITIONAL(HAVE_DISTRO, test x$have_distro = xyes)

dnl Check for system dependent features.
AC_C_BIGENDIAN
AC_C_INLINE

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h unistd.h byteswap.h sys/time.h sys/timeb.h sys/types.h sys/utsname.h termios.h)

dnl Checks for library functions.
AC_CHECK_FUNCS(strcasecmp strdup popen vsnprintf gettimeofday glob mkstemp)

dnl Check if math library is needed.
AC_CHECK_FUNC(pow)
if test "$ac_cv_func_pow" = no; then
  AC_CHECK_LIB(m, pow)
fi

dnl Large File Support
AC_SYS_LARGEFILE
AC_FUNC_FSEEKO
if test "$ac_cv_sys_file_offset_bits" = "64"; then
  dnl This is either a 64-bit platform or a 32-bit platform
  dnl that supports large files.
  dnl There are to many broken glibc+kernel's out there that
  dnl detect 64-bit off_t on 32-bit system and require
  dnl _LARGEFILE_SOURCE to make fseeko() go into 64-bit mode.
  dnl Force defining _LARGEFILE_SOURCE always.  My assumption
  dnl is its harmlessly ignored on 64-bit platforms and platforms
  dnl that chose other solutions.
  AC_MSG_NOTICE(Forcing _LARGEFILE_SOURCE to be safe since off_t is 64 bits)
  AC_DEFINE(_LARGEFILE_SOURCE, 1)
fi

dnl Allow libtool to be silenced
AC_MSG_CHECKING([whether libtool should be silenced])
AC_ARG_ENABLE(silent-libtool,
    AC_HELP_STRING([--disable-silent-libtool], [Verbose libtool]),,enable_silent_libtool=yes)
AC_MSG_RESULT($enable_silent_libtool)
if test "$enable_silent_libtool" = "yes"; then
    LIBTOOLFLAGS=--silent
fi
AC_SUBST(LIBTOOLFLAGS)



dnl Check for GOMP library
AC_MSG_CHECKING([whether to use GOMP])
AC_ARG_ENABLE(gomp,
    AC_HELP_STRING([--disable-gomp], [Don't use GOMP.]),,enable_gomp=yes)
AC_MSG_RESULT($enable_gomp)
if test "$enable_gomp" = "yes"; then
    AC_CHECK_HEADERS(omp.h,
        AC_CHECK_LIB(gomp, omp_get_thread_num, GOMP_LIBS="$GOMP_LIBS -lgomp", enable_gomp=no),
        enable_gomp=no)
fi
if test "$enable_gomp" = yes; then
   AC_DEFINE(HAVE_OPENMP, 1, [Define to 1 if you have GOMP.])
   CFLAGS="$CFLAGS -fopenmp"
fi
AM_CONDITIONAL(HAVE_OPENMP, test x$enable_gomp = xyes)
AC_SUBST(GOMP_LIBS)



dnl Check for magic library
AC_ARG_WITH(magic,
    AC_HELP_STRING([--without-magic],
        [Don't try to use magic]))
using_magic=no
if test "$with_magic" != "no"; then
    using_magic=yes
    AC_CHECK_HEADER(magic.h,
        [AC_CHECK_LIB(magic, magic_open, MAGIC_LIBS="-lmagic",using_magic=no)],
        using_magic=no)
    if test "$with_magic" = "yes" -a "$using_magic" = "no"; then
        AC_MSG_FAILURE([cannot find magic])
    fi
fi
if test "$using_magic" = yes; then
   AC_DEFINE(HAVE_MAGIC, 1, [Define to 1 if you have magic.])
fi
AM_CONDITIONAL(HAVE_MAGIC, test x$using_magic = xyes)
AC_SUBST(MAGIC_LIBS)



dnl Check for png libraries
AC_ARG_WITH(png,
    AC_HELP_STRING([--without-png],
        [Don't try to use png]))
using_png=no
if test "$with_png" != "no"; then
    using_png=yes
    AC_CHECK_HEADER(png.h,
        [AC_CHECK_LIB(png, png_set_rows, PNG_LIBS="$PNG_LIBS -lpng -lz" ,using_png=no, -lz)],
        using_png=no)
    if test "$with_png" = "yes" -a "$using_png" = "no"; then
        AC_MSG_FAILURE([cannot find png])
    fi
fi
if test "$using_png" = yes; then
   AC_DEFINE(HAVE_PNG, 1, [Define to 1 if you have PNG.])
fi
AM_CONDITIONAL(HAVE_PNG, test x$using_png = xyes)
AC_SUBST(PNG_LIBS)



dnl Test for LADSPA
AC_ARG_WITH(ladspa,
    AC_HELP_STRING([--without-ladspa], [Don't try to use LADSPA]))
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
    AC_HELP_STRING([--with-ladspa-path], [Default search path for LADSPA plugins]))
AC_SUBST(LADSPA_PATH)



dnl Check for MAD libraries
AC_ARG_WITH(mad,
    AC_HELP_STRING([--without-mad],
        [Don't try to use MAD (MP3 Audio Decoder)]))
using_mad=no
if test "$with_mad" != "no"; then
    using_mad=yes
    AC_CHECK_HEADERS(mad.h, [AC_CHECK_LIB(mad, mad_stream_buffer, MP3_LIBS="$MP3_LIBS -lmad",using_mad=no)], using_mad=no)
    if test "$with_mad" = "yes" -a "$using_mad" = "no"; then
        AC_MSG_FAILURE([cannot find libmad])
    fi
fi



dnl Check for id3tag libraries
AC_ARG_WITH(id3tag,
    AC_HELP_STRING([--without-id3tag],
        [Don't try to use id3tag]))
using_id3tag=no
if test "$with_id3tag" != "no"; then
    using_id3tag=yes
    AC_CHECK_HEADER(id3tag.h,
        [AC_CHECK_LIB(id3tag, id3_file_open, MP3_LIBS="$MP3_LIBS -lid3tag -lz",using_id3tag=no, -lz)],
        using_id3tag=no)
    if test "$with_id3tag" = "yes" -a "$using_id3tag" = "no"; then
        AC_MSG_FAILURE([cannot find id3tag])
    fi
fi
if test "$using_id3tag" = yes; then
   AC_DEFINE(HAVE_ID3TAG, 1, [Define to 1 if you have id3tag.])
fi
AM_CONDITIONAL(HAVE_ID3TAG, test x$using_id3tag = xyes)



dnl Check for LAME library.
AC_ARG_WITH(lame,
    AC_HELP_STRING([--without-lame],
        [Don't try to use LAME (LAME Ain't an MP3 Encoder)]))
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



dnl Check for FLAC libraries
dnl  Note passing in OGG_VORBIS_LIBS.  That is because FLAC has optional
dnl  support for OGG and if OGG libraries are found on this
dnl  system then its highly likely to be compiled into FLAC
dnl  and will require these at link time.
case $target in
  *mingw*)
  FLAC_LIBS="-lwsock32"
  ;;
esac
AC_OPTIONAL_FORMAT(flac, FLAC, FLAC/all.h, FLAC, FLAC__stream_encoder_new, "-lFLAC $FLAC_LIBS $OGG_VORBIS_LIBS", $FLAC_LIBS $OGG_VORBIS_LIBS, yes)



AC_OPTIONAL_FORMAT(amrwb, AMRWB, amrwb/dec.h, amrwb, D_IF_init, "$AMR_WB_LIBS -lamrwb", [$AMR_WB_LIBS -lamrwb])



AC_OPTIONAL_FORMAT(amrnb, AMRNB, amrnb/sp_dec.h, amrnb, Decoder_Interface_init, "$AMR_WB_LIBS -lamrnb", [$AMR_WB_LIBS -lamrnb])



AC_OPTIONAL_FORMAT(wavpack, WAVPACK, wavpack/wavpack.h, wavpack, WavpackGetSampleRate, "$WAVPACK_LIBS -lwavpack", [$WAVPACK_LIBS -lwavpack])



AC_OPTIONAL_FORMAT(sndio, SNDIO, sndio.h, sndio, sio_open, "$SNDIO_LIBS -lsndio", [$SNDIO_LIBS -lsndio],yes)



AC_OPTIONAL_FORMAT(coreaudio, COREAUDIO, CoreAudio/CoreAudio.h, CoreAudio, AudioHardwareGetProperty, [$COREAUDIO_LIBS -Wl,-framework,CoreAudio], [$COREAUDIO_LIBS -Wl,-framework,CoreAudio],yes)



AC_OPTIONAL_FORMAT(alsa, ALSA, alsa/asoundlib.h, asound, snd_pcm_open, "$ALSA_LIBS -lasound", [$ALSA_LIBS -lasound],yes)



AC_OPTIONAL_FORMAT(ao, AO, ao/ao.h, ao, ao_play, "$AO_LIBS -lao", [$AO_LIBS -lao])



AC_OPTIONAL_FORMAT(pulseaudio, PULSEAUDIO, pulse/simple.h, pulse, pa_simple_new, "$PULSEAUDIO_LIBS -lpulse -lpulse-simple", [$PULSEAUDIO_LIBS -lpulse -lpulse-simple])



dnl Check for Ogg Vorbis libraries
AC_ARG_WITH(oggvorbis, AC_HELP_STRING([--with-oggvorbis=dyn], [load oggvorbis dynamically]))
using_oggvorbis=$with_oggvorbis
if test "_$with_oggvorbis" = _dyn; then
  if test $using_libltdl != yes; then
    AC_MSG_FAILURE([not using libltdl; cannot load oggvorbis dynamically])
  fi
elif test "_$with_oggvorbis" = _; then
  using_oggvorbis=yes
elif test "_$with_oggvorbis" != _yes -a "_$with_oggvorbis" != _no; then
  AC_MSG_FAILURE([invalid selection --with-oggvorbis=$with_oggvorbis])
fi
if test _$with_oggvorbis != _no; then
  AC_CHECK_HEADER(vorbis/codec.h,
      [AC_CHECK_LIB(ogg, ogg_packet_clear, OGG_VORBIS_LIBS="$OGG_VORBIS_LIBS -logg", using_oggvorbis=no)
      AC_CHECK_LIB(vorbis, vorbis_analysis_headerout, OGG_VORBIS_LIBS="-lvorbis $OGG_VORBIS_LIBS", using_oggvorbis=no, $OGG_VORBIS_LIBS)
      AC_CHECK_LIB(vorbisfile, ov_clear, OGG_VORBIS_LIBS="-lvorbisfile $OGG_VORBIS_LIBS", using_oggvorbis=no, $OGG_VORBIS_LIBS)
      AC_CHECK_LIB(vorbisenc, vorbis_encode_init_vbr, OGG_VORBIS_LIBS="-lvorbisenc $OGG_VORBIS_LIBS", using_oggvorbis=no, $OGG_VORBIS_LIBS)],
      using_oggvorbis=no)
  if test _$with_oggvorbis != _ -a $using_oggvorbis = no; then
    AC_MSG_FAILURE([cannot find oggvorbis])
  fi
fi
if test "$using_oggvorbis" != no; then
  AC_DEFINE(HAVE_OGG_VORBIS, 1, [Define to 1 if you have oggvorbis.])
  if test "$using_oggvorbis" = yes; then
    AC_DEFINE(STATIC_OGG_VORBIS, 1, [Define to 1 if you have static oggvorbis.])
  fi
fi
AM_CONDITIONAL(HAVE_OGG_VORBIS, test $using_oggvorbis != no)
AM_CONDITIONAL(STATIC_OGG_VORBIS, test $using_oggvorbis = yes)
AC_SUBST(OGG_VORBIS_LIBS)



dnl Check for libsndfile
AC_ARG_WITH(sndfile, AC_HELP_STRING([--with-sndfile=dyn], [load sndfile dynamically]))
using_sndfile=$with_sndfile
if test "_$with_sndfile" = _dyn; then
  if test $using_libltdl != yes; then
    AC_MSG_FAILURE([not using libltdl; cannot load sndfile dynamically])
  fi
elif test "_$with_sndfile" = _; then
  using_sndfile=yes
elif test "_$with_sndfile" != _yes -a "_$with_sndfile" != _no; then
  AC_MSG_FAILURE([invalid selection --with-sndfile=$with_sndfile])
fi
if test _$with_sndfile != _no; then
  SOX_PATH_SNDFILE(, using_sndfile=no)
  if test _$with_sndfile != _ -a $using_sndfile = no; then
    AC_MSG_FAILURE([cannot find sndfile])
  fi
fi
if test "$using_sndfile" != no; then
  AC_DEFINE(HAVE_SNDFILE, 1, [Define to 1 if you have sndfile.])
  if test "$using_sndfile" = yes; then
    AC_DEFINE(STATIC_SNDFILE, 1, [Define to 1 if you have static sndfile.])
  fi
fi
AM_CONDITIONAL(HAVE_SNDFILE, test $using_sndfile != no)
AM_CONDITIONAL(STATIC_SNDFILE, test $using_sndfile = yes)
AC_SUBST(SNDFILE_LIBS)
AC_SUBST(SNDFILE_CFLAGS)



dnl Check for libffmpeg
AC_ARG_WITH(ffmpeg, AC_HELP_STRING([--with-ffmpeg=dyn], [load ffmpeg dynamically]))
using_ffmpeg=$with_ffmpeg
if test "_$with_ffmpeg" = _dyn; then
  if test $using_libltdl != yes; then
    AC_MSG_FAILURE([not using libltdl; cannot load ffmpeg dynamically])
  fi
elif test "_$with_ffmpeg" = _; then
  using_ffmpeg=yes
elif test "_$with_ffmpeg" != _yes -a "_$with_ffmpeg" != _no; then
  AC_MSG_FAILURE([invalid selection --with-ffmpeg=$with_ffmpeg])
fi
if test _$with_ffmpeg != _no; then
  SOX_PATH_FFMPEG(, using_ffmpeg=no)
  if test _$with_ffmpeg != _ -a $using_ffmpeg = no; then
    AC_MSG_FAILURE([cannot find ffmpeg])
  fi
fi
if test "$using_ffmpeg" != no; then
  AC_DEFINE(HAVE_FFMPEG, 1, [Define to 1 if you have ffmpeg.])
  if test "$using_ffmpeg" = yes; then
    AC_DEFINE(STATIC_FFMPEG, 1, [Define to 1 if you have static ffmpeg.])
  fi
fi
AM_CONDITIONAL(HAVE_FFMPEG, test $using_ffmpeg != no)
AM_CONDITIONAL(STATIC_FFMPEG, test $using_ffmpeg = yes)
AC_SUBST(FFMPEG_LIBS)



dnl Check for OSS
AC_ARG_WITH(oss, AC_HELP_STRING([--with-oss=dyn], [load oss dynamically]))
using_oss=$with_oss
if test "_$with_oss" = _dyn; then
  if test $using_libltdl != yes; then
    AC_MSG_FAILURE([not using libltdl; cannot load oss dynamically])
  fi
elif test "_$with_oss" = _; then
  using_oss=yes
elif test "_$with_oss" != _yes -a "_$with_oss" != _no; then
  AC_MSG_FAILURE([invalid selection --with-oss=$with_oss])
fi
if test _$with_oss != _no; then
  AC_CHECK_HEADERS(sys/soundcard.h,,
      [AC_CHECK_HEADERS(machine/soundcard.h,
	  [AC_CHECK_LIB(ossaudio, _oss_ioctl, OSS_LIBS="$OSS_LIBS -lossaudio")],
	  enable_oss=no)])
  if test _$with_oss != _ -a $using_oss = no; then
    AC_MSG_FAILURE([cannot find oss])
  fi
fi
if test "$using_oss" != no; then
  AC_DEFINE(HAVE_OSS, 1, [Define to 1 if you have oss.])
  if test "$using_oss" = yes; then
    AC_DEFINE(STATIC_OSS, 1, [Define to 1 if you have static oss.])
  fi
fi
AM_CONDITIONAL(HAVE_OSS, test $using_oss != no)
AM_CONDITIONAL(STATIC_OSS, test $using_oss = yes)
AC_SUBST(OSS_LIBS)



dnl Check for Sun audio
AC_ARG_WITH(sunaudio, AC_HELP_STRING([--with-sunaudio=dyn], [load sunaudio dynamically]))
using_sunaudio=$with_sunaudio
if test "_$with_sunaudio" = _dyn; then
  if test $using_libltdl != yes; then
    AC_MSG_FAILURE([not using libltdl; cannot load sunaudio dynamically])
  fi
elif test "_$with_sunaudio" = _; then
  using_sunaudio=yes
elif test "_$with_sunaudio" != _yes -a "_$with_sunaudio" != _no; then
  AC_MSG_FAILURE([invalid selection --with-sunaudio=$with_sunaudio])
fi
if test _$with_sunaudio != _no; then
  AC_CHECK_HEADERS(sys/audioio.h,,
      [AC_CHECK_HEADERS(sun/audioio.h,, using_sunaudio=no)])
  if test _$with_sunaudio != _ -a $using_sunaudio = no; then
    AC_MSG_FAILURE([cannot find sunaudio])
  fi
fi
if test "$using_sunaudio" != no; then
  AC_DEFINE(HAVE_SUN_AUDIO, 1, [Define to 1 if you have sunaudio.])
  if test "$using_sunaudio" = yes; then
    AC_DEFINE(STATIC_SUN_AUDIO, 1, [Define to 1 if you have static sunaudio.])
  fi
fi
AM_CONDITIONAL(HAVE_SUN_AUDIO, test $using_sunaudio != no)
AM_CONDITIONAL(STATIC_SUN_AUDIO, test $using_sunaudio = yes)
AC_SUBST(SUN_AUDIO_LIBS)



dnl MP3 format depends on libmad || LAME
AC_ARG_WITH(mp3, AC_HELP_STRING([--with-mp3=dyn], [load mp3 dynamically]))
using_mp3=$with_mp3
if test "_$with_mp3" = _dyn; then
  if test $using_libltdl != yes; then
    AC_MSG_FAILURE([not using libltdl; cannot load mp3 dynamically])
  fi
elif test "_$with_mp3" = _; then
  using_mp3=yes
elif test "_$with_mp3" != _yes -a "_$with_mp3" != _no; then
  AC_MSG_FAILURE([invalid selection --with-mp3=$with_mp3])
fi
if test _$with_mp3 != _no; then
  if test "$using_mad" != yes -a "$using_lame" != yes; then
    using_mp3=no
  fi
  if test _$with_mp3 != _ -a $using_mp3 = no; then
    AC_MSG_FAILURE([cannot find mp3])
  fi
fi
if test "$using_mp3" != no; then
  AC_DEFINE(HAVE_MP3, 1, [Define to 1 if you have mp3.])
  if test "$using_mp3" = yes; then
    AC_DEFINE(STATIC_MP3, 1, [Define to 1 if you have static mp3.])
  fi
fi
AM_CONDITIONAL(HAVE_MP3, test $using_mp3 != no)
AM_CONDITIONAL(STATIC_MP3, test $using_mp3 = yes)
AC_SUBST(MP3_LIBS)



dnl Check for libgsm
found_libgsm=yes
AC_CHECK_HEADERS(gsm/gsm.h, ,
    [AC_CHECK_HEADERS(gsm.h, ,found_libgsm=no)])
    AC_CHECK_LIB(gsm, gsm_create, GSM_LIBS="$GSM_LIBS -lgsm", found_libgsm=no)
if test "$found_libgsm" = yes; then
    AC_DEFINE(EXTERNAL_GSM, 1, [Define if you are using an external GSM library])
else
    LIBGSM_LIBADD=../libgsm/libgsm.la
fi
AM_CONDITIONAL(EXTERNAL_GSM, test x$found_libgsm = xyes)
AC_SUBST(LIBGSM_LIBADD)
AC_SUBST(GSM_LIBS)



dnl GSM format depends on libgsm
AC_ARG_WITH(gsm, AC_HELP_STRING([--with-gsm=dyn], [load gsm dynamically]))
using_gsm=$with_gsm
if test "_$with_gsm" = _dyn; then
  if test $using_libltdl != yes; then
    AC_MSG_FAILURE([not using libltdl; cannot load gsm dynamically])
  fi
elif test "_$with_gsm" = _; then
  using_gsm=yes
elif test "_$with_gsm" != _yes -a "_$with_gsm" != _no; then
  AC_MSG_FAILURE([invalid selection --with-gsm=$with_gsm])
fi
# No need to check; GSM is always found
if test "$using_gsm" != no; then
  AC_DEFINE(HAVE_GSM, 1, [Define to 1 if you have gsm.])
  if test "$using_gsm" = yes; then
    AC_DEFINE(STATIC_GSM, 1, [Define to 1 if you have static gsm.])
  fi
fi
AM_CONDITIONAL(HAVE_GSM, test $using_gsm != no)
AM_CONDITIONAL(STATIC_GSM, test $using_gsm = yes)



dnl Check for liblpc10
found_liblpc10=yes
AC_CHECK_HEADERS(lpc10.h, ,
    [AC_CHECK_HEADERS(lpc10.h, ,found_liblpc10=no)])
    AC_CHECK_LIB(lpc10, create_lpc10_encoder_state, LPC10_LIBS="$LPC10_LIBS -llpc10", found_liblpc10=no)
if test "$found_liblpc10" = yes; then
    AC_DEFINE(EXTERNAL_LPC10, 1, [Define if you are using an external LPC10 library])
else
    LIBLPC10_LIBADD=../lpc10/liblpc10.la
fi
AM_CONDITIONAL(EXTERNAL_LPC10, test x$found_liblpc10 = xyes)
AC_SUBST(LIBLPC10_LIBADD)
AC_SUBST(LPC10_LIBS)



dnl LPC10 format
AC_ARG_WITH(lpc10, AC_HELP_STRING([--with-lpc10=dyn], [load lpc10 dynamically]))
using_lpc10=$with_lpc10
if test "_$with_lpc10" = _dyn; then
  if test $using_libltdl != yes; then
    AC_MSG_FAILURE([not using libltdl; cannot load lpc10 dynamically])
  fi
elif test "_$with_lpc10" = _; then
  using_lpc10=yes
elif test "_$with_lpc10" != _yes -a "_$with_lpc10" != _no; then
  AC_MSG_FAILURE([invalid selection --with-lpc10=$with_lpc10])
fi
# No need to check; LPC10 is always found
if test "$using_lpc10" != no; then
  AC_DEFINE(HAVE_LPC10, 1, [Define to 1 if you have lpc10.])
  if test "$using_lpc10" = yes; then
    AC_DEFINE(STATIC_LPC10, 1, [Define to 1 if you have static lpc10.])
  fi
fi
AM_CONDITIONAL(HAVE_LPC10, test $using_lpc10 != no)
AM_CONDITIONAL(STATIC_LPC10, test $using_lpc10 = yes)



dnl Check if we want to disable all symlinks
AC_MSG_CHECKING([whether to enable symlinks])
AC_ARG_ENABLE(symlinks,
    AC_HELP_STRING([--disable-symlinks], [Don't make any symlinks to sox.]),,enable_symlinks=yes)
AC_MSG_RESULT($enable_symlinks)

enable_playrec_symlinks=no
if test "$enable_symlinks" = "yes"; then
  SYMLINKS=yes
  if test false \
      -o "$enable_alsa" = yes \
      -o "$enable_ao" = yes \
      -o "$enable_coreaudio" = yes \
      -o "$enable_oss" = yes \
      -o "$enable_pulseaudio" = yes \
      -o "$enable_sndio" = yes \
    ; then
    PLAYRECLINKS=yes
    enable_playrec_symlinks=yes
  fi
fi
AC_SUBST(SYMLINKS)
AC_SUBST(PLAYRECLINKS)



dnl Generate output files.
AX_CREATE_STDINT_H(src/soxstdint.h)
AC_CONFIG_FILES(Makefile src/Makefile libgsm/Makefile lpc10/Makefile sox.pc)
AC_OUTPUT

if test "$using_gsm" != "no"; then
  if test "$found_libgsm" = "yes"; then
    gsm_option="(external)"
  else
    gsm_option="(in-tree)"
  fi
fi

if test "$using_lpc10" != "no"; then
  if test "$found_liblpc10" = "yes"; then
    lpc10_option="(external)"
  else
    lpc10_option="(in-tree)"
  fi
fi

if test "$using_pkgconfig" = "no"; then
  pkgconfig_option="disabled"
else
  pkgconfig_option="$with_pkgconfigdir"
fi



# Report configuration.
echo
echo "BUILD OPTIONS"
echo "Debugging build............$enable_debug"
echo "Distro name ...............$DISTRO"
echo "Dynamic loading support....$using_libltdl"
echo "GOMP support...............$enable_gomp"
echo "Pkg-config location........$pkgconfig_option"
echo "Play and rec symlinks......$enable_playrec_symlinks"
echo "Symlinks enabled...........$enable_symlinks"
echo
echo "OPTIONAL DEVICE DRIVERS"
echo "ao (Xiph)..................$using_ao"
echo "alsa (Linux)...............$using_alsa"
echo "coreaudio (MAC OS X).......$using_coreaudio"
echo "sndio (OpenBSD)............$using_sndio"
echo "oss........................$using_oss"
echo "pulseaudio.................$using_pulseaudio"
echo "sunaudio...................$using_sunaudio"
echo
echo "OPTIONAL FILE FORMATS"
echo "amrnb......................$using_amrnb"
echo "amrwb......................$using_amrwb"
echo "ffmpeg.....................$using_ffmpeg"
echo "flac.......................$using_flac"
echo "gsm........................$using_gsm $gsm_option"
echo "lpc10......................$using_lpc10 $lpc10_option"
echo "mp3........................$using_mp3$mp3_option"
echo " id3tag....................$using_id3tag"
echo " lame......................$using_lame"
echo " mad.......................$using_mad"
echo "oggvorbis..................$using_oggvorbis"
echo "sndfile....................$using_sndfile"
echo "wavpack....................$using_wavpack"
echo
echo "MISC OPTIONS"
echo "ladspa effects.............$using_ladspa"
echo "magic support..............$using_magic"
echo "png support................$using_png"
echo
echo "Configure finished.  Do 'make && make install' to compile and install SoX."
echo
