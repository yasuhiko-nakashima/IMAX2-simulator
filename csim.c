
static char RcsHeader[] = "$Header: /usr/home/nakashim/proj-arm64/src/csim/RCS/csim.c,v 1.68 2022/03/03 14:59:26 nakashim Exp nakashim $";

/* ARM Simulator                       */
/*         Copyright (C) 2005 by NAIST */
/*         Primary writer: Y.Nakashima */
/*                nakashim@is.naist.jp */

/* csim.c 2005/3/22 */ 

#include "csim.h"
#include "../conv-c2c/emax6.h"
Ull get_tcureg_valid();
Ull get_tcureg_last();
Ull get_tcureg();

main(argc, argv)
  int argc;  char **argv;
{
  int  parse_args = 0;
  char *armprog = NULL;
  int  armargc = 0;
  char **armargv;
  Uchar *memp;
  Uint  tid, cid, did, mid;
  Uint  i, j, k, l, stat, alive=1;

  EMAX_DEPTH = 64; /* default */
  /* オプション解析 */
  for (argc--, argv++; argc; argc--, argv++) {
    switch (parse_args) {
    case 0:
      if (**argv == '-') { /* regard as a command */
	switch (*(*argv+1)) {
	case 'u':
	  sscanf(*argv+2, "%d", &EMAX_DEPTH);
	  switch (EMAX_DEPTH) {
	  case 64:
	  case 32:
	  case 16:
	  case  8:
	    break;
	  default:
	    printf("usage: csim [-uxx -m -x -p -bxxx -exxx -dxxx] <arm-object> [arm-args]\n");
	    printf(" xx should be 64,32,16,8\n");
	    exit(1);
	  }
	  break;
	case 'm':
	  flag |= USE_PTHREAD;
	  break;
	case 'x':
	  flag |= IMAGEWIN;
	  break;
	case 'p':
	  flag |= TRACE_PIPE;
	  break;
	case 'b':
	  flag |= TRACE_RANGE;
	  sscanf(*argv+2, "%x", &trace_on);
	  trace_off = 0xffffffff;
	  break;
	case 'e':
	  flag |= TRACE_RANGE;
	  sscanf(*argv+2, "%x", &trace_off);
	  break;
	case 'd':
	  flag |= DUMP_DDR;
	  break;
	default:
	  printf("usage: csim [-uxx -m -x -p -bxxx -exxx -dxxx] <arm-object> [arm-args]\n");
	  printf("       -u64 : 64 units\n");
	  printf("       -u32 : 32 units\n");
	  printf("       -u16 : 16 units\n");
	  printf("       -u8  : 8 units\n");
	  printf("       -m   : with pthread\n");
	  printf("       -x   : open image window\n");
	  printf("       -p   : trace pipe (EMAX6)\n");
	  printf("       -bxxx: trace from xxx_step\n");
	  printf("       -exxx: trace until xxx_step\n");
	  printf("       -d   : dump DDR to conf/lmmi/regv/load.dat for FPGA_siml\n");
	  exit(1);
	}
	break;
      }
      else if (**argv != '/' || *(*argv+1) != '/') { /* regard as a command */
	parse_args++;
	strcpy(armprog = (char*)malloc(strlen(*argv) + 1), *argv);
	armargc = 1;
	armargv = argv;
	continue;
      }
      else {
	parse_args++;
	parse_args++;
	continue;
      }
    case 1:
      if (**argv != '/' || *(*argv+1) != '/') { /* regard as a command */
	armargc++;
	break;
      }
      else {
	parse_args++;
	continue;
      }
    }
  }

  printf("ARM+EMAX6 Simulator Version %s\n", version());
  printf(" MAXTHRD   =  %d\n", MAXTHRD);
  printf(" MAXCORE   =  %d\n", MAXCORE);
  printf(" THR/CORE  =  %f (should be integer)\n", (double)MAXTHRD/(double)MAXCORE);
  printf(" ROBSIZE   =  %d (actives are CORE_ROBSIZE-1)\n", CORE_ROBSIZE-1);
  printf(" LINESIZE  =  %dB\n", LINESIZE);
  printf(" I1SIZE    =  %dB (%dway delay=%d)\n", I1SIZE, I1WAYS, I1DELAY);
  printf(" D1SIZE    =  %dB (%dway delay=%d)\n", D1SIZE, D1WAYS, D1DELAY);
  printf(" L2SIZE    =  %dB (%dway dirdl=%d, cc=%d, mm=%d)\n", L2SIZE, L2WAYS, L2DIRDL, CCDELAY, MMDELAY);
  printf(" MAXL1BK   =  %d\n", MAXL1BK);
  printf(" MAXL2BK   =  %d\n", MAXL2BK);
  printf(" MAXMMBK   =  %d\n", MAXMMBK);
  printf(" memspace  =  %08.8x-%08.8x\n", 0, MEMSIZE-1);
  printf(" arm_hdr   =  %08.8x-\n", HDRADDR);
  printf(" arm_param =  %08.8x-\n", PARAM);
  printf(" aloclimit = -%08.8x\n", ALOCLIMIT);
  printf(" stack/thr =  %08.8x\n", STACKPERTHREAD);
  printf(" stackinit = -%08.8x\n", STACKINIT);
  
  /* ARMベンチマークプログラム走行用引数を格納 */
  memp = &mem[0][HDRADDR+24];	/* argc */
  *(Ull*)memp = armargc;
  i = PARAM;
  for (memp+=8; armargc; armargc--, armargv++, memp+=8) {
    *(Ull*)memp = i;		/* *argv */
    do {
      *(int*)&mem[0][i] = *(int*)(*armargv);	/* **argv */
      i += 4;
      (*armargv)+=4;
    } while (*((*armargv)-4)&&*((*armargv)-3)&&*((*armargv)-2)&&*((*armargv)-1));
  }

  t[0].ib.pc = read_armelf(armprog);
  t[0].status = ARM_NORMAL;
  for (tid=1; tid<MAXTHRD; tid++) {
    t[tid].ib.pc = t[0].ib.pc;
    t[tid].status = ARM_STOP;
  }
  for (tid=0; tid<MAXTHRD; tid++)
    t[tid].cpsr = 0x000000d3; /* FIQ,IRQ=off,Supervisor Mode */
  for (cid=0; cid<MAXCORE; cid++) {
    c[cid].if_nexttid = cid;
    c[cid].rob_nexttid = cid;
  }

  /* プロセッサ機能・リセット(GP600M上の機能は自己リセット) */
  printf("<ARM-PARAMS>\n");
  printf(" start_address=0x%08.8x\n", t[0].ib.pc);
  memp = &mem[0][HDRADDR];         /* initial malloc pointer */
  printf(" malloc_topadr=0x%08.8x_%08.8x\n", (Uint)((*(Ull*)memp)>>32), (Uint)(*(Ull*)memp));
  memp = &mem[0][HDRADDR+8];       /* latest malloc pointer */
  printf(" malloc_latest=0x%08.8x_%08.8x\n", (Uint)((*(Ull*)memp)>>32), (Uint)(*(Ull*)memp));
  memp = &mem[0][HDRADDR+16];      /* initial stack pointer */
  printf(" stack_pointer=0x%08.8x_%08.8x\n", (Uint)((*(Ull*)memp)>>32), (Uint)(*(Ull*)memp));
  for (i=0; i<256; i+=4) {
    if ((i%32) == 0)
      printf("%08.8x:", HDRADDR+i);
    printf(" %08.8x", *(int*)&mem[0][HDRADDR+i]);
    if ((i%32) == 28)
      printf("\n");
  }

  signal(SIGINT,  onintr_exit);
  signal(SIGQUIT, onintr_exit);
  signal(SIGKILL, onintr_exit);
  signal(SIGPIPE, onintr_exit);
  signal(SIGTERM, onintr_exit);

  /*emax_lmm_init();*/

  x11_open();

  restme();

  /*****************************************************************************/
  /* Main Loop start */
  /*****************************************************************************/
  while (alive) {
    alive = 0;
    if (flag & USE_PTHREAD) {
      for (cid=0; cid<MAXCORE; cid++) pthread_create(&th_p[cid], NULL, sim_core, (void*)((cid<<16)|PTHREAD_TICKS));
      for (did=0; did<MAXL2BK; did++) pthread_create(&th_d[did], NULL, sim_mreq, (void*)((did<<16)|PTHREAD_TICKS));
      for (mid=0; mid<MAXMMBK; mid++) pthread_create(&th_m[mid], NULL, sim_mem,  (void*)((mid<<16)|PTHREAD_TICKS));
      for (cid=0; cid<MAXCORE; cid++) pthread_join(th_p[cid], &tr_p[cid]);
      for (did=0; did<MAXL2BK; did++) pthread_join(th_d[did], &tr_d[did]);
      for (mid=0; mid<MAXMMBK; mid++) pthread_join(th_m[mid], &tr_m[mid]);
    }
    else {
      for (cid=0; cid<MAXCORE; cid++) sim_core((cid<<16)|STHREAD_TICKS);
      for (did=0; did<MAXL2BK; did++) sim_mreq((did<<16)|STHREAD_TICKS);
      for (mid=0; mid<MAXMMBK; mid++) sim_mem ((mid<<16)|STHREAD_TICKS);
    }

    for (tid=0; tid<MAXTHRD; tid++) {
      switch (t[tid].status) {
      case ARM_PTHREAD:
	/**********************************************************************************/
	/* _gettid:       svc   0x01001   no cache_sync                                   */
	/* _barrier:      svc   0x01002   barrier0 write b[pid]=%o0 and wait for all=%o0  */
        /*                bne   _barrier                                                  */
	/* pthread_create:svc	0x01003   no cache_sync                                   */
	/* pthread_join:  svc	0x01004   no cache_sync                                   */
	/* tcureg_valid:  svc   0x01010   tcureg_valid->x0                                */
	/* tcureg_ready:  svc   0x01011   1->tcureg_ready                                 */
	/* tcureg_last:   svc   0x01012   tcureg_last->x0                                 */
	/* tcureg_term:   svc   0x01013   1->tcureg_term                                  */
	/* tcureg         svc   0x01014   tcureg[3:0]->x3,2,1,0                           */
	/* getclk         svc   0x010fe   get cycle                                       */
	/* getpa          svc   0x010ff   display PA                                      */
	/**********************************************************************************/
	if (flag & TRACE_ARM)
	  printf("%03.3d:PT %08.8x_%08.8x %08.8x pth_opcd=%04.4x", tid,
		 (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), t[tid].ib.pc, t[tid].svc_opcd);
	switch (t[tid].svc_opcd) {
	case 0x001: /* _gettid() */
	  grw(tid, 0, (Ull)tid);
	  t[tid].status = ARM_NORMAL;
	  alive++;
	  break;
	case 0x002: /* _barrier(val) */
	  t[tid].barrier = (Uint)grr(tid, 0); /* read %o0 */
	  for (i=0; i<MAXTHRD; i++) {
	    if (t[i].status != ARM_COMPLETE && t[i].status != ARM_STOP && t[i].barrier != t[tid].barrier)
	      break;
	  }
	  if (i<MAXTHRD) { /* wait for barrier */
	    if (flag & TRACE_ARM)
	      printf(":BARRIER WAITING");
	    ccw(tid, 0LL);
	  }
	  else {
	    if (flag & TRACE_ARM)
	      printf(":BARRIER OK");
	    ccw(tid, (Ull)CC_Z);
	  }
	  t[tid].status = ARM_NORMAL;
	  alive++;
	  break;
	case 0x003: /* pthred_create(int id, NULL, (void*)func, void *arg) */
	  {
	    Uint target = (Uint)grr(tid, 0); /* 起動先tidを直接指定（本物pthreadと違う）自身と同じtidを指定するとエラー */
	    Uint func   = (Uint)grr(tid, 2);
	    Uint param  = (Uint)grr(tid, 3);

	    if (tid == target) {
	      if (flag & TRACE_ARM)
		printf(":ERROR pthread_create target=%d (==tid) (illegal specification)", target);
	      else
		printf("%03.3d:ERROR %08.8x_%08.8x pthread_create target=%d (==tid) (illegal specification)\n",
		       tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), target);
	      t[tid].status = ARM_STOP;
	    }
	    else if (target < MAXTHRD) {
	      /* 自身のmem-op完了待ち中は ARM_PTHREAD を維持 */
	      for (i=0; i<MAXL1BK; i++) {
		if (c[tid2cid(tid)].l1rq[i].v_stat && c[tid2cid(tid)].l1rq[i].tid == tid)
		  break;
	      }
	      if (i==MAXL1BK) {
		/* target起動 */
		grw(target, 30, 0LL); /* for detecting pthread exits */
		grw(target, 31, (Ull)(STACKINIT-STACKPERTHREAD*target));
		t[target].ib.pc = func;
		grw(target, 0, (Ull)param);
		t[target].status = ARM_NORMAL;
		if (flag & TRACE_ARM)
		  printf(":PTHREAD pthread_create target=%d\n", target);
		else
		  printf("%03.3d:PTHREAD %08.8x_%08.8x pthread_create target=%d func=%08.8x\n",
			 tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), target, func);
		t[tid].status = ARM_NORMAL;
	      }
	      alive++;
	    }
	    else {
	      if (flag & TRACE_ARM)
		printf(":ERROR pthread_create target=%d (>=MAXTHRD) (illegal specification)", target);
	      else
		printf("%03.3d:ERROR %08.8x_%08.8x pthread_create target=%d (>==MAXTHRD) (illegal specification)\n",
		       tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), target);
	      t[tid].status = ARM_NORMAL;
	      alive++;
	    }
	  }
	  break;
	case 0x004: /* pthread_join(int id, NULL) */
	  {
	    Uint target = (Uint)grr(tid, 0); /* 起動先tidを直接指定（本物pthreadと違う）自身と同じtidを指定するとエラー */

	    if (tid == target) {
	      if (flag & TRACE_ARM)
		printf(":ERROR pthread_join target=%d (==tid) (illegal specification)", target);
	      else
		printf("%03.3d:ERROR %08.8x_%08.8x pthread_join target=%d (==tid) (illegal specification)\n",
		       tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), target);
	      t[tid].status = ARM_STOP;
	    }
	    else if (target < MAXTHRD) {
	      int i;
	      /* target停止待ち中は ARM_PTHREAD を維持 */
	      for (i=0; i<MAXL1BK; i++) {
		if (c[tid2cid(target)].l1rq[i].v_stat && c[tid2cid(target)].l1rq[i].tid == target)
		  break;
	      }
	      if (i==MAXL1BK && (t[target].status == ARM_COMPLETE || t[target].status == ARM_STOP))
		t[tid].status = ARM_NORMAL;
	      alive++;
	    }
	    else {
	      if (flag & TRACE_ARM)
		printf(":ERROR pthread_join target=%d (>==MAXTHRD) (illegal specification)\n", target);
	      else
		printf("%03.3d:ERROR %08.8x_%08.8x pthread_join target=%d (>==MAXTHRD) (illegal specification)\n",
		       tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), target);
	      t[tid].status = ARM_NORMAL;
	      alive++;
	    }
	  }
	  break;
	case 0x010: /* fsm[x0].tcureg_valid->x0 */
	  grw(tid, 0, (Ull)get_tcureg_valid((Uint)grr(tid, 0), (Uint)grr(tid, 1)));
	  t[tid].status = ARM_NORMAL;
	  alive++;
	  break;
	case 0x011: /* fsm[x0].tcureg_ready=1 */
	  put_tcureg_ready((Uint)grr(tid, 0), (Uint)grr(tid, 1)); /* cid#,col# */
	  t[tid].status = ARM_NORMAL;
	  alive++;
	  break;
	case 0x012: /* fsm[x0].tcureg_last->x0 */
	  grw(tid, 0, (Ull)get_tcureg_last((Uint)grr(tid, 0), (Uint)grr(tid, 1)));
	  t[tid].status = ARM_NORMAL;
	  alive++;
	  break;
	case 0x013: /* fsm[x0].tcureg_term=1 */
	  put_tcureg_term((Uint)grr(tid, 0), (Uint)grr(tid, 1)); /* cid#,col# */
	  t[tid].status = ARM_NORMAL;
	  alive++;
	  break;
	case 0x014: /* fsm[x0].tcureg[3:0]->x3,2,1,0 */
	  i = (Uint)grr(tid, 0);
	  j = (Uint)grr(tid, 1);
	  grw(tid, 0, (Ull)get_tcureg(i, j, 0));
	  grw(tid, 1, (Ull)get_tcureg(i, j, 1));
	  grw(tid, 2, (Ull)get_tcureg(i, j, 2));
	  grw(tid, 3, (Ull)get_tcureg(i, j, 3));
	  t[tid].status = ARM_NORMAL;
	  alive++;
	  break;
	case 0x0fe: /* get cycle */
	  grw(tid, 0, t[tid].total_cycle);
	  t[tid].status = ARM_NORMAL;
	  alive++;
	  break;
	case 0x0ff: /* report and reset performance counter */
	  if (flag & TRACE_ARM)
	    printf("\n");
	  printpa();
	  t[tid].status = ARM_NORMAL;
	  alive++;
	  break;
	default:
	  t[tid].status = ARM_STOP;
	  break;
	}
	if (flag & TRACE_ARM)
	  printf("\n");
	break;
      case ARM_EXCSVC:
	/* L2(dirty)を全てDDRに書き戻す */
	/* この際にl2tag更新も必要 */
	/* L2DIRのDIRTYも消去が必要 */
	/* ★ただし，ZYNQ+EMAX6の場合,conf[][]はR/Oのためflush不要 */
	/* ★        regv[][]とlmmi[][]はACPバスを経由しL2から直接読み出すため，性能的にはL2のflushオーバヘッドは不要  */
	/* ★        ACPでもL2ミスすればDDRに行くので論理的にはDDR空間からのloadだが，性能的にはflushしないのと同じ    */
	/* ★        以上のことから，以下の区別によりZYNQ+ACP+EMAX6の性能モデル化か可能                                */
	/* ★   1. stack領域には，lmmi[][],regv[][]があり，機能的にはL2へのflushが必要．ただし性能オーバヘッドは不要   */
	/* ★   2. EMAX実行前は，dirtyを全てDDRに書き戻しつつ，flushcycleは計上しない．また，L2から消さずdirty=0に変更 */
        /* ★   3. EMAX実行中にtransactionが走ると,cacheを使用する.この時,dirty=1に戻るcacheが存在.                    */
	/*         EMAX本体は,主記憶を直接更新.当該領域はtransaction対象cache-lineとは異なる前提.                      */
	/* ★   4. EMAX実行後,transaction結果はcacheに残りdirty=1,EMAX本体の結果は主記憶が有効でcacheは古いまま        */
	/*         ただし,transactionの場合,EMAX本体は主記憶を更新しない前提でもよい(tricountは特に)                   */
	/*         ・transactionの結果はcacheに残すのでL2$無効化しない                                                 */
	/*         ・transactionがない場合(lmm_dirty=1が存在)はcacheから追い出してEMAX本体の結果をARMが参照可能とする  */

	/* EMAX5と異なり,LMM->DDRはARMによる明示的DMAなので,直前のDMAでFLUSH済のラインは対象外 */
	for (i=0; i<MAXL2BK; i++) {
	  for (j=0; j<L2WMAXINDEX; j++) {
	    for (k=0; k<L2WAYS; k++) {
	      int cid = tid2cid(tid);
	      if (c[cid].l2tag[i][j][k].v && c[cid].l2tag[i][j][k].dirty && (t[tid].svc_keep_or_drain || !c[cid].l2tag[i][j][k].drain)) { /* dirtyの場合主記憶へ書き戻す */
		Uint l2toa = (c[cid].l2tag[i][j][k].la*L2TAGMASK)|((j*MAXL2BK+i)*LINESIZE);
		c[cid].l2tag[i][j][k].drain = 1; /* mark */
		for (l=0; l<LINESIZE/8; l++)
		  mmw(l2toa+l*8, 0xffffffffffffffffLL, c[cid].l2line[i][j][k].d[l]);
#if 1
		/* ★   2. EMAX実行前のEMAX参照(malloc)領域はflushcycle計上.それ以外もDDRに書き戻すがflushcycleは計上ぜず,L2に残す */
		if (l2toa >= *(Ull*)&mem[0][HDRADDR] && l2toa < ALOCLIMIT) {
		  t[tid].total_cycle       +=LINESIZE/8; /* ★L2flush コスト加算の考え方に関しては上記コメント参照 */
		  t[tid].pa_cycle          +=LINESIZE/8; /* ★L2flush コスト加算の考え方に関しては上記コメント参照 */
		  t[tid].pa_svcL2flushcycle+=LINESIZE/8; /* ★L2flush コスト加算の考え方に関しては上記コメント参照 */
		  /* lmmwb=0の場合,EMAXによるDDR更新はないので,L1とL2はdirtyのままでOK */
		  /* lmmwb=1の場合,EMAXによるDDR更新が有り得るのでL1とL2.dirtyは0に戻す */
		  if (t[tid].svc_keep_or_drain) { /* 0:keep, 1:drain */
		    d[i].l2dir[c[cid].l2tag[i][j][k].la*L2WMAXINDEX+j].l2dir_d[cid/64] &= ~(1LL<<(cid%64));
		    c[cid].l2tag[i][j][k].dirty = 0;
		  }
		}
#endif
	      }
	    }
	  }
	}
	switch (exec_svc(tid, t[tid].svc_opcd)) { /* この時点で実際にSVCをHOST実行 */
	case SVC_MEM_UPDATE:
	  for (i=0; i<MAXL2BK; i++) {
	    for (j=0; j<L2WMAXINDEX; j++) {
	      for (k=0; k<L2WAYS; k++) {
		int cid = tid2cid(tid);
		if (c[cid].l2tag[i][j][k].v) {
		  d[i].l2dir[c[cid].l2tag[i][j][k].la*L2WMAXINDEX+j].l2dir_v[cid/64] &= ~(1LL<<(cid%64));
		  c[cid].l2tag[i][j][k].v = 0;
		}
	      }
	    }
	  }
	case SVC_MEM_NOUPDATE:
	  t[tid].status = ARM_NORMAL;
	  break;
	case SVC_EMAX:
	  /* EMAX5と異なり,LMM->DDRはARMによる明示的DMAなので,ここでのFLUSHはタイミングが早いが,後続LMM->DMA完了までALOC領域を参照することはないので問題無し */
	  if (t[tid].svc_keep_or_drain) {
	    for (i=0; i<MAXL2BK; i++) {
	      for (j=0; j<L2WMAXINDEX; j++) {
		for (k=0; k<L2WAYS; k++) {
		  int cid = tid2cid(tid);
		  if (c[cid].l2tag[i][j][k].v) {
#if 1
		    Uint l2toa = (c[cid].l2tag[i][j][k].la*L2TAGMASK)|((j*MAXL2BK+i)*LINESIZE);
		    /* ★   3. EMAX実行後のEMAX参照(malloc)領域はL2から消去.それ以外はL2に残す */
		    if (l2toa >= *(Ull*)&mem[0][HDRADDR] && l2toa < ALOCLIMIT) {
		      d[i].l2dir[c[cid].l2tag[i][j][k].la*L2WMAXINDEX+j].l2dir_v[cid/64] &= ~(1LL<<(cid%64));
		      c[cid].l2tag[i][j][k].v = 0;
		    }
#endif
		  }
		}
	      }
	    }
	  }
	  t[tid].status = ARM_NORMAL;
	  break;
	case SVC_ARM_COMPLETE:
	  printf("\033[7m%03.3d:EXCSVC ARM normal end\033[0m\n", tid);
	  t[tid].status = ARM_COMPLETE;
	  break;
	case SVC_ARM_STOP:
	  printf("\033[7m%03.3d:EXCSVC ARM undefined sys_call=%d\033[0m\n", tid, t[tid].svc_opcd);
	  t[tid].status = ARM_STOP;
	  break;
	}
	alive++;
        break;
      case ARM_NORMAL:
      case ARM_FLUSH:
	alive++;
        break;
      }
    }
  }

exit:
  onintr_exit(0);
}

/*****************************************************************************/

void onintr_exit(x) int x;
{
  Uchar *memp;
  Uint  tid, cid, did, mid;
  int   i, j;

  memp = &mem[0][HDRADDR+8]; /* latest malloc pointer */
  printf("arm_malloc_top = 0x%08.8x\n", *(int*)memp);

  if (x == 0) {
    printf("==== Program normal end. ==== Hit any key in X.\n");
    while (x11_checkevent());
  }
  else
    printf("==== Interrupt end. ====\n");

  printf("====debug information (all bitmaps should be 0)====\n");

  printf("l2rq.v_stat:");
  for (cid=0; cid<MAXCORE; cid++) {
    for (i=0; i<MAXL2BK; i++)
      printf("%01.1x ", c[cid].l2rq[i].v_stat);
  }
  printf("\n");

  for (did=0; did<MAXL2BK; did++) {
    printf("d[%02.2d].bm/lk:", did);
    for (cid=0; cid<MAXCORE; cid++)
      printf("%01.1x", d[did].l2rq_bitmap[cid]);
    for (i=MAXCORbitmaps-1; i>=0; i--)
      printf("/%08.8x_%08.8x ", (Uint)(d[did].l2rq_lock[i]>>32), (Uint)(d[did].l2rq_lock[i]));
    printf("\n");
  }

  printf("====execution time====\n");
  memp = &mem[0][HDRADDR+8]; /* latest malloc pointer */
  printf("exec_ptime=%d/%d(_SC_CLK_TCK)\n", gettme(), sysconf(_SC_CLK_TCK));
  printf("malloc_top=%08.8x\n", *memp<<24|*(memp+1)<<16|*(memp+2)<<8|*(memp+3));

  printpa();

  show_rutil();

  exit(x);
}

char *
version()
{
  char *i;

  for (i=RcsHeader; *i && *i!=' '; i++);
  for (           ; *i && *i==' '; i++);
  for (           ; *i && *i!=' '; i++);
  for (           ; *i && *i==' '; i++);
  return (i);
}

/*****************************************************************************/

chck_svc(tid, opcd) Uint tid; Uint opcd;
{
  Uint   addr, len;
  Uint   val, retval;

  switch (opcd) {
  case 0xf0: /* emax6_pre_with_keep_cache (transaction) */
  case 0xfd: /* _copyX */
  case 0xfe: /* _updateX */
  case 0x69: /* write */
  case 0x66: /* open */
  case 0x6b: /* seek */
  case 0x68: /* close */
  case 0x6e: /* isatty */
  case 0x20: /* sbrk */
    return (0);
  case 0xf1: /* emax6_pre_with_drain_cache (normal array) */
  case 0x11: /* _exit */
  case 0x6a: /* read */
  case 0x21: /* fstat */
    return (1);
  case 0x22: /* times */
    addr = (Uint)grr(tid, 0);
    if (!addr)
      return (0);
    else
      return (1);
  default:
    return (0);
  }
}

exec_svc(tid, opcd) Uint tid; Uint opcd;
{
  Uint   color = 0;
  Uint   addr, len;
  Uint   val, retval;
  Uint   gr0, gr1;
  unsigned char *memp;
  struct stat fstatbuf;
  int fstatval;

  if (flag & TRACE_ARM)
    printf("%03.3d:SVC %08.8x_%08.8x exec_svc: opcd=0x%x\n", tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), opcd);

  switch (opcd) {
  case 0xf0: /* emax6_pre_with_keep_cache (transaction) */
    printf("%03.3d:SVC %08.8x_%08.8x emax6_pre_with_keep_cache (transaction)\n", tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps));
    retval = SVC_EMAX;
    goto end;
  case 0xf1: /* emax6_pre_with_drain_cache (normal array) */
    printf("%03.3d:SVC %08.8x_%08.8x emax6_pre_with_drain_cache (normal array)\n", tid, (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps));
    retval = SVC_EMAX;
    goto end;
  case 0xfd: /* _copyX */
    gr0 = (Uint)grr(tid, 0); /* id */
    gr1 = (Uint)grr(tid, 1); /* *from */
    BGR_to_X(gr0, &mem[0][gr1]);
    grw(tid, 0, 0LL);
    retval = SVC_MEM_NOUPDATE;
    goto end;
  case 0xfe: /* _updateX */
    x11_update();
    grw(tid, 0, 0LL);
    retval = SVC_MEM_NOUPDATE;
    goto end;
  case 0x11: /* _exit */
    retval = SVC_ARM_COMPLETE;
    goto end;
  case 0x6a: /* read */
    addr = (Uint)grr(tid, 1);
    len  = (Uint)grr(tid, 2);
    val  = read((Uint)grr(tid, 0), &mem[0][addr], len);
    grw(tid, 0, (Sll)val);
    /* printf("read:addr=%x len=%d val=%d\n", addr, len, val); */
    retval = SVC_MEM_UPDATE;
    goto end; /* update mem */
  case 0x69: /* write */
    addr = (Uint)grr(tid, 1);
    len  = (Uint)grr(tid, 2);
    if ((Uint)grr(tid, 0) == 1) {
      color = 1;
      write(1, "\033[36;2m", 7);
    }
    val = write((Uint)grr(tid, 0), &mem[0][addr], len);
    grw(tid, 0, (Sll)val);
    /* printf("write:addr=%x len=%d val=%d\n", addr, len, val); */
    if (color) write(1, "\033[0m", 4);
    retval = SVC_MEM_NOUPDATE;
    goto end;
  case 0x66: /* open */
    if (!strcmp((char*)&mem[0][(Uint)grr(tid, 0)], ":tt")) {
      if ((Uint)grr(tid, 1) == 0) /* mode="r" */
	grw(tid, 0, 0LL);
      else /* mode="w" */
	grw(tid, 0, 1LL);
    }
    else {
      int fd;
      if ((Uint)grr(tid, 1)&4)
	grw(tid, 0, (Sll)open(&mem[0][(Uint)grr(tid, 0)], O_CREAT | O_TRUNC | O_WRONLY, 0644));
      else
	grw(tid, 0, (Sll)open(&mem[0][(Uint)grr(tid, 0)], (Uint)grr(tid, 1), (Uint)grr(tid, 2)));
    }
    retval = SVC_MEM_NOUPDATE;
    goto end;
  case 0x6b: /* seek */
    grw(tid, 0, (Sll)lseek((Uint)grr(tid, 0), (Uint)grr(tid, 1), (Uint)grr(tid, 2)));
    retval = SVC_MEM_NOUPDATE;
    goto end;
  case 0x68: /* close */
    grw(tid, 0, (Sll)close((Uint)grr(tid, 0)));
    retval = SVC_MEM_NOUPDATE;
    goto end;
  case 0x6e: /* isatty */
    grw(tid, 0, (Sll)isatty((Uint)grr(tid, 0)));
    retval = SVC_MEM_NOUPDATE;
    goto end;
  case 0x20: /* sbrk */
    len = (Uint)grr(tid, 0);
    memp = &mem[0][HDRADDR+8]; /* malloc top */
    if (*(Ull*)memp + (Ull)len > (Ull)ALOCLIMIT) {
      printf("%03.3d:SVC %08.8x_%08.8x %08.8x sbrk size=%08.8x exceeds ALOCLIMIT\n", tid,
		 (Uint)(t[tid].total_steps>>32), (Uint)(t[tid].total_steps), t[tid].ib.pc, len);
      retval = SVC_ARM_COMPLETE;
      goto end;
    }
    grw(tid, 0, *(Ull*)memp);
    *(Ull*)memp += (Ull)len;
    retval = SVC_MEM_NOUPDATE;
    goto end;
  case 0x21: /* fstat */
    fstatval = fstat((Uint)grr(tid, 0), &fstatbuf);
    *(Ushort*)(&mem[0][(Uint)grr(tid, 1)]+ 0) = fstatbuf.st_dev;
    *(Ushort*)(&mem[0][(Uint)grr(tid, 1)]+ 2) = fstatbuf.st_ino;
    *(Uint*)  (&mem[0][(Uint)grr(tid, 1)]+ 4) = fstatbuf.st_mode;
    *(Ull*)   (&mem[0][(Uint)grr(tid, 1)]+16) = fstatbuf.st_size;
    grw(tid, 0, (Sll)fstatval);
    retval = SVC_MEM_UPDATE;
    goto end;
  case 0x22: /* times */
    addr = (Uint)grr(tid, 0);
    if (!addr) {
      grw(tid, 0, t[tid].total_cycle);
      retval = SVC_MEM_NOUPDATE;
    }
    else {
      memp = &mem[0][addr];
      *((Uint*)memp+0) = (Uint)(t[tid].total_cycle/1024); /* tms_utime */
      *((Uint*)memp+1) = 0;                               /* tms_stime */
      *((Uint*)memp+2) = (Uint)(t[tid].total_cycle/1024); /* tms_cutime */
      *((Uint*)memp+3) = 0;                               /* tms_stime */
      retval = SVC_MEM_UPDATE;
    }
    goto end;
  default:
    retval = SVC_ARM_STOP;
    goto end; /* undefined */
  }

end:
  return (retval);
}

struct tms      utms;
long            tmssave;

restme()
{
        times(&utms);
        tmssave = utms.tms_utime;
}

long gettme()
{
        times(&utms);
        return (utms.tms_utime-tmssave);
}

struct rusage rusage;

printpa()
{
  int tid, i, j, k;

  printf("====PE steps, cycles, cache statistics (l1:I$ d1:D$ l2:incore-L2$ g2:other-L2$)====\n");
  for (tid=0; tid<MAXTHRD; tid++) {
    printf("%03.3d:step=%08.8x_%08.8x cycle=%08.8x_%08.8x i1(%5.1f%%)wait=%08.8x_%08.8x d1(%5.1f%% hit=%08.8x_%08.8x mis=%08.8x_%08.8x)wait=%08.8x_%08.8x l2(%5.1f%% hit=%08.8x_%08.8x mis=%08.8x_%08.8x) g2(%5.1f%% hit=%08.8x_%08.8x mis=%08.8x_%08.8x) flush(L1->%08.8x_%08.8xcycle, L2->%08.8x_%08.8xcycle)\n",
	   tid,
	   (Uint)(t[tid].pa_steps>>32),
	   (Uint)(t[tid].pa_steps),
	   (Uint)(t[tid].pa_cycle>>32),
	   (Uint)(t[tid].pa_cycle),
	   (double)(t[tid].pa_i1hit)*100.0/(t[tid].pa_i1hit+t[tid].pa_i1mis),
	   (Uint)(t[tid].pa_i1waitcycle>>32),
	   (Uint)(t[tid].pa_i1waitcycle),
	   (double)(t[tid].pa_d1hit)*100.0/(t[tid].pa_d1hit+t[tid].pa_d1mis),
	   (Uint)(t[tid].pa_d1hit>>32), (Uint)t[tid].pa_d1hit,
	   (Uint)(t[tid].pa_d1mis>>32), (Uint)t[tid].pa_d1mis,
	   (Uint)(t[tid].pa_d1waitcycle>>32),
	   (Uint)(t[tid].pa_d1waitcycle),
	   (double)(t[tid].pa_l2hit)*100.0/(t[tid].pa_l2hit+t[tid].pa_l2mis),
	   (Uint)(t[tid].pa_l2hit>>32), (Uint)t[tid].pa_l2hit,
	   (Uint)(t[tid].pa_l2mis>>32), (Uint)t[tid].pa_l2mis,
	   (double)(t[tid].pa_g2hit)*100.0/(t[tid].pa_g2hit+t[tid].pa_g2mis),
	   (Uint)(t[tid].pa_g2hit>>32), (Uint)t[tid].pa_g2hit,
	   (Uint)(t[tid].pa_g2mis>>32), (Uint)t[tid].pa_g2mis,
	   (Uint)(t[tid].pa_svcL1flushcycle>>32), (Uint)t[tid].pa_svcL1flushcycle,
	   (Uint)(t[tid].pa_svcL2flushcycle>>32), (Uint)t[tid].pa_svcL2flushcycle);
  }

#if 0
  printf("====THREAD instruction counts (over 5%%)====");
  for (tid=0; tid<MAXTHRD; tid++) {
    int last_th = -1;
    extern Uchar **adis;
    for (i=0; i<128; i++) {
      if ((double)t[tid].insn_count[i]/(double)(t[tid].insn_total+1) > 0.05) {
	if (last_th != tid)
	  printf("\nth%03.3d:", tid);
	last_th = tid;
	printf("%5.1f%%:%s", (double)t[tid].insn_count[i]*100.0/(double)(t[tid].insn_total+1), adis[i]);
      }
    }
  }
  printf("\n");
#endif

  fflush(stdout);

  for (tid=0; tid<MAXTHRD; tid++) {
    t[tid].pa_steps       = 0LL;
    t[tid].pa_cycle       = 0LL;
    t[tid].pa_i1hit       = 0LL;
    t[tid].pa_i1mis       = 0LL;
    t[tid].pa_i1waitcycle = 0LL;
    t[tid].pa_d1hit       = 0LL;
    t[tid].pa_d1mis       = 0LL;
    t[tid].pa_d1waitcycle = 0LL;
    t[tid].pa_l2hit       = 0LL;
    t[tid].pa_l2mis       = 0LL;
    t[tid].pa_g2hit       = 0LL;
    t[tid].pa_g2mis       = 0LL;
    t[tid].pa_svcL1flushcycle = 0LL;
    t[tid].pa_svcL2flushcycle = 0LL;
    t[tid].insn_total = 0LL;
    for (i=0; i<128;i++)
      t[tid].insn_count[i] = 0LL;
  }
}

show_rutil()
{
  long ticks;
  times(&utms);
  ticks = utms.tms_utime-tmssave+1;

  printf("====SELF===\n");
  getrusage(RUSAGE_SELF, &rusage);
  printf("\033[31;1m ru_utime   = %d.%06dsec ", rusage.ru_utime.tv_sec, rusage.ru_utime.tv_usec);
  printf(" ru_stime   = %d.%06dsec\033[0m\n", rusage.ru_stime.tv_sec, rusage.ru_stime.tv_usec);
  printf(" ru_maxrss  = %6dKB  ", rusage.ru_maxrss);          /* max resident set size */
  printf(" ru_ixrss   = %6dKB  ", rusage.ru_ixrss/ticks);     /* integral shared text memory size */
  printf(" ru_idrss   = %6dKB  ", rusage.ru_idrss/ticks);     /* integral unshared data size */
  printf(" ru_isrss   = %6dKB\n", rusage.ru_isrss/ticks);   /* integral unshared stack size */
  printf(" ru_minflt  = %8d  ", rusage.ru_minflt);          /* page reclaims */
  printf(" ru_majflt  = %8d  ", rusage.ru_majflt);          /* page faults */
  printf(" ru_nswap   = %8d  ", rusage.ru_nswap);           /* swaps */
  printf(" ru_inblock = %8d\n", rusage.ru_inblock);         /* block input operations */
  printf(" ru_oublock = %8d  ", rusage.ru_oublock);         /* block output operations */
  printf(" ru_msgsnd  = %8d  ", rusage.ru_msgsnd);          /* messages sent */
  printf(" ru_msgrcv  = %8d  ", rusage.ru_msgrcv);          /* messages received */
  printf(" ru_nsignals= %8d\n", rusage.ru_nsignals);        /* signals received */
  printf(" ru_nvcsww  = %8d  ", rusage.ru_nvcsw);           /* voluntary context switches */
  printf(" ru_nivcsw  = %8d\n", rusage.ru_nivcsw);          /* involuntary context switches */

  printf("====CHILD===\n");
  getrusage(RUSAGE_CHILDREN, &rusage);
  printf("\033[31;1m ru_utime   = %d.%06dsec ", rusage.ru_utime.tv_sec, rusage.ru_utime.tv_usec);
  printf(" ru_stime   = %d.%06dsec\033[0m\n", rusage.ru_stime.tv_sec, rusage.ru_stime.tv_usec);
  printf(" ru_maxrss  = %6dKB  ", rusage.ru_maxrss);          /* max resident set size */
  printf(" ru_ixrss   = %6dKB  ", rusage.ru_ixrss/ticks);     /* integral shared text memory size */
  printf(" ru_idrss   = %6dKB  ", rusage.ru_idrss/ticks);     /* integral unshared data size */
  printf(" ru_isrss   = %6dKB\n", rusage.ru_isrss/ticks);   /* integral unshared stack size */
  printf(" ru_minflt  = %8d  ", rusage.ru_minflt);          /* page reclaims */
  printf(" ru_majflt  = %8d  ", rusage.ru_majflt);          /* page faults */
  printf(" ru_nswap   = %8d  ", rusage.ru_nswap);           /* swaps */
  printf(" ru_inblock = %8d\n", rusage.ru_inblock);         /* block input operations */
  printf(" ru_oublock = %8d  ", rusage.ru_oublock);         /* block output operations */
  printf(" ru_msgsnd  = %8d  ", rusage.ru_msgsnd);          /* messages sent */
  printf(" ru_msgrcv  = %8d  ", rusage.ru_msgrcv);          /* messages received */
  printf(" ru_nsignals= %8d\n", rusage.ru_nsignals);        /* signals received */
  printf(" ru_nvcsww  = %8d  ", rusage.ru_nvcsw);           /* voluntary context switches */
  printf(" ru_nivcsw  = %8d\n", rusage.ru_nivcsw);          /* involuntary context switches */
}
