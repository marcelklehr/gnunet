/*
  This file is part of GNUnet.
  (C) 2008--2013 Christian Grothoff (and other contributing authors)

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
 * @file testbed/gnunet-service-testbed-logger.c
 * @brief service for collecting messages and writing to a file
 * @author Sree Harsha Totakura
 */

#include "platform.h"
#include "gnunet_util_lib.h"

/**
 * Generic logging shorthand
 */
#define LOG(type, ...)                         \
  GNUNET_log (type, __VA_ARGS__)

/**
 * Debug logging shorthand
 */
#define LOG_DEBUG(...)                          \
  LOG (GNUNET_ERROR_TYPE_DEBUG, __VA_ARGS__)

/**
 * The message queue for sending messages to clients
 */
struct MessageQueue
{
  /**
   * The message to be sent
   */
  struct GNUNET_MessageHeader *msg;

  /**
   * The client to send the message to
   */
  struct GNUNET_SERVER_Client *client;

  /**
   * next pointer for DLL
   */
  struct MessageQueue *next;

  /**
   * prev pointer for DLL
   */
  struct MessageQueue *prev;
};

/**
 * Current Transmit Handle; NULL if no notify transmit exists currently
 */
static struct GNUNET_SERVER_TransmitHandle *transmit_handle;

/**
 * The message queue head
 */
static struct MessageQueue *mq_head;

/**
 * The message queue tail
 */
static struct MessageQueue *mq_tail;

/**
 * Handle for buffered writing.
 */
struct GNUNET_BIO_WriteHandle *bio;

/**
 * The shutdown task handle
 */
static GNUNET_SCHEDULER_TaskIdentifier shutdown_task_id;


/**
 * Function called to notify a client about the connection begin ready to queue
 * more data.  "buf" will be NULL and "size" zero if the connection was closed
 * for writing in the meantime.
 *
 * @param cls NULL
 * @param size number of bytes available in buf
 * @param buf where the callee should write the message
 * @return number of bytes written to buf
 */
static size_t
transmit_ready_notify (void *cls, size_t size, void *buf)
{
  struct MessageQueue *mq_entry;

  transmit_handle = NULL;
  mq_entry = mq_head;
  GNUNET_assert (NULL != mq_entry);
  if (0 == size)
    return 0;
  GNUNET_assert (ntohs (mq_entry->msg->size) <= size);
  size = ntohs (mq_entry->msg->size);
  memcpy (buf, mq_entry->msg, size);
  GNUNET_free (mq_entry->msg);
  GNUNET_SERVER_client_drop (mq_entry->client);
  GNUNET_CONTAINER_DLL_remove (mq_head, mq_tail, mq_entry);
  GNUNET_free (mq_entry);
  mq_entry = mq_head;
  if (NULL != mq_entry)
    transmit_handle =
        GNUNET_SERVER_notify_transmit_ready (mq_entry->client,
                                             ntohs (mq_entry->msg->size),
                                             GNUNET_TIME_UNIT_FOREVER_REL,
                                             &transmit_ready_notify, NULL);
  return size;
}


/**
 * Queues a message in send queue for sending to the service
 *
 * @param client the client to whom the queued message has to be sent
 * @param msg the message to queue
 */
void
queue_message (struct GNUNET_SERVER_Client *client,
                   struct GNUNET_MessageHeader *msg)
{
  struct MessageQueue *mq_entry;
  uint16_t type;
  uint16_t size;

  type = ntohs (msg->type);
  size = ntohs (msg->size);
  mq_entry = GNUNET_malloc (sizeof (struct MessageQueue));
  mq_entry->msg = msg;
  mq_entry->client = client;
  GNUNET_SERVER_client_keep (client);
  LOG_DEBUG ("Queueing message of type %u, size %u for sending\n", type,
             ntohs (msg->size));
  GNUNET_CONTAINER_DLL_insert_tail (mq_head, mq_tail, mq_entry);
  if (NULL == transmit_handle)
    transmit_handle =
        GNUNET_SERVER_notify_transmit_ready (client, size,
                                             GNUNET_TIME_UNIT_FOREVER_REL,
                                             &transmit_ready_notify, NULL);
}


/**
 * Message handler for GNUNET_MESSAGE_TYPE_TESTBED_ADDHOST messages
 *
 * @param cls NULL
 * @param client identification of the client
 * @param message the actual message
 */
static void
handle_log_msg (void *cls, struct GNUNET_SERVER_Client *client,
                const struct GNUNET_MessageHeader *msg)
{
  uint16_t ms;

  ms = ntohs (msg->size);
  ms -= sizeof (struct GNUNET_MessageHeader);
  GNUNET_BIO_write (bio, &msg[1], ms);
  GNUNET_SERVER_receive_done (client, GNUNET_OK);
}


/**
 * Task to clean up and shutdown nicely
 *
 * @param cls NULL
 * @param tc the TaskContext from scheduler
 */
static void
shutdown_task (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  struct MessageQueue *mq_entry;

  shutdown_task_id = GNUNET_SCHEDULER_NO_TASK;
  if (NULL != transmit_handle)
    GNUNET_SERVER_notify_transmit_ready_cancel (transmit_handle);
  while (NULL != (mq_entry = mq_head))
  {
    GNUNET_free (mq_entry->msg);
    GNUNET_SERVER_client_drop (mq_entry->client);
    GNUNET_CONTAINER_DLL_remove (mq_head, mq_tail, mq_entry);
    GNUNET_free (mq_entry);
  }
  GNUNET_BIO_write_close (bio);
}


/**
 * Testbed setup
 *
 * @param cls closure
 * @param server the initialized server
 * @param cfg configuration to use
 */
static void
logger_run (void *cls, struct GNUNET_SERVER_Handle *server,
             const struct GNUNET_CONFIGURATION_Handle *cfg)
{
  static const struct GNUNET_SERVER_MessageHandler message_handlers[] = {
    {&handle_log_msg, NULL, GNUNET_MESSAGE_TYPE_TESTBED_LOGGER_MSG, 0},
    {NULL, NULL, 0, 0}
  };
  char *dir;
  char *fn;
  pid_t pid;

  if (GNUNET_OK !=
      GNUNET_CONFIGURATION_get_value_filename (cfg, "TESTBED-LOGGER", "DIR",
                                               &dir))
  {
    LOG (GNUNET_ERROR_TYPE_ERROR, "Not logging directory definied.  Exiting\n");    
    return;
  }
  pid = getpid ();
  (void) GNUNET_asprintf (&fn, "%s/%jd.dat", dir, (intmax_t) pid);
  GNUNET_free (dir);
  if (NULL == (bio = GNUNET_BIO_write_open (fn)))
  {
    GNUNET_free (fn);
    return;
  }
  GNUNET_free (fn);
  GNUNET_SERVER_add_handlers (server, message_handlers);
  shutdown_task_id =
      GNUNET_SCHEDULER_add_delayed (GNUNET_TIME_UNIT_FOREVER_REL,
                                    &shutdown_task, NULL);
  LOG_DEBUG ("TESTBED-LOGGER startup complete\n");
}


/**
 * The starting point of execution
 */
int
main (int argc, char *const *argv)
{
  //sleep (15);                 /* Debugging */
  return (GNUNET_OK ==
          GNUNET_SERVICE_run (argc, argv, "testbed-logger", 
                              GNUNET_SERVICE_OPTION_NONE,
                              &logger_run, NULL)) ? 0 : 1;
}

/* end of gnunet-service-testbed.c */
