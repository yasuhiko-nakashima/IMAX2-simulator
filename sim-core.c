
static char RcsHeader[] = "$Header: /usr/home/nakashim/proj-arm64/src/csim/RCS/sim-core.c,v 1.48 2018/11/15 15:22:44 nakashim Exp nakashim $";

/* ARM Simulator                       */
/*         Copyright (C) 2010 by NAIST */
/*         Primary writer: Y.Nakashima */
/*                nakashim@is.naist.jp */

/* sim-core.c 2010/7/10 */ 

#define DISASM
#include "csim.h"
#undef DISASM

/* ���������o1���Ȥξ�� */
/*  d=0,s=0: load  -> o1-hit   -- none --     */
/*  d=0,s=1: load  -> o1-hit   -- none --     */
/*  d=1,s=0: load  -> o1-hit   -- none --     */
/*  d=1,s=1: load  -> o1-hit   -- none --     */
/*  d=0,s=0: store -> o1-hit l1rq_push=0 pull=0 l2rq_push=0 pull=0 ccrq=no  l2=dirty l1st l1=dirty,!shared */
/*  d=0,s=1: store -> o1-hit l1rq_push=0 pull=0 l2rq_push=0 pull=0 ccrq=inv l2=dirty l1st l1=dirty,!shared */
/*  d=1,s=0: store -> o1-hit   -- none --     */
/*  d=1,s=1: store -> o1-hit l1rq_push=0 pull=0 l2rq_push=0 pull=0 ccrq=inv l2=dirty l1st l1=dirty,!shared */

/*  d=x,s=x: load  -> o1-mis l1rq_push=X pull=1 �ɽ���l2ɬ��hit(��) ���θ�l1rq_pull(load) �б�(�̾��l2rq) */
/*  d=x,s=x: store -> o1-mis l1rq_push=X pull=1 �ɽ���l2ɬ��hit(��) ���θ�l1rq_pull(store)�б�(�̾��l2rq) */

/* o1miss,dirty��l2���ɤ��Ф����(l2w��ɬ��hit���뤬ccrq��ɬ�פʾ�礬����) */
/*  d=0,s=0: store -> l2-hit                    l2rq_push=0 pull=0 ccrq=no  l2=dirty l1st l1=dirty,!shared */
/*  d=0,s=1: store -> l2-hit                    l2rq_push=0 pull=0 ccrq=inv l2=dirty l1st l1=dirty,!shared */
/*  d=1,s=0: store -> l2-hit   -- none --     */
/*  d=1,s=1: store -> l2-hit                    l2rq_push=0 pull=0 ccrq=inv l2=dirty l1st l1=dirty,!shared */

/* Ĺ���������󥹤ϡ�
 o1-store(!dirty|share) stat=1(pullADRͭ��)     ->l1rq(push=0,pull=0)                   ->l2rq(push=0,pull=0)->l2cc(inv)->l2rqend->l1rqend->o1st
 o1-store(miss)         stat=2(push+pullADRͭ��)->l1rq(push=1,pull=1)->l2w(store stat=1)->l2rq(push=0,pull=0)->l2cc(inv)->l2rqend(o1rqBUF>l2write l1rq.push=0)
                                                      (push=0,pull=1)->l2r(store stat=1)->l2rq(push=0,pull=0)->l2cc(inv)->l2rqend(o1rqBUF>o1write l1rqend)
 o1-store(miss)         stat=2(push+pullADRͭ��)->l1rq(push=1,pull=1)->l2w(store stat=1)->l2rq(push=0,pull=0)->l2cc(inv)->l2rqend(o1rqBUF>l2write l1rq.push=0)
                                                      (push=0,pull=1)->l2r(store stat=2)->l2rq(push=1,pull=1)->l2cc(inv)/mmcc(push)->l2cc/mmcc(pull)->l2rqend(l2rqBUF>l2write l1rqend)
*/

/* ��������pull�Τ����¾l2��dirty���ɤ��Ф���� */
/*  l2��hit��������l2���ɤ��Ф� */
/*  o1��hit��������o1���񤭤����ɤ��Ф� */

/* ��������λ���l2cc�ξ�� */
/*  o1��hit��������o1���ǿ� */
/*  l2��hit��������l2���ǿ� */

/*  my        my        other   */
/*  l1v d s   l2v d s   l2v d s                              LOAD                                                 |v(!dirty|sha)  v(!dirty|sha)  STORE                                              */
/*    0 * *     0 * *     0 * *  l1(pul+l1fil) l2(pul+l2fil)                  mmcc(pull) l2rq -> l2 -> l1rq -> l1 | l1(pul+l1fil)  l2(pul+l2fil) l2dir(dirty)   mmcc(pull) l2rq -> l2 -> l1rq -> l1 */
/*    0 * *     0 * *     1 0 0  l1(pul+l1fil) l2(pul+l2fil) l2cc(share)      mmcc(pull) l2rq -> l2 -> l1rq -> l1 | l1(pul+l1fil)  l2(pul+l2fil) l2cc(inv)      mmcc(pull) l2rq -> l2 -> l1rq -> l1 */
/*    0 * *     0 * *     1 0 1  l1(pul+l1fil) l2(pul+l2fil) l2cc(share)      mmcc(pull) l2rq -> l2 -> l1rq -> l1 | l1(pul+l1fil)  l2(pul+l2fil) l2cc(inv)      mmcc(pull) l2rq -> l2 -> l1rq -> l1 */
/*    0 * *     0 * *     1 1 0  l1(pul+l1fil) l2(pul+l2fil) l2cc(pull+share)            l2rq -> l2 -> l1rq -> l1 | l1(pul+l1fil)  l2(pul+l2fil) l2cc(pull+inv)            l2rq -> l2 -> l1rq -> l1 */
/*    0 * *     0 * *     1 1 1  l1(pul+l1fil) l2(pul+l2fil) l2cc(pull+share)            l2rq -> l2 -> l1rq -> l1 | l1(pul+l1fil)  l2(pul+l2fil) l2cc(pull+inv)            l2rq -> l2 -> l1rq -> l1 */

/*    0 * *     1 0 0     0 * *  l1(pul+l1fil) hit                                       l2rq -> l2 -> l1rq -> l1 | l1(pul+l1fil) *l2(pul)       l2dir(ditry)              l2rq -> l2 -> l1rq -> l1 */
/*    0 * *     1 0 1     0 * *  l1(pul+l1fil) hit                                       l2rq -> l2 -> l1rq -> l1 | l1(pul+l1fil) *l2(pul)       l2cc(inv)                 l2rq -> l2 -> l1rq -> l1 */
/*    0 * *     1 0 1     1 0 1  l1(pul+l1fil) hit                                       l2rq -> l2 -> l1rq -> l1 | l1(pul+l1fil) *l2(pul)       l2cc(inv)                 l2rq -> l2 -> l1rq -> l1 */
/*    0 * *     1 0 1     1 1 1  l1(pul+l1fil) hit                                       l2rq -> l2 -> l1rq -> l1 | l1(pul+l1fil) *l2(pul)       l2cc(inv)v=1�ʤ�l2fil���� l2rq -> l2 -> l1rq -> l1 */
/*    0 * *     1 1 0     0 * *  l1(pul+l1fil) hit                                       l2rq -> l2 -> l1rq -> l1 | l1(pul+l1fil)  hit                                     l2rq -> l2 -> l1rq -> l1 */
/*    0 * *     1 1 1     0 * *  l1(pul+l1fil) hit                                       l2rq -> l2 -> l1rq -> l1 | l1(pul+l1fil) *l2(pul)       l2cc(inv)                 l2rq -> l2 -> l1rq -> l1 */
/*    0 * *     1 1 1     1 0 1  l1(pul+l1fil) hit                                       l2rq -> l2 -> l1rq -> l1 | l1(pul+l1fil) *l2(pul)       l2cc(inv)                 l2rq -> l2 -> l1rq -> l1 */

/*    1 0 0     1 0 0     0 * *  hit                                                                              |*l1(pul)        l2(pul)       l2dir(dirty)              l2rq -> l2 -> l1rq -> l1 */
/*    1 0 1     1 0 1     0 * *  hit                                                                              |*l1(pul)        l2(pul)       l2cc(inv)                 l2rq -> l2 -> l1rq -> l1 */
/*    1 0 1     1 0 1     1 0 1  hit                                                                              |*l1(pul)        l2(pul)       l2cc(inv)                 l2rq -> l2 -> l1rq -> l1 */
/*    1 0 1     1 0 1     1 1 1  hit                                                                              |*l1(pul)        l2(pul)       l2cc(inv)v=1�ʤ�l2fil���� l2rq -> l2 -> l1rq -> l1 */
/*    1 1 0     1 1 0     0 * *  hit                                                                              | hit                                                                             */
/*    1 1 1     1 1 1     0 * *  hit                                                                              |*l1(pul)        l2(pul)       l2cc(inv)                 l2rq -> l2 -> l1rq -> l1 */
/*    1 1 1     1 1 1     1 0 1  hit                                                                              |*l1(pul)        l2(pul)       l2cc(inv)v=1�ʤ�l2fil���� l2rq -> l2 -> l1rq -> l1 */

int  monitor_mask = 0x000f0000;
int  monitor_step = 0x00010000;
int  monitor_addr = 0x00000000;

void *sim_core(param) Uint param;
{ 
  Uint  cid = param >> 16, tid;
  int   cycle = param & 0xffff;
  Uint  l1bank, l2bank, insn, a, o;
  Ull   m[2], membuf[2], D0[2];
  int   index, fmin, i, j, k, l, w, hit1rq, hit1, hit2rq, hit2;
  int   rv_share, pull_way, req_pull, rq_push, rq_pull, stat, inhibit_replace;
  Uint  push_adr;

  while (cycle-- > 0) {
    /**************************************************************************************/
    /* MONITOR                                                                            */
    /**************************************************************************************/

#if 0
if ((Uint)(t[7].total_steps) >= 0xbd856) { /* 600000 */
  flag |= TRACE_ARM;
  flag |= TRACE_EMAX;
}
if ((Uint)(t[7].total_steps) >= 0xbd85b) { /* 600000 */
  flag &= ~TRACE_ARM;
  flag &= ~TRACE_EMAX;
}
#endif

    if (!cid) {
      tid = 0;
      if (((Uint)(t[tid].total_steps) & monitor_mask) == monitor_addr) {
	printf("Monitor-%03.3d:%08.8x_%08.8x %08.8x ",
	       tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), t[tid].ib.pc);
	monitor_addr = (monitor_addr+monitor_step)&monitor_mask;
	printf("IPC=%5.3f ROB=", (double)t[tid].pa_steps/(double)(t[tid].pa_cycle+1));
	for (i=c[cid].rob_bot; i!=c[cid].rob_top; i=(i+1)%CORE_ROBSIZE) {
	  printf("%d:%d ", c[cid].rob[i].tid, c[cid].rob[i].stat);
	}
	printf("\n");
      }
      if (flag & TRACE_RANGE) {
	if ((Uint)(t[tid].total_steps>>32) >= 0x00000000) {
	  if ((Uint)(t[tid].total_steps) >= trace_on) { /* 600000 */
	    flag |= TRACE_ARM;
	    flag |= TRACE_EMAX;
	  }
	  if ((Uint)(t[tid].total_steps) >= trace_off) { /* 800000 */
	    flag &= ~TRACE_ARM;
	    flag &= ~TRACE_EMAX;
	  }
	}
      }
    }
    /**************************************************************************************/
    /* EMAX6 siml                                                                         */
    /**************************************************************************************/
    siml_emax6(cid, flag & TRACE_EMAX, flag & TRACE_PIPE);
    /**************************************************************************************/
    /* ������å�/core�ξ��ִƻ�, ������ˣ��ĤǤ�ARM_FLUSH��¸�ߤ�����,FLUSH����ǰ     */
    /**************************************************************************************/
    for (tid=cid; tid<MAXTHRD; tid+=MAXCORE) {
      if (t[tid].status == ARM_FLUSH || t[tid].status == ARM_EXCSVC)
	break;
    }
    if (tid<MAXTHRD) { /* found */
      for (i=cid; i<MAXTHRD; i+=MAXCORE) {
	t[i].total_cycle++;        /* ��L1flush �����Ȳû� */
	t[i].pa_cycle++;           /* ��L1flush �����Ȳû� */
	t[i].pa_svcL1flushcycle++; /* ��L1flush �����Ȳû� */
      }
    }
    else
      goto skip_to_core_management;

    if (t[tid].status == ARM_FLUSH) { /* some ARM_FLUSH in progress */
      for (i=0; i<MAXL1BK; i++) {
	if (c[cid].l1rq[i].v_stat) {
	  if (flag & TRACE_ARM)
	    printf("%03.3d:FL %08.8x_%08.8x FLUSH waiting for l1rq finished\n",
		   tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps));
	  goto skip_to_cache_management;
	}
      }  
      for (i=0; i<MAXL2BK; i++) {
	if (c[cid].l2rq[i].v_stat) {
	  if (flag & TRACE_ARM)
	    printf("%03.3d:FL %08.8x_%08.8x FLUSH waiting for l2rq finished\n",
		   tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps));
	  goto skip_to_cache_management;
	}
      }  
      if (c[cid].system_counter < D1SIZE/LINESIZE) {
	Uint addr = c[cid].system_counter*LINESIZE;
	switch (o_flush(tid, addr, t[tid].svc_keep_or_drain)) { /* all-entry by flush_counter */
	case 0: /* normal_end */
	  if (flag & TRACE_ARM)
	    printf("%03.3d:FL %08.8x_%08.8x FLUSH counter=%d/%d invalidate clean line A=%08.8x\n",
		   tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), c[cid].system_counter, D1SIZE/LINESIZE, addr);
	  c[cid].system_counter++;
	  break;
	case 1: /* mem_wait(queue-full) */
	  t[tid].pa_d1waitcycle++;
	  if (flag & TRACE_ARM)
	    printf("%03.3d:FL WAIT\n", tid);
	  break;
	case 2: /* mem_wait(queue-ok) */
	  if (flag & TRACE_ARM)
	    printf("%03.3d:FL %08.8x_%08.8x FLUSH counter=%d/%d delayed(miss) queued l1rq A=%08.8x\n",
		   tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), c[cid].system_counter, D1SIZE/LINESIZE, addr);
	  c[cid].system_counter++;
	  break;
	}
      }
      else {
	for (i=0; i<MAXL1BK; i++) {
	  if (c[cid].l1rq[i].v_stat)
	    break;
	}
	if (i<MAXL1BK) {
	  if (flag & TRACE_ARM)
	    printf("%03.3d:FL L1RQ WAIT\n", tid);
	}
	else {
	  if (flag & TRACE_ARM)
	    printf("%03.3d:FL %08.8x_%08.8x FLUSH_END -> SVC restart\n",
		   tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps));
	  /* ���λ����Ǥ�,L1-flush�Τߴ�λ��SVC������L2-flush��ɬ�� */
	  if (t[tid].svc_keep_or_drain) {
	    for (i=0; i<MAXL1BK; i++) {
	      for (j=0; j<D1WMAXINDEX; j++) {
		for (k=0; k<D1WAYS; k++) {
		  if (c[cid].d1tag[i][j][k].v)
		    printf("%03.3d:FL %08.8x_%08.8x VALID L1 remains in d1tag[%d][%d][%d] (malfunction)\n",
			   tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), i, j, k);
		}
	      }
	    }
	  }
	  t[tid].status = ARM_EXCSVC;
	}
      }
      goto skip_to_cache_management;
    }
    else if (t[tid].status == ARM_EXCSVC) { /* some ARM_EXCSVC in progress */
      goto skip_to_cache_management;
    }
  skip_to_core_management:
    /**************************************************************************************/
    /* ������å�/core�ξ��ִƻ�, ARM_FLUSH��¸�ߤ��ʤ����,�̾�ư��                      */
    /**************************************************************************************/
    for (tid=cid; tid<MAXTHRD; tid+=MAXCORE) {
      t[tid].total_cycle++;
      t[tid].pa_cycle++;
      switch (t[tid].status) {
      case ARM_NORMAL:
	t[tid].hangup_counter = 0;
	break;
      case ARM_COMPLETE:
      case ARM_STOP:
	if (t[tid].o_stat != t[tid].status) {
	  printf("\033[7m%03.3d:STOP/COMPLETE steps/cycle=%08.8x_%08.8x/%08.8x_%08.8x\033[0m\n",
		 tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), (Uint)(t[tid].total_cycle>>32), (Uint)(t[tid].total_cycle));
	  fflush(stdout);
	  /* t[tid].ib.status, ib.mc, ib.pc ���� */
	  /* ����tid��IB�õ� */
	  /* ROB���tid����ȥ�õ� */
#if 1
	  t[tid].ib.status = IB_EMPTY;
	  /* reset map */
	  for (i=0; i<MAXLREG; i++)
	    t[tid].map[i].x = 0;
	  /* flush fifo */
	  for (i=0; i<CORE_IMLPIPE; i++) {
	    if (c[cid].imlpipe[i].tid == tid)
	      c[cid].imlpipe[i].v = 0;
	  }
	  for (i=0; i<CORE_VECPIPE; i++) {
	    if (c[cid].vecpipe[i].tid == tid)
	      c[cid].vecpipe[i].v = 0;
	  }
	  if (c[cid].divque.tid == tid)
	    c[cid].divque.v = 0;
	  /* reset rob */
	  for (i=c[cid].rob_bot; i!=c[cid].rob_top; i=(i+1)%CORE_ROBSIZE) {
	    if (c[cid].rob[i].tid == tid) /* mapped and waiting */
	      c[cid].rob[i].stat = ROB_EMPTY;
	  }
#endif
#if 0
	  /* reset l1rq */
	  /* ARM_STOP�������Ǥ�,��������$��¾���������¤����뤿��õ���Բ� */
	  for (l1bank=0; l1bank<MAXL1BK; l1bank++) {
	    if (tid == c[cid].l1rq[l1bank].tid) {
	      c[cid].l1rq[l1bank].v_stat = 0;
	      c[cid].l1rq[l1bank].rq_push = 0;
	      c[cid].l1rq[l1bank].rq_pull = 0;
	    }
	  }
	  /* reset l2rq */
	  for (l2bank=0; l2bank<MAXL2BK; l2bank++) {
	    if (tid == c[cid].l2rq[l2bank].tid) {
	      c[cid].l2rq[l2bank].v_stat = 0;
	      c[cid].l2rq[l2bank].rq_push = 0;
	      c[cid].l2rq[l2bank].rq_pull = 0;
	      d[l2bank].l2rq_bitmap[cid] = 0; /* �ɤ߹����襯�饹�� */
	    }
	  }
#endif
	}
	break;
      }
      t[tid].o_stat = t[tid].status;
    }
    /**************************************************************************************/
    /* (i$hit or i$mis+enq)/core                                                          */
    /**************************************************************************************/
    for (tid=c[cid].if_nexttid,i=0; i<THR_PER_CORE; tid=(tid+MAXCORE)%MAXTHRD,i++) {
      if (t[tid].status == ARM_NORMAL && t[tid].ib.status == IB_EMPTY) {
	switch (o_ifetch(tid, t[tid].ib.pc, &insn, &t[tid].ib)) {
	case 0: /* i1hit normal end */
	  if (flag & TRACE_ARM) {
	    printf("%03.3d:IF %08.8x_%08.8x %08.8x [%08.8x]\n", tid, 
		   (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), t[tid].ib.pc, insn);
	  }
	  t[tid].pa_i1hit++;
	  t[tid].ib.status = IB_VALID;
	  t[tid].ib.mc     = 0;
	  t[tid].ib.insn   = insn;
	  goto exit_ifqueue;
	case 1: /* l1mis l1rq busy */
	  if (flag & TRACE_ARM) {
	    printf("%03.3d:IF %08.8x_%08.8x %08.8x l1rq busy\n", tid, 
		   (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), t[tid].ib.pc);
	  }
	  t[tid].pa_i1waitcycle++;
	  continue;
	case 2: /* i1mis l1rq enqueued */
	  if (flag & TRACE_ARM) {
	    printf("%03.3d:IF %08.8x_%08.8x %08.8x l1rq enqueued\n", tid, 
		   (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), t[tid].ib.pc);
	  }
	  t[tid].pa_i1mis++;
	  t[tid].ib.status = IB_ISSUED;
	  goto exit_ifqueue;
	case 3: /* error (ouf of range) */
	  if (flag & TRACE_ARM) {
	    printf("%03.3d:IF %08.8x_%08.8x %08.8x address out of range\n", tid, 
		   (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), t[tid].ib.pc);
	  }
	  t[tid].ib.status = IB_VALID;
	  t[tid].ib.mc     = 0;
	  t[tid].ib.insn   = 0xd41fffe1; /* undefined/SVC */
	  goto exit_ifqueue;
	}
      }
    }
  exit_ifqueue:
    c[cid].if_nexttid = (c[cid].if_nexttid + MAXCORE) % MAXTHRD;
    /**************************************************************************************/
    /* rob-enq/core                                                                       */
    /**************************************************************************************/
    for (tid=c[cid].rob_nexttid,i=0; i<THR_PER_CORE; tid=(tid+MAXCORE)%MAXTHRD,i++) {
      if (t[tid].status == ARM_NORMAL && t[tid].ib.status == IB_VALID) {
	if ((c[cid].rob_top+1)%CORE_ROBSIZE != c[cid].rob_bot) { /* entry available */
	  /* LDM/STM is divided into multiple ROB entries */
	  /* enqueue one ROB/cycle */
	  i = c[cid].rob_top;
	  switch (insn_decode(tid, t[tid].ib.mc, t[tid].ib.pc, t[tid].ib.insn, &c[cid].rob[i])) {
	  case 0: /* normal end */
	    if (flag & TRACE_ARM) {
	      printf("%03.3d:DE %08.8x_%08.8x %08.8x [%08.8x] %c [cond=%1x:updt=%1x] rob[%d].stat=%d sr=%d%d%d%d%d%d dr=%d%d%d %6s\n",
		     tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), c[cid].rob[i].pc,
		     t[tid].ib.insn,
		     c[cid].rob[i].term ? '*':'-',
		     c[cid].rob[i].cond, c[cid].rob[i].updt,
		     i,
		     c[cid].rob[i].stat,
		     c[cid].rob[i].sr[5].t, c[cid].rob[i].sr[4].t, c[cid].rob[i].sr[3].t,
		     c[cid].rob[i].sr[2].t, c[cid].rob[i].sr[1].t, c[cid].rob[i].sr[0].t,
		     c[cid].rob[i].dr[3].t, c[cid].rob[i].dr[2].t, c[cid].rob[i].dr[1].t,
		     adis[c[cid].rob[i].type<<4|c[cid].rob[i].opcd]);
	    }
	    c[cid].rob_top = (c[cid].rob_top+1)%CORE_ROBSIZE;
	    t[tid].ib.status = IB_EMPTY;
	    if (c[cid].rob[i].bpred)
	      t[tid].ib.pc = c[cid].rob[i].target; /* architecture TARGET (for ifetch) */
	    else
	      t[tid].ib.pc += 4; /* architecture NPC (for ifetch) */
	    break;
	  case 1: /* continue (LDM/STM) */
	    c[cid].rob_top = (c[cid].rob_top+1)%CORE_ROBSIZE;
	    t[tid].ib.mc++;
	    break;
	  }
	  goto exit_robqueue;
	}
      }
    }
  exit_robqueue:
    c[cid].rob_nexttid = (c[cid].rob_nexttid + MAXCORE) % MAXTHRD;
    /**************************************************************************************/
    /* check write buffer and write back to ROB                                           */
    /**************************************************************************************/
    if (c[cid].imlpipe[CORE_IMLPIPE-1].v) {
      tid = c[cid].imlpipe[CORE_IMLPIPE-1].tid;
      if (flag & TRACE_ARM)
	printf("%03.3d:IML delayed arrived to reg", tid);
      if (c[cid].imlpipe[CORE_IMLPIPE-1].dr1t)
	ex_drw(tid, 0LL, c[cid].imlpipe[CORE_IMLPIPE-1].dr1v, c[cid].imlpipe[CORE_IMLPIPE-1].rob, 1);
      if (flag & TRACE_ARM)
	printf("\n");
      c[cid].imlpipe[CORE_IMLPIPE-1].rob->stat = ROB_COMPLETE;
    }
    for (i=CORE_IMLPIPE-1; i>0; i--) c[cid].imlpipe[i] = c[cid].imlpipe[i-1]; c[cid].imlpipe[0].v = 0;
    if (c[cid].vecpipe[CORE_VECPIPE-1].v) {
      tid = c[cid].vecpipe[CORE_VECPIPE-1].tid;
      if (flag & TRACE_ARM)
	printf("%03.3d:VEC delayed arrived to reg", tid);
      if (c[cid].vecpipe[CORE_VECPIPE-1].dr1t)
	ex_drw(tid, c[cid].vecpipe[CORE_VECPIPE-1].dr1v[1], c[cid].vecpipe[CORE_VECPIPE-1].dr1v[0], c[cid].vecpipe[CORE_VECPIPE-1].rob, 1);
      if (c[cid].vecpipe[CORE_VECPIPE-1].dr3t)
	ex_drw(tid, 0LL, (Ull)c[cid].vecpipe[CORE_VECPIPE-1].dr3v, c[cid].vecpipe[CORE_VECPIPE-1].rob, 3);
      if (flag & TRACE_ARM)
	printf("\n");
      c[cid].vecpipe[CORE_VECPIPE-1].rob->stat = ROB_COMPLETE;
    }
    for (i=CORE_VECPIPE-1; i>0; i--) c[cid].vecpipe[i] = c[cid].vecpipe[i-1]; c[cid].vecpipe[0].v = 0;
    if (c[cid].divque.v && c[cid].divque.t==0) {
      tid = c[cid].divque.tid;
      if (flag & TRACE_ARM)
	printf("%03.3d:DIV delayed arrived to reg", tid);
      if (c[cid].divque.dr1t)
	ex_drw(tid, 0LL, c[cid].divque.dr1v, c[cid].divque.rob, 1);
      if (flag & TRACE_ARM)
	printf("\n");
      c[cid].divque.rob->stat = ROB_COMPLETE;
      c[cid].divque.v = 0;
    }
    if (c[cid].divque.v && c[cid].divque.t)
      c[cid].divque.t--;
    /**************************************************************************************/
    /* rob-deq/core and exec                                                              */
    /**************************************************************************************/
    for (i=c[cid].rob_bot; i!=c[cid].rob_top; i=(i+1)%CORE_ROBSIZE) {
      if (c[cid].rob[i].stat == ROB_MAPPED) { /* mapped and waiting */
	tid = c[cid].rob[i].tid;
	if (c[cid].rob[i].sr[0].t == 3 || c[cid].rob[i].sr[1].t == 3 || c[cid].rob[i].sr[2].t == 3
	 || c[cid].rob[i].sr[3].t == 3 || c[cid].rob[i].sr[4].t == 3 || c[cid].rob[i].sr[5].t == 3)
	  continue;
	if (flag & TRACE_ARM) {
	  printf("%03.3d:EX %08.8x_%08.8x %08.8x %c [cond=%1x:updt=%1x] rob[%d].stat=%d sr=%d%d%d%d%d%d dr=%d%d%d %6s ",
		 tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), c[cid].rob[i].pc,
		 c[cid].rob[i].term ? '*':'-',
		 c[cid].rob[i].cond, c[cid].rob[i].updt,
		 i,
		 c[cid].rob[i].stat,
		 c[cid].rob[i].sr[5].t, c[cid].rob[i].sr[4].t, c[cid].rob[i].sr[3].t,
		 c[cid].rob[i].sr[2].t, c[cid].rob[i].sr[1].t, c[cid].rob[i].sr[0].t,
		 c[cid].rob[i].dr[3].t, c[cid].rob[i].dr[2].t, c[cid].rob[i].dr[1].t,
		 adis[c[cid].rob[i].type<<4|c[cid].rob[i].opcd]);
	}
	insn_exec(c[cid].rob[i].tid, &c[cid].rob[i]);
	if (flag & TRACE_ARM)
	  printf("\n");
	goto exit_robexec;
      }
    }
  exit_robexec:
    /**************************************************************************************/
    /* dump map                                                                           */
    /**************************************************************************************/
#if 0
    if (flag & TRACE_ARM) {
      if (c[cid].rob_bot != c[cid].rob_top) {
	for (tid=cid; tid<MAXTHRD; tid+=MAXCORE) {
	  printf("t%03.3d:MAP", tid);
	  for (i=0; i<MAXLREG; i++)
	    printf(" %d.%d", t[tid].map[i].x, t[tid].map[i].rob);
	  printf("\n");
	}
      }
    }
#endif
    /**************************************************************************************/
    /* dump rob                                                                           */
    /**************************************************************************************/
    if (flag & TRACE_ARM) {
      if (c[cid].rob_bot != c[cid].rob_top) {
	printf("c%02.2d:RB", cid);
	for (i=c[cid].rob_bot; i!=c[cid].rob_top; i=(i+1)%CORE_ROBSIZE) {
	  printf(" %d[t%d:%d]", i, c[cid].rob[i].tid, c[cid].rob[i].stat);
	}
	printf("\n");
      }
    }
    /**************************************************************************************/
    /* retire/core                                                                        */
    /**************************************************************************************/
    while (c[cid].rob_bot != c[cid].rob_top && c[cid].rob[c[cid].rob_bot].stat == ROB_EMPTY) /* valid rob exists */
      c[cid].rob_bot = (c[cid].rob_bot+1)%CORE_ROBSIZE;
    if (c[cid].rob_bot != c[cid].rob_top) {
      i = c[cid].rob_bot;
      tid = c[cid].rob[i].tid;
      if (c[cid].rob[i].stat >= ROB_COMPLETE && c[cid].rob[i].stat != ROB_D1WRQWAIT) { /* valid rob exists */
	if (flag & TRACE_ARM) {
	  printf("%03.3d:RT %08.8x_%08.8x %08.8x %c [cond=%1x:updt=%1x] %6s",
		 tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), c[cid].rob[i].pc,
		 c[cid].rob[i].term ? '*':'-',
		 c[cid].rob[i].cond, c[cid].rob[i].updt, adis[c[cid].rob[i].type<<4|c[cid].rob[i].opcd]);
	}
	for (j=1; j<4; j++) {
	  if (c[cid].rob[i].dr[j].t) {
	    rt_drw(tid, c[cid].rob[i].dr[j].n, c[cid].rob[i].dr[j].val);
#if 0
	    if (c[cid].rob[i].dr[j].n == 15 ) { /* update PC */
	      c[cid].rob[i].target = c[cid].rob[i].dr[j].val[0];
	      c[cid].rob[i].stat = ROB_RESTART;
	    }
	    else
#endif
	    { /* normal reg */
	      for (k=i; k!=c[cid].rob_top; k=(k+1)%CORE_ROBSIZE) {
		for (l=0; l<6; l++) {
		  if (c[cid].rob[k].sr[l].t==2 && c[cid].rob[k].sr[l].x == j && c[cid].rob[k].sr[l].n == c[cid].rob[i].dr[j].p) {
		    c[cid].rob[k].sr[l].x = 0; /* lreg */
		    c[cid].rob[k].sr[l].n = c[cid].rob[i].dr[j].n; /* lregno */
		  }
		}
	      }
	    }
	    if (t[tid].map[c[cid].rob[i].dr[j].n].rob == c[cid].rob[i].dr[j].p) /* map.rob�ֹ椬��rob�ֹ��Ʊ�����Τ�map��ꥻ�åȲ�ǽ */
	      t[tid].map[c[cid].rob[i].dr[j].n].x = 0; /* reset map */
	  }
	}
	if (flag & TRACE_ARM)
	  printf("\n");
      }
      if (c[cid].rob[i].stat >= ROB_STREXWAIT && c[cid].rob[i].stbf.t) { /* valid stbf exists */
	if (c[cid].rob[i].stat == ROB_STREXWAIT) {
	  for (j=0; j<MAXL1BK; j++) {
	    if (c[cid].l1rq[j].v_stat && c[cid].l1rq[j].tid == tid)
	      break;
	  }
	} /* strex����l1rq��äޤ��Ե� */
      	if ((c[cid].rob[i].stat == ROB_STREXWAIT && j == MAXL1BK) || c[cid].rob[i].stbf.t == 1) {
	  
	  /***for EMAX6-I/O****/
	  if (MEMSIZE <= c[cid].rob[i].ls_addr) { /* I/O space */
	    /*emax6_ctl(tid, 4, opcd, addr, mask, val); type=4:store */
	    if (c[cid].iorq.v_stat)
	      c[cid].rob[i].stat = ROB_D1WRQWAIT;
	    else {
	      c[cid].iorq.tid  = c[cid].rob[i].tid;
	      c[cid].iorq.type = c[cid].rob[i].type; /* load/store/ldstub */
	      c[cid].iorq.opcd = c[cid].rob[i].opcd; /* OP-fetch */
	      c[cid].iorq.ADR  = c[cid].rob[i].ls_addr;
	      c[cid].iorq.BUF[0] = c[cid].rob[i].stbf.val[0]; /* for store */
	      c[cid].iorq.BUF[1] = c[cid].rob[i].stbf.val[1]; /* for store */
	      c[cid].iorq.rob = NULL;
	      c[cid].iorq.v_stat = (3<<2)|(1); /* valid */
	      c[cid].rob[i].stat = ROB_COMPLETE;
	    }
	  }
	  /***for EMAX6-I/O****/

	  else { /* normal space */
	    /* try rob[].stbf->d1w() */
	    l1bank = (c[cid].rob[i].ls_addr/LINESIZE)%MAXL1BK;
	    if (c[cid].l1rq[l1bank].v_stat || c[cid].rob[i].stat == ROB_STREXWAIT) /* strex */
	      inhibit_replace = 1;
	    else
	      inhibit_replace = 0;
	    if (flag & TRACE_ARM) {
	      printf("%03.3d:RT %08.8x_%08.8x %08.8x %c [cond=%1x:updt=%1x] %6s:A=%08.8x,M=%08.8x_%08.8x_%08.8x_%08.8x<-%08.8x_%08.8x_%08.8x_%08.8x",
		     tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), c[cid].rob[i].pc,
  		     c[cid].rob[i].term ? '*':'-',
		     c[cid].rob[i].cond, c[cid].rob[i].updt, adis[c[cid].rob[i].type<<4|c[cid].rob[i].opcd],
		     c[cid].rob[i].ls_addr,
		     (Uint)(c[cid].rob[i].stbf.mask[1]>>32), (Uint)c[cid].rob[i].stbf.mask[1],
		     (Uint)(c[cid].rob[i].stbf.mask[0]>>32), (Uint)c[cid].rob[i].stbf.mask[0],
		     (Uint)(c[cid].rob[i].stbf.val[1]>>32), (Uint)c[cid].rob[i].stbf.val[1],
		     (Uint)(c[cid].rob[i].stbf.val[0]>>32), (Uint)c[cid].rob[i].stbf.val[0]);
	    }
	    stat = d1w(tid, c[cid].rob[i].opcd, c[cid].rob[i].ls_addr, c[cid].rob[i].stbf.mask, 0, 0, c[cid].rob[i].stbf.val, &pull_way, &rq_push, &push_adr, inhibit_replace?NULL:c[cid].l1rq[l1bank].BUF); /* D1-cache */
	    /* strex�ξ��,d1w��l1rq����Ѥ����������rd=0,�۾����rd=1�Ȥ���ľ���˽�λ */
	    if (c[cid].rob[i].stat == ROB_STREXWAIT) { /* strex */
	      if (flag & TRACE_ARM)
		printf(":strex %s", stat?"FAILED":"OK");
	      ex_drw(tid, 0LL, (Ull)(stat?1:0), &c[cid].rob[i], 2);
	      rt_drw(tid, c[cid].rob[i].dr[2].n, c[cid].rob[i].dr[2].val);
	      for (k=i; k!=c[cid].rob_top; k=(k+1)%CORE_ROBSIZE) {
		for (l=0; l<6; l++) {
		  if (c[cid].rob[k].sr[l].t==2 && c[cid].rob[k].sr[l].x == 2 && c[cid].rob[k].sr[l].n == c[cid].rob[i].dr[2].p) {
		    c[cid].rob[k].sr[l].x = 0; /* lreg */
		    c[cid].rob[k].sr[l].n = c[cid].rob[i].dr[2].n; /* lregno */
		  }
		}
	      }
	      if (t[tid].map[c[cid].rob[i].dr[2].n].rob == c[cid].rob[i].dr[2].p) /* map.rob�ֹ椬��rob�ֹ��Ʊ�����Τ�map��ꥻ�åȲ�ǽ */
		t[tid].map[c[cid].rob[i].dr[2].n].x = 0; /* reset map */
	      c[cid].rob[i].stat = ROB_RESTART;
	    }
	    else { /* !strex */
	      if (stat) rq_pull = 1; /* request l2/mm data (includes stat==1) */
	      else      rq_pull = 0; /* use l1-data */
	      if (stat == 2 || stat == 1) { /* d1r:miss d1w:miss|!dirty|shared || DDR-miss */
		if (inhibit_replace) { /* busy */
		  if (flag & TRACE_ARM)
		    printf(" D1W D1RQWAIT stat=%d A=%08.8x STBF=%08.8x_%08.8x_%08.8x_%08.8x",
			   stat, c[cid].rob[i].ls_addr,
			   (Uint)(c[cid].rob[i].stbf.val[1]>>32), (Uint)c[cid].rob[i].stbf.val[1],
			   (Uint)(c[cid].rob[i].stbf.val[0]>>32), (Uint)c[cid].rob[i].stbf.val[0]);
		  c[cid].rob[i].stat = ROB_D1WRQWAIT;
		  t[tid].pa_d1waitcycle++;
		}
		else {
		  if (flag & TRACE_ARM)
		    printf(" D1W ENQUEUED stat=%d A=%08.8x STBF=%08.8x_%08.8x_%08.8x_%08.8x",
			   stat, c[cid].rob[i].ls_addr,
			   (Uint)(c[cid].rob[i].stbf.val[1]>>32), (Uint)c[cid].rob[i].stbf.val[1],
			   (Uint)(c[cid].rob[i].stbf.val[0]>>32), (Uint)c[cid].rob[i].stbf.val[0]);
		  c[cid].l1rq[l1bank].rv_l1fil = (stat==2); /* stat==1:l1reuse */
		  c[cid].l1rq[l1bank].t    = D1DELAY;
		  c[cid].l1rq[l1bank].tid  = c[cid].rob[i].tid;
		  c[cid].l1rq[l1bank].type = c[cid].rob[i].type; /* load/store/ldstub */
		  c[cid].l1rq[l1bank].opcd = c[cid].rob[i].opcd; /* OP-fetch */
		  c[cid].l1rq[l1bank].rq_push = rq_push;   /* miss:repl_d !dirty|shared:0 */
		  c[cid].l1rq[l1bank].push_ADR = push_adr;
		  c[cid].l1rq[l1bank].rq_pull = rq_pull;   /* miss:pull !dirty|shared:1(�������פ����ץ�ȥ����ñ���Τ���) */
		  c[cid].l1rq[l1bank].pull_ADR = c[cid].rob[i].ls_addr;
		  c[cid].l1rq[l1bank].pull_way = pull_way;
		  c[cid].l1rq[l1bank].STD[0] = c[cid].rob[i].stbf.val[0]; /* for store */
		  c[cid].l1rq[l1bank].STD[1] = c[cid].rob[i].stbf.val[1]; /* for store */
		  c[cid].l1rq[l1bank].ib = NULL; /* not used */
		  c[cid].l1rq[l1bank].rob = NULL;
		  c[cid].l1rq[l1bank].v_stat = (3<<2)|(1); /* valid */
		  /* L1:index=10bit,linesize=6bit,L2:bank=2bit,blocksize=12bit�ʤΤ�,L1��Ʊ��饤���L2��Ʊ��Х󥯤ˤʤ� */
		  /* ���Τ��ᡤpush_ADR��pull_ADR��Ʊ��Х󥯤Ȥʤ�Τǡ�bitmap������1bit�����Ǥ褤 */
		  c[cid].rob[i].stat = ROB_COMPLETE;
		  t[tid].pa_d1mis++;
		}
	      }
	      else {
		if (flag & TRACE_ARM)
		  printf(" D1W COMPLETED");
#if 1
    {
      Uint l2bank = (c[cid].rob[i].ls_addr/LINESIZE)%MAXL2BK;
      Uint index = (c[cid].rob[i].ls_addr/LINESIZE/MAXL2BK)%L2WMAXINDEX;
      int w;
      for (w=0; w<L2WAYS; w++) {
	if (c[cid].l2tag[l2bank][index][w].v && c[cid].l2tag[l2bank][index][w].la == c[cid].rob[i].ls_addr/L2TAGMASK) { /* hit */
	  break;
	}
      }
      if (flag & TRACE_ARM)
	printf(":d1w L2 includes l2bank=%d index=%d w=%d A=%08.8x", l2bank, index, w, c[cid].rob[i].ls_addr);
    }
#endif
	        c[cid].rob[i].stat = ROB_COMPLETE;
		t[tid].pa_d1hit++;
	      }
	    }
	    if (flag & TRACE_ARM)
	      printf("\n");
	  }
	}
      }

      switch (c[cid].rob[i].stat) {
      case ROB_EMPTY:
      case ROB_MAPPED:
      case ROB_ISSUED:
      case ROB_STREXWAIT:
	break;
      case ROB_COMPLETE: /* normal retire */
	if (c[cid].rob[i].term) {
	  t[tid].total_steps++;
	  t[tid].pa_steps++;
	}
	c[cid].rob[i].stat = ROB_EMPTY;
	c[cid].rob_bot = (c[cid].rob_bot+1)%CORE_ROBSIZE;
	break;
      case ROB_D1WRQWAIT: /* store->L1 pending */
	if (flag&TRACE_ARM)
	  printf("%03.3d:RT %08.8x_%08.8x %08.8x PENDING rob.stbf->D1W\n",
		 tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), c[cid].rob[i].pc);
	break;
      case ROB_RESTART: /* restart */
	/* t[tid].ib.status, ib.mc, ib.pc ���� */
	/* ����tid��IB�õ� */
	/* ROB���tid����ȥ�õ� */
	if (c[cid].rob[i].type == 6) { /* SVC/EMAX */
	  t[tid].svc_opcd = c[cid].rob[i].sr[1].n;
	  t[tid].svc_keep_or_drain = chck_svc(tid, t[tid].svc_opcd);
	  t[tid].status = ARM_FLUSH;
	  c[cid].system_counter = 0;
	}
	else if (c[cid].rob[i].type == 7) { /* PTHREAD */
	  t[tid].svc_opcd = c[cid].rob[i].sr[1].n;
	  t[tid].status = ARM_PTHREAD;
	}
	if (flag&TRACE_ARM)
	  printf("%03.3d:RT %08.8x_%08.8x %08.8x RESTARTING\n", 
		 tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), c[cid].rob[i].pc);
	if (c[cid].rob[i].term) {
	  t[tid].total_steps++;
	  t[tid].pa_steps++;
	}
	t[tid].ib.mc     = 0;
	t[tid].ib.pc     = c[cid].rob[i].target;
#if 1
	t[tid].ib.status = IB_EMPTY;
	/* reset map */
	for (i=0; i<MAXLREG; i++)
	  t[tid].map[i].x = 0;
	/* flush fifo */
	for (i=0; i<CORE_IMLPIPE; i++) {
	  if (c[cid].imlpipe[i].tid == tid)
	    c[cid].imlpipe[i].v = 0;
	}
	for (i=0; i<CORE_VECPIPE; i++) {
	  if (c[cid].vecpipe[i].tid == tid)
	    c[cid].vecpipe[i].v = 0;
	}
	if (c[cid].divque.tid == tid)
	  c[cid].divque.v = 0;
	/* reset rob */
	for (i=c[cid].rob_bot; i!=c[cid].rob_top; i=(i+1)%CORE_ROBSIZE) {
	  if (c[cid].rob[i].tid == tid) /* mapped and waiting */
	    c[cid].rob[i].stat = ROB_EMPTY;
	}
#endif
	break;
      case ROB_DECERR: /* error */
	printf("%03.3d:RT %08.8x_%08.8x %08.8x rob[%d].stat=DECERR\n",
	       tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), c[cid].rob[i].pc, i);
	t[tid].status = ARM_STOP;
	break;
      case ROB_EXECERR: /* error */
	printf("%03.3d:RT %08.8x_%08.8x %08.8x rob[%d].stat=EXECERR\n",
	       tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), c[cid].rob[i].pc, i);
	t[tid].status = ARM_STOP;
	break;
      default:
	printf("%03.3d:RT %08.8x_%08.8x %08.8x unexpected rob[%d].stat=%d\n",
	       tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), c[cid].rob[i].pc, i, c[cid].rob[i].stat);
	t[tid].status = ARM_STOP;
	break;
      }
    }
  skip_to_cache_management:
#if 0
    if (flag & TRACE_ARM) {
      printf("===:L2 l2bank=49 index=11 w=0 -> v=%d dirty=%d la=%08.8x 0e32bc48=%08.8x_%08.8x\n",
	     c[1].l2tag[49][11][0].v,
	     c[1].l2tag[49][11][0].dirty,
	     c[1].l2tag[49][11][0].la*L2TAGMASK|((11*MAXL2BK+49)*LINESIZE),
	     (Uint)(c[1].l2line[49][11][0].d[1]>>32),
	     (Uint)c[1].l2line[49][11][0].d[1]);
      printf("===:L2 l2bank=49 index=11 w=2 -> v=%d dirty=%d la=%08.8x 0e32bc48=%08.8x_%08.8x\n",
	     c[1].l2tag[49][11][2].v,
	     c[1].l2tag[49][11][2].dirty,
	     c[1].l2tag[49][11][2].la*L2TAGMASK|((11*MAXL2BK+49)*LINESIZE),
	     (Uint)(c[1].l2line[49][11][2].d[1]>>32),
	     (Uint)c[1].l2line[49][11][2].d[1]);
    }
#endif
    /**************************************************************************************/
    /* IOrq->rob����                                                                      */
    /**************************************************************************************/
    tid = c[cid].iorq.tid;
    switch (c[cid].iorq.v_stat&3) { /* stat */
    case 0: /* do nothing */
      break;
    case 1: /* busy */
      break;
    case 2: /* axi */
      break;
    case 3: /* RD->iorq completed */
      if (c[cid].iorq.type == 3) { /* load */
	struct rob *rob;
	rob = c[cid].iorq.rob;
	if (rob->stat != ROB_ISSUED || c[cid].iorq.ADR != rob->ls_addr) { /* mismatch by restarting */
	  if (flag & TRACE_ARM)
	    printf("%03.3d:IOR %08.8x_%08.8x %08.8x arrived but rejected (ib->pc=%08.8x)\n",
		   tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps),
		   c[cid].iorq.ADR, rob->ls_addr);
	}
	else { /* ROB_ISSUED && address_match */
	  /* SC�Ϲ������ʤ�        */
	  /* CC�Ϲ������ʤ�        */
	  /* DR�Ϲ���              */
	  switch (c[cid].iorq.opcd) {
	  case 2: /* LDRW */
	    D0[0] = c[cid].iorq.BUF[0];
	    if (rob->dr[1].t) ex_drw(c[cid].iorq.tid, 0LL, D0[0], rob, 1);
	    if (rob->dr[2].t) ex_drw(c[cid].iorq.tid, 0LL, rob->dr[2].val[0], rob, 2); /* update base_addr */
	    rob->stat = ROB_COMPLETE; /* done */
	    break;
	  case 3: /* LDR */
	    D0[0] = c[cid].iorq.BUF[0];
	    if (rob->dr[1].t) ex_drw(c[cid].iorq.tid, 0LL, D0[0], rob, 1);
	    if (rob->dr[2].t) ex_drw(c[cid].iorq.tid, 0LL, rob->dr[2].val[0], rob, 2); /* update base_addr */
	    rob->stat = ROB_COMPLETE; /* done */
	    break;
	  case 12: /* VLDRQ */
	    D0[0] = c[cid].iorq.BUF[0];
	    D0[1] = c[cid].iorq.BUF[1];
	    if (rob->dr[1].t) ex_drw(c[cid].iorq.tid, D0[1], D0[0], rob, 1);
	    if (rob->dr[2].t) ex_drw(c[cid].iorq.tid, 0LL, rob->dr[2].val[0], rob, 2); /* update base_addr */
	    rob->stat = ROB_COMPLETE; /* done */
	    break;
	  }
	}
	c[cid].iorq.v_stat = 0;
      }
      else {
	printf("%03.3d:IOx not implemented A=%08.8x\n",
	       tid, (c[cid].iorq.ADR&~(LINESIZE-1))+i*8);
      }
      break;
    }
    /**************************************************************************************/
    /* L1rq->L1����/��l1bank                                                              */
    /**************************************************************************************/
    for (l1bank=0; l1bank<MAXL1BK; l1bank++) {
      tid = c[cid].l1rq[l1bank].tid;
      switch (c[cid].l1rq[l1bank].v_stat&3) { /* stat */
      case 0: /* do nothing */
	break;
      case 1: /* L1rq busy */
	break;
      case 3: /* L2->l1rq(IF1 load) completed L2����ǡ�������,i1�ؽ񤭹��� */
	/* �ºݤˤ�,tid����äƤ�,�ɤ�thread��������äƤ�褤 */
	/* l1rq.buf[]����i1line.d[]��ž�� */
	index = (c[cid].l1rq[l1bank].pull_ADR/LINESIZE/MAXL1BK)%I1WMAXINDEX;
	c[cid].i1tag[l1bank][index][c[cid].l1rq[l1bank].pull_way].v   = 1;
	c[cid].i1tag[l1bank][index][c[cid].l1rq[l1bank].pull_way].lru = 127; /* reset lru */
	c[cid].i1tag[l1bank][index][c[cid].l1rq[l1bank].pull_way].la  = c[cid].l1rq[l1bank].pull_ADR/I1TAGMASK;
	/* i1�إ��ԡ� */
	for (l=0; l<LINESIZE/8; l++) {
	  c[cid].i1line[l1bank][index][c[cid].l1rq[l1bank].pull_way].d[l] = c[cid].l1rq[l1bank].BUF[l];
	  if (flag & TRACE_ARM)
	    printf("%03.3d:I1 delayed(miss) arrived l1rq->i1 A=%08.8x->%08.8x_%08.8x\n",
		   tid, (c[cid].l1rq[l1bank].pull_ADR&~(LINESIZE-1))+l*8,
		   (Uint)(c[cid].l1rq[l1bank].BUF[l]>>32), (Uint)c[cid].l1rq[l1bank].BUF[l]);
	}
	if (t[tid].status == ARM_STOP || c[cid].l1rq[l1bank].pull_ADR != c[cid].l1rq[l1bank].ib->pc) { /* mismatch by restarting */
	  if (flag & TRACE_ARM)
	    printf("%03.3d:I1 %08.8x_%08.8x %08.8x arrived but rejected (ib->pc=%08.8x)\n", tid, 
		   (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps),
		   c[cid].l1rq[l1bank].pull_ADR, c[cid].l1rq[l1bank].ib->pc);
	}
	else {
	  c[cid].l1rq[l1bank].ib->status = IB_VALID;
	  c[cid].l1rq[l1bank].ib->mc     = 0;
	  c[cid].l1rq[l1bank].ib->insn   = c[cid].l1rq[l1bank].BUF[(c[cid].l1rq[l1bank].pull_ADR&(LINESIZE-1))>>3]>>((c[cid].l1rq[l1bank].pull_ADR&4)<<3);
	  if (flag & TRACE_ARM) {
	    printf("%03.3d:I1 %08.8x_%08.8x %08.8x [%08.8x] arrived \n", tid, 
		   (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps),
		   c[cid].l1rq[l1bank].ib->pc, c[cid].l1rq[l1bank].ib->insn);
	  }
	}
	c[cid].l1rq[l1bank].v_stat = 0;
	break;
      case 2: /* L2->l1rq(OP1 load/store) completed L2����ǡ�������,o1�ؽ񤭹��� */
	if (c[cid].l1rq[l1bank].type == 0) {
	  /* o_flush�ξ��,O1���������� */
	}
	else if (c[cid].l1rq[l1bank].type == 3) { /* pull & load */
	  struct rob *rob;
	  /* l1rq.d[]����d1line.d[]��ž�� */
	  index = (c[cid].l1rq[l1bank].pull_ADR/LINESIZE/MAXL1BK)%D1WMAXINDEX;
	  c[cid].d1tag[l1bank][index][c[cid].l1rq[l1bank].pull_way].v     = 1;
	  c[cid].d1tag[l1bank][index][c[cid].l1rq[l1bank].pull_way].lru   = 127; /* reset lru */
	  c[cid].d1tag[l1bank][index][c[cid].l1rq[l1bank].pull_way].dirty = (c[cid].l1rq[l1bank].opcd==7); /* clean (other than ldrex) */
	  c[cid].d1tag[l1bank][index][c[cid].l1rq[l1bank].pull_way].share = c[cid].l1rq[l1bank].rv_share; /* sim-mreq�λؼ��˽��� */
	  c[cid].d1tag[l1bank][index][c[cid].l1rq[l1bank].pull_way].drain = 0; /* reset */
	  c[cid].d1tag[l1bank][index][c[cid].l1rq[l1bank].pull_way].la    = c[cid].l1rq[l1bank].pull_ADR/D1TAGMASK;
	  if (c[cid].l1rq[l1bank].opcd == 7) /* ldrex */
	    c[cid].d1tag[l1bank][index][c[cid].l1rq[l1bank].pull_way].thr_per_core = tid/MAXCORE; /* reset thr_per_core */
	  if (flag & TRACE_ARM)
	    printf("%03.3d:D1R delayed(miss) pull+load arrived l1rq->o1 dirty=%d share=%d A=%08.8x\n",
		   tid, (c[cid].l1rq[l1bank].opcd==7), c[cid].l1rq[l1bank].rv_share,
		       (c[cid].l1rq[l1bank].pull_ADR&~(LINESIZE-1)));
	  /* o1�إ��ԡ� */
	  if (c[cid].l1rq[l1bank].rv_l1fil) {
	    for (l=0; l<LINESIZE/8; l++) {
	      c[cid].d1line[l1bank][index][c[cid].l1rq[l1bank].pull_way].d[l] = c[cid].l1rq[l1bank].BUF[l];
	      if (flag & TRACE_ARM)
		printf("%03.3d:D1R delayed(miss) arrived l1rq->o1 rv_share=%d index=%d way=%d A=%08.8x->%08.8x_%08.8x\n",
		       tid, c[cid].l1rq[l1bank].rv_share, index, c[cid].l1rq[l1bank].pull_way,
		       (c[cid].l1rq[l1bank].pull_ADR&~(LINESIZE-1))+l*8,
		       (Uint)(c[cid].l1rq[l1bank].BUF[l]>>32), (Uint)c[cid].l1rq[l1bank].BUF[l]);
#ifdef CHECK_CACHE
	      {
		Ull membuf2;
		/* ������ ����å���������LOAD���� ������ */
		/* ������  �����Ǽ絭��������Ӳ�ǽ  ������ */
		membuf2 = mmr_chkc(tid, (c[cid].l1rq[l1bank].pull_ADR&~(LINESIZE-1))+l*8);
		if (c[cid].l1rq[l1bank].BUF[l] != membuf2)
		  printf("%03.3d:D1R %08.8x_%08.8x delayed(miss) arrived l1rq->o1 rv_share=%d index=%d way=%d A=%08.8x->%08.8x_%08.8x mismatch mmr %08.8x_%08.8x\n",
			 tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps),
			 c[cid].l1rq[l1bank].rv_share, index, c[cid].l1rq[l1bank].pull_way,
			 (c[cid].l1rq[l1bank].pull_ADR&~(LINESIZE-1))+l*8,
			 (Uint)(c[cid].l1rq[l1bank].BUF[l]>>32), (Uint)c[cid].l1rq[l1bank].BUF[l],
			 (Uint)(membuf2>>32), (Uint)membuf2);
	      }
#endif
	    }
	  }
	  rob = c[cid].l1rq[l1bank].rob;
	  if (rob->stat != ROB_ISSUED || c[cid].l1rq[l1bank].pull_ADR != rob->ls_addr) { /* mismatch by restarting */
	    if (flag & TRACE_ARM)
	      printf("%03.3d:D1R %08.8x_%08.8x %08.8x arrived but rejected (ib->pc=%08.8x)\n",
		     tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps),
		     c[cid].l1rq[l1bank].pull_ADR, rob->ls_addr);
	  }
	  else { /* ROB_ISSUED && address_match */
	    index = (c[cid].l1rq[l1bank].pull_ADR&(LINESIZE-1))>>3; /* ����6bit�ξ��3bit (8���) */
	    /* SC�Ϲ������ʤ�        */
	    /* CC�Ϲ������ʤ�        */
	    /* DR�Ϲ���              */
	    if (flag & TRACE_ARM)
	      printf("%03.3d:D1R delayed(miss) arrived to reg", tid);
	    switch (c[cid].l1rq[l1bank].opcd) {
	    case 0: /* LDRB */
	    case 8: /* VLDRB */
              D0[0] = (c[cid].l1rq[l1bank].BUF[index]>>((c[cid].l1rq[l1bank].pull_ADR&7)<<3))&0x00000000000000ffLL;
              if (rob->dr[1].t) ex_drw(c[cid].l1rq[l1bank].tid, 0LL, D0[0], rob, 1);
              if (rob->dr[2].t) ex_drw(c[cid].l1rq[l1bank].tid, 0LL, rob->dr[2].val[0], rob, 2); /* update base_addr */
              rob->stat = ROB_COMPLETE; /* done */
              break;
	    case 4: /* LDRSB */
              D0[0] = (Sll)((c[cid].l1rq[l1bank].BUF[index]>>((c[cid].l1rq[l1bank].pull_ADR&7)<<3))<<56)>>56;
              if (rob->dr[1].t) ex_drw(c[cid].l1rq[l1bank].tid, 0LL, D0[0], rob, 1);
              if (rob->dr[2].t) ex_drw(c[cid].l1rq[l1bank].tid, 0LL, rob->dr[2].val[0], rob, 2); /* update base_addr */
              rob->stat = ROB_COMPLETE; /* done */
              break;
	    case 1: /* LDRH */
	    case 9: /* VLDRH */
              D0[0] = (c[cid].l1rq[l1bank].BUF[index]>>((c[cid].l1rq[l1bank].pull_ADR&6)<<3))&0x000000000000ffffLL;
              if (rob->dr[1].t) ex_drw(c[cid].l1rq[l1bank].tid, 0LL, D0[0], rob, 1);
              if (rob->dr[2].t) ex_drw(c[cid].l1rq[l1bank].tid, 0LL, rob->dr[2].val[0], rob, 2); /* update base_addr */
              rob->stat = ROB_COMPLETE; /* done */
              break;
	    case 5: /* LDRSH */
              D0[0] = (Sll)((c[cid].l1rq[l1bank].BUF[index]>>((c[cid].l1rq[l1bank].pull_ADR&6)<<3))<<48)>>48;
              if (rob->dr[1].t) ex_drw(c[cid].l1rq[l1bank].tid, 0LL, D0[0], rob, 1);
              if (rob->dr[2].t) ex_drw(c[cid].l1rq[l1bank].tid, 0LL, rob->dr[2].val[0], rob, 2); /* update base_addr */
              rob->stat = ROB_COMPLETE; /* done */
              break;
	    case 2: /* LDRW */
	    case 7: /* LDREX */
	    case 10: /* VLDRS */
              D0[0] = (c[cid].l1rq[l1bank].BUF[index]>>((c[cid].l1rq[l1bank].pull_ADR&4)<<3))&0x00000000ffffffffLL;
	      if (rob->dr[1].t) ex_drw(c[cid].l1rq[l1bank].tid, 0LL, D0[0], rob, 1);
              if (rob->dr[2].t) ex_drw(c[cid].l1rq[l1bank].tid, 0LL, rob->dr[2].val[0], rob, 2); /* update base_addr */
              rob->stat = ROB_COMPLETE; /* done */
              break;
	    case 6: /* LDRSW */
              D0[0] = (Sll)((c[cid].l1rq[l1bank].BUF[index]>>((c[cid].l1rq[l1bank].pull_ADR&4)<<3))<<32)>>32;
              if (rob->dr[1].t) ex_drw(c[cid].l1rq[l1bank].tid, 0LL, D0[0], rob, 1);
              if (rob->dr[2].t) ex_drw(c[cid].l1rq[l1bank].tid, 0LL, rob->dr[2].val[0], rob, 2); /* update base_addr */
              rob->stat = ROB_COMPLETE; /* done */
              break;
	    case 3: /* LDR */
	    case 11: /* VLDRD */
	      D0[0] = c[cid].l1rq[l1bank].BUF[index];
              if (rob->dr[1].t) ex_drw(c[cid].l1rq[l1bank].tid, 0LL, D0[0], rob, 1);
              if (rob->dr[2].t) ex_drw(c[cid].l1rq[l1bank].tid, 0LL, rob->dr[2].val[0], rob, 2); /* update base_addr */
              rob->stat = ROB_COMPLETE; /* done */
              break;
	    case 12: /* VLDRQ */
	      D0[0] = c[cid].l1rq[l1bank].BUF[index];
	      D0[1] = c[cid].l1rq[l1bank].BUF[index|1];
              if (rob->dr[1].t) ex_drw(c[cid].l1rq[l1bank].tid, D0[1], D0[0], rob, 1);
              if (rob->dr[2].t) ex_drw(c[cid].l1rq[l1bank].tid, 0LL, rob->dr[2].val[0], rob, 2); /* update base_addr */
              rob->stat = ROB_COMPLETE; /* done */
              break;
	    }
	    if (flag & TRACE_ARM)
	      printf("\n");
	  }
	}
	else if (c[cid].l1rq[l1bank].type == 4) { /* pull & store */
	  /* l1rq.d[]����d1line.d[]��ž�� */
	  index = (c[cid].l1rq[l1bank].pull_ADR/LINESIZE/MAXL1BK)%D1WMAXINDEX;
	  c[cid].d1tag[l1bank][index][c[cid].l1rq[l1bank].pull_way].v     = 1;
	  c[cid].d1tag[l1bank][index][c[cid].l1rq[l1bank].pull_way].lru   = 127; /* reset lru */
	  c[cid].d1tag[l1bank][index][c[cid].l1rq[l1bank].pull_way].dirty = 1; /* dirty */
	  c[cid].d1tag[l1bank][index][c[cid].l1rq[l1bank].pull_way].share = c[cid].l1rq[l1bank].rv_share; /* sim-mreq�λؼ��˽��� */
	  c[cid].d1tag[l1bank][index][c[cid].l1rq[l1bank].pull_way].drain = 0; /* reset */
	  c[cid].d1tag[l1bank][index][c[cid].l1rq[l1bank].pull_way].la    = c[cid].l1rq[l1bank].pull_ADR/D1TAGMASK;
	  if (flag & TRACE_ARM)
	    printf("%03.3d:D1R delayed(miss) pull+store arrived l1rq->o1 dirty=%d share=%d opcd=%d A=%08.8x STD=%08.8x_%08.8x_%08.8x_%08.8x\n",
		   tid, 1, c[cid].l1rq[l1bank].rv_share, c[cid].l1rq[l1bank].opcd,
		   c[cid].l1rq[l1bank].pull_ADR, (Uint)(c[cid].l1rq[l1bank].STD[1]>>32), (Uint)c[cid].l1rq[l1bank].STD[1], (Uint)(c[cid].l1rq[l1bank].STD[0]>>32), (Uint)c[cid].l1rq[l1bank].STD[0]);
	  /* l1rq.store��retire���ʤΤ�cancel����뤳�ȤϤʤ� */
	  /* o1�إ��ԡ� */
	  if (c[cid].l1rq[l1bank].rv_l1fil) {
	    for (l=0; l<LINESIZE/8; l++) {
	      c[cid].d1line[l1bank][index][c[cid].l1rq[l1bank].pull_way].d[l] = c[cid].l1rq[l1bank].BUF[l];
	      if (flag & TRACE_ARM)
		printf("%03.3d:D1W delayed(miss) arrived l1rq->o1 rv_share=%d index=%d way=%d A=%08.8x<-%08.8x_%08.8x\n",
		       tid, c[cid].l1rq[l1bank].rv_share, index, c[cid].l1rq[l1bank].pull_way,
		       (c[cid].l1rq[l1bank].pull_ADR&~(LINESIZE-1))+l*8,
		       (Uint)(c[cid].l1rq[l1bank].BUF[l]>>32), (Uint)c[cid].l1rq[l1bank].BUF[l]);
#ifdef CHECK_CACHE
	      {
		Ull membuf2;
		/* ������ ����å���������STORE���� ������ */
		/* ������  �����Ǽ絭��������Ӳ�ǽ   ������ */
		membuf2 = mmr_chkc(tid, (c[cid].l1rq[l1bank].pull_ADR&~(LINESIZE-1))+l*8);
		if (c[cid].l1rq[l1bank].BUF[l] != membuf2)
		  printf("%03.3d:D1W %08.8x_%08.8x delayed(miss) arrived l1rq->o1 rv_share=%d index=%d way=%d A=%08.8x->%08.8x_%08.8x mismatch mmr %08.8x_%08.8x\n",
			 tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps),
			 c[cid].l1rq[l1bank].rv_share, index, c[cid].l1rq[l1bank].pull_way,
			 (c[cid].l1rq[l1bank].pull_ADR&~(LINESIZE-1))+l*8,
			 (Uint)(c[cid].l1rq[l1bank].BUF[l]>>32), (Uint)c[cid].l1rq[l1bank].BUF[l],
			 (Uint)(membuf2>>32), (Uint)membuf2);
	      }
#endif
	    }
	  }
	  /* l1rq->d[]�ȥޡ��� */
	  o = ((c[cid].l1rq[l1bank].pull_ADR&8)!=0); /* 0:even, 1:odd */
	  switch (c[cid].l1rq[l1bank].opcd) {
	  case 0: /* STRB */
	  case 8: /* VSTRB */
	    m[o]   = 0x00000000000000ffLL<<((c[cid].l1rq[l1bank].pull_ADR&7)<<3);
	    m[1-o] = 0x0000000000000000LL;
	    membuf[o]   = (c[cid].l1rq[l1bank].STD[0]<<((c[cid].l1rq[l1bank].pull_ADR&7)<<3));
	    membuf[1-o] = 0x0000000000000000LL;
	    break;
	  case 1: /* STRH */
	  case 9: /* VSTRH */
	    m[o]   = 0x000000000000ffffLL<<((c[cid].l1rq[l1bank].pull_ADR&6)<<3);
	    m[1-o] = 0x0000000000000000LL;
	    membuf[o]   = (c[cid].l1rq[l1bank].STD[0]<<((c[cid].l1rq[l1bank].pull_ADR&6)<<3));
	    membuf[1-o] = 0x0000000000000000LL;
	    break;
	  case 2: /* STRW */
	  case 7: /* STREX not reached */
	  case 10: /* VSTRS */
	    m[o]   = 0x00000000ffffffffLL<<((c[cid].l1rq[l1bank].pull_ADR&4)<<3);
	    m[1-o] = 0x0000000000000000LL;
	    membuf[o]   = (c[cid].l1rq[l1bank].STD[0]<<((c[cid].l1rq[l1bank].pull_ADR&4)<<3));
	    membuf[1-o] = 0x0000000000000000LL;
	    break;
	  case 3: /* STR */
	  case 11: /* VSTRD */
	    m[o]   = 0xffffffffffffffffLL;
	    m[1-o] = 0x0000000000000000LL;
	    membuf[o]   = c[cid].l1rq[l1bank].STD[0];
	    membuf[1-o] = 0x0000000000000000LL;
	    break;
	  case 12: /* VSTRQ */
	    m[o]   = 0xffffffffffffffffLL;
	    m[1-o] = 0xffffffffffffffffLL;
	    membuf[o]   = c[cid].l1rq[l1bank].STD[0];
	    membuf[1-o] = c[cid].l1rq[l1bank].STD[1];
	    break;
	  }
	  if (m[1]) {
	    c[cid].d1line[l1bank][index][c[cid].l1rq[l1bank].pull_way].d[((c[cid].l1rq[l1bank].pull_ADR|8)&(LINESIZE-1))>>3]
	      = (c[cid].d1line[l1bank][index][c[cid].l1rq[l1bank].pull_way].d[((c[cid].l1rq[l1bank].pull_ADR|8)&(LINESIZE-1))>>3]&~m[1])|(membuf[1]&m[1]);
#ifdef CHECK_CACHE
	    mmw_chkc(tid, (c[cid].l1rq[l1bank].pull_ADR&~7)|8, m[1], membuf[1]);
#endif
	  }
	  if (m[0]) {
	    c[cid].d1line[l1bank][index][c[cid].l1rq[l1bank].pull_way].d[(c[cid].l1rq[l1bank].pull_ADR&(LINESIZE-1))>>3]
	      = (c[cid].d1line[l1bank][index][c[cid].l1rq[l1bank].pull_way].d[(c[cid].l1rq[l1bank].pull_ADR&(LINESIZE-1))>>3]&~m[0])|(membuf[0]&m[0]);
#ifdef CHECK_CACHE
	    mmw_chkc(tid, c[cid].l1rq[l1bank].pull_ADR&~7, m[0], membuf[0]);
#endif
	  }
	  if (flag & TRACE_ARM)
            printf("%03.3d:D1W delayed(miss) arrived & STD_stored l1rq->o1 rv_share=%d index=%d way=%d A=%08.8x,M=%08.8x_%08.8x_%08.8x_%08.8x<-%08.8x_%08.8x_%08.8x_%08.8x\n",
                   tid, c[cid].l1rq[l1bank].rv_share, index, c[cid].l1rq[l1bank].pull_way,
                   (c[cid].l1rq[l1bank].pull_ADR&~(LINESIZE-1)),
                   (Uint)(m[1]>>32), (Uint)m[1], (Uint)(m[0]>>32), (Uint)m[0],
                   (Uint)(membuf[1]>>32), (Uint)membuf[1], (Uint)(membuf[0]>>32), (Uint)membuf[0]);
	}
	else {
	  printf("%03.3d:D1x not implemented A=%08.8x\n",
		 tid, (c[cid].l1rq[l1bank].pull_ADR&~(LINESIZE-1))+i*8);
	}
	c[cid].l1rq[l1bank].v_stat = 0;
	break;
      }
    }
    /****************************************************************************/
    /* L2rq->L1rq����                                                           */
    /****************************************************************************/
    for (l2bank=0; l2bank<MAXL2BK; l2bank++) {
      tid = c[cid].l2rq[l2bank].tid;
      l1bank = l2bank % MAXL1BK;
      switch (c[cid].l2rq[l2bank].v_stat&3) { /* stat */
      case 0: /* do nothing */
	break;
      case 1: /* L2rq busy */
	break;
      case 2: /* dirty-writeback-ok */
	if (c[cid].l2rq[l2bank].tid != tid)
	  break;
	/* CCreq(inv�Τ�)�ξ��,c[cid].l2rq.rq_push=0 */
	/*  l1rq.rq_push=1�ξ��,o1-dirty�ɤ��Ф��׵�l2w()��stat=1��pending(l1rqBUF��l1�ɤ��Ф�line���ݻ�)����Ƥ���. l1rqBUF��l2�˽񤭹����l1rq.push=0�Ȥ��ƽ�λ */
	/*  l1rq.rq_push=0�ξ��,o1-repl �ɤ߽Ф��׵�l2r()��stat=1��pending(l1rqBUF��l2�ɤ߽Ф�line���ݻ�)����Ƥ���. l1rqBUF��o1�˽񤭹���ǽ�λ */
	if (!c[cid].l2rq[l2bank].rq_push) { /* CCreq(inv�Τ�,mm��������)�ξ��,o1rqBUF�����Ƥ���� */
	  printf("%03.3d:L2W delayed(dirty writeback cc) ready but l2rq should not accept L1(dirty)->L2(!dirty|shared)->L2WB->MM (malfunction)\n", tid);
	  c[cid].l1rq[l1bank].v_stat = (c[cid].l1rq[l1bank].v_stat&0xc)|2; /* OP����OK */
	}
	else { /* CCreq(inv+mm�ؤν񤭹���)�ξ��ϲ��⤷�ʤ��Ƥ褤 */
	}
	c[cid].l2rq[l2bank].v_stat = 0;
	break;
      case 3: /* DIR->l2rq completed DIR����ǡ�������,l2�ؽ񤭹��� */
	if (c[cid].l2rq[l2bank].tid != tid)
	  break;
	/* l2rq.buf[]����i2_line.d[]��ž�� */
	index = (c[cid].l2rq[l2bank].pull_ADR/LINESIZE/MAXL2BK)%L2WMAXINDEX;
	if (flag & TRACE_ARM)
	  printf("%03.3d:L2R delayed(miss) arrived l2tag.la changed (l2bank=%d index=%d w=%d v=%d->%d dirty=%d->%d) from %08.8x to %08.8x\n",
		 tid, l2bank, index, c[cid].l2rq[l2bank].pull_way,
		 c[cid].l2tag[l2bank][index][c[cid].l2rq[l2bank].pull_way].v,
		 1,
		 c[cid].l2tag[l2bank][index][c[cid].l2rq[l2bank].pull_way].dirty,
		 ((c[cid].l2rq[l2bank].type == 3 && c[cid].l2rq[l2bank].opcd == 7)||c[cid].l2rq[l2bank].type == 4),
		 c[cid].l2tag[l2bank][index][c[cid].l2rq[l2bank].pull_way].la*L2TAGMASK|((index*MAXL2BK+l2bank)*LINESIZE),
		 (Uint)c[cid].l2rq[l2bank].pull_ADR);
	if (!c[cid].l2tag[l2bank][index][c[cid].l2rq[l2bank].pull_way].v)
	  c[cid].l2rq[l2bank].rv_l2fil = 1; /* ľ���˾ä��줿���,l2filɬ�� */
	c[cid].l2tag[l2bank][index][c[cid].l2rq[l2bank].pull_way].v     = 1;
	c[cid].l2tag[l2bank][index][c[cid].l2rq[l2bank].pull_way].lru   = 127; /* reset lru */
	c[cid].l2tag[l2bank][index][c[cid].l2rq[l2bank].pull_way].dirty = ((c[cid].l2rq[l2bank].type == 3 && c[cid].l2rq[l2bank].opcd == 7)||c[cid].l2rq[l2bank].type == 4); /* store/ldrex */
	c[cid].l2tag[l2bank][index][c[cid].l2rq[l2bank].pull_way].share = c[cid].l2rq[l2bank].rv_share;
	c[cid].l2tag[l2bank][index][c[cid].l2rq[l2bank].pull_way].drain = 0; /* reset */
	c[cid].l2tag[l2bank][index][c[cid].l2rq[l2bank].pull_way].la    = c[cid].l2rq[l2bank].pull_ADR/L2TAGMASK;
	if (flag & TRACE_ARM)
	  printf("%03.3d:L2R delayed(miss) arrived l2rq->l1rq dirty=%d share=%d l2fil=%d l1fil=%d A=%08.8x\n",
		 tid, ((c[cid].l2rq[l2bank].type == 3 && c[cid].l2rq[l2bank].opcd == 7)||c[cid].l2rq[l2bank].type == 4), c[cid].l2rq[l2bank].rv_share,
		 c[cid].l2rq[l2bank].rv_l2fil,
		 c[cid].l1rq[l1bank].rv_l1fil,
		 (c[cid].l2rq[l2bank].pull_ADR&~(LINESIZE-1)));
	/* l2�إ��ԡ� */
	if (c[cid].l2rq[l2bank].rv_l2fil) {
	  for (l=0; l<LINESIZE/8; l++) {
#if 0
	    int oid, dirindex, index1, l1way, l2way;
	    dirindex = c[cid].l2rq[l2bank].pull_ADR/LINESIZE/MAXL2BK;
	    index1 = (c[cid].l2rq[l2bank].pull_ADR/LINESIZE/MAXL1BK)%D1WMAXINDEX;
	    for (oid=0; oid<MAXCORE; oid++) {
	      if (oid == cid) continue;
	      if (d[l2bank].l2dir[dirindex].l2dir_v[0] & (1LL<<oid)) {
		int l1hit = 0;
		for (l1way=0; l1way<D1WAYS; l1way++) {
		  if (c[oid].d1tag[l1bank][index1][l1way].v && c[oid].d1tag[l1bank][index1][l1way].la == c[cid].l2rq[l2bank].pull_ADR/D1TAGMASK) {
		    l1hit = 1;
		    if (c[oid].d1line[l1bank][index1][l1way].d[l] != c[cid].l2rq[l2bank].BUF[l]) {
		      printf("%03.3d:L2R delayed(miss) arrived l2rq->l2 l2bank=%d index=%d way=%d A=%08.8x->%08.8x_%08.8x mismatch cid=%d L1 %08.8x_%08.8x\n",
			     tid, l2bank, index, c[cid].l2rq[l2bank].pull_way,
			     (c[cid].l2rq[l2bank].pull_ADR&~(LINESIZE-1))+l*8,
			     (Uint)(c[cid].l2rq[l2bank].BUF[l]>>32), (Uint)c[cid].l2rq[l2bank].BUF[l],
			     oid, 
			     (Uint)(c[oid].d1line[l1bank][index1][l1way].d[l]>>32), (Uint)c[oid].d1line[l1bank][index1][l1way].d[l]);
		    }
		  }
		}
		if (!l1hit) {
		  for (l2way=0; l2way<L2WAYS; l2way++) {
		    if (c[oid].l2tag[l2bank][index][l2way].v && c[oid].l2tag[l2bank][index][l2way].la == c[cid].l2rq[l2bank].pull_ADR/L2TAGMASK) {
		      if (c[oid].l2line[l2bank][index][l2way].d[l] != c[cid].l2rq[l2bank].BUF[l]) {
			printf("%03.3d:L2R delayed(miss) arrived l2rq->l2 l2bank=%d index=%d way=%d A=%08.8x->%08.8x_%08.8x mismatch cid=%d L2 %08.8x_%08.8x\n",
			       tid, l2bank, index, c[cid].l2rq[l2bank].pull_way,
			       (c[cid].l2rq[l2bank].pull_ADR&~(LINESIZE-1))+l*8,
			       (Uint)(c[cid].l2rq[l2bank].BUF[l]>>32), (Uint)c[cid].l2rq[l2bank].BUF[l],
			       oid, 
			       (Uint)(c[oid].l2line[l2bank][index][l2way].d[l]>>32), (Uint)c[oid].l2line[l2bank][index][l2way].d[l]);
		      }
		    }
		  }
		}
	      }
	    }
#endif
	    c[cid].l2line[l2bank][index][c[cid].l2rq[l2bank].pull_way].d[l] = c[cid].l2rq[l2bank].BUF[l];
	    if (flag & TRACE_ARM)
	      printf("%03.3d:L2R delayed(miss) arrived l2rq->l2 l2bank=%d index=%d way=%d A=%08.8x->%08.8x_%08.8x\n",
		     tid, l2bank, index, c[cid].l2rq[l2bank].pull_way,
		     (c[cid].l2rq[l2bank].pull_ADR&~(LINESIZE-1))+l*8,
		     (Uint)(c[cid].l2rq[l2bank].BUF[l]>>32), (Uint)c[cid].l2rq[l2bank].BUF[l]);
	  }
	  c[cid].l1rq[l1bank].rv_l1fil = 1; /* l1fil�������(fil=0)�ʹߤ�l2dir�ѹ�ͭ��ξ��,override */ 
	}
	if (c[cid].l1rq[l1bank].rv_l1fil) { /* l2fil=0,l1fil=1��ͭ������ */
	  for (l=0; l<LINESIZE/8; l++) {
	    c[cid].l1rq[l1bank].BUF[l] = c[cid].l2line[l2bank][index][c[cid].l2rq[l2bank].pull_way].d[l]; /* L1rq�� */
	    if (flag & TRACE_ARM)
	      printf("%03.3d:L2R delayed(miss) arrived l2rq->l1rq l2bank=%d index=%d way=%d A=%08.8x->%08.8x_%08.8x\n",
		     tid, l2bank, index, c[cid].l2rq[l2bank].pull_way,
		     (c[cid].l2rq[l2bank].pull_ADR&~(LINESIZE-1))+l*8,
		     (Uint)(c[cid].l1rq[l1bank].BUF[l]>>32), (Uint)c[cid].l1rq[l1bank].BUF[l]);
	  }
	}
	
	if (c[cid].l2rq[l2bank].rv_share)
	  c[cid].l1rq[l1bank].rv_share = 1;
	else
	  c[cid].l1rq[l1bank].rv_share = 0;
	c[cid].l1rq[l1bank].v_stat = (c[cid].l1rq[l1bank].v_stat&0xc)|((c[cid].l1rq[l1bank].opcd==15)?3:2); /* IF:OP */
	c[cid].l2rq[l2bank].v_stat = 0;
	break;
      }
    }
    /**************************************************************************************/
    /* L1rq->L2rq����å������                                                           */
    /**************************************************************************************/
    for (l1bank=0; l1bank<MAXL1BK; l1bank++) {
      tid = c[cid].l1rq[l1bank].tid;
      if (c[cid].l1rq[l1bank].v_stat == ((3<<2)|1)) { /* L1 request */
	if (c[cid].l1rq[l1bank].t)
	  c[cid].l1rq[l1bank].t--;
	if (c[cid].l1rq[l1bank].t == 0) {
	  Uint l2bank_push = (c[cid].l1rq[l1bank].push_ADR/LINESIZE)%MAXL2BK;
	  Uint l2bank_pull = (c[cid].l1rq[l1bank].pull_ADR/LINESIZE)%MAXL2BK;
#if 0
	  if (c[cid].l1rq[l1bank].rq_pull && c[cid].l2rq[l2bank_pull].v_stat) { /* l2rq�������Ƥ��ʤ���лߤ�� */
	    if (flag & TRACE_ARM)
	      printf("%03.3d:L1RQ->L2RQ pull l2rq ful waiting (l2rq.v_stat=%x t=%d tid=%d type=%d opcd=%d rq_push=%d push_A=%08.8x rq_pull=%d pull_A=%08.8x)\n",
		     tid, c[cid].l2rq[l2bank_pull].v_stat, c[cid].l2rq[l2bank_pull].t, c[cid].l2rq[l2bank_pull].tid, c[cid].l2rq[l2bank_pull].type, c[cid].l2rq[l2bank_pull].opcd,
		     c[cid].l2rq[l2bank_pull].rq_push, c[cid].l2rq[l2bank_pull].push_ADR, c[cid].l2rq[l2bank_pull].rq_pull, c[cid].l2rq[l2bank_pull].pull_ADR);
	  }
	  else
#endif
	    { /* l2rq�������Ƥ���Х��å� */
	    if (c[cid].l1rq[l1bank].rq_push) { /* l1����push�׵� inclusive�ʤΤ�ɬ��l2hit����Ϥ�����shared�ξ���l2dir̵������ɬ�� */
	      stat = l2w(tid, c[cid].l1rq[l1bank].push_ADR, c[cid].l1rq[l1bank].BUF, &pull_way, &rq_push, &push_adr, NULL);
	      if (flag & TRACE_ARM)
		printf("%03.3d:L2W for %s stat=%d push A=%08.8x OP=%d rq_push=%d push_adr=%08.8x\n",
		       tid, c[cid].l1rq[l1bank].type==3?"load":"store", stat, c[cid].l1rq[l1bank].push_ADR, c[cid].l1rq[l1bank].opcd,
		       rq_push, push_adr);
	      if (!stat) { /* hit on l2 -> extract to i1/o1 */
		if (!c[cid].l1rq[l1bank].rq_pull) {
		  c[cid].l1rq[l1bank].rv_share = 0; /* sim-core�Ǥ�̵�� */
		  c[cid].l1rq[l1bank].v_stat = (c[cid].l1rq[l1bank].v_stat&0xc)|2; /* OP����OK */
		  if (flag & TRACE_ARM)
		    printf("%03.3d:L1RQ->L2RQ push hit L2 + no pull\n", tid);
		}
		else { /* PUSH����λ(��³��PULLư��) */
		  c[cid].l1rq[l1bank].rq_push = 0;
		  c[cid].l1rq[l1bank].t = D1DELAY;
		  if (flag & TRACE_ARM)
		    printf("%03.3d:L1RQ->L2RQ push hit L2 + continue pull\n", tid);
		}
	      }
	      else if (stat == 2) {
		printf("%03.3d:L1RQ->L2RQ push miss violates inclusive cache l1bank=%d A=%08.8x OP=%d L1=dirty,L2=invalid (malfunction)\n",
		       tid, l1bank, c[cid].l1rq[l1bank].push_ADR, c[cid].l1rq[l1bank].opcd);
		c[cid].l1rq[l1bank].rv_share = 0; /* sim-core�Ǥ�̵�� */
		c[cid].l1rq[l1bank].v_stat = (c[cid].l1rq[l1bank].v_stat&0xc)|2; /* OP����OK */
	      }
	      else if (stat == 1) { /* ¾������shared��L1ȿ�ǺѤΤ���,L1->L2�ɤ��Ф���˸���ˤʤ�ʤ� */
		printf("%03.3d:L1RQ->L2RQ push miss violates inclusive cache l1bank=%d A=%08.8x OP=%d L1=dirty,L2=!dirty|shared (malfunction)\n",
		       tid, l1bank, c[cid].l1rq[l1bank].push_ADR, c[cid].l1rq[l1bank].opcd);
		c[cid].l1rq[l1bank].rv_share = 0; /* sim-core�Ǥ�̵�� */
		c[cid].l1rq[l1bank].v_stat = (c[cid].l1rq[l1bank].v_stat&0xc)|2; /* OP����OK */
	      }
	    }
	    else if (c[cid].l1rq[l1bank].rq_pull) { /* PULL:insn,cacheable-l/s,nc-load */
	      /* l1miss�Υ����� */
	      /* ��դ��٤���, !dirty|shared���֤�o1���Ф���store������� */
	      /* o1��hit���Ƥ�miss�Ȥ���l2���׵��Ф�,l2��hit���Ƥ�miss�Ȥ���l2dir���׵��Ф��ʤ���Фʤ�ʤ��� */
	      /* ���ξ��ϡ�o1w()��miss -> l2r()��miss -> mm��������load,l2dir�����Ȥʤ� */
	      /* l2r()��miss�����뤿��ˤϡ�l2r()���Ф���,l1r.type==9�Ǥ���ݤ����Τ��ʤ���Фʤ�ʤ� */
#if 1
	      if (c[cid].l2rq[l2bank_pull].v_stat)
		inhibit_replace = 1;
	      else
		inhibit_replace = 0;
#else
	      inhibit_replace = 0;
#endif
	      stat = l2r(tid, (c[cid].l1rq[l1bank].type==3&&c[cid].l1rq[l1bank].opcd==7)||c[cid].l1rq[l1bank].type==4, c[cid].l1rq[l1bank].pull_ADR, c[cid].l1rq[l1bank].BUF, &rv_share, &pull_way, &rq_push, &push_adr, inhibit_replace?NULL:c[cid].l2rq[l2bank_pull].BUF);
	      if (flag & TRACE_ARM)
		printf("%03.3d:L2R for %s stat=%d pull A=%08.8x OP=%d rq_push=%d push_adr=%08.8x\n",
		       tid, c[cid].l1rq[l1bank].type==3?"load":"store", stat, c[cid].l1rq[l1bank].pull_ADR, c[cid].l1rq[l1bank].opcd,
		       rq_push, push_adr);
	      if (!stat) { /* hit on l2 -> extract to i1/o1 */
		c[cid].l1rq[l1bank].rv_share = rv_share;
		c[cid].l1rq[l1bank].v_stat = (c[cid].l1rq[l1bank].v_stat&0xc)|((c[cid].l1rq[l1bank].opcd==15)?3:2); /* IF:OP */
		if (flag & TRACE_ARM)
		  printf("%03.3d:L1RQ->L2RQ pull hit L2\n", tid);
		t[tid].pa_l2hit++;
	      }
	      else {
		if (inhibit_replace) { /* l2rq�������Ƥ��ʤ���лߤ�� */
		  if (flag & TRACE_ARM)
		    printf("%03.3d:L1RQ->L2RQ pull l2rq ful waiting (l2rq.v_stat=%x t=%d tid=%d type=%d opcd=%d rq_push=%d push_A=%08.8x rq_pull=%d pull_A=%08.8x)\n",
			   tid, c[cid].l2rq[l2bank_pull].v_stat, c[cid].l2rq[l2bank_pull].t, c[cid].l2rq[l2bank_pull].tid, c[cid].l2rq[l2bank_pull].type, c[cid].l2rq[l2bank_pull].opcd,
			   c[cid].l2rq[l2bank_pull].rq_push, c[cid].l2rq[l2bank_pull].push_ADR, c[cid].l2rq[l2bank_pull].rq_pull, c[cid].l2rq[l2bank_pull].pull_ADR);
		}
		else {
		  c[cid].l2rq[l2bank_pull].rv_l2fil = ((c[cid].l1rq[l1bank].type==3&&c[cid].l1rq[l1bank].opcd==7)||c[cid].l1rq[l1bank].type==4) ? (stat==2) : 1; /* store&stat==1�ξ��fill̵�� */
		  c[cid].l2rq[l2bank_pull].t    = L2DIRDL;
		  c[cid].l2rq[l2bank_pull].tid  = tid;
		  c[cid].l2rq[l2bank_pull].type = c[cid].l1rq[l1bank].type;
		  c[cid].l2rq[l2bank_pull].opcd = c[cid].l1rq[l1bank].opcd; /* IF��ޤޤ�� */
		  c[cid].l2rq[l2bank_pull].rq_push = rq_push;
		  c[cid].l2rq[l2bank_pull].push_ADR = push_adr;
		  c[cid].l2rq[l2bank_pull].rq_pull = 1;
		  c[cid].l2rq[l2bank_pull].pull_ADR = c[cid].l1rq[l1bank].pull_ADR&~(LINESIZE-1); /* dirty�ɤ��Ф����L2rq.d����L2�˽񤭹���line�Υ��ɥ쥹���� */
		  c[cid].l2rq[l2bank_pull].pull_way = pull_way;
		  c[cid].l2rq[l2bank_pull].v_stat = (3<<2)|(1); /* valid */
		  if (rq_push)
		    d[(push_adr/LINESIZE)%MAXL2BK].l2rq_bitmap[cid] = 1; /* �ɤ��Ф��襯�饹�� */
		  d[l2bank_pull].l2rq_bitmap[cid] = 1; /* �ɤ߹����襯�饹�� */
		  if (flag & TRACE_ARM) {
		    if (rq_push)
		      printf("%03.3d:L1RQ->L2RQ %s push+pull queued l2rq l2bank=%d push_A=%08.8x\n", tid, c[cid].l1rq[l1bank].type==3?"load":"store",
			     (push_adr/LINESIZE)%MAXL2BK, push_adr);
		    printf("%03.3d:L1RQ->L2RQ %s pull-only queued l2rq l2bank=%d w=%d pull_A=%08.8x\n", tid, c[cid].l1rq[l1bank].type==3?"load":"store",
			   l2bank_pull, pull_way, c[cid].l2rq[l2bank_pull].pull_ADR);
		  }
		  t[tid].pa_l2mis++;
		}
	      }
	    }	  
	  }
	}
      }
    }
    /**************************************************************************************/
    /* L2cc->L1����å������                                                             */
    /**************************************************************************************/
    for (l2bank=0; l2bank<MAXL2BK; l2bank++) {
      if (c[cid].l2cc[l2bank].v_stat == ((3<<2)|1)) { /* L2CC request (thread#0 captures) */
	if (c[cid].l2cc[l2bank].t)
	  c[cid].l2cc[l2bank].t--;
	if (c[cid].l2cc[l2bank].t == 0) {
	  /* d1�򸡺� */
	  l1bank = (c[cid].l2cc[l2bank].target_ADR/LINESIZE)%MAXL1BK;
#if 1
	  if ((c[cid].l1rq[l1bank].v_stat&3) == 2 && (!c[cid].l1rq[l1bank].rq_push && c[cid].l1rq[l1bank].rq_pull && (c[cid].l1rq[l1bank].pull_ADR&~(LINESIZE-1)) == c[cid].l2cc[l2bank].target_ADR)) {
	    if (flag & TRACE_ARM)
	      printf("c%02.2d:L2CC[%d] copyback waiting l1rq->d1 A=%08.8x\n", cid, l2bank, c[cid].l2cc[l2bank].target_ADR);
	    continue; /* wait for L2rq->L2 */
	  }
#endif
	  index = (c[cid].l2cc[l2bank].target_ADR/LINESIZE/MAXL1BK)%D1WMAXINDEX;
	  fmin = MAXINT;
	  j = -1;
	  k = -1;
	  hit1rq = 0;
	  hit1 = 0;
	  if (c[cid].l1rq[l1bank].v_stat == ((3<<2)|1) && (c[cid].l1rq[l1bank].rq_push && c[cid].l1rq[l1bank].push_ADR == (c[cid].l2cc[l2bank].target_ADR&~(LINESIZE-1))))
	    hit1rq = 1;
	  for (w=0; w<D1WAYS; w++) {
	    if (c[cid].d1tag[l1bank][index][w].v && c[cid].d1tag[l1bank][index][w].la == c[cid].l2cc[l2bank].target_ADR/D1TAGMASK) { /* hit */
	      j = w;
	      hit1 = 1;
	    }
	  }
	  if (c[cid].l2cc[l2bank].rq_pull) { /* PULL */
	    /* �ޤ�l1rq�򸡺� */
	    if (hit1rq) {
	      for (l=0; l<LINESIZE/8; l++) {
		c[cid].l2cc[l2bank].BUF[l] = c[cid].l1rq[l1bank].BUF[l];
		if (flag & TRACE_ARM)
		  printf("c%02.2d:L2CC[%d] copyback steal l1rq->l2cc A=%08.8x->%08.8x_%08.8x\n",
			 cid, l2bank, (c[cid].l2cc[l2bank].target_ADR&~(LINESIZE-1))+l*8,
			 (Uint)(c[cid].l2cc[l2bank].BUF[l]>>32), (Uint)c[cid].l2cc[l2bank].BUF[l]);
	      }
	    }
	    else if (hit1) {
	      for (l=0; l<LINESIZE/8; l++) { /* 4way�ʤΤǼºݤˤɤ������ɤ߽Ф��Ƥ���Ϥ� */
		c[cid].l2cc[l2bank].BUF[l] = c[cid].d1line[l1bank][index][j].d[l];
		if (flag & TRACE_ARM)
		  printf("c%02.2d:L2CC[%d] copyback get d1->l2cc index=%d way=%d A=%08.8x->%08.8x_%08.8x\n",
			 cid, l2bank, index, j, (c[cid].l2cc[l2bank].target_ADR&~(LINESIZE-1))+l*8,
			 (Uint)(c[cid].l2cc[l2bank].BUF[l]>>32), (Uint)c[cid].l2cc[l2bank].BUF[l]);
	      }
	    }
	  }
	  if (flag & TRACE_ARM)
	    printf("c%02.2d:L2CC[%d] copyback(d1-hit) response rq_inv_share=%d index=%d\n", cid, l2bank, c[cid].l2cc[l2bank].rq_inv_share, index);
	  if (hit1rq) {
	    if (!c[cid].l2cc[l2bank].rq_inv_share) /* INVALIDATE */
	      c[cid].l1rq[l1bank].rq_push = 0;
	  }
	  if (hit1) {
	    if (!c[cid].l2cc[l2bank].rq_inv_share) /* INVALIDATE */
	      c[cid].d1tag[l1bank][index][j].v = 0;
	    else /* SHARE */
	      c[cid].d1tag[l1bank][index][j].share = 1;
	  }
	  
	  /* ����l2�򸡺� */
#if 1
	  if ((c[cid].l2rq[l2bank].v_stat&3) == 2 && (!c[cid].l2rq[l2bank].rq_push && c[cid].l2rq[l2bank].rq_pull && (c[cid].l2rq[l2bank].pull_ADR&~(LINESIZE-1)) == c[cid].l2cc[l2bank].target_ADR)) {
	    if (flag & TRACE_ARM)
	      printf("c%02.2d:L2CC[%d] copyback waiting l2rq->l2 A=%08.8x\n", cid, l2bank, c[cid].l2cc[l2bank].target_ADR);
	    continue; /* wait for L2rq->L2 */
	  }
#endif
	  index = (c[cid].l2cc[l2bank].target_ADR/LINESIZE/MAXL2BK)%L2WMAXINDEX;
	  fmin = MAXINT;
	  j = -1;
	  k = -1;
	  hit2rq = 0;
	  hit2 = 0;
	  if (c[cid].l2rq[l2bank].v_stat == ((3<<2)|1) && (c[cid].l2rq[l2bank].rq_push && c[cid].l2rq[l2bank].push_ADR == (c[cid].l2cc[l2bank].target_ADR&~(LINESIZE-1))))
	    hit2rq = 1;
	  for (w=0; w<L2WAYS; w++) {
	    if (c[cid].l2tag[l2bank][index][w].v && c[cid].l2tag[l2bank][index][w].la == c[cid].l2cc[l2bank].target_ADR/L2TAGMASK) { /* hit */
	      j = w;
	      hit2 = 1;
	    }
	  }
	  /* l2cc̵������,Ʊ����o1���ȥ��ߥ���ȯ���������L2R(store)��hit�����,o1��dirty�Τޤ޻Ĥ�, */
	  /* �ʲ��Υ����ɤˤ��,L2��̵���������.���η��,inclusive����������ʤ��ʤ� 20131107 Nakashima */
	  /* ���Τ褦�ʥ�������,l2cc�Ȥ��Ȥ߹�碌��ȯ������Τ�,l2rq�������l2cc����դ��ʤ�����⤢�����뤬 */
	  /* �ߤ�������l2cc��ɬ�פȤ�����deadlock�����ǽ�������� */
	  /* �����������ɥ쥹�ΰ��פޤǸ��������,l2dir��lock�ǲ���Ǥ��� */
	  if (c[cid].l2cc[l2bank].rq_pull && !hit1rq && !hit1) { /* PULL */
	    if (hit2rq) {
	      /* �ޤ�l2rq�򸡺� */
	      for (l=0; l<LINESIZE/8; l++) {
		c[cid].l2cc[l2bank].BUF[l] = c[cid].l2rq[l2bank].BUF[l];
		if (flag & TRACE_ARM)
		  printf("c%02.2d:L2CC[%d] copyback steal l2rq->l2cc A=%08.8x->%08.8x_%08.8x\n",
			 cid, l2bank, (c[cid].l2cc[l2bank].target_ADR&~(LINESIZE-1))+l*8,
			 (Uint)(c[cid].l2cc[l2bank].BUF[l]>>32), (Uint)c[cid].l2cc[l2bank].BUF[l]);
	      }
	    }
	    else if (hit2) {
	      for (l=0; l<LINESIZE/8; l++) { /* 4way�ʤΤǼºݤˤɤ������ɤ߽Ф��Ƥ���Ϥ� */
		c[cid].l2cc[l2bank].BUF[l] = c[cid].l2line[l2bank][index][j].d[l];
		if (flag & TRACE_ARM)
		  printf("c%02.2d:L2CC[%d] copyback get l2->l2cc index=%d way=%d A=%08.8x->%08.8x_%08.8x\n",
			 cid, l2bank, index, j, (c[cid].l2cc[l2bank].target_ADR&~(LINESIZE-1))+l*8,
			 (Uint)(c[cid].l2cc[l2bank].BUF[l]>>32), (Uint)c[cid].l2cc[l2bank].BUF[l]);
	      }
	    }
	  }
	  if (flag & TRACE_ARM)
	    printf("c%02.2d:L2CC[%d] copyback(l2-hit) response rq_inv_share=%d index=%d\n", cid, l2bank, c[cid].l2cc[l2bank].rq_inv_share, index);
	  if (hit2rq) {
	    if (!c[cid].l2cc[l2bank].rq_inv_share) /* INVALIDATE */
	      c[cid].l2rq[l2bank].rq_push = 0;
	  }
	  if (hit2) {
	    if (!c[cid].l2cc[l2bank].rq_inv_share) /* INVALIDATE */
	      c[cid].l2tag[l2bank][index][j].v = 0;
	    else /* SHARE */
	      c[cid].l2tag[l2bank][index][j].share = 1;
	  }
	  /* l2cc_ack_bitmap�˱���Ω�Ƥ� */
	  c[cid].l2cc[l2bank].v_stat = (c[cid].l2cc[l2bank].v_stat&0xc)|2;
	  d[l2bank].l2cc_ack_bitmap[cid] = 1;
	  if (flag & TRACE_ARM)
	    printf("c%02.2d:L2CC[%d] copyback l2cc_ack_bitmap[%d] is set\n", cid, l2bank, cid);
	  if (!hit1rq && !hit1 && !hit2rq && !hit2) { /* not found */
	    if (c[cid].l2cc[l2bank].rq_pull) { /* PULL */
	      printf("c%02.2d:L2CC[%d] copyback A=%08.8x pull not found (malfunction)\n",
		     cid, l2bank, c[cid].l2cc[l2bank].target_ADR);
	    }
	  }
	}
      }
    }
  }
  return (0);
}
