/*
     This file is part of GNUnet

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
 * @file applications/fs/fsui/namespace_info.c
 * @brief keeping track of namespaces.  This module
 *  is supposed to keep track of other namespaces (and
 *  their advertisments), as well as of our own namespaces
 *  and the updateable content stored therein.
 * @author Christian Grothoff
 */

#include "platform.h"
#include "gnunet_fsui_lib.h"
#include "fsui.h"

#define NS_DIR "data" DIR_SEPARATOR_STR "namespaces" DIR_SEPARATOR_STR
#define NS_UPDATE_DIR "data" DIR_SEPARATOR_STR "namespace-updates" DIR_SEPARATOR_STR

static void writeNamespaceInfo(const char * namespaceName,
			       const struct ECRS_MetaData * meta,
			       int ranking) {
  unsigned int size;
  unsigned int tag;
  char * buf;
  char * fn;
  char * fnBase;

  fn = getConfigurationString(NULL, "GNUNET_HOME");
  fnBase = expandFileName(fn);
  FREE(fn);
  fn = MALLOC(strlen(fnBase) + 
	      strlen(NS_DIR) + 
	      strlen(namespaceName) + 
	      6);
  strcpy(fn, fnBase);
  strcat(fn, DIR_SEPARATOR_STR);
  strcat(fn, NS_DIR);
  mkdirp(fn);
  strcat(fn, DIR_SEPARATOR_STR);
  strcat(fn, namespaceName);
  FREE(fnBase);

  size = ECRS_sizeofMetaData(meta);
  tag = size + sizeof(int);
  buf = MALLOC(tag);
  ((int *) buf)[0] = htonl(ranking); /* ranking */
  GNUNET_ASSERT(size == ECRS_serializeMetaData(meta,
					       &buf[sizeof(int)],
					       size,
					       NO));
  writeFile(fn,
	    buf,
	    tag,
	    "660");
  FREE(fn);
  FREE(buf);
}

static int readNamespaceInfo(const char * namespaceName,
			     struct ECRS_MetaData ** meta,
			     int * ranking) {
  unsigned int size;
  unsigned int tag;
  char * buf;
  char * fn;
  char * fnBase;

  *meta = NULL;
  fn = getConfigurationString(NULL, "GNUNET_HOME");
  fnBase = expandFileName(fn);
  FREE(fn);
  fn = MALLOC(strlen(fnBase) + 
	      strlen(NS_DIR) + 
	      strlen(namespaceName) + 
	      6);
  strcpy(fn, fnBase);
  strcat(fn, DIR_SEPARATOR_STR);
  strcat(fn, NS_DIR);
  strcat(fn, DIR_SEPARATOR_STR);
  strcat(fn, namespaceName);
  FREE(fnBase);
  
  tag = getFileSize(fn);
  if (tag <= sizeof(int)) {
    FREE(fn);
    return SYSERR;
  }
  if (tag > 16 * 1024 * 1024) {
    /* too big, must be invalid! remove! */
    BREAK();
    UNLINK(fn);
    FREE(fn);
    return SYSERR;
  }
  buf = MALLOC(tag);  
  if (size != readFile(fn,
		       tag,
		       buf)) {
    FREE(buf);
    FREE(fn);
    return SYSERR;
  }
  
  size = tag - sizeof(int);  
  *ranking = ntohl(((int *) buf)[0]);  
  if (OK != ECRS_deserializeMetaData(meta,
				     &buf[sizeof(int)],
				     size)) {
    /* invalid data! remove! */
    BREAK();
    UNLINK(fn);
    FREE(buf);
    FREE(fn);
    return SYSERR;
  }
  FREE(fn);
  FREE(buf);
  return OK;
}
					    

/**
 * Create a new namespace (and publish an advertismement).
 * This function is synchronous, but may block the system
 * for a while since it must create a public-private key pair!
 *
 * @param meta meta-data about the namespace (maybe NULL)
 * @param root set to the URI of the namespace, NULL if no advertisement
 *        was created
 *
 * @return OK on success, SYSERR on error (namespace already exists)
 */
int FSUI_createNamespace(struct FSUI_Context * ctx,
			 unsigned int anonymityLevel,
			 const char * namespaceName,
			 const struct ECRS_MetaData * meta,
			 const struct ECRS_URI * advertisementURI,
			 const HashCode512 * rootEntry,
			 struct ECRS_URI ** root) {
  int ret;

  ret = ECRS_createNamespace(namespaceName,
			     meta,
			     anonymityLevel,
			     getConfigurationInt("FS", "INSERT-PRIORITY"),
			     getConfigurationInt("FS", "INSERT-EXPIRATION") * cronYEARS + cronTime(NULL),
			     advertisementURI,
			     rootEntry,
			     root);
  /* store binding of namespaceName to 'meta' in state DB! */
  if (ret == OK) {
    writeNamespaceInfo(namespaceName,
		       meta,
		       0);
  }
  return ret;
}

/**
 * Change the ranking of a (non-local) namespace.
 *
 * @param ns the name of the namespace, as obtained
 *  from ECRS_getNamespaceName
 * @param delta by how much should the rating be
 *  changed?
 * @return new rating of the namespace
 */
int FSUI_rankNamespace(struct FSUI_Context * ctx,
		       const char * ns,
		       int delta) {
  struct ECRS_MetaData * meta;
  int ret;
  int ranking;

  ret = readNamespaceInfo(ns,
			  &meta,
			  &ranking);
  if (ret == SYSERR) {
    ranking = 0;
    meta = ECRS_createMetaData();
  }
  ranking += delta;
  writeNamespaceInfo(ns,
		     meta,
		     ranking);
  ECRS_freeMetaData(meta);
  return ranking;
}

typedef struct {
  FSUI_NamespaceIterator iterator;
  void * closure;
  int ret;
} LNClosure;

static int localListNamespaceHelper(const HashCode512 * nsid,
				    const char * name,
				    void * cls) {
  LNClosure * c = cls;
  int ret;
  struct ECRS_MetaData * meta;
  int rating;

  if (c->ret == SYSERR)
    return SYSERR;
  if (OK != readNamespaceInfo(name,
			      &meta,
			      &rating)) {
    rating = 0;
    meta = ECRS_createMetaData();
  }
  ret = c->iterator(c->closure,
		    name,
		    nsid,
		    meta,
		    rating);
  ECRS_freeMetaData(meta);
  if (ret == SYSERR)
    c->ret = ret;
  else
    c->ret++;
  return OK;
}

static void listNamespaceHelper(const char * fn,
				const char * dirName,
				void * cls) {
  LNClosure * c = cls;
  int ret;
  struct ECRS_MetaData * meta;
  int rating;
  HashCode512 id;

  if (c->ret == SYSERR)
    return;
  if (OK != enc2hash(fn,
		     &id)) 
    return; /* invalid name */
  if (OK != readNamespaceInfo(fn,
			      &meta,
			      &rating))
    return; /* ignore entry */
  ret = c->iterator(c->closure,
		    fn,
		    &id,
		    meta,
		    rating);
  ECRS_freeMetaData(meta);
  if (ret == SYSERR)
    c->ret = ret;
  else
    c->ret++;
}

/**
 * List all available (local or non-local) namespaces.
 * 
 * @param local only list local namespaces (if NO, only
 *   non-local known namespaces are listed)
 */
int FSUI_listNamespaces(struct FSUI_Context * ctx,
			int local,
			FSUI_NamespaceIterator iterator,
			void * closure) {
  LNClosure cls;

  cls.iterator = iterator;
  cls.closure = closure;
  cls.ret = 0;

  if (local == YES) {
    ECRS_listNamespaces(&localListNamespaceHelper,
			&cls);
  } else {
    char * fn;
    char * fnBase;

    fn = getConfigurationString(NULL, "GNUNET_HOME");
    fnBase = expandFileName(fn);
    FREE(fn);
    fn = MALLOC(strlen(fnBase) + 
		strlen(NS_DIR) + 
		4);
    strcpy(fn, fnBase);
    strcat(fn, DIR_SEPARATOR_STR);
    strcat(fn, NS_DIR);
    
    scanDirectory(fn,
		  &listNamespaceHelper,
		  &cls);
    FREE(fn);
  }
  return cls.ret;
}

/**
 * Get the filename (or directory name) for the given
 * namespace and content identifier.
 * @param lastId maybe NULL
 */
static char * getUpdateDataFilename(const char * nsname,
				    const HashCode512 * lastId) {
  char * tmp;
  char * ret;

  ret = getConfigurationString(NULL, "GNUNET_HOME");
  tmp = expandFileName(ret);
  FREE(ret);
  ret = MALLOC(strlen(tmp) + strlen(NS_UPDATE_DIR) +
	       strlen(nsname) + sizeof(EncName) + 20);
  strcpy(ret, tmp);
  FREE(tmp);
  strcat(ret, DIR_SEPARATOR_STR);
  strcat(ret, NS_UPDATE_DIR);
  strcat(ret, nsname);
  strcat(ret, DIR_SEPARATOR_STR);
  mkdirp(ret);
  if (lastId != NULL) {
    EncName enc;

    hash2enc(lastId, &enc);
    strcat(ret, (char*) &enc);
  }  
  return ret;
}

struct UpdateData {
  cron_t updateInterval;
  cron_t lastPubTime;
  HashCode512 nextId;
  HashCode512 thisId;  
};

/**
 * Read content update information about content
 * published in the given namespace under 'lastId'.
 *
 * @param fi maybe NULL
 * @return OK if update data was found, SYSERR if not.
 */
static int readUpdateData(const char * nsname,
			  const HashCode512 * lastId,
			  HashCode512 * nextId,
			  ECRS_FileInfo * fi,
			  cron_t * updateInterval,
			  cron_t * lastPubTime) {
  char * fn;
  struct UpdateData * buf;
  char * uri;
  size_t size;
  size_t pos;

  fn = getUpdateDataFilename(nsname,
			     lastId);
  size = getFileSize(fn);
  if ( (size == 0) ||
       (size <= sizeof(struct UpdateData)) || 
       (size > 1024 * 1024 * 16) )
    return SYSERR;
  
  buf = MALLOC(size);
  if (size != readFile(fn,
		       size,	
		       buf)) {
    FREE(buf);
    return SYSERR;
  }
  FREE(fn);
  if ( ! equalsHashCode512(lastId,
			   &buf->thisId)) {
    FREE(buf);
    return SYSERR;
  }
  uri = (char*) &buf[1];
  size -= sizeof(struct UpdateData);
  pos = 0;
  while ( (pos < size) &&
	  (uri[pos] != '\0') )
    pos++;
  size -= pos;
  if (size == 0) {
    FREE(buf);
    BREAK();
    return SYSERR;
  }
  if (OK != ECRS_deserializeMetaData(&fi->meta,
				     &uri[pos],
				     size)) {
    FREE(buf);
    BREAK();
    return SYSERR;
  }
  fi->uri = ECRS_stringToUri(uri);
  if (fi->uri == NULL) {
    ECRS_freeMetaData(fi->meta);
    fi->meta = NULL;
    FREE(buf);
    BREAK();
    return SYSERR;
  }
  *updateInterval = ntohll(buf->updateInterval);
  *lastPubTime = ntohll(buf->lastPubTime);
  *nextId = buf->nextId;
  FREE(buf);
  return OK;
}

/**
 * Write content update information.
 */
static int writeUpdateData(const char * nsname,
			   const HashCode512 * thisId,
			   const HashCode512 * nextId,
			   const ECRS_FileInfo * fi,
			   const cron_t updateInterval,
			   const cron_t lastPubTime) {
  char * fn;
  char * uri;
  size_t metaSize;
  size_t size;
  struct UpdateData * buf;

  uri = ECRS_uriToString(fi->uri);
  metaSize = ECRS_sizeofMetaData(fi->meta);
  size = sizeof(struct UpdateData) + metaSize + strlen(uri) + 1;
  buf = MALLOC(size);
  buf->nextId = *nextId;
  buf->thisId = *thisId;
  buf->updateInterval = htonll(updateInterval);
  buf->lastPubTime = htonll(lastPubTime);
  memcpy(&buf[1],
	 uri,
	 strlen(uri)+1);
  FREE(uri);
  GNUNET_ASSERT(OK == 
		ECRS_serializeMetaData(fi->meta,
				       &((char*)&buf[1])[strlen(uri)+1],
				       metaSize,
				       NO));  
  fn = getUpdateDataFilename(nsname,
			     thisId);
  writeFile(fn,
	    buf,
	    size,
	    "400"); /* no editing, just deletion */
  FREE(fn);
  FREE(buf);
  return OK;
}
			  


/**
 * Add an entry into a namespace (also for publishing
 * updates).  
 *
 * @param name in which namespace to publish
 * @param updateInterval the desired frequency for updates
 * @param lastId the ID of the last value (maybe NULL)
 * @param thisId the ID of the update (maybe NULL)
 * @param nextId the ID of the next update (maybe NULL)
 * @param dst to which URI should the namespace entry refer?
 * @param md what meta-data should be associated with the
 *        entry?
 * @param uri set to the resulting URI
 */
int FSUI_addToNamespace(struct FSUI_Context * ctx,
			unsigned int anonymityLevel,
			const char * name,
			cron_t updateInterval,
			const HashCode512 * lastId,
			const HashCode512 * thisId,
			const HashCode512 * nextId,
			const struct ECRS_URI * dst,
			const struct ECRS_MetaData * md,
			struct ECRS_URI ** uri) {
  int ret;
  cron_t creationTime;
  HashCode512 nid;
  HashCode512 tid;
  cron_t now;
  cron_t lastTime;
  cron_t lastInterval;
  ECRS_FileInfo fi;
  char * old;

  /* computation of IDs of update(s).  Not as terrible as
     it looks, just enumerating all of the possible cases
     of periodic/sporadic updates and how IDs are computed. */
  creationTime = cronTime(&now);
  if (updateInterval != ECRS_SBLOCK_UPDATE_NONE) {
    if ( (lastId != NULL) &&
	 (OK == readUpdateData(name,
			       lastId,
			       &tid,
			       NULL,
			       &lastInterval,
			       &lastTime)) ) {
      if (lastInterval != updateInterval) {
	LOG(LOG_WARNING,
	    _("Publication interval for periodic publication changed."));
      }      
      /* try to compute tid and/or
	 nid based on information read from lastId */

      if (updateInterval != ECRS_SBLOCK_UPDATE_SPORADIC) {
	HashCode512 delta;

	deltaId(lastId,
		&tid,
		&delta);	

	creationTime = lastTime + updateInterval;
	while (creationTime < now - updateInterval) {
	  creationTime += updateInterval;
	  addHashCodes(&tid,
		       &delta,
		       &tid);
	}
	if (creationTime > cronTime(NULL) + 7 * cronDAYS) {
	  LOG(LOG_WARNING,
	      _("Publishing update for periodically updated content more than a week ahead of schedule.\n"));
	}
	if (thisId != NULL)
	  tid = *thisId; /* allow override! */
	addHashCodes(&tid,
		     &delta,
		     &nid);
	if (nextId != NULL)
	  nid = *nextId; /* again, allow override */
      } else {
	/* sporadic ones are unpredictable,
	   tid has been obtained from IO, pick random nid if
	   not specified */
	if (thisId != NULL) 
	  tid = *thisId; /* allow user override */	
	if (nextId == NULL) {
	  makeRandomId(&nid);
	} else {
	  nid = *nextId;
	}
      }      
    } else { /* no previous ID found or given */
      if (nextId == NULL) {
	/* no previous block found and nextId not specified;
	   pick random nid */
	makeRandomId(&nid);
      } else {
	nid = *nextId;
      }
      if (thisId != NULL) {
	tid = *thisId;
      } else {
	makeRandomId(&tid);  
      }
    }
  } else {
    if (thisId != NULL) {
      nid = tid = *thisId;
    } else {
      makeRandomId(&tid);
      nid = tid;
    }
  }
  ret = ECRS_addToNamespace(name,
			    anonymityLevel,
			    getConfigurationInt("FS", "INSERT-PRIORITY"),
			    getConfigurationInt("FS", "INSERT-EXPIRATION") * cronYEARS + cronTime(NULL),
			    creationTime,
			    updateInterval,
			    &tid,
			    &nid,
			    dst,
			    md,
			    uri);  
  if (updateInterval != ECRS_SBLOCK_UPDATE_NONE) {
    fi.uri = (struct ECRS_URI*) uri;
    fi.meta = (struct ECRS_MetaData*) md;
    writeUpdateData(name,
		    &tid,
		    &nid,
		    &fi,
		    updateInterval,
		    creationTime);
  }
  if (lastId != NULL) {
    old = getUpdateDataFilename(name,
				lastId);
    UNLINK(old);
    FREE(old);
  }
  return ret;
}

struct lNCC {
  const char * name;
  FSUI_UpdateIterator it;
  void * closure;
  int cnt;
};

void lNCHelper(const char * fil,
	       const char * dir,
	       struct lNCC * cls) {
  ECRS_FileInfo fi;
  HashCode512 lastId;
  HashCode512 nextId;
  cron_t pubFreq;
  cron_t lastTime;
  cron_t nextTime;
  cron_t now;
  
  if (cls->cnt == SYSERR)
    return;
  if (OK != enc2hash(fil,
		     &lastId))
    return;
  fi.uri = NULL;
  fi.meta = NULL;
  if (OK != readUpdateData(cls->name,
			   &lastId,
			   &nextId,
			   &fi,
			   &pubFreq,
			   &lastTime))
    return;
  cls->cnt++;
  if (pubFreq == ECRS_SBLOCK_UPDATE_SPORADIC) {
    nextTime = 0;
  } else {
    cronTime(&now);
    nextTime = lastTime;
    while (nextTime + pubFreq < now)
      nextTime += pubFreq;
  }
  if (cls->it != NULL) {
    if (OK != cls->it(cls->closure,
		      &fi,
		      &lastId,
		      &nextId,
		      pubFreq,
		      nextTime)) {
      cls->cnt = SYSERR;
    }
  }
  ECRS_freeUri(fi.uri);
  ECRS_freeMetaData(fi.meta);
}

/**
 * List all updateable content in a given namespace.
 */
int FSUI_listNamespaceContent(struct FSUI_Context * ctx,
			      const char * name,
			      FSUI_UpdateIterator iterator,
			      void * closure) {
  struct lNCC cls;
  char * dirName;

  cls.name = name;
  cls.it = iterator;
  cls.closure = closure;
  cls.cnt = 0;
  dirName = getUpdateDataFilename(name,
				  NULL);
  scanDirectory(dirName,
		(DirectoryEntryCallback)&lNCHelper,
		&cls);
  FREE(dirName);
  return cls.cnt;
}

static int mergeMeta(EXTRACTOR_KeywordType type,
		     const char * data,
		     void * cls) {
  struct ECRS_MetaData * meta = cls;
  ECRS_addToMetaData(meta,
		     type,
		     data);
  return OK;
}

/**
 * Add a namespace to the set of known namespaces.
 * For all namespace advertisements that we discover
 * FSUI should automatically call this function.
 * 
 * @param ns the namespace identifier
 */
void FSUI_addNamespaceInfo(const struct ECRS_URI * uri,
			   const struct ECRS_MetaData * meta) {
  char * name;
  int ranking;
  struct ECRS_MetaData * old;
  HashCode512 id;

  if (! ECRS_isNamespaceURI(uri)) {
    BREAK();
    return;
  }
  ECRS_getNamespaceId(uri,
		      &id);
  name = ECRS_getNamespaceName(&id);
  if (name == NULL)
    return;
  ranking = 0;
  if (OK == readNamespaceInfo(name,
			      &old,
			      &ranking)) {
    ECRS_getMetaData(meta,
		     &mergeMeta,
		     old);
    writeNamespaceInfo(name,
		       old,
		       ranking);
    ECRS_freeMetaData(old);
  } else {  
    writeNamespaceInfo(name,
		       meta,
		       ranking);
  }
  FREE(name);
}

/* end of namespace_info.c */
