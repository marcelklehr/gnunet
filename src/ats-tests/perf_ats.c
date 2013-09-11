/*
     This file is part of GNUnet.
     (C) 2010-2013 Christian Grothoff (and other contributing authors)

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
 * @file ats/perf_ats.c
 * @brief ats benchmark: start peers and modify preferences, monitor change over time
 * @author Christian Grothoff
 * @author Matthias Wachs
 */
#include "platform.h"
#include "gnunet_util_lib.h"
#include "gnunet_testbed_service.h"
#include "gnunet_ats_service.h"
#include "gnunet_core_service.h"

#define TEST_TIMEOUT GNUNET_TIME_relative_multiply (GNUNET_TIME_UNIT_SECONDS, 10)
#define BENCHMARK_DURATION GNUNET_TIME_relative_multiply (GNUNET_TIME_UNIT_SECONDS, 5)
#define TESTNAME_PREFIX "perf_ats_"
#define DEFAULT_SLAVES_NUM 3
#define DEFAULT_MASTERS_NUM 1

#define TEST_MESSAGE_TYPE_PING 12345
#define TEST_MESSAGE_TYPE_PONG 12346
#define TEST_MESSAGE_SIZE 1000
#define TEST_MESSAGE_FREQUENCY GNUNET_TIME_relative_multiply (GNUNET_TIME_UNIT_SECONDS, 1)


/**
 * Information we track for a peer in the testbed.
 */
struct BenchmarkPeer
{
  /**
   * Handle with testbed.
   */
  struct GNUNET_TESTBED_Peer *peer;

  /**
   * Unique identifier
   */
  int no;

  /**
   *  master: GNUNET_YES/NO
   */
  int master;

  /**
   *  Peer ID
   */
  struct GNUNET_PeerIdentity id;

  /**
   *  Core handle
   */
  struct GNUNET_CORE_Handle *ch;

  /**
   * Testbed operation to connect to ATS performance service
   */
  struct GNUNET_TESTBED_Operation *ats_perf_op;

  /**
   * Testbed operation to get peer information
   */
  struct GNUNET_TESTBED_Operation *info_op;

  /**
   * Testbed operation to connect to core
   */
  struct GNUNET_TESTBED_Operation *core_op;

  /**
   * ATS performance handle
   */
  struct GNUNET_ATS_PerformanceHandle *p_handle;

  /**
   * Testbed connecect operation
   */
  struct ConnectOperation *connect_ops;

	/**
	 * ATS Measurement Partner
	 */
  struct BenchmarkPeer *destination;

  GNUNET_SCHEDULER_TaskIdentifier ats_task;

  /* Message exchange */

  /**
   * Core transmit handle
   */
  struct GNUNET_CORE_TransmitHandle *cth;

  /**
   * DLL for pending messages: head
   */
	struct PendingMessages *p_head;

  /**
   * DLL for pending messages: tail
   */
	struct PendingMessages *p_tail;

	/**
	 *  Bit-mask for next partner selection
	 */
	uint32_t send_mask;

	/**
	 * Current message for partner?
	 */
	int partner_msg;

	/**
	 * Number of core connections
	 */
  int core_connections;

	/**
	 * Number of slave connections
	 */
  int slave_connections;

  /**
   * Statistics
   */
  unsigned int messages_sent;
  unsigned int messages_sent_partner;
  unsigned int messages_received;
};


static int c_master_peers;

/**
 * Array of master peers
 * Preferences to be set for
 */
static struct BenchmarkPeer *bp_master;

static int c_slave_peers;

/**
 * Array of slave peers
 * Peer used for measurements
 */
static struct BenchmarkPeer *bp_slaves;


struct BenchmarkState
{
	/* Are we connected to ATS service of all peers: GNUNET_YES/NO */
	int connected_ATS_service;

	/* Are we connected to CORE service of all peers: GNUNET_YES/NO */
	int connected_CORE_service;

	/* Are we connected to all peers: GNUNET_YES/NO */
	int connected_PEERS;

	/* Are we connected to all slave peers on CORE level: GNUNET_YES/NO */
	int connected_CORE;

	/* Are we connected to CORE service of all peers: GNUNET_YES/NO */
	int benchmarking;

	int *core_connections;

	uint32_t partner_map;
};

static struct BenchmarkState state;

/**
 * Shutdown task
 */
static GNUNET_SCHEDULER_TaskIdentifier shutdown_task;

static int result;
static char *solver;
static char *pref_str;
static int pref_val;

/**
 * Pending Responses
 */
struct PendingMessages
{
	struct PendingMessages *prev;
	struct PendingMessages *next;
	struct GNUNET_PeerIdentity target;
};


/**
 * Information we track for a peer in the testbed.
 */
struct ConnectOperation
{
	struct BenchmarkPeer *master;

	struct BenchmarkPeer *slave;
  /**
   * Testbed operation to connect peers
   */
  struct GNUNET_TESTBED_Operation *connect_op;

};


static void
core_connect_completion_cb (void *cls,
			    struct GNUNET_TESTBED_Operation *op,
			    void *ca_result,
			    const char *emsg );


static void evaluate ()
{
	int c_p;
	struct BenchmarkPeer *bp;
	int total_out;
	int partner_out;

  for (c_p = 0; c_p < c_master_peers; c_p++)
  {
  	bp = &bp_master[c_p];
  	total_out = (bp->messages_sent * TEST_MESSAGE_SIZE) / 10240;
  	partner_out = (bp->messages_sent_partner * TEST_MESSAGE_SIZE) / 10240;
    fprintf (stderr, _("Peer %u: Out total: %u KiB/s, out partner %u KiB/s\n"),
    		bp->no,
    		total_out, partner_out
    		/*partner_out / (total_out / 100)*/);
  }
}

/**
 * Shutdown nicely
 *
 * @param cls NULL
 * @param tc the task context
 */
static void
do_shutdown (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  int c_p;
  int c_op;
  struct PendingMessages *cur;
  struct PendingMessages *next;

  shutdown_task = GNUNET_SCHEDULER_NO_TASK;

  state.benchmarking = GNUNET_NO;

  evaluate ();
  GNUNET_log (GNUNET_ERROR_TYPE_INFO, _("Benchmarking done\n"));

  for (c_p = 0; c_p < c_master_peers; c_p++)
  {
  	next = bp_master[c_p].p_head;
  	for (cur = next; cur != NULL; cur = next )
  	{
  		next = cur->next;
  		GNUNET_CONTAINER_DLL_remove (bp_master[c_p].p_head, bp_master[c_p].p_tail, cur);
  		GNUNET_free (cur);
  	}

  	if (GNUNET_SCHEDULER_NO_TASK != bp_master[c_p].ats_task)
  	{
  		GNUNET_SCHEDULER_cancel (bp_master[c_p].ats_task);
  		bp_master[c_p].ats_task = GNUNET_SCHEDULER_NO_TASK;
  	}

  	if (NULL != bp_master[c_p].cth)
  	{
  		GNUNET_CORE_notify_transmit_ready_cancel(bp_master[c_p].cth);
  		bp_master[c_p].cth = NULL;
  	}

  	if (NULL != bp_master[c_p].ats_perf_op)
  	{
  		GNUNET_TESTBED_operation_done (bp_master[c_p].ats_perf_op);
  		bp_master[c_p].ats_perf_op = NULL;
  	}

  	if (NULL != bp_master[c_p].core_op)
  	{
  		GNUNET_TESTBED_operation_done (bp_master[c_p].core_op);
  		bp_master[c_p].core_op = NULL;
  	}

  	if (NULL != bp_master[c_p].info_op)
  	{
  		GNUNET_break (0);
  		GNUNET_TESTBED_operation_done (bp_master[c_p].info_op);
  		bp_master[c_p].info_op = NULL;
  	}

  	for (c_op = 0; c_op < c_slave_peers; c_op++)
  	{
  		if (NULL != bp_master[c_p].connect_ops[c_op].connect_op)
  		{
    		GNUNET_log (GNUNET_ERROR_TYPE_INFO, _("Failed to connect peer 0 and %u\n"), c_p);
    		GNUNET_TESTBED_operation_done (bp_master[c_p].connect_ops[c_op].connect_op);
    		bp_master[c_p].connect_ops[c_op].connect_op = NULL;
      	result = 1;
  		}
  	}
  	GNUNET_free (bp_master[c_p].connect_ops);
  }

  for (c_p = 0; c_p < c_slave_peers; c_p++)
  {
  	next = bp_slaves[c_p].p_head;
  	for (cur = next; cur != NULL; cur = next )
  	{
  		next = cur->next;
  		GNUNET_CONTAINER_DLL_remove (bp_slaves[c_p].p_head, bp_slaves[c_p].p_tail, cur);
  		GNUNET_free (cur);
  	}

  	if (NULL != bp_slaves[c_p].cth)
  	{
  		GNUNET_CORE_notify_transmit_ready_cancel(bp_slaves[c_p].cth);
  		bp_slaves[c_p].cth = NULL;
  	}

  	if (NULL != bp_slaves[c_p].ats_perf_op)
  	{
  		GNUNET_TESTBED_operation_done (bp_slaves[c_p].ats_perf_op);
  		bp_slaves[c_p].ats_perf_op = NULL;
  	}

  	if (NULL != bp_slaves[c_p].core_op)
  	{
  		GNUNET_TESTBED_operation_done (bp_slaves[c_p].core_op);
  		bp_slaves[c_p].core_op = NULL;
  	}

  	if (NULL != bp_slaves[c_p].info_op)
  	{
  		GNUNET_break (0);
  		GNUNET_TESTBED_operation_done (bp_slaves[c_p].info_op);
  		bp_slaves[c_p].info_op = NULL;
  	}

  }

	GNUNET_SCHEDULER_shutdown();
}

static struct BenchmarkPeer *
find_peer (const struct GNUNET_PeerIdentity * peer)
{
	int c_p;

  for (c_p = 0; c_p < c_master_peers; c_p++)
  {
    if (0 == memcmp (&bp_master[c_p].id, peer, sizeof (struct GNUNET_PeerIdentity)))
    	return &bp_master[c_p];
  }

  for (c_p = 0; c_p < c_slave_peers; c_p++)
  {
    if (0 == memcmp (&bp_slaves[c_p].id, peer, sizeof (struct GNUNET_PeerIdentity)))
    	return &bp_slaves[c_p];
  }

	return NULL;
}


static void
store_information (struct GNUNET_PeerIdentity *id,
		 const struct GNUNET_HELLO_Address *address,
		 int address_active,
		 struct GNUNET_BANDWIDTH_Value32NBO bandwidth_out,
		 struct GNUNET_BANDWIDTH_Value32NBO bandwidth_in,
		 const struct GNUNET_ATS_Information *ats,
		 uint32_t ats_count)
{
	struct BenchmarkPeer *bp;

	bp = find_peer (id);

	if (NULL == bp)
	{
		GNUNET_break (0);
		return;
	}
}

static void
ats_performance_info_cb (void *cls,
			 const struct GNUNET_HELLO_Address *address,
			 int address_active,
			 struct GNUNET_BANDWIDTH_Value32NBO bandwidth_out,
			 struct GNUNET_BANDWIDTH_Value32NBO bandwidth_in,
			 const struct GNUNET_ATS_Information *ats,
			 uint32_t ats_count)
{
	struct BenchmarkPeer *bp = cls;
	int c_a;
	char *peer_id;

	peer_id = GNUNET_strdup (GNUNET_i2s (&bp->id));
	for (c_a = 0; c_a < ats_count; c_a++)
	{
		/*GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, _("%c %03u: %s %s %u\n"),
					(GNUNET_YES == p->master) ? 'M' : 'S',
					p->no,
			    GNUNET_i2s (&address->peer),
			    GNUNET_ATS_print_property_type(ntohl(ats[c_a].type)),
			    ntohl(ats[c_a].value));*/
	}

	if ((GNUNET_YES == bp->master) &&
	(0 == memcmp (&address->peer,  &bp->destination->id,
			sizeof (struct GNUNET_PeerIdentity))))
	{
		GNUNET_log (GNUNET_ERROR_TYPE_INFO, "Bandwidth for master %u: %lu %lu\n",
				bp->no,
				(long unsigned int) ntohl(bandwidth_in.value__),
				(long unsigned int) ntohl(bandwidth_in.value__));
	}

	store_information (&bp->id, address, address_active,
			bandwidth_in, bandwidth_out,
			ats, ats_count);

	GNUNET_free (peer_id);
}

static size_t
core_send_ready (void *cls, size_t size, void *buf)
{
	static char msgbuf[TEST_MESSAGE_SIZE];
	struct BenchmarkPeer *bp = cls;
	struct GNUNET_MessageHeader *msg;

	bp->cth = NULL;

	bp->messages_sent ++;
	if (GNUNET_YES == bp->partner_msg)
	{
		bp->messages_sent_partner ++;
		bp->partner_msg = GNUNET_NO;
	}

	msg = (struct GNUNET_MessageHeader *) &msgbuf;
	memset (&msgbuf, 'a', TEST_MESSAGE_SIZE);
	msg->type = htons (TEST_MESSAGE_TYPE_PING);
	msg->size = htons (TEST_MESSAGE_SIZE);
	memcpy (buf, msg, TEST_MESSAGE_SIZE);
	return TEST_MESSAGE_SIZE;
}

static struct BenchmarkPeer *
get_next (struct BenchmarkPeer *p)
{
	uint32_t b_index;
	uint32_t index;
	int counter;

	if (0 == p->send_mask)
		p->send_mask = (1 << c_slave_peers) - 1; /* Next round */

	GNUNET_assert (p->send_mask <= (1 << c_slave_peers) - 1);
	counter = 0;
	do
	{
		index = GNUNET_CRYPTO_random_u32 (GNUNET_CRYPTO_QUALITY_WEAK, c_slave_peers);
		b_index = 1 << index;
		counter++;
	}
	while	((b_index != (p->send_mask & b_index)) && (counter < c_slave_peers));
	if ((b_index != (p->send_mask & b_index)) && (counter == c_slave_peers))
	{
		/* To many random attempts use fcfs */
		for (index = 0; index < c_slave_peers - 1; index ++)
		{
			b_index = 1 << index;
			if (b_index == (p->send_mask & b_index))
				break;
		}
	}
	p->send_mask ^= b_index; /* Remove bit */
	return &bp_slaves[index];

}


static void
ats_pref_task (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
	static double last = 1;
	struct BenchmarkPeer *bp = cls;

	bp->ats_task = GNUNET_SCHEDULER_NO_TASK;

	GNUNET_log (GNUNET_ERROR_TYPE_INFO, "Set preference for master %u: %f\n",
			bp->no, last);
	GNUNET_ATS_performance_change_preference (bp->p_handle, &bp->destination->id,
			pref_val, (double) last,
			GNUNET_ATS_PREFERENCE_END);
	last++;
	bp->ats_task = GNUNET_SCHEDULER_add_delayed (GNUNET_TIME_UNIT_SECONDS,
			&ats_pref_task, bp);
}


static void 
do_benchmark ()
{
	int c_m;
	struct BenchmarkPeer *s;
	struct BenchmarkPeer *bp;

	if ((state.connected_ATS_service == GNUNET_NO) ||
			(state.connected_CORE_service == GNUNET_NO) ||
			(state.connected_PEERS == GNUNET_NO) ||
			(state.connected_CORE == GNUNET_NO))
		return;

	state.benchmarking = GNUNET_YES;
	GNUNET_log (GNUNET_ERROR_TYPE_INFO,
			_("Benchmarking start\n"));

	if (GNUNET_SCHEDULER_NO_TASK != shutdown_task)
		GNUNET_SCHEDULER_cancel (shutdown_task);
	shutdown_task = GNUNET_SCHEDULER_add_delayed (BENCHMARK_DURATION, &do_shutdown, NULL);

	/* Start sending test messages */
	for (c_m = 0; c_m < c_master_peers; c_m ++)
	{
		bp = &bp_master[c_m];
		s = get_next (bp);
		if (0 == memcmp(&s->id, &bp->destination->id, sizeof (struct GNUNET_PeerIdentity)))
			bp->partner_msg = GNUNET_YES;
		bp->cth = GNUNET_CORE_notify_transmit_ready (bp->ch,
					GNUNET_NO, 0, GNUNET_TIME_UNIT_MINUTES,
					&s->id,
					TEST_MESSAGE_SIZE, &core_send_ready, bp);
		bp->ats_task = GNUNET_SCHEDULER_add_delayed (GNUNET_TIME_UNIT_SECONDS,
				&ats_pref_task, bp);
	}


}


static void 
connect_completion_callback (void *cls,
			     struct GNUNET_TESTBED_Operation *op,
			     const char *emsg)
{
	struct ConnectOperation *cop = cls;
	static int ops = 0 ;
	int c;
	if (NULL == emsg)
	{
		GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
				_("Connected master peer %u with peer %u\n"), cop->master->no, cop->slave->no);
	}
	else
	{
		GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
				_("Failed to connect master peer%u with peer %u\n"), cop->master->no, cop->slave->no);
		GNUNET_break (0);
		if (GNUNET_SCHEDULER_NO_TASK != shutdown_task)
			GNUNET_SCHEDULER_cancel(shutdown_task);
		shutdown_task = GNUNET_SCHEDULER_add_now (do_shutdown, NULL);
	}
	GNUNET_TESTBED_operation_done(op);
	ops++;
	for (c = 0; c < c_slave_peers; c++)
	{
		if (cop == &cop->master->connect_ops[c])
			cop->master->connect_ops[c].connect_op = NULL;
	}
	if (ops == c_master_peers * c_slave_peers)
	{
		state.connected_PEERS = GNUNET_YES;
		GNUNET_SCHEDULER_add_now (&do_benchmark, NULL);
	}
}


static void
do_connect_peers (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
	int c_m;
	int c_s;
	struct BenchmarkPeer *bp;

	if ((state.connected_ATS_service == GNUNET_NO) ||
			(state.connected_CORE_service == GNUNET_NO))
	{
		return;
	}

	GNUNET_log (GNUNET_ERROR_TYPE_INFO, _("Connecting peers on CORE level\n"));

	for (c_m = 0; c_m < c_master_peers; c_m ++)
	{
		bp = &bp_master[c_m];
		bp->connect_ops = GNUNET_malloc (c_slave_peers * sizeof (struct ConnectOperation));

		for (c_s = 0; c_s < c_slave_peers; c_s ++)
		{
			GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, _("Connecting master peer %u with slave peer %u\n"),
					bp->no, bp_slaves[c_s].no);

			bp->connect_ops[c_s].master = bp;
			bp->connect_ops[c_s].slave = &bp_slaves[c_s];
			bp->connect_ops[c_s].connect_op = GNUNET_TESTBED_overlay_connect( NULL,
					&connect_completion_callback,
					&bp->connect_ops[c_s],
					bp_slaves[c_s].peer,
					bp->peer);

			if (NULL == bp->connect_ops[c_s].connect_op)
			{
				GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
						_("Could not connect master peer %u and slave peer %u\n"),
						bp->no, bp_slaves[c_s].no);
				GNUNET_break (0);
				if (GNUNET_SCHEDULER_NO_TASK != shutdown_task)
					GNUNET_SCHEDULER_cancel(shutdown_task);
				shutdown_task = GNUNET_SCHEDULER_add_now (do_shutdown, NULL);
				return;
			}
		}
	}
}


/**
 * Controller event callback
 *
 * @param cls NULL
 * @param event the controller event
 */
static void
controller_event_cb (void *cls,
                     const struct GNUNET_TESTBED_EventInformation *event)
{
	//struct BenchmarkPeer *p = cls;
  switch (event->type)
  {
  case GNUNET_TESTBED_ET_CONNECT:
    break;
  case GNUNET_TESTBED_ET_OPERATION_FINISHED:
    break;
  default:
    GNUNET_break (0);
    result = 2;
    GNUNET_SCHEDULER_cancel (shutdown_task);
    shutdown_task = GNUNET_SCHEDULER_add_now (&do_shutdown, NULL);
  }
}

/**
 * Method called whenever a given peer connects.
 *
 * @param cls closure
 * @param peer peer identity this notification is about
 */
static void
core_connect_cb (void *cls, const struct GNUNET_PeerIdentity * peer)
{
  struct BenchmarkPeer *p = cls;
  struct BenchmarkPeer *t;
  char *id;
  int c;
  int cs;

  t = find_peer (peer);
  if (NULL == t)
  {
  	GNUNET_break (0);
  	return;
  }

  id = GNUNET_strdup (GNUNET_i2s (&p->id));
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
	      "%s %s connected to %s %s\n",
	      (p->master == GNUNET_YES) ? "Master": "Slave",
	      id,
	      (t->master == GNUNET_YES) ? "Master": "Slave",
	      GNUNET_i2s (peer));

  p->core_connections ++;
  if ((GNUNET_YES == p->master) && (GNUNET_NO == t->master) && (GNUNET_NO == state.connected_CORE))
  {
  	p->slave_connections ++;

		if (p->slave_connections == c_slave_peers)
		{
			GNUNET_log (GNUNET_ERROR_TYPE_INFO,
					"Master %u connected all slaves\n", p->no);
		}
		cs = GNUNET_YES;
		for (c = 0; c < c_master_peers; c ++)
		{
			if (bp_master[c].slave_connections != c_slave_peers)
				cs = GNUNET_NO;
		}
		if (GNUNET_YES == cs)
		{
			GNUNET_log (GNUNET_ERROR_TYPE_INFO,
			"All master peers connected all slave peers\n", id, GNUNET_i2s (peer));
			state.connected_CORE = GNUNET_YES;
			GNUNET_SCHEDULER_add_now (&do_benchmark, NULL);
		}
	}
	GNUNET_free (id);
}


/**
 * Method called whenever a peer disconnects.
 *
 * @param cls closure
 * @param peer peer identity this notification is about
 */
static void
core_disconnect_cb (void *cls, const struct GNUNET_PeerIdentity * peer)
{
  struct BenchmarkPeer *p = cls;
  struct BenchmarkPeer *t;
  char *id;

  t = find_peer (peer);
  if (NULL == t)
  {
  	GNUNET_break (0);
  	return;
  }

  id = GNUNET_strdup (GNUNET_i2s (&p->id));
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
	      "%s disconnected from %s \n", id, GNUNET_i2s (peer));
  GNUNET_assert (p->core_connections > 0);
  p->core_connections --;

  if ((GNUNET_YES == state.benchmarking) &&
  		((GNUNET_YES == p->master) || (GNUNET_YES == t->master)))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
  	      "%s disconnected from %s while benchmarking \n", id, GNUNET_i2s (peer));
  }

  GNUNET_free (id);
}

static size_t
core_send_echo_queued_ready (void *cls, size_t size, void *buf);

static size_t
core_send_echo_ready (void *cls, size_t size, void *buf)
{
	static char msgbuf[TEST_MESSAGE_SIZE];
	struct BenchmarkPeer *bp = cls;
	struct GNUNET_MessageHeader *msg;

	bp->cth = NULL;

	msg = (struct GNUNET_MessageHeader *) &msgbuf;
	memset (&msgbuf, 'a', TEST_MESSAGE_SIZE);
	msg->type = htons (TEST_MESSAGE_TYPE_PONG);
	msg->size = htons (TEST_MESSAGE_SIZE);
	memcpy (buf, msg, TEST_MESSAGE_SIZE);

	/* send echo */
	if (NULL != bp->p_head)
		bp->cth = GNUNET_CORE_notify_transmit_ready (bp->ch,
				GNUNET_NO, 0, GNUNET_TIME_UNIT_MINUTES,
				&bp->p_head->target,
				TEST_MESSAGE_SIZE, &core_send_echo_queued_ready, bp);

	return TEST_MESSAGE_SIZE;
}

static size_t
core_send_echo_queued_ready (void *cls, size_t size, void *buf)
{
	struct BenchmarkPeer *bp = cls;
	struct PendingMessages *pm;
	GNUNET_assert (NULL != bp->p_head);

	pm = bp->p_head;
	GNUNET_CONTAINER_DLL_remove (bp->p_head, bp->p_tail, pm);
	GNUNET_free (pm);


	return core_send_echo_ready (cls, size, buf);

}


static int
core_handle_ping (void *cls, const struct GNUNET_PeerIdentity *other,
                const struct GNUNET_MessageHeader *message)
{
	struct BenchmarkPeer *me = cls;
	struct BenchmarkPeer *remote;
	struct PendingMessages *pm;

	remote = find_peer (other);

	if (NULL == remote)
	{
		GNUNET_break (0);
		return GNUNET_SYSERR;
	}

	if (NULL != me->cth)
	{
		pm = GNUNET_malloc (sizeof (struct PendingMessages));
		pm->target = (*other);
		GNUNET_CONTAINER_DLL_insert_tail (me->p_head, me->p_tail, pm);
		return GNUNET_OK;
	}

	if (GNUNET_NO == remote->master)
	{
		GNUNET_break (0);
		return GNUNET_OK;
	}

	me->messages_received ++;
	/* send echo */
	me->cth = GNUNET_CORE_notify_transmit_ready (me->ch,
				GNUNET_NO, 0, GNUNET_TIME_UNIT_MINUTES,
				&remote->id,
				TEST_MESSAGE_SIZE, &core_send_echo_ready, me);
	return GNUNET_OK;
}

static int
core_handle_pong (void *cls, const struct GNUNET_PeerIdentity *other,
                const struct GNUNET_MessageHeader *message)
{
	struct BenchmarkPeer *me = cls;
	struct BenchmarkPeer *remote;
	struct BenchmarkPeer *next;

	remote = find_peer (other);

	if (NULL == remote)
	{
		GNUNET_break (0);
		return GNUNET_SYSERR;
	}

	if (NULL != me->cth)
	{
		GNUNET_break (0);
		return GNUNET_OK;
	}

	if (GNUNET_YES == remote->master)
	{
		GNUNET_break (0);
		return GNUNET_OK;
	}
	me->messages_received ++;
	next = get_next (me);
	if (0 == memcmp(&remote->id, &me->destination->id, sizeof (struct GNUNET_PeerIdentity)))
			me->partner_msg = GNUNET_YES;
	me->cth = GNUNET_CORE_notify_transmit_ready (me->ch,
				GNUNET_NO, 0, GNUNET_TIME_UNIT_MINUTES,
				&next->id,
				TEST_MESSAGE_SIZE, &core_send_ready, me);

	return GNUNET_OK;
}


/**
 * Called to open a connection to the peer's ATS performance
 *
 * @param cls peer context
 * @param cfg configuration of the peer to connect to; will be available until
 *          GNUNET_TESTBED_operation_done() is called on the operation returned
 *          from GNUNET_TESTBED_service_connect()
 * @return service handle to return in 'op_result', NULL on error
 */
static void *
core_connect_adapter (void *cls,
                      const struct GNUNET_CONFIGURATION_Handle *cfg)
{
  struct BenchmarkPeer *peer = cls;

  static const struct GNUNET_CORE_MessageHandler handlers[] = {
      {&core_handle_ping, TEST_MESSAGE_TYPE_PING, 0},
      {&core_handle_pong, TEST_MESSAGE_TYPE_PONG, 0},
      {NULL, 0, 0}
  };

  peer->ch = GNUNET_CORE_connect(cfg, peer, NULL,
				 core_connect_cb, core_disconnect_cb,
				 NULL, GNUNET_NO, NULL, GNUNET_NO, handlers);
  if (NULL == peer->ch)
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
		"Failed to create core connection \n");
  return peer->ch;
}


/**
 * Callback to be called when a service connect operation is completed
 *
 * @param cls the callback closure from functions generating an operation
 * @param op the operation that has been finished
 * @param ca_result the service handle returned from GNUNET_TESTBED_ConnectAdapter()
 * @param emsg error message in case the operation has failed; will be NULL if
 *          operation has executed successfully.
 */
static void
core_connect_completion_cb (void *cls,
			    struct GNUNET_TESTBED_Operation *op,
			    void *ca_result,
			    const char *emsg )
{
	static int core_done = 0;
	if ((NULL != emsg) || (NULL == ca_result))
	{
		GNUNET_log (GNUNET_ERROR_TYPE_INFO,
				_("Initialization failed, shutdown\n"));
		GNUNET_break (0);
		if (GNUNET_SCHEDULER_NO_TASK != shutdown_task)
			GNUNET_SCHEDULER_cancel(shutdown_task);
		shutdown_task = GNUNET_SCHEDULER_add_now (do_shutdown, NULL);
		return;
	}
	core_done ++;

	if (core_done == c_slave_peers + c_master_peers)
	{
		GNUNET_log (GNUNET_ERROR_TYPE_INFO,
				"Connected to all CORE services\n");
		state.connected_CORE_service = GNUNET_YES;
		GNUNET_SCHEDULER_add_now (&do_connect_peers, NULL);
	}
}


/**
 * Called to disconnect from peer's statistics service
 *
 * @param cls peer context
 * @param op_result service handle returned from the connect adapter
 */
static void
core_disconnect_adapter (void *cls, void *op_result)
{
  struct BenchmarkPeer *peer = cls;

  GNUNET_CORE_disconnect (peer->ch);
  peer->ch = NULL;
}

static void
do_connect_core (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
	int c_p;
	GNUNET_log (GNUNET_ERROR_TYPE_INFO,
			"Connecting to all CORE services\n");
  for (c_p = 0; c_p < c_master_peers; c_p++)
  {
    bp_master[c_p].core_op = GNUNET_TESTBED_service_connect (NULL,
    							bp_master[c_p].peer, "core",
						      core_connect_completion_cb, NULL,
						      &core_connect_adapter,
						      &core_disconnect_adapter,
						      &bp_master[c_p]);

  }

  for (c_p = 0; c_p < c_slave_peers; c_p++)
  {
    bp_slaves[c_p].core_op = GNUNET_TESTBED_service_connect (NULL,
    						bp_slaves[c_p].peer, "core",
					      core_connect_completion_cb, NULL,
					      &core_connect_adapter,
					      &core_disconnect_adapter,
					      &bp_slaves[c_p]);
  }
}



/**
 * Called to open a connection to the peer's ATS performance
 *
 * @param cls peer context
 * @param cfg configuration of the peer to connect to; will be available until
 *          GNUNET_TESTBED_operation_done() is called on the operation returned
 *          from GNUNET_TESTBED_service_connect()
 * @return service handle to return in 'op_result', NULL on error
 */
static void *
ats_perf_connect_adapter (void *cls,
                      const struct GNUNET_CONFIGURATION_Handle *cfg)
{
  struct BenchmarkPeer *peer = cls;

  peer->p_handle = GNUNET_ATS_performance_init (cfg, &ats_performance_info_cb, peer);
  if (NULL == peer->p_handle)
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "Failed to create ATS performance handle \n");
  return peer->p_handle;
}


/**
 * Called to disconnect from peer's statistics service
 *
 * @param cls peer context
 * @param op_result service handle returned from the connect adapter
 */
static void
ats_perf_disconnect_adapter (void *cls, void *op_result)
{
  struct BenchmarkPeer *peer = cls;

  GNUNET_ATS_performance_done(peer->p_handle);
  peer->p_handle = NULL;
}


/**
 * Callback to be called when a service connect operation is completed
 *
 * @param cls the callback closure from functions generating an operation
 * @param op the operation that has been finished
 * @param ca_result the service handle returned from GNUNET_TESTBED_ConnectAdapter()
 * @param emsg error message in case the operation has failed; will be NULL if
 *          operation has executed successfully.
 */
static void
ats_connect_completion_cb (void *cls,
			   struct GNUNET_TESTBED_Operation *op,
			   void *ca_result,
			   const char *emsg )
{
	static int op_done = 0;
	if ((NULL != emsg) || (NULL == ca_result))
	{
		GNUNET_log (GNUNET_ERROR_TYPE_INFO,
				_("Initialization failed, shutdown\n"));
		GNUNET_break (0);
		if (GNUNET_SCHEDULER_NO_TASK != shutdown_task)
			GNUNET_SCHEDULER_cancel(shutdown_task);
		shutdown_task = GNUNET_SCHEDULER_add_now (do_shutdown, NULL);
		return;
	}

	op_done ++;
	if (op_done == (c_slave_peers + c_master_peers))
	{
		GNUNET_log (GNUNET_ERROR_TYPE_INFO,
				"Connected to all ATS services\n");
		state.connected_ATS_service = GNUNET_YES;
		GNUNET_SCHEDULER_add_now (&do_connect_core, NULL);
	}
}

static void
do_connect_ats (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
	int c_p;
	GNUNET_log (GNUNET_ERROR_TYPE_INFO,
			"Connecting to all ATS services %u\n", c_slave_peers);
  for (c_p = 0; c_p < c_master_peers; c_p++)
  {
    bp_master[c_p].ats_perf_op = GNUNET_TESTBED_service_connect (NULL,
    						bp_master[c_p].peer, "ats",
							  ats_connect_completion_cb, NULL,
							  &ats_perf_connect_adapter,
							  &ats_perf_disconnect_adapter,
							  &bp_master[c_p]);

  }

  for (c_p = 0; c_p < c_slave_peers; c_p++)
  {
    bp_slaves[c_p].ats_perf_op = GNUNET_TESTBED_service_connect (NULL,
    						bp_slaves[c_p].peer, "ats",
							  ats_connect_completion_cb, NULL,
							  &ats_perf_connect_adapter,
							  &ats_perf_disconnect_adapter,
							  &bp_slaves[c_p]);
  }

}


/**
 * Callback to be called when the requested peer information is available
 *
 * @param cb_cls the closure from GNUNET_TETSBED_peer_get_information()
 * @param op the operation this callback corresponds to
 * @param pinfo the result; will be NULL if the operation has failed
 * @param emsg error message if the operation has failed; will be NULL if the
 *          operation is successfull
 */
static void
peerinformation_cb (void *cb_cls,
		    struct GNUNET_TESTBED_Operation *op,
		    const struct GNUNET_TESTBED_PeerInformation*pinfo,
		    const char *emsg)
{
  struct BenchmarkPeer *p = cb_cls;
	static int done = 0;

  if (pinfo->pit == GNUNET_TESTBED_PIT_IDENTITY)
  {
    p->id = *pinfo->result.id;
    GNUNET_log (GNUNET_ERROR_TYPE_INFO,
		"[%c %03u] Peers %s\n",
		(p->master == GNUNET_YES) ? 'M' : 'S', p->no, GNUNET_i2s (&p->id));
  }
  else
  {
    GNUNET_assert (0);
  }
  GNUNET_TESTBED_operation_done (op);
  p->info_op = NULL;
  done++;

  if (done == c_master_peers + c_slave_peers)
  {
		GNUNET_log (GNUNET_ERROR_TYPE_INFO,
				"Retrieved all peer ID, connect to ATS\n");
		state.connected_CORE_service = GNUNET_YES;
		GNUNET_SCHEDULER_add_now (&do_connect_ats, NULL);
  }
}


/**
 * Signature of a main function for a testcase.
 *
 * @param cls closure
 * @param num_peers number of peers in 'peers'
 * @param peers_ handle to peers run in the testbed
 * @param links_succeeded the number of overlay link connection attempts that
 *          succeeded
 * @param links_failed the number of overlay link connection attempts that
 *          failed
 */
static void
test_main (void *cls,
	   struct GNUNET_TESTBED_RunHandle *h,
	   unsigned int num_peers,
	   struct GNUNET_TESTBED_Peer **peers_,
	   unsigned int links_succeeded,
	   unsigned int links_failed)
{
  int c_p;

  GNUNET_log (GNUNET_ERROR_TYPE_INFO,
	      _("Benchmarking solver `%s' on preference `%s' with %u master and %u slave peers\n"),
	      solver, pref_str, c_master_peers, c_slave_peers);

  shutdown_task = GNUNET_SCHEDULER_add_delayed (GNUNET_TIME_relative_multiply(TEST_TIMEOUT, c_master_peers + c_slave_peers), &do_shutdown, NULL);

  GNUNET_assert (NULL == cls);
  GNUNET_assert (c_slave_peers + c_master_peers == num_peers);
  GNUNET_assert (NULL != peers_);

  GNUNET_log (GNUNET_ERROR_TYPE_INFO,
	      _("Initializing... \n"));

  for (c_p = 0; c_p < c_master_peers; c_p++)
  {
    GNUNET_assert (NULL != peers_[c_p]);
    bp_master[c_p].no = c_p;
    bp_master[c_p].send_mask = (1 << c_slave_peers) - 1;
    bp_master[c_p].master = GNUNET_YES;
    bp_master[c_p].peer = peers_[c_p];
    bp_master[c_p].info_op = GNUNET_TESTBED_peer_get_information (bp_master[c_p].peer,
							   GNUNET_TESTBED_PIT_IDENTITY,
							   &peerinformation_cb, &bp_master[c_p]);

    /* Select ATS measurement partner */
    bp_master[c_p].destination = &bp_slaves[c_p];
  }

  for (c_p = 0; c_p < c_slave_peers; c_p++)
  {
    GNUNET_assert (NULL != peers_[c_p + c_master_peers]);
    bp_slaves[c_p].no = c_p + c_master_peers;
    bp_slaves[c_p].master = GNUNET_NO;
    bp_slaves[c_p].peer = peers_[c_p + c_master_peers];
    bp_slaves[c_p].info_op = GNUNET_TESTBED_peer_get_information (bp_slaves[c_p].peer,
							   GNUNET_TESTBED_PIT_IDENTITY, 
							   &peerinformation_cb, &bp_slaves[c_p]);
  }

}


int
main (int argc, char *argv[])
{
  char *tmp;
  char *tmp_sep;
  char *test_name;
  char *conf_name;
  char *dotexe;
  char *prefs[GNUNET_ATS_PreferenceCount] = GNUNET_ATS_PreferenceTypeString;
  int c;

  result = 0;

  /* figure out testname */
  tmp = strstr (argv[0], TESTNAME_PREFIX);
  if (NULL == tmp)
  {
  	fprintf (stderr, "Unable to parse test name `%s'\n", argv[0]);
  	return GNUNET_SYSERR;
  }
  tmp += strlen(TESTNAME_PREFIX);
  solver = GNUNET_strdup (tmp);
  if (NULL != (dotexe = strstr (solver, ".exe")) &&
      dotexe[4] == '\0')
    dotexe[0] = '\0';
  tmp_sep = strchr (solver, '_');
  if (NULL == tmp_sep)
  {
  	fprintf (stderr, "Unable to parse test name `%s'\n", argv[0]);
  	GNUNET_free (solver);
  	return GNUNET_SYSERR;
  }
  tmp_sep[0] = '\0';
  pref_str = GNUNET_strdup(tmp_sep + 1);

  GNUNET_asprintf(&conf_name, "%s%s_%s.conf", TESTNAME_PREFIX, solver, pref_str);
  GNUNET_asprintf(&test_name, "%s%s_%s", TESTNAME_PREFIX, solver, pref_str);

  for (c = 0; c <= strlen (pref_str); c++)
  {
  	pref_str[c] = toupper(pref_str[c]);
  }
  pref_val = -1;
	for (c = 1; c < GNUNET_ATS_PreferenceCount; c++)
	{
		if (0 == strcmp (pref_str, prefs[c]))
		{
			pref_val = c;
			break;
		}
	}
	if (-1 == pref_val)
	{
		fprintf (stderr, "Unknown preference: `%s'\n", pref_str);
	  GNUNET_free (solver);
	  GNUNET_free (pref_str);
	  return -1;
	}

  for (c = 0; c < (argc -1); c++)
  {
  	if (0 == strcmp(argv[c], "-s"))
  		break;
  }
  if (c < argc-1)
  {
    if ((0L != (c_slave_peers = strtol (argv[c + 1], NULL, 10))) && (c_slave_peers >= 1))
      fprintf (stderr, "Starting %u slave peers\n", c_slave_peers);
    else
    	c_slave_peers = DEFAULT_SLAVES_NUM;
  }
  else
  	c_slave_peers = DEFAULT_SLAVES_NUM;

  for (c = 0; c < (argc -1); c++)
  {
  	if (0 == strcmp(argv[c], "-m"))
  		break;
  }
  if (c < argc-1)
  {
    if ((0L != (c_master_peers = strtol (argv[c + 1], NULL, 10))) && (c_master_peers >= 2))
      fprintf (stderr, "Starting %u master peers\n", c_master_peers);
    else
    	c_master_peers = DEFAULT_MASTERS_NUM;
  }
  else
  	c_master_peers = DEFAULT_MASTERS_NUM;

  bp_slaves = GNUNET_malloc (c_slave_peers * sizeof (struct BenchmarkPeer));
  bp_master = GNUNET_malloc (c_master_peers * sizeof (struct BenchmarkPeer));

  state.connected_ATS_service = GNUNET_NO;
  state.connected_CORE_service = GNUNET_NO;
  state.connected_PEERS = GNUNET_NO;

  /* Start topology */
  uint64_t event_mask;
  event_mask = 0;
  event_mask |= (1LL << GNUNET_TESTBED_ET_CONNECT);
  event_mask |= (1LL << GNUNET_TESTBED_ET_OPERATION_FINISHED);
  (void) GNUNET_TESTBED_test_run (test_name,
                                  conf_name, c_slave_peers + c_master_peers,
                                  event_mask, &controller_event_cb, NULL,
                                  &test_main, NULL);

  GNUNET_free (solver);
  GNUNET_free (pref_str);
  GNUNET_free (conf_name);
  GNUNET_free (test_name);
  GNUNET_free (bp_slaves);

  return result;
}

/* end of file perf_ats.c */