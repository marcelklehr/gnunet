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

/** 
 * @file include/gnunet_identity_service.h
 * @brief Identity service; implements identity management for GNUnet
 * @author Christian Grothoff
 *
 * Identities in GNUnet are ECDSA keys.  You assume an identity by
 * using (signing with) a particular private key.  As GNUnet users are
 * expected to have many pseudonyms, we need an identity service to
 * allow users to manage their pseudonyms.  The identity service
 * manages the pseudonyms (private keys) of the local user; it does
 * NOT manage identities of other users (public keys).  For giving
 * names to other users and manage their public keys securely, we
 * use GADS/GNS.
 */
#ifndef GNUNET_IDENTITY_SERVICE_H
#define GNUNET_IDENTITY_SERVICE_H

#ifdef __cplusplus
extern "C"
{
#if 0                           /* keep Emacsens' auto-indent happy */
}
#endif
#endif

#include "gnunet_util_lib.h"


/** 
 * Version number of GNUnet Identity API.
 */
#define GNUNET_IDENTITY_VERSION 0x00000000

/** 
 * Handle to access the identity service.
 */
struct GNUNET_IDENTITY_Handle;

/** 
 * Handle for a pseudonym.
 */
struct GNUNET_IDENTITY_Pseudonym;


/**
 * Obtain the ECC key associated with a pseudonym.
 *
 * @param pseudonym the pseudonym
 * @return associated ECC key, valid as long as the pseudonym is valid
 */
const struct GNUNET_CRYPTO_EccPrivateKey *
GNUNET_IDENTITY_pseudonym_get_key (struct GNUNET_IDENTITY_Pseudonym *pseudonym);


/** 
 * Method called to inform about the pseudonyms of
 * this peer. 
 *
 * When used with 'GNUNET_IDENTITY_connect', this function is
 * initially called for all pseudonyms and then again whenever a
 * pseudonym's identifier changes or if it is deleted.  At the end of
 * the initial pass over all pseudonyms, the function is once called
 * with 'NULL' for 'pseu'. That does NOT mean that the callback won't
 * be invoked in the future or that there was an error.
 *
 * When used with 'GNUNET_IDENTITY_create' or 'GNUNET_IDENTITY_get',
 * this function is only called ONCE, and 'NULL' being passed in
 * 'pseu' does indicate an error (i.e. name is taken or no default
 * value is known).  If 'pseu' is non-NULL and if '*ctx'
 * is set in those callbacks, the value WILL be passed to a subsequent
 * call to the identity callback of 'GNUNET_IDENTITY_connect' (if 
 * that one was not NULL).
 *
 * @param cls closure
 * @param pseu pseudonym handle
 * @param pseu_ctx context for application to store data for this pseudonym
 *                 (during the lifetime of this process, initially NULL)
 * @param identifier identifier assigned by the user for this pseudonym,
 *                   NULL if the user just deleted the pseudonym and it
 *                   must thus no longer be used
 */
typedef void (*GNUNET_IDENTITY_Callback)(void *cls,
					 struct GNUNET_IDENTITY_Pseudonym *pseu,
					 void **ctx,
					 const char *identifier);


/** 
 * Connect to the identity service.
 *
 * @param cfg Configuration to contact the identity service.
 * @param cb function to call on all identity events, can be NULL
 * @param cb_cls closure for 'cb'
 * @return handle to communicate with identity service
 */
struct GNUNET_IDENTITY_Handle *
GNUNET_IDENTITY_connect (const struct GNUNET_CONFIGURATION_Handle *cfg,
			 GNUNET_IDENTITY_Callback cb,
			 void *cb_cls);


/**
 * Obtain the identity that is currently preferred/default
 * for a service.
 *
 * @param id identity service to query
 * @param service_name for which service is an identity wanted
 * @param cb function to call with the result (will only be called once)
 * @param cb_cls closure for cb
 * @return handle to abort the operation
 */
struct GNUNET_IDENTITY_Operation *
GNUNET_IDENTITY_get (struct GNUNET_IDENTITY_Handle *id,
		     const char *service_name,
		     GNUNET_IDENTITY_Callback cb,
		     void *cb_cls);


/**
 * Function called once the requested operation has
 * been completed.
 *
 * @param cls closure
 * @param emsg NULL on success, otherwise an error message
 */
typedef void (*GNUNET_IDENTITY_Continuation)(void *cls,
					     const char *emsg);


/**
 * Set the preferred/default identity for a service.
 *
 * @param id identity service to inform
 * @param service_name for which service is an identity set
 * @param pseu new default identity to be set for this service
 * @param cont function to call once the operation finished
 * @return handle to abort the operation
 */
struct GNUNET_IDENTITY_Operation *
GNUNET_IDENTITY_set (struct GNUNET_IDENTITY_Handle *id,
		     const char *service_name,
		     struct GNUNET_IDENTITY_Pseudonym *pseu,
		     GNUNET_IDENTITY_Continuation cont,
		     void *cont_cls);


/**
 * Disconnect from identity service.
 *
 * @param id identity service to disconnect
 */ 
void
GNUNET_IDENTITY_disconnect (struct GNUNET_IDENTITY_Handle *id);


/** 
 * Create a new identity with the given identifier.
 *
 * @param id identity service to use
 * @param identifier desired identifier
 * @param cb function to call with the result (will only be called once)
 * @param cb_cls closure for cb
 * @return handle to abort the operation
 */
struct GNUNET_IDENTITY_Operation *
GNUNET_IDENTITY_create (struct GNUNET_IDENTITY_Handle *id,
			const char *identifier,
			GNUNET_IDENTITY_Callback cb,
			void *cb_cls);


/**
 * Cancel an identity operation.
 *
 * @param op operation to cancel
 */
void
GNUNET_IDENITY_cancel (struct GNUNET_IDENTITY_Operation *op);


#if 0                           /* keep Emacsens' auto-indent happy */
{
#endif
#ifdef __cplusplus
}
#endif

/* ifndef GNUNET_IDENTITY_SERVICE_H */
#endif
/* end of gnunet_identity_service.h */