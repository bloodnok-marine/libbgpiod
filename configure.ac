/# ----------
#  configure.ac
# 
#       Autoconf for libbgpiod
# 
#       Copyright (c) 2023 Marc Munro
#       Fileset: libbgpiod - basic/bloodnok gpio device library
#       Author:  Marc Munro
# 	License: GPL-3.0
#  
#  ----------
#

# Must init the autoconf setup
# The first parameter is project name
# second is version number
# third is bug report address: TODO: add this
#
AC_INIT([libbgpiod], [0.3.1])

# Check for source dir existance.
#
AC_CONFIG_SRCDIR([lib/bgpiod.h])
AC_SUBST([VERSIONDATE], ["November 2023"])
AC_SUBST([AUTHOR], ["Marc Munro <marc@bloodnok.com>"])

# Should we want to install by default in /usr/bin rather /usr/local/bin
#

# Checks for programs.
AC_PROG_CC

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_TYPE_SIGNAL

AC_MSG_CHECKING(whether to enable debugging)
debug_default="no"
AC_ARG_ENABLE(debug, [  --enable-debug=[no/yes] turn on debugging
                       [default=$debug_default]],, enable_debug=$debug_default)

dnl Yes, DEBUG symbol will be defined
if test "x$enable_debug" = "xyes"; then
        AC_SUBST(DEBUG, "yes")
        AC_MSG_RESULT(yes)
else
        AC_SUBST(DEBUG)
        AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING(whether to enable c assertions)
cassert_default="no"
AC_ARG_ENABLE(cassert, [  --enable-cassert=[no/yes] turn on c assertions
                  [default=cassert_default]],, enable_cassert=$cassert_default)

dnl Yes, CASSERT symbol will be defined
if test "x$enable_cassert" = "xyes"; then
        AC_SUBST(CASSERT, "yes")
        AC_MSG_RESULT(yes)
else
        AC_SUBST(CASSERT)
        AC_MSG_RESULT(no)
fi


# Find doxygen and helpers
AC_CHECK_PROGS([DOXYGEN], [doxygen])
if test -z "$DOXYGEN"; then
  AC_MSG_WARN([Doxygen not found - continue without Doxygen support])
else
  AC_CHECK_PROGS([DOT], [dot])
  if test -z "$DOT"; then
    AC_MSG_ERROR([Doxygen needs dot, please install dot (available in graphviz)])
  fi
  AC_CHECK_PROGS([PDFLATEX], [pdflatex])
  if test -z "$PDFLATEX"; then
    AC_MSG_ERROR([Doxygen needs pdflatex program, it is part of TeX http://www.tug.org/texlive/acquire-netinstall.html])
  fi
  AC_CHECK_PROGS([PANDOC], [pandoc])
  if test -z "$PANDOC"; then
    AC_MSG_ERROR([Document build needs pandoc program, please install pandoc])
  fi
fi

AC_CHECK_PROGS([HELP2MAN], [help2man])
if test -z "$HELP2MAN"; then
  AC_MSG_WARN([help2man not found - continue without help2man support])
fi


AM_CONDITIONAL([HAVE_DOXYGEN],
  [test -n "${DOXYGEN}" && test -n "${DOT}" && test -n "${PDFLATEX}"])

AC_CHECK_PROGS([PANDOC], [pandoc])
AM_CONDITIONAL([HAVE_PANDOC], [test -n "${PANDOC}"])

#AC_SUBST([HOST], [AC_CANONICAL_HOST])
AC_SUBST([TARNAME], [AC_PACKAGE_TARNAME])
AC_SUBST([VERSION], [AC_PACKAGE_VERSION])

# Tells automake to create Makefile.global
AC_CONFIG_FILES([Makefile.global])
AC_CONFIG_FILES([tools/bgpiotools.h])

AM_COND_IF([HAVE_DOXYGEN],
  [AC_CONFIG_FILES([docs/Doxyfile])])


# Generate the output
AC_OUTPUT
