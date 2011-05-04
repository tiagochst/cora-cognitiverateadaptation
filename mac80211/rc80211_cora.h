/*
 * Copyright (C) 2010 Tiago Chedraoui Silva <tsilva@lrc.ic.unicamp.br>
 * 
 * Based on rc80211_minstrel.h:
 * Copyright (C) 2008 Felix Fietkau <nbd@openwrt.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __RC_CORA_H
#define __RC_CORA_H

/* Aqui vão ficar as informações referentes a uma das várias taxas de
 * transmissão. Existe uma entrada desta struct para cada uma das estações com
 * a qual o mac80211 está se comunicando. Aqui então ficam informações como
 * numero de pacotes enviados, perdas, etc. */
struct cora_rate {
	int bitrate;
	int rix;
	
	// Informações usadas para calcular o throughput
	unsigned int perfect_tx_time;	// tempo para transmissao de um quadro de 1200 bytes usando esta taxa
	unsigned int ack_time;		// tempo para transmissao do ack usando esta taxa
	
	// O CORA não precisa dessas coisas
	// FIXME comentar isso aqui depois
	unsigned int retry_count;
	unsigned int retry_count_cts;
	unsigned int retry_count_rtscts;
	unsigned int adjusted_retry_count;
	
	u32 success;        // quantidade de pacotes transmitidos com sucesso durante o último intervalo
	u32 attempts;       // quantidade de tentativas durante o último intervalo
	u64 succ_hist;      // numero total de sucessos desde o início do algoritmo
	u64 att_hist;       // numero total de tentativas desde o início do algoritmo
	u32 last_attempts;  // valor de success no momento da última chamada da minstrel_update_stats
	u32 last_success;   // valor de attempts no momento da última chamada da minstrel_update_stats
	
	/* parts per thousand */
	u32 cur_prob;       // probabilidade de entrega atual (para o ultimo intervalo)
  	/* per-rate throughput */
	u32 cur_tp;         // vazão atual (para o ultimo intervalo)
	u32 throughput;     // vazão da taxa já calculada com a EWMA
	u32 times_called;   // numero de vezes que a taxa foi usada
	unsigned int sort_adapt; // shifts da taxa durante ordenacao

};


/* cora_sta_info é alocada para cada estação atualmente conectada ao
 * dispositivo. As informações desta struct permitem fazer a adaptação de
 * maneira independente para cada enlace sem fio. De maneira sucinta, isso
 * reflete à classe CORALink do simulador */
struct cora_sta_info {
	unsigned int oldMean;           // media inicial
	unsigned int dev;               // desvio padrao inicial
	unsigned long stats_update;     // DIFS?
	unsigned int sp_ack_dur;	// tempo gasto para transmitir um pacote de ACK usando a taxa de controle mais baixa
	unsigned int rate_avg;		

	/* Os índices a seguir entao numerados de 0 até N, e dizem respeito somente as taxas suportadas pela estação */
	unsigned int lowest_rix;        // é o índice da menor taxa disponível para uso (já é retornado pelo proprio ieee80211)
	unsigned int max_tp_rate;       // é o índice da taxa com o maior throughput
	unsigned int use_this_rate_now; // é a taxa sorteada de arcordo com a distribuicao normal para ser usada durante um intervalo
	unsigned int packet_count;      // contagem de pacotes (é incrementado toda vez que entra na função cora_get_rate e NAO é pacote de gerenciamento)
	unsigned int sort_adapt;        // Quantas vezes uma taxa foi alterada de posição durante ordenação? Usado para aumentar limitador de busca
	unsigned int n_rates;	        // numero de taxas suportadas pela estação
	struct cora_rate *r;        	// ponteiro para o vetor de rates para esta estação (é alocado na cora_alloc_sta)
	
#ifdef CONFIG_MAC80211_DEBUGFS
	struct dentry *dbg_stats;	// apontador para arquivo de debug (usado na cora_add_sta_debugfs)
#endif
};


/* cora_priv é alocada uma única vez. As informações desta struct são
 * compartilhadas com todas as cora_sta_info. De maneira sucinta equivale à
 * classe CORA do simulador */
struct cora_priv {
	struct ieee80211_hw *hw;      // ponteiro para recuperar informacoes da interface de rede
	bool has_mrr;                 // em cora_alloc verifica se o hardware suporta mrr
	unsigned int cw_min;	      // janela de congestionamento mínima
	unsigned int cw_max;	      // janela de congestionamento máxima
	unsigned int max_retry;       // número máximo de tentativas antes de descartar o pacote (fica default)
	unsigned int ewma_level;      // usado para atualizar o throughput. Reflete o peso das informações na base (ANTIGO)
	unsigned int segment_size;    // tempo máximo que o hardware permite ficar em um mesmo segmento MRR
	unsigned int update_interval; // tempo entre chamadas consecutivas de cora_update_stats
};

struct cora_debugfs_info {
	size_t len;
	char buf[];
};

extern struct rate_control_ops mac80211_cora;
void cora_add_sta_debugfs(void *priv, void *priv_sta, struct dentry *dir);
void cora_remove_sta_debugfs(void *priv, void *priv_sta);

/* debugfs */
int cora_stats_open(struct inode *inode, struct file *file);
ssize_t cora_stats_read(struct file *file, char __user *buf, size_t len, loff_t *ppos);
int cora_stats_release(struct inode *inode, struct file *file);

#endif

