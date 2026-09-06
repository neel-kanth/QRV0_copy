/*
 * Utilities for DLTDOA base configuration.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "util_dltdoa_config.h"

/*
* R0: Anchor 1 (initiator), Anchor 2,3,4,5,6 (Responder)
* R1: Anchor 2 (initiator), Anchor 1,3,4,5,6 (Responder)
* R2: Anchor 3 (initiator), Anchor 1,2,4,5,6 (Responder)
* R3: Anchor 4 (initiator), Anchor 1,2,3,5,6 (Responder)
* R7: Anchor 5 (initiator), Anchor 1,2,3,4,6 (Responder)
* R8: Anchor 6 (initiator), Anchor 1,2,3,4,5 (Responder)
* R9: Anchor 1 (initiator), Anchor 2,3,4,5,6 (Responder)
*/
const struct cherry_fira_anchor_round_config round_conf_7_clusters[6][7] = {
	{ { 0,
	    CHERRY_FIRA_ANCHOR_ROLE_INITIATOR,
	    5,
	    { MAC_ADDRESS_ANCHOR2, MAC_ADDRESS_ANCHOR3, MAC_ADDRESS_ANCHOR4,
	      MAC_ADDRESS_ANCHOR5, MAC_ADDRESS_ANCHOR6 } },
	  { 1, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 2, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 3, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 7, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 8, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 9,
	    CHERRY_FIRA_ANCHOR_ROLE_INITIATOR,
	    5,
	    { MAC_ADDRESS_ANCHOR2, MAC_ADDRESS_ANCHOR3, MAC_ADDRESS_ANCHOR4,
	      MAC_ADDRESS_ANCHOR5, MAC_ADDRESS_ANCHOR6 } } },
	{ { 0, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 1,
	    CHERRY_FIRA_ANCHOR_ROLE_INITIATOR,
	    5,
	    { MAC_ADDRESS_ANCHOR1, MAC_ADDRESS_ANCHOR3, MAC_ADDRESS_ANCHOR4,
	      MAC_ADDRESS_ANCHOR5, MAC_ADDRESS_ANCHOR6 } },
	  { 2, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 3, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 7, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 8, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 9, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } } },
	{ { 0, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 1, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 2,
	    CHERRY_FIRA_ANCHOR_ROLE_INITIATOR,
	    5,
	    { MAC_ADDRESS_ANCHOR1, MAC_ADDRESS_ANCHOR2, MAC_ADDRESS_ANCHOR4,
	      MAC_ADDRESS_ANCHOR5, MAC_ADDRESS_ANCHOR6 } },
	  { 3, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 7, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 8, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 9, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } } },
	{ { 0, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 1, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 2, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 3,
	    CHERRY_FIRA_ANCHOR_ROLE_INITIATOR,
	    5,
	    { MAC_ADDRESS_ANCHOR1, MAC_ADDRESS_ANCHOR2, MAC_ADDRESS_ANCHOR3,
	      MAC_ADDRESS_ANCHOR5, MAC_ADDRESS_ANCHOR6 } },
	  { 7, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 8, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 9, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } } },
	{ { 0, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 1, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 2, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 3, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 7,
	    CHERRY_FIRA_ANCHOR_ROLE_INITIATOR,
	    5,
	    { MAC_ADDRESS_ANCHOR1, MAC_ADDRESS_ANCHOR2, MAC_ADDRESS_ANCHOR3,
	      MAC_ADDRESS_ANCHOR4, MAC_ADDRESS_ANCHOR6 } },
	  { 8, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 9, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } } },
	{ { 0, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 1, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 2, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 3, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 7, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } },
	  { 8,
	    CHERRY_FIRA_ANCHOR_ROLE_INITIATOR,
	    5,
	    { MAC_ADDRESS_ANCHOR1, MAC_ADDRESS_ANCHOR2, MAC_ADDRESS_ANCHOR3,
	      MAC_ADDRESS_ANCHOR4, MAC_ADDRESS_ANCHOR5 } },
	  { 9, CHERRY_FIRA_ANCHOR_ROLE_RESPONDER, 0, { 0 } } }

};

const struct dl_tdoa_config_light dltdoa_multi_configs[6] = {
	{
		.device_mac_address = MAC_ADDRESS_ANCHOR1,
		.round_config = round_conf_7_clusters[0],
		.n_rounds = 7,
	},
	{
		.device_mac_address = MAC_ADDRESS_ANCHOR2,
		.round_config = round_conf_7_clusters[1],
		.n_rounds = 7,
	},
	{
		.device_mac_address = MAC_ADDRESS_ANCHOR3,
		.round_config = round_conf_7_clusters[2],
		.n_rounds = 7,
	},
	{
		.device_mac_address = MAC_ADDRESS_ANCHOR4,
		.round_config = round_conf_7_clusters[3],
		.n_rounds = 7,
	},
	{
		.device_mac_address = MAC_ADDRESS_ANCHOR5,
		.round_config = round_conf_7_clusters[4],
		.n_rounds = 7,
	},
	{
		.device_mac_address = MAC_ADDRESS_ANCHOR6,
		.round_config = round_conf_7_clusters[5],
		.n_rounds = 7,
	},
};
