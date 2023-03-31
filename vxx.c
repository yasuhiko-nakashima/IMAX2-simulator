
static char RcsHeader[] = "$Header: /usr/home/nakashim/proj-arm64/src/csim/RCS/vxx.c,v 1.35 2022/10/11 08:06:56 nakashim Exp nakashim $";

/* ARM Instruction Simulator           */
/*        Copyright (C) 2007 by NAIST. */
/*         Primary writer: Y.Nakashima */
/*                nakashim@is.naist.jp */

/* vxx.c 2005/3/22 */

#include "csim.h"

insn_exec_vxx(tid, rob) Uint tid; struct rob *rob;
{
  Uint cid = tid2cid(tid);
  union {
    Ull x;
    Uint w[2];
    Ushort h[4];
    Uchar b[8];
    double d;
    float s[2];
  } S1[2], S2[2], S3[2], D0[2];
  Uint NZCV;
  Uint i, index;
  int  shift;
  Ull  fill;

  if (rob->sr[0].t) ex_srr(tid, &S1[1].x, &S1[0].x, rob, 0);
  if (rob->sr[1].t) ex_srr(tid, &S2[1].x, &S2[0].x, rob, 1);
  if (rob->sr[2].t) ex_srr(tid, &S3[1].x, &S3[0].x, rob, 2);

  if (rob->opcd <= 14) { /* other than FDIV */
    switch (rob->opcd) {
    case 0: /* FMOVL/FMOVH */
      /* rob->dir 0:RR<-VR, 1:VR<-RR */
      /* rob->dbl 0:WR, 1:XR */
      switch (rob->sop) {
      case 0: /* FMOVL */
	if (rob->dir==0 && rob->dbl==0) {                 D0[0].w[1] = 0; D0[0].w[0] = S1[0].w[0];}
	if (rob->dir==0 && rob->dbl==1) {                                 D0[0].x    = S1[0].x;}
	if (rob->dir==1 && rob->dbl==0) { D0[1].x = 0LL;  D0[0].w[1] = 0; D0[0].w[0] = S1[0].w[0];}
	if (rob->dir==1 && rob->dbl==1) { D0[1].x = S2[1].x;              D0[0].x    = S1[0].x;}
	break;
      case 1: /* FMOVH */
	if (rob->dir==0 && rob->dbl==1) {                                 D0[0].x    = S1[1].x;}
	if (rob->dir==1 && rob->dbl==1) { D0[1].x = S1[0].x;              D0[0].x    = S2[0].x;}
	break;
      case 4: /* C7.3.133 FMOV.mov */
	if (rob->dbl==0) {                D0[0].w[0] = S1[0].w[0];}
	else             {                D0[0].x    = S1[0].x;}
	break;
      case 5: /* C7.3.133 FMOV.abs */
	if (rob->dbl==0) {                D0[0].w[0] = S1[0].w[0] & 0x7fffffff;}
	else             {                D0[0].x    = S1[0].x    & 0x7fffffffffffffffLL;}
	break;
      case 6: /* C7.3.133 FMOV.neg */
	if (rob->dbl==0) {                D0[0].w[0] = S1[0].w[0] ^ 0x80000000;}
	else             {                D0[0].x    = S1[0].x    ^ 0x8000000000000000LL;}
	break;
      case 7: /* C7.3.133 FMOV.sqrt */
	return (ROB_EXECERR); /* N.A. */
      default: /* undef */
	return (ROB_EXECERR); /* error */
      }
      break;
    case 1: /* FCVT */
      switch (rob->sop) {
      case 0: /* single<-half */
	return (ROB_EXECERR); /* unimplemented */
      case 1: /* double<-half */
	return (ROB_EXECERR); /* unimplemented */
      case 2: /* half  <-single */
	return (ROB_EXECERR); /* unimplemented */
      case 3: /* double<-single */
	D0[0].d = (double)S1[0].s[0];
	D0[1].x = 0LL;
	break;
      case 4: /* half  <-double */
	return (ROB_EXECERR); /* unimplemented */
      case 5: /* single<-double */
	D0[0].s[0] = (float)S1[0].d;
	D0[0].w[1] = 0;
	D0[1].x = 0LL;
	break;
      case 6: /* FRINTM(floor32) */
	D0[0].s[0] = floorf(S1[0].s[0]);
	D0[0].w[1] = 0;
	D0[1].x = 0LL;
	break;
      case 7: /* FRINTM(floor64) */
	D0[0].d = floor(S1[0].d);
	D0[1].x = 0LL;
	break;
      default: /* undef */
	return (ROB_EXECERR); /* error */
      }
      break;
    case 2: /* CVTSS,CVTSD */
      switch (rob->sop) {
	/* CVTSS */
      case 0: /* 0:rint */
	if (rob->dir==0 && rob->dbl==0) {                 D0[0].w[1] = 0; D0[0].w[0] = (int)rintf(S1[0].s[0]);}
	if (rob->dir==0 && rob->dbl==1) {                                 D0[0].x    = (Sll)rintf(S1[0].s[0]);}
	break;
      case 1: /* 1:ceil */
	if (rob->dir==0 && rob->dbl==0) {                 D0[0].w[1] = 0; D0[0].w[0] = (int)ceilf(S1[0].s[0]);}
	if (rob->dir==0 && rob->dbl==1) {                                 D0[0].x    = (Sll)ceilf(S1[0].s[0]);}
	break;
      case 2: /* 2:floor */
	if (rob->dir==0 && rob->dbl==0) {                 D0[0].w[1] = 0; D0[0].w[0] = (int)floorf(S1[0].s[0]);}
	if (rob->dir==0 && rob->dbl==1) {                                 D0[0].x    = (Sll)floorf(S1[0].s[0]);}
	break;
      case 3: /* 3:trunc */
	if (rob->dir==0 && rob->dbl==0) {                 D0[0].w[1] = 0; D0[0].w[0] = (int)truncf(S1[0].s[0]);}
	if (rob->dir==0 && rob->dbl==1) {                                 D0[0].x    = (Sll)truncf(S1[0].s[0]);}
	break;
      case 4: /* 4:round */
	if (rob->dir==0 && rob->dbl==0) {                 D0[0].w[1] = 0; D0[0].w[0] = (int)roundf(S1[0].s[0]);}
	if (rob->dir==0 && rob->dbl==1) {                                 D0[0].x    = (Sll)roundf(S1[0].s[0]);}
	if (rob->dir==1 && rob->dbl==0) { D0[1].x = 0LL;  D0[0].s[1] = 0; D0[0].s[0] = (float)(int)S1[0].w[0];}
	if (rob->dir==1 && rob->dbl==1) { D0[1].x = 0LL;  D0[0].s[1] = 0; D0[0].s[0] = (float)(Sll)S1[0].x;}
	break;
	/* CVTSD */
      case 8: /* 0:rint */
	if (rob->dir==0 && rob->dbl==0) {                 D0[0].w[1] = 0; D0[0].w[0] = (int)rint(S1[0].d);}
	if (rob->dir==0 && rob->dbl==1) {                                 D0[0].x    = (Sll)rint(S1[0].d);}
	break;
      case 9: /* 1:ceil */
	if (rob->dir==0 && rob->dbl==0) {                 D0[0].w[1] = 0; D0[0].w[0] = (int)ceil(S1[0].d);}
	if (rob->dir==0 && rob->dbl==1) {                                 D0[0].x    = (Sll)ceil(S1[0].d);}
	break;
      case 10: /* 2:floor */
	if (rob->dir==0 && rob->dbl==0) {                 D0[0].w[1] = 0; D0[0].w[0] = (int)floor(S1[0].d);}
	if (rob->dir==0 && rob->dbl==1) {                                 D0[0].x    = (Sll)floor(S1[0].d);}
	break;
      case 11: /* 3:trunc */
	if (rob->dir==0 && rob->dbl==0) {                 D0[0].w[1] = 0; D0[0].w[0] = (int)trunc(S1[0].d);}
	if (rob->dir==0 && rob->dbl==1) {                                 D0[0].x    = (Sll)trunc(S1[0].d);}
	break;
      case 12: /* 4:round */
	if (rob->dir==0 && rob->dbl==0) {                 D0[0].w[1] = 0; D0[0].w[0] = (int)round(S1[0].d);}
	if (rob->dir==0 && rob->dbl==1) {                                 D0[0].x    = (Sll)round(S1[0].d);}
	if (rob->dir==1 && rob->dbl==0) { D0[1].x = 0LL;                  D0[0].d    = (double)(int)S1[0].w[0];}
	if (rob->dir==1 && rob->dbl==1) { D0[1].x = 0LL;                  D0[0].d    = (double)(Sll)S1[0].x;}
	break;
      }
      break;
    case 3: /* CVTUS,CVTUD */
      switch (rob->sop) {
	/* CVTUS */
      case 0: /* 0:rint */
	if (rob->dir==0 && rob->dbl==0) {                 D0[0].w[1] = 0; D0[0].w[0] = (Uint)rintf(S1[0].s[0]);}
	if (rob->dir==0 && rob->dbl==1) {                                 D0[0].x    = (Ull) rintf(S1[0].s[0]);}
	break;
      case 1: /* 1:ceil */
	if (rob->dir==0 && rob->dbl==0) {                 D0[0].w[1] = 0; D0[0].w[0] = (Uint)ceilf(S1[0].s[0]);}
	if (rob->dir==0 && rob->dbl==1) {                                 D0[0].x    = (Ull) ceilf(S1[0].s[0]);}
	break;
      case 2: /* 2:floor */
	if (rob->dir==0 && rob->dbl==0) {                 D0[0].w[1] = 0; D0[0].w[0] = (Uint)floorf(S1[0].s[0]);}
	if (rob->dir==0 && rob->dbl==1) {                                 D0[0].x    = (Ull) floorf(S1[0].s[0]);}
	break;
      case 3: /* 3:trunc */
	if (rob->dir==0 && rob->dbl==0) {                 D0[0].w[1] = 0; D0[0].w[0] = (Uint)truncf(S1[0].s[0]);}
	if (rob->dir==0 && rob->dbl==1) {                                 D0[0].x    = (Ull) truncf(S1[0].s[0]);}
	break;
      case 4: /* 4:round */
	if (rob->dir==0 && rob->dbl==0) {                 D0[0].w[1] = 0; D0[0].w[0] = (Uint)roundf(S1[0].s[0]);}
	if (rob->dir==0 && rob->dbl==1) {                                 D0[0].x    = (Ull) roundf(S1[0].s[0]);}
	if (rob->dir==1 && rob->dbl==0) { D0[1].x = 0LL;  D0[0].s[1] = 0; D0[0].s[0] = (float)S1[0].w[0];}
	if (rob->dir==1 && rob->dbl==1) { D0[1].x = 0LL;  D0[0].s[1] = 0; D0[0].s[0] = (float)S1[0].x;}
	break;
	/* CVTUD */
      case 8: /* 0:rint */
	if (rob->dir==0 && rob->dbl==0) {                 D0[0].w[1] = 0; D0[0].w[0] = (Uint)rint(S1[0].d);}
	if (rob->dir==0 && rob->dbl==1) {                                 D0[0].x    = (Ull) rint(S1[0].d);}
	break;
      case 9: /* 1:ceil */
	if (rob->dir==0 && rob->dbl==0) {                 D0[0].w[1] = 0; D0[0].w[0] = (Uint)ceil(S1[0].d);}
	if (rob->dir==0 && rob->dbl==1) {                                 D0[0].x    = (Ull) ceil(S1[0].d);}
	break;
      case 10: /* 2:floor */
	if (rob->dir==0 && rob->dbl==0) {                 D0[0].w[1] = 0; D0[0].w[0] = (Uint)floor(S1[0].d);}
	if (rob->dir==0 && rob->dbl==1) {                                 D0[0].x    = (Ull) floor(S1[0].d);}
	break;
      case 11: /* 3:trunc */
	if (rob->dir==0 && rob->dbl==0) {                 D0[0].w[1] = 0; D0[0].w[0] = (Uint)trunc(S1[0].d);}
	if (rob->dir==0 && rob->dbl==1) {                                 D0[0].x    = (Ull) trunc(S1[0].d);}
	break;
      case 12: /* 4:round */
	if (rob->dir==0 && rob->dbl==0) {                 D0[0].w[1] = 0; D0[0].w[0] = (Uint)round(S1[0].d);}
	if (rob->dir==0 && rob->dbl==1) {                                 D0[0].x    = (Ull) round(S1[0].d);}
	if (rob->dir==1 && rob->dbl==0) { D0[1].x = 0LL;                  D0[0].d    = (double)S1[0].w[0];}
	if (rob->dir==1 && rob->dbl==1) { D0[1].x = 0LL;                  D0[0].d    = (double)S1[0].x;}
	break;
      }
      break;
    case 4: /* SHIFT */
      switch (rob->sop) {
      case 0: /* SSHR(scalar) */
	shift = 64*2 - S2[0].b[0];
	if (shift < 0)
	  return (ROB_EXECERR); /* error */
	D0[0].x = (Sll)S1[0].x >> shift;
	D0[1].x = 0LL;
	break;
      case 1: /* USHR(scalar) */
	shift = 64*2 - S2[0].b[0];
	if (shift < 0)
	  return (ROB_EXECERR); /* error */
	D0[0].x = S1[0].x >> shift;
	D0[1].x = 0LL;
	break;
      case 2: /* SSHR(vector) */
	switch (S2[0].b[0] >> 3) { /* immh */
	case 1: /* 8B/16B */
	  shift = 8*2 - S2[0].b[0];
	  for (i=0; i<8; i++) D0[0].b[i] = (char)S1[0].b[i] >> shift;
	  if (rob->dbl)
	    for (i=0; i<8; i++) D0[1].b[i] = (char)S1[1].b[i] >> shift;
	  else
	    D0[1].x = 0x0000000000000000LL;
	  break;
	case 2:	case 3: /* 4H/8H */
	  shift = 16*2 - S2[0].b[0];
	  for (i=0; i<4; i++) D0[0].h[i] = (short)S1[0].h[i] >> shift;;
	  if (rob->dbl)
	    for (i=0; i<4; i++) D0[1].h[i] = (short)S1[1].h[i] >> shift;
	  else
	    D0[1].x = 0x0000000000000000LL;
	  break;
	case 4:	case 5:	case 6:	case 7: /* 2S/4S */
	  shift = 32*2 - S2[0].b[0];
	  for (i=0; i<2; i++) D0[0].w[i] = (int)S1[0].w[i] >> shift;
	  if (rob->dbl)
	    for (i=0; i<2; i++) D0[1].w[i] = (int)S1[1].w[i] >> shift;
	  else
	    D0[1].x = 0x0000000000000000LL;
	  break;
	case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 15: /* 2D */
	  shift = 64*2 - S2[0].b[0];
	  if (rob->dbl) {
	    D0[0].x = (Sll)S1[0].x >> shift;
	    D0[1].x = (Sll)S1[1].x >> shift;
	  }
	  else
	    return (ROB_EXECERR); /* error */
	  break;
	default:
	  return (ROB_EXECERR); /* error */
	}
	break;
      case 3: /* USHR(vector) */
	switch (S2[0].b[0] >> 3) { /* immh */
	case 1: /* 8B/16B */
	  shift = 8*2 - S2[0].b[0];
	  for (i=0; i<8; i++) D0[0].b[i] = S1[0].b[i] >> shift;
	  if (rob->dbl)
	    for (i=0; i<8; i++) D0[1].b[i] = S1[1].b[i] >> shift;
	  else
	    D0[1].x = 0x0000000000000000LL;
	  break;
	case 2:	case 3: /* 4H/8H */
	  shift = 16*2 - S2[0].b[0];
	  for (i=0; i<4; i++) D0[0].h[i] = S1[0].h[i] >> shift;;
	  if (rob->dbl)
	    for (i=0; i<4; i++) D0[1].h[i] = S1[1].h[i] >> shift;
	  else
	    D0[1].x = 0x0000000000000000LL;
	  break;
	case 4:	case 5:	case 6:	case 7: /* 2S/4S */
	  shift = 32*2 - S2[0].b[0];
	  for (i=0; i<2; i++) D0[0].w[i] = S1[0].w[i] >> shift;
	  if (rob->dbl)
	    for (i=0; i<2; i++) D0[1].w[i] = S1[1].w[i] >> shift;
	  else
	    D0[1].x = 0x0000000000000000LL;
	  break;
	case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 15: /* 2D */
	  shift = 64*2 - S2[0].b[0];
	  if (rob->dbl) {
	    D0[0].x = S1[0].x >> shift;
	    D0[1].x = S1[1].x >> shift;
	  }
	  else
	    return (ROB_EXECERR); /* error */
	  break;
	default:
	  return (ROB_EXECERR); /* error */
	}
	break;
      case 4: /* SHL(scalar) */
	shift = S2[0].b[0] - 64;
	if (shift < 0)
	  return (ROB_EXECERR); /* error */
	D0[0].x = S1[0].x << shift;
	D0[1].x = 0LL;
	break;
      case 5: /* SHL(vector) */
	switch (S2[0].b[0] >> 3) { /* immh */
	case 1: /* 8B/16B */
	  shift = S2[0].b[0] - 8;
	  for (i=0; i<8; i++) D0[0].b[i] = S1[0].b[i] << shift;
	  if (rob->dbl)
	    for (i=0; i<8; i++) D0[1].b[i] = S1[1].b[i] << shift;
	  else
	    D0[1].x = 0x0000000000000000LL;
	  break;
	case 2:	case 3: /* 4H/8H */
	  shift = S2[0].b[0] - 16;
	  for (i=0; i<4; i++) D0[0].h[i] = S1[0].h[i] << shift;;
	  if (rob->dbl)
	    for (i=0; i<4; i++) D0[1].h[i] = S1[1].h[i] << shift;
	  else
	    D0[1].x = 0x0000000000000000LL;
	  break;
	case 4:	case 5:	case 6:	case 7: /* 2S/4S */
	  shift = S2[0].b[0] - 32;
	  for (i=0; i<2; i++) D0[0].w[i] = S1[0].w[i] << shift;
	  if (rob->dbl)
	    for (i=0; i<2; i++) D0[1].w[i] = S1[1].w[i] << shift;
	  else
	    D0[1].x = 0x0000000000000000LL;
	  break;
	case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 15: /* 2D */
	  shift = S2[0].b[0] - 64;
	  if (rob->dbl) {
	    D0[0].x = S1[0].x << shift;
	    D0[1].x = S1[1].x << shift;
	  }
	  else
	    return (ROB_EXECERR); /* error */
	  break;
	default:
	  return (ROB_EXECERR); /* error */
	}
	break;
      case 6: /* SSHLL/SSHLL2(vector) */
	switch (S2[0].b[0] >> 3) { /* immh */
	case 1: /* 8B/16B */
	  shift = S2[0].b[0] - 8;
	  if (!rob->dbl) { /* lower64 */
	    for (i=0; i<4; i++) D0[0].h[i] = (short)(char)S1[0].b[i  ] << shift;
	    for (i=0; i<4; i++) D0[1].h[i] = (short)(char)S1[0].b[i+4] << shift;
	  }
	  else { /* upper64 */
	    for (i=0; i<4; i++) D0[0].h[i] = (short)(char)S1[1].b[i  ] << shift;
	    for (i=0; i<4; i++) D0[1].h[i] = (short)(char)S1[1].b[i+4] << shift;
	  } 
	  break;
	case 2:	case 3: /* 4H/8H */
	  shift = S2[0].b[0] - 16;
	  if (!rob->dbl) { /* lower64 */
	    for (i=0; i<2; i++) D0[0].w[i] = (int)(short)S1[0].h[i  ] << shift;
	    for (i=0; i<2; i++) D0[1].w[i] = (int)(short)S1[0].h[i+2] << shift;
	  }
	  else { /* upper64 */
	    for (i=0; i<2; i++) D0[0].w[i] = (int)(short)S1[1].h[i  ] << shift;
	    for (i=0; i<2; i++) D0[1].w[i] = (int)(short)S1[1].h[i+2] << shift;
	  } 
	  break;
	case 4:	case 5:	case 6:	case 7: /* 2S/4S */
	  shift = S2[0].b[0] - 32;
	  if (!rob->dbl) { /* lower64 */
	    for (i=0; i<1; i++) D0[0].x = (Sll)(int)S1[0].w[i  ] << shift;
	    for (i=0; i<1; i++) D0[1].x = (Sll)(int)S1[0].w[i+1] << shift;
	  }
	  else { /* upper64 */
	    for (i=0; i<1; i++) D0[0].x = (Sll)(int)S1[1].w[i  ] << shift;
	    for (i=0; i<1; i++) D0[1].x = (Sll)(int)S1[1].w[i+1] << shift;
	  } 
	  break;
	default:
	  return (ROB_EXECERR); /* error */
	}
	break;
      case 7: /* USHLL/USHLL2(vector) */
	switch (S2[0].b[0] >> 3) { /* immh */
	case 1: /* 8B/16B */
	  shift = S2[0].b[0] - 8;
	  if (!rob->dbl) { /* lower64 */
	    for (i=0; i<4; i++) D0[0].h[i] = (Ushort)S1[0].b[i  ] << shift;
	    for (i=0; i<4; i++) D0[1].h[i] = (Ushort)S1[0].b[i+4] << shift;
	  }
	  else { /* upper64 */
	    for (i=0; i<4; i++) D0[0].h[i] = (Ushort)S1[1].b[i  ] << shift;
	    for (i=0; i<4; i++) D0[1].h[i] = (Ushort)S1[1].b[i+4] << shift;
	  } 
	  break;
	case 2:	case 3: /* 4H/8H */
	  shift = S2[0].b[0] - 16;
	  if (!rob->dbl) { /* lower64 */
	    for (i=0; i<2; i++) D0[0].w[i] = (Uint)S1[0].h[i  ] << shift;
	    for (i=0; i<2; i++) D0[1].w[i] = (Uint)S1[0].h[i+2] << shift;
	  }
	  else { /* upper64 */
	    for (i=0; i<2; i++) D0[0].w[i] = (Uint)S1[1].h[i  ] << shift;
	    for (i=0; i<2; i++) D0[1].w[i] = (Uint)S1[1].h[i+2] << shift;
	  } 
	  break;
	case 4:	case 5:	case 6:	case 7: /* 2S/4S */
	  shift = S2[0].b[0] - 32;
	  if (!rob->dbl) { /* lower64 */
	    for (i=0; i<1; i++) D0[0].x = (Ull)S1[0].w[i  ] << shift;
	    for (i=0; i<1; i++) D0[1].x = (Ull)S1[0].w[i+1] << shift;
	  }
	  else { /* upper64 */
	    for (i=0; i<1; i++) D0[0].x = (Ull)S1[1].w[i  ] << shift;
	    for (i=0; i<1; i++) D0[1].x = (Ull)S1[1].w[i+1] << shift;
	  } 
	  break;
	default:
	  return (ROB_EXECERR); /* error */
	}
	break;
      default: /* not used */
	return (ROB_EXECERR); /* error */
      }
      break;
    case 5: /* COMPARE */
      if (!rob->iinv) { /* register */
	switch (rob->sop) {
	case 0: /* CMGT(scalar) *//*01011110--1-----001101*/
	  if (rob->size != 3)
	    return (ROB_EXECERR); /* error */
	  D0[0].x = (Sll)S1[0].x>(Sll)S2[0].x ? 0xffffffffffffffffLL:0LL;
	  D0[1].x = 0LL;
	  break;
	case 1: /* CMGT(vector) *//*0Q001110--1-----001101*/
	  switch (rob->size) {
	  case 0: /* 8B/16B */
	    if (!rob->dbl) {
	      for (i=0; i<8; i++) D0[0].b[i] = (char)S1[0].b[i]>(char)S2[0].b[i] ? 0xff:0x00;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<8; i++) D0[0].b[i] = (char)S1[0].b[i]>(char)S2[0].b[i] ? 0xff:0x00;
	      for (i=0; i<8; i++) D0[1].b[i] = (char)S1[1].b[i]>(char)S2[1].b[i] ? 0xff:0x00;
	    }
	    break;
	  case 1: /* 4H/8H */
	    if (!rob->dbl) {
	      for (i=0; i<4; i++) D0[0].h[i] = (short)S1[0].h[i]>(short)S2[0].h[i] ? 0xffff:0x0000;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<4; i++) D0[0].h[i] = (short)S1[0].h[i]>(short)S2[0].h[i] ? 0xffff:0x0000;
	      for (i=0; i<4; i++) D0[1].h[i] = (short)S1[1].h[i]>(short)S2[1].h[i] ? 0xffff:0x0000;
	    }
	    break;
	  case 2: /* 2S/4S */
	    if (!rob->dbl) {
	      for (i=0; i<2; i++) D0[0].w[i] = (int)S1[0].w[i]>(int)S2[0].w[i] ? 0xffffffff:0x00000000;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<2; i++) D0[0].w[i] = (int)S1[0].w[i]>(int)S2[0].w[i] ? 0xffffffff:0x00000000;
	      for (i=0; i<2; i++) D0[1].w[i] = (int)S1[1].w[i]>(int)S2[1].w[i] ? 0xffffffff:0x00000000;
	    }
	    break;
	  case 3: /* 2D */
	    if (!rob->dbl)
	      return (ROB_EXECERR); /* error */
	    for (i=0; i<1; i++) D0[0].x = (Sll)S1[0].x>(Sll)S2[0].x ? 0xffffffffffffffffLL:0LL;
	    for (i=0; i<1; i++) D0[1].x = (Sll)S1[1].x>(Sll)S2[1].x ? 0xffffffffffffffffLL:0LL;
	    break;
	  default: /* undef */
	    return (ROB_EXECERR); /* error */
          }
	  break;
	case 2: /* CMHI(scalar) *//*01111110--1-----001101*/
	  if (rob->size != 3)
	    return (ROB_EXECERR); /* error */
	  D0[0].x = S1[0].x>S2[0].x ? 0xffffffffffffffffLL:0LL;
	  D0[1].x = 0LL;
	  break;
	case 3: /* CMHI(vector) *//*0Q101110--1-----001101*/
	  switch (rob->size) {
	  case 0: /* 8B/16B */
	    if (!rob->dbl) {
	      for (i=0; i<8; i++) D0[0].b[i] = S1[0].b[i]>S2[0].b[i] ? 0xff:0x00;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<8; i++) D0[0].b[i] = S1[0].b[i]>S2[0].b[i] ? 0xff:0x00;
	      for (i=0; i<8; i++) D0[1].b[i] = S1[1].b[i]>S2[1].b[i] ? 0xff:0x00;
	    }
	    break;
	  case 1: /* 4H/8H */
	    if (!rob->dbl) {
	      for (i=0; i<4; i++) D0[0].h[i] = S1[0].h[i]>S2[0].h[i] ? 0xffff:0x0000;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<4; i++) D0[0].h[i] = S1[0].h[i]>S2[0].h[i] ? 0xffff:0x0000;
	      for (i=0; i<4; i++) D0[1].h[i] = S1[1].h[i]>S2[1].h[i] ? 0xffff:0x0000;
	    }
	    break;
	  case 2: /* 2S/4S */
	    if (!rob->dbl) {
	      for (i=0; i<2; i++) D0[0].w[i] = S1[0].w[i]>S2[0].w[i] ? 0xffffffff:0x00000000;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<2; i++) D0[0].w[i] = S1[0].w[i]>S2[0].w[i] ? 0xffffffff:0x00000000;
	      for (i=0; i<2; i++) D0[1].w[i] = S1[1].w[i]>S2[1].w[i] ? 0xffffffff:0x00000000;
	    }
	    break;
	  case 3: /* 2D */
	    if (!rob->dbl)
	      return (ROB_EXECERR); /* error */
	    for (i=0; i<1; i++) D0[0].x = S1[0].x>S2[0].x ? 0xffffffffffffffffLL:0LL;
	    for (i=0; i<1; i++) D0[1].x = S1[1].x>S2[1].x ? 0xffffffffffffffffLL:0LL;
	    break;
	  default: /* undef */
	    return (ROB_EXECERR); /* error */
          }
	  break;
	case 4: /* CMGE(scalar) *//*01011110--1-----001111*/
	  if (rob->size != 3)
	    return (ROB_EXECERR); /* error */
	  D0[0].x = (Sll)S1[0].x>=(Sll)S2[0].x ? 0xffffffffffffffffLL:0LL;
	  D0[1].x = 0LL;
	  break;
	case 5: /* CMGE(vector) *//*0Q001110--1-----001111*/
	  switch (rob->size) {
	  case 0: /* 8B/16B */
	    if (!rob->dbl) {
	      for (i=0; i<8; i++) D0[0].b[i] = (char)S1[0].b[i]>=(char)S2[0].b[i] ? 0xff:0x00;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<8; i++) D0[0].b[i] = (char)S1[0].b[i]>=(char)S2[0].b[i] ? 0xff:0x00;
	      for (i=0; i<8; i++) D0[1].b[i] = (char)S1[1].b[i]>=(char)S2[1].b[i] ? 0xff:0x00;
	    }
	    break;
	  case 1: /* 4H/8H */
	    if (!rob->dbl) {
	      for (i=0; i<4; i++) D0[0].h[i] = (short)S1[0].h[i]>=(short)S2[0].h[i] ? 0xffff:0x0000;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<4; i++) D0[0].h[i] = (short)S1[0].h[i]>=(short)S2[0].h[i] ? 0xffff:0x0000;
	      for (i=0; i<4; i++) D0[1].h[i] = (short)S1[1].h[i]>=(short)S2[1].h[i] ? 0xffff:0x0000;
	    }
	    break;
	  case 2: /* 2S/4S */
	    if (!rob->dbl) {
	      for (i=0; i<2; i++) D0[0].w[i] = (int)S1[0].w[i]>=(int)S2[0].w[i] ? 0xffffffff:0x00000000;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<2; i++) D0[0].w[i] = (int)S1[0].w[i]>=(int)S2[0].w[i] ? 0xffffffff:0x00000000;
	      for (i=0; i<2; i++) D0[1].w[i] = (int)S1[1].w[i]>=(int)S2[1].w[i] ? 0xffffffff:0x00000000;
	    }
	    break;
	  case 3: /* 2D */
	    if (!rob->dbl)
	      return (ROB_EXECERR); /* error */
	    for (i=0; i<1; i++) D0[0].x = (Sll)S1[0].x>=(Sll)S2[0].x ? 0xffffffffffffffffLL:0LL;
	    for (i=0; i<1; i++) D0[1].x = (Sll)S1[1].x>=(Sll)S2[1].x ? 0xffffffffffffffffLL:0LL;
	    break;
	  default: /* undef */
	    return (ROB_EXECERR); /* error */
          }
	  break;
	case 6: /* CMHS(scalar) *//*01111110--1-----001111*/
	  if (rob->size != 3)
	    return (ROB_EXECERR); /* error */
	  D0[0].x = S1[0].x>=S2[0].x ? 0xffffffffffffffffLL:0LL;
	  D0[1].x = 0LL;
	  break;
	case 7: /* CMHS(vector) *//*0Q101110--1-----001111*/
	  switch (rob->size) {
	  case 0: /* 8B/16B */
	    if (!rob->dbl) {
	      for (i=0; i<8; i++) D0[0].b[i] = S1[0].b[i]>=S2[0].b[i] ? 0xff:0x00;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<8; i++) D0[0].b[i] = S1[0].b[i]>=S2[0].b[i] ? 0xff:0x00;
	      for (i=0; i<8; i++) D0[1].b[i] = S1[1].b[i]>=S2[1].b[i] ? 0xff:0x00;
	    }
	    break;
	  case 1: /* 4H/8H */
	    if (!rob->dbl) {
	      for (i=0; i<4; i++) D0[0].h[i] = S1[0].h[i]>=S2[0].h[i] ? 0xffff:0x0000;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<4; i++) D0[0].h[i] = S1[0].h[i]>=S2[0].h[i] ? 0xffff:0x0000;
	      for (i=0; i<4; i++) D0[1].h[i] = S1[1].h[i]>=S2[1].h[i] ? 0xffff:0x0000;
	    }
	    break;
	  case 2: /* 2S/4S */
	    if (!rob->dbl) {
	      for (i=0; i<2; i++) D0[0].w[i] = S1[0].w[i]>=S2[0].w[i] ? 0xffffffff:0x00000000;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<2; i++) D0[0].w[i] = S1[0].w[i]>=S2[0].w[i] ? 0xffffffff:0x00000000;
	      for (i=0; i<2; i++) D0[1].w[i] = S1[1].w[i]>=S2[1].w[i] ? 0xffffffff:0x00000000;
	    }
	    break;
	  case 3: /* 2D */
	    if (!rob->dbl)
	      return (ROB_EXECERR); /* error */
	    for (i=0; i<1; i++) D0[0].x = S1[0].x>=S2[0].x ? 0xffffffffffffffffLL:0LL;
	    for (i=0; i<1; i++) D0[1].x = S1[1].x>=S2[1].x ? 0xffffffffffffffffLL:0LL;
	    break;
	  default: /* undef */
	    return (ROB_EXECERR); /* error */
          }
	  break;
	case 8: /* CMTST(scalar) *//*01011110--1-----100011*/
	  if (rob->size != 3)
	    return (ROB_EXECERR); /* error */
	  D0[0].x = S1[0].x&S2[0].x ? 0xffffffffffffffffLL:0LL;
	  D0[1].x = 0LL;
	  break;
	case 9: /* CMTST(vector) *//*0Q001110--1-----100011*/
	  switch (rob->size) {
	  case 0: /* 8B/16B */
	    if (!rob->dbl) {
	      for (i=0; i<8; i++) D0[0].b[i] = S1[0].b[i]&S2[0].b[i] ? 0xff:0x00;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<8; i++) D0[0].b[i] = S1[0].b[i]&S2[0].b[i] ? 0xff:0x00;
	      for (i=0; i<8; i++) D0[1].b[i] = S1[1].b[i]&S2[1].b[i] ? 0xff:0x00;
	    }
	    break;
	  case 1: /* 4H/8H */
	    if (!rob->dbl) {
	      for (i=0; i<4; i++) D0[0].h[i] = S1[0].h[i]&S2[0].h[i] ? 0xffff:0x0000;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<4; i++) D0[0].h[i] = S1[0].h[i]&S2[0].h[i] ? 0xffff:0x0000;
	      for (i=0; i<4; i++) D0[1].h[i] = S1[1].h[i]&S2[1].h[i] ? 0xffff:0x0000;
	    }
	    break;
	  case 2: /* 2S/4S */
	    if (!rob->dbl) {
	      for (i=0; i<2; i++) D0[0].w[i] = S1[0].w[i]&S2[0].w[i] ? 0xffffffff:0x00000000;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<2; i++) D0[0].w[i] = S1[0].w[i]&S2[0].w[i] ? 0xffffffff:0x00000000;
	      for (i=0; i<2; i++) D0[1].w[i] = S1[1].w[i]&S2[1].w[i] ? 0xffffffff:0x00000000;
	    }
	    break;
	  case 3: /* 2D */
	    if (!rob->dbl)
	      return (ROB_EXECERR); /* error */
	    for (i=0; i<1; i++) D0[0].x = S1[0].x&S2[0].x ? 0xffffffffffffffffLL:0LL;
	    for (i=0; i<1; i++) D0[1].x = S1[1].x&S2[1].x ? 0xffffffffffffffffLL:0LL;
	    break;
	  default: /* undef */
	    return (ROB_EXECERR); /* error */
          }
	  break;
	case 10: /* CMEQ(scalar) *//*01111110--1-----100011*/
	  if (rob->size != 3)
	    return (ROB_EXECERR); /* error */
	  D0[0].x = S1[0].x==S2[0].x ? 0xffffffffffffffffLL:0LL;
	  D0[1].x = 0LL;
	  break;
	case 11: /* CMEQ(vector) *//*0Q101110--1-----100011*/
	  switch (rob->size) {
	  case 0: /* 8B/16B */
	    if (!rob->dbl) {
	      for (i=0; i<8; i++) D0[0].b[i] = S1[0].b[i]==S2[0].b[i] ? 0xff:0x00;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<8; i++) D0[0].b[i] = S1[0].b[i]==S2[0].b[i] ? 0xff:0x00;
	      for (i=0; i<8; i++) D0[1].b[i] = S1[1].b[i]==S2[1].b[i] ? 0xff:0x00;
	    }
	    break;
	  case 1: /* 4H/8H */
	    if (!rob->dbl) {
	      for (i=0; i<4; i++) D0[0].h[i] = S1[0].h[i]==S2[0].h[i] ? 0xffff:0x0000;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<4; i++) D0[0].h[i] = S1[0].h[i]==S2[0].h[i] ? 0xffff:0x0000;
	      for (i=0; i<4; i++) D0[1].h[i] = S1[1].h[i]==S2[1].h[i] ? 0xffff:0x0000;
	    }
	    break;
	  case 2: /* 2S/4S */
	    if (!rob->dbl) {
	      for (i=0; i<2; i++) D0[0].w[i] = S1[0].w[i]==S2[0].w[i] ? 0xffffffff:0x00000000;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<2; i++) D0[0].w[i] = S1[0].w[i]==S2[0].w[i] ? 0xffffffff:0x00000000;
	      for (i=0; i<2; i++) D0[1].w[i] = S1[1].w[i]==S2[1].w[i] ? 0xffffffff:0x00000000;
	    }
	    break;
	  case 3: /* 2D */
	    if (!rob->dbl)
	      return (ROB_EXECERR); /* error */
	    for (i=0; i<1; i++) D0[0].x = S1[0].x==S2[0].x ? 0xffffffffffffffffLL:0LL;
	    for (i=0; i<1; i++) D0[1].x = S1[1].x==S2[1].x ? 0xffffffffffffffffLL:0LL;
	    break;
	  default: /* undef */
	    return (ROB_EXECERR); /* error */
          }
	  break;
	default: /* not used */
	  return (ROB_EXECERR); /* error */
	}
      }
      else { /* zero */
	switch (rob->sop) {
	case 0: /* CMGTZ(scalar) *//*01011110--100000100010*/
	  if (rob->size != 3)
	    return (ROB_EXECERR); /* error */
	  D0[0].x = (Sll)S1[0].x>0LL ? 0xffffffffffffffffLL:0LL;
	  D0[1].x = 0LL;
	  break;
	case 1: /* CMGTZ(vector) *//*0Q001110--100000100010*/
	  switch (rob->size) {
	  case 0: /* 8B/16B */
	    if (!rob->dbl) {
	      for (i=0; i<8; i++) D0[0].b[i] = (char)S1[0].b[i]>0 ? 0xff:0x00;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<8; i++) D0[0].b[i] = (char)S1[0].b[i]>0 ? 0xff:0x00;
	      for (i=0; i<8; i++) D0[1].b[i] = (char)S1[1].b[i]>0 ? 0xff:0x00;
	    }
	    break;
	  case 1: /* 4H/8H */
	    if (!rob->dbl) {
	      for (i=0; i<4; i++) D0[0].h[i] = (short)S1[0].h[i]>0 ? 0xffff:0x0000;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<4; i++) D0[0].h[i] = (short)S1[0].h[i]>0 ? 0xffff:0x0000;
	      for (i=0; i<4; i++) D0[1].h[i] = (short)S1[1].h[i]>0 ? 0xffff:0x0000;
	    }
	    break;
	  case 2: /* 2S/4S */
	    if (!rob->dbl) {
	      for (i=0; i<2; i++) D0[0].w[i] = (int)S1[0].w[i]>0 ? 0xffffffff:0x00000000;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<2; i++) D0[0].w[i] = (int)S1[0].w[i]>0 ? 0xffffffff:0x00000000;
	      for (i=0; i<2; i++) D0[1].w[i] = (int)S1[1].w[i]>0 ? 0xffffffff:0x00000000;
	    }
	    break;
	  case 3: /* 2D */
	    if (!rob->dbl)
	      return (ROB_EXECERR); /* error */
	    for (i=0; i<1; i++) D0[0].x = (Sll)S1[0].x>0LL ? 0xffffffffffffffffLL:0LL;
	    for (i=0; i<1; i++) D0[1].x = (Sll)S1[1].x>0LL ? 0xffffffffffffffffLL:0LL;
	    break;
	  default: /* undef */
	    return (ROB_EXECERR); /* error */
          }
	  break;
	case 2: /* CMGEZ(scalar) *//*01111110--100000100010*/
	  if (rob->size != 3)
	    return (ROB_EXECERR); /* error */
	  D0[0].x = (Sll)S1[0].x>=0LL ? 0xffffffffffffffffLL:0LL;
	  D0[1].x = 0LL;
	  break;
	case 3: /* CMGEZ(vector) *//*0Q101110--100000100010*/
	  switch (rob->size) {
	  case 0: /* 8B/16B */
	    if (!rob->dbl) {
	      for (i=0; i<8; i++) D0[0].b[i] = (char)S1[0].b[i]>=0 ? 0xff:0x00;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<8; i++) D0[0].b[i] = (char)S1[0].b[i]>=0 ? 0xff:0x00;
	      for (i=0; i<8; i++) D0[1].b[i] = (char)S1[1].b[i]>=0 ? 0xff:0x00;
	    }
	    break;
	  case 1: /* 4H/8H */
	    if (!rob->dbl) {
	      for (i=0; i<4; i++) D0[0].h[i] = (short)S1[0].h[i]>=0 ? 0xffff:0x0000;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<4; i++) D0[0].h[i] = (short)S1[0].h[i]>=0 ? 0xffff:0x0000;
	      for (i=0; i<4; i++) D0[1].h[i] = (short)S1[1].h[i]>=0 ? 0xffff:0x0000;
	    }
	    break;
	  case 2: /* 2S/4S */
	    if (!rob->dbl) {
	      for (i=0; i<2; i++) D0[0].w[i] = (int)S1[0].w[i]>=0 ? 0xffffffff:0x00000000;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<2; i++) D0[0].w[i] = (int)S1[0].w[i]>=0 ? 0xffffffff:0x00000000;
	      for (i=0; i<2; i++) D0[1].w[i] = (int)S1[1].w[i]>=0 ? 0xffffffff:0x00000000;
	    }
	    break;
	  case 3: /* 2D */
	    if (!rob->dbl)
	      return (ROB_EXECERR); /* error */
	    for (i=0; i<1; i++) D0[0].x = (Sll)S1[0].x>=0LL ? 0xffffffffffffffffLL:0LL;
	    for (i=0; i<1; i++) D0[1].x = (Sll)S1[1].x>=0LL ? 0xffffffffffffffffLL:0LL;
	    break;
	  default: /* undef */
	    return (ROB_EXECERR); /* error */
          }
	  break;
	case 4: /* CMEQZ(scalar) *//*01011110--100000100110*/
	  if (rob->size != 3)
	    return (ROB_EXECERR); /* error */
	  D0[0].x = S1[0].x==0LL ? 0xffffffffffffffffLL:0LL;
	  D0[1].x = 0LL;
	  break;
	case 5: /* CMEQZ(vector) *//*0Q001110--100000100110*/
	  switch (rob->size) {
	  case 0: /* 8B/16B */
	    if (!rob->dbl) {
	      for (i=0; i<8; i++) D0[0].b[i] = S1[0].b[i]==0 ? 0xff:0x00;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<8; i++) D0[0].b[i] = S1[0].b[i]==0 ? 0xff:0x00;
	      for (i=0; i<8; i++) D0[1].b[i] = S1[1].b[i]==0 ? 0xff:0x00;
	    }
	    break;
	  case 1: /* 4H/8H */
	    if (!rob->dbl) {
	      for (i=0; i<4; i++) D0[0].h[i] = S1[0].h[i]==0 ? 0xffff:0x0000;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<4; i++) D0[0].h[i] = S1[0].h[i]==0 ? 0xffff:0x0000;
	      for (i=0; i<4; i++) D0[1].h[i] = S1[1].h[i]==0 ? 0xffff:0x0000;
	    }
	    break;
	  case 2: /* 2S/4S */
	    if (!rob->dbl) {
	      for (i=0; i<2; i++) D0[0].w[i] = S1[0].w[i]==0 ? 0xffffffff:0x00000000;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<2; i++) D0[0].w[i] = S1[0].w[i]==0 ? 0xffffffff:0x00000000;
	      for (i=0; i<2; i++) D0[1].w[i] = S1[1].w[i]==0 ? 0xffffffff:0x00000000;
	    }
	    break;
	  case 3: /* 2D */
	    if (!rob->dbl)
	      return (ROB_EXECERR); /* error */
	    for (i=0; i<1; i++) D0[0].x = S1[0].x==0LL ? 0xffffffffffffffffLL:0LL;
	    for (i=0; i<1; i++) D0[1].x = S1[1].x==0LL ? 0xffffffffffffffffLL:0LL;
	    break;
	  default: /* undef */
	    return (ROB_EXECERR); /* error */
          }
	  break;
	case 6: /* CMLEZ(scalar) *//*01111110--100000100110*/
	  if (rob->size != 3)
	    return (ROB_EXECERR); /* error */
	  D0[0].x = (Sll)S1[0].x<=0LL ? 0xffffffffffffffffLL:0LL;
	  D0[1].x = 0LL;
	  break;
	case 7: /* CMLEZ(vector) *//*0Q101110--100000100110*/
	  switch (rob->size) {
	  case 0: /* 8B/16B */
	    if (!rob->dbl) {
	      for (i=0; i<8; i++) D0[0].b[i] = (char)S1[0].b[i]<=0 ? 0xff:0x00;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<8; i++) D0[0].b[i] = (char)S1[0].b[i]<=0 ? 0xff:0x00;
	      for (i=0; i<8; i++) D0[1].b[i] = (char)S1[1].b[i]<=0 ? 0xff:0x00;
	    }
	    break;
	  case 1: /* 4H/8H */
	    if (!rob->dbl) {
	      for (i=0; i<4; i++) D0[0].h[i] = (short)S1[0].h[i]<=0 ? 0xffff:0x0000;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<4; i++) D0[0].h[i] = (short)S1[0].h[i]<=0 ? 0xffff:0x0000;
	      for (i=0; i<4; i++) D0[1].h[i] = (short)S1[1].h[i]<=0 ? 0xffff:0x0000;
	    }
	    break;
	  case 2: /* 2S/4S */
	    if (!rob->dbl) {
	      for (i=0; i<2; i++) D0[0].w[i] = (int)S1[0].w[i]<=0 ? 0xffffffff:0x00000000;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<2; i++) D0[0].w[i] = (int)S1[0].w[i]<=0 ? 0xffffffff:0x00000000;
	      for (i=0; i<2; i++) D0[1].w[i] = (int)S1[1].w[i]<=0 ? 0xffffffff:0x00000000;
	    }
	    break;
	  case 3: /* 2D */
	    if (!rob->dbl)
	      return (ROB_EXECERR); /* error */
	    for (i=0; i<1; i++) D0[0].x = (Sll)S1[0].x<=0LL ? 0xffffffffffffffffLL:0LL;
	    for (i=0; i<1; i++) D0[1].x = (Sll)S1[1].x<=0LL ? 0xffffffffffffffffLL:0LL;
	    break;
	  default: /* undef */
	    return (ROB_EXECERR); /* error */
          }
	  break;
	case 8: /* CMLTZ(scalar) *//*01011110--100000101010*/
	  if (rob->size != 3)
	    return (ROB_EXECERR); /* error */
	  D0[0].x = (Sll)S1[0].x<0LL ? 0xffffffffffffffffLL:0LL;
	  D0[1].x = 0LL;
	  break;
	case 9: /* CMLTZ(vector) *//*0Q001110--100000101010*/
	  switch (rob->size) {
	  case 0: /* 8B/16B */
	    if (!rob->dbl) {
	      for (i=0; i<8; i++) D0[0].b[i] = (char)S1[0].b[i]<0 ? 0xff:0x00;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<8; i++) D0[0].b[i] = (char)S1[0].b[i]<0 ? 0xff:0x00;
	      for (i=0; i<8; i++) D0[1].b[i] = (char)S1[1].b[i]<0 ? 0xff:0x00;
	    }
	    break;
	  case 1: /* 4H/8H */
	    if (!rob->dbl) {
	      for (i=0; i<4; i++) D0[0].h[i] = (short)S1[0].h[i]<0 ? 0xffff:0x0000;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<4; i++) D0[0].h[i] = (short)S1[0].h[i]<0 ? 0xffff:0x0000;
	      for (i=0; i<4; i++) D0[1].h[i] = (short)S1[1].h[i]<0 ? 0xffff:0x0000;
	    }
	    break;
	  case 2: /* 2S/4S */
	    if (!rob->dbl) {
	      for (i=0; i<2; i++) D0[0].w[i] = (int)S1[0].w[i]<0 ? 0xffffffff:0x00000000;
	      D0[1].x = 0LL;
	    }
	    else {
	      for (i=0; i<2; i++) D0[0].w[i] = (int)S1[0].w[i]<0 ? 0xffffffff:0x00000000;
	      for (i=0; i<2; i++) D0[1].w[i] = (int)S1[1].w[i]<0 ? 0xffffffff:0x00000000;
	    }
	    break;
	  case 3: /* 2D */
	    if (!rob->dbl)
	      return (ROB_EXECERR); /* error */
	    for (i=0; i<1; i++) D0[0].x = (Sll)S1[0].x<0LL ? 0xffffffffffffffffLL:0LL;
	    for (i=0; i<1; i++) D0[1].x = (Sll)S1[1].x<0LL ? 0xffffffffffffffffLL:0LL;
	    break;
	  default: /* undef */
	    return (ROB_EXECERR); /* error */
          }
	  break;
	default: /* not used */
	  return (ROB_EXECERR); /* error */
	}
      }
      break;
    case 6: /* MOVI/MVNI/ORR/BIC/BIF/BIT/BSL/EOR */
      switch (rob->sop) {
      case 0: /* MOVI/MVNI */
	if (!rob->iinv) { /* MOVI */
	  D0[0].x =            S2[0].x;
	  D0[1].x = (rob->dbl)?S2[0].x:0x0000000000000000LL;
	}
	else { /* MVNI */
	  D0[0].x =            ~S2[0].x;
	  D0[1].x = (rob->dbl)?~S2[0].x:0x0000000000000000LL;
	}
	break;
      case 1: /* OR */
	if (!rob->iinv) { /* ORR */
	  D0[0].x =             S1[0].x|S2[0].x;
	  D0[1].x = (rob->dbl)?(S1[1].x|S2[0].x):0x0000000000000000LL;
	}
	else { /* ORN */
	  D0[0].x =             S1[0].x|~S2[0].x;
	  D0[1].x = (rob->dbl)?(S1[1].x|~S2[0].x):0x0000000000000000LL;
	}
	break;
      case 2: /* AND */
	if (!rob->iinv) { /* AND */
	  D0[0].x =             S1[0].x&S2[0].x;
	  D0[1].x = (rob->dbl)?(S1[1].x&S2[0].x):0x0000000000000000LL;
	}
	else { /* BIC */
	  D0[0].x =             S1[0].x&~S2[0].x;
	  D0[1].x = (rob->dbl)?(S1[1].x&~S2[0].x):0x0000000000000000LL;
	}
	break;
      case 4: /* EOR */
	if (!rob->dbl) {
	  D0[0].x = S3[0].x ^ ((0LL ^ S2[0].x) & 0xffffffffffffffffLL);
	  D0[1].x = 0LL;
	}
	else {
	  D0[0].x = S3[0].x ^ ((0LL ^ S2[0].x) & 0xffffffffffffffffLL);
	  D0[1].x = S3[1].x ^ ((0LL ^ S2[1].x) & 0xffffffffffffffffLL);
	}
	break;
      case 5: /* BSL */
	if (!rob->dbl) {
	  D0[0].x = S3[0].x ^ ((S3[0].x ^ S2[0].x) & S1[0].x);
	  D0[1].x = 0LL;
	}
	else {
	  D0[0].x = S3[0].x ^ ((S3[0].x ^ S2[0].x) & S1[0].x);
	  D0[1].x = S3[1].x ^ ((S3[1].x ^ S2[1].x) & S1[1].x);
	}
	break;
      case 6: /* BIT */
	if (!rob->dbl) {
	  D0[0].x = S1[0].x ^ ((S1[0].x ^ S2[0].x) & S3[0].x);
	  D0[1].x = 0LL;
	}
	else {
	  D0[0].x = S1[0].x ^ ((S1[0].x ^ S2[0].x) & S3[0].x);
	  D0[1].x = S1[1].x ^ ((S1[1].x ^ S2[1].x) & S3[1].x);
	}
	break;
      case 7: /* BIF */
	if (!rob->dbl) {
	  D0[0].x = S1[0].x ^ ((S1[0].x ^ S2[0].x) & ~S3[0].x);
	  D0[1].x = 0LL;
	}
	else {
	  D0[0].x = S1[0].x ^ ((S1[0].x ^ S2[0].x) & ~S3[0].x);
	  D0[1].x = S1[1].x ^ ((S1[1].x ^ S2[1].x) & ~S3[1].x);
	}
	break;
      default:
	return (ROB_EXECERR); /* error */
      }
      break;
    case 7: /* DUP(vector), XTN, UMOV, INS, MAXMIN */
      switch (rob->sop) {
      case 0: /* DUP */
	index = rob->idx; /* 15-0 */
	fill = (index>=8)?S1[1].x>>((index-8)*8):S1[0].x>>(index*8);
	switch (rob->size) {
	case 1: /* 1B */
	  fill &= 0x00000000000000ffLL;
	  for (i=0; i<8; i++) D0[0].b[i] = fill;
	  if (rob->dbl)
	    for (i=0; i<8; i++) D0[1].b[i] = fill;
	  else
	    D0[1].x = 0x0000000000000000LL;
	  break;
	case 2: /* 2B */
	  fill &= 0x000000000000ffffLL;
	  for (i=0; i<4; i++) D0[0].h[i] = fill;
	  if (rob->dbl)
	    for (i=0; i<4; i++) D0[1].h[i] = fill;
	  else
	    D0[1].x = 0x0000000000000000LL;
	  break;
	case 4: /* 4B */
	  fill &= 0x00000000ffffffffLL;
	  for (i=0; i<2; i++) D0[0].w[i] = fill;
	  if (rob->dbl)
	    for (i=0; i<2; i++) D0[1].w[i] = fill;
	  else
	    D0[1].x = 0x0000000000000000LL;
	  break;
	case 8: /* 8B */
	  fill &= 0xffffffffffffffffLL;
	  D0[0].x = fill;
	  if (rob->dbl)
	    D0[1].x = fill;
	  else
	    D0[1].x = 0x0000000000000000LL;
	  break;
	}
	break;
      case 1: /* XTN,XTN2 */
	switch (rob->size) {
	case 0: /* 8B/16B */
	  if (!rob->dbl) {
	    for (i=0; i<4; i++) D0[0].b[i  ] = S1[0].h[i];
	    for (i=0; i<4; i++) D0[0].b[i+4] = S1[1].h[i];
	    D0[1].x = S1[1].x;
	  }
	  else {
	    D0[0].x = S1[0].x;
	    for (i=0; i<4; i++) D0[1].b[i  ] = S1[0].h[i];
	    for (i=0; i<4; i++) D0[1].b[i+4] = S1[1].h[i];
	  }
	  break;
	case 1: /* 4H/8H */
	  if (!rob->dbl) {
	    for (i=0; i<2; i++) D0[0].h[i  ] = S1[0].w[i];
	    for (i=0; i<2; i++) D0[0].h[i+2] = S1[1].w[i];
	    D0[1].x = S1[1].x;
	  }
	  else {
	    D0[0].x = S1[0].x;
	    for (i=0; i<2; i++) D0[1].h[i  ] = S1[0].w[i];
	    for (i=0; i<2; i++) D0[1].h[i+2] = S1[1].w[i];
	  }
	  break;
	case 2: /* 2S/4S */
	  if (!rob->dbl) {
	    for (i=0; i<1; i++) D0[0].w[i  ] = S1[0].x;
	    for (i=0; i<1; i++) D0[0].w[i+1] = S1[1].x;
	    D0[1].x = S1[1].x;
	  }
	  else {
	    D0[0].x = S1[0].x;
	    for (i=0; i<1; i++) D0[1].w[i  ] = S1[0].x;
	    for (i=0; i<1; i++) D0[1].w[i+1] = S1[1].x;
	  }
	  break;
	default: /* undef */
	  return (ROB_EXECERR); /* error */
	}
	break;
      case 2: /* UMOV */
	index = rob->idx; /* 15-0 */
	fill = (index>=8)?S1[1].x>>((index-8)*8):S1[0].x>>(index*8);
	switch (rob->size) {
	case 1: /* 1B */
	  fill &= 0x00000000000000ffLL;
	  break;
	case 2: /* 1H */
	  fill &= 0x000000000000ffffLL;
	  break;
	case 4: /* 1S */
	  fill &= 0x00000000ffffffffLL;
	  break;
	case 8: /* 1D */
	  fill &= 0xffffffffffffffffLL;
	  break;
	}
	if (rob->dbl)
	  D0[0].x = fill;
	else
	  D0[0].x = fill & 0x00000000ffffffffLL;
	break;
      case 3: /* INS */
	index = rob->idx; /* 15-0 */
	switch (rob->size) {
	case 1: /* 1B */
	  if (index<8) {
	    D0[0].x = (S1[0].x & ~(0x00000000000000ffLL<<(index  )*8)) | (S2[0].b[0]<<(index  )*8);
	    D0[1].x =  S1[1].x;
	  }
	  else {
	    D0[0].x =  S1[0].x;
	    D0[1].x = (S1[1].x & ~(0x00000000000000ffLL<<(index-8)*8)) | (S2[0].b[0]<<(index-8)*8);
	  }
	  break;
	case 2: /* 1H */
	  if (index<8) {
	    D0[0].x = (S1[0].x & ~(0x000000000000ffffLL<<(index  )*8)) | (S2[0].h[0]<<(index  )*8);
	    D0[1].x =  S1[1].x;
	  }
	  else {
	    D0[0].x =  S1[0].x;
	    D0[1].x = (S1[1].x & ~(0x000000000000ffffLL<<(index-8)*8)) | (S2[0].h[0]<<(index-8)*8);
	  }
	  break;
	case 4: /* 1S */
	  if (index<8) {
	    D0[0].x = (S1[0].x & ~(0x00000000ffffffffLL<<(index  )*8)) | (S2[0].w[0]<<(index  )*8);
	    D0[1].x =  S1[1].x;
	  }
	  else {
	    D0[0].x =  S1[0].x;
	    D0[1].x = (S1[1].x & ~(0x00000000ffffffffLL<<(index-8)*8)) | (S2[0].w[0]<<(index-8)*8);
	  }
	  break;
	case 8: /* 1D */
	  if (index<8) {
	    D0[0].x = S2[0].x;
	    D0[1].x = S1[1].x;
	  }
	  else {
	    D0[0].x = S1[0].x;
	    D0[1].x = S2[0].x;
	  }
	  break;
	}
	break;
      case 4: /* SMAX */
	switch (rob->size) {
	case 0: /* 8B/16B */
	  if (!rob->dbl) {
	    for (i=0; i<8; i++) D0[0].b[i] = (char)S1[0].b[i]>(char)S2[0].b[i] ? S1[0].b[i]:S2[0].b[i];
	    D0[1].x = 0LL;
	  }
	  else {
	    for (i=0; i<8; i++) D0[0].b[i] = (char)S1[0].b[i]>(char)S2[0].b[i] ? S1[0].b[i]:S2[0].b[i];
	    for (i=0; i<8; i++) D0[1].b[i] = (char)S1[1].b[i]>(char)S2[1].b[i] ? S1[1].b[i]:S2[1].b[i];
	  }
	  break;
	case 1: /* 4H/8H */
	  if (!rob->dbl) {
	    for (i=0; i<4; i++) D0[0].h[i] = (short)S1[0].h[i]>(short)S2[0].h[i] ? S1[0].h[i]:S2[0].h[i];
	    D0[1].x = 0LL;
	  }
	  else {
	    for (i=0; i<4; i++) D0[0].h[i] = (short)S1[0].h[i]>(short)S2[0].h[i] ? S1[0].h[i]:S2[0].h[i];
	    for (i=0; i<4; i++) D0[1].h[i] = (short)S1[1].h[i]>(short)S2[1].h[i] ? S1[1].h[i]:S2[1].h[i];
	  }
	  break;
	case 2: /* 2S/4S */
	  if (!rob->dbl) {
	    for (i=0; i<2; i++) D0[0].w[i] = (int)S1[0].w[i]>(int)S2[0].w[i] ? S1[0].w[i]:S2[0].w[i];
	    D0[1].x = 0LL;
	  }
	  else {
	    for (i=0; i<2; i++) D0[0].w[i] = (int)S1[0].w[i]>(int)S2[0].w[i] ? S1[0].w[i]:S2[0].w[i];
	    for (i=0; i<2; i++) D0[1].w[i] = (int)S1[1].w[i]>(int)S2[1].w[i] ? S1[1].w[i]:S2[1].w[i];
	  }
	  break;
	default: /* undef */
	  return (ROB_EXECERR); /* error */
	}
	break;
      case 5: /* UMAX */
	switch (rob->size) {
	case 0: /* 8B/16B */
	  if (!rob->dbl) {
	    for (i=0; i<8; i++) D0[0].b[i] = S1[0].b[i]>S2[0].b[i] ? S1[0].b[i]:S2[0].b[i];
	    D0[1].x = 0LL;
	  }
	  else {
	    for (i=0; i<8; i++) D0[0].b[i] = S1[0].b[i]>S2[0].b[i] ? S1[0].b[i]:S2[0].b[i];
	    for (i=0; i<8; i++) D0[1].b[i] = S1[1].b[i]>S2[1].b[i] ? S1[1].b[i]:S2[1].b[i];
	  }
	  break;
	case 1: /* 4H/8H */
	  if (!rob->dbl) {
	    for (i=0; i<4; i++) D0[0].h[i] = S1[0].h[i]>S2[0].h[i] ? S1[0].h[i]:S2[0].h[i];
	    D0[1].x = 0LL;
	  }
	  else {
	    for (i=0; i<4; i++) D0[0].h[i] = S1[0].h[i]>S2[0].h[i] ? S1[0].h[i]:S2[0].h[i];
	    for (i=0; i<4; i++) D0[1].h[i] = S1[1].h[i]>S2[1].h[i] ? S1[1].h[i]:S2[1].h[i];
	  }
	  break;
	case 2: /* 2S/4S */
	  if (!rob->dbl) {
	    for (i=0; i<2; i++) D0[0].w[i] = S1[0].w[i]>S2[0].w[i] ? S1[0].w[i]:S2[0].w[i];
	    D0[1].x = 0LL;
	  }
	  else {
	    for (i=0; i<2; i++) D0[0].w[i] = S1[0].w[i]>S2[0].w[i] ? S1[0].w[i]:S2[0].w[i];
	    for (i=0; i<2; i++) D0[1].w[i] = S1[1].w[i]>S2[1].w[i] ? S1[1].w[i]:S2[1].w[i];
	  }
	  break;
	default: /* undef */
	  return (ROB_EXECERR); /* error */
	}
	break;
      case 6: /* SMIN */
	switch (rob->size) {
	case 0: /* 8B/16B */
	  if (!rob->dbl) {
	    for (i=0; i<8; i++) D0[0].b[i] = (char)S1[0].b[i]<(char)S2[0].b[i] ? S1[0].b[i]:S2[0].b[i];
	    D0[1].x = 0LL;
	  }
	  else {
	    for (i=0; i<8; i++) D0[0].b[i] = (char)S1[0].b[i]<(char)S2[0].b[i] ? S1[0].b[i]:S2[0].b[i];
	    for (i=0; i<8; i++) D0[1].b[i] = (char)S1[1].b[i]<(char)S2[1].b[i] ? S1[1].b[i]:S2[1].b[i];
	  }
	  break;
	case 1: /* 4H/8H */
	  if (!rob->dbl) {
	    for (i=0; i<4; i++) D0[0].h[i] = (short)S1[0].h[i]<(short)S2[0].h[i] ? S1[0].h[i]:S2[0].h[i];
	    D0[1].x = 0LL;
	  }
	  else {
	    for (i=0; i<4; i++) D0[0].h[i] = (short)S1[0].h[i]<(short)S2[0].h[i] ? S1[0].h[i]:S2[0].h[i];
	    for (i=0; i<4; i++) D0[1].h[i] = (short)S1[1].h[i]<(short)S2[1].h[i] ? S1[1].h[i]:S2[1].h[i];
	  }
	  break;
	case 2: /* 2S/4S */
	  if (!rob->dbl) {
	    for (i=0; i<2; i++) D0[0].w[i] = (int)S1[0].w[i]<(int)S2[0].w[i] ? S1[0].w[i]:S2[0].w[i];
	    D0[1].x = 0LL;
	  }
	  else {
	    for (i=0; i<2; i++) D0[0].w[i] = (int)S1[0].w[i]<(int)S2[0].w[i] ? S1[0].w[i]:S2[0].w[i];
	    for (i=0; i<2; i++) D0[1].w[i] = (int)S1[1].w[i]<(int)S2[1].w[i] ? S1[1].w[i]:S2[1].w[i];
	  }
	  break;
	default: /* undef */
	  return (ROB_EXECERR); /* error */
	}
	break;
      case 7: /* UMIN */
	switch (rob->size) {
	case 0: /* 8B/16B */
	  if (!rob->dbl) {
	    for (i=0; i<8; i++) D0[0].b[i] = S1[0].b[i]<S2[0].b[i] ? S1[0].b[i]:S2[0].b[i];
	    D0[1].x = 0LL;
	  }
	  else {
	    for (i=0; i<8; i++) D0[0].b[i] = S1[0].b[i]<S2[0].b[i] ? S1[0].b[i]:S2[0].b[i];
	    for (i=0; i<8; i++) D0[1].b[i] = S1[1].b[i]<S2[1].b[i] ? S1[1].b[i]:S2[1].b[i];
	  }
	  break;
	case 1: /* 4H/8H */
	  if (!rob->dbl) {
	    for (i=0; i<4; i++) D0[0].h[i] = S1[0].h[i]<S2[0].h[i] ? S1[0].h[i]:S2[0].h[i];
	    D0[1].x = 0LL;
	  }
	  else {
	    for (i=0; i<4; i++) D0[0].h[i] = S1[0].h[i]<S2[0].h[i] ? S1[0].h[i]:S2[0].h[i];
	    for (i=0; i<4; i++) D0[1].h[i] = S1[1].h[i]<S2[1].h[i] ? S1[1].h[i]:S2[1].h[i];
	  }
	  break;
	case 2: /* 2S/4S */
	  if (!rob->dbl) {
	    for (i=0; i<2; i++) D0[0].w[i] = S1[0].w[i]<S2[0].w[i] ? S1[0].w[i]:S2[0].w[i];
	    D0[1].x = 0LL;
	  }
	  else {
	    for (i=0; i<2; i++) D0[0].w[i] = S1[0].w[i]<S2[0].w[i] ? S1[0].w[i]:S2[0].w[i];
	    for (i=0; i<2; i++) D0[1].w[i] = S1[1].w[i]<S2[1].w[i] ? S1[1].w[i]:S2[1].w[i];
	  }
	  break;
	default: /* undef */
	  return (ROB_EXECERR); /* error */
	}
	break;
      default:
	return (ROB_EXECERR); /* error */
      }
      break;
    case 8: /* FADD/FSUB(vector) */
      if (rob->iinv==0) { /* FADD */
	switch (rob->sop) {
	case 0: /* 2S/4S */
	  for (i=0; i<2; i++) D0[0].s[i] = S1[0].s[i] + S2[0].s[i];
	  if (rob->dbl)
	    for (i=0; i<2; i++) D0[1].s[i] = S1[1].s[i] + S2[1].s[i];
	  else
	    D0[1].x = 0x0000000000000000LL;
	  break;
	case 1: /* 2D */
	  D0[0].d = S1[0].d + S2[0].d;
	  if (rob->dbl)
	    D0[1].d = S1[1].d + S2[1].d;
	  else
	    return (ROB_EXECERR); /* error */
	  break;
	case 2: /* pair-2S/4S */
	  D0[0].s[0] = S1[0].s[0] + S1[0].s[1];
	  D0[0].s[1] = S1[1].s[0] + S1[1].s[1];
	  if (rob->dbl) {
	    D0[1].s[0] = S2[0].s[0] + S2[0].s[1];
	    D0[1].s[1] = S2[1].s[0] + S2[1].s[1];
	  }
	  else
	    D0[1].x = 0x0000000000000000LL;
	  break;
	case 3: /* pair-2D */
	  D0[0].d = S1[0].d + S1[1].d;
	  if (rob->dbl)
	    D0[1].d = S2[0].d + S2[1].d;
	  else
	    return (ROB_EXECERR); /* error */
	  break;
	default:
	  return (ROB_EXECERR); /* error */
	}
      }
      else { /* FSUB */
	switch (rob->sop) {
	case 0: /* 2S/4S */
	  for (i=0; i<2; i++) D0[0].s[i] = S1[0].s[i] - S2[0].s[i];
	  if (rob->dbl)
	    for (i=0; i<2; i++) D0[1].s[i] = S1[1].s[i] - S2[1].s[i];
	  else
	    D0[1].x = 0x0000000000000000LL;
	  break;
	case 1: /* 2D */
	  D0[0].d = S1[0].d - S2[0].d;
	  if (rob->dbl)
	    D0[1].d = S1[1].d - S2[1].d;
	  else
	    return (ROB_EXECERR); /* error */
	  break;
	case 2: /* pair-2S/4S */
	  D0[0].s[0] = S1[0].s[0] - S1[0].s[1];
	  D0[0].s[1] = S1[1].s[0] - S1[1].s[1];
	  if (rob->dbl) {
	    D0[1].s[0] = S2[0].s[0] - S2[0].s[1];
	    D0[1].s[1] = S2[1].s[0] - S2[1].s[1];
	  }
	  else
	    D0[1].x = 0x0000000000000000LL;
	  break;
	case 3: /* pair-2D */
	  D0[0].d = S1[0].d - S1[1].d;
	  if (rob->dbl)
	    D0[1].d = S2[0].d - S2[1].d;
	  else
	    return (ROB_EXECERR); /* error */
	  break;
	default:
	  return (ROB_EXECERR); /* error */
	}
      }
      break;
    case 9: /* FMLA/FMLS(vector) */
      if (rob->iinv==0) { /* FMLA */
	switch (rob->sop) {
	case 0: /* 2S/4S */
	  for (i=0; i<2; i++) D0[0].s[i] = S3[0].s[i] + S1[0].s[i] * S2[0].s[i];
	  if (rob->dbl)
	    for (i=0; i<2; i++) D0[1].s[i] = S3[1].s[i] + S1[1].s[i] * S2[1].s[i];
	  else
	    D0[1].x = 0x0000000000000000LL;
	  break;
	case 1: /* 2D */
	  D0[0].d = S3[0].d + S1[0].d * S2[0].d;
	  if (rob->dbl)
	    D0[1].d = S3[1].d + S1[1].d * S2[1].d;
	  else
	    return (ROB_EXECERR); /* error */
	  break;
	default:
	  return (ROB_EXECERR); /* error */
	}
      }
      else { /* FMLS */
	switch (rob->sop) {
	case 0: /* 2S/4S */
	  for (i=0; i<2; i++) D0[0].s[i] = S3[0].s[i] - S1[0].s[i] * S2[0].s[i];
	  if (rob->dbl)
	    for (i=0; i<2; i++) D0[1].s[i] = S3[1].s[i] - S1[1].s[i] * S2[1].s[i];
	  else
	    D0[1].x = 0x0000000000000000LL;
	  break;
	case 1: /* 2D */
	  D0[0].d = S3[0].d - S1[0].d * S2[0].d;
	  if (rob->dbl)
	    D0[1].d = S3[1].d - S1[1].d * S2[1].d;
	  else
	    return (ROB_EXECERR); /* error */
	  break;
	default:
	  return (ROB_EXECERR); /* error */
	}
      }
      break;
    case 10: /* FMUL(vector) */
      switch (rob->sop) {
      case 0: /* 2S/4S */
	for (i=0; i<2; i++) D0[0].s[i] = S1[0].s[i] * S2[0].s[i];
	if (rob->dbl)
	  for (i=0; i<2; i++) D0[1].s[i] = S1[1].s[i] * S2[1].s[i];
	else
	  D0[1].x = 0x0000000000000000LL;
	break;
      case 1: /* 2D */
	D0[0].d = S1[0].d * S2[0].d;
	if (rob->dbl)
	  D0[1].d = S1[1].d * S2[1].d;
	else
	  return (ROB_EXECERR); /* error */
	break;
      default:
	return (ROB_EXECERR); /* error */
      }
      break;
    case 11: /* MLA/MLS(vector) */
      if (rob->iinv==0) { /* MLA */
	switch (rob->sop) {
	case 0: /* 8B/16B */
	  for (i=0; i<8; i++) {
	    D0[0].b[i] =           S3[0].b[i] + S1[0].b[i] * (rob->sr[1].t?S2[0].b[i]:1);
	    D0[1].b[i] = rob->dbl?(S3[1].b[i] + S1[1].b[i] * (rob->sr[1].t?S2[1].b[i]:1)):0;
	  }
	  break;
	case 1: /* 4H/8H */
	  for (i=0; i<4; i++) {
	    D0[0].h[i] =           S3[0].h[i] + S1[0].h[i] * (rob->sr[1].t?S2[0].h[i]:1);
	    D0[1].h[i] = rob->dbl?(S3[1].h[i] + S1[1].h[i] * (rob->sr[1].t?S2[1].h[i]:1)):0;
	  }
	  break;
	case 2: /* 2S/4S */
	  for (i=0; i<2; i++) {
	    D0[0].w[i] =           S3[0].w[i] + S1[0].w[i] * (rob->sr[1].t?S2[0].w[i]:1);
	    D0[1].w[i] = rob->dbl?(S3[1].w[i] + S1[1].w[i] * (rob->sr[1].t?S2[1].w[i]:1)):0;
	  }
	  break;
	case 3: /* 1D for ADD(scalar)/2D for ADD(vector) */
  	    D0[0].x    =           S3[0].x    + S1[0].x    * (rob->sr[1].t?S2[0].x:1LL);
	    D0[1].x    = rob->dbl?(S3[1].x    + S1[1].x    * (rob->sr[1].t?S2[1].x:1LL)):0LL;
	  break;
	default:
	  return (ROB_EXECERR); /* error */
	}
      }
      else { /* MLS */
	switch (rob->sop) {
	case 0: /* 8B/16B */
	  for (i=0; i<8; i++) {
	    D0[0].b[i] =           S3[0].b[i] - S1[0].b[i] * (rob->sr[1].t?S2[0].b[i]:1);
	    D0[1].b[i] = rob->dbl?(S3[1].b[i] - S1[1].b[i] * (rob->sr[1].t?S2[1].b[i]:1)):0;
	  }
	  break;
	case 1: /* 4H/8H */
	  for (i=0; i<4; i++) {
	    D0[0].h[i] =           S3[0].h[i] - S1[0].h[i] * (rob->sr[1].t?S2[0].h[i]:1);
	    D0[1].h[i] = rob->dbl?(S3[1].h[i] - S1[1].h[i] * (rob->sr[1].t?S2[1].h[i]:1)):0;
	  }
	  break;
	case 2: /* 2S/4S */
	  for (i=0; i<2; i++) {
	    D0[0].w[i] =           S3[0].w[i] - S1[0].w[i] * (rob->sr[1].t?S2[0].w[i]:1);
	    D0[1].w[i] = rob->dbl?(S3[1].w[i] - S1[1].w[i] * (rob->sr[1].t?S2[1].w[i]:1)):0;
	  }
	  break;
	case 3: /* 1D for SUB(scalar)/2D for SUB(vector) */
  	    D0[0].x    =           S3[0].x    - S1[0].x    * (rob->sr[1].t?S2[0].x:1LL);
	    D0[1].x    = rob->dbl?(S3[1].x    - S1[1].x    * (rob->sr[1].t?S2[1].x:1LL)):0LL;
	  break;
	default:
	  return (ROB_EXECERR); /* error */
	}
      }
      break;
    case 12: /* FADD/FSUB(scalar) */
      if (rob->iinv==0) { /* FADD */
	if (rob->dbl) {
	  D0[0].d = S1[0].d + S2[0].d;
	  D0[1].x = 0LL;
	}
	else {
	  D0[0].s[0] = S1[0].s[0] + S2[0].s[0];
	  D0[0].w[1] = 0;
	  D0[1].x = 0LL;
	}
      }
      else { /* FSUB/FCMP */
	if (rob->dbl) {
	  D0[0].d = S1[0].d - S2[0].d;
	  D0[1].x = 0LL;
	  /* eq: NZCV=0110 */
	  /* lt: NZCV=1000 */
	  /* gt: NZCV=0010 */
	  /* uo: NZCV=0011 */
	  if      (S1[0].d == S2[0].d) NZCV = 0x6; /* eq */
	  else if (S1[0].d <  S2[0].d) NZCV = 0x8; /* lt */
	  else                         NZCV = 0x2; /* gt */
	}
	else {
	  D0[0].s[0] = S1[0].s[0] - S2[0].s[0];
	  D0[0].w[1] = 0;
	  D0[1].x = 0LL;
	  /* eq: NZCV=0110 */
	  /* lt: NZCV=1000 */
	  /* gt: NZCV=0010 */
	  /* uo: NZCV=0011 */
	  if      (S1[0].s[0] == S2[0].s[0]) NZCV = 0x6; /* eq */
	  else if (S1[0].s[0] <  S2[0].s[0]) NZCV = 0x8; /* lt */
	  else                               NZCV = 0x2; /* gt */
	}
      }
      break;
    case 13: /* FMADD/FMSUB(scalar) */
      if (rob->dbl) {
	if (rob->iinv)
	  S3[0].d = -S3[0].d;
	if (rob->oinv)
	  S1[0].d = -S1[0].d;
	D0[0].d = S3[0].d + S1[0].d * S2[0].d;
	D0[1].x = 0LL;
      }
      else {
	if (rob->iinv)
	  S3[0].s[0] = -S3[0].s[0];
	if (rob->oinv)
	  S1[0].s[0] = -S1[0].s[0];
	D0[0].s[0] = S3[0].s[0] + S1[0].s[0] * S2[0].s[0];
	D0[0].w[1] = 0;
	D0[1].x = 0LL;
      }
      break;
    case 14: /* FMUL(scalar) */
      if (rob->dbl) {
	D0[0].d = S1[0].d * S2[0].d;
	if (rob->oinv) /* 0:normal, 1:negate */
	  D0[0].d = -D0[0].d;
	D0[1].x = 0LL;
      }
      else {
	D0[0].s[0] = S1[0].s[0] * S2[0].s[0];
	if (rob->oinv) /* 0:normal, 1:negate */
	  D0[0].s[0] = -D0[0].s[0];
	D0[0].w[1] = 0;
	D0[1].x = 0LL;
      }
      break;
    default:
      return (ROB_EXECERR); /* error */
    }

    c[cid].vecpipe[0].v = 1;
    c[cid].vecpipe[0].tid = tid;
    c[cid].vecpipe[0].rob = rob;
    if (rob->dr[1].t) {
      c[cid].vecpipe[0].dr1t = 1;
      c[cid].vecpipe[0].dr1v[0] = D0[0].x;
      c[cid].vecpipe[0].dr1v[1] = D0[1].x;
    }
    else
      c[cid].vecpipe[0].dr1t = 0;
    if (rob->dr[3].t) {
      c[cid].vecpipe[0].dr3t = 1;
      c[cid].vecpipe[0].dr3v = NZCV;
    }
    else
      c[cid].vecpipe[0].dr3t = 0;

    return (ROB_ISSUED); /* issued */
  }
 
  else { /* FDIV */
    if (c[cid].divque.v)
      return (ROB_MAPPED); /* no change */

    switch (rob->opcd) {
    case 15: /* FDIV(scalar) */
      if (rob->dbl) {
	D0[0].d = S1[0].d / S2[0].d;
	D0[1].x = 0LL;
      }
      else {
	D0[0].s[0] = S1[0].s[0] / S2[0].s[0];
	D0[0].w[1] = 0;
	D0[1].x = 0LL;
      }
      break;
    default:
      return (ROB_EXECERR); /* error */
    }

    c[cid].divque.v = 1;
    c[cid].divque.t = CORE_DIVDELAY;
    c[cid].divque.tid = tid;
    c[cid].divque.rob = rob;
    if (rob->dr[1].t) {
      c[cid].divque.dr1t = 1;
      c[cid].divque.dr1v = D0[0].x;
    }
    else
      c[cid].divque.dr1t = 0;

    return (ROB_ISSUED); /* issued */
  }
}
