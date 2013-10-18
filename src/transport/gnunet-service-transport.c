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
 * @file transport/gnunet-service-transport.c
 * @brief
 * @author Christian Grothoff
 */
#include "platform.h"
#include "gnunet_util_lib.h"
#include "gnunet_hello_lib.h"
#include "gnunet_statistics_service.h"
#include "gnunet_transport_service.h"
#include "gnunet_peerinfo_service.h"
#include "gnunet_ats_service.h"
#include "gnunet-service-transport.h"
#include "gnunet-service-transport_blacklist.h"
#include "gnunet-service-transport_clients.h"
#include "gnunet-service-transport_hello.h"
#include "gnunet-service-transport_neighbours.h"
#include "gnunet-service-transport_plugins.h"
#include "gnunet-service-transport_validation.h"
#include "gnunet-service-transport_manipulation.h"
#include "transport.h"

/* globals */

/**
 * Statistics handle.
 */
struct GNUNET_STATISTICS_Handle *GST_stats;

/**
 * Configuration handle.
 */
const struct GNUNET_CONFIGURATION_Handle *GST_cfg;

/**
 * Configuration handle.
 */
struct GNUNET_PeerIdentity GST_my_identity;

/**
 * Handle to peerinfo service.
 */
struct GNUNET_PEERINFO_Handle *GST_peerinfo;

/**
 * Handle to our service's server.
 */
static struct GNUNET_SERVER_Handle *GST_server;

/**
 * Our private key.
 */
struct GNUNET_CRYPTO_EddsaPrivateKey *GST_my_private_key;

/**
 * ATS handle.
 */
struct GNUNET_ATS_SchedulingHandle *GST_ats;

/**
 * DEBUGGING connection counter
 */
static int connections;

/**
 * Hello address expiration
 */
struct GNUNET_TIME_Relative hello_expiration;


/**
 * Transmit our HELLO message to the given (connected) neighbour.
 *
 * @param cls the 'HELLO' message
 * @param target a connected neighbour
 * @param address the address
 * @param bandwidth_in inbound quota in NBO
 * @param bandwidth_out outbound quota in NBO
 */
static void
transmit_our_hello (void *cls, const struct GNUNET_PeerIdentity *target,
                    const struct GNUNET_HELLO_Address *address,
                    struct GNUNET_BANDWIDTH_Value32NBO bandwidth_in,
                    struct GNUNET_BANDWIDTH_Value32NBO bandwidth_out)
{
  const struct GNUNET_MessageHeader *hello = cls;

  GST_neighbours_send (target, (const char *) hello, ntohs (hello->size),
                       hello_expiration, NULL, NULL);
}


/**
 * My HELLO has changed. Tell everyone who should know.
 *
 * @param cls unused
 * @param hello new HELLO
 */
static void
process_hello_update (void *cls, const struct GNUNET_MessageHeader *hello)
{
  GST_clients_broadcast (hello, GNUNET_NO);
  GST_neighbours_iterate (&transmit_our_hello, (void *) hello);
}



/**
 * We received some payload.  Prepare to pass it on to our clients.
 *
 * @param peer (claimed) identity of the other peer
 * @param address the address
 * @param session session used
 * @param message the message to process
 * @return how long the plugin should wait until receiving more data
 */
static struct GNUNET_TIME_Relative
process_payload (const struct GNUNET_PeerIdentity *peer,
                 const struct GNUNET_HELLO_Address *address,
                 struct Session *session,
                 const struct GNUNET_MessageHeader *message)
{
  struct GNUNET_TIME_Relative ret;
  int do_forward;
  struct InboundMessage *im;
  size_t msg_size = ntohs (message->size);
  size_t size =
      sizeof (struct InboundMessage) + msg_size;
  char buf[size] GNUNET_ALIGN;

  do_forward = GNUNET_SYSERR;
  ret = GST_neighbours_calculate_receive_delay (peer, msg_size, &do_forward);

  if (!GST_neighbours_test_connected (peer))
  {

    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Discarded %u bytes type %u payload from peer `%s'\n", msg_size,
                ntohs (message->type), GNUNET_i2s (peer));

    GNUNET_STATISTICS_update (GST_stats,
                              gettext_noop
                              ("# bytes payload discarded due to not connected peer "),
                              msg_size, GNUNET_NO);
    return ret;
  }

  GST_ats_add_address ((struct GNUNET_HELLO_Address *) address, session);

  if (do_forward != GNUNET_YES)
    return ret;
  im = (struct InboundMessage *) buf;
  im->header.size = htons (size);
  im->header.type = htons (GNUNET_MESSAGE_TYPE_TRANSPORT_RECV);
  im->peer = *peer;
  memcpy (&im[1], message, ntohs (message->size));

  GST_clients_broadcast (&im->header, GNUNET_YES);

  return ret;
}


/**
 * Function called by the transport for each received message.
 * This function should also be called with "NULL" for the
 * message to signal that the other peer disconnected.
 *
 * @param cls closure, const char* with the name of the plugin we received the message from
 * @param peer (claimed) identity of the other peer
 * @param message the message, NULL if we only care about
 *                learning about the delay until we should receive again -- FIXME!
 * @param session identifier used for this session (NULL for plugins
 *                that do not offer bi-directional communication to the sender
 *                using the same "connection")
 * @param sender_address binary address of the sender (if we established the
 *                connection or are otherwise sure of it; should be NULL
 *                for inbound TCP/UDP connections since it it not clear
 *                that we could establish ourselves a connection to that
 *                IP address and get the same system)
 * @param sender_address_len number of bytes in sender_address
 * @return how long the plugin should wait until receiving more data
 *         (plugins that do not support this, can ignore the return value)
 */
struct GNUNET_TIME_Relative
GST_receive_callback (void *cls, const struct GNUNET_PeerIdentity *peer,
                             const struct GNUNET_MessageHeader *message,
                             struct Session *session,
                             const char *sender_address,
                             uint16_t sender_address_len)
{
  const char *plugin_name = cls;
  struct GNUNET_TIME_Relative ret;
  struct GNUNET_HELLO_Address address;
  uint16_t type;

  address.peer = *peer;
  address.address = sender_address;
  address.address_length = sender_address_len;
  address.transport_name = plugin_name;
  ret = GNUNET_TIME_UNIT_ZERO;
  if (NULL == message)
    goto end;
  type = ntohs (message->type);
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Received Message with type %u from peer `%s'\n", type, GNUNET_i2s (peer));

  GNUNET_STATISTICS_update (GST_stats,
                        gettext_noop
                        ("# bytes total received"),
                            ntohs (message->size), GNUNET_NO);
  GST_neighbours_notify_data_recv (peer, &address, session, message);

  switch (type)
  {
  case GNUNET_MESSAGE_TYPE_HELLO_LEGACY:
    /* Legacy HELLO message, discard  */
    return ret;
  case GNUNET_MESSAGE_TYPE_HELLO:
    GST_validation_handle_hello (message);
    return ret;
  case GNUNET_MESSAGE_TYPE_TRANSPORT_PING:
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG | GNUNET_ERROR_TYPE_BULK,
                "Processing `%s' from `%s'\n", "PING",
                (sender_address !=
                 NULL) ? GST_plugins_a2s (&address) : TRANSPORT_SESSION_INBOUND_STRING);
    GST_validation_handle_ping (peer, message, &address, session);
    break;
  case GNUNET_MESSAGE_TYPE_TRANSPORT_PONG:
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG | GNUNET_ERROR_TYPE_BULK,
                "Processing `%s' from `%s'\n", "PONG",
                (sender_address !=
                 NULL) ? GST_plugins_a2s (&address) : TRANSPORT_SESSION_INBOUND_STRING);
    GST_validation_handle_pong (peer, message);
    break;
  case GNUNET_MESSAGE_TYPE_TRANSPORT_SESSION_CONNECT:
    GST_neighbours_handle_connect (message, peer, &address, session);
    break;
  case GNUNET_MESSAGE_TYPE_TRANSPORT_SESSION_CONNECT_ACK:
    GST_neighbours_handle_connect_ack (message, peer, &address, session);
    break;
  case GNUNET_MESSAGE_TYPE_TRANSPORT_SESSION_ACK:
    GST_neighbours_handle_session_ack (message, peer, &address, session);
    break;
  case GNUNET_MESSAGE_TYPE_TRANSPORT_SESSION_DISCONNECT:
    GST_neighbours_handle_disconnect_message (peer, message);
    break;
  case GNUNET_MESSAGE_TYPE_TRANSPORT_SESSION_KEEPALIVE:
    GST_neighbours_keepalive (peer);
    break;
  case GNUNET_MESSAGE_TYPE_TRANSPORT_SESSION_KEEPALIVE_RESPONSE:
    GST_neighbours_keepalive_response (peer);
    break;
  default:
    /* should be payload */
    GNUNET_STATISTICS_update (GST_stats,
                              gettext_noop
                              ("# bytes payload received"),
                              ntohs (message->size), GNUNET_NO);
    GST_neighbours_notify_payload_recv (peer, &address, session, message);
    ret = process_payload (peer, &address, session, message);
    break;
  }
end:
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Allowing receive from peer %s to continue in %s\n",
              GNUNET_i2s (peer),
	      GNUNET_STRINGS_relative_time_to_string (ret, GNUNET_YES));
  return ret;
}


/**
 * Function that will be called for each address the transport
 * is aware that it might be reachable under.  Update our HELLO.
 *
 * @param cls name of the plugin (const char*)
 * @param add_remove should the address added (YES) or removed (NO) from the
 *                   set of valid addresses?
 * @param addr one of the addresses of the host
 *        the specific address format depends on the transport
 * @param addrlen length of the address
 * @param dest_plugin destination plugin to use this address with
 */
static void
plugin_env_address_change_notification (void *cls, int add_remove,
                                        const void *addr, size_t addrlen,
                                        const char *dest_plugin)
{
  struct GNUNET_HELLO_Address address;

  address.peer = GST_my_identity;
  address.transport_name = dest_plugin;
  address.address = addr;
  address.address_length = addrlen;
  GST_hello_modify_addresses (add_remove, &address);
}


/**
 * Function that will be called whenever the plugin internally
 * cleans up a session pointer and hence the service needs to
 * discard all of those sessions as well.  Plugins that do not
 * use sessions can simply omit calling this function and always
 * use NULL wherever a session pointer is needed.  This function
 * should be called BEFORE a potential "TransmitContinuation"
 * from the "TransmitFunction".
 *
 * @param cls closure
 * @param peer which peer was the session for
 * @param session which session is being destoyed
 */
static void
plugin_env_session_end (void *cls, const struct GNUNET_PeerIdentity *peer,
                        struct Session *session)
{
  const char *transport_name = cls;
  struct GNUNET_HELLO_Address address;

  GNUNET_assert (strlen (transport_name) > 0);
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Session %p to peer `%s' ended \n",
              session, GNUNET_i2s (peer));
  if (NULL != session)
    GNUNET_log_from (GNUNET_ERROR_TYPE_DEBUG | GNUNET_ERROR_TYPE_BULK,
                     "transport-ats",
                     "Telling ATS to destroy session %p from peer %s\n",
                     session, GNUNET_i2s (peer));
  address.peer = *peer;
  address.address = NULL;
  address.address_length = 0;
  address.transport_name = transport_name;
  GST_neighbours_session_terminated (peer, session);

  /* Tell ATS that session has ended */
  GNUNET_ATS_address_destroyed (GST_ats, &address, session);
}


/**
 * Function that will be called to figure if an address is an loopback,
 * LAN, WAN etc. address
 *
 * @param cls closure
 * @param addr binary address
 * @param addrlen length of the address
 * @return ATS Information containing the network type
 */
static struct GNUNET_ATS_Information
plugin_env_address_to_type (void *cls,
                            const struct sockaddr *addr,
                            size_t addrlen)
{
  struct GNUNET_ATS_Information ats;
  ats.type = htonl (GNUNET_ATS_NETWORK_TYPE);
  ats.value = htonl (GNUNET_ATS_NET_UNSPECIFIED);
  if (GST_ats == NULL)
  {
    GNUNET_break (0);
    return ats;
  }
  if (((addr->sa_family != AF_INET) && (addrlen != sizeof (struct sockaddr_in))) &&
      ((addr->sa_family != AF_INET6) && (addrlen != sizeof (struct sockaddr_in6))) &&
      (addr->sa_family != AF_UNIX))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR, "Malformed address with length %u `%s'\n",
                addrlen,
                GNUNET_a2s(addr, addrlen));
    GNUNET_break (0);
    return ats;
  }
  return GNUNET_ATS_address_get_type(GST_ats, addr, addrlen);
}


/**
 * Notify ATS about the new address including the network this address is
 * located in.
 *
 * @param address the address
 * @param session the session
 */
void
GST_ats_add_address (const struct GNUNET_HELLO_Address *address,
						 	 	 	 	 struct Session *session)
{
  struct GNUNET_TRANSPORT_PluginFunctions *papi;
	struct GNUNET_ATS_Information ats;
	uint32_t net;

  /* valid new address, let ATS know! */
  if (NULL == address->transport_name)
  {
  	GNUNET_break (0);
  	return;
  }
  if (NULL == (papi = GST_plugins_find (address->transport_name)))
  {
    /* we don't have the plugin for this address */
  	GNUNET_break (0);
  	return;
  }

  if (GNUNET_YES == GNUNET_ATS_session_known (GST_ats, address, session))
  	return;

	net = papi->get_network (NULL, (void *) session);
  if (GNUNET_ATS_NET_UNSPECIFIED == net)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
    						_("Could not obtain a valid network for `%s' %s\n"),
                GNUNET_i2s (&address->peer), GST_plugins_a2s (address));
  	GNUNET_break (0);
  }
	ats.type = htonl (GNUNET_ATS_NETWORK_TYPE);
	ats.value = htonl(net);
	GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
			"Notifying ATS about peer `%s''s new address `%s' session %p in network %s\n",
			GNUNET_i2s (&address->peer),
			(0 == address->address_length) ? "<inbound>" : GST_plugins_a2s (address),
			session,
			GNUNET_ATS_print_network_type(net));
	GNUNET_ATS_address_add (GST_ats,
			address, session, &ats, 1);
}


/**
 * Notify ATS about property changes to an address
 *
 * @param peer the peer
 * @param address the address
 * @param session the session
 * @param ats performance information
 * @param ats_count number of elements in ats
 */
void
GST_ats_update_metrics (const struct GNUNET_PeerIdentity *peer,
			const struct GNUNET_HELLO_Address *address,
			struct Session *session,
			const struct GNUNET_ATS_Information *ats,
			uint32_t ats_count)
{
	struct GNUNET_ATS_Information *ats_new;

  if (GNUNET_NO == GNUNET_ATS_session_known (GST_ats, address, session))
    return;

  /* Call to manipulation to manipulate ATS information */
  ats_new = GST_manipulation_manipulate_metrics (peer, address, session, ats,
      ats_count);
  if (NULL == ats_new)
  {
    GNUNET_break(0);
    return;
  }
  if (GNUNET_NO == GNUNET_ATS_address_update (GST_ats,
      address, session, ats_new, ats_count))
  {
    GNUNET_log(GNUNET_ERROR_TYPE_ERROR,
        _("Address or session unknown: failed to update properties for peer `%s' plugin `%s' address `%s' session %p\n"),
        GNUNET_i2s (peer), address->transport_name, GST_plugins_a2s (address),
        session);
  }
  GNUNET_free(ats_new);
}


/**
 * Function that will be called to figure if an address is an loopback,
 * LAN, WAN etc. address
 *
 * @param cls closure
 * @param peer the peer
 * @param address binary address
 * @param address_len length of the address
 * @param session the session
 * @param ats the ats information to update
 * @param ats_count the number of ats elements
 */
static void
plugin_env_update_metrics (void *cls,
			   const struct GNUNET_PeerIdentity *peer,
			   const void *address,
			   uint16_t address_len,
			   struct Session *session,
			   const struct GNUNET_ATS_Information *ats,
			   uint32_t ats_count)
{
  struct GNUNET_HELLO_Address haddress;
  const char *plugin_name = cls;

	if ((NULL == ats) || (0 == ats_count))
		return;
	GNUNET_assert (NULL != GST_ats);


	haddress.peer = *peer;
	haddress.address = address;
  haddress.address_length = address_len;
  haddress.transport_name = plugin_name;

	GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Updating metrics for peer `%s' address %s session %p\n",
			GNUNET_i2s (peer), GST_plugins_a2s(&haddress), session);
  GST_ats_update_metrics (peer, &haddress, session, ats, ats_count);
}

/**
 * Plugin tells transport service about a new (inbound) session
 *
 * @param cls unused
 * @param peer the peer
 * @param plugin plugin name
 * @param address address
 * @param address_len address length
 * @param session the new session
 * @param ats ats information
 * @param ats_count number of ats information
 */

static void
plugin_env_session_start (void *cls, const struct GNUNET_PeerIdentity *peer,
    const char *plugin, const void *address, uint16_t address_len,
    struct Session *session, const struct GNUNET_ATS_Information *ats,
    uint32_t ats_count)
{
  struct GNUNET_HELLO_Address *addr;
  if (NULL == peer)
  {
    GNUNET_break(0);
    return;
  }
  if (NULL == plugin)
  {
    GNUNET_break(0);
    return;
  }
  if (NULL == session)
  {
    GNUNET_break(0);
    return;
  }

  addr = GNUNET_HELLO_address_allocate (peer, plugin, address, address_len);
  GNUNET_log(GNUNET_ERROR_TYPE_DEBUG,
      "Notification from plugin `%s' about new session %p from peer `%s' address `%s'\n",
      plugin, session, GNUNET_i2s (peer), GST_plugins_a2s (addr));
  GST_ats_add_address (addr, session);

  if (0 < ats_count)
    GST_ats_update_metrics (peer, addr, session, ats, ats_count);
  GNUNET_free(addr);
}

/**
 * Function called by ATS to notify the callee that the
 * assigned bandwidth or address for a given peer was changed.  If the
 * callback is called with address/bandwidth assignments of zero, the
 * ATS disconnect function will still be called once the disconnect
 * actually happened.
 *
 * @param cls closure
 * @param address address to use (for peer given in address)
 * @param session session to use (if available)
 * @param bandwidth_out assigned outbound bandwidth for the connection in NBO,
 * 	0 to disconnect from peer
 * @param bandwidth_in assigned inbound bandwidth for the connection in NBO,
 * 	0 to disconnect from peer
 * @param ats ATS information
 * @param ats_count number of ATS elements
 */
static void
ats_request_address_change (void *cls,
                            const struct GNUNET_HELLO_Address *address,
                            struct Session *session,
                            struct GNUNET_BANDWIDTH_Value32NBO bandwidth_out,
                            struct GNUNET_BANDWIDTH_Value32NBO bandwidth_in,
                            const struct GNUNET_ATS_Information *ats,
                            uint32_t ats_count)
{
  uint32_t bw_in = ntohl (bandwidth_in.value__);
  uint32_t bw_out = ntohl (bandwidth_out.value__);

  /* ATS tells me to disconnect from peer */
  if ((bw_in == 0) && (bw_out == 0))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_INFO,
                "ATS tells me to disconnect from peer `%s'\n",
                GNUNET_i2s (&address->peer));
    GST_neighbours_force_disconnect (&address->peer);
    return;
  }
  GST_neighbours_switch_to_address (&address->peer, address, session, ats,
                                         ats_count, bandwidth_in,
                                         bandwidth_out);
}


/**
 * Function called to notify transport users that another
 * peer connected to us.
 *
 * @param cls closure
 * @param peer the peer that connected
 * @param bandwidth_in inbound bandwidth in NBO
 * @param bandwidth_out outbound bandwidth in NBO
 */
static void
neighbours_connect_notification (void *cls,
                                 const struct GNUNET_PeerIdentity *peer,
                                 struct GNUNET_BANDWIDTH_Value32NBO bandwidth_in,
                                 struct GNUNET_BANDWIDTH_Value32NBO bandwidth_out)
{
  size_t len = sizeof (struct ConnectInfoMessage);
  char buf[len] GNUNET_ALIGN;
  struct ConnectInfoMessage *connect_msg = (struct ConnectInfoMessage *) buf;

  connections++;
  GNUNET_log (GNUNET_ERROR_TYPE_INFO,
              "We are now connected to peer `%s' and %u peers in total\n",
              GNUNET_i2s (peer), connections);

  connect_msg->header.size = htons (sizeof (buf));
  connect_msg->header.type = htons (GNUNET_MESSAGE_TYPE_TRANSPORT_CONNECT);
  connect_msg->id = *peer;
  connect_msg->quota_in = bandwidth_in;
  connect_msg->quota_out = bandwidth_out;
  GST_clients_broadcast (&connect_msg->header, GNUNET_NO);
}


/**
 * Function called to notify transport users that another
 * peer disconnected from us.
 *
 * @param cls closure
 * @param peer the peer that disconnected
 */
static void
neighbours_disconnect_notification (void *cls,
                                    const struct GNUNET_PeerIdentity *peer)
{
  struct DisconnectInfoMessage disconnect_msg;

  connections--;
  GNUNET_log (GNUNET_ERROR_TYPE_INFO,
              "Peer `%s' disconnected and we are connected to %u peers\n",
              GNUNET_i2s (peer), connections);

  GST_manipulation_peer_disconnect (peer);
  disconnect_msg.header.size = htons (sizeof (struct DisconnectInfoMessage));
  disconnect_msg.header.type = htons (GNUNET_MESSAGE_TYPE_TRANSPORT_DISCONNECT);
  disconnect_msg.reserved = htonl (0);
  disconnect_msg.peer = *peer;
  GST_clients_broadcast (&disconnect_msg.header, GNUNET_NO);
}


/**
 * Function called to notify transport users that a neighbour peer changed its
 * active address.
 *
 * @param cls closure
 * @param peer peer this update is about (never NULL)
 * @param address address, NULL on disconnect
 */
static void
neighbours_address_notification (void *cls,
                                 const struct GNUNET_PeerIdentity *peer,
                                 const struct GNUNET_HELLO_Address *address)
{
  GST_clients_broadcast_address_notification (peer, address);
}


/**
 * Function called when the service shuts down.  Unloads our plugins
 * and cancels pending validations.
 *
 * @param cls closure, unused
 * @param tc task context (unused)
 */
static void
shutdown_task (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  GST_neighbours_stop ();
  GST_validation_stop ();
  GST_plugins_unload ();

  GNUNET_ATS_scheduling_done (GST_ats);
  GST_ats = NULL;
  GST_clients_stop ();
  GST_blacklist_stop ();
  GST_hello_stop ();
  GST_manipulation_stop ();

  if (NULL != GST_peerinfo)
  {
    GNUNET_PEERINFO_disconnect (GST_peerinfo);
    GST_peerinfo = NULL;
  }
  if (NULL != GST_stats)
  {
    GNUNET_STATISTICS_destroy (GST_stats, GNUNET_NO);
    GST_stats = NULL;
  }
  if (NULL != GST_my_private_key)
  {
    GNUNET_free (GST_my_private_key);
    GST_my_private_key = NULL;
  }
  GST_server = NULL;
}


/**
 * Initiate transport service.
 *
 * @param cls closure
 * @param server the initialized server
 * @param c configuration to use
 */
static void
run (void *cls, struct GNUNET_SERVER_Handle *server,
     const struct GNUNET_CONFIGURATION_Handle *c)
{
  char *keyfile;
  struct GNUNET_CRYPTO_EddsaPrivateKey *pk;
  long long unsigned int max_fd_cfg;
  int max_fd_rlimit;
  int max_fd;
  int friend_only;

  /* setup globals */
  GST_cfg = c;
  if (GNUNET_OK !=
      GNUNET_CONFIGURATION_get_value_filename (c, "PEER", "PRIVATE_KEY",
                                               &keyfile))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                _
                ("Transport service is lacking key configuration settings.  Exiting.\n"));
    GNUNET_SCHEDULER_shutdown ();
    return;
  }
  if (GNUNET_OK !=
      GNUNET_CONFIGURATION_get_value_time (c, "transport", "HELLO_EXPIRATION",
                                           &hello_expiration))
  {
    hello_expiration = GNUNET_CONSTANTS_HELLO_ADDRESS_EXPIRATION;
  }
  GST_server = server;
  pk = GNUNET_CRYPTO_eddsa_key_create_from_file (keyfile);
  GNUNET_free (keyfile);
  GNUNET_assert (NULL != pk);
  GST_my_private_key = pk;

  GST_stats = GNUNET_STATISTICS_create ("transport", GST_cfg);
  GST_peerinfo = GNUNET_PEERINFO_connect (GST_cfg);
  GNUNET_CRYPTO_eddsa_key_get_public (GST_my_private_key,
						  &GST_my_identity.public_key);
  GNUNET_assert (NULL != GST_my_private_key);

  GNUNET_log (GNUNET_ERROR_TYPE_INFO,
              "My identity is `%4s'\n", GNUNET_i2s (&GST_my_identity));

  GNUNET_SCHEDULER_add_delayed (GNUNET_TIME_UNIT_FOREVER_REL, &shutdown_task,
                                NULL);
  if (NULL == GST_peerinfo)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                _("Could not access PEERINFO service.  Exiting.\n"));
    GNUNET_SCHEDULER_shutdown ();
    return;
  }

  max_fd_rlimit = 0;
  max_fd_cfg = 0;
#if HAVE_GETRLIMIT
  struct rlimit r_file;
  if (0 == getrlimit (RLIMIT_NOFILE, &r_file))
  {
    max_fd_rlimit = r_file.rlim_cur;
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
		"Maximum number of open files was: %u/%u\n", r_file.rlim_cur,
		r_file.rlim_max);
  }
  max_fd_rlimit = (9 * max_fd_rlimit) / 10; /* Keep 10% for rest of transport */
#endif
  GNUNET_CONFIGURATION_get_value_number (GST_cfg, "transport", "MAX_FD", &max_fd_cfg);

  if (max_fd_cfg > max_fd_rlimit)
  	max_fd = max_fd_cfg;
  else
  	max_fd = max_fd_rlimit;
  if (max_fd < DEFAULT_MAX_FDS)
  	max_fd = DEFAULT_MAX_FDS;

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Limiting number of sockets to %u: validation %u, neighbors: %u\n",
              max_fd, (max_fd / 3) , (max_fd / 3) * 2);

  friend_only = GNUNET_CONFIGURATION_get_value_yesno(GST_cfg, "topology","FRIENDS-ONLY");
  if (GNUNET_SYSERR == friend_only)
  	friend_only = GNUNET_NO; /* According to topology defaults */
  /* start subsystems */
  GST_hello_start (friend_only, &process_hello_update, NULL);
  GNUNET_assert (NULL != GST_hello_get());
  GST_blacklist_start (GST_server, GST_cfg, &GST_my_identity);
  GST_ats =
      GNUNET_ATS_scheduling_init (GST_cfg, &ats_request_address_change, NULL);
  GST_manipulation_init (GST_cfg);
  GST_plugins_load (&GST_manipulation_recv,
                    &plugin_env_address_change_notification,
                    &plugin_env_session_start,
                    &plugin_env_session_end,
                    &plugin_env_address_to_type,
                    &plugin_env_update_metrics);
  GST_neighbours_start (NULL,
                        &neighbours_connect_notification,
                        &neighbours_disconnect_notification,
                        &neighbours_address_notification,
                        (max_fd / 3) * 2);
  GST_clients_start (GST_server);
  GST_validation_start ((max_fd / 3));
}


/**
 * The main function for the transport service.
 *
 * @param argc number of arguments from the command line
 * @param argv command line arguments
 * @return 0 ok, 1 on error
 */
int
main (int argc, char *const *argv)
{
  return (GNUNET_OK ==
          GNUNET_SERVICE_run (argc, argv, "transport",
                              GNUNET_SERVICE_OPTION_NONE, &run, NULL)) ? 0 : 1;
}

/* end of file gnunet-service-transport.c */
