dnl SOX_PATH_FFMPEG
dnl cbagwell@users.sourceforge.net 1-3-2007
dnl
# SOX_PATH_FFMPEG([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
# Test for libavformat, and define FFMPEG_CFLAGS and FFMPEG_LIBS
AC_DEFUN([SOX_PATH_FFMPEG],
[dnl
# Step 1: Use PKG_CHECK_MODULES if available
m4_ifdef([PKG_CHECK_MODULES],
  [# PKG_CHECK_MODULES available
  PKG_CHECK_MODULES(FFMPEG, [libavformat libavcodec libavutil], 
                    have_ffmpeg="maybe",
                    have_ffmpeg="no")],
  [# Step 2: Use pkg-config manually if available
  AC_PATH_PROG([PKGCONFIG], [pkg-config], [none])
  if test "$PKGCONFIG" != "none" && `$PKGCONFIG --exists libavformat libavcodec libavutil`
  then
    FFMPEG_CFLAGS=`$PKGCONFIG --cflags libavformat`
    FFMPEG_LIBS=`$PKGCONFIG --libs libavformat libavcodec libavutil`
    have_ffmpeg="maybe"
  else
    have_ffmpeg="no"
  fi])

# Step 3: Even if pkg-config says its not installed, user may have
# manually installed libraries with no pkg-config support
if test "$have_ffmpeg" = "no"
then
  # Some packages distribute a <package>-config which we could check
  # for but libavformat doesn't have that.  We could use AC_PATH_PROG() 
  # similar to above for finding pkg-config.

  # As a last resort, just hope that header and library can
  # be found in default paths and that it doesn't need
  # to link against any other libraries. 
  FFMPEG_LIBS="-lavformat -lavcodec -lavutil"
  have_ffmpeg="maybe"
fi

# Even if pkg-config or similar told us how to find the library,
# do a safety check.
if test "$have_ffmpeg" != "no"
then
  ac_save_CFLAGS="$CFLAGS"
  ac_save_CPPFLAGS="$CPPFLAGS"
  ac_save_LIBS="$LIBS"
  CFLAGS="$CFLAGS $FFMPEG_CFLAGS"
  CPPFLAGS="$CPPFLAGS $FFMPEG_CFLAGS"
  LIBS="$LIBS $FFMPEG_LIBS"
  have_ffmpeg="no"
  AC_CHECK_HEADERS([libavformat/avformat.h ffmpeg/avformat.h],
    [AC_CHECK_LIB(avformat, av_open_input_file, 
                 have_ffmpeg=yes)
    break])
  CFLAGS="$ac_save_CFLAGS"
  CPPFAGS="$ac_save_CPPFLAGS"
  LIBS="$ac_save_LIBS"
fi

if test "$have_ffmpeg" != "yes"
then
  ifelse([$2], , :, [$2])
fi
])dnl SOX_PATH_FFMPEG
