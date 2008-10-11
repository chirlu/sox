dnl SOX_PATH_SAMPLERATE
dnl Based off of shout.m4 from xiph package.
dnl cbagwell@users.sourceforge.net 1-3-2007
dnl
dnl Original Authors:
dnl Jack Moffitt <jack@icecast.org> 08-06-2001
dnl Rewritten for libshout 2
dnl Brendan Cully <brendan@xiph.org> 20030612
dnl
# SOX_PATH_SAMPLERATE([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
# Test for libsamplerate, and define SAMPLERATE_CFLAGS and SAMPLERATE_LIBS
AC_DEFUN([SOX_PATH_SAMPLERATE],
[dnl
# Step 1: Use PKG_CHECK_MODULES if available
m4_ifdef([PKG_CHECK_MODULES],
  [# PKG_CHECK_MODULES available
  PKG_CHECK_MODULES(SAMPLERATE, samplerate, have_samplerate="maybe",
                    have_samplerate="no")],
  [# Step 2: Use pkg-config manually if available
  AC_PATH_PROG([PKGCONFIG], [pkg-config], [none])
  if test "$PKGCONFIG" != "none" && `$PKGCONFIG --exists samplerate`
  then
    SAMPLERATE_CFLAGS=`$PKGCONFIG --cflags samplerate`
    SAMPLERATE_LIBS=`$PKGCONFIG --libs samplerate`
    have_samplerate="maybe"
  else
    have_samplerate="no"
  fi])

# Step 3: Even if pkg-config says its not installed, user may have
# manually installed libraries with no pkg-config support
if test "$have_samplerate" = "no"
then
  # Some packages distribute a <package>-config which we could check
  # for but libsamplerate doesn't have that.  We could use AC_PATH_PROG() 
  # similar to above for finding pkg-config.

  # As a last resort, just hope that header and library can
  # be found in default paths and that it doesn't need
  # to link against any other libraries. 
  SAMPLERATE_LIBS="-lsamplerate"
  have_samplerate="maybe"
fi

# Even if pkg-config or similar told us how to find the library,
# do a safety check.
if test "$have_samplerate" != "no"
then
  ac_save_CFLAGS="$CFLAGS"
  ac_save_LIBS="$LIBS"
  CFLAGS="$CFLAGS $SAMPLERATE_CFLAGS"
  LIBS="$LIBS $SAMPLERATE_LIBS"
  AC_CHECK_HEADER([samplerate.h], [
    AC_DEFINE([HAVE_SAMPLERATE_H], 1, [Define if you have <samplerate.h>])
    AC_CHECK_FUNC([src_new], [
      ifelse([$1], , :, [$1])
      have_samplerate="yes"
    ])
  ])
  CFLAGS="$ac_save_CFLAGS"
  LIBS="$ac_save_LIBS"
  if test "$have_samplerate" != "yes"
  then
    SAMPLERATE_LIBS=""
    SAMPLERATE_CFLAGS=""
  fi
fi

if test "$have_samplerate" != "yes"
then
  ifelse([$2], , :, [$2])
fi
])dnl SOX_PATH_SAMPLERATE
