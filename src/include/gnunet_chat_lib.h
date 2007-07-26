/*
     This file is part of GNUnet
     (C) 2007 Christian Grothoff (and other contributing authors)

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
 * @file include/gnunet_chat_lib.h
 * @brief support for chat
 * @author Christian Grothoff
 */

#ifndef GNUNET_CHAT_LIB_H
#define GNUNET_CHAT_LIB_H

#include "gnunet_util_core.h"

#ifdef __cplusplus
extern "C"
{
#if 0                           /* keep Emacsens' auto-indent happy */
}
#endif
#endif

/**
 * Version number.
 */
#define GNUNET_CHAT_VERSION "0.0.0"

typedef enum {
  /**
   * No special options.
   */
  GNUNET_CHAT_MSG_OPTION_NONE = 0,

  /**
   * Encrypt the message so that only the
   * receiver can decrypt it.
   */
  GNUNET_CHAT_MSG_PRIVATE = 1,

  /**
   * Hide the identity of the sender.
   */
  GNUNET_CHAT_MSG_ANONYMOUS = 2,

  /**
   * Sign the content, authenticating the
   * sender (using the provided private
   * key, which may represent a pseudonym).
   */
  GNUNET_CHAT_MSG_AUTHENTICATED = 4,

  /**
   * Authenticate for the receiver, but
   * ensure that receiver cannot prove
   * authenticity to third parties later.
   */
  GNUNET_CHAT_MSG_OFF_THE_RECORD = 8,

  /**
   * Require signed acknowledgement before 
   * completing delivery (and of course, only
   * acknowledge if delivery is guaranteed).
   */
  GNUNET_CHAT_MSG_ACKNOWLEDGED = 16,

} GNUNET_CHAT_MSG_OPTIONS;

/**
 * Callback function to iterate over rooms.
 *
 * @return OK to continue, SYSERR to abort iteration
 */
typedef int (*GNUNET_CHAT_Room_Iterator)(const char * room,
					 const char * topic,
					 void * cls);

/**
 * List all of the (publically visible) chat rooms.
 * @return number of rooms on success, SYSERR if iterator aborted
 */
int GNUNET_CHAT_list_rooms(struct GE_Context * ectx,
			   struct GC_Configuration * cfg,
			   GNUNET_CHAT_Room_Iterator it,
			   void * cls);


/**
 * Handle for a (joined) chat room.
 */
struct GNUNET_CHAT_Room;

/**
 * A message was send in the chat to us.
 *
 * @param timestamp when was the message sent?
 * @param senderNick what is the nickname of the sender? (maybe NULL)
 * @param message the message (maybe NULL, especially if confirmation
 *        is requested before delivery; the protocol will ensure
 *        that this function is called again with the full message
 *        if a confirmation is transmitted; if the message is NULL,
 *        the user is merely asked if engaging in the exchange is ok
 * @param room in which room was the message received?
 * @param options options for the message
 * @return OK to accept the message now, NO to 
 *         accept (but user is away), SYSERR to signal denied delivery
 */
typedef int (*GNUNET_CHAT_Message_Callback)(void * cls,
					    struct GNUNET_CHAT_Room * room,
					    const char * senderNick,
					    const char * message,
					    cron_t timestamp,
					    GNUNET_CHAT_MSG_OPTIONS options);

/**
 * Join a chat room.  
 *
 * @param nickname the nick you want to use
 * @param memberInfo public information about you
 * @param callback which function to call if a message has
 *        been received?
 * @param cls argument to callback
 * @return NULL on error 
 */ 
struct GNUNET_CHAT_Room *
GNUNET_CHAT_join_room(struct GE_Context * ectx,
		      struct GC_Configuration * cfg,
		      const char * nickname,
		      const PublicKey * me,
		      const struct PrivateKey * key,
		      const char * memberInfo,
		      GNUNET_CHAT_Message_Callback callback,
		      void * cls);

/**
 * Leave a chat room.
 */ 
void GNUNET_CHAT_leave_room(struct GNUNET_CHAT_Room * room);


/**
 * Message delivery confirmations.
 *
 * @param timestamp when was the message sent?
 * @param senderNick what is the nickname of the receiver?
 * @param message the message (maybe NULL)
 * @param room in which room was the message received?
 * @param options what were the options of the message
 * @param response what was the receivers response (OK, NO, SYSERR).
 * @param receipt signature confirming delivery (maybe NULL, only
 *        if confirmation was requested)
 * @return OK to continue, SYSERR to refuse processing further
 *         confirmations from anyone for this message
 */
typedef int (*GNUNET_CHAT_Message_Confirmation)(void * cls,
						struct GNUNET_CHAT_Room * room,
						const char * receiverNick,
						const PublicKey * receiverKey,
						const char * message,
						cron_t timestamp,
						GNUNET_CHAT_MSG_OPTIONS options,
						int response,
						const Signature * receipt);


/**
 * Send a message.
 *
 * @param receiver use NULL to send to everyone in the room
 * @return OK on success, SYSERR on error
 */
int
GNUNET_CHAT_send_message(struct GNUNET_CHAT_Room * room,
			 const char * message,
			 GNUNET_CHAT_Message_Confirmation callback,
			 void * cls,
			 GNUNET_CHAT_MSG_OPTIONS options,
			 const PublicKey * receiver);


/**
 * Callback function to iterate over room members.
 */
typedef int (*GNUNET_CHAT_Member_Iterator)(const char * nickname,
					   const PublicKey * owner,
					   const char * memberInfo,
					   cron_t lastConfirmed,
					   void * cls);

/**
 * List all of the (known) chat members.
 * @return number of rooms on success, SYSERR if iterator aborted
 */
int GNUNET_CHAT_list_members(struct GNUNET_CHAT_Room * room,
			     GNUNET_CHAT_Member_Iterator it,
			     void * cls);



#if 0                           /* keep Emacsens' auto-indent happy */
{
#endif
#ifdef __cplusplus
}
#endif

#endif
