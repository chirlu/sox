dnl SOX_PATH_SNDFILE
dnl Based off of shout.m4 from xiph package.
dnl cbagwell@users.sourceforge.net 1-3-2007
dnl
dnl Original Authors:
dnl Jack Moffitt <jack@icecast.org> 08-06-2001
dnl Rewritten for libshout 2
dnl Brendan Cully <brendan@xiph.org> 20030612
dnl
# SOX_PATH_SNDFILE([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
# Test for libsndfile, and define LIBSNDFILE_CFLAGS and LIBSNDFILE_LIBS
AC_DEFUN([SOX_PATH_SNDFILE],
[dnl
# Step 1: Use pkg-config if available
m4_ifdef([PKG_CHECK_MODULES],
  [# PKG_CHECK_MODULES available
  PKG_CHECK_MODULES(LIBSNDFILE, sndfile, have_sndfile="maybe",
                    have_sndfile="no")],
  [# Step 2: use pkg-config manually if available
  AC_PATH_PROG([PKGCONFIG], [pkg-config], [none])
  if test "$PKGCONFIG" != "none" && `$PKGCONFIG --exists sndfile`
  then
    LIBSNDFILE_CFLAGS=`$PKGCONFIG --cflags sndfile`" $ac_save_SNDFILE_CFLAGS $LIBSNDFILE_CFLAGS"
    LIBSNDFILE_LIBS=`$PKGCONFIG --libs sndfile`" $ac_save_SNDFILE_LIBS $LIBSNDFILE_LIBS"
    have_sndfile="maybe"
  else
    have_sndfile="no"
  fi
  ])

# Step 3: Even if pkg-config says its not installed, user may have
# manually installed libraries with no -kg-config support.
if test "$have_sndfile" = "no"
then
  # As a last resort, just hope that header and ilbrary can
  # be found in default paths and that it doesn't need
  # to link against any other libraries.
  LIBSNDFILE_LIBS="-lsndfile $LIBSNDFILE_LIBS"
  have_sndfile="maybe"
fi

# Even if pkg-config or similar told us how to find library,
# do a safety check.
if test "$have_sndfile" != "no"
then
  ac_save_CFLAGS="$CFLAGS"
  ac_save_LIBS="$LIBS"
  CFLAGS="$CFLAGS $LIBSNDFILE_CFLAGS"
  LIBS="$LIBS $LIBSNDFILE_LIBS"
  AC_CHECK_HEADER([sndfile.h], [
    AC_DEFINE([HAVE_SNDFILE_H], 1, [Define if you have <sndfile.h>])
    AC_CHECK_FUNC([sf_open_virtual], [
      ifelse([$1], , :, [$1])
      have_sndfile="yes"
    ])
  ])
  CFLAGS="$ac_save_CFLAGS"
  LIBS="$ac_save_LIBS"
fi

if test "$have_sndfile" != "yes"
then
  ifelse([$2], , :, [$2])
fi
])dnl SOX_PATH_SNDFILE
