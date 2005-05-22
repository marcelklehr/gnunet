/*
     This file is part of GNUnet.
     (C) 2001, 2002, 2003, 2004, 2005 Christian Grothoff (and other contributing authors)

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
 * @file applications/fs/module/fs.c
 * @brief main functions of the file sharing service
 * @author Christian Grothoff
 *
 * FS CORE. This is the code that is plugged into the GNUnet core to
 * enable File Sharing.
 */

#include "platform.h"
#include "gnunet_util.h"
#include "gnunet_protocols.h"
#include "gnunet_gap_service.h"
#include "gnunet_dht_service.h"
#include "gnunet_datastore_service.h"
#include "gnunet_traffic_service.h"
#include "ecrs_core.h"
#include "migration.h"
#include "ondemand.h"
#include "querymanager.h"
#include "fs.h"


typedef struct {
  struct DHT_GET_RECORD * rec;
  unsigned int prio;
} DHT_GET_CLS;

typedef struct {
  struct DHT_PUT_RECORD * rec;
} DHT_PUT_CLS;

/**
 * Global core API.
 */
static CoreAPIForApplication * coreAPI;

/**
 * GAP service.
 */
static GAP_ServiceAPI * gap;

/**
 * DHT service.  Maybe NULL!
 */
static DHT_ServiceAPI * dht;

/**
 * Datastore service.
 */
static Datastore_ServiceAPI * datastore;

/**
 * Traffic service.
 */
static Traffic_ServiceAPI * traffic;

static Mutex lock;

/**
 * ID of the FS table in the DHT infrastructure.
 */
static DHT_TableId dht_table;

/**
 * Store an item in the datastore.
 *
 * @param key the key of the item
 * @param value the value to store
 * @param prio how much does our routing code value
 *        this datum?
 * @return OK if the value could be stored,
 *         NO if the value verifies but is not stored,
 *         SYSERR if the value is malformed
 */
static int gapPut(void * closure,
		  const HashCode512 * key,
		  const DataContainer * value,
		  unsigned int prio) {
  Datastore_Value * dv;
  GapWrapper * gw;
  unsigned int size;
  int ret;
  HashCode512 hc;
  cron_t et;
  cron_t now;
  EncName enc;

  if (ntohl(value->size) < sizeof(GapWrapper)) {
    BREAK();
    return SYSERR;
  }
  gw = (GapWrapper*) value;
  size = ntohl(gw->dc.size)
    - sizeof(GapWrapper)
    + sizeof(Datastore_Value);
  if ( (OK != getQueryFor(size - sizeof(Datastore_Value),
			  (DBlock*)&gw[1],
			  &hc)) ||
       (! equalsHashCode512(&hc, key)) ) {
    BREAK(); /* value failed verification! */
    return SYSERR;
  }

  dv = MALLOC(size);
  dv->size = htonl(size);
  dv->type = htonl(getTypeOfBlock(size - sizeof(Datastore_Value),
				  (DBlock*) &gw[1]));
  dv->prio = htonl(prio);
  dv->anonymityLevel = htonl(0);
  et = ntohll(gw->timeout);
  cronTime(&now);
  /* bound ET to MAX_MIGRATION_EXP from now */
  if (et > now) {
    et -= now;
    et = et % MAX_MIGRATION_EXP;
    et += now;
  }
  dv->expirationTime = htonll(et);
  memcpy(&dv[1],
	 &gw[1],
	 size - sizeof(Datastore_Value));
  if (YES != isDatumApplicable(ntohl(dv->type),
			       ntohl(dv->size) - sizeof(Datastore_Value),
			       (DBlock*) &dv[1],
			       0,
			       key)) {
    BREAK();
    FREE(dv);
    return SYSERR;
  }
  processResponse(key, dv);
  IFLOG(LOG_DEBUG,
	hash2enc(key,
		 &enc));
  LOG(LOG_DEBUG,
      "FS received GAP-PUT request (query: %s)\n",
      &enc);
  ret = datastore->putUpdate(key,
			     dv);
  FREE(dv);
  return ret;
}

static int get_result_callback(const HashCode512 * key,
			       const DataContainer * value,
			       DHT_GET_CLS * cls) {
  EncName enc;

  IFLOG(LOG_DEBUG,
	hash2enc(key,
		 &enc));
  LOG(LOG_DEBUG,
      "Found reply to query %s.\n",
      &enc);
  gapPut(NULL,
	 key,
	 value,
	 cls->prio);
  return OK;
}				

static void get_complete_callback(DHT_GET_CLS * cls) {
  dht->get_stop(cls->rec);
  FREE(cls);
}

static void put_complete_callback(DHT_PUT_CLS * cls) {
  dht->put_stop(cls->rec);
  FREE(cls);
}

/**
 * Process a query from the client. Forwards to the network.
 *
 * @return SYSERR if the TCP connection should be closed, otherwise OK
 */
static int csHandleRequestQueryStart(ClientHandle sock,
				     const CS_HEADER * req) {
  const RequestSearch * rs;
  unsigned int keyCount;
  EncName enc;

  if (ntohs(req->size) < sizeof(RequestSearch)) {
    BREAK();
    return SYSERR;
  }
  rs = (const RequestSearch*) req;
  IFLOG(LOG_DEBUG,
	hash2enc(&rs->query[0],
		 &enc));
  LOG(LOG_DEBUG,
      "FS received QUERY START (query: %s)\n",
      &enc);
  trackQuery(&rs->query[0],
	     ntohl(rs->type),
	     sock);
  keyCount = 1 + (ntohs(req->size) - sizeof(RequestSearch)) / sizeof(HashCode512);
  gap->get_start(ntohl(rs->type),
		 ntohl(rs->anonymityLevel),		
		 keyCount,
		 &rs->query[0],
		 ntohll(rs->expiration),
		 ntohl(rs->prio));
  if ( (ntohl(rs->anonymityLevel) == 0) &&
       (dht != NULL) ) {
    DHT_GET_CLS * cls;

    cls = MALLOC(sizeof(DHT_GET_CLS));
    cls->prio = ntohl(rs->prio);
    cls->rec = dht->get_start(&dht_table,
			      ntohl(rs->type),
			      keyCount,
			      &rs->query[0],
			      ntohll(rs->expiration),
			      (DataProcessor) &get_result_callback,
			      cls,
			      (DHT_OP_Complete) &get_complete_callback,
			      cls);
  }
  return OK;
}

/**
 * Stop processing a query.
 *
 * @return SYSERR if the TCP connection should be closed, otherwise OK
 */
static int csHandleRequestQueryStop(ClientHandle sock,
				    const CS_HEADER * req) {
  RequestSearch * rs;
  EncName enc;

  if (ntohs(req->size) < sizeof(RequestSearch)) {
    BREAK();
    return SYSERR;
  }
  rs = (RequestSearch*) req;
  IFLOG(LOG_DEBUG,
	hash2enc(&rs->query[0],
		 &enc));
  LOG(LOG_DEBUG,
      "FS received QUERY STOP (query: %s)\n",
      &enc);
  if (ntohl(rs->anonymityLevel) == 0) {
    /* FIXME 0.7.1: cancel with dht? */
  }
  gap->get_stop(ntohl(rs->type),
		1 + (ntohs(req->size) - sizeof(RequestSearch)) / sizeof(HashCode512),
		&rs->query[0]);
  untrackQuery(&rs->query[0], sock);
  return OK;
}

/**
 * Process a request to insert content from the client.
 *
 * @return SYSERR if the TCP connection should be closed, otherwise OK
 */
static int csHandleRequestInsert(ClientHandle sock,
				 const CS_HEADER * req) {
  const RequestInsert * ri;
  Datastore_Value * datum;
  int ret;
  HashCode512 query;
  unsigned int type;
  EncName enc;

  if (ntohs(req->size) < sizeof(RequestInsert)) {
    BREAK();
    return SYSERR;
  }
  ri = (const RequestInsert*) req;
  datum = MALLOC(sizeof(Datastore_Value) +
		 ntohs(req->size) - sizeof(RequestInsert));
  datum->size = htonl(sizeof(Datastore_Value) +
		      ntohs(req->size) - sizeof(RequestInsert));
  datum->expirationTime = ri->expiration;
  datum->prio = ri->prio;
  datum->anonymityLevel = ri->anonymityLevel;
  if (OK != getQueryFor(ntohs(ri->header.size) - sizeof(RequestInsert),
			(const DBlock*)&ri[1],
			&query)) {
    BREAK();
    FREE(datum);
    return SYSERR;
  }
  IFLOG(LOG_DEBUG,
	hash2enc(&query,
		 &enc));
  type = getTypeOfBlock(ntohs(ri->header.size) - sizeof(RequestInsert),
			(const DBlock*) &ri[1]);
  LOG(LOG_DEBUG,
      "FS received REQUEST INSERT (query: %s, type: %u)\n",
      &enc,
      type);
  datum->type = htonl(type);
  memcpy(&datum[1],
	 &ri[1],
	 ntohs(req->size) - sizeof(RequestInsert));
  MUTEX_LOCK(&lock);
  ret = datastore->put(&query,
		       datum);
  MUTEX_UNLOCK(&lock);
  if ( (ntohl(ri->anonymityLevel) == 0) &&
       (dht != NULL) ) {
    GapWrapper * gw;
    unsigned int size;
    cron_t now;
    cron_t et;
    DHT_PUT_CLS * cls;

    size = sizeof(GapWrapper) +
      ntohs(ri->header.size) - sizeof(RequestInsert) -
      sizeof(Datastore_Value);
    gw = MALLOC(size);
    gw->dc.size = htonl(size);
    et = ntohll(ri->expiration);
    /* expiration time normalization and randomization */
    cronTime(&now);
    if (et > now) {
      et -= now;
      et = et % MAX_MIGRATION_EXP;
      if (et > 0)
	et = randomi(et);
      et = et + now;
    }
    gw->timeout = htonll(et);
    memcpy(&gw[1],
	   &ri[1],
	   size - sizeof(GapWrapper));
    cls = MALLOC(sizeof(DHT_PUT_CLS));
    cls->rec = dht->put_start(&dht_table,
			      &query,
			      15 * cronSECONDS, /* FIXME 0.7.1: better timeout for DHT PUT operation */
			      &gw->dc,
			      (DHT_OP_Complete) &put_complete_callback,
			      cls);
  }

  FREE(datum);
  return coreAPI->sendValueToClient(sock,
				    ret);
}

/**
 * Process a request to symlink a file
 */
static int csHandleRequestInitIndex(ClientHandle sock,
				    const CS_HEADER * req) {
  int ret;
  char *fn;
  RequestInitIndex *ri;
  int fnLen;

  if (ntohs(req->size) < sizeof(RequestInitIndex)) {
    BREAK();
    return SYSERR;
  }

  ri = (RequestInitIndex *) req;

  fnLen = ntohs(ri->header.size) - sizeof(RequestInitIndex);
#if CYGWIN
  if (fnLen > _MAX_PATH)
    return SYSERR;
#endif
  fn = MALLOC(fnLen + 1);
  strncpy(fn, (char*) &ri[1], fnLen+1);
  fn[fnLen] = 0;

  ret = ONDEMAND_initIndex(&ri->fileId,
          fn);

  FREE(fn);

  LOG(LOG_DEBUG,
      "Sending confirmation (%s) of index initialization request to client\n",
      ret == OK ? "success" : "failure");
  return coreAPI->sendValueToClient(sock,
            ret);
}

/**
 * Process a request to index content from the client.
 *
 * @return SYSERR if the TCP connection should be closed, otherwise OK
 */
static int csHandleRequestIndex(ClientHandle sock,
				const CS_HEADER * req) {
  int ret;
  const RequestIndex * ri;

  if (ntohs(req->size) < sizeof(RequestIndex)) {
    BREAK();
    return SYSERR;
  }
  ri = (const RequestIndex*) req;
  ret = ONDEMAND_index(datastore,
		       ntohl(ri->prio),
		       ntohll(ri->expiration),
		       ntohll(ri->fileOffset),
		       ntohl(ri->anonymityLevel),
		       &ri->fileId,
		       ntohs(ri->header.size) - sizeof(RequestIndex),
		       (const DBlock*) &ri[1]);
  LOG(LOG_DEBUG,
      "Sending confirmation (%s) of index request to client\n",
      ret == OK ? "success" : "failure");
  return coreAPI->sendValueToClient(sock,
				    ret);
}

/**
 * If the data portion and type of the value match our value in the
 * closure, copy the header (prio, anonymityLevel, expirationTime) and
 * abort the iteration: we found what we're looing for.  Otherwise
 * continue.
 */
static int completeValue(const HashCode512 * key,
			 const Datastore_Value * value,
			 void * closure) {
  Datastore_Value * comp = closure;

  if ( (comp->size != value->size) ||
       (0 != memcmp(&value[1],
		    &comp[1],
		    ntohl(value->size) - sizeof(Datastore_Value))) ) {
    LOG(LOG_DEBUG,
	"'%s' found value that does not match (%u, %u).\n",
	__FUNCTION__,
	ntohl(comp->size),
	ntohl(value->size));
    return OK;
  }
  *comp = *value; /* make copy! */
  LOG(LOG_DEBUG,
      "'%s' found value that matches.\n",
      __FUNCTION__);
  return SYSERR;
}

/**
 * Process a query to delete content.
 *
 * @return SYSERR if the TCP connection should be closed, otherwise OK
 */
static int csHandleRequestDelete(ClientHandle sock,
				 const CS_HEADER * req) {
  int ret;
  const RequestDelete * rd;
  Datastore_Value * value;
  HashCode512 query;
  unsigned int type;
  EncName enc;

  if (ntohs(req->size) < sizeof(RequestDelete)) {
    BREAK();
    return SYSERR;
  }
  rd = (const RequestDelete*) req;
  value = MALLOC(sizeof(Datastore_Value) +
		 ntohs(req->size) - sizeof(RequestDelete));
  value->size = ntohl(sizeof(Datastore_Value) +
		      ntohs(req->size) - sizeof(RequestDelete));
  type = getTypeOfBlock(ntohs(rd->header.size) - sizeof(RequestDelete),
			(const DBlock*)&rd[1]);
  value->type = htonl(type);
  memcpy(&value[1],
	 &rd[1],
	 ntohs(req->size) - sizeof(RequestDelete));
  if (OK != getQueryFor(ntohs(rd->header.size) - sizeof(RequestDelete),
			(const DBlock*)&rd[1],
			&query)) {
    FREE(value);
    BREAK();
    return SYSERR;
  }
  IFLOG(LOG_DEBUG,
	hash2enc(&query,
		 &enc));
  LOG(LOG_DEBUG,
      "FS received REQUEST DELETE (query: %s, type: %u)\n",
      &enc,
      type);

  MUTEX_LOCK(&lock);
  if (SYSERR == datastore->get(&query,
			       type,
			       &completeValue,
			       value)) /* aborted == found! */
    ret = datastore->del(&query,
			 value);
  else /* not found */
    ret = SYSERR;
  MUTEX_UNLOCK(&lock);
  FREE(value);
  LOG(LOG_DEBUG,
      "Sending confirmation (%s) of delete request to client\n",
      ret != SYSERR ? "success" : "failure");
  return coreAPI->sendValueToClient(sock,
				    ret);
}

/**
 * Process a client request unindex content.
 */
static int csHandleRequestUnindex(ClientHandle sock,
				  const CS_HEADER * req) {
  int ret;
  RequestUnindex * ru;

  if (ntohs(req->size) != sizeof(RequestUnindex)) {
    BREAK();
    return SYSERR;
  }
  ru = (RequestUnindex*) req;
  LOG(LOG_DEBUG,
      "FS received REQUEST UNINDEX\n");
  ret = ONDEMAND_unindex(datastore,
			 ntohl(ru->blocksize),
			 &ru->fileId);
  return coreAPI->sendValueToClient(sock,
				    ret);
}

/**
 * Process a client request to test if certain
 * data is indexed.
 */
static int csHandleRequestTestIndexed(ClientHandle sock,
				      const CS_HEADER * req) {
  int ret;
  RequestTestindex * ru;

  if (ntohs(req->size) != sizeof(RequestTestindex)) {
    BREAK();
    return SYSERR;
  }
  ru = (RequestTestindex*) req;
  LOG(LOG_DEBUG,
      "FS received REQUEST TESTINDEXED\n");
  ret = ONDEMAND_testindexed(datastore,
			     &ru->fileId);
  return coreAPI->sendValueToClient(sock,
				    ret);
}

/**
 * Process a client request to obtain the current
 * averge priority.
 */
static int csHandleRequestGetAvgPriority(ClientHandle sock,
					 const CS_HEADER * req) {
  LOG(LOG_DEBUG,
      "FS received REQUEST GETAVGPRIORITY\n");
  return coreAPI->sendValueToClient(sock,
				    gap->getAvgPriority());
}

/**
 * Closure for the gapGetConverter method.
 */
typedef struct {
  DataProcessor resultCallback;
  void * resCallbackClosure;
  unsigned int keyCount;
  const HashCode512 * keys;
  int count;
} GGC;

/**
 * Callback that converts the Datastore_Value values
 * from the datastore to Blockstore values for the
 * gap routing protocol.
 */
static int gapGetConverter(const HashCode512 * key,
			   const Datastore_Value * invalue,
			   void * cls) {
  GGC * ggc = (GGC*) cls;
  GapWrapper * gw;
  int ret;
  unsigned int size;
  cron_t et;
  cron_t now;
  const Datastore_Value * value;
  Datastore_Value * xvalue;

  if (ntohl(invalue->type) == ONDEMAND_BLOCK) {
    if (OK != ONDEMAND_getIndexed(datastore,
				  invalue,
				  key,
				  &xvalue))
      return SYSERR;
    value = xvalue;
  } else {
    xvalue = NULL;
    value = invalue;
  }
		
  ret = isDatumApplicable(ntohl(value->type),
			  ntohl(value->size) - sizeof(Datastore_Value),
			  (const DBlock*) &value[1],
			  ggc->keyCount,
			  ggc->keys);
  if (ret == SYSERR) {
    FREENONNULL(xvalue);
    return SYSERR; /* no query will ever match */
  }
  if (ret == NO) {
    FREENONNULL(xvalue);
    return OK; /* Additional filtering based on type;
		  i.e., namespace request and namespace
		  in reply does not match namespace in query */
  }
  size = sizeof(GapWrapper) +
    ntohl(value->size) -
    sizeof(Datastore_Value);

  if (ntohl(value->anonymityLevel) != 0) {
    /* consider traffic volume before migrating;
       ok, so this is not 100% clean since it kind-of
       belongs into the gap code (since it is concerned
       with anonymity and GAP messages).  So we should
       probably move it below the callback by passing
       the anonymity level along.  But that would
       require changing the DataProcessor somewhat,
       which would also be ugly.  So to keep things
       simple, we do the anonymity-level check for
       outgoing content right here. */
    if (traffic != NULL) {
      unsigned int level;
      unsigned int count;
      unsigned int peers;
      unsigned int sizes;
      unsigned int timevect;

      level = ntohl(value->anonymityLevel) - 1;
      if (OK != traffic->get(5 * cronSECONDS / TRAFFIC_TIME_UNIT, /* TTL_DECREMENT/TTU */
			     GAP_p2p_PROTO_RESULT,
			     TC_RECEIVED,
			     &count,
			     &peers,
			     &sizes,
			     &timevect)) {
	LOG(LOG_WARNING,
	    _("Failed to get traffic stats.\n"));
	FREENONNULL(xvalue);
	return OK;
      }
      if (level > 1000) {
	if (peers < level / 1000) {
	  LOG(LOG_DEBUG,
	      "Not enough cover traffic to satisfy anonymity requirements. Result dropped.\n");
	  FREENONNULL(xvalue);
	  return OK;
	}
	if (count < level % 1000) {
	  LOG(LOG_DEBUG,
	      "Not enough cover traffic to satisfy anonymity requirements. Result dropped.\n");
	  FREENONNULL(xvalue);
	  return OK;
	}
      } else {
	if (count < level) {
	  LOG(LOG_DEBUG,
	      "Not enough cover traffic to satisfy anonymity requirements. Result dropped.\n");
	  FREENONNULL(xvalue);
	  return OK;
	}
      }
    } else {
      /* traffic required by module not loaded;
	 refuse to hand out data that requires
	 anonymity! */
      FREENONNULL(xvalue);
      return OK;
    }
  }

  gw = MALLOC(size);
  gw->dc.size = htonl(size);
  et = ntohll(value->expirationTime);
  /* expiration time normalization and randomization */
  cronTime(&now);
  if (et > now) {
    et -= now;
    et = et % MAX_MIGRATION_EXP;
    if (et > 0)
      et = randomi(et);
    et = et + now;
  }
  gw->timeout = htonll(et);
  memcpy(&gw[1],
	 &value[1],
	 size - sizeof(GapWrapper));

  if (ggc->resultCallback != NULL)
    ret = ggc->resultCallback(key,
			      &gw->dc,
			      ggc->resCallbackClosure);
  else
    ret = OK;
  ggc->count++;
  FREE(gw);
  FREENONNULL(xvalue);
  return ret;
}

/**
 * Lookup an item in the datastore.
 *
 * @param key the value to lookup
 * @param resultCallback function to call for each result that was found
 * @param resCallbackClosure extra argument to resultCallback
 * @return number of results, SYSERR on error
 */
static int gapGet(void * closure,
		  unsigned int type,
		  unsigned int prio,
		  unsigned int keyCount,
		  const HashCode512 * keys,
		  DataProcessor resultCallback,
		  void * resCallbackClosure) {
  int ret;
  GGC myClosure;
  EncName enc;

  IFLOG(LOG_DEBUG,
	hash2enc(&keys[0],
		 &enc));
  LOG(LOG_DEBUG,
      "GAP requests content for %s of type %u\n",
      &enc,
      type);
  myClosure.count = 0;
  myClosure.keyCount = keyCount;
  myClosure.keys = keys;
  myClosure.resultCallback = resultCallback;
  myClosure.resCallbackClosure = resCallbackClosure;
  ret = OK;
  if (type == D_BLOCK) {
    ret = datastore->get(&keys[0],
			 ONDEMAND_BLOCK,
			 &gapGetConverter,
			 &myClosure);
  }
  if (ret != SYSERR)
    ret = datastore->get(&keys[0],
			 type,
			 &gapGetConverter,
			 &myClosure);
  if (ret != SYSERR)
    ret = myClosure.count; /* return number of actual
			      results (unfiltered) that
			      were found */
  return ret;
}

/**
 * Remove an item from the datastore.
 *
 * @param key the key of the item
 * @param value the value to remove, NULL for all values of the key
 * @return OK if the value could be removed, SYSERR if not (i.e. not present)
 */
static int gapDel(void * closure,
		  const HashCode512 * key,
		  const DataContainer * value) {
  BREAK(); /* gap does not use 'del'! */
  return SYSERR;
}

/**
 * Iterate over all keys in the local datastore
 *
 * @param processor function to call on each item
 * @param cls argument to processor
 * @return number of results, SYSERR on error
 */
static int gapIterate(void * closure,		
		      DataProcessor processor,
		      void * cls) {
  BREAK(); /* gap does not use 'iterate' */
  return SYSERR;
}


/**
 * Callback that converts the Datastore_Value values
 * from the datastore to Blockstore values for the
 * DHT routing protocol.
 */
static int dhtGetConverter(const HashCode512 * key,
			   const Datastore_Value * invalue,
			   void * cls) {
  GGC * ggc = (GGC*) cls;
  GapWrapper * gw;
  int ret;
  unsigned int size;
  cron_t et;
  cron_t now;
  const Datastore_Value * value;
  Datastore_Value * xvalue;

  if (ntohl(invalue->type) == ONDEMAND_BLOCK) {
    if (OK != ONDEMAND_getIndexed(datastore,
				  invalue,
				  key,
				  &xvalue))
      return SYSERR;
    value = xvalue;
  } else {
    xvalue = NULL;
    value = invalue;
  }

  ret = isDatumApplicable(ntohl(value->type),
			  ntohl(value->size) - sizeof(Datastore_Value),
			  (const DBlock*) &value[1],
			  ggc->keyCount,
			  ggc->keys);
  if (ret == SYSERR) {
    FREENONNULL(xvalue);
    return SYSERR; /* no query will ever match */
  }
  if (ret == NO) {
    FREENONNULL(xvalue);
    return OK; /* Additional filtering based on type;
		  i.e., namespace request and namespace
		  in reply does not match namespace in query */
  }
  size = sizeof(GapWrapper) +
    ntohl(value->size) -
    sizeof(Datastore_Value);

  if (ntohl(value->anonymityLevel) != 0) {
    FREENONNULL(xvalue);
    return OK; /* do not allow anonymous content to leak through DHT */
  }

  gw = MALLOC(size);
  gw->dc.size = htonl(size);
  et = ntohll(value->expirationTime);
  /* expiration time normalization and randomization */
  cronTime(&now);
  if (et > now) {
    et -= now;
    et = et % MAX_MIGRATION_EXP;
    if (et > 0)
      et = randomi(et);
    et = et + now;
  }
  gw->timeout = htonll(et);
  memcpy(&gw[1],
	 &value[1],
	 size - sizeof(GapWrapper));

  if (ggc->resultCallback != NULL)
    ret = ggc->resultCallback(key,
			      &gw->dc,
			      ggc->resCallbackClosure);
  else
    ret = OK;
  FREE(gw);
  FREENONNULL(xvalue);
  return ret;
}

/**
 * Lookup an item in the datastore.
 *
 * @param key the value to lookup
 * @param resultCallback function to call for each result that was found
 * @param resCallbackClosure extra argument to resultCallback
 * @return number of results, SYSERR on error
 */
static int dhtGet(void * closure,
		  unsigned int type,
		  unsigned int prio,
		  unsigned int keyCount,
		  const HashCode512 * keys,
		  DataProcessor resultCallback,
		  void * resCallbackClosure) {
  int ret;
  GGC myClosure;
  EncName enc;

  IFLOG(LOG_DEBUG,
	hash2enc(&keys[0],
		 &enc));
  LOG(LOG_DEBUG,
      "DHT requests content for %s of type %u\n",
      &enc,
      type);
  myClosure.keyCount = keyCount;
  myClosure.keys = keys;
  myClosure.resultCallback = resultCallback;
  myClosure.resCallbackClosure = resCallbackClosure;
  ret = datastore->get(&keys[0],
		       type,
		       &dhtGetConverter,
		       &myClosure);
  if (ret != SYSERR)
    ret = myClosure.count; /* return number of actual
			      results (unfiltered) that
			      were found */
  return ret;
}

static int uniqueReplyIdentifier(const DataContainer * content,
				 unsigned int type,
				 const HashCode512 * primaryKey) {
  HashCode512 q;
  unsigned int t;
  const GapWrapper * gw;
  unsigned int size;

  size = ntohl(content->size);
  if (size < sizeof(GapWrapper)) {
    BREAK();
    return NO;
  }
  gw = (const GapWrapper*) content;
  if ( (OK == getQueryFor(size - sizeof(GapWrapper),
			  (const DBlock*) &gw[1],
			  &q)) &&
       (equalsHashCode512(&q,
			  primaryKey)) &&
       ( (type == ANY_BLOCK) ||
	 (type == (t = getTypeOfBlock(size - sizeof(GapWrapper),
				      (const DBlock*)&gw[1]) ) ) ) ) {
    switch(type) {
    case D_BLOCK:
      return YES;
    default:
      return NO;
    }
  } else
    return NO;
}


/**
 * Initialize the FS module. This method name must match
 * the library name (libgnunet_XXX => initialize_XXX).
 *
 * @return SYSERR on errors
 */
int initialize_module_fs(CoreAPIForApplication * capi) {
  static Blockstore dsGap;
  static Blockstore dsDht;

  hash("GNUNET_FS",
       strlen("GNUNET_FS"),
       &dht_table);
  if (getConfigurationInt("FS",
			  "QUOTA") <= 0) {
    LOG(LOG_ERROR,
	_("You must specify a postive number for '%s' in the configuration in section '%s'.\n"),
	"QUOTA", "FS");
    return SYSERR;
  }
  datastore = capi->requestService("datastore");
  if (datastore == NULL) {
    BREAK();
    return SYSERR;
  }
  traffic = capi->requestService("traffic");
  gap = capi->requestService("gap");
  if (gap == NULL) {
    BREAK();
    capi->releaseService(datastore);
    return SYSERR;
  }
  /* dht = capi->requestService("dht"); */
  dht = NULL;

  coreAPI = capi;
  MUTEX_CREATE(&lock);
  dsGap.closure = NULL;
  dsGap.get = &gapGet;
  dsGap.put = &gapPut;
  dsGap.del = &gapDel;
  dsGap.iterate = &gapIterate;
  initQueryManager(capi);
  gap->init(&dsGap, &uniqueReplyIdentifier);

  if (dht != NULL) {
    dsDht.closure = NULL;
    dsDht.get = &dhtGet;
    dsDht.put = &gapPut; /* exactly the same method for gap/dht*/
    dsDht.del = &gapDel; /* exactly the same method for gap/dht*/
    dsDht.iterate = &gapIterate;  /* exactly the same method for gap/dht*/
    dht->join(&dsDht, &dht_table);
  }

  LOG(LOG_DEBUG,
      _("'%s' registering client handlers %d %d %d %d %d %d %d %d %d\n"),
      "fs",
      AFS_CS_PROTO_QUERY_START,
      AFS_CS_PROTO_QUERY_STOP,
      AFS_CS_PROTO_INSERT,
      AFS_CS_PROTO_INDEX,
      AFS_CS_PROTO_DELETE,
      AFS_CS_PROTO_UNINDEX,
      AFS_CS_PROTO_TESTINDEX,
      AFS_CS_PROTO_GET_AVG_PRIORITY,
      AFS_CS_PROTO_INIT_INDEX);

  GNUNET_ASSERT(SYSERR != capi->registerClientHandler(AFS_CS_PROTO_QUERY_START,
						      &csHandleRequestQueryStart));
  GNUNET_ASSERT(SYSERR != capi->registerClientHandler(AFS_CS_PROTO_QUERY_STOP,
						      &csHandleRequestQueryStop));
  GNUNET_ASSERT(SYSERR != capi->registerClientHandler(AFS_CS_PROTO_INSERT,
						      &csHandleRequestInsert));
  GNUNET_ASSERT(SYSERR != capi->registerClientHandler(AFS_CS_PROTO_INDEX,
						      &csHandleRequestIndex));
  GNUNET_ASSERT(SYSERR != capi->registerClientHandler(AFS_CS_PROTO_INIT_INDEX,
						      &csHandleRequestInitIndex));
  GNUNET_ASSERT(SYSERR != capi->registerClientHandler(AFS_CS_PROTO_DELETE,
						      &csHandleRequestDelete));
  GNUNET_ASSERT(SYSERR != capi->registerClientHandler(AFS_CS_PROTO_UNINDEX,
						      &csHandleRequestUnindex));
  GNUNET_ASSERT(SYSERR != capi->registerClientHandler(AFS_CS_PROTO_TESTINDEX,
						      &csHandleRequestTestIndexed));
  GNUNET_ASSERT(SYSERR != capi->registerClientHandler(AFS_CS_PROTO_GET_AVG_PRIORITY,
						      &csHandleRequestGetAvgPriority));
  initMigration(capi, datastore, gap, dht);
  return OK;
}

void done_module_fs() {
  doneMigration();
  if (dht != NULL) {
    LOG(LOG_INFO,
	"Leaving DHT (this may take a while).");
    dht->leave(&dht_table);
    LOG(LOG_INFO,
	"Leaving DHT complete.");

  }
  GNUNET_ASSERT(SYSERR != coreAPI->unregisterClientHandler(AFS_CS_PROTO_QUERY_START,
							   &csHandleRequestQueryStart));
  GNUNET_ASSERT(SYSERR != coreAPI->unregisterClientHandler(AFS_CS_PROTO_QUERY_STOP,
							   &csHandleRequestQueryStop));
  GNUNET_ASSERT(SYSERR != coreAPI->unregisterClientHandler(AFS_CS_PROTO_INSERT,
							   &csHandleRequestInsert));
  GNUNET_ASSERT(SYSERR != coreAPI->unregisterClientHandler(AFS_CS_PROTO_INDEX,
							   &csHandleRequestIndex));
  GNUNET_ASSERT(SYSERR != coreAPI->unregisterClientHandler(AFS_CS_PROTO_INIT_INDEX,
							   &csHandleRequestInitIndex));
  GNUNET_ASSERT(SYSERR != coreAPI->unregisterClientHandler(AFS_CS_PROTO_DELETE,
							   &csHandleRequestDelete));
  GNUNET_ASSERT(SYSERR != coreAPI->unregisterClientHandler(AFS_CS_PROTO_UNINDEX,
							   &csHandleRequestUnindex));
  GNUNET_ASSERT(SYSERR != coreAPI->unregisterClientHandler(AFS_CS_PROTO_TESTINDEX,
							   &csHandleRequestTestIndexed));
  GNUNET_ASSERT(SYSERR != coreAPI->unregisterClientHandler(AFS_CS_PROTO_GET_AVG_PRIORITY,
							   &csHandleRequestGetAvgPriority));
  doneQueryManager();
  coreAPI->releaseService(datastore);
  datastore = NULL;
  coreAPI->releaseService(gap);
  gap = NULL;
  if (dht != NULL) {
    coreAPI->releaseService(dht);
    dht = NULL;
  }
  if (traffic != NULL) {
    coreAPI->releaseService(traffic);
    traffic = NULL;
  }
  coreAPI = NULL;
  MUTEX_DESTROY(&lock);
}

/**
 * Update FS module.
 */
void update_module_fs(UpdateAPI * uapi) {
  uapi->updateModule("datastore");
  uapi->updateModule("dht");
  uapi->updateModule("gap");
  uapi->updateModule("traffic");
}

/* end of fs.c */
