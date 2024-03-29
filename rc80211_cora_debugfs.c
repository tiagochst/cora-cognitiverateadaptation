/*
 * Copyright (C) 2010 Tiago Chedraoui Silva <tsilva@lrc.ic.unicamp.br>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Based on rc80211_minstrel_debugfs.c:
 *   Copyright (C) 2008 Felix Fietkau <nbd@openwrt.org>
 *
 * Based on minstrel.c:
 *   Copyright (C) 2005-2007 Derek Smithies <derek@indranet.co.nz>
 *   Sponsored by Indranet Technologies Ltd
 *
 * Based on sample.c:
 *   Copyright (c) 2005 John Bicket
 *   All rights reserved.
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
#include <linux/ieee80211.h>
#include <linux/slab.h>
#include <net/mac80211.h>
#include "rc80211_cora.h"

int
cora_stats_open (struct inode *inode, struct file *file)
{
	struct cora_sta_info *ci = inode->i_private;
	struct cora_debugfs_info *cs;
	unsigned int i, avg_tp, avg_prob, cur_tp, cur_prob;
	char *p;

	cs = kmalloc (sizeof (*cs) + 4096, GFP_KERNEL);
	if (!cs)
		return -ENOMEM;

	file->private_data = cs;
	p = cs->buf;

	/* Table header */
	p += sprintf(p, "\n Rate Table:\n");
	p += sprintf(p, "    | rate | avg_thp | avg_pro | cur_thp | cur_pro | "
			"succ ( atte ) | success | attempts | #used \n");

	/* Table lines */
	for (i = 0; i < ci->n_rates; i++) {
		struct cora_rate *cr = &ci->r[i];

		/* Print T for the rate with highest throughput (the mean of normal
		 * curve), P for the rate with hisgest delivery probability and print *
		 * for the rate been used now */
		*(p++) = (i == ci->random_rate_ndx)		? '*' : ' ';
		*(p++) = (i == ci->max_tp_rate_ndx) 	? 'T' : ' ';   
		*(p++) = (i == ci->max_prob_rate_ndx) 	? 'P' : ' ';   

		p += sprintf(p, " |%3u%s ", cr->bitrate / 2,
				(cr->bitrate & 1 ? ".5" : "  "));

		/* Converting the internal thp and prob format */
		avg_tp = cr->avg_tp / ((1800 << 10) / 96);
		cur_tp = cr->cur_tp / ((1800 << 10) / 96);
		avg_prob = cr->avg_prob;
		cur_prob = cr->cur_prob;

		p += sprintf (
				p, 
				"| %5u.%1u | %7u | %5u.%1u | %7u "
				"| %4u ( %4u ) | %7llu | %8llu | %5u\n",
				avg_tp / 10, avg_tp % 10,
				avg_prob / 18,
				cur_tp / 10, cur_tp % 10,
				cur_prob / 18,
				cr->last_success, cr->last_attempts,
				(unsigned long long) cr->succ_hist,
				(unsigned long long) cr->att_hist,
				cr->times_called
			);
	}

	/* Chain table  */
	p += sprintf(p, "\n MRR Chain Table:\n");
	p += sprintf(p, " type |  rate | count | success | attempts \n");

	for (i = 0; i < 4; i++) {
		struct chain_table *ct = &ci->t[i];

		p += sprintf (
			p, 
			" %s | %3u%s | %5u | %7llu | %8llu\n",
			ct->type == 0 ? "rand": ct->type == 1 ? "best" : ct->type == 2 ? "prob" : "lowr", 
			ct->bitrate / 2, (ct->bitrate & 1 ? ".5" : "  "),
			ct->count,
			(unsigned long long) ct->suc,
			(unsigned long long) ct->att
		);
	}

	/* Table footer */
	p += sprintf(p, "\n COgnitive Rate Adaptation (CORA):\n"
			"   Number of rates:      %u\n"
			"   Recovery Mode:        %s\n"
			"   Interval based on:    %s  (current interval: %u)\n"
			"   MRR table inversion:  %s\n"
			"   AAA trigger type:     %s\n"
			"   Current Normal Mean:  %u\n"
		   	"   Current Normal Stdev: %u.%2u\n",
			ci->n_rates,
#ifdef CORA_FAST_RECOVERY
			"ENABLE",
#else
			"DISABLE",
#endif
#ifdef CORA_PKT_BASED
			"PACKETS",
#else
			"TIME",
#endif
			ci->update_interval,
#ifdef CORA_INVERT_MRR
			"ENABLE",
#else
			"DISABLE",
#endif
#ifdef CORA_AAA_THP_CHANGE
			"THROUGHPUT",
#else
			"NORMAL MEAN",
#endif
			ci->max_tp_rate_ndx,
			ci->cur_stdev / 100, ci->cur_stdev % 100
		);

	cs->len = p - cs->buf;
	return 0;
}

ssize_t
cora_stats_read (struct file *file, char __user *buf, size_t len, loff_t *ppos)
{
	struct cora_debugfs_info *cs;

	cs = file->private_data;
	return simple_read_from_buffer (buf, len, ppos, cs->buf, cs->len);
}

int
cora_stats_release (struct inode *inode, struct file *file)
{
	kfree (file->private_data);
	return 0;
}

static const struct file_operations cora_stat_fops = {
	.owner = THIS_MODULE,
	.open = cora_stats_open,
	.read = cora_stats_read,
	.release = cora_stats_release,
};

void
cora_add_sta_debugfs (void *priv, void *priv_sta, struct dentry *dir)
{
	struct cora_sta_info *ci = priv_sta;

	ci->dbg_stats = debugfs_create_file ("rc_stats", S_IRUGO, dir,
			ci, &cora_stat_fops); 
}

void
cora_remove_sta_debugfs(void *priv, void *priv_sta)
{
	struct cora_sta_info *ci = priv_sta;
	
	debugfs_remove (ci->dbg_stats);
}
