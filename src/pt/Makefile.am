AM_CPPFLAGS = -I$(top_srcdir)/src/include

if MINGW
  WINFLAGS = -Wl,--no-undefined -Wl,--export-all-symbols
endif

if USE_COVERAGE
  AM_CFLAGS = --coverage -O0
endif

pkgcfgdir= $(pkgdatadir)/config.d/

libexecdir= $(pkglibdir)/libexec/

plugindir = $(libdir)/gnunet

dist_pkgcfg_DATA = \
  pt.conf

libexec_PROGRAMS = \
  gnunet-daemon-pt

gnunet_daemon_pt_SOURCES = \
 gnunet-daemon-pt.c
gnunet_daemon_pt_LDADD = \
  $(top_builddir)/src/vpn/libgnunetvpn.la \
  $(top_builddir)/src/cadet/libgnunetcadet.la \
  $(top_builddir)/src/dht/libgnunetdht.la \
  $(top_builddir)/src/dns/libgnunetdns.la \
  $(top_builddir)/src/dns/libgnunetdnsparser.la \
  $(top_builddir)/src/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(GN_LIBINTL)

if HAVE_TESTING
if HAVE_MHD
if HAVE_LIBGNURL
if LINUX
 VPN_TEST = \
 test_gnunet_vpn-4_to_6 \
 test_gnunet_vpn-6_to_4 \
 test_gnunet_vpn-6_over \
 test_gnunet_vpn-4_over \
 test_gns_vpn
endif
if MINGW
 VPN_TEST = \
 test_gnunet_vpn-4_to_6 \
 test_gnunet_vpn-6_to_4 \
 test_gnunet_vpn-6_over \
 test_gnunet_vpn-4_over
endif
endif
endif
endif

check_PROGRAMS = $(VPN_TEST)

if ENABLE_TEST_RUN
AM_TESTS_ENVIRONMENT=export GNUNET_PREFIX=$${GNUNET_PREFIX:-@libdir@};export PATH=$${GNUNET_PREFIX:-@prefix@}/bin:$$PATH;
TESTS = $(check_PROGRAMS)
endif

EXTRA_DIST = \
 test_gnunet_vpn.conf \
 test_gns_vpn.conf



test_gns_vpn_SOURCES = \
 test_gns_vpn.c
test_gns_vpn_LDADD = -lmicrohttpd @LIBGNURL@ \
 $(top_builddir)/src/namestore/libgnunetnamestore.la \
 $(top_builddir)/src/gnsrecord/libgnunetgnsrecord.la \
 $(top_builddir)/src/testing/libgnunettesting.la \
 $(top_builddir)/src/util/libgnunetutil.la
test_gns_vpn_CPPFLAGS = \
 @LIBGNURL_CPPFLAGS@ $(AM_CPPFLAGS)

test_gnunet_vpn_4_over_SOURCES = \
 test_gnunet_vpn.c
test_gnunet_vpn_4_over_LDADD = -lmicrohttpd @LIBGNURL@ \
 $(top_builddir)/src/vpn/libgnunetvpn.la \
 $(top_builddir)/src/testing/libgnunettesting.la \
 $(top_builddir)/src/util/libgnunetutil.la
test_gnunet_vpn_4_over_CPPFLAGS = \
 @LIBGNURL_CPPFLAGS@ $(AM_CPPFLAGS)

test_gnunet_vpn_6_over_SOURCES = \
 test_gnunet_vpn.c
test_gnunet_vpn_6_over_LDADD = -lmicrohttpd @LIBGNURL@ \
 $(top_builddir)/src/vpn/libgnunetvpn.la \
 $(top_builddir)/src/testing/libgnunettesting.la \
 $(top_builddir)/src/util/libgnunetutil.la
test_gnunet_vpn_6_over_CPPFLAGS = \
 @LIBGNURL_CPPFLAGS@ $(AM_CPPFLAGS)

test_gnunet_vpn_4_to_6_SOURCES = \
 test_gnunet_vpn.c
test_gnunet_vpn_4_to_6_LDADD = -lmicrohttpd @LIBGNURL@ \
 $(top_builddir)/src/vpn/libgnunetvpn.la \
 $(top_builddir)/src/testing/libgnunettesting.la \
 $(top_builddir)/src/util/libgnunetutil.la
test_gnunet_vpn_4_to_6_CPPFLAGS = \
 @LIBGNURL_CPPFLAGS@ $(AM_CPPFLAGS)

test_gnunet_vpn_6_to_4_SOURCES = \
 test_gnunet_vpn.c
test_gnunet_vpn_6_to_4_LDADD = -lmicrohttpd @LIBGNURL@ \
 $(top_builddir)/src/vpn/libgnunetvpn.la \
 $(top_builddir)/src/testing/libgnunettesting.la \
 $(top_builddir)/src/util/libgnunetutil.la
test_gnunet_vpn_6_to_4_CPPFLAGS = \
 @LIBGNURL_CPPFLAGS@ $(AM_CPPFLAGS)


