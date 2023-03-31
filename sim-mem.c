
static char RcsHeader[] = "$Header: /usr/home/nakashim/proj-arm64/src/csim/RCS/sim-mem.c,v 1.4 2017/04/21 03:28:45 nakashim Exp nakashim $";

/* SPARC Simulator                     */
/*         Copyright (C) 2010 by NAIST */
/*         Primary writer: Y.Nakashima */
/*                nakashim@is.naist.jp */

/* sim-mem.c 2010/7/10 */

#include "csim.h"

void *sim_mem(param) Uint param;
{
  Uint  mid = param >> 16;
  int   cycle = param & 0xffff;
  int   dir, index, i, l;

  while (cycle-- > 0) {
    /*=====================================================================*/
    /* ��sim_cluster�ϡ�m[mid].v_stat�򸡺�����push/pullư��               */
    /*=====================================================================*/
    for (dir=0; dir<MAXL2BK; dir++) {
      if (m[mid].mmcc[dir].v_stat == ((3<<2)|1)) { /* L2CC request */
	if (m[mid].mmcc[dir].t)
	  m[mid].mmcc[dir].t--;
	if (m[mid].mmcc[dir].t == 0) {
	  /* ddr */
	  if (m[mid].mmcc[dir].rq_push) { /* PUSH */
	    for (l=0; l<LINESIZE/8; l++) {
	      mmw((m[mid].mmcc[dir].push_ADR&~(LINESIZE-1))+l*8, 0xffffffffffffffffLL, m[mid].mmcc[dir].BUF[l]);
	      if (flag & TRACE_ARM)                                       
		printf("m%02.2d:MMCC push to DDR (pull=%d) A=%08.8x<-%08.8x_%08.8x\n",
		       mid, m[mid].mmcc[dir].rq_pull,
		       (m[mid].mmcc[dir].push_ADR&~(LINESIZE-1))+l*8, (Uint)(m[mid].mmcc[dir].BUF[l]>>32), (Uint)m[mid].mmcc[dir].BUF[l]);
	    }                                                              
	  }
	  else if (m[mid].mmcc[dir].rq_pull) {
	    for (l=0; l<LINESIZE/8; l++) {
	      m[mid].mmcc[dir].BUF[l] = mmr((m[mid].mmcc[dir].pull_ADR&~(LINESIZE-1))+l*8);
	      if (flag & TRACE_ARM)
		printf("m%02.2d:MMCC pull from DDR A=%08.8x->%08.8x_%08.8x\n",
		       mid, (m[mid].mmcc[dir].pull_ADR&~(LINESIZE-1))+l*8,
		       (Uint)(m[mid].mmcc[dir].BUF[l]>>32), (Uint)m[mid].mmcc[dir].BUF[l]);
	    }
	  }
	  if (m[mid].mmcc[dir].rq_push && m[mid].mmcc[dir].rq_pull) {
	    m[mid].mmcc[dir].rq_push = 0;       /* rq_pull next time */
	    m[mid].mmcc[dir].t       = MMDELAY; /* rq_pull next time */
	  }
	  else {
	    m[mid].mmcc[dir].v_stat = (m[mid].mmcc[dir].v_stat&0xc)|2;
	    /* mmcc_ack_bitmap�˱���Ω�Ƥ� */
	    d[dir].mmcc_ack_bitmap[mid] = 1;
	    if (flag & TRACE_ARM)
	      printf("d%02.2d:MMCC mmcc_ack_bitmap[%d] is set\n", dir, mid);
	  }
        }
      }
    }
  }
}

/* CHECK_CACHE�� */
#ifdef CHECK_CACHE

struct critical {
  Uint addr;
  int tid; /* ldr set tid */
} critical[] = {
  {0x0e3c4028, -1},   /* nnextfrontiers__neighbors  8B-aligned */
  {0x0e444038, -1},   /* nfrontier_edges__neighbors 8B-aligned */
};

Ull mmr_chkc(tid, addr) Uint tid, addr;
{
  Uchar *mp;
  Ull val;
  int i;

  if (addr < MEMSIZE) {
    mp = &mem[0][addr];
    val = ((Ull)*(mp+7)<<56)|((Ull)*(mp+6)<<48)|((Ull)*(mp+5)<<40)|((Ull)*(mp+4)<<32)|((Ull)*(mp+3)<<24)|((Ull)*(mp+2)<<16)|((Ull)*(mp+1)<< 8)|((Ull)*(mp+0)<< 0);
  }
  else {
    val = 0x0000000000000000LL;
  }

#if 0
  for (i=0; i<sizeof(critical)/sizeof(struct critical); i++) {
    if (critical[i].addr == addr) {
      if (flag & TRACE_ARM)
	printf("%03.3d:CHKC %08.8x_%08.8x READ critical section A=%08.8x VAL=%08.8x_%08.8x\n",
	       tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps),
	       addr, (Uint)(val>>32), (Uint)val);
      if (tid) /* tid=0 should be ignored (tid=0 will not use lock/unlock before pthread_create) */
	critical[i].tid = tid;
    }
  }
#endif

  return (val);
}

Ull mmw_chkc(tid, addr, mask, val) Uint tid, addr; Ull mask, val;
{
  Uchar *mp;
  int i;

  if (addr < MEMSIZE) {
    mp = &mem[0][addr];
    if (mask & 0xff00000000000000LL) *(mp+7) = val>>56;
    if (mask & 0x00ff000000000000LL) *(mp+6) = val>>48;
    if (mask & 0x0000ff0000000000LL) *(mp+5) = val>>40;
    if (mask & 0x000000ff00000000LL) *(mp+4) = val>>32;
    if (mask & 0x00000000ff000000LL) *(mp+3) = val>>24;
    if (mask & 0x0000000000ff0000LL) *(mp+2) = val>>16;
    if (mask & 0x000000000000ff00LL) *(mp+1) = val>> 8;
    if (mask & 0x00000000000000ffLL) *(mp+0) = val;
  }
  else {
  }

#if 0
  for (i=0; i<sizeof(critical)/sizeof(struct critical); i++) {
    if (critical[i].addr == addr) {
      if (critical[i].tid != -1 && critical[i].tid != tid)
	printf("%03.3d:CHKC %08.8x_%08.8x VIOLATES critical section A=%08.8x VAL=%08.8x_%08.8x/%08.8x_%08.8x prev_load_tid=%d\n",
	       tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps),
	       addr, (Uint)(val>>32), (Uint)val, (Uint)(mask>>32), (Uint)mask, critical[i].tid);
      if (flag & TRACE_ARM)
	printf("%03.3d:CHKC %08.8x_%08.8x WRITE critical section A=%08.8x VAL=%08.8x_%08.8x/%08.8x_%08.8x\n",
	       tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps),
	       addr, (Uint)(val>>32), (Uint)val, (Uint)(mask>>32), (Uint)mask);
      critical[i].tid = -1;
    }
  }
#endif

  return (0);
}
#endif

/* ����,DDR���֤ϥ��饹���˶���ʬ�� */
Ull mmr(addr) Uint addr;
{
  Uchar *mp;
  Ull val;

  if (addr < MEMSIZE) {
    mp = &mem[0][addr];
    val = ((Ull)*(mp+7)<<56)|((Ull)*(mp+6)<<48)|((Ull)*(mp+5)<<40)|((Ull)*(mp+4)<<32)|((Ull)*(mp+3)<<24)|((Ull)*(mp+2)<<16)|((Ull)*(mp+1)<< 8)|((Ull)*(mp+0)<< 0);
    if (flag & TRACE_ARM)
      printf("MMR(A=%08.8x->%08.8x_%08.8x)\n", addr, (Uint)(val>>32), (Uint)val);
  }
  else {
    val = 0x0000000000000000LL;
    printf("MMR out of range (A=%08.8x->%08.8x_%08.8x)\n", addr, (Uint)(val>>32), (Uint)val);
  }

  return (val);
}

Ull mmw(addr, mask, val) Uint addr; Ull mask, val;
{
  Uchar *mp;

  if (addr < MEMSIZE) {
    mp = &mem[0][addr];
    if (mask & 0xff00000000000000LL) *(mp+7) = val>>56;
    if (mask & 0x00ff000000000000LL) *(mp+6) = val>>48;
    if (mask & 0x0000ff0000000000LL) *(mp+5) = val>>40;
    if (mask & 0x000000ff00000000LL) *(mp+4) = val>>32;
    if (mask & 0x00000000ff000000LL) *(mp+3) = val>>24;
    if (mask & 0x0000000000ff0000LL) *(mp+2) = val>>16;
    if (mask & 0x000000000000ff00LL) *(mp+1) = val>> 8;
    if (mask & 0x00000000000000ffLL) *(mp+0) = val;
    if (flag & TRACE_ARM)
      printf("MMW(A=%08.8x<-%08.8x_%08.8x)\n", addr, (Uint)(val>>32), (Uint)val);
  }
  else {
    printf("MMW out of range (A=%08.8x<-%08.8x_%08.8x)\n", addr, (Uint)(val>>32), (Uint)val);
  }
#if 0
  switch (addr) {
  case CREGBASE+CCT_RQTYPE:/* ���è�0:��ư�׵� 1:����׵� */
    /* �񤭹��߸��Фˤ��,¾������ư/��� */
    /* CCT_CORMSK:/* ���è��¥����ֹ�����ޥ�����64bit�� 0:�����оݳ���1:�����о� */
    /* CCT_RQTYPE:/* ���è�0:��ư�׵� 1:����׵�                                  */
    /* CCT_SPINIT:/* ���è�%sp����͡�DSM�����ж��̥��ɥ쥹��                     */
    /* CCT_SPLIMT:/* ���è�%sp�����͡�DSM�����ж��̥��ɥ쥹��                     */
    /* CCT_PSTART:/* ���è�PC����͡�DDR3�ⶦ�̳��ϥ��ɥ쥹��                     */
    p[pid].trig_cct = 1;
    break;
  case CREGBASE+DTU_CONTRL:/* ���è�0:ž������ 1:��߻ؼ� */
    /* �񤭹��߸��Фˤ��,DTU��ư/��� */
    /* DTU_CONTRL:/* ���è�0:ž������ 1:��߻ؼ�����������������                  */
    /* DTU_BLKTOP:/* ���è�DSM����������֥�å���Ƭ���ɥ쥹                      */
    /* DTU_CBLKAD:/* ���è�����������֥�å����ɥ쥹��ɽ����                     */
    /* DTU_CBLKST:/* ���è�����������֥�å�������ɽ����                         */
    p[pid].trig_dtu = 1;
    break;
  }
#endif
  return (0);
}
