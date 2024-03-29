AM_CPPFLAGS = -I$(top_srcdir)/src/include -I$(top_builddir)/src/include

plugindir = $(libdir)/gnunet

libexecdir= $(pkglibdir)/libexec/

pkgcfgdir= $(pkgdatadir)/config.d/

dist_pkgcfg_DATA = \
  util.conf

pkgcfg_DATA = \
  resolver.conf

if MINGW
noinst_LTLIBRARIES = \
  libgnunetutilwin.la
libgnunetutilwin_la_SOURCES = \
  win.c \
  winproc.c
libgnunetutilwin_la_LDFLAGS = \
  -no-undefined -Wl,--export-all-symbols
libgnunetutilwin_la_LIBADD = \
  -lshell32 -liconv -lstdc++ \
  -lcomdlg32 -lgdi32 -liphlpapi
WINLIB = libgnunetutilwin.la
W32CAT = w32cat
W32CONSOLEHELPER = gnunet-helper-w32-console
endif

if !MINGW
 SERVER_CLIENT_UNIX = test_server_with_client_unix
endif

if USE_COVERAGE
  AM_CFLAGS = --coverage -O0
  XLIB = -lgcov
endif

w32cat_SOURCES = w32cat.c

gnunet_helper_w32_console_SOURCES = \
  gnunet-helper-w32-console.c \
  gnunet-helper-w32-console.h
gnunet_helper_w32_console_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la
gnunet_helper_w32_console_DEPENDENCIES = \
 libgnunetutil.la

noinst_PROGRAMS = \
 gnunet-config-diff \
 $(W32CAT) \
 test_common_logging_dummy


gnunet_config_diff_SOURCES = \
 gnunet-config-diff.c
gnunet_config_diff_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la
gnunet_config_diff_DEPENDENCIES = \
 libgnunetutil.la

test_common_logging_dummy_SOURCES = \
 test_common_logging_dummy.c
test_common_logging_dummy_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la
test_common_logging_dummy_DEPENDENCIES = \
 libgnunetutil.la

lib_LTLIBRARIES = libgnunetutil.la

libgnunetutil_la_SOURCES = \
  bandwidth.c \
  bio.c \
  client.c \
  client_manager.c \
  common_allocation.c \
  common_endian.c \
  common_logging.c \
  configuration.c \
  connection.c \
  container_bloomfilter.c \
  container_heap.c \
  container_meta_data.c \
  container_multihashmap.c \
  container_multipeermap.c \
  container_multihashmap32.c \
  container_slist.c \
  crypto_symmetric.c \
  crypto_crc.c \
  crypto_ecc.c \
  crypto_hash.c \
  crypto_hkdf.c \
  crypto_kdf.c \
  crypto_mpi.c \
  crypto_paillier.c \
  crypto_random.c \
  disk.c \
  disk.h \
  getopt.c \
  getopt_helpers.c \
  helper.c \
  load.c \
  mq.c \
  network.c \
  os_installation.c \
  os_network.c \
  os_priority.c \
  peer.c \
  plugin.c \
  program.c \
  resolver_api.c resolver.h \
  scheduler.c \
  server.c \
  server_mst.c \
  server_nc.c \
  server_tc.c \
  service.c \
  signal.c \
  strings.c \
  time.c \
  speedup.c speedup.h

libgnunetutil_la_LIBADD = \
  $(GCLIBADD) $(WINLIB) \
  $(LIBGCRYPT_LIBS) \
  $(LTLIBICONV) \
  $(LTLIBINTL) \
  -lltdl $(Z_LIBS) -lunistring $(XLIB)

libgnunetutil_la_LDFLAGS = \
  $(GN_LIB_LDFLAGS) \
  -version-info 12:0:0

if HAVE_TESTING
  GNUNET_ECC = gnunet-ecc
  GNUNET_SCRYPT = gnunet-scrypt
endif


libexec_PROGRAMS = \
 gnunet-service-resolver \
 $(W32CONSOLEHELPER)

bin_SCRIPTS =\
 gnunet-qr

bin_PROGRAMS = \
 gnunet-resolver \
 gnunet-config \
 $(GNUNET_ECC) \
 $(GNUNET_SCRYPT) \
 gnunet-uri

do_subst = $(SED) -e 's,[@]PYTHON[@],$(PYTHON),g'

gnunet-qr: gnunet-qr.py.in Makefile
	$(do_subst) < gnunet-qr.py.in > gnunet-qr
	chmod +x gnunet-qr

gnunet_service_resolver_SOURCES = \
 gnunet-service-resolver.c
gnunet_service_resolver_LDADD = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(GN_LIBINTL)
gnunet_service_resolver_DEPENDENCIES = \
  libgnunetutil.la


gnunet_resolver_SOURCES = \
 gnunet-resolver.c
gnunet_resolver_LDADD = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(GN_LIBINTL)
gnunet_resolver_DEPENDENCIES = \
  libgnunetutil.la


gnunet_ecc_SOURCES = \
 gnunet-ecc.c
gnunet_ecc_LDADD = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(GN_LIBINTL) -lgcrypt
gnunet_ecc_DEPENDENCIES = \
  libgnunetutil.la

gnunet_scrypt_SOURCES = \
 gnunet-scrypt.c
gnunet_scrypt_LDADD = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(GN_LIBINTL) -lgcrypt
gnunet_scrypt_DEPENDENCIES = \
  libgnunetutil.la


gnunet_config_SOURCES = \
 gnunet-config.c
gnunet_config_LDADD = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(GN_LIBINTL)
gnunet_config_DEPENDENCIES = \
  libgnunetutil.la


gnunet_uri_SOURCES = \
 gnunet-uri.c
gnunet_uri_LDADD = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(GN_LIBINTL)
gnunet_uri_DEPENDENCIES = \
  libgnunetutil.la

plugin_LTLIBRARIES = \
  libgnunet_plugin_test.la

libgnunet_plugin_test_la_SOURCES = \
  test_plugin_plug.c
libgnunet_plugin_test_la_LDFLAGS = \
 $(GN_PLUGIN_LDFLAGS)

if HAVE_BENCHMARKS
 BENCHMARKS = \
  perf_crypto_hash \
  perf_crypto_symmetric \
  perf_malloc
endif

check_PROGRAMS = \
 test_bio \
 test_client \
 test_common_allocation \
 test_common_endian \
 test_common_logging \
 test_configuration \
 test_container_bloomfilter \
 test_container_meta_data \
 test_container_multihashmap \
 test_container_multihashmap32 \
 test_container_multipeermap \
 test_container_heap \
 test_container_slist \
 test_crypto_symmetric \
 test_crypto_crc \
 test_crypto_ecdsa \
 test_crypto_eddsa \
 test_crypto_ecdhe \
 test_crypto_hash \
 test_crypto_hkdf \
 test_crypto_paillier \
 test_crypto_random \
 test_disk \
 test_getopt \
 test_connection \
 test_connection_addressing \
 test_connection_receive_cancel \
 test_connection_timeout \
 test_connection_timeout_no_connect \
 test_connection_transmit_cancel \
 test_mq \
 test_mq_client \
 test_os_network \
 test_peer \
 test_plugin \
 test_program \
 test_resolver_api \
 test_scheduler \
 test_scheduler_delay \
 test_server_mst_interrupt \
 test_server \
 test_server_disconnect \
 test_server_with_client \
 $(SERVER_CLIENT_UNIX) \
 test_service \
 test_strings \
 test_strings_to_data \
 test_time \
 test_speedup \
 $(BENCHMARKS) \
 test_os_start_process \
 test_common_logging_runtime_loglevels

if ENABLE_TEST_RUN
AM_TESTS_ENVIRONMENT=export GNUNET_PREFIX=$${GNUNET_PREFIX:-@libdir@};export PATH=$${GNUNET_PREFIX:-@prefix@}/bin:$$PATH;
TESTS = $(check_PROGRAMS)
endif

test_bio_SOURCES = \
 test_bio.c
test_bio_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la


test_os_start_process_SOURCES = \
 test_os_start_process.c
test_os_start_process_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la
test_os_start_process_DEPENDENCIES = \
 $(WINCAT)

test_client_SOURCES = \
 test_client.c
test_client_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_common_allocation_SOURCES = \
 test_common_allocation.c
test_common_allocation_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_common_endian_SOURCES = \
 test_common_endian.c
test_common_endian_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_common_logging_SOURCES = \
 test_common_logging.c
test_common_logging_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_common_logging_runtime_loglevels_SOURCES = \
 test_common_logging_runtime_loglevels.c
test_common_logging_runtime_loglevels_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_configuration_SOURCES = \
 test_configuration.c
test_configuration_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_container_bloomfilter_SOURCES = \
 test_container_bloomfilter.c
test_container_bloomfilter_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_container_meta_data_SOURCES = \
 test_container_meta_data.c
test_container_meta_data_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la -lextractor

test_container_multihashmap_SOURCES = \
 test_container_multihashmap.c
test_container_multihashmap_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_container_multihashmap32_SOURCES = \
 test_container_multihashmap32.c
test_container_multihashmap32_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_container_multipeermap_SOURCES = \
 test_container_multipeermap.c
test_container_multipeermap_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_container_heap_SOURCES = \
 test_container_heap.c
test_container_heap_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_container_slist_SOURCES = \
 test_container_slist.c
test_container_slist_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_crypto_symmetric_SOURCES = \
 test_crypto_symmetric.c
test_crypto_symmetric_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_crypto_crc_SOURCES = \
 test_crypto_crc.c
test_crypto_crc_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_crypto_ecdsa_SOURCES = \
 test_crypto_ecdsa.c
test_crypto_ecdsa_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(LIBGCRYPT_LIBS)

test_crypto_eddsa_SOURCES = \
 test_crypto_eddsa.c
test_crypto_eddsa_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(LIBGCRYPT_LIBS)

test_crypto_ecdhe_SOURCES = \
 test_crypto_ecdhe.c
test_crypto_ecdhe_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(LIBGCRYPT_LIBS)

test_crypto_hash_SOURCES = \
 test_crypto_hash.c
test_crypto_hash_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_crypto_hkdf_SOURCES = \
 test_crypto_hkdf.c
test_crypto_hkdf_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_crypto_paillier_SOURCES = \
 test_crypto_paillier.c
test_crypto_paillier_LDADD = \
 -lgcrypt \
 $(top_builddir)/src/util/libgnunetutil.la

test_crypto_random_SOURCES = \
 test_crypto_random.c
test_crypto_random_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_disk_SOURCES = \
 test_disk.c
test_disk_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_getopt_SOURCES = \
 test_getopt.c
test_getopt_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_connection_SOURCES = \
 test_connection.c
test_connection_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_connection_addressing_SOURCES = \
 test_connection_addressing.c
test_connection_addressing_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_connection_receive_cancel_SOURCES = \
 test_connection_receive_cancel.c
test_connection_receive_cancel_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_connection_timeout_SOURCES = \
 test_connection_timeout.c
test_connection_timeout_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_connection_timeout_no_connect_SOURCES = \
 test_connection_timeout_no_connect.c
test_connection_timeout_no_connect_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_connection_transmit_cancel_SOURCES = \
 test_connection_transmit_cancel.c
test_connection_transmit_cancel_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_mq_SOURCES = \
 test_mq.c
test_mq_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_mq_client_SOURCES = \
 test_mq_client.c
test_mq_client_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_os_network_SOURCES = \
 test_os_network.c
test_os_network_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_peer_SOURCES = \
 test_peer.c
test_peer_LDADD = \
$(top_builddir)/src/util/libgnunetutil.la -lgcrypt

test_plugin_SOURCES = \
 test_plugin.c
test_plugin_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_program_SOURCES = \
 test_program.c
test_program_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_resolver_api_SOURCES = \
 test_resolver_api.c
test_resolver_api_LDADD = \
  $(top_builddir)/src/util/libgnunetutil.la

test_scheduler_SOURCES = \
 test_scheduler.c
test_scheduler_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_scheduler_delay_SOURCES = \
 test_scheduler_delay.c
test_scheduler_delay_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_server_mst_interrupt_SOURCES = \
 test_server_mst_interrupt.c
test_server_mst_interrupt_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_server_SOURCES = \
 test_server.c
test_server_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_server_disconnect_SOURCES = \
 test_server_disconnect.c
test_server_disconnect_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_server_with_client_SOURCES = \
 test_server_with_client.c
test_server_with_client_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_server_with_client_unix_SOURCES = \
 test_server_with_client_unix.c
test_server_with_client_unix_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la


test_service_SOURCES = \
 test_service.c
test_service_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_strings_SOURCES = \
 test_strings.c
test_strings_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la


test_strings_to_data_SOURCES = \
 test_strings_to_data.c
test_strings_to_data_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la


test_time_SOURCES = \
 test_time.c
test_time_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

test_speedup_SOURCES = \
 test_speedup.c
test_speedup_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

perf_crypto_hash_SOURCES = \
 perf_crypto_hash.c
perf_crypto_hash_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

perf_crypto_symmetric_SOURCES = \
 perf_crypto_symmetric.c
perf_crypto_symmetric_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la

perf_malloc_SOURCES = \
 perf_malloc.c
perf_malloc_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la


EXTRA_DIST = \
  test_configuration_data.conf \
  test_program_data.conf \
  test_resolver_api_data.conf \
  test_service_data.conf \
  test_speedup_data.conf \
  gnunet-qr.py.in
