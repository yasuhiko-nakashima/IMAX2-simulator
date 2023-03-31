
static char RcsHeader[] = "$Header: /usr/home/nakashim/proj-arm64/src/csim/RCS/decode.c,v 1.109 2022/10/11 08:06:49 nakashim Exp nakashim $";

/* ARM Instruction Simulator           */
/*        Copyright (C) 2007 by NAIST. */
/*         Primary writer: Y.Nakashima */
/*                nakashim@is.naist.jp */

/* decode.c 2005/3/22 */ 

/* armi[Q_ARMI] から hst1[Q_HSTC] へ */

#include "csim.h"

insn_decode(tid, mc, pc, i, rob) Uint tid, mc, pc; union armf i; struct rob *rob;
{
  /* return 0:normal end, 1:continue (ex. LDP/STP), 2:error */
  Uint cid = tid2cid(tid);
  Uint robid = rob-&c[cid].rob[0];
  Uint s, rm, obits, bits;
  Uint s1, s2, s3, v0, v1;
  int  j, k, l;

  rob->stat   = ROB_MAPPED;
  rob->term   = 1;
  rob->tid    = tid;
  rob->pc     = pc;
  rob->target = pc+4;
  rob->bpred  = 0; /* not taken */
  rob->cond   = 14; /* always */
  rob->sop    = 0;
  rob->ptw    = 0;
  rob->size   = 0;
  rob->idx    = 0;
  rob->rep    = 0;
  rob->dir    = 0;
  rob->iinv   = 0; /* invert operand2 */
  rob->oinv   = 0; /* invert result */
  rob->dbl    = 0; /* 0:single, 1:double */
  rob->plus   = 0; /* EAG */
  rob->pre    = 0; /* EAG */
  rob->wb     = 0; /* EAG */
  rob->updt   = 0; /* update CC */
  rob->sr[0].t  = 0; /* src1 */
  rob->sr[1].t  = 0; /* src2 */
  rob->sr[2].t  = 0; /* src3 */
  rob->sr[3].t  = 0; /* aux-src */
  rob->sr[4].t  = 0; /* aux-src */
  rob->sr[5].t  = 0; /* in-cc for conditional exec */
  rob->dr[1].t    = 0; /* out-reg */
  rob->dr[1].p    = robid;
  rob->dr[1].mask[0] = 0x00000000;
  rob->dr[1].val[0] = 0x00000000;
  rob->dr[1].mask[1] = 0x00000000;
  rob->dr[1].val[1] = 0x00000000;
  rob->dr[2].t    = 0;
  rob->dr[2].p    = robid;
  rob->dr[2].mask[0] = 0x00000000;
  rob->dr[2].val[0]  = 0x00000000;
  rob->dr[2].mask[1] = 0x00000000;
  rob->dr[2].val[1]  = 0x00000000;
  rob->dr[3].t    = 0; /* out-cc */
  rob->dr[3].p    = robid;
  rob->dr[3].mask[0] = 0x00000000;
  rob->dr[3].val[0]  = 0x00000000;
  rob->dr[3].mask[1] = 0x00000000;
  rob->dr[3].val[1]  = 0x00000000;
  rob->ls_addr  = 0;
  rob->stbf.t   = 0;

  /* 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00 */
  /*  1     0  0  1  0  0  0  0  0  0                 1  1  1  1  1  1 stlxr                         */
  /*  1     0  0  1  0  0  0  0  1  0  1  1  1  1  1  1  1  1  1  1  1 ldaxr                         */
  /*           0  1  0  1  0           and_shifted                                                   */
  /*           0  1  0  1  1        0  add_sub_shifted                                               */
  /*           0  1  0  1  1  0  0  1  add_sub_extreg                                                */
  /*        1  0  1  0  0              ldp_stp                                                       */
  /*        1  0  1  1  0              ldp_stp(simd&fp)                                              */
  /*  0     0  0  1  1  0  0        0  ld1_st1_mult                                                  */
  /*  0     0  0  1  1  0  1           ld1_st1_sing   x  x     x                                     */
  /*  0     0  0  1  1  0  1           ld1_st1_(repl) 1  1     0                                     */
  /*  0     0  0  1  1  1  0  0  0  0                 0  0  0  0  0  1 dup_vec                       */
  /*  0     0  0  1  1  1  0  0  0  0                 0  0  0  0  1  1 dup_general                   */
  /*  0  1  0  0  1  1  1  0  0  0  0                 0  0  0  1  1  1 mov/ins                       */
  /*  0     0  0  1  1  1  0  0  0  0                 0  0  1  1  1  1 mov/umov                      */
  /*  0     0  0  1  1  1  0        1                 0  0  0  1  1  1 and/orr(vector)               */
  /*  0     0  0  1  1  1  0        1  0  0  0  0  1  0  0  1  0  1  0 xtn,xtn2                      */
  /*  0        0  1  1  1  0        1                 0  1  1  0     1 maxmin                        */
  /*  0        0  1  1  1  0        1                 1  0  0  0  0  1 add/sub(vector)               */
  /*  0        0  1  1  1  0        1                 1  0  0  1  0  1 mla/mls(vector)               */
  /*  0        0  1  1  1  0        1                 1  1  0  1  0  1 fadd(vector)                  */
  /*  0     0  0  1  1  1  0        1                 1  1  0  0  1  1 fmla(vector)                  */
  /*  0        0  1  1  1  0        1                 1  0  0  0  1  1 cmtst/eq(vector)              */
  /*  0        0  1  1  1  0        1                 0  0  1  1  0  1 cmgt/hi(vector)               */
  /*  0        0  1  1  1  0        1                 0  0  1  1  1  1 cmge/hs(vector)               */
  /*  0     1  0  1  1  1  0        1                 0  0  0  1  1  1 bitwise(eor/bsl/bit/bif)      */
  /*  0     1  0  1  1  1  0  0     1                 1  1  0  1  1  1 fmul(vector)                  */
  /*  0        0  1  1  1  1  0  0  0  0  0                       0  1 movi                          */
  /*  0        0  1  1  1  1  0  -immh!=0--           0  0  0  0  0  1 sshr,ushr(vector)             */
  /*  0        0  1  1  1  1  0  -immh!=0--           1  0  1  0  0  1 ushll,ushll2                  */
  /*           1  0  0  0  0           adr                                                           */
  /*           1  0  0  0  1           add_sub_imm                                                   */
  /*           1  0  0  1  0  0        and_imm                                                       */
  /*           1  0  0  1  0  1        mov                                                           */
  /*           1  0  0  1  1  0        sft_imm                                                       */
  /*     0  0  1  0  0  1  1  1     0  ror_imm                                                       */
  /*     0  0  1  0  1                 bl                                                            */
  /*     0  1  1  0  1  0              cbz                                                           */
  /*     0  1  1  0  1  1              tbz                                                           */
  /*  0  1  0  1  0  1  0  0           b_cond                                          0             */
  /*  1  1  0  1  0  1  0  0  0  0  0                                  svc             0  0  0  0  1 */
  /*  1  1  0  1  0  1  1  0  0        1  1  1  1  1  0  0  0  0  0  0 blr             0  0  0  0  0 */
  /*        0  1  1  0  0  0           ldr_str_literal                                               */
  /*        1  1  1  0  0  0        1  ldr_str_reg                1  0                               */
  /*        1  1  1  1  0  0        1  ldr_str_reg(simd)          1  0                               */
  /*        1  1  1  0  0  0        0  ldr_str_unsc               0  0                               */
  /*        1  1  1  0  0  0        0  ldr_str_imm(post)          0  1                               */
  /*        1  1  1  0  0  0        0  ldr_str_imm(pre)           1  1                               */
  /*        1  1  1  0  0  1           ldr_str_imm(uns)                                              */
  /*           1  1  0  1  0  0  0  0  adc_sbc        0  0  0  0  0  0                               */
  /*        1  1  1  0  1  0  0  1  0  ccm                                             0             */
  /*        0  1  1  0  1  0  1  0  0  csel                       0                                  */
  /*     0  0  1  1  0  1  0  1  1  0  sft_reg        0  0  1  0                                     */
  /*  1  0  0  1  1  0  1  0  1  1  0                 0  0  0  0  1  0 udiv                          */
  /*  1  0  0  1  1  0  1  0  1  1  0                 0  0  0  0  1  1 sdiv                          */
  /*     1  0  1  1  0  1  0  1  1  0  0  0  0  0  0  0  0  0  0       rev                           */
  /*     1  0  1  1  0  1  0  1  1  0  0  0  0  0  0  0  0  0  1  0    cls                           */
  /*     0  0  1  1  0  1  1  0  0  0  mad                                                           */
  /*  1  0  0  1  1  0  1  1     0  1  mad                                                           */
  /*  1  0  0  1  1  0  1  1  0  1  0                 0  1  1  1  1  1 smulh                         */
  /*  1  0  0  1  1  0  1  1  1  1  0                 0  1  1  1  1  1 umulh                         */
  /*        0  1  1  1  0  0           ldr_str_literal(simd&fp)                                      */
  /*        1  1  1  1  0  0     1  0  ldur(simd&fp)              0  0                               */
  /*        1  1  1  1  0  0        0  ldr_str_imm(post,simd&fp)  0  1                               */
  /*        1  1  1  1  0  0        0  ldr_str_imm(pre,simd&fp)   1  1                               */
  /*        1  1  1  1  0  1           ldr_str_imm(uns,simd&fp)                                      */
  /*     0  0  1  1  1  1  0        1  fmov(general)  0  0  0  0  0  0                               */
  /*  0  0  0  1  1  1  1  0        1  fmov(scalar,immediate)  1  0  0  0  0  0  0  0                */
  /*  0  0  0  1  1  1  1  0        1  fadds          0  0  1     1  0                               */
  /*  0  0  0  1  1  1  1  0        1  fmul(scalar)      0  0  0  1  0                               */
  /*  0  0  0  1  1  1  1  0        1  fdiv(scalar)   0  0  0  1  1  0                               */
  /*  0  0  0  1  1  1  1  0  0  x  1                 0  0  1  0  0  0 fcmp            0  x  0  0  0 */
  /*  0  0  0  1  1  1  1  0  0  x  1                 0  0  1  0  0  0 fcmpe           1  x  0  0  0 */
  /*  0  0  0  1  1  1  1  0  0  x  1  0  0  0  0        1  0  0  0  0 fmov(register)                */
  /*  0  0  0  1  1  1  1  0        1  0  0  0  1        1  0  0  0  0 fcvt(scalar)                  */
  /*  0  0  0  1  1  1  1  0  0  x  1  0  0  1  0  1  0  1  0  0  0  0 frintm(scalar)                */
  /*  0  1     1  1  1  1  0        1                 1  0  0  0  0  1 add/sub(scalar)               */
  /*  0  1     1  1  1  1  0  1  1  1                 1  0  0  0  1  1 cmtst/eq(scalar)              */
  /*  0  1     1  1  1  1  0  1  1  1                 0  0  1  1  0  1 cmgt/hi(scalar)               */
  /*  0  1     1  1  1  1  0  1  1  1                 0  0  1  1  1  1 cmge/hs(scalar)               */
  /*  0  0  0  1  1  1  1  1                                           fmadd                         */
  /*  0  1     1  1  1  1  1  0  -immh!=0--           0  0  0  0  0  1 sshr,ushr(scalar)             */
  /*  0  1  0  1  1  1  1  1  0  -immh!=0--           0  1  0  1  0  1 shl(scalar)                   */

  if (i.adr.op28_24==0x10) { /* C6.6.9 ADR, C6.6.10 ADRP */
    Ull offset = i.adr.op ? (Sll)(((Ull)i.adr.immhi<<45)|((Ull)i.adr.immlo<<43))>>31 : (Sll)(((Ull)i.adr.immhi<<45)|((Ull)i.adr.immlo<<43))>>43;
    rob->type  = 0; /* ALU */
    rob->opcd  = 4; /* ADD */
    rob->sop   = 0; /* LSL */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = 1; /* always 64bit */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    rob->sr[0].t = 1; /* PC */
    rob->sr[0].x = 0; /* renamig N.A. */
    rob->sr[0].n = i.adr.op ? pc&0xfffff000 : pc; /* PC */
    rob->sr[1].t = 1; /* immediate */
    rob->sr[1].x = 0; /* renamig N.A. */
    rob->sr[1].n = offset;
    rob->sr[2].t = 1; /* immediate */
    rob->sr[2].x = 0; /* renamig N.A. */
    rob->sr[2].n = 0; /* LSL 0 */
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = i.adr.rd;
    t[tid].map[i.adr.rd].x = 1;
    t[tid].map[i.adr.rd].rob = robid;
    return (0);
  }
  else if (i.add_sub_imm.op28_24==0x11) { /* C6.6.4 ADD(immediate), C6.6.7 ADDS(immediate), C6.6.42 CMN(immediate), C6.6.45 CMP(immediate), C6.6.121 MOV(to/from SP), C6.6.195 SUB(immediate), C6.6.198 SUBS(immediate) */
    rob->type  = 0; /* ALU */
    rob->opcd  = i.add_sub_imm.op==0?4:2; /* ADD/SUB */
    rob->sop   = 0; /* LSL */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.add_sub_imm.sf; /* 0:add32, 1:add64 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = i.add_sub_imm.S;
    rob->sr[0].t = !t[tid].map[i.add_sub_imm.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.add_sub_imm.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[i.add_sub_imm.rn].x; /* reg */
    rob->sr[0].n = !t[tid].map[i.add_sub_imm.rn].x ? i.add_sub_imm.rn : t[tid].map[i.add_sub_imm.rn].rob;
    rob->sr[1].t = 1; /* immediate */
    rob->sr[1].x = 0; /* renamig N.A. */
    rob->sr[1].n = i.add_sub_imm.shift==0?(Ull)i.add_sub_imm.imm12:(Ull)(i.add_sub_imm.imm12<<12);
    rob->sr[2].t = 1; /* immediate */
    rob->sr[2].x = 0; /* renamig N.A. */
    rob->sr[2].n = 0; /* LSL 0 */
    /* dest */
    if (i.add_sub_imm.rd!=31 || !i.add_sub_imm.S) {
      rob->dr[1].t = 1;
      rob->dr[1].n = i.add_sub_imm.rd;
      t[tid].map[i.add_sub_imm.rd].x = 1;
      t[tid].map[i.add_sub_imm.rd].rob = robid;
    }
    if (i.add_sub_imm.S) {
      rob->dr[3].t = 1; /* nzcv */
      rob->dr[3].n = CPSREGTOP;
      t[tid].map[CPSREGTOP].x = 3;
      t[tid].map[CPSREGTOP].rob = robid;
    }
    return (0);
  }
  else if (i.adc_sbc.op28_21==0xd0 && i.adc_sbc.op15_10==0) { /* C6.6.1 ADC, C6.6.2 ADCS, C6.6.137 NGC, C6.6.138 NGCS, C6.6.155 SBC, C6.6.156 SBCS */
    rob->type  = 0; /* ALU */
    rob->opcd  = i.adc_sbc.op==0?5:6; /* ADC/SBC */
    rob->sop   = 0; /* LSL */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.adc_sbc.sf; /* 0:adc32, 1:adc64 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = i.adc_sbc.S;
    rob->sr[0].t = i.adc_sbc.rn==31 ? 1 : /* ZERO */
                   !t[tid].map[i.adc_sbc.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.adc_sbc.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = i.adc_sbc.rn==31 ? 0 : /* ZERO */
                   t[tid].map[i.adc_sbc.rn].x; /* reg */
    rob->sr[0].n = i.adc_sbc.rn==31 ? 0 : /* ZERO */
                   !t[tid].map[i.adc_sbc.rn].x ? i.adc_sbc.rn : t[tid].map[i.adc_sbc.rn].rob;
    rob->sr[1].t = i.adc_sbc.rm==31 ? 1 : /* ZERO */
                   !t[tid].map[i.adc_sbc.rm].x ? 2 :
                   (c[cid].rob[t[tid].map[i.adc_sbc.rm].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = i.adc_sbc.rm==31 ? 0 : /* ZERO */
                   t[tid].map[i.adc_sbc.rm].x; /* reg */
    rob->sr[1].n = i.adc_sbc.rm==31 ? 0 : /* ZERO */
                   !t[tid].map[i.adc_sbc.rm].x ? i.adc_sbc.rm : t[tid].map[i.adc_sbc.rm].rob;
    rob->sr[2].t = 1; /* immediate */
    rob->sr[2].x = 0; /* renamig N.A. */
    rob->sr[2].n = 0; /* LSL 0 */
    rob->sr[5].t = !t[tid].map[CPSREGTOP].x ? 2 : (c[cid].rob[t[tid].map[CPSREGTOP].rob].stat>=ROB_COMPLETE)? 2 : 3; /* cpsr */
    rob->sr[5].x =  t[tid].map[CPSREGTOP].x; /* cpsr */
    rob->sr[5].n = !t[tid].map[CPSREGTOP].x ? CPSREGTOP : t[tid].map[CPSREGTOP].rob;
    /* dest */
    if (i.adc_sbc.rd!=31) {
      rob->dr[1].t = 1;
      rob->dr[1].n = i.adc_sbc.rd;
      t[tid].map[i.adc_sbc.rd].x = 1;
      t[tid].map[i.adc_sbc.rd].rob = robid;
    }
    if (i.adc_sbc.S) {
      rob->dr[3].t = 1; /* nzcv */
      rob->dr[3].n = CPSREGTOP;
      t[tid].map[CPSREGTOP].x = 3;
      t[tid].map[CPSREGTOP].rob = robid;
    }
    return (0);
  }
  else if (i.csel.op29_21==0x0d4 && i.csel.op11==0) { /* C6.6.36 CINC, C6.6.37 CINV, C6.6.47 CNEG, C6.6.50 CSEL, C6.6.51 CSET, C6.6.52 CSETM, C6.6.53 CSINC, C6.6.54 CSINV, C6.6.55 CSNEG */
    rob->type  = 0; /* ALU */
    rob->opcd  =10; /* CSEL */
    rob->sop   = 0; /* LSL */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = i.csel.op; /* 0:result, 1:~result */
    rob->dbl   = i.csel.sf; /* 0:csel32, 1:csel64 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    rob->sr[0].t = i.csel.rn==31 ? 1 : /* ZERO */
                   !t[tid].map[i.csel.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.csel.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = i.csel.rn==31 ? 0 : /* ZERO */
                   t[tid].map[i.csel.rn].x; /* reg */
    rob->sr[0].n = i.csel.rn==31 ? 0 : /* ZERO */
                   !t[tid].map[i.csel.rn].x ? i.csel.rn : t[tid].map[i.csel.rn].rob;
    rob->sr[1].t = i.csel.rm==31 ? 1 : /* ZERO */
                   !t[tid].map[i.csel.rm].x ? 2 :
                   (c[cid].rob[t[tid].map[i.csel.rm].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = i.csel.rm==31 ? 0 : /* ZERO */
                   t[tid].map[i.csel.rm].x; /* reg */
    rob->sr[1].n = i.csel.rm==31 ? 0 : /* ZERO */
                   !t[tid].map[i.csel.rm].x ? i.csel.rm : t[tid].map[i.csel.rm].rob;
    rob->sr[2].t = 1; /* immediate */
    rob->sr[2].x = 0; /* renamig N.A. */
    rob->sr[2].n = 0; /* LSL 0 */
    rob->sr[3].t = 1; /* immediate */
    rob->sr[3].x = 0; /* renamig N.A. */
    rob->sr[3].n = i.csel.cond;
    rob->sr[4].t = 1; /* immediate */
    rob->sr[4].x = 0; /* renamig N.A. */
    rob->sr[4].n = i.csel.o2; /* o2(inc=1) */
    rob->sr[5].t = !t[tid].map[CPSREGTOP].x ? 2 : (c[cid].rob[t[tid].map[CPSREGTOP].rob].stat>=ROB_COMPLETE)? 2 : 3; /* cpsr */
    rob->sr[5].x =  t[tid].map[CPSREGTOP].x; /* cpsr */
    rob->sr[5].n = !t[tid].map[CPSREGTOP].x ? CPSREGTOP : t[tid].map[CPSREGTOP].rob;
    /* dest */
    if (i.csel.rd!=31) {
      rob->dr[1].t = 1;
      rob->dr[1].n = i.csel.rd;
      t[tid].map[i.csel.rd].x = 1;
      t[tid].map[i.csel.rd].rob = robid;
    }
    return (0);
  }
  else if (i.ccm.op29_21==0x1d2 && i.ccm.op4==0) { /* C6.6.32 CCMN(immediate), C6.6.33 CCMN(register), C6.6.34 CCMP(immediate), C6.6.35 CCMP(register) */
    rob->type  = 0; /* ALU */
    rob->opcd  =11; /* CCMN/CCMP */
    rob->sop   = 0; /* LSL */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = i.ccm.op==1; /* CCMP */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.ccm.sf; /* 0:ccmp32, 1:ccmp64 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    rob->sr[0].t = i.ccm.rn==31 ? 1 : /* ZERO */
                   !t[tid].map[i.ccm.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.ccm.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = i.ccm.rn==31 ? 0 : /* ZERO */
                   t[tid].map[i.ccm.rn].x; /* reg */
    rob->sr[0].n = i.ccm.rn==31 ? 0 : /* ZERO */
                   !t[tid].map[i.ccm.rn].x ? i.ccm.rn : t[tid].map[i.ccm.rn].rob;
    switch (i.ccm.op11_10) {
    case 0: /* register */
      rob->sr[1].t = i.ccm.rm_imm5==31 ? 1 : /* ZERO */
                     !t[tid].map[i.ccm.rm_imm5].x ? 2 :
                     (c[cid].rob[t[tid].map[i.ccm.rm_imm5].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[1].x = i.ccm.rm_imm5==31 ? 0 : /* ZERO */
                     t[tid].map[i.ccm.rm_imm5].x; /* reg */
      rob->sr[1].n = i.ccm.rm_imm5==31 ? 0 : /* ZERO */
                     !t[tid].map[i.ccm.rm_imm5].x ? i.ccm.rm_imm5 : t[tid].map[i.ccm.rm_imm5].rob;
      break;
    case 2: /* immediate */
      rob->sr[1].t = 1; /* immediate */
      rob->sr[1].x = 0; /* renamig N.A. */
      rob->sr[1].n = i.ccm.rm_imm5;
      break;
    default:
      i_xxx(rob);
      return (0);
    }
    rob->sr[2].t = 1; /* immediate */
    rob->sr[2].x = 0; /* renamig N.A. */
    rob->sr[2].n = 0; /* LSL 0 */
    rob->sr[3].t = 1; /* immediate */
    rob->sr[3].x = 0; /* renamig N.A. */
    rob->sr[3].n = i.ccm.cond;
    rob->sr[4].t = 1; /* immediate */
    rob->sr[4].x = 0; /* renamig N.A. */
    rob->sr[4].n = i.ccm.nzcv;
    rob->sr[5].t = !t[tid].map[CPSREGTOP].x ? 2 : (c[cid].rob[t[tid].map[CPSREGTOP].rob].stat>=ROB_COMPLETE)? 2 : 3; /* cpsr */
    rob->sr[5].x =  t[tid].map[CPSREGTOP].x; /* cpsr */
    rob->sr[5].n = !t[tid].map[CPSREGTOP].x ? CPSREGTOP : t[tid].map[CPSREGTOP].rob;
    /* dest */
    rob->dr[3].t = 1; /* nzcv */
    rob->dr[3].n = CPSREGTOP;
    t[tid].map[CPSREGTOP].x = 3;
    t[tid].map[CPSREGTOP].rob = robid;
    return (0);
  }
  else if (i.mov.op28_23==0x25) { /* C6.6.122 MOV(inverted wide immediate), C6.6.123 MOV(wide immediate), C6.6.126 MOVK, C6.6.127 MOVN, C6.6.128 MOVZ */
    rob->type  = 0; /* ALU */
    rob->opcd  = 7; /* MOVN, MOVZ, MOVK */
    rob->sop   = 0; /* LSL */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = i.mov.opc==0; /* MOVN */
    rob->dbl   = i.mov.sf; /* 0:add32, 1:add64 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    {
      Uint pos = i.mov.hw<<4;
      Uint inzero;
      switch (i.mov.opc) {
      case 0: inzero=1; break; /* MOVN */
      case 2: inzero=1; break; /* MOVZ */
      case 3: inzero=0; break; /* MOVK */
      default:
        i_xxx(rob);
        return (0);
      }
      rob->sr[0].t = inzero ? 1 : /* ZERO */
                     !t[tid].map[i.mov.rd].x ? 2 :
                     (c[cid].rob[t[tid].map[i.mov.rd].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[0].x = inzero ? 0 : /* ZERO */
                     t[tid].map[i.mov.rd].x; /* reg */
      rob->sr[0].n = inzero ? 0 : /* ZERO */
                     !t[tid].map[i.mov.rd].x ? i.mov.rd : t[tid].map[i.mov.rd].rob;
      rob->sr[1].t = 1; /* immediate */
      rob->sr[1].x = 0; /* renamig N.A. */
      rob->sr[1].n = (Ull)i.mov.imm16<<pos; /* POS 0,16,32,48 -> wmask in alu.c */
      rob->sr[2].t = 1; /* immediate */
      rob->sr[2].x = 0; /* renamig N.A. */
      rob->sr[2].n = 0; /* LSL 0 */
      rob->sr[3].t = 1; /* immediate */
      rob->sr[3].x = 0; /* renamig N.A. */
      rob->sr[3].n = 0x000000000000FFFFLL<<pos; /* POS 0,16,32,48 -> wmask in alu.c */
    }
    /* dest */
    if (i.mov.rd!=31) {
      rob->dr[1].t = 1;
      rob->dr[1].n = i.mov.rd;
      t[tid].map[i.mov.rd].x = 1;
      t[tid].map[i.mov.rd].rob = robid;
    }
    return (0);
  }
  else if (i.rev.op30_12==0x5ac00) { /* C6.6.147 RBIT, C6.6.149 REV, C6.6.150 REV16, C6.6.151 REV32 */
    rob->type  = 0; /* ALU */
    rob->opcd  =12; /* REV */
    rob->sop   = 0; /* LSL */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.rev.sf; /* 0:rev32, 1:rev64 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    rob->sr[0].t = i.rev.rn==31 ? 1 : /* ZERO */
                   !t[tid].map[i.rev.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.rev.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = i.rev.rn==31 ? 0 : /* ZERO */
                   t[tid].map[i.rev.rn].x; /* reg */
    rob->sr[0].n = i.rev.rn==31 ? 0 : /* ZERO */
                   !t[tid].map[i.rev.rn].x ? i.rev.rn : t[tid].map[i.rev.rn].rob;
    rob->sr[1].t = 1; /* immediate */
    rob->sr[1].x = 0; /* renamig N.A. */
    rob->sr[1].n = i.rev.opc; /* 0:REV, 1:REV16, 2:REV32, 3:REV64 */
    rob->sr[2].t = 1; /* immediate */
    rob->sr[2].x = 0; /* renamig N.A. */
    rob->sr[2].n = 0; /* LSL 0 */
    /* dest */
    if (i.rev.rd!=31) {
      rob->dr[1].t = 1;
      rob->dr[1].n = i.rev.rd;
      t[tid].map[i.rev.rd].x = 1;
      t[tid].map[i.rev.rd].rob = robid;
    }
    return (0);
  }
  else if (i.cls.op30_11==0xb5802) { /* C6.6.39 CLS, C6.6.40 CLZ */
    rob->type  = 0; /* ALU */
    rob->opcd  =13; /* CLS/CLZ */
    rob->sop   = 0; /* LSL */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.cls.sf; /* 0:cls32, 1:cls64 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    rob->sr[0].t = i.cls.rn==31 ? 1 : /* ZERO */
                   !t[tid].map[i.cls.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.cls.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = i.cls.rn==31 ? 0 : /* ZERO */
                   t[tid].map[i.cls.rn].x; /* reg */
    rob->sr[0].n = i.cls.rn==31 ? 0 : /* ZERO */
                   !t[tid].map[i.cls.rn].x ? i.cls.rn : t[tid].map[i.cls.rn].rob;
    rob->sr[1].t = 1; /* immediate */
    rob->sr[1].x = 0; /* renamig N.A. */
    rob->sr[1].n = i.cls.op; /* 0:CLZ, 1:CLS */
    rob->sr[2].t = 1; /* immediate */
    rob->sr[2].x = 0; /* renamig N.A. */
    rob->sr[2].n = 0; /* LSL 0 */
    /* dest */
    if (i.cls.rd!=31) {
      rob->dr[1].t = 1;
      rob->dr[1].n = i.cls.rd;
      t[tid].map[i.cls.rd].x = 1;
      t[tid].map[i.cls.rd].rob = robid;
    }
    return (0);
  }
  else if (i.add_sub_shifted.op28_24==0x0b && i.add_sub_shifted.op21==0) { /* C6.6.5 ADD(shifted register), C6.6.8 ADDS(shifted register), C6.6.43 CMN(shifted register), C6.6.46 CMP(shifted register), C6.6.135 NEG(shifted register), C6.6.136 NEGS(shifted register), C6.6.196 SUB(shifted register), C6.6.199 SUBS(shifted register) */
    rob->type  = 0; /* ALU */
    rob->opcd  = i.add_sub_shifted.op==0?4:2; /* ADD/SUB */
    rob->sop   = i.add_sub_shifted.shift; /* LSL/LSR/ASR/ROR */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.add_sub_shifted.sf; /* 0:add32, 1:add64 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = i.add_sub_shifted.S; /* updtCC */
    rob->sr[0].t = i.add_sub_shifted.rn==31 ? 1 : /* ZERO */
                   !t[tid].map[i.add_sub_shifted.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.add_sub_shifted.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = i.add_sub_shifted.rn==31 ? 0 : /* ZERO */
                   t[tid].map[i.add_sub_shifted.rn].x; /* reg */
    rob->sr[0].n = i.add_sub_shifted.rn==31 ? 0 : /* ZERO */
                   !t[tid].map[i.add_sub_shifted.rn].x ? i.add_sub_shifted.rn : t[tid].map[i.add_sub_shifted.rn].rob;
    rob->sr[1].t = i.add_sub_shifted.rm==31 ? 1 : /* ZERO */
                   !t[tid].map[i.add_sub_shifted.rm].x ? 2 :
                   (c[cid].rob[t[tid].map[i.add_sub_shifted.rm].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = i.add_sub_shifted.rm==31 ? 0 : /* ZERO */
                   t[tid].map[i.add_sub_shifted.rm].x; /* reg */
    rob->sr[1].n = i.add_sub_shifted.rm==31 ? 0 : /* ZERO */
                   !t[tid].map[i.add_sub_shifted.rm].x ? i.add_sub_shifted.rm : t[tid].map[i.add_sub_shifted.rm].rob;
    rob->sr[2].t = 1; /* immediate */
    rob->sr[2].x = 0; /* renamig N.A. */
    rob->sr[2].n = i.add_sub_shifted.imm6; /* LSL/LSR/ASR/ROR */
    /* dest */
    if (i.add_sub_shifted.rd!=31) {
      rob->dr[1].t = 1;
      rob->dr[1].n = i.add_sub_shifted.rd;
      t[tid].map[i.add_sub_shifted.rd].x = 1;
      t[tid].map[i.add_sub_shifted.rd].rob = robid;
    }
    if (i.add_sub_shifted.S) { /* updtCC */
      rob->dr[3].t = 1; /* nzcv */
      rob->dr[3].n = CPSREGTOP;
      t[tid].map[CPSREGTOP].x = 3;
      t[tid].map[CPSREGTOP].rob = robid;
    }
    return (0);
  }
  else if (i.add_sub_extreg.op28_24==0x0b && i.add_sub_extreg.op23_22==0 && i.add_sub_extreg.op21==1) { /* C6.6.3 ADD(extended register), C6.6.6 ADDS(extended register), C6.6.41 CMN(extended register), C6.6.44 CMP(extended register), C6.6.194 SUB(extended register), C6.6.197 SUBS(extended register) */
    rob->type  = 0; /* ALU */
    rob->opcd  = i.add_sub_extreg.op==0?4:2; /* ADD/SUB */
    rob->sop   = 8|i.add_sub_extreg.option; /* 0:uxtb, 1:uxth, 2:uxtw, 3:uxtx, 4:sxtb, 5:sxth, 6:sxtw, 7:sxtx */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.add_sub_extreg.sf; /* 0:add32, 1:add64 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = i.add_sub_extreg.S; /* updtCC */
    rob->sr[0].t = !t[tid].map[i.add_sub_extreg.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.add_sub_extreg.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[i.add_sub_extreg.rn].x; /* reg */
    rob->sr[0].n = !t[tid].map[i.add_sub_extreg.rn].x ? i.add_sub_extreg.rn : t[tid].map[i.add_sub_extreg.rn].rob;
    rob->sr[1].t = i.add_sub_extreg.rm==31 ? 1 : /* ZERO */
                   !t[tid].map[i.add_sub_extreg.rm].x ? 2 :
                   (c[cid].rob[t[tid].map[i.add_sub_extreg.rm].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = i.add_sub_extreg.rm==31 ? 0 : /* ZERO */
                   t[tid].map[i.add_sub_extreg.rm].x; /* reg */
    rob->sr[1].n = i.add_sub_extreg.rm==31 ? 0 : /* ZERO */
                   !t[tid].map[i.add_sub_extreg.rm].x ? i.add_sub_extreg.rm : t[tid].map[i.add_sub_extreg.rm].rob;
    rob->sr[2].t = 1; /* immediate */
    rob->sr[2].x = 0; /* renamig N.A. */
    rob->sr[2].n = i.add_sub_extreg.imm3; /* extend_reg_shift(0,1,2,3,4) */
    /* dest */
    if (i.add_sub_extreg.rd!=31 || !i.add_sub_extreg.S) {
      rob->dr[1].t = 1;
      rob->dr[1].n = i.add_sub_extreg.rd;
      t[tid].map[i.add_sub_extreg.rd].x = 1;
      t[tid].map[i.add_sub_extreg.rd].rob = robid;
    }
    if (i.add_sub_extreg.S) { /* updtCC */
      rob->dr[3].t = 1; /* nzcv */
      rob->dr[3].n = CPSREGTOP;
      t[tid].map[CPSREGTOP].x = 3;
      t[tid].map[CPSREGTOP].rob = robid;
    }
    return (0);
  }
  else if (i.and_shifted.op28_24==0x0a) { /* C6.6.12 AND(shifted register), C6.6.14 ANDS(shifted register), C6.6.24 BIC(shifted register), C6.6.25 BICS(shifted register), C6.6.63 EON(shifted register), C6.6.65 EOR(shifted register), C6.6.125 MOV(register), C6.6.134 MVN(register), C6.6.140 ORN(shifted register), C6.6.142 ORR(shifted register), C6.6.210 TST(shifted register) */
    rob->type  = 0; /* ALU */
    rob->opcd  = i.and_shifted.opc==0?0:i.and_shifted.opc==1?3:i.and_shifted.opc==2?1:0; /* AND/ORR/EOR/ANDS */
    rob->sop   = i.and_shifted.shift; /* LSL/LSR/ASR/ROR */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = i.and_shifted.N; /* invert */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.and_shifted.sf; /* 0:and32, 1:and64 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = i.and_shifted.opc==3; /* ANDS */
    rob->sr[0].t = i.and_shifted.rn==31 ? 1 : /* ZERO */
                   !t[tid].map[i.and_shifted.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.and_shifted.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = i.and_shifted.rn==31 ? 0 : /* ZERO */
                   t[tid].map[i.and_shifted.rn].x; /* reg */
    rob->sr[0].n = i.and_shifted.rn==31 ? 0 : /* ZERO */
                   !t[tid].map[i.and_shifted.rn].x ? i.and_shifted.rn : t[tid].map[i.and_shifted.rn].rob;
    rob->sr[1].t = i.and_shifted.rm==31 ? 1 : /* ZERO */
                   !t[tid].map[i.and_shifted.rm].x ? 2 :
                   (c[cid].rob[t[tid].map[i.and_shifted.rm].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = i.and_shifted.rm==31 ? 0 : /* ZERO */
                   t[tid].map[i.and_shifted.rm].x; /* reg */
    rob->sr[1].n = i.and_shifted.rm==31 ? 0 : /* ZERO */
                   !t[tid].map[i.and_shifted.rm].x ? i.and_shifted.rm : t[tid].map[i.and_shifted.rm].rob;
    rob->sr[2].t = 1; /* immediate */
    rob->sr[2].x = 0; /* renamig N.A. */
    rob->sr[2].n = i.and_shifted.imm6; /* LSL/LSR/ASR/ROR */
    /* dest */
    if (i.and_shifted.rd!=31) {
      rob->dr[1].t = 1;
      rob->dr[1].n = i.and_shifted.rd;
      t[tid].map[i.and_shifted.rd].x = 1;
      t[tid].map[i.and_shifted.rd].rob = robid;
    }
    if (i.and_shifted.opc==3) { /* ANDS */
      rob->dr[3].t = 1; /* nzcv */
      rob->dr[3].n = CPSREGTOP;
      t[tid].map[CPSREGTOP].x = 3;
      t[tid].map[CPSREGTOP].rob = robid;
    }
    return (0);
  }
  else if (i.and_imm.op28_23==0x24) { /* C6.6.11 AND(immediate), C6.6.13 ANDS(immediate), C6.6.64 EOR(immediate), C6.6.124 MOV(bitmask immediate), C6.6.141 ORR(immediate), C6.6.209 TST(immediate) */
    rob->type  = 0; /* ALU */
    rob->opcd  = i.and_imm.opc==0?0:i.and_imm.opc==1?3:i.and_imm.opc==2?1:0; /* AND/ORR/EOR/ANDS */
    rob->sop   = 0; /* LSL */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0;
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.and_imm.sf; /* 0:and32, 1:and64 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = i.and_imm.opc==3; /* ANDS */
    rob->sr[0].t = i.and_imm.rn==31 ? 1 : /* ZERO */
                   !t[tid].map[i.and_imm.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.and_imm.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = i.and_imm.rn==31 ? 0 : /* ZERO */
                   t[tid].map[i.and_imm.rn].x; /* reg */
    rob->sr[0].n = i.and_imm.rn==31 ? 0 : /* ZERO */
                   !t[tid].map[i.and_imm.rn].x ? i.and_imm.rn : t[tid].map[i.and_imm.rn].rob;
    {
      int imm, len, levels, esize;
      Uint S, R;
      Ull welem, wmask;
      imm = (i.and_imm.N<<6)|((~i.and_imm.imms)&0x3f);
      for (len=6; len>=0; len--) {
        if ((imm>>len)&1)
          break;
      }
      if (len < 1) { /* reserved */
        i_xxx(rob);
        return (0);
      }
                                                           /* len=1  len=2  len=3  len=4  len=5  len=6  */
      levels = (Uint)((int)0x80000000>>(len-1))>>(32-len); /* 000001,000011,000111,001111,011111,111111 */
      S = (Uint)(i.and_imm.imms & levels);
      R = (Uint)(i.and_imm.immr & levels);
      esize = 1 << len; /* 2..64 (len=1..6) */
      welem = (Ull)((Sll)0x8000000000000000LL>>S)>>(63-S); /* width:esize-bits */
      if (R>0) welem = (welem<<(esize-R))|(welem>>R);
      wmask = welem;
      for (len=0; len<64; len+=esize)
        wmask = (wmask<<esize) | welem;
      if (!i.and_imm.sf)
        wmask &= 0x00000000ffffffffLL;

      rob->sr[1].t = 1; /* immediate */
      rob->sr[1].x = 0; /* renamig N.A. */
      rob->sr[1].n = wmask;
    }
    rob->sr[2].t = 1; /* immediate */
    rob->sr[2].x = 0; /* renamig N.A. */
    rob->sr[2].n = 0; /* LSL 0 */
    /* dest */
    if (i.and_imm.rd!=31 || i.and_imm.opc!=3) {
      rob->dr[1].t = 1;
      rob->dr[1].n = i.and_imm.rd;
      t[tid].map[i.and_imm.rd].x = 1;
      t[tid].map[i.and_imm.rd].rob = robid;
    }
    if (i.and_imm.opc==3) { /* ANDS */
      rob->dr[3].t = 1; /* nzcv */
      rob->dr[3].n = CPSREGTOP;
      t[tid].map[CPSREGTOP].x = 3;
      t[tid].map[CPSREGTOP].rob = robid;
    }
    return (0);
  }
  else if (i.sft_imm.op28_23==0x26) { /* C6.6.16 ASR(immediate), C6.6.21 BFI, C6.6.22 BFM, C6.6.23 BFXIL, C6.6.114 LSL(immediate), C6.6.117 LSR(immediate), C6.6.157 SBFIZ, C6.6.158 SBFM, C6.6.159 SBFX, C6.6.201 SXTB, C6.6.202 SXTH, C6.6.203 SXTW, C6.6.211 UBFIZ, C6.6.212 UBFM, C6.6.213 UBFX, C6.6.220 UXTB, C6.6.221 UXTH */
    rob->type  = 0; /* ALU */
    rob->opcd  = i.sft_imm.opc==0?8:9; /* 8:SBFM/9:BFM/UBFM */
    rob->sop   = 3; /* ROR */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0;
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.sft_imm.sf; /* 0:sft32, 1:sft64 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    {
      int imm, len, levels, esize;
      Uint S, R, d, inzero, extend;
      Ull welem, wmask, telem, tmask;
      imm = (i.sft_imm.N<<6)|((~i.sft_imm.imms)&0x3f);
      for (len=6; len>=0; len--) {
        if ((imm>>len)&1)
          break;
      }
      if (len < 1) { /* reserved */
        i_xxx(rob);
        return (0);
      }
                                                           /* len=1  len=2  len=3  len=4  len=5  len=6  */
      levels = (Uint)((int)0x80000000>>(len-1))>>(32-len); /* 000001,000011,000111,001111,011111,111111 */
      S = (Uint)(i.sft_imm.imms & levels);
      R = (Uint)(i.sft_imm.immr & levels);
      esize = 1 << len; /* 2..64 (len=1..6) */
      /*welem = ZeroExtend(Ones(S + 1), esize);*/
      /*wmask = Replicate(ROR(welem, R));*/
      welem = (Ull)((Sll)0x8000000000000000LL>>S)>>(63-S); /* width:esize-bits */
      if (R>0) welem = (welem<<(esize-R))|(welem>>R);
      wmask = welem;
      for (len=0; len<64; len+=esize)
        wmask = (wmask<<esize) | welem;
      if (!i.sft_imm.sf)
        wmask &= 0x00000000ffffffffLL;

      d = (S - R)&levels;
      /*telem = ZeroExtend(Ones(d + 1), esize);*/
      /*tmask = Replicate(telem);*/
      telem = (Ull)((Sll)0x8000000000000000LL>>d)>>(63-d); /* width:esize-bits */
      tmask = telem;
      for (len=0; len<64; len+=esize)
        tmask = (tmask<<esize) | telem;
      if (!i.sft_imm.sf)
        tmask &= 0x00000000ffffffffLL;

      switch (i.sft_imm.opc) {
      case 0: inzero=1; extend=1; break; /* SBFM */
      case 1: inzero=0; extend=0; break; /* BFM */
      case 2: inzero=1; extend=0; break; /* UBFM */
      default:
        i_xxx(rob);
        return (0);
      }
      rob->sr[0].t = inzero ? 1 : /* ZERO */
                     !t[tid].map[i.sft_imm.rd].x ? 2 :
                     (c[cid].rob[t[tid].map[i.sft_imm.rd].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[0].x = inzero ? 0 : /* ZERO */
                     t[tid].map[i.sft_imm.rd].x; /* reg */
      rob->sr[0].n = inzero ? 0 : /* ZERO */
                     !t[tid].map[i.sft_imm.rd].x ? i.sft_imm.rd : t[tid].map[i.sft_imm.rd].rob;
      rob->sr[1].t = i.sft_imm.rn==31 ? 1 : /* ZERO */
                     !t[tid].map[i.sft_imm.rn].x ? 2 :
                     (c[cid].rob[t[tid].map[i.sft_imm.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[1].x = i.sft_imm.rn==31 ? 0 : /* ZERO */
                     t[tid].map[i.sft_imm.rn].x; /* reg */
      rob->sr[1].n = i.sft_imm.rn==31 ? 0 : /* ZERO */
                     !t[tid].map[i.sft_imm.rn].x ? i.sft_imm.rn : t[tid].map[i.sft_imm.rn].rob;
      rob->sr[2].t = 1; /* immediate */
      rob->sr[2].x = 0; /* renamig N.A. */
      rob->sr[2].n = (S<<8)|R; /* S for sign_extention | R for ROR */
      rob->sr[3].t = 1; /* immediate */
      rob->sr[3].x = 0; /* renamig N.A. */
      rob->sr[3].n = wmask;
      rob->sr[4].t = 1; /* immediate */
      rob->sr[4].x = 0; /* renamig N.A. */
      rob->sr[4].n = tmask;
    }
    /* dest */
    if (i.sft_imm.rd!=31) {
      rob->dr[1].t = 1;
      rob->dr[1].n = i.sft_imm.rd;
      t[tid].map[i.sft_imm.rd].x = 1;
      t[tid].map[i.sft_imm.rd].rob = robid;
    }
    return (0);
  }
  else if (i.ror_imm.op30_23==0x27 && i.ror_imm.op21==0) { /* C6.6.67 EXTR(immediate), C6.6.152 ROR(immediate) */
    rob->type  = 0; /* ALU */
    rob->opcd  =14; /* EXT */
    rob->sop   = 0; /* LSL 0 */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.ror_imm.sf; /* 0:ext32, 1:ext64 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    rob->sr[0].t = i.ror_imm.rn==31 ? 1 : /* ZERO */
                   !t[tid].map[i.ror_imm.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.ror_imm.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = i.ror_imm.rn==31 ? 0 : /* ZERO */
                   t[tid].map[i.ror_imm.rn].x; /* reg */
    rob->sr[0].n = i.ror_imm.rn==31 ? 0 : /* ZERO */
                   !t[tid].map[i.ror_imm.rn].x ? i.ror_imm.rn : t[tid].map[i.ror_imm.rn].rob;
    rob->sr[1].t = i.ror_imm.rm==31 ? 1 : /* ZERO */
                   !t[tid].map[i.ror_imm.rm].x ? 2 :
                   (c[cid].rob[t[tid].map[i.ror_imm.rm].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = i.ror_imm.rm==31 ? 0 : /* ZERO */
                   t[tid].map[i.ror_imm.rm].x; /* reg */
    rob->sr[1].n = i.ror_imm.rm==31 ? 0 : /* ZERO */
                   !t[tid].map[i.ror_imm.rm].x ? i.ror_imm.rm : t[tid].map[i.ror_imm.rm].rob;
    rob->sr[2].t = 1; /* immediate */
    rob->sr[2].x = 0; /* renamig N.A. */
    rob->sr[2].n = 0; /* LSL 0 */
    rob->sr[3].t = 1; /* immediate */
    rob->sr[3].x = 0; /* renamig N.A. */
    rob->sr[3].n = i.ror_imm.imms; /* ROR */
    /* dest */
    if (i.ror_imm.rd!=31) {
      rob->dr[1].t = 1;
      rob->dr[1].n = i.ror_imm.rd;
      t[tid].map[i.ror_imm.rd].x = 1;
      t[tid].map[i.ror_imm.rd].rob = robid;
    }
    return (0);
  }
  else if (i.sft_reg.op30_21==0x0d6 && i.sft_reg.op15_12==2) { /* C6.6.15 ASR, C6.6.17 ASRV, C6.6.113 LSL(register), C6.6.115 LSLV, C6.6.116 LSR(register), C6.6.118 LSRV */
    rob->type  = 0; /* ALU */
    rob->opcd  = 3; /* ORR */
    rob->sop   = i.sft_reg.op2; /* LSL/LSR/ASR/ROR */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.sft_reg.sf; /* 0:lsl32, 1:lsl64 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    rob->sr[0].t = 1; /* immediate */
    rob->sr[0].x = 0; /* renamig N.A. */
    rob->sr[0].n = 0; /* ORR 0 */
    rob->sr[1].t = i.sft_reg.rn==31 ? 1 : /* ZERO */
                   !t[tid].map[i.sft_reg.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.sft_reg.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = i.sft_reg.rn==31 ? 0 : /* ZERO */
                   t[tid].map[i.sft_reg.rn].x; /* reg */
    rob->sr[1].n = i.sft_reg.rn==31 ? 0 : /* ZERO */
                   !t[tid].map[i.sft_reg.rn].x ? i.sft_reg.rn : t[tid].map[i.sft_reg.rn].rob;
    rob->sr[2].t = i.sft_reg.rm==31 ? 1 : /* ZERO */
                   !t[tid].map[i.sft_reg.rm].x ? 2 :
                   (c[cid].rob[t[tid].map[i.sft_reg.rm].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[2].x = i.sft_reg.rm==31 ? 0 : /* ZERO */
                   t[tid].map[i.sft_reg.rm].x; /* reg */
    rob->sr[2].n = i.sft_reg.rm==31 ? 0 : /* ZERO */
                   !t[tid].map[i.sft_reg.rm].x ? i.sft_reg.rm : t[tid].map[i.sft_reg.rm].rob;
    /* dest */
    if (i.sft_reg.rd!=31) {
      rob->dr[1].t = 1;
      rob->dr[1].n = i.sft_reg.rd;
      t[tid].map[i.sft_reg.rd].x = 1;
      t[tid].map[i.sft_reg.rd].rob = robid;
    }
    return (0);
  }
  else if (i.b_cond.op31_24==0x54 && i.b_cond.op4==0) { /* C6.6.19 B.cond */
    rob->target = pc + ((Sll)((Ull)i.b_cond.imm19<<45)>>43);
    if (i.b_cond.cond==14||i.b_cond.cond==15) /* unconditional */
      rob->bpred = 1;
    else if (i.b_cond.imm19&0x40000) /* conditional backward */
      rob->bpred = 1;
    rob->cond  = i.b_cond.cond;
    rob->type  = 5; /* BRANCH */
    rob->opcd  = 0; /* B */
    rob->sop   = 0; /* not used */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = 0; /* not used */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    if (i.b_cond.cond!=14&&i.b_cond.cond!=15) { /* conditional exec */
      rob->sr[5].t = !t[tid].map[CPSREGTOP].x ? 2 : (c[cid].rob[t[tid].map[CPSREGTOP].rob].stat>=ROB_COMPLETE)? 2 : 3; /* cpsr */
      rob->sr[5].x =  t[tid].map[CPSREGTOP].x; /* cpsr */
      rob->sr[5].n = !t[tid].map[CPSREGTOP].x ? CPSREGTOP : t[tid].map[CPSREGTOP].rob;
    }
    return (0);
  }
  else if (i.bl.op30_26==0x05) { /* C6.6.20 B, C6.6.26 BL */
    rob->target = pc + ((Sll)((Ull)i.bl.imm26<<38)>>36);
    rob->bpred = 1;
    rob->type  = 5; /* BRANCH */
    rob->opcd  = i.bl.op; /* B/BL */
    rob->sop   = 0; /* not used */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = 0; /* not used */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    /* dest */
    if (i.bl.op) { /* BL */
      rob->dr[1].t = 1;
      rob->dr[1].n = 30;
      t[tid].map[30].x = 1;
      t[tid].map[30].rob = robid;
    }
    return (0);
  }
  else if (i.cbz.op30_25==0x1a) { /* C6.6.30 CBNZ, C6.6.31 CBZ */
    rob->target = pc + ((Sll)((Ull)i.cbz.imm19<<45)>>43);
    if (i.cbz.imm19&0x40000) /* conditional backward */
      rob->bpred = 1;
    rob->type  = 5; /* BRANCH */
    rob->opcd  = i.cbz.op==0?2:3; /* CBZ/CBNZ */
    rob->sop   = 0; /* not used */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.cbz.sf; /* 0:cbz32, 1:cbz64 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    rob->sr[0].t = i.cbz.rt==31 ? 1 : /* ZERO */
                   !t[tid].map[i.cbz.rt].x ? 2 :
                   (c[cid].rob[t[tid].map[i.cbz.rt].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = i.cbz.rt==31 ? 0 : /* ZERO */
                   t[tid].map[i.cbz.rt].x; /* reg */
    rob->sr[0].n = i.cbz.rt==31 ? 0 : /* ZERO */
                   !t[tid].map[i.cbz.rt].x ? i.cbz.rt : t[tid].map[i.cbz.rt].rob;
    return (0);
  }
  else if (i.tbz.op30_25==0x1b) { /* C6.6.206 TBNZ, C6.6.207 TBZ */
    rob->target = pc + ((Sll)((Ull)i.tbz.imm14<<50)>>48);
    if (i.tbz.imm14&0x2000) /* conditional backward */
      rob->bpred = 1;
    rob->type  = 5; /* BRANCH */
    rob->opcd  = i.tbz.op==0?4:5; /* TBZ/TBNZ */
    rob->sop   = 0; /* not used */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.tbz.b5; /* 0:tbz32, 1:tbz64 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    rob->sr[0].t = i.tbz.rt==31 ? 1 : /* ZERO */
                   !t[tid].map[i.tbz.rt].x ? 2 :
                   (c[cid].rob[t[tid].map[i.tbz.rt].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = i.tbz.rt==31 ? 0 : /* ZERO */
                   t[tid].map[i.tbz.rt].x; /* reg */
    rob->sr[0].n = i.tbz.rt==31 ? 0 : /* ZERO */
                   !t[tid].map[i.tbz.rt].x ? i.tbz.rt : t[tid].map[i.tbz.rt].rob;
    rob->sr[1].t = 1; /* immediate */
    rob->sr[1].x = 0; /* renamig N.A. */
    rob->sr[1].n = (i.tbz.b5<<5)|i.tbz.b40;
    return (0);
  }
  else if (i.blr.op31_23==0x1ac && i.blr.op20_10==0x7c0 && i.blr.op4_0==0) { /* C6.6.27 BLR, C6.6.28 BR, C6.6.148 RET */
    rob->type  = 5; /* BRANCH */
    rob->opcd  = i.blr.op==1?6:7; /* CALL/JMP/RET */
    rob->sop   = 0; /* not used */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = 0; /* not used */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    rob->sr[0].t = i.blr.rn==31 ? 1 : /* ZERO */
                   !t[tid].map[i.blr.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.blr.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = i.blr.rn==31 ? 0 : /* ZERO */
                   t[tid].map[i.blr.rn].x; /* reg */
    rob->sr[0].n = i.blr.rn==31 ? 0 : /* ZERO */
                   !t[tid].map[i.blr.rn].x ? i.blr.rn : t[tid].map[i.blr.rn].rob;
    if (i.blr.op==1) { /* CALL */
      rob->dr[1].t = 1;
      rob->dr[1].n = 30;
      t[tid].map[30].x = 1;
      t[tid].map[30].rob = robid;
    }
    return (0);
  }
  else if (i.svc.op31_21==0x6a0 && i.svc.op4_0==0x01) { /* C6.6.200 SVC */
    rob->target = pc + 4; /* for restart */
    rob->bpred = 0; /* not used */
    rob->type  = (i.svc.type==0)?6:7; /* SVC/PTHREAD(csim special) */
    rob->opcd  = 0; /* not used */
    rob->sop   = 0; /* not used */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = 0; /* not used */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    rob->sr[1].t = 1; /* immediate */
    rob->sr[1].x = 0; /* renamig N.A. */
    rob->sr[1].n = i.svc.imm12;
    return (0);
  }
  else if (i.ldaxr_stlxr.op29_24==0x08 && i.ldaxr_stlxr.o2==0 && i.ldaxr_stlxr.o1==0 && i.ldaxr_stlxr.o0==1 && i.ldaxr_stlxr.rt2==31) { /* C6.6.77 LDAXR, C6.6.173 STLXR */
    switch (i.ldaxr_stlxr.size) {
    case 0:
    case 1:
      i_xxx(rob);
      return (0);
    case 2:
    case 3:
      break;
    }
    /* LDREX */ /* i.base.r1 -> Rn(addr)       */
                /* i.base.r2 -> Rt(dest reg)   */
                /* Rt = load4(A=Rn) */
                /* storeに読み替えてL1(v=1,d=1,s=0)への変更を試みる */
    /* STREX */ /* i.base.r1 -> Rn(addr)       */
                /* i.base.r2 -> Rd(result)     */
                /* i.base.r4 -> Rt(store data) */
                /* L1(v=1,d=1,s=0)への占有storeを試みる.失敗なら */
                /* if (success) { store4(A=Rn) = Rt; Rd = 0; } */
                /* if (failed)  {                    Rd = 1; } */
    rob->type  = i.ldaxr_stlxr.L?3:4; /* LD/ST */
    rob->opcd  = 7; /* LDREX/STREX */
    rob->sop   = 0; /* not used */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = 0; /* not used */
    rob->plus  = 1; /* plus */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    rob->sr[0].t = !t[tid].map[i.ldaxr_stlxr.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.ldaxr_stlxr.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[i.ldaxr_stlxr.rn].x; /* reg */
    rob->sr[0].n = !t[tid].map[i.ldaxr_stlxr.rn].x ? i.ldaxr_stlxr.rn : t[tid].map[i.ldaxr_stlxr.rn].rob;
    rob->sr[1].t = 1; /* immediate */
    rob->sr[1].x = 0; /* renamig N.A. */
    rob->sr[1].n = 0; /* ZERO */
    if (i.ldaxr_stlxr.L==0) { /* ST */
      rob->sr[3].t = i.ldaxr_stlxr.rt==31 ? 1 : /* ZERO */
                     !t[tid].map[i.ldaxr_stlxr.rt].x ? 2 :
                     (c[cid].rob[t[tid].map[i.ldaxr_stlxr.rt].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[3].x = i.ldaxr_stlxr.rt==31 ? 0 : /* ZERO */
                     t[tid].map[i.ldaxr_stlxr.rt].x; /* reg */
      rob->sr[3].n = i.ldaxr_stlxr.rt==31 ? 0 : /* ZERO */
                     !t[tid].map[i.ldaxr_stlxr.rt].x ? i.ldaxr_stlxr.rt : t[tid].map[i.ldaxr_stlxr.rt].rob;
    }
    /* dest */
    if (i.ldaxr_stlxr.L && i.ldaxr_stlxr.rt!=31) { /* LD */
      rob->dr[1].t = 1;
      rob->dr[1].n = i.ldaxr_stlxr.rt;
      t[tid].map[i.ldaxr_stlxr.rt].x = 1;
      t[tid].map[i.ldaxr_stlxr.rt].rob = robid;
    }
    if (!i.ldaxr_stlxr.L && i.ldaxr_stlxr.rs!=31) { /* ST */
      rob->dr[2].t = 1;
      rob->dr[2].n = i.ldaxr_stlxr.rs;
      t[tid].map[i.ldaxr_stlxr.rs].x = 2;
      t[tid].map[i.ldaxr_stlxr.rs].rob = robid;
    }
    return (0);
  }
  else if (i.ldp_stp.op29_25==0x14) { /* C6.6.81 LDP, C6.6.82 LDPSW, C6.6.177 STP */
    int post_index = i.ldp_stp.op24_23==1;
    int pre_index  = i.ldp_stp.op24_23==3;
    int sig_offset = i.ldp_stp.op24_23==2;
    Ull offset     = (Sll)((Ull)i.ldp_stp.imm7<<57)>>(i.ldp_stp.opc==2?54:55);
    switch (mc) { /* multi-cycle */
    case 0: /* start */
      rob->type  = 0; /* ALU */
      rob->opcd  = 4; /* ADD */
      rob->sop   = 0; /* LSL 0 */
      rob->ptw   = 0; /* not used */
      rob->size  = 0; /* not used */
      rob->idx   = 0; /* not used */
      rob->rep   = 0; /* not used */
      rob->dir   = 0; /* not used */
      rob->iinv  = 0; /* not used */
      rob->oinv  = 0; /* not used */
      rob->dbl   = 0; /* not used */
      rob->plus  = 0; /* not used */
      rob->pre   = 0; /* not used */
      rob->wb    = 0; /* not used */
      rob->updt  = 0; /* inhibit cc-update */
      rob->sr[0].t = !t[tid].map[i.ldp_stp.rn].x ? 2 :
                     (c[cid].rob[t[tid].map[i.ldp_stp.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[0].x = t[tid].map[i.ldp_stp.rn].x; /* reg */
      rob->sr[0].n = !t[tid].map[i.ldp_stp.rn].x ? i.ldp_stp.rn : t[tid].map[i.ldp_stp.rn].rob;
      rob->sr[1].t = 1; /* immediate */
      rob->sr[1].x = 0; /* renamig N.A. */
      rob->sr[1].n = i.ldp_stp.opc==2?8:4; /* 0:ldst32, 2:ldst64 */
      rob->sr[2].t = 1; /* immediate */
      rob->sr[2].x = 0; /* renamig N.A. */
      rob->sr[2].n = 0; /* LSL 0 */
      rob->dr[1].t = 1; /* drt */
      rob->dr[1].n = AUXREGTOP;
      t[tid].map[AUXREGTOP].x = 1;
      t[tid].map[AUXREGTOP].rob = robid;
      rob->term = 0;
      return (1);
    case 1:
      rob->type  = i.ldp_stp.L?3:4; /* LD/ST */
      rob->opcd  = i.ldp_stp.opc==2?3:i.ldp_stp.opc==0?2:6; /* LDR/LDRW/LDRSW/STR/STRW */
      rob->sop   = 0; /* not used */
      rob->ptw   = 0; /* not used */
      rob->size  = 0; /* not used */
      rob->idx   = 0; /* not used */
      rob->rep   = 0; /* not used */
      rob->dir   = 0; /* not used */
      rob->iinv  = 0; /* not used */
      rob->oinv  = 0; /* not used */
      rob->dbl   = i.ldp_stp.opc!=0; /* 0:ldst32, 1:ldst64 */
      rob->plus  = 1; /* plus */
      rob->pre   = pre_index || sig_offset;
      rob->wb    = post_index || pre_index;
      rob->updt  = 0; /* not used */
      rob->sr[0].t = !t[tid].map[i.ldp_stp.rn].x ? 2 :
                     (c[cid].rob[t[tid].map[i.ldp_stp.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[0].x = t[tid].map[i.ldp_stp.rn].x; /* reg */
      rob->sr[0].n = !t[tid].map[i.ldp_stp.rn].x ? i.ldp_stp.rn : t[tid].map[i.ldp_stp.rn].rob;
      rob->sr[1].t = 1; /* immediate */
      rob->sr[1].x = 0; /* renamig N.A. */
      rob->sr[1].n = offset;
      if (i.ldp_stp.L==0) { /* ST */
        rob->sr[3].t = i.ldp_stp.rt==31 ? 1 : /* ZERO */
                       !t[tid].map[i.ldp_stp.rt].x ? 2 :
                       (c[cid].rob[t[tid].map[i.ldp_stp.rt].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
        rob->sr[3].x = i.ldp_stp.rt==31 ? 0 : /* ZERO */
                       t[tid].map[i.ldp_stp.rt].x; /* reg */
        rob->sr[3].n = i.ldp_stp.rt==31 ? 0 : /* ZERO */
                       !t[tid].map[i.ldp_stp.rt].x ? i.ldp_stp.rt : t[tid].map[i.ldp_stp.rt].rob;
      }
      /* dest */
      if (i.ldp_stp.L && i.ldp_stp.rt!=31) { /* LD */
        rob->dr[1].t = 1;
        rob->dr[1].n = i.ldp_stp.rt;
        t[tid].map[i.ldp_stp.rt].x = 1;
        t[tid].map[i.ldp_stp.rt].rob = robid;
      }
      if (rob->wb) {
        rob->dr[2].t = 1; /* writeback-basereg */
        rob->dr[2].n = i.ldp_stp.rn;
        t[tid].map[i.ldp_stp.rn].x = 2;
        t[tid].map[i.ldp_stp.rn].rob = robid;
      }
      rob->term = 0;
      return (1);
    case 2:
      rob->type  = i.ldp_stp.L?3:4; /* LD/ST */
      rob->opcd  = i.ldp_stp.opc==2?3:i.ldp_stp.opc==0?2:6; /* LDR/LDRW/LDRSW/STR/STRW */
      rob->sop   = 0; /* not used */
      rob->ptw   = 0; /* not used */
      rob->size  = 0; /* not used */
      rob->idx   = 0; /* not used */
      rob->rep   = 0; /* not used */
      rob->dir   = 0; /* not used */
      rob->iinv  = 0; /* not used */
      rob->oinv  = 0; /* not used */
      rob->dbl   = i.ldp_stp.opc!=0; /* 0:ldst32, 1:ldst64 */
      rob->plus  = 1; /* plus */
      rob->pre   = pre_index || sig_offset;
      rob->wb    = 0; /* not used */
      rob->updt  = 0; /* not used */
      rob->sr[0].t = !t[tid].map[AUXREGTOP].x ? 2 :
                     (c[cid].rob[t[tid].map[AUXREGTOP].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[0].x =  t[tid].map[AUXREGTOP].x; /* reg */
      rob->sr[0].n = !t[tid].map[AUXREGTOP].x ? AUXREGTOP : t[tid].map[AUXREGTOP].rob;
      rob->sr[1].t = 1; /* immediate */
      rob->sr[1].x = 0; /* renamig N.A. */
      rob->sr[1].n = offset;
      if (i.ldp_stp.L==0) { /* ST */
        rob->sr[3].t = i.ldp_stp.rt2==31 ? 1 : /* ZERO */
                       !t[tid].map[i.ldp_stp.rt2].x ? 2 :
                       (c[cid].rob[t[tid].map[i.ldp_stp.rt2].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
        rob->sr[3].x = i.ldp_stp.rt2==31 ? 0 : /* ZERO */
                       t[tid].map[i.ldp_stp.rt2].x; /* reg */
        rob->sr[3].n = i.ldp_stp.rt2==31 ? 0 : /* ZERO */
                       !t[tid].map[i.ldp_stp.rt2].x ? i.ldp_stp.rt2 : t[tid].map[i.ldp_stp.rt2].rob;
      }
      /* dest */
      if (i.ldp_stp.L && i.ldp_stp.rt2!=31) { /* LD */
        rob->dr[1].t = 1;
        rob->dr[1].n = i.ldp_stp.rt2;
        t[tid].map[i.ldp_stp.rt2].x = 1;
        t[tid].map[i.ldp_stp.rt2].rob = robid;
      }
      return (0);
    default:
      i_xxx(rob);
      return (0);
    }
  }
  else if (i.ldp_stp.op29_25==0x16) { /* C7.3.165 LDP(SIMD&FP), C7.3.284 STP(SIMD&FP) */
    int post_index = i.ldp_stp.op24_23==1;
    int pre_index  = i.ldp_stp.op24_23==3;
    int sig_offset = i.ldp_stp.op24_23==2;
    Ull offset     = (Sll)((Ull)i.ldp_stp.imm7<<57)>>(i.ldp_stp.opc==0?55:i.ldp_stp.opc==1?54:53);
    switch (mc) { /* multi-cycle */
    case 0: /* start */
      rob->type  = 0; /* ALU */
      rob->opcd  = 4; /* ADD */
      rob->sop   = 0; /* LSL 0 */
      rob->ptw   = 0; /* not used */
      rob->size  = 0; /* not used */
      rob->idx   = 0; /* not used */
      rob->rep   = 0; /* not used */
      rob->dir   = 0; /* not used */
      rob->iinv  = 0; /* not used */
      rob->oinv  = 0; /* not used */
      rob->dbl   = 0; /* not used */
      rob->plus  = 0; /* not used */
      rob->pre   = 0; /* not used */
      rob->wb    = 0; /* not used */
      rob->updt  = 0; /* inhibit cc-update */
      rob->sr[0].t = !t[tid].map[i.ldp_stp.rn].x ? 2 :
                     (c[cid].rob[t[tid].map[i.ldp_stp.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[0].x = t[tid].map[i.ldp_stp.rn].x; /* reg */
      rob->sr[0].n = !t[tid].map[i.ldp_stp.rn].x ? i.ldp_stp.rn : t[tid].map[i.ldp_stp.rn].rob;
      rob->sr[1].t = 1; /* immediate */
      rob->sr[1].x = 0; /* renamig N.A. */
      rob->sr[1].n = i.ldp_stp.opc==0?4:i.ldp_stp.opc==1?8:16; /* 0:ldst32, 1:ldst64, 2:ldst128 */
      rob->sr[2].t = 1; /* immediate */
      rob->sr[2].x = 0; /* renamig N.A. */
      rob->sr[2].n = 0; /* LSL 0 */
      rob->dr[1].t = 1; /* drt */
      rob->dr[1].n = AUXREGTOP;
      t[tid].map[AUXREGTOP].x = 1;
      t[tid].map[AUXREGTOP].rob = robid;
      rob->term = 0;
      return (1);
    case 1:
      rob->type  = i.ldp_stp.L?3:4; /* LD/ST */
      rob->opcd  = i.ldp_stp.opc==0?10:i.ldp_stp.opc==1?11:12; /* LDRS/LDRD/LDRQ */
      rob->sop   = 0; /* not used */
      rob->ptw   = 0; /* not used */
      rob->size  = 0; /* not used */
      rob->idx   = 0; /* not used */
      rob->rep   = 0; /* not used */
      rob->dir   = 0; /* not used */
      rob->iinv  = 0; /* not used */
      rob->oinv  = 0; /* not used */
      rob->dbl   = i.ldp_stp.opc==2; /* 0:ldst32/64, 1:ldst128 */
      rob->plus  = 1; /* plus */
      rob->pre   = pre_index || sig_offset;
      rob->wb    = post_index || pre_index;
      rob->updt  = 0; /* not used */
      v0 = VECREGTOP+i.ldp_stp.rt;
      rob->sr[0].t = !t[tid].map[i.ldp_stp.rn].x ? 2 :
                     (c[cid].rob[t[tid].map[i.ldp_stp.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[0].x = t[tid].map[i.ldp_stp.rn].x; /* reg */
      rob->sr[0].n = !t[tid].map[i.ldp_stp.rn].x ? i.ldp_stp.rn : t[tid].map[i.ldp_stp.rn].rob;
      rob->sr[1].t = 1; /* immediate */
      rob->sr[1].x = 0; /* renamig N.A. */
      rob->sr[1].n = offset;
      if (i.ldp_stp.L==0) { /* ST */
        rob->sr[3].t = !t[tid].map[v0].x ? 2 :
                       (c[cid].rob[t[tid].map[v0].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
        rob->sr[3].x = t[tid].map[v0].x; /* reg */
        rob->sr[3].n = !t[tid].map[v0].x ? v0 : t[tid].map[v0].rob;
      }
      /* dest */
      if (i.ldp_stp.L) { /* LD */
        rob->dr[1].t = 1;
        rob->dr[1].n = v0;
        t[tid].map[v0].x = 1;
        t[tid].map[v0].rob = robid;
      }
      if (rob->wb) {
        rob->dr[2].t = 1; /* writeback-basereg */
        rob->dr[2].n = i.ldp_stp.rn;
        t[tid].map[i.ldp_stp.rn].x = 2;
        t[tid].map[i.ldp_stp.rn].rob = robid;
      }
      rob->term = 0;
      return (1);
    case 2:
      rob->type  = i.ldp_stp.L?3:4; /* LD/ST */
      rob->opcd  = i.ldp_stp.opc==0?10:i.ldp_stp.opc==1?11:12; /* LDRS/LDRD/LDRQ */
      rob->sop   = 0; /* not used */
      rob->ptw   = 0; /* not used */
      rob->size  = 0; /* not used */
      rob->idx   = 0; /* not used */
      rob->rep   = 0; /* not used */
      rob->dir   = 0; /* not used */
      rob->iinv  = 0; /* not used */
      rob->oinv  = 0; /* not used */
      rob->dbl   = i.ldp_stp.opc==2; /* 0:ldst32/64, 1:ldst128 */
      rob->plus  = 1; /* plus */
      rob->pre   = pre_index || sig_offset;
      rob->wb    = 0; /* not used */
      rob->updt  = 0; /* not used */
      v0 = VECREGTOP+i.ldp_stp.rt2;
      rob->sr[0].t = !t[tid].map[AUXREGTOP].x ? 2 :
                     (c[cid].rob[t[tid].map[AUXREGTOP].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[0].x =  t[tid].map[AUXREGTOP].x; /* reg */
      rob->sr[0].n = !t[tid].map[AUXREGTOP].x ? AUXREGTOP : t[tid].map[AUXREGTOP].rob;
      rob->sr[1].t = 1; /* immediate */
      rob->sr[1].x = 0; /* renamig N.A. */
      rob->sr[1].n = offset;
      if (i.ldp_stp.L==0) { /* ST */
        rob->sr[3].t = !t[tid].map[v0].x ? 2 :
                       (c[cid].rob[t[tid].map[v0].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
        rob->sr[3].x = t[tid].map[v0].x; /* reg */
        rob->sr[3].n = !t[tid].map[v0].x ? v0 : t[tid].map[v0].rob;
      }
      /* dest */
      if (i.ldp_stp.L) { /* LD */
        rob->dr[1].t = 1;
        rob->dr[1].n = v0;
        t[tid].map[v0].x = 1;
        t[tid].map[v0].rob = robid;
      }
      return (0);
    default:
      i_xxx(rob);
      return (0);
    }
  }
  else if (i.ldr_str_imm.op29_25==0x1c && ((i.ldr_str_imm.op24==0 && !(i.ldr_str_imm.imm12&0x800) && ((i.ldr_str_imm.imm12&3)==1 || (i.ldr_str_imm.imm12&3)==3))||(i.ldr_str_imm.op24==1))) { /* C6.6.83 LDR(immediate), C6.6.86 LDRB(immediate), C6.6.88 LDRH(immediate), C6.6.90 LDRSB(immediate), C6.6.92 LDRSH(immediate), C6.6.94 LDRSW(immediate), C6.6.178 STR(immediate), C6.6.180 STRB(immediate), C6.6.182 STR(immediate) */
    /* C6.6.86 ldrb(32):  size=00, opc=01 | C6.6.180 strb(32): size=00, opc=00 */
    /* C6.6.88 ldrh(32):  size=01, opc=01 | C6.6.182 strh(32): size=01, opc=00 */
    /* C6.6.83 ldr(32):   size=10, opc=01 | C6.6.178 str(32):  size=10, opc=00 */
    /* C6.6.83 ldr(64):   size=11, opc=01 | C6.6.178 str(64):  size=11, opc=00 */
    /* C6.6.90 ldrsb(64): size=00, opc=10 */
    /* C6.6.92 ldrsh(64): size=01, opc=10 */
    /* C6.6.94 ldrsw(64): size=10, opc=10 */
    /* C6.6.90 ldrsb(32): size=00, opc=11 */
    /* C6.6.92 ldrsh(32): size=01, opc=11 */
    int post_index = i.ldr_str_imm.op24==0 && (i.ldr_str_imm.imm12&3)==1;
    int pre_index  = i.ldr_str_imm.op24==0 && (i.ldr_str_imm.imm12&3)==3;
    int uns_offset = i.ldr_str_imm.op24==1;
    Ull offset     = (post_index || pre_index)?(Sll)((Ull)i.ldr_str_imm.imm12<<53)>>55:(Ull)i.ldr_str_imm.imm12<<i.ldr_str_imm.size;
    rob->type  = i.ldr_str_imm.opc?3:4; /* LD/ST */
    switch (i.ldr_str_imm.size) {
    case 0:
      switch (i.ldr_str_imm.opc) {
      case 0:
      case 1:
        rob->opcd = 0; /* LDRB/STRB(32) */
        rob->dbl  = 0; /* ldst32 */
        break;
      case 2:
        rob->opcd = 4; /* LDRSB(64) */
        rob->dbl  = 1; /* ldst64 */
        break;
      case 3:
        rob->opcd = 4; /* LDRSB(32) */
        rob->dbl  = 0; /* ldst32 */
        break;
      }
      break;
    case 1:
      switch (i.ldr_str_imm.opc) {
      case 0:
      case 1:
        rob->opcd = 1; /* LDRH/STRH(32) */
        rob->dbl  = 0; /* ldst32 */
        break;
      case 2:
        rob->opcd = 5; /* LDRSH(64) */
        rob->dbl  = 1; /* ldst64 */
        break;
      case 3:
        rob->opcd = 5; /* LDRSH(32) */
        rob->dbl  = 0; /* ldst32 */
        break;
      }
      break;
    case 2:
      switch (i.ldr_str_imm.opc) {
      case 0:
      case 1:
        rob->opcd = 2; /* LDR(32)/STR(32) */
        rob->dbl  = 0; /* ldst32 */
        break;
      case 2:
        rob->opcd = 6; /* LDRSW(64) */
        rob->dbl  = 1; /* ldst64 */
        break;
      case 3:
        i_xxx(rob);
        return (0);
      }
      break;
    case 3:
      switch (i.ldr_str_imm.opc) {
      case 0:
      case 1:
        rob->opcd = 3; /* LDR(64)/STR(64) */
        rob->dbl  = 1; /* ldst64 */
        break;
      case 2:
      case 3:
        i_xxx(rob);
        return (0);
      }
    }
    rob->sop   = 0; /* not used */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->plus  = 1; /* plus */
    rob->pre   = pre_index || uns_offset;
    rob->wb    = post_index || pre_index;
    rob->updt  = 0; /* not used */
    rob->sr[0].t = !t[tid].map[i.ldr_str_imm.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.ldr_str_imm.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[i.ldr_str_imm.rn].x; /* reg */
    rob->sr[0].n = !t[tid].map[i.ldr_str_imm.rn].x ? i.ldr_str_imm.rn : t[tid].map[i.ldr_str_imm.rn].rob;
    rob->sr[1].t = 1; /* immediate */
    rob->sr[1].x = 0; /* renamig N.A. */
    rob->sr[1].n = offset;
    if (i.ldr_str_imm.opc==0) { /* ST */
      rob->sr[3].t = i.ldr_str_imm.rt==31 ? 1 : /* ZERO */
                     !t[tid].map[i.ldr_str_imm.rt].x ? 2 :
                     (c[cid].rob[t[tid].map[i.ldr_str_imm.rt].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[3].x = i.ldr_str_imm.rt==31 ? 0 : /* ZERO */
                     t[tid].map[i.ldr_str_imm.rt].x; /* reg */
      rob->sr[3].n = i.ldr_str_imm.rt==31 ? 0 : /* ZERO */
                     !t[tid].map[i.ldr_str_imm.rt].x ? i.ldr_str_imm.rt : t[tid].map[i.ldr_str_imm.rt].rob;
    }
    /* dest */
    if (i.ldr_str_imm.opc && i.ldr_str_imm.rt!=31) { /* LD */
      rob->dr[1].t = 1;
      rob->dr[1].n = i.ldr_str_imm.rt;
      t[tid].map[i.ldr_str_imm.rt].x = 1;
      t[tid].map[i.ldr_str_imm.rt].rob = robid;
    }
    if (rob->wb) {
      rob->dr[2].t = 1; /* writeback-basereg */
      rob->dr[2].n = i.ldr_str_imm.rn;
      t[tid].map[i.ldr_str_imm.rn].x = 2;
      t[tid].map[i.ldr_str_imm.rn].rob = robid;
    }
    return (0);
  }
  else if (i.ldr_str_imm.op29_25==0x1e && ((i.ldr_str_imm.op24==0 && !(i.ldr_str_imm.imm12&0x800) && ((i.ldr_str_imm.imm12&3)==1 || (i.ldr_str_imm.imm12&3)==3))||(i.ldr_str_imm.op24==1))) { /* C7.3.166 LDR(immediate,SIMD&FP), C7.3.285 STR(immediate,SIMD&FP) */
    /* C7.3.166 ldrb(8):   size=00, opc=01 | C7.3.285 strb(8):   size=00, opc=00 */
    /* C7.3.166 ldrh(16):  size=01, opc=01 | C7.3.285 strh(16):  size=01, opc=00 */
    /* C7.3.166 ldrs(32):  size=10, opc=01 | C7.3.285 strs(32):  size=10, opc=00 */
    /* C7.3.166 ldrd(64):  size=11, opc=01 | C7.3.285 strd(64):  size=11, opc=00 */
    /* C7.3.166 ldrq(128): size=00, opc=11 | C7.3.285 strq(128): size=00, opc=10 */
    int post_index = i.ldr_str_imm.op24==0 && (i.ldr_str_imm.imm12&3)==1;
    int pre_index  = i.ldr_str_imm.op24==0 && (i.ldr_str_imm.imm12&3)==3;
    int uns_offset = i.ldr_str_imm.op24==1;
    Ull offset     = (post_index || pre_index)?(Sll)((Ull)i.ldr_str_imm.imm12<<53)>>55:(Ull)i.ldr_str_imm.imm12<<((i.ldr_str_imm.opc<<1&4)|i.ldr_str_imm.size);
    rob->type  = (i.ldr_str_imm.opc&1)?3:4; /* LD/ST */
    switch (i.ldr_str_imm.size) {
    case 0:
      switch (i.ldr_str_imm.opc) {
      case 0:
      case 1:
        rob->opcd = 8; /* LDRB/STRB(8) */
        rob->dbl  = 0; /* ldst8 */
        break;
      case 2:
      case 3:
        rob->opcd =12; /* LDRQ/STRQ(128) */
        rob->dbl  = 1; /* ldst128 */
        break;
      }
      break;
    case 1:
      switch (i.ldr_str_imm.opc) {
      case 0:
      case 1:
        rob->opcd = 9; /* LDRH/STRH(16) */
        rob->dbl  = 0; /* ldst16 */
        break;
      case 2:
      case 3:
        i_xxx(rob);
        return (0);
      }
      break;
    case 2:
      switch (i.ldr_str_imm.opc) {
      case 0:
      case 1:
        rob->opcd =10; /* LDRS(32)/STRS(32) */
        rob->dbl  = 0; /* ldst32 */
        break;
      case 2:
      case 3:
        i_xxx(rob);
        return (0);
      }
      break;
    case 3:
      switch (i.ldr_str_imm.opc) {
      case 0:
      case 1:
        rob->opcd =11; /* LDRD(64)/STRD(64) */
        rob->dbl  = 0; /* ldst64 */
        break;
      case 2:
      case 3:
        i_xxx(rob);
        return (0);
      }
      break;
    }
    rob->sop   = 0; /* not used */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->plus  = 1; /* plus */
    rob->pre   = pre_index || uns_offset;
    rob->wb    = post_index || pre_index;
    rob->updt  = 0; /* not used */
    v0 = VECREGTOP+i.ldr_str_imm.rt;
    rob->sr[0].t = !t[tid].map[i.ldr_str_imm.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.ldr_str_imm.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[i.ldr_str_imm.rn].x; /* reg */
    rob->sr[0].n = !t[tid].map[i.ldr_str_imm.rn].x ? i.ldr_str_imm.rn : t[tid].map[i.ldr_str_imm.rn].rob;
    rob->sr[1].t = 1; /* immediate */
    rob->sr[1].x = 0; /* renamig N.A. */
    rob->sr[1].n = offset;
    if ((i.ldr_str_imm.opc&1)==0) { /* ST */
      rob->sr[3].t = !t[tid].map[v0].x ? 2 :
                     (c[cid].rob[t[tid].map[v0].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[3].x = t[tid].map[v0].x; /* reg */
      rob->sr[3].n = !t[tid].map[v0].x ? v0 : t[tid].map[v0].rob;
    }
    /* dest */
    if (i.ldr_str_imm.opc&1) { /* LD */
      rob->dr[1].t = 1;
      rob->dr[1].n = v0;
      t[tid].map[v0].x = 1;
      t[tid].map[v0].rob = robid;
    }
    if (rob->wb) {
      rob->dr[2].t = 1; /* writeback-basereg */
      rob->dr[2].n = i.ldr_str_imm.rn;
      t[tid].map[i.ldr_str_imm.rn].x = 2;
      t[tid].map[i.ldr_str_imm.rn].rob = robid;
    }
    return (0);
  }
  else if (i.ldr_str_unsc.op29_25==0x1c && i.ldr_str_unsc.op24==0 && i.ldr_str_unsc.op21==0 && i.ldr_str_unsc.op11_10==0) { /* C6.6.103 LDUR, C6.6.104 LDURB, C6.6.105 LDURH, C6.6.106 LDURSB, C6.6.107 LDURSH, C6.6.108 LDURSW, C6.6.187 STUR, C6.6.188 STURB, C6.6.189 STURH */
    /* C6.6.104 ldurb(32):  size=00, opc=01 | C6.6.188 sturb(32): size=00, opc=00 */
    /* C6.6.105 ldurh(32):  size=01, opc=01 | C6.6.189 sturh(32): size=01, opc=00 */
    /* C6.6.103 ldur(32):   size=10, opc=01 | C6.6.187 stur(32):  size=10, opc=00 */
    /* C6.6.103 ldur(64):   size=11, opc=01 | C6.6.187 stur(64):  size=11, opc=00 */
    /* C6.6.106 ldursb(64): size=00, opc=10 */
    /* C6.6.107 ldursh(64): size=01, opc=10 */
    /* C6.6.108 ldursw(64): size=10, opc=10 */
    /* C6.6.106 ldursb(32): size=00, opc=11 */
    /* C6.6.107 ldursh(32): size=01, opc=11 */
    Ull offset = (Sll)((Ull)i.ldr_str_unsc.imm9<<55)>>55;
    rob->type  = i.ldr_str_unsc.opc?3:4; /* LD/ST */
    switch (i.ldr_str_unsc.size) {
    case 0:
      switch (i.ldr_str_unsc.opc) {
      case 0:
      case 1:
        rob->opcd = 0; /* LDURB/STURB(32) */
        rob->dbl  = 0; /* ldst32 */
        break;
      case 2:
        rob->opcd = 4; /* LDURSB(64) */
        rob->dbl  = 1; /* ldst64 */
        break;
      case 3:
        rob->opcd = 4; /* LDURSB(32) */
        rob->dbl  = 0; /* ldst32 */
        break;
      }
      break;
    case 1:
      switch (i.ldr_str_unsc.opc) {
      case 0:
      case 1:
        rob->opcd = 1; /* LDURH/STURH(32) */
        rob->dbl  = 0; /* ldst32 */
        break;
      case 2:
        rob->opcd = 5; /* LDURSH(64) */
        rob->dbl  = 1; /* ldst64 */
        break;
      case 3:
        rob->opcd = 5; /* LDURSH(32) */
        rob->dbl  = 0; /* ldst32 */
        break;
      }
      break;
    case 2:
      switch (i.ldr_str_unsc.opc) {
      case 0:
      case 1:
        rob->opcd = 2; /* LDUR(32)/STUR(32) */
        rob->dbl  = 0; /* ldst32 */
        break;
      case 2:
        rob->opcd = 6; /* LDURSW(64) */
        rob->dbl  = 1; /* ldst64 */
        break;
      case 3:
        i_xxx(rob);
        return (0);
      }
      break;
    case 3:
      switch (i.ldr_str_unsc.opc) {
      case 0:
      case 1:
        rob->opcd = 3; /* LDUR(64)/STUR(64) */
        rob->dbl  = 1; /* ldst64 */
        break;
      case 2:
      case 3:
        i_xxx(rob);
        return (0);
      }
    }
    rob->sop   = 0; /* not used */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->plus  = 1; /* plus */
    rob->pre   = 1; /* addr+offset */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    rob->sr[0].t = !t[tid].map[i.ldr_str_unsc.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.ldr_str_unsc.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[i.ldr_str_unsc.rn].x; /* reg */
    rob->sr[0].n = !t[tid].map[i.ldr_str_unsc.rn].x ? i.ldr_str_unsc.rn : t[tid].map[i.ldr_str_unsc.rn].rob;
    rob->sr[1].t = 1; /* immediate */
    rob->sr[1].x = 0; /* renamig N.A. */
    rob->sr[1].n = offset;
    if (i.ldr_str_unsc.opc==0) { /* ST */
      rob->sr[3].t = i.ldr_str_unsc.rt==31 ? 1 : /* ZERO */
                     !t[tid].map[i.ldr_str_unsc.rt].x ? 2 :
                     (c[cid].rob[t[tid].map[i.ldr_str_unsc.rt].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[3].x = i.ldr_str_unsc.rt==31 ? 0 : /* ZERO */
                     t[tid].map[i.ldr_str_unsc.rt].x; /* reg */
      rob->sr[3].n = i.ldr_str_unsc.rt==31 ? 0 : /* ZERO */
                     !t[tid].map[i.ldr_str_unsc.rt].x ? i.ldr_str_unsc.rt : t[tid].map[i.ldr_str_unsc.rt].rob;
    }
    /* dest */
    if (i.ldr_str_unsc.opc && i.ldr_str_unsc.rt!=31) { /* LD */
      rob->dr[1].t = 1;
      rob->dr[1].n = i.ldr_str_unsc.rt;
      t[tid].map[i.ldr_str_unsc.rt].x = 1;
      t[tid].map[i.ldr_str_unsc.rt].rob = robid;
    }
    return (0);
  }
  else if (i.ldr_str_unsc.op29_25==0x1e && i.ldr_str_unsc.op24==0 && i.ldr_str_unsc.op21==0 && i.ldr_str_unsc.op11_10==0) { /* C7.3.169 LDUR(SIMD&FP), C7.3.287 STUR(SIMD&FP) */
    /* C7.3.169 ldur(8):  size=00, opc=01 | C7.3.287 stur(8):  size=00, opc=00 */
    /* C7.3.169 ldur(16): size=01, opc=01 | C7.3.287 stur(16): size=01, opc=00 */
    /* C7.3.169 ldur(32): size=10, opc=01 | C7.3.287 stur(32): size=10, opc=00 */
    /* C7.3.169 ldur(64): size=11, opc=01 | C7.3.287 stur(64): size=11, opc=00 */
    /* C7.3.169 ldur(128):size=00, opc=11 | C7.3.287 stur(128):size=00, opc=10 */
    Ull offset = (Sll)((Ull)i.ldr_str_unsc.imm9<<55)>>55;
    rob->type  = (i.ldr_str_unsc.opc&1)?3:4; /* LD/ST */
    switch (i.ldr_str_unsc.size) {
    case 0:
      switch (i.ldr_str_unsc.opc) {
      case 0:
      case 1:
        rob->opcd = 8; /* LDRB/STRB(8) */
        rob->dbl  = 0; /* ldst8 */
        break;
      case 2:
      case 3:
        rob->opcd =12; /* LDRQ/STRQ(128) */
        rob->dbl  = 1; /* ldst128 */
        break;
      }
      break;
    case 1:
      switch (i.ldr_str_unsc.opc) {
      case 0:
      case 1:
        rob->opcd = 9; /* LDRH/STRH(16) */
        rob->dbl  = 0; /* ldst16 */
        break;
      case 2:
      case 3:
        i_xxx(rob);
        return (0);
      }
      break;
    case 2:
      switch (i.ldr_str_unsc.opc) {
      case 0:
      case 1:
        rob->opcd =10; /* LDRS(32)/STRS(32) */
        rob->dbl  = 0; /* ldst32 */
        break;
      case 2:
      case 3:
        i_xxx(rob);
        return (0);
      }
      break;
    case 3:
      switch (i.ldr_str_unsc.opc) {
      case 0:
      case 1:
        rob->opcd =11; /* LDRD(64)/STRD(64) */
        rob->dbl  = 0; /* ldst64 */
        break;
      case 2:
      case 3:
        i_xxx(rob);
        return (0);
      }
    }
    rob->sop   = 0; /* not used */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->plus  = 1; /* plus */
    rob->pre   = 1; /* addr+offset */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    v0 = VECREGTOP+i.ldr_str_unsc.rt;
    rob->sr[0].t = !t[tid].map[i.ldr_str_unsc.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.ldr_str_unsc.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[i.ldr_str_unsc.rn].x; /* reg */
    rob->sr[0].n = !t[tid].map[i.ldr_str_unsc.rn].x ? i.ldr_str_unsc.rn : t[tid].map[i.ldr_str_unsc.rn].rob;
    rob->sr[1].t = 1; /* immediate */
    rob->sr[1].x = 0; /* renamig N.A. */
    rob->sr[1].n = offset;
    if ((i.ldr_str_unsc.opc&1)==0) { /* ST */
      rob->sr[3].t = !t[tid].map[v0].x ? 2 :
                     (c[cid].rob[t[tid].map[v0].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[3].x = t[tid].map[v0].x; /* reg */
      rob->sr[3].n = !t[tid].map[v0].x ? v0 : t[tid].map[v0].rob;
    }
    /* dest */
    if (i.ldr_str_unsc.opc&1) { /* LD */
      rob->dr[1].t = 1;
      rob->dr[1].n = v0;
      t[tid].map[v0].x = 1;
      t[tid].map[v0].rob = robid;
    }
    return (0);
  }
  else if (i.ldr_str_reg.op29_25==0x1c && i.ldr_str_reg.op24==0 && i.ldr_str_reg.op21==1 && i.ldr_str_reg.op11_10==2) { /* C6.6.85 LDR(register), C6.6.87 LDRB(register), C6.6.89 LDRH(register), C6.6.91 LDRSB(register), C6.6.93 LDRSH(register), C6.6.96 LDRSW(register), C6.6.179 STR(register), C6.6.181 STRB(register), C6.6.183 STRH(register) */
    /* C6.6.87 ldrb(32):  size=00, opc=01 | C6.6.181 strb(32): size=00, opc=00 */
    /* C6.6.89 ldrh(32):  size=01, opc=01 | C6.6.183 strh(32): size=01, opc=00 */
    /* C6.6.85 ldr(32):   size=10, opc=01 | C6.6.179 str(32):  size=10, opc=00 */
    /* C6.6.85 ldr(64):   size=11, opc=01 | C6.6.179 str(64):  size=11, opc=00 */
    /* C6.6.91 ldrsb(64): size=00, opc=10 */
    /* C6.6.93 ldrsh(64): size=01, opc=10 */
    /* C6.6.96 ldrsw(64): size=10, opc=10 */
    /* C6.6.91 ldrsb(32): size=00, opc=11 */
    /* C6.6.93 ldrsh(32): size=01, opc=11 */
    /* option: 000:(uxtb), 001:(uxth), 010:uxtw, 011:uxtx, 100:(sxtb), 101:(sxth), 110:sxtw, 111:sxtx */
    /* shift:  S==1 ? size : 0 */
    rob->type  = i.ldr_str_reg.opc?3:4; /* LD/ST */
    switch (i.ldr_str_reg.size) {
    case 0:
      switch (i.ldr_str_reg.opc) {
      case 0:
      case 1:
        rob->opcd = 0; /* LDRB/STRB(32) */
        rob->dbl  = 0; /* ldst32 */
        break;
      case 2:
        rob->opcd = 4; /* LDRSB(64) */
        rob->dbl  = 1; /* ldst64 */
        break;
      case 3:
        rob->opcd = 4; /* LDRSB(32) */
        rob->dbl  = 0; /* ldst32 */
        break;
      }
      break;
    case 1:
      switch (i.ldr_str_reg.opc) {
      case 0:
      case 1:
        rob->opcd = 1; /* LDRH/STRH(32) */
        rob->dbl  = 0; /* ldst32 */
        break;
      case 2:
        rob->opcd = 5; /* LDRSH(64) */
        rob->dbl  = 1; /* ldst64 */
        break;
      case 3:
        rob->opcd = 5; /* LDRSH(32) */
        rob->dbl  = 0; /* ldst32 */
        break;
      }
      break;
    case 2:
      switch (i.ldr_str_reg.opc) {
      case 0:
      case 1:
        rob->opcd = 2; /* LDR(32)/STR(32) */
        rob->dbl  = 0; /* ldst32 */
        break;
      case 2:
        rob->opcd = 6; /* LDRSW(64) */
        rob->dbl  = 1; /* ldst64 */
        break;
      case 3:
        i_xxx(rob);
        return (0);
      }
      break;
    case 3:
      switch (i.ldr_str_reg.opc) {
      case 0:
      case 1:
        rob->opcd = 3; /* LDR(64)/STR(64) */
        rob->dbl  = 1; /* ldst64 */
        break;
      case 2:
      case 3:
        i_xxx(rob);
        return (0);
      }
    }
    rob->sop   = 8|i.ldr_str_reg.option; /* option: 000:(uxtb), 001:(uxth), 010:uxtw, 011:uxtx, 100:(sxtb), 101:(sxth), 110:sxtw, 111:sxtx */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->plus  = 1; /* plus */
    rob->pre   = 1; /* addr+offset */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    rob->sr[0].t = !t[tid].map[i.ldr_str_reg.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.ldr_str_reg.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[i.ldr_str_reg.rn].x; /* reg */
    rob->sr[0].n = !t[tid].map[i.ldr_str_reg.rn].x ? i.ldr_str_reg.rn : t[tid].map[i.ldr_str_reg.rn].rob;
    rob->sr[1].t = i.ldr_str_reg.rm==31 ? 1 : /* ZERO */
                   !t[tid].map[i.ldr_str_reg.rm].x ? 2 :
                   (c[cid].rob[t[tid].map[i.ldr_str_reg.rm].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = i.ldr_str_reg.rm==31 ? 0 : /* ZERO */
                   t[tid].map[i.ldr_str_reg.rm].x; /* reg */
    rob->sr[1].n = i.ldr_str_reg.rm==31 ? 0 : /* ZERO */
                   !t[tid].map[i.ldr_str_reg.rm].x ? i.ldr_str_reg.rm : t[tid].map[i.ldr_str_reg.rm].rob;
    rob->sr[2].t = 1; /* immediate */
    rob->sr[2].x = 0; /* renamig N.A. */
    rob->sr[2].n = i.ldr_str_reg.S?i.ldr_str_reg.size:0; /* shift(0,2,3) */
    if (i.ldr_str_reg.opc==0) { /* ST */
      rob->sr[3].t = i.ldr_str_reg.rt==31 ? 1 : /* ZERO */
                     !t[tid].map[i.ldr_str_reg.rt].x ? 2 :
                     (c[cid].rob[t[tid].map[i.ldr_str_reg.rt].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[3].x = i.ldr_str_reg.rt==31 ? 0 : /* ZERO */
                     t[tid].map[i.ldr_str_reg.rt].x; /* reg */
      rob->sr[3].n = i.ldr_str_reg.rt==31 ? 0 : /* ZERO */
                     !t[tid].map[i.ldr_str_reg.rt].x ? i.ldr_str_reg.rt : t[tid].map[i.ldr_str_reg.rt].rob;
    }
    /* dest */
    if (i.ldr_str_reg.opc && i.ldr_str_reg.rt!=31) { /* LD */
      rob->dr[1].t = 1;
      rob->dr[1].n = i.ldr_str_reg.rt;
      t[tid].map[i.ldr_str_reg.rt].x = 1;
      t[tid].map[i.ldr_str_reg.rt].rob = robid;
    }
    return (0);
  }
  else if (i.ldr_str_reg.op29_25==0x1e && i.ldr_str_reg.op24==0 && i.ldr_str_reg.op21==1 && i.ldr_str_reg.op11_10==2) { /* C7.3.168 LDR(immediate,SIMD&FP), C7.3.286 STR(immediate,SIMD&FP) */
    /* C7.3.168 ldrb(8):   size=00, opc=01 | C7.3.286 strb(8):   size=00, opc=00 */
    /* C7.3.168 ldrh(16):  size=01, opc=01 | C7.3.286 strh(16):  size=01, opc=00 */
    /* C7.3.168 ldrs(32):  size=10, opc=01 | C7.3.286 strs(32):  size=10, opc=00 */
    /* C7.3.168 ldrd(64):  size=11, opc=01 | C7.3.286 strd(64):  size=11, opc=00 */
    /* C7.3.168 ldrq(128): size=00, opc=11 | C7.3.286 strq(128): size=00, opc=10 */
    /* option: 000:(uxtb), 001:(uxth), 010:uxtw, 011:uxtx, 100:(sxtb), 101:(sxth), 110:sxtw, 111:sxtx */
    /* shift:  S==1 ? size : 0 */
    rob->type  = (i.ldr_str_reg.opc&1)?3:4; /* LD/ST */
    switch (((i.ldr_str_reg.opc&2)<<1)|i.ldr_str_reg.size) {
    case 0:
      rob->opcd = 8; /* VLDRB(8) */
      break;
    case 1:
      rob->opcd = 9; /* VLDRH(16) */
      break;
    case 2:
      rob->opcd = 10; /* VLDRS(32) */
      break;
    case 3:
      rob->opcd = 11; /* VLDRD(64) */
      break;
    case 4:
      rob->opcd = 12; /* VLDRQ(128) */
      break;
    default:
      i_xxx(rob);
      return (0);
    }
    rob->sop   = 8|i.ldr_str_reg.option; /* option: 000:(uxtb), 001:(uxth), 010:uxtw, 011:uxtx, 100:(sxtb), 101:(sxth), 110:sxtw, 111:sxtx */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = 1; /* ldst128 */
    rob->plus  = 1; /* plus */
    rob->pre   = 1; /* addr+offset */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    v0 = VECREGTOP+i.ldr_str_reg.rt;
    rob->sr[0].t = !t[tid].map[i.ldr_str_reg.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.ldr_str_reg.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[i.ldr_str_reg.rn].x; /* reg */
    rob->sr[0].n = !t[tid].map[i.ldr_str_reg.rn].x ? i.ldr_str_reg.rn : t[tid].map[i.ldr_str_reg.rn].rob;
    rob->sr[1].t = i.ldr_str_reg.rm==31 ? 1 : /* ZERO */
                   !t[tid].map[i.ldr_str_reg.rm].x ? 2 :
                   (c[cid].rob[t[tid].map[i.ldr_str_reg.rm].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = i.ldr_str_reg.rm==31 ? 0 : /* ZERO */
                   t[tid].map[i.ldr_str_reg.rm].x; /* reg */
    rob->sr[1].n = i.ldr_str_reg.rm==31 ? 0 : /* ZERO */
                   !t[tid].map[i.ldr_str_reg.rm].x ? i.ldr_str_reg.rm : t[tid].map[i.ldr_str_reg.rm].rob;
    rob->sr[2].t = 1; /* immediate */
    rob->sr[2].x = 0; /* renamig N.A. */
    rob->sr[2].n = i.ldr_str_reg.S?((i.ldr_str_reg.opc&2)<<1)|i.ldr_str_reg.size:0; /* shift(0,2,3) */
    if ((i.ldr_str_reg.opc&1)==0) { /* ST */
      rob->sr[3].t = !t[tid].map[v0].x ? 2 :
                     (c[cid].rob[t[tid].map[v0].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[3].x = t[tid].map[v0].x; /* reg */
      rob->sr[3].n = !t[tid].map[v0].x ? v0 : t[tid].map[v0].rob;
    }
    /* dest */
    if (i.ldr_str_reg.opc&1) { /* LD */
      rob->dr[1].t = 1;
      rob->dr[1].n = v0;
      t[tid].map[v0].x = 1;
      t[tid].map[v0].rob = robid;
    }
    return (0);
  }
  else if (i.ldr_str_literal.op29_24==0x18) { /* C6.6.84 LDR(literal), C6.6.95 LDRSW(literal) */
    rob->type  = 3; /* LD */
    rob->opcd  = i.ldr_str_literal.opc==1?3:i.ldr_str_literal.opc==0?2:6; /* LDR/LDRW/LDRSW */
    rob->sop   = 0; /* not used */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.ldr_str_literal.opc!=0; /* 0:ld32, 1:ld64 */
    rob->plus  = 1; /* plus */
    rob->pre   = 1; /* addr+offset */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    rob->sr[0].t = 1; /* PC */
    rob->sr[0].x = 0; /* renamig N.A. */
    rob->sr[0].n = pc; /* PC */
    rob->sr[1].t = 1; /* immediate */
    rob->sr[1].x = 0; /* renamig N.A. */
    rob->sr[1].n = (Sll)((Ull)i.ldr_str_literal.imm19<<45)>>43;
    /* dest */
    if (i.ldr_str_literal.rt!=31) { /* LD */
      rob->dr[1].t = 1;
      rob->dr[1].n = i.ldr_str_literal.rt;
      t[tid].map[i.ldr_str_literal.rt].x = 1;
      t[tid].map[i.ldr_str_literal.rt].rob = robid;
    }
    return (0);
  }
  else if (i.ldr_str_literal.op29_24==0x1c) { /* C7.3.167 LDR(literal,simd&fp) */
    rob->type  = 3; /* LD */
    rob->opcd  = i.ldr_str_literal.opc==0?10:i.ldr_str_literal.opc==1?11:12; /* VLDRS/VLDRD/VLDRQ */
    rob->sop   = 0; /* not used */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.ldr_str_literal.opc!=0; /* 0:ldst32/64, 1:ldst128 */
    rob->plus  = 1; /* plus */
    rob->pre   = 1; /* addr+offset */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    v0 = VECREGTOP+i.ldr_str_literal.rt;
    rob->sr[0].t = 1; /* PC */
    rob->sr[0].x = 0; /* renamig N.A. */
    rob->sr[0].n = pc; /* PC */
    rob->sr[1].t = 1; /* immediate */
    rob->sr[1].x = 0; /* renamig N.A. */
    rob->sr[1].n = (Sll)((Ull)i.ldr_str_literal.imm19<<45)>>43;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.ld1_st1_mult.op31==0 && i.ld1_st1_mult.op29_25==0x06 && i.ld1_st1_mult.op24==0 && i.ld1_st1_mult.R==0) { /* C7.3.152 LD1(multiple structures), C7.3.155 LD2(multiple structures), C7.3.158 LD3(multiple structures), C7.3.161 LD4(multiple structures), C7.3.275 ST1(multiple structures), C7.3.277 ST2(multiple structures), C7.3.279 ST3(multiple structures), C7.3.281 ST4(multiple structures) */
    /* C7.3.152 ld1_nofs(1r  ,rpt=1,sel=1):op23=0,op=0111      |C7.3.275 st1_nofs(1r  ) */
    /* C7.3.152 ld1_post(1r+r,rpt=1,sel=1):op23=1,op=0111,rm<31|C7.3.275 st1_post(1r+r) */
    /* C7.3.152 ld1_post(1r+i,rpt=1,sel=1):op23=1,op=0111,rm=31|C7.3.275 st1_post(1r+i) */

    /* C7.3.152 ld1_nofs(2r  ,rpt=2,sel=1):op23=0,op=1010      |C7.3.275 st1_nofs(2r  ) */
    /* C7.3.152 ld1_post(2r+r,rpt=2,sel=1):op23=1,op=1010,rm<31|C7.3.275 st1_post(2r+r) */
    /* C7.3.152 ld1_post(2r+i,rpt=2,sel=1):op23=1,op=1010,rm=31|C7.3.275 st1_post(2r+i) */

    /* C7.3.152 ld1_nofs(3r  ,rpt=3,sel=1):op23=0,op=0110      |C7.3.275 st1_nofs(3r  ) */
    /* C7.3.152 ld1_post(3r+r,rpt=3,sel=1):op23=1,op=0110,rm<31|C7.3.275 st1_post(3r+r) */
    /* C7.3.152 ld1_post(3r+i,rpt=3,sel=1):op23=1,op=0110,rm=31|C7.3.275 st1_post(3r+i) */

    /* C7.3.152 ld1_nofs(4r  ,rpt=4,sel=1):op23=0,op=0010      |C7.3.275 st1_nofs(4r  ) */
    /* C7.3.152 ld1_post(4r+r,rpt=4,sel=1):op23=1,op=0010,rm<31|C7.3.275 st1_post(4r+r) */
    /* C7.3.152 ld1_post(4r+i,rpt=4,sel=1):op23=1,op=0010,rm=31|C7.3.275 st1_post(4r+i) */

    /* C7.3.155 ld2_nofs(2r  ,rpt=1,sel=2):op23=0,op=1000      |C7.3.277 st2_nofs(2r  ) *//* 当面未実装 */
    /* C7.3.155 ld2_post(2r+r,rpt=1,sel=2):op23=1,op=1000,rm<31|C7.3.277 st2_post(2r+r) *//* 当面未実装 */
    /* C7.3.155 ld2_post(2r+i,rpt=1,sel=2):op23=1,op=1000,rm=31|C7.3.277 st2_post(2r+i) *//* 当面未実装 */

    /* C7.3.158 ld3_nofs(3r  ,rpt=1,sel=3):op23=0,op=0100      |C7.3.279 st3_nofs(3r  ) *//* 当面未実装 */
    /* C7.3.158 ld3_post(3r+r,rpt=1,sel=3):op23=1,op=0100,rm<31|C7.3.279 st3_post(3r+r) *//* 当面未実装 */
    /* C7.3.158 ld3_post(3r+i,rpt=1,sel=3):op23=1,op=0100,rm=31|C7.3.279 st3_post(3r+i) *//* 当面未実装 */

    /* C7.3.161 ld4_nofs(4r  ,rpt=1,sel=4):op23=0,op=0000      |C7.3.281 st4_nofs(4r  ) *//* 当面未実装 */
    /* C7.3.161 ld4_post(4r+r,rpt=1,sel=4):op23=1,op=0000,rm<31|C7.3.281 st4_post(4r+r) *//* 当面未実装 */
    /* C7.3.161 ld4_post(4r+i,rpt=1,sel=4):op23=1,op=0000,rm=31|C7.3.281 st4_post(4r+i) *//* 当面未実装 */
    /*     T=8b : size=00,Q=0     */
    /*     T=16b: size=00,Q=1     */
    /*     T=4h : size=01,Q=0     */
    /*     T=8h : size=01,Q=1     */
    /*     T=2s : size=10,Q=0     */
    /*     T=4s : size=10,Q=1     */
    /*     T=1d : size=11,Q=0     */
    /*     T=2d : size=11,Q=1     */
    /*==============================================================================================*/
    /* integer datasize = if Q == '1' then 128 else 64;                                             */
    /* integer esize = 8 << UInt(size);                                    8,16,32,64               */
    /* integer elements = datasize DIV esize; 128b:16,8,4,2 64b:8,4,2,1   16, 8, 4, 2 / 8, 4, 2, 1  */
    /* offs = 0;                                                                                    */
    /*==============================================================================================*/
    /* レジスタ内で連続領域 o_ldst(128bit)を4命令分割,各回1レジスタに直接格納                       */
    /* for r = t,t+1,t+2,t+3 (11r:1 12r:2 13r:3 14r:4)                                              */
    /*  for e = 0 to elements-1 0-16,8,4,2,1                              16, 8, 4, 2 / 8, 4, 2, 1  */
    /*   LD: V[r].Elem[e, esize] = Mem[addr+offs, ebytes, AccT]; loc[e*esize] <- m[esize]           */
    /*   ST: Mem[addr+offs, ebytes, AccT] = V[r].Elem[e, esize]; m[esize] <- loc[e*esize]           */
    /*   offs = offs + ebytes;                                   m+=esize                           */
    /*==============================================================================================*/
    /* 複数レジスタをインタリーブ使用 o_ldst(128bit)を4命令分割,AUXに格納,AUX*4->UREGを4命令追加    *//* 当面未実装 */
    /* for e = 0 to elements-1 0-16,8,4,2,1                               16, 8, 4, 2 / 8, 4, 2, 1  */
    /*  for s = t,t+1,t+2,t+3 (22r:2 33r:3 44r:4)                                                   */
    /*   LD: V[s].Elem[e, esize] = Mem[addr+offs, ebytes, AccT]; loc[e*esize] <- m[esize]           */
    /*   ST: Mem[addr+offs, ebytes, AccT] = V[s].Elem[e, esize]; m[esize] <- loc[e*esize]           */
    /*   offs = offs + ebytes;                                   m+=esize                           */
    /*==============================================================================================*/
    /* if wback then                                                                                */
    /*  if m != 31 then    offs = X[m];                                                             */
    /*  if n == 31  SP[] = address + offs;                                                          */
    /*  else        X[n] = address + offs;                                                          */
    /*==============================================================================================*/
    int num_ldst;
    int no_post_inc = i.ld1_st1_mult.op23==0;
    int post_imm = i.ld1_st1_mult.op23 && i.ld1_st1_mult.rm==31; /* auto_immediate_offset */
    int post_reg = i.ld1_st1_mult.op23 && i.ld1_st1_mult.rm!=31; /* auto_immediate_offset */
    switch (i.ld1_st1_mult.opcode) {
    case  7: num_ldst = 1; break;
    case 10: num_ldst = 2; break;
    case  6: num_ldst = 3; break;
    case  2: num_ldst = 4; break;
    default:   i_xxx(rob); return (0);
    }
    if (!post_reg || mc<num_ldst) {
      rob->type  = i.ld1_st1_mult.L?3:4; /* LD/ST */
      rob->opcd  = i.ld1_st1_mult.Q?12:11; /* 12:VLDRQ/VSTRQ(16), 11:VLDRD/VSTRD(8) */
      rob->sop   = 0; /* not used */
      rob->ptw   = 0; /* not used */
      rob->size  = 0; /* not used */
      rob->idx   = 0; /* not used */
      rob->rep   = 0; /* not used */
      rob->dir   = 0; /* not used */
      rob->iinv  = 0; /* not used */
      rob->oinv  = 0; /* not used */
      rob->dbl   = i.ld1_st1_mult.Q; /* 0:ld64, 1:ld128 */
      rob->plus  = 1; /* plus */
      rob->pre   = !post_imm; /* addr+offset */
      rob->wb    = post_imm;
      rob->updt  = 0; /* not used */
      v0 = VECREGTOP+(i.ld1_st1_mult.rt + mc)%VECREG;
      rob->sr[0].t = !t[tid].map[i.ld1_st1_mult.rn].x ? 2 :
                     (c[cid].rob[t[tid].map[i.ld1_st1_mult.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[0].x = t[tid].map[i.ld1_st1_mult.rn].x; /* reg */
      rob->sr[0].n = !t[tid].map[i.ld1_st1_mult.rn].x ? i.ld1_st1_mult.rn : t[tid].map[i.ld1_st1_mult.rn].rob;
      rob->sr[1].t = 1; /* immediate */
      rob->sr[1].x = 0; /* renamig N.A. */
      rob->sr[1].n = (i.ld1_st1_mult.Q?16:8)*(post_imm?1:mc);
      if (i.ld1_st1_mult.L==0) { /* ST */
	rob->sr[3].t = !t[tid].map[v0].x ? 2 :                                              
                       (c[cid].rob[t[tid].map[v0].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
	rob->sr[3].x = t[tid].map[v0].x; /* reg */                                          
	rob->sr[3].n = !t[tid].map[v0].x ? v0 : t[tid].map[v0].rob;                         
      }
      /* dest */
      if (i.ld1_st1_mult.L) { /* LD */
        rob->dr[1].t = 1;
        rob->dr[1].n = v0;
        t[tid].map[v0].x = 1;
        t[tid].map[v0].rob = robid;
      }
      if (rob->wb) {
        rob->dr[2].t = 1; /* writeback-basereg */
        rob->dr[2].n = i.ld1_st1_mult.rn;
        t[tid].map[i.ld1_st1_mult.rn].x = 2;
        t[tid].map[i.ld1_st1_mult.rn].rob = robid;
      }
      if (mc+1 < num_ldst) { /* continue */
        rob->term = 0;
        return (1); /* continue */
      }
      else if (post_reg && mc < num_ldst) {
        rob->term = 0;
        return (1); /* continue */
      }
      else
        return (0); /* finish */
    }
    else { /* post_reg & update base */
      rob->type  = 0; /* ALU */
      rob->opcd  = 4; /* ADD */
      rob->sop   = 0; /* LSL */
      rob->ptw   = 0; /* not used */
      rob->size  = 0; /* not used */
      rob->idx   = 0; /* not used */
      rob->rep   = 0; /* not used */
      rob->dir   = 0; /* not used */
      rob->iinv  = 0; /* not used */
      rob->oinv  = 0; /* not used */
      rob->dbl   = 1; /* 1:add64 */
      rob->plus  = 0; /* not used */
      rob->pre   = 0; /* not used */
      rob->wb    = 0; /* not used */
      rob->updt  = 0; /* not used */
      rob->sr[0].t = !t[tid].map[i.ld1_st1_mult.rn].x ? 2 :
                     (c[cid].rob[t[tid].map[i.ld1_st1_mult.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[0].x = t[tid].map[i.ld1_st1_mult.rn].x; /* reg */
      rob->sr[0].n = !t[tid].map[i.ld1_st1_mult.rn].x ? i.ld1_st1_mult.rn : t[tid].map[i.ld1_st1_mult.rn].rob;
      rob->sr[1].t = !t[tid].map[i.ld1_st1_mult.rm].x ? 2 :
                     (c[cid].rob[t[tid].map[i.ld1_st1_mult.rm].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[1].x = t[tid].map[i.ld1_st1_mult.rm].x; /* reg */
      rob->sr[1].n = !t[tid].map[i.ld1_st1_mult.rm].x ? i.ld1_st1_mult.rm : t[tid].map[i.ld1_st1_mult.rm].rob;
      rob->sr[2].t = 1; /* immediate */
      rob->sr[2].x = 0; /* renamig N.A. */
      rob->sr[2].n = 0; /* LSL 0 */
      /* dest */
      rob->dr[1].t = 1;
      rob->dr[1].n = i.ld1_st1_mult.rn;
      t[tid].map[i.ld1_st1_mult.rn].x = 1;
      t[tid].map[i.ld1_st1_mult.rn].rob = robid;
      return (0); /* finish */
    }
  }
  else if (i.ld1_st1_sing.op31==0 && i.ld1_st1_sing.op29_25==0x06 && i.ld1_st1_sing.op24==1 && (i.ld1_st1_sing.opcode&6)!=6) { /* C7.3.153 LD1(single structures), C7.3.156 LD2(single structure), C7.3.159 LD3(single structure), C7.3.162 LD4(single structure), C7.3.276 ST1(single structure), C7.3.278 ST2(single structure), C7.3.280 ST3(single structure), C7.3.282 ST4(single structure) */
    /* C7.3.153 ld1_nofs(1r  ):op23=0,R=0,op=xx0      |C7.3.276 st1_nofs(1r  ) */
    /* C7.3.153 ld1_post(1r+r):op23=1,R=0,op=xx0,rm<31|C7.3.276 st1_post(1r+r) */
    /* C7.3.153 ld1_post(1r+i):op23=1,R=0,op=xx0,rm=31|C7.3.276 st1_post(1r+i) */
    /*     B: opc=000             -> scl=0 esz=1B selem=1 idx=Q:S:size<1:0>    */
    /*     H: opc=010     size=x0 -> scl=1 esz=2B selem=1 idx=Q:S:size<1>      */
    /*     S: opc=100     size=00 -> scl=2 esz=4B selem=1 idx=Q:S              */
    /*     D: opc=100 S=0 size=01 -> scl=3 esz=8B selem=1 idx=Q                */
    /*=========================================================================================*/
    /* 1レジスタの指定位置に直接格納                                                           */
    /*  offs = 0;                                                                              */
    /*  LD: V[r].Elem[idx, esz] = Mem[addr+offs, ebytes, AccT]; loc[idx*esz] <- m[esz]         */
    /*  ST: Mem[addr+offs, ebytes, AccT] = V[r].Elem[idx, esz]; m[esz] <- loc[idx*esz]         */
    /*  offs = offs + ebytes;                                     m+=esz                       */
    /*=========================================================================================*/
    /* C7.3.156 ld2_nofs(2r  ):op23=0,R=1,op=xx0      |C7.3.278 st2_nofs(2r  ) *//* 当面未実装 */
    /* C7.3.156 ld2_post(2r+r):op23=1,R=1,op=xx0,rm<31|C7.3.278 st2_post(2r+r) *//* 当面未実装 */
    /* C7.3.156 ld2_post(2r+i):op23=1,R=1,op=xx0,rm=31|C7.3.278 st2_post(2r+i) *//* 当面未実装 */

    /* C7.3.159 ld3_nofs(3r  ):op23=0,R=0,op=xx1      |C7.3.280 st3_nofs(3r  ) *//* 当面未実装 */
    /* C7.3.159 ld3_post(3r+r):op23=1,R=0,op=xx1,rm<31|C7.3.280 st3_post(3r+r) *//* 当面未実装 */
    /* C7.3.159 ld3_post(3r+i):op23=1,R=0,op=xx1,rm=31|C7.3.280 st3_post(3r+i) *//* 当面未実装 */

    /* C7.3.162 ld4_nofs(4r  ):op23=0,R=1,op=xx1      |C7.3.282 st4_nofs(4r  ) *//* 当面未実装 */
    /* C7.3.162 ld4_post(4r+r):op23=1,R=1,op=xx1,rm<31|C7.3.282 st4_post(4r+r) *//* 当面未実装 */
    /* C7.3.162 ld4_post(4r+i):op23=1,R=1,op=xx1,rm=31|C7.3.282 st4_post(4r+i) *//* 当面未実装 */
    /*=========================================================================================*/
    /* if wback then                                                                           */
    /*  if m != 31 then    offs = X[m];                                                        */
    /*  if n == 31  SP[] = address + offs;                                                     */
    /*  else        X[n] = address + offs;                                                     */
    /*=========================================================================================*/
    int num_ldst;
    int esz, idx;
    int no_post_inc = i.ld1_st1_sing.op23==0;
    int post_imm = i.ld1_st1_sing.op23 && i.ld1_st1_sing.rm==31; /* auto_immediate_offset */
    int post_reg = i.ld1_st1_sing.op23 && i.ld1_st1_sing.rm!=31; /* auto_immediate_offset */
    switch (i.ld1_st1_sing.opcode) {
    case 0: { num_ldst=1; esz=1; idx=(i.ld1_st1_sing.Q<<3)|(i.ld1_st1_sing.S<<2)|i.ld1_st1_sing.size; break;}
    case 2: { num_ldst=1; esz=2; idx=(i.ld1_st1_sing.Q<<3)|(i.ld1_st1_sing.S<<2)|i.ld1_st1_sing.size; break;}
    case 4: if ((i.ld1_st1_sing.size&1)==0)
            { num_ldst=1; esz=4; idx=(i.ld1_st1_sing.Q<<3)|(i.ld1_st1_sing.S<<2); break;}
            else
	    { num_ldst=1; esz=8; idx=(i.ld1_st1_sing.Q<<3); break;}
    default: i_xxx(rob); return (0);
    }
    if (!post_reg || mc<num_ldst) {
      rob->type  = i.ld1_st1_sing.L?3:4; /* LD/ST */
      rob->opcd  = esz==1?8:esz==2?9:esz==4?10:11; /* 8:VLDRB(8), 9:VlDRH(16), 10:VLDRS(32), 11:VLDRD(64) */
                                                   /* 8:VSTRB(8), 9:VSTRH(16), 10:VSTRS(32), 11:VSTRD(64) */
      rob->sop   = 0; /* not used */
      rob->ptw   = 1; /* partial_reg_write */
      rob->size  = 0; /* not used */
      rob->idx   = idx; /* reg_index 15-0 byte-portion */
      rob->rep   = 0; /* not used */
      rob->dir   = 0; /* not used */
      rob->iinv  = 0; /* not used */
      rob->oinv  = 0; /* not used */
      rob->dbl   = i.ld1_st1_sing.Q; /* 0:ld64, 1:ld128 */
      rob->plus  = 1; /* plus */
      rob->pre   = !post_imm; /* addr+offset */
      rob->wb    = post_imm;
      rob->updt  = 0; /* not used */
      v0 = VECREGTOP+(i.ld1_st1_sing.rt + mc)%VECREG;
      rob->sr[0].t = !t[tid].map[i.ld1_st1_sing.rn].x ? 2 :
                     (c[cid].rob[t[tid].map[i.ld1_st1_sing.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[0].x = t[tid].map[i.ld1_st1_sing.rn].x; /* reg */
      rob->sr[0].n = !t[tid].map[i.ld1_st1_sing.rn].x ? i.ld1_st1_sing.rn : t[tid].map[i.ld1_st1_sing.rn].rob;
      rob->sr[1].t = 1; /* immediate */
      rob->sr[1].x = 0; /* renamig N.A. */
      rob->sr[1].n = esz*(post_imm?1:mc);
      rob->sr[3].t = !t[tid].map[v0].x ? 2 : /* for partial_LD and ST */
                     (c[cid].rob[t[tid].map[v0].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[3].x = t[tid].map[v0].x; /* reg */
      rob->sr[3].n = !t[tid].map[v0].x ? v0 : t[tid].map[v0].rob;
      /* dest */
      if (i.ld1_st1_sing.L) { /* LD */
        rob->dr[1].t = 1;
        rob->dr[1].n = v0;
        t[tid].map[v0].x = 1;
        t[tid].map[v0].rob = robid;
      }
      if (rob->wb) {
        rob->dr[2].t = 1; /* writeback-basereg */
        rob->dr[2].n = i.ld1_st1_sing.rn;
        t[tid].map[i.ld1_st1_sing.rn].x = 2;
        t[tid].map[i.ld1_st1_sing.rn].rob = robid;
      }
      if (mc+1 < num_ldst) { /* continue */
        rob->term = 0;
        return (1); /* continue */
      }
      else if (post_reg && mc < num_ldst) {
        rob->term = 0;
        return (1); /* continue */
      }
      else
        return (0); /* finish */
    }
    else { /* post_reg & update base */
      rob->type  = 0; /* ALU */
      rob->opcd  = 4; /* ADD */
      rob->sop   = 0; /* LSL */
      rob->ptw   = 0; /* not used */
      rob->size  = 0; /* not used */
      rob->idx   = 0; /* not used */
      rob->rep   = 0; /* not used */
      rob->dir   = 0; /* not used */
      rob->iinv  = 0; /* not used */
      rob->oinv  = 0; /* not used */
      rob->dbl   = 1; /* 1:add64 */
      rob->plus  = 0; /* not used */
      rob->pre   = 0; /* not used */
      rob->wb    = 0; /* not used */
      rob->updt  = 0; /* not used */
      rob->sr[0].t = !t[tid].map[i.ld1_st1_sing.rn].x ? 2 :
                     (c[cid].rob[t[tid].map[i.ld1_st1_sing.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[0].x = t[tid].map[i.ld1_st1_sing.rn].x; /* reg */
      rob->sr[0].n = !t[tid].map[i.ld1_st1_sing.rn].x ? i.ld1_st1_sing.rn : t[tid].map[i.ld1_st1_sing.rn].rob;
      rob->sr[1].t = !t[tid].map[i.ld1_st1_sing.rm].x ? 2 :
                     (c[cid].rob[t[tid].map[i.ld1_st1_sing.rm].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[1].x = t[tid].map[i.ld1_st1_sing.rm].x; /* reg */
      rob->sr[1].n = !t[tid].map[i.ld1_st1_sing.rm].x ? i.ld1_st1_sing.rm : t[tid].map[i.ld1_st1_sing.rm].rob;
      rob->sr[2].t = 1; /* immediate */
      rob->sr[2].x = 0; /* renamig N.A. */
      rob->sr[2].n = 0; /* LSL 0 */
      /* dest */
      rob->dr[1].t = 1;
      rob->dr[1].n = i.ld1_st1_sing.rn;
      t[tid].map[i.ld1_st1_sing.rn].x = 1;
      t[tid].map[i.ld1_st1_sing.rn].rob = robid;
      return (0); /* finish */
    }
  }
  else if (i.ld1_st1_sing.op31==0 && i.ld1_st1_sing.op29_25==0x06 && i.ld1_st1_sing.op24==1 && i.ld1_st1_sing.L==1 && (i.ld1_st1_sing.opcode&6)==6) { /* C7.3.154 LD1R, C7.3.157 LD2R, C7.3.160 LD3R, C7.3.163 LD4R */
    /* C7.3.154 ld1_nofs(1r  ):op23=0,R=0,op=110       */
    /* C7.3.154 ld1_post(1r+r):op23=1,R=0,op=110,rm<31 */
    /* C7.3.154 ld1_post(1r+i):op23=1,R=0,op=110,rm=31 */
    /*    8B: opc=110 S=0 size=00 Q=0 -> scl=4 esz=16B selem=1 replicate */
    /*   16B: opc=110 S=0 size=00 Q=1 -> scl=4 esz=16B selem=1 replicate */
    /*    4H: opc=110 S=0 size=01 Q=0 -> scl=4 esz=16B selem=1 replicate */
    /*    8H: opc=110 S=0 size=01 Q=1 -> scl=4 esz=16B selem=1 replicate */
    /*    2S: opc=110 S=0 size=10 Q=0 -> scl=4 esz=16B selem=1 replicate */
    /*    4S: opc=110 S=0 size=10 Q=1 -> scl=4 esz=16B selem=1 replicate */
    /*    1D: opc=110 S=0 size=11 Q=0 -> scl=4 esz=16B selem=1 replicate */
    /*    2D: opc=110 S=0 size=11 Q=1 -> scl=4 esz=16B selem=1 replicate */
    /*=================================================================*/
    /*=================================================================*/
    /* C7.3.157 ld2_nofs(2r  ):op23=0,R=1,op=110       *//* 当面未実装 */
    /* C7.3.157 ld2_post(2r+r):op23=1,R=1,op=110,rm<31 *//* 当面未実装 */
    /* C7.3.157 ld2_post(2r+i):op23=1,R=1,op=110,rm=31 *//* 当面未実装 */

    /* C7.3.160 ld3_nofs(3r  ):op23=0,R=0,op=111       *//* 当面未実装 */
    /* C7.3.160 ld3_post(3r+r):op23=1,R=0,op=111,rm<31 *//* 当面未実装 */
    /* C7.3.160 ld3_post(3r+i):op23=1,R=0,op=111,rm=31 *//* 当面未実装 */

    /* C7.3.163 ld4_nofs(4r  ):op23=0,R=1,op=111       *//* 当面未実装 */
    /* C7.3.163 ld4_post(4r+r):op23=1,R=1,op=111,rm<31 *//* 当面未実装 */
    /* C7.3.163 ld4_post(4r+i):op23=1,R=1,op=111,rm=31 *//* 当面未実装 */
    /*=================================================================*/
    /* if wback then                                                   */
    /*  if m != 31 then    offs = X[m];                                */
    /*  if n == 31  SP[] = address + offs;                             */
    /*  else        X[n] = address + offs;                             */
    /*=================================================================*/
    int num_ldst;
    int esz;
    int no_post_inc = i.ld1_st1_sing.op23==0;
    int post_imm = i.ld1_st1_sing.op23 && i.ld1_st1_sing.rm==31; /* auto_immediate_offset */
    int post_reg = i.ld1_st1_sing.op23 && i.ld1_st1_sing.rm!=31; /* auto_immediate_offset */
    switch (i.ld1_st1_sing.opcode) {
    case 6: { num_ldst=1; esz=1<<i.ld1_st1_sing.size; break;}
    default: i_xxx(rob); return (0);
    }
    if (!post_reg || mc<num_ldst) {
      rob->type  = 3; /* LD */
      rob->opcd  = esz==1?8:esz==2?9:esz==4?10:11; /* 8:VLDRB(8), 9:VlDRH(16), 10:VLDRS(32), 11:VLDRD(64) */
      rob->sop   = 0; /* not used */
      rob->ptw   = 0; /* not used */
      rob->size  = 0; /* not used */
      rob->idx   = 0; /* not used */
      rob->rep   = 1; /* repeat */
      rob->dir   = 0; /* not used */
      rob->iinv  = 0; /* not used */
      rob->oinv  = 0; /* not used */
      rob->dbl   = i.ld1_st1_sing.Q; /* 0:ld64, 1:ld128 */
      rob->plus  = 1; /* plus */
      rob->pre   = !post_imm; /* addr+offset */
      rob->wb    = post_imm;
      rob->updt  = 0; /* not used */
      v0 = VECREGTOP+(i.ld1_st1_sing.rt + mc)%VECREG;
      rob->sr[0].t = !t[tid].map[i.ld1_st1_sing.rn].x ? 2 :
                     (c[cid].rob[t[tid].map[i.ld1_st1_sing.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[0].x = t[tid].map[i.ld1_st1_sing.rn].x; /* reg */
      rob->sr[0].n = !t[tid].map[i.ld1_st1_sing.rn].x ? i.ld1_st1_sing.rn : t[tid].map[i.ld1_st1_sing.rn].rob;
      rob->sr[1].t = 1; /* immediate */
      rob->sr[1].x = 0; /* renamig N.A. */
      rob->sr[1].n = esz*(post_imm?1:mc);
      /* dest */
      if (i.ld1_st1_sing.L) { /* LD */
        rob->dr[1].t = 1;
        rob->dr[1].n = v0;
        t[tid].map[v0].x = 1;
        t[tid].map[v0].rob = robid;
      }
      if (rob->wb) {
        rob->dr[2].t = 1; /* writeback-basereg */
        rob->dr[2].n = i.ld1_st1_sing.rn;
        t[tid].map[i.ld1_st1_sing.rn].x = 2;
        t[tid].map[i.ld1_st1_sing.rn].rob = robid;
      }
      if (mc+1 < num_ldst) { /* continue */
        rob->term = 0;
        return (1); /* continue */
      }
      else if (post_reg && mc < num_ldst) {
        rob->term = 0;
        return (1); /* continue */
      }
      else
        return (0); /* finish */
    }
    else { /* post_reg & update base */
      rob->type  = 0; /* ALU */
      rob->opcd  = 4; /* ADD */
      rob->sop   = 0; /* LSL */
      rob->ptw   = 0; /* not used */
      rob->size  = 0; /* not used */
      rob->idx   = 0; /* not used */
      rob->rep   = 0; /* not used */
      rob->dir   = 0; /* not used */
      rob->iinv  = 0; /* not used */
      rob->oinv  = 0; /* not used */
      rob->dbl   = 1; /* 1:add64 */
      rob->plus  = 0; /* not used */
      rob->pre   = 0; /* not used */
      rob->wb    = 0; /* not used */
      rob->updt  = 0; /* not used */
      rob->sr[0].t = !t[tid].map[i.ld1_st1_sing.rn].x ? 2 :
                     (c[cid].rob[t[tid].map[i.ld1_st1_sing.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[0].x = t[tid].map[i.ld1_st1_sing.rn].x; /* reg */
      rob->sr[0].n = !t[tid].map[i.ld1_st1_sing.rn].x ? i.ld1_st1_sing.rn : t[tid].map[i.ld1_st1_sing.rn].rob;
      rob->sr[1].t = !t[tid].map[i.ld1_st1_sing.rm].x ? 2 :
                     (c[cid].rob[t[tid].map[i.ld1_st1_sing.rm].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[1].x = t[tid].map[i.ld1_st1_sing.rm].x; /* reg */
      rob->sr[1].n = !t[tid].map[i.ld1_st1_sing.rm].x ? i.ld1_st1_sing.rm : t[tid].map[i.ld1_st1_sing.rm].rob;
      rob->sr[2].t = 1; /* immediate */
      rob->sr[2].x = 0; /* renamig N.A. */
      rob->sr[2].n = 0; /* LSL 0 */
      /* dest */
      rob->dr[1].t = 1;
      rob->dr[1].n = i.ld1_st1_sing.rn;
      t[tid].map[i.ld1_st1_sing.rn].x = 1;
      t[tid].map[i.ld1_st1_sing.rn].rob = robid;
      return (0); /* finish */
    }
    return (0); /* finish */
  }
  else if (i.mad.sf==1 && i.mad.op30_24==0x1b && i.mad.op22_21==1) { /* C6.6.163 SMADDL, C6.6.165 SMNEGL, C6.6.166 SMSUBL, C6.6.168 SMULL, C6.6.215 UMADDL, C6.6.216 UMNEGL, C6.6.217 UMSUBL, C6.6.219 UMULL */
    rob->type  = 1; /* MUL */
    rob->opcd  = i.mad.U?(i.mad.o0==0?0:1):(i.mad.o0==0?2:3); /* UMADDL/UMSUBL/SMADDL/SMSUBL */
    rob->sop   = 0; /* not used */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.mad.sf; /* always 64bit */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    rob->sr[0].t = i.mad.rn==31 ? 1 : /* ZERO */
                   !t[tid].map[i.mad.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.mad.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = i.mad.rn==31 ? 0 : /* ZERO */
                   t[tid].map[i.mad.rn].x; /* reg */
    rob->sr[0].n = i.mad.rn==31 ? 0 : /* ZERO */
                   !t[tid].map[i.mad.rn].x ? i.mad.rn : t[tid].map[i.mad.rn].rob;
    rob->sr[1].t = i.mad.rm==31 ? 1 : /* ZERO */
                   !t[tid].map[i.mad.rm].x ? 2 :
                   (c[cid].rob[t[tid].map[i.mad.rm].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = i.mad.rm==31 ? 0 : /* ZERO */
                   t[tid].map[i.mad.rm].x; /* reg */
    rob->sr[1].n = i.mad.rm==31 ? 0 : /* ZERO */
                   !t[tid].map[i.mad.rm].x ? i.mad.rm : t[tid].map[i.mad.rm].rob;
    rob->sr[2].t = i.mad.ra==31 ? 1 : /* ZERO */
                   !t[tid].map[i.mad.ra].x ? 2 :
                   (c[cid].rob[t[tid].map[i.mad.ra].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[2].x = i.mad.ra==31 ? 0 : /* ZERO */
                   t[tid].map[i.mad.ra].x; /* reg */
    rob->sr[2].n = i.mad.ra==31 ? 0 : /* ZERO */
                   !t[tid].map[i.mad.ra].x ? i.mad.ra : t[tid].map[i.mad.ra].rob;
    /* dest */
    if (i.mad.rd!=31) { /* MUL */
      rob->dr[1].t = 1;
      rob->dr[1].n = i.mad.rd;
      t[tid].map[i.mad.rd].x = 1;
      t[tid].map[i.mad.rd].rob = robid;
    }
    return (0);
  }
  else if (i.mad.op30_24==0x1b && i.mad.U==0 && i.mad.op22_21==0) { /* C6.6.119 MADD, C6.6.133 MUL, C6.6.120 MNEG, C6.6.132 MSUB */
    rob->type  = 1; /* MUL */
    rob->opcd  = i.mad.o0==0?4:5; /* MADD/MSUB */
    rob->sop   = 0; /* not used */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.mad.sf; /* 0:mad32, 1:mad64 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    rob->sr[0].t = i.mad.rn==31 ? 1 : /* ZERO */
                   !t[tid].map[i.mad.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.mad.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = i.mad.rn==31 ? 0 : /* ZERO */
                   t[tid].map[i.mad.rn].x; /* reg */
    rob->sr[0].n = i.mad.rn==31 ? 0 : /* ZERO */
                   !t[tid].map[i.mad.rn].x ? i.mad.rn : t[tid].map[i.mad.rn].rob;
    rob->sr[1].t = i.mad.rm==31 ? 1 : /* ZERO */
                   !t[tid].map[i.mad.rm].x ? 2 :
                   (c[cid].rob[t[tid].map[i.mad.rm].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = i.mad.rm==31 ? 0 : /* ZERO */
                   t[tid].map[i.mad.rm].x; /* reg */
    rob->sr[1].n = i.mad.rm==31 ? 0 : /* ZERO */
                   !t[tid].map[i.mad.rm].x ? i.mad.rm : t[tid].map[i.mad.rm].rob;
    rob->sr[2].t = i.mad.ra==31 ? 1 : /* ZERO */
                   !t[tid].map[i.mad.ra].x ? 2 :
                   (c[cid].rob[t[tid].map[i.mad.ra].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[2].x = i.mad.ra==31 ? 0 : /* ZERO */
                   t[tid].map[i.mad.ra].x; /* reg */
    rob->sr[2].n = i.mad.ra==31 ? 0 : /* ZERO */
                   !t[tid].map[i.mad.ra].x ? i.mad.ra : t[tid].map[i.mad.ra].rob;
    /* dest */
    if (i.mad.rd!=31) { /* MUL */
      rob->dr[1].t = 1;
      rob->dr[1].n = i.mad.rd;
      t[tid].map[i.mad.rd].x = 1;
      t[tid].map[i.mad.rd].rob = robid;
    }
    return (0);
  }
  else if (i.mad.sf==1 && i.mad.op30_24==0x1b && i.mad.op22_21==2 && i.mad.o0==0 && i.mad.ra==31) { /* C6.6.167 SMULH, C6.6.218 UMULH */
    rob->type  = 1; /* MUL */
    rob->opcd  = i.mad.U==0?6:7; /* SMULH/UMULH */
    rob->sop   = 0; /* not used */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = 1; /* 0:mad32, 1:mad64 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    rob->sr[0].t = i.mad.rn==31 ? 1 : /* ZERO */
                   !t[tid].map[i.mad.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.mad.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = i.mad.rn==31 ? 0 : /* ZERO */
                   t[tid].map[i.mad.rn].x; /* reg */
    rob->sr[0].n = i.mad.rn==31 ? 0 : /* ZERO */
                   !t[tid].map[i.mad.rn].x ? i.mad.rn : t[tid].map[i.mad.rn].rob;
    rob->sr[1].t = i.mad.rm==31 ? 1 : /* ZERO */
                   !t[tid].map[i.mad.rm].x ? 2 :
                   (c[cid].rob[t[tid].map[i.mad.rm].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = i.mad.rm==31 ? 0 : /* ZERO */
                   t[tid].map[i.mad.rm].x; /* reg */
    rob->sr[1].n = i.mad.rm==31 ? 0 : /* ZERO */
                   !t[tid].map[i.mad.rm].x ? i.mad.rm : t[tid].map[i.mad.rm].rob;
    /* dest */
    if (i.mad.rd!=31) { /* MUL */
      rob->dr[1].t = 1;
      rob->dr[1].n = i.mad.rd;
      t[tid].map[i.mad.rd].x = 1;
      t[tid].map[i.mad.rd].rob = robid;
    }
    return (0);
  }
  else if (i.div.op30_21==0x0d6 && i.div.op15_11==0x01) { /* C6.6.160 SDIV, C6.6.214 UDIV */
    rob->type  = 1; /* MUL */
    rob->opcd  = i.div.o1==0?8:9; /* UDIV/SDIV */
    rob->sop   = 0; /* not used */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.div.sf; /* 0:div32, 1:div64 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    rob->sr[0].t = i.div.rn==31 ? 1 : /* ZERO */
                   !t[tid].map[i.div.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.div.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = i.div.rn==31 ? 0 : /* ZERO */
                   t[tid].map[i.div.rn].x; /* reg */
    rob->sr[0].n = i.div.rn==31 ? 0 : /* ZERO */
                   !t[tid].map[i.div.rn].x ? i.div.rn : t[tid].map[i.div.rn].rob;
    rob->sr[1].t = i.div.rm==31 ? 1 : /* ZERO */
                   !t[tid].map[i.div.rm].x ? 2 :
                   (c[cid].rob[t[tid].map[i.div.rm].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = i.div.rm==31 ? 0 : /* ZERO */
                   t[tid].map[i.div.rm].x; /* reg */
    rob->sr[1].n = i.div.rm==31 ? 0 : /* ZERO */
                   !t[tid].map[i.div.rm].x ? i.div.rm : t[tid].map[i.div.rm].rob;
    /* dest */
    if (i.div.rd!=31) { /* DIV */
      rob->dr[1].t = 1;
      rob->dr[1].n = i.div.rd;
      t[tid].map[i.div.rd].x = 1;
      t[tid].map[i.div.rd].rob = robid;
    }
    return (0);
  }
  else if (i.fmov_gen.op30_24==0x1e && i.fmov_gen.op21==1 && i.fmov_gen.op15_10==0) { /* C7.3.114 FMOV(general), C7.3.59 FCVTAS(scalar), C7.3.61 FCVTAU(scalar), C7.3.64 FCVTMS(scalar), C7.3.66 FCVTMU(scalar), C7.3.69 FCVTNS(scalar), C7.3.71 FCVTNU(scalar), C7.3.73 FCVTPS(scalar), C7.3.75 FCVTPU(scalar), C7.3.80 FCVTZS(scalar,integer), C7.3.84 FCVTZU(scalar,integer), C7.3.210 SCVTF(scalar,integer), C7.3.308 UCVTF(scalar,integer) */
    /* C7.3.114 fmov   s <-w: sf=0, type=0, rm=0, opc=111 |              C7.3.114 fmov   w<-s:  sf=0, type=0, rm=0, opc=110 */
    /* C7.3.114 fmov   d0<-x: sf=1, type=1, rm=0, opc=111 |              C7.3.114 fmov   x<-d0: sf=1, type=1, rm=0, opc=110 */
    /* C7.3.114 fmov   d1<-x: sf=1, type=2, rm=1, opc=111 |              C7.3.114 fmov   x<-d1: sf=1, type=2, rm=1, opc=110 */

    /*                                                      (int) rintf  C7.3.69  fcvtns w <-s: sf=0, type=0, rm=0, opc=000 */
    /*                                                      (Sll) rintf  C7.3.69  fcvtns x <-s: sf=1, type=0, rm=0, opc=000 */
    /*                                                      (int) rint   C7.3.69  fcvtns w <-d: sf=0, type=1, rm=0, opc=000 */
    /*                                                      (Sll) rint   C7.3.69  fcvtns x <-d: sf=1, type=1, rm=0, opc=000 */
    /*                                                      (int) ceilf  C7.3.73  fcvtps w <-s: sf=0, type=0, rm=1, opc=000 */
    /*                                                      (Sll) ceilf  C7.3.73  fcvtps x <-s: sf=1, type=0, rm=1, opc=000 */
    /*                                                      (int) ceil   C7.3.73  fcvtps w <-d: sf=0, type=1, rm=1, opc=000 */
    /*                                                      (Sll) ceil   C7.3.73  fcvtps x <-d: sf=1, type=1, rm=1, opc=000 */
    /*                                                      (int) floorf C7.3.64  fcvtms w <-s: sf=0, type=0, rm=2, opc=000 */
    /*                                                      (Sll) floorf C7.3.64  fcvtms x <-s: sf=1, type=0, rm=2, opc=000 */
    /*                                                      (int) floor  C7.3.64  fcvtms w <-d: sf=0, type=1, rm=2, opc=000 */
    /*                                                      (Sll) floor  C7.3.64  fcvtms x <-d: sf=1, type=1, rm=2, opc=000 */
    /*                                                      (int) truncf C7.3.80  fcvtzs w <-s: sf=0, type=0, rm=3, opc=000 */
    /*                                                      (Sll) truncf C7.3.80  fcvtzs x <-s: sf=1, type=0, rm=3, opc=000 */
    /*                                                      (int) trunc  C7.3.80  fcvtzs w <-d: sf=0, type=1, rm=3, opc=000 */
    /*                                                      (Sll) trunc  C7.3.80  fcvtzs x <-d: sf=1, type=1, rm=3, opc=000 */
    /* C7.3.210 scvtf  s <-w: sf=0, type=0, rm=0, opc=010 | (int) roundf C7.3.59  fcvtas w <-s: sf=0, type=0, rm=0, opc=100 */
    /* C7.3.210 scvtf  s <-x: sf=1, type=0, rm=0, opc=010 | (Sll) roundf C7.3.59  fcvtas x <-s: sf=1, type=0, rm=0, opc=100 */
    /* C7.3.210 scvtf  d <-w: sf=0, type=1, rm=0, opc=010 | (int) round  C7.3.59  fcvtas w <-d: sf=0, type=1, rm=0, opc=100 */
    /* C7.3.210 scvtf  d <-x: sf=1, type=1, rm=0, opc=010 | (Sll) round  C7.3.59  fcvtas x <-d: sf=1, type=1, rm=0, opc=100 */

    /*                                                      (Uint)rintf  C7.3.71  fcvtnu w <-s: sf=0, type=0, rm=0, opc=001 */
    /*                                                      (Ull) rintf  C7.3.71  fcvtnu x <-s: sf=1, type=0, rm=0, opc=001 */
    /*                                                      (Uint)rint   C7.3.71  fcvtnu w <-d: sf=0, type=1, rm=0, opc=001 */
    /*                                                      (Ull) rint   C7.3.71  fcvtnu x <-d: sf=1, type=1, rm=0, opc=001 */
    /*                                                      (Uint)ceilf  C7.3.75  fcvtpu w <-s: sf=0, type=0, rm=1, opc=001 */
    /*                                                      (Ull) ceilf  C7.3.75  fcvtpu x <-s: sf=1, type=0, rm=1, opc=001 */
    /*                                                      (Uint)ceil   C7.3.75  fcvtpu w <-d: sf=0, type=1, rm=1, opc=001 */
    /*                                                      (Ull) ceil   C7.3.75  fcvtpu x <-d: sf=1, type=1, rm=1, opc=001 */
    /*                                                      (Uint)floorf C7.3.66  fcvtmu w <-s: sf=0, type=0, rm=2, opc=001 */
    /*                                                      (Ull) floorf C7.3.66  fcvtmu x <-s: sf=1, type=0, rm=2, opc=001 */
    /*                                                      (Uint)floor  C7.3.66  fcvtmu w <-d: sf=0, type=1, rm=2, opc=001 */
    /*                                                      (Ull) floor  C7.3.66  fcvtmu x <-d: sf=1, type=1, rm=2, opc=001 */
    /*                                                      (Uint)truncf C7.3.84  fcvtzu w <-s: sf=0, type=0, rm=3, opc=001 */
    /*                                                      (Ull) truncf C7.3.84  fcvtzu x <-s: sf=1, type=0, rm=3, opc=001 */
    /*                                                      (Uint)trunc  C7.3.84  fcvtzu w <-d: sf=0, type=1, rm=3, opc=001 */
    /*                                                      (Ull) trunc  C7.3.84  fcvtzu x <-d: sf=1, type=1, rm=3, opc=001 */
    /* C7.3.308 ucvtf  s <-w: sf=0, type=0, rm=0, opc=011 | (Uint)roundf C7.3.61  fcvtau w <-s: sf=0, type=0, rm=0, opc=101 */
    /* C7.3.308 ucvtf  s <-x: sf=1, type=0, rm=0, opc=011 | (Ull) roundf C7.3.61  fcvtau x <-s: sf=1, type=0, rm=0, opc=101 */
    /* C7.3.308 ucvtf  d <-w: sf=0, type=1, rm=0, opc=011 | (Uint)round  C7.3.61  fcvtau w <-d: sf=0, type=1, rm=0, opc=101 */
    /* C7.3.308 ucvtf  d <-x: sf=1, type=1, rm=0, opc=011 | (Ull) round  C7.3.61  fcvtau x <-d: sf=1, type=1, rm=0, opc=101 */
    rob->type  = 2; /* VXX */
    rob->opcd  =  (i.fmov_gen.opc>=6 && i.fmov_gen.type< 2)?0:   /* FMOVL */
                  (i.fmov_gen.opc>=6 && i.fmov_gen.type==2)?0:   /* FMOVH */
                (!(i.fmov_gen.opc&1) && i.fmov_gen.type==0)?2:   /* CVTSS with sop|=0 */
                (!(i.fmov_gen.opc&1) && i.fmov_gen.type==1)?2:   /* CVTSD with sop|=8 */
                ( (i.fmov_gen.opc&1) && i.fmov_gen.type==0)?3:   /* CVTUS with sop|=0 */
                ( (i.fmov_gen.opc&1) && i.fmov_gen.type==1)?3:0; /* CVTUD with sop|=8 /error */
    rob->sop   = (i.fmov_gen.opc>=6)?(i.fmov_gen.type<2?0:1):    /* 0:FMOVL, 1:FMOVH  */
               ((!(i.fmov_gen.opc&1) && i.fmov_gen.type==1)      /* CVTSD with sop|=8 */
              ||( (i.fmov_gen.opc&1) && i.fmov_gen.type==1)?8:0) /* CVTUD with sop|=8 */
	        |((i.fmov_gen.opc<=1)?i.fmov_gen.mode:4);        /* 0:rint, 1:ceil, 2:floor, 3:trunc, 4:round */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = (i.fmov_gen.opc==2 || i.fmov_gen.opc==3 || i.fmov_gen.opc==7)?1:0; /* 0:RR<-VR, 1:VR<-RR */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.fmov_gen.sf; /* 0:WR, 1:XR */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = !rob->dir? VECREGTOP+i.fmov_gen.rn : i.fmov_gen.rn; /* 0:RR<-VR, 1:VR<-RR */
    v0 = !rob->dir? i.fmov_gen.rd : VECREGTOP+i.fmov_gen.rd; /* 0:RR<-VR, 1:VR<-RR */
    rob->sr[0].t = (rob->dir && s1==31) ? 1 : /* ZERO */
                   !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = (rob->dir && s1==31) ? 0 : /* ZERO */
                   t[tid].map[s1].x; /* reg */
    rob->sr[0].n = (rob->dir && s1==31) ? 0 : /* ZERO */
                   !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    if (i.fmov_gen.opc==7 && i.fmov_gen.sf) { /* partial update needs d0 */
      rob->sr[1].t = !t[tid].map[v0].x ? 2 :
                     (c[cid].rob[t[tid].map[v0].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[1].x = t[tid].map[v0].x; /* reg */
      rob->sr[1].n = !t[tid].map[v0].x ? v0 : t[tid].map[v0].rob;
    }
    /* dest */
    if (rob->dir || v0!=31) { /* MOV */
      rob->dr[1].t = 1;
      rob->dr[1].n = v0;
      t[tid].map[v0].x = 1;
      t[tid].map[v0].rob = robid;
    }
    return (0);
  }
  else if (i.fmov_sca_imm.op31_24==0x1e && (i.fmov_sca_imm.type&2)==0 && i.fmov_sca_imm.op21==1 && i.fmov_sca_imm.op12_10==4 && i.fmov_sca_imm.rn==0 ) { /* C7.3.115 FMOV(scalar,immediate) */
    Ull i8  = i.fmov_sca_imm.imm8;
    Ull s   = i8>>7;
    Ull xe6 = (~i8>>6)&1;
    Ull e6  = (i8>>6)&1;
    Ull e54 = (i8>>4)&3;
    Ull f   = i8&15;
    rob->type  = 2; /* VXX */
    rob->opcd  = 0; /* FMOVL */
    rob->sop   = 0; /* not used */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 1; /* 0:RR<-VR, 1:VR<-RR */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.fmov_sca_imm.type&1; /* 0:Sd, 1:Dd */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    v0 = VECREGTOP+i.fmov_sca_imm.rd;
    rob->sr[0].t = 1; /* immediate */
    rob->sr[0].x = 0; /* renamig N.A. */
    if ((i.fmov_sca_imm.type&1)==0) { /* single s=1 e=8  f=23 */
      /* s.xe6.e6.e6.e6.e6.e6.e54.f.zero(19) */
      rob->sr[0].n = (s<<31)|(xe6<<30)|(e6<<29)|(e6<<28)|(e6<<27)|(e6<<26)|(e6<<25)|(e54<<23)|(f<<19);
    }
    else { /* double s=1 e=11 f=52 */
      /* s.xe6.e6.e6.e6.e6.e6.e6.e6.e6.e54.f.zero(48) */
      rob->sr[0].n = (s<<63)|(xe6<<62)|(e6<<61)|(e6<<60)|(e6<<59)|(e6<<58)|(e6<<57)|(e6<<56)|(e6<<55)|(e6<<54)|(e54<<52)|(f<<48);
    }
    rob->sr[1].t = 1; /* immediate */
    rob->sr[1].x = 0; /* renamig N.A. */
    rob->sr[1].n = 0; /* ZERO */
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.movi.op31==0 && i.movi.op28_19==0x1e0 && i.movi.op11_10==1) { /* C7.3.12 BIC(vector,immediate), C7.3.112 FMOV(vector,immediate), C7.3.179 MOVI, C7.3.183 MVNI, C7.3.187 ORR(vector,immediate) */
    /* C7.3.179 movi32  Vd.T,#imm8,lsl #0 :      cmode=0000, op=0 */
    /* C7.3.183 mvni32  Vd.T,#imm8,lsl #0 :      cmode=0000, op=1 */
    /* C7.3.187 orr32   Vd.T,#imm8,lsl #0 :      cmode=0001, op=0 */
    /* C7.3.12  bic32   Vd.T,#imm8,lsl #0 :      cmode=0001, op=1 */
    /* C7.3.179 movi32  Vd.T,#imm8,lsl #8 :      cmode=0010, op=0 */
    /* C7.3.183 mvni32  Vd.T,#imm8,lsl #8 :      cmode=0010, op=1 */
    /* C7.3.187 orr32   Vd.T,#imm8,lsl #8 :      cmode=0011, op=0 */
    /* C7.3.12  bic32   Vd.T,#imm8,lsl #8 :      cmode=0011, op=1 */
    /* C7.3.179 movi32  Vd.T,#imm8,lsl #16:      cmode=0100, op=0 */
    /* C7.3.183 mvni32  Vd.T,#imm8,lsl #16:      cmode=0100, op=1 */
    /* C7.3.187 orr32   Vd.T,#imm8,lsl #16:      cmode=0101, op=0 */
    /* C7.3.12  bic32   Vd.T,#imm8,lsl #16:      cmode=0101, op=1 */
    /* C7.3.179 movi32  Vd.T,#imm8,lsl #24:      cmode=0110, op=0 */
    /* C7.3.183 mvni32  Vd.T,#imm8,lsl #24:      cmode=0110, op=1 */
    /* C7.3.187 orr32   Vd.T,#imm8,lsl #24:      cmode=0111, op=0 */
    /* C7.3.12  bic32   Vd.T,#imm8,lsl #24:      cmode=0111, op=1 */
    /*                     2s               Q=0                   */
    /*                     4s               Q=1                   */
    /* C7.3.179 movi16  Vd.T,#imm8,lsl #0 :      cmode=1000, op=0 */
    /* C7.3.183 mvni16  Vd.T,#imm8,lsl #0 :      cmode=1000, op=1 */
    /* C7.3.187 orr16   Vd.T,#imm8,lsl #0 :      cmode=1001  op=0 */
    /* C7.3.12  bic16   Vd.T,#imm8,lsl #0 :      cmode=1001  op=1 */
    /* C7.3.179 movi16  Vd.T,#imm8,lsl #8 :      cmode=1010, op=0 */
    /* C7.3.183 mvni16  Vd.T,#imm8,lsl #8 :      cmode=1010, op=1 */
    /* C7.3.187 orr16   Vd.T,#imm8,lsl #8 :      cmode=1011  op=0 */
    /* C7.3.12  bic16   Vd.T,#imm8,lsl #8 :      cmode=1011  op=1 */
    /*                     4h               Q=0                   */
    /*                     8h               Q=1                   */
    /* C7.3.179 movi32o Vd.T,#imm8,msl #8 :      cmode=1100, op=0 */
    /* C7.3.183 mvni32o Vd.T,#imm8,msl #8 :      cmode=1100, op=1 */
    /* C7.3.179 movi32o Vd.T,#imm8,msl #16:      cmode=1101, op=0 */
    /* C7.3.183 mvni32o Vd.T,#imm8,msl #16:      cmode=1101, op=1 */
    /*                     2s               Q=0                   */
    /*                     4s               Q=1                   */
    /* C7.3.179 movi8   Vd.T,#imm8,lsl #0 :      cmode=1110, op=0 */
    /*                     8b               Q=0                   */
    /*                     16b              Q=1                   */
    /* C7.3.112 fmov64  Dd,   #imm        : Q=0, cmode=1110, op=1 */
    /* C7.3.112 fmov64  Vd.2D,#imm        : Q=1, cmode=1110, op=1 */
    /* C7.3.112 fmov64                    :      cmode=1111, op=0 */
    /* C7.3.112 fmov64                    :      cmode=1111, op=1 */
    Ull imm8 = (i.movi.a<<7)|(i.movi.b<<6)|(i.movi.c<<5)|(i.movi.d<<4)|(i.movi.e<<3)|(i.movi.f<<2)|(i.movi.g<<1)|(i.movi.h);
    rob->type  = 2; /* VXX */
    rob->opcd  = 6; /* MOVI/MVNI/ORR/BIC */
    switch (i.movi.cmode) {
    case 0: case 2: case 4: case 6: case 8: case 10:
    case 12: case 13: case 14: case 15:
      rob->sop   = 0; /* MOVI/MVNI */
      break;
    case 1: case 3: case 5: case 7: case 9: case 11:
      if (i.movi.op==0) /* ORR */
	rob->sop = 1; /* ORR */
      else /* BIC */
	rob->sop = 2; /* BIC */
      break;
    }
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = (i.movi.cmode<=13&&i.movi.op)?1:0; /* invert imm */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.movi.Q; /* 0:single, 1:double */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    v0 = VECREGTOP+i.movi.rd;
    if (rob->opcd==7) {  /* ORR/BIC */
      rob->sr[0].t = !t[tid].map[v0].x ? 2 :
                     (c[cid].rob[t[tid].map[v0].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[0].x = t[tid].map[v0].x; /* reg */
      rob->sr[0].n = !t[tid].map[v0].x ? v0 : t[tid].map[v0].rob;
    }
    rob->sr[1].t = 1; /* immediate */
    rob->sr[1].x = 0; /* renamig N.A. */
    switch (i.movi.cmode) {
    case 0: case 1:
      rob->sr[1].n = (imm8<<32)|(imm8);
      break;
    case 2: case 3:
      rob->sr[1].n = (imm8<<40)|(imm8<<8);
      break;
    case 4: case 5:
      rob->sr[1].n = (imm8<<48)|(imm8<<16);
      break;
    case 6: case 7:
      rob->sr[1].n = (imm8<<56)|(imm8<<24);
      break;
    case 8: case 9:
      rob->sr[1].n = (imm8<<48)|(imm8<<32)|(imm8<<16)|(imm8);
      break;
    case 10: case 11:
      rob->sr[1].n = (imm8<<56)|(imm8<<40)|(imm8<<24)|(imm8<<8);
      break;
    case 12:
      rob->sr[1].n = (imm8<<40)|0x000000ff00000000LL|(imm8<<8)|0x00000000000000ffLL;
      break;
    case 13:
      rob->sr[1].n = (imm8<<48)|0x0000ffff00000000LL|(imm8<<16)|0x000000000000ffffLL;
      break;
    case 14:
      if (i.movi.op==0)
        rob->sr[1].n = (imm8<<56)|(imm8<<48)|(imm8<<40)|(imm8<<32)|(imm8<<24)|(imm8<<16)|(imm8<<8)|(imm8);
      else {
        Ull a=i.movi.a, b=i.movi.b, c=i.movi.c, d=i.movi.d, e=i.movi.e, f=i.movi.f, g=i.movi.g, h=i.movi.h;
        rob->sr[1].n = (a<<63)|(a<<62)|(a<<61)|(a<<60)|(a<<59)|(a<<58)|(a<<57)|(a<<56)
                      |(b<<55)|(b<<54)|(b<<53)|(b<<52)|(b<<51)|(b<<50)|(b<<49)|(b<<48)
                      |(c<<47)|(c<<46)|(c<<45)|(c<<44)|(c<<43)|(c<<42)|(c<<41)|(c<<40)
                      |(d<<39)|(d<<38)|(d<<37)|(d<<36)|(d<<35)|(d<<34)|(d<<33)|(d<<32)
                      |(e<<31)|(e<<30)|(e<<29)|(e<<28)|(e<<27)|(e<<26)|(e<<25)|(e<<24)
                      |(f<<23)|(f<<22)|(f<<21)|(f<<20)|(f<<19)|(f<<18)|(f<<17)|(f<<16)
                      |(g<<15)|(g<<14)|(g<<13)|(g<<12)|(g<<11)|(g<<10)|(g<< 9)|(g<< 8)
                      |(h<< 7)|(h<< 6)|(h<< 5)|(h<< 4)|(h<< 3)|(h<< 2)|(h<< 1)|(h    );
      }
      break;
    case 15:
      if (i.movi.op==0) {
        Ull a=i.movi.a, b=i.movi.b, c=i.movi.c, d=i.movi.d, e=i.movi.e, f=i.movi.f, g=i.movi.g, h=i.movi.h;
        rob->sr[1].n = (a<<63)|((1-b)<<62)|(b<<61)|(b<<60)|(b<<59)|(b<<58)|(b<<57)|(c<<56)|(d<<55)|(e<<54)|(f<<53)|(g<<52)|(h<<51)
                      |(a<<31)|((1-b)<<30)|(b<<29)|(b<<28)|(b<<27)|(b<<26)|(b<<25)|(c<<24)|(d<<23)|(e<<22)|(f<<21)|(g<<20)|(h<<19);
      }
      else {
        Ull a=i.movi.a, b=i.movi.b, c=i.movi.c, d=i.movi.d, e=i.movi.e, f=i.movi.f, g=i.movi.g, h=i.movi.h;
        rob->sr[1].n = (a<<63)|((1-b)<<62)|(b<<61)|(b<<60)|(b<<59)|(b<<58)|(b<<57)|(b<<56)|(b<<55)|(b<<54)|(c<<53)|(d<<52)|(e<<51)|(f<<50)|(g<<49)|(h<<48);
      }
      break;
    }
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.movv.op31==0 && i.movv.op29_24==0x0e && i.movv.op21==1 && i.movv.op15_10==0x07) { /* C7.3.11 AND(vector), C7.3.13 BIC(vector,register), C7.3.177 MOV(vector), C7.3.186 ORN(vector), C7.3.188 ORR(vector,register) */
    rob->type  = 2; /* VXX */
    rob->opcd  = 6; /* MOVI/MVNI/ORR/BIC */
    rob->sop   = i.movv.andor?1:2; /* OR/AND */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = i.movv.inv; /* invert s2 */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.movv.Q; /* 0:single, 1:double */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.movv.rn;
    s2 = VECREGTOP+i.movv.rm;
    v0 = VECREGTOP+i.movv.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    rob->sr[1].t = !t[tid].map[s2].x ? 2 :
                   (c[cid].rob[t[tid].map[s2].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = t[tid].map[s2].x; /* reg */
    rob->sr[1].n = !t[tid].map[s2].x ? s2 : t[tid].map[s2].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.bitwise.op31==0 && i.bitwise.op29_24==0x2e && i.bitwise.op21==1 && i.bitwise.op15_10==0x07) { /* C7.3.14 BIF, C7.3.15 BIT, C7.3.16 BSL, C7.3.33 EOR */
    rob->type  = 2; /* VXX */
    rob->opcd  = 6; /* MOVI/MVNI/ORR/BIC/EOR/BSL/BIT/BIF */
    rob->sop   = 4|i.bitwise.opc2; /* EOR/BSL/BIT/BIF */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.bitwise.Q; /* 0:64, 1:128 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.bitwise.rn;
    s2 = VECREGTOP+i.bitwise.rm;
    v0 = VECREGTOP+i.bitwise.rd;
    rob->sr[0].t = !t[tid].map[v0].x ? 2 :
                   (c[cid].rob[t[tid].map[v0].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[v0].x; /* reg */
    rob->sr[0].n = !t[tid].map[v0].x ? v0 : t[tid].map[v0].rob;
    rob->sr[1].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = t[tid].map[s1].x; /* reg */
    rob->sr[1].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    rob->sr[2].t = !t[tid].map[s2].x ? 2 :
                   (c[cid].rob[t[tid].map[s2].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[2].x = t[tid].map[s2].x; /* reg */
    rob->sr[2].n = !t[tid].map[s2].x ? s2 : t[tid].map[s2].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.shr_sca.op31_30==1 && i.shr_sca.op28_23==0x3e && i.shr_sca.immh && i.shr_sca.op15_10==0x01) { /* C7.3.271 SSHR(scalar), C7.3.340 USHR(scalar) */
    rob->type  = 2; /* VXX */
    rob->opcd  = 4; /* SHIFT */
    rob->sop   = 0|i.shr_sca.U; /* 0:signed(SSHR), 1:signed(USHR) */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = 0; /* not used */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.shr_sca.rn;
    v0 = VECREGTOP+i.shr_sca.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    rob->sr[1].t = 1; /* immediate */
    rob->sr[1].x = 0; /* renamig N.A. */
    rob->sr[1].n = (i.shr_sca.immh<<3)|i.shr_sca.immb;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.shr_sca.op31_30==1 && i.shr_sca.U==0 && i.shr_sca.op28_23==0x3e && i.shr_sca.immh && i.shr_sca.op15_10==0x15) { /* C7.3.222 SHL(scalar) */
    rob->type  = 2; /* VXX */
    rob->opcd  = 4; /* SHIFT */
    rob->sop   = 4; /* SHL(scalar) */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = 0; /* not used */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.shr_sca.rn;
    v0 = VECREGTOP+i.shr_sca.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    rob->sr[1].t = 1; /* immediate */
    rob->sr[1].x = 0; /* renamig N.A. */
    rob->sr[1].n = (i.shr_sca.immh<<3)|i.shr_sca.immb;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.sft_vec.op31==0 && i.sft_vec.op28_23==0x1e && i.sft_vec.immh && i.sft_vec.op15_10==0x01) { /* C7.3.271 SSHR(vector), C7.3.340 USHR(vector) */
    rob->type  = 2; /* VXX */
    rob->opcd  = 4; /* SHIFT */
    rob->sop   = 2|i.sft_vec.U; /* 2:signed(SSHR), 3:signed(USHR) */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.sft_vec.Q; /* 0:single, 1:double */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.sft_vec.rn;
    v0 = VECREGTOP+i.sft_vec.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    rob->sr[1].t = 1; /* immediate */
    rob->sr[1].x = 0; /* renamig N.A. */
    rob->sr[1].n = (i.sft_vec.immh<<3)|i.sft_vec.immb;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.sft_vec.op31==0 && i.shr_sca.U==0 && i.sft_vec.op28_23==0x1e && i.sft_vec.immh && i.sft_vec.op15_10==0x15) { /* C7.3.222 SHL(vector) */
    rob->type  = 2; /* VXX */
    rob->opcd  = 4; /* SHIFT */
    rob->sop   = 5; /* SHL(vector) */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.sft_vec.Q; /* 0:single, 1:double */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.sft_vec.rn;
    v0 = VECREGTOP+i.sft_vec.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    rob->sr[1].t = 1; /* immediate */
    rob->sr[1].x = 0; /* renamig N.A. */
    rob->sr[1].n = (i.sft_vec.immh<<3)|i.sft_vec.immb;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.sft_vec.op31==0 && i.sft_vec.op28_23==0x1e && i.shr_sca.immh && i.sft_vec.op15_10==0x29) { /* C7.3.270 SSHLL/SSHLL2, C7.3.339 USHLL/USHLL2 */
    rob->type  = 2; /* VXX */
    rob->opcd  = 4; /* SHIFT */
    rob->sop   = 6|i.sft_vec.U; /* 6:SSHLL/SSHLL2, 7:USHLL/USHLL2 */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.sft_vec.Q; /* 0:lower64, 1:upper64 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.sft_vec.rn;
    v0 = VECREGTOP+i.sft_vec.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    rob->sr[1].t = 1; /* immediate */
    rob->sr[1].x = 0; /* renamig N.A. */
    rob->sr[1].n = (i.sft_vec.immh<<3)|i.sft_vec.immb;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.dup.op31==0 && i.dup.op29_21==0x070 && i.dup.op15_10==0x01) { /* C7.3.31 DUP(vector) */
    Uint size  = (i.dup.imm5&1)?1:(i.dup.imm5&2)?2:(i.dup.imm5&4)?4:8; /* bytes */
    Uint index = (i.dup.imm5&1)?(i.dup.imm5&~1)>>1:(i.dup.imm5&2)?(i.dup.imm5&~3)>>1:(i.dup.imm5&4)?(i.dup.imm5&~7)>>1:(i.dup.imm5&~15)>>1; /* byte-portion: 15,14..0, 14,12..0, 12,8..0, 8,0 */
    rob->type  = 2; /* VXX */
    rob->opcd  = 7; /* DUP */
    rob->sop   = 0; /* DUP */
    rob->ptw   = 0; /* not used */
    rob->size  = size; /* 1-8 */
    rob->idx   = index; /* 15-0 */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.dup.Q; /* 0:dup64, 1:dup128 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.dup.rn;
    v0 = VECREGTOP+i.dup.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.dup.op31==0 && i.dup.op29_21==0x070 && i.dup.op15_10==0x03) { /* C7.3.32 DUP(general) */
    Uint size  = (i.dup.imm5&1)?1:(i.dup.imm5&2)?2:(i.dup.imm5&4)?4:8; /* bytes */
    rob->type  = 2; /* VXX */
    rob->opcd  = 7; /* DUP */
    rob->sop   = 0; /* DUP */
    rob->ptw   = 0; /* not used */
    rob->size  = size; /* 1-8 */
    rob->idx   = 0; /* 0 */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.dup.Q; /* 0:dup64, 1:dup128 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    v0 = VECREGTOP+i.dup.rd;
    rob->sr[0].t = !t[tid].map[i.dup.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.dup.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[i.dup.rn].x; /* reg */
    rob->sr[0].n = !t[tid].map[i.dup.rn].x ? i.dup.rn : t[tid].map[i.dup.rn].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.umov.op31==0 && i.umov.Q==1 && i.umov.op29_21==0x070 && i.umov.op15_10==0x07) { /* C7.3.176 MOV(from general), C7.3.151 INS(general) */
    Uint size  = (i.umov.imm5&1)?1:(i.umov.imm5&2)?2:(i.umov.imm5&4)?4:8; /* bytes */
    Uint index = (i.umov.imm5&1)?(i.umov.imm5&~1)>>1:(i.umov.imm5&2)?(i.umov.imm5&~3)>>1:(i.umov.imm5&4)?(i.umov.imm5&~7)>>1:(i.umov.imm5&~15)>>1; /* byte-portion: 15,14..0, 14,12..0, 12,8..0, 8,0 */
    rob->type  = 2; /* VXX */
    rob->opcd  = 7; /* DUP */
    rob->sop   = 3; /* INS */
    rob->ptw   = 0; /* not used */
    rob->size  = size; /* 1-8 */
    rob->idx   = index; /* 15-0 */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = 0; /* not used */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    v0 = VECREGTOP+i.umov.rd;
    rob->sr[0].t = !t[tid].map[v0].x ? 2 :
                   (c[cid].rob[t[tid].map[v0].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[v0].x; /* reg */
    rob->sr[0].n = !t[tid].map[v0].x ? v0 : t[tid].map[v0].rob;
    rob->sr[1].t = !t[tid].map[i.umov.rn].x ? 2 :
                   (c[cid].rob[t[tid].map[i.umov.rn].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = t[tid].map[i.umov.rn].x; /* reg */
    rob->sr[1].n = !t[tid].map[i.umov.rn].x ? i.umov.rn : t[tid].map[i.umov.rn].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.umov.op31==0 && i.umov.op29_21==0x070 && i.umov.op15_10==0x0f) { /* C7.3.178 MOV(to general), C7.3.321 UMOV */
    Uint size  = (i.umov.imm5&1)?1:(i.umov.imm5&2)?2:(i.umov.imm5&4)?4:8; /* bytes */
    Uint index = (i.umov.imm5&1)?(i.umov.imm5&~1)>>1:(i.umov.imm5&2)?(i.umov.imm5&~3)>>1:(i.umov.imm5&4)?(i.umov.imm5&~7)>>1:(i.umov.imm5&~15)>>1; /* byte-portion: 15,14..0, 14,12..0, 12,8..0, 8,0 */
    rob->type  = 2; /* VXX */
    rob->opcd  = 7; /* DUP */
    rob->sop   = 2; /* UMOV */
    rob->ptw   = 0; /* not used */
    rob->size  = size; /* 1-8 */
    rob->idx   = index; /* 15-0 */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.umov.Q; /* 0:umov64, 1:umov128 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.umov.rn;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = i.umov.rd;
    t[tid].map[i.umov.rd].x = 1;
    t[tid].map[i.umov.rd].rob = robid;
    return (0);
  }
  else if (i.xtn.op31==0 && i.xtn.op29_24==0x0e && i.xtn.op21_10==0x84a) { /* C7.3.348 XTN/XTN2 */
    rob->type  = 2; /* VXX */
    rob->opcd  = 7; /* DUP */
    rob->sop   = 1; /* XTN/XTN2 */
    rob->ptw   = 0; /* not used */
    rob->size  = i.xtn.size;;
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.xtn.Q; /* 0:lower64, 1:upper64 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.xtn.rn;
    v0 = VECREGTOP+i.xtn.rd;
    rob->sr[0].t = !t[tid].map[v0].x ? 2 :
                   (c[cid].rob[t[tid].map[v0].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[v0].x; /* reg */
    rob->sr[0].n = !t[tid].map[v0].x ? v0 : t[tid].map[v0].rob;
    rob->sr[1].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = t[tid].map[s1].x; /* reg */
    rob->sr[1].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.maxmin.op31==0 && i.maxmin.op28_24==0x0e && i.maxmin.op21==1 && i.maxmin.op15_12==6 && i.maxmin.op10==1) { /* C7.3.227 SMAX, C7.3.230 SMIN, C7.3.311 UMAX, C7.3.314 UMIN */
    rob->type  = 2; /* VXX */
    rob->opcd  = 7; /* DUP */
    rob->sop   = 4|(i.maxmin.op10<<1)|i.maxmin.U; /* 4:SMAX, 5:UMAX, 6:SMIN, 7:UMIN */
    rob->ptw   = 0; /* not used */
    rob->size  = i.maxmin.size;;
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.maxmin.Q; /* 0:64, 1:128 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.maxmin.rn;
    s2 = VECREGTOP+i.maxmin.rm;
    v0 = VECREGTOP+i.maxmin.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    rob->sr[1].t = !t[tid].map[s2].x ? 2 :
                   (c[cid].rob[t[tid].map[s2].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = t[tid].map[s2].x; /* reg */
    rob->sr[1].n = !t[tid].map[s2].x ? s2 : t[tid].map[s2].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.cmpr_sca.op31_30==1 && i.cmpr_sca.op28_24==0x1e && i.cmpr_sca.op21==1 && (i.cmpr_sca.op15_12==3||(i.cmpr_sca.op15_12==8&&i.cmpr_sca.eq)) && i.cmpr_sca.op10==1) { /* C7.3.19 CMEQ(scalar), C7.3.21 CMGE(scalar), C7.3.23 CMGT(scalar), C7.3.25 CMHI(scalar), C7.3.26 CMHS(scalar), C7.3.29 CMTST(scalar) */
    rob->type  = 2; /* VXX */
    rob->opcd  = 5; /* CMP */
    rob->sop   = (i.cmpr_sca.op15_12==3 && i.cmpr_sca.U==0 && i.cmpr_sca.eq==0) ? 0 :     /* CMGT */
                 (i.cmpr_sca.op15_12==3 && i.cmpr_sca.U==1 && i.cmpr_sca.eq==0) ? 2 :     /* CMHI */
                 (i.cmpr_sca.op15_12==3 && i.cmpr_sca.U==0 && i.cmpr_sca.eq==1) ? 4 :     /* CMGE */
                 (i.cmpr_sca.op15_12==3 && i.cmpr_sca.U==1 && i.cmpr_sca.eq==1) ? 6 :     /* CMHS */
                 (i.cmpr_sca.op15_12==8 && i.cmpr_sca.U==0 && i.cmpr_sca.eq==1) ? 8 :     /* CMTST */
                 (i.cmpr_sca.op15_12==8 && i.cmpr_sca.U==1 && i.cmpr_sca.eq==1) ?10 : 12; /* CMEQ */
    rob->ptw   = 0; /* not used */
    rob->size  = i.cmpr_sca.size;
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* 0:register, 1:zero */
    rob->oinv  = 0; /* not used */
    rob->dbl   = 0; /* not used */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.cmpr_sca.rn;
    s2 = VECREGTOP+i.cmpr_sca.rm;
    v0 = VECREGTOP+i.cmpr_sca.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    rob->sr[1].t = !t[tid].map[s2].x ? 2 :
                   (c[cid].rob[t[tid].map[s2].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = t[tid].map[s2].x; /* reg */
    rob->sr[1].n = !t[tid].map[s2].x ? s2 : t[tid].map[s2].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.cmpr_vec.op31==0 && i.cmpr_vec.op28_24==0x0e && i.cmpr_vec.op21==1 && (i.cmpr_vec.op15_12==3||(i.cmpr_vec.op15_12==8&&i.cmpr_vec.eq)) && i.cmpr_vec.op10==1) { /* C7.3.19 CMEQ(vector), C7.3.21 CMGE(vector), C7.3.23 CMGT(vector), C7.3.25 CMHI(vector), C7.3.26 CMHS(vector), C7.3.29 CMTST(vector) */
    rob->type  = 2; /* VXX */
    rob->opcd  = 5; /* CMP */
    rob->sop   = (i.cmpr_vec.op15_12==3 && i.cmpr_vec.U==0 && i.cmpr_vec.eq==0) ? 1 :     /* CMGT */
                 (i.cmpr_vec.op15_12==3 && i.cmpr_vec.U==1 && i.cmpr_vec.eq==0) ? 3 :     /* CMHI */
                 (i.cmpr_vec.op15_12==3 && i.cmpr_vec.U==0 && i.cmpr_vec.eq==1) ? 5 :     /* CMGE */
                 (i.cmpr_vec.op15_12==3 && i.cmpr_vec.U==1 && i.cmpr_vec.eq==1) ? 7 :     /* CMHS */
                 (i.cmpr_vec.op15_12==8 && i.cmpr_vec.U==0 && i.cmpr_vec.eq==1) ? 9 :     /* CMTST */
                 (i.cmpr_vec.op15_12==8 && i.cmpr_vec.U==1 && i.cmpr_vec.eq==1) ?11 : 13; /* CMEQ */
    rob->ptw   = 0; /* not used */
    rob->size  = i.cmpr_vec.size;
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* 0:register, 1:zero */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.cmpr_vec.Q; /* 0:64, 1:128 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.cmpr_vec.rn;
    s2 = VECREGTOP+i.cmpr_vec.rm;
    v0 = VECREGTOP+i.cmpr_vec.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    rob->sr[1].t = !t[tid].map[s2].x ? 2 :
                   (c[cid].rob[t[tid].map[s2].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = t[tid].map[s2].x; /* reg */
    rob->sr[1].n = !t[tid].map[s2].x ? s2 : t[tid].map[s2].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.cmpz_sca.op31_30==1 && i.cmpz_sca.op28_24==0x1e && (i.cmpz_sca.op21_13==0x104||(i.cmpz_sca.op21_13==0x105&&i.cmpz_sca.op==0)) && i.cmpz_sca.op11_10==2) { /* C7.3.20 CMEQZ(scalar), C7.3.22 CMGEZ(scalar), C7.3.24 CMGTZ(scalar), C7.3.27 CMLEZ(scalar), C7.3.28 CMLTZ(scalar) */
    rob->type  = 2; /* VXX */
    rob->opcd  = 5; /* CMP */
    rob->sop   = (i.cmpz_sca.op21_13==0x104 && i.cmpz_sca.U==0 && i.cmpz_sca.op==0) ? 0 :     /* CMGTZ */
                 (i.cmpz_sca.op21_13==0x104 && i.cmpz_sca.U==1 && i.cmpz_sca.op==0) ? 2 :     /* CMGEZ */
                 (i.cmpz_sca.op21_13==0x104 && i.cmpz_sca.U==0 && i.cmpz_sca.op==1) ? 4 :     /* CMEQZ */
                 (i.cmpz_sca.op21_13==0x104 && i.cmpz_sca.U==1 && i.cmpz_sca.op==1) ? 6 :     /* CMLEZ */
                 (i.cmpz_sca.op21_13==0x105 && i.cmpz_sca.U==0 && i.cmpz_sca.op==0) ? 8 : 10; /* CMLTZ */
    rob->ptw   = 0; /* not used */
    rob->size  = i.cmpz_sca.size;
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 1; /* 0:register, 1:zero */
    rob->oinv  = 0; /* not used */
    rob->dbl   = 0; /* not used */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.cmpz_sca.rn;
    v0 = VECREGTOP+i.cmpz_sca.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.cmpz_vec.op31==0 && i.cmpz_vec.op28_24==0x0e && (i.cmpz_vec.op21_13==0x104||(i.cmpz_vec.op21_13==0x105&&i.cmpz_vec.op==0)) && i.cmpz_vec.op11_10==2) { /* C7.3.20 CMEQZ(vector), C7.3.22 CMGEZ(vector), C7.3.24 CMGTZ(vector), C7.3.27 CMLEZ(vector), C7.3.28 CMLTZ(vector) */
    rob->type  = 2; /* VXX */
    rob->opcd  = 5; /* CMP */
    rob->sop   = (i.cmpz_vec.op21_13==0x104 && i.cmpz_vec.U==0 && i.cmpz_vec.op==0) ? 1 :     /* CMGTZ */
                 (i.cmpz_vec.op21_13==0x104 && i.cmpz_vec.U==1 && i.cmpz_vec.op==0) ? 3 :     /* CMGEZ */
                 (i.cmpz_vec.op21_13==0x104 && i.cmpz_vec.U==0 && i.cmpz_vec.op==1) ? 5 :     /* CMEQZ */
                 (i.cmpz_vec.op21_13==0x104 && i.cmpz_vec.U==1 && i.cmpz_vec.op==1) ? 7 :     /* CMLEZ */
                 (i.cmpz_vec.op21_13==0x105 && i.cmpz_vec.U==0 && i.cmpz_vec.op==0) ? 9 : 11; /* CMLTZ */
    rob->ptw   = 0; /* not used */
    rob->size  = i.cmpz_vec.size;
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 1; /* 0:register, 1:zero */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.cmpz_vec.Q; /* 0:64, 1:128 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.cmpz_vec.rn;
    v0 = VECREGTOP+i.cmpz_vec.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.fadd.op31==0 && i.fadd.op28_24==0x0e && i.fadd.op21==1 && i.fadd.op15_10==0x35) { /* C7.3.40 FADD(vector), C7.3.148 FSUB(vector) */
    rob->type  = 2; /* VXX */
    rob->opcd  = 8; /* FADD/FSUB */
    rob->sop   = (i.fadd.U<<1)|i.fadd.sz; /* 0:2S/4S, 1:2D, 2:pair2S/4S, 3:pair-2D */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = i.fadd.op==0?0:1; /* 0:FADD, 1:FSUB */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.fadd.Q; /* 0:fadd64, 1:fadd128 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.fadd.rn;
    s2 = VECREGTOP+i.fadd.rm;
    v0 = VECREGTOP+i.fadd.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    rob->sr[1].t = !t[tid].map[s2].x ? 2 :
                   (c[cid].rob[t[tid].map[s2].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = t[tid].map[s2].x; /* reg */
    rob->sr[1].n = !t[tid].map[s2].x ? s2 : t[tid].map[s2].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.fmla.op31==0 && i.fmla.op29_24==0x0e && i.fmla.op21==1 && i.fmla.op15_10==0x33) { /* C7.3.109 FMLA(vector), C7.3.111 FMLS(vector) */
    rob->type  = 2; /* VXX */
    rob->opcd  = 9; /* FMLA/FMLS */
    rob->sop   = i.fmla.sz; /* 0:2S/4S, 1:2D */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = i.fmla.op; /* 0:FMLA, 1:FMLS */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.fmla.Q; /* 0:fmla64, 1:fmla128 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.fmla.rn;
    s2 = VECREGTOP+i.fmla.rm;
    s3 = VECREGTOP+i.fmla.rd;
    v0 = VECREGTOP+i.fmla.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    rob->sr[1].t = !t[tid].map[s2].x ? 2 :
                   (c[cid].rob[t[tid].map[s2].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = t[tid].map[s2].x; /* reg */
    rob->sr[1].n = !t[tid].map[s2].x ? s2 : t[tid].map[s2].rob;
    rob->sr[2].t = !t[tid].map[s3].x ? 2 :
                   (c[cid].rob[t[tid].map[s3].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[2].x = t[tid].map[s3].x; /* reg */
    rob->sr[2].n = !t[tid].map[s3].x ? s3 : t[tid].map[s3].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.fmla.op31==0 && i.fmla.op29_24==0x2e && i.fmla.op==0 && i.fmla.op21==1 && i.fmla.op15_10==0x37) { /* C7.3.118 FMUL(vector) */
    rob->type  = 2; /* VXX */
    rob->opcd  =10; /* FMUL */
    rob->sop   = i.fmla.sz; /* 0:2S/4S, 1:2D */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.fmla.Q; /* 0:fmla64, 1:fmla128 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.fmla.rn;
    s2 = VECREGTOP+i.fmla.rm;
    v0 = VECREGTOP+i.fmla.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    rob->sr[1].t = !t[tid].map[s2].x ? 2 :
                   (c[cid].rob[t[tid].map[s2].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = t[tid].map[s2].x; /* reg */
    rob->sr[1].n = !t[tid].map[s2].x ? s2 : t[tid].map[s2].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.mla.op31==0 && i.mla.op28_24==0x0e && i.mla.op21==1 && i.mla.op15_10==0x25) { /* C7.3.171 MLA(vector), C7.3.173 MLS(vector) */
    rob->type  = 2; /* VXX */
    rob->opcd  =11; /* MLA/MLS */
    rob->sop   = i.mla.size; /* 0:8B/16B, 1:4H/8H, 2:2S/4S */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = i.mla.U; /* 0:MLA, 1:MLS */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.mla.Q; /* 0:mla64, 1:mla128 */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.mla.rn;
    s2 = VECREGTOP+i.mla.rm;
    s3 = VECREGTOP+i.mla.rd;
    v0 = VECREGTOP+i.mla.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    rob->sr[1].t = !t[tid].map[s2].x ? 2 :
                   (c[cid].rob[t[tid].map[s2].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = t[tid].map[s2].x; /* reg */
    rob->sr[1].n = !t[tid].map[s2].x ? s2 : t[tid].map[s2].rob;
    rob->sr[2].t = !t[tid].map[s3].x ? 2 :
                   (c[cid].rob[t[tid].map[s3].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[2].x = t[tid].map[s3].x; /* reg */
    rob->sr[2].n = !t[tid].map[s3].x ? s3 : t[tid].map[s3].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.fadd_sca.op31_24==0x1e && (i.fadd_sca.type&2)==0 && i.fadd_sca.op21==1 && i.fadd_sca.op15_13==1 && i.fadd_sca.op11_10==2) { /* C7.3.41 FADD(scalar), C7.3.149 FSUB(scalar) */
    rob->type  = 2; /* VXX */
    rob->opcd  =12; /* FADD(scalar) */
    rob->sop   = 0; /* not used */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = i.fadd_sca.op;
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.fadd_sca.type&1; /* 0:Sd, 1:Dd */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.fadd_sca.rn;
    s2 = VECREGTOP+i.fadd_sca.rm;
    v0 = VECREGTOP+i.fadd_sca.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    rob->sr[1].t = !t[tid].map[s2].x ? 2 :
                   (c[cid].rob[t[tid].map[s2].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = t[tid].map[s2].x; /* reg */
    rob->sr[1].n = !t[tid].map[s2].x ? s2 : t[tid].map[s2].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.fmadd.op31_24==0x1f && (i.fmadd.type&2)==0) { /* C7.3.87 FMADD, C7.3.116 FMSUB, C7.3.124 FNMADD, C7.3.125 FNMSUB */
    rob->type  = 2; /* VXX */
    rob->opcd  =13; /* FMADD/FMSUB */
    rob->sop   = 0; /* not used */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = i.fmadd.o1;             /* negate s3, o1o0=00->FMADD */
    rob->oinv  = i.fmadd.o0!=i.fmadd.o1; /* negate s1, o1o0=01->FMSUB */
    rob->dbl   = i.fmadd.type&1; /* 0:Sd, 1:Dd */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.fmadd.rn;
    s2 = VECREGTOP+i.fmadd.rm;
    s3 = VECREGTOP+i.fmadd.ra;
    v0 = VECREGTOP+i.fmadd.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    rob->sr[1].t = !t[tid].map[s2].x ? 2 :
                   (c[cid].rob[t[tid].map[s2].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = t[tid].map[s2].x; /* reg */
    rob->sr[1].n = !t[tid].map[s2].x ? s2 : t[tid].map[s2].rob;
    rob->sr[2].t = !t[tid].map[s3].x ? 2 :
                   (c[cid].rob[t[tid].map[s3].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[2].x = t[tid].map[s3].x; /* reg */
    rob->sr[2].n = !t[tid].map[s3].x ? s3 : t[tid].map[s3].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.fmul_sca.op31_24==0x1e && (i.fmul_sca.type&2)==0 && i.fmul_sca.op21==1 && i.fmul_sca.op14_10==0x02) { /* C7.3.119 FMUL(scalar), C7.3.126 FNMUL(scalar) */
    rob->type  = 2; /* VXX */
    rob->opcd  =14; /* FMUL */
    rob->sop   = 0; /* not used */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = i.fmul_sca.op; /* 0:normal 1:negate */
    rob->dbl   = i.fmul_sca.type&1; /* 0:Sd, 1:Dd */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.fmul_sca.rn;
    s2 = VECREGTOP+i.fmul_sca.rm;
    v0 = VECREGTOP+i.fmul_sca.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    rob->sr[1].t = !t[tid].map[s2].x ? 2 :
                   (c[cid].rob[t[tid].map[s2].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = t[tid].map[s2].x; /* reg */
    rob->sr[1].n = !t[tid].map[s2].x ? s2 : t[tid].map[s2].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.fdiv_sca.op31_24==0x1e && (i.fdiv_sca.type&2)==0 && i.fdiv_sca.op21==1 && i.fdiv_sca.op15_10==0x06) { /* C7.3.86 FDIV(scalar) */
    rob->type  = 2; /* VXX */
    rob->opcd  =15; /* FDIV */
    rob->sop   = 0; /* not used */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.fdiv_sca.type&1; /* 0:Sd, 1:Dd */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.fdiv_sca.rn;
    s2 = VECREGTOP+i.fdiv_sca.rm;
    v0 = VECREGTOP+i.fdiv_sca.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    rob->sr[1].t = !t[tid].map[s2].x ? 2 :
                   (c[cid].rob[t[tid].map[s2].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[1].x = t[tid].map[s2].x; /* reg */
    rob->sr[1].n = !t[tid].map[s2].x ? s2 : t[tid].map[s2].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.fcmp.op31_24==0x1e && i.fcmp.op21==1 && i.fcmp.op15_10==0x08 && i.fcmp.op2_0==0) { /* C7.3.54 FCMP, C7.3.55 FCMPE */
    rob->type  = 2; /* VXX */
    rob->opcd  =12; /* FSUB */
    rob->sop   = 0; /* not used */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 1; /* FSUB */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.fcmp.type&1; /* 0:Sd, 1:Dd */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.fcmp.rn;
    s2 = VECREGTOP+i.fcmp.rm;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    if ((i.fcmp.opc&1)==0) { /* reg */
      rob->sr[1].t = !t[tid].map[s2].x ? 2 :
                     (c[cid].rob[t[tid].map[s2].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
      rob->sr[1].x = t[tid].map[s2].x; /* reg */
      rob->sr[1].n = !t[tid].map[s2].x ? s2 : t[tid].map[s2].rob;
    }
    else if (i.fcmp.rm==0 && (i.fcmp.opc&1)) { /* immediate */
      rob->sr[1].t = 1; /* immediate */
      rob->sr[1].x = 0; /* renamig N.A. */
      rob->sr[1].n = 0; /* ZERO */
    }
    else {
      i_xxx(rob);
      return (0);
    }
    /* dest */
    rob->dr[3].t = 1; /* nzcv */
    rob->dr[3].n = CPSREGTOP;
    t[tid].map[CPSREGTOP].x = 3;
    t[tid].map[CPSREGTOP].rob = robid;
    return (0);
  }
  else if (i.fmov_reg.op31_24==0x1e && i.fmov_reg.op21_17==0x10 && i.fmov_reg.op14_10==0x10) { /* C7.3.113 FMOV(register) */
    rob->type  = 2; /* VXX */
    rob->opcd  = 0; /* FMOVL */
    rob->sop   = 4|i.fmov_reg.opc; /* 4:mov, 5:abs, 6:neg, 7:sqrt, 8:undef */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.fmov_reg.type&1; /* 0:Sd, 1:Dd */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.fmov_reg.rn;
    v0 = VECREGTOP+i.fmov_reg.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.fcvt_sca.op31_24==0x1e && i.fcvt_sca.op21_17==0x11 && i.fcvt_sca.op14_10==0x10) { /* C7.3.57 FCVT(scalar) */
    rob->type  = 2; /* VXX */
    rob->opcd  = 1; /* FCVT */
    if      (i.fcvt_sca.type==3 && i.fcvt_sca.opc==0) rob->sop = 0; /* single<-half */
    else if (i.fcvt_sca.type==3 && i.fcvt_sca.opc==1) rob->sop = 1; /* double<-half */
    else if (i.fcvt_sca.type==0 && i.fcvt_sca.opc==3) rob->sop = 2; /* half  <-single */
    else if (i.fcvt_sca.type==0 && i.fcvt_sca.opc==1) rob->sop = 3; /* double<-single */
    else if (i.fcvt_sca.type==1 && i.fcvt_sca.opc==3) rob->sop = 4; /* half  <-double */
    else if (i.fcvt_sca.type==1 && i.fcvt_sca.opc==0) rob->sop = 5; /* single<-double */
    else                                              rob->sop = 8; /* undef */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = 0; /* not used */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.fcvt_sca.rn;
    v0 = VECREGTOP+i.fcvt_sca.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.fcvt_sca.op31_24==0x1e && i.fcvt_sca.op21_17==0x12 && i.fcvt_sca.op14_10==0x10) { /* C7.3.135 FRINTM(scalar) */
    rob->type  = 2; /* VXX */
    rob->opcd  = 1; /* FCVT */
    if      (i.fcvt_sca.type==0 && i.fcvt_sca.opc==2) rob->sop = 6; /* floor32 */
    else if (i.fcvt_sca.type==1 && i.fcvt_sca.opc==2) rob->sop = 7; /* floor64 */
    else                                              rob->sop = 8; /* undef */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = 0; /* not used */
    rob->oinv  = 0; /* not used */
    rob->dbl   = 0; /* not used */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    s1 = VECREGTOP+i.fcvt_sca.rn;
    v0 = VECREGTOP+i.fcvt_sca.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.add_sca.op31_30==1 && i.add_sca.op28_24==0x1e && i.add_sca.op21==1 && i.add_sca.op15_10==0x21) { /* C7.3.2 ADD(scalar), C7.3.288 SUB(scalar) */
    rob->type  = 2; /* VXX */
    rob->opcd  =11; /* MLA/MLS ... use ADD/SUB only */
    rob->sop   = 3; /* 0:8B/16B, 1:4H/8H, 2:2S/4S, 3:1D(scalar) dbl=0 */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = i.add_sca.U;
    rob->oinv  = 0; /* not used */
    rob->dbl   = 0; /* 0:64bit, 1:128bit */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    if (i.add_sca.size != 3) {
      i_xxx(rob);
      return (0);
    }
    s1 = VECREGTOP+i.add_sca.rn;
    s2 = VECREGTOP+i.add_sca.rm;
    v0 = VECREGTOP+i.add_sca.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    rob->sr[2].t = !t[tid].map[s2].x ? 2 :
                   (c[cid].rob[t[tid].map[s2].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[2].x = t[tid].map[s2].x; /* reg */
    rob->sr[2].n = !t[tid].map[s2].x ? s2 : t[tid].map[s2].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else if (i.add_vec.op31==0 && i.add_vec.op28_24==0x0e && i.add_vec.op21==1 && i.add_vec.op15_10==0x21) { /* C7.3.2 ADD(vector), C7.3.288 SUB(vector) */
    rob->type  = 2; /* VXX */
    rob->opcd  =11; /* MLA/MLS ... use ADD/SUB only */
    rob->sop   = i.add_vec.size; /* 0:8B/16B, 1:4H/8H, 2:2S/4S, 3:2D(vector) dbl=1 */
    rob->ptw   = 0; /* not used */
    rob->size  = 0; /* not used */
    rob->idx   = 0; /* not used */
    rob->rep   = 0; /* not used */
    rob->dir   = 0; /* not used */
    rob->iinv  = i.add_vec.U;
    rob->oinv  = 0; /* not used */
    rob->dbl   = i.add_vec.Q; /* 0:64bit, 1:128bit */
    rob->plus  = 0; /* not used */
    rob->pre   = 0; /* not used */
    rob->wb    = 0; /* not used */
    rob->updt  = 0; /* not used */
    if (i.add_vec.size == 3 && i.add_vec.Q == 0) {
      i_xxx(rob);
      return (0);
    }
    s1 = VECREGTOP+i.add_vec.rn;
    s2 = VECREGTOP+i.add_vec.rm;
    v0 = VECREGTOP+i.add_vec.rd;
    rob->sr[0].t = !t[tid].map[s1].x ? 2 :
                   (c[cid].rob[t[tid].map[s1].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[0].x = t[tid].map[s1].x; /* reg */
    rob->sr[0].n = !t[tid].map[s1].x ? s1 : t[tid].map[s1].rob;
    rob->sr[2].t = !t[tid].map[s2].x ? 2 :
                   (c[cid].rob[t[tid].map[s2].rob].stat>=ROB_COMPLETE)? 2 : 3; /* reg */
    rob->sr[2].x = t[tid].map[s2].x; /* reg */
    rob->sr[2].n = !t[tid].map[s2].x ? s2 : t[tid].map[s2].rob;
    /* dest */
    rob->dr[1].t = 1;
    rob->dr[1].n = v0;
    t[tid].map[v0].x = 1;
    t[tid].map[v0].rob = robid;
    return (0);
  }
  else {
    i_xxx(rob);
    return (0);
  }
  return (0); /* never reached */
}

i_xxx(rob) struct rob *rob;
{
  rob->stat = ROB_DECERR; /* decode error */
  return (0);
}
