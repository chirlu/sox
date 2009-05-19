dnl Process this file with autoconf to produce a configure script.

AC_INIT(SoX, 14.3.0, sox-devel@lists.sourceforge.net)

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
        [Don't try to use libltdl for external dynamic library support]),
    with_libltdl=$withval, with_libltdl=default)

if test "$with_libltdl" = "default"; then
    dnl Default to no libltdl support when building only static libraries
    if test "$enable_shared" != "no"; then
        using_libltdl=yes
    else
        using_libltdl=no
    fi
    with_libltdl="yes"
else
    using_libltdl=$with_libltdl
fi

if test "$with_libltdl" != "no"; then
    dnl Force off using libltdl on targets that are know to have
    dnl problems.
    case $target in
      *mingw32* | *os2*)
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

AC_ARG_WITH(dyn-default,AC_HELP_STRING([--with-dyn-default],[Default to loading optional formats dynamically]),opt_default=dyn,opt_default=yes)

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
    AC_CHECK_HEADERS(mad.h,, using_mad=no)
    AC_MSG_CHECKING([whether to dlopen mad])
    AC_ARG_ENABLE(dl_mad,
      AC_HELP_STRING([--enable-dl-mad], [Dlopen mad instead of linking in.]),
      enable_dl_mad=$enableval, enable_dl_mad=no)
    AC_MSG_RESULT($enable_dl_mad)
    if test "x$using_libltdl" = "xyes" -a "x$enable_dl_mad" = "xyes"; then
      AC_DEFINE(DL_MAD, 1, [Define if to dlopen() mad.])
    else
      AC_CHECK_LIB(mad, mad_stream_buffer, MP3_LIBS="$MP3_LIBS -lmad",using_mad=no)
      if test "$with_mad" = "yes" -a "$using_mad" = "no"; then
        AC_MSG_FAILURE([cannot find libmad])
      fi
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
    AC_MSG_CHECKING([whether to dlopen lame])
    AC_ARG_ENABLE(dl_lame,
      AC_HELP_STRING([--enable-dl-lame], [Dlopen lame instead of linking in.]),
      enable_dl_lame=$enableval, enable_dl_lame=no)
    AC_MSG_RESULT($enable_dl_lame)
    if test "x$using_libltdl" = "xyes" -a "x$enable_dl_lame" = "xyes"; then
      AC_DEFINE(DL_LAME, 1, [Define if to dlopen() lame.])
    else
      AC_CHECK_LIB(mp3lame, lame_init, MP3_LIBS="$MP3_LIBS -lmp3lame",using_lame=no)
      if test "$with_lame" = "yes" -a "$using_lame" = "no"; then
        AC_MSG_FAILURE([cannot find LAME])
      fi
    fi
fi

# Check for libgsm
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



# Check for liblpc10
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



# Check for Ogg Vorbis
AC_OPTIONAL_FORMAT(oggvorbis, OGG_VORBIS, [AC_CHECK_HEADER(vorbis/codec.h,
  [AC_CHECK_LIB(ogg, ogg_packet_clear, OGG_VORBIS_LIBS="$OGG_VORBIS_LIBS -logg", using_oggvorbis=no)
  AC_CHECK_LIB(vorbis, vorbis_analysis_headerout, OGG_VORBIS_LIBS="-lvorbis $OGG_VORBIS_LIBS", using_oggvorbis=no, $OGG_VORBIS_LIBS)
  AC_CHECK_LIB(vorbisfile, ov_clear, OGG_VORBIS_LIBS="-lvorbisfile $OGG_VORBIS_LIBS", using_oggvorbis=no, $OGG_VORBIS_LIBS)
  AC_CHECK_LIB(vorbisenc, vorbis_encode_init_vbr, OGG_VORBIS_LIBS="-lvorbisenc $OGG_VORBIS_LIBS", using_oggvorbis=no, $OGG_VORBIS_LIBS)],
  using_oggvorbis=no)])



# Check for FLAC libraries
#  Note passing in OGG_VORBIS_LIBS.  That is because FLAC has optional
#  support for OGG and if OGG libraries are found on this
#  system then its highly likely to be compiled into FLAC
#  and will require these at link time.
case $target in
  *mingw*)
  FLAC_LIBS="-lwsock32"
  ;;
esac
AC_OPTIONAL_FORMAT(flac, FLAC, [AC_CHECK_HEADER(FLAC/all.h, [AC_CHECK_LIB(FLAC, FLAC__stream_encoder_new, FLAC_LIBS="-lFLAC $FLAC_LIBS $OGG_VORBIS_LIBS",using_flac=no, $FLAC_LIBS $OGG_VORBIS_LIBS)], using_flac=no)])



AC_OPTIONAL_FORMAT(amrwb, AMRWB, [AC_CHECK_HEADER(amrwb/dec.h, [AC_CHECK_LIB(amrwb, D_IF_init, AMRWB_LIBS="$AMR_WB_LIBS -lamrwb",using_amrwb=no)], using_amrwb=no)])



AC_OPTIONAL_FORMAT(amrnb, AMRNB, [AC_CHECK_HEADER(amrnb/sp_dec.h, [AC_CHECK_LIB(amrnb, Decoder_Interface_init, AMRNB_LIBS="$AMR_WB_LIBS -lamrnb",using_amrnb=no)], using_amrnb=no)])



AC_OPTIONAL_FORMAT(wavpack, WAVPACK, [AC_CHECK_HEADER(wavpack/wavpack.h, [AC_CHECK_LIB(wavpack, WavpackGetSampleRate, WAVPACK_LIBS="$WAVPACK_LIBS -lwavpack",using_wavpack=no)], using_wavpack=no)])



AC_OPTIONAL_FORMAT(sndio, SNDIO, [AC_CHECK_HEADER(sndio.h, [AC_CHECK_LIB(sndio, sio_open, SNDIO_LIBS="$SNDIO_LIBS -lsndio",using_sndio=no)], using_sndio=no)])



AC_OPTIONAL_FORMAT(coreaudio, COREAUDIO, [AC_CHECK_HEADER(CoreAudio/CoreAudio.h, [COREAUDIO_LIBS="$COREAUDIO_LIBS -Wl,-framework,CoreAudio"], using_coreaudio=no)])



AC_OPTIONAL_FORMAT(alsa, ALSA, [AC_CHECK_HEADER(alsa/asoundlib.h, [AC_CHECK_LIB(asound, snd_pcm_open, ALSA_LIBS="$ALSA_LIBS -lasound",using_alsa=no)], using_alsa=no)])



AC_OPTIONAL_FORMAT(ao, AO, [AC_CHECK_HEADER(ao/ao.h, [AC_CHECK_LIB(ao, ao_play, AO_LIBS="$AO_LIBS -lao",using_ao=no)], using_ao=no)])



AC_OPTIONAL_FORMAT(pulseaudio, PULSEAUDIO, [AC_CHECK_HEADER(pulse/simple.h, [AC_CHECK_LIB(pulse, pa_simple_new, PULSEAUDIO_LIBS="$PULSEAUDIO_LIBS -lpulse -lpulse-simple",using_pulseaudio=no,"-lpulse-simple")], using_pulseaudio=no)])



AC_OPTIONAL_FORMAT(sndfile, SNDFILE, [SOX_PATH_SNDFILE(, using_sndfile=no)])
AC_SUBST(SNDFILE_CFLAGS)



AC_OPTIONAL_FORMAT(ffmpeg, FFMPEG, [SOX_PATH_FFMPEG(, using_ffmpeg=no)])



AC_OPTIONAL_FORMAT(oss, OSS, [AC_CHECK_HEADERS(sys/soundcard.h,,
      [AC_CHECK_HEADERS(machine/soundcard.h,
	  [AC_CHECK_LIB(ossaudio, _oss_ioctl, OSS_LIBS="$OSS_LIBS -lossaudio")],
	  using_oss=no)])])



AC_OPTIONAL_FORMAT(sunaudio, SUN_AUDIO, [AC_CHECK_HEADERS(sys/audioio.h,,
      [AC_CHECK_HEADERS(sun/audioio.h,, using_sunaudio=no)])])



# MP3 format depends on libmad || LAME
AC_OPTIONAL_FORMAT(mp3, MP3, [
  if test "$using_mad" != yes -a "$using_lame" != yes; then
    using_mp3=no
  fi])



# GSM format depends on libgsm
# No need to check; GSM is always found
AC_OPTIONAL_FORMAT(gsm, GSM)



# LPC10 format depends on liblpc10
# No need to check; LPC10 is always found
AC_OPTIONAL_FORMAT(lpc10, LPC10)



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
echo "Pkg-config location........$pkgconfig_option"
echo "Play and rec symlinks......$enable_playrec_symlinks"
echo "Symlinks enabled...........$enable_symlinks"
echo
echo "OPTIONAL DEVICE DRIVERS"
echo "ao (Xiph)..................$using_ao"
echo "alsa (Linux)...............$using_alsa"
echo "coreaudio (Mac OS X).......$using_coreaudio"
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
echo "mp3........................$using_mp3"
echo " id3tag....................$using_id3tag"
echo " lame......................$using_lame"
if test "x$using_lame" = "xyes"; then
echo " dlopen lame...............$enable_dl_lame"
fi
echo " mad.......................$using_mad"
if test "x$using_mad" = "xyes"; then
echo " dlopen mad................$enable_dl_mad"
fi
echo "oggvorbis..................$using_oggvorbis"
echo "sndfile....................$using_sndfile"
echo "wavpack....................$using_wavpack"
echo
echo "OTHER OPTIONS"
echo "ladspa effects.............$using_ladspa"
echo "magic support..............$using_magic"
echo "png support................$using_png"
echo "GOMP support...............$enable_gomp"
echo
echo "Configure finished.  Do 'make -s && make install' to compile and install SoX."
echo
