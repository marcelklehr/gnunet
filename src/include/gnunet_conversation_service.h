/*
  This file is part of GNUnet
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
 * @file include/gnunet_conversation_service.h
 * @brief API to the conversation service
 * @author Simon Dieterle
 * @author Andreas Fuchs
 * @author Christian Grothoff
 *
 * TODO:
 * - call waiting
 * - put on hold
 */
#ifndef GNUNET_CONVERSATION_SERVICE_H
#define GNUNET_CONVERSATION_SERVICE_H

#ifdef __cplusplus
extern "C"
{
#if 0				/* keep Emacsens' auto-indent happy */
}
#endif
#endif

#include "gnunet_util_lib.h"

/**
 * Version of the conversation API.
 */
#define GNUNET_CONVERSATION_VERSION 0x00000001


enum GNUNET_CONVERSATION_RejectReason
{
  GNUNET_CONVERSATION_REJECT_REASON_GENERIC = 0,
  GNUNET_CONVERSATION_REJECT_REASON_NOT_AVAILABLE,
  GNUNET_CONVERSATION_REJECT_REASON_NO_CLIENT,
  GNUNET_CONVERSATION_REJECT_REASON_ACTIVE_CALL,
  GNUNET_CONVERSATION_REJECT_REASON_NOT_WANTED,
  GNUNET_CONVERSATION_REJECT_REASON_NO_ANSWER

};


enum GNUNET_CONVERSATION_NotificationType
{
  GNUNET_CONVERSATION_NT_SERVICE_BLOCKED = 0,
  GNUNET_CONVERSATION_NT_NO_PEER,
  GNUNET_CONVERSATION_NT_NO_ANSWER,
  GNUNET_CONVERSATION_NT_AVAILABLE_AGAIN,
  GNUNET_CONVERSATION_NT_CALL_ACCEPTED,
  GNUNET_CONVERSATION_NT_CALL_TERMINATED
};


/**
 *
 */
struct GNUNET_CONVERSATION_MissedCall
{
  struct GNUNET_PeerIdentity peer;
  struct GNUNET_TIME_Absolute time; 
};


struct GNUNET_CONVERSATION_MissedCallNotification
{
  int number;
  struct GNUNET_CONVERSATION_MissedCall *calls;
};

struct GNUNET_CONVERSATION_CallInformation;

struct GNUNET_CONVERSATION_Handle;


/**
 * Method called whenever a call is incoming
 *
 * @param cls closure
 * @param handle to the conversation session
 * @param caller peer that calls you
 */
typedef void (GNUNET_CONVERSATION_CallHandler) (void *cls,
						struct GNUNET_CONVERSATION_Handle *handle,
						const struct GNUNET_PeerIdentity *caller);


/**
 * Method called whenever a call is rejected
 *
 * @param cls closure
 * @param handle to the conversation session
 * @param reason reason given for rejecting the call
 * @param peer peer that rejected your call
 */
typedef void (GNUNET_CONVERSATION_RejectHandler) (void *cls,
						  struct GNUNET_CONVERSATION_Handle *handle,
						  enum GNUNET_CONVERSATION_RejectReason reason,
						  const struct GNUNET_PeerIdentity *peer);


/**
 * Method called whenever a notification is there
 *
 * @param cls closure
 * @param handle to the conversation session
 * @param type the type of the notification
 * @param peer peer that the notification is about
 */
typedef void (GNUNET_CONVERSATION_NotificationHandler) (void *cls,
							struct GNUNET_CONVERSATION_Handle *handle,
							enum GNUNET_CONVERSATION_NotificationType type,
							const struct GNUNET_PeerIdentity *peer);


/**
 * Method called whenever a notification for missed calls is there
 *
 * @param cls closure
 * @param handle to the conversation session
 * @param missed_calls a list of missed calls
 */
typedef void (GNUNET_CONVERSATION_MissedCallHandler) (void *cls,
						      struct GNUNET_CONVERSATION_Handle *handle,
						      struct GNUNET_CONVERSATION_MissedCallNotification *missed_calls);


/**
 * Connect to the VoIP service
 *
 * @param cfg configuration
 * @param cls NULL
 * @param call_handler the callback which is called when a call is incoming
 * @param reject_handler the callback which is called when a call is rejected
 * @param notification_handler the callback which is called when there is a notification
 * @param missed_call_handler the callback which is called when the service notifies the client about missed clients
 * @return handle to the connection to the conversation service
 */
struct GNUNET_CONVERSATION_Handle *
GNUNET_CONVERSATION_connect (const struct GNUNET_CONFIGURATION_Handle *cfg, 
			     void *cls,
			     GNUNET_CONVERSATION_CallHandler call_handler,
			     GNUNET_CONVERSATION_RejectHandler reject_handler,
			     GNUNET_CONVERSATION_NotificationHandler notification_handler,
			     GNUNET_CONVERSATION_MissedCallHandler missed_call_handler);


/**
 * Disconnect from the VoIP service
 *
 * @param handle handle to the VoIP connection
 */
void
GNUNET_CONVERSATION_disconnect (struct GNUNET_CONVERSATION_Handle *handle);


/**
 * Establish a call
 *
 * @param handle handle to the VoIP connection
 * @param callee the peer (PeerIdentity or GNS name) to call
 * @param doGnsLookup 0 = no GNS lookup or 1  = GNS lookup
 */
void
GNUNET_CONVERSATION_call (struct GNUNET_CONVERSATION_Handle *handle, 
			  const char *callee,
			  int doGnsLookup);


/**
 * Terminate the active call
 *
 * @param handle handle to the VoIP connection
 */
void 
GNUNET_CONVERSATION_hangup (struct GNUNET_CONVERSATION_Handle *handle);


/**
 * Accept an incoming call
 *
 * @param handle handle to the VoIP connection
 */
void
GNUNET_CONVERSATION_accept (struct GNUNET_CONVERSATION_Handle *handle);


/**
 * Reject an incoming call
 *
 * @param handle handle to the VoIP connection
 */
void
GNUNET_CONVERSATION_reject (struct GNUNET_CONVERSATION_Handle *handle);



////////////////////////////////////////////////////////////////////
////////////////////////// NEW API /////////////////////////////////
////////////////////////////////////////////////////////////////////

/* 
   NOTE: This API is deliberately deceptively simple; the idea
   is that advanced features (such as answering machines) will
   be done with a separate service (an answering machine service)
   with its own APIs; the speaker/microphone abstractions are
   used to facilitate plugging in custom logic for implementing
   such a service later by creating "software" versions of
   speakers and microphones that record to disk or play a file.
   Notifications about missed calls should similarly be done
   using a separate service; CONVERSATION is supposed to be just
   the "bare bones" voice service.

   Meta data passing is supported so that advanced services
   can identify themselves appropriately.

   As this is supposed to be a "secure" service, caller ID is of
   course provided as part of the basic implementation, as only the
   CONVERSATION service can know for sure who it is that we are
   talking to.
 */


#include "gnunet_util_lib.h"
#include "gnunet_identity_service.h"
#include "gnunet_namestore_service.h"
#include "gnunet_speaker_lib.h"
#include "gnunet_microphone_lib.h"


/**
 * Information about the current status of a call.  Each call
 * progresses from ring over ready to terminated.  Steps may
 * be skipped.
 */
enum GNUNET_CONVERSATION_EventCode
{
  /**
   * The phone is ringing, caller ID is provided in the varargs as 
   * a `const char *`.  The caller ID will be a GNS name.
   */
  GNUNET_CONVERSATION_EC_RING,

  /**
   * We are the caller and are now ringing the other party.  
   * The varargs will be empty.
   */
  GNUNET_CONVERSATION_EC_RINGING,
  
  /**
   * We are ready to talk, metadata about the call may be supplied
   * as a `const char *` in the varargs.
   */
  GNUNET_CONVERSATION_EC_READY,

  /**
   * We failed to locate a phone record in GNS.
   */
  GNUNET_CONVERSATION_EC_GNS_FAIL,

  /**
   * The phone is busy.  Varargs will be empty.
   */
  GNUNET_CONVERSATION_EC_BUSY,
  
  /**
   * The conversation was terminated, a reason may be supplied
   * as a `const char *` in the varargs.
   */
  GNUNET_CONVERSATION_EC_TERMINATED
  
};


/**
 * Function called with an event emitted by a phone.
 *
 * @param cls closure
 * @param code type of the event on the phone
 * @param ... additional information, depends on @a code
 */
typedef void (*GNUNET_CONVERSATION_EventHandler)(void *cls,
						 enum GNUNET_CONVERSATION_EventCode code,
						 ...);


/**
 * A phone is a device that can ring to signal an incoming call and
 * that you can pick up to answer the call and hang up to terminate
 * the call.  You can also hang up a ringing phone immediately
 * (without picking it up) to stop it from ringing.  Phones have
 * caller ID.  You can ask the phone for its record and make that
 * record available (via GNS) to enable others to call you.
 * Multiple phones maybe connected to the same line (the line is
 * something rather internal to a phone and not obvious from it).
 * You can only have one conversation per phone at any time.
 */
struct GNUNET_CONVERSATION_Phone;


/**
 * Create a new phone.
 *
 * @param cfg configuration for the phone; specifies the phone service and
 *        which line the phone is to be connected to
 * @param ego ego to use for name resolution (when determining caller ID)
 * @param event_handler how to notify the owner of the phone about events
 * @param event_handler_cls closure for @a event_handler
 */
struct GNUNET_CONVERSATION_Phone *
GNUNET_CONVERSATION_phone_create (const struct GNUNET_CONFIGURATION_Handle *cfg,
                                  const struct GNUNET_IDENTITY_Ego *ego,
				  GNUNET_CONVERSATION_EventHandler event_handler,
				  void *event_handler_cls);


/**
 * Fill in a namestore record with the contact information
 * for this phone.  Note that the filled in "data" value
 * is only valid until the phone is destroyed.
 *
 * @param phone phone to create a record for
 * @param rd namestore record to fill in
 */
void
GNUNET_CONVERSATION_phone_get_record (struct GNUNET_CONVERSATION_Phone *phone,
				      struct GNUNET_NAMESTORE_RecordData *rd);


/**
 * Picks up a (ringing) phone.  This will connect the speaker 
 * to the microphone of the other party, and vice versa.
 *
 * @param phone phone to pick up
 * @param metadata meta data to give to the other user about the pick up event
 * @param speaker speaker to use
 * @param mic microphone to use
 */
void
GNUNET_CONVERSATION_phone_pick_up (struct GNUNET_CONVERSATION_Phone *phone,
                                   const char *metadata,
                                   struct GNUNET_SPEAKER_Handle *speaker,
                                   struct GNUNET_MICROPHONE_Handle *mic);


/**
 * Hang up up a (possibly ringing) phone.  This will notify the other
 * party that we are no longer interested in talking with them.
 *
 * @param phone phone to pick up
 * @param reason text we give to the other party about why we terminated the conversation
 */
void
GNUNET_CONVERSATION_phone_hang_up (struct GNUNET_CONVERSATION_Phone *phone,
                                   const char *reason);


/**
 * Destroys a phone.
 *
 * @param phone phone to destroy
 */
void
GNUNET_CONVERSATION_phone_destroy (struct GNUNET_CONVERSATION_Phone *phone);


/**
 * Handle for an outgoing call.
 */
struct GNUNET_CONVERSATION_Call;


/**
 * Call the phone of another user.
 *
 * @param cfg configuration to use, specifies our phone service
 * @param caller_id identity of the caller
 * @param callee GNS name of the callee (used to locate the callee's record)
 * @param speaker speaker to use (will be used automatically immediately once the
 *        #GNUNET_CONVERSATION_EC_READY event is generated); we will NOT generate
 *        a ring tone on the speaker
 * @param mic microphone to use (will be used automatically immediately once the
 *        #GNUNET_CONVERSATION_EC_READY event is generated)
 * @param event_handler how to notify the owner of the phone about events
 * @param event_handler_cls closure for @a event_handler
 */
struct GNUNET_CONVERSATION_Call *
GNUNET_CONVERSATION_call_start (const struct GNUNET_CONFIGURATION_Handle *cfg,
				struct GNUNET_IDENTITY_Ego *caller_id,
				const char *callee,
				struct GNUNET_SPEAKER_Handle *speaker,
				struct GNUNET_MICROPHONE_Handle *mic,
				GNUNET_CONVERSATION_EventHandler event_handler,
				void *event_handler_cls);


/**
 * Terminate a call.  The call may be ringing or ready at this time.
 *
 * @param call call to terminate
 * @param reason if the call was active (ringing or ready) this will be the
 *        reason given to the other user for why we hung up
 */
void
GNUNET_CONVERSATION_call_stop (struct GNUNET_CONVERSATION_Call *call,
			       const char *reason);


#if 0				/* keep Emacsens' auto-indent happy */
{
#endif
#ifdef __cplusplus
}
#endif

#endif
