/*
     This file is part of GNUnet.
     (C) 2001 - 2011 Christian Grothoff (and other contributing authors)

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
 * @author Bartlomiej Polot
 * @file cadet/cadet_protocol.h
 */

#ifndef CADET_PROTOCOL_H_
#define CADET_PROTOCOL_H_

#include "platform.h"
#include "gnunet_util_lib.h"
#include "cadet.h"

#ifdef __cplusplus

struct GNUNET_CADET_TunnelMessage;
extern "C"
{
#if 0
  /* keep Emacsens' auto-indent happy */
}
#endif
#endif

/******************************************************************************/
/********************      CADET NETWORK MESSAGES     **************************/
/******************************************************************************/

GNUNET_NETWORK_STRUCT_BEGIN

/**
 * Message for cadet connection creation.
 */
struct GNUNET_CADET_ConnectionCreate
{
    /**
     * Type: GNUNET_MESSAGE_TYPE_CADET_CONNECTION_CREATE
     *
     * Size: sizeof (struct GNUNET_CADET_ConnectionCreate) +
     *       path_length * sizeof (struct GNUNET_PeerIdentity)
     */
  struct GNUNET_MessageHeader header;

    /**
     * ID of the connection
     */
  struct GNUNET_CADET_Hash cid;

    /**
     * path_length structs defining the *whole* path from the origin [0] to the
     * final destination [path_length-1].
     */
  /* struct GNUNET_PeerIdentity peers[path_length]; */
};

/**
 * Message for ack'ing a connection
 */
struct GNUNET_CADET_ConnectionACK
{
    /**
     * Type: GNUNET_MESSAGE_TYPE_CADET_CONNECTION_ACK
     */
  struct GNUNET_MessageHeader header;

    /**
     * ID of the connection.
     */
  struct GNUNET_CADET_Hash cid;

};


/**
 * Message for encapsulation of a Key eXchange message in a connection.
 */
struct GNUNET_CADET_KX
{
    /**
     * Type: GNUNET_MESSAGE_TYPE_CADET_KX.
     */
  struct GNUNET_MessageHeader header;

    /**
     * ID of the connection.
     */
  struct GNUNET_CADET_Hash cid;

  /* Specific KX message follows. */
};


/**
 * Message transmitted with the signed ephemeral key of a peer.  The
 * session key is then derived from the two ephemeral keys (ECDHE).
 *
 * As far as possible, same as CORE's EphemeralKeyMessage.
 */
struct GNUNET_CADET_KX_Ephemeral
{

  /**
   * Message type is GNUNET_MESSAGE_TYPE_CADET_KX_EPHEMERAL.
   */
  struct GNUNET_MessageHeader header;

  /**
   * Status of the sender (should be in "enum PeerStateMachine"), nbo.
   */
  int32_t sender_status GNUNET_PACKED;

  /**
   * An ECC signature of the 'origin' asserting the validity of
   * the given ephemeral key.
   */
  struct GNUNET_CRYPTO_EddsaSignature signature;

  /**
   * Information about what is being signed.
   */
  struct GNUNET_CRYPTO_EccSignaturePurpose purpose;

  /**
   * At what time was this key created (beginning of validity).
   */
  struct GNUNET_TIME_AbsoluteNBO creation_time;

  /**
   * When does the given ephemeral key expire (end of validity).
   */
  struct GNUNET_TIME_AbsoluteNBO expiration_time;

  /**
   * Ephemeral public ECC key (always for NIST P-521) encoded in a format suitable
   * for network transmission as created using 'gcry_sexp_sprint'.
   */
  struct GNUNET_CRYPTO_EcdhePublicKey ephemeral_key;

  /**
   * Public key of the signing peer (persistent version, not the ephemeral public key).
   */
  struct GNUNET_PeerIdentity origin_identity;
};


/**
 * We're sending an (encrypted) PING to the other peer to check if he
 * can decrypt.  The other peer should respond with a PONG with the
 * same content, except this time encrypted with the receiver's key.
 */
struct GNUNET_CADET_KX_Ping
{
  /**
   * Message type is GNUNET_MESSAGE_TYPE_CADET_KX_PING.
   */
  struct GNUNET_MessageHeader header;

  /**
   * Seed for the IV
   */
  uint32_t iv GNUNET_PACKED;

  /**
   * Intended target of the PING, used primarily to check
   * that decryption actually worked.
   */
  struct GNUNET_PeerIdentity target;

  /**
   * Random number chosen to make reply harder.
   */
  uint32_t nonce GNUNET_PACKED;
};


/**
 * Response to a PING.  Includes data from the original PING.
 */
struct GNUNET_CADET_KX_Pong
{
  /**
   * Message type is GNUNET_MESSAGE_TYPE_CADET_KX_PONG.
   */
  struct GNUNET_MessageHeader header;

  /**
   * Seed for the IV
   */
  uint32_t iv GNUNET_PACKED;

  /**
   * Same nonce as in the reve.
   */
  uint32_t nonce GNUNET_PACKED;
};


/**
 * Tunnel(ed) message.
 */
struct GNUNET_CADET_Encrypted
{
  /**
   * Type: GNUNET_MESSAGE_TYPE_CADET_ENCRYPTED
   */
  struct GNUNET_MessageHeader header;

  /**
   * ID of the connection.
   */
  struct GNUNET_CADET_Hash cid;

  /**
   * ID of the packet (hop by hop).
   */
  uint32_t pid GNUNET_PACKED;

  /**
   * Number of hops to live.
   */
  uint32_t ttl GNUNET_PACKED;

  /**
   * Initialization Vector for payload encryption.
   */
  uint32_t iv GNUNET_PACKED;

  /**
   * MAC of the encrypted message, used to verify message integrity.
   * Everything after this value  will be encrypted and authenticated.
   */
  struct GNUNET_CADET_Hash hmac;

  /**
   * Encrypted content follows.
   */
};


/**
 * Message to create a Channel.
 */
struct GNUNET_CADET_ChannelCreate
{
  /**
   * Type: GNUNET_MESSAGE_TYPE_CADET_CHANNEL_CREATE
   */
  struct GNUNET_MessageHeader header;

  /**
   * ID of the channel
   */
  CADET_ChannelNumber chid GNUNET_PACKED;

  /**
   * Destination port.
   */
  uint32_t port GNUNET_PACKED;

  /**
   * Channel options.
   */
  uint32_t opt GNUNET_PACKED;
};


/**
 * Message to manage a Channel (ACK, NACK, Destroy).
 */
struct GNUNET_CADET_ChannelManage
{
  /**
   * Type: GNUNET_MESSAGE_TYPE_CADET_CHANNEL_{ACK|NACK|DESTROY}
   */
  struct GNUNET_MessageHeader header;

  /**
   * ID of the channel
   */
  CADET_ChannelNumber chid GNUNET_PACKED;
};


/**
 * Message for cadet data traffic.
 */
struct GNUNET_CADET_Data
{
    /**
     * Type: GNUNET_MESSAGE_TYPE_CADET_UNICAST,
     *       GNUNET_MESSAGE_TYPE_CADET_TO_ORIGIN
     */
  struct GNUNET_MessageHeader header;

    /**
     * Unique ID of the payload message
     */
  uint32_t mid GNUNET_PACKED;

    /**
     * ID of the channel
     */
  CADET_ChannelNumber chid GNUNET_PACKED;

    /**
     * Payload follows
     */
};


/**
 * Message to acknowledge end-to-end data.
 */
struct GNUNET_CADET_DataACK
{
  /**
   * Type: GNUNET_MESSAGE_TYPE_CADET_DATA_ACK
   */
  struct GNUNET_MessageHeader header;

  /**
   * ID of the channel
   */
  CADET_ChannelNumber chid GNUNET_PACKED;

  /**
   * Bitfield of already-received newer messages
   * pid +  1 @ LSB
   * pid + 64 @ MSB
   */
  uint64_t futures GNUNET_PACKED;

  /**
   * Last message ID received.
   */
  uint32_t mid GNUNET_PACKED;
};


/**
 * Message to acknowledge cadet encrypted traffic.
 */
struct GNUNET_CADET_ACK
{
    /**
     * Type: GNUNET_MESSAGE_TYPE_CADET_ACK
     */
  struct GNUNET_MessageHeader header;

    /**
     * Maximum packet ID authorized.
     */
  uint32_t ack GNUNET_PACKED;

    /**
     * ID of the connection.
     */
  struct GNUNET_CADET_Hash cid;
};


/**
 * Message to query a peer about its Flow Control status regarding a tunnel.
 */
struct GNUNET_CADET_Poll
{
    /**
     * Type: GNUNET_MESSAGE_TYPE_CADET_POLL
     */
  struct GNUNET_MessageHeader header;

    /**
     * Last packet sent.
     */
  uint32_t pid GNUNET_PACKED;

    /**
     * ID of the connection.
     */
  struct GNUNET_CADET_Hash cid;

};


/**
 * Message for notifying a disconnection in a path
 */
struct GNUNET_CADET_ConnectionBroken
{
    /**
     * Type: GNUNET_MESSAGE_TYPE_CADET_CONNECTION_BROKEN
     */
  struct GNUNET_MessageHeader header;

    /**
     * ID of the connection.
     */
  struct GNUNET_CADET_Hash cid;

    /**
     * ID of the endpoint
     */
  struct GNUNET_PeerIdentity peer1;

    /**
     * ID of the endpoint
     */
  struct GNUNET_PeerIdentity peer2;
};


/**
 * Message to destroy a connection.
 */
struct GNUNET_CADET_ConnectionDestroy
{
    /**
     * Type: GNUNET_MESSAGE_TYPE_CADET_CONNECTION_DESTROY
     */
  struct GNUNET_MessageHeader header;

    /**
     * ID of the connection.
     */
  struct GNUNET_CADET_Hash cid;
};


GNUNET_NETWORK_STRUCT_END

#if 0                           /* keep Emacsens' auto-indent happy */
{
#endif
#ifdef __cplusplus
}
#endif

/* ifndef CADET_PROTOCOL_H */
#endif
/* end of cadet_protocol.h */
