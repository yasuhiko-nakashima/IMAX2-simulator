
static char RcsHeader[] = "$Header: /usr/home/nakashim/proj-arm64/src/csim/RCS/ld.c,v 1.26 2017/08/23 15:27:49 nakashim Exp nakashim $";

/* ARM Instruction Simulator           */
/*        Copyright (C) 2007 by NAIST. */
/*         Primary writer: Y.Nakashima */
/*                nakashim@is.naist.jp */

/* ld.c 2005/3/22 */ 

#include "csim.h"

insn_exec_ld(tid, rob) Uint tid; struct rob *rob;
{
  Ull  s1, s2, s3, m0; /* source, through, middle(sft,shifter-carry), destination(alu) */
  Ull  D0[2];
  Uint addr, wb, wbaddr, rot;
  Ull  mask[2];

  if (rob->sr[0].t) ex_srr(tid, NULL, &s1, rob, 0);
  if (rob->sr[1].t) ex_srr(tid, NULL, &s2, rob, 1);
  if (rob->sr[2].t) ex_srr(tid, NULL, &s3, rob, 2);
  if (rob->sr[3].t) ex_srr(tid, &rob->dr[0].val[1], &rob->dr[0].val[0], rob, 3);
  if (rob->ptw) { /* save old_val for future partial_reg_write in dr[0] */
    switch (rob->opcd) {
    case 8:  /* VLDRB(8) */
    case 9:  /* VlDRH(16) */
    case 10: /* VLDRS(32) */
    case 11: /* VLDRD(64) */
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
  rot    = 0;

  /* LOAD */
  switch (rob->opcd) {
  case 0: /* LDRB */
  case 4: /* LDRSB */
  case 8: /* VLDRB */
    mask[0] = 0x00000000000000ffLL;
    mask[1] = 0x0000000000000000LL;
    break;
  case 1: /* LDRH */
  case 5: /* LDRSH */
  case 9: /* VLDRH */
    mask[0] = 0x000000000000ffffLL;
    mask[1] = 0x0000000000000000LL;
    break;
  case 2: /* LDRW */
  case 6: /* LDRSW */
  case 7: /* LDREX */
  case 10: /* VLDRS */
    mask[0] = 0x00000000ffffffffLL;
    mask[1] = 0x0000000000000000LL;
    break;
  case 3: /* LDR */
  case 11: /* VLDRD */
    mask[0] = 0xffffffffffffffffLL;
    mask[1] = 0x0000000000000000LL;
    break;
  case 12: /* VLDRQ */
    mask[0] = 0xffffffffffffffffLL;
    mask[1] = 0xffffffffffffffffLL;
    break;
  default:
    return (ROB_EXECERR); /* error */
  }

  /* read cache */
  switch (o_ldst(tid, rob->type, rob->opcd, addr, mask, rot, D0, rob)) {
  case 0: /* i1hit normal end */
    if (flag & TRACE_ARM) {
      printf(":%08.8x->[%08.8x_%08.8x_%08.8x_%08.8x]", addr, (Uint)(D0[1]>>32), (Uint)D0[1], (Uint)(D0[0]>>32), (Uint)D0[0]);
    }
    t[tid].pa_d1hit++;
    switch (rob->opcd) {
    case 4: /* LDRSB */
      D0[0] = (Sll)(D0[0]<<56)>>56;
      break;
    case 5: /* LDRSH */
      D0[0] = (Sll)(D0[0]<<48)>>48;
      break;
    case 6: /* LDRSW */
      D0[0] = (Sll)(D0[0]<<32)>>32;
      break;
    }
    if (      rob->dr[1].t) ex_drw(tid, D0[1], D0[0], rob, 1);
    if (wb && rob->dr[2].t) ex_drw(tid, 0LL, (Ull)wbaddr, rob, 2); /* update base */
    return (ROB_COMPLETE); /* done */
  case 1: /* d1mis l1rq busy */
    if (flag & TRACE_ARM) {
      printf(":%08.8x iorq/l1rq busy", addr);
    }
    t[tid].pa_d1waitcycle++;
    return (ROB_MAPPED); /* no change */
  case 2: /* d1mis l1rq enqueued */
    if (flag & TRACE_ARM) {
      printf(":%08.8x iorq/l1rq enqueued", addr);
    }
    if (wb && rob->dr[2].t) rob->dr[2].val[0] = wbaddr; /* should be rewritten ex_drw() by if not ROB_COMPLETE */
    t[tid].pa_d1mis++;
    return (ROB_ISSUED); /* error */
  case 3: /* error (ouf of range) */
    if (flag & TRACE_ARM) {
      printf(":%08.8x address out of range", addr);
    }
    return (ROB_EXECERR); /* error */
  }
}
