dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.62)
LT_PREREQ(2.4)

AC_INIT(SoX, 14.4.3git, sox-devel@lists.sourceforge.net)

dnl Increase version when binary compatibility with previous version is broken
SHLIB_VERSION=3:0:0
AC_SUBST(SHLIB_VERSION)

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR(sox.1)
AC_CONFIG_HEADERS([src/soxconfig.h])

AM_INIT_AUTOMAKE([subdir-objects])
AM_SILENT_RULES([yes])

LT_INIT([win32-dll])

AC_ARG_WITH(distro,
    AS_HELP_STRING([--with-distro=name], [Provide distribution name]),
    AC_DEFINE_UNQUOTED(DISTRO, ["$with_distro"], [Distribution name])
    SOX_REPORT([build], [distro name], [${with_distro:-unspecified}]))

dnl Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LN_S

PKG_PROG_PKG_CONFIG
PKG_INSTALLDIR

AX_APPEND_COMPILE_FLAGS([-fstack-protector-strong])
AX_APPEND_COMPILE_FLAGS([-Wall -Wmissing-prototypes -Wstrict-prototypes])
AX_APPEND_LINK_FLAGS([-Wl,--as-needed])

dnl Check for system dependent features.
AC_C_BIGENDIAN
AC_C_INLINE

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h unistd.h byteswap.h sys/ioctl.h sys/stat.h sys/time.h sys/timeb.h sys/types.h sys/utsname.h termios.h glob.h fenv.h)

dnl Checks for library functions.
AC_CHECK_FUNCS(strcasecmp strdup popen vsnprintf gettimeofday mkstemp fmemopen sigaction)

dnl Check if math library is needed.
AC_SEARCH_LIBS([pow], [m])
AC_SEARCH_LIBS([lrint], [m])
AC_CHECK_FUNCS([lrint])

dnl Large File Support
AC_SYS_LARGEFILE
AC_FUNC_FSEEKO

dnl Check for OpenMP
AC_OPENMP
CFLAGS="$CFLAGS $OPENMP_CFLAGS"
SOX_REPORT([other], [OpenMP], [${OPENMP_CFLAGS:-no}])

if test "$ac_cv_header_glob_h" != yes; then
    AC_CHECK_FUNC([FindFirstFileA], [
        AC_DEFINE([HAVE_GLOB_H], [1], [Define if glob.h exists])
        HAVE_WIN32_GLOB=yes])
fi
AM_CONDITIONAL(HAVE_WIN32_GLOB, test "$HAVE_WIN32_GLOB" = yes)

SOX_WITH_LIB([libltdl], [ltdl.h], [ltdl], [lt_dlinit])

SOX_WITH([ladspa], [Enable LADSPA plugin support],
    [SOX_NEED_DL([${with_ladspa:+dyn}], [--with-ladspa])],
    [AC_DEFINE([HAVE_LADSPA_H], [1], [Define if LADSPA support is enabled])
     AS_CASE([$HAVE_LADSPA], [yes], [LADSPA_PATH='${libdir}/ladspa'],
        [LADSPA_PATH=$HAVE_LADSPA])], [], [], [*], [$HAVE_LIBLTDL])
AC_SUBST([LADSPA_PATH])
SOX_REPORT([other], [LADSPA effect plugins], [$HAVE_LADSPA])

dnl Various libraries

SOX_WITH_LIB([magic], [magic.h], [magic], [magic_open])
SOX_CHECK_LIB([ZLIB], [zlib.h], [z], [uncompress])
SOX_WITH_LIB([png], [png.h], [png], [png_set_rows], [], [], [], [$ZLIB_LIBS])
SOX_WITH_LIB([id3tag], [id3tag.h], [id3tag], [id3_file_open])
SOX_WITH_LIB([libgsm], [gsm/gsm.h gsm.h], [gsm], [gsm_create])

dnl Optional formats

SOX_ENABLE([formats], [Default setting for optional formats],
    [SOX_NEED_DL([$HAVE_FORMATS], [--enable-formats=dyn])], [], [], [], [dyn])

SOX_DL_LIB([opencore-amrnb], [opencore-amrnb/interf_dec.h], [opencore-amrnb],
    [Decoder_Interface_init])
SOX_FMT_REQ([amrnb], [OPENCORE_AMRNB])

SOX_DL_LIB([opencore-amrwb], [opencore-amrwb/dec_if.h], [opencore-amrwb],
    [D_IF_init])
SOX_DL_LIB([vo-amrwbenc], [vo-amrwbenc/enc_if.h], [vo-amrwbenc], [E_IF_init])
SOX_FMT_REQ([amrwb], [OPENCORE_AMRWB VO_AMRWBENC])

SOX_FMT_PKG([flac], [flac])
SOX_FMT_REQ([gsm], [LIBGSM])

SOX_CHECK_LIB([LIBLPC10], [lpc10.h], [lpc10], [create_lpc10_encoder_state],
    [LPC10_LIBS="$LIBLPC10_LIBS"],
    [LPC10_LIBS=../lpc10/liblpc10.la; LPC10_CFLAGS='-I${top_srcdir}/lpc10'])
AM_CONDITIONAL([HAVE_LIBLPC10], [test $HAVE_LIBLPC10 = yes])
SOX_FMT([lpc10])

SOX_DL_LIB([mad], [mad.h], [mad], [mad_stream_buffer])
SOX_DL_LIB([lame], [lame/lame.h lame.h], [mp3lame], [lame_init])
SOX_DL_LIB([twolame], [twolame.h], [twolame], [twolame_init])
SOX_FMT_REQ([mp3], [MAD LAME TWOLAME])

SOX_FMT_PKG([oggvorbis], [ogg vorbis vorbisenc vorbisfile])
SOX_FMT_PKG([opus], [opusfile])

SOX_DL_LIB([libsndfile], [sndfile.h], [sndfile], [sf_open_virtual])
SOX_FMT_REQ([sndfile], [LIBSNDFILE])

SOX_FMT_LIB([wavpack], [wavpack/wavpack.h], [wavpack], [WavpackGetSampleRate])

dnl Audio devices

SOX_FMT_LIB([alsa], [alsa/asoundlib.h], [asound], [snd_pcm_open], [], [],
    [devices])
SOX_FMT_LIB([ao], [ao/ao.h], [ao], [ao_play], [], [], [devices])
SOX_FMT_LIB([coreaudio], [CoreAudio/CoreAudio.h], [], [AudioDeviceStart], [],
    [[-Wl,-framework,CoreAudio]], [devices])
SOX_FMT_LIB([pulseaudio], [pulse/simple.h], [pulse], [pa_simple_new], [],
    [-lpulse-simple], [devices])
SOX_FMT_HEADERS([oss], [sys/soundcard.h], [SOUND_MIXER_MUTE], [], [devices])
SOX_FMT_LIB([sndio], [sndio.h], [sndio], [sio_open], [], [], [devices])
SOX_FMT_HEADERS([sunaudio], [sys/audioio.h sun/audioio.h],
    [AUDIO_GETINFO], [], [devices])
SOX_FMT_LIB([waveaudio], [mmsystem.h], [winmm], [waveOutOpen], [windows.h], [],
    [devices])

dnl Check if we want to create symlinks
AC_ARG_ENABLE([symlinks],
    AS_HELP_STRING([--enable-symlinks], [Create soxi/play/rec links (YES/no)]))
AS_CASE([${enable_symlinks:=yes}], [yes], [
    SYMLINKS=soxi
    AS_IF([test -n "$sox_devices"], [SYMLINKS="$SYMLINKS play rec"])])
AC_SUBST([SYMLINKS])
SOX_REPORT([build], [symlinks], [${SYMLINKS:-none}])

AM_CONDITIONAL(STATIC_LIBSOX_ONLY, test "$enable_shared" = "no")

dnl Generate output files.
AC_CONFIG_FILES(Makefile src/Makefile lpc10/Makefile sox.pc)
AC_OUTPUT

SOX_REPORT_SECTION([build],     [Build options:])
SOX_REPORT_SECTION([with_libs], [Optional libraries:], [sort])
SOX_REPORT_SECTION([formats],   [Optional formats:],   [sort])
SOX_REPORT_SECTION([devices],   [Audio devices:],      [sort])
SOX_REPORT_SECTION([other],     [Other options:])
SOX_REPORT_PRINT

echo
echo "Configure finished.  Run 'make' to compile SoX."
echo
