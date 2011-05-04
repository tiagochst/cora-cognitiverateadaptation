/*
 *  Copyright (C) 2010 Tiago Chedraoui Silva <tsilva@lrc.ic.unicamp.br>
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

#define CORA_STD_025 25
#define CORA_STD_030 30
#define CORA_STD_035 35
#define CORA_STD_040 40
#define CORA_STD_045 45
#define CORA_STD_050 50
#define CORA_STD_060 60
#define CORA_STD_070 70
#define CORA_STD_080 80
#define CORA_STD_090 90
#define CORA_STD_100 100
#define CORA_STD_110 110
#define CORA_STD_120 120

int AAA(int oldMean, int oldDev, int mean){
	int val=0;
	int aaafi =2;
	int aaapd=10;

	if(mean!= oldMean){
		/* A melhor taxa modificou - aumentar desvio padrao*/
		val=oldDev*aaafi;
		if(val>120)
			val=120;
	}else{
		/*A melhor taxa permaneceu - diminuir o desvio padrão*/
		val=oldDev-aaapd;
		if(val<40)
			val = 40;
	}
	return val;
}

/* Generating number based on normal distribution using the convolution method,
 * refer to "Raj Jain", "The art of computer systems performance analysis", page 494)
 */
int rc80211_cora_normal_generator(int mean, int variance, int n_rates)
{
	u32 rand;
	int value;
	int i;
	int quociente;
	int max;	

	value = 0;
	for (i = 0; i < n_rates; i++) {
		get_random_bytes(&rand, 4);
		value += (int)(rand & 0x00FFFFFF);	// used to avoid overflow
	}

	// 0x5FFFFFA = n/2 * 0x00FFFFFF, considering n=12 and the largest value of 0x00FFFFFF
	max = n_rates*0x00FFFFFF;
	value -= max/2;

	// Applying the variance
	switch(variance){
	case CORA_STD_025: 
		value /= 4;
		break;
	case CORA_STD_030:
		value *= 3;
		value /= 10;
		break;
	case CORA_STD_035:
		value *= 7;
		value /= 20;
		break;
	case CORA_STD_040:
		value *= 2;
		value /= 5;
		break;
	case CORA_STD_045:
		value *= 9;
		value /= 20;
		break;
	case CORA_STD_050:
		value /= 2;
		break; 
	case CORA_STD_060: 
		value *= 3;
		value /= 5;
		break;
	case CORA_STD_070:
		value *= 7;
		value /= 10;
		break;
	case CORA_STD_080:
		value *= 4;
		value /= 5;
		break;
	case CORA_STD_090:
		value *= 9;
		value /= 10;
		break;
	case CORA_STD_100:
		break;
	case CORA_STD_110:
		value *= 11 ;
		value /= 10 ;
		break;
	case CORA_STD_120:
		value *= 6 ;
		value /= 5 ;
		break;
	default:
		break;

	}
	
	/*--------------------------------------------------------------------------------*/
	/* Daqui pra cima não pode mudar porque faz parte da formula pra gerar os numeros */

	/* menor numero possivel de ser sorteado: 0x00000000 */
	/* maior numero possivel de ser sorteado: 0x0BFFFFF4 */

	/* se eu quero eles entre 0 e n_rates-1, vamos fazer 0x0BFFFFF4 / (n_rates-1)*/
	quociente = max/(n_rates);


	value --;
	value /= quociente;

	if(value+mean<0)
		value=mean*(-1);
	if(value+mean>=n_rates)
		value=n_rates-1-mean;

	return value + mean;


}


/* convert mac80211 rate index to local array index */
static inline int
rix_to_ndx(struct cora_sta_info *mi, int rix)
{
	int i = rix;

	for (i = rix+mi->sort_adapt; i >= 0; i--)
		if (mi->r[i].rix == rix)
			break;
	return i;
}

/* sort rates by bitrates*/
int sort_bitrates(struct cora_sta_info *mi, int n_rates, int max_rates){
	int max=0,adapt=0,max_adapt=0;
	int i,x=0;

		
	for(i=0;i<n_rates;i++){
		mi->r[i].sort_adapt=i;
		/*Taxa já está na posição certa?*/
		if(mi->r[i].bitrate>=max)
			/*Sim, atualiza valor de taxa máxima*/
			max=mi->r[i].bitrate; 
		
		else {
			x=i;
			adapt=0;
			while(mi->r[x].bitrate < mi->r[x-1].bitrate){
				mi->r[max_rates] =  mi->r[x-1];     
				mi->r[x-1] = mi->r[x];
				mi->r[x]=mi->r[max_rates];     
				x--;
				if(x==0)
					break;
			}
		}
	}

	for(i=0;i<n_rates;i++){
		adapt=i- mi->r[i].sort_adapt;
		if(adapt>max_adapt)
			max_adapt=adapt;
	}
	
	return max_adapt;
}

/* Função que faz a atualização dos dados. Ela é chamada para uma determinada
 * estação, e deve atualizar as informações de todas as taxas associadas a esta
 * estação. A função é chamada a cada intervalo de cora_priv "update_interval" */
static void
cora_update_stats(struct cora_priv *mp, struct cora_sta_info *mi)
{
	u32 max_tp = 0, index_max_tp = 0;
	u32 usecs;
	u32 p;
	int i;
	int oldDev=CORA_STD_120;


	mi->stats_update = jiffies;

	/* Para cada taxa suportada pela estação... */
	for (i = 0; i < mi->n_rates; i++) {
		struct cora_rate *mr = &mi->r[i]; 

		/* Se nao houver nenhuma estimativa de transmissão, coloca valor padrao
		 * (Esse valor não é muito grande?) */
		usecs = mr->perfect_tx_time;
		if (!usecs)
			usecs = 1000000;

		/* To avoid rounding issues, probabilities scale from 0 (0%)
		 * to 18000 (100%) */
		if (mr->attempts) {  // se houveram tentativas, então executa
			p = (mr->success * 18000) / mr->attempts;
			mr->succ_hist += mr->success;
			mr->att_hist += mr->attempts;
			mr->cur_prob = p;
			// TODO: De onde que surgiu esse 1000000 aí em cima?

			/* Agora é atualizar o throughput da taxa usando a EWMA com o cur_tp */
			mr->throughput = ((mr->cur_tp * (100 - mp->ewma_level)) + 
					  (mr->throughput * mp->ewma_level)) / 100;
			mr->cur_tp = p * (1000000 / usecs);
			mr->throughput = p * (1000000 / usecs);

		}

		/* Atualizar os contadores de tentativas e sucessos */
		mr->last_success = mr->success;
		mr->last_attempts = mr->attempts;
		mr->success = 0;
		mr->attempts = 0;
	}

	/* Procura a taxa com o maior throughput */
	for (i = 0; i < mi->n_rates; i++) {
		struct cora_rate *mr = &mi->r[i];
		if (max_tp < mr->throughput) {
			index_max_tp = i;
			max_tp = mr->throughput;
		}
		
	}

	/* Chama funcão de ajuste automático de agressivisade 
	 * oldMean: mi->max_tp_rate, oldDev: mi->dev, mean: index_max_tp
	 */
	oldDev=mi->dev;
	/* Define no cora_sta_info qual a taxa com o melhor throughput */
	mi->max_tp_rate = index_max_tp;
	mi->dev = AAA(mi->oldMean,oldDev,mi->max_tp_rate);	
	/* Sorteia a taxa para o proximo intervalo de acordo com uma distribuição normal */
	mi->use_this_rate_now = rc80211_cora_normal_generator(mi->max_tp_rate,mi->dev,mi->n_rates);
	mi->oldMean=mi->max_tp_rate;
	mi->r[mi->use_this_rate_now].times_called++;
}

/* Essa função é chamada para todos os quadros transmitidos imediatamente
 * depois que eles são transmitidos */
static void
cora_tx_status(void *priv, struct ieee80211_supported_band *sband,
               struct ieee80211_sta *sta, void *priv_sta, 
	       struct sk_buff *skb)
{
	struct cora_sta_info *mi = priv_sta;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ieee80211_tx_rate *ar = info->status.rates;
	int i, ndx;
	int success;
 
	/* Verifica se a transmissão foi ou não um sucesso */
	success = !!(info->flags & IEEE80211_TX_STAT_ACK);

	/* Percorre cada uma das taxas do ieee80211_tx_rate *ar */
	for (i = 0; i < IEEE80211_TX_MAX_RATES; i++) {
		
		/* Uma taxa -1 indica uma taxa inválida, e que todas as seguintes devem ser ignoradas */
		if (ar[i].idx < 0)
			break;

		/* Converte o índice da taxa ieee80211 no índice do vetor local */
		ndx = rix_to_ndx(mi, ar[i].idx);  
		if (ndx < 0)
			continue;

		/* Incrementa o numero de tentativas para a taxa ndx*/
		mi->r[ndx].attempts += ar[i].count;

		/* Incrementa o numero de sucessos para a taxa ndx. Está condicionado à
		 * ter ocorrido um sucesso com esta taxa. */
		if ((i != IEEE80211_TX_MAX_RATES - 1) && (ar[i + 1].idx < 0))
			mi->r[ndx].success += success;
	}
}

// Acho que essa função é chamada na hora de enviar cada pacote
static void
cora_get_rate(void *priv, struct ieee80211_sta *sta,
	      void *priv_sta, struct ieee80211_tx_rate_control *txrc)
{
	struct sk_buff *skb = txrc->skb;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct cora_sta_info *mi = priv_sta;
	struct cora_priv *mp = priv;
	struct ieee80211_tx_rate *ar = info->control.rates;
	unsigned int ndx = 0;
	bool mrr;
	unsigned int i;

	/* Verifica se o pacote (txrc->skb) é de gerenciamento ou controle, para
	 * retornar verdadeiro caso ele deva ser enviado com a taxa mais baixa. Se
	 * isso acontecer, simplismente retorna da função função cora_get_rate
	 * */
	if (rate_control_send_low(sta, priv_sta, txrc))
		return;

	/* Verificamos se o hardware suporta MRR */
	mrr = mp->has_mrr && !txrc->rts && !txrc->bss_conf->use_cts_prot;

	/* Verifica se já se passaram mp->update_interval unidades de tempo desde a
	 * ultima execução do rate adapter (cora_update_stats). Se sim, atualiza os
	 * contadores e escolhe uma nova taxa para transmissão durante o poximo
	 * intervalo. */
	if (time_after(jiffies, mi->stats_update + (mp->update_interval * HZ) / 1000))
		cora_update_stats(mp, mi);

	/* Recupera o índice da taxa que será usada até a proxima chamada do
	 * "cora_update_stats" */
	ndx = mi->use_this_rate_now;
	
	/* Populando o MRR */
	/* Essa é a linha que define para o ieee80211_tx_rate qual taxa usar. Aqui
	 * está ocorrendo a conversão do ndx para o índice correto da mac80211 */
	ar[0].idx=mi->r[mi->use_this_rate_now].rix;
	ar[0].count = mp->max_retry;

	if (mrr) {
		for (i=1; i < IEEE80211_TX_MAX_RATES; i++) {
			ar[i].idx = -1;
			ar[i].count = 0;
		}
	}
}


/* Função usada pelo cora para estimar o tempo de transmissão de um único
 * pacote pela rede sem fio */
static void
calc_rate_durations(struct cora_sta_info *mi, struct ieee80211_local *local,
                    struct cora_rate *d, struct ieee80211_rate *rate)
{
	int erp = !!(rate->flags & IEEE80211_RATE_ERP_G);

	/* FIXME Estamos assumindo que todos os pacotes são de 1200 bytes */
	d->perfect_tx_time = ieee80211_frame_duration(local, 1200,
						      rate->bitrate, erp, 1);
	d->ack_time = ieee80211_frame_duration(local, 10,
					       rate->bitrate, erp, 1);
}

/* TODO: Função chamada pelo ieee80211_add_station. Até o momento eu acho que
 * ela inicializa as taxas que cada estação suporta */
static void
cora_rate_init(void *priv, struct ieee80211_supported_band *sband,
               struct ieee80211_sta *sta, void *priv_sta)
{
	struct cora_sta_info *mi = priv_sta;
	struct cora_priv *mp = priv;
	struct ieee80211_local *local = hw_to_local(mp->hw);
	struct ieee80211_rate *ctl_rate;
	unsigned int i, n = 0;
	unsigned int t_slot = 9; /* FIXME: get real slot time */

	/* Recupera o índice da menor taxa para uma determinada estacao. Em seguida
	 * recupera o que chamamos de taxa de controle (a menor taxa) e calcula o
	 * tempo de deração da transmissão de um pacote de ACK */
	mi->lowest_rix = rate_lowest_index(sband, sta);
	mi->dev = 120 ;/*Começa com maior desvio padrao possivel*/
	mi->oldMean = 0 ;/*Começa com maior desvio padrao possivel*/

	ctl_rate = &sband->bitrates[mi->lowest_rix];
	mi->sp_ack_dur = ieee80211_frame_duration(local, 10, ctl_rate->bitrate,
						  !!(ctl_rate->flags & IEEE80211_RATE_ERP_G), 1);

	/* Populando as informações para cada taxa disponível */
	for (i = 0; i < sband->n_bitrates; i++) {
		struct cora_rate *mr = &mi->r[n];
		unsigned int tx_time = 0, tx_time_cts = 0, tx_time_rtscts = 0;
		unsigned int tx_time_single;
		unsigned int cw = mp->cw_min;

		if (!rate_supported(sta, sband->band, i))
			continue;
		n++;
		memset(mr, 0, sizeof(*mr));		// limpa todas as informações da taxa

		/* Define o índice, a taxa e o tempo de transmissão de um quadro de
		 * 1200 com esta taxa */ 
		mr->rix = i;
		mr->bitrate = sband->bitrates[i].bitrate / 5;
		calc_rate_durations(mi, local, mr,
				    &sband->bitrates[i]);

		/* calculate maximum number of retransmissions before
		 * fallback (based on maximum segment size) */
		mr->retry_count = 1;
		mr->retry_count_cts = 1;
		mr->retry_count_rtscts = 1;
		mr->times_called = 0;/*contando quantas vezes uma taxa é usada*/
		
		/* Calculando as quantidades de retransmissões possíveis dada as
		 * condições: normal, com CTS, com CTS e RTS, e o adjusted eu não sei o
		 * que é! */
		tx_time = mr->perfect_tx_time + mi->sp_ack_dur;
		do {
			/* add one retransmission */
			tx_time_single = mr->ack_time + mr->perfect_tx_time;

			/* contention window */
			tx_time_single += t_slot + min(cw, mp->cw_max);
			cw = (cw << 1) | 1;

			tx_time += tx_time_single;
			tx_time_cts += tx_time_single + mi->sp_ack_dur;
			tx_time_rtscts += tx_time_single + 2 * mi->sp_ack_dur;
			if ((tx_time_cts < mp->segment_size) &&
			    (mr->retry_count_cts < mp->max_retry))
				mr->retry_count_cts++;
			if ((tx_time_rtscts < mp->segment_size) &&
			    (mr->retry_count_rtscts < mp->max_retry))
				mr->retry_count_rtscts++;

			/* Repete até chegar no máximo de tentativas ou estourar o tempo do
			 * hardware */
		} while ((tx_time < mp->segment_size) &&
			 (++mr->retry_count < mp->max_retry));
		mr->adjusted_retry_count = mr->retry_count;

	}

	/*Ordena taxas usadas*/
	mi->sort_adapt = sort_bitrates(mi, n,sband->n_bitrates);

	/* Simplismente coloca -1 para indicar que as últimas taxas não são
	 * suportadas */
	for (i = n; i < sband->n_bitrates; i++) {
		struct cora_rate *mr = &mi->r[i];
		mr->rix = -1;
	}

	mi->n_rates = n;
	mi->stats_update = jiffies;


}

/* Função chamada cada vez que uma nova estação se associa. Aloca as estruturas
 * apropriadas para esta estação. */
static void *
cora_alloc_sta(void *priv, struct ieee80211_sta *sta, gfp_t gfp)
{
	struct ieee80211_supported_band *sband;
	struct cora_sta_info *mi;
	struct cora_priv *mp = priv;
	struct ieee80211_hw *hw = mp->hw;
	int max_rates = 0;
	int i;

	mi = kzalloc(sizeof(struct cora_sta_info), gfp);
	if (!mi)
		return NULL;

	/* Calcula a quantidade de taxas com base nas bandas disponíveis. Isso aqui
	 * tá funcionando perfeitamente! */
	for (i = 0; i < IEEE80211_NUM_BANDS; i++) {
		sband = hw->wiphy->bands[i];
		if (sband && sband->n_bitrates > max_rates)
			max_rates = sband->n_bitrates;
	}

	/* Alocando o "vetor" de rates para a estação */
	/*O mais um é usado na ordenaçã das taxas*/
	mi->r = kzalloc(sizeof(struct cora_rate) * (max_rates+1), gfp);
	if (!mi->r) {
		kfree(mi);
		return NULL;
	}

	/* Jiffies é uma medida mínima de tempo (muito provavelmente é o DIFS). Não
	 * sei bem pra que usa, mas serve nos calculos de quando devemos atualizar
	 * as informações. */
	mi->stats_update = jiffies;
	return mi;
}


/* Usado para liberar memória de uma estação quando esta se desassocia. */
static void
cora_free_sta(void *priv, struct ieee80211_sta *sta, void *priv_sta)
{
	struct cora_sta_info *mi = priv_sta;

	kfree(mi->r);
	kfree(mi);
}

/* Função chamada uma única vez para alocar a estrutura do CORA */
static void *
cora_alloc(struct ieee80211_hw *hw, struct dentry *debugfsdir)
{
	struct cora_priv *mp;

	mp = kzalloc(sizeof(struct cora_priv), GFP_ATOMIC);
	if (!mp)
		return NULL;

	/* contention window settings
	 * Just an approximation. Using the per-queue values would complicate
	 * the calculations and is probably unnecessary */
	mp->cw_min = 15;
	mp->cw_max = 1023;

	/* moving average weight for EWMA for the throughput */
	mp->ewma_level = 10;

	/* maximum time that the hw is allowed to stay in one MRR segment */
	mp->segment_size = 6000;

	if (hw->max_rate_tries > 0)
		mp->max_retry = hw->max_rate_tries;
	else
		/* safe default, does not necessarily have to match hw properties */
		mp->max_retry = 7;

	/* Verifica se o hardware suporta mrr e seta como true a flag do cora_priv */
	if (hw->max_rates >= 4)
		mp->has_mrr = true;

	mp->hw = hw;

	/* Intervalo entre adaptações de taxa (100 ms) */
	mp->update_interval = 100;

	return mp;
}

static void
cora_free(void *priv)
{
	kfree(priv);
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
	return ieee80211_rate_control_register(&mac80211_cora);
}

void
rc80211_cora_exit(void)
{
	ieee80211_rate_control_unregister(&mac80211_cora);
}

/*
 * struct ieee80211_tx_rate - rate selection/status
 *
 * @idx: rate index to attempt to send with
 * @flags: rate control flags (&enum mac80211_rate_control_flags)
 * @count: number of tries in this rate before going to the next rate
 *
 * A value of -1 for @idx indicates an invalid rate and, if used
 * in an array of retry rates, that no more rates should be tried.
 *
 * When used for transmit status reporting, the driver should
 * always report the rate along with the flags it used.
 *
 * &struct ieee80211_tx_info contains an array of these structs
 * in the control information, and it will be filled by the rate
 * control algorithm according to what should be sent. For example,
 * if this array contains, in the format { <idx>, <count> } the
 * information
 *    { 3, 2 }, { 2, 2 }, { 1, 4 }, { -1, 0 }, { -1, 0 }
 * then this means that the frame should be transmitted
 * up to twice at rate 3, up to twice at rate 2, and up to four
 * times at rate 1 if it doesn't get acknowledged. Say it gets
 * acknowledged by the peer after the fifth attempt, the status
 * information should then contain
 *   { 3, 2 }, { 2, 2 }, { 1, 1 }, { -1, 0 } ...
 * since it was transmitted twice at rate 3, twice at rate 2
 * and once at rate 1 after which we received an acknowledgement.
 */


