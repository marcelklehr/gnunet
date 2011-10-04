/*
     This file is part of GNUnet.
     (C) 2009 Christian Grothoff (and other contributing authors)

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
 * @author Christian Grothoff
 * @file util/resolver.h
 */
#ifndef RESOLVER_H
#define RESOLVER_H

#include "gnunet_common.h"

#define DEBUG_RESOLVER GNUNET_EXTRA_LOGGING

/**
 * Request for the resolver.  Followed by either
 * the "struct sockaddr" or the 0-terminated hostname.
 *
 * The response will be one or more messages of type
 * RESOLVER_RESPONSE, each with the message header
 * immediately followed by the requested data
 * (hostname or struct sockaddr, depending on direction).
 * The last RESOLVER_RESPONSE will just be a header
 * without any data (used to indicate the end of the list).
 */
struct GNUNET_RESOLVER_GetMessage
{
  /**
   * Type:  GNUNET_MESSAGE_TYPE_STATISTICS_VALUE
   */
  struct GNUNET_MessageHeader header;

  /**
   * GNUNET_YES to get hostname from IP,
   * GNUNET_NO to get IP from hostname.
   */
  int32_t direction GNUNET_PACKED;

  /**
   * Domain to use (AF_INET, AF_INET6 or AF_UNSPEC).
   */
  int32_t domain GNUNET_PACKED;

};

#endif
