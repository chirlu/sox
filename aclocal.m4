dnl @synopsis AC_COMPILE_CHECK_SIZEOF(TYPE [, HEADERS [, EXTRA_SIZES...]])
dnl
dnl This macro checks for the size of TYPE using compile checks, not
dnl run checks. You can supply extra HEADERS to look into. the check
dnl will cycle through 1 2 4 8 16 and any EXTRA_SIZES the user
dnl supplies. If a match is found, it will #define SIZEOF_`TYPE' to
dnl that value. Otherwise it will emit a configure time error
dnl indicating the size of the type could not be determined.
dnl
dnl The trick is that C will not allow duplicate case labels. While
dnl this is valid C code:
dnl
dnl      switch (0) case 0: case 1:;
dnl
dnl The following is not:
dnl
dnl      switch (0) case 0: case 0:;
dnl
dnl Thus, the AC_TRY_COMPILE will fail if the currently tried size
dnl does not match.
dnl
dnl Here is an example skeleton configure.in script, demonstrating the
dnl macro's usage:
dnl
dnl      AC_PROG_CC
dnl      AC_CHECK_HEADERS(stddef.h unistd.h)
dnl      AC_TYPE_SIZE_T
dnl      AC_CHECK_TYPE(ssize_t, int)
dnl
dnl      headers='#ifdef HAVE_STDDEF_H
dnl      #include <stddef.h>
dnl      #endif
dnl      #ifdef HAVE_UNISTD_H
dnl      #include <unistd.h>
dnl      #endif
dnl      '
dnl
dnl      AC_COMPILE_CHECK_SIZEOF(char)
dnl      AC_COMPILE_CHECK_SIZEOF(short)
dnl      AC_COMPILE_CHECK_SIZEOF(int)
dnl      AC_COMPILE_CHECK_SIZEOF(long)
dnl      AC_COMPILE_CHECK_SIZEOF(unsigned char *)
dnl      AC_COMPILE_CHECK_SIZEOF(void *)
dnl      AC_COMPILE_CHECK_SIZEOF(size_t, $headers)
dnl      AC_COMPILE_CHECK_SIZEOF(ssize_t, $headers)
dnl      AC_COMPILE_CHECK_SIZEOF(ptrdiff_t, $headers)
dnl      AC_COMPILE_CHECK_SIZEOF(off_t, $headers)
dnl
dnl @author Kaveh Ghazi <ghazi@caip.rutgers.edu>
dnl @version $Id: aclocal.m4,v 1.5 2006/11/11 22:58:04 rrt Exp $
dnl
AC_DEFUN([AC_COMPILE_CHECK_SIZEOF],
[changequote(<<, >>)dnl
dnl The name to #define.
define(<<AC_TYPE_NAME>>, translit(sizeof_$1, [a-z *], [A-Z_P]))dnl
dnl The cache variable name.
define(<<AC_CV_NAME>>, translit(ac_cv_sizeof_$1, [ *], [_p]))dnl
changequote([, ])dnl
AC_MSG_CHECKING(size of $1)
AC_CACHE_VAL(AC_CV_NAME,
[for ac_size in 4 8 1 2 16 $3 ; do # List sizes in rough order of prevalence.
  AC_TRY_COMPILE([#include "confdefs.h"
#include <sys/types.h>
], [switch (0) case 0: case (sizeof ($1) == $ac_size):;], AC_CV_NAME=$ac_size)
  if test x$AC_CV_NAME != x ; then break; fi
done
])
if test x$AC_CV_NAME = x ; then
  AC_MSG_ERROR([cannot determine a size for $1])
fi
AC_MSG_RESULT($AC_CV_NAME)
AC_DEFINE_UNQUOTED(AC_TYPE_NAME, $AC_CV_NAME, [The number of bytes in type $1])
undefine([AC_TYPE_NAME])dnl
undefine([AC_CV_NAME])dnl
])

dnl AC_CREATE_STDINT_H [( HEADER-TO-GENERATE [, HEDERS-TO-CHECK])] -*- sh -*-
dnl
dnl the "ISO C9X: 7.18 Integer types <stdint.h>" section requires the
dnl existence of an include file <stdint.h> that defines a set of 
dnl typedefs, especially uint8_t,int32_t,uintptr_t.
dnl Many older installations will not provide this file, but some will
dnl have the very same definitions in <inttypes.h>. In other enviroments
dnl we can use the inet-types in <sys/types.h> which would define the
dnl typedefs int8_t and u_int8_t respectivly.
dnl
dnl This macros will create a local "_stdint.h" or the headerfile given as 
dnl an argument. In many cases that file will just have a singular 
dnl "#include <stdint.h>" or "#include <inttypes.h>" statement, while
dnl in other environments it will provide the set of basic 'stdint's defined: 
dnl int8_t,uint8_t,int16_t,uint16_t,int32_t,uint32_t,intptr_t,uintptr_t
dnl int_least32_t.. int_fast32_t.. intmax_t
dnl which may or may not rely on the definitions of other files,
dnl or using the AC_COMPILE_CHECK_SIZEOF macro to determine the actual
dnl sizeof each type.
dnl
dnl if your header files require the stdint-types you will want to create an
dnl installable file mylib-int.h that all your other installable header
dnl may include. So if you have a library package named "mylib", just use
dnl      AC_CREATE_STDINT_H(mylib-int.h) 
dnl in configure.in and go to install that very header file in Makefile.am
dnl along with the other headers (mylib.h) - and the mylib-specific headers
dnl can simply use "#include <mylib-int.h>" to obtain the stdint-types.
dnl
dnl Remember, if the system already had a valid <stdint.h>, the generated
dnl file will include it directly. No need for fuzzy HAVE_STDINT_H things...
dnl
dnl (this file is part of the http://ac-archive.sf.net/gstdint project)
dnl @version $Id: aclocal.m4,v 1.5 2006/11/11 22:58:04 rrt Exp $
dnl @author  Guido Draheim <guidod@gmx.de>       STATUS: used on new platforms

AC_DEFUN([AC_CREATE_STDINT_H],
[# ------ AC CREATE STDINT H -------------------------------------
AC_MSG_CHECKING([for stdint-types....])
ac_stdint_h=`echo ifelse($1, , _stdint.h, $1)`
if test "$ac_stdint_h" = "stdint.h" ; then
 AC_MSG_RESULT("(are you sure you want them in ./stdint.h?)")
elif test "$ac_stdint_h" = "inttypes.h" ; then
 AC_MSG_RESULT("(are you sure you want them in ./inttypes.h?)")
else
 AC_MSG_RESULT("(putting them into $ac_stdint_h)")
fi

inttype_headers=`echo inttypes.h sys/inttypes.h sys/inttypes.h $2 \
| sed -e 's/,/ /g'`

 ac_cv_header_stdint_x="no-file"
 ac_cv_header_stdint_o="no-file"
 ac_cv_header_stdint_u="no-file"
 for i in stdint.h $inttype_headers ; do
   unset ac_cv_type_uintptr_t 
   unset ac_cv_type_uint64_t
   _AC_CHECK_TYPE_NEW(uintptr_t,[ac_cv_header_stdint_x=$i],dnl
     continue,[#include <$i>])
   AC_CHECK_TYPE(uint64_t,[and64="(uint64_t too)"],[and64=""],[#include<$i>])
   AC_MSG_RESULT(... seen our uintptr_t in $i $and64)
   break;
 done
 if test "$ac_cv_header_stdint_x" = "no-file" ; then
 for i in stdint.h $inttype_headers ; do
   unset ac_cv_type_uint32_t
   unset ac_cv_type_uint64_t
   AC_CHECK_TYPE(uint32_t,[ac_cv_header_stdint_o=$i],dnl
     continue,[#include <$i>])
   AC_CHECK_TYPE(uint64_t,[and64="(uint64_t too)"],[and64=""],[#include<$i>])
   AC_MSG_RESULT(... seen our uint32_t in $i $and64)
   break;
 done
 if test "$ac_cv_header_stdint_o" = "no-file" ; then
 for i in sys/types.h $inttype_headers ; do
   unset ac_cv_type_u_int32_t
   unset ac_cv_type_u_int64_t
   AC_CHECK_TYPE(u_int32_t,[ac_cv_header_stdint_u=$i],dnl
     continue,[#include <$i>])
   AC_CHECK_TYPE(uint64_t,[and64="(u_int64_t too)"],[and64=""],[#include<$i>])
   AC_MSG_RESULT(... seen our u_int32_t in $i $and64)
   break;
 done
 fi
 fi

# ----------------- DONE inttypes.h checks MAYBE C basic types --------

if test "$ac_cv_header_stdint_x" = "no-file" ; then
   AC_COMPILE_CHECK_SIZEOF(char)
   AC_COMPILE_CHECK_SIZEOF(short)
   AC_COMPILE_CHECK_SIZEOF(int)
   AC_COMPILE_CHECK_SIZEOF(long)
   AC_COMPILE_CHECK_SIZEOF(void*)
   ac_cv_header_stdint_test="yes"
else
   ac_cv_header_stdint_test="no"
fi

# ----------------- DONE inttypes.h checks START header -------------
_ac_stdint_h=AS_TR_CPP(_$ac_stdint_h)
AC_MSG_RESULT(creating $ac_stdint_h : $_ac_stdint_h)
echo "#ifndef" $_ac_stdint_h >$ac_stdint_h
echo "#define" $_ac_stdint_h "1" >>$ac_stdint_h
echo "#ifndef" _GENERATED_STDINT_H >>$ac_stdint_h
echo "#define" _GENERATED_STDINT_H '"'$PACKAGE $VERSION'"' >>$ac_stdint_h
if test "$GCC" = "yes" ; then
  echo "/* generated using a gnu compiler version" `$CC --version` "*/" \
  >>$ac_stdint_h
else
  echo "/* generated using $CC */" >>$ac_stdint_h
fi
echo "" >>$ac_stdint_h

if test "$ac_cv_header_stdint_x" != "no-file" ; then
   ac_cv_header_stdint="$ac_cv_header_stdint_x"
elif  test "$ac_cv_header_stdint_o" != "no-file" ; then
   ac_cv_header_stdint="$ac_cv_header_stdint_o"
elif  test "$ac_cv_header_stdint_u" != "no-file" ; then
   ac_cv_header_stdint="$ac_cv_header_stdint_u"
else
   ac_cv_header_stdint="stddef.h"
fi

if test "$ac_cv_header_stdint" != "stddef.h" ; then
if test "$ac_cv_header_stdint" != "stdint.h" ; then
AC_MSG_RESULT(..adding include stddef.h)
   echo "#include <stddef.h>" >>$ac_stdint_h
fi ; fi
AC_MSG_RESULT(..adding include $ac_cv_header_stdint)
   echo "#include <$ac_cv_header_stdint>" >>$ac_stdint_h
echo "" >>$ac_stdint_h

# ----------------- DONE header START basic int types -------------
if test "$ac_cv_header_stdint_x" = "no-file" ; then
   AC_MSG_RESULT(... need to look at C basic types)
dnl ac_cv_header_stdint_test="yes" # moved up before creating the file
else
   AC_MSG_RESULT(... seen good stdint.h inttypes)
dnl ac_cv_header_stdint_test="no"  # moved up before creating the file
fi

if test "$ac_cv_header_stdint_u" != "no-file" ; then
   AC_MSG_RESULT(... seen bsd/sysv typedefs)
   cat >>$ac_stdint_h <<EOF

/* int8_t int16_t int32_t defined by inet code, redeclare the u_intXX types */
typedef u_int8_t uint8_t;
typedef u_int16_t uint16_t;
typedef u_int32_t uint32_t;
EOF
    cat >>$ac_stdint_h <<EOF

/* glibc compatibility */
#ifndef __int8_t_defined
#define __int8_t_defined
#endif
EOF
fi

ac_cv_sizeof_x="$ac_cv_sizeof_char:$ac_cv_sizeof_short"
ac_cv_sizeof_X="$ac_cv_sizeof_x:$ac_cv_sizeof_int"
ac_cv_sizeof_X="$ac_cv_sizeof_X:$ac_cv_sizeof_voidp:$ac_cv_sizeof_long"
if test "$ac_cv_header_stdint" = "stddef.h" ; then
#   we must guess all the basic types. Apart from byte-adressable system,
# there a few 32-bit-only dsp-systems. nibble-addressable systems are way off.
    cat >>$ac_stdint_h <<EOF
/* ------------ BITSPECIFIC INTTYPES SECTION --------------- */
EOF
    t="typedefs for a"
    case "$ac_cv_sizeof_X" in
     1:2:2:2:4) AC_MSG_RESULT(..adding $t normal 16-bit system)
                cat >>$ac_stdint_h <<EOF
/*              a normal 16-bit system                       */
typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned long   uint32_t;
#ifndef __int8_t_defined
#define __int8_t_defined
typedef          char    int8_t;
typedef          short   int16_t;
typedef          long    int32_t;
#endif
EOF
;;
     1:2:2:4:4) AC_MSG_RESULT(..adding $t 32-bit system derived from a 16-bit)
                cat >>$ac_stdint_h <<EOF
/*              a 32-bit system derived from a 16-bit        */
typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned int    uint32_t;
#ifndef __int8_t_defined
#define __int8_t_defined
typedef          char    int8_t;
typedef          short   int16_t;
typedef          int     int32_t;
#endif
EOF
;;
     1:2:4:4:4) AC_MSG_RESULT(..adding $t normal 32-bit system)
                cat >>$ac_stdint_h <<EOF
/*              a normal 32-bit system                       */
typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned int    uint32_t;
#ifndef __int8_t_defined
#define __int8_t_defined
typedef          char    int8_t;
typedef          short   int16_t;
typedef          int     int32_t;
#endif
EOF
;;
     1:2:4:4:8) AC_MSG_RESULT(..adding $t 32-bit system prepared for 64-bit)
                cat >>$ac_stdint_h <<EOF

/*              a 32-bit system prepared for 64-bit          */
typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned int    uint32_t;
#ifndef __int8_t_defined
#define __int8_t_defined
typedef          char    int8_t;
typedef          short   int16_t;
typedef          int     int32_t;
#endif
EOF
;;
     1:2:4:8:8) AC_MSG_RESULT(..adding $t normal 64-bit system)
                cat >>$ac_stdint_h <<EOF

/*              a normal 64-bit system                       */
typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned int    uint32_t;
#ifndef __int8_t_defined
#define __int8_t_defined
typedef          char    int8_t;
typedef          short   int16_t;
typedef          int     int32_t;
#endif
EOF
;;
     1:2:4:8:4) AC_MSG_RESULT(..adding $t 64-bit system derived from a 32-bit)
                cat >>$ac_stdint_h <<EOF

/*              a 64-bit system derived from a 32-bit system */
typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned int    uint32_t;
#ifndef __int8_t_defined
#define __int8_t_defined
typedef          char    int8_t;
typedef          short   int16_t;
typedef          int     int32_t;
#endif
EOF
;;
  *)
    AC_MSG_ERROR([ $ac_cv_sizeof_X dnl
 what is that a system? contact the author, quick! http://ac-archive.sf.net])
    exit 1
;;
   esac
fi

# ------------- DONE basic int types START int64_t types ------------
if test "$ac_cv_type_uint64_t" = "yes" 
then AC_MSG_RESULT(... seen good uint64_t)
     cat >>$ac_stdint_h <<EOF

/* system headers have good uint64_t */
#ifndef _HAVE_UINT64_T
#define _HAVE_UINT64_T
#endif
EOF

elif test "$ac_cv_type_u_int64_t" = "yes" 
then AC_MSG_RESULT(..adding typedef u_int64_t uint64_t)
     cat >>$ac_stdint_h <<EOF

/* system headers have an u_int64_t */
#ifndef _HAVE_UINT64_T
#define _HAVE_UINT64_T
typedef u_int64_t uint64_t;
#endif
EOF
else AC_MSG_RESULT(..adding generic uint64_t runtime checks)
     cat >>$ac_stdint_h <<EOF

/* -------------------- 64 BIT GENERIC SECTION -------------------- */
/* here are some common heuristics using compiler runtime specifics */
#if defined __STDC_VERSION__ && defined __STDC_VERSION__ > 199901L

#ifndef _HAVE_UINT64_T
#define _HAVE_UINT64_T
typedef long long int64_t;
typedef unsigned long long uint64_t;
#endif

#elif !defined __STRICT_ANSI__
#if defined _MSC_VER || defined __WATCOMC__ || defined __BORLANDC__

#ifndef _HAVE_UINT64_T
#define _HAVE_UINT64_T
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#endif

#elif defined __GNUC__ || defined __MWERKS__ || defined __ELF__
dnl /* note: all ELF-systems seem to have loff-support which needs 64-bit */

#if !defined _NO_LONGLONG
#ifndef _HAVE_UINT64_T
#define _HAVE_UINT64_T
typedef long long int64_t;
typedef unsigned long long uint64_t;
#endif
#endif

#elif defined __alpha || (defined __mips && defined _ABIN32)

#if !defined _NO_LONGLONG
#ifndef _HAVE_UINT64_T
#define _HAVE_UINT64_T
typedef long int64_t;
typedef unsigned long uint64_t;
#endif
#endif
  /* compiler/cpu type ... or just ISO C99 */
#endif
#endif
EOF

# plus a default 64-bit for systems that are likely to be 64-bit ready 
  case "$ac_cv_sizeof_x:$ac_cv_sizeof_voidp:$ac_cv_sizeof_long" in
    1:2:8:8) AC_MSG_RESULT(..adding uint64_t default, normal 64-bit system)
cat >>$ac_stdint_h <<EOF
/* DEFAULT: */
/* seen normal 64-bit system, CC has sizeof(long and void*) == 8 bytes */
#ifndef _HAVE_UINT64_T
#define _HAVE_UINT64_T
typedef long int64_t;
typedef unsigned long uint64_t;
#endif 
EOF
;;
    1:2:4:8) AC_MSG_RESULT(..adding uint64_t default, typedef to long)
cat >>$ac_stdint_h <<EOF
/* DEFAULT: */
/* seen 32-bit system prepared for 64-bit, CC has sizeof(long) == 8 bytes */
#ifndef _HAVE_UINT64_T
#define _HAVE_UINT64_T
typedef long int64_t;
typedef unsigned long uint64_t;
#endif 
EOF
;;
    1:2:8:4) AC_MSG_RESULT(..adding uint64_t default, typedef long long)
cat >>$ac_stdint_h <<EOF
/* DEFAULT: */
/* seen 64-bit derived from a 32-bit, CC has sizeof(long) == 4 bytes */
#ifndef _HAVE_UINT64_T
#define _HAVE_UINT64_T
typedef long long int64_t;
typedef unsigned long long uint64_t;
#endif 
EOF
;;
   *)
cat >>$ac_stdint_h <<EOF
/* NOTE: */
/* the configure-checks for the basic types did not make us believe */
/* that we could add a fallback to a 'long long' typedef to int64_t */
EOF
  esac
fi

# ------------- DONE int64_t types START intptr types ------------
if test "$ac_cv_header_stdint_x" = "no-file" ; then
  cat >>$ac_stdint_h <<EOF

/* -------------------------- INPTR SECTION --------------------------- */
EOF
  case "$ac_cv_sizeof_x:$ac_cv_sizeof_voidp" in
  1:2:2)
    a="int16_t" ; cat >>$ac_stdint_h <<EOF
/* we tested sizeof(void*) to be of 2 chars, hence we declare it 16-bit */

typedef uint16_t uintptr_t;
typedef  int16_t  intptr_t;
EOF
;;
  1:2:4)
    a="int32_t" ; cat >>$ac_stdint_h <<EOF
/* we tested sizeof(void*) to be of 4 chars, hence we declare it 32-bit */

typedef uint32_t uintptr_t;
typedef  int32_t  intptr_t;
EOF
;;
  1:2:8)
    a="int64_t" ; cat >>$ac_stdint_h <<EOF
/* we tested sizeof(void*) to be of 8 chars, hence we declare it 64-bit */

typedef uint64_t uintptr_t;
typedef  int64_t  intptr_t;
EOF
;;
  *)
    a="long" ; cat >>$ac_stdint_h <<EOF
/* we tested sizeof(void*) but got no guess, hence we declare it as if long */

typedef unsigned long uintptr_t;
typedef          long  intptr_t;
EOF
;;
  esac
AC_MSG_RESULT(..adding typedef $a intptr_t)
fi

# ------------- DONE intptr types START int_least types ------------
if test "$ac_cv_header_stdint_x" = "no-file" ; then
AC_MSG_RESULT(..adding generic int_least-types)
     cat >>$ac_stdint_h <<EOF

/* --------------GENERIC INT_LEAST / INTMAX SECTION ------------------ */

typedef  int8_t    int_least8_t;
typedef  int16_t   int_least16_t;
typedef  int32_t   int_least32_t;

typedef uint8_t   uint_least8_t;
typedef uint16_t  uint_least16_t;
typedef uint32_t  uint_least32_t;

#ifdef _HAVE_INT64_T
typedef int64_t        intmax_t;
typedef uint64_t      uintmax_t;
#else
typedef long int       intmax_t;
typedef unsigned long uintmax_t;
#endif
EOF
fi

AC_MSG_RESULT(... DONE $ac_stdint_h)
   cat >>$ac_stdint_h <<EOF

  /* once */
#endif
#endif
EOF
])

dnl quote from SunOS-5.8 sys/inttypes.h:
dnl Use at your own risk.  As of February 1996, the committee is squarely
dnl behind the fixed sized types; the "least" and "fast" types are still being
dnl discussed.  The probability that the "fast" types may be removed before
dnl the standard is finalized is high enough that they are not currently
dnl implemented.

# pkg.m4 - Macros to locate and utilise pkg-config.            -*- Autoconf -*-
# 
# Copyright © 2004 Scott James Remnant <scott@netsplit.com>.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
# As a special exception to the GNU General Public License, if you
# distribute this file as part of a program that contains a
# configuration script generated by Autoconf, you may include it under
# the same distribution terms that you use for the rest of that program.

# PKG_PROG_PKG_CONFIG([MIN-VERSION])
# ----------------------------------
AC_DEFUN([PKG_PROG_PKG_CONFIG],
[m4_pattern_forbid([^_?PKG_[A-Z_]+$])
m4_pattern_allow([^PKG_CONFIG(_PATH)?$])
AC_ARG_VAR([PKG_CONFIG], [path to pkg-config utility])dnl
if test "x$ac_cv_env_PKG_CONFIG_set" != "xset"; then
	AC_PATH_TOOL([PKG_CONFIG], [pkg-config])
fi
if test -n "$PKG_CONFIG"; then
	_pkg_min_version=m4_default([$1], [0.9.0])
	AC_MSG_CHECKING([pkg-config is at least version $_pkg_min_version])
	if $PKG_CONFIG --atleast-pkgconfig-version $_pkg_min_version; then
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
		PKG_CONFIG=""
	fi
		
fi[]dnl
])# PKG_PROG_PKG_CONFIG

# PKG_CHECK_EXISTS(MODULES, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
#
# Check to see whether a particular set of modules exists.  Similar
# to PKG_CHECK_MODULES(), but does not set variables or print errors.
#
#
# Similar to PKG_CHECK_MODULES, make sure that the first instance of
# this or PKG_CHECK_MODULES is called, or make sure to call
# PKG_CHECK_EXISTS manually
# --------------------------------------------------------------
AC_DEFUN([PKG_CHECK_EXISTS],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
if test -n "$PKG_CONFIG" && \
    AC_RUN_LOG([$PKG_CONFIG --exists --print-errors "$1"]); then
  m4_ifval([$2], [$2], [:])
m4_ifvaln([$3], [else
  $3])dnl
fi])


# _PKG_CONFIG([VARIABLE], [COMMAND], [MODULES])
# ---------------------------------------------
m4_define([_PKG_CONFIG],
[if test -n "$PKG_CONFIG"; then
    if test -n "$$1"; then
        pkg_cv_[]$1="$$1"
    else
        PKG_CHECK_EXISTS([$3],
                         [pkg_cv_[]$1=`$PKG_CONFIG --[]$2 "$3" 2>/dev/null`],
			 [pkg_failed=yes])
    fi
else
	pkg_failed=untried
fi[]dnl
])# _PKG_CONFIG

# _PKG_SHORT_ERRORS_SUPPORTED
# -----------------------------
AC_DEFUN([_PKG_SHORT_ERRORS_SUPPORTED],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])
if $PKG_CONFIG --atleast-pkgconfig-version 0.20; then
        _pkg_short_errors_supported=yes
else
        _pkg_short_errors_supported=no
fi[]dnl
])# _PKG_SHORT_ERRORS_SUPPORTED


# PKG_CHECK_MODULES(VARIABLE-PREFIX, MODULES, [ACTION-IF-FOUND],
# [ACTION-IF-NOT-FOUND])
#
#
# Note that if there is a possibility the first call to
# PKG_CHECK_MODULES might not happen, you should be sure to include an
# explicit call to PKG_PROG_PKG_CONFIG in your configure.ac
#
#
# --------------------------------------------------------------
AC_DEFUN([PKG_CHECK_MODULES],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
AC_ARG_VAR([$1][_CFLAGS], [C compiler flags for $1, overriding pkg-config])dnl
AC_ARG_VAR([$1][_LIBS], [linker flags for $1, overriding pkg-config])dnl

pkg_failed=no
AC_MSG_CHECKING([for $1])

_PKG_CONFIG([$1][_CFLAGS], [cflags], [$2])
_PKG_CONFIG([$1][_LIBS], [libs], [$2])

m4_define([_PKG_TEXT], [Alternatively, you may set the environment variables $1[]_CFLAGS
and $1[]_LIBS to avoid the need to call pkg-config.
See the pkg-config man page for more details.])

if test $pkg_failed = yes; then
        _PKG_SHORT_ERRORS_SUPPORTED
        if test $_pkg_short_errors_supported = yes; then
	        $1[]_PKG_ERRORS=`$PKG_CONFIG --short-errors --errors-to-stdout --print-errors "$2"`
        else 
	        $1[]_PKG_ERRORS=`$PKG_CONFIG --errors-to-stdout --print-errors "$2"`
        fi
	# Put the nasty error message in config.log where it belongs
	echo "$$1[]_PKG_ERRORS" >&AS_MESSAGE_LOG_FD

	ifelse([$4], , [AC_MSG_ERROR(dnl
[Package requirements ($2) were not met:

$$1_PKG_ERRORS

Consider adjusting the PKG_CONFIG_PATH environment variable if you
installed software in a non-standard prefix.

_PKG_TEXT
])],
		[AC_MSG_RESULT([no])
                $4])
elif test $pkg_failed = untried; then
	ifelse([$4], , [AC_MSG_FAILURE(dnl
[The pkg-config script could not be found or is too old.  Make sure it
is in your PATH or set the PKG_CONFIG environment variable to the full
path to pkg-config.

_PKG_TEXT

To get pkg-config, see <http://www.freedesktop.org/software/pkgconfig>.])],
		[$4])
else
	$1[]_CFLAGS=$pkg_cv_[]$1[]_CFLAGS
	$1[]_LIBS=$pkg_cv_[]$1[]_LIBS
        AC_MSG_RESULT([yes])
	ifelse([$3], , :, [$3])
fi[]dnl
])# PKG_CHECK_MODULES
