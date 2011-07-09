/*
     This file is part of GNUnet
     (C) 2009, 2011 Christian Grothoff (and other contributing authors)

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
 * @file src/fragmentation/fragmentation.h
 * @brief library to help fragment messages
 * @author Christian Grothoff
 */
#ifndef FRAGMENTATION_H
#define FRAGMENTATION_H
#include "platform.h"
#include "gnunet_fragmentation_lib.h"

/**
 * Header for a message fragment.
 */
struct FragmentHeader
{

  struct GNUNET_MessageHeader header;

};


/**
 * Message fragment acknowledgement.
 */
struct FragmentAcknowledgement
{

  struct GNUNET_MessageHeader header;

  /**
   * Bits that are being acknowledged, in big-endian.
   * (bits that are set correspond to fragments that
   * have not yet been received).
   */
  uint64_t bits;

};


#endif
