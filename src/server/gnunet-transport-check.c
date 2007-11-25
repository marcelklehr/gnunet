/*
     This file is part of GNUnet.
     (C) 2001, 2002, 2003, 2004, 2006 Christian Grothoff (and other contributing authors)

     GNUnet is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 2, or (at your
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
 * @file server/gnunet-transport-check.c
 * @brief Test for the transports.
 * @author Christian Grothoff
 *
 * This utility can be used to test if a transport mechanism for
 * GNUnet is properly configured.
 */

#include "platform.h"
#include "gnunet_util.h"
#include "gnunet_directories.h"
#include "gnunet_protocols.h"
#include "gnunet_transport_service.h"
#include "gnunet_identity_service.h"
#include "gnunet_pingpong_service.h"
#include "gnunet_bootstrap_service.h"
#include "core.h"
#include "connection.h"
#include "handler.h"
#include "startup.h"

#define DEBUG_TRANSPORT_CHECK GNUNET_NO

static struct GNUNET_Semaphore *sem;

static int terminate;

static unsigned long long timeout;

static GNUNET_Transport_ServiceAPI *transport;

static GNUNET_Identity_ServiceAPI *identity;

static GNUNET_Pingpong_ServiceAPI *pingpong;

static GNUNET_Bootstrap_ServiceAPI *bootstrap;

static int ok;

static int ping;

static char *expectedValue;

static unsigned long long expectedSize;

static struct GNUNET_GC_Configuration *cfg;

static struct GNUNET_GE_Context *ectx;

static struct GNUNET_CronManager *cron;

static char *cfgFilename = GNUNET_DEFAULT_DAEMON_CONFIG_FILE;

static void
semUp (void *arg)
{
  struct GNUNET_Semaphore *sem = arg;

  terminate = GNUNET_YES;
  GNUNET_semaphore_up (sem);
}

static int
noiseHandler (const GNUNET_PeerIdentity * peer,
              const GNUNET_MessageHeader * msg, GNUNET_TSession * s)
{
  if ((ntohs (msg->size) ==
       sizeof (GNUNET_MessageHeader) + expectedSize) &&
      (0 == memcmp (expectedValue, &msg[1], expectedSize)))
    ok = GNUNET_YES;
  GNUNET_semaphore_up (sem);
  return GNUNET_OK;
}

/**
 * Test the given transport API.
 */
static void
testTAPI (TransportAPI * tapi, void *ctx)
{
  int *res = ctx;
  GNUNET_MessageHello *helo;
  GNUNET_TSession *tsession;
  unsigned long long repeat;
  unsigned long long total;
  GNUNET_CronTime start;
  GNUNET_CronTime end;
  GNUNET_MessageHeader *noise;
  int ret;

  GNUNET_GE_ASSERT (ectx, tapi != NULL);
  if (tapi->protocolNumber == GNUNET_TRANSPORT_PROTOCOL_NUMBER_NAT)
    {
      *res = GNUNET_OK;
      return;                   /* NAT cannot be tested */
    }
  helo = tapi->createhello ();
  if (helo == NULL)
    {
      fprintf (stderr, _("`%s': Could not create hello.\n"), tapi->transName);
      *res = GNUNET_SYSERR;
      return;
    }
  tsession = NULL;
  if (GNUNET_OK != tapi->connect (helo, &tsession, GNUNET_NO))
    {
      fprintf (stderr, _("`%s': Could not connect.\n"), tapi->transName);
      *res = GNUNET_SYSERR;
      GNUNET_free (helo);
      return;
    }
  GNUNET_free (helo);
  if (-1 == GNUNET_GC_get_configuration_value_number (cfg,
                                               "TRANSPORT-CHECK",
                                               "REPEAT",
                                               1,
                                               (unsigned long) -1,
                                               1, &repeat))
    {
      *res = GNUNET_SYSERR;
      return;
    }
  total = repeat;
  sem = GNUNET_semaphore_create (0);
  start = GNUNET_get_time ();
  noise = GNUNET_malloc (expectedSize + sizeof (GNUNET_MessageHeader));
  noise->type = htons (GNUNET_P2P_PROTO_NOISE);
  noise->size = htons (expectedSize + sizeof (GNUNET_MessageHeader));
  memcpy (&noise[1], expectedValue, expectedSize);
  while ((repeat > 0) && (GNUNET_shutdown_test () == GNUNET_NO))
    {
      repeat--;
      ok = GNUNET_NO;
      ret = GNUNET_NO;
      while (ret == GNUNET_NO)
        ret = sendPlaintext (tsession, (char *) noise, ntohs (noise->size));
      if (ret != GNUNET_OK)
        {
          fprintf (stderr, _("`%s': Could not send.\n"), tapi->transName);
          *res = GNUNET_SYSERR;
          tapi->disconnect (tsession);
          GNUNET_semaphore_destroy (sem);
          GNUNET_free (noise);
          return;
        }
      GNUNET_cron_add_job (cron, &semUp, timeout, 0, sem);
      GNUNET_semaphore_down (sem, GNUNET_YES);
      GNUNET_cron_suspend_jobs (cron, GNUNET_NO);
      GNUNET_cron_del_job (cron, &semUp, 0, sem);
      GNUNET_cron_resume_jobs (cron, GNUNET_NO);
      if (ok != GNUNET_YES)
        {
          FPRINTF (stderr,
                   _("`%s': Did not receive message within %llu ms.\n"),
                   tapi->transName, timeout);
          *res = GNUNET_SYSERR;
          tapi->disconnect (tsession);
          GNUNET_semaphore_destroy (sem);
          GNUNET_free (noise);
          return;
        }
    }
  GNUNET_free (noise);
  end = GNUNET_get_time ();
  if (GNUNET_OK != tapi->disconnect (tsession))
    {
      fprintf (stderr, _("`%s': Could not disconnect.\n"), tapi->transName);
      *res = GNUNET_SYSERR;
      GNUNET_semaphore_destroy (sem);
      return;
    }
  GNUNET_semaphore_destroy (sem);
  printf (_
          ("`%s' transport OK.  It took %ums to transmit %llu messages of %llu bytes each.\n"),
          tapi->transName,
          (unsigned int) ((end - start) / GNUNET_CRON_MILLISECONDS), total,
          expectedSize);
}

static void
pingCallback (void *unused)
{
  ok = GNUNET_YES;
  GNUNET_semaphore_up (sem);
}

static void
testPING (const GNUNET_MessageHello * xhello, void *arg)
{
  int *stats = arg;
  GNUNET_TSession *tsession;
  GNUNET_MessageHello *hello;
  GNUNET_MessageHello *myHello;
  GNUNET_MessageHeader *ping;
  char *msg;
  int len;
  GNUNET_PeerIdentity peer;
  unsigned long long verbose;

  stats[0]++;                   /* one more seen */
  if (GNUNET_NO == transport->isAvailable (ntohs (xhello->protocol)))
    {
      GNUNET_GE_LOG (ectx,
              GNUNET_GE_DEBUG | GNUNET_GE_REQUEST | GNUNET_GE_USER,
              _(" Transport %d is not being tested\n"),
              ntohs (xhello->protocol));
      return;
    }
  if (ntohs (xhello->protocol) == GNUNET_TRANSPORT_PROTOCOL_NUMBER_NAT)
    return;                     /* NAT cannot be tested */

  stats[1]++;                   /* one more with transport 'available' */
  GNUNET_GC_get_configuration_value_number (cfg,
                                     "GNUNET",
                                     "VERBOSE",
                                     0, (unsigned long long) -1, 0, &verbose);
  if (verbose > 0)
    {
      char *str;
      void *addr;
      unsigned int addr_len;
      int have_addr;

      have_addr = transport->helloToAddress (xhello, &addr, &addr_len);
      if (have_addr == GNUNET_NO)
        {
          str = GNUNET_strdup ("NAT");  /* most likely */
        }
      else
        {
          str = GNUNET_get_ip_as_string (addr, addr_len, GNUNET_YES);
          GNUNET_free (addr);
        }
      fprintf (stderr, _("\nContacting `%s'."), str);
      GNUNET_free (str);
    }
  else
    fprintf (stderr, ".");
  hello = GNUNET_malloc (ntohs (xhello->header.size));
  memcpy (hello, xhello, ntohs (xhello->header.size));

  myHello = transport->createhello (ntohs (xhello->protocol));
  if (myHello == NULL)
    /* try NAT */
    myHello = transport->createhello (GNUNET_TRANSPORT_PROTOCOL_NUMBER_NAT);
  if (myHello == NULL)
    {
      GNUNET_free (hello);
      return;
    }
  if (verbose > 0)
    fprintf (stderr, ".");
  tsession = NULL;
  peer = hello->senderIdentity;
  tsession = transport->connect (hello, __FILE__, GNUNET_NO);
  GNUNET_free (hello);
  if (tsession == NULL)
    {
      fprintf (stderr, _(" Connection failed\n"));
      return;
    }
  if (tsession == NULL)
    {
      GNUNET_GE_BREAK (ectx, 0);
      fprintf (stderr, _(" Connection failed (bug?)\n"));
      return;
    }
  if (verbose > 0)
    fprintf (stderr, ".");

  sem = GNUNET_semaphore_create (0);
  ping = pingpong->pingUser (&peer, &pingCallback, NULL, GNUNET_YES, rand ());
  len = ntohs (ping->size) + ntohs (myHello->header.size);
  msg = GNUNET_malloc (len);
  memcpy (msg, myHello, ntohs (myHello->header.size));
  memcpy (&msg[ntohs (myHello->header.size)], ping, ntohs (ping->size));
  GNUNET_free (myHello);
  GNUNET_free (ping);
  /* send ping */
  ok = GNUNET_NO;
  if (GNUNET_OK != sendPlaintext (tsession, msg, len))
    {
      fprintf (stderr, "Send failed.\n");
      GNUNET_free (msg);
      transport->disconnect (tsession, __FILE__);
      return;
    }
  GNUNET_free (msg);
  if (verbose > 0)
    fprintf (stderr, ".");
  /* check: received pong? */
#if DEBUG_TRANSPORT_CHECK
  GNUNET_GE_LOG (ectx, GNUNET_GE_DEBUG | GNUNET_GE_REQUEST | GNUNET_GE_USER, "Waiting for PONG\n");
#endif
  terminate = GNUNET_NO;
  GNUNET_cron_add_job (cron, &semUp, timeout, 5 * GNUNET_CRON_SECONDS, sem);
  GNUNET_semaphore_down (sem, GNUNET_YES);

  if (verbose > 0)
    {
      if (ok != GNUNET_YES)
        FPRINTF (stderr, _("Timeout after %llums.\n"), timeout);
      else
        fprintf (stderr, _("OK!\n"));
    }
  GNUNET_cron_suspend_jobs (cron, GNUNET_NO);
  GNUNET_cron_del_job (cron, &semUp, 5 * GNUNET_CRON_SECONDS, sem);
  GNUNET_cron_resume_jobs (cron, GNUNET_NO);
  GNUNET_semaphore_destroy (sem);
  sem = NULL;
  transport->disconnect (tsession, __FILE__);
  if (ok == GNUNET_YES)
    stats[2]++;
}

static int
testTerminate (void *arg)
{
  if (GNUNET_shutdown_test () == GNUNET_NO)
    return GNUNET_YES;
  return GNUNET_NO;
}

/**
 * All gnunet-transport-check command line options
 */
static struct GNUNET_CommandLineOption gnunettransportcheckOptions[] = {
  GNUNET_COMMAND_LINE_OPTION_CFG_FILE (&cfgFilename),   /* -c */
  GNUNET_COMMAND_LINE_OPTION_HELP (gettext_noop ("Tool to test if GNUnet transport services are operational.")),        /* -h */
  GNUNET_COMMAND_LINE_OPTION_HOSTNAME,  /* -H */
  GNUNET_COMMAND_LINE_OPTION_LOGGING,   /* -L */
  {'p', "ping", NULL,
   gettext_noop ("ping peers from HOSTLISTURL that match transports"),
   0, &GNUNET_getopt_configure_set_one, &ping},
  {'r', "repeat", "COUNT",
   gettext_noop ("send COUNT messages"),
   1, &GNUNET_getopt_configure_set_option, "TRANSPORT-CHECK:REPEAT"},
  {'s', "size", "SIZE",
   gettext_noop ("send messages with SIZE bytes payload"),
   1, &GNUNET_getopt_configure_set_option, "TRANSPORT-CHECK:SIZE"},
  {'t', "transport", "TRANSPORT",
   gettext_noop ("specifies which TRANSPORT should be tested"),
   1, &GNUNET_getopt_configure_set_option, "GNUNETD:TRANSPORTS"},
  {'T', "timeout", "MS",
   gettext_noop ("specifies after how many MS to time-out"),
   1, &GNUNET_getopt_configure_set_option, "TRANSPORT-CHECK:TIMEOUT"},
  {'u', "user", "LOGIN",
   gettext_noop ("run as user LOGIN"),
   1, &GNUNET_getopt_configure_set_option, "GNUNETD:USER"},
  GNUNET_COMMAND_LINE_OPTION_VERSION (PACKAGNUNET_GE_VERSION), /* -v */
  GNUNET_COMMAND_LINE_OPTION_VERBOSE,
  {'X', "Xrepeat", "X",
   gettext_noop ("repeat each test X times"),
   1, &GNUNET_getopt_configure_set_option, "TRANSPORT-CHECK:X-REPEAT"},
  GNUNET_COMMAND_LINE_OPTION_END,
};

int
main (int argc, char *const *argv)
{
  int res;
  unsigned long long Xrepeat;
  char *trans;
  int stats[3];
  int pos;

  res = GNUNET_init (argc,
                     argv,
                     "gnunet-transport-check",
                     &cfgFilename, gnunettransportcheckOptions, &ectx, &cfg);
  if ((res == -1) || (GNUNET_OK != changeUser (ectx, cfg)))
    {
      GNUNET_fini (ectx, cfg);
      return -1;
    }

  if (-1 == GNUNET_GC_get_configuration_value_number (cfg,
                                               "TRANSPORT-CHECK",
                                               "SIZE",
                                               1, 60000, 12, &expectedSize))
    {
      GNUNET_fini (ectx, cfg);
      return 1;
    }
  if (-1 == GNUNET_GC_get_configuration_value_number (cfg,
                                               "TRANSPORT-CHECK",
                                               "TIMEOUT",
                                               1,
                                               60 * GNUNET_CRON_SECONDS,
                                               3 * GNUNET_CRON_SECONDS,
                                               &timeout))
    {
      GNUNET_fini (ectx, cfg);
      return 1;
    }

  expectedValue = GNUNET_malloc (expectedSize);
  pos = expectedSize;
  expectedValue[--pos] = '\0';
  while (pos-- > 0)
    expectedValue[pos] = 'A' + (pos % 26);

  trans = NULL;
  if (-1 == GNUNET_GC_get_configuration_value_string (cfg,
                                               "GNUNETD",
                                               "TRANSPORTS",
                                               "udp tcp http", &trans))
    {
      GNUNET_free (expectedValue);
      GNUNET_fini (ectx, cfg);
      return 1;
    }
  GNUNET_GE_ASSERT (ectx, trans != NULL);
  if (ping)
    printf (_("Testing transport(s) %s\n"), trans);
  else
    printf (_("Available transport(s): %s\n"), trans);
  GNUNET_free (trans);
  if (!ping)
    {
      /* disable blacklists (loopback is often blacklisted)... */
      GNUNET_GC_set_configuration_value_string (cfg, ectx, "TCP", "BLACKLIST", "");
      GNUNET_GC_set_configuration_value_string (cfg, ectx, "TCP6", "BLACKLIST", "");
      GNUNET_GC_set_configuration_value_string (cfg, ectx, "UDP", "BLACKLIST", "");
      GNUNET_GC_set_configuration_value_string (cfg, ectx, "UDP6", "BLACKLIST", "");
      GNUNET_GC_set_configuration_value_string (cfg, ectx, "HTTP", "BLACKLIST", "");
    }
  cron = cron_create (ectx);
  if (GNUNET_OK != initCore (ectx, cfg, cron, NULL))
    {
      GNUNET_free (expectedValue);
      GNUNET_cron_destroy (cron);
      GNUNET_fini (ectx, cfg);
      return 1;
    }
  initConnection (ectx, cfg, NULL, cron);
  registerPlaintextHandler (GNUNET_P2P_PROTO_NOISE, &noiseHandler);
  enableCoreProcessing ();
  identity = requestService ("identity");
  transport = requestService ("transport");
  pingpong = requestService ("pingpong");
  GNUNET_cron_start (cron);

  GNUNET_GC_get_configuration_value_number (cfg,
                                     "TRANSPORT-CHECK",
                                     "X-REPEAT",
                                     1, (unsigned long long) -1, 1, &Xrepeat);
  res = GNUNET_OK;
  if (ping)
    {
      bootstrap = requestService ("bootstrap");

      stats[0] = 0;
      stats[1] = 0;
      stats[2] = 0;
      bootstrap->bootstrap (&testPING, &stats[0], &testTerminate, NULL);
      printf (_
              ("\n%d out of %d peers contacted successfully (%d times transport unavailable).\n"),
              stats[2], stats[1], stats[0] - stats[1]);
      releaseService (bootstrap);
    }
  else
    {
      while ((Xrepeat-- > 0) && (GNUNET_shutdown_test () == GNUNET_NO))
        transport->forEach (&testTAPI, &res);
    }
  GNUNET_cron_stop (cron);
  releaseService (identity);
  releaseService (transport);
  releaseService (pingpong);
  disableCoreProcessing ();
  unregisterPlaintextHandler (GNUNET_P2P_PROTO_NOISE, &noiseHandler);
  doneConnection ();
  doneCore ();
  GNUNET_free (expectedValue);
  GNUNET_cron_destroy (cron);
  GNUNET_fini (ectx, cfg);

  if (res != GNUNET_OK)
    return -1;
  return 0;
}


/* end of gnunet-transport-check */
