/*
     This file is part of GNUnet.
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
 * @file applications/testing/remotetest.c
 * @brief testcase for testing library
 * @author Not Christian Grothoff
 */

#include "platform.h"
#include "gnunet_protocols.h"
#include "gnunet_testing_lib.h"

/**
 * Testcase
 * @return 0: ok, -1: error
 */
int
main (int argc, const char **argv)
{  
  static char *configFile = "/tmp/fake.conf";
  static char *path;
  static char *fullpath;
  int res;
  struct GNUNET_GC_Configuration *cfg;
  struct GNUNET_GE_Context *ectx;
  struct GNUNET_GC_Configuration *hostConfig;
  

static struct GNUNET_CommandLineOption gnunetstatsOptions[] = {
  GNUNET_COMMAND_LINE_OPTION_CFG_FILE (&configFile),   /* -c */
  GNUNET_COMMAND_LINE_OPTION_HELP (gettext_noop ("Print statistics about GNUnet operations.")), /* -h */
  GNUNET_COMMAND_LINE_OPTION_HOSTNAME,  /* -H */
  GNUNET_COMMAND_LINE_OPTION_LOGGING,   /* -L */
  GNUNET_COMMAND_LINE_OPTION_VERSION (PACKAGE_VERSION), /* -v */
  GNUNET_COMMAND_LINE_OPTION_END,
};

  

  res = GNUNET_init (argc,
                     argv,
                     "testingtest",
                     &configFile, gnunetstatsOptions, &ectx, &cfg);
  if (res == -1)
  {
    GNUNET_fini (ectx, cfg);
    return -1;
  }

  GNUNET_GC_get_configuration_value_filename(cfg,"","CONFIG","",&path);
  
  fullpath = GNUNET_malloc(strlen(path) + 64);
  strcpy(fullpath,path);
  strcat(fullpath,configFile);
    
  
    
  if (GNUNET_OK != GNUNET_TESTING_remote_read_config (fullpath,&hostConfig))
  {
   	printf("Problem with main host configuration file...\n");
   	exit(1);	
  }
    
  if (GNUNET_TESTING_remote_check_config(&hostConfig) != GNUNET_OK)
  {
   	printf("Problem with main host configuration file...\n");
   	exit(1);	
  }
                      
  //GNUNET_TESTING_remote_start_daemon(one,two,six,three,four,five);
	
  GNUNET_TESTING_remote_start_daemons(&hostConfig);
	
  return GNUNET_OK;
}

/* end of testingtest.c */
