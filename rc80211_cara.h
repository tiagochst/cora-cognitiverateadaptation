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

#ifndef __RC_CARA_H
#define __RC_CARA_H

#define CARA_SUCC_THRS		10
#define CARA_TIMEOUT		    15

/* cara_rate is allocated once for each available rate at each cara_sta_info.
 * Information in this struct is private to this rate at this station */ 
struct cara_rate {
	int bitrate;
	int rix;
};


/* cara_sta_info is allocated once per station and holds information that allows
 * independed rate adaptation for each station */
struct cara_sta_info {
	unsigned int n_rates;			// number o supported rates 
	unsigned int current_rate_ndx;	// index of current rate
	
	u32 timer;						// packet timer
	u32 success;					// consecutive tx success
	u32 failures;					// consecutive tx failures
	bool recovery;					// recovery mode
	unsigned int success_thrs;		// success threshold
	unsigned int timeout;			// packet timer timeout

	/* Rate pointer for each station (created in cara_alloc_sta) */
	struct cara_rate *r;

#ifdef CONFIG_MAC80211_DEBUGFS
	struct dentry *dbg_stats;		// debug file pointer 
#endif
};


/* cara_priv is allocated once and holds information shared among all
 * cara_sta_info. */
struct cara_priv {
	struct ieee80211_hw *hw; 		// hardware properties
	unsigned int max_retry;		  	// default max number o retries before frame discard
};


/* Common functions */
extern struct rate_control_ops mac80211_cara;
void cara_add_sta_debugfs (void *priv, void *priv_sta, struct dentry *dir);
void cara_remove_sta_debugfs (void *priv, void *priv_sta);

/* Debugfs */
struct cara_debugfs_info {
	size_t len;
	char buf[];
};

int cara_stats_open (struct inode *inode, struct file *file);
ssize_t cara_stats_read (struct file *file, char __user *buf, size_t len, 
		loff_t *ppos);
int cara_stats_release (struct inode *inode, struct file *file);

#endif

