/*
     This file is part of GNUnet.
     (C) 2010,2011 Christian Grothoff (and other contributing authors)

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
 * @file ats/perf_ats_mlp
 * @brief performance test for the MLP solver
 * @author Christian Grothoff
 * @author Matthias Wachs

 */

/*
     This file is part of GNUnet.
     (C) 2010,2011 Christian Grothoff (and other contributing authors)

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
 * @file ats/test_ats_mlp.c
 * @brief basic test for the MLP solver
 * @author Christian Grothoff
 * @author Matthias Wachs

 */
#include "platform.h"
#include "gnunet_util_lib.h"
#include "gnunet_statistics_service.h"
#include "gnunet_ats_service.h"
#include "gnunet-service-ats_addresses_mlp.h"
#include "test_ats_api_common.h"

#define PEERS_START 100
#define PEERS_END 	100

#define ADDRESSES 10

int count_p;
int count_a;


struct PerfPeer
{
	struct GNUNET_PeerIdentity id;

	struct ATS_Address *head;
	struct ATS_Address *tail;
};

static int ret;
static int numeric;

static int N_peers_start;
static int N_peers_end;
static int N_address;

/**
 * Statistics handle
 */
struct GNUNET_STATISTICS_Handle * stats;

/**
 * MLP solver handle
 */
struct GAS_MLP_Handle *mlp;

/**
 * Hashmap containing addresses
 */
struct GNUNET_CONTAINER_MultiHashMap * addresses;

struct PerfPeer *peers;

static void
end_now (int res)
{
  if (NULL != stats)
  {
  	  GNUNET_STATISTICS_destroy(stats, GNUNET_NO);
  	  stats = NULL;
  }
  /*
  if (NULL != addresses)
  {
  		GNUNET_CONTAINER_multihashmap_iterate (addresses, &addr_it, NULL);
  		GNUNET_CONTAINER_multihashmap_destroy (addresses);
  		addresses = NULL ;
  }*/
  if (NULL != peers)
	{
		GNUNET_free (peers);
	}
  if (NULL != mlp)
  {
  		GAS_mlp_done (mlp);
  		mlp = NULL;
  }

	ret = res;
}


static void
bandwidth_changed_cb (void *cls, struct ATS_Address *address)
{

}

static void
perf_create_peer (int cp)
{
	GNUNET_CRYPTO_hash_create_random(GNUNET_CRYPTO_QUALITY_WEAK, &peers[cp].id.hashPubKey);
	GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Creating peer #%u: %s \n", cp, GNUNET_i2s (&peers[cp].id));
}

static struct ATS_Address *
perf_create_address (int cp, int ca)
{
	struct ATS_Address *a;
	a = create_address (&peers[cp].id, "Test 1", "test 1", strlen ("test 1") + 1, 0);
	GNUNET_CONTAINER_DLL_insert (peers[cp].head, peers[cp].tail, a);
	GNUNET_CONTAINER_multihashmap_put (addresses, &peers[cp].id.hashPubKey, a, GNUNET_CONTAINER_MULTIHASHMAPOPTION_MULTIPLE);
	return a;
}





static void
check (void *cls, char *const *args, const char *cfgfile,
       const struct GNUNET_CONFIGURATION_Handle *cfg)
{
  int quotas[GNUNET_ATS_NetworkTypeCount] = GNUNET_ATS_NetworkType;
  unsigned long long  quotas_in[GNUNET_ATS_NetworkTypeCount];
  unsigned long long  quotas_out[GNUNET_ATS_NetworkTypeCount];
	int cp;
	int ca;
	struct ATS_Address * cur_addr;

  stats = GNUNET_STATISTICS_create("ats", cfg);
  if (NULL == stats)
  {
  	GNUNET_break (0);
    end_now (1);
    return;
  }

  /* Load quotas */
  if (GNUNET_ATS_NetworkTypeCount != load_quotas (cfg, quotas_out, quotas_in,
  			GNUNET_ATS_NetworkTypeCount))
  {
    	GNUNET_break (0);
      end_now (1);
      return;
  }

  GNUNET_assert (N_peers_end >= N_peers_start);
  GNUNET_assert (N_address >= 0);

  if ((0 == N_peers_start) && (0 == N_peers_end))
  {
  		N_peers_start = PEERS_START;
  		N_peers_end = PEERS_END;
  }
  if (0 == N_address)
  		N_address = ADDRESSES;

  fprintf (stderr, "Solving problem for %u..%u peers with %u addresses\n",
  		N_peers_start, N_peers_end, N_address);

  count_p = N_peers_end;
  count_a = N_address;
  peers = GNUNET_malloc ((count_p) * sizeof (struct PerfPeer));
  /* Setup address hashmap */
  addresses = GNUNET_CONTAINER_multihashmap_create (N_address, GNUNET_NO);

  /* Init MLP solver */
  mlp  = GAS_mlp_init (cfg, stats, quotas, quotas_out, quotas_in,
  		GNUNET_ATS_NetworkTypeCount, &bandwidth_changed_cb, NULL);
  if (NULL == mlp)
  {
    	GNUNET_break (0);
      end_now (1);
      return;
  }
  mlp->mlp_auto_solve = GNUNET_NO;

	for (cp = 0; cp < count_p; cp++)
			perf_create_peer (cp);

	if (GNUNET_YES == numeric)
		fprintf (stderr, "#peers;#addresses per peer;LP/MIP state;presolv;exec build in ms;exec LP in ms; exec MIP in ms;#cols;#rows;#nonzero elements\n");

	for (cp = 0; cp < count_p; cp++)
	{
			for (ca = 0; ca < count_a; ca++)
			{
					cur_addr = perf_create_address(cp, ca);
					/* add address */
					GAS_mlp_address_add (mlp, addresses, cur_addr);
					GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Adding address for peer %u address %u: \n", cp, ca);
			}
			GAS_mlp_get_preferred_address( mlp, addresses, &peers[cp].id);
			/* solve */
			if (cp + 1 >= N_peers_start)
			{

				GAS_mlp_solve_problem (mlp, addresses);
				if (GNUNET_NO == numeric)
					fprintf (stderr, "%u peers each %u addresses;  LP/MIP state [%s/%s] presolv [%s/%s], (build/LP/MIP in ms): %04llu %04llu %04llu; size (cols x rows, nonzero elements): [%u x %u] = %u\n",
							cp + 1, ca,
							(GNUNET_OK == mlp->ps.lp_res) ? "OK" : "FAIL",
							(GNUNET_OK == mlp->ps.mip_res) ? "OK" : "FAIL",
							(GLP_YES == mlp->ps.lp_presolv) ? "YES" : "NO",
							(GNUNET_OK == mlp->ps.mip_presolv) ? "YES" : "NO",
							(unsigned long long) mlp->ps.build_dur.rel_value,
							(unsigned long long) mlp->ps.lp_dur.rel_value,
							(unsigned long long) mlp->ps.mip_dur.rel_value,
							mlp->ps.p_cols, mlp->ps.p_rows, mlp->ps.p_elements);
				else
					fprintf (stderr, "%u;%u;%s;%s;%s;%s;%04llu;%04llu;%04llu;%u;%u;%u\n",
							cp + 1, ca,
							(GNUNET_OK == mlp->ps.lp_res) ? "OK" : "FAIL",
							(GNUNET_OK == mlp->ps.mip_res) ? "OK" : "FAIL",
							(GLP_YES == mlp->ps.lp_presolv) ? "YES" : "NO",
							(GNUNET_OK == mlp->ps.mip_presolv) ? "YES" : "NO",
							(unsigned long long) mlp->ps.build_dur.rel_value,
							(unsigned long long) mlp->ps.lp_dur.rel_value,
							(unsigned long long) mlp->ps.mip_dur.rel_value,
							mlp->ps.p_cols, mlp->ps.p_rows, mlp->ps.p_elements);
			}

	}


	struct ATS_Address *cur;
	struct ATS_Address *next;
	for (cp = 0; cp < count_p; cp++)
	{
			for (cur = peers[cp].head; cur != NULL; cur = next)
			{
					GAS_mlp_address_delete (mlp, addresses, cur, GNUNET_NO);
					next = cur->next;
					GNUNET_CONTAINER_DLL_remove (peers[cp].head, peers[cp].tail, cur);
					GNUNET_free (cur);
			}

	}
	GNUNET_free (peers);

}


int
main (int argc, char *argv[])
{

  static char *const argv2[] = { "perf_ats_mlp",
    "-c",
    "test_ats_mlp.conf",
    "-L", "WARNING",
    NULL
  };

  N_peers_start = 0;
  N_peers_end = 0;
  N_address = 0;
  int c;
  for (c = 0; c < argc; c++)
  {
  		if ((0 == strcmp (argv[c], "-q")) && (c < argc))
  		{
  				if (0 != atoi(argv[c+1]))
  				{
  						N_peers_start = atoi(argv[c+1]);
  				}
  		}
  		if ((0 == strcmp (argv[c], "-w")) && (c < argc))
  		{
  				if (0 != atoi(argv[c+1]))
  				{
  						N_peers_end = atoi(argv[c+1]);
  				}
  		}
  		if ((0 == strcmp (argv[c], "-a")) && (c < argc))
  		{
  				if (0 != atoi(argv[c+1]))
  				{
  						N_address = atoi(argv[c+1]);
  				}
  		}
  		if ((0 == strcmp (argv[c], "-n")))
  		{
  				numeric = GNUNET_YES;
  		}
  }

  static const struct GNUNET_GETOPT_CommandLineOption options[] = {
    GNUNET_GETOPT_OPTION_END
  };


  GNUNET_PROGRAM_run ((sizeof (argv2) / sizeof (char *)) - 1, argv2,
                      "perf_ats_mlp", "nohelp", options,
                      &check, NULL);


  return ret;
}

/* end of file perf_ats_mlp.c */
