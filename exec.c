
static char RcsHeader[] = "$Header: /usr/home/nakashim/proj-arm64/src/csim/RCS/exec.c,v 1.13 2017/04/21 03:28:45 nakashim Exp nakashim $";

/* ARM Instruction Simulator           */
/*        Copyright (C) 2007 by NAIST. */
/*         Primary writer: Y.Nakashima */
/*                nakashim@is.naist.jp */

/* exec.c 2005/3/22 */ 

#include "csim.h"

insn_exec(tid, rob) Uint tid; struct rob *rob;
{
  Uint cid = tid2cid(tid);
  Ull  s1, s2, cc;
  int  exec; /* 0:null 1:exec */

  if (rob->cond < 14)
    ex_srr(tid, NULL, &cc, rob, 5); /* CPSREG */
  else
    cc = 0; /* dummy */

  switch (rob->cond) {
  case 0: /* EQ (Z set) */
    if (cc & CC_Z) exec = 1;
    else           exec = 0;
    break;
  case 1:  /* NE (Z clear) */
    if (cc & CC_Z) exec = 0;
    else           exec = 1;
    break;
  case 2:  /* CS (C set) */
    if (cc & CC_C) exec = 1;
    else           exec = 0;
    break;
  case 3:  /* CC (C clear) */
    if (cc & CC_C) exec = 0;
    else           exec = 1;
    break;
  case 4:  /* MI (N set) */
    if (cc & CC_N) exec = 1;
    else           exec = 0;
    break;
  case 5:  /* PL (N clear) */
    if (cc & CC_N) exec = 0;
    else           exec = 1;
    break;
  case 6:  /* VS (V set) */
    if (cc & CC_V) exec = 1;
    else           exec = 0;
    break;
  case 7:  /* VC (V clear) */
    if (cc & CC_V) exec = 0;
    else           exec = 1;
    break;
  case 8:  /* HI (C=1&Z=0) */
    if ((cc & CC_C) && !(cc & CC_Z)) exec = 1;
    else                             exec = 0;
    break;
  case 9:  /* LS (C=0|Z=1) */
    if ((cc & CC_C) && !(cc & CC_Z)) exec = 0;
    else                             exec = 1;
    break;
  case 10: /* GE (N==V) */
    if (((cc & CC_N) && (cc & CC_V)) || (!(cc & CC_N) && !(cc & CC_V))) exec = 1;
    else                                                                exec = 0;
    break;
  case 11: /* LT (N!=V) */
    if (((cc & CC_N) && (cc & CC_V)) || (!(cc & CC_N) && !(cc & CC_V))) exec = 0;
    else                                                                exec = 1;
    break;
  case 12: /* GT (Z=0&N==V */
    if (!(cc & CC_Z) && (((cc & CC_N) && (cc & CC_V)) || (!(cc & CC_N) && !(cc & CC_V)))) exec = 1;
    else                                                                                  exec = 0;
    break;
  case 13: /* LE (Z=1|N!=V */
    if (!(cc & CC_Z) && (((cc & CC_N) && (cc & CC_V)) || (!(cc & CC_N) && !(cc & CC_V)))) exec = 0;
    else                                                                                  exec = 1;
    break;
  case 14: /* AL (Always) */
    exec = 1;
    break;
  case 15: /* AL (Always) */
    exec = 1;
    break;
  }

  switch (rob->type) {
  case 0: /* 0:ALU */
    rob->stat = insn_exec_alu(tid, rob);
    break;
  case 1: /* 1:MUL */
    rob->stat = insn_exec_mul(tid, rob);
    break;
  case 2: /* 2:VXX */
    rob->stat = insn_exec_vxx(tid, rob);
    break;
  case 3: /* 3:LD */
    rob->stat = insn_exec_ld(tid, rob);
    break;
  case 4: /* 4:ST */
    rob->stat = insn_exec_st(tid, rob);
    break;
  case 5: /* 5:BRANCH */
    switch (rob->opcd) {
    case 1: /* BL */
      ex_drw(tid, 0LL, (Ull)rob->pc+4, rob, 1);
    case 0: /* B */
      break;
    case 2: /* CBZ */
      ex_srr(tid, NULL, &s1, rob, 0);
      if (!rob->dbl)
	s1 &= 0x00000000ffffffffLL;
      if (s1 == 0LL) exec = 1;
      else           exec = 0;
      break;
    case 3:/* CBNZ */
      ex_srr(tid, NULL, &s1, rob, 0);
      if (!rob->dbl)
	s1 &= 0x00000000ffffffffLL;
      if (s1 != 0LL) exec = 1;
      else           exec = 0;
      break;
    case 4: /* TBZ */
      ex_srr(tid, NULL, &s1, rob, 0);
      ex_srr(tid, NULL, &s2, rob, 1);
      if (s1&(0x0000000000000001LL<<s2)) exec = 0;
      else                               exec = 1;
      break;
    case 5:/* TBNZ */
      ex_srr(tid, NULL, &s1, rob, 0);
      ex_srr(tid, NULL, &s2, rob, 1);
      if (s1&(0x0000000000000001LL<<s2)) exec = 1;
      else                               exec = 0;
      break;
    case 6:/* CALL(BLR) */
      ex_drw(tid, 0LL, (Ull)rob->pc+4, rob, 1);
    case 7:/* JMP/RET(BR) */
      ex_srr(tid, NULL, &(rob->target), rob, 0);
      rob->stat = ROB_RESTART; /* restart pipe */
      break;
    }
    if (exec && !rob->bpred) /* bpred miss taken */
      rob->stat = ROB_RESTART; /* restart pipe */
    else if (!exec && rob->bpred) { /* bpred miss not-taken */
      rob->target = rob->pc+4;
      rob->stat = ROB_RESTART; /* restart pipe */
    }
    else
      rob->stat = ROB_COMPLETE; /* empty */
    break;
  case 6: /* 6:SVC */
    rob->stat = ROB_RESTART; /* restart pipe */
    break;
  case 7: /* 7:PTHREAD */
    rob->stat = ROB_RESTART; /* restart pipe */
    break;
  }

  return (0);
}
