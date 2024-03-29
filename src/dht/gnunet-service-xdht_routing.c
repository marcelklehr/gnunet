/*
     This file is part of GNUnet.
     (C) 2011 - 2014 Christian Grothoff (and other contributing authors)

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
 * @file dht/gnunet-service-xdht_routing.c
 * @brief GNUnet DHT tracking of requests for routing replies
 * @author Supriti Singh
 */
#include "platform.h"
#include "gnunet-service-xdht_neighbours.h"
#include "gnunet-service-xdht_routing.h"
#include "gnunet-service-xdht.h"


/**
 * FIXME: Check if its better to store pointer to friend rather than storing
 * peer identity next_hop or prev_hop. 
 * keep entries in destnation and source peer also. so when we send the trail
 * teardown message then we don't know the source but if source gets the message
 * then it shold remove that trail id from its finger table. But how does
 * source know what is the desination finger ? It will whenevr contact a trail
 * will do a lookup in routing table and if no trail id present the remove
 * that trail of the finger and if only one trail then remove the finger.
 * because of this use case of trail teardown I think trail compression
 * and trail teardown should not be merged. 
 * 2. store a pointer to friendInfo in place o peer identity. 
 */
/**
 * Maximum number of entries in routing table.
 */
#define ROUTING_TABLE_THRESHOLD 64

/**
 * Routing table entry .
 */
struct RoutingTrail
{
  /**
   * Global Unique identifier of the trail.
   */
  struct GNUNET_HashCode trail_id;

  /**
   * The peer to which this request should be passed to.
   */
  struct GNUNET_PeerIdentity next_hop; 

  /**
   * Peer just before next hop in the trail.
   */
  struct GNUNET_PeerIdentity prev_hop;  
};

/**
 * Routing table of the peer
 */
static struct GNUNET_CONTAINER_MultiHashMap *routing_table;

/**
 * Update the prev. hop of the trail. Call made by trail teardown where
 * if you are the first friend now in the trail then you need to update
 * your prev. hop.
 * @param trail_id
 * @return #GNUNET_OK success
 *         #GNUNET_SYSERR in case no matching entry found in routing table.
 */
int
GDS_ROUTING_update_trail_prev_hop (const struct GNUNET_HashCode trail_id,
                                   struct GNUNET_PeerIdentity prev_hop)
{
  struct RoutingTrail *trail;

  trail = GNUNET_CONTAINER_multihashmap_get (routing_table, &trail_id);

  if (NULL == trail)
    return GNUNET_SYSERR;

  trail->prev_hop = prev_hop;
  return GNUNET_OK;
}



/**
 * Get the next hop for trail corresponding to trail_id
 * @param trail_id Trail id to be searched.
 * @return Next_hop if found
 *         NULL If next hop not found.
 */
struct GNUNET_PeerIdentity *
GDS_ROUTING_get_next_hop (const struct GNUNET_HashCode trail_id,
                          enum GDS_ROUTING_trail_direction trail_direction)
{
  struct RoutingTrail *trail;

  trail = GNUNET_CONTAINER_multihashmap_get (routing_table, &trail_id);

  if (NULL == trail)
    return NULL;

  switch (trail_direction)
  {
    case GDS_ROUTING_SRC_TO_DEST:
      return &(trail->next_hop);
    case GDS_ROUTING_DEST_TO_SRC:
      return &(trail->prev_hop);
  }
  return NULL;
}


/**
 * Remove trail with trail_id
 * @param trail_id Trail id to be removed
 * @return #GNUNET_YES success
 *         #GNUNET_NO if entry not found.
 */
int
GDS_ROUTING_remove_trail (const struct GNUNET_HashCode remove_trail_id)
{
  struct RoutingTrail *remove_entry;

  remove_entry = GNUNET_CONTAINER_multihashmap_get (routing_table, &remove_trail_id);

  if (NULL == remove_entry)
    return GNUNET_NO;

  if (GNUNET_YES == GNUNET_CONTAINER_multihashmap_remove (routing_table,
                                                          &remove_trail_id,
                                                          remove_entry))
  {
    GNUNET_free (remove_entry);
    return GNUNET_YES;
  }
  return GNUNET_NO;
}


/**
 * Iterate over routing table and remove entries with value as part of any trail.
 * 
 * @param cls closure
 * @param key current public key
 * @param value value in the hash map
 * @return #GNUNET_YES if we should continue to iterate,
 *         #GNUNET_NO if not.
 */
static int remove_matching_trails (void *cls,
                                   const struct GNUNET_HashCode *key,
                                   void *value)
{
  struct RoutingTrail *remove_trail = cls;
  struct GNUNET_PeerIdentity *disconnected_peer = value;
  
  /* If disconnected_peer is next_hop, then send a trail teardown message through
   * prev_hop in direction from destination to source. */
  if (0 == GNUNET_CRYPTO_cmp_peer_identity (&remove_trail->next_hop, 
                                            disconnected_peer)) 
  {
    GDS_NEIGHBOURS_send_trail_teardown (remove_trail->trail_id, 
                                        GDS_ROUTING_DEST_TO_SRC,
                                        &remove_trail->prev_hop);
  }
  
  /* If disconnected_peer is prev_hop, then send a trail teardown through
   * next_hop in direction from Source to Destination. */
  if (0 == GNUNET_CRYPTO_cmp_peer_identity (&remove_trail->prev_hop, 
                                            disconnected_peer))
  {
    GDS_NEIGHBOURS_send_trail_teardown (remove_trail->trail_id, 
                                        GDS_ROUTING_SRC_TO_DEST,
                                        &remove_trail->next_hop);
  }
  
  GNUNET_assert (GNUNET_YES ==
                   GNUNET_CONTAINER_multihashmap_remove (routing_table,
                                                         &remove_trail->trail_id,
                                                         remove_trail));
  GNUNET_free (remove_trail);
  return GNUNET_YES;
}


/**
 * Remove every trail where peer is either next_hop or prev_hop. Also send a 
 * trail teardown message in direction of hop which is not disconnected.
 * @param peer Peer identity. Trail containing this peer should be removed.
 */
void
GDS_ROUTING_remove_trail_by_peer (const struct GNUNET_PeerIdentity *peer)
{
  GNUNET_CONTAINER_multihashmap_iterate (routing_table, &remove_matching_trails,
                                         (void *)peer);
}


/**
 * Add a new entry in routing table
 * @param new_trail_id
 * @param prev_hop
 * @param next_hop
 * @return #GNUNET_OK success
 *         #GNUNET_SYSERR in case new_trail_id already exists in the network
 *                         but with different prev_hop/next_hop
 */
int
GDS_ROUTING_add (struct GNUNET_HashCode new_trail_id,
                 struct GNUNET_PeerIdentity prev_hop,
                 struct GNUNET_PeerIdentity next_hop)
{
  struct RoutingTrail *new_entry;

  new_entry = GNUNET_malloc (sizeof (struct RoutingTrail));
  new_entry->trail_id = new_trail_id;
  new_entry->next_hop = next_hop;
  new_entry->prev_hop = prev_hop;
  return GNUNET_CONTAINER_multihashmap_put (routing_table,
                                            &new_trail_id, new_entry,
                                            GNUNET_CONTAINER_MULTIHASHMAPOPTION_UNIQUE_ONLY);
}


/**
 * Check if the size of routing table has crossed threshold.
 * @return #GNUNET_YES, if threshold crossed else #GNUNET_NO.
 */
int
GDS_ROUTING_threshold_reached (void)
{
  return (GNUNET_CONTAINER_multihashmap_size(routing_table) >
          ROUTING_TABLE_THRESHOLD) ? GNUNET_YES:GNUNET_NO;
}


/**
 * Initialize routing subsystem.
 */
void
GDS_ROUTING_init (void)
{
  routing_table = GNUNET_CONTAINER_multihashmap_create (ROUTING_TABLE_THRESHOLD * 4 / 3,
                                                        GNUNET_NO);
}


/**
 * Shutdown routing subsystem.
 */
void
GDS_ROUTING_done (void)
{
  GNUNET_assert (0 == GNUNET_CONTAINER_multihashmap_size (routing_table));
  GNUNET_CONTAINER_multihashmap_destroy (routing_table);
}

/* end of gnunet-service-xdht_routing.c */
