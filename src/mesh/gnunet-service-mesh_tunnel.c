/*
     This file is part of GNUnet.
     (C) 2013 Christian Grothoff (and other contributing authors)

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

#include "platform.h"
#include "gnunet_util_lib.h"

#include "gnunet_statistics_service.h"

#include "mesh_protocol_enc.h"

#include "gnunet-service-mesh_tunnel.h"
#include "gnunet-service-mesh_connection.h"
#include "gnunet-service-mesh_channel.h"
#include "gnunet-service-mesh_peer.h"
#include "mesh_path.h"

#define LOG(level, ...) GNUNET_log_from(level,"mesh-tun",__VA_ARGS__)


/******************************************************************************/
/********************************   STRUCTS  **********************************/
/******************************************************************************/

struct MeshTChannel
{
  struct MeshTChannel *next;
  struct MeshTChannel *prev;
  struct MeshChannel *ch;
};

struct MeshTConnection
{
  struct MeshTConnection *next;
  struct MeshTConnection *prev;
  struct MeshConnection *c;
};

/**
 * Struct containing all information regarding a tunnel to a peer.
 */
struct MeshTunnel3
{
    /**
     * Endpoint of the tunnel.
     */
  struct MeshPeer *peer;

    /**
     * State of the tunnel.
     */
  enum MeshTunnelState state;

  /**
   * Local peer ephemeral private key
   */
  struct GNUNET_CRYPTO_EccPrivateKey *my_eph_key;

  /**
   * Local peer ephemeral public key
   */
  struct GNUNET_CRYPTO_EccPublicSignKey *my_eph;

  /**
   * Remote peer's public key.
   */
  struct GNUNET_CRYPTO_EccPublicSignKey *peers_eph;

  /**
   * Encryption ("our") key.
   */
  struct GNUNET_CRYPTO_SymmetricSessionKey e_key;

  /**
   * Decryption ("their") key.
   */
  struct GNUNET_CRYPTO_SymmetricSessionKey d_key;

  /**
   * Paths that are actively used to reach the destination peer.
   */
  struct MeshTConnection *connection_head;
  struct MeshTConnection *connection_tail;

  /**
   * Next connection number.
   */
  uint32_t next_cid;

  /**
   * Channels inside this tunnel.
   */
  struct MeshTChannel *channel_head;
  struct MeshTChannel *channel_tail;

  /**
   * Channel ID for the next created channel.
   */
  MESH_ChannelNumber next_chid;

  /**
   * Channel ID for the next incoming channel.
   */
  MESH_ChannelNumber next_local_chid;

  /**
   * Pending message count.
   */
  int pending_messages;

  /**
   * Destroy flag: if true, destroy on last message.
   */
  int destroy;

  /**
   * Queued messages, to transmit once tunnel gets connected.
   */
  struct MeshTunnelQueue *tq_head;
  struct MeshTunnelQueue *tq_tail;
};


/**
 * Struct used to queue messages in a tunnel.
 */
struct MeshTunnelQueue
{
  /**
   * DLL
   */
  struct MeshTunnelQueue *next;
  struct MeshTunnelQueue *prev;

  /**
   * Channel.
   */
  struct MeshChannel *ch;

  /**
   * Message to send.
   */
  /* struct GNUNET_MessageHeader *msg; */
};

/******************************************************************************/
/*******************************   GLOBALS  ***********************************/
/******************************************************************************/

/**
 * Global handle to the statistics service.
 */
extern struct GNUNET_STATISTICS_Handle *stats;

/**
 * Default TTL for payload packets.
 */
static unsigned long long default_ttl;

/**
 * Local peer own ID (memory efficient handle).
 */
static GNUNET_PEER_Id my_short_id;

/**
 * Local peer own ID (full value).
 */
const static struct GNUNET_PeerIdentity *my_full_id;

/**
 * Own private key.
 */
const static struct GNUNET_CRYPTO_EccPrivateKey *my_private_key;


/******************************************************************************/
/********************************   STATIC  ***********************************/
/******************************************************************************/

/**
 * Get string description for tunnel state.
 *
 * @param s Tunnel state.
 *
 * @return String representation.
 */
static const char *
GMT_state2s (enum MeshTunnelState s)
{
  static char buf[128];

  switch (s)
  {
    case MESH_TUNNEL_NEW:
      return "MESH_TUNNEL_NEW";
    case MESH_TUNNEL_SEARCHING:
      return "MESH_TUNNEL_SEARCHING";
    case MESH_TUNNEL_WAITING:
      return "MESH_TUNNEL_WAITING";
    case MESH_TUNNEL_READY:
      return "MESH_TUNNEL_READY";
    case MESH_TUNNEL_RECONNECTING:
      return "MESH_TUNNEL_RECONNECTING";

    default:
      sprintf (buf, "%u (UNKNOWN STATE)", s);
      return buf;
  }
}


/**
 * Search for a channel by global ID using full PeerIdentities.
 *
 * @param t Tunnel containing the channel.
 * @param chid Public channel number.
 *
 * @return channel handler, NULL if doesn't exist
 */
static struct MeshChannel *
get_channel (struct MeshTunnel3 *t, MESH_ChannelNumber chid)
{
  struct MeshTChannel *iter;

  if (NULL == t)
    return NULL;

  for (iter = t->channel_head; NULL != iter; iter = iter->next)
  {
    if (GMCH_get_id (iter->ch) == chid)
      break;
  }

  return NULL == iter ? NULL : iter->ch;
}


/**
 * Pick a connection on which send the next data message.
 *
 * @param t Tunnel on which to send the message.
 * @param fwd Is this a fwd message?
 *
 * @return The connection on which to send the next message.
 */
static struct MeshConnection *
tunnel_get_connection (struct MeshTunnel3 *t, int fwd)
{
  struct MeshTConnection *iter;
  struct MeshConnection *best;
  unsigned int qn;
  unsigned int lowest_q;

  LOG (GNUNET_ERROR_TYPE_DEBUG, "tunnel_get_connection %s\n", GMP_2s (t->peer));
  best = NULL;
  lowest_q = UINT_MAX;
  for (iter = t->connection_head; NULL != iter; iter = iter->next)
  {
    LOG (GNUNET_ERROR_TYPE_DEBUG, "  connection %s: %u\n",
         GNUNET_h2s (GMC_get_id (iter->c)), GMC_get_state (iter->c));
    if (MESH_CONNECTION_READY == GMC_get_state (iter->c))
    {
      qn = GMC_get_qn (iter->c, fwd);
      LOG (GNUNET_ERROR_TYPE_DEBUG, "    q_n %u, \n", qn);
      if (qn < lowest_q)
      {
        best = iter->c;
        lowest_q = qn;
      }
    }
  }
  return best;
}


void
handle_data (struct MeshTunnel3 *t,
             const struct GNUNET_MESH_Data *msg,
             int fwd)
{
  struct MeshChannel *ch;
  uint16_t type;
  size_t size;

  /* Check size */
  size = ntohs (msg->header.size);
  if (size <
      sizeof (struct GNUNET_MESH_Data) +
      sizeof (struct GNUNET_MessageHeader))
  {
    GNUNET_break (0);
    return;
  }
  type = ntohs (msg->header.type);
  LOG (GNUNET_ERROR_TYPE_DEBUG, "got a %s message\n",
              GNUNET_MESH_DEBUG_M2S (type));
  LOG (GNUNET_ERROR_TYPE_DEBUG, " payload of type %s\n",
              GNUNET_MESH_DEBUG_M2S (ntohs (msg[1].header.type)));

  /* Check channel */
  ch = get_channel (t, ntohl (msg->chid));
  if (NULL == ch)
  {
    GNUNET_STATISTICS_update (stats, "# data on unknown channel",
                              1, GNUNET_NO);
    LOG (GNUNET_ERROR_TYPE_DEBUG, "WARNING channel %u unknown\n",
         ntohl (msg->chid));
    return;
  }

  GMT_change_state (t, MESH_TUNNEL_READY);
  GMCH_handle_data (ch, msg, fwd);
}

void
handle_data_ack (struct MeshTunnel3 *t,
                 const struct GNUNET_MESH_DataACK *msg,
                 int fwd)
{
  struct MeshChannel *ch;
  size_t size;

  /* Check size */
  size = ntohs (msg->header.size);
  if (size != sizeof (struct GNUNET_MESH_DataACK))
  {
    GNUNET_break (0);
    return;
  }

  /* Check channel */
  ch = get_channel (t, ntohl (msg->chid));
  if (NULL == ch)
  {
    GNUNET_STATISTICS_update (stats, "# data ack on unknown channel",
                              1, GNUNET_NO);
    LOG (GNUNET_ERROR_TYPE_DEBUG, "WARNING channel %u unknown\n",
         ntohl (msg->chid));
    return;
  }

  GMCH_handle_data_ack (ch, msg, fwd);
}

void
handle_ch_create (struct MeshTunnel3 *t,
                  const struct GNUNET_MESH_ChannelCreate *msg,
                  int fwd)
{
  struct MeshTChannel *tch;
  struct MeshChannel *ch;
  size_t size;

  /* Check size */
  size = ntohs (msg->header.size);
  if (size != sizeof (struct GNUNET_MESH_ChannelCreate))
  {
    GNUNET_break (0);
    return;
  }

  /* Check channel */
  ch = get_channel (t, ntohl (msg->chid));
  if (NULL != ch)
  {
    /* Probably a retransmission, safe to ignore */
    LOG (GNUNET_ERROR_TYPE_DEBUG, "   already exists...\n");
  }
  else
  {
    ch = GMCH_handle_create (msg, fwd);
  }

  tch = GNUNET_new (struct MeshTChannel);
  tch->ch = ch;
  GNUNET_CONTAINER_DLL_insert (t->channel_head, t->channel_tail, tch);
}

void
handle_ch_ack (struct MeshTunnel3 *t,
               const struct GNUNET_MESH_ChannelManage *msg,
               int fwd)
{
  struct MeshChannel *ch;
  size_t size;

  /* Check size */
  size = ntohs (msg->header.size);
  if (size != sizeof (struct GNUNET_MESH_ChannelManage))
  {
    GNUNET_break (0);
    return;
  }

  /* Check channel */
  ch = get_channel (t, ntohl (msg->chid));
  if (NULL == ch)
  {
    GNUNET_STATISTICS_update (stats, "# channel ack on unknown channel",
                              1, GNUNET_NO);
    LOG (GNUNET_ERROR_TYPE_DEBUG, "WARNING channel %u unknown\n",
         ntohl (msg->chid));
    return;
  }

  GMCH_handle_ack (ch, msg, fwd);
}

void
handle_ch_destroy (struct MeshTunnel3 *t,
                   const struct GNUNET_MESH_ChannelManage *msg,
                   int fwd)
{
  struct MeshChannel *ch;
  size_t size;

  /* Check size */
  size = ntohs (msg->header.size);
  if (size != sizeof (struct GNUNET_MESH_ChannelManage))
  {
    GNUNET_break (0);
    return;
  }

  /* Check channel */
  ch = get_channel (t, ntohl (msg->chid));
  if (NULL == ch)
  {
    /* Probably a retransmission, safe to ignore */
    return;
  }

  GMCH_handle_destroy (ch, msg, fwd);
}

/******************************************************************************/
/********************************    API    ***********************************/
/******************************************************************************/

/**
 * Demultiplex by message type and call appropriate handler for a message
 * towards a channel of a local tunnel.
 *
 * @param t Tunnel this message came on.
 * @param msgh Message header.
 * @param fwd Is this message fwd?
 */
void
GMT_handle_decrypted (struct MeshTunnel3 *t,
                      const struct GNUNET_MessageHeader *msgh,
                      int fwd)
{
  uint16_t type;

  type = ntohs (msgh->type);
  LOG (GNUNET_ERROR_TYPE_DEBUG,
       "Got a %s message!\n",
       GNUNET_MESH_DEBUG_M2S (type));

  switch (type)
  {
    case GNUNET_MESSAGE_TYPE_MESH_DATA:
      /* Don't send hop ACK, wait for client to ACK */
      handle_data (t, (struct GNUNET_MESH_Data *) msgh, fwd);
      break;

    case GNUNET_MESSAGE_TYPE_MESH_DATA_ACK:
      handle_data_ack (t, (struct GNUNET_MESH_DataACK *) msgh, fwd);
      break;

    case GNUNET_MESSAGE_TYPE_MESH_CHANNEL_CREATE:
      handle_ch_create (t,
                        (struct GNUNET_MESH_ChannelCreate *) msgh,
                        fwd);
      break;

    case GNUNET_MESSAGE_TYPE_MESH_CHANNEL_ACK:
      handle_ch_ack (t,
                     (struct GNUNET_MESH_ChannelManage *) msgh,
                     fwd);
      break;

    case GNUNET_MESSAGE_TYPE_MESH_CHANNEL_DESTROY:
      handle_ch_destroy (t,
                         (struct GNUNET_MESH_ChannelManage *) msgh,
                         fwd);
      break;

    default:
      GNUNET_break_op (0);
      LOG (GNUNET_ERROR_TYPE_DEBUG,
           "end-to-end message not known (%u)\n",
           ntohs (msgh->type));
  }
}


/**
 * Cache a message to be sent once tunnel is online.
 *
 * @param t Tunnel to hold the message.
 * @param ch Channel the message is about.
 * @param msg Message itself (copy will be made).
 * @param fwd Is this fwd?
 */
void
GMT_queue_data (struct MeshTunnel3 *t,
                struct MeshChannel *ch,
                struct GNUNET_MessageHeader *msg,
                int fwd)
{
  struct MeshTunnelQueue *tq;
  uint16_t size = ntohs (msg->size);

  tq = GNUNET_malloc (sizeof (struct MeshTunnelQueue) + size);

  tq->ch = ch;
  memcpy (&tq[1], msg, size);
  GNUNET_CONTAINER_DLL_insert_tail (t->tq_head, t->tq_tail, tq);

  if (MESH_TUNNEL_READY == t->state)
    GMT_send_queued_data (t, fwd);
}


/**
 * Send all cached messages that we can, tunnel is online.
 *
 * @param t Tunnel that holds the messages.
 * @param fwd Is this fwd?
 */
void
GMT_send_queued_data (struct MeshTunnel3 *t, int fwd)
{
  struct MeshTunnelQueue *tq;
  struct MeshTunnelQueue *next;
  unsigned int room;

  LOG (GNUNET_ERROR_TYPE_DEBUG,
              "GMT_send_queued_data on tunnel %s\n",
              GMP_2s (t->peer));
  room = GMT_get_buffer (t, fwd);
  LOG (GNUNET_ERROR_TYPE_DEBUG, "  buffer space: %u\n", room);
  for (tq = t->tq_head; NULL != tq && room > 0; tq = next)
  {
    next = tq->next;
    room--;
    GNUNET_CONTAINER_DLL_remove (t->tq_head, t->tq_tail, tq);
    GMCH_send_prebuilt_message ((struct GNUNET_MessageHeader *) &tq[1],
                                tq->ch, fwd);

    GNUNET_free (tq);
  }
}


/**
 * Initialize the tunnel subsystem.
 *
 * @param c Configuration handle.
 * @param id Peer identity.
 * @param key ECC private key, to derive all other keys and do crypto.
 */
void
GMT_init (const struct GNUNET_CONFIGURATION_Handle *c,
          const struct GNUNET_PeerIdentity *id,
          const struct GNUNET_CRYPTO_EccPrivateKey *key)
{
  if (GNUNET_OK !=
      GNUNET_CONFIGURATION_get_value_number (c, "MESH", "DEFAULT_TTL",
                                             &default_ttl))
  {
    GNUNET_log_config_invalid (GNUNET_ERROR_TYPE_WARNING,
                               "MESH", "DEFAULT_TTL", "USING DEFAULT");
    default_ttl = 64;
  }
  my_full_id = id;
  my_private_key = key;
  my_short_id = GNUNET_PEER_intern (my_full_id);
}


/**
 * Shut down the tunnel subsystem.
 */
void
GMT_shutdown (void)
{
  GNUNET_PEER_change_rc (my_short_id, -1);
}


/**
 * Create a tunnel.
 */
struct MeshTunnel3 *
GMT_new (void)
{
  struct MeshTunnel3 *t;

  t = GNUNET_new (struct MeshTunnel3);
  t->next_chid = 0;
  t->next_local_chid = GNUNET_MESH_LOCAL_CHANNEL_ID_SERV;
//   if (GNUNET_OK !=
//       GNUNET_CONTAINER_multihashmap_put (tunnels, tid, t,
//                                          GNUNET_CONTAINER_MULTIHASHMAPOPTION_UNIQUE_FAST))
//   {
//     GNUNET_break (0);
//     tunnel_destroy (t);
//     return NULL;
//   }

//   char salt[] = "salt";
//   GNUNET_CRYPTO_kdf (&t->e_key, sizeof (struct GNUNET_CRYPTO_SymmetricSessionKey),
//                      salt, sizeof (salt),
//                      &t->e_key, sizeof (struct GNUNET_CRYPTO_SymmetricSessionKey),
//                      &my_full_id, sizeof (struct GNUNET_PeerIdentity),
//                      GNUNET_PEER_resolve2 (t->peer->id), sizeof (struct GNUNET_PeerIdentity),
//                      NULL);
//   GNUNET_CRYPTO_kdf (&t->d_key, sizeof (struct GNUNET_CRYPTO_SymmetricSessionKey),
//                      salt, sizeof (salt),
//                      &t->d_key, sizeof (struct GNUNET_CRYPTO_SymmetricSessionKey),
//                      GNUNET_PEER_resolve2 (t->peer->id), sizeof (struct GNUNET_PeerIdentity),
//                      &my_full_id, sizeof (struct GNUNET_PeerIdentity),
//                      NULL);

  return t;
}



/**
 * Change the tunnel state.
 *
 * @param t Tunnel whose state to change.
 * @param state New state.
 */
void
GMT_change_state (struct MeshTunnel3* t, enum MeshTunnelState state)
{
  if (NULL == t)
    return;
  LOG (GNUNET_ERROR_TYPE_DEBUG,
              "Tunnel %s state was %s\n",
              GMP_2s (t->peer),
              GMT_state2s (t->state));
  LOG (GNUNET_ERROR_TYPE_DEBUG,
              "Tunnel %s state is now %s\n",
              GMP_2s (t->peer),
              GMT_state2s (state));
  t->state = state;
}


/**
 * Add a connection to a tunnel.
 *
 * @param t Tunnel.
 * @param c Connection.
 */
void
GMT_add_connection (struct MeshTunnel3 *t, struct MeshConnection *c)
{
  struct MeshTConnection *aux;

  for (aux = t->connection_head; aux != NULL; aux = aux->next)
    if (aux->c == c)
      return;

  aux = GNUNET_new (struct MeshTConnection);
  aux->c = c;
  GNUNET_CONTAINER_DLL_insert_tail (t->connection_head, t->connection_tail, aux);
}


/**
 * Remove a connection from a tunnel.
 *
 * @param t Tunnel.
 * @param c Connection.
 */
void
GMT_remove_connection (struct MeshTunnel3 *t, struct MeshConnection *c)
{
  struct MeshTConnection *aux;

  for (aux = t->connection_head; aux != NULL; aux = aux->next)
    if (aux->c == c)
    {
      GNUNET_CONTAINER_DLL_remove (t->connection_head, t->connection_tail, aux);
      GNUNET_free (aux);
      return;
    }
}


/**
 * Tunnel is empty: destroy it.
 *
 * Notifies all connections about the destruction.
 *
 * @param t Tunnel to destroy.
 */
void
GMT_destroy_empty (struct MeshTunnel3 *t)
{
  struct MeshTConnection *iter;

  for (iter = t->connection_head; NULL != iter; iter = iter->next)
  {
    GMC_send_destroy (iter->c);
  }

  if (0 == t->pending_messages)
    GMT_destroy (t);
  else
    t->destroy = GNUNET_YES;
}


/**
 * Destroy tunnel if empty (no more channels).
 *
 * @param t Tunnel to destroy if empty.
 */
void
GMT_destroy_if_empty (struct MeshTunnel3 *t)
{
  if (1 < GMT_count_channels (t))
    return;

  GMT_destroy_empty (t);
}


/**
 * Destroy the tunnel.
 *
 * This function does not generate any warning traffic to clients or peers.
 *
 * Tasks:
 * Cancel messages belonging to this tunnel queued to neighbors.
 * Free any allocated resources linked to the tunnel.
 *
 * @param t The tunnel to destroy.
 */
void
GMT_destroy (struct MeshTunnel3 *t)
{
  struct MeshTConnection *iter;
  struct MeshTConnection *next;

  if (NULL == t)
    return;

  LOG (GNUNET_ERROR_TYPE_DEBUG, "destroying tunnel %s\n", GMP_2s (t->peer));

//   if (GNUNET_YES != GNUNET_CONTAINER_multihashmap_remove (tunnels, &t->id, t))
//     GNUNET_break (0);

  for (iter = t->connection_head; NULL != iter; iter = next)
  {
    next = iter->next;
    GMC_destroy (iter->c);
    GNUNET_free (iter);
  }

  GNUNET_STATISTICS_update (stats, "# tunnels", -1, GNUNET_NO);
  GMP_set_tunnel (t->peer, NULL);

  GNUNET_free (t);
}


/**
 * Notifies a tunnel that a connection has broken that affects at least
 * some of its peers. Sends a notification towards the root of the tree.
 * In case the peer is the owner of the tree, notifies the client that owns
 * the tunnel and tries to reconnect.
 *
 * FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
 *
 * @param t Tunnel affected.
 * @param p1 Peer that got disconnected from p2.
 * @param p2 Peer that got disconnected from p1.
 *
 * @return Short ID of the peer disconnected (either p1 or p2).
 *         0 if the tunnel remained unaffected.
 */
GNUNET_PEER_Id
GMT_notify_connection_broken (struct MeshTunnel3* t,
                              GNUNET_PEER_Id p1, GNUNET_PEER_Id p2)
{
//   if (myid != p1 && myid != p2) FIXME
//   {
//     return;
//   }
//
//   if (tree_get_predecessor (t->tree) != 0)
//   {
//     /* We are the peer still connected, notify owner of the disconnection. */
//     struct GNUNET_MESH_PathBroken msg;
//     struct GNUNET_PeerIdentity neighbor;
//
//     msg.header.size = htons (sizeof (msg));
//     msg.header.type = htons (GNUNET_MESSAGE_TYPE_MESH_PATH_BROKEN);
//     GNUNET_PEER_resolve (t->id.oid, &msg.oid);
//     msg.tid = htonl (t->id.tid);
//     msg.peer1 = my_full_id;
//     GNUNET_PEER_resolve (pid, &msg.peer2);
//     GNUNET_PEER_resolve (tree_get_predecessor (t->tree), &neighbor);
//     send_prebuilt_message (&msg.header, &neighbor, t);
//   }
  return 0;
}

/**
 * @brief Use the given path for the tunnel.
 * Update the next and prev hops (and RCs).
 * (Re)start the path refresh in case the tunnel is locally owned.
 *
 * @param t Tunnel to update.
 * @param p Path to use.
 *
 * @return Connection created.
 */
struct MeshConnection *
GMT_use_path (struct MeshTunnel3 *t, struct MeshPeerPath *p)
{
  struct MeshConnection *c;
  struct GNUNET_HashCode cid;
  unsigned int own_pos;

  if (NULL == t || NULL == p)
  {
    GNUNET_break (0);
    return NULL;
  }

  for (own_pos = 0; own_pos < p->length; own_pos++)
  {
    if (p->peers[own_pos] == my_short_id)
      break;
  }
  if (own_pos > p->length - 1)
  {
    GNUNET_break (0);
    return NULL;
  }

  GNUNET_CRYPTO_hash_create_random (GNUNET_CRYPTO_QUALITY_NONCE, &cid);
  c = GMC_new (&cid, t, p, own_pos);
  GMT_add_connection (t, c);
  return c;
}


/**
 * FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
 * Encrypt data with the tunnel key.
 *
 * @param t Tunnel whose key to use.
 * @param dst Destination for the encrypted data.
 * @param src Source of the plaintext.
 * @param size Size of the plaintext.
 * @param iv Initialization Vector to use.
 * @param fwd Is this a fwd message?
 */
void
GMT_encrypt (struct MeshTunnel3 *t,
             void *dst, const void *src,
             size_t size, uint64_t iv, int fwd)
{
  memcpy (dst, src, size);
}


/**
 * FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
 * Decrypt data with the tunnel key.
 *
 * @param t Tunnel whose key to use.
 * @param dst Destination for the plaintext.
 * @param src Source of the encrypted data.
 * @param size Size of the encrypted data.
 * @param iv Initialization Vector to use.
 * @param fwd Is this a fwd message?
 */
void
GMT_decrypt (struct MeshTunnel3 *t,
             void *dst, const void *src,
             size_t size, uint64_t iv, int fwd)
{
  memcpy (dst, src, size);
}


/**
 * Count established (ready) connections of a tunnel.
 *
 * @param t Tunnel on which to count.
 *
 * @return Number of connections.
 */
unsigned int
GMT_count_connections (struct MeshTunnel3 *t)
{
  struct MeshTConnection *iter;
  unsigned int count;

  for (count = 0, iter = t->connection_head;
       NULL != iter;
       iter = iter->next, count++);

  return count;
}

/**
 * Count channels of a tunnel.
 *
 * @param t Tunnel on which to count.
 *
 * @return Number of channels.
 */
unsigned int
GMT_count_channels (struct MeshTunnel3 *t)
{
  struct MeshTChannel *iter;
  unsigned int count;

  for (count = 0, iter = t->channel_head;
       NULL != iter;
  iter = iter->next, count++);

  return count;
}


/**
 * Get the state of a tunnel.
 *
 * @param t Tunnel.
 *
 * @return Tunnel's state.
 */
enum MeshTunnelState
GMT_get_state (struct MeshTunnel3 *t)
{
  return t->state;
}

/**
 * Get the total buffer space for a tunnel.
 *
 * @param t Tunnel.
 * @param fwd Is this for FWD traffic?
 *
 * @return Buffer space offered by all connections in the tunnel.
 */
unsigned int
GMT_get_buffer (struct MeshTunnel3 *t, int fwd)
{
  struct MeshTConnection *iter;
  unsigned int buffer;

  iter = t->connection_head;
  buffer = 0;

  /* If terminal, return biggest channel buffer */
  if (NULL == iter || GMC_is_terminal (iter->c, fwd))
  {
    struct MeshTChannel *iter_ch;
    unsigned int ch_buf;

    if (NULL == t->channel_head)
      return 64;

    for (iter_ch = t->channel_head; NULL != iter_ch; iter_ch = iter_ch->next)
    {
      ch_buf = GMCH_get_buffer (iter_ch->ch, fwd);
      if (ch_buf > buffer)
        buffer = ch_buf;
    }
    return buffer;
  }

  /* If not terminal, return sum of connection buffers */
  while (NULL != iter)
  {
    if (GMC_get_state (iter->c) != MESH_CONNECTION_READY)
    {
      iter = iter->next;
      continue;
    }

    buffer += GMC_get_buffer (iter->c, fwd);
    iter = iter->next;
  }

  return buffer;
}

/**
 * Sends an already built message on a tunnel, choosing the best connection.
 *
 * @param message Message to send. Function modifies it.
 * @param t Tunnel on which this message is transmitted.
 * @param ch Channel on which this message is transmitted.
 * @param fwd Is this a fwd message?
 */
void
GMT_send_prebuilt_message (struct GNUNET_MESH_Encrypted *msg,
                           struct MeshTunnel3 *t,
                           struct MeshChannel *ch,
                           int fwd)
{
  struct MeshConnection *c;
  uint16_t type;

  LOG (GNUNET_ERROR_TYPE_DEBUG, "Send on Tunnel %s\n", GMP_2s (t->peer));
  c = tunnel_get_connection (t, fwd);
  if (NULL == c)
  {
    GNUNET_break (GNUNET_YES == t->destroy);
    return;
  }
  type = ntohs (msg->header.type);
  switch (type)
  {
    case GNUNET_MESSAGE_TYPE_MESH_FWD:
    case GNUNET_MESSAGE_TYPE_MESH_BCK:
    case GNUNET_MESSAGE_TYPE_MESH_CHANNEL_CREATE:
    case GNUNET_MESSAGE_TYPE_MESH_CHANNEL_DESTROY:
      msg->cid = *GMC_get_id (c);
      msg->ttl = htonl (default_ttl);
      break;
    default:
      LOG (GNUNET_ERROR_TYPE_DEBUG, "unkown type %s\n",
           GNUNET_MESH_DEBUG_M2S (type));
      GNUNET_break (0);
  }
  msg->reserved = 0;

  GMC_send_prebuilt_message (&msg->header, c, ch, fwd);
}