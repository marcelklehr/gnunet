/*
      This file is part of GNUnet
      (C) 2013 Christian Grothoff (and other contributing authors)

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
 * @file include/gnunet_set_service.h
 * @brief two-peer set operations
 * @author Florian Dold
 */

#ifndef GNUNET_SET_SERVICE_H
#define GNUNET_SET_SERVICE_H

#ifdef __cplusplus
extern "C"
{
#if 0                           /* keep Emacsens' auto-indent happy */
}
#endif
#endif

#include "platform.h"
#include "gnunet_common.h"
#include "gnunet_time_lib.h"
#include "gnunet_configuration_lib.h"


/**
 * Opaque handle to a set.
 */
struct GNUNET_SET_Handle;

/**
 * Opaque handle to a set operation request from another peer.
 */
struct GNUNET_SET_Request;

/**
 * Opaque handle to a listen operation.
 */
struct GNUNET_SET_ListenHandle;

/**
 * Opaque handle to a set operation.
 */
struct GNUNET_SET_OperationHandle;


/**
 * Opaque handle to a listen operation.
 */
struct GNUNET_SET_ListenHandle;

/**
 * The operation that a set set supports.
 */
enum GNUNET_SET_OperationType
{
  /**
   * Set intersection, only return elements that are in both sets.
   */
  GNUNET_SET_OPERATION_INTERSECTION,
  /**
   * Set union, return all elements that are in at least one of the sets.
   */
  GNUNET_SET_OPERATION_UNION
};

/**
 * Status for the result callback
 */
enum GNUNET_SET_Status
{
  /**
   * Everything went ok.
   */
  GNUNET_SET_STATUS_OK,
  /**
   * There was a timeout.
   */
  GNUNET_SET_STATUS_TIMEOUT,
  /*
   * The other peer refused to to the operation with us
   */
  GNUNET_SET_STATUS_REFUSED,
  /*
   * Success, all elements have been sent.
   */
  GNUNET_SET_STATUS_DONE
};

/**
 * The way results are given to the client.
 */
enum GNUNET_SET_ResultMode
{
  /**
   * Client gets every element in the resulting set.
   */
  GNUNET_SET_RESULT_FULL,
  /**
   * Client gets only elements that have been added to the set.
   * Only works with set union.
   */
  GNUNET_SET_RESULT_ADDED,
  /**
   * Client gets only elements that have been removed from the set.
   * Only works with set intersection.
   */
  GNUNET_SET_RESULT_REMOVED
};

/**
 * Element stored in a set.
 */
struct GNUNET_SET_Element
{
  /**
   * Number of bytes in the buffer pointed to by data.
   */
  uint16_t size;
  /**
   * Application-specific element type.
   */
  uint16_t type;
  /**
   * Actual data of the element
   */
  void *data;
};


/**
 * Continuation used for some of the set operations
 *
 * @cls closure
 */
typedef void (*GNUNET_SET_Continuation) (void *cls);

/**
 * Callback for set operation results. Called for each element
 * in the result set.
 *
 * @param cls closure
 * @param element a result element, only valid if status is GNUNET_SET_STATUS_OK
 * @param status see enum GNUNET_SET_Status
 */
typedef void
(*GNUNET_SET_ResultIterator) (void *cls,
                              struct GNUNET_SET_Element *element,
                              enum GNUNET_SET_Status status);


/**
 * Called when another peer wants to do a set operation with the
 * local peer
 *
 * @param other_peer the other peer
 * @param context_msg message with application specific information from
 *        the other peer
 * @param request request from the other peer, use GNUNET_SET_accept
 *        to accept it, otherwise the request will be refused
 *        Note that we don't use a return value here, as it is also
 *        necessary to specify the set we want to do the operation with,
 *        whith sometimes can be derived from the context message.
 *        Also necessary to specify the timeout.
 */
typedef void
(*GNUNET_SET_ListenCallback) (void *cls,
                              const struct GNUNET_PeerIdentity *other_peer,
                              const struct GNUNET_MessageHeader *context_msg,
                              struct GNUNET_SET_Request *request);



/**
 * Create an empty set, supporting the specified operation.
 *
 * @param cfg configuration to use for connecting to the
 *        set service
 * @param op operation supported by the set
 *        Note that the operation has to be specified
 *        beforehand, as certain set operations need to maintain
 *        data structures spefific to the operation
 * @return a handle to the set
 */
struct GNUNET_SET_Handle *
GNUNET_SET_create (struct GNUNET_CONFIGURATION_Handle *cfg,
                   enum GNUNET_SET_OperationType op);


/**
 * Add an element to the given set.
 * After the element has been added (in the sense of being
 * transmitted to the set service), cont will be called.
 * Calls to add_element can be queued
 *
 * @param set set to add element to
 * @param element element to add to the set
 * @param cont continuation called after the element has been added
 * @param cont_cls closure for cont
 */
void
GNUNET_SET_add_element (struct GNUNET_SET_Handle *set,
                        const struct GNUNET_SET_Element *element,
                        GNUNET_SET_Continuation cont,
                        void *cont_cls);


/**
 * Remove an element to the given set.
 * After the element has been removed (in the sense of the
 * request being transmitted to the set service), cont will be called.
 * Calls to remove_element can be queued
 *
 * @param set set to remove element from
 * @param element element to remove from the set
 * @param cont continuation called after the element has been removed
 * @param cont_cls closure for cont
 */
void
GNUNET_SET_remove_element (struct GNUNET_SET_Handle *set,
                           const struct GNUNET_SET_Element *element,
                           GNUNET_SET_Continuation cont,
                           void *cont_cls);


/**
 * Destroy the set handle, and free all associated resources.
 */
void
GNUNET_SET_destroy (struct GNUNET_SET_Handle *set);


/**
 * Evaluate a set operation with our set and the set of another peer.
 *
 * @param set set to use
 * @param other_peer peer with the other set
 * @param app_id hash for the application using the set
 * @param context_msg additional information for the request
 * @param result_cb called on error or success
 * @param result_cls closure for result_cb
 * @return a handle to cancel the operation
 */
struct GNUNET_SET_OperationHandle *
GNUNET_SET_evaluate (struct GNUNET_SET_Handle *set,
                     const struct GNUNET_PeerIdentity *other_peer,
                     const struct GNUNET_HashCode *app_id,
                     const struct GNUNET_MessageHeader *context_msg,
                     struct GNUNET_TIME_Relative timeout,
                     enum GNUNET_SET_ResultMode result_mode,
                     GNUNET_SET_ResultIterator result_cb,
                     void *result_cls);




/**
 * Wait for set operation requests for the given application id
 * 
 * @param cfg configuration to use for connecting to
 *            the set service
 * @param operation operation we want to listen for
 * @param app_id id of the application that handles set operation requests
 * @param listen_cb called for each incoming request matching the operation
 *                  and application id
 * @param listen_cls handle for listen_cb
 * @return a handle that can be used to cancel the listen operation
 */
struct GNUNET_SET_ListenHandle *
GNUNET_SET_listen (struct GNUNET_CONFIGURATION_Handle *cfg,
                   enum GNUNET_SET_OperationType op_type,
                   const struct GNUNET_HashCode *app_id,
                   GNUNET_SET_ListenCallback listen_cb,
                   void *listen_cls);



/**
 * Cancel the given listen operation.
 *
 * @param lh handle for the listen operation
 */
void
GNUNET_SET_listen_cancel (struct GNUNET_SET_ListenHandle *lh);


/**
 * Accept a request we got via GNUNET_SET_listen
 *
 * @param request request to accept
 * @param set set used for the requested operation 
 * @param timeout timeout for the set operation
 * @param result_cb callback for the results
 * @param cls closure for result_cb
 */
struct GNUNET_SET_OperationHandle *
GNUNET_SET_accept (struct GNUNET_SET_Request *request,
                   struct GNUNET_SET_Handle *set,
                   struct GNUNET_TIME_Relative timeout,
                   enum GNUNET_SET_ResultMode result_mode,
                   GNUNET_SET_ResultIterator result_cb,
                   void *cls);


/**
 * Cancel the given set operation.
 *
 * @param op set operation to cancel
 */
void
GNUNET_SET_operation_cancel (struct GNUNET_SET_OperationHandle *oh);


#if 0                           /* keep Emacsens' auto-indent happy */
{
#endif
#ifdef __cplusplus
}
#endif

#endif