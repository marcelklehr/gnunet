/*
     This file is part of GNUnet.
     (C) 2010,2011 Christian Grothoff (and other contributing authors)

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
 * @file ats/test_ats_normalization_update_quality.c
 * @brief test updating an address
 * @author Christian Grothoff
 * @author Matthias Wachs
 */
#include "platform.h"
#include "gnunet_ats_service.h"
#include "gnunet_testing_lib.h"
#include "ats.h"
#include "test_ats_api_common.h"

static GNUNET_SCHEDULER_TaskIdentifier die_task;

/**
 * Scheduling handle
 */
static struct GNUNET_ATS_SchedulingHandle *sched_ats;

/**
 * Return value
 */
static int ret;

/**
 * Test address
 */
static struct Test_Address test_addr[3];

/**
 * Test peer
 */
static struct PeerContext p[2];

/**
 * HELLO test address
 */

struct GNUNET_HELLO_Address test_hello_address[3];

/**
 * Test ats info
 */
struct GNUNET_ATS_Information test_ats_info[3];

/**
 * Test ats count
 */
uint32_t test_ats_count;


static void
end_badly (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  die_task = GNUNET_SCHEDULER_NO_TASK;

  if (sched_ats != NULL)
    GNUNET_ATS_scheduling_done (sched_ats);
  free_test_address (&test_addr[0]);
  free_test_address (&test_addr[1]);
  free_test_address (&test_addr[2]);
  ret = 0;
}


static void
end ()
{
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Shutting down\n");
  if (die_task != GNUNET_SCHEDULER_NO_TASK)
  {
    GNUNET_SCHEDULER_cancel (die_task);
    die_task = GNUNET_SCHEDULER_NO_TASK;
  }
  GNUNET_ATS_scheduling_done (sched_ats);
  sched_ats = NULL;
  free_test_address (&test_addr[0]);
  free_test_address (&test_addr[1]);
  free_test_address (&test_addr[2]);
}


static void
address_suggest_cb (void *cls, const struct GNUNET_HELLO_Address *address,
                    struct Session *session,
                    struct GNUNET_BANDWIDTH_Value32NBO bandwidth_out,
                    struct GNUNET_BANDWIDTH_Value32NBO bandwidth_in,
                    const struct GNUNET_ATS_Information *atsi,
                    uint32_t ats_count)
{
  static int stage = 0;
  if (0 == stage)
  {
    GNUNET_ATS_suggest_address_cancel (sched_ats, &p[0].id);

    /* Update address */
    /* Prepare ATS Information */

    test_ats_info[0].type = htonl (GNUNET_ATS_QUALITY_NET_DELAY);
    test_ats_info[0].value = htonl(20);
    test_ats_count = 1;

    GNUNET_ATS_address_update (sched_ats, &test_hello_address[0], NULL, test_ats_info, test_ats_count);

    test_ats_info[0].type = htonl (GNUNET_ATS_QUALITY_NET_DELAY);
    test_ats_info[0].value = htonl(20);
    test_ats_count = 1;

    GNUNET_ATS_address_update (sched_ats, &test_hello_address[0], NULL, test_ats_info, test_ats_count);


    /* Request address */
    stage ++;
  }
}

static void
run (void *cls,
     const struct GNUNET_CONFIGURATION_Handle *cfg,
     struct GNUNET_TESTING_Peer *peer)
{
  die_task = GNUNET_SCHEDULER_add_delayed (TIMEOUT, &end_badly, NULL);

  /* Connect to ATS scheduling */
  sched_ats = GNUNET_ATS_scheduling_init (cfg, &address_suggest_cb, NULL);
  if (sched_ats == NULL)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "Could not connect to ATS scheduling!\n");
    ret = 1;
    end ();
    return;
  }

  /* Set up peer */
  if (GNUNET_SYSERR == GNUNET_CRYPTO_hash_from_string(PEERID0, &p[0].id.hashPubKey))
  {
      GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "Could not setup peer!\n");
      ret = GNUNET_SYSERR;
      end ();
      return;
  }

  GNUNET_assert (0 == strcmp (PEERID0, GNUNET_i2s_full (&p[0].id)));

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Created peer `%s'\n",
              GNUNET_i2s_full(&p[0].id));

  /* Set up peer */
  if (GNUNET_SYSERR == GNUNET_CRYPTO_hash_from_string(PEERID1, &p[1].id.hashPubKey))
  {
      GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "Could not setup peer!\n");
      ret = GNUNET_SYSERR;
      end ();
      return;
  }

  GNUNET_assert (0 == strcmp (PEERID1, GNUNET_i2s_full (&p[1].id)));

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Created peer `%s'\n",
              GNUNET_i2s_full(&p[1].id));



  /* Adding address for peer 0 */
  create_test_address (&test_addr[0], "test", &test_addr, "test_p0_a0", strlen ("test_p0_a0") + 1);
  /* Prepare ATS Information */
  test_ats_info[0].type = htonl (GNUNET_ATS_NETWORK_TYPE);
  test_ats_info[0].value = htonl(GNUNET_ATS_NET_WAN);
  test_ats_info[1].type = htonl (GNUNET_ATS_QUALITY_NET_DELAY);
  test_ats_info[1].value = htonl(30);
  test_ats_count = 2;

  test_hello_address[0].peer = p[0].id;
  test_hello_address[0].transport_name = test_addr[0].plugin;
  test_hello_address[0].address = test_addr[0].addr;
  test_hello_address[0].address_length = test_addr[0].addr_len;
  GNUNET_ATS_address_add (sched_ats, &test_hello_address[0], NULL, test_ats_info, test_ats_count);

  /* Adding address for peer 1 */
  create_test_address (&test_addr[1], "test", &test_addr, "test_p1_a0", strlen ("test_p1_a0") + 1);
  test_ats_info[0].type = htonl (GNUNET_ATS_NETWORK_TYPE);
  test_ats_info[0].value = htonl(GNUNET_ATS_NET_WAN);
  test_ats_info[1].type = htonl (GNUNET_ATS_QUALITY_NET_DELAY);
  test_ats_info[1].value = htonl(10);
  test_ats_count = 2;

  test_hello_address[1].peer = p[1].id;
  test_hello_address[1].transport_name = test_addr[1].plugin;
  test_hello_address[1].address = test_addr[1].addr;
  test_hello_address[1].address_length = test_addr[1].addr_len;
  GNUNET_ATS_address_add (sched_ats, &test_hello_address[1], NULL, test_ats_info, test_ats_count);

  /* Adding 2nd address for peer 1 */
  test_ats_info[0].type = htonl (GNUNET_ATS_NETWORK_TYPE);
  test_ats_info[0].value = htonl(GNUNET_ATS_NET_WAN);
  test_ats_info[1].type = htonl (GNUNET_ATS_QUALITY_NET_DELAY);
  test_ats_info[1].value = htonl(20);
  test_ats_count = 2;

  create_test_address (&test_addr[2], "test", &test_addr, "test_p1_a1", strlen ("test_p1_a1") + 1);
  test_hello_address[2].peer = p[1].id;
  test_hello_address[2].transport_name = test_addr[2].plugin;
  test_hello_address[2].address = test_addr[2].addr;
  test_hello_address[2].address_length = test_addr[2].addr_len;
  GNUNET_ATS_address_add (sched_ats, &test_hello_address[2], NULL, test_ats_info, test_ats_count);

  /* Request address */
  GNUNET_ATS_suggest_address (sched_ats, &p[0].id);
}


int
main (int argc, char *argv[])
{
  if (0 != GNUNET_TESTING_peer_run ("test_ats_normalization_update_quality",
                                    "test_ats_api.conf",
                                    &run, NULL))
    return 1;
  return ret;
}

/* end of file test_ats_normalization_update_quality.c*/