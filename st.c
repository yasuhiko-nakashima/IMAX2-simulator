
static char RcsHeader[] = "$Header: /usr/home/nakashim/proj-arm64/src/csim/RCS/st.c,v 1.20 2017/04/21 03:28:45 nakashim Exp nakashim $";

/* ARM Instruction Simulator           */
/*        Copyright (C) 2007 by NAIST. */
/*         Primary writer: Y.Nakashima */
/*                nakashim@is.naist.jp */

/* st.c 2005/3/22 */

#include "csim.h"

insn_exec_st(tid, rob) Uint tid; struct rob *rob;
{
  Ull  s1, s2, s3, m0; /* source, through, middle(sft,shifter-carry), destination(alu) */
  Ull  D0[2];
  Uint addr, wb, wbaddr;
  Ull  mask[2];

  if (rob->sr[0].t) ex_srr(tid, NULL, &s1, rob, 0);
  if (rob->sr[1].t) ex_srr(tid, NULL, &s2, rob, 1);
  if (rob->sr[2].t) ex_srr(tid, NULL, &s3, rob, 2);
  if (rob->sr[3].t) ex_srr(tid, &D0[1], &D0[0], rob, 3);
  if (rob->ptw) { /* shift for partial_reg_read */
    switch (rob->opcd) {
    case 8:  /* VSTRB(8) */
    case 9:  /* VSTRH(16) */
    case 10: /* VSTRS(32) */
    case 11: /* VSTRD(64) */
      if (rob->idx >= 8) /* 15-8 */
	D0[0] = D0[1]>>((rob->idx-8)*8);
      else               /* 7-0 */
	D0[0] = D0[0]>>(rob->idx*8);
      break;
    }
  }

  /* Shifter */
  switch (rob->sop) {
  case 0: /* nop */
    m0 = s2;
    break;
  case 8: /* 0:uxtb */
    m0 = (Ull)(s2<<56)>>(56-s3);
    break;
  case 9: /* 1:uxth */
    m0 = (Ull)(s2<<48)>>(48-s3);
    break;
  case 10: /* 2:uxtw */
    m0 = (Ull)(s2<<32)>>(32-s3);
    break;
  case 11: /* 3:uxtx */
    m0 = (Ull)(s2<<s3);
    break;
  case 12: /* 4:sxtb */
    m0 = (Sll)(s2<<56)>>(56-s3);
    break;
  case 13: /* 5:sxth */
    m0 = (Sll)(s2<<48)>>(48-s3);
    break;
  case 14: /* 6:sxtw */
    m0 = (Sll)(s2<<32)>>(32-s3);
    break;
  case 15: /* 7:sxtx */
    m0 = (Sll)(s2<<s3);
    break;
  }

  if (!rob->plus) m0 = -(int)m0;
  addr   = s1 + ((rob->pre)?m0:0);
  wbaddr = s1 + m0;
  wb     = rob->wb;

  /* STORE */
  switch (rob->opcd) {
  case 0: /* STRB */
  case 8: /* VSTRB */
    mask[0] = 0x00000000000000ffLL;
    mask[1] = 0x0000000000000000LL;
    break;
  case 1: /* STRH */
  case 9: /* VSTRH */
    mask[0] = 0x000000000000ffffLL;
    mask[1] = 0x0000000000000000LL;
    break;
  case 2: /* STRW */
  case 7: /* STREX */
  case 10: /* VSTRS */
    mask[0] = 0x00000000ffffffffLL;
    mask[1] = 0x0000000000000000LL;
    break;
  case 3: /* STR */
  case 11: /* VSTRD */
    mask[0] = 0xffffffffffffffffLL;
    mask[1] = 0x0000000000000000LL;
    break;
  case 12: /* VSTRQ */
    mask[0] = 0xffffffffffffffffLL;
    mask[1] = 0xffffffffffffffffLL;
    break;
  default:
    return (ROB_EXECERR); /* error */
  }

  /* write cache */
  switch (o_ldst(tid, rob->type, rob->opcd, addr, mask, 0, D0, rob)) {
  case 0: /* d1hit normal end */
    if (wb && rob->dr[2].t) ex_drw(tid, 0LL, (Ull)wbaddr, rob, 2); /* update base */
    if (flag & TRACE_ARM) {
      printf(":%08.8x<-[%08.8x_%08.8x_%08.8x_%08.8x] ROB%03.3d.stbf", addr, (Uint)(D0[1]>>32), (Uint)D0[1], (Uint)(D0[0]>>32), (Uint)D0[0], rob-&c[tid2cid(tid)].rob[0]);
    }
    if (rob->type == 4 && rob->opcd == 7) /* strex */
      return (ROB_STREXWAIT); /* done */
    else
      return (ROB_COMPLETE); /* done */
#if 0
  case 1: /* d1mis l1rq busy */
    if (flag & TRACE_ARM) {
      printf(":%08.8x l1rq busy", addr);
    }
    t[tid].pa_i1waitcycle++;
    return (ROB_MAPPED); /* no change */
  case 2: /* d1mis l1rq enqueued */
    if (flag & TRACE_ARM) {
      printf(":%08.8x l1rq enqueued", addr);
    }
    t[tid].pa_d1mis++;
    return (ROB_ISSUED); /* error */
#endif
  case 3: /* error (ouf of range) */
    if (flag & TRACE_ARM) {
      printf(":%08.8x address out of range", addr);
    }
    return (ROB_EXECERR); /* error */
  }
}
