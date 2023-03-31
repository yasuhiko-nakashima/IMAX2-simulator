
static char RcsHeader[] = "$Header: /usr/home/nakashim/proj-arm64/src/csim/RCS/mul.c,v 1.15 2017/04/21 03:28:45 nakashim Exp nakashim $";

/* ARM Instruction Simulator           */
/*        Copyright (C) 2007 by NAIST. */
/*         Primary writer: Y.Nakashima */
/*                nakashim@is.naist.jp */

/* mul.c 2005/3/22 */ 

#include "csim.h"

insn_exec_mul(tid, rob) Uint tid; struct rob *rob;
{
  Uint cid = tid2cid(tid);
  Ull  s1, s2, s3; /* source, through, middle(sft,shifter-carry), destination(alu) */
  union {
    Ull  ll;
    Uint l[2];
  } S1, S2, T[4], D[4]; /* for 128bit-multiply */
  Ull D0;

  if (rob->sr[0].t) ex_srr(tid, NULL, &s1, rob, 0);
  if (rob->sr[1].t) ex_srr(tid, NULL, &s2, rob, 1);
  if (rob->sr[2].t) ex_srr(tid, NULL, &s3, rob, 2);

  if (rob->opcd <= 7) { /* MUL */
    switch (rob->opcd) {
    case 0: /* UMADDL */
      s2 &= 0x00000000ffffffffLL;
      s3 &= 0x00000000ffffffffLL;
      D0 = s3 + s1 * s2;
      break;
    case 1: /* UMSUBL */
      s2 &= 0x00000000ffffffffLL;
      s3 &= 0x00000000ffffffffLL;
      D0 = s3 - s1 * s2;
      break;
    case 2: /* SMADDL */
      s2 = (Sll)(s2<<32)>>32;
      s3 = (Sll)(s3<<32)>>32;
      D0 = (Sll)s3 + (Sll)s1 * (Sll)s2;
      break;
    case 3: /* SMSUBL */
      s2 = (Sll)(s2<<32)>>32;
      s3 = (Sll)(s3<<32)>>32;
      D0 = (Sll)s3 - (Sll)s1 * (Sll)s2;
      break;
    case 4: /* MADD */
      D0 = s3 + s1 * s2;
      if (!rob->dbl)
	D0 &= 0x00000000ffffffffLL;
      break;
    case 5: /* MSUB */
      D0 = s3 - s1 * s2;
      if (!rob->dbl)
	D0 &= 0x00000000ffffffffLL;
      break;
    case 6: /* SMULH */
      /*                     T[0].l[1] T[0].l[0] */
      /*           T[1].l[1] T[1].l[0]           */
      /*           T[2].l[1] T[2].l[0]           */
      /* T[3].l[1] T[3].l[0]                     */
      /* --------------------------------------- */
      /* D[3].l[0] D[2].l[0] D[1].l[0] D[0].l[0] */
      /* <-------D0-------->                     */
      S1.ll = ((Sll)s1>=0LL)? (Sll)s1 : -(Sll)s1;
      S2.ll = ((Sll)s2>=0LL)? (Sll)s2 : -(Sll)s2;
      T[0].ll = (Ull)S1.l[0] * (Ull)S2.l[0];
      T[1].ll = (Ull)S1.l[1] * (Ull)S2.l[0];
      T[2].ll = (Ull)S1.l[0] * (Ull)S2.l[1];
      T[3].ll = (Ull)S1.l[1] * (Ull)S2.l[1];
      D[0].ll =                                                                    (Ull)T[0].l[0];
      D[1].ll =                  (Ull)T[2].l[0] + (Ull)T[1].l[0] + (Ull)T[0].l[1];
      D[2].ll = (Ull)T[3].l[0] + (Ull)T[2].l[1] + (Ull)T[1].l[1] + (Ull)D[1].l[1];
      D[3].ll = (Ull)T[3].l[1] + (Ull)D[2].l[1];
      if (((Sll)s1>=0LL)^((Sll)s2>=0LL)) { /* invert */
	T[0].l[0] = ~D[0].l[0];
	T[1].l[0] = ~D[1].l[0];
	T[2].l[0] = ~D[2].l[0];
	T[3].l[0] = ~D[3].l[0];
	D[0].ll = (Ull)T[0].l[0] + 1;
	D[1].ll = (Ull)T[1].l[0] + (Ull)D[0].l[1];
	D[2].ll = (Ull)T[2].l[0] + (Ull)D[1].l[1];
	D[3].ll = (Ull)T[3].l[0] + (Ull)D[2].l[1];
      }
      D0 = ((Ull)D[3].l[0]<<32)|(Ull)D[2].l[0];
      break;
    case 7: /* UMULH */
      S1.ll = (Sll)s1;
      S2.ll = (Sll)s2;
      T[0].ll = (Ull)S1.l[0] * (Ull)S2.l[0];
      T[1].ll = (Ull)S1.l[1] * (Ull)S2.l[0];
      T[2].ll = (Ull)S1.l[0] * (Ull)S2.l[1];
      T[3].ll = (Ull)S1.l[1] * (Ull)S2.l[1];
      D[0].ll =                                                                    (Ull)T[0].l[0];
      D[1].ll =                  (Ull)T[2].l[0] + (Ull)T[1].l[0] + (Ull)T[0].l[1];
      D[2].ll = (Ull)T[3].l[0] + (Ull)T[2].l[1] + (Ull)T[1].l[1] + (Ull)D[1].l[1];
      D[3].ll = (Ull)T[3].l[1] + (Ull)D[2].l[1];
      D0 = ((Ull)D[3].l[0]<<32)|(Ull)D[2].l[0];
      break;
    }

    c[cid].imlpipe[0].v = 1;
    c[cid].imlpipe[0].tid = tid;
    c[cid].imlpipe[0].rob = rob;
    if (rob->dr[1].t) {
      c[cid].imlpipe[0].dr1t = 1;
      c[cid].imlpipe[0].dr1v = D0;
    }
    else
      c[cid].imlpipe[0].dr1t = 0;

    return (ROB_ISSUED); /* issued */
  }

  else { /* DIV */
    if (c[cid].divque.v)
      return (ROB_MAPPED); /* no change */

    switch (rob->opcd) {
    case 8: /* UDIV */
      if (!rob->dbl) {
	s1 &= 0x00000000ffffffffLL;
	s2 &= 0x00000000ffffffffLL;
      }
      if (s2)
	D0 = s1 / s2;
      else
	D0 == 0LL;
      break;
    case 9: /* SDIV */
      if (!rob->dbl) {
	s2 = (Sll)(s2<<32)>>32;
	s3 = (Sll)(s3<<32)>>32;
      }
      if (s2)
	D0 = (Sll)s1 / (Sll)s2;
      else
	D0 == 0LL;
      break;
    }

    c[cid].divque.v = 1;
    c[cid].divque.t = CORE_DIVDELAY;
    c[cid].divque.tid = tid;
    c[cid].divque.rob = rob;
    if (rob->dr[1].t) {
      c[cid].divque.dr1t = 1;
      c[cid].divque.dr1v = D0;
    }
    else
      c[cid].divque.dr1t = 0;

    return (ROB_ISSUED); /* issued */
  }
}
