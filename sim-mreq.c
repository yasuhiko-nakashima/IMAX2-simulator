
static char RcsHeader[] = "$Header: /usr/home/nakashim/proj-arm64/src/csim/RCS/sim-mreq.c,v 1.4 2017/04/21 03:28:45 nakashim Exp nakashim $";

/* SPARC Simulator                     */
/*         Copyright (C) 2010 by NAIST */
/*         Primary writer: Y.Nakashima */
/*                nakashim@is.naist.jp */

/* sim-mreq.c 2010/7/10 */

#include "csim.h"

void *sim_mreq(param) Uint param;
{
  Uint  did = param >> 16;
  int   cycle = param & 0xffff;
  int   i, j, l, index, rq_cid, tg_cid, tg_mid, stat, rq_push;
  Ull   push_adr;

  while (cycle-- > 0) {
    /*========================================================================================================*/
    /* 各PE付随のl2rqを走査して起動対応      */
    /* L2rq->MAXL2BK->L2CC/MMCC 送信等         */
    /*========================================================================================================*/
    for (i=0; i<MAXCORE; i++) {
      rq_cid = (d[did].rq_cid_first_search+i)%MAXCORE;
      if (d[did].l2rq_bitmap[rq_cid]) {
	if (!c[rq_cid].l2rq[did].rq_push && !c[rq_cid].l2rq[did].rq_pull) { /* inv only */
	  j = (c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK;
	  if (!(d[did].l2rq_lock[rq_cid/ULLBITS] & ~(1LL<<(rq_cid%ULLBITS))) && did==j)
	    break; /* accept request */
	}
	else {
	  if (c[rq_cid].l2rq[did].rq_push) { /* push */
	    j = (c[rq_cid].l2rq[did].push_ADR/LINESIZE)%MAXL2BK;
	    if (!(d[did].l2rq_lock[rq_cid/ULLBITS] & ~(1LL<<(rq_cid%ULLBITS))) && did==j)
	      break; /* accept request */
	  }
	  if (c[rq_cid].l2rq[did].rq_pull) { /* pull */
	    j = (c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK;
	    if (!(d[did].l2rq_lock[rq_cid/ULLBITS] & ~(1LL<<(rq_cid%ULLBITS))) && did==j)
	      break; /* accept request */
	  }
	}
	if (flag & TRACE_ARM)
	  printf("d%02.2d:rq_cid=%d:l2rq_lock=%08.8x_%08.8x LOCKED by other core\n",
		 did, rq_cid, (Uint)(d[did].l2rq_lock[0]>>32), (Uint)d[did].l2rq_lock[0]);
      }
    }

    if (i < MAXCORE) {
      if (flag & TRACE_ARM) {
	printf("d%02.2d:l2rq_bitmap=", did);	
	for (i=0; i<MAXCORE; i++)
	  printf("%d", d[did].l2rq_bitmap[i]);
	printf(":l2rq_lock=%08.8x_%08.8x\n", (Uint)(d[did].l2rq_lock[0]>>32), (Uint)(d[did].l2rq_lock[0]));
	i = c[rq_cid].l2rq[did].push_ADR/LINESIZE/MAXL2BK;
	j = (c[rq_cid].l2rq[did].push_ADR/LINESIZE)%MAXL2BK;
	printf("   %cv=%x push=%d l2rq_push A=%08.8x: v=%08.8x_%08.8x d=%08.8x_%08.8x s=%08.8x_%08.8x%c\n",
	       (did==j)?' ':'(', c[rq_cid].l2rq[did].v_stat, c[rq_cid].l2rq[did].rq_push,
	       c[rq_cid].l2rq[did].push_ADR,
	       (Uint)(d[did].l2dir[i].l2dir_v[0]>>32), (Uint)(d[did].l2dir[i].l2dir_v[0]),
	       (Uint)(d[did].l2dir[i].l2dir_d[0]>>32), (Uint)(d[did].l2dir[i].l2dir_d[0]),
	       (Uint)(d[did].l2dir[i].l2dir_s[0]>>32), (Uint)(d[did].l2dir[i].l2dir_s[0]),
	       (did==j)?' ':')');
	i = c[rq_cid].l2rq[did].pull_ADR/LINESIZE/MAXL2BK;
	j = (c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK;
	printf("   %cv=%x pull=%d l2rq_pull A=%08.8x: v=%08.8x_%08.8x d=%08.8x_%08.8x s=%08.8x_%08.8x%c\n",
	       (did==j)?' ':'(', c[rq_cid].l2rq[did].v_stat, c[rq_cid].l2rq[did].rq_pull,
	       c[rq_cid].l2rq[did].pull_ADR,
	       (Uint)(d[did].l2dir[i].l2dir_v[0]>>32), (Uint)(d[did].l2dir[i].l2dir_v[0]),
	       (Uint)(d[did].l2dir[i].l2dir_d[0]>>32), (Uint)(d[did].l2dir[i].l2dir_d[0]),
	       (Uint)(d[did].l2dir[i].l2dir_s[0]>>32), (Uint)(d[did].l2dir[i].l2dir_s[0]),
	       (did==j)?' ':')');
      }
      if (c[rq_cid].l2rq[did].v_stat == ((3<<2)|1)) { /* L2 request */
	d[did].l2rq_lock[rq_cid/ULLBITS] |= (1LL<<(rq_cid%ULLBITS)); /* 他のpeからのリクエストを待たせる */
	d[did].root_cid = rq_cid; /* 処理対象 root pid を記録 */
	if (flag & TRACE_ARM)
	  printf("d%02.2d:rq_cid[%d]:LOCKED l2rq.t=%d\n", did, rq_cid, c[rq_cid].l2rq[did].t);
	if (c[rq_cid].l2rq[did].t)
	  c[rq_cid].l2rq[did].t--;
	if (c[rq_cid].l2rq[did].t == 0) {
	  /* 0:out of range, 1:DDR3, 2:CTR, 3:DSM(my space), 4:DSM(other space), 5:CSM */
	  if (d[did].l2d_state == L2DSTATE_EMPTY && !c[rq_cid].l2rq[did].rq_push && !c[rq_cid].l2rq[did].rq_pull) { /* inv only */
	    d[did].l2dir_ent = c[rq_cid].l2rq[did].pull_ADR/LINESIZE/MAXL2BK;
#if 0
	    for (k=MAXCORbitmaps-1; k>=0; k--) { d[did].l2dir_v_new0[k] = (k==MAXCORbitmaps-1)?0xffffffffffffffffLL>>(ULLBITS-((MAXCORE-1)%ULLBITS+1)):0xffffffffffffffffLL; }
	    for (k=MAXCORbitmaps-1; k>=0; k--) { d[did].l2dir_d_new0[k] = (k==MAXCORbitmaps-1)?0xffffffffffffffffLL>>(ULLBITS-((MAXCORE-1)%ULLBITS+1)):0xffffffffffffffffLL; }
	    for (k=MAXCORbitmaps-1; k>=0; k--) { d[did].l2dir_s_new0[k] = (k==MAXCORbitmaps-1)?0xffffffffffffffffLL>>(ULLBITS-((MAXCORE-1)%ULLBITS+1)):0xffffffffffffffffLL; }
	    for (k=MAXCORbitmaps-1; k>=0; k--) { d[did].l2dir_v_new1[k] = 0x0000000000000000LL;
	    for (k=MAXCORbitmaps-1; k>=0; k--) { d[did].l2dir_d_new1[k] = 0x0000000000000000LL;
	    for (k=MAXCORbitmaps-1; k>=0; k--) { d[did].l2dir_s_new1[k] = 0x0000000000000000LL;
#endif
	    d[did].l2dir_v_new0[0] = 0xffffffffffffffffLL>>(64-MAXCORE);
	    d[did].l2dir_d_new0[0] = 0xffffffffffffffffLL>>(64-MAXCORE);
	    d[did].l2dir_s_new0[0] = 0xffffffffffffffffLL>>(64-MAXCORE);
	    d[did].l2dir_v_new1[0] = 0x0000000000000000LL;
	    d[did].l2dir_d_new1[0] = 0x0000000000000000LL;
	    d[did].l2dir_s_new1[0] = 0x0000000000000000LL;
	    Ull exclusive =(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0] & ~(1LL<<rq_cid))
	                  &~d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]
	                  &~d[did].l2dir[d[did].l2dir_ent].l2dir_s[0];
	    Ull shared    =(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0] & ~(1LL<<rq_cid))
	                  &~d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]
	                  & d[did].l2dir[d[did].l2dir_ent].l2dir_s[0];
	    Ull modified  =(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0] & ~(1LL<<rq_cid))
	                  & d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]
	                  &~d[did].l2dir[d[did].l2dir_ent].l2dir_s[0];
	    Ull owned     =(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0] & ~(1LL<<rq_cid))
	                  & d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]
	                  & d[did].l2dir[d[did].l2dir_ent].l2dir_s[0];
	    if (flag & TRACE_ARM)
	      printf("d%02.2d:l2dir[%d]: inv rq_cid=%d pull_ADR=%08.8x v=%08.8x_%08.8x d=%08.8x_%08.8x s=%08.8x_%08.8x read\n",
		     did, d[did].l2dir_ent, rq_cid,
		     c[rq_cid].l2rq[did].pull_ADR,
		     (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0]),
		     (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]),
		     (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_s[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_s[0]));
	    /* l2が有効なので,l2rqからのpullは不要.invalidateのみ.対象は参照l2cc */
	    d[did].l2d_state = L2DSTATE_DRAIN;
	    if (shared|exclusive) { /* clean found */
	      for (i=0; i<MAXCORE; i++) {
		if ((shared|exclusive) & (1LL<<i)) {
		  tg_cid = i;
		  if (c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].v_stat) { /* l2ccが空いていなければ止める */
		    if (flag & TRACE_ARM)
		      printf("d%02.2d:L2CC rq_cid[%d]:tg_cid[%d]:tg_cid.l2cc[%d] pull inv shared/exclusive l2cc ful\n",
			     did, rq_cid, tg_cid, (Uint)((c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK));
		  }
		  else { /* l2ccにエンキュー */
		    c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].t            = CCDELAY;
		    c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].rq_pull      = 0;
		    c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].rq_inv_share = 0; /* store->inv */
		    c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].target_ADR   = c[rq_cid].l2rq[did].pull_ADR;
		    c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].v_stat       = (3<<2)|(1); /* valid */
		    d[did].l2cc_req_bitmap[0] |= 1LL<<tg_cid;
		    if (flag & TRACE_ARM)
		      printf("d%02.2d:L2CC rq_cid[%d]:tg_cid[%d]:tg_cid.l2cc[%d] pull inv shared/exclusive l2cc queued\n",
			     did, rq_cid, tg_cid, (Uint)((c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK));
		    d[did].l2dir_v_new0[0] &= ~1LL<<tg_cid;
		    d[did].l2dir_d_new0[0] &= ~1LL<<tg_cid;
		    d[did].l2dir_s_new0[0] &= ~1LL<<tg_cid;
		  }
		}
	      }
	    }
	    d[did].l2dir_v_new1[0] |=  1LL<<rq_cid;
	    d[did].l2dir_d_new1[0] |=  1LL<<rq_cid;
	    d[did].l2dir_s_new0[0] &= ~1LL<<rq_cid;
	  }
	  else if (d[did].l2d_state == L2DSTATE_EMPTY && c[rq_cid].l2rq[did].rq_push) { /* PUSH:flush,dirty-wb,nc-store */
	    d[did].l2dir_ent = c[rq_cid].l2rq[did].push_ADR/LINESIZE/MAXL2BK;
	    d[did].l2dir_v_new0[0] = 0xffffffffffffffffLL>>(64-MAXCORE);
	    d[did].l2dir_d_new0[0] = 0xffffffffffffffffLL>>(64-MAXCORE);
	    d[did].l2dir_s_new0[0] = 0xffffffffffffffffLL>>(64-MAXCORE);
	    d[did].l2dir_v_new1[0] = 0x0000000000000000LL;
	    d[did].l2dir_d_new1[0] = 0x0000000000000000LL;
	    d[did].l2dir_s_new1[0] = 0x0000000000000000LL;
	    Ull exclusive =(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0] & ~(1LL<<rq_cid))
	                  &~d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]
	                  &~d[did].l2dir[d[did].l2dir_ent].l2dir_s[0];
	    Ull shared    =(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0] & ~(1LL<<rq_cid))
	                  &~d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]
	                  & d[did].l2dir[d[did].l2dir_ent].l2dir_s[0];
	    Ull modified  =(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0] & ~(1LL<<rq_cid))
	                  & d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]
	                  &~d[did].l2dir[d[did].l2dir_ent].l2dir_s[0];
	    Ull owned     =(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0] & ~(1LL<<rq_cid))
	                  & d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]
	                  & d[did].l2dir[d[did].l2dir_ent].l2dir_s[0];
	    if (flag & TRACE_ARM)
	      printf("d%02.2d:l2dir[%d]: push rq_cid=%d push_ADR=%08.8x v=%08.8x_%08.8x d=%08.8x_%08.8x s=%08.8x_%08.8x read\n",
		     did, d[did].l2dir_ent, rq_cid,
		     c[rq_cid].l2rq[did].push_ADR,
		     (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0]),
		     (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]),
		     (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_s[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_s[0]));
	    /* l2rqからのpushはdirtyの追い出し.追い出し先はmmcc,無効化対象は参照l2cc */
	    d[did].l2d_state = L2DSTATE_DRAIN;
	    if (shared|exclusive) { /* clean found */
	      for (i=0; i<MAXCORE; i++) {
		if ((shared|exclusive) & (1LL<<i)) {
		  tg_cid = i;
		  if (c[tg_cid].l2cc[(c[rq_cid].l2rq[did].push_ADR/LINESIZE)%MAXL2BK].v_stat) { /* l2ccが空いていなければ止める */
		    if (flag & TRACE_ARM)
		      printf("d%02.2d:L2CC rq_cid[%d]:tg_cid[%d]:tg_cid.l2cc[%d] push inv shared/exclusive l2cc ful\n",
			     did, rq_cid, tg_cid, (Uint)((c[rq_cid].l2rq[did].push_ADR/LINESIZE)%MAXL2BK));
		  }
		  else { /* l2ccにエンキュー */
		    c[tg_cid].l2cc[(c[rq_cid].l2rq[did].push_ADR/LINESIZE)%MAXL2BK].t            = CCDELAY;
		    c[tg_cid].l2cc[(c[rq_cid].l2rq[did].push_ADR/LINESIZE)%MAXL2BK].rq_pull      = 0;
		    c[tg_cid].l2cc[(c[rq_cid].l2rq[did].push_ADR/LINESIZE)%MAXL2BK].rq_inv_share = 0; /* store->inv */
		    c[tg_cid].l2cc[(c[rq_cid].l2rq[did].push_ADR/LINESIZE)%MAXL2BK].target_ADR   = c[rq_cid].l2rq[did].push_ADR;
		    c[tg_cid].l2cc[(c[rq_cid].l2rq[did].push_ADR/LINESIZE)%MAXL2BK].v_stat       = (3<<2)|(1); /* valid */
		    d[did].l2cc_req_bitmap[0] |= 1LL<<tg_cid;
		    if (flag & TRACE_ARM)
		      printf("d%02.2d:L2CC rq_p[%d]:tg_cid[%d]:tg_cid.l2cc[%d] push inv shared/exclusive l2cc queued\n",
			     did, rq_cid, tg_cid, (Uint)((c[rq_cid].l2rq[did].push_ADR/LINESIZE)%MAXL2BK));
		    d[did].l2dir_v_new0[0] &= ~1LL<<tg_cid;
		    d[did].l2dir_d_new0[0] &= ~1LL<<tg_cid;
		    d[did].l2dir_s_new0[0] &= ~1LL<<tg_cid;
		  }
		}
	      }
	    }
	    if (m[c[rq_cid].l2rq[did].push_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].v_stat) { /* mmccが空いていなければ止める */
	      if (flag & TRACE_ARM)
		printf("d%02.2d:MMCC rq_cid[%d]:l2rq->m[%d].mmcc[%d] push mmcc ful\n", did, rq_cid, (Uint)(c[rq_cid].l2rq[did].push_ADR/(MEMSIZE/MAXMMBK)), did);
	    }
	    else {
	      m[c[rq_cid].l2rq[did].push_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].t        = MMDELAY;
	      m[c[rq_cid].l2rq[did].push_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].rq_push  = c[rq_cid].l2rq[did].rq_push;
	      m[c[rq_cid].l2rq[did].push_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].push_ADR = c[rq_cid].l2rq[did].push_ADR;
	      m[c[rq_cid].l2rq[did].push_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].rq_pull  = 0;
	      for (l=0; l<LINESIZE/8; l++)
		m[c[rq_cid].l2rq[did].push_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].BUF[l] = c[rq_cid].l2rq[did].BUF[l];
	      m[c[rq_cid].l2rq[did].push_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].v_stat   = (3<<2)|(1); /* valid */
	      d[did].mmcc_req_bitmap[0] |= 1LL<<(c[rq_cid].l2rq[did].push_ADR/(MEMSIZE/MAXMMBK));
	      if (flag & TRACE_ARM)
		printf("d%02.2d:MMCC rq_cid[%d]:l2rq->m[%d].mmcc[%d] push mmcc queued\n", did, rq_cid, (Uint)(c[rq_cid].l2rq[did].push_ADR/(MEMSIZE/MAXMMBK)), did);
	      d[did].l2dir_v_new0[0] &= ~1LL<<rq_cid;
	      d[did].l2dir_d_new0[0] &= ~1LL<<rq_cid;
	      d[did].l2dir_s_new0[0] &= ~1LL<<rq_cid;
	      t[c[rq_cid].l2rq[did].tid].pa_g2mis++;
	    }
	  }
	  else if (d[did].l2d_state == L2DSTATE_EMPTY && c[rq_cid].l2rq[did].rq_pull) { /* PULL load/store */
	    d[did].l2dir_ent = c[rq_cid].l2rq[did].pull_ADR/LINESIZE/MAXL2BK;
	    d[did].l2dir_v_new0[0] = 0xffffffffffffffffLL>>(64-MAXCORE);
	    d[did].l2dir_d_new0[0] = 0xffffffffffffffffLL>>(64-MAXCORE);
	    d[did].l2dir_s_new0[0] = 0xffffffffffffffffLL>>(64-MAXCORE);
	    d[did].l2dir_v_new1[0] = 0x0000000000000000LL;
	    d[did].l2dir_d_new1[0] = 0x0000000000000000LL;
	    d[did].l2dir_s_new1[0] = 0x0000000000000000LL;
	    Ull exclusive =(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0] & ~(1LL<<rq_cid))
	                  &~d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]
	                  &~d[did].l2dir[d[did].l2dir_ent].l2dir_s[0];
	    Ull shared    =(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0] & ~(1LL<<rq_cid))
	                  &~d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]
	                  & d[did].l2dir[d[did].l2dir_ent].l2dir_s[0];
	    Ull modified  =(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0] & ~(1LL<<rq_cid))
	                  & d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]
	                  &~d[did].l2dir[d[did].l2dir_ent].l2dir_s[0];
	    Ull owned     =(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0] & ~(1LL<<rq_cid))
	                  & d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]
	                  & d[did].l2dir[d[did].l2dir_ent].l2dir_s[0];
	    if (flag & TRACE_ARM)
	      printf("d%02.2d:l2dir[%d]: pull rq_cid=%d pull_ADR=%08.8x v=%08.8x_%08.8x d=%08.8x_%08.8x s=%08.8x_%08.8x read\n",
		     did, d[did].l2dir_ent, rq_cid,
		     c[rq_cid].l2rq[did].pull_ADR,
		     (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0]),
		     (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]),
		     (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_s[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_s[0]));
	    /* l2rqからのpullはl2ccまたはmmccからの読み込み */
	    d[did].l2d_state = L2DSTATE_PULL;
	    /* まずl2ccにあるかどうかl2dirを確認 */
	    /* load の場合,                                                                                                */
	    /*             l2dir検査 tg_pidがあればpull+share要求   owned/modified      l2dir tg_pid該当bit: v=1 d=1 s=1   */
	    /*                                                      shared/exclusive    l2dir tg_pid該当bit: v=1 d=0 s=1   */
	    /*                                                                          l2dir rq_pid該当bit: v=1 d=0 s=1   */
	    /*                                                                          l2dir その他該当bit:  無変更       */
	    /*             l2dir無し mmccへpull要求                                     l2dir rq_pid該当bit: v=1 d=0 s=0   */
	    
	    /* storeの場合,                                                                                                */
	    /*             l2dir検査 tg_pidがあればpull+inv                             l2dir tg_pid該当bit: v=0 d=0 s=0   */
	    /*                                                                          l2dir rq_pid該当bit: v=1 d=1 s=0   */
	    /*                       その他の全sharedにもinv要求                        l2dir その他該当bit: v=0 d=0 s=0   */
	    /*             l2dir無し mmccへpull要求                                     l2dir rq_pid該当bit: v=1 d=1 s=0   */
	    if (c[rq_cid].l2rq[did].type == 3 && c[rq_cid].l2rq[did].opcd != 7) { /* load (other than ldrex) */
	      tg_cid = -1;
	      if (owned|modified) { /* dirty found */
		for (i=0; i<MAXCORE; i++) {
		  if ((owned|modified) & (1LL<<i)) {
		    if (tg_cid >= 0) {
		      printf("d%02.2d:rq_cid[%d]:load l2dir[%d].l2dir has multiple owned|modified v=%08.8x_%08.8x d=%08.8x_%08.8x s=%08.8x_%08.8x\n",
			     did, rq_cid, (Uint)(d[did].l2dir_ent),
			     (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0]),
			     (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]),
			     (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_s[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_s[0]));
		    }
		    tg_cid = i;
		  }
		}
		if (c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].v_stat) { /* l2ccが空いていなければ止める */
		  if (flag & TRACE_ARM)
		    printf("d%02.2d:L2CC rq_cid[%d]:tg_p[%d]:tg_cid.l2cc[%d] pull load owned/modified l2cc ful\n",
			   did, rq_cid, tg_cid, (Uint)((c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK));
		}
		else { /* l2ccにエンキュー */
		  c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].t            = CCDELAY;
		  c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].rq_pull      = 1;
		  c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].rq_inv_share = 1; /* load->share */
		  c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].target_ADR   = c[rq_cid].l2rq[did].pull_ADR;
		  c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].v_stat       = (3<<2)|(1); /* valid */
		  d[did].l2cc_req_bitmap[0] |= 1LL<<tg_cid;
		  if (flag & TRACE_ARM)
		    printf("d%02.2d:L2CC rq_p[%d]:tg_cid[%d]:tg_cid.l2cc[%d] pull load owned/modified l2cc queued target=%08.8x\n",
			   did, rq_cid, tg_cid, (Uint)((c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK),
			   c[rq_cid].l2rq[did].pull_ADR);
		  d[did].l2dir_v_new1[0] |=  1LL<<tg_cid;
		  d[did].l2dir_d_new1[0] |=  1LL<<tg_cid;
		  d[did].l2dir_s_new1[0] |=  1LL<<tg_cid;
		  d[did].l2dir_v_new1[0] |=  1LL<<rq_cid;
		  d[did].l2dir_d_new0[0] &= ~1LL<<rq_cid;
		  d[did].l2dir_s_new1[0] |=  1LL<<rq_cid;
		  t[c[rq_cid].l2rq[did].tid].pa_g2hit++;
		}
	      }
	      else if (shared|exclusive) { /* clean found */
		for (i=0; i<MAXCORE; i++) {
		  if ((shared|exclusive) & (1LL<<i)) {
		    tg_cid = i;
		    d[did].l2dir_v_new1[0] |=  1LL<<tg_cid;
		    d[did].l2dir_d_new0[0] &= ~1LL<<tg_cid;
		    d[did].l2dir_s_new1[0] |=  1LL<<tg_cid;
		  }
		}
		/* l2ccにエンキューすると,l2-cleanが追い出されている可能性があるので,mmccにエンキューする */
		/* ただし，share状態には遷移させる */
		if (m[c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].v_stat) { /* mmccが空いていなければ止める */
		  if (flag & TRACE_ARM)
		    printf("d%02.2d:MMCC rq_cid[%d]:l2rq->m[%d].mmcc[%d] pull load (shared/exclusive) mmcc ful\n", did, rq_cid, (Uint)(c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)), did);
		}
		else {
		  m[c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].t        = MMDELAY;
		  m[c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].rq_push  = 0;
		  m[c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].rq_pull  = 1;
		  m[c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].pull_ADR = c[rq_cid].l2rq[did].pull_ADR;
		  m[c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].v_stat   = (3<<2)|(1); /* valid */
		  d[did].mmcc_req_bitmap[0] |= 1LL<<(c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK));
		  if (flag & TRACE_ARM)
		    printf("d%02.2d:MMCC rq_p[%d]:l2rq->m[%d].mmcc[%d] pull load (shared/exclusive) mmcc queued target=%08.8x\n",
			   did, rq_cid, (Uint)(c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)), did,
			   c[rq_cid].l2rq[did].pull_ADR);
		  d[did].l2dir_v_new1[0] |=  1LL<<rq_cid;
		  d[did].l2dir_d_new0[0] &= ~1LL<<rq_cid;
		  d[did].l2dir_s_new1[0] |=  1LL<<rq_cid;
		  t[c[rq_cid].l2rq[did].tid].pa_g2mis++;
		}
	      }
	      else { /* l2dir not found */
		if (m[c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].v_stat) { /* mmccが空いていなければ止める */
		  if (flag & TRACE_ARM)
		    printf("d%02.2d:MMCC rq_cid[%d]:l2rq->m[%d].mmcc[%d] pull load mmcc ful\n", did, rq_cid, (Uint)(c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)), did);
		}
		else {
		  m[c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].t        = MMDELAY;
		  m[c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].rq_push  = 0;
		  m[c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].rq_pull  = 1;
		  m[c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].pull_ADR = c[rq_cid].l2rq[did].pull_ADR;
		  m[c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].v_stat   = (3<<2)|(1); /* valid */
		  d[did].mmcc_req_bitmap[0] |= 1LL<<(c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK));
		  if (flag & TRACE_ARM)
		    printf("d%02.2d:MMCC rq_p[%d]:l2rq->m[%d].mmcc[%d] pull load mmcc queued pull_ADR=%08.8x\n",
			   did, rq_cid, (Uint)(c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)), did,
			   c[rq_cid].l2rq[did].pull_ADR);
		  d[did].l2dir_v_new1[0] |=  1LL<<rq_cid;
		  d[did].l2dir_d_new0[0] &= ~1LL<<rq_cid;
		  d[did].l2dir_s_new0[0] &= ~1LL<<rq_cid;
		  t[c[rq_cid].l2rq[did].tid].pa_g2mis++;
		}
	      }
	    }
	    else { /* store */
	      tg_cid = -1;
	      if (owned|modified) { /* dirty found */
		for (i=0; i<MAXCORE; i++) {
		  if ((owned|modified) & (1LL<<i)) {
		    if (tg_cid >= 0) {
		      printf("d%02.2d:rq_cid[%d]:store l2dir[%d].l2dir has multiple owned|modified v=%08.8x_%08.8x d=%08.8x_%08.8x s=%08.8x_%08.8x\n",
			     did, rq_cid, (Uint)(d[did].l2dir_ent),
			     (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0]),
			     (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]),
			     (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_s[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_s[0]));
		    }
		    tg_cid = i;
		  }
		}
		if (c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].v_stat) { /* l2ccが空いていなければ止める */
		  if (flag & TRACE_ARM)
		    printf("d%02.2d:L2CC rq_cid[%d]:tg_cid[%d]:tg_cid.l2cc[%d] pull store owned/modified l2cc ful\n",
			   did, rq_cid, tg_cid, (Uint)((c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK));
		}
		else { /* l2ccにエンキュー */
		  c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].t            = CCDELAY;
		  c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].rq_pull      = 1;
		  c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].rq_inv_share = 0; /* store->inv */
		  c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].target_ADR   = c[rq_cid].l2rq[did].pull_ADR;
		  c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].v_stat       = (3<<2)|(1); /* valid */
		  d[did].l2cc_req_bitmap[0] |= 1LL<<tg_cid;
		  d[did].l2dir_v_new0[0] &= ~1LL<<tg_cid;
		  d[did].l2dir_d_new0[0] &= ~1LL<<tg_cid;
		  d[did].l2dir_s_new0[0] &= ~1LL<<tg_cid;
		  d[did].l2dir_v_new1[0] |=  1LL<<rq_cid;
		  d[did].l2dir_d_new1[0] |=  1LL<<rq_cid;
		  d[did].l2dir_s_new0[0] &= ~1LL<<rq_cid;
		  t[c[rq_cid].l2rq[did].tid].pa_g2hit++;
#if 0
		  /* shareが信用できないケースがあるので,l2filは変更しない */
		  if (d[did].l2dir[d[did].l2dir_ent].l2dir_v[0] & (1LL<<rq_cid))
		    c[rq_cid].l2rq[did].rv_l2fil = 0; /* l2_fill不要 */
		  else
		    c[rq_cid].l2rq[did].rv_l2fil = 1; /* always l2_fill from l2rq.BUF[] */
#endif
		  if (flag & TRACE_ARM)
		    printf("d%02.2d:L2CC rq_cid[%d]:tg_cid[%d]:tg_cid.l2cc[%d] pull store owned/modified l2cc queued target=%08.8x l2fil=%d\n",
			   did, rq_cid, tg_cid, (Uint)((c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK),
			   c[rq_cid].l2rq[did].pull_ADR, c[rq_cid].l2rq[did].rv_l2fil);
		}
	      }
	      else if (!c[rq_cid].l2rq[did].rv_l2fil && (d[did].l2dir[d[did].l2dir_ent].l2dir_v[0] & (1LL<<rq_cid))) { /* rq_cid has valid line */
		/* l2dirではsharedであるが，直前の入れ換え時clean追い出しによりl2tagに有効ラインがない場合がある．この時l2rqからrv_l2fil=1を要求 */
		if (flag & TRACE_ARM)
		  printf("d%02.2d:L2CC rq_cid[%d] can reuse l2line target=%08.8x\n",
			 did, rq_cid, c[rq_cid].l2rq[did].pull_ADR);
		d[did].l2dir_v_new1[0] |=  1LL<<rq_cid;
		d[did].l2dir_d_new1[0] |=  1LL<<rq_cid;
		d[did].l2dir_s_new0[0] &= ~1LL<<rq_cid;
	      }
	      else { /* rq_cid has no valid dirty line */
		if (m[c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].v_stat) { /* mmccが空いていなければ止める */
		  if (flag & TRACE_ARM)
		    printf("d%02.2d:MMCC rq_cid[%d]:l2rq->m[%d].mmcc[%d] pull store mmcc ful\n", did, rq_cid, (Uint)(c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)), did);
		}
		else {
		  m[c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].t        = MMDELAY;
		  m[c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].rq_push  = 0;
		  m[c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].rq_pull  = c[rq_cid].l2rq[did].rq_pull;
		  m[c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].pull_ADR = c[rq_cid].l2rq[did].pull_ADR;
		  m[c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)].mmcc[did].v_stat   = (3<<2)|(1); /* valid */
		  d[did].mmcc_req_bitmap[0] |= 1LL<<(c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK));
		  if (flag & TRACE_ARM)
		    printf("d%02.2d:MMCC rq_cid[%d]:l2rq->m[%d].mmcc[%d] pull store mmcc queued target=%08.8x\n",
			   did, rq_cid, (Uint)(c[rq_cid].l2rq[did].pull_ADR/(MEMSIZE/MAXMMBK)), did,
			   c[rq_cid].l2rq[did].pull_ADR);
		  d[did].l2dir_v_new1[0] |=  1LL<<rq_cid;
		  d[did].l2dir_d_new1[0] |=  1LL<<rq_cid;
		  d[did].l2dir_s_new0[0] &= ~1LL<<rq_cid;
		  t[c[rq_cid].l2rq[did].tid].pa_g2mis++;
		}
	      }
	      if (shared|exclusive) { /* if other cache has clean line */
		for (i=0; i<MAXCORE; i++) { /* invalidate other caches */
		  if ((shared|exclusive) & (1LL<<i)) {
		    tg_cid = i;
		    if (c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].v_stat) { /* l2ccが空いていなければ止める */
		      if (flag & TRACE_ARM)
			printf("d%02.2d:L2CC rq_cid[%d]:tg_cid[%d]:tg_cid.l2cc[%d] pull inv store shared/exclusive l2cc ful\n",
			       did, rq_cid, tg_cid, (Uint)((c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK));
		    }
		    else { /* l2ccにエンキュー */
		      c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].t            = CCDELAY;
		      c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].rq_pull      = 0;
		      c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].rq_inv_share = 0; /* store->inv */
		      c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].target_ADR   = c[rq_cid].l2rq[did].pull_ADR;
		      c[tg_cid].l2cc[(c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK].v_stat       = (3<<2)|(1); /* valid */
		      d[did].l2cc_req_bitmap[0] |= 1LL<<tg_cid;
		      if (flag & TRACE_ARM)
			printf("d%02.2d:L2CC rq_p[%d]:tg_cid[%d]:tg_cid.l2cc[%d] pull inv store shared/exclusive l2cc queued target=%08.8x\n",
			       did, rq_cid, tg_cid, (Uint)((c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK),
			       c[rq_cid].l2rq[did].pull_ADR);
		      d[did].l2dir_v_new0[0] &= ~1LL<<tg_cid;
		      d[did].l2dir_d_new0[0] &= ~1LL<<tg_cid;
		      d[did].l2dir_s_new0[0] &= ~1LL<<tg_cid;
		    }
		  }
		}
	      }
	    }
	  }
	}    
      }
    }
    /*========================================================================================================*/
    /* 各BANK付随のl2ccを走査してsim-coreの応答を回収↓ */
    /* 各BANK付随のmmccを走査してsim-clusterの応答を回収↓ */
    /* l2cc要求＆応答待ちが残っていれば */
    /*========================================================================================================*/
    if (d[did].l2d_state == L2DSTATE_DRAIN) { /* l2cc_drainの場合 */
      rq_cid = d[did].root_cid;
      if (d[did].l2cc_req_bitmap[0]) {
	for (tg_cid=0; tg_cid<MAXCORE; tg_cid++) {
	  /* l2cc要求後の新規応答tg_pidを抽出 */
	  if ((d[did].l2cc_req_bitmap[0] & (1LL<<tg_cid)) && d[did].l2cc_ack_bitmap[tg_cid])
	    break;
	}
	if (tg_cid < MAXCORE) { /* done */
	  d[did].l2cc_req_bitmap[0] &= ~(1LL<<tg_cid); /* ACKに対応する元要求を消去 */
	  d[did].l2cc_ack_bitmap[tg_cid] = 0;
	  c[tg_cid].l2cc[did].v_stat = 0;
	  if (flag & TRACE_ARM)
	    printf("d%02.2d:l2cc_ack tg_cid=%d\n", did, tg_cid);
	}
      }
      if (d[did].mmcc_req_bitmap[0]) {
	for (tg_mid=0; tg_mid<MAXMMBK; tg_mid++) {
	  /* mmcc要求後の新規応答tg_midを抽出 */
	  if ((d[did].mmcc_req_bitmap[0] & (1LL<<tg_mid)) && d[did].mmcc_ack_bitmap[tg_mid])
	    break;
	}
	if (tg_mid < MAXMMBK) { /* done */
	  d[did].mmcc_req_bitmap[0] &= ~(1LL<<tg_mid); /* ACKに対応する元要求を消去 */
	  d[did].mmcc_ack_bitmap[tg_mid] = 0;
	  m[tg_mid].mmcc[did].v_stat = 0;
	  if (flag & TRACE_ARM)
	    printf("d%02.2d:mmcc_ack tg_pid=%d\n", did, tg_mid);
	}
      }
      if (!d[did].l2cc_req_bitmap[0] && !d[did].mmcc_req_bitmap[0]) { /* 全てのL2/MMcc-requestを回収したら */
	/* !!!ここでl2dir更新!!! */
	if (flag & TRACE_ARM)
	  printf("d%02.2d:l2dir[%d]: v=%08.8x_%08.8x d=%08.8x_%08.8x s=%08.8x_%08.8x ->\n",
		 did, d[did].l2dir_ent, 
		 (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0]),
		 (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]),
		 (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_s[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_s[0]));
	d[did].l2dir[d[did].l2dir_ent].l2dir_v[0] &= d[did].l2dir_v_new0[0];
	d[did].l2dir[d[did].l2dir_ent].l2dir_d[0] &= d[did].l2dir_d_new0[0];
	d[did].l2dir[d[did].l2dir_ent].l2dir_s[0] &= d[did].l2dir_s_new0[0];
	d[did].l2dir[d[did].l2dir_ent].l2dir_v[0] |= d[did].l2dir_v_new1[0];
	d[did].l2dir[d[did].l2dir_ent].l2dir_d[0] |= d[did].l2dir_d_new1[0];
	d[did].l2dir[d[did].l2dir_ent].l2dir_s[0] |= d[did].l2dir_s_new1[0];
	if (flag & TRACE_ARM)
	  printf("d%02.2d:l2dir[%d]: v=%08.8x_%08.8x d=%08.8x_%08.8x s=%08.8x_%08.8x\n",
		 did, d[did].l2dir_ent, 
		 (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0]),
		 (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]),
		 (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_s[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_s[0]));

	if (!c[rq_cid].l2rq[did].rq_pull) {
	  c[rq_cid].l2rq[did].rv_share = 0; /* sim-coreでは無視 */
	  c[rq_cid].l2rq[did].v_stat = (c[rq_cid].l2rq[did].v_stat&0xc)|2; /* writeback OK */
	  if (flag & TRACE_ARM)
	    printf("d%02.2d:rq_cid[%d]:UNLOCKED L2/MMcc push+no_pull completed\n", did, rq_cid);
	  d[did].l2rq_bitmap[rq_cid] = 0;
	  d[did].l2rq_lock[0] &= ~(1LL<<rq_cid); /* unlock d[].l2rq */
	  d[did].rq_cid_first_search = (rq_cid+1)%MAXCORE;
	}
	else if (c[rq_cid].l2rq[did].rq_push) {
	  c[rq_cid].l2rq[did].rq_push = 0;       /* PUSH completed */
	  c[rq_cid].l2rq[did].t       = L2DIRDL; /* PUSH completed */
	  if (flag & TRACE_ARM)
	    printf("d%02.2d:rq_cid[%d]:L2/MMcc push completed (continue pull)\n", did, rq_cid);
	  /* pushとpullが同一l2dirの場合,l2r_bitmapを1のままにしておく */
	  if (((c[rq_cid].l2rq[did].push_ADR/LINESIZE)%MAXL2BK) != ((c[rq_cid].l2rq[did].pull_ADR/LINESIZE)%MAXL2BK)) {
	    d[did].l2rq_bitmap[rq_cid] = 0;
	    d[did].l2rq_lock[0] &= ~(1LL<<rq_cid); /* unlock d[].l2rq */
	    d[did].rq_cid_first_search = (rq_cid+1)%MAXCORE;
	  }
	}
	d[did].l2d_state = L2DSTATE_EMPTY;
      }
    }
    else if (d[did].l2d_state == L2DSTATE_PULL) { /* l2cc_pullの場合 */
      rq_cid = d[did].root_cid;
      if (d[did].l2cc_req_bitmap[0]) {
	for (tg_cid=0; tg_cid<MAXCORE; tg_cid++) {
	  /* l2cc要求後の新規応答tg_cidを抽出 */
	  if ((d[did].l2cc_req_bitmap[0] & (1LL<<tg_cid)) && d[did].l2cc_ack_bitmap[tg_cid])
	    break;
	}
	if (tg_cid < MAXCORE) { /* done */
	  d[did].l2cc_req_bitmap[0] &= ~(1LL<<tg_cid); /* ACKに対応する元要求を消去 */
	  d[did].l2cc_ack_bitmap[tg_cid] = 0;
	  c[tg_cid].l2cc[did].v_stat = 0;
	  if (c[tg_cid].l2cc[did].rq_pull) { /* if pull data exists */
	    for (l=0; l<LINESIZE/8; l++) {
	      c[rq_cid].l2rq[did].BUF[l] = c[tg_cid].l2cc[did].BUF[l]; /* copy BUF */
	      if (flag & TRACE_ARM)
		printf("d%02.2d:rq_cid[%d]:tg_cid[%d]:l2cc pull response A=%08.8x->%08.8x_%08.8x\n",
		       did, rq_cid, tg_cid, c[rq_cid].l2rq[did].pull_ADR+l*8,
		       (Uint)(c[rq_cid].l2rq[did].BUF[l]>>32), (Uint)(c[rq_cid].l2rq[did].BUF[l]));
	    }
	  }
	}
      }
      if (d[did].mmcc_req_bitmap[0]) {
	for (tg_mid=0; tg_mid<MAXMMBK; tg_mid++) {
	  /* mmcc要求後の新規応答tg_midを抽出 */
	  if ((d[did].mmcc_req_bitmap[0] & (1LL<<tg_mid)) && d[did].mmcc_ack_bitmap[tg_mid])
	    break;
	}
	if (tg_mid < MAXMMBK) { /* done */
	  d[did].mmcc_req_bitmap[0] &= ~(1LL<<tg_mid); /* ACKに対応する元要求を消去 */
	  d[did].mmcc_ack_bitmap[tg_mid] = 0;
	  m[tg_mid].mmcc[did].v_stat = 0;
	  if (m[tg_mid].mmcc[did].rq_pull) { /* if pull data exists */
	    for (l=0; l<LINESIZE/8; l++) {
	      c[rq_cid].l2rq[did].BUF[l] = m[tg_mid].mmcc[did].BUF[l]; /* copy BUF */
	      if (flag & TRACE_ARM)
		printf("d%02.2d:rq_cid[%d]:tg_m[%d]:mmcc pull response A=%08.8x->%08.8x_%08.8x\n",
		       did, rq_cid, tg_mid, c[rq_cid].l2rq[did].pull_ADR+l*8,
		       (Uint)(c[rq_cid].l2rq[did].BUF[l]>>32), (Uint)(c[rq_cid].l2rq[did].BUF[l]));
	    }
	  }
	}
      }
      if (!d[did].l2cc_req_bitmap[0] && !d[did].mmcc_req_bitmap[0]) { /* 全てのL2/MMcc-requestを回収したら */
	/* !!!ここでl2dir更新!!! */
	if (flag & TRACE_ARM)
	  printf("d%02.2d:l2dir[%d]: v=%08.8x_%08.8x d=%08.8x_%08.8x s=%08.8x_%08.8x ->\n",
		 did, d[did].l2dir_ent, 
		 (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0]),
		 (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]),
		 (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_s[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_s[0]));
	d[did].l2dir[d[did].l2dir_ent].l2dir_v[0] &= d[did].l2dir_v_new0[0];
	d[did].l2dir[d[did].l2dir_ent].l2dir_d[0] &= d[did].l2dir_d_new0[0];
	d[did].l2dir[d[did].l2dir_ent].l2dir_s[0] &= d[did].l2dir_s_new0[0];
	d[did].l2dir[d[did].l2dir_ent].l2dir_v[0] |= d[did].l2dir_v_new1[0];
	d[did].l2dir[d[did].l2dir_ent].l2dir_d[0] |= d[did].l2dir_d_new1[0];
	d[did].l2dir[d[did].l2dir_ent].l2dir_s[0] |= d[did].l2dir_s_new1[0];
	if (flag & TRACE_ARM)
	  printf("d%02.2d:l2dir[%d]: v=%08.8x_%08.8x d=%08.8x_%08.8x s=%08.8x_%08.8x\n",
		 did, d[did].l2dir_ent, 
		 (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_v[0]),
		 (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_d[0]),
		 (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_s[0]>>32), (Uint)(d[did].l2dir[d[did].l2dir_ent].l2dir_s[0]));

	if (d[did].l2dir[c[rq_cid].l2rq[did].pull_ADR/LINESIZE/MAXL2BK].l2dir_s[0] & (1LL<<rq_cid))
	  c[rq_cid].l2rq[did].rv_share = 1;
	else
	  c[rq_cid].l2rq[did].rv_share = 0;
	c[rq_cid].l2rq[did].v_stat = (c[rq_cid].l2rq[did].v_stat&0xc)|3; /* IF/OP準備OK */
	if (flag & TRACE_ARM)
	  printf("d%02.2d:rq_cid[%d]:UNLOCKED L2/MMcc pull completed rv_share=%d pull_A=%08.8x\n",
		 did, rq_cid, c[rq_cid].l2rq[did].rv_share, c[rq_cid].l2rq[did].pull_ADR);
	d[did].l2rq_bitmap[rq_cid] = 0;
	d[did].l2rq_lock[0] &= ~(1LL<<rq_cid); /* unlock d[].l2rq */
	d[did].rq_cid_first_search = (rq_cid+1)%MAXCORE;
	d[did].l2d_state = L2DSTATE_EMPTY;
      }
    }
  }
}
