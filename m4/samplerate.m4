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
# Step 1: Use pkg-config if available
m4_ifdef([PKG_CHECK_MODULES],
  [# PKG_CHECK_MODULES available
  PKG_CHECK_MODULES(SAMPLERATE, samplerate, have_samplerate="maybe",
                    have_samplerate="no")],
  [# PKG_CHECK_MODULES is unavailable, search for pkg-config program
  AC_PATH_PROG([PKGCONFIG], [pkg-config], [none])
  if test "$PKGCONFIG" != "none" && `$PKGCONFIG --exists samplerate`
  then
    SAMPLERATE_CFLAGS=`$PKGCONFIG --cflags samplerate`
    SAMPLERATE_LIBS=`$PKGCONFIG --libs samplerate`
    have_samplerate="maybe"
  else
    if test "$PKGCONFIG" != "none"
    then
      AC_MSG_NOTICE([$PKGCONFIG couldn't find libsamplerate. Try adjusting PKG_CONFIG_PATH.])
    fi
    # libsamplerate doesn't have samplerate-config but other
    # packages do and so keep around as an example.
    # Step 2: try samplerate-config
    #AC_PATH_PROG([SAMPLERATECONFIG], [samplerate-config], [none])
    #if test "$SAMPLERATECONFIG" != "none" && test `$SAMPLERATECONFIG --package` = "libsamplerate"
    #then
    #  SAMPLERATE_CFLAGS=`$SAMPLERATECONFIG --cflags`
    #  SAMPLERATE_LIBS=`$SAMPLERATECONFIG --libs`
    #  have_samplerate="maybe"
    #fi
  fi
  ])

m4_ifndef([PKG_CHECK_MODULES],
  [# PKG_CHECK_MODULES not available
  # Best guess is that samplerate only needs to link against itself
  SAMPLERATE_LIBS="-lsamplerate"
  ])

# Now try actually using libsamplerate
if test "$have_samplerate" != "no"
then
  ac_save_CFLAGS="$CFLAGS"
  ac_save_LIBS="$LIBS"
  CFLAGS="$CFLAGS $SAMPLERATE_CFLAGS"
  LIBS="$LIBS $SAMPLERATE_LIBS"
  dnl Must hardcode samplerate library if we can not get it from pkg-config
  AC_CHECK_HEADER([samplerate.h], [
    AC_DEFINE([HAVE_SAMPLERATE_H], 1, [Define if you have <samplerate.h>])
    AC_CHECK_FUNC([src_new], [
      ifelse([$1], , :, [$1])
      have_samplerate="yes"
    ])
  ])
  CFLAGS="$ac_save_CFLAGS"
  LIBS="$ac_save_LIBS"
fi

if test "$have_samplerate" != "yes"
then
  ifelse([$2], , :, [$2])
fi
])dnl SOX_PATH_SAMPLERATE
