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

#ifndef __RC_FIXED_H
#define __RC_FIXED_H

#define FIXED_RATE_NDX			7

struct fixed_rate {
	int bitrate;
	int rix;
};

struct fixed_sta_info {
	unsigned int n_rates;
	unsigned int fixed_rate_ndx;
	
	struct fixed_rate *r;

#ifdef CONFIG_MAC80211_DEBUGFS
	struct dentry *dbg_stats;		// debug file pointer 
#endif
};

struct fixed_priv {
	struct ieee80211_hw *hw; 		// hardware properties
	unsigned int max_retry;		  	// default max number o retries before frame discard
};


/* Common functions */
extern struct rate_control_ops mac80211_fixed;
void fixed_add_sta_debugfs (void *priv, void *priv_sta, struct dentry *dir);
void fixed_remove_sta_debugfs (void *priv, void *priv_sta);

/* Debugfs */
struct fixed_debugfs_info {
	size_t len;
	char buf[];
};

int fixed_stats_open (struct inode *inode, struct file *file);
ssize_t fixed_stats_read (struct file *file, char __user *buf, size_t len, 
		loff_t *ppos);
int fixed_stats_release (struct inode *inode, struct file *file);

#endif

