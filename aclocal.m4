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
dnl @version $Id: aclocal.m4,v 1.2 2001/12/03 16:11:52 cbagwell Exp $
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
[for ac_size in 4 8 1 2 16 $2 ; do # List sizes in rough order of prevalence.
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

dnl @synopsis AC_CHECK_TYPEDEF_(TYPEDEF, HEADER [, ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND ]])
dnl
dnl check if the given typedef-name is recognized as a type. The trick is
dnl to use a sizeof(TYPEDEF) and see if the compiler is happy with that.
dnl
dnl this can be thought of as a mixture of AC_CHECK_TYPE(TYPEDEF,DEFAULT)
dnl and AC_CHECK_LIB(LIBRARY,FUNCTION,ACTION-IF-FOUND,ACTION-IF-NOT-FOUND)
dnl
dnl a convenience macro AC_CHECK_TYPEDEF_ is provided that will not emit
dnl any message to the user - it just executes one of the actions.
dnl
dnl @version $Id: aclocal.m4,v 1.2 2001/12/03 16:11:52 cbagwell Exp $
dnl @author  Guido Draheim <guidod@gmx.de>

AC_DEFUN(AC_CHECK_TYPEDEF_,
[dnl
ac_lib_var=`echo $1['_']$2 | sed 'y%./+-%__p_%'`
AC_CACHE_VAL(ac_cv_lib_$ac_lib_var,
[ eval "ac_cv_type_$ac_lib_var='not-found'"
  ac_cv_check_typedef_header=`echo ifelse([$2], , stddef.h, $2)`
  AC_TRY_COMPILE( [#include <$ac_cv_check_typedef_header>], 
        [int x = sizeof($1); x = x;],
        eval "ac_cv_type_$ac_lib_var=yes" ,
        eval "ac_cv_type_$ac_lib_var=no" )
  if test `eval echo '$ac_cv_type_'$ac_lib_var` = "no" ; then 
     ifelse([$4], , :, $4)
  else 
     ifelse([$3], , :, $3) 
  fi
])])

dnl AC_CHECK_TYPEDEF(TYPEDEF, HEADER [, ACTION-IF-FOUND,
dnl    [, ACTION-IF-NOT-FOUND ]])
AC_DEFUN(AC_CHECK_TYPEDEF,
[dnl
 AC_MSG_CHECKING([for $1 in $2])
 AC_CHECK_TYPEDEF_($1,$2,AC_MSG_RESULT(yes),AC_MSG_RESULT(no))dnl
])

dnl @synopsis AC_NEED_STDINT_H [( HEADER-TO-GENERATE [, HEDERS-TO-CHECK])]
dnl
dnl the "ISO C9X: 7.18 Integer types <stdint.h>" section requires the
dnl existence of an include file <stdint.h> that defines a set of 
dnl typedefs, especially uint8_t,int32_t,uintptr_t.
dnl Many older installations will not provide this file, but some will
dnl have the very same definitions in <inttypes.h>. In other enviroments
dnl we can use the inet-types in <sys/types.h> which would define the
dnl typedefs int8_t and u_int8_t respectivly.
dnl
dnl This macros will create a local "stdint.h" if it cannot find the
dnl global <stdint.h> (or it will create the headerfile given as an argument).
dnl In many cases that file will just have a singular "#include <inttypes.h>"
dnl statement, while in other environments it will provide the set of basic
dnl stdint's defined: 
dnl int8_t,uint8_t,int16_t,uint16_t,int32_t,uint32_t,intptr_t,uintptr_t
dnl int_least32_t.. int_fast32_t.. intmax_t
dnl which may or may not rely on the definitions of other files,
dnl or using the AC_COMPILE_CHECK_SIZEOF macro to determine the actual
dnl sizeof each type.
dnl
dnl if your header files require the stdint-types you will want to create an
dnl installable file package-stdint.h that all your other installable header
dnl may include. So if you have a library package named "mylib", just use
dnl      AC_NEED_STDINT(zziplib-stdint.h) 
dnl in configure.in and go to install that very header file in Makefile.am
dnl along with the other headers (mylib.h) - and the mylib-specific headers
dnl can simply use "#include <mylib-stdint.h>" to obtain the stdint-types.
dnl
dnl Remember, if the system already had a valid <stdint.h>, the generated
dnl file will include it directly. No need for fuzzy HAVE_STDINT_H things...
dnl
dnl @version $Id: aclocal.m4,v 1.2 2001/12/03 16:11:52 cbagwell Exp $
dnl @author  Guido Draheim <guidod@gmx.de>       STATUS: used on new platforms

AC_DEFUN([AC_NEED_STDINT_H],
[AC_MSG_CHECKING([for stdint-types])
 ac_cv_header_stdint="no-file"
 ac_cv_header_stdint_u="no-file"
 for i in $1 inttypes.h sys/inttypes.h sys/int_types.h stdint.h ; do
   AC_CHECK_TYPEDEF_(uint32_t, $i, [ac_cv_header_stdint=$i])
 done
 for i in $1 sys/types.h inttypes.h sys/inttypes.h sys/int_types.h ; do
   AC_CHECK_TYPEDEF_(u_int32_t, $i, [ac_cv_header_stdint_u=$i])
 done
 dnl debugging: __AC_MSG( !$ac_cv_header_stdint!$ac_cv_header_stdint_u! ...)

 ac_stdint_h=`echo ifelse($1, , stdint.h, $1)`
 if test "$ac_cv_header_stdint" != "no-file" ; then
   if test "$ac_cv_header_stdint" != "$ac_stdint_h" ; then
     AC_MSG_RESULT(found in $ac_cv_header_stdint)
     echo "#include <$ac_cv_header_stdint>" >$ac_stdint_h
     AC_MSG_RESULT(creating $ac_stdint_h - (just to include  $ac_cv_header_stdint) )
   else
     AC_MSG_RESULT(found in $ac_stdint_h)
   fi
   ac_cv_header_stdint_generated=false
 elif test "$ac_cv_header_stdint_u" != "no-file" ; then
   AC_MSG_RESULT(found u_types in $ac_cv_header_stdint_u)
   if test $ac_cv_header_stdint = "$ac_stdint_h" ; then
     AC_MSG_RESULT(creating $ac_stdint_h - includes $ac_cv_header_stdint, expect problems!)
   else
     AC_MSG_RESULT(creating $ac_stdint_h - (include inet-types in $ac_cv_header_stdint_u and re-typedef))
   fi
   cat >$ac_stdint_h <<EOF
#ifndef __AC_STDINT_H
#define __AC_STDINT_H 1
#include <stddef.h>
#include <$ac_cv_header_stdint_u>
/* int8_t int16_t int32_t defined by inet code */
typedef u_int8_t uint8_t;
typedef u_int16_t uint16_t;
typedef u_int32_t uint32_t;
/* it's a networkable system, but without any stdint.h */
/* hence it's an older 32-bit system... (a wild guess that seems to work) */
typedef u_int32_t uintptr_t;
typedef   int32_t  intptr_t;
EOF
   ac_cv_header_stdint_generated=true
 else
   AC_MSG_RESULT(not found, need to guess the types now... )
   AC_COMPILE_CHECK_SIZEOF(long, 32)
   AC_COMPILE_CHECK_SIZEOF(void*, 32)
   AC_MSG_RESULT( creating $ac_stdint_h - using detected values for sizeof long and sizeof void* )
   cat >$ac_stdint_h <<EOF

#ifndef __AC_STDINT_H
#define __AC_STDINT_H 1
/* ISO C 9X: 7.18 Integer types <stdint.h> */

#define __int8_t_defined  
typedef   signed char    int8_t;
typedef unsigned char   uint8_t;
typedef   signed short  int16_t;
typedef unsigned short uint16_t;
EOF

   if test "$ac_cv_sizeof_long" = "64" ; then
     cat >>$ac_stdint_h <<EOF

typedef   signed int    int32_t;
typedef unsigned int   uint32_t;
typedef   signed long   int64_t;
typedef unsigned long  uint64_t;
#define  int64_t  int64_t
#define uint64_t uint64_t
EOF

   else
    cat >>$ac_stdint_h <<EOF

typedef   signed long   int32_t;
typedef unsigned long  uint32_t;
EOF

   fi
   if test "$ac_cv_sizeof_long" != "$ac_cv_sizeof_voidp" ; then
     cat >>$ac_stdint_h <<EOF

typedef   signed int   intptr_t;
typedef unsigned int  uintptr_t;
EOF
   else
     cat >>$ac_stdint_h <<EOF

typedef   signed long   intptr_t;
typedef unsigned long  uintptr_t;
EOF
     ac_cv_header_stdint_generated=true
   fi
 fi   

 if "$ac_cv_header_stdint_generated" ; then
     cat >>$ac_stdint_h <<EOF

typedef  int8_t    int_least8_t;
typedef  int16_t   int_least16_t;
typedef  int32_t   int_least32_t;

typedef uint8_t   uint_least8_t;
typedef uint16_t  uint_least16_t;
typedef uint32_t  uint_least32_t;

typedef  int8_t    int_fast8_t; 
typedef  int32_t   int_fast16_t;
typedef  int32_t   int_fast32_t;

typedef uint8_t   uint_fast8_t; 
typedef uint32_t  uint_fast16_t;
typedef uint32_t  uint_fast32_t;

typedef long int       intmax_t;
typedef unsigned long uintmax_t;
#endif
EOF
  fi dnl
])
