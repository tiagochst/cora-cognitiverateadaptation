/*
 * Copyright (C) 2011 Luciano Jerez Chaves <luciano@lrc.ic.unicamp.br>
 * 
 * Based on rc80211_cora.h:
 * Copyright (C) 2010 Tiago Chedraoui Silva <tsilva@lrc.ic.unicamp.br>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __RC_ARF_H
#define __RC_ARF_H

#define ARF_SUCC_THRS		10
#define ARF_TIMEOUT		    15

/* arf_rate is allocated once for each available rate at each arf_sta_info.
 * Information in this struct is private to this rate at this station */ 
struct arf_rate {
	int bitrate;
	int rix;
};


/* arf_sta_info is allocated once per station and holds information that allows
 * independed rate adaptation for each station */
struct arf_sta_info {
	unsigned int n_rates;			// number o supported rates 
	unsigned int current_rate_ndx;	// index of current rate
	
	u32 timer;						// packet timer
	u32 success;					// consecutive tx success
	u32 failures;					// consecutive tx failures
	bool recovery;					// recovery mode
	unsigned int success_thrs;		// success threshold
	unsigned int timeout;			// packet timer timeout

	/* Rate pointer for each station (created in arf_alloc_sta) */
	struct arf_rate *r;

#ifdef CONFIG_MAC80211_DEBUGFS
	struct dentry *dbg_stats;		// debug file pointer 
#endif
};


/* arf_priv is allocated once and holds information shared among all
 * arf_sta_info. */
struct arf_priv {
	struct ieee80211_hw *hw; 		// hardware properties
	unsigned int max_retry;		  	// default max number o retries before frame discard
};


/* Common functions */
extern struct rate_control_ops mac80211_arf;
void arf_add_sta_debugfs (void *priv, void *priv_sta, struct dentry *dir);
void arf_remove_sta_debugfs (void *priv, void *priv_sta);

/* Debugfs */
struct arf_debugfs_info {
	size_t len;
	char buf[];
};

int arf_stats_open (struct inode *inode, struct file *file);
ssize_t arf_stats_read (struct file *file, char __user *buf, size_t len, 
		loff_t *ppos);
int arf_stats_release (struct inode *inode, struct file *file);

#endif

