
/* MC Memory Simulator                 */
/*        Copyright (C) 2014 by NAIST. */
/*         Primary writer: Y.Nakashima */
/*                nakashim@is.naist.jp */

/* csim.h 2005/7/18 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <pthread.h>
#include <signal.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>

#ifndef UTYPEDEF
#define UTYPEDEF
typedef unsigned char      Uchar;
typedef unsigned short     Ushort;
typedef unsigned int       Uint;
typedef unsigned long long Ull;
typedef long     long int  Sll;
#if __AARCH64EL__ == 1
typedef long double Dll;
#else
typedef struct {Ull u[2];} Dll;
#endif
#endif

/* ARM structure */

Uint  ex_srr(), ex_drw(), rt_drw(), grw(), ccw();
Ull   grr();
Ull   mmr_chkc(), mmw_chkc(), mmr(), mmw();
void  *sim_core(), *sim_mreq(), *sim_mem();
char  *version();
long  gettme();
void  onintr_exit();

#define MAXINT          0x7fffffff
#define HANGUP_LIMIT    1000000
#define PTHREAD_TICKS   10000
#define STHREAD_TICKS   1

#define USE_PTHREAD     0x00000001
#define IMAGEWIN        0x00000002
#define TRACE_PIPE      0x00000004
#define TRACE_RANGE     0x00000008
#define TRACE_ARM       0x00000010
#define TRACE_EMAX      0x00000020
#define DUMP_DDR        0x00000040

#define CC_N            0x8LL
#define CC_Z            0x4LL
#define CC_C            0x2LL
#define CC_V            0x1LL

/* (*default_conf)  DeepROB   MS   GPU   Phi  EMAX6              */
/* MAXTHRD               2    64  2048   256    32*  tid         */
/* MAXCORE               1    32    64    64     8*  cid .. EMAX */
/* CORE_ROBSIZE         16*   16*  256     4    16*              */
/* USE_EMAX              -     -     -     -     8               */
/* MAXL1BK               1    32    64*   64*   64*  did         */
/* MAXL2BK               1    32    64*   64*   64*  did         */
/* MAXMMBK               1    32    64     8*    8*  mid .. TRAN */

#define MAXPREFETCH     16       /* not used */
/*¡ú¡ú¡ú#define MAXTHRD 256      /* physical threads (should be MAXCORE*int) */
/*¡ú¡ú¡ú#define MAXCORE 64       /* physical cores   ( any integer is OK )   */
#define MAXTHRD         16       /* physical threads (should be MAXCORE*int) */
#define MAXCORE         4        /* physical cores   ( any integer is OK )   */
#define ULLBITS         64
#define MAXCORbitmaps  ((MAXCORE+63)/64)
#define THR_PER_CORE   (MAXTHRD/MAXCORE) /* max 32 */
#define tid2cid(tid)   (tid%MAXCORE)

#define CORE_ROBSIZE    17       /* actives are CORE_ROBSIZE-1 */
#define CORE_IMLPIPE    4
#define CORE_VECPIPE    4
#define CORE_DIVDELAY   16
#
#define LINESIZE        64       /* 64B */
#define MAXL1BK         8        /* 8bank */
#define I1SIZE          16384
#define I1WAYS          4
#define I1WMAXINDEX    (I1SIZE/I1WAYS/MAXL1BK/LINESIZE) /* 16 */
#define I1TAGMASK      (I1WMAXINDEX*MAXL1BK*LINESIZE) /* 8192(lower-13bit) */
#define I1DELAY         16
#define D1SIZE          16384
#define D1WAYS          4
#define D1WMAXINDEX    (D1SIZE/D1WAYS/MAXL1BK/LINESIZE) /* 16 */
#define D1TAGMASK      (D1WMAXINDEX*MAXL1BK*LINESIZE) /* 8192(lower-13bit) */
#define D1DELAY         16
#define MAXL2BK         8        /* 8bank */
#define L2SIZE          131072
#define L2WAYS          4
#define L2WMAXINDEX    (L2SIZE/L2WAYS/MAXL2BK/LINESIZE) /* 128 */
#define L2TAGMASK      (L2WMAXINDEX*MAXL2BK*LINESIZE) /* 131072(lower-17bit) */
/*¡ú¡ú¡ú ARM:2.4GHz   EMAX6:1.2GHz  */
#define ARM_EMAX6_RATIO 2
/*¡ú¡ú¡ú ARM:50cycle EMAX6:25cycle */
#define L2DIRDL         50
/*¡ú¡ú¡ú ARM:100cycle EMAX6:50cycle */
#define CCDELAY         100
/*¡ú¡ú¡ú ARM:150cycle EMAX6:75cycle */
#define MMDELAY         150
#define MAXMMBK         4        /* 4bank */
#define MEMINTERLEAVE   64       /* 64B */

#define MEMTOP          0x00000000
#define MINADDR         0x00000004              /* minimum address is 4B         */
#define HDRADDR         0x00001000              /* default header is 4KB         */
#define PARAM           0x00001080              /* default header is 4KB         */
#define DEFAULTSTART    0x00010000              /* default entry                 */
#define ALOCLIMIT       0x4dffff00              /* global variables:1.5GB        */
#define STACKPERTHREAD  0x00010000              /* stack/thread:64KB(0x10000)    */
#define STACKINIT       0x4fffff00              /* MAXTHRD:1-512 STACK:0x02000000 */
#define MEMSIZE         0x50000000              /* ARM_MEM */
#define DMA_BASE2_PHYS	0x50000000              /* keep same as in emax6lib.c    */
#define REG_BASE2_PHYS	0x50100000              /* keep same as in emax6lib.c    */
#define LMM_BASE2_PHYS	0x60000000              /* keep same as in emax6lib.c    */
#define MEM_VALID_ADDR	0xafffffff              /* keep same as in emax6lib.c    */

Uint       flag; /* control_flag */
Uint       trace_on, trace_off;
Uint       dump_ddr_done; /* 0 -> 1 */
FILE       *fp_conf, *fp_lmmi, *fp_regv, *fp_load;

pthread_t th_p[MAXCORE],  th_d[MAXL2BK],  th_m[MAXMMBK];
void     *tr_p[MAXCORE], *tr_d[MAXL2BK], *tr_m[MAXMMBK];

enum { ARM_NORMAL, ARM_FLUSH, ARM_PTHREAD, ARM_EXCSVC, ARM_EXCEMAX, ARM_COMPLETE, ARM_STOP };
enum { IB_EMPTY, IB_ISSUED, IB_VALID };
enum { SVC_MEM_NOUPDATE, SVC_MEM_UPDATE, SVC_EMAX, SVC_ARM_COMPLETE, SVC_ARM_STOP };
struct t {
  int      hangup_counter;
  int      o_stat;            /* processor status        */
  int      status;            /* processor status        */
  Uint     svc_opcd;          /* svc opcd                */
  Uint     svc_keep_or_drain; /* 0:keep-cache, 1:drain_cache */
  int      barrier;           /* for _barrier()          */
  Ull      total_steps;       /* original steps          */
  Ull      total_cycle;       /* real cycles             */
  Ull      insn_total;
  Ull      insn_count[128];
  Ull      pa_steps;
  Ull      pa_cycle;
  Ull      pa_i1hit;
  Ull      pa_i1mis;
  Ull      pa_i1waitcycle;    /* number of if-cache miss cycle */
  Ull      pa_d1hit;
  Ull      pa_d1mis;
  Ull      pa_d1waitcycle;    /* number of op-cache miss cycle */
  Ull      pa_l2hit;
  Ull      pa_l2mis;
  Ull      pa_g2hit;
  Ull      pa_g2mis;
  Ull      pa_svcL1flushcycle; /* before starting SVC (including EMAX) */
  Ull      pa_svcL2flushcycle; /* before starting SVC (including EMAX) */

  struct ib {
    Uint     status;/* 0:idle, 1:ifqueue registered, 2:ifqueue done+ib_valid */
    Uint     mc;    /* for LDM/STM counting */
    Uint     pc;    /* pc of instrucion */
    Uint     insn;  /* valid instrucion */
  } ib;

#define USRREGTOP 0
#define USRREG    32
  Ull      usr[USRREG]; /* R0-30, R31:SP */
                     /* PC is not implemented here */
#define AUXREGTOP 32
#define AUXREG    4
  Ull      aux[AUXREG]; /* AUX reg for LDM/STM */
#define CPSREGTOP 36
#define CPSREG    1
  Ull      cpsr;     /* bit31-28(NZCV) bit6-5(FT) bit4-0(M4-0) */
                     /* M4-0: 0b10000=User       0b10001=FIQ   0b10010=IRQ       */
                     /*       0b10011=Supervisor 0b10111=Abort 0b11011=Undefined */
                     /*       0b11111=System                                     */
#define VECREGTOP 64
#define VECREG    32
  union vec {
    Uchar  b[16];
    Ushort h[8];
    Uint   s[4];
    Ull    d[2];
    /*     q[1];*/
  } vec[32]; /* each has 128bits */
#define MAXLREG   128

  struct map { /* logical# -> ROB# */
    Uint  x      :  2; /* 0:invalid 1/2/3:logical# is mapped on ROB.dst7/8/9 */
    Uint  rob    : 10; /* ROB entry¡Êretire»þ¤ËÈæ³Ó¤·°ìÃ×¤¹¤ì¤Ðv=0¤ËÌá¤¹.ÉÔ°ìÃ×¤Î¾ì¹çmap°Ý»ý¡Ë*/
  } map[MAXLREG]; /* 32+1+...+32 */

} t[MAXTHRD];

enum { ROB_EMPTY, ROB_MAPPED, ROB_ISSUED, ROB_STREXWAIT, ROB_COMPLETE, ROB_D1WRQWAIT, ROB_RESTART, ROB_DECERR, ROB_EXECERR };
           /* case load:  ROB_ISSUED -> ROB_COMPLETE -> ROB_D1RQWAIT(l1rq_full) -> ROB_EMPTY(l1rq_queued) */ 
           /* case other: ROB_ISSUED -> ROB_COMPLETE -> ROB_EMPTY */
struct c { /* physical core */
  Uint if_nexttid; /* round robin for IF */
  Uint rob_nexttid; /* round robin for ROB */

  Uint rob_top; /* next entry to be filled */
  Uint rob_bot; /* last entry to be dequeued (top==bot): empty */
  struct rob { /* LDM¤Î¾ì¹çÊ£¿ô¥¨¥ó¥È¥ê¾ÃÈñ */
    Uint  stat   :  4; /* 0:empty 1:mapped 2:issued 3:strexwait 4:complete 5:d1wrqwait, 6:restart_pipe 7:decode_err 8:exec_err */
    Uint  term   :  1; /* final ROB entry of multiple ROBs */
    Uint  tid    : 12; /* thread id */
    Uint  pc         ; /* copy of current PC */
    Ull   target     ; /* copy of branch target */
    Uint  bpred  :  1; /* 0:not taken, 1:taken */
    Uint  cond   :  4;
    Uint  type   :  3; /* 0:ALU, 1:MUL, 2:VXX, 3:LD, 4:ST, 5:BRANCH, 6:SVC 7:PTHREAD                      */
    Uint  opcd   :  4; /*   ALU... 0:AND,  1:EOR,  2:SUB,  3:ORR, 4:ADD, 5:ADC, 6:SBC, 7:MOV              */
                       /*          8:SBFM, 9:UBFM, A:CSEL, B:CCM, C:REV, D:CLS                            */
                       /*   MUL... 0:UMADDL,1:UMSUBL,2:SMADDL,3:SMSUBL,4:MADD,5:MSUB,6:UDIV,7:SDIV        */
                       /*   VXX... 0:FMOV,1:FCVT,2:CVTSX,3:CVTUX,4:SHIFT,5:CMP,6:MOV                      */
                       /*          7:DUP/XTN/UMOV/INS/MAXMIN                                              */
                       /*                            sint<>sf sint<>df uint<>sf uint>?df                  */
                       /*          8:FADD, 9:FMLA, 10:FMUL, 11:MLA, 12:FADDS, 13:FMADD, 14:FMUL, 15:FDIV  */
                       /*   LD.... 0:LDRB, 1:LDRH, 2:LDRW, 3:LDR,  4:LDRSB, 5:LDRSH, 6:LDRSW, 7:LDREX     */
                       /*          8:VLDRB, 9:VLDRH,10:VLDRS,11:VLDRD,12:VLDRQ                            */
                       /*   ST.... 0:STRB, 1:STRH, 2:STRW, 3:STR                                          */
                       /*          8:VSTRB, 9:VSTRH,10:VSTRS,11:VSTRD,12:VSTRQ                            */
                       /*   BRANCH 0:B, 1:BL, 2:CBZ, 3:CBNZ, 4:TBZ, 5:TBNZ, 6:BLR, 7:BR                   */
                       /*   SVC... 0:SVC                                                                  */
                       /*   PTHREAD... --                                                                 */
    Uint  sop    :  4; /*   ALU... 0:LSL, 1:LSR, 2:ASR, 3:ROR                                             */
                       /*          8:uxtb, 9:uxth, 10:uxtw, 11:uxtx, 12:sxtb, 13:sxth, 14:sxtw, 15:sxtx   */
                       /*   LD.... 8:uxtb, 9:uxth, 10:uxtw, 11:uxtx, 12:sxtb, 13:sxth, 14:sxtw, 15:sxtx   */
                       /*   ST.... 8:uxtb, 9:uxth, 10:uxtw, 11:uxtx, 12:sxtb, 13:sxth, 14:sxtw, 15:sxtx   */
                       /*   VXX... 0/8:rint, 1/9:ceil, 2/10:floor, 3/11:trunc, 4/12:round                 */
    Uint  ptw    :  1; /*   LD/ST  0:full_reg_write, 1:partial_reg_write                                  */
    Uint  size   :  4; /*   VXX... size                                                                   */
    Uint  idx    :  4; /*   LD.... reg_index 15-0 byte-portion                                            */
                       /*   ST.... reg_index 15-0 byte-portion                                            */
    Uint  rep    :  1; /*   LD.... 0:normal, 1:load repeat in reg                                         */
    Uint  dir    :  1; /*   VXX... 0:VR->RR, 1:RR<-VR                                                     */
    Uint  iinv   :  1; /*   0:operand2, 1:~operand2                                                       */
    Uint  oinv   :  1; /*   0:result, 1:~result                                                           */
    Uint  dbl    :  1; /*   LD.... 0:LD(single), 1:LD(double) XR64bit-VR128bit                            */
                       /*   ST.... 0:ST(single), 1:ST(double) XR64bit-VR128bit                            */
                       /*   VXX... 0:Rreg=32bit, 1:Rreg=64bit                                             */
    Uint  plus   :  1; /*   0: eag sub.     1:eag add                                                     */
    Uint  pre    :  1; /*   0: post_update, 1:pre_update                                                  */
    Uint  wb     :  1; /*   0: normal,      1:addr_writeback                                              */
    Uint  updt   :  1; /*   S-bit                                                                         */

    struct sr {      /*   6R/3W + conditional 3R -> MAXIMUM 9R/3W required in ROB */
      Uint  t  :  2; /* 0:invalid 1:imm 2:reg 3:wait */             /* source #1.low  */
      Uint  x  :  2; /* 0:logical# 1/2/3:ROB.dstX   */              /* source #1.low  */
      Ull   n;       /* imm/regno: usr[31],SP,---,vec[32] or ROB# *//* source #1.low  */
    } sr[6];
    struct dr {
      Uint  t  :  1; /* 0:invalid 1:valid */            /* for DRT low  */
      Uint  n  :  7; /* regno: usr[31],SP,---,vec[32] *//* for DRT low  */
      Uint  p  : 10; /* ROB entry (myself) */           /* for DRT low  */
      Ull   mask[2]; /* valid bits to be loaded (excepting prev-valid-STBF) */
      Ull   val[2];  /* reg-value */                    /* for DRT low  */
    } dr[4];         /* dr[0] is dummy */

    Uint   ls_addr;  /* should be compared when l1rq->l1 */
    struct stbf {
      Uint t   :  2; /* 0:invalid 1:valid 3:wait */
      Ull  mask[2];  /* valid bits to be stored */
      Ull  val[2];   /* write value */
    } stbf;
    /* MAP»þ       rob[w].stat  = ROB_MAPPED                         */
    /*             rob[m].srX_t = ÀßÄê                               */
    /*             rob[m].srX_x = map[cid][src1].x                   */
    /*             rob[m].srX_n = map[cid][src1].x ? map[cid][src1].rob : src1 */
    /*             rob[m].drX_t = 1;                                 */
    /*             rob[m].drX_n = dstX;                              */
    /*             rob[m].drX_p = m;                                 */
    /*             map[cid][dstX].x = X;                             */
    /*             map[cid][dstX].rob = m;                           */
    /* ISSUE»þ     skip (rob[e].srX_t==3)                            */
    /*             0:ALU Á´srcOK¤ÇÌµ¾ò·ïÈ¯¹Ô²ÄÇ½                     */
    /*             1:MUL Á´srcOK¤ÇÌµ¾ò·ïÈ¯¹Ô²ÄÇ½                     */
    /*             2:VXX Á´srcOK¤ÇÌµ¾ò·ïÈ¯¹Ô²ÄÇ½                     */
    /*             3:LD  Á´srcOK¤ÇÌµ¾ò·ïÈ¯¹Ô²ÄÇ½(¸å¤ÇÆ±°ìbankÀè¹ÔST¤òscan) */
    /*             4:ST  Á´srcOK¤ÇÌµ¾ò·ïÈ¯¹Ô²ÄÇ½(¸å¤ÇÆ±°ìbankÀè¹ÔST¤òscan) */
    /*             5:BRC Á´srcOK¤ÇÌµ¾ò·ïÈ¯¹Ô²ÄÇ½                     */
    /*             6:SVC Á´srcOK¤ÇÌµ¾ò·ïÈ¯¹Ô²ÄÇ½                     */
    /* EXEC»þ  srX=rob[e].srX_t==0 ? 0 :                             */
    /*             rob[e].srX_t==1 ? rob[e].srX_n :                  */
    /*             rob[e].srX_x==0 ? t[tid].reg[rob[e].srX_n] :      */
    /*             rob[e].srX_x==1 ? rob[rob[e].srX_n].dr1_v :       */
    /*             rob[e].srX_x==2 ? rob[rob[e].srX_n].dr2_v :       */
    /*             rob[e].srX_x==3 ? rob[rob[e].srX_n].dr3_v : 0;    */
    /*    WRITE»þ  rob[w].stat = done(2)                             */
    /*             rob[w].drX_v = result                             */
    /*             for (i) { if (rob[i].srX_t==3 && rob[i].srX_x && rob[i].srX_n == rob[w].drX_p) */
    /*                           rob[i].srX_t = 2;}                                               */
    /*             0:ALU ex_drw()                                    */
    /*             1:MUL imlpipe = D0 -> ex_drw()                    */
    /*             2:VXX vecpipe = D0 -> ex_drw()                    */
    /*             3:LD  exec_ld()->addr+mask->o_ldst                */
    /*              ... for (rob[all_st].addr)Æ±°ìbankÀè¹ÔST¤ò±Û¤¨¤Ê¤¤¸¡ºº                */
    /*                    Æ±°ìbank & rob[].stbf³ÎÄê¤¬¤¢¤ì¤Ðval¤Ëmerge                     */
    /*	                  Æ±°ìbank & rob[].stbfÂÔ¤Á¤¬¤¢¤ì¤ÐÃæ»ß (ROB_MAPPED°Ý»ý)          */
    /*                    Æ±°ìbank & rob[].stbfÂÔ¤ÁÌµ & rmask»Ä¤ê=0¤Ç¤¢¤ì¤Ð¤³¤³¤ÇÀµ¾ï½ªÎ» */
    /*              ... d1r()->hit($load)¤Ê¤é,merge¸åÀµ¾ï½ªÎ»                             */
    /*	   	             ->mis($load)¤Ê¤é,mergeºÑdata¤Èrmask¤òdr[].v+m¤Ë³ÊÇ¼,½é´ümask¤È¶¦¤Ël1rq */
    /*                             L1ÅþÃå»þ¤Ëdr[].v¤ÎÌ¤ÅþÃåÉôÊ¬¤ò¹¹¿·                     */
    /*             4:ST  exec_st()->addr+mask->o_ldst                                     */
    /*              ... Àè¹ÔST¤ò±Û¤¨¤Ê¤¤¸¡ºº¤Ï¢¨ÉÔÍ×¢¨                                    */
    /*              ... d1w()¤Ï»È¤ï¤Ê¤¤. rob[].stbf¤Ëwmask¤È¶¦¤Ë³ÊÇ¼                      */
    /*             5:BRC taken»þROB_RESTART¤Î¤ß                      */
    /*             6:SVC ROB_RESTART¤Î¤ß                             */
    /* RETIRE»þ    t[tid].reg[rob[r].srX_n] = rob[r].drX_v                                        */
    /*             for (i) { if (rob[i].srX_t==2 && rob[i].srX_x && rob[i].srX_n == rob[r].drX_p) */
    /*                           rob[i].srX_x = 0;                                                */
    /*                           rob[i].srX_n = rob[r].drX_n;                                     */
    /*                           map[tid/MAXCORE][rob[r].drX_n].x = 0;                            */
    /*             0:ALU rt_drw()+sr_update+map_update               */
    /*             1:MUL rt_drw()+sr_update+map_update               */
    /*             2:VXX rt_drw()+sr_update+map_update               */
    /*             3:LD  rt_drw()+sr_update+map_update               */
    /*             4:ST  rt_drw()+sr_update+map_update + d1w() hit->ROB_EMPTY, mis->ROB_D1RQWAIT */
    /*             5:BRC rt_drw()+sr_update+map_update+restart ib.pc */
    /*             6:SVC                               restart ib.pc */
  } rob[CORE_ROBSIZE];

  struct imlpipe {
    Uint   v     :  1;
    Uint   tid   : 12;
    struct rob *rob;
    Uint   dr1t  :  1;
    Ull    dr1v; /* 64bit reg value */
  } imlpipe[CORE_IMLPIPE]; /* for cycle accurate      */
  struct vecpipe {
    Uint   v     :  1;
    Uint   tid   : 12;
    struct rob *rob;
    Uint   dr1t  :  1;
    Ull    dr1v[2]; /* 128bit reg value */
    Uint   dr3t  :  1;
    Uint   dr3v;    /* 4bit nzcv value */
  } vecpipe[CORE_VECPIPE]; /* for cycle accurate      */
  struct divque {
    Uint   v     :  1;
    Uint   t     :  8;
    Uint   tid   : 12;
    struct rob *rob;
    Uint   dr1t  :  1;
    Ull    dr1v; /* 64bit reg value */
  } divque; /* for cycle accurate      */

  Uint system_counter;
  struct i1tag {
    Uint  v  : 1;
    Uint  lru: 7;
    Uint  la :24;
  } i1tag[MAXL1BK][I1WMAXINDEX][I1WAYS];
  struct i1line {
    Ull    d[LINESIZE/8]; /* body */
  } i1line[MAXL1BK][I1WMAXINDEX][I1WAYS];

  struct d1tag { /* v--:exclusive v-s:shared vd-:modified vds:N.A. */
    Uint   v     :  1;        /* valid                   */
    Uint   lru   :  7;        /* lru counter             */
    Uint   dirty :  1;        /* dirty-flag              */
    Uint   share :  1;        /* shared-flag             */
    Uint   drain :  1;        /* for keep_or_drain(transaction) */
    Uint   la    : 24;        /* logical addr            */
    char   thr_per_core;      /* THR_PER_CORE -1,0-31    */
  } d1tag[MAXL1BK][D1WMAXINDEX][D1WAYS];
  struct d1line {
    Ull    d[LINESIZE/8];     /* body                    */
  } d1line[MAXL1BK][D1WMAXINDEX][D1WAYS];

  struct l2tag { /* v--:exclusive v-s:shared vd-:modified vds:owned */
    Uint   v     :  1;        /* valid                   */
    Uint   lru   :  7;        /* lru counter             */
    Uint   dirty :  1;        /* dirty                   */
    Uint   share :  1;        /* shared-flag             */
    Uint   drain :  1;        /* for keep_or_drain(transaction) */
    Uint   la    : 24;        /* logical addr            */
  } l2tag[MAXL2BK][L2WMAXINDEX][L2WAYS];
  struct l2line {
    Ull    d[LINESIZE/8];
  } l2line[MAXL2BK][L2WMAXINDEX][L2WAYS];

  struct iorq {
    Uint   v_stat      : 4;   /* v 0:empty 1:reserve 3:inuse | stat 0:empty 1:busy 2:axi 3:RD-ok */
    Uint   tid         :12;
    Uint   type        : 4;   /* type                    */
    Uint   opcd        : 6;   /* opcd                    */
    Uint   ADR            ;   /* °ÊÁ°¤ÎADDR¤ËÂÐ±þ */                        /* ¡úread by sim-core */
    Ull    BUF[2]         ;   /* for load/store */
    struct rob *rob;          /* for DATA */
  } iorq;

  struct l1rq {
    Uint   v_stat      : 4;   /* v 0:empty 1:reserve 3:inuse | stat 0:empty 1:busy 2:OP-ok 3:IF-ok */
    Uint   rv_share    : 1;   /* retval 0:normal, 1:share(update L1-TAG) */ /* ¡úread by sim-core */
    Uint   rv_l1fil    : 1;   /* retval 0:normal, 1:l1fil(import buf) */    /* ¡úread by sim-core(store) */
    Uint   t           :10;   /* timer                   */
    Uint   tid         :12;
    Uint   type        : 4;   /* type                    */
    Uint   opcd        : 6;   /* opcd                    */
    Uint   rq_push     : 1;   /* 0:normal, 1:require L1->L2 */
    Uint   push_ADR;          /* °ÊÁ°¤ÎDADR¤ËÂÐ±þ */
    Uint   rq_pull     : 1;   /* 0:normal, 1:require L2->L1 */              /* ¡úread by sim-core(ld/st) */
    Uint   pull_ADR;          /* °ÊÁ°¤ÎADDR¤ËÂÐ±þ */                        /* ¡úread by sim-core */
    Uint   pull_way    : 8;   /* (output) L1Æþ¤ì´¹¤¨ÂÐ¾Ý */
    Ull    STD[2]         ;   /* for store */
    struct ib  *ib;           /* for IF */
    struct rob *rob;          /* for DATA */
    Ull    BUF[LINESIZE/8];   /* [LINESIZE/8] */
  } l1rq[MAXL1BK];

  struct l2rq {
    Uint   v_stat      : 4;   /* v 0:empty 1:reserve 3:inuse | stat 0:empty 1:busy 2:writebackOK 3:pullready */
    Uint   rv_share    : 1;   /* retval 0:normal, 1:share(update L1-TAG) */ /* ¡úread by sim-core */
    Uint   rv_l2fil    : 1;   /* retval 0:normal, 1:l1fil(import buf) */    /* ¡úread by sim-core(store) */
    Uint   t           :10;   /* timer                   */
    Uint   tid         :12;
    Uint   type        : 4;   /* type                    */
    Uint   opcd        : 6;   /* opcd                    */
    Uint   rq_push     : 1;   /* 0:normal, 1:require L1->L2 */
    Uint   push_ADR;          /* °ÊÁ°¤ÎDADR¤ËÂÐ±þ */
    Uint   rq_pull     : 1;   /* 0:normal, 1:require L2->L1 */              /* ¡úread by sim-core(ld/st) */
    Uint   pull_ADR;          /* °ÊÁ°¤ÎADDR¤ËÂÐ±þ */                        /* ¡úread by sim-core */
    Uint   pull_way    : 8;   /* (output) L1Æþ¤ì´¹¤¨ÂÐ¾Ý */
    Ull    BUF[LINESIZE/8];   /* [LINESIZE/8] */
  } l2rq[MAXL2BK];

  struct l2cc {
    Uint   v_stat      : 4;   /* v 0:empty 1:reserve 3:inuse | stat 0:empty 1:busy 2:CC-ok */
    Uint   t           :10;   /* timer                   */
    Uint   rq_pull     : 1;   /* 0:no-pull, 1:pull target L1 */             /* ¡úread by sim-core */
    Uint   rq_inv_share: 1;   /* 0:invalidate, 1:share target L1 */         /* ¡úread by sim-core */
    Uint   target_ADR;        /* p/s 00:Invalidate(BUF¡ß) 01:Share(BUF¡ß) 10:cpbk+I 11:cpbk+S */
    Ull    BUF[LINESIZE/8];   /* [LINESIZE/8] */
  } l2cc[MAXL2BK];
} c[MAXCORE];

    /*¡ÚLARGE-INSTRUCTION-WINDOW¡Û
        ¢¢  ¢¢  ¢¢  ¢¢  ¢¢  ¢¢  ¢¢  ¢¢   PC(MAXTHRD/MAXCORE)
    ¢¬  ¢®¢®¢®¢®¢®¢®¢®¢®¢®¢®¢®¢®¢®¢®¢®¢® addr%MAXL1BK
  I$¨¢  ¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢ I1$(MAXL1BK)
  µÛ¨¢  ¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢ l1rq(MAXL1BK) ¢ª MAXL1BK<MAXL2BK>MAXMMBK
  ¼ý¢­  ¢¢  ¢¢  ¢¢  ¢¢  ¢¢  ¢¢  ¢¢  ¢¢   IB(MAXTHRD/MAXCORE)
        ¨¦¨¨¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¥
          ¢­enq-1entry/cycle
°ÍÂ¸¢¬    ¢¢MAP ¢¢LREG
µÛ¼ý¢­  ¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢  ROB(MAXROB)           ¢«¨¡¨¡¨¡¨¤
        ¨¦¨¨¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¥                                ¨¢
          ¢­deq-1entry/cycle                           AL  ML  FL  DV   ¨¢
    ¢¬  ¢®¢®¢®¢®¢®¢®¢®¢®¢®¢®¢®¢®¢®¢®¢®¢® addr%MAXL1BK  ¢®  ¢®  ¢®  ¢®   ¨¢
  D$¨¢  ¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢ D1$(MAXL1BK)  ¢¢  ¢¢  ¢¢  ¢¢   ¨¢
  µÛ¨¢  ¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢ l1rq(MAXL1BK)     ¢¢  ¢¢ time  ¨¢
  ¼ý¢­  ¢¢  ¢¢  ¢¢  ¢¢  ¢¢  ¢¢  ¢¢  ¢¢   XB(MAXTHRD/MAXCORE)¢¢  ¢¢      ¨¢
        ¨¦¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨ª¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨ª¨¡¨ª¨¡¨ª¨¡¨ª¨¡¨¥1entry/cycle */

/*¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢*/
/*¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­¢­*//* I1/O1¤ò$lineÃ±°Ì¤Ç¥Ð¥ó¥¯Ê¬³ä */
/*¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢ I1/PE(128)         ¡¡¨¡¨¤sim_core()
  ¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢ D1/PE(128)         ¡¡  ¨¢
  ¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢ l1rq(t)                ¨¢
  ¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢¨¢ ¢­¢¬                   ¨¢
  ¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢ L2(60)                 ¨¢
                                                                                                   l2rq(t) l2cc(t)      ¨¡¨¥
                                                                                                   ¢­¢¬..¢¬¢­
  ¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢ L2DIR(64)¨¡¨¡sim_mreq()
    ¡¡¡¡¡¡¡¡¡¡¡¡¡¡¡¡¡¡¡¡¡¡¡¡                                                     ¡¡¡¡¡¡¡¡¡¡            ..¢­¢¬
  ¡¡                        ¡¡¡¡¡¡¡¡¡¡¡¡¡¡                                     ¡¡¡¡¡¡¡¡¡¡¡¡¡¡¡¡¡¡          mmcc(t)
                                                                                                      ¢¢¢¢¢¢¢¢ MM       ¨¡¨¡sim_cluster()*/

struct d { /* ADDRESS¤Ë¤è¤êÃ´ÅöL2DIR¤Ï°ì°Õ¤Ë·èÄê */
  Uint  rq_cid_first_search;
  Uchar l2rq_bitmap[MAXCORE];  /* PE0,1,...,PE59 atomic-R/W¤Î¤¿¤á¤Ëbitmap¤Ç¤Ï¤Ê¤¯ÇÛÎó²½ */
  Ull   l2rq_lock[MAXCORbitmaps]; /* PE0,1,...,PE15 */
#define L2DSTATE_EMPTY     0
#define L2DSTATE_DRAIN     1
#define L2DSTATE_PULL      2
  Uint  root_cid;
  Uint  l2d_state;
  struct l2dir {
    Ull  l2dir_v[MAXCORbitmaps]; /* L2-status 3bit per Cluster */
    Ull  l2dir_d[MAXCORbitmaps]; /* 0 - -  Invalid */
    Ull  l2dir_s[MAXCORbitmaps]; /* 1 0 0  Exclusive-clean */
                                 /* 1 0 1  Shared clean */
                                 /* 1 1 0  Mod */
                                 /* 1 1 1  Shared Owned(mod) */
  } l2dir[MEMSIZE/LINESIZE/MAXL2BK];
  Uint  l2dir_ent;               /* ÁàºîÂÐ¾Ýl2dir¥¨¥ó¥È¥ê */
  Ull   l2dir_v_new0[MAXCORbitmaps]; /* ¥ê¥»¥Ã¥ÈÂÐ¾Ý¥Ñ¥¿¥ó */
  Ull   l2dir_d_new0[MAXCORbitmaps]; /* ¥ê¥»¥Ã¥ÈÂÐ¾Ý¥Ñ¥¿¥ó */
  Ull   l2dir_s_new0[MAXCORbitmaps]; /* ¥ê¥»¥Ã¥ÈÂÐ¾Ý¥Ñ¥¿¥ó */
  Ull   l2dir_v_new1[MAXCORbitmaps]; /* ¥»¥Ã¥ÈÂÐ¾Ý¥Ñ¥¿¥ó */
  Ull   l2dir_d_new1[MAXCORbitmaps]; /* ¥»¥Ã¥ÈÂÐ¾Ý¥Ñ¥¿¥ó */
  Ull   l2dir_s_new1[MAXCORbitmaps]; /* ¥»¥Ã¥ÈÂÐ¾Ý¥Ñ¥¿¥ó */
  Ull   l2cc_req_bitmap[MAXCORbitmaps]; /* L2CCWÈ¯¹ÔºÑPID_bitmap sim-cluster¤¬½ñ¤­¹þ¤à(l1dir¤ò¸µ¤ËÀ¸À®) */
  Uchar l2cc_ack_bitmap[MAXCORE];    /* L2CCW´°Î»ºÑPID_bitmap ¡úsim-core¤¬½ñ¤­¹þ¤à) */
  Ull   mmcc_req_bitmap[MAXCORbitmaps]; /* MMCCWÈ¯¹ÔºÑbitmap sim-mem¤¬½ñ¤­¹þ¤à(l2dir¤ò¸µ¤ËÀ¸À®) */
  Uchar mmcc_ack_bitmap[MAXMMBK];    /* MMCCW´°Î»ºÑbitmap sim-cluster¤¬½ñ¤­¹þ¤à) */
} d[MAXL2BK];

struct m {
  struct mmcc {
    Uint   v_stat      : 4;   /* v 0:empty 1:reserve 3:inuse | stat 0:empty 1:busy 2:OP-ok 3:IF-ok */
    Uint   t           :10;   /* timer                   */
    Uint   rq_push     : 1;   /* 0:normal, 1:require L1->L2 */
    Uint   push_ADR;          /* °ÊÁ°¤ÎDADR¤ËÂÐ±þ */
    Uint   rq_pull     : 1;   /* 0:normal, 1:require L2->L1 */              /* ¡úread by sim-core(ld/st) */
    Uint   pull_ADR;          /* °ÊÁ°¤ÎADDR¤ËÂÐ±þ */                        /* ¡úread by sim-core */
    Ull    BUF[LINESIZE/8];   /* [LINESIZE/8] */
  } mmcc[MAXL2BK];
} m[MAXMMBK];

Uchar mem[MAXMMBK][MEMSIZE/MAXMMBK];

union armf {
  struct raw {
    Uint   word;
  } raw;
  struct adr { /* C6.6.9 ADR, C6.6.10 ADRP */
    Uint   rd      : 5;
    Uint   immhi   :19;
    Uint   op28_24 : 5;
    Uint   immlo   : 2;
    Uint   op      : 1;
  } adr;
  struct add_sub_imm { /* C6.6.4 ADD(immediate), C6.6.7 ADDS(immediate), C6.6.42 CMN(immediate), C6.6.45 CMP(immediate), C6.6.121 MOV(to/from SP), C6.6.195 SUB(immediate), C6.6.198 SUBS(immediate) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   imm12   :12;
    Uint   shift   : 2;
    Uint   op28_24 : 5;
    Uint   S       : 1;
    Uint   op      : 1;
    Uint   sf      : 1;
  } add_sub_imm;
  struct adc_sbc { /* C6.6.1 ADC, C6.6.2 ADCS, C6.6.137 NGC, C6.6.138 NGCS, C6.6.155 SBC, C6.6.156 SBCS */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op15_10 : 6; /* 0 */
    Uint   rm      : 5;
    Uint   op28_21 : 8;
    Uint   S       : 1;
    Uint   op      : 1;
    Uint   sf      : 1;
  } adc_sbc;
  struct csel { /* C6.6.36 CINC, C6.6.37 CINV, C6.6.47 CNEG, C6.6.50 CSEL, C6.6.51 CSET, C6.6.52 CSETM, C6.6.53 CSINC, C6.6.54 CSINV, C6.6.55 CSNEG */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   o2      : 1;
    Uint   op11    : 1;
    Uint   cond    : 4;
    Uint   rm      : 5;
    Uint   op29_21 : 9; /* 0x0d4 */
    Uint   op      : 1;
    Uint   sf      : 1;
  } csel;
  struct ccm { /* C6.6.32 CCMN(immediate), C6.6.33 CCMN(register), C6.6.34 CCMP(immediate), C6.6.35 CCMP(register) */
    Uint   nzcv    : 4;
    Uint   op4     : 1; /* 0 */
    Uint   rn      : 5;
    Uint   op11_10 : 2; /* 00/10 */
    Uint   cond    : 4;
    Uint   rm_imm5 : 5;
    Uint   op29_21 : 9; /* 0x1d2 */
    Uint   op      : 1;
    Uint   sf      : 1;
  } ccm;
  struct mov { /* C6.6.122 MOV(inverted wide immediate), C6.6.123 MOV(wide immediate), C6.6.126 MOVK, C6.6.127 MOVN, C6.6.128 MOVZ */
    Uint   rd      : 5;
    Uint   imm16   :16;
    Uint   hw      : 2;
    Uint   op28_23 : 6;
    Uint   opc     : 2;
    Uint   sf      : 1;
  } mov;
  struct rev { /* C6.6.147 RBIT, C6.6.149 REV, C6.6.150 REV16, C6.6.151 REV32 */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   opc     : 2;
    Uint   op30_12 :19;
    Uint   sf      : 1;
  } rev;
  struct cls { /* C6.6.39 CLS, C6.6.40 CLZ */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op      : 1;
    Uint   op30_11 :20;
    Uint   sf      : 1;
  } cls;
  struct add_sub_shifted { /* C6.6.5 ADD(shifted register), C6.6.8 ADDS(shifted register), C6.6.43 CMN(shifted register), C6.6.46 CMP(shifted register), C6.6.135 NEG(shifted register), C6.6.136 NEGS(shifted register), C6.6.196 SUB(shifted register), C6.6.199 SUBS(shifted register) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   imm6    : 6;
    Uint   rm      : 5;
    Uint   op21    : 1;
    Uint   shift   : 2;
    Uint   op28_24 : 5;
    Uint   S       : 1;
    Uint   op      : 1;
    Uint   sf      : 1;
  } add_sub_shifted;
  struct add_sub_extreg { /* C6.6.3 ADD(extended register), C6.6.6 ADDS(extended register), C6.6.41 CMN(extended register), C6.6.44 CMP(extended register), C6.6.194 SUB(extended register), C6.6.197 SUBS(extended register) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   imm3    : 3;
    Uint   option  : 3;
    Uint   rm      : 5;
    Uint   op21    : 1; /* 1 */
    Uint   op23_22 : 2; /* 0 */
    Uint   op28_24 : 5; /* 0x0b */
    Uint   S       : 1;
    Uint   op      : 1;
    Uint   sf      : 1;
  } add_sub_extreg;
  struct and_shifted { /* C6.6.12 AND(shifted register), C6.6.14 ANDS(shifted register), C6.6.24 BIC(shifted register), C6.6.25 BICS(shifted register), C6.6.63 EON(shifted register), C6.6.65 EOR(shifted register), C6.6.125 MOV(register), C6.6.134 MVN(register), C6.6.140 ORN(shifted register), C6.6.142 ORR(shifted register), C6.6.210 TST(shifted register) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   imm6    : 6;
    Uint   rm      : 5;
    Uint   N       : 1;
    Uint   shift   : 2;
    Uint   op28_24 : 5;
    Uint   opc     : 2;
    Uint   sf      : 1;
  } and_shifted;
  struct and_imm { /* C6.6.11 AND(immediate), C6.6.13 ANDS(immediate), C6.6.64 EOR(immediate), C6.6.124 MOV(bitmask immediate), C6.6.141 ORR(immediate), C6.6.209 TST(immediate) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   imms    : 6;
    Uint   immr    : 6;
    Uint   N       : 1;
    Uint   op28_23 : 6;
    Uint   opc     : 2;
    Uint   sf      : 1;
  } and_imm;
  struct sft_imm { /* C6.6.16 ASR(immediate), C6.6.21 BFI, C6.6.22 BFM, C6.6.23 BFXIL, C6.6.114 LSL(immediate), C6.6.117 LSR(immediate), C6.6.157 SBFIZ, C6.6.158 SBFM, C6.6.159 SBFX, C6.6.201 SXTB, C6.6.202 SXTH, C6.6.203 SXTW, C6.6.211 UBFIZ, C6.6.212 UBFM, C6.6.213 UBFX,  C6.6.220 UXTB, C6.6.221 UXTH */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   imms    : 6;
    Uint   immr    : 6;
    Uint   N       : 1;
    Uint   op28_23 : 6;
    Uint   opc     : 2;
    Uint   sf      : 1;
  } sft_imm;
  struct ror_imm { /* C6.6.67 EXTR(immediate), C6.6.152 ROR(immediate) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   imms    : 6;
    Uint   rm      : 5;
    Uint   op21    : 1; /* 0 */
    Uint   N       : 1;
    Uint   op30_23 : 8; /* 0x27 */
    Uint   sf      : 1;
  } ror_imm;
  struct sft_reg { /* C6.6.15 ASR, C6.6.17 ASRV, C6.6.113 LSL(register), C6.6.115 LSLV, C6.6.116 LSR(register), C6.6.118 LSRV, C6.6.153 ROR(register), C6.6.154 RORV */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op2     : 2;
    Uint   op15_12 : 4; /* 2 */
    Uint   rm      : 5;
    Uint   op30_21 :10; /* 0x0d6 */
    Uint   sf      : 1;
  } sft_reg;
  struct b_cond { /* C6.6.19 B.cond */
    Uint   cond    : 4;
    Uint   op4     : 1;
    Uint   imm19   :19;
    Uint   op31_24 : 8;
  } b_cond;
  struct bl { /* C6.6.20 B, C6.6.26 BL */
    Uint   imm26   :26;
    Uint   op30_26 : 5;
    Uint   op      : 1;
  } bl;
  struct cbz { /* C6.6.30 CBNZ, C6.6.31 CBZ */
    Uint   rt      : 5;
    Uint   imm19   :19;
    Uint   op      : 1;
    Uint   op30_25 : 6;
    Uint   sf      : 1;
  } cbz;
  struct tbz { /* C6.6.206 TBNZ, C6.6.207 TBZ */
    Uint   rt      : 5;
    Uint   imm14   :14;
    Uint   b40     : 5;
    Uint   op      : 1;
    Uint   op30_25 : 6;
    Uint   b5      : 1;
  } tbz;
  struct blr { /* C6.6.27 BLR, C6.6.28 BR, C6.6.148 RET */
    Uint   op4_0   : 5;
    Uint   rn      : 5;
    Uint   op20_10 :11;
    Uint   op      : 2;
    Uint   op31_23 : 9;
  } blr;
  struct svc { /* C6.6.200 SVC */
    Uint   op4_0   : 5;
    Uint   imm12   :12;
    Uint   type    : 4;
    Uint   op31_21 :11;
  } svc;
  struct ldaxr_stlxr { /* C6.6.77 LDAXR, C6.6.173 STLXR */
    Uint   rt      : 5;
    Uint   rn      : 5;
    Uint   rt2     : 5;
    Uint   o0      : 1;
    Uint   rs      : 5;
    Uint   o1      : 1;
    Uint   L       : 1;
    Uint   o2      : 1;
    Uint   op29_24 : 6;
    Uint   size    : 2;
  } ldaxr_stlxr;
  struct ldp_stp { /* C6.6.81 LDP, C6.6.177 STP */
                   /* C7.3.165 LDP(SIMD&FP), C7.3.284 STP(SIMD&FP) */
    Uint   rt      : 5;
    Uint   rn      : 5;
    Uint   rt2     : 5;
    Uint   imm7    : 7;
    Uint   L       : 1;
    Uint   op24_23 : 2;
    Uint   op29_25 : 5;
    Uint   opc     : 2;
  } ldp_stp;
  struct ldr_str_imm { /* C6.6.83 LDR(immediate), C6.6.86 LDRB(immediate), C6.6.88 LDRH(immediate), C6.6.90 LDRSB(immediate), C6.6.92 LDRSH(immediate), C6.6.94 LDRSW(immediate), C6.6.178 STR(immediate), C6.6.180 STRB(immediate), C6.6.182 STR(immediate) */
                       /* C7.3.168 LDR(immediate,SIMD&FP), C7.3.285 STR(immediate,SIMD&FP) */
    Uint   rt      : 5;
    Uint   rn      : 5;
    Uint   imm12   :12;
    Uint   opc     : 2;
    Uint   op24    : 1;
    Uint   op29_25 : 5;
    Uint   size    : 2;
  } ldr_str_imm;
  struct ldr_str_unsc { /* C6.6.103 LDUR, C6.6.104 LDURB, C6.6.105 LDURH, C6.6.106 LDURSB, C6.6.107 LDURSH, C6.6.108 LDURSW, C6.6.187 STUR, C6.6.188 STURB, C6.6.189 STURH */
                        /* C7.3.169 LDUR(SIMD&FP), C7.3.287 STUR(SIMD&FP) */
    Uint   rt      : 5;
    Uint   rn      : 5;
    Uint   op11_10 : 2;
    Uint   imm9    : 9;
    Uint   op21    : 1;
    Uint   opc     : 2;
    Uint   op24    : 1;
    Uint   op29_25 : 5;
    Uint   size    : 2;
  } ldr_str_unsc;
  struct ldr_str_reg { /* C6.6.85 LDR(register), C6.6.87 LDRB(register), C6.6.89 LDRH(register), C6.6.91 LDRSB(register), C6.6.93 LDRSH(register), C6.6.96 LDRSW(register), C6.6.179 STR(register), C6.6.181 STRB(register), C6.6.183 STRH(register) */
                       /* C7.3.168 LDR(register,simd&fp), C7.3.286 STR(register,simd&fp) */
    Uint   rt      : 5;
    Uint   rn      : 5;
    Uint   op11_10 : 2;
    Uint   S       : 1;
    Uint   option  : 3;
    Uint   rm      : 5;
    Uint   op21    : 1;
    Uint   opc     : 2;
    Uint   op24    : 1;
    Uint   op29_25 : 5;
    Uint   size    : 2;
  } ldr_str_reg;
  struct ldr_str_literal { /* C6.6.84 LDR(literal), C6.6.95 LDRSW(literal) */
                           /* C7.3.167 LDR(literal,simd&fp) */
    Uint   rt      : 5;
    Uint   imm19   :19;
    Uint   op29_24 : 6;
    Uint   opc     : 2;
  } ldr_str_literal;
  struct ld1_st1_mult { /* C7.3.152 LD1(multiple structures), C7.3.155 LD2(multiple structures), C7.3.158 LD3(multiple structures), C7.3.161 LD4(multiple structures), C7.3.275 ST1(multiple structures), C7.3.277 ST2(multiple structures), C7.3.279 ST3(multiple structures), C7.3.281 ST4(multiple structures) */
    Uint   rt      : 5;
    Uint   rn      : 5;
    Uint   size    : 2;
    Uint   opcode  : 4;
    Uint   rm      : 5;
    Uint   R       : 1;
    Uint   L       : 1;
    Uint   op23    : 1;
    Uint   op24    : 1;
    Uint   op29_25 : 5;
    Uint   Q       : 1;
    Uint   op31    : 1;
  } ld1_st1_mult;
  struct ld1_st1_sing { /* C7.3.153 LD1(single structures), C7.3.156 LD2(single structure), C7.3.159 LD3(single structure), C7.3.162 LD4(single structure), C7.3.276 ST1(single structure), C7.3.278 ST2(single structure), C7.3.280 ST3(single structure), C7.3.282 ST4(single structure) */
                        /* C7.3.154 LD1R, C7.3.157 LD2R, C7.3.160 LD3R, C7.3.163 LD4R */
    Uint   rt      : 5;
    Uint   rn      : 5;
    Uint   size    : 2;
    Uint   S       : 1;
    Uint   opcode  : 3;
    Uint   rm      : 5;
    Uint   R       : 1;
    Uint   L       : 1;
    Uint   op23    : 1;
    Uint   op24    : 1;
    Uint   op29_25 : 5;
    Uint   Q       : 1;
    Uint   op31    : 1;
  } ld1_st1_sing;
  struct mad { /* C6.6.163 SMADDL, C6.6.165 SMNEGL, C6.6.166 SMSUBL, C6.6.168 SMULL, C6.6.215 UMADDL, C6.6.216 UMNEGL, C6.6.217 UMSUBL, C6.6.219 UMULL */
               /* C6.6.119 MADD, C6.6.133 MUL, C6.6.120 MNEG, C6.6.132 MSUB */
               /* C6.6.167 SMULH, C6.6.218 UMULH */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   ra      : 5;
    Uint   o0      : 1; /* 0 */
    Uint   rm      : 5;
    Uint   op22_21 : 2; /* 01(madd),10(smulh/umulh) */
    Uint   U       : 1;
    Uint   op30_24 : 7;
    Uint   sf      : 1;
  } mad;
  struct div { /* C6.6.160 SDIV, C6.6.214 UDIV */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   o1      : 1;
    Uint   op15_11 : 5;
    Uint   rm      : 5;
    Uint   op30_21 :10;
    Uint   sf      : 1;
  } div;
  struct fmov_gen { /* C7.3.114 FMOV(general), C7.3.59 FCVTAS(scalar), C7.3.61 FCVTAU(scalar), C7.3.64 FCVTMS(scalar), C7.3.66 FCVTMU(scalar), C7.3.69 FCVTNS(scalar), C7.3.71 FCVTNU(scalar), C7.3.73 FCVTPS(scalar), C7.3.75 FCVTPU(scalar), C7.3.80 FCVTZS(scalar,integer), C7.3.84 FCVTZU(scalar,integer), C7.3.210 SCVTF(scalar,integer), C7.3.308 UCVTF(scalar,integer) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op15_10 : 6;
    Uint   opc     : 3;
    Uint   mode    : 2;
    Uint   op21    : 1;
    Uint   type    : 2;
    Uint   op30_24 : 7;
    Uint   sf      : 1;
  } fmov_gen;
  struct fmov_sca_imm { /* C7.3.115 FMOV(scalar,immediate) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op12_10 : 3;
    Uint   imm8    : 8;
    Uint   op21    : 1;
    Uint   type    : 2;
    Uint   op31_24 : 8;
  } fmov_sca_imm;
  struct movi { /* C7.3.12 BIC(vector,immediate), C7.3.112 FMOV(vector,immediate), C7.3.179 MOVI, C7.3.183 MVNI, C7.3.187 ORR(vector,immediate) */
    Uint   rd      : 5;
    Uint   h       : 1;
    Uint   g       : 1;
    Uint   f       : 1;
    Uint   e       : 1;
    Uint   d       : 1;
    Uint   op11_10 : 2;
    Uint   cmode   : 4;
    Uint   c       : 1;
    Uint   b       : 1;
    Uint   a       : 1;
    Uint   op28_19 :10;
    Uint   op      : 1;
    Uint   Q       : 1;
    Uint   op31    : 1;
  } movi;
  struct movv { /* C7.3.11 AND(vector), C7.3.13 BIC(vector,register), C7.3.177 MOV(vector), C7.3.186 ORN(vector), C7.3.188 ORR(vector,register) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op15_10 : 6;
    Uint   rm      : 5;
    Uint   op21    : 1;
    Uint   inv     : 1;
    Uint   andor   : 1;
    Uint   op29_24 : 6;
    Uint   Q       : 1;
    Uint   op31    : 1;
  } movv;
  struct shr_sca { /* C7.3.271 SSHR(scalar), C7.3.340 USHR(scalar) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op15_10 : 6;
    Uint   immb    : 3;
    Uint   immh    : 4;
    Uint   op28_23 : 6;
    Uint   U       : 1;
    Uint   op31_30 : 2;
  } shr_sca;
  struct sft_vec { /* C7.3.270 SSHLL/SSHLL2, C7.3.271 SSHR(vector), C7.3.339 USHLL/USHLL2, C7.3.340 USHR(vector) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op15_10 : 6;
    Uint   immb    : 3;
    Uint   immh    : 4;
    Uint   op28_23 : 6;
    Uint   U       : 1;
    Uint   Q       : 1;
    Uint   op31    : 1;
  } sft_vec;
  struct dup { /* C7.3.31 DUP(vector) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op15_10 : 6;
    Uint   imm5    : 5;
    Uint   op29_21 : 9;
    Uint   Q       : 1;
    Uint   op31    : 1;
  } dup;
  struct xtn { /* C7.3.348 XTN,XTN2 */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op21_10 :12;
    Uint   size    : 2;
    Uint   op29_24 : 6;
    Uint   Q       : 1;
    Uint   op31    : 1;
  } xtn;
  struct umov { /* C7.3.176 MOV(from general), C7.3.151 INS(general) */
                /* C7.3.178 MOV(to general), C7.3.321 UMOV */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op15_10 : 6;
    Uint   imm5    : 5;
    Uint   op29_21 : 9;
    Uint   Q       : 1;
    Uint   op31    : 1;
  } umov;
  struct maxmin { /* C7.3.227 SMAX, C7.3.230 SMIN, C7.3.311 UMAX, C7.3.314 UMIN */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op10    : 1;
    Uint   o1      : 1;
    Uint   op15_12 : 4;
    Uint   rm      : 5;
    Uint   op21    : 1;
    Uint   size    : 2;
    Uint   op28_24 : 5;
    Uint   U       : 1;
    Uint   Q       : 1;
    Uint   op31    : 1;
  } maxmin;
  struct cmpr_sca { /* C7.3.19 CMEQ(scalar), C7.3.21 CMGE(scalar), C7.3.23 CMGT(scalar), C7.3.25 CMHI(scalar), C7.3.26 CMHS(scalar), C7.3.29 CMTST(scalar) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op10    : 1;
    Uint   eq      : 1;
    Uint   op15_12 : 4;
    Uint   rm      : 5;
    Uint   op21    : 1;
    Uint   size    : 2;
    Uint   op28_24 : 5;
    Uint   U       : 1;
    Uint   op31_30 : 2;
  } cmpr_sca;
  struct cmpr_vec { /* C7.3.19 CMEQ(vector), C7.3.21 CMGE(vector), C7.3.23 CMGT(vector), C7.3.25 CMHI(vector), C7.3.26 CMHS(vector), C7.3.29 CMTST(vector) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op10    : 1;
    Uint   eq      : 1;
    Uint   op15_12 : 4;
    Uint   rm      : 5;
    Uint   op21    : 1;
    Uint   size    : 2;
    Uint   op28_24 : 5;
    Uint   U       : 1;
    Uint   Q       : 1;
    Uint   op31    : 1;
  } cmpr_vec;
  struct cmpz_sca { /* C7.3.20 CMEQZ(scalar), C7.3.22 CMGEZ(scalar), C7.3.24 CMGTZ(scalar), C7.3.27 CMLEZ(scalar), C7.3.28 CMLTZ(scalar) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op11_10 : 2;
    Uint   op      : 1;
    Uint   op21_13 : 9;
    Uint   size    : 2;
    Uint   op28_24 : 5;
    Uint   U       : 1;
    Uint   op31_30 : 2;
  } cmpz_sca;
  struct cmpz_vec { /* C7.3.20 CMEQZ(vector), C7.3.22 CMGEZ(vector), C7.3.24 CMGTZ(vector), C7.3.27 CMLEZ(vector), C7.3.28 CMLTZ(vector) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op11_10 : 2;
    Uint   op      : 1;
    Uint   op21_13 : 9;
    Uint   size    : 2;
    Uint   op28_24 : 5;
    Uint   U       : 1;
    Uint   Q       : 1;
    Uint   op31    : 1;
  } cmpz_vec;
  struct bitwise { /* C7.3.14 BIF, C7.3.15 BIT, C7.3.16 BSL, C7.3.33 EOR */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op15_10 : 6;
    Uint   rm      : 5;
    Uint   op21    : 1;
    Uint   opc2    : 2;
    Uint   op29_24 : 6;
    Uint   Q       : 1;
    Uint   op31    : 1;
  } bitwise;
  struct fadd { /* C7.3.40 FADD(vector), C7.3.148 FSUB(vector) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op15_10 : 6;
    Uint   rm      : 5;
    Uint   op21    : 1;
    Uint   sz      : 1;
    Uint   op      : 1;
    Uint   op28_24 : 5;
    Uint   U       : 1;
    Uint   Q       : 1;
    Uint   op31    : 1;
  } fadd;
  struct fmla { /* C7.3.109 FMLA(vector), C7.3.111 FMLS(vector) */
                /* C7.3.118 FMUL(vector) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op15_10 : 6;
    Uint   rm      : 5;
    Uint   op21    : 1;
    Uint   sz      : 1;
    Uint   op      : 1;
    Uint   op29_24 : 6;
    Uint   Q       : 1;
    Uint   op31    : 1;
  } fmla;
  struct mla { /* C7.3.171 MLA(vector), C7.3.173 MLS(vector) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op15_10 : 6;
    Uint   rm      : 5;
    Uint   op21    : 1;
    Uint   size    : 2;
    Uint   op28_24 : 5;
    Uint   U       : 1;
    Uint   Q       : 1;
    Uint   op31    : 1;
  } mla;
  struct fadd_sca { /* C7.3.41 FADD(scalar), C7.3.149 FSUB(scalar) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op11_10 : 2;
    Uint   op      : 1;
    Uint   op15_13 : 3;
    Uint   rm      : 5;
    Uint   op21    : 1;
    Uint   type    : 2;
    Uint   op31_24 : 8;
  } fadd_sca;
  struct fmadd { /* C7.3.87 FMADD, C7.3.116 FMSUB, C7.3.124 FNMADD, C7.3.125 FNMSUB */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   ra      : 5;
    Uint   o0      : 1;
    Uint   rm      : 5;
    Uint   o1      : 1;
    Uint   type    : 2;
    Uint   op31_24 : 8;
  } fmadd;
  struct fmul_sca { /* C7.3.119 FMUL(scalar), C7.3.126 FNMUL(scalar) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op14_10 : 5;
    Uint   op      : 1;
    Uint   rm      : 5;
    Uint   op21    : 1;
    Uint   type    : 2;
    Uint   op31_24 : 8;
  } fmul_sca;
  struct fdiv_sca { /* C7.3.86 FDIV(scalar) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op15_10 : 6;
    Uint   rm      : 5;
    Uint   op21    : 1;
    Uint   type    : 2;
    Uint   op31_24 : 8;
  } fdiv_sca;
  struct fcmp { /* C7.3.54 FCMP, C7.3.55 FCMPE */
    Uint   op2_0   : 3;
    Uint   opc     : 2;
    Uint   rn      : 5;
    Uint   op15_10 : 6;
    Uint   rm      : 5;
    Uint   op21    : 1;
    Uint   type    : 2;
    Uint   op31_24 : 8;
  } fcmp;
  struct fmov_reg { /* C7.3.113 FMOV(register) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op14_10 : 5;
    Uint   opc     : 2;
    Uint   op21_17 : 5;
    Uint   type    : 2;
    Uint   op31_24 : 8;
  } fmov_reg;
  struct fcvt_sca { /* C7.3.57 FCVT(scalar), C7.3.135 FRINTM(scalar) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op14_10 : 5;
    Uint   opc     : 2;
    Uint   op21_17 : 5;
    Uint   type    : 2;
    Uint   op31_24 : 8;
  } fcvt_sca;
  struct add_sca { /* C7.3.2 ADD(scalar), C7.3.288 SUB(scalar) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op15_10 : 6;
    Uint   rm      : 5;
    Uint   op21    : 1;
    Uint   size    : 2;
    Uint   op28_24 : 5;
    Uint   U       : 1;
    Uint   op31_30 : 2;
  } add_sca;
  struct add_vec { /* C7.3.2 ADD(vector), C7.3.288 SUB(vector) */
    Uint   rd      : 5;
    Uint   rn      : 5;
    Uint   op15_10 : 6;
    Uint   rm      : 5;
    Uint   op21    : 1;
    Uint   size    : 2;
    Uint   op28_24 : 5;
    Uint   U       : 1;
    Uint   Q       : 1;
    Uint   op31    : 1;
  } add_vec;
  /* C6.6.129 MRS */
  /* C6.6.130 MSR(immediate) */
  /* C6.6.131 MSR(register) */
};

#ifdef DISASM
Uchar *adis[] = {
  "and  ", /* type=0, opcd=00 */
  "eor  ", /* type=0, opcd=01 */
  "sub  ", /* type=0, opcd=02 */
  "orr  ", /* type=0, opcd=03 */
  "add  ", /* type=0, opcd=04 */
  "adc  ", /* type=0, opcd=05 */
  "sbc  ", /* type=0, opcd=06 */
  "mov  ", /* type=0, opcd=07 */
  "sbfm ", /* type=0, opcd=08 */
  "ubfm ", /* type=0, opcd=09 */
  "csel ", /* type=0, opcd=10 */
  "ccm  ", /* type=0, opcd=11 */
  "rev  ", /* type=0, opcd=12 */
  "cls  ", /* type=0, opcd=13 */
  "ext  ", /* type=0, opcd=14 */
  "---  ", /* type=0, opcd=15 */
  "umadd", /* type=1, opcd=00 */
  "umsub", /* type=1, opcd=01 */
  "smadd", /* type=1, opcd=02 */
  "smsub", /* type=1, opcd=03 */
  "madd ", /* type=1, opcd=04 */
  "msub ", /* type=1, opcd=05 */
  "smulh", /* type=1, opcd=06 */
  "umulh", /* type=1, opcd=07 */
  "udiv ", /* type=1, opcd=08 */
  "sdiv ", /* type=1, opcd=09 */
  "---  ", /* type=1, opcd=10 */
  "---  ", /* type=1, opcd=11 */
  "---  ", /* type=1, opcd=12 */
  "---  ", /* type=1, opcd=13 */
  "---  ", /* type=1, opcd=14 */
  "---  ", /* type=1, opcd=15 */
  "fmov ", /* type=2, opcd=00 */
  "fcvt ", /* type=2, opcd=01 */
  "cvtsx", /* type=2, opcd=02 */
  "cvtux", /* type=2, opcd=03 */
  "shift", /* type=2, opcd=04 */
  "cmp  ", /* type=2, opcd=05 */
  "mov  ", /* type=2, opcd=06 */
  "dup  ", /* type=2, opcd=07 */
  "fadd ", /* type=2, opcd=08 */
  "fmla ", /* type=2, opcd=09 */
  "fmul ", /* type=2, opcd=10 */
  "mla  ", /* type=2, opcd=11 */
  "fadds", /* type=2, opcd=12 */
  "fmadd", /* type=2, opcd=13 */
  "fmuls", /* type=2, opcd=14 */
  "fdivs", /* type=2, opcd=15 */
  "ldrb ", /* type=3, opcd=00 */
  "ldrh ", /* type=3, opcd=01 */
  "ldrw ", /* type=3, opcd=02 */
  "ldr  ", /* type=3, opcd=03 */
  "ldrsb", /* type=3, opcd=04 */
  "ldrsh", /* type=3, opcd=05 */
  "ldrsw", /* type=3, opcd=06 */
  "ldrex", /* type=3, opcd=07 */
  "vldrb", /* type=3, opcd=08 */
  "vldrh", /* type=3, opcd=09 */
  "vldrs", /* type=3, opcd=10 */
  "vldrd", /* type=3, opcd=11 */
  "vldrq", /* type=3, opcd=12 */
  "---  ", /* type=3, opcd=13 */
  "---  ", /* type=3, opcd=14 */
  "---  ", /* type=3, opcd=15 */
  "strb ", /* type=4, opcd=00 */
  "strh ", /* type=4, opcd=01 */
  "strw ", /* type=4, opcd=02 */
  "str  ", /* type=4, opcd=03 */
  "---  ", /* type=4, opcd=04 */
  "---  ", /* type=4, opcd=05 */
  "---  ", /* type=4, opcd=06 */
  "strex", /* type=4, opcd=07 */
  "vstrb", /* type=4, opcd=08 */
  "vstrh", /* type=4, opcd=09 */
  "vstrs", /* type=4, opcd=10 */
  "vstrd", /* type=4, opcd=11 */
  "vstrq", /* type=4, opcd=12 */
  "---  ", /* type=4, opcd=13 */
  "---  ", /* type=4, opcd=14 */
  "---  ", /* type=4, opcd=15 */
  "b    ", /* type=5, opcd=00 */
  "bl   ", /* type=5, opcd=01 */
  "cbz  ", /* type=5, opcd=02 */
  "cbnz ", /* type=5, opcd=03 */
  "tbz  ", /* type=5, opcd=04 */
  "tbnz ", /* type=5, opcd=05 */
  "blr  ", /* type=5, opcd=06 */
  "br   ", /* type=5, opcd=07 */
  "---  ", /* type=5, opcd=08 */
  "---  ", /* type=5, opcd=09 */
  "---  ", /* type=5, opcd=10 */
  "---  ", /* type=5, opcd=11 */
  "---  ", /* type=5, opcd=12 */
  "---  ", /* type=5, opcd=13 */
  "---  ", /* type=5, opcd=14 */
  "---  ", /* type=5, opcd=15 */
  "svc  ", /* type=6, opcd=00 */
  "---  ", /* type=6, opcd=01 */
  "---  ", /* type=6, opcd=02 */
  "---  ", /* type=6, opcd=03 */
  "---  ", /* type=6, opcd=04 */
  "---  ", /* type=6, opcd=05 */
  "---  ", /* type=6, opcd=06 */
  "---  ", /* type=6, opcd=07 */
  "---  ", /* type=6, opcd=08 */
  "---  ", /* type=6, opcd=09 */
  "---  ", /* type=6, opcd=10 */
  "---  ", /* type=6, opcd=11 */
  "---  ", /* type=6, opcd=12 */
  "---  ", /* type=6, opcd=13 */
  "---  ", /* type=6, opcd=14 */
  "---  ", /* type=6, opcd=15 */
  "pth  ", /* type=7, opcd=00 */
  "---  ", /* type=7, opcd=01 */
  "---  ", /* type=7, opcd=02 */
  "---  ", /* type=7, opcd=03 */
  "---  ", /* type=7, opcd=04 */
  "---  ", /* type=7, opcd=05 */
  "---  ", /* type=7, opcd=06 */
  "---  ", /* type=7, opcd=07 */
  "---  ", /* type=7, opcd=08 */
  "---  ", /* type=7, opcd=09 */
  "---  ", /* type=7, opcd=10 */
  "---  ", /* type=7, opcd=11 */
  "---  ", /* type=7, opcd=12 */
  "---  ", /* type=7, opcd=13 */
  "---  ", /* type=7, opcd=14 */
  "---  ", /* type=7, opcd=15 */
};
#endif

/**/
