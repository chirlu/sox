dnl Process this file with autoconf to produce a configure script.

AC_INIT(SoX, 14.2.0, sox-devel@lists.sourceforge.net)

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

# Increase version when binary compatibility with previous version is broken
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
      *cygwin* | *os2*)
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

dnl Get distro name from /etc/issue
if test -r /etc/issue; then
  AC_MSG_CHECKING([distro name])
  DISTRO=`sed -e "s/.[0-9][0-9;]*[mJH]//g" < /etc/issue|tr "\n" " " | sed -e "s/(\\\\?\\\\\\\\.*//" -e "s/  */ /g" -e "s/^ //" -e "s/ $//"`
  AC_MSG_RESULT("$DISTRO")
  AC_SUBST(DISTRO)
  AC_DEFINE(HAVE_DISTRO, 1, [1 if DISTRO is defined])
  have_distro=yes
fi
AM_CONDITIONAL(HAVE_DISTRO, test x$have_distro = xyes)

dnl Check for system dependent features.
AC_C_BIGENDIAN
AC_C_INLINE

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h unistd.h byteswap.h sys/time.h sys/timeb.h sys/types.h sys/utsname.h)

dnl Checks for library functions.
AC_CHECK_FUNCS(strcasecmp strdup popen vsnprintf gettimeofday)

dnl Check if math library is needed.
AC_CHECK_FUNC(pow)
if test "$ac_cv_func_pow" = no; then
  AC_CHECK_LIB(m, pow)
fi

dnl Large File Support
AC_SYS_LARGEFILE
AC_FUNC_FSEEKO
if test "$ac_cv_sys_file_offset_bits" = "64"; then
  # This is either a 64-bit platform or a 32-bit platform
  # that supports large files.
  # There are to many broken glibc+kernel's out there that
  # detect 64-bit off_t on 32-bit system and require
  # _LARGEFILE_SOURCE to make fseeko() go into 64-bit mode.
  # Force defining _LARGEFILE_SOURCE always.  My assumption
  # is its harmlessly ignored on 64-bit platforms and platforms
  # that chose other solutions.
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
   
audio_driver_found=no  
    
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
   audio_driver_found=yes
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
   audio_driver_found=yes
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
   audio_driver_found=yes
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
   audio_driver_found=yes
fi
AM_CONDITIONAL(HAVE_SUN_AUDIO, test x$enable_sun_audio = xyes)

dnl Check for Mac OSX CoreAudio
AC_MSG_CHECKING([whether to try building CoreAudio driver])
AC_ARG_ENABLE(coreaudio,
    AC_HELP_STRING([--disable-coreaudio], [Don't build CoreAudio driver.]),,enable_coreaudio=yes)
AC_MSG_RESULT($enable_coreaudio)
if test "$enable_coreaudio" = "yes"; then
    AC_CHECK_HEADERS(CoreAudio/CoreAudio.h, [COREAUDIO_LIBS="$COREAUDIO_LIBS -Wl,-framework,CoreAudio"], enable_coreaudio=no)
fi
if test "$enable_coreaudio" = yes; then
   AC_DEFINE(HAVE_COREAUDIO, 1, [Define to 1 if you have CoreAudio.])
   audio_driver_found=yes
fi
AM_CONDITIONAL(HAVE_COREAUDIO, test x$enable_coreaudio = xyes)
AC_SUBST(COREAUDIO_LIBS)

dnl Check if we want to disable all symlinks
AC_MSG_CHECKING([whether to enable symlinks])
AC_ARG_ENABLE(symlinks,
    AC_HELP_STRING([--disable-symlinks], [Don't make any symlinks to sox.]),,enable_symlinks=yes)
AC_MSG_RESULT($enable_symlinks)

enable_playrec_symlinks=no
if test "$enable_symlinks" = "yes"; then
    SYMLINKS=yes
    if test "$audio_driver_found" = "yes"; then
        PLAYRECLINKS=yes
        enable_playrec_symlinks=yes
    fi
fi
AC_SUBST(SYMLINKS)
AC_SUBST(PLAYRECLINKS)
 
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
AC_SUBST(LIBGSM_LIBADD)
AC_SUBST(GSM_LIBS)
AM_CONDITIONAL(EXTERNAL_GSM, test x$found_libgsm = xyes)

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
AC_SUBST(LIBLPC10_LIBADD)
AC_SUBST(LPC10_LIBS)
AM_CONDITIONAL(EXTERNAL_LPC10, test x$found_liblpc10 = xyes)

dnl Check for libsndfile
AC_ARG_WITH(sndfile,
    AC_HELP_STRING([--without-sndfile],
    [Don't try to use libsndfile]))
using_sndfile=no
if test "$with_sndfile" != "no"; then
    using_sndfile=yes
    SOX_PATH_SNDFILE(, using_sndfile=no)
    if test "$with_sndfile" = "yes" -a "$using_sndfile" = "no"; then
        AC_MSG_FAILURE([cannot find libsndfile])
    fi
fi
if test "$using_sndfile" = yes; then
   AC_DEFINE(HAVE_SNDFILE, 1, [Define to 1 if you have libsndfile.])
fi
AM_CONDITIONAL(HAVE_SNDFILE, test x$using_sndfile = xyes)
AC_SUBST(SNDFILE_CFLAGS)
AC_SUBST(SNDFILE_LIBS)
        
dnl Check for Ogg Vorbis libraries
AC_ARG_WITH(ogg,
    AC_HELP_STRING([--without-ogg], 
	[Don't try to use Ogg Vorbis]))
using_ogg=no
if test "$with_ogg" != "no"; then
    using_ogg=yes
    AC_CHECK_HEADER(vorbis/codec.h,
        [AC_CHECK_LIB(ogg, ogg_packet_clear, OGG_LIBS="$OGG_LIBS -logg", using_ogg=no)
        AC_CHECK_LIB(vorbis, vorbis_analysis_headerout, OGG_LIBS="-lvorbis $OGG_LIBS", using_ogg=no, $OGG_LIBS)
        AC_CHECK_LIB(vorbisfile, ov_clear, OGG_LIBS="-lvorbisfile $OGG_LIBS", using_ogg=no, $OGG_LIBS)
        AC_CHECK_LIB(vorbisenc, vorbis_encode_init_vbr, OGG_LIBS="-lvorbisenc $OGG_LIBS", using_ogg=no, $OGG_LIBS)],
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
case $target in
  *mingw*)
  FLAC_LIBS="-lwsock32"
  ;;
esac

AC_ARG_WITH(flac,
    AC_HELP_STRING([--without-flac],
        [Don't try to use FLAC]))
using_flac=no
if test "$with_flac" != "no"; then
    using_flac=yes
    dnl Note passing in OGG_LIBS.  That is because FLAC has optional
    dnl support for OGG and if OGG libraries are found ont this
    dnl system then its highly likely to be compiled into FLAC
    dnl and will require these at link time.
    AC_CHECK_HEADER(FLAC/all.h,
        [AC_CHECK_LIB(FLAC, FLAC__stream_encoder_new, FLAC_LIBS="-lFLAC $FLAC_LIBS $OGG_LIBS",using_flac=no, $FLAC_LIBS $OGG_LIBS)],
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
        [Don't try to use ffmpeg]))
using_ffmpeg=no
if test "$with_ffmpeg" != "no"; then
    using_ffmpeg=yes
    SOX_PATH_FFMPEG(, using_ffmpeg=no)
    if test "$with_ffmpeg" = "yes" -a "$using_ffmpeg" = "no"; then
        AC_MSG_FAILURE([cannot find ffmpeg])
    fi
fi
if test "$using_ffmpeg" = yes; then
   AC_DEFINE(HAVE_FFMPEG, 1, [Define to 1 if you have ffmpeg.])
fi
AM_CONDITIONAL(HAVE_FFMPEG, test x$using_ffmpeg = xyes)
AC_SUBST(FFMPEG_LIBS)

dnl Check for magic libraries
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

dnl Check for MAD libraries
AC_ARG_WITH(mad,
    AC_HELP_STRING([--without-mad],
        [Don't try to use MAD (MP3 Audio Decoder)]))
using_mad=no
if test "$with_mad" != "no"; then
    using_mad=yes
    AC_CHECK_HEADERS(mad.h,
        [AC_CHECK_LIB(mad, mad_stream_buffer, MP3_LIBS="$MP3_LIBS -lmad",using_mad=no)],
        using_mad=no)
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

dnl Test for LAME library.
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

dnl MP3 library depends on libmad || LAME
if test "$using_mad" = yes -o "$using_lame" = yes; then
   AC_DEFINE(HAVE_MP3, 1, [Define to 1 if you have lame or mad support.])
fi
AM_CONDITIONAL(HAVE_MP3, test "x$using_mad" = "xyes" -o "x$using_lame" = "xyes")
AC_SUBST(MP3_LIBS)

dnl Check for amr-wb libraries
AC_ARG_WITH(amr-wb,
    AC_HELP_STRING([--without-amr-wb],
        [Don't try to use amr-wb]))
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
   AC_DEFINE(HAVE_AMRWB, 1, [Define to 1 if you have AMR-WB.])
fi
AM_CONDITIONAL(HAVE_AMRWB, test x$using_amr_wb = xyes)
AC_SUBST(AMR_WB_LIBS)

dnl Check for amr-nb libraries
AC_ARG_WITH(amr-nb,
    AC_HELP_STRING([--without-amr-nb],
        [Don't try to use amr-nb]))
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
   AC_DEFINE(HAVE_AMRNB, 1, [Define to 1 if you have AMR-NB.])
fi
AM_CONDITIONAL(HAVE_AMRNB, test x$using_amr_nb = xyes)
AC_SUBST(AMR_NB_LIBS)

dnl Check for wavpack libraries
AC_ARG_WITH(wavpack,
    AC_HELP_STRING([--without-wavpack],
        [Don't try to use wavpack]))
using_wavpack=no
if test "$with_wavpack" != "no"; then
    using_wavpack=yes
    AC_CHECK_HEADER(wavpack/wavpack.h,
        [AC_CHECK_LIB(wavpack, WavpackGetSampleRate, WAVPACK_LIBS="$WAVPACK_LIBS -lwavpack" ,using_wavpack=no)],
        using_wavpack=no)
    if test "$with_wavpack" = "yes" -a "$using_wavpack" = "no"; then
        AC_MSG_FAILURE([cannot find wavpack])
    fi
fi
if test "$using_wavpack" = yes; then
   AC_DEFINE(HAVE_WAVPACK, 1, [Define to 1 if you have WavPack.])
fi
AM_CONDITIONAL(HAVE_WAVPACK, test x$using_wavpack = xyes)
AC_SUBST(WAVPACK_LIBS)

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

dnl Test for libsamplerate.
AC_ARG_WITH(samplerate,
    AC_HELP_STRING([--without-samplerate],
        [Don't try to use libsamplerate (aka Secret Rabbit Code)]))
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

dnl Generate output files.
AX_CREATE_STDINT_H(src/soxstdint.h)
AC_CONFIG_FILES(Makefile src/Makefile libgsm/Makefile lpc10/Makefile sox.pc)
AC_OUTPUT

if test "$found_libgsm" = "yes"; then
   gsm_option="external"
else
   gsm_option="in-tree"
fi

if test "$found_liblpc10" = "yes"; then
   lpc10_option="external"
else
   lpc10_option="in-tree"
fi

if test "$using_pkgconfig" = "no"; then
    pkgconfig_option="disabled"
else
    pkgconfig_option="$with_pkgconfigdir"
fi

dnl Report configuration.
echo
echo "Debugging build................... $enable_debug"
echo "External module support........... $using_libltdl"
echo "ALSA driver....................... $enable_alsa"
echo "libao driver...................... $enable_libao"
echo "OSS driver........................ $enable_oss"
echo "SUN audio driver.................. $enable_sun_audio"
echo "CoreAudio driver.................. $enable_coreaudio"
echo "symlinks enabled.................. $enable_symlinks"
echo "play and rec symlinks............. $enable_playrec_symlinks"
echo "libgsm............................ $gsm_option"
echo "liblpc10.......................... $lpc10_option"
echo "libsndfile formats................ $using_sndfile"
echo "Ogg Vorbis format................. $using_ogg"
echo "FLAC format....................... $using_flac"
echo "ffmpeg formats.................... $using_ffmpeg"
echo "magic library..................... $using_magic"
echo "MAD MP3 reader.................... $using_mad"
echo "id3tag library.................... $using_id3tag"
echo "LAME MP3 writer................... $using_lame"
echo "AMR-WB format..................... $using_amr_wb"
echo "AMR-NB format..................... $using_amr_nb"
echo "WavPack format.................... $using_wavpack"
echo "LADSPA effects.................... $using_ladspa"
echo "PNG support....................... $using_png"
echo "Secret Rabbit Code resampling..... $using_samplerate"
echo "pkg-config location............... $pkgconfig_option"
echo
echo "Configure finished.  Do 'make && make install' to compile and install SoX."
echo
