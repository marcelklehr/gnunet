/*
     This file is part of GNUnet.
     (C) 2009 Christian Grothoff (and other contributing authors)

     GNUnet is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 3, or (at your
     option) any later version.

     GNUnet is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with GNUnet; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
*/
/**
 * @file gns/test_gns_max_queries.c
 * @brief base testcase for testing GNS background queries
 * in particular query replacement and clean shutdown
 */
#include "platform.h"
#include "gnunet_testing_lib.h"
#include "gnunet_core_service.h"
#include "block_dns.h"
#include "gns.h"
#include "gnunet_signatures.h"
#include "gnunet_namestore_service.h"
#include "gnunet_dnsparser_lib.h"
#include "gnunet_gns_service.h"


/* Timeout for entire testcase */
#define TIMEOUT GNUNET_TIME_relative_multiply(GNUNET_TIME_UNIT_SECONDS, 20)

/* test records to resolve */
#define TEST_DOMAIN "www.gads"
#define TEST_DOMAIN_NACK "doesnotexist.bob.gads"
#define TEST_IP "127.0.0.1"
#define TEST_RECORD_NAME "www"
#define TEST_ADDITIONAL_LOOKUPS 5
#define TEST_AUTHORITY_NAME "bob"

#define KEYFILE_BOB "../namestore/zonefiles/HGU0A0VCU334DN7F2I9UIUMVQMM7JMSD142LIMNUGTTV9R0CF4EG.zkey"

/* Task handle to use to schedule test failure */
static GNUNET_SCHEDULER_TaskIdentifier die_task;

/* Global return value (0 for success, anything else for failure) */
static int ok;

static struct GNUNET_NAMESTORE_Handle *namestore_handle;

static struct GNUNET_GNS_Handle *gns_handle;

static const struct GNUNET_CONFIGURATION_Handle *cfg;

static unsigned long long max_parallel_lookups;

static struct GNUNET_GNS_LookupRequest **requests;

static unsigned int num_requests;


/**
 * Check if the get_handle is being used, if so stop the request.  Either
 * way, schedule the end_badly_cont function which actually shuts down the
 * test.
 */
static void
end_badly (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  unsigned int i;

  for (i=0;i<num_requests;i++)
    if (NULL != requests[i])
      GNUNET_GNS_cancel_lookup_request (requests[i]);
  die_task = GNUNET_SCHEDULER_NO_TASK;
  if (NULL != gns_handle)
  {
    GNUNET_GNS_disconnect(gns_handle);
    gns_handle = NULL;
  }
  if (NULL != namestore_handle)
  {
    GNUNET_NAMESTORE_disconnect (namestore_handle);
    namestore_handle = NULL;
  }
  GNUNET_break (0);
  GNUNET_SCHEDULER_shutdown ();
  GNUNET_free (requests);
  ok = 1;
}


static void 
shutdown_task (void *cls,
	       const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  unsigned int i;

  if (GNUNET_SCHEDULER_NO_TASK != die_task)
  {
      GNUNET_SCHEDULER_cancel (die_task);
      die_task = GNUNET_SCHEDULER_NO_TASK;
  }
  for (i=0;i<num_requests;i++)
    if (NULL != requests[i])
      GNUNET_GNS_cancel_lookup_request (requests[i]);
  if (NULL != gns_handle)
  {
    GNUNET_GNS_disconnect (gns_handle);
    gns_handle = NULL;
  }
  if (NULL != namestore_handle)
  {
    GNUNET_NAMESTORE_disconnect (namestore_handle);
    namestore_handle = NULL;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_INFO, "Shutting down peer!\n");
  GNUNET_SCHEDULER_shutdown ();
  GNUNET_free (requests);
}


static void
on_lookup_result_dummy (void *cls, uint32_t rd_count,
			const struct GNUNET_NAMESTORE_RecordData *rd)
{
  struct GNUNET_GNS_LookupRequest **request = cls;
  static int replies = 0;

  *request = NULL;
  if (GNUNET_SCHEDULER_NO_TASK != die_task)
  {
      GNUNET_SCHEDULER_cancel (die_task);
      die_task = GNUNET_SCHEDULER_add_delayed (TIMEOUT, &end_badly, NULL);
  }
  if (rd_count != 0)
  {
    GNUNET_log(GNUNET_ERROR_TYPE_ERROR,
               "Got %d results from dummy lookup! Wanted: 0\n",
               rd_count);
    ok = -1;
  }
  replies++;
  fprintf (stderr, ".");
}


static void
on_lookup_result (void *cls, uint32_t rd_count,
		  const struct GNUNET_NAMESTORE_RecordData *rd)
{
  struct in_addr a;
  int i;
  char* addr;
  
  requests[max_parallel_lookups + TEST_ADDITIONAL_LOOKUPS] = NULL;
  if (GNUNET_SCHEDULER_NO_TASK != die_task)
  {
      GNUNET_SCHEDULER_cancel (die_task);
      die_task = GNUNET_SCHEDULER_NO_TASK;
  }

  GNUNET_NAMESTORE_disconnect (namestore_handle);
  namestore_handle = NULL;
  fprintf (stderr, "\n");
  if (rd_count == 0)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Lookup failed, rp_filtering?\n");
    ok = 2;
  }
  else
  {
    ok = 1;
    GNUNET_log (GNUNET_ERROR_TYPE_INFO, "name: %s\n", (char*)cls);
    for (i=0; i<rd_count; i++)
    {
      GNUNET_log (GNUNET_ERROR_TYPE_INFO, "type: %d\n", rd[i].record_type);
      if (rd[i].record_type == GNUNET_GNS_RECORD_A)
      {
        memcpy(&a, rd[i].data, sizeof(a));
        addr = inet_ntoa(a);
        GNUNET_log (GNUNET_ERROR_TYPE_INFO, "address: %s\n", addr);
        if (0 == strcmp(addr, TEST_IP))
        {
          GNUNET_log (GNUNET_ERROR_TYPE_INFO,
                    "%s correctly resolved to %s!\n", TEST_DOMAIN, addr);
          ok = 0;
        }
      }
      else
      {
        GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "No resolution!\n");
      }
    }
  }
  GNUNET_SCHEDULER_add_now (&shutdown_task, NULL);
}


/**
 * Function scheduled to be run on the successful start of services
 * tries to look up the dns record for TEST_DOMAIN
 */
static void
commence_testing (void *cls, int32_t success, const char *emsg)
{
  int i;
  char lookup_name[GNUNET_DNSPARSER_MAX_NAME_LENGTH];
  struct GNUNET_GNS_LookupRequest *lr;
  
  gns_handle = GNUNET_GNS_connect(cfg);

  if (NULL == gns_handle)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Failed to connect to GNS!\n");
    ok = 2;
  }


  /* Now lookup some non existing records */
  for (i=0; i<max_parallel_lookups+TEST_ADDITIONAL_LOOKUPS; i++)
  {
    GNUNET_snprintf(lookup_name,
                    GNUNET_DNSPARSER_MAX_NAME_LENGTH,
                    "www.doesnotexist-%d.bob.gads", i);
    lr = GNUNET_GNS_lookup (gns_handle, lookup_name, GNUNET_GNS_RECORD_A,
			    GNUNET_NO,
			    NULL,
			    &on_lookup_result_dummy, &requests[num_requests]);
    requests[num_requests++] = lr;
  }
  lr = GNUNET_GNS_lookup (gns_handle, TEST_DOMAIN, GNUNET_GNS_RECORD_A,
			  GNUNET_NO,
			  NULL,
			  &on_lookup_result, TEST_DOMAIN);
  requests[num_requests++] = lr;
  GNUNET_assert (num_requests == max_parallel_lookups + TEST_ADDITIONAL_LOOKUPS + 1);
}



static void
do_check (void *cls,
          const struct GNUNET_CONFIGURATION_Handle *ccfg,
          struct GNUNET_TESTING_Peer *peer)
{
  struct GNUNET_CRYPTO_RsaPublicKeyBinaryEncoded alice_pkey;
  struct GNUNET_CRYPTO_RsaPublicKeyBinaryEncoded bob_pkey;
  struct GNUNET_CRYPTO_RsaPrivateKey *alice_key;
  struct GNUNET_CRYPTO_RsaPrivateKey *bob_key;
  char* alice_keyfile;
  struct GNUNET_CRYPTO_ShortHashCode bob_hash;

  cfg = ccfg;
  die_task = GNUNET_SCHEDULER_add_delayed (TIMEOUT, &end_badly, NULL);


  /* put records into namestore */
  namestore_handle = GNUNET_NAMESTORE_connect(cfg);
  if (NULL == namestore_handle)
  {
    GNUNET_log(GNUNET_ERROR_TYPE_ERROR, "Failed to connect to namestore\n");
    GNUNET_SCHEDULER_shutdown ();
    return;
  }

  if (GNUNET_OK != GNUNET_CONFIGURATION_get_value_filename (cfg, "gns",
                                                          "ZONEKEY",
                                                          &alice_keyfile))
  {
    GNUNET_log(GNUNET_ERROR_TYPE_ERROR, "Failed to get key from cfg\n");
    GNUNET_SCHEDULER_shutdown ();
    return;
  }

  if (GNUNET_OK != GNUNET_CONFIGURATION_get_value_number (cfg, "gns",
							  "MAX_PARALLEL_BACKGROUND_QUERIES",
							  &max_parallel_lookups))
  {
    GNUNET_log(GNUNET_ERROR_TYPE_ERROR, "Failed to get max queries from cfg\n");
    GNUNET_SCHEDULER_shutdown ();
    GNUNET_free (alice_keyfile);
    return;
  }
  requests = GNUNET_malloc ((max_parallel_lookups + TEST_ADDITIONAL_LOOKUPS + 1) *
			    sizeof (struct GNUNET_GNS_LookupRequest *));
  alice_key = GNUNET_CRYPTO_rsa_key_create_from_file (alice_keyfile);
  bob_key = GNUNET_CRYPTO_rsa_key_create_from_file (KEYFILE_BOB);

  GNUNET_CRYPTO_rsa_key_get_public (alice_key, &alice_pkey);
  GNUNET_CRYPTO_rsa_key_get_public (bob_key, &bob_pkey);
  
  GNUNET_free(alice_keyfile);

  struct GNUNET_NAMESTORE_RecordData rd;
  char* ip = TEST_IP;
  struct in_addr *web = GNUNET_malloc(sizeof(struct in_addr));
  rd.expiration_time = UINT64_MAX;
  GNUNET_assert(1 == inet_pton (AF_INET, ip, web));
  rd.data_size = sizeof(struct in_addr);
  rd.data = web;
  rd.record_type = GNUNET_DNSPARSER_TYPE_A;
  rd.flags = GNUNET_NAMESTORE_RF_AUTHORITY;

  GNUNET_NAMESTORE_record_create (namestore_handle,
                                  alice_key,
                                  TEST_RECORD_NAME,
                                  &rd,
                                  NULL,
                                  NULL);

  GNUNET_CRYPTO_short_hash(&bob_pkey, sizeof(bob_pkey), &bob_hash);
  rd.data_size = sizeof(struct GNUNET_CRYPTO_ShortHashCode);
  rd.data = &bob_hash;
  rd.record_type = GNUNET_GNS_RECORD_PKEY;

  GNUNET_NAMESTORE_record_create (namestore_handle,
                                  alice_key,
                                  TEST_AUTHORITY_NAME,
                                  &rd,
                                  &commence_testing,
                                  NULL);
  
  GNUNET_CRYPTO_rsa_key_free(alice_key);
  GNUNET_CRYPTO_rsa_key_free(bob_key);
  GNUNET_free(web);

}


int
main (int argc, char *argv[])
{
  ok = 1;

  GNUNET_log_setup ("test-gns-max-queries",
                    "WARNING",
                    NULL);
  GNUNET_TESTING_peer_run ("test-gns-max-queries", "test_gns_simple_lookup.conf", &do_check, NULL);
  return ok;
}


/* end of test_gns_simple_zkey_lookup.c */
