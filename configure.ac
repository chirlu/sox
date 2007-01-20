dnl Process this file with autoconf to produce a configure script.

AC_INIT(SoX, 12.18.3, sox-devel@lists.sourceforge.net)
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

dnl Check for Sun-style audio
with_sun_audio=no
AC_CHECK_HEADERS(sys/audioio.h, with_sun_audio=yes)
AC_CHECK_HEADERS(sun/audioio.h, with_sun_audio=yes)
if test "$with_sun_audio" = yes; then
   AC_DEFINE(HAVE_SUN_AUDIO, 1, [Define to 1 if you have Sun /dev/audio.])
fi

dnl Check for libsndfile
with_sndfile=yes
AC_ARG_WITH(sndfile,
    AC_HELP_STRING([--with-sndfile],
        [Use libsndfile if present (default is YES)]))
if test "$with_sndfile" = yes; then
  SOX_PATH_SNDFILE(, 
                      [with_sndfile=no
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
with_flac=yes
AC_ARG_WITH(flac,
    AC_HELP_STRING([--with-flac],
        [Use FLAC if present (default is YES)]))
if test "$with_flac" = yes; then
    AC_CHECK_HEADER(FLAC/file_encoder.h,
        [AC_CHECK_LIB(FLAC, FLAC__file_encoder_new,,with_flac=no)],
        with_flac=no)
fi

dnl Check for MAD libraries
with_mad=yes
AC_ARG_WITH(mad,
    AC_HELP_STRING([--with-mad],
        [Use MAD (MP3 Audio Decoder) if present (default is YES)]))
if test "$with_mad" = yes; then
    AC_CHECK_HEADER(mad.h,
        [AC_CHECK_LIB(mad, mad_stream_buffer,,with_mad=no)],
        with_mad=no)
fi

dnl Test for LAME library.
with_lame=yes
AC_ARG_WITH(lame,
    AC_HELP_STRING([--with-lame],
        [Use LAME (LAME Ain't an MP3 Encoder) if present (default is YES)]))
if test "$with_lame" = yes; then
    AC_CHECK_HEADER(lame/lame.h,
        [AC_CHECK_LIB(mp3lame, lame_init,,with_lame=no)],
        with_lame=no)
fi

dnl Test for libsamplerate.
with_samplerate=yes
AC_ARG_WITH(samplerate,
    AC_HELP_STRING([--with-samplerate],
        [Use libsamplerate (aka Secret Rabbit Code) if present (default is YES)]))
if test "$with_samplerate" = yes; then
  SOX_PATH_SAMPLERATE(, 
                      [with_samplerate=no
                       AC_MSG_NOTICE([Could not find a usable libsamplerate])])
fi
AC_SUBST(SAMPLERATE_CFLAGS)
AC_SUBST(SAMPLERATE_LIBS)

dnl LUA, LUA_INCLUDES, LUA_LIBS

dnl Arguments
AC_ARG_WITH([lua-prefix],
            [AC_HELP_STRING([--with-lua-prefix=DIR],
                            [Lua files are in DIR])])
AC_ARG_WITH([lua-includes],
            [AC_HELP_STRING([--with-lua-includes=DIR],
                            [Lua include files are in DIR])])
AC_ARG_WITH([lua-libraries],
            [AC_HELP_STRING([--with-lua-libraries=DIR],
                            [Lua library files are in DIR])])
AC_ARG_WITH([lua-suffix],
            [AC_HELP_STRING([--with-lua-suffix=ARG],
                            [Lua binary and library files are suffixed with
                             ARG])])

dnl Default to built-in Lua; if external Lua supplied, test it
if test "x$with_lua_prefix" = x && test "x$with_lua_includes" = x && \
   test "x$with_lua_libraries" = x && test "x$with_lua_suffix" = x; then
   AC_MSG_NOTICE("Using built-in Lua")
   with_lua_prefix="\$(top_srcdir)/lua"
   want_internal_lua=yes
   LUA="$with_lua_prefix/lua"
   LUA_INCLUDES="-I$with_lua_prefix/src -I\$(top_builddir)/lua/src"
   LUA_LIBS="-L\$(top_builddir)/lua/src -llua"
else
   want_internal_lua=no

AC_SUBST(LUA)
AC_SUBST(LUA_INCLUDES)
AC_SUBST(LUA_LIBS)

dnl LUA
if test "x$with_lua_prefix" = x; then
    lua_search_path="$PATH"
else
    lua_search_path="$with_lua_prefix/bin"
fi
if test "x$LUA" = x; then
    AC_PATH_PROG([LUA], [lua$with_lua_suffix], [], [$lua_search_path])
    test "x$LUA" = x && AC_MSG_FAILURE([*** Can't build SoX without Lua])
fi

dnl lua_version
AC_MSG_CHECKING([for Lua version == 5.1])
lua_version=$($LUA -v 2>&1 | head -n 1 | cut -d' ' -f2)
case $lua_version in
5.1*)
    AC_MSG_RESULT([yes (found $lua_version)])
    ;;
*)
    AC_MSG_RESULT([no (found $lua_version)])
    AC_MSG_FAILURE([*** Can't build SoX without Lua])
    ;;
esac
dnl

dnl LUA_INCLUDES
if test "x$with_lua_includes" != x; then
    LUA_INCLUDES="-I$with_lua_includes"
elif test "x$with_lua_prefix" != x; then
    LUA_INCLUDES="-I$with_lua_prefix/include"
fi
save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $LUA_INCLUDES"
AC_CHECK_HEADERS([lua.h lualib.h], [], [AC_MSG_FAILURE([*** Can't build SoX without Lua])], [])
CFLAGS="$save_CFLAGS"

dnl LUA_LIBS
if test "x$with_lua_libraries" != x; then
    LUA_LIBS="-L$with_lua_libraries"
elif test "x$with_lua_prefix" != x; then
    LUA_LIBS="-L$with_lua_prefix/lib"
fi
AC_CHECK_LIB([m], [exp], [lua_extra_libs="$lua_extra_libs -lm"], [])
AC_CHECK_LIB([dl], [dlopen], [lua_extra_libs="$lua_extra_libs -ldl"], [])
AC_CHECK_LIB([lua$with_lua_suffix],
             [lua_call],
             [LUA_LIBS="$LUA_LIBS -llua$with_lua_suffix $lua_extra_libs"],
             [AC_MSG_FAILURE([*** Can't build SoX without Lua])],
             [$LUA_LIBS $lua_extra_libs])

dnl liblua_version
AC_MSG_CHECKING([for liblua version == 5.1])
save_CFLAGS="$CFLAGS"
save_LIBS="$LIBS"
CFLAGS="$CFLAGS $LUA_INCLUDES"
LIBS="$LIBS $LUA_LIBS"
AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <lua.h>
#include <stdlib.h>
#include <stdio.h>
int main()
{
    printf("(found %s, %d)... ", LUA_VERSION, LUA_VERSION_NUM);
    if (LUA_VERSION_NUM == 501)
        exit(EXIT_SUCCESS);
    exit(EXIT_FAILURE);
}

]])],
              [AC_MSG_RESULT([yes])],
              [AC_MSG_RESULT([no])])
CFLAGS="$save_CFLAGS"
LIBS="$save_LIBS"

fi dnl Check external Lua

dnl Build internal Lua if needed
AM_CONDITIONAL([INTERNAL_LUA], [test "$want_internal_lua" = yes])


dnl Generate output files.
AX_CREATE_STDINT_H(src/ststdint.h)
AC_CONFIG_FILES(Makefile src/Makefile src/libgsm/Makefile src/libst-config)
AC_CONFIG_SUBDIRS([lua])
AC_OUTPUT

dnl Report configuration.
echo
echo "ALSA Driver....................... $with_alsa"
echo "OSS Driver........................ $with_oss"
echo "SUN /dev/audio.................... $with_sun_audio"
echo "libsndfile support................ $with_sndfile"
echo "Ogg Vorbis support................ $with_ogg_vorbis"
echo "FLAC support...................... $with_flac"
echo "MAD MP3 Decoder................... $with_mad"
echo "LAME MP3 Encoder.................. $with_lame"
echo "Secret Rabbit Code resampling..... $with_samplerate"
echo
echo "Configure finished.  Do 'make; make install' to compile and install SoX."
echo
