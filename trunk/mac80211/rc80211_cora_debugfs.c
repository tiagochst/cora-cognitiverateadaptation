/*
 *  Copyright (C) 2010 Tiago Chedraoui Silva <tsilva@lrc.ic.unicamp.br>
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
cora_stats_open(struct inode *inode, struct file *file)
{
	struct cora_sta_info *mi = inode->i_private;
	struct cora_debugfs_info *ms;
	unsigned int i, tp, prob;
	char *p;

	ms = kmalloc(sizeof(*ms) + 4096, GFP_KERNEL);
	if (!ms)
		return -ENOMEM;

	file->private_data = ms;
	p = ms->buf;
	p += sprintf(p, "rate    | throughput ewma | this prob | "
		     "this succ/attempt | success | attempts | count \n");
	for (i = 0; i < mi->n_rates; i++) {
		struct cora_rate *mr = &mi->r[i];

		*(p++) = (i == mi->use_this_rate_now) ? 'T' : ' ';
		p += sprintf(p, "%3u%s", mr->bitrate / 2,
			     (mr->bitrate & 1 ? ".5" : "  "));

		tp=mr->throughput/ ((18000 << 10) / 96);	
		prob = mr->cur_prob / 18;

		p += sprintf(p, " |       %6u.%1u|   %6u.%1u | "
			     "%3u(%3u)| %8llu | %8llu | %6u\n ",
			     tp / 10, tp % 10,
			     prob / 10, prob % 10,
			     mr->last_success,
			     mr->last_attempts,
			     (unsigned long long)mr->succ_hist,
			     (unsigned long long)mr->att_hist,
			     mr->times_called);
	}
	p += sprintf(p, "\n TCS - CORA -- Total packet count::  ideal %d Desvio atual %d, taxa max tp %d\n", mi->packet_count, mi->dev,mi->max_tp_rate);
	ms->len = p - ms->buf;

	return 0;
}

ssize_t
cora_stats_read(struct file *file, char __user *buf, size_t len, loff_t *ppos)
{
	struct cora_debugfs_info *ms;

	ms = file->private_data;
	return simple_read_from_buffer(buf, len, ppos, ms->buf, ms->len);
}

int
cora_stats_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static const struct file_operations cora_stat_fops = {
	.owner = THIS_MODULE,
	.open = cora_stats_open,
	.read = cora_stats_read,
	.release = cora_stats_release,
};

void
cora_add_sta_debugfs(void *priv, void *priv_sta, struct dentry *dir)
{
	struct cora_sta_info *mi = priv_sta;
	mi->dbg_stats = debugfs_create_file("rc_stats", S_IRUGO, dir,
					    mi,&cora_stat_fops); 

}

void
cora_remove_sta_debugfs(void *priv, void *priv_sta)
{
	struct cora_sta_info *mi = priv_sta;

	debugfs_remove(mi->dbg_stats);
}