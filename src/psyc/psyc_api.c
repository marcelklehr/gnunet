/*
 * This file is part of GNUnet
 * (C) 2013 Christian Grothoff (and other contributing authors)
 *
 * GNUnet is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 3, or (at your
 * option) any later version.
 *
 * GNUnet is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNUnet; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * @file psyc/psyc_api.c
 * @brief PSYC service; high-level access to the PSYC protocol
 *        note that clients of this API are NOT expected to
 *        understand the PSYC message format, only the semantics!
 *        Parsing (and serializing) the PSYC stream format is done
 *        within the implementation of the libgnunetpsyc library,
 *        and this API deliberately exposes as little as possible
 *        of the actual data stream format to the application!
 * @author Gabor X Toth
 */

#include <inttypes.h>

#include "platform.h"
#include "gnunet_util_lib.h"
#include "gnunet_env_lib.h"
#include "gnunet_multicast_service.h"
#include "gnunet_psyc_service.h"
#include "gnunet_psyc_util_lib.h"
#include "psyc.h"

#define LOG(kind,...) GNUNET_log_from (kind, "psyc-api",__VA_ARGS__)


/**
 * Handle to access PSYC channel operations for both the master and slaves.
 */
struct GNUNET_PSYC_Channel
{
  /**
   * Configuration to use.
   */
  const struct GNUNET_CONFIGURATION_Handle *cfg;

  /**
   * Client connection to the service.
   */
  struct GNUNET_CLIENT_MANAGER_Connection *client;

  /**
   * Transmission handle;
   */
  struct GNUNET_PSYC_TransmitHandle *tmit;

  /**
   * Receipt handle;
   */
  struct GNUNET_PSYC_ReceiveHandle *recv;

  /**
   * Message to send on reconnect.
   */
  struct GNUNET_MessageHeader *connect_msg;

  /**
   * Are we polling for incoming messages right now?
   */
  uint8_t in_receive;

  /**
   * Is this a master or slave channel?
   */
  uint8_t is_master;
};


/**
 * Handle for the master of a PSYC channel.
 */
struct GNUNET_PSYC_Master
{
  struct GNUNET_PSYC_Channel chn;

  GNUNET_PSYC_MasterStartCallback start_cb;

  /**
   * Join request callback.
   */
  GNUNET_PSYC_JoinRequestCallback join_req_cb;

  /**
   * Closure for the callbacks.
   */
  void *cb_cls;
};


/**
 * Handle for a PSYC channel slave.
 */
struct GNUNET_PSYC_Slave
{
  struct GNUNET_PSYC_Channel chn;

  GNUNET_PSYC_SlaveConnectCallback connect_cb;

  GNUNET_PSYC_JoinDecisionCallback join_dcsn_cb;

  /**
   * Closure for the callbacks.
   */
  void *cb_cls;
};


/**
 * Handle that identifies a join request.
 *
 * Used to match calls to #GNUNET_PSYC_JoinRequestCallback to the
 * corresponding calls to GNUNET_PSYC_join_decision().
 */
struct GNUNET_PSYC_JoinHandle
{
  struct GNUNET_PSYC_Master *mst;
  struct GNUNET_CRYPTO_EddsaPublicKey slave_key;
};


/**
 * Handle for a pending PSYC transmission operation.
 */
struct GNUNET_PSYC_SlaveTransmitHandle
{

};


/**
 * Handle to a story telling operation.
 */
struct GNUNET_PSYC_Story
{

};


/**
 * Handle for a state query operation.
 */
struct GNUNET_PSYC_StateQuery
{

};


static void
channel_send_connect_msg (struct GNUNET_PSYC_Channel *chn)
{
  uint16_t cmsg_size = ntohs (chn->connect_msg->size);
  struct GNUNET_MessageHeader * cmsg = GNUNET_malloc (cmsg_size);
  memcpy (cmsg, chn->connect_msg, cmsg_size);
  GNUNET_CLIENT_MANAGER_transmit_now (chn->client, cmsg);
}


static void
channel_recv_disconnect (void *cls,
                         struct GNUNET_CLIENT_MANAGER_Connection *client,
                         const struct GNUNET_MessageHeader *msg)
{
  struct GNUNET_PSYC_Channel *
    chn = GNUNET_CLIENT_MANAGER_get_user_context_ (client, sizeof (*chn));
  GNUNET_CLIENT_MANAGER_reconnect (client);
  channel_send_connect_msg (chn);
}


static void
channel_recv_message (void *cls,
                      struct GNUNET_CLIENT_MANAGER_Connection *client,
                      const struct GNUNET_MessageHeader *msg)
{
  struct GNUNET_PSYC_Channel *
    chn = GNUNET_CLIENT_MANAGER_get_user_context_ (client, sizeof (*chn));
  GNUNET_PSYC_receive_message (chn->recv,
                               (const struct GNUNET_PSYC_MessageHeader *) msg);
}


static void
channel_recv_message_ack (void *cls,
                          struct GNUNET_CLIENT_MANAGER_Connection *client,
                          const struct GNUNET_MessageHeader *msg)
{
  struct GNUNET_PSYC_Channel *
    chn = GNUNET_CLIENT_MANAGER_get_user_context_ (client, sizeof (*chn));
  GNUNET_PSYC_transmit_got_ack (chn->tmit);
}


static void
master_recv_start_ack (void *cls,
                       struct GNUNET_CLIENT_MANAGER_Connection *client,
                       const struct GNUNET_MessageHeader *msg)
{
  struct GNUNET_PSYC_Master *
    mst = GNUNET_CLIENT_MANAGER_get_user_context_ (client,
                                                   sizeof (struct GNUNET_PSYC_Channel));

  struct CountersResult *cres = (struct CountersResult *) msg;
  if (NULL != mst->start_cb)
    mst->start_cb (mst->cb_cls, GNUNET_ntohll (cres->max_message_id));
}


static void
master_recv_join_request (void *cls,
                          struct GNUNET_CLIENT_MANAGER_Connection *client,
                          const struct GNUNET_MessageHeader *msg)
{
  struct GNUNET_PSYC_Master *
    mst = GNUNET_CLIENT_MANAGER_get_user_context_ (client,
                                                   sizeof (struct GNUNET_PSYC_Channel));

  const struct MasterJoinRequest *req = (const struct MasterJoinRequest *) msg;

  struct GNUNET_PSYC_MessageHeader *pmsg = NULL;
  if (ntohs (req->header.size) <= sizeof (*req) + sizeof (*pmsg))
    pmsg = (struct GNUNET_PSYC_MessageHeader *) &req[1];

  struct GNUNET_PSYC_JoinHandle *jh = GNUNET_malloc (sizeof (*jh));
  jh->mst = mst;
  jh->slave_key = req->slave_key;

  if (NULL != mst->join_req_cb)
    mst->join_req_cb (mst->cb_cls, &req->slave_key, pmsg, jh);
}


static void
slave_recv_join_ack (void *cls,
                     struct GNUNET_CLIENT_MANAGER_Connection *client,
                     const struct GNUNET_MessageHeader *msg)
{
  struct GNUNET_PSYC_Slave *
    slv = GNUNET_CLIENT_MANAGER_get_user_context_ (client,
                                                   sizeof (struct GNUNET_PSYC_Channel));
  struct CountersResult *cres = (struct CountersResult *) msg;
  if (NULL != slv->connect_cb)
    slv->connect_cb (slv->cb_cls, GNUNET_ntohll (cres->max_message_id));
}


static void
slave_recv_join_decision (void *cls,
                          struct GNUNET_CLIENT_MANAGER_Connection *client,
                          const struct GNUNET_MessageHeader *msg)
{
  struct GNUNET_PSYC_Slave *
    slv = GNUNET_CLIENT_MANAGER_get_user_context_ (client,
                                                   sizeof (struct GNUNET_PSYC_Channel));
  const struct SlaveJoinDecision *
    dcsn = (const struct SlaveJoinDecision *) msg;

  struct GNUNET_PSYC_MessageHeader *pmsg = NULL;
  if (ntohs (dcsn->header.size) <= sizeof (*dcsn) + sizeof (*pmsg))
    pmsg = (struct GNUNET_PSYC_MessageHeader *) &dcsn[1];

  struct GNUNET_PSYC_JoinHandle *jh = GNUNET_malloc (sizeof (*jh));
  if (NULL != slv->join_dcsn_cb)
    slv->join_dcsn_cb (slv->cb_cls, ntohl (dcsn->is_admitted), pmsg);
}


static struct GNUNET_CLIENT_MANAGER_MessageHandler master_handlers[] =
{
  { &channel_recv_disconnect, NULL, 0, 0, GNUNET_NO },

  { &channel_recv_message, NULL,
    GNUNET_MESSAGE_TYPE_PSYC_MESSAGE,
    sizeof (struct GNUNET_PSYC_MessageHeader), GNUNET_YES },

  { &channel_recv_message_ack, NULL,
    GNUNET_MESSAGE_TYPE_PSYC_MESSAGE_ACK,
    sizeof (struct GNUNET_MessageHeader), GNUNET_NO },

  { &master_recv_start_ack, NULL,
    GNUNET_MESSAGE_TYPE_PSYC_MASTER_START_ACK,
    sizeof (struct CountersResult), GNUNET_NO },

  { &master_recv_join_request, NULL,
    GNUNET_MESSAGE_TYPE_PSYC_JOIN_REQUEST,
    sizeof (struct MasterJoinRequest), GNUNET_YES },

  { NULL, NULL, 0, 0, GNUNET_NO }
};


static struct GNUNET_CLIENT_MANAGER_MessageHandler slave_handlers[] =
{
  { &channel_recv_disconnect, NULL, 0, 0, GNUNET_NO },

  { &channel_recv_message, NULL,
    GNUNET_MESSAGE_TYPE_PSYC_MESSAGE,
    sizeof (struct GNUNET_PSYC_MessageHeader), GNUNET_YES },

  { &channel_recv_message_ack, NULL,
    GNUNET_MESSAGE_TYPE_PSYC_MESSAGE_ACK,
    sizeof (struct GNUNET_MessageHeader), GNUNET_NO },

  { &slave_recv_join_ack, NULL,
    GNUNET_MESSAGE_TYPE_PSYC_SLAVE_JOIN_ACK,
    sizeof (struct CountersResult), GNUNET_NO },

  { &slave_recv_join_decision, NULL,
    GNUNET_MESSAGE_TYPE_PSYC_JOIN_DECISION,
    sizeof (struct SlaveJoinDecision), GNUNET_YES },

  { NULL, NULL, 0, 0, GNUNET_NO }
};


/**
 * Start a PSYC master channel.
 *
 * Will start a multicast group identified by the given ECC key.  Messages
 * received from group members will be given to the respective handler methods.
 * If a new member wants to join a group, the "join" method handler will be
 * invoked; the join handler must then generate a "join" message to approve the
 * joining of the new member.  The channel can also change group membership
 * without explicit requests.  Note that PSYC doesn't itself "understand" join
 * or part messages, the respective methods must call other PSYC functions to
 * inform PSYC about the meaning of the respective events.
 *
 * @param cfg  Configuration to use (to connect to PSYC service).
 * @param channel_key  ECC key that will be used to sign messages for this
 *        PSYC session. The public key is used to identify the PSYC channel.
 *        Note that end-users will usually not use the private key directly, but
 *        rather look it up in GNS for places managed by other users, or select
 *        a file with the private key(s) when setting up their own channels
 *        FIXME: we'll likely want to use NOT the p521 curve here, but a cheaper
 *        one in the future.
 * @param policy  Channel policy specifying join and history restrictions.
 *        Used to automate join decisions.
 * @param message_cb  Function to invoke on message parts received from slaves.
 * @param join_request_cb  Function to invoke when a slave wants to join.
 * @param master_start_cb  Function to invoke after the channel master started.
 * @param cls  Closure for @a method and @a join_cb.
 *
 * @return Handle for the channel master, NULL on error.
 */
struct GNUNET_PSYC_Master *
GNUNET_PSYC_master_start (const struct GNUNET_CONFIGURATION_Handle *cfg,
                          const struct GNUNET_CRYPTO_EddsaPrivateKey *channel_key,
                          enum GNUNET_PSYC_Policy policy,
                          GNUNET_PSYC_MasterStartCallback start_cb,
                          GNUNET_PSYC_JoinRequestCallback join_request_cb,
                          GNUNET_PSYC_MessageCallback message_cb,
                          void *cls)
{
  struct GNUNET_PSYC_Master *mst = GNUNET_malloc (sizeof (*mst));
  struct GNUNET_PSYC_Channel *chn = &mst->chn;

  struct MasterStartRequest *req = GNUNET_malloc (sizeof (*req));
  req->header.size = htons (sizeof (*req));
  req->header.type = htons (GNUNET_MESSAGE_TYPE_PSYC_MASTER_START);
  req->channel_key = *channel_key;
  req->policy = policy;

  chn->connect_msg = (struct GNUNET_MessageHeader *) req;
  chn->cfg = cfg;
  chn->is_master = GNUNET_YES;

  mst->start_cb = start_cb;
  mst->join_req_cb = join_request_cb;
  mst->cb_cls = cls;

  chn->client = GNUNET_CLIENT_MANAGER_connect (cfg, "psyc", master_handlers);
  GNUNET_CLIENT_MANAGER_set_user_context_ (chn->client, mst, sizeof (*chn));

  chn->tmit = GNUNET_PSYC_transmit_create (chn->client);
  chn->recv = GNUNET_PSYC_receive_create (message_cb, message_cb, cls);

  channel_send_connect_msg (chn);
  return mst;
}


/**
 * Stop a PSYC master channel.
 *
 * @param master PSYC channel master to stop.
 * @param keep_active  FIXME
 */
void
GNUNET_PSYC_master_stop (struct GNUNET_PSYC_Master *mst)
{
  GNUNET_CLIENT_MANAGER_disconnect (mst->chn.client, GNUNET_YES);
  GNUNET_free (mst);
}


/**
 * Function to call with the decision made for a join request.
 *
 * Must be called once and only once in response to an invocation of the
 * #GNUNET_PSYC_JoinCallback.
 *
 * @param jh Join request handle.
 * @param is_admitted  #GNUNET_YES    if the join is approved,
 *                     #GNUNET_NO     if it is disapproved,
 *                     #GNUNET_SYSERR if we cannot answer the request.
 * @param relay_count Number of relays given.
 * @param relays Array of suggested peers that might be useful relays to use
 *        when joining the multicast group (essentially a list of peers that
 *        are already part of the multicast group and might thus be willing
 *        to help with routing).  If empty, only this local peer (which must
 *        be the multicast origin) is a good candidate for building the
 *        multicast tree.  Note that it is unnecessary to specify our own
 *        peer identity in this array.
 * @param join_resp  Application-dependent join response message.
 *
 * @return #GNUNET_OK on success,
 *         #GNUNET_SYSERR if the message is too large.
 */
int
GNUNET_PSYC_join_decision (struct GNUNET_PSYC_JoinHandle *jh,
                           int is_admitted,
                           uint32_t relay_count,
                           const struct GNUNET_PeerIdentity *relays,
                           const struct GNUNET_PSYC_MessageHeader *join_resp)
{
  struct GNUNET_PSYC_Channel *chn = &jh->mst->chn;
  struct MasterJoinDecision *dcsn;
  uint16_t join_resp_size
    = (NULL != join_resp) ? ntohs (join_resp->header.size) : 0;
  uint16_t relay_size = relay_count * sizeof (*relays);

  if (GNUNET_MULTICAST_FRAGMENT_MAX_PAYLOAD
      < sizeof (*dcsn) + relay_size + join_resp_size)
    return GNUNET_SYSERR;

  dcsn = GNUNET_malloc (sizeof (*dcsn) + relay_size + join_resp_size);
  dcsn->header.size = htons (sizeof (*dcsn) + relay_size + join_resp_size);
  dcsn->header.type = htons (GNUNET_MESSAGE_TYPE_PSYC_JOIN_DECISION);
  dcsn->is_admitted = htonl (is_admitted);
  dcsn->slave_key = jh->slave_key;

  if (0 < join_resp_size)
    memcpy (&dcsn[1], join_resp, join_resp_size);

  GNUNET_CLIENT_MANAGER_transmit (chn->client, &dcsn->header);
  return GNUNET_OK;
}


/**
 * Send a message to call a method to all members in the PSYC channel.
 *
 * @param master Handle to the PSYC channel.
 * @param method_name Which method should be invoked.
 * @param notify_mod Function to call to obtain modifiers.
 * @param notify_data Function to call to obtain fragments of the data.
 * @param notify_cls Closure for @a notify_mod and @a notify_data.
 * @param flags Flags for the message being transmitted.
 *
 * @return Transmission handle, NULL on error (i.e. more than one request queued).
 */
struct GNUNET_PSYC_MasterTransmitHandle *
GNUNET_PSYC_master_transmit (struct GNUNET_PSYC_Master *mst,
                             const char *method_name,
                             GNUNET_PSYC_TransmitNotifyModifier notify_mod,
                             GNUNET_PSYC_TransmitNotifyData notify_data,
                             void *notify_cls,
                             enum GNUNET_PSYC_MasterTransmitFlags flags)
{
  if (GNUNET_OK
      == GNUNET_PSYC_transmit_message (mst->chn.tmit, method_name, NULL,
                                       notify_mod, notify_data, notify_cls,
                                       flags))
    return (struct GNUNET_PSYC_MasterTransmitHandle *) mst->chn.tmit;
  else
    return NULL;
}


/**
 * Resume transmission to the channel.
 *
 * @param tmit  Handle of the request that is being resumed.
 */
void
GNUNET_PSYC_master_transmit_resume (struct GNUNET_PSYC_MasterTransmitHandle *tmit)
{
  GNUNET_PSYC_transmit_resume ((struct GNUNET_PSYC_TransmitHandle *) tmit);
}


/**
 * Abort transmission request to the channel.
 *
 * @param tmit  Handle of the request that is being aborted.
 */
void
GNUNET_PSYC_master_transmit_cancel (struct GNUNET_PSYC_MasterTransmitHandle *tmit)
{
  GNUNET_PSYC_transmit_cancel ((struct GNUNET_PSYC_TransmitHandle *) tmit);
}


/**
 * Convert a channel @a master to a @e channel handle to access the @e channel
 * APIs.
 *
 * @param master Channel master handle.
 *
 * @return Channel handle, valid for as long as @a master is valid.
 */
struct GNUNET_PSYC_Channel *
GNUNET_PSYC_master_get_channel (struct GNUNET_PSYC_Master *master)
{
  return &master->chn;
}


/**
 * Join a PSYC channel.
 *
 * The entity joining is always the local peer.  The user must immediately use
 * the GNUNET_PSYC_slave_transmit() functions to transmit a @e join_msg to the
 * channel; if the join request succeeds, the channel state (and @e recent
 * method calls) will be replayed to the joining member.  There is no explicit
 * notification on failure (as the channel may simply take days to approve,
 * and disapproval is simply being ignored).
 *
 * @param cfg  Configuration to use.
 * @param channel_key  ECC public key that identifies the channel we wish to join.
 * @param slave_key  ECC private-public key pair that identifies the slave, and
 *        used by multicast to sign the join request and subsequent unicast
 *        requests sent to the master.
 * @param origin  Peer identity of the origin.
 * @param relay_count  Number of peers in the @a relays array.
 * @param relays  Peer identities of members of the multicast group, which serve
 *        as relays and used to join the group at.
 * @param message_cb  Function to invoke on message parts received from the
 *        channel, typically at least contains method handlers for @e join and
 *        @e part.
 * @param slave_connect_cb  Function invoked once we have connected to the
 *        PSYC service.
 * @param join_decision_cb  Function invoked once we have received a join
 *	  decision.
 * @param cls  Closure for @a message_cb and @a slave_joined_cb.
 * @param method_name  Method name for the join request.
 * @param env  Environment containing transient variables for the request, or NULL.
 * @param data  Payload for the join message.
 * @param data_size  Number of bytes in @a data.
 *
 * @return Handle for the slave, NULL on error.
 */
struct GNUNET_PSYC_Slave *
GNUNET_PSYC_slave_join (const struct GNUNET_CONFIGURATION_Handle *cfg,
                        const struct GNUNET_CRYPTO_EddsaPublicKey *channel_key,
                        const struct GNUNET_CRYPTO_EddsaPrivateKey *slave_key,
                        const struct GNUNET_PeerIdentity *origin,
                        uint32_t relay_count,
                        const struct GNUNET_PeerIdentity *relays,
                        GNUNET_PSYC_MessageCallback message_cb,
                        GNUNET_PSYC_SlaveConnectCallback connect_cb,
                        GNUNET_PSYC_JoinDecisionCallback join_decision_cb,
                        void *cls,
                        const char *method_name,
                        const struct GNUNET_ENV_Environment *env,
                        const void *data,
                        uint16_t data_size)
{
  struct GNUNET_PSYC_Slave *slv = GNUNET_malloc (sizeof (*slv));
  struct GNUNET_PSYC_Channel *chn = &slv->chn;
  struct SlaveJoinRequest *req
    = GNUNET_malloc (sizeof (*req) + relay_count * sizeof (*relays));
  req->header.size = htons (sizeof (*req)
                            + relay_count * sizeof (*relays));
  req->header.type = htons (GNUNET_MESSAGE_TYPE_PSYC_SLAVE_JOIN);
  req->channel_key = *channel_key;
  req->slave_key = *slave_key;
  req->origin = *origin;
  req->relay_count = htonl (relay_count);
  memcpy (&req[1], relays, relay_count * sizeof (*relays));

  chn->connect_msg = (struct GNUNET_MessageHeader *) req;
  chn->cfg = cfg;
  chn->is_master = GNUNET_NO;

  slv->connect_cb = connect_cb;
  slv->join_dcsn_cb = join_decision_cb;
  slv->cb_cls = cls;

  chn->client = GNUNET_CLIENT_MANAGER_connect (cfg, "psyc", slave_handlers);
  GNUNET_CLIENT_MANAGER_set_user_context_ (chn->client, slv, sizeof (*chn));

  chn->recv = GNUNET_PSYC_receive_create (message_cb, message_cb, cls);
  chn->tmit = GNUNET_PSYC_transmit_create (chn->client);

  channel_send_connect_msg (chn);
  return slv;
}


/**
 * Part a PSYC channel.
 *
 * Will terminate the connection to the PSYC service.  Polite clients should
 * first explicitly send a part request (via GNUNET_PSYC_slave_transmit()).
 *
 * @param slave Slave handle.
 */
void
GNUNET_PSYC_slave_part (struct GNUNET_PSYC_Slave *slv)
{
  GNUNET_CLIENT_MANAGER_disconnect (slv->chn.client, GNUNET_YES);
  GNUNET_free (slv);
}


/**
 * Request a message to be sent to the channel master.
 *
 * @param slave Slave handle.
 * @param method_name Which (PSYC) method should be invoked (on host).
 * @param notify_mod Function to call to obtain modifiers.
 * @param notify_data Function to call to obtain fragments of the data.
 * @param notify_cls Closure for @a notify.
 * @param flags Flags for the message being transmitted.
 *
 * @return Transmission handle, NULL on error (i.e. more than one request
 *         queued).
 */
struct GNUNET_PSYC_SlaveTransmitHandle *
GNUNET_PSYC_slave_transmit (struct GNUNET_PSYC_Slave *slv,
                            const char *method_name,
                            GNUNET_PSYC_TransmitNotifyModifier notify_mod,
                            GNUNET_PSYC_TransmitNotifyData notify_data,
                            void *notify_cls,
                            enum GNUNET_PSYC_SlaveTransmitFlags flags)

{
  if (GNUNET_OK
      == GNUNET_PSYC_transmit_message (slv->chn.tmit, method_name, NULL,
                                       notify_mod, notify_data, notify_cls,
                                       flags))
    return (struct GNUNET_PSYC_SlaveTransmitHandle *) slv->chn.tmit;
  else
    return NULL;
}


/**
 * Resume transmission to the master.
 *
 * @param tmit Handle of the request that is being resumed.
 */
void
GNUNET_PSYC_slave_transmit_resume (struct GNUNET_PSYC_SlaveTransmitHandle *tmit)
{
  GNUNET_PSYC_transmit_resume ((struct GNUNET_PSYC_TransmitHandle *) tmit);
}


/**
 * Abort transmission request to master.
 *
 * @param tmit Handle of the request that is being aborted.
 */
void
GNUNET_PSYC_slave_transmit_cancel (struct GNUNET_PSYC_SlaveTransmitHandle *tmit)
{
  GNUNET_PSYC_transmit_cancel ((struct GNUNET_PSYC_TransmitHandle *) tmit);
}


/**
 * Convert @a slave to a @e channel handle to access the @e channel APIs.
 *
 * @param slv Slave handle.
 *
 * @return Channel handle, valid for as long as @a slave is valid.
 */
struct GNUNET_PSYC_Channel *
GNUNET_PSYC_slave_get_channel (struct GNUNET_PSYC_Slave *slv)
{
  return &slv->chn;
}


/**
 * Add a slave to the channel's membership list.
 *
 * Note that this will NOT generate any PSYC traffic, it will merely update the
 * local database to modify how we react to <em>membership test</em> queries.
 * The channel master still needs to explicitly transmit a @e join message to
 * notify other channel members and they then also must still call this function
 * in their respective methods handling the @e join message.  This way, how @e
 * join and @e part operations are exactly implemented is still up to the
 * application; for example, there might be a @e part_all method to kick out
 * everyone.
 *
 * Note that channel slaves are explicitly trusted to execute such methods
 * correctly; not doing so correctly will result in either denying other slaves
 * access or offering access to channel data to non-members.
 *
 * @param channel Channel handle.
 * @param slave_key Identity of channel slave to add.
 * @param announced_at ID of the message that announced the membership change.
 * @param effective_since Addition of slave is in effect since this message ID.
 */
void
GNUNET_PSYC_channel_slave_add (struct GNUNET_PSYC_Channel *chn,
                               const struct GNUNET_CRYPTO_EddsaPublicKey *slave_key,
                               uint64_t announced_at,
                               uint64_t effective_since)
{
  struct ChannelSlaveAdd *add = GNUNET_malloc (sizeof (*add));
  add->header.type = htons (GNUNET_MESSAGE_TYPE_PSYC_CHANNEL_SLAVE_ADD);
  add->header.size = htons (sizeof (*add));
  add->announced_at = GNUNET_htonll (announced_at);
  add->effective_since = GNUNET_htonll (effective_since);
  GNUNET_CLIENT_MANAGER_transmit (chn->client, &add->header);
}


/**
 * Remove a slave from the channel's membership list.
 *
 * Note that this will NOT generate any PSYC traffic, it will merely update the
 * local database to modify how we react to <em>membership test</em> queries.
 * The channel master still needs to explicitly transmit a @e part message to
 * notify other channel members and they then also must still call this function
 * in their respective methods handling the @e part message.  This way, how
 * @e join and @e part operations are exactly implemented is still up to the
 * application; for example, there might be a @e part_all message to kick out
 * everyone.
 *
 * Note that channel members are explicitly trusted to perform these
 * operations correctly; not doing so correctly will result in either
 * denying members access or offering access to channel data to
 * non-members.
 *
 * @param channel Channel handle.
 * @param slave_key Identity of channel slave to remove.
 * @param announced_at ID of the message that announced the membership change.
 */
void
GNUNET_PSYC_channel_slave_remove (struct GNUNET_PSYC_Channel *chn,
                                  const struct GNUNET_CRYPTO_EddsaPublicKey *slave_key,
                                  uint64_t announced_at)
{
  struct ChannelSlaveRemove *rm = GNUNET_malloc (sizeof (*rm));
  rm->header.type = htons (GNUNET_MESSAGE_TYPE_PSYC_CHANNEL_SLAVE_RM);
  rm->header.size = htons (sizeof (*rm));
  rm->announced_at = GNUNET_htonll (announced_at);
  GNUNET_CLIENT_MANAGER_transmit (chn->client, &rm->header);
}


/**
 * Request to be told the message history of the channel.
 *
 * Historic messages (but NOT the state at the time) will be replayed (given to
 * the normal method handlers) if available and if access is permitted.
 *
 * To get the latest message, use 0 for both the start and end message ID.
 *
 * @param channel Which channel should be replayed?
 * @param start_message_id Earliest interesting point in history.
 * @param end_message_id Last (exclusive) interesting point in history.
 * @param message_cb Function to invoke on message parts received from the story.
 * @param finish_cb Function to call when the requested story has been fully
 *        told (counting message IDs might not suffice, as some messages
 *        might be secret and thus the listener would not know the story is
 *        finished without being told explicitly) once this function
 *        has been called, the client must not call
 *        GNUNET_PSYC_channel_story_tell_cancel() anymore.
 * @param cls Closure for the callbacks.
 *
 * @return Handle to cancel story telling operation.
 */
struct GNUNET_PSYC_Story *
GNUNET_PSYC_channel_story_tell (struct GNUNET_PSYC_Channel *channel,
                                uint64_t start_message_id,
                                uint64_t end_message_id,
                                GNUNET_PSYC_MessageCallback message_cb,
                                GNUNET_PSYC_FinishCallback finish_cb,
                                void *cls)
{
  return NULL;
}


/**
 * Abort story telling.
 *
 * This function must not be called from within method handlers (as given to
 * GNUNET_PSYC_slave_join()) of the slave.
 *
 * @param story Story telling operation to stop.
 */
void
GNUNET_PSYC_channel_story_tell_cancel (struct GNUNET_PSYC_Story *story)
{

}


/**
 * Retrieve the best matching channel state variable.
 *
 * If the requested variable name is not present in the state, the nearest
 * less-specific name is matched; for example, requesting "_a_b" will match "_a"
 * if "_a_b" does not exist.
 *
 * @param channel Channel handle.
 * @param full_name Full name of the requested variable, the actual variable
 *        returned might have a shorter name..
 * @param cb Function called once when a matching state variable is found.
 *        Not called if there's no matching state variable.
 * @param cb_cls Closure for the callbacks.
 *
 * @return Handle that can be used to cancel the query operation.
 */
struct GNUNET_PSYC_StateQuery *
GNUNET_PSYC_channel_state_get (struct GNUNET_PSYC_Channel *channel,
                               const char *full_name,
                               GNUNET_PSYC_StateCallback cb,
                               void *cb_cls)
{
  return NULL;
}


/**
 * Return all channel state variables whose name matches a given prefix.
 *
 * A name matches if it starts with the given @a name_prefix, thus requesting
 * the empty prefix ("") will match all values; requesting "_a_b" will also
 * return values stored under "_a_b_c".
 *
 * The @a state_cb is invoked on all matching state variables asynchronously, as
 * the state is stored in and retrieved from the PSYCstore,
 *
 * @param channel Channel handle.
 * @param name_prefix Prefix of the state variable name to match.
 * @param cb Function to call with the matching state variables.
 * @param cb_cls Closure for the callbacks.
 *
 * @return Handle that can be used to cancel the query operation.
 */
struct GNUNET_PSYC_StateQuery *
GNUNET_PSYC_channel_state_get_prefix (struct GNUNET_PSYC_Channel *channel,
                                      const char *name_prefix,
                                      GNUNET_PSYC_StateCallback cb,
                                      void *cb_cls)
{
  return NULL;
}


/**
 * Cancel a state query operation.
 *
 * @param query Handle for the operation to cancel.
 */
void
GNUNET_PSYC_channel_state_get_cancel (struct GNUNET_PSYC_StateQuery *query)
{

}


/* end of psyc_api.c */
