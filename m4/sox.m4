dnl
dnl SOX_NAME_TAG(NAME-TAG, [BODY])
dnl
dnl Define using m4_pushdef:
dnl   N               First word of NAME-TAG
dnl   L               N sanitised for use as shell variable
dnl   T               Second word of NAME-TAG, if present, else
dnl                   L converted to upper case
dnl
dnl If BODY is provided, substitute it and m4_popdef N, L, and T.
dnl Otherwise, leave these macros defined.
dnl
AC_DEFUN([SOX_NAME_TAG], [
    m4_pushdef([nt], m4_split(m4_normalize($1)))
    m4_pushdef([N], m4_car(nt))
    m4_pushdef([L], m4_bpatsubst(N, [[^0-9A-Za-z]], [_]))
    m4_pushdef([T], m4_default(m4_argn([2], nt), m4_toupper(L)))
    m4_popdef([nt])
    m4_ifnblank([$2], [$2 m4_popdef([N], [L], [T])])
])

dnl
dnl SOX_INCLUDE(HEADER)
dnl
dnl Expands to "#include <HEADER>" followed by a newline.
dnl
AC_DEFUN([SOX_INCLUDE], [[#include <]]$1[[>
]])

dnl
dnl SOX_CHECK_HEADERS(HEADERS, [DECL], [IF-FOUND], [IF-NOT-FOUND],
dnl                   [EXTRA-HEADERS])
dnl
dnl Check for presence of headers.
dnl
dnl HEADERS        List of headers to try, stopping when one is found
dnl DECL           Optional symbol to check for
dnl IF-FOUND       Action to take if one of HEADERS is found
dnl IF-NOT-FOUND   Action to take if none of HEADERS are found
dnl EXTRA-HEADERS  Additional headers to #include
dnl
dnl Outputs:
dnl HAVE_HEADER:   Set with AC_DEFINE to 1 for the first header found
dnl
AC_DEFUN([SOX_CHECK_HEADERS], [
    sox_ch_found=no
    AC_CHECK_HEADERS([$1], [sox_ch_found=$ac_header; break], [],
        [m4_map([SOX_INCLUDE], m4_split([$5]))])
    AS_CASE([$sox_ch_found], [no], [$4], [m4_ifblank([$2], [$3],
        [AC_CHECK_DECL([$2], [$3], [$4], [SOX_INCLUDE([$sox_ch_found])])])])
])

dnl
dnl SOX_CHECK_LIB(TAG, HEADERS, LIB, FUNC, [IF-FOUND], [IF-NOT-FOUND],
dnl               [EXTRA-HEADERS], [EXTRA-LIBS])
dnl
dnl Check for presence of headers and library.
dnl
dnl Arguments:
dnl   TAG             Prefix/suffix for output variable
dnl   HEADERS         List of headers searched with SOX_CHECK_HEADERS
dnl   LIB             Name of library to check
dnl   FUNC            Name of function in library
dnl   IF-FOUND        Action to take on success
dnl   IF-NOT-FOUND    Action to take on failure
dnl   EXTRA-HEADERS   Passed to SOX_CHECK_HEADERS
dnl   EXTRA-LIBS      Additional libraries (-lLIB) and linker flags
dnl
dnl Outputs:
dnl   HAVE_TAG        AC_DEFINE to 1 if found
dnl                   Set shell variable to 'yes' on success, 'no' otherwise
dnl   TAG_LIBS        Add '-lLIB EXTRA-LIBS' to start of shell variable
dnl
AC_DEFUN([SOX_CHECK_LIB], [
    m4_pushdef([lib], [m4_default([$3], [c])])
    m4_pushdef([flags], [m4_ifnblank([$3], [-l$3 $8], [$8])])
    HAVE_[]$1=no
    SOX_CHECK_HEADERS([$2], [], [AC_CHECK_LIB(lib, [$4],
        [HAVE_[]$1=yes], [], [$8 $$1[]_LIBS])], [], [$7])
    AS_CASE([$HAVE_[]$1], [yes], [
        AC_DEFINE([HAVE_]$1, [1], [Define if $4 exists in ]flags)
        $1[]_LIBS="flags $$1[]_LIBS"
        $5], [$6])
    AC_SUBST($1[_CFLAGS])
    AC_SUBST($1[_LIBS])
    m4_popdef([lib], [flags])
])

dnl
dnl SOX_NEED_DL(VAL, DEP)
dnl
dnl Exit with an error message if VAL equals 'dyn' and dynamic loading
dnl is not available.
dnl
AC_DEFUN([SOX_NEED_DL], [
    AS_CASE([$1-$HAVE_LIBLTDL], [dyn-no],
        [AC_MSG_ERROR([dynamic loading not available, needed by $2])])
])

dnl
dnl SOX_ARG(TYPE, NAME, DESC, [TEST], [IF-YES], [IF-NO], [IF-FAIL],
dnl         [EXTRA-CHOICES], [DEFAULT])
dnl
AC_DEFUN([SOX_ARG], [SOX_NAME_TAG([$2], [
    m4_pushdef([ATU], m4_toupper($1))
    m4_pushdef([ATL], m4_tolower($1))
    m4_pushdef([optdef], m4_default([$9], [yes]))
    m4_pushdef([opts], m4_split([yes no $8]))
    m4_pushdef([opts_help], m4_bpatsubst(m4_apply([m4_join], [[/], opts]),
        \<optdef\>, m4_toupper(optdef)))
    m4_pushdef([opts_sh], m4_apply([m4_join], [[|], opts]))
    m4_indir(AC_ARG_[]ATU, [N], AS_HELP_STRING([--ATL-N], [$3 (opts_help)]))
    HAVE_[]T=${ATL[]_[]L:-optdef}
    AS_CASE([$HAVE_[]T],
        [no],       [],
        [opts_sh],  [$4],
                    [AC_MSG_ERROR([invalid value for --ATL-N])])
    AS_CASE([$ATL[]_[]L-$HAVE_[]T],
        [no-*|-no], [$6],
        [*-no],     [$7],
                    [$5])
    AM_CONDITIONAL([HAVE_]T, [test $HAVE_[]T != no])
    m4_popdef([ATU], [ATL], [optdef], [opts], [opts_help], [opts_sh])
])])

dnl
dnl SOX_ENABLE(NAME, DESC, [TEST], [IF-YES], [IF-NO], [IF-FAIL],
dnl            [EXTRA-CHOICES], [DEFAULT])
dnl
AC_DEFUN([SOX_ENABLE], [
    SOX_ARG([enable], $@)
])

dnl
dnl SOX_WITH(NAME, DESC, [TEST], [IF-YES], [IF-NO], [IF-FAIL],
dnl          [EXTRA-CHOICES], [DEFAULT])
dnl
AC_DEFUN([SOX_WITH], [
    SOX_ARG([with], $@)
])

dnl
dnl SOX_WITH_LIB(NAME, HEADERS, LIB, FUNC, [IF-FOUND], [IF-NOT-FOUND],
dnl              [INCLUDES], [EXTRA-LIBS], [EXTRA-CHOICES], [DEFAULT])
dnl
AC_DEFUN([SOX_WITH_LIB], [SOX_NAME_TAG([$1], [
    SOX_WITH([$1], [Use N],
        [AS_CASE([$HAVE_[]T], [yes|dyn], [sox_wl=$3], [sox_wl=$HAVE_[]T])
         SOX_CHECK_LIB([T], [$2], [$sox_wl], [$4], [], [], [$7], [$8])],
        [$5], [$6], [AC_MSG_FAILURE([N not found])],
        [$9 *], m4_argn([8], m4_shift2($@))) dnl BSD m4 can't count to 10
    SOX_REPORT([with_libs], [N], [$HAVE_]T)
])])

dnl
dnl SOX_DL_LIB(NAME, HEADERS, LIB, FUNC, [IF-STATIC], [IF-DL], [IF-NOT-FOUND],
dnl            [EXTRA-HEADERS], [EXTRA-LIBS])
dnl
AC_DEFUN([SOX_DL_LIB], [SOX_NAME_TAG([$1], [
    SOX_NEED_DL([$with_[]L], [--with-L=dyn])
    SOX_WITH_LIB([$1], [$2], [$3], [$4], [], [$7], [$8], [$9], [dyn])
    AS_CASE([$with_[]L-$HAVE_[]T],
         [dyn-*], [AC_DEFINE([DL_]T, 1, [Define to dlopen() ]N)
                   HAVE_[]T=dyn; $6],
         [*-yes], [$5])
])])

dnl
dnl SOX_REQUIRE1(FEATURE, TAG, [IF-FOUND])
dnl
AC_DEFUN([SOX_REQUIRE1], [
    AS_CASE([$HAVE_$1],
        [yes|dyn],  [$2_CFLAGS="$$2_CFLAGS $$1_CFLAGS"; $3])
    AS_CASE([$HAVE_$1],
        [yes],      [$2_LIBS="$$2_LIBS $$1_LIBS"])
])

dnl
dnl SOX_REQUIRE(FEATURES, TAG, [IF-NOT-FOUND])
dnl
AC_DEFUN([SOX_REQUIRE], [
    sox_req_found=no
    m4_map_args_w([$1], [SOX_REQUIRE1(], [, $2, [sox_req_found=yes])])
    AS_CASE([$sox_req_found], [yes], [], [$3])
])

dnl
dnl SOX_FMT(NAME, [TEST], [SECTION])
dnl
dnl Add an optional format with corresponding --enable flag.
dnl
dnl Arguments:
dnl   NAME            Name of format, passed to SOX_NAME_TAG setting N and T
dnl   TEST            Test for prerequisites, must set HAVE_T to 'no' if not met
dnl   SECTION         Section format belongs to, default 'formats'
dnl
dnl Outputs:
dnl   HAVE_T          Set shell variable to 'yes', 'no', or 'dyn'
dnl                   AC_DEFINE and AM_CONDITIONAL true if not 'no'
dnl   STATIC_T        AC_DEFINE and AM_CONDITIONAL true if HAVE_T = 'yes'
dnl
AC_DEFUN([SOX_FMT], [SOX_NAME_TAG([$1], [
    m4_pushdef([section], m4_default([$3], [formats]))
    SOX_NEED_DL([$with_[]L], [--enable-L=dyn])
    SOX_ENABLE([$1], [Enable N], [$2],
        [AC_DEFINE([HAVE_]T, [1], [Define if ]N[ is enabled])
         sox_[]section="$sox_[]section L"], [],
        [AC_MSG_FAILURE([N not available])],
        [dyn], [${HAVE_FORMATS:-yes}])
    AS_CASE([$HAVE_[]T],
        [yes],      [AC_DEFINE([STATIC_]T, [1], [Define if ]N[ is linked in])])
    AC_SUBST(T[_CFLAGS])
    AC_SUBST(T[_LIBS])
    AM_CONDITIONAL([STATIC_]T, [test $HAVE_[]T = yes])
    SOX_REPORT(section, [N], [$HAVE_]T)
    m4_popdef([section])
])])

dnl
dnl SOX_FMT_REQ(NAME, FEATURES, [SECTION])
dnl
AC_DEFUN([SOX_FMT_REQ], [
    SOX_FMT([$1], [SOX_REQUIRE([$2], [T], [HAVE_[]T=no])], [$3])
])

dnl
dnl SOX_FMT_HEADERS(NAME, HEADERS, [DECL], [EXTRA-HEADERS], [SECTION])
dnl
dnl Wrapper for SOX_FMT with SOX_CHECK_HEADERS as test.
dnl
AC_DEFUN([SOX_FMT_HEADERS], [
    SOX_FMT([$1],
        [SOX_CHECK_HEADERS([$2], [$3], [], [HAVE_[]T=no], [$4])], [$5])
])

dnl
dnl SOX_FMT_LIB(NAME, HEADERS, LIB, FUNC, [EXTRA-HEADERS], [EXTRA-LIBS],
dnl             [SECTION])
dnl
dnl Wrapper for SOX_FMT with SOX_CHECK_LIB as test.
dnl
AC_DEFUN([SOX_FMT_LIB], [
    SOX_FMT([$1], [
        SOX_CHECK_LIB([LIB[]T], [$2], [$3], [$4], [], [HAVE_[]T=no], [$5], [$6])
        T[]_CFLAGS=$LIB[]T[]_CFLAGS
        T[]_LIBS=$LIB[]T[]_LIBS], [$7])
])

dnl
dnl SOX_FMT_PKG(NAME, PKG)
dnl
AC_DEFUN([SOX_FMT_PKG], [
    SOX_FMT([$1], [PKG_CHECK_MODULES(T, [$2], [], [HAVE_[]T=no])])
])

dnl
dnl SOX_REPORT_SECTION(NAME, TITLE, [FILTER])
dnl
AC_DEFUN([SOX_REPORT_SECTION], [
    m4_append([sox_rep_sections], [$1], [ ])
    m4_define([sox_rep_title_$1], [$2])
    m4_define([sox_rep_filter_$1], m4_default([$3], [cat]))
])

dnl
dnl SOX_REPORT(SECTION, DESC, VAL)
dnl
AC_DEFUN([SOX_REPORT], [
    m4_append([sox_rep_text_$1], AS_HELP_STRING([$2], [$3]), m4_newline)
])

dnl
dnl SOX_REPORT_PRINT1(SECTION)
dnl
AC_DEFUN([SOX_REPORT_PRINT1], [
    echo; echo "sox_rep_title_$1"
    sox_rep_filter_$1 <<EOF
sox_rep_text_$1
EOF
])

dnl
dnl SOX_REPORT_PRINT
dnl
AC_DEFUN([SOX_REPORT_PRINT], [
    m4_map([SOX_REPORT_PRINT1], m4_split(sox_rep_sections))
])
