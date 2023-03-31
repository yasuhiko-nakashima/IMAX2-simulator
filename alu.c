
static char RcsHeader[] = "$Header: /usr/home/nakashim/proj-arm64/src/csim/RCS/alu.c,v 1.25 2017/04/21 03:28:45 nakashim Exp nakashim $";

/* ARM Instruction Simulator           */
/*        Copyright (C) 2007 by NAIST. */
/*         Primary writer: Y.Nakashima */
/*                nakashim@is.naist.jp */

/* alu.c 2005/3/22 */ 

#include "csim.h"

insn_exec_alu(tid, rob) Uint tid; struct rob *rob;
{
  Ull  s1, s2, s3, S, sc, t0, m0; /* source, through, middle(sft,shifter-carry), destination(alu) */
  Ull  wmask, tmask, top, bot;
  Ull  D0;
  Uint i, exec;
  Uchar ccn=0, ccz=0, ccc=0, ccv=0;

  if (rob->sr[0].t) ex_srr(tid, NULL, &s1, rob, 0);
  if (rob->sr[1].t) ex_srr(tid, NULL, &s2, rob, 1);
  if (rob->sr[2].t) ex_srr(tid, NULL, &s3, rob, 2);
  if (rob->sr[3].t) ex_srr(tid, NULL, &wmask, rob, 3); /* BFM/SBFM/UBFM/MOV/CSINV/ROR */
  if (rob->sr[4].t) ex_srr(tid, NULL, &tmask, rob, 4); /* BFM/SBFM/UBFM/   /CSINV     */
  if (rob->sr[5].t) ex_srr(tid, NULL, &sc, rob, 5);

  switch (rob->opcd) {
  case 8: /* SBFM extend=1 */
  case 9: /* BFM/UBFM extend=0 */
    S  = s3 >> 8;   /* S */
    s3 = s3 & 0xff; /* R */
    break;
  }

  switch (rob->opcd) {
  case 10: /* CSINV */
  case 11: /* CCMN/CCMP */
    switch (wmask) {
    case 0: /* EQ (Z set) */
      if (sc & CC_Z) exec = 1;
      else           exec = 0;
      break;
    case 1:  /* NE (Z clear) */
      if (sc & CC_Z) exec = 0;
      else           exec = 1;
      break;
    case 2:  /* CS (C set) */
      if (sc & CC_C) exec = 1;
      else           exec = 0;
      break;
    case 3:  /* CC (C clear) */
      if (sc & CC_C) exec = 0;
      else           exec = 1;
      break;
    case 4:  /* MI (N set) */
      if (sc & CC_N) exec = 1;
      else           exec = 0;
      break;
    case 5:  /* PL (N clear) */
      if (sc & CC_N) exec = 0;
      else           exec = 1;
      break;
    case 6:  /* VS (V set) */
      if (sc & CC_V) exec = 1;
      else           exec = 0;
      break;
    case 7:  /* VC (V clear) */
      if (sc & CC_V) exec = 0;
      else           exec = 1;
      break;
    case 8:  /* HI (C=1&Z=0) */
      if ((sc & CC_C) && !(sc & CC_Z)) exec = 1;
      else                             exec = 0;
      break;
    case 9:  /* LS (C=0|Z=1) */
      if ((sc & CC_C) && !(sc & CC_Z)) exec = 0;
      else                             exec = 1;
      break;
    case 10: /* GE (N==V) */
      if (((sc & CC_N) && (sc & CC_V)) || (!(sc & CC_N) && !(sc & CC_V))) exec = 1;
      else                                                                exec = 0;
      break;
    case 11: /* LT (N!=V) */
      if (((sc & CC_N) && (sc & CC_V)) || (!(sc & CC_N) && !(sc & CC_V))) exec = 0;
      else                                                                exec = 1;
      break;
    case 12: /* GT (Z=0&N==V */
      if (!(sc & CC_Z) && (((sc & CC_N) && (sc & CC_V)) || (!(sc & CC_N) && !(sc & CC_V)))) exec = 1;
      else                                                                                  exec = 0;
      break;
    case 13: /* LE (Z=1|N!=V */
      if (!(sc & CC_Z) && (((sc & CC_N) && (sc & CC_V)) || (!(sc & CC_N) && !(sc & CC_V)))) exec = 0;
      else                                                                                  exec = 1;
      break;
    case 14: /* AL (Always) */
      exec = 1;
      break;
    case 15: /* AL (Always) */
      exec = 1;
      break;
    }
  }

  /* Shifter */
  switch (rob->sop) {
  case 0: /* LSL */
    if (rob->dbl) {
      s3 &= 63;
      if      (!s3)    { m0 = s2;    }
      else if (s3<64)  { m0 = s2<<s3;}
      else if (s3==64) { m0 = 0LL;   }
      else             { m0 = 0LL;   }
    }
    else {
      s3 &= 31;
      if      (!s3)    { m0 = (Ull)((Uint)s2);}
      else if (s3<32)  { m0 = (Ull)((Uint)s2<<s3);}
      else if (s3==32) { m0 = 0LL;   }
      else             { m0 = 0LL;   }
    }
    break;
  case 1: /* LSR */
    if (rob->dbl) {
      s3 &= 63;
      if      (!s3)    { m0 = s2;    }
      else if (s3<64)  { m0 = s2>>s3;}
      else if (s3==64) { m0 = 0LL;   }
      else             { m0 = 0LL;   }
    }
    else {
      s3 &= 31;
      if      (!s3)    { m0 = (Ull)((Uint)s2);}
      else if (s3<32)  { m0 = (Ull)((Uint)s2>>s3);}
      else if (s3==32) { m0 = 0LL;   }
      else             { m0 = 0LL;   }
    }
    break;
  case 2: /* ASR */
    if (rob->dbl) {
      s3 &= 63;
      if      (!s3)   { m0 = s2;}
      else if (s3<64) { m0 = (Sll)s2>>s3;}
      else            { m0 = !((Sll)s2>>63)?0:0xffffffffffffffffLL;}
    }
    else {
      s3 &= 31;
      if      (!s3)   { m0 = (Sll)((int)s2);}
      else if (s3<32) { m0 = (Sll)((int)s2>>s3);}
      else            { m0 = !((int)s2>>31)?0:0x00000000ffffffffLL;}
    }
    break;
  case 3: /* ROR */
    if (rob->dbl) {
      s3 &= 63;
      if      (!s3)   { m0 = s2;                    }
      else            { m0 = (s2<<(64-s3))|(s2>>s3);}
    }
    else {
      s3 &= 31;
      if      (!s3)   { m0 = (Ull)((Uint)s2);       }
      else            { m0 = (Ull)((Uint)s2<<(32-s3))|(Ull)((Uint)s2>>s3);}
    }
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

  if (rob->iinv) m0 = ~m0;

  /* ALU */
  switch (rob->opcd) {
  case 0: /* AND */
    D0 = s1 & m0;
    if (rob->dbl) {
      ccn  = D0>>63;
      ccz  = D0==0LL;
      ccc  = 0;
      ccv  = 0;
    }
    else {
      D0 &= 0x00000000ffffffffLL;
      ccn  = D0>>31;
      ccz  = D0==0LL;
      ccc  = 0;
      ccv  = 0;
    }
    break;
  case 1: /* EOR */
    D0 = s1 ^ m0;
    if (rob->dbl) {
      ccn  = D0>>63;
      ccz  = D0==0LL;
      ccc  = 0;
      ccv  = 0;
    }
    else {
      D0 &= 0x00000000ffffffffLL;
      ccn  = D0>>31;
      ccz  = D0==0LL;
      ccc  = 0;
      ccv  = 0;
    }
    break;
  case 3: /* ORR */
    D0 = s1 | m0;
    if (rob->dbl) {
      ccn  = D0>>63;
      ccz  = D0==0LL;
      ccc  = 0;
      ccv  = 0;
    }
    else {
      D0 &= 0x00000000ffffffffLL;
      ccn  = D0>>31;
      ccz  = D0==0LL;
      ccc  = 0;
      ccv  = 0;
    }
    break;
  case 4: /* ADD */
  case 5: /* ADC */
    if (rob->opcd == 4)
      D0 = s1 + m0;
    else
      D0 = s1 + m0 + ((sc&CC_C)!=0LL);
    if (rob->dbl) {
      ccn  = D0>>63;
      ccz  = D0==0LL;
      ccc  = (s1>>63&&m0>>63) || (!(D0>>63)&&(s1>>63||m0>>63));
      ccv  = (s1>>63&&m0>>63&&!(D0>>63)) || (!(s1>>63)&&!(m0>>63)&&D0>>63);
    }
    else {
      s1 &= 0x00000000ffffffffLL;
      m0 &= 0x00000000ffffffffLL;
      D0 &= 0x00000000ffffffffLL;
      ccn  = D0>>31;
      ccz  = D0==0LL;
      ccc  = (s1>>31&&m0>>31) || (!(D0>>31)&&(s1>>31||m0>>31));
      ccv  = (s1>>31&&m0>>31&&!(D0>>31)) || (!(s1>>31)&&!(m0>>31)&&D0>>31);
    }
    break;
  case 2: /* SUB */
  case 6: /* SBC */
    if (rob->opcd == 2)
      D0 = s1 - m0;
    else
      D0 = s1 - m0 - ((sc&CC_C)==0LL);
    if (rob->dbl) {
      ccn  = D0>>63;
      ccz  = D0==0LL;
      ccc  = !((!(s1>>63)&&m0>>63) || (D0>>63&&(!(s1>>63)||m0>>63)));
      ccv  = (s1>>63&&!(m0>>63)&&!(D0>>63)) || (!(s1>>63)&&m0>>63&&D0>>63);
    }
    else {
      s1 &= 0x00000000ffffffffLL;
      m0 &= 0x00000000ffffffffLL;
      D0 &= 0x00000000ffffffffLL;
      ccn  = D0>>31;
      ccz  = D0==0LL;
      ccc  = !((!(s1>>31)&&m0>>31) || (D0>>31&&(!(s1>>31)||m0>>31)));
      ccv  = (s1>>31&&!(m0>>31)&&!(D0>>31)) || (!(s1>>31)&&m0>>31&&D0>>31);
    }
    break;
  case 7: /* MOVN MOVZ MOVK */
    D0 = (s1 & ~wmask)|(m0 & wmask);
    if (rob->oinv)
      D0 = ~D0;
    if (!rob->dbl)
      D0 &= 0x00000000ffffffffLL;
    break;
  case 8: /* SBFM extend=1 */
    bot = (s1 & ~wmask)|(m0 & wmask);
    top = (Sll)(s2<<(63-S))>>63;
    D0 = (top & ~tmask)|(bot & tmask);
    if (!rob->dbl)
      D0 &= 0x00000000ffffffffLL;
    break;
  case 9:  /* BFM/UBFM extend=0 */
    bot = (s1 & ~wmask)|(m0 & wmask);
    top = s1;
    D0 = (top & ~tmask)|(bot & tmask);
    if (!rob->dbl)
      D0 &= 0x00000000ffffffffLL;
    break;
  case 10: /* CSINV */
    /* wmask:cond, tmask<0>:inc */
    if (exec)
      D0 = s1;
    else {
      D0 = s2;
      if (rob->oinv) /* inv */
	D0 = ~D0;
      if (tmask) /* inc */
	D0++;
    }
    if (!rob->dbl)
      D0 &= 0x00000000ffffffffLL;
    break;
  case 11: /* CCMN, CCMP */
    /* wmask:cond, tmask:nzcv */
    if (exec) {
      D0 = s1 + m0 + (rob->iinv); /* carry_in */
      if (rob->dbl) {
	ccn  = D0>>63;
	ccz  = D0==0LL;
	ccc  = (s1>>63&&m0>>63) || (!(D0>>63)&&(s1>>63||m0>>63));
	ccv  = (s1>>63&&m0>>63&&!(D0>>63)) || (!(s1>>63)&&!(m0>>63)&&D0>>63);
      }
      else {
	s1 &= 0x00000000ffffffffLL;
	m0 &= 0x00000000ffffffffLL;
	D0 &= 0x00000000ffffffffLL;
	ccn  = D0>>31;
	ccz  = D0==0LL;
	ccc  = (s1>>31&&m0>>31) || (!(D0>>31)&&(s1>>31||m0>>31));
	ccv  = (s1>>31&&m0>>31&&!(D0>>31)) || (!(s1>>31)&&!(m0>>31)&&D0>>31);
      }
    }
    else {
      ccn = (tmask & CC_N)!=0;
      ccz = (tmask & CC_Z)!=0;
      ccc = (tmask & CC_C)!=0;
      ccv = (tmask & CC_V)!=0;
    }
    break;
  case 12: /* REV */
    /* s2:revmode 0:8bit, 1:16bit, 2:32bit, 3:64bit */
    D0 = 0LL;
    switch (s2) {
    case 0: /* RBIT */
      if (rob->dbl) { /* b0 b1 .... b63 */
	for (i=0; i<64; i++) { bot = 0x0000000000000001LL&(s1>>i); D0 |= bot<<(63-i); }
      }
      else { /* 0 0 ... 0 b0 b1 .... b31 */
	for (i=0; i<32; i++) { bot = 0x0000000000000001LL&(s1>>i); D0 |= bot<<(31-i); }
      }
      break;
    case 1: /* REV16 */
      if (rob->dbl) { /* B6 B7 | B4 B5 | B2 B3 | B0 B1 */
	for (i=0; i<64; i+=16) { bot = 0x00000000000000ffLL&(s1>>i); D0 |= bot<<(i+8); }
	for (i=8; i<64; i+=16) { bot = 0x00000000000000ffLL&(s1>>i); D0 |= bot<<(i-8); }
      }
      else { /* 0 0 | 0 0 | B2 B3 | B0 B1 */
	for (i=0; i<32; i+=16) { bot = 0x00000000000000ffLL&(s1>>i); D0 |= bot<<(i+8); }
	for (i=8; i<32; i+=16) { bot = 0x00000000000000ffLL&(s1>>i); D0 |= bot<<(i-8); }
      }
      break;
    case 2: /* REV32 */
      if (rob->dbl) { /* B4 B5 | B6 B7 | B0 B1 | B2 B3 */
	for (i= 0; i<64; i+=32) { bot = 0x00000000000000ffLL&(s1>>i); D0 |= bot<<(i+24); }
	for (i= 8; i<64; i+=32) { bot = 0x00000000000000ffLL&(s1>>i); D0 |= bot<<(i+ 8); }
	for (i=16; i<64; i+=32) { bot = 0x00000000000000ffLL&(s1>>i); D0 |= bot<<(i- 8); }
	for (i=24; i<64; i+=32) { bot = 0x00000000000000ffLL&(s1>>i); D0 |= bot<<(i-24); }
      }
      else { /* 0 0 | 0 0 | B0 B1 | B2 B3 */
	for (i= 0; i<32; i+=32) { bot = 0x00000000000000ffLL&(s1>>i); D0 |= bot<<(i+24); }
	for (i= 8; i<32; i+=32) { bot = 0x00000000000000ffLL&(s1>>i); D0 |= bot<<(i+ 8); }
	for (i=16; i<32; i+=32) { bot = 0x00000000000000ffLL&(s1>>i); D0 |= bot<<(i- 8); }
	for (i=24; i<32; i+=32) { bot = 0x00000000000000ffLL&(s1>>i); D0 |= bot<<(i-24); }
      }
      break;
    default: /* REV64 B0 B1 | B2 B3 | B4 B5 | B6 B7 */
      {
	bot = 0x00000000000000ffLL&(s1>> 0); D0 |= bot<<(56);
	bot = 0x00000000000000ffLL&(s1>> 8); D0 |= bot<<(48);
	bot = 0x00000000000000ffLL&(s1>>16); D0 |= bot<<(40);
	bot = 0x00000000000000ffLL&(s1>>24); D0 |= bot<<(32);
	bot = 0x00000000000000ffLL&(s1>>32); D0 |= bot<<(24);
	bot = 0x00000000000000ffLL&(s1>>40); D0 |= bot<<(16);
	bot = 0x00000000000000ffLL&(s1>>48); D0 |= bot<<( 8);
	bot = 0x00000000000000ffLL&(s1>>56); D0 |= bot<<( 0);
      }
    }
    if (!rob->dbl)
      D0 &= 0x00000000ffffffffLL;
    break;
  case 13: /* CLZ/CLS */
    /* s2:mode 0:clz, 1:cls */
    D0 = 0LL;
    switch (s2) {
    case 1: /* CLS */
      s1 = (s1 ^ (s1<<1))|1LL;
    default: /* CLZ */
      if (rob->dbl) {
	for (i=0; i<64; i++) {
	  if ((0x8000000000000000LL&(s1<<i))==0LL)
	    D0++;
	  else
	    break;
	}
      }
      else {
	for (i=0; i<32; i++) {
	  if ((0x0000000080000000LL&(s1<<i))==0LL)
	    D0++;
	  else
	    break;
	}
      }
      break;
    }
    break;
  case 14: /* EXT */
    if (rob->dbl) {
      wmask &= 63;
      if (!wmask) { D0 = m0;                          }
      else        { D0 = (s1<<(64-wmask))|(m0>>wmask);}
    }
    else {
      wmask &= 31;
      if (!wmask) { D0 = (Ull)((Uint)m0);       }
      else        { D0 = (Ull)((Uint)s1<<(32-wmask))|(Ull)((Uint)m0>>wmask);}
    }
    break;
  }

  if (rob->dr[1].t) ex_drw(tid, 0LL, D0, rob, 1);
  if (rob->dr[3].t) ex_drw(tid, 0LL, (Ull)((ccn<<3)|(ccz<<2)|(ccc<<1)|ccv), rob, 3);

  return (ROB_COMPLETE); /* complete */
}
