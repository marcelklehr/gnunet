AM_CPPFLAGS = -I$(top_srcdir)/src/include

pkgcfgdir= $(pkgdatadir)/config.d/

libexecdir= $(pkglibdir)/libexec/

pkgcfg_DATA = \
  psyc.conf


if MINGW
 WINFLAGS = -Wl,--no-undefined -Wl,--export-all-symbols
endif

if USE_COVERAGE
  AM_CFLAGS = --coverage -O0
  XLIB = -lgcov
endif

lib_LTLIBRARIES = libgnunetpsycutil.la libgnunetpsyc.la

libgnunetpsycutil_la_SOURCES = \
  psyc_util_lib.c
libgnunetpsycutil_la_LIBADD = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/env/libgnunetenv.la \
  $(GN_LIBINTL) $(XLIB)
libgnunetpsycutil_la_LDFLAGS = \
  $(GN_LIB_LDFLAGS)  $(WINFLAGS) \
  -version-info 0:0:0
libgnunetpsycutil_la_DEPENDENCIES = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/env/libgnunetenv.la

libgnunetpsyc_la_SOURCES = \
  psyc_api.c psyc.h
libgnunetpsyc_la_LIBADD = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/env/libgnunetenv.la \
  $(top_builddir)/src/psyc/libgnunetpsycutil.la \
  $(GN_LIBINTL) $(XLIB)
libgnunetpsyc_la_LDFLAGS = \
  $(GN_LIB_LDFLAGS)  $(WINFLAGS) \
  -version-info 0:0:0
libgnunetpsyc_la_DEPENDENCIES = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/env/libgnunetenv.la \
  $(top_builddir)/src/psyc/libgnunetpsycutil.la

bin_PROGRAMS =

libexec_PROGRAMS = \
 gnunet-service-psyc

gnunet_service_psyc_SOURCES = \
  gnunet-service-psyc.c
gnunet_service_psyc_LDADD = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/multicast/libgnunetmulticast.la \
  $(top_builddir)/src/psycstore/libgnunetpsycstore.la \
  $(top_builddir)/src/psyc/libgnunetpsycutil.la \
  $(GN_LIBINTL)
gnunet_service_psyc_DEPENDENCIES = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/multicast/libgnunetmulticast.la \
  $(top_builddir)/src/psycstore/libgnunetpsycstore.la \
  $(top_builddir)/src/psyc/libgnunetpsycutil.la
gnunet_service_psyc_CFLAGS = $(AM_CFLAGS)


if HAVE_TESTING
check_PROGRAMS = \
 test_psyc
endif

if ENABLE_TEST_RUN
AM_TESTS_ENVIRONMENT=export GNUNET_PREFIX=$${GNUNET_PREFIX:-@libdir@};export PATH=$${GNUNET_PREFIX:-@prefix@}/bin:$$PATH;
TESTS = $(check_PROGRAMS)
endif

test_psyc_SOURCES = \
 test_psyc.c
test_psyc_LDADD = \
  libgnunetpsyc.la \
  $(top_builddir)/src/testing/libgnunettesting.la \
  $(top_builddir)/src/env/libgnunetenv.la \
  $(top_builddir)/src/util/libgnunetutil.la
test_psyc_DEPENDENCIES = \
  libgnunetpsyc.la \
  $(top_builddir)/src/testing/libgnunettesting.la \
  $(top_builddir)/src/env/libgnunetenv.la \
  $(top_builddir)/src/util/libgnunetutil.la

EXTRA_DIST = \
  test_psyc.conf
