
static char RcsHeader[] = "$Header: /usr/home/nakashim/proj-arm64/src/csim/RCS/reg.c,v 1.45 2017/09/11 15:40:44 nakashim Exp nakashim $";

/* SPARC Simulator                     */
/*         Copyright (C) 2010 by NAIST */
/*         Primary writer: Y.Nakashima */
/*                nakashim@is.naist.jp */

/* reg.c 2010/7/10 */

#include "csim.h"

Uint ex_srr(tid, val1, val0, rob, pos) Uint tid; Ull *val1, *val0; struct rob *rob; Uint pos; /* sr[0-5] */
{
  Uint cid = tid2cid(tid);

  if (pos > 5) {
    printf(":ex_srr.pos=%d internal error", pos);
    *val0 = 0LL;
    *val1 = 0LL;
  }
  else {
    if (rob->sr[pos].t==0) 
      printf(":ex_grr.sr[%d].t=%d internal error", pos, rob->sr[pos].t);
    *val0 = rob->sr[pos].t==0 ? 0 :
            rob->sr[pos].t==1 ? rob->sr[pos].n : /* PC is included here */
            rob->sr[pos].x==0 ? ((rob->sr[pos].n<=USRREGTOP+USRREG-1) ? t[tid].usr[rob->sr[pos].n] :
		  	         (rob->sr[pos].n<=AUXREGTOP+AUXREG-1) ? t[tid].aux[rob->sr[pos].n-AUXREGTOP] :
			         (rob->sr[pos].n==CPSREGTOP+CPSREG-1) ? t[tid].cpsr :
                                 (rob->sr[pos].n< VECREGTOP         ) ? 0 : /* not reached */
			         (rob->sr[pos].n<=VECREGTOP+VECREG-1) ? t[tid].vec[rob->sr[pos].n-VECREGTOP].d[0] : 0) :
                                c[cid].rob[rob->sr[pos].n].dr[rob->sr[pos].x].val[0];
    if (val1) {
      *val1 = rob->sr[pos].t==0 ? 0 :
              rob->sr[pos].t==1 ? 0 :
              rob->sr[pos].x==0 ? ((rob->sr[pos].n< VECREGTOP         ) ? 0 : /* not reached */
	  		           (rob->sr[pos].n<=VECREGTOP+VECREG-1) ? t[tid].vec[rob->sr[pos].n-VECREGTOP].d[1] : 0) :
                                  c[cid].rob[rob->sr[pos].n].dr[rob->sr[pos].x].val[1];
    }
  }
  if (flag&TRACE_ARM) {
    switch (rob->sr[pos].t) {
    case 1:
      printf(":IMM%08.8x_%08.8x", (Uint)(*val0>>32), (Uint)*val0);
      break;
    case 2:
      if (rob->sr[pos].n< VECREGTOP) {
	if (rob->sr[pos].x==0)
	  printf(":R%02.2d->%08.8x_%08.8x", (Uint)rob->sr[pos].n, (Uint)(*val0>>32), (Uint)*val0);
	else
	  printf(":ROB%03.3d.%d->%08.8x_%08.8x", (Uint)rob->sr[pos].n, rob->sr[pos].x, (Uint)(*val0>>32), (Uint)*val0);
      }
      else {
	if (rob->sr[pos].x==0)
	  printf(":V%02.2d->%08.8x_%08.8x_%08.8x_%08.8x", (Uint)rob->sr[pos].n-VECREGTOP, (Uint)(*val1>>32), (Uint)*val1, (Uint)(*val0>>32), (Uint)*val0);
	else
	  printf(":ROB%03.3d.%d->%08.8x_%08.8x_%08.8x_%08.8x", (Uint)rob->sr[pos].n, rob->sr[pos].x, (Uint)(*val1>>32), (Uint)*val1, (Uint)(*val0>>32), (Uint)*val0);
      }
      break;
    }
  }
  
  return (0);
}

Uint ex_drw(tid, val1, val0, rob, pos) Uint tid; Ull val1, val0; struct rob *rob; Uint pos; /* dr[1-3] */
{
  Uint cid = tid2cid(tid);
  Ull tval[2];
  int i, j;

  if (pos > 3) {
    printf(":ex_drw.pos=%d internal error", pos);
  }
  else {
    if (rob->dr[pos].t==0) 
      printf(":ex_drw.dr[%d].t=%d v=%08.8x_%08.8x_%08.8x_%08.8x internal error", pos, rob->dr[pos].t, (Uint)(val1>>32), (Uint)val1, (Uint)(val0>>32), (Uint)val0);
    /* ldの場合,stbfとマージのためmask参照 */
    tval[0] = (rob->dr[pos].val[0] & rob->dr[pos].mask[0]) | (val0 & ~rob->dr[pos].mask[0]);
    tval[1] = (rob->dr[pos].val[1] & rob->dr[pos].mask[1]) | (val1 & ~rob->dr[pos].mask[1]);
    if (rob->ptw && pos==1) { /* partial_reg_write */
      Ull mask[2];
      switch (rob->opcd) { /* 8:VLDRB(8), 9:VlDRH(16), 10:VLDRS(32), 11:VLDRD(64) */
      case 8:  /* VLDRB(8) */
	if (rob->idx >= 8) { /* 15-8 */
	  mask[1] = 0x00000000000000ffLL << ((rob->idx-8)*8);
	  tval[1] = tval[0]              << ((rob->idx-8)*8);
	  mask[0] = 0x0000000000000000LL;
	  tval[0] = 0x0000000000000000LL;
	}
	else {               /* 7-0 */
	  mask[1] = 0x0000000000000000LL;
	  tval[1] = 0x0000000000000000LL;
	  mask[0] = 0x00000000000000ffLL << (rob->idx*8);
	  tval[0] = tval[0]              << (rob->idx*8);
	}
	break;
      case 9:  /* VlDRH(16) */
	if (rob->idx >= 8) { /* 15-8 */
	  mask[1] = 0x000000000000ffffLL << ((rob->idx-8)*8);
	  tval[1] = tval[0]              << ((rob->idx-8)*8);
	  mask[0] = 0x0000000000000000LL;
	  tval[0] = 0x0000000000000000LL;
	}
	else {               /* 7-0 */
	  mask[1] = 0x0000000000000000LL;
	  tval[1] = 0x0000000000000000LL;
	  mask[0] = 0x000000000000ffffLL << (rob->idx*8);
	  tval[0] = tval[0]              << (rob->idx*8);
	}
	break;
      case 10: /* VLDRS(32) */
	if (rob->idx >= 8) { /* 15-8 */
	  mask[1] = 0x00000000ffffffffLL << ((rob->idx-8)*8);
	  tval[1] = tval[0]              << ((rob->idx-8)*8);
	  mask[0] = 0x0000000000000000LL;
	  tval[0] = 0x0000000000000000LL;
	}
	else {               /* 7-0 */
	  mask[1] = 0x0000000000000000LL;
	  tval[1] = 0x0000000000000000LL;
	  mask[0] = 0x00000000ffffffffLL << (rob->idx*8);
	  tval[0] = tval[0]              << (rob->idx*8);
	}
	break;
      case 11: /* VLDRD(64) */
	if (rob->idx >= 8) { /* 15-8 */
	  mask[1] = 0xffffffffffffffffLL;
	  tval[1] = tval[0];
	  mask[0] = 0x0000000000000000LL;
	  tval[0] = 0x0000000000000000LL;
	}
	else {               /* 7-0 */
	  mask[1] = 0x0000000000000000LL;
	  tval[1] = 0x0000000000000000LL;
	  mask[0] = 0xffffffffffffffffLL;
	  tval[0] = tval[0];
	}
	break;
      }
      rob->dr[pos].val[0] = (rob->dr[0].val[0] & ~mask[0]) | (tval[0] & mask[0]);
      rob->dr[pos].val[1] = (rob->dr[0].val[1] & ~mask[1]) | (tval[1] & mask[1]);
    }
    else if (rob->rep && pos==1) { /* repeat in reg */
      switch (rob->opcd) { /* 8:VLDRB(8), 9:VlDRH(16), 10:VLDRS(32), 11:VLDRD(64) */
      case 8:  /* VLDRB(8) */
	if (rob->dbl) {
	  rob->dr[pos].val[0] = (tval[0]<<56)|(tval[0]<<48)|(tval[0]<<40)|(tval[0]<<32)|(tval[0]<<24)|(tval[0]<<16)|(tval[0]<<8)|tval[0];
	  rob->dr[pos].val[1] = (tval[0]<<56)|(tval[0]<<48)|(tval[0]<<40)|(tval[0]<<32)|(tval[0]<<24)|(tval[0]<<16)|(tval[0]<<8)|tval[0];
	}
	else {
	  rob->dr[pos].val[0] = (tval[0]<<56)|(tval[0]<<48)|(tval[0]<<40)|(tval[0]<<32)|(tval[0]<<24)|(tval[0]<<16)|(tval[0]<<8)|tval[0];
	  rob->dr[pos].val[1] = 0LL;
	}
	break;
      case 9:  /* VlDRH(16) */
	if (rob->dbl) {
	  rob->dr[pos].val[0] = (tval[0]<<48)|(tval[0]<<32)|(tval[0]<<16)|tval[0];
	  rob->dr[pos].val[1] = (tval[0]<<48)|(tval[0]<<32)|(tval[0]<<16)|tval[0];
	}
	else {
	  rob->dr[pos].val[0] = (tval[0]<<48)|(tval[0]<<32)|(tval[0]<<16)|tval[0];
	  rob->dr[pos].val[1] = 0LL;
	}
	break;
      case 10: /* VLDRS(32) */
	if (rob->dbl) {
	  rob->dr[pos].val[0] = (tval[0]<<32)|tval[0];
	  rob->dr[pos].val[1] = (tval[0]<<32)|tval[0];
	}
	else {
	  rob->dr[pos].val[0] = (tval[0]<<32)|tval[0];
	  rob->dr[pos].val[1] = 0LL;
	}
	break;
      case 11: /* VLDRD(64) */
	if (rob->dbl) {
	  rob->dr[pos].val[0] = tval[0];
	  rob->dr[pos].val[1] = tval[1];
	}
	else {
	  rob->dr[pos].val[0] = tval[0];
	  rob->dr[pos].val[1] = 0LL;
	}
	break;
      }
    }
    else {
      rob->dr[pos].val[0] = tval[0];
      rob->dr[pos].val[1] = tval[1];
    }
    for (i=rob-&c[cid].rob[0]; i!=c[cid].rob_top; i=(i+1)%CORE_ROBSIZE) {
      for (j=0; j<6; j++) {
	if (c[cid].rob[i].sr[j].t==3 && c[cid].rob[i].sr[j].x == pos && c[cid].rob[i].sr[j].n == rob->dr[pos].p)
	  c[cid].rob[i].sr[j].t = 2; /* ready */
      }
    }
  }
  if (flag&TRACE_ARM) {
    if (rob->dr[pos].t) {
      if (rob->dr[pos].n < VECREGTOP)
	printf(":ROB%03.3d.%d(R%02.2d)<-%08.8x_%08.8x", rob->dr[pos].p, pos, rob->dr[pos].n, (Uint)(rob->dr[pos].val[0]>>32), (Uint)rob->dr[pos].val[0]);
      else
	printf(":ROB%03.3d.%d(V%02.2d)<-%08.8x_%08.8x_%08.8x_%08.8x", rob->dr[pos].p, pos, rob->dr[pos].n-VECREGTOP, (Uint)(rob->dr[pos].val[1]>>32), (Uint)rob->dr[pos].val[1], (Uint)(rob->dr[pos].val[0]>>32), (Uint)rob->dr[pos].val[0]);
    }
  }

  return (0);
}

Uint rt_drw(tid, regno, val) Uint tid, regno; Ull *val;
{
  if (regno >= MAXLREG) {
    printf(":rt_drw.regno=%d internal error", regno);
  }
  else {
    if      (regno <= USRREGTOP+USRREG-1) {t[tid].usr[regno]                = val[0];}
    else if (regno <= AUXREGTOP+AUXREG-1) {t[tid].aux[regno-AUXREGTOP]      = val[0];}
    else if (regno == CPSREGTOP+CPSREG-1) {t[tid].cpsr                      = val[0];}
    else if (regno <  VECREGTOP         ) {} /* non existent */
    else if (regno <= VECREGTOP+VECREG-1) {t[tid].vec[regno-VECREGTOP].d[0] = val[0];
					   t[tid].vec[regno-VECREGTOP].d[1] = val[1];}
    else                                  {} /* non existent */
  }
  if (flag&TRACE_ARM) {
    if (regno < VECREGTOP)
      printf(":R%02.2d<-%08.8x_%08.8x", regno, (Uint)(val[0]>>32), (Uint)val[0]);
    else
      printf(":V%02.2d<-%08.8x_%08.8x_%08.8x_%08.8x", regno-VECREGTOP, (Uint)(val[1]>>32), (Uint)val[1], (Uint)(val[0]>>32), (Uint)val[0]);
  }

  return (0);
}

Ull grr(tid, n) Uint tid, n;
{
  Ull val;

  if (n < 32) val = t[tid].usr[n];
  else        val = t[tid].ib.pc;

  if (flag&TRACE_ARM)
    printf(":R%02.2d->%08.8x_%08.8x", n, (Uint)(val>>32), (Uint)val);
  
  return (val);
}

Uint grw(tid, n, val) Uint tid, n; Ull val;
{
  if (n < 32) t[tid].usr[n] = val;
  else        t[tid].ib.pc = val;

  if (flag&TRACE_ARM)
    printf(":R%02.2d<-%08.8x_%08.8x", n, (Uint)(val>>32), (Uint)val);

  return (0);
}

Uint ccw(tid, val) Uint tid; Ull val;
{
  t[tid].cpsr = val;

  if (flag&TRACE_ARM)
    printf(":CC<-%08.8x_%08.8x", (Uint)(val>>32), (Uint)val);

  return (0);
}

/* I-fetchから呼び出すインタフェース */
int o_ifetch(tid, addr, val, ib) Uint tid, addr; Uint *val; struct ib *ib;
     /* retval=0:normal_end, 1:l1rq enqueued, 2:l1rq busy, 3:error (out of range) */
{
  Uint cid    = tid2cid(tid);
  Uint l1bank = (addr/LINESIZE)%MAXL1BK;
  int  stat, pull_way;

  if ((addr & 3) || addr < MINADDR || ALOCLIMIT <= addr)
    return (3);

  stat = i1r(tid, addr, val, &pull_way); /* I1-cache */

  if (stat == 2) { /* miss */
    if (c[cid].l1rq[l1bank].v_stat)
      return (1);
    c[cid].l1rq[l1bank].rv_l1fil = 1; /* for load */
    c[cid].l1rq[l1bank].t    = I1DELAY;
    c[cid].l1rq[l1bank].tid  = tid;
    c[cid].l1rq[l1bank].type = 3;  /* load */
    c[cid].l1rq[l1bank].opcd = 15; /* I-fetch */
    c[cid].l1rq[l1bank].rq_push = 0;
    c[cid].l1rq[l1bank].push_ADR = 0; /* not used */
    c[cid].l1rq[l1bank].rq_pull = 1;
    c[cid].l1rq[l1bank].pull_ADR = addr;
    c[cid].l1rq[l1bank].pull_way = pull_way;
    c[cid].l1rq[l1bank].STD[0] = 0LL; /* not used */
    c[cid].l1rq[l1bank].STD[1] = 0LL; /* not used */
    c[cid].l1rq[l1bank].ib = ib;
    c[cid].l1rq[l1bank].rob = NULL; /* not used */
    c[cid].l1rq[l1bank].v_stat = (3<<2)|(1); /* valid */
    /* L1:index=10bit,linesize=6bit,L2:bank=2bit,blocksize=12bitなので,L1の同一ラインはL2の同一バンクになる */
    /* このため，push_ADRとpull_ADRは同一バンクとなるので，bitmap更新は1bitだけでよい */
    return (2);
  }
  else
    return (0);
}

/* L1-flushインタフェース */
int o_flush(tid, addr, keep_or_drain) Uint tid, addr, keep_or_drain;
     /* retval 0:normal_end, 1:mem_wait(queue-full), 2:mem_wait(queue-ok), 3:error, 4:flush_end */
     /* flush=0:keep v&dirty, flush=1:clear v&dirty */
{
  Uint cid    = tid2cid(tid);
  Uint l1bank = (addr/LINESIZE)%MAXL1BK;
  int  stat, pull_way, rq_push;
  Uint push_adr;

  stat = d1w(tid, 0, addr, NULL, 1, keep_or_drain, NULL, &pull_way, &rq_push, &push_adr, c[cid].l1rq[l1bank].BUF); /* D1-cache */
  /* DSM領域にL1-dirtyは存在しないのでmode=0でOK */
  if (stat == 2) { /* dirty */
    if (c[cid].l1rq[l1bank].v_stat)
      return (1);
    c[cid].l1rq[l1bank].rv_l1fil = 0;
    c[cid].l1rq[l1bank].t    = D1DELAY;
    c[cid].l1rq[l1bank].tid  = tid;
    c[cid].l1rq[l1bank].type = 0;
    c[cid].l1rq[l1bank].opcd = 0;
    c[cid].l1rq[l1bank].rq_push = 1;
    c[cid].l1rq[l1bank].push_ADR = push_adr;
    c[cid].l1rq[l1bank].rq_pull = 0;
    c[cid].l1rq[l1bank].pull_ADR = 0;
    c[cid].l1rq[l1bank].pull_way = 0;
    c[cid].l1rq[l1bank].STD[0] = 0LL; /* not used */
    c[cid].l1rq[l1bank].STD[1] = 0LL; /* not used */
    c[cid].l1rq[l1bank].ib  = NULL;
    c[cid].l1rq[l1bank].rob  = NULL;
    c[cid].l1rq[l1bank].v_stat = (3<<2)|(1); /* valid */
    /* L1:index=10bit,linesize=6bit,L2:bank=2bit,blocksize=12bitなので,L1の同一ラインはL2の同一バンクになる */
    /* このため，push_ADRとpull_ADRは同一バンクとなるので，bitmap更新は1bitだけでよい */
    if (flag & TRACE_ARM)
      printf("%03.3d:FL %08.8x_%08.8x delayed(miss) queued push_A=%08.8x\n",
	     tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), push_adr);
    return (2);
  }
  else
    return (0);
}

/* LD/ST命令から呼び出すインタフェース */
int o_ldst(tid, type, opcd, addr, mask, rot, val, rob)
     Uint tid, type, opcd, addr; Ull *mask; Uint rot; Ull *val; struct rob *rob;
     /* type=3:load, 4:store */
     /* retval=0:normal_end, 1:l1rq busy, 2:l1rq enqueued, 3:error (out of range) */
{
  Uint cid    = tid2cid(tid);
  Uint l1bank = (addr/LINESIZE)%MAXL1BK;
  Uint push_adr, a, o;
  Ull  m[2], dm[2], dv[2], lv[2];
  int  i, stat, rq_push, pull_way, inhibit_replace;

  if (addr < MINADDR || MEM_VALID_ADDR < addr)
    return (3);

  /***for EMAX6-I/O****/
  if (type == 3 && MEMSIZE <= addr) { /* I/O space */
    /*emax6_ctl(tid, 3, opcd, addr, mask, val); type=3:load */

    /* check preceeding rob.stbf *//*★★★IOLDは,先行(IO)STの完了を待ち合わせる★★★*/
    for (i=c[cid].rob_bot; i!=rob-&c[cid].rob[0]; i=(i+1)%CORE_ROBSIZE) {
      if (c[cid].rob[i].stat == ROB_MAPPED && c[cid].rob[i].type == 4) { /* ROB_MAPPED: rob->ls_addr is unknown .. all succeeding loads should wait */
	if (flag & TRACE_ARM)
	  printf(":(waiting prev_store rob[%d].stat=%d)", i, c[cid].rob[i].stat);
	return (1);
      }
      else if ((c[cid].rob[i].stat == ROB_STREXWAIT || c[cid].rob[i].stat == ROB_COMPLETE || c[cid].rob[i].stat == ROB_D1WRQWAIT) && c[cid].rob[i].tid == tid && c[cid].rob[i].stbf.t) { /* ROB_COMPLETE: rob->stbf.t = 1 */ 
	if (flag & TRACE_ARM)
	  printf(":(waiting prev_store rob[%d].stat=%d)", i, c[cid].rob[i].stat);
	return (1);
      }
    }

    if (c[cid].iorq.v_stat)
      return (1);
    else {
      rob->ls_addr = addr;
      c[cid].iorq.tid  = tid;
      c[cid].iorq.type = type; /* load */
      c[cid].iorq.opcd = opcd; /* OP-fetch */
      c[cid].iorq.ADR = addr;
      c[cid].iorq.BUF[0] = val[0]; /* for store */
      c[cid].iorq.BUF[1] = val[1]; /* for store */
      c[cid].iorq.rob = rob;
      c[cid].iorq.v_stat = (3<<2)|(1); /* valid */
      if (flag & TRACE_ARM)
	printf("%03.3d:IO %08.8x_%08.8x cycle=%08.8x_%08.8x ---EMAX6 IOR type=%x adr=%08.8x----", cid, (Uint)(t[cid].total_steps>>32), (Uint)t[cid].total_steps, (Uint)(t[cid].total_cycle>>32), (Uint)t[cid].total_cycle, c[cid].iorq.type, c[cid].iorq.ADR);
      return (2);
    }
  }
  /***for EMAX6-I/O****/

  /* NON-BLOCKING-CACHE */
  if (c[cid].l1rq[l1bank].v_stat)
    inhibit_replace = 1;
  else
    inhibit_replace = 0;

#if 0
  /* Specualtive execution can produce unaligned error */
  if (( mask[1]                         && (addr & 15)) ||
      ((mask[0] & 0xffffffff00000000LL) && (addr &  7)) ||
      ((mask[0] & 0x00000000ffff0000LL) && (addr &  3)) ||
      ((mask[0] & 0x000000000000ff00LL) && (addr &  1))) {
    if (flag & TRACE_ARM) printf("\n");
    printf("%03.3d:LDST %08.8x_%08.8x %08.8x A=%08.8x M=%08.8x_%08.8x_%08.8x_%08.8x =unaligned=",
	   tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps),
	   rob->pc, addr, (Uint)(mask[1]>>32), (Uint)mask[1], (Uint)(mask[0]>>32), (Uint)mask[0]);
    if (!(flag & TRACE_ARM)) printf("\n");
  }
#endif

  switch (type) {
  case 3: /* load */
    a  = addr & ~15;
    o  = ((addr&8)!=0); /* 0:even, 1:odd */
    m[o]   = mask[0]<<((addr&7)<<3);
    m[1-o] = mask[1]; /* if odd, mask[1] is all-0 */
    dm[0] = 0x0000000000000000LL; /* valid mask from prev stbf */
    dm[1] = 0x0000000000000000LL; /* valid mask from prev stbf */
    dv[0] = 0x0000000000000000LL; /* valid data from prev stbf */
    dv[1] = 0x0000000000000000LL; /* valid data from prev stbf */
    /* check preceeding l1rq.STD */
    if (c[cid].l1rq[l1bank].v_stat && c[cid].l1rq[l1bank].type == 4) {
      Uint sa, so; Ull sm[2], sv[2];
      sa = c[cid].l1rq[l1bank].pull_ADR & ~15;
      so = ((c[cid].l1rq[l1bank].pull_ADR&8)!=0); /* 0:evan, 1:odd */
      switch (c[cid].l1rq[l1bank].opcd) {
      case 0: /* STRB */
      case 8: /* VSTRB */
	sm[so]   = 0x00000000000000ffLL       <<((c[cid].l1rq[l1bank].pull_ADR&7)<<3);
	sv[so]   = (c[cid].l1rq[l1bank].STD[0]<<((c[cid].l1rq[l1bank].pull_ADR&7)<<3));
	sm[1-so] = 0x0000000000000000LL;
	sv[1-so] = 0x0000000000000000LL;
	break;
      case 1: /* STRH */
      case 9: /* VSTRH */
	sm[so]   = 0x000000000000ffffLL       <<((c[cid].l1rq[l1bank].pull_ADR&6)<<3);
	sv[so]   = (c[cid].l1rq[l1bank].STD[0]<<((c[cid].l1rq[l1bank].pull_ADR&6)<<3));
	sm[1-so] = 0x0000000000000000LL;
	sv[1-so] = 0x0000000000000000LL;
	break;
      case 2: /* STRW */
      case 7: /* STREX */
      case 10: /* VSTRS */
	sm[so]   = 0x00000000ffffffffLL       <<((c[cid].l1rq[l1bank].pull_ADR&4)<<3);
	sv[so]   = (c[cid].l1rq[l1bank].STD[0]<<((c[cid].l1rq[l1bank].pull_ADR&4)<<3));
	sm[1-so] = 0x0000000000000000LL;
	sv[1-so] = 0x0000000000000000LL;
	break;
      case 3: /* STR */
      case 11: /* VSTRD */
	sm[so]   = 0xffffffffffffffffLL;
	sv[so]   = c[cid].l1rq[l1bank].STD[0];
	sm[1-so] = 0x0000000000000000LL;
	sv[1-so] = 0x0000000000000000LL;
	break;
      case 12: /* VSTRQ */
	sm[so]   = 0xffffffffffffffffLL;
	sv[so]   = c[cid].l1rq[l1bank].STD[0];
	sm[1-so] = 0xffffffffffffffffLL;
	sv[1-so] = c[cid].l1rq[l1bank].STD[1];
	break;
      }
      if (a == sa) {
	if (m[0] & sm[0]) {
	  dm[0] =  dm[0] | (m[0] & sm[0]);
	  dv[0] = (dv[0] & ~(m[0] & sm[0])) | (sv[0] & (m[0] & sm[0]));
	}
	if (m[1] & sm[1]) {
	  dm[1] =  dm[1] | (m[1] & sm[1]);
	  dv[1] = (dv[1] & ~(m[1] & sm[1])) | (sv[1] & (m[1] & sm[1]));
	}
      }
    }
    /* check preceeding rob.stbf */
    for (i=c[cid].rob_bot; i!=rob-&c[cid].rob[0]; i=(i+1)%CORE_ROBSIZE) {
      if (c[cid].rob[i].stat == ROB_MAPPED && c[cid].rob[i].type == 4) { /* ROB_MAPPED: rob->ls_addr is unknown .. all succeeding loads should wait */
	if (flag & TRACE_ARM)
	  printf(":(waiting prev_store rob[%d].stat=%d)", i, c[cid].rob[i].stat);
	return (1);
      }
      else if ((c[cid].rob[i].stat == ROB_STREXWAIT || c[cid].rob[i].stat == ROB_COMPLETE || c[cid].rob[i].stat == ROB_D1WRQWAIT) && c[cid].rob[i].tid == tid && c[cid].rob[i].stbf.t) { /* ROB_COMPLETE: rob->stbf.t = 1 */ 
	Uint sa, so; Ull sm[2], sv[2];
	sa = c[cid].rob[i].ls_addr & ~15;
	so = ((c[cid].rob[i].ls_addr&8)!=0); /* 0:evan, 1:odd */
	sm[so]   = c[cid].rob[i].stbf.mask[0]<<((c[cid].rob[i].ls_addr&7)<<3);
	sv[so]   = c[cid].rob[i].stbf.val[0] <<((c[cid].rob[i].ls_addr&7)<<3);
	sm[1-so] = c[cid].rob[i].stbf.mask[1];
	sv[1-so] = c[cid].rob[i].stbf.val[1];
	if (a == sa) {
	  if (c[cid].rob[i].stbf.t == 3) { /* wait for previous store */
	    if ((m[0] & sm[0]) || (m[1] & sm[1])) {
	      if (flag & TRACE_ARM)
		printf(":(waiting prev_store rob[%d].stat=%d)", i, c[cid].rob[i].stat);
	      return (1);
	    }
	  }
	  else if (c[cid].rob[i].stbf.t == 1) { /* valid previous store */
	    if (m[0] & sm[0]) {
	      dm[0] =  dm[0] | (m[0] & sm[0]);
	      dv[0] = (dv[0] & ~(m[0] & sm[0])) | (sv[0] & (m[0] & sm[0]));
	    }
	    if (m[1] & sm[1]) {
	      dm[1] =  dm[1] | (m[1] & sm[1]);
	      dv[1] = (dv[1] & ~(m[1] & sm[1])) | (sv[1] & (m[1] & sm[1]));
	    }
	  }
	}
      }
    }

    if (o==0) { /* even */
      dm[0] = dm[0] >> ((addr&7)<<3);
      dv[0] = dv[0] >> ((addr&7)<<3);
      dm[1] = dm[1];
      dv[1] = dv[1];
    }
    else { /* odd */
      dm[0] = dm[1] >> ((addr&7)<<3);
      dv[0] = dv[1] >> ((addr&7)<<3);
      dm[1] = 0LL;
      dv[1] = 0LL;
    }
    if (dm[0]) {
      rob->dr[1].mask[0] = dm[0];
      rob->dr[1].val[0]  = dv[0];
    }
    if (dm[1]) {
      rob->dr[1].mask[1] = dm[1];
      rob->dr[1].val[1]  = dv[1];
    }
    if (dm[0] == mask[0] && dm[1] == mask[1]) {
      val[0] = dv[0];
      val[1] = dv[1];
      if (opcd == 7) /* ldrex should not get value from bypass */
	return (1);
      else
	return (0);
    }

    /* stbf does not cover all mask */
    lv[0] = 0LL;
    lv[1] = 0LL;
    stat = d1r(tid, opcd, addr, mask, rot, lv, &pull_way, &rq_push, &push_adr, inhibit_replace?NULL:c[cid].l1rq[l1bank].BUF); /* I1-cache */
    val[0] = (dv[0] & dm[0]) | (lv[0] & ~dm[0]);
    val[1] = (dv[1] & dm[1]) | (lv[1] & ~dm[1]);
    break;
  case 4: /* store */
    /* store to rob.stbf */
    stat = 0;
    rob->ls_addr      = addr;
    rob->stbf.t       = (opcd==7)?3:1; /* valid */
    rob->stbf.mask[0] = mask[0];
    rob->stbf.mask[1] = mask[1];
    rob->stbf.val[0]  = val[0];
    rob->stbf.val[1]  = val[1];
    break;
  default: /* not implemented */
    if (flag & TRACE_ARM)
      printf(":o_ldst A=%08.8x type=%d not implemented", addr, type);
    else {
      if (flag & TRACE_ARM) printf("\n");
      printf("%03.3d:WARNING %08.8x_%08.8x o_ldst A=%08.8x type=%d not implemented",
	     tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), addr, type);
      if (!(flag & TRACE_ARM)) printf("\n");
    }
    return (3);
  }

  if (stat == 2 || stat == 1) { /* d1r:miss d1w:miss|!dirty|shared || DDR-miss */
    if (inhibit_replace) { /* busy */
      if (flag & TRACE_ARM)
	printf(":(inhibit_replace=1 tid=%d type=%d opcd=%d rob=%d)", tid, type, opcd, rob-&c[cid].rob[0]);
      return (1);
    }
    rob->ls_addr = addr;
    c[cid].l1rq[l1bank].rv_l1fil = 1; /* for load */
    c[cid].l1rq[l1bank].t    = D1DELAY;
    c[cid].l1rq[l1bank].tid  = tid;
    c[cid].l1rq[l1bank].type = type; /* load/store/ldstub */
    c[cid].l1rq[l1bank].opcd = opcd; /* OP-fetch */
    c[cid].l1rq[l1bank].rq_push = rq_push;   /* miss:repl_d !dirty|shared:0 */
    c[cid].l1rq[l1bank].push_ADR = push_adr;
    c[cid].l1rq[l1bank].rq_pull = 1;   /* miss:pull !dirty|shared:1(本来不要だがプロトコル簡単化のため) */
    c[cid].l1rq[l1bank].pull_ADR = addr;
    c[cid].l1rq[l1bank].pull_way = pull_way;
    c[cid].l1rq[l1bank].STD[0] = val[0]; /* for store */
    c[cid].l1rq[l1bank].STD[1] = val[1]; /* for store */
    c[cid].l1rq[l1bank].ib = NULL; /* not used */
    c[cid].l1rq[l1bank].rob = rob;
    c[cid].l1rq[l1bank].v_stat = (3<<2)|(1); /* valid */
    /* L1:index=10bit,linesize=6bit,L2:bank=2bit,blocksize=12bitなので,L1の同一ラインはL2の同一バンクになる */
    /* このため，push_ADRとpull_ADRは同一バンクとなるので，bitmap更新は1bitだけでよい */
    return (2);
  }
  else
    return (0);
}

int i1r(tid, addr, val, pull_way) Uint tid, addr; Uint *val; int *pull_way;
     /* アドレス空間の判別後に使用 */
     /* return 0:normal end, 2:miss */
{
  Uint cid = tid2cid(tid);
  Uint l1bank = (addr/LINESIZE)%MAXL1BK;
  Uint index  = (addr/LINESIZE/MAXL1BK)%I1WMAXINDEX;
  Ull  membuf;
  int  fmin, i, j, k, w, hit;

  fmin = MAXINT;
  j = -1;
  k = -1;
  hit = 0;
  for (w=0; w<I1WAYS; w++) {
    if (c[cid].i1tag[l1bank][index][w].lru)
      c[cid].i1tag[l1bank][index][w].lru--;
    if (c[cid].i1tag[l1bank][index][w].v && c[cid].i1tag[l1bank][index][w].la == addr/I1TAGMASK) { /* hit */
      c[cid].i1tag[l1bank][index][w].lru = 127; /* reset lru */
      j = w;
      hit = 1;
    }
    else if (!c[cid].i1tag[l1bank][index][w].v) { /* first empty entry */
      if (j < 0)
	j = w;
    }
    else { /* fixed nonbusy entry */
      if (fmin > c[cid].i1tag[l1bank][index][w].lru) {
	fmin = c[cid].i1tag[l1bank][index][w].lru;
	k = w;
      }
    }
  }
  if (!hit) {
    if (j >= 0) /* first empty entry */
      *pull_way = j;
    else /* least used entry */
      *pull_way = k;
    return (2); /* miss */
  }
  else { /* hit */
    membuf = c[cid].i1line[l1bank][index][j].d[(addr%LINESIZE)>>3];
    val[0] = membuf>>((addr&4)<<3);
    return (0); /* hit */
  }
}

int d1r(tid, opcd, addr, mask, rot, val, pull_way, rq_push, push_adr, repl_d)
     Uint tid, opcd, addr; Ull *mask; Uint rot; Ull *val;
     int *pull_way, *rq_push; Uint *push_adr; Ull *repl_d;
     /* アドレス空間の判別後に使用 */
     /* return 0:normal end, 1:wait for CC, 2:miss + after-care */
{
  int cid = tid2cid(tid);
  Uint l1bank = (addr/LINESIZE)%MAXL1BK;
  Uint index  = (addr/LINESIZE/MAXL1BK)%D1WMAXINDEX;
  Ull  membuf;
  Uint a;
  int  fmin, i, j, k, l, w, hit;
  
  a = addr & ~7;

  fmin = MAXINT;
  j = -1;
  k = -1;
  hit = 0;
  for (w=0; w<D1WAYS; w++) {
    if (c[cid].d1tag[l1bank][index][w].lru)
      if (repl_d) c[cid].d1tag[l1bank][index][w].lru--;
    if (c[cid].d1tag[l1bank][index][w].v && c[cid].d1tag[l1bank][index][w].la == a/D1TAGMASK) { /* hit */
      c[cid].d1tag[l1bank][index][w].lru = 127; /* reset lru */
      j = w;
      hit = 1;
    }
    else if (!c[cid].d1tag[l1bank][index][w].v) { /* first empty entry */
      if (j < 0)
	j = w;
    }
    else { /* fixed nonbusy entry */
      if (fmin > c[cid].d1tag[l1bank][index][w].lru) {
	fmin = c[cid].d1tag[l1bank][index][w].lru;
	k = w;
      }
    }
  }
  if (!hit) {
    if (j >= 0) /* first empty entry */
      *pull_way = j;
    else /* least used entry */
      *pull_way = k;
    if (c[cid].d1tag[l1bank][index][*pull_way].v) {
      *rq_push  =  c[cid].d1tag[l1bank][index][*pull_way].dirty;
      *push_adr = (c[cid].d1tag[l1bank][index][*pull_way].la * D1TAGMASK)|((index*MAXL1BK+l1bank) * LINESIZE);
      if (*rq_push && repl_d) { /* L1CTを使う場合（non-blocking-cache），repl_d=NULLで来る */
	c[cid].d1tag[l1bank][index][*pull_way].v = 0;
	for (l=0; l<LINESIZE/8; l++) /* 4wayなので実際にどこかに読み出してあるはず */
	  repl_d[l] = c[cid].d1line[l1bank][index][*pull_way].d[l]; /* osimでは代わりにL2_ctl.d[]を使用 */
      }
    }
    else { /* least used entry */
      *rq_push = 0;
      *push_adr = 0;
    }
#if 1
    if (flag & TRACE_ARM)
      printf(":D1R miss (l1bank=%d A=%08.8x rq_push=%d pushA=%08.8x)", l1bank, addr, *rq_push, *push_adr);
#endif
    return (2); /* miss */
  }
  else if (opcd == 7 && (!c[cid].d1tag[l1bank][index][j].dirty || c[cid].d1tag[l1bank][index][j].share)) { /* LDREX hit but !dirty|shared */
    /* LDREXが後続STREXの成功確率を上げるためにdirty=1,share=0を目指す(保証はしない) */
    *pull_way = j;
    *rq_push = 0;
    *push_adr = 0;
#if 1
    if (flag & TRACE_ARM)
      printf(":D1R LDREX !dirty|shared dirty=%d share=%d (l1bank=%d A=%08.8x)",
	     c[cid].d1tag[l1bank][index][j].dirty, c[cid].d1tag[l1bank][index][j].share, l1bank, addr);
#endif
    return (1); /* wait for c-coherence */
  }
  else { /* hit */
    if (opcd == 7) { /* ldrex */
      if (c[cid].d1tag[l1bank][index][j].thr_per_core == -1) { /* check thr_per_core */
	if (flag & TRACE_ARM)
	  printf(":LDREX KEEP");
	c[cid].d1tag[l1bank][index][j].thr_per_core = tid/MAXCORE; /* keep */
      }
      else {
	if (flag & TRACE_ARM)
	  printf(":LDREX MISS");
      }
    }
    if      (mask[1] & 0xffffffffffffffffLL) {
      membuf = c[cid].d1line[l1bank][index][j].d[((a|8)&(LINESIZE-1))>>3];
#ifdef CHECK_CACHE
      {
	Ull membuf2;
	membuf2 = mmr_chkc(tid, a|8);
	if (membuf != membuf2) {
	  if (flag & TRACE_ARM) printf("\n");
	  printf("%03.3d:D1R %08.8x_%08.8x hit (l1bank=%d,index=%d,way=%d,A=%08.8x->%08.8x_%08.8x mismatch mmr %08.8x_%08.8x",
		 tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps),
		 l1bank,index, j, a|8, (Uint)(membuf>>32), (Uint)membuf, (Uint)(membuf2>>32), (Uint)membuf2);
	  if (!(flag & TRACE_ARM)) printf("\n");
	}
      }
#endif
      val[1] = membuf;
    }
    membuf = c[cid].d1line[l1bank][index][j].d[(a&(LINESIZE-1))>>3];
#ifdef CHECK_CACHE
    {
      Ull membuf2;
      membuf2 = mmr_chkc(tid, a);
      if (membuf != membuf2) {
	if (flag & TRACE_ARM) printf("\n");
	printf("%03.3d:D1R %08.8x_%08.8x hit (l1bank=%d,index=%d,way=%d,A=%08.8x->%08.8x_%08.8x mismatch mmr %08.8x_%08.8x",
	       tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps),
	       l1bank,index, j, a, (Uint)(membuf>>32), (Uint)membuf, (Uint)(membuf2>>32), (Uint)membuf2);
	if (!(flag & TRACE_ARM)) printf("\n");
      }
    }
#endif
    if      (mask[0] & 0xffffffff00000000LL) val[0] = membuf;
    else if (mask[0] & 0x00000000ffff0000LL) {
      if (rot)
	rot = (addr & 3)*8;
      val[0]=(membuf>>((addr&4)<<3))&mask[0];
      if (rot)
	val[0]=((val[0]<<(32-rot))|(val[0]>>rot))&mask[0];
    }
    else if (mask[0] & 0x000000000000ff00LL) val[0]=(membuf>>((addr&6)<<3))&mask[0];
    else if (mask[0] & 0x00000000000000ffLL) val[0]=(membuf>>((addr&7)<<3))&mask[0];
#if 1
    if (flag & TRACE_ARM)
      printf(":l1bank=%d,index=%d,way=%d,A=%08.8x,M=%08.8x_%08.8x_%08.8x_%08.8x->%08.8x_%08.8x_%08.8x_%08.8x,d=%d,s=%d",
	     l1bank,index, j,
	     addr,
	     (Uint)(mask[1]>>32), (Uint)mask[1], (Uint)(mask[0]>>32), (Uint)mask[0],
	     (Uint)(val[1]>>32), (Uint)(val[1]), (Uint)(val[0]>>32), (Uint)(val[0]),
	     c[cid].d1tag[l1bank][index][j].dirty, c[cid].d1tag[l1bank][index][j].share);
#endif

#if 1
    /* check inclusive w/ L2 */
    {
      Uint l2bank = (addr/LINESIZE)%MAXL2BK;
      Uint index = (addr/LINESIZE/MAXL2BK)%L2WMAXINDEX;
      int w;
      for (w=0; w<L2WAYS; w++) {
	if (c[cid].l2tag[l2bank][index][w].v && c[cid].l2tag[l2bank][index][w].la == addr/L2TAGMASK) { /* hit */
	  break;
	}
      }
      if (w == L2WAYS) {
	if (flag & TRACE_ARM)
	  printf(":d1r violates inclusive L2 cache l1bank=%d l2bank=%d A=%08.8x (malfunction)", l1bank, l2bank, addr);
	else {
	  if (flag & TRACE_ARM) printf("\n");
	  printf("%03.3d:WARNING %08.8x_%08.8x d1r violates inclusive L2 cache l1bank=%d l2bank=%d A=%08.8x (malfunction)",
		 tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), l1bank, l2bank, addr);
	  if (!(flag & TRACE_ARM)) printf("\n");
	}
      }
    }
#endif
    return (0); /* hit */
  }
}

int d1w(tid, opcd, addr, mask, flush, keep_or_drain, val, pull_way, rq_push, push_adr, repl_d)
     Uint tid, opcd, flush, keep_or_drain; Uint addr; Ull *mask, *val; 
     int *pull_way, *rq_push; Uint *push_adr; Ull *repl_d;
     /* アドレス空間の判別後に使用 */
     /* DSM missの場合,repl_dは更新しないが,上位ではl1rqにstore-REQ発行 */
     /* return 0:normal end, 1:wait for CC, 2:miss + after-care */
     /* keep_or_drain: t[tid].status == ARM_DRAIN */
{
  int cid = tid2cid(tid);
  Uint l1bank = (addr/LINESIZE)%MAXL1BK;
  Uint index  = (addr/LINESIZE/MAXL1BK)%D1WMAXINDEX;
  Ull  membuf, m;
  Uint a;
  int fmin, i, j, k, l, w, hit;

  a = addr & ~7;

  if (flush) {
    *rq_push = 0;
    *push_adr = 0;
    w = (addr/D1TAGMASK)%D1WAYS;
    if (c[cid].d1tag[l1bank][index][w].v) {
      if (keep_or_drain) /* 0:keep,1:drain */
	c[cid].d1tag[l1bank][index][w].v = 0;
      if (c[cid].d1tag[l1bank][index][w].dirty && (keep_or_drain || !c[cid].d1tag[l1bank][index][w].drain)) {
	c[cid].d1tag[l1bank][index][w].drain = 1; /* mark */
	*rq_push = 1;
	*push_adr = (c[cid].d1tag[l1bank][index][w].la * D1TAGMASK)|((index*MAXL1BK+l1bank) * LINESIZE);
	if (repl_d) {
	  for (l=0; l<LINESIZE/8; l++) /* 4wayなので実際にどこかに読み出してあるはず */
	    repl_d[l] = c[cid].d1line[l1bank][index][w].d[l]; /* osimでは代わりにL2_ctl.d[]を使用 */
	}
	return (2);
      }
      return (1);
    }
    return (0);
  }

  fmin = MAXINT;
  j = -1;
  k = -1;
  hit = 0;
  for (w=0; w<D1WAYS; w++) {
    if (c[cid].d1tag[l1bank][index][w].lru)
      if (repl_d) c[cid].d1tag[l1bank][index][w].lru--;
    if (c[cid].d1tag[l1bank][index][w].v && c[cid].d1tag[l1bank][index][w].la == a/D1TAGMASK) { /* hit */
      c[cid].d1tag[l1bank][index][w].lru = 127; /* reset lru */
      j = w;
      hit = 1;
    }
    else if (!c[cid].d1tag[l1bank][index][w].v) { /* first empty entry */
      if (j < 0)
	j = w;
    }
    else { /* fixed nonbusy entry */
      if (fmin > c[cid].d1tag[l1bank][index][w].lru) {
	fmin = c[cid].d1tag[l1bank][index][w].lru;
	k = w;
      }
    }
  }
  if (!hit) {
    if (j >= 0) /* first empty entry */
      *pull_way = j;
    else /* least used entry */
      *pull_way = k;
    if (c[cid].d1tag[l1bank][index][*pull_way].v) { /* strex時は無変更 */
      *rq_push  =  c[cid].d1tag[l1bank][index][*pull_way].dirty;
      *push_adr = (c[cid].d1tag[l1bank][index][*pull_way].la * D1TAGMASK)|((index*MAXL1BK+l1bank) * LINESIZE);
      if (*rq_push && repl_d) { /* L1CTを使う場合（non-blocking-cache），repl_d=NULLで来る */
	c[cid].d1tag[l1bank][index][*pull_way].v = 0;
	for (l=0; l<LINESIZE/8; l++) /* 4wayなので実際にどこかに読み出してあるはず */
	  repl_d[l] = c[cid].d1line[l1bank][index][*pull_way].d[l]; /* osimでは代わりにL2_ctl.d[]を使用 */
      }
    }
    else {
      *rq_push = 0;
      *push_adr = 0;
    }
#if 1
    if (flag & TRACE_ARM)
      printf(":D1W miss (l1bank=%d A=%08.8x rq_push=%d pushA=%08.8x)", l1bank, addr, *rq_push, *push_adr);
#endif
    return (2); /* miss */
  }
  else if (!c[cid].d1tag[l1bank][index][j].dirty || c[cid].d1tag[l1bank][index][j].share) { /* hit but !dirty|shared */
    /* ストアデータが待っているが,この時点では書き込みできない(CC解決後に改めて書き込む) */
    *pull_way = j;
    *rq_push = 0;
    *push_adr = 0;
#if 1
    if (flag & TRACE_ARM)
      printf(":D1W !dirty|shared dirty=%d share=%d (l1bank=%d A=%08.8x)",
	     c[cid].d1tag[l1bank][index][j].dirty, c[cid].d1tag[l1bank][index][j].share, l1bank, addr);
#endif
    return (1); /* wait for c-coherence */
  }
  else { /* hit and dirty & !shared */
    if (opcd == 7) { /* strex */
      if (c[cid].d1tag[l1bank][index][j].thr_per_core == tid/MAXCORE) { /* check thr_per_core */
	if (flag & TRACE_ARM)
	  printf(":STREX SUCCEEDED");
	c[cid].d1tag[l1bank][index][j].thr_per_core = -1; /* reset */
      }
      else {
	if (flag & TRACE_ARM)
	  printf(":STREX FAIL current thr_per_core=%d", c[cid].d1tag[l1bank][index][j].thr_per_core);
	return (1); /* strex failed */
      }
    }

    if      (mask[1] & 0xffffffffffffffffLL) {
      m = 0xffffffffffffffffLL;
      membuf = val[1];
      c[cid].d1line[l1bank][index][j].d[((a|8)&(LINESIZE-1))>>3] = membuf;
#ifdef CHECK_CACHE
      mmw_chkc(tid, a|8, m, membuf);
#endif
    }
    if      (mask[0] & 0xffffffff00000000LL) { m = 0xffffffffffffffffLL;                membuf = (val[0]               );}
    else if (mask[0] & 0x00000000ffff0000LL) { m = 0x00000000ffffffffLL<<((addr&4)<<3); membuf = (val[0]<<((addr&4)<<3));}
    else if (mask[0] & 0x000000000000ff00LL) { m = 0x000000000000ffffLL<<((addr&6)<<3); membuf = (val[0]<<((addr&6)<<3));}
    else if (mask[0] & 0x00000000000000ffLL) { m = 0x00000000000000ffLL<<((addr&7)<<3); membuf = (val[0]<<((addr&7)<<3));}
    c[cid].d1line[l1bank][index][j].d[(a&(LINESIZE-1))>>3] = (c[cid].d1line[l1bank][index][j].d[(a&(LINESIZE-1))>>3]&~m)|(membuf&m);
#ifdef CHECK_CACHE
    mmw_chkc(tid, a, m, membuf);
#endif
#if 1
    if (flag & TRACE_ARM)
      printf(":l1bank=%d,index=%d,way=%d,A=%08.8x,M=%08.8x_%08.8x_%08.8x_%08.8x<-%08.8x_%08.8x_%08.8x_%08.8x,d=%d,s=%d",
	     l1bank,index, j,
	     addr,
	     (Uint)(mask[1]>>32), (Uint)mask[1], (Uint)(mask[0]>>32), (Uint)mask[0],
	     (Uint)(val[1]>>32), (Uint)val[1], (Uint)(val[0]>>32), (Uint)val[0],
	     c[cid].d1tag[l1bank][index][j].dirty, c[cid].d1tag[l1bank][index][j].share);
#endif

#if 1
    /* check inclusive w/ L2 */
    {
      Uint l2bank = (addr/LINESIZE)%MAXL2BK;
      Uint index = (addr/LINESIZE/MAXL2BK)%L2WMAXINDEX;
      int w;
      for (w=0; w<L2WAYS; w++) {
	if (c[cid].l2tag[l2bank][index][w].v && c[cid].l2tag[l2bank][index][w].la == addr/L2TAGMASK) { /* hit */
	  break;
	}
      }
      if (w == L2WAYS) {
	if (flag & TRACE_ARM)
	  printf(":d1w violates inclusive L2 cache l1bank=%d l2bank=%d A=%08.8x (malfunction)", l1bank, l2bank, addr);
	else
	  printf("%03.3d:WARNING %08.8x_%08.8x d1w violates inclusive L2 cache l1bank=%d l2bank=%d A=%08.8x (malfunction)\n",
		 tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), l1bank, l2bank, addr);
      }
    }
#endif
    return (0); /* hit */
  }
}

int l2r(tid, write, addr, val, rv_share, pull_way, rq_push, push_adr, repl_d)
     Uint tid, write, addr; Ull *val; Uint *rv_share;
     int *pull_way, *rq_push; Uint *push_adr; Ull *repl_d;
     /* return 0:normal end, 2:miss + after care */
{
  int cid = tid2cid(tid);
  Uint l1bank = (addr/LINESIZE)%MAXL1BK;
  Uint l2bank = (addr/LINESIZE)%MAXL2BK;
  Uint index  = (addr/LINESIZE/MAXL2BK)%L2WMAXINDEX;
  Uint a;
  int fmin, i, j, k, l, w, hit;

  a = addr & ~(LINESIZE-1);

  fmin = MAXINT;
  j = -1;
  k = -1;
  hit = 0;
  for (w=0; w<L2WAYS; w++) {
    if (c[cid].l2tag[l2bank][index][w].lru)
      if (repl_d) c[cid].l2tag[l2bank][index][w].lru--;
    if (c[cid].l2tag[l2bank][index][w].v && c[cid].l2tag[l2bank][index][w].la == a/L2TAGMASK) { /* hit */
      c[cid].l2tag[l2bank][index][w].lru = 127; /* reset lru */
      j = w;
      hit = 1;
    }
    else if (!c[cid].l2tag[l2bank][index][w].v) { /* first empty entry */
      if (j < 0)
	j = w;
    }
    else { /* fixed nonbusy entry */
      if (fmin > c[cid].l2tag[l2bank][index][w].lru) {
	fmin = c[cid].l2tag[l2bank][index][w].lru;
	k = w;
      }
    }
  }
  if (!hit) {
    if (j >= 0) /* first empty entry */
      *pull_way = j;
    else /* least used entry */
      *pull_way = k;
    if (c[cid].l2tag[l2bank][index][*pull_way].v) {
      *rq_push  =  c[cid].l2tag[l2bank][index][*pull_way].dirty;
      *push_adr = (c[cid].l2tag[l2bank][index][*pull_way].la * L2TAGMASK)|((index*MAXL2BK+l2bank) * LINESIZE);
      if (!*rq_push || repl_d) { /* L2CTを使う場合（non-blocking-cache），repl_d=NULLで来る */
	c[cid].l2tag[l2bank][index][*pull_way].v = 0;
	if (*rq_push && repl_d) {
	  for (l=0; l<LINESIZE/8; l++) /* 4wayなので実際にどこかに読み出してあるはず */
	    repl_d[l] = c[cid].l2line[l2bank][index][*pull_way].d[l]; /* osimでは代わりにL2_ctl.d[]を使用 */
	}
	index = (*push_adr/LINESIZE/MAXL1BK)%D1WMAXINDEX;
	for (w=0; w<D1WAYS; w++) {
	  if (c[cid].d1tag[l1bank][index][w].v && c[cid].d1tag[l1bank][index][w].la == *push_adr/D1TAGMASK) /* hit */
	    break;
	}
	if (w<D1WAYS) {
	  c[cid].d1tag[l1bank][index][w].v = 0; /* invalidate */
	  if (*rq_push && repl_d) {
	    for (l=0; l<LINESIZE/8; l++) /* 4wayなので実際にどこかに読み出してあるはず */
	      repl_d[l] = c[cid].d1line[l1bank][index][w].d[l]; /* osimでは代わりにL2_ctl.d[]を使用 */
	  }
	}
      }
    }
    else { /* least used entry */
      *rq_push = 0;
      *push_adr = 0;
    }
#if 1
    if (flag & TRACE_ARM)
      printf("%03.3d:L2R %08.8x_%08.8x miss (l2bank=%d A=%08.8x)\n", tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), l2bank, addr);
#endif
    return (2); /* miss */
  }
  else if (write && (!c[cid].l2tag[l2bank][index][j].dirty || c[cid].l2tag[l2bank][index][j].share)) { /* hit but !dirty|shared */
    /* d1-missストアのために必要なl2有効データは読み出しておく */
    for (l=0; l<LINESIZE/8; l++) {
      *(val+l) = c[cid].l2line[l2bank][index][j].d[l];
#if 1
      if (flag & TRACE_ARM)
        printf("%03.3d:L2R %08.8x_%08.8x hit (A=%08.8x->%08.8x_%08.8x)\n", tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), (a+l*8), (Uint)(*(val+l)>>32), (Uint)*(val+l));
#endif
    }
    *pull_way = j;
    *rq_push = 0;
    *push_adr = 0;
#if 1
    if (flag & TRACE_ARM)
      printf("%03.3d:L2R %08.8x_%08.8x for store !dirty|shared dirty=%d share=%d (l2bank=%d A=%08.8x)\n",
	     tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), c[cid].l2tag[l2bank][index][j].dirty, c[cid].l2tag[l2bank][index][j].share, l2bank, addr);
#endif
    return (1); /* wait for c-coherence */
  }
  else { /* hit */
    for (l=0; l<LINESIZE/8; l++) {
      *(val+l) = c[cid].l2line[l2bank][index][j].d[l];
#if 1
      if (flag & TRACE_ARM)
        printf("%03.3d:L2R %08.8x_%08.8x hit (A=%08.8x->%08.8x_%08.8x)\n", tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), (a+l*8), (Uint)(*(val+l)>>32), (Uint)*(val+l));
#endif
    }
    *rv_share = c[cid].l2tag[l2bank][index][j].share;
    *rq_push = 0;
    *push_adr = 0;
    return (0); /* hit */
  }
}

int l2w(tid, addr, val, pull_way, rq_push, push_adr, repl_d)
     Uint tid, addr; Ull *val;
     int *pull_way, *rq_push; Uint *push_adr; Ull *repl_d;
     /* return 0:normal end, 1:wait for CC, 2:miss + after care */
{
  int cid = tid2cid(tid);
  Uint l1bank = (addr/LINESIZE)%MAXL1BK;
  Uint l2bank = (addr/LINESIZE)%MAXL2BK;
  Uint index  = (addr/LINESIZE/MAXL2BK)%L2WMAXINDEX;
  Uint a;
  int  fmin, i, j, k, l, w, hit;

  a = addr & ~(LINESIZE-1);

  fmin = MAXINT;
  j = -1;
  k = -1;
  hit = 0;
  for (w=0; w<L2WAYS; w++) {
    if (c[cid].l2tag[l2bank][index][w].lru)
      if (repl_d) c[cid].l2tag[l2bank][index][w].lru--;
    if (c[cid].l2tag[l2bank][index][w].v && c[cid].l2tag[l2bank][index][w].la == a/L2TAGMASK) { /* hit */
      c[cid].l2tag[l2bank][index][w].lru = 127; /* reset lru */
      j = w;
      hit = 1;
    }
    else if (!c[cid].l2tag[l2bank][index][w].v) { /* first empty entry */
      if (j < 0)
	j = w;
    }
    else { /* fixed nonbusy entry */
      if (fmin > c[cid].l2tag[l2bank][index][w].lru) {
	fmin = c[cid].l2tag[l2bank][index][w].lru;
	k = w;
      }
    }
  }
  if (!hit) {
    if (j >= 0) /* first empty entry */
      *pull_way = j;
    else /* least used entry */
      *pull_way = k;
    if (c[cid].l2tag[l2bank][index][*pull_way].v) {
      *rq_push  =  c[cid].l2tag[l2bank][index][*pull_way].dirty;
      *push_adr = (c[cid].l2tag[l2bank][index][*pull_way].la * L2TAGMASK)|((index*MAXL2BK+l2bank) * LINESIZE);
      if (!*rq_push || repl_d) { /* L2CTを使う場合（non-blocking-cache），repl_d=NULLで来る */
	c[cid].l2tag[l2bank][index][*pull_way].v = 0;
	if (*rq_push && repl_d) { /* L2CTを使う場合（non-blocking-cache），repl_d=NULLで来る */
	  for (l=0; l<LINESIZE/8; l++) /* 4wayなので実際にどこかに読み出してあるはず */
	    repl_d[l] = c[cid].l2line[l2bank][index][*pull_way].d[l]; /* osimでは代わりにL2_ctl.d[]を使用 */
	}
	index = (*push_adr/LINESIZE/MAXL1BK)%D1WMAXINDEX;
	for (w=0; w<D1WAYS; w++) {
	  if (c[cid].d1tag[l1bank][index][w].v && c[cid].d1tag[l1bank][index][w].la == *push_adr/D1TAGMASK) /* hit */
	    break;
	}
	if (w<D1WAYS) {
	  c[cid].d1tag[l1bank][index][w].v = 0; /* invalidate */
	  if (*rq_push && repl_d) { /* L2CTを使う場合（non-blocking-cache），repl_d=NULLで来る */
	    for (l=0; l<LINESIZE/8; l++) /* 4wayなので実際にどこかに読み出してあるはず */
	      repl_d[l] = c[cid].d1line[l1bank][index][w].d[l]; /* osimでは代わりにL2_ctl.d[]を使用 */
	  }
	}
      }
    }
    else { /* least used entry */
      *rq_push = 0;
      *push_adr = 0;
    }
#if 1
    if (flag & TRACE_ARM)
      printf("%03.3d:L2W %08.8x_%08.8x miss (l2bank=%d A=%08.8x rq_push=%d pushA=%08.8x)\n", tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), l2bank, addr, *rq_push, *push_adr);
#endif
    return (2); /* miss */
  }
  else if (!c[cid].l2tag[l2bank][index][j].dirty /*|| c[cid].l2tag[l2bank][index][j].share*/) { /* hit but !dirty|shared */
    /* d1から追い出されたdirtyデータが待っているが,この時点では書き込みできない(CC解決後に改めて書き込む) */
    *pull_way = j;
    *rq_push = 0;
    *push_adr = 0;
#if 1
    if (flag & TRACE_ARM)
      printf("%03.3d:L2W %08.8x_%08.8x !dirty|shared dirty=%d share=%d (l2bank=%d A=%08.8x)\n",
	     tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), c[cid].l2tag[l2bank][index][j].dirty, c[cid].l2tag[l2bank][index][j].share, l2bank, addr);
#endif
    return (1); /* wait for c-coherence */
  }
  else { /* hit and dirty & !shared */
    for (l=0; l<LINESIZE/8; l++) {
#if 0
	    int oid, dirindex, index1, l1way, l2way;
	    dirindex = a/LINESIZE/MAXL2BK;
	    index1 = (a/LINESIZE/MAXL1BK)%D1WMAXINDEX;
	    for (oid=0; oid<MAXCORE; oid++) {
	      if (oid == cid) continue;
	      if (d[l2bank].l2dir[dirindex].l2dir_v[0] & (1LL<<oid)) {
		int l1hit = 0;
		for (l1way=0; l1way<D1WAYS; l1way++) {
		  if (c[oid].d1tag[l1bank][index1][l1way].v && c[oid].d1tag[l1bank][index1][l1way].la == a/D1TAGMASK) {
		    l1hit = 1;
		    if (c[oid].d1line[l1bank][index1][l1way].d[l] != *(val+l)) {
		      printf("%03.3d:L2W A=%08.8x->%08.8x_%08.8x mismatch cid=%d L1 %08.8x_%08.8x\n",
			     tid, 
			     (a&~(LINESIZE-1))+l*8,
			     (Uint)(*(val+l)>>32), (Uint)*(val+l),
			     oid, 
			     (Uint)(c[oid].d1line[l1bank][index1][l1way].d[l]>>32), (Uint)c[oid].d1line[l1bank][index1][l1way].d[l]);
		    }
		  }
		}
		if (!l1hit) {
		  for (l2way=0; l2way<L2WAYS; l2way++) {
		    if (c[oid].l2tag[l2bank][index][l2way].v && c[oid].l2tag[l2bank][index][l2way].la == a/L2TAGMASK) {
		      if (c[oid].l2line[l2bank][index][l2way].d[l] != *(val+l)) {
			printf("%03.3d:L2W A=%08.8x->%08.8x_%08.8x mismatch cid=%d L2 %08.8x_%08.8x\n",
			       tid, 
			       (a&~(LINESIZE-1))+l*8,
			       (Uint)(*(val+l)>>32), (Uint)*(val+l),
			       oid,
			       (Uint)(c[oid].l2line[l2bank][index][l2way].d[l]>>32), (Uint)c[oid].l2line[l2bank][index][l2way].d[l]);
		      }
		    }
		  }
		}
	      }
	    }
#endif
      c[cid].l2line[l2bank][index][j].d[l] = *(val+l); /* L2LIへ単純に書き込み */
#if 1
      if (flag & TRACE_ARM)
        printf("%03.3d:L2W %08.8x_%08.8x hit (A=%08.8x<-%08.8x_%08.8x) clean store\n", tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), (a+l*8), (Uint)(*(val+l)>>32), (Uint)*(val+l));
#endif
    }
    *rq_push = 0;
    *push_adr = 0;
    return (0); /* hit */
  }
}
