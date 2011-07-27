/*
 * Copyright (C) 2010 Tiago Chedraoui Silva <tsilva@lrc.ic.unicamp.br>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Based on rc80211_minstrel.c:
 *   Copyright (C) 2008 Felix Fietkau <nbd@openwrt.org>
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer,
 *      without modification.
 *   2. Redistributions in binary form must reproduce at minimum a disclaimer
 *      similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *      redistribution must be conditioned upon including a substantially
 *      similar Disclaimer requirement for further binary redistribution.
 *   3. Neither the names of the above-listed copyright holders nor the names
 *      of any contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *   Alternatively, this software may be distributed under the terms of the
 *   GNU General Public License ("GPL") version 2 as published by the Free
 *   Software Foundation.
 *
 *   NO WARRANTY
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 *   AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 *   THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 *   OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 *   IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *   THE POSSIBILITY OF SUCH DAMAGES.
 */
#include <linux/netdevice.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/debugfs.h>
#include <linux/random.h>
#include <linux/ieee80211.h>
#include <linux/slab.h>
#include <net/mac80211.h>
#include "rate.h"
#include "rc80211_cora.h"

/* CORA Automatic Agressivness Adjustment (AAA) */
static inline int
cora_aaa (int last_mean, int curr_mean, int stdev)
{
	if (curr_mean != last_mean) {
		stdev *= 2;
		if (stdev > CORA_MAX_STDEV)
			stdev = CORA_MAX_STDEV; 
	} else {
		stdev -= 10;
		if (stdev < CORA_MIN_STDEV)
			stdev = CORA_MIN_STDEV;
	}
	return stdev;
}


/* CORA Normal random number generator. 
 * stdev parameter has to be stedv * 100 (to avoid FP operations) */
static int
rc80211_cora_normal_generator (int mean, int stdev_times100)
{
	u32 rand = 0;
	int value = 0;
	int round_fac = 0;
	unsigned int i = 0;

	/* Convolution method to generate random normal numbers
	 * Refer to "Raj Jain", "The art of computer systems performance analysis",
	 * 1 edition, page 494.
	 *
	 * In LaTeX Syntax:
	 * \mathcal{N}(\mu, \sigma) = \mu + \sigma
	 * \dfrac{(\sum_{i=1}^{n}{\mathcal{U}_{i}}) -
	 * \frac{n}{2}}{\sqrt{\frac{n}{12}}}
	 *
	 * Since we are using twelve integer uniform numbers of one byte (n in
	 * [0,255]) instead of uniform numbers from 0 to 1, this code returns a
	 * variable from a normal curve with mean of ((12 * 255)-12)/2 = 1524 and
	 * stdev of 256 (1 byte) */
	for (i = 0; i < 12; i++) {
		get_random_bytes(&rand, 4);
		value += (int)(rand & 0x000000FF);
	}

	/* Calculating a rouding factor used to avoid problems during integer
	 * rounding. The following values were obtained from simulations. I'm still
	 * not sure about this, but is working well... */
	switch (stdev_times100) {
	case 25:
		round_fac = 45;
		break;
	case 30:
		round_fac = 50;
		break;
	case 35:
		round_fac = 55;
		break;
	case 40:
		round_fac = 57;
		break;
	case 45:
		round_fac = 60;
		break;
	case 50:
		round_fac = 63;
		break;
	case 55: case 60:
		round_fac = 66;
		break;
	case 65: case 70:
		round_fac = 68;
		break;
	case 75:
		round_fac = 70;
		break;
	case 80:
		round_fac = 72;
		break;
	case 85: case 90:
		round_fac = 73;
		break;
	case 95:
		round_fac = 74;
		break;
	case 100: case 105:
		round_fac = 75;
		break;
	case 110: case 115:
		round_fac = 76;
		break;
	case 120:
		round_fac = 77;
		break;
	case 125:
		round_fac = 78;
		break;
	case 130: case 135:
		round_fac = 79;
		break;
	case 140: case 145:
		round_fac = 80;
		break;
	case 150: case 155:
		round_fac = 81;
		break;
	case 160: case 165: case 170:
		round_fac = 82;
		break;
	case 175: case 180: case 185:
		round_fac = 83;
		break;
	case 190: case 195:
		round_fac = 84;
		break;
	case 200:
		round_fac = 85;
		break;
	default:
		if (stdev_times100 < 25)
			round_fac = 40;
		else
			round_fac = 87;	
	}

	/* Converting the above normal random variable to the appropriate curve
	 * from parameter mean and stdev (considering stdev_times100 = stdev * 100)
	 * return mean + ((value - 1524) * stdev) / 256)) */
	return (int)(mean + ((value - 1524) * stdev_times100)/(round_fac * 256));
}


/* Converting mac80211 rate index into local array index */
static inline int
rix_to_ndx (struct cora_sta_info *ci, int rix)
{
	int i;
	for (i = ci->n_rates - 1; i >= 0; i--)
		if (ci->r[i].rix == rix)
			break;
	return i;
}


/* Sort rates by bitrates. Is called once by cora_rate_init */
static void
sort_bitrates (struct cora_sta_info *ci, int n_rates)
{
	int i, j;
	struct cora_rate cr_key;

	/* Insertion sort */
	for (j = 1; j < n_rates; j++) {
		cr_key = ci->r[j];
		i = j - 1;
		while ((i >= 0) && (ci->r[i].bitrate > cr_key.bitrate)) {
			ci->r[i+1] = ci->r[i];
			i--;
		}
		ci->r[i+1] = cr_key;
	}
}


/* cora_update_stats is called by cora_get_rate when the update_interval timer
 * expires. It sumarizes statistics information and use cora core algorithm to
 * select the rate to be used during next update_interval. */  
static void
cora_update_stats (struct cora_priv *cp, struct cora_sta_info *ci)
{
	u32 max_tp = 0, max_prob = 0;
	u32 usecs;
	unsigned int i, max_tp_ndx, max_prob_ndx;
	unsigned int old_stdev, old_mean;

	old_stdev = ci->cur_stdev;
	old_mean = ci->max_tp_rate_ndx;
	ci->stats_update = jiffies;

	/* For each supported rate... */
	for (i = 0; i < ci->n_rates; i++) {
		struct cora_rate *cr = &ci->r[i]; 

		/* To avoid rounding issues, probabilities scale from 0 (0%)
		 * to 1800 (100%) */
		if (cr->attempts) {
			
			usecs = cr->perfect_tx_time;
			if (!usecs)
				usecs = 1000000;

			/* Update thp and prob for last interval */
			cr->cur_prob = (cr->success * 1800) / cr->attempts;
			cr->cur_tp = cr->cur_prob * (1000000 / usecs);

			/* Update average thp and prob with EWMA */
			cr->avg_prob = ((cr->cur_prob * (100 - cp->ewma_level)) + 
					  (cr->avg_prob * cp->ewma_level)) / 100;
			cr->avg_tp = ((cr->cur_tp    * (100 - cp->ewma_level)) + 
					  (cr->avg_tp * cp->ewma_level)) / 100;

			/* Update success and attempt counters */
			cr->succ_hist += cr->success;
			cr->att_hist += cr->attempts;
		}

		/* Update success and attempt counters */
		cr->last_success = cr->success;
		cr->last_attempts = cr->attempts;
		cr->success = 0;
		cr->attempts = 0;
	}

	/* Look for the rate with highest throughput and probability */
	for (i = 0; i < ci->n_rates; i++) {
		struct cora_rate *cr = &ci->r[i];
		if (max_tp < cr->avg_tp) {
			max_tp_ndx = i;
			max_tp = cr->avg_tp;
		}
		if (max_prob < cr->avg_prob) {
			max_prob_ndx = i;
			max_prob = cr->avg_prob;
		}
	}
	ci->max_tp_rate_ndx = max_tp_ndx;
	ci->max_prob_rate_ndx = max_prob_ndx;

	/* Adjusting stdev with Cora AAA */
	ci->cur_stdev = cora_aaa ((int)old_mean, (int)ci->max_tp_rate_ndx,
			(int)old_stdev);

   	/* Get a new random rate (using a normal distribution) that will be used
	 * during the next update_interval. */
	ci->random_rate_ndx = rc80211_cora_normal_generator ((int)ci->max_tp_rate_ndx,
			(int)ci->cur_stdev);
	ci->r[ci->random_rate_ndx].times_called++;
}


/* cora_tx_status is called just after frame tx and it is used to update
 * statistics information for the used rate */
static void
cora_tx_status (void *priv, struct ieee80211_supported_band *sband, 
		struct ieee80211_sta *sta, void *priv_sta, struct sk_buff *skb)
{
	struct cora_sta_info *ci = priv_sta;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ieee80211_tx_rate *ar = info->status.rates;
	int i, ndx;
	int success;
 
 	/* Checking for a success in frame transmission */
	success = !!(info->flags & IEEE80211_TX_STAT_ACK);

	/* Updating information for each used rate */
	for (i = 0; i < IEEE80211_TX_MAX_RATES; i++) {
	
		/* A rate idx -1 means that the following rates weren't used in tx */
		if (ar[i].idx < 0)
			break;

		/* Getting the local idx for this ieee80211_tx_rate */
		ndx = rix_to_ndx (ci, ar[i].idx);
		if (ndx < 0)
			continue;

		/* Increasing attempts counter */
		ci->r[ndx].attempts += ar[i].count;

		/* If it is the last used rate and resultesd in tx success, also
		 * increse the success counter */
		if ((i != IEEE80211_TX_MAX_RATES - 1) && (ar[i + 1].idx < 0))
			ci->r[ndx].success += success;
	}
}


/* Retrieves the retry count for a specific random cora rate */
static inline unsigned int
cora_get_retry_count (struct cora_rate *cr_rnd, struct cora_rate *cr_bst,
		struct ieee80211_tx_info *info)
{
	/* At least two attempts are always performed */	
	unsigned int retry = max (2U, cr_rnd->retry_count); 

	/* If rate has lower delivery probability sample at most twice */
	if (cr_rnd->avg_prob < 180)
		retry = 2U;
	
	/* If random rate has lower bitrate than the best one sample less */
	else if (cr_rnd->bitrate < cr_bst->bitrate)
		retry = max (2U, cr_rnd->retry_count>>1);

	return retry;
}


/* cora_get_rate is called just before each frame tx and sets the appropriate
 * data rate to be used */
static void
cora_get_rate (void *priv, struct ieee80211_sta *sta, void *priv_sta,
		struct ieee80211_tx_rate_control *txrc)
{
	struct sk_buff *skb = txrc->skb;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct cora_sta_info *ci = priv_sta;
	struct cora_priv *cp = priv;
	struct ieee80211_tx_rate *ar = info->control.rates;
	bool mrr;

	/* Check for management or control packet, which should be transmitted
	 * unsing lower rate */
	if (rate_control_send_low(sta, priv_sta, txrc))
		return;

	/* Check MRR hardware support */
	mrr = cp->has_mrr && !txrc->rts && !txrc->bss_conf->use_cts_prot;

	/* Check the need of an update_stats based on update_interval.
	 * This work as a timmer. */
	if (time_after (jiffies, ci->stats_update + 
			(cp->update_interval * HZ) / 1000))
		cora_update_stats (cp, ci);

	/* Setting up tx rate information. Be careful to convert ndx indexes into
	 * ieee80211_tx_rate indexes */

	if (!mrr) {
		ar[0].idx = ci->r[ci->random_rate_ndx].rix;
		ar[0].count = cp->max_retry;
		ar[1].idx = -1;
		ar[1].count = 0;
		return;
	}
	
	/* MRR setup */
	/* Random CORA rate. For this rate the retry count depends on the random
	 * value and wil be defined by cora_get_retry_count function */
	ar[0].idx = ci->r[ci->random_rate_ndx].rix;
	ar[0].count = cora_get_retry_count (&ci->r[ci->random_rate_ndx],
			&ci->r[ci->max_tp_rate_ndx], info);

	/* Best throughput. For this rate on the retry count is the one defined in
	 * cora_rate_init function observing the max segment_size */
	ar[1].idx = ci->r[ci->max_tp_rate_ndx].rix;;
	ar[1].count = ci->r[ci->max_tp_rate_ndx].retry_count;

	/* Best probability */
	ar[2].idx = ci->r[ci->max_prob_rate_ndx].rix;
	ar[2].count = ci->r[ci->max_prob_rate_ndx].retry_count;

	/* Lowest rate */
	ar[3].idx = ci->lowest_rix;
	ar[3].count = ci->r[rix_to_ndx (ci, ci->lowest_rix)].retry_count;
}


/* calc_rate_durations estimates the tx time for a single data frame of 1200
 * bytes and for an ack frame of standard size for a specific rate */
static void
calc_rate_durations (struct ieee80211_local *local, struct cora_rate *cr,
		struct ieee80211_rate *rate)
{
	int erp = !!(rate->flags & IEEE80211_RATE_ERP_G);

	cr->perfect_tx_time = ieee80211_frame_duration (local, 1200, 
			rate->bitrate, erp, 1);
	cr->ack_time = ieee80211_frame_duration (local, 10, 
			rate->bitrate, erp, 1);
}


/* cora_rate_init is called after cora_alloc_sta to check and populate
 * information for supported rates */
static void
cora_rate_init (void *priv, struct ieee80211_supported_band *sband,
		struct ieee80211_sta *sta, void *priv_sta)
{
	struct cora_sta_info *ci = priv_sta;
	struct cora_priv *cp = priv;
	struct ieee80211_local *local = hw_to_local(cp->hw);
	struct ieee80211_rate *ctl_rate;
	unsigned int i, n = 0;
	unsigned int t_slot = 9;
	unsigned int sp_ack_dur;

	/* Get the lowest index rate and calculate the duration of a ack tx */
	ci->lowest_rix = rate_lowest_index (sband, sta);
	ctl_rate = &sband->bitrates[ci->lowest_rix];
	sp_ack_dur = ieee80211_frame_duration(local, 10, ctl_rate->bitrate,
			!!(ctl_rate->flags & IEEE80211_RATE_ERP_G), 1);

	/* Populating information for each supported rate */
	for (i = 0; i < sband->n_bitrates; i++) {
		struct cora_rate *cr = &ci->r[n];
		unsigned int tx_time = 0;
		unsigned int tx_time_single;
		unsigned int cw = cp->cw_min;

		/* Check rate support */
		if (!rate_supported (sta, sband->band, i))
			continue;

		n++;
		memset (cr, 0, sizeof (*cr));

		/* Set index, bitrate and tx duration */
		cr->rix = i;
		cr->bitrate = sband->bitrates[i].bitrate / 5;
		calc_rate_durations (local, cr, &sband->bitrates[i]);

		/* Calculate maximum number of retransmissions before
		 * fallback (based on maximum segment size) */
		cr->retry_count = 1;
		
		do {
			/* Add one retransmission */
			tx_time_single = cr->perfect_tx_time + cr->ack_time;

			/* Contention window */
			tx_time_single += t_slot + min (cw, cp->cw_max);
			cw = (cw << 1) | 1;

			/* Supose normal mode, with cts and with rts/cts */
			tx_time += tx_time_single;
		} while ((tx_time < cp->segment_size) &&
			 	(++cr->retry_count < cp->max_retry));
	}

	/* Sort rates based on bitrate */
	sort_bitrates (ci, n);

	/* Mark unsupported rates with rix = -1 */
	for (i = n; i < sband->n_bitrates; i++) {
		struct cora_rate *cr = &ci->r[i];
		cr->rix = -1;
	}

	ci->cur_stdev = CORA_MAX_STDEV;
	ci->n_rates = n;
	ci->stats_update = jiffies;

}


/* cora_alloc_sta is called every time a new station joins the networks */
static void *
cora_alloc_sta (void *priv, struct ieee80211_sta *sta, gfp_t gfp)
{
	struct ieee80211_supported_band *sband;
	struct cora_sta_info *ci;
	struct cora_priv *cp = priv;
	struct ieee80211_hw *hw = cp->hw;
	int max_rates = 0;
	int i;

	ci = kzalloc (sizeof (struct cora_sta_info), gfp);
	if (!ci)
		return NULL;

	/* Get the maximum number of rates based on available bands */
	for (i = 0; i < IEEE80211_NUM_BANDS; i++) {
		sband = hw->wiphy->bands[i];
		if (sband && sband->n_bitrates > max_rates)
			max_rates = sband->n_bitrates;
	}

	/* Memory allocation for rates that can be used by this station */
	ci->r = kzalloc (sizeof (struct cora_rate) * (max_rates), gfp);
	if (!ci->r) {
		kfree(ci);
		return NULL;
	}

	/* The Linux kernel maintains a global variable called jiffies, which
	 * represents the number of timer ticks since the machine started. */
   	ci->stats_update = jiffies;
	return ci;
}


/* cora_free_sta used to release memory when a station leaves the network */
static void
cora_free_sta (void *priv, struct ieee80211_sta *sta, void *priv_sta)
{
	struct cora_sta_info *ci = priv_sta;

	kfree (ci->r);
	kfree (ci);
}


/* cora_alloc called once before turning on the wireless interface */
static void *
cora_alloc (struct ieee80211_hw *hw, struct dentry *debugfsdir)
{
	struct cora_priv *cp;

	cp = kzalloc (sizeof (struct cora_priv), GFP_ATOMIC);
	if (!cp)
		return NULL;

	/* Default cora values */
	cp->cw_min = 15;
	cp->cw_max = 1023;
	cp->ewma_level = CORA_EWMA_LEVEL;
	cp->update_interval = 100;

 	/* Max time the hw can stay in one MRR segment */
	cp->segment_size = 6000;

	/* Max number of retries */
	if (hw->max_rate_tries > 0)
		cp->max_retry = hw->max_rate_tries;
	else
		cp->max_retry = 7;

	/* MRR hardware support */
	if (hw->max_rates >= 4)
		cp->has_mrr = true;

	cp->hw = hw;
	return cp;
}


/* cora_free called once before turning off the wireless interface */
static void
cora_free (void *priv)
{
	kfree (priv);
}

struct rate_control_ops mac80211_cora = {
	.name = "cora",
	.tx_status = cora_tx_status,
	.get_rate = cora_get_rate,
	.rate_init = cora_rate_init,
	.alloc = cora_alloc,
	.free = cora_free,
	.alloc_sta = cora_alloc_sta,
	.free_sta = cora_free_sta,
#ifdef CONFIG_MAC80211_DEBUGFS
	.add_sta_debugfs = cora_add_sta_debugfs,
	.remove_sta_debugfs = cora_remove_sta_debugfs,
#endif
};

int __init
rc80211_cora_init(void)
{
	printk ("LJC & TCS CORA algorithm.\n");
	return ieee80211_rate_control_register(&mac80211_cora);
}

void
rc80211_cora_exit(void)
{
	ieee80211_rate_control_unregister(&mac80211_cora);
}

