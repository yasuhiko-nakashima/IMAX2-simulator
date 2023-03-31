
static char RcsHeader[] = "$Header: /usr/home/nakashim/proj-arm64/src/csim/RCS/emax6.c,v 1.408 2022/10/28 09:13:28 nakashim Exp nakashim $";

/* EMAX6 Simulator                     */
/*         Copyright (C) 2012 by NAIST */
/*         Primary writer: Y.Nakashima */
/*                nakashim@is.naist.jp */

/* emax6.c 2012/9/22 */

#include <stdio.h>
#include "csim.h"
#include "../conv-c2c/emax6.h"
#include "../conv-c2c/emax6lib.c"

/* CGRA hardware */

struct axiif { /* axi status of EMAX6 */
  /* physical interface of AXI/ZYNQ */
  Ull   axi_awaddr            ; /* aligned-address of mm                    v                         */
  Ull   axi_awlen          :16; /* aligned-length  of mm                    v                         */
  Ull   axi_awvalid        : 1; /* axi <- fsm                                                         */
  Ull   axi_awready        : 1; /* axi -> fsm                                                         */
  Ull   axi_wstrb          :32; /* axi byte-enable for 32B                                            */
  Ull   axi_wdata[UNIT_WIDTH] ; /* axi -> write-data                        v                         */
  Ull   axi_wvalid         : 1; /* axi -> fsm write-valid                                             */
  Ull   axi_wlast          : 1; /* axi -> fsm write-last                                              */
  Ull   axi_wready         : 1; /* axi <- fsm write-ready                                             */
  Ull   axi_araddr            ; /* aligned-address of mm    v   v   v   v           v                 */
  Ull   axi_arlen          :16; /* aligned-length  of mm    v   v   v   v           1                 */
  Ull   axi_arvalid        : 1; /* axi <- fsm                                                         */
  Ull   axi_arready        : 1; /* axi -> fsm                                                         */
  Ull   axi_rdata[UNIT_WIDTH] ; /* axi read-data            v   v   v   v           v                 */
  Ull   axi_rvalid         : 1; /* axi <- fsm read-valid                                              */
  Ull   axi_rlast          : 1; /* axi <- fsm read-last                                               */
  Ull   axi_rready         : 1; /* axi -> fsm read-ready                                              */

  /* work for axi-side */
  Ull   dma_stat           : 2; /* 0:none, 2:dma_lmmread, 3:dma_lmmwrite */
  Ull   wadr_sent          : 1; /* 0:none, 1:sent for write */
  Ull   radr_sent          : 1; /* 0:none, 1:sent for read */
  Ull   dadr               :31; /* ddr-adr (internal) */
  Ull   madr               :31; /* lmm-adr (internal) */
  Ull   mlen               :16; /* len (internal) */
  Ull   mreq               :16; /* counter (internal) */
  Ull   fmask              : 8; /* mask for first 32B-chunk                                           */
  Ull   lmask              : 8; /* mask for last  32B-chunk                                           */

  /* work for emax-side */
  Ull   wadr_recv          : 1; /* 0:none, 1:recv for write */
  Ull   radr_recv          : 1; /* 0:none, 1:recv for read */
  Ull   reqn               :16; /* AXI¤«¤éÁ÷½Ð¤µ¤ì¤¿on-the-fly req¿ô */
  Ull   creg               : 1; /* 0:RD unit regs, 1:RD control regs */
  Ull   srw                : 1; /* 0:read, 1:write */
  Ull   sadr               :31; /* adr (internal) */
  Ull   slen               :16; /* len (internal) */
  Ull   sreq               :16; /* counter (internal) */

  /* physical interface (pipelined) to EMAX6-body */
  Ull   axring_ful2   : 2; /* 0:empty, 2:full */
  Ull   axring_b_top  : 2; /* to be enqueued next */
  Ull   axring_b_bot  : 2; /* to be dequeued next */
  Ull   exring_deq_wait : 1; /* synchronize exring deq */
#define AXRING_BR_BUF 2
  struct axring_br {
    Ull   rw          : 1; /* 0:read, 1:write */
    Ull   ty          : 3; /* 0:reg/conf, 1:reg/breg, 2:reg/addr, 3:lddmq/tr, 4:lmm, 567:-- */
    Ull   col         : 2; /* logical col# for target lmm */
    Ull   sq          :16; /* sequential # for pipelined DMA 0:last, init by awlen/arlen and decremented */
    Ull   av          : 1; /* address valid */
    Ull   a           :31; /* logical addr reg/lmm */
    Ull   dm          :32; /* <- lmm wdata       */
    Ull   d[UNIT_WIDTH]  ; /* <- lmm wdata/rdata */
  } axring_br[AXRING_BR_BUF]; /* reg */
  Ull   deq_wait      : 1; /* AXI->unit[EMAX_DEPTH/LMRING_MUX-1, EMAX_DEPTH/LMRING_MUX*2-1,...] */
} axiif[MAXCORE]; /* used as EMAX_NCHIP */

struct exring { /* ex status of EMAX6 */
  Ull   cmd_busy        : 1; /* reg_ctrl.stat¤ËÄ¾ÀÜÈ¿±Ç */
  Ull   unit_busy       : 1; /* reg_ctrl.stat¤ËÄ¾ÀÜÈ¿±Ç */
  Ull   cycle           : 3;

  struct unit { /* hardware status of EMAX6 units */
    Ull   cmd           : 2; /* internal copy of reg_ctrl.cmd */
    Ull   cycle         : 3; /* previous unit1_exec/stop is reffered every 4 cycles */
                             /* previous br[0/1] is switched every 4 cycles */
                             /* brout[cycle-4] is updated */
    Ull   l_row         : 6; /* 0..63 *//* 0¤ÎÊªÍý¹Ô¤¬µ¯Æ°¤Îµ¯ÅÀ */
    Ull   scon_count    : 7; /* conf.mapdist*2 */
    Ull   one_shot      : 1; /* reg  *//* self_loop_control 0:init 1:self_loop, keep 0 in first 4 cycles */
    Ull   one_shot2     : 1; /* one_shot for stage2 */
    Ull   one_shot_fold : 1; /* foldingÍÑ¤Ëone_shot¤«¤é4¦ÓÃÙ±ä */
    Ull   one_shot_fold2: 1; /* foldingÍÑ¤Ëone_shot¤«¤é5¦ÓÃÙ±ä */
    Ull   one_shot_fold3: 1; /* foldingÍÑ¤Ëone_shot¤«¤é6¦ÓÃÙ±ä */
    Ull   one_shot_fold4: 1; /* foldingÍÑ¤Ëone_shot¤«¤é7¦ÓÃÙ±ä */
    Ull   unit1_exec    : 1; /* ¼¡¤ÎÆ°ºî¤ò»Ø¼¨ (cex,exe,eag), 0:wait 1:exec *//* reg_ctrl.stat¤ËÄ¾ÀÜÈ¿±Ç */
    Ull   unit1_fold    : 1; /* foldingÍÑ¤Ëunit1_exec¤«¤é4¦ÓÃÙ±ä */
    Ull   stage_forstat : 2; /* from for()for(), bit0:LOOP0=zero, bit1:LOOP1=zero stage2¤Ë¤ª¤¤¤ÆËè¥µ¥¤¥¯¥ëÀ¸À® */
    Ull   unit1_forstat : 2; /* from for()for(), bit0:LOOP0=zero, bit1:LOOP1=zero 4¦ÓËè¤ËÀ¸À® */
    Ull   unit1_forstat2: 2; /* unit1_forstat for stage2 */
    Ull   unit1_forstat_fold:  2; /* forstat+folding for stage1 */
    Ull   unit1_forstat_fold2: 2; /* forstat+folding for stage2 */
    Ull   unit1_forstat_fold3: 2; /* forstat+folding for stage3 */
    Ull   unit1_forstat_fold4: 2; /* forstat+folding for stage4 */
    Ull   unit1_arbrk   : 1; /* loopºÇ½ªÌ¿Îá¤Î¼Â¹Ô´°Î»¤òÉ½¼¨ */
    Ull   unit1_stop    : 1; /* ¼¡¤ÎÆ°ºî¤ò»Ø¼¨ (cex,exe,eag), 0:wait 1:stop */
    Ull   tr_valid      : 1; /* TR¤Î¾õÂÖ¤òÉ½¼¨ */
    Ull   unit2_exec    : 1; /* ¼¡¤ÎÆ°ºî¤ò»Ø¼¨ (lmm),         0:wait 1:exec *//* reg_ctrl.stat¤ËÄ¾ÀÜÈ¿±Ç */
    Ull   unit2_fold    : 1; /* foldingÍÑ¤Ëunit2_exec¤«¤é4¦ÓÃÙ±ä */
    Ull   unit2_forstat : 2; /* from for()for(), bit0:LOOP0=zero, bit1:LOOP1=zero */
    Ull   unit2_stop    : 1; /* ¼¡¤ÎÆ°ºî¤ò»Ø¼¨ (lmm),         0:wait 1:stop */
    Ull   brout_valid   : 1; /* BR¤Î¾õÂÖ¤òÉ½¼¨ ¾ï»þtr_valid¤ò1¦Ó¸å¤ËÅÁÈÂ */

    Ull   stage2_exec   : 1; /* unit1_exec  -> stage2_exec */
    Ull   stage2_fold   : 1; /* foldingÍÑ¤Ëstage2_exec¤«¤é4¦ÓÃÙ±ä */
    Ull   stage3_exec   : 1; /* stage2_exec -> stage3_exec */
    Ull   stage3_fold   : 1; /* foldingÍÑ¤Ëstage3_exec¤«¤é4¦ÓÃÙ±ä */
    Ull   stage4_exec   : 1; /* stage3_exec -> stage4_exec */
    Ull   stage4_fold   : 1; /* foldingÍÑ¤Ëstage4_exec¤«¤é4¦ÓÃÙ±ä */
    Ull   cx[EMAX_WIDTH]   ; /* reg  */
    Ull   cx2dr         : 2; /* reg  *//* bit1: 0:none 1:exec, bit0: 0:none 1:exec */
    Ull   cx3dr         : 2; /* reg  *//* bit1: 0:none 1:exec, bit0: 0:none 1:exec */
    Ull   ex1              ; /* reg  *//* in for ALU */
    Ull   ex2              ; /* reg  *//* in for ALU */
    Ull   ex3              ; /* reg  *//* in for ALU */
    Ull   ex2passr1     : 8; /* reg  *//* pass r1 for OP_SFMA */
    Ull   ex2passr2        ; /* reg  *//* pass r4 for OP_SFMA/x11_softu64_dist */
    Ull   ex2passr3        ; /* reg  *//* pass r4 for OP_SFMA/x11_softu64_dist */
    Ull   ex2passr4     : 8; /* reg  *//* pass r4 for OP_SFMA */
    Ull   ex2dr            ; /* reg  *//* out from first-stage */
    Ull   ex2dr_sfma0      ; /* reg  *//* out from first-stage/softu64 */
    Ull   ex2dr_sfma1      ; /* reg  *//* out from first-stage/softu64 */
    Ull   ex2dr_sfma2      ; /* reg  *//* out from first-stage/softu64 */
    Ull   ex2dr_sfma3      ; /* reg  *//* out from first-stage/softu64 */
    Ull   ex2dr_sfma4      ; /* reg  *//* out from first-stage/softu64 */
    Ull   ex2dr_sfma5      ; /* reg  *//* out from first-stage/softu64 */
    Ull   ex2dr_sfma6      ; /* reg  *//* out from first-stage/softu64 */
    Ull   ex2dr_sfma7      ; /* reg  *//* out from first-stage/softu64 */
    Ull   ex3passr1     : 8; /* reg  *//* pass r1 for OP_SFMA */
    Ull   ex3passr2        ; /* reg  *//* pass r4 for OP_SFMA/x11_softu64_dist */
    Ull   ex3passr3        ; /* reg  *//* pass r4 for OP_SFMA/x11_softu64_dist */
    Ull   ex3dr            ; /* reg  *//* out from second-stage */
    Ull   ex4dr_prev       ; /* reg  *//* for siml-loop only */
    Ull   ex4dr            ; /* reg  *//* out from third-stage */
    Ull   eab           :18; /* wire *//* in for ALU */
    Ull   eao           :64; /* wire *//* in for ALU */
    Ull   ea0b          :18; /* reg  *//* in for EA0 */
    Ull   ea0o          :64; /* reg  *//* in for EA0 */
    Ull   ea1b          :18; /* reg  *//* in for EA1 */
    Ull   ea1o          :64; /* reg  *//* in for EA1 */
    Ull   ea02dofs         ; /* reg  *//* ¡úfor passing eag offset */
    Ull   ea02dr           ; /* reg  *//* for mex(&addr) pointer */
    Ull   ea12dofs         ; /* reg  *//* ¡úfor passing eag offset */
    Ull   ea12dr           ; /* reg  *//* for mex(&addr) pointer */
    Ull   ea03woofs     :18; /* reg  *//* ¡úfor mex(&addr) feedback */
    Ull   ea03dr           ; /* reg  *//* for eag(&addr) pointer */
    Ull   ea13woofs     :18; /* reg  *//* ¡úfor mex(&addr) feedback */
    Ull   ea13dr           ; /* reg  *//* for eag(&addr) pointer */
    Ull   ea04_lmask    :18; /* wire *//* offset */
    Ull   ea04_umask    : 2; /* wire *//* partition */
    Ull   ea04woofs_prev:18; /* reg  *//* ¡úfor siml-loop only */
    Ull   ea04woofs     :18; /* reg  *//* ¡úfor mex(&addr) feedback */
    Ull   ea04dr        :18; /* reg  *//* base+mex+ofs */
    Ull   ea14_lmask    :18; /* wire *//* offset */
    Ull   ea14_umask    : 2; /* wire *//* partition */
    Ull   ea14woofs_prev:18; /* reg  *//* ¡úfor siml-loop only */
    Ull   ea14woofs     :18; /* reg  *//* ¡úfor mex(&addr) feedback */
    Ull   ea14dr        :18; /* reg  *//* base+mex+ofs */
    Ull   tx[UNIT_WIDTH]   ; /* reg  */
    Ull   tx2dr[UNIT_WIDTH]; /* reg  */
    Ull   tx3dr[UNIT_WIDTH]; /* reg  */
    Ull   tx4dr[UNIT_WIDTH]; /* reg  */

    Ull   ranger_ok     : 8; /* wire *//* lmringÍ×µá¤¬read &ty==4&adr[col]<>lmm_rangeÆâ */
    Ull   rangew_ok     : 8; /* wire *//* lmringÍ×µá¤¬write&ty==4&adr[col]<>lmm_rangeÆâ */
    Ull   lmranger_ok   : 8; /* wire *//* lmringÍ×µá¤¬read &ty==4&adr[col]<>lmm_rangeÆâ */
    Ull   lmrangew_ok   : 8; /* wire *//* lmringÍ×µá¤¬write&ty==4&adr[col]<>lmm_rangeÆâ */
    Ull   lmlddmqw_ok   : 1; /* wire *//* lmringÍ×µá¤¬write&ty==3&op1[col]==LDDMQ */
    Ull   lmea0sfma     : 1; /* wire *//* sfma+ea0.stbrÂ¸ºß 4¥µ¥¤¥¯¥ë¤ËÊ¬¤±¤Æ¼Â¹Ô */
    Ull   lmea0strq     : 1; /* wire *//* ea0.strqÂ¸ºß      4¥µ¥¤¥¯¥ë¤ËÊ¬¤±¤Æ¼Â¹Ô */
    Ull   lmea0strqcol  : 2; /* wire *//* ea0.strq_colÈÖ¹æ  4¥µ¥¤¥¯¥ë¤ËÊ¬¤±¤Æ¼Â¹Ô */
    Ull   lmring_ea0bsy : 1; /* wire *//* ea0Í­¸ú */
    Ull   lmring_ea1bsy : 1; /* wire *//* ea1Í­¸ú */
    Ull   lmring_ful    : 1; /* wire *//* (ful2==3)|(ful1 & (ful2==2)) */
    Ull   deq_wait      : 1; /* wire *//* lmring_ful|(ranger_ok&ea1)|(rangew_ok&ea0)|(lddmqw_ok&col!=j) */
    Ull   lmring_ful1   : 1; /* 0:rw/ty/co/sq/a/di/dmÌµ¸ú, 1:rw/ty/co/sq/a/di/dmÍ­¸ú */
    struct lmring_tr {
      Ull   rw          : 1; /* 0:read, 1:write */
      Ull   ty          : 3; /* 0:reg/conf, 1:reg/breg, 2:reg/addr, 3:lddmq/tr, 4:lmm, 567:-- */
      Ull   col         : 2; /* logical col# for target lmm */
      Ull   sq          :16; /* sequential # for pipelined DMA */
      Ull   av          : 1; /* address valid */
      Ull   a           :31; /* logical addr reg/lmm */
      Ull   dm          :32; /* <- lmm wdata       */
      Ull   d[UNIT_WIDTH]  ; /* <- lmm wdata/rdata */
      Ull   merge       : 8; /* wordwise 0:merge_lmm, 1:pass_lmm */
    } lmring_tr;             /* reg */

    Ull   lmco          : 2; /* wire *//* -> col# */
    Ull   lmca          :18; /* wire *//* -> ea01dr *//* col#¤Ë¤è¤ë2bitÊäÀµÁ°addr */
    Ull   lmwm          :32; /* wire *//* <- axi   */
    Ull   lmwd[UNIT_WIDTH] ; /* wire *//* <- axi   */
    Ull   lmrd[UNIT_WIDTH] ; /* wire *//* -> axi   */
    Ull   mwmux[UNIT_WIDTH]; /* wire for mw0[] */
    struct lmm {
      Ull   en0         : 1; /* internal reg       */
      Ull   en1         : 1; /* internal reg       */
      Ull   rw0         : 1; /* 0:read, 1:write    */
      Ull   rw1         : 1; /* 0:read,(1:write)   */
      Ull   ma0         :18; /* internal reg  addr(32B aligned)      */
      Ull   ma1         :18; /* internal reg  addr(32B aligned)      */
      Ull   mm0         :32; /* internal reg  mask */
      Ull   mw0[UNIT_WIDTH]; /* internal reg  data */
      Ull   mr0[UNIT_WIDTH]; /* internal wire data */
      Ull   mr1[UNIT_WIDTH]; /* internal wire data */
      Uchar m[LMEM_SIZE]   ; /* local memory       */
    } lmm;

    Ull   mr0mux        : 2; /* mr0[3-0] -> brs0     */
    Ull   mr1mux        : 2; /* mr1[3-0] -> brs1     */
    Ull   mr0d             ; /* muxed data for BR[0] */
    Ull   mr1d             ; /* muxed data for BR[1] */
    Ull   mexmr0d_prev     ; /* ¡úfor mex */
    Ull   mexmr0d          ; /* ¡úfor mex */
    Ull   mexmr1d_prev     ; /* ¡úfor mex */
    Ull   mexmr1d          ; /* ¡úfor mex */
    struct {Ull r[UNIT_WIDTH];} b[2][EMAX_WIDTH]; /* shadow_breg *//* constant¤ÏÎ¾Êý¤Ë¥»¥Ã¥È */

    Ull   lmring_ful2   : 2; /* 0:empty, 3:full */
    Ull   lmring_b_top  : 2; /* to be enqueued next */
    Ull   lmring_b_bot  : 2; /* to be dequeued next */
#define LMRING_MUX    8
#define LMRING_BR_BUF 3
    struct lmring_br {
      Ull   rw          : 1; /* 0:read, 1:write */
      Ull   ty          : 3; /* 0:reg/conf, 1:reg/breg, 2:reg/addr, 3:lddmq/tr, 4:lmm, 567:-- */
      Ull   col         : 2; /* logical col# for target lmm */
      Ull   sq          :16; /* sequential # for pipelined DMA */
      Ull   av          : 1; /* address valid */
      Ull   a           :31; /* logical addr reg/lmm */
      Ull   dm          :32; /* <- lmm wdata       */
      Ull   d[UNIT_WIDTH]  ; /* <- lmm wdata/rdata */
    } lmring_br[LMRING_BR_BUF]; /* reg */
  } unit[AMAP_DEPTH];
} exring[MAXCORE];

Ull get_tcureg_valid(cid, col) int cid, col;
{
#ifndef IGNORE_LDDMQ_HANDSHAKE
  return (emax5[cid].fsm[col].tcureg_valid);
#else
  return (0);
#endif
}

put_tcureg_ready(cid, col) int cid, col;
{
#ifndef IGNORE_LDDMQ_HANDSHAKE
  emax5[cid].fsm[col].tcureg_ready = 1;
#endif
}

Ull get_tcureg_last(cid, col) int cid, col;
{
#ifndef IGNORE_LDDMQ_HANDSHAKE
  return (emax5[cid].fsm[col].tcureg_last);
#else
  return (0);
#endif
}

put_tcureg_term(cid, col) int cid, col;
{
#ifndef IGNORE_LDDMQ_HANDSHAKE
  emax5[cid].fsm[col].tcureg_term = 1;
#endif
}

Ull get_tcureg(cid, col, n) int cid, col, n;
{
#ifndef IGNORE_LDDMQ_HANDSHAKE
  return (emax5[cid].fsm[col].tcureg[n]);
#else
  return (0);
#endif
}

emax_lmm_init()
{
  int i, j, k;

  for (i=0; i<MAXCORE; i++) {
    for (j=0; j<EMAX_DEPTH; j++) {
      for (k=0; k<LMEM_SIZE; k++)
	exring[i].unit[j].lmm.m[k] = 0xff;
    }
  }
}

/* EMAX6 control */
struct dma_ctrl dma_ctrl; /* body of dma_ctrl */
struct reg_ctrl reg_ctrl; /* body of reg_ctrl */

//application -> emax6_start((Ull*)emax6_conf_x1, (Ull*)emax6_lmmi_x1, (Ull*)emax6_regv_x1);
//            -> svc 0xf1
siml_emax6(cid, trace, trace_pipe)
     /* coreËè¤Ëemax¤Ï1¤Ä¤Ê¤Î¤Ç,t[cid]¤ò»ÈÍÑ */
     /* ¤¿¤À¤·EMAX6¤Î¥«¥¹¥±¡¼¥ÉÀÜÂ³ÈÇIMAX¤Ç¤Ï¡¤core[0]¤ËÁ´EMAX6¤òÀÜÂ³. siml¤ÎÅÔ¹ç¾å,EMAX_NCHIP <= MAXCORE */
     Uint cid; Uint trace, trace_pipe;
{
  /*¨£¨¡¨¤  ¨£¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¤¨£¨¡¨¤¨£¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¤   */
  /*¨¢ A¨¢  ¨¢awaddr        $1 WR/RD-req      awaddr¨¢¨¢ A¨¢¨¢awaddr        $1 WR/RD-req      awaddr¨¢   */
  /*¨¢ R¨¢  ¨¢awdata  ¨£¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¤awdata¨¢¨¢ R¨¢¨¢awdata  ¨£¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¤awdata¨¢   */
  /*¨¢ M¨¢  ¨¢araddr  ¨¢  ¨£exring¨¡¢«¨¡¨¤  ¨¢araddr¨¢¨¢ M¨¢¨¢araddr  ¨¢  ¨£exring¨¡¢«¨¡¨¤  ¨¢araddr¨¢   */
  /*¨¢¨¡¨©$1¨§¨¡¨¤  $3¨¢$3¨¢¨£¨¤¨£¨¤¨£¨¤¨¢  ¨¢  ¨£¨¡¨©¨¢  ¨¢¨§¨¡¨¤  $3¨¢$3¨¢¨£¨¤¨£¨¤¨£¨¤¨¢  ¨¢  ¨£¨¡¨©   */
  /*¨¢ I¨§¨¡¨©PD¨§¢ª¨¡¢¡¨¡¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¨¤¨¦¢ª¨©PD¨§¨¡¢ª¨¡¨©PD¨§¢ª¨¡¢¡¨¡¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¨¤¨¦¢ª¨©PD¨§¢ª */
  /*¨¢ /¨¢  ¨¢IM¨¢SL l¨§¨¡¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¨©  MA¨¢IM¨¢¨¢  ¨¢¨¢IM¨¢SL l¨§¨¡¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¨©  MA¨¢IM¨¢   */
  /*¨¢ O¨§¨¡¨©OA¨§¨¤ m¨§¨¡¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¨©  ¨£¨©OA¨§¨¡¢«¨¡¨©OA¨§¨¤ m¨§¨¡¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¨©  ¨£¨©OA¨§¢« */
  /*¨¢¨¡¨©$2¨§¨¡¨¥¨¢ r¨§¨¡¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¨©  ¨¢¨¦¨¡¨©¨¢  ¨¢¨§¨¡¨¥¨¢ r¨§¨¡¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¨©  ¨¢¨¦¨¡¨©   */
  /*¨¢#0¨¢  ¨¢¡¡  ¨¢ i¨§¨¡¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¨©  ¨¢    ¨¢¨¢#1¨¢¨¢    ¨¢ i¨§¨¡¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¨©  ¨¢    ¨¢   */
  /*¨¢¡¡¨¢  ¨¢  ¡¡¨¢ n¨§¨¡¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¨©  ¨¢¡¡  ¨¢¨¢  ¨¢¨¢  ¡¡¨¢ n¨§¨¡¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¨©  ¨¢¡¡  ¨¢   */
  /*¨¢¡¡¨¢  ¨¢    ¨¢ g¨§¨¡¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¨©  ¨¢    ¨¢¨¢  ¨¢¨¢    ¨¢ g¨§¨¡¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¨©  ¨¢    ¨¢   */
  /*¨¢¡¡¨¢  ¨¢    ¨¢  ¨¦¨¡¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¨©  ¨¢    ¨¢¨¢  ¨¢¨¢    ¨¢  ¨¦¨¡¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¨©  ¨¢    ¨¢   */
  /*¨¢ L¨¢  ¨¢ ¡¡ ¨¢      ¨¦¨¥¨¦¨¥¨¦¨¥¨¦¨¥¢­$4¨¢ ¡¡ ¨¢¨¢ L¨¢¨¢ ¡¡ ¨¢      ¨¦¨¥¨¦¨¥¨¦¨¥¨¦¨¥¢­$4¨¢ ¡¡ ¨¢   */
  /*¨¢ 1¨¢  ¨¢ ¡¡ ¨¦¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¢«¨¡¨¡¢¡¨¡¨¥ ¡¡ ¨¢¨¢ 1¨¢¨¢ ¡¡ ¨¦¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¢«¨¡¨¡¢¡¨¡¨¥ ¡¡ ¨¢   */
  /*¨¢ $¨¢  ¨¢ardata               $4 RD-wait ardata¨¢¨¢ $¨¢¨¢ardata               $4 RD-wait ardata¨¢   */
  /*¨¦¨¡¨¥  ¨¦¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¥¨¦¨¡¨¥¨¦¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¥   */
  /*                               chip#0                             | #1             #2                */
  /* siml_axi_iorq(cid, trace)     c[0].iorq <- axiif[0]       $2     | -              -                 */
  /* siml_iorq_axi(cid, trace)     c[0].iorq -> axiif[0]       $1     | -              -                 */
  /* for (row) {                   */
  /*   siml_unit_lmm(cid, row)     */
  /*   siml_unit_stage5(cid, i)    */
  /* }                             */
  /* for (row)                     */
  /*   siml_unit_stage4_pre(cid, row)*/
  /* siml_exring_deq_wait(cid)     axiif.exring_deq_wait(axi->lmring) |                                  */
  /* for (row) {                   */
  /*   siml_unit_stage4(cid, row)  bri: axring/lmringÀÚÂØ¤¨                                              */
  /*   siml_unit_stage3(cid, row)  */
  /*   siml_unit_stage2(cid, row)  */
  /*   siml_unit_stage1(cid, row)  */
  /* }                             */
  /* siml_lmring_axi(cid, trace)   axiif[0] <- lmring[0.term]  $4     | lm[1.term]     lm[2.term]        */
  /* siml_axi_lmring(cid, trace)   axiif[0] -> axiif[0].axring $3     |                                  */

  int i, j, k, row0, prev_stat[EMAX_NCHIP], emax_stat, busy;
  Ull a;
  Ull steps = t[cid].total_steps;
  Ull cycle = t[cid].total_cycle;

#if 1
  if (cycle % ARM_EMAX6_RATIO) /* ARM:2.0GHz EMAX6:1.0GHz */
    return (0);
#endif

  if (cid>=EMAX_NCHIP) { /* EMAX6 is attached on cid=0:EMAX_NCHIP-1 */
    if (EMAX_NCHIP > MAXCORE)
      printf("EMAX_NCHIP(%d) should be <= MAXCORE(%d)\n", EMAX_NCHIP, MAXCORE);
    return (0);
  }

  /* update LMRING/EXRING */
  prev_stat[cid] =  c[0].iorq.v_stat || (reg_ctrl.i[cid].stat & 0xffff00ff);
  /* ¥½¥Õ¥È¤«¤é¤Îstat¸¡ºº»þ¤Ë(c[cid].iorq.v_stat)¤ò´Þ¤á¤ë¤È¾ï¤Ëbusy */
  reg_ctrl.i[cid].stat = ((LMEM_SIZE==131072)?2:(LMEM_SIZE==65536)?1:0)<<12
                       | ((EMAX_DEPTH==64)?3:(EMAX_DEPTH==32)?2:(EMAX_DEPTH==16)?1:0)<<8
                       | (!(axiif[cid].axi_arvalid||axiif[cid].axi_awvalid||axiif[cid].axi_wvalid||axiif[cid].reqn)?LMRING_IDLE:LMRING_BUSY)<<4
                       | (!(exring[cid].cmd_busy||exring[cid].unit_busy)?EXRING_IDLE:EXRING_BUSY);
  if (prev_stat[cid] && !c[0].iorq.v_stat && !(reg_ctrl.i[cid].stat & 0xffff00ff))
    printf("%03.3d:EE %08.8x_%08.8x cycle=%08.8x_%08.8x ---EMAX6 IO/CMD-END----\n", cid, (Uint)(steps>>32), (Uint)steps, (Uint)(cycle>>32), (Uint)cycle);

#if 1
  emax_stat = 0;
  for (i=0; i<EMAX_NCHIP; i++) emax_stat |= (reg_ctrl.i[i].stat & 0xffff00ff);
  if (!c[0].iorq.v_stat && !emax_stat) return (0);
#endif

  if (cid == 0) { /* cid#0¤Î¤ß */
    siml_axi_iorq(cid, trace);   /* AXI->IORQ */
    siml_iorq_axi(cid, trace);   /* IORQ->AXI */
  }

  /* find top_row */
  for (row0=0; row0<EMAX_DEPTH; row0++)           /* EXRING¤ÏÏÀÍýrow0¤«¤ésiml¤¹¤ëÉ¬Í×¤¬¤¢¤ê,LMRING¤â¹ç¤ï¤»¤ë */
    if (exring[cid].unit[row0].l_row == 0) break; /* LMRING¤Ï¼ÂºÝ¤Ë¤ÏÊªÍýrow0¤¬fsm¤ËÀÜÂ³¤µ¤ì¤Æ¤¤¤ë¤¬,siml»þ¤ÏÏÀÍýrow0¤¬ÀèÆ¬¤È¹Í¤¨¤ÆÌäÂê¤Ê¤¤ */

  /********************************************************/
  siml_lmring_axi(cid, trace); /* LMRING->AXI */

  /* siml_unit_stage4()¤ÎÁ°¤ËLMRING_MUX¸Ä¤Îdeq_wait¤ò½¸Ìó */
  /* ¤Ê¤ª,Á´stage5¤ÈÁ´stage4¤ò¸ò¸ß¤Ësiml¤¹¤ë¤È,Ëè¥µ¥¤¥¯¥ëlmring¤¬2stage¿Ê¤à¤³¤È¤Ë¤Ê¤ë */
  /* ¤·¤«¤·.lmring_axi¤Ï1stage¤·¤«½èÍý¤·¤Ê¤¤¤Î¤Ç,À­Ç½¸«ÀÑ¤ê¾å¤ÎÊÀ³²¤Ï¤Ê¤¤ 20190828 */
  for (i=(row0+EMAX_DEPTH-1)%EMAX_DEPTH;; i=(i+EMAX_DEPTH-1)%EMAX_DEPTH) { /* for each unit */
    siml_unit_lmm(cid, i);
    siml_unit_stage5(cid, i); /* stage-5 (4DR->BROUT)(LMRING_TR->LMRING_BROUT) */
    if (i==row0)
      break;
  }

  for (i=(row0+EMAX_DEPTH-1)%EMAX_DEPTH;; i=(i+EMAX_DEPTH-1)%EMAX_DEPTH) { /* for each unit */
    siml_unit_stage4_pre(cid, i);
    if (i==row0)
      break;
  }
  siml_exring_deq_wait(cid, trace);

  /* siml unit */
  for (i=(row0+EMAX_DEPTH-1)%EMAX_DEPTH;; i=(i+EMAX_DEPTH-1)%EMAX_DEPTH) { /* for each unit */
    siml_unit_stage4(cid, i); /* stage-4 (3DR->4DR)  (LMRING_BRIN->LMRING_TR) */
    siml_unit_stage3(cid, i); /* stage-3 (2DR->3DR) */
    siml_unit_stage2(cid, i); /* stage-2 (EX/TX->2DR) */
    siml_unit_stage1(cid, i); /* stage-1 (BRIN->EX/TX) */
    if (i==row0)
      break;
  }

  siml_axi_lmring(cid, trace); /* AXI->LMRING */
  /********************************************************/

  busy = 0;
  for (i=0; i<EMAX_DEPTH; i++) { /* for each unit */
    if (exring[cid].unit[i].unit1_exec || exring[cid].unit[i].unit1_fold || exring[cid].unit[i].unit1_stop || exring[cid].unit[i].unit2_exec || exring[cid].unit[i].unit2_fold || exring[cid].unit[i].unit2_stop)
      busy = 1;
  }
  exring[cid].unit_busy = busy;

  if (trace && trace_pipe)
    show_emax6_status(cid);

#if 0
printf("@@@@ cid=%d c[0].iorq.v_stat=%d arvalid=%d, awvalid=%d, wvalid=%d, reqn=%d, cmd_busy=%d, unit_busy=%d axring_ful2=%d", cid, c[0].iorq.v_stat,
axiif[cid].axi_arvalid, axiif[cid].axi_awvalid, axiif[cid].axi_wvalid, axiif[cid].reqn, exring[cid].cmd_busy, exring[cid].unit_busy, axiif[cid].axring_ful2);

for (i=0; i<EMAX_DEPTH; i++) {
if (i%8==0) printf(" ");
printf("%d%d", exring[cid].unit[i].lmring_ful1, exring[cid].unit[i].lmring_ful2);
}
printf("\n");
#endif

  return (0);
}

siml_axi_iorq(cid, trace) Uint cid, trace;
{
  /* LMM¤Îcolumn¥Þ¥ë¥Á¥¹¥ì¥Ã¥Ç¥¤¥ó¥°¤Ï,EXRING¤òÍ¥Àè¤·,LMRING¤Ï·ä´Ö¤ÇÆ°ºî¤µ¤»¤ë */
  /* LMRING¤Ï¼ÂºÝ¤Ë¤ÏÊªÍýrow0¤¬fsm¤ËÀÜÂ³¤µ¤ì¤Æ¤¤¤ë¤¬,siml»þ¤ÏÏÀÍýrow0¤¬ÀèÆ¬¤È¹Í¤¨¤ÆÌäÂê¤Ê¤¤ */

  /*                                                                                                 *//* axi_write_busy axi_read_busy */
  /* ÏÀÍýÈÖ¹æ           ÊªÍýÈÖ¹æ                     V<--------ENQ (siml_axiif¤¬Àè¤ËENQ)             *//* awaddr+awlen+awvalid ^> awready *//* wdata[]+wstrb+wvalid+wlast -> wready->next */
  /*                              axiif.axring_br =======bri_ful2 ÊªÍý#0¤Îpi¤Ïaxring¤Ëº¹¤·ÂØ¤¨       *//* araddr+arlen+arvalid -> arready */
  /* row0+DEPTH-2              0        |            V^--------waiti(unit[0].deq_wait)               */
  /*                             unit[].lmring_br -------  ²¼¤«¤ésiml.deq_wait¤¬Æ±°ì¦Ó¤Ë¥É¥ß¥ÎÅÝ¤·.  */
  /*                                    |                  ¼Âµ¡¤È°ã¤¦¤¬output¤Î½ÐÊý¤ÏÆ±¤¸            */
  /* row0+DEPTH-1              1        |            V        ¢¬                                     */
  /*     simlµ¯ÅÀ(brout¤ÏÄê¿ô)   unit[].lmring_br -------  SIMLµ¯ÅÀ(1¦ÓÁ°¤Î¼¡ÃÊdeq_wait¤ò»È¤¦.Àµ¾ï)  */
  /* row0                     62        |            V     SIMLºÇ¸å(Á°ÃÊbr¤ÎÃÍ¤¬1¦ÓÌ¤Íè¤Ë¤Ê¤ë)       */
  /*                             unit[].lmring_br -------     ¢¬                                     */
  /* row0+1                   63        |            V                                               */
  /*                             unit[].lmring_br -------bro_ful2                                    */
  /*                                                 |^--------waito(axiif.deq_wait)                 */
  /*                                                 +-------->DEQ (siml_axiif¤¬Àè¤ËDEQ)             *//* rdata[]+rvalid+rlast -> rready->next  */

  /* iorq.v_stat      : 4; v 0:empty 1:reserve 3:inuse | stat 0:empty 1:busy 3:RD-ok */
  /* iorq.tid         :12;                         */
  /* iorq.type        : 4; type                    */
  /* iorq.opcd        : 6; opcd                    */
  /* iorq.ADR            ; °ÊÁ°¤ÎADDR¤ËÂÐ±þ        */
  /* iorq.BUF[2]         ; for load/store          */
  /* iorq.rob            ; for DATA                */

  /* exring.deq_wait  : 1; 0:deq,1:wait            */
  /* axring_ful2      : 2; 0:empty, 2:ful          */
  /* axring_br.rw     : 1; 0:read, 1:write         */
  /* axring_br.ty     : 3; 0:reg/conf, 1:reg/breg, 2:reg/addr, 3:lddmq/tr, 4:lmm, 567:-- */
  /* axring_br.col    : 2; logical col# for target lmm */
  /* axring_br.sq     :16; sequential # for pipelined DMA */
  /* axring_br.a      :31; logical addr reg/lmm    */
  /* axring_br.dm     :32; <- lmm wdata            */
  /* axring_br.d[4]      ; <- lmm wdata/rdata      */

  /* read: LMRING½ªÃ¼¤«¤é¤Î²ó¼ý bro->axiif->iorq */
  /*       HOST:AXIIF->IORQ (ARM-restart¤Ë¤è¤ê,iorq¤Ë³ºÅö¥¨¥ó¥È¥ê¤¬¤Ê¤¤¾ì¹ç¤¬¤¢¤ë) */
  /************************************************************************************************************************************************************************************************************/
  /*                        |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       | */
  /* clk                   _/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/ */
  /* iorq.v_stat : 4 *1101===X=0000==========X========1101===========================X=1111                                                                                                                   */
  /* iorq.tid    :12                                                                 A                                                                                                                        */
  /* iorq.type   : 4  4:write                         3:read                         |                                                                                                                        */
  /* iorq.opcd   : 6  2:LD/STRW 3:LD/STR 12:VLD/VSTRQ                                |                                                                                                                        */
  /* iorq.ADR                                           ===A======                   |                                                                                                                        */
  /* iorq.BUF[2]                                                                -----<=D=====>---                                                                                                             */
  /* iorq.rob                                                                                                                                                                                                 */
  /* rdata[]    SLAVE                                                   -----<=D=====>---                                                                                                                     */
  /* rvalid     SLAVE                                                   _____/~~~~~~~\___                                                                                                                     */
  /* rlast      SLAVE                                                   _____/~~~~~~~\___                                                                                                                     */
  /* rready    MASTER*                                                  _____/~~~~~~~\___                                                                                                                     */
  /************************************************************************************************************************************************************************************************************/

  axiif[cid].axi_rready = 1; /* always 1 */

  if (axiif[cid].dma_stat == 2) { /* DMA RD active */
    if (axiif[cid].mreq <= axiif[cid].mlen) { /* write active */
      if (axiif[cid].axi_rvalid && axiif[cid].axi_rready) { /* new read_req starts */
        printf("%03.3d:DMA RD adr=%08.8x data=%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x\n",
               cid, axiif[cid].madr+axiif[cid].mreq*sizeof(Ull)*UNIT_WIDTH,
               (Uint)(axiif[cid].axi_rdata[3]>>32), (Uint)axiif[cid].axi_rdata[3],
               (Uint)(axiif[cid].axi_rdata[2]>>32), (Uint)axiif[cid].axi_rdata[2],
               (Uint)(axiif[cid].axi_rdata[1]>>32), (Uint)axiif[cid].axi_rdata[1],
               (Uint)(axiif[cid].axi_rdata[0]>>32), (Uint)axiif[cid].axi_rdata[0]);
        if (axiif[cid].mreq == 0) {
          if (axiif[cid].fmask&0x01) mmw(axiif[cid].dadr + axiif[cid].mreq*sizeof(Ull)*UNIT_WIDTH +  0, 0x00000000ffffffffLL, axiif[cid].axi_rdata[0]);
          if (axiif[cid].fmask&0x02) mmw(axiif[cid].dadr + axiif[cid].mreq*sizeof(Ull)*UNIT_WIDTH +  0, 0xffffffff00000000LL, axiif[cid].axi_rdata[0]);
          if (axiif[cid].fmask&0x04) mmw(axiif[cid].dadr + axiif[cid].mreq*sizeof(Ull)*UNIT_WIDTH +  8, 0x00000000ffffffffLL, axiif[cid].axi_rdata[1]);
          if (axiif[cid].fmask&0x08) mmw(axiif[cid].dadr + axiif[cid].mreq*sizeof(Ull)*UNIT_WIDTH +  8, 0xffffffff00000000LL, axiif[cid].axi_rdata[1]);
          if (axiif[cid].fmask&0x10) mmw(axiif[cid].dadr + axiif[cid].mreq*sizeof(Ull)*UNIT_WIDTH + 16, 0x00000000ffffffffLL, axiif[cid].axi_rdata[2]);
          if (axiif[cid].fmask&0x20) mmw(axiif[cid].dadr + axiif[cid].mreq*sizeof(Ull)*UNIT_WIDTH + 16, 0xffffffff00000000LL, axiif[cid].axi_rdata[2]);
          if (axiif[cid].fmask&0x40) mmw(axiif[cid].dadr + axiif[cid].mreq*sizeof(Ull)*UNIT_WIDTH + 24, 0x00000000ffffffffLL, axiif[cid].axi_rdata[3]);
          if (axiif[cid].fmask&0x80) mmw(axiif[cid].dadr + axiif[cid].mreq*sizeof(Ull)*UNIT_WIDTH + 24, 0xffffffff00000000LL, axiif[cid].axi_rdata[3]);
        }
        else if (axiif[cid].mreq == axiif[cid].mlen) {
          if (axiif[cid].lmask&0x01) mmw(axiif[cid].dadr + axiif[cid].mreq*sizeof(Ull)*UNIT_WIDTH +  0, 0x00000000ffffffffLL, axiif[cid].axi_rdata[0]);
          if (axiif[cid].lmask&0x02) mmw(axiif[cid].dadr + axiif[cid].mreq*sizeof(Ull)*UNIT_WIDTH +  0, 0xffffffff00000000LL, axiif[cid].axi_rdata[0]);
          if (axiif[cid].lmask&0x04) mmw(axiif[cid].dadr + axiif[cid].mreq*sizeof(Ull)*UNIT_WIDTH +  8, 0x00000000ffffffffLL, axiif[cid].axi_rdata[1]);
          if (axiif[cid].lmask&0x08) mmw(axiif[cid].dadr + axiif[cid].mreq*sizeof(Ull)*UNIT_WIDTH +  8, 0xffffffff00000000LL, axiif[cid].axi_rdata[1]);
          if (axiif[cid].lmask&0x10) mmw(axiif[cid].dadr + axiif[cid].mreq*sizeof(Ull)*UNIT_WIDTH + 16, 0x00000000ffffffffLL, axiif[cid].axi_rdata[2]);
          if (axiif[cid].lmask&0x20) mmw(axiif[cid].dadr + axiif[cid].mreq*sizeof(Ull)*UNIT_WIDTH + 16, 0xffffffff00000000LL, axiif[cid].axi_rdata[2]);
          if (axiif[cid].lmask&0x40) mmw(axiif[cid].dadr + axiif[cid].mreq*sizeof(Ull)*UNIT_WIDTH + 24, 0x00000000ffffffffLL, axiif[cid].axi_rdata[3]);
          if (axiif[cid].lmask&0x80) mmw(axiif[cid].dadr + axiif[cid].mreq*sizeof(Ull)*UNIT_WIDTH + 24, 0xffffffff00000000LL, axiif[cid].axi_rdata[3]);
        }
        else {
          mmw(axiif[cid].dadr + axiif[cid].mreq*sizeof(Ull)*UNIT_WIDTH +  0, 0xffffffffffffffffLL, axiif[cid].axi_rdata[0]);
          mmw(axiif[cid].dadr + axiif[cid].mreq*sizeof(Ull)*UNIT_WIDTH +  8, 0xffffffffffffffffLL, axiif[cid].axi_rdata[1]);
          mmw(axiif[cid].dadr + axiif[cid].mreq*sizeof(Ull)*UNIT_WIDTH + 16, 0xffffffffffffffffLL, axiif[cid].axi_rdata[2]);
          mmw(axiif[cid].dadr + axiif[cid].mreq*sizeof(Ull)*UNIT_WIDTH + 24, 0xffffffffffffffffLL, axiif[cid].axi_rdata[3]);
        }
        axiif[cid].mreq++;
      }
    }
    else { /* data´°Î» */
      dma_ctrl.ZDMA_CH_STATUS = (dma_ctrl.ZDMA_CH_STATUS & ~3); /* free */
      dma_ctrl.ZDMA_CH_CTRL2  = (dma_ctrl.ZDMA_CH_CTRL2  & ~1); /* free */
      axiif[cid].dma_stat   = 0; /* reset */
      axiif[cid].radr_sent  = 0; /* reset */
    }
  }

  if (c[cid].iorq.v_stat == ((3<<2)|1) && c[cid].iorq.type == 3) { /* load *//* emax6_reg()µ¡Ç½¤ÏºÇ½ªÅª¤Ë¤Ï³ÆÃ´Åöstage¤ËÇÛÃÖ */
    /*emax6_reg(c[cid].iorq.tid, c[cid].iorq.type, c[cid].iorq.opcd, c[cid].iorq.ADR, c[cid].iorq.BUF);*/
    if (c[cid].iorq.ADR < REG_BASE2_PHYS) { /* dma space ... ¸ÇÄê°ÌÃÖ (DMA_BASE2_PHYS-REG_BASE2_PHYS) */
      switch (c[cid].iorq.opcd) {
      case 2:if (c[cid].iorq.ADR & (sizeof(Uint)-1)) printf("%03.3d:emax6_ctl: dma_space load: opcd=%x adr=%08.8x (should be aligned to Uint)\n", c[cid].iorq.tid, c[cid].iorq.opcd, c[cid].iorq.ADR);
             else                                    c[cid].iorq.BUF[0] = *(Uint*)((Uchar*)&dma_ctrl+(c[cid].iorq.ADR-DMA_BASE2_PHYS)); break;
      case 3:if (c[cid].iorq.ADR & (sizeof(Ull )-1)) printf("%03.3d:emax6_ctl: dma_space load: opcd=%x adr=%08.8x (should be aligned to Ull)\n", c[cid].iorq.tid, c[cid].iorq.opcd, c[cid].iorq.ADR);
             else                                    c[cid].iorq.BUF[0] = *(Ull *)((Uchar*)&dma_ctrl+(c[cid].iorq.ADR-DMA_BASE2_PHYS)); break;
      default:                                       printf("%03.3d:emax6_ctl: dma_space load: opcd=%x adr=%08.8x (should be LDRW/LDR)\n", c[cid].iorq.tid, c[cid].iorq.opcd, c[cid].iorq.ADR); break; }
      c[cid].iorq.v_stat = (c[cid].iorq.v_stat&0xc)|3; /* return to sim-core.c */
      if (trace)
        printf("%03.3d:PIO->IORQ RD opcd=%d adr=%08.8x data=%08.8x_%08.8x\n", cid, c[cid].iorq.opcd, c[cid].iorq.ADR, (Uint)(c[cid].iorq.BUF[0]>>32), (Uint)c[cid].iorq.BUF[0]);
    }
    else if (axiif[cid].axi_rvalid && axiif[cid].axi_rready) { /* new read_req starts */
      printf("%03.3d:PIO RD adr=%08.8x data=%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x\n",
             cid, axiif[cid].sadr,
             (Uint)(axiif[cid].axi_rdata[3]>>32), (Uint)axiif[cid].axi_rdata[3],
             (Uint)(axiif[cid].axi_rdata[2]>>32), (Uint)axiif[cid].axi_rdata[2],
             (Uint)(axiif[cid].axi_rdata[1]>>32), (Uint)axiif[cid].axi_rdata[1],
             (Uint)(axiif[cid].axi_rdata[0]>>32), (Uint)axiif[cid].axi_rdata[0]);
      switch (c[cid].iorq.opcd) { /* 2:LD/STRW 3:LD/STR 12:VLD/VSTRQ */
      case 2: /* 32bit */
        switch (c[cid].iorq.ADR&((UNIT_WIDTH-1)*sizeof(Ull))) {
        case  0: c[cid].iorq.BUF[0] = axiif[cid].axi_rdata[0]>>((c[cid].iorq.ADR & sizeof(int))*8); break;
        case  8: c[cid].iorq.BUF[0] = axiif[cid].axi_rdata[1]>>((c[cid].iorq.ADR & sizeof(int))*8); break;
        case 16: c[cid].iorq.BUF[0] = axiif[cid].axi_rdata[2]>>((c[cid].iorq.ADR & sizeof(int))*8); break;
        case 24: c[cid].iorq.BUF[0] = axiif[cid].axi_rdata[3]>>((c[cid].iorq.ADR & sizeof(int))*8); break;
        }
        break;
      case 3: /* 64bit */
        switch (c[cid].iorq.ADR&((UNIT_WIDTH-1)*sizeof(Ull))) {
        case  0: c[cid].iorq.BUF[0] = axiif[cid].axi_rdata[0]; break;
        case  8: c[cid].iorq.BUF[0] = axiif[cid].axi_rdata[1]; break;
        case 16: c[cid].iorq.BUF[0] = axiif[cid].axi_rdata[2]; break;
        case 24: c[cid].iorq.BUF[0] = axiif[cid].axi_rdata[3]; break;
        }
        break;
      case 12: /* 128bit */
        switch (c[cid].iorq.ADR&((UNIT_WIDTH-1)*sizeof(Ull))) {
        case  0: c[cid].iorq.BUF[0] = axiif[cid].axi_rdata[0];
                 c[cid].iorq.BUF[1] = axiif[cid].axi_rdata[1]; break;
        case 16: c[cid].iorq.BUF[0] = axiif[cid].axi_rdata[2];
                 c[cid].iorq.BUF[1] = axiif[cid].axi_rdata[3]; break;
        }
        break;
      }
      c[cid].iorq.v_stat = (c[cid].iorq.v_stat&0xc)|3; /* return to sim-core.c */
      axiif[cid].radr_sent = 0;
      if (trace)
        printf("%03.3d:AXIIF->IORQ RD opcd=%d adr=%08.8x data=%08.8x%08.8x_%08.8x%08.8x\n", cid, c[cid].iorq.opcd, c[cid].iorq.ADR,
               (Uint)(c[cid].iorq.BUF[1]>>32), (Uint)c[cid].iorq.BUF[1],
               (Uint)(c[cid].iorq.BUF[0]>>32), (Uint)c[cid].iorq.BUF[0]);
    }
  }

  return (0);
}

siml_iorq_axi(cid, trace) Uint cid, trace;
{
  int    i, k;

  /* read/write LMRINGÀèÃ¼¤Ø¤ÎÅêÆþ   iorq->axiif->bri */
  /*       HOST:IORQ->AXIIF (ARM-restart¤Ë¤è¤ê,iorq¤Ë³ºÅö¥¨¥ó¥È¥ê¤¬¤Ê¤¤¾ì¹ç¤¬¤¢¤ë) */
  /************************************************************************************************************************************************************************************************************/
  /*                        |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       | */
  /* clk                   _/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/ */
  /* iorq.v_stat : 4 *1101===X=0000==========X========1101===========================X=1111                                                                                                                   */
  /* iorq.tid    :12                                                                                                                                                                                          */
  /* iorq.type   : 4  4:write                         3:read                                                                                                                                                  */
  /* iorq.opcd   : 6  2:LD/STRW 3:LD/STR 12:VLD/VSTRQ                                                                                                                                                         */
  /* iorq.ADR           ===A======                      ===A======                                                                                                                                            */
  /* iorq.BUF[2]        ===D======                                                                                                                                                                            */
  /* iorq.rob                                                                                                                                                                                                 */
  /* awaddr    MASTER*  -----<=A=====>---                                                                                                                                                                     */
  /* awlen     MASTER*  -----<=0=====>---                                                                                                                                                                     */
  /* awvalid   MASTER*  _____/~~~~~~~\___ valid=1                                                                                                                                                             */
  /* awready    SLAVE   ~~~~~~~~~~~~~\___ ready=1»þ¤Ë¼ø¼õ                                                                                                                                                     */
  /* wstrb     MASTER*  -----<=M=====>---                                                                                                                                                                     */
  /* wdata[]   MASTER*  -----<=D=====>---                                                                                                                                                                     */
  /* wvalid    MASTER*  _____/~~~~~~~\___                                                                                                                                                                     */
  /* wlast     MASTER*  _____/~~~~~~~\___ PIO¤Î¾ì¹ç,¾ï¤Ë1                                                                                                                                                     */
  /* wready     SLAVE   ~~~~~~~~~~~~~\___                                                                                                                                                                     */
  /* araddr    MASTER*                                  -----<=A=====>---                                                                                                                                     */
  /* arlen     MASTER*                                  -----<=0=====>---                                                                                                                                     */
  /* arvalid   MASTER*                                  _____/~~~~~~~\___                                                                                                                                     */
  /* arready    SLAVE                                   ~~~~~~~~~~~~~\___                                                                                                                                     */
  /************************************************************************************************************************************************************************************************************/

  if ((exring[cid].cycle&3) == 3 && (reg_ctrl.i[cid].cmd&3) == CMD_RESET) {
    axiif[cid].dma_stat   = 0;
    axiif[cid].wadr_sent  = 0;
    axiif[cid].radr_sent  = 0;
    axiif[cid].dadr       = 0;
    axiif[cid].madr       = 0;
    axiif[cid].mlen       = 0;
    axiif[cid].mreq       = 0;
    axiif[cid].fmask      = 0;
    axiif[cid].lmask      = 0;
  }

  if (axiif[cid].dma_stat == 3) { /* DMA WR active */
    if (!axiif[cid].wadr_sent) {
      if (!axiif[cid].axi_awvalid) {
        axiif[cid].axi_awaddr   = axiif[cid].madr;
        axiif[cid].axi_awlen    = axiif[cid].mlen;
        axiif[cid].axi_awvalid  = 1; /* on */
        printf("%03.3d:DMA WR start ddradr=%08.8x lmmadr=%08.8x len=%04.4x\n",
               cid, axiif[cid].dadr, axiif[cid].madr, axiif[cid].mlen);
      }
      else if (axiif[cid].axi_awready) { /* adr´°Î» */
        axiif[cid].wadr_sent    = 1; /* fin */
        /*axiif[cid].mreq       = 0; *//* length */
        axiif[cid].axi_awvalid  = 0; /* off */
      }
    }
    else if (axiif[cid].mreq <= axiif[cid].mlen) { /* write active */
      if (axiif[cid].mreq == 0)
        axiif[cid].axi_wstrb = ((axiif[cid].fmask&0x80)?0xf0000000:0)
                             | ((axiif[cid].fmask&0x40)?0x0f000000:0)
                             | ((axiif[cid].fmask&0x20)?0x00f00000:0)
                             | ((axiif[cid].fmask&0x10)?0x000f0000:0)
                             | ((axiif[cid].fmask&0x08)?0x0000f000:0)
                             | ((axiif[cid].fmask&0x04)?0x00000f00:0)
                             | ((axiif[cid].fmask&0x02)?0x000000f0:0)
                             | ((axiif[cid].fmask&0x01)?0x0000000f:0);
      else if (axiif[cid].mreq == axiif[cid].mlen)
        axiif[cid].axi_wstrb = ((axiif[cid].lmask&0x80)?0xf0000000:0)
                             | ((axiif[cid].lmask&0x40)?0x0f000000:0)
                             | ((axiif[cid].lmask&0x20)?0x00f00000:0)
                             | ((axiif[cid].lmask&0x10)?0x000f0000:0)
                             | ((axiif[cid].lmask&0x08)?0x0000f000:0)
                             | ((axiif[cid].lmask&0x04)?0x00000f00:0)
                             | ((axiif[cid].lmask&0x02)?0x000000f0:0)
                             | ((axiif[cid].lmask&0x01)?0x0000000f:0);
      else
        axiif[cid].axi_wstrb = 0xffffffff;
      axiif[cid].axi_wdata[0] = mmr(axiif[cid].dadr                );
      axiif[cid].axi_wdata[1] = mmr(axiif[cid].dadr|(sizeof(Ull)*1));
      axiif[cid].axi_wdata[2] = mmr(axiif[cid].dadr|(sizeof(Ull)*2));
      axiif[cid].axi_wdata[3] = mmr(axiif[cid].dadr|(sizeof(Ull)*3));
      axiif[cid].axi_wvalid   = 1; /* on */
      if (axiif[cid].mreq == axiif[cid].mlen)
        axiif[cid].axi_wlast  = 1; /* on */
      else
        axiif[cid].axi_wlast  = 0; /* off */
      if (axiif[cid].axi_wready) { /* prepare next write */
	printf("%03.3d:DMA WR mreq=%d last=%x wstrb=%08.8x data=%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x\n",
	       cid, axiif[cid].mreq, axiif[cid].axi_wlast, axiif[cid].axi_wstrb,
	       (Uint)(axiif[cid].axi_wdata[3]>>32), (Uint)axiif[cid].axi_wdata[3],
	       (Uint)(axiif[cid].axi_wdata[2]>>32), (Uint)axiif[cid].axi_wdata[2],
	       (Uint)(axiif[cid].axi_wdata[1]>>32), (Uint)axiif[cid].axi_wdata[1],
	       (Uint)(axiif[cid].axi_wdata[0]>>32), (Uint)axiif[cid].axi_wdata[0]);
        axiif[cid].dadr+=sizeof(Ull)*UNIT_WIDTH;
        axiif[cid].mreq++;
      }
    }
    else { /* data´°Î» */
      printf("%03.3d:DMA WR FIN\n", cid);
      dma_ctrl.ZDMA_CH_STATUS = (dma_ctrl.ZDMA_CH_STATUS & ~3); /* free */
      dma_ctrl.ZDMA_CH_CTRL2  = (dma_ctrl.ZDMA_CH_CTRL2  & ~1); /* free */
      axiif[cid].dma_stat   = 0; /* reset */
      axiif[cid].wadr_sent  = 0; /* reset */
      axiif[cid].axi_wvalid = 0; /* off */
      axiif[cid].axi_wlast  = 0; /* off */
    }
  }

  if (axiif[cid].dma_stat == 2) { /* DMA RD active */
    if (!axiif[cid].radr_sent) {
      if (!axiif[cid].axi_arvalid) {
        axiif[cid].axi_araddr   = axiif[cid].madr;
        axiif[cid].axi_arlen    = axiif[cid].mlen;
        axiif[cid].axi_arvalid  = 1; /* on */
        printf("%03.3d:DMA RD start lmmadr=%08.8x ddradr=%08.8x len=%04.4x\n",
               cid, axiif[cid].madr, axiif[cid].dadr, axiif[cid].mlen);
      }
      else if (axiif[cid].axi_arready) { /* adr´°Î» */
        axiif[cid].radr_sent    = 1; /* fin */
        /*axiif[cid].mreq       = 0; *//* length */
        axiif[cid].axi_arvalid  = 0; /* off */
      }
    }
  }

  if (c[cid].iorq.v_stat == ((3<<2)|1) && c[cid].iorq.type == 4) { /* store *//* emax6_reg()µ¡Ç½¤ÏºÇ½ªÅª¤Ë¤Ï³ÆÃ´Åöstage¤ËÇÛÃÖ */
    /*emax6_reg(c[cid].iorq.tid, c[cid].iorq.type, c[cid].iorq.opcd, c[cid].iorq.ADR, c[cid].iorq.BUF);*/
    if (c[cid].iorq.ADR < REG_BASE2_PHYS) { /* dma space ... ¸ÇÄê°ÌÃÖ (DMA_BASE2_PHYS-REG_BASE2_PHYS) */
      switch (c[cid].iorq.opcd) {
      case 2:if (c[cid].iorq.ADR & (sizeof(Uint)-1)) printf("%03.3d:emax6_ctl: dma_space store: opcd=%x adr=%08.8x (should be aligned to Uint)\n", c[cid].iorq.tid, c[cid].iorq.opcd, c[cid].iorq.ADR);
             else                                    *(Uint*)((Uchar*)&dma_ctrl+(c[cid].iorq.ADR-DMA_BASE2_PHYS)) = c[cid].iorq.BUF[0]; break;
      case 3:if (c[cid].iorq.ADR & (sizeof(Ull )-1)) printf("%03.3d:emax6_ctl: dma_space store: opcd=%x adr=%08.8x (should be aligned to Ull)\n", c[cid].iorq.tid, c[cid].iorq.opcd, c[cid].iorq.ADR);
             else                                    *(Ull *)((Uchar*)&dma_ctrl+(c[cid].iorq.ADR-DMA_BASE2_PHYS)) = c[cid].iorq.BUF[0]; break;
      default:                                       printf("%03.3d:emax6_ctl: dma_space store: opcd=%x adr=%08.8x (should be STRW/STR)\n", c[cid].iorq.tid, c[cid].iorq.opcd, c[cid].iorq.ADR); break; }
      c[cid].iorq.v_stat = 0; /* immediately finished */
      if (trace)
        printf("%03.3d:IORQ->PIO WR opcd=%d adr=%08.8x data=%08.8x_%08.8x\n", cid, c[cid].iorq.opcd, c[cid].iorq.ADR, (Uint)(c[cid].iorq.BUF[0]>>32), (Uint)c[cid].iorq.BUF[0]);
      if ((dma_ctrl.ZDMA_CH_STATUS&3) != 2 && (dma_ctrl.ZDMA_CH_CTRL2 & 1)) {
        dma_ctrl.ZDMA_CH_STATUS = (dma_ctrl.ZDMA_CH_STATUS & ~3) | 2; /* busy */
        if (dma_ctrl.ZDMA_CH_SRC_DSCR_WORD0 < LMM_BASE2_PHYS) { /* mem->lmm */
          axiif[cid].dma_stat = 3; /* write */
          axiif[cid].dadr     = dma_ctrl.ZDMA_CH_SRC_DSCR_WORD0 & ~(sizeof(Ull)*UNIT_WIDTH-1); /* mem addr */
          axiif[cid].madr     = dma_ctrl.ZDMA_CH_DST_DSCR_WORD0 & ~(sizeof(Ull)*UNIT_WIDTH-1); /* lmm addr */
          axiif[cid].mlen     = ((dma_ctrl.ZDMA_CH_SRC_DSCR_WORD0/sizeof(Uint)+dma_ctrl.ZDMA_CH_SRC_DSCR_WORD2/sizeof(Uint)-1)/(UNIT_WIDTH*2))
                              - ((dma_ctrl.ZDMA_CH_SRC_DSCR_WORD0/sizeof(Uint)                                               )/(UNIT_WIDTH*2)); /* 0:1cycle, 1:2cycle */
          axiif[cid].mreq     = 0;
          axiif[cid].fmask    = 0xff << ( (dma_ctrl.ZDMA_CH_SRC_DSCR_WORD0/sizeof(Uint)                                               ) & (UNIT_WIDTH*2-1));
          axiif[cid].lmask    = 0xff >> (~(dma_ctrl.ZDMA_CH_SRC_DSCR_WORD0/sizeof(Uint)+dma_ctrl.ZDMA_CH_SRC_DSCR_WORD2/sizeof(Uint)-1) & (UNIT_WIDTH*2-1));
          if (axiif[cid].mlen==0) {
            axiif[cid].fmask &= axiif[cid].lmask;
            axiif[cid].lmask &= axiif[cid].fmask;
          }
          printf("%03.3d:DMA WR src=%08.8x dst=%08.8x len=%04.4x\n", cid,
                 axiif[cid].dadr, axiif[cid].madr, axiif[cid].mlen);
        }
        else { /* lmm->mem */
          axiif[cid].dma_stat = 2; /* read */
          axiif[cid].dadr     = dma_ctrl.ZDMA_CH_DST_DSCR_WORD0 & ~(sizeof(Ull)*UNIT_WIDTH-1); /* mem addr */
          axiif[cid].madr     = dma_ctrl.ZDMA_CH_SRC_DSCR_WORD0 & ~(sizeof(Ull)*UNIT_WIDTH-1); /* lmm addr */
          axiif[cid].mlen     = ((dma_ctrl.ZDMA_CH_SRC_DSCR_WORD0/sizeof(Uint)+dma_ctrl.ZDMA_CH_SRC_DSCR_WORD2/sizeof(Uint)-1)/(UNIT_WIDTH*2))
                              - ((dma_ctrl.ZDMA_CH_SRC_DSCR_WORD0/sizeof(Uint)                                               )/(UNIT_WIDTH*2)); /* 0:1cycle, 1:2cycle */
          axiif[cid].mreq     = 0;
          axiif[cid].fmask    = 0xff << ( (dma_ctrl.ZDMA_CH_SRC_DSCR_WORD0/sizeof(Uint)                                               ) & (UNIT_WIDTH*2-1));
          axiif[cid].lmask    = 0xff >> (~(dma_ctrl.ZDMA_CH_SRC_DSCR_WORD0/sizeof(Uint)+dma_ctrl.ZDMA_CH_SRC_DSCR_WORD2/sizeof(Uint)-1) & (UNIT_WIDTH*2-1));
          if (axiif[cid].mlen==0) {
            axiif[cid].fmask &= axiif[cid].lmask;
            axiif[cid].lmask &= axiif[cid].fmask;
          }
          printf("%03.3d:DMA RD src=%08.8x dst=%08.8x len=%04.4x\n", cid,
                 axiif[cid].madr, axiif[cid].dadr, axiif[cid].mlen);
        }
      }
    }
    else if (!axiif[cid].dma_stat) { /* DMA is inactive */
      if (!axiif[cid].wadr_sent) {
        if (!axiif[cid].axi_awvalid) {
          axiif[cid].axi_awaddr   = c[cid].iorq.ADR & ~(sizeof(Ull)*UNIT_WIDTH-1);
          axiif[cid].axi_awlen    = 0;
          axiif[cid].axi_awvalid  = 1; /* on */
          if (trace)
            printf("%03.3d:IORQ->AXIIF WR opcd=%x adr=%08.8x\n",
                   cid, c[cid].iorq.opcd, (Uint)axiif[cid].axi_awaddr);
        }
        else if (axiif[cid].axi_awready) { /* adr´°Î» */
          axiif[cid].wadr_sent    = 1; /* fin */
          axiif[cid].mreq         = 0; /* length */
          axiif[cid].axi_awvalid  = 0; /* off */
        }
      }
      else if (axiif[cid].mreq <= 0) { /* write active */
        switch (c[cid].iorq.opcd) { /* 2:LD/STRW 3:LD/STR 12:VLD/VSTRQ */
        case 2: /* 32bit */
          axiif[cid].axi_wstrb    = ((c[cid].iorq.ADR&((UNIT_WIDTH-1)*sizeof(Ull)))== 0 ? 0x0000000f<<(c[cid].iorq.ADR&sizeof(int)) : 0x00000000)
                                  | ((c[cid].iorq.ADR&((UNIT_WIDTH-1)*sizeof(Ull)))== 8 ? 0x00000f00<<(c[cid].iorq.ADR&sizeof(int)) : 0x00000000)
                                  | ((c[cid].iorq.ADR&((UNIT_WIDTH-1)*sizeof(Ull)))==16 ? 0x000f0000<<(c[cid].iorq.ADR&sizeof(int)) : 0x00000000)
                                  | ((c[cid].iorq.ADR&((UNIT_WIDTH-1)*sizeof(Ull)))==24 ? 0x0f000000<<(c[cid].iorq.ADR&sizeof(int)) : 0x00000000);
          axiif[cid].axi_wdata[0] = c[cid].iorq.BUF[0]<<(c[cid].iorq.ADR & sizeof(int))*8;
          axiif[cid].axi_wdata[1] = c[cid].iorq.BUF[0]<<(c[cid].iorq.ADR & sizeof(int))*8;
          axiif[cid].axi_wdata[2] = c[cid].iorq.BUF[0]<<(c[cid].iorq.ADR & sizeof(int))*8;
          axiif[cid].axi_wdata[3] = c[cid].iorq.BUF[0]<<(c[cid].iorq.ADR & sizeof(int))*8;
          break;
        case 3: /* 64bit */
          axiif[cid].axi_wstrb    = ((c[cid].iorq.ADR&((UNIT_WIDTH-1)*sizeof(Ull)))== 0 ? 0x000000ff : 0x00000000)
                                  | ((c[cid].iorq.ADR&((UNIT_WIDTH-1)*sizeof(Ull)))== 8 ? 0x0000ff00 : 0x00000000)
                                  | ((c[cid].iorq.ADR&((UNIT_WIDTH-1)*sizeof(Ull)))==16 ? 0x00ff0000 : 0x00000000)
                                  | ((c[cid].iorq.ADR&((UNIT_WIDTH-1)*sizeof(Ull)))==24 ? 0xff000000 : 0x00000000);
          axiif[cid].axi_wdata[0] = c[cid].iorq.BUF[0];
          axiif[cid].axi_wdata[1] = c[cid].iorq.BUF[0];
          axiif[cid].axi_wdata[2] = c[cid].iorq.BUF[0];
          axiif[cid].axi_wdata[3] = c[cid].iorq.BUF[0];
          break;
        case 12: /* 128bit */
          axiif[cid].axi_wstrb    = ((c[cid].iorq.ADR&((UNIT_WIDTH-1)*sizeof(Ull)))== 0 ? 0x0000ffff : 0x00000000)
                                  | ((c[cid].iorq.ADR&((UNIT_WIDTH-1)*sizeof(Ull)))==16 ? 0xffff0000 : 0x00000000);
          axiif[cid].axi_wdata[0] = c[cid].iorq.BUF[0];
          axiif[cid].axi_wdata[1] = c[cid].iorq.BUF[1];
          axiif[cid].axi_wdata[2] = c[cid].iorq.BUF[0];
          axiif[cid].axi_wdata[3] = c[cid].iorq.BUF[1];
          break;
        }
        axiif[cid].axi_wvalid   = 1; /* on */
        if (axiif[cid].mreq == 0)
          axiif[cid].axi_wlast  = 1; /* on */
        else
          axiif[cid].axi_wlast  = 0; /* off */
	printf("%03.3d:PIO WR adr=%08.8x msk=%08.8x data=%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x\n",
	       cid, (Uint)axiif[cid].axi_awaddr, axiif[cid].axi_wstrb,
	       (Uint)(axiif[cid].axi_wdata[3]>>32), (Uint)axiif[cid].axi_wdata[3],
	       (Uint)(axiif[cid].axi_wdata[2]>>32), (Uint)axiif[cid].axi_wdata[2],
	       (Uint)(axiif[cid].axi_wdata[1]>>32), (Uint)axiif[cid].axi_wdata[1],
	       (Uint)(axiif[cid].axi_wdata[0]>>32), (Uint)axiif[cid].axi_wdata[0]);
        if (axiif[cid].axi_wready) { /* data´°Î» */
          axiif[cid].mreq++;
        }
      }
      else {
        c[cid].iorq.v_stat      = 0; /* immediately finished */
        axiif[cid].wadr_sent    = 0; /* reset */
        axiif[cid].axi_wvalid   = 0; /* off */
        axiif[cid].axi_wlast    = 0; /* off */
      }
    }
  }

  if (c[cid].iorq.v_stat == ((3<<2)|1) && c[cid].iorq.type == 3) { /* load *//* emax6_reg()µ¡Ç½¤ÏºÇ½ªÅª¤Ë¤Ï³ÆÃ´Åöstage¤ËÇÛÃÖ */
    /*emax6_reg(c[cid].iorq.tid, c[cid].iorq.type, c[cid].iorq.opcd, c[cid].iorq.ADR, c[cid].iorq.BUF);*/
    if (c[cid].iorq.ADR < REG_BASE2_PHYS) { /* dma space ... ¸ÇÄê°ÌÃÖ (DMA_BASE2_PHYS-REG_BASE2_PHYS) */
    }
    else if (!axiif[cid].dma_stat) { /* DMA is inactive */
      if (!axiif[cid].radr_sent) {
        if (!axiif[cid].axi_arvalid) {
          axiif[cid].axi_araddr   = c[cid].iorq.ADR & ~(sizeof(Ull)*UNIT_WIDTH-1); /* Á´dwordÆÉ¤ß½Ð¤· */
          axiif[cid].axi_arlen    = 0;
          axiif[cid].axi_arvalid  = 1; /* on */
          if (trace)
            printf("%03.3d:IORQ->AXIIF AR opcd=%x adr=%08.8x\n",
                   cid, c[cid].iorq.opcd, (Uint)axiif[cid].axi_araddr);
        }
        else if (axiif[cid].axi_arready) { /* adr´°Î» */
          axiif[cid].radr_sent    = 1;
          axiif[cid].mreq         = 0; /* length */
          axiif[cid].axi_arvalid  = 0; /* off */
        }
      }
    }
  }

  return (0);
}

siml_exring_deq_wait(cid, trace) Uint cid, trace;
{
  int i;

  /* axiifwÂ¦¤Î½àÈ÷ */
  /* ÀèÆ¬¤Îexring.deq_wait¤ò½¸¤á¤Æ¤ª¤­,siml_stage4()¤ÎÁ°¤Ëaxiif[cid].exring_deq_wait¤ò³ÎÄê */
  axiif[cid].exring_deq_wait=0;
  for (i=0; i<LMRING_MUX; i++)
    axiif[cid].exring_deq_wait |= exring[cid].unit[EMAX_DEPTH/LMRING_MUX*i].deq_wait;/* top deq_wait */
}

siml_lmring_axi(cid, trace) Uint cid, trace;
{
  /* LMM¤Îcolumn¥Þ¥ë¥Á¥¹¥ì¥Ã¥Ç¥¤¥ó¥°¤Ï,EXRING¤òÍ¥Àè¤·,LMRING¤Ï·ä´Ö¤ÇÆ°ºî¤µ¤»¤ë */
  /* LMRING¤Ï¼ÂºÝ¤Ë¤ÏÊªÍýrow0¤¬fsm¤ËÀÜÂ³¤µ¤ì¤Æ¤¤¤ë¤¬,siml»þ¤ÏÏÀÍýrow0¤¬ÀèÆ¬¤È¹Í¤¨¤ÆÌäÂê¤Ê¤¤ */

  /*                                                                                                 *//* axi_write_busy axi_read_busy */
  /* ÏÀÍýÈÖ¹æ           ÊªÍýÈÖ¹æ                     V<--------ENQ (siml_axiif¤¬Àè¤ËENQ)             *//* awaddr+awlen+awvalid ^> awready *//* wdata[]+wstrb+wvalid+wlast -> wready->next */
  /*                              axiif.axring_br =======bri_ful2 ÊªÍý#0¤Îpi¤Ïaxring¤Ëº¹¤·ÂØ¤¨       *//* araddr+arlen+arvalid -> arready */
  /* row0+DEPTH-2              0        |            V^--------waiti(unit[0].deq_wait)               */
  /*                             unit[].lmring_br -------  ²¼¤«¤ésiml.deq_wait¤¬Æ±°ì¦Ó¤Ë¥É¥ß¥ÎÅÝ¤·.  */
  /*                                    |                  ¼Âµ¡¤È°ã¤¦¤¬output¤Î½ÐÊý¤ÏÆ±¤¸            */
  /* row0+DEPTH-1              1        |            V        ¢¬                                     */
  /*     simlµ¯ÅÀ(brout¤ÏÄê¿ô)   unit[].lmring_br -------  SIMLµ¯ÅÀ(1¦ÓÁ°¤Î¼¡ÃÊdeq_wait¤ò»È¤¦.Àµ¾ï)  */
  /* row0                     62        |            V     SIMLºÇ¸å(Á°ÃÊbr¤ÎÃÍ¤¬1¦ÓÌ¤Íè¤Ë¤Ê¤ë)       */
  /*                             unit[].lmring_br -------     ¢¬                                     */
  /* row0+1                   63        |            V                                               */
  /*                             unit[].lmring_br -------bro_ful2                                    */
  /*                                                 |^--------waito(axiif.deq_wait)                 */
  /*                                                 +-------->DEQ (siml_axiif¤¬Àè¤ËDEQ)             *//* rdata[]+rvalid+rlast -> rready->next  */

  /* exring.deq_wait  : 1; 0:deq,1:wait            */
  /* axring_ful2      : 2; 0:empty, 2:ful          */
  /* axring_br.rw     : 1; 0:read, 1:write         */
  /* axring_br.ty     : 3; 0:reg/conf, 1:reg/breg, 2:reg/addr, 3:lddmq/tr, 4:lmm, 567:-- */
  /* axring_br.col    : 2; logical col# for target lmm */
  /* axring_br.sq     :16; sequential # for pipelined DMA */
  /* axring_br.a      :31; logical addr reg/lmm    */
  /* axring_br.dm     :32; <- lmm wdata            */
  /* axring_br.d[4]      ; <- lmm wdata/rdata      */

  /* axiif.deq_wait   : 1; 0:deq,1:wait            */
  /* lmring_ful2      : 2; 0:empty, 3:ful          */
  /* lmring_br.rw     : 1; 0:read, 1:write         */
  /* lmring_br.ty     : 3; 0:reg/conf, 1:reg/breg, 2:reg/addr, 3:lddmq/tr, 4:lmm, 567:-- */
  /* lmring_br.col    : 2; logical col# for target lmm */
  /* lmring_br.sq     :16; sequential # for pipelined DMA */
  /* lmring_br.a      :31; logical addr reg/lmm    */
  /* lmring_br.dm     :32; <- lmm wdata            */
  /* lmring_br.d[4]      ; <- lmm wdata/rdata      */

  int    i, k;
  int    bro_ful2, bro_av;
  struct lmring_br *bro[LMRING_MUX]; /* wire */

  for (i=0, bro_ful2=1, bro_av=0; i<LMRING_MUX; i++) {
    bro_ful2 &= (exring[cid].unit[EMAX_DEPTH/LMRING_MUX*(i+1)-1].lmring_ful2 > 0);
    bro[i]    = &exring[cid].unit[EMAX_DEPTH/LMRING_MUX*(i+1)-1].lmring_br[exring[cid].unit[EMAX_DEPTH/LMRING_MUX*(i+1)-1].lmring_b_bot]; /* AXI<-EMAXÂ¦ */
    bro_av   |= bro[i]->av;
  }

  /* read: LMRING½ªÃ¼¤«¤é¤Î²ó¼ý bro->axiif->iorq */
  /*       EMAX:BRO->AXIIF  */
  /************************************************************************************************************************************************************************************************************/
  /*                        |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       | */
  /* clk                   _/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/ */
  /* bro_ful2  0:emp,3:ful 0_/~1~~~~~\                                                                                                                                                                        */
  /* bro.rw    0:rd.1:wr   --<=0=====>-                                                                                                                                                                       */
  /* bro.ty    0:reg,4:lmm --<=*=====>-                                                                                                                                                                       */
  /* bro.col   :  2        reg_ctrl.csel                                                                                                                                                                      */
  /* bro.sq    : 16        --<=0=====>-                                                                                                                                                                       */
  /* bro.av    :  1        --<=0=====>-                                                                                                                                                                       */
  /* bro.a     : 31        --<=A=====>-                                                                                                                                                                       */
  /* bro.dm    : 32        ------------                                                                                                                                                                       */
  /* bro.d[4]  :256        --<=D=====>-                                                                                                                                                                       */
  /* axiif_reqn                                                         _____/~~~~~~~~~~~                                                                                                                     */
  /* axiif_srw                  -----<=1=====>---                       -----<=0=====>---                                                                                                                     */
  /* axiif_sadr                 -----<=A=====>---                       -----<=A=====>---                                                                                                                     */
  /* axiif_sreq                 -----<=0=====>---                       -----<=0=====>---                                                                                                                     */
  /* rdata[]    SLAVE*                                                  -----<=D=====>---                                                                                                                     */
  /* rvalid     SLAVE*                                                  _____/~~~~~~~\___                                                                                                                     */
  /* rlast      SLAVE*                                                  _____/~~~~~~~\___                                                                                                                     */
  /* rready    MASTER                                                   ~~~~~~~~~~~~~~~~~                                                                                                                     */
  /************************************************************************************************************************************************************************************************************/

  /* °Ê²¼,cid<(EMAX_NCHIP-1)¤Î¾ì¹ç,¼¡ÃÊIMAX¤¬Â¸ºß */
  /*¨£¨¡¨¤  ¨£¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¤¨£¨¡¨¤¨£¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¤   */
  /*¨¢  ¨¢  ¨¢        ¨£¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¤      ¨¢¨¢  ¨¢¨¢        ¨£¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¤      ¨¢   */
  /*¨¢¨¡¨©$1¨§¨¡¨¤  $3¨¢$3¡¡            ¡¡  ¨¢  ¨£¨¡¨©¨¢  ¨¢¨§¨¡¨¤  $3¨¢$3¡¡            ¡¡  ¨¢  ¨£¨¡¨©   */
  /*¨¢ I¨§¨¡¨©PD¨§¢ª¨¡¡þ¨¡¨£¨¡¨¡¨¡¨¡¨¡¨¡¨¤¨¤¨¦¢ª¨©PD¨§¨¡¢ª¨¡¨©PD¨§¢ª¨¡¡þ¨¡¨£¨¡¨¡¨¡¨¡¨¡¨¡¨¤¨¤¨¦¢ª¨©PD¨§¢ª */
  /*¨¢ O¨§¨¡¨©OA¨§¨¤  ¨§¨¡¨¢            ¨¢¨©  ¨£¨©OA¨§¨¡¢«¨¡¨©OA¨§¨¤  ¨§¨¡¨¢            ¨¢¨©  ¨£¨©OA¨§¢« */
  /*¨¢¨¡¨©$2¨§¨¡¨¥¨¢  ¨§¨¡¨¢            ¨¢¨©  ¨¢¨¦¨¡¨©¨¢  ¨¢¨§¨¡¨¥¨¢  ¨§¨¡¨¢            ¨¢¨©  ¨¢¨¦¨¡¨©   */
  /*¨¢¡¡¨¢  ¨¢    ¨¢  ¨¦¨¡¨¦¨¡¨¡¨¡¨¡¨¡¨¡¨¥¨©  ¨¢    ¨¢¨¢  ¨¢¨¢    ¨¢  ¨¦¨¡¨¦¨¡¨¡¨¡¨¡¨¡¨¡¨¥¨©  ¨¢    ¨¢   */
  /*¨¢  ¨¢  ¨¢ ¡¡ ¨¢                      ¢­$4¨¢ ¡¡ ¨¢¨¢  ¨¢¨¢ ¡¡ ¨¢                      ¢­$4¨¢ ¡¡ ¨¢   */
  /*¨¢  ¨¢  ¨¢ ¡¡ ¨¦¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¢«¨¡¨¡¢¡¨¡¨¥ ¡¡ ¨¢¨¢  ¨¢¨¢ ¡¡ ¨¦¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¢«¨¡¨¡¢¡¨¡¨¥ ¡¡ ¨¢   */
  /*¨¦¨¡¨¥  ¨¦¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¥¨¦¨¡¨¥¨¦¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¥   */
  /*               |  IMAX[cid]                       |  IMAX[cid+1]                                     */
  /*       rready> | <rvalid <deq_wait bro+rw rready> | <rvalid <deq_wait                                */
  /*          x    |    <0       <1      0x     0>    |     x    ... do  nothing           */
  /*          x    |    <0       <0      11     0>    |     x    ... deq bro               */
  /*          0    |    <0       <1      10     0>    |     x    ... wait for left-rready  */
  /*               |                                  |                                    */
  /*          1    |    <0       <1      10     0>    |     0    ... ¢­¤ØÁ«°Ü              */
  /*          1    |    <0       <1      10     1>    |     0    ... wait for right-rready */
  /*          1    |    <1       <0      10     1>    |     1    ... deq bro               */

  if (bro_ful2 && bro[0]->rw==1) { /* WR¤ÏÌµ¾ò·ï¤Ëdequeue */
    /* write¤Î¾ì¹ç,UNITËè¤ËreqnÉüµ¢.¸åÃÊ¤òÂÔ¤¿¤Ê¤¤ */
    axiif[cid].deq_wait = 0; /* ¾åÎ®axi¤Ë¤è¤ëdeq-OK */
    axiif[cid].reqn--;
    axiif[cid].axi_rvalid = 0; /* ¾åÎ®axi¤ÏreadÉÔ²Ä */
    if (cid < EMAX_NCHIP-1)
      axiif[cid+1].axi_rready = 0; /* ²¼Î®axi¤«¤é¤Î¼õ¼èÉÔ²Ä */
    if (!bro_av)
      printf("%03.3d:BRO WR no unit covers ty=%x adr=%08.8x(%08.8x) (maybe out-of-order/speculative load)\n", cid, bro[0]->ty, bro[0]->a, bro[0]->a-reg_ctrl.i[cid].adtr);
    if (trace)
      printf("%03.3d:BRO->AXIIF WR reqn--=%d ty=%x adr=%08.8x dm=%08.8x\n",
             cid, axiif[cid].reqn, bro[0]->ty, bro[0]->a, bro[0]->dm);
  }
  else if (bro_ful2 && bro[0]->rw==0) { /* lmring¤ËRDÍ×µáÍ­¤ê */
    /* read¤Î¾ì¹ç,¸åÃÊ¤òÂÔ¤Á,·ë²Ìmerge axiif[cid+1].axi_rdata[UNIT_WIDTH] */
    /*                                 axiif[cid+1].axi_rvalid            */
    /*                                 axiif[cid+1].axi_rlast             */
    /*                                 axiif[cid+1].axi_rready            */
    if (!axiif[cid].axi_rready) { /* ¾åÎ®¤¬¶õ¤¤¤Æ¤¤¤Ê¤¤ */
      axiif[cid].deq_wait = 1; /* ¾åÎ®axi¤Ë¤è¤ëdeq-ÉÔ²Ä */
      axiif[cid].axi_rvalid = 0; /* ¾åÎ®axi¤ÏreadÉÔ²Ä */
      if (cid < EMAX_NCHIP-1)
	axiif[cid+1].axi_rready = 0; /* ²¼Î®axi¤«¤é¤Î¼õ¼èÉÔ²Ä */
    }
    else if (cid < reg_ctrl.i[cid].mcid && !axiif[cid+1].axi_rvalid) { /* ²¼Î®¤«¤é²¿¤â¤Ê¤· */
      printf("%03.3d:BRO waiting for next IMAX[%d]\n", cid, cid+1);
      axiif[cid].deq_wait = 1; /* ¾åÎ®axi¤Ë¤è¤ëdeq-ÉÔ²Ä */
      axiif[cid].axi_rvalid = 0; /* ¾åÎ®axi¤ÏreadÉÔ²Ä */
      axiif[cid+1].axi_rready = 1; /* ¼õ¿®²ÄÇ½¤ËÊÑ¹¹ */
    }
    else { /* ²¼Î®¤«¤é¼õ¼è */
      for (k=0; k<UNIT_WIDTH; k++) {
	if (cid < reg_ctrl.i[cid].mcid)
	  axiif[cid].axi_rdata[k] = axiif[cid+1].axi_rdata[k];
	else
	  axiif[cid].axi_rdata[k] = 0LL;
	for (i=0; i<LMRING_MUX; i++) {
	  if (bro[i]->av)
	    axiif[cid].axi_rdata[k] |= bro[i]->d[k];
	}
      }
      axiif[cid].deq_wait = 0; /* ¾åÎ®axi¤Ë¤è¤ëdeq-OK */
      axiif[cid].reqn--;
      axiif[cid].axi_rvalid = 1; /* ¾åÎ®axi¤Ïread-OK */
      if (cid < EMAX_NCHIP-1)
	axiif[cid+1].axi_rready = 1;
      if (!bro_av)
	printf("%03.3d:BRO RD no unit covers ty=%x adr=%08.8x(%08.8x) (maybe out-of-order/speculative load)\n", cid, bro[0]->ty, bro[0]->a, bro[0]->a-reg_ctrl.i[cid].adtr);
      else
	printf("%03.3d:AXI RD adr=%08.8x data=%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x\n",
	       cid, axiif[cid].sadr,
	       (Uint)(axiif[cid].axi_rdata[3]>>32), (Uint)axiif[cid].axi_rdata[3],
	       (Uint)(axiif[cid].axi_rdata[2]>>32), (Uint)axiif[cid].axi_rdata[2],
	       (Uint)(axiif[cid].axi_rdata[1]>>32), (Uint)axiif[cid].axi_rdata[1],
	       (Uint)(axiif[cid].axi_rdata[0]>>32), (Uint)axiif[cid].axi_rdata[0]);
    }
  }
  else if (axiif[cid].creg) { /* RD control regs */
    /* CREG¤Ï¸åÃÊIMAX¤ËÁ÷¿®¤·¤Ê¤¤¡¥ÀèÆ¬¤Î¤ß±þÅú */
    for (k=0; k<UNIT_WIDTH; k++)
      axiif[cid].axi_rdata[k] = *((Ull*)((Uchar*)&reg_ctrl.i[cid]+(axiif[cid].sadr-REG_BASE2_PHYS))+k);
    axiif[cid].axi_rvalid = 1; /* ¾åÎ®axi¤Ïread-OK */
    axiif[cid].creg = 0; /* reset RD control regs */
    printf("%03.3d:PIO RD adr=%08.8x data=%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x\n",
           cid, axiif[cid].sadr,
           (Uint)(axiif[cid].axi_rdata[3]>>32), (Uint)axiif[cid].axi_rdata[3],
           (Uint)(axiif[cid].axi_rdata[2]>>32), (Uint)axiif[cid].axi_rdata[2],
           (Uint)(axiif[cid].axi_rdata[1]>>32), (Uint)axiif[cid].axi_rdata[1],
           (Uint)(axiif[cid].axi_rdata[0]>>32), (Uint)axiif[cid].axi_rdata[0]);
  }
  else {
    axiif[cid].deq_wait = 1; /* ¾åÎ®axi¤Ë¤è¤ëdeq-ÉÔ²Ä */
    axiif[cid].axi_rvalid = 0; /* ¾åÎ®axi¤ÏreadÉÔ²Ä */
    if (cid < EMAX_NCHIP-1)
      axiif[cid+1].axi_rready = 0; /* ²¼Î®axi¤«¤é¤Î¼õ¼èÉÔ²Ä */
  }

  return (0);
}

siml_axi_lmring(cid, trace) Uint cid, trace;
{
  int    i, k, mask;
  int    bri_ful2 = axiif[cid].axring_ful2;
  struct axring_br *bri = &axiif[cid].axring_br[axiif[cid].axring_b_top]; /* AXI->EMAXÂ¦ */

  /* read/write LMRINGÀèÃ¼¤Ø¤ÎÅêÆþ   iorq->axiif->bri */
  /*       EMAX:AXIIF->BRI  */
  /************************************************************************************************************************************************************************************************************/
  /*                        |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       | */
  /* clk                   _/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/ */
  /* awaddr    MASTER   -----<=A=====>---                                                                                                                                                                     */
  /* awlen     MASTER   -----<=0=====>---                                                                                                                                                                     */
  /* awvalid   MASTER   _____/~~~~~~~\___ valid=1                                                                                                                                                             */
  /* awready    SLAVE*  ~~~~~~~~~~~~~\___ ready=1¤Î»þ¤Ë¼ø¼õ                                                                                                                                                   */
  /* wstrb     MASTER   -----<=M=====>---                                                                                                                                                                     */
  /* wdata[]   MASTER   -----<=D=====>---                                                                                                                                                                     */
  /* wvalid    MASTER   _____/~~~~~~~\___                                                                                                                                                                     */
  /* wlast     MASTER   _____/~~~~~~~\___ PIO¤Î¾ì¹ç,¾ï¤Ë1                                                                                                                                                     */
  /* wready     SLAVE*  ~~~~~~~~~~~^~\___                                                                                                                                                                     */
  /* araddr    MASTER                A                          -----<=A=====>---                                                                                                                             */
  /* arlen     MASTER                |                          -----<=0=====>---                                                                                                                             */
  /* arvalid   MASTER                |                          _____/~~~~~~~\___                                                                                                                             */
  /* arready    SLAVE*               |                          ~~~~~~~~~~~~~\___                                                                                                                             */
  /* axiif_busy           *                                             _____/~~~~~~~~~~~                                                                                                                     */
  /* axiif_srw            *     -----<=1=====>---                       -----<=0=====>---                                                                                                                     */
  /* axiif_sadr           *     -----<=A=====>---                       -----<=A=====>---                                                                                                                     */
  /* axiif_sreq           *     -----<=0=====>---                       -----<=0=====>---                                                                                                                     */
  /* bri_ful2  0:emp,2:ful      __1__/~2~ update axring_b_top           __1__/~2~ update axring_b_top                                                                                                         */
  /* bri.rw    0:rd.1:wr  *     -----<=1=====>---                       -----<=0=====>---                                                                                                                     */
  /* bri.ty    0:reg,4:lmm*     -----<=*=====>---                       -----<=*=====>---                                                                                                                     */
  /* bri.col   :  2       *     reg_ctrl.csel                           reg_ctrl.csel                                                                                                                         */
  /* bri.sq    : 16       *     -----<=0=====>---                       -----<=0=====>---                                                                                                                     */
  /* bri.av    :  1       *     sadr <=0=====>---                       sadr <=0=====>---                                                                                                                     */
  /* bri.a     : 31       *     sadr <=A=====>---                       sadr <=A=====>---                                                                                                                     */
  /* bri.dm    : 32       *     wstrb<=M=====>---                       -----------------                                                                                                                     */
  /* bri.d[4]  :256       *     wdata<=0=====>---                       -----------------                                                                                                                     */
  /* deq_waiti 0:dq,1:wait      update axring_b_bot                     update axring_b_bot                                                                                                                   */
  /************************************************************************************************************************************************************************************************************/

  /* °Ê²¼,cid<(EMAX_NCHIP-1)¤Î¾ì¹ç,¼¡ÃÊIMAX¤¬Â¸ºß */
  /*¨£¨¡¨¤  ¨£¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¤¨£¨¡¨¤¨£¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¤   */
  /*¨¢  ¨¢  ¨¢        ¨£¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¤      ¨¢¨¢  ¨¢¨¢        ¨£¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¤      ¨¢   */
  /*¨¢¨¡¨©$1¨§¨¡¨¤  $3¨¢$3¡¡            ¡¡  ¨¢  ¨£¨¡¨©¨¢  ¨¢¨§¨¡¨¤  $3¨¢$3¡¡            ¡¡  ¨¢  ¨£¨¡¨©   */
  /*¨¢ I¨§¨¡¨©PD¨§¢ª¨¡¢¡¨¡¨£¨¡¨¡¨¡¨¡¨¡¨¡¨¤¨¤¨¦¢ª¨©PD¨§¨¡¢ª¨¡¨©PD¨§¢ª¨¡¢¡¨¡¨£¨¡¨¡¨¡¨¡¨¡¨¡¨¤¨¤¨¦¢ª¨©PD¨§¢ª */
  /*¨¢ O¨§¨¡¨©OA¨§¨¤  ¨§¨¡¨¢            ¨¢¨©  ¨£¨©OA¨§¨¡¢«¨¡¨©OA¨§¨¤  ¨§¨¡¨¢            ¨¢¨©  ¨£¨©OA¨§¢« */
  /*¨¢¨¡¨©$2¨§¨¡¨¥¨¢  ¨§¨¡¨¢            ¨¢¨©  ¨¢¨¦¨¡¨©¨¢  ¨¢¨§¨¡¨¥¨¢  ¨§¨¡¨¢            ¨¢¨©  ¨¢¨¦¨¡¨©   */
  /*¨¢¡¡¨¢  ¨¢    ¨¢  ¨¦¨¡¨¦¨¡¨¡¨¡¨¡¨¡¨¡¨¥¨©  ¨¢    ¨¢¨¢  ¨¢¨¢    ¨¢  ¨¦¨¡¨¦¨¡¨¡¨¡¨¡¨¡¨¡¨¥¨©  ¨¢    ¨¢   */
  /*¨¢  ¨¢  ¨¢ ¡¡ ¨¢                      ¢­$4¨¢ ¡¡ ¨¢¨¢  ¨¢¨¢ ¡¡ ¨¢                      ¢­$4¨¢ ¡¡ ¨¢   */
  /*¨¢  ¨¢  ¨¢ ¡¡ ¨¦¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¢«¨¡¨¡¡þ¨¡¨¥ ¡¡ ¨¢¨¢  ¨¢¨¢ ¡¡ ¨¦¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¢«¨¡¨¡¡þ¨¡¨¥ ¡¡ ¨¢   */
  /*¨¦¨¡¨¥  ¨¦¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¥¨¦¨¡¨¥¨¦¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¡¨¥   */
  /* Á°ÃÊ¤Ø¤Îarready/awready¤Ï¡¤¼«axring¤Ë¶õ¤­+¸åÃÊ¤¬arready/awready¤Î¾ì¹ç¤Ë1                            */
  axiif[cid].axi_arready = (bri_ful2 < AXRING_BR_BUF && !axiif[cid].radr_recv && (cid<EMAX_NCHIP-1?axiif[cid+1].axi_arready:1));
  axiif[cid].axi_awready = (bri_ful2 < AXRING_BR_BUF && !axiif[cid].wadr_recv && (cid<EMAX_NCHIP-1?axiif[cid+1].axi_awready:1));
  axiif[cid].axi_wready  = (bri_ful2 < AXRING_BR_BUF                          && (cid<EMAX_NCHIP-1?axiif[cid+1].axi_wready :1));

  if (axiif[cid].axi_arvalid && axiif[cid].axi_arready) { /* new read_req starts */
    axiif[cid].radr_recv = 1; /* fin */
    axiif[cid].srw  = 0; /* read */
    axiif[cid].sadr = axiif[cid].axi_araddr;
    axiif[cid].slen = axiif[cid].axi_arlen;
    axiif[cid].sreq = 0;
    if (cid < reg_ctrl.i[cid].mcid) {
      axiif[cid+1].axi_araddr   = axiif[cid].axi_araddr;
      axiif[cid+1].axi_arlen    = axiif[cid].axi_arlen;
      axiif[cid+1].axi_arvalid  = 1; /* on */
    }
  }
  else if (axiif[cid].radr_recv) {
    Uint a;
    if (cid < EMAX_NCHIP-1 && axiif[cid+1].axi_arvalid && axiif[cid+1].axi_arready)
      axiif[cid+1].axi_arvalid = 0; /* off */
    a = axiif[cid].sadr;
    if (a < REG_BASE2_PHYS+REG_CONF_OFFS) { /* control space ... ¸ÇÄê°ÌÃÖ *//* ËÜÍè¤Ïbri_ful2¤Î±Æ¶Á¤ò¼õ¤±¤Ê¤¤¤¬,Åý°ì´ÉÍý¤Î¤¿¤á¤Ëbri_ful2¤ò»ÈÍÑ */
      axiif[cid].radr_recv = 0; /* reset */
      axiif[cid].creg = 1; /* set RD control regs */
      if (trace)
        printf("%03.3d:AXIIF->REG AR adr=%08.8x\n", cid, a);
    }
    else if (axiif[cid].sreq <= axiif[cid].slen) { /* (burst 256bit_LMM -> 256bit_AXI_read (256bit*256count = 8KB)) */
      if (bri_ful2 < AXRING_BR_BUF) {
	if (axiif[cid].sreq == axiif[cid].slen)
	  axiif[cid].radr_recv = 0; /* reset */
	bri->rw   = 0; /* read */
	bri->ty   = ( a              >=LMM_BASE2_PHYS) ? 4 : /* lmm  */
                    ((a&REG_AREA_MASK)>=REG_LDDM_OFFS) ? 3 : /* lddm */
                    ((a&REG_AREA_MASK)>=REG_ADDR_OFFS) ? 2 : /* addr */
                    ((a&REG_AREA_MASK)>=REG_BREG_OFFS) ? 1 : /* breg */
                                                       0 ; /* conf */
	bri->col  = reg_ctrl.i[cid].csel; /* logical col# for target lmm */
	bri->sq   = axiif[cid].sreq; /* from axiif[cid].axi_awlen to 0 */
	bri->av   = 0; /* initial */
	bri->a    = a + ((bri->ty==4)?reg_ctrl.i[cid].adtr:0) + axiif[cid].sreq*sizeof(Ull)*UNIT_WIDTH;
	bri->d[0] = 0;
	bri->d[1] = 0;
	bri->d[2] = 0;
	bri->d[3] = 0;
	axiif[cid].reqn++;
	axiif[cid].sreq++;
	axiif[cid].axring_ful2++;
	axiif[cid].axring_b_top = (axiif[cid].axring_b_top + 1)%AXRING_BR_BUF;
	if (trace)
	  printf("%03.3d:AXIIF->BRI AR reqn++=%d axring_ful2=%x adr=%08.8x\n", cid, axiif[cid].reqn, axiif[cid].axring_ful2, bri->a);
      }
    }
  }
  else if (axiif[cid].axi_awvalid && axiif[cid].axi_awready) {
    axiif[cid].wadr_recv = 1; /* fin */
    axiif[cid].srw  = 1; /* write */
    axiif[cid].sadr = axiif[cid].axi_awaddr;
    axiif[cid].slen = axiif[cid].axi_awlen;
    axiif[cid].sreq = 0;
    if ((axiif[cid].axi_awaddr < REG_BASE2_PHYS+REG_CONF_OFFS || cid < reg_ctrl.i[cid].mcid) && cid < EMAX_NCHIP-1) {
      axiif[cid+1].axi_awaddr   = axiif[cid].axi_awaddr;
      axiif[cid+1].axi_awlen    = axiif[cid].axi_awlen;
      axiif[cid+1].axi_awvalid  = 1; /* on */
    }
  }
  else if (axiif[cid].wadr_recv) {
    Uint a;
    if (cid < EMAX_NCHIP-1 && axiif[cid+1].axi_awvalid && axiif[cid+1].axi_awready)
      axiif[cid+1].axi_awvalid = 0; /* off */
    a = axiif[cid].sadr;
    if (a < REG_BASE2_PHYS+REG_CONF_OFFS) { /* control space ... ¸ÇÄê°ÌÃÖ *//* ËÜÍè¤Ïbri_ful2¤Î±Æ¶Á¤ò¼õ¤±¤Ê¤¤¤¬,Åý°ì´ÉÍý¤Î¤¿¤á¤Ëbri_ful2¤ò»ÈÍÑ */
      if (axiif[cid].axi_wvalid && axiif[cid].axi_wready) {
        axiif[cid].wadr_recv = 0; /* reset */
        if      (axiif[cid].axi_wstrb & 0x000000ff) { k=0; mask=axiif[cid].axi_wstrb     & 0xff; }
        else if (axiif[cid].axi_wstrb & 0x0000ff00) { k=1; mask=axiif[cid].axi_wstrb>> 8 & 0xff; }
        else if (axiif[cid].axi_wstrb & 0x00ff0000) { k=2; mask=axiif[cid].axi_wstrb>>16 & 0xff; }
        else if (axiif[cid].axi_wstrb & 0xff000000) { k=3; mask=axiif[cid].axi_wstrb>>24 & 0xff; }
        if (mask & 0x0f)
          *((Uint*)((Ull*)((Uchar*)&reg_ctrl.i[cid]+(a-REG_BASE2_PHYS))+k)  ) = axiif[cid].axi_wdata[k];
        if (mask & 0xf0)
          *((Uint*)((Ull*)((Uchar*)&reg_ctrl.i[cid]+(a-REG_BASE2_PHYS))+k)+1) = axiif[cid].axi_wdata[k]>>32;
        if (trace)
          printf("%03.3d:AXIIF->REG WR adr=%08.8x wstrb=%08.8x data=%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x\n",
                 cid, a, axiif[cid].axi_wstrb,
                 (Uint)(axiif[cid].axi_wdata[3]>>32), (Uint)axiif[cid].axi_wdata[3],
                 (Uint)(axiif[cid].axi_wdata[2]>>32), (Uint)axiif[cid].axi_wdata[2],
                 (Uint)(axiif[cid].axi_wdata[1]>>32), (Uint)axiif[cid].axi_wdata[1],
                 (Uint)(axiif[cid].axi_wdata[0]>>32), (Uint)axiif[cid].axi_wdata[0]);
        if (cid < EMAX_NCHIP-1) {
          axiif[cid+1].axi_wstrb    = axiif[cid].axi_wstrb; /* set cid=0,1,2,3 */;
          axiif[cid+1].axi_wdata[0] = axiif[cid].axi_wdata[0];
          axiif[cid+1].axi_wdata[1] = axiif[cid].axi_wdata[1];
          axiif[cid+1].axi_wdata[2] = axiif[cid].axi_wdata[2] + (a==REG_BASE2_PHYS?0x000010000LL:0x000000000LL); /* set cid=0,1,2,3 */
          axiif[cid+1].axi_wdata[3] = axiif[cid].axi_wdata[3];
          axiif[cid+1].axi_wvalid   = 1; /* on */
          axiif[cid+1].axi_wlast    = 1; /* on */
        }
        switch (reg_ctrl.i[cid].cmd&3) {
        case CMD_RESET:
	  if (!exring[cid].cmd_busy) {
	    printf("%03.3d:ES %08.8x_%08.8x cycle=%08.8x_%08.8x ---EMAX6 CMD(%x) START---\n", cid,
		   (Uint)(t[cid].total_steps>>32), (Uint)t[cid].total_steps,
		   (Uint)(t[cid].total_cycle>>32), (Uint)t[cid].total_cycle, reg_ctrl.i[cid].cmd&3);
	    exring[cid].cmd_busy = 1;
          }
          break;
        case CMD_SCON:  /* scon */
        case CMD_EXEC:  /* exec */
	  if (cid <= reg_ctrl.i[cid].mcid) {
	    if (!exring[cid].cmd_busy) {
	      printf("%03.3d:ES %08.8x_%08.8x cycle=%08.8x_%08.8x ---EMAX6 CMD(%x) START---\n", cid,
		     (Uint)(t[cid].total_steps>>32), (Uint)t[cid].total_steps,
		     (Uint)(t[cid].total_cycle>>32), (Uint)t[cid].total_cycle, reg_ctrl.i[cid].cmd&3);
	      exring[cid].cmd_busy = 1;
	    }
          }
          break;
        }
      }
      else {
        if (cid < EMAX_NCHIP-1)
          axiif[cid+1].axi_wvalid   = 0; /* off */
      }
    }
    else if (axiif[cid].sreq <= axiif[cid].slen) { /* 0-3:conf/breg/addr/lddm, 4:lmm (burst 256bit_AXI_write -> 256bit_LMM (256bit*256count = 8KB)) */
      if (axiif[cid].axi_wvalid && axiif[cid].axi_wready) {
        if (axiif[cid].sreq == axiif[cid].slen)
          axiif[cid].wadr_recv = 0; /* reset */
        bri->rw   = 1; /* write */
        bri->ty   = ( a              >=LMM_BASE2_PHYS) ? 4 : /* lmm  */
                    ((a&REG_AREA_MASK)>=REG_LDDM_OFFS) ? 3 : /* lddm */
                    ((a&REG_AREA_MASK)>=REG_ADDR_OFFS) ? 2 : /* addr */
                    ((a&REG_AREA_MASK)>=REG_BREG_OFFS) ? 1 : /* breg */
                                                         0 ; /* conf */
        bri->col  = reg_ctrl.i[cid].csel;   /* logical col# for target lmm */
        bri->sq   = axiif[cid].sreq; /* from 0 to axiif[cid].axi_awlen */
        bri->av   = 0; /* initial */
        bri->a    = a + ((bri->ty==4)?reg_ctrl.i[cid].adtr:0) + axiif[cid].sreq*sizeof(Ull)*UNIT_WIDTH;
        bri->dm   = axiif[cid].axi_wstrb;
        bri->d[0] = axiif[cid].axi_wdata[0];
        bri->d[1] = axiif[cid].axi_wdata[1];
        bri->d[2] = axiif[cid].axi_wdata[2];
        bri->d[3] = axiif[cid].axi_wdata[3];
        axiif[cid].reqn++;
        axiif[cid].sreq++;
        axiif[cid].axring_ful2++;
        axiif[cid].axring_b_top = (axiif[cid].axring_b_top + 1)%AXRING_BR_BUF;
        if (trace)
          printf("%03.3d:AXIIF->BRI WR reqn++=%x axring_ful2=%x adr=%08.8x dm=%08.8x data=%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x\n",
                 cid, axiif[cid].reqn, axiif[cid].axring_ful2, bri->a, bri->dm,
                 (Uint)(bri->d[3]>>32), (Uint)bri->d[3],
                 (Uint)(bri->d[2]>>32), (Uint)bri->d[2],
                 (Uint)(bri->d[1]>>32), (Uint)bri->d[1],
                 (Uint)(bri->d[0]>>32), (Uint)bri->d[0]);
	if (cid < reg_ctrl.i[cid].mcid) {
          axiif[cid+1].axi_wstrb    = axiif[cid].axi_wstrb;
          axiif[cid+1].axi_wdata[0] = axiif[cid].axi_wdata[0];
          axiif[cid+1].axi_wdata[1] = axiif[cid].axi_wdata[1];
          axiif[cid+1].axi_wdata[2] = axiif[cid].axi_wdata[2];
          axiif[cid+1].axi_wdata[3] = axiif[cid].axi_wdata[3];
          axiif[cid+1].axi_wvalid   = 1; /* on */
          if (axiif[cid].sreq == axiif[cid].slen)
            axiif[cid+1].axi_wlast  = 1; /* on */
          else
            axiif[cid+1].axi_wlast  = 0; /* off */
        }
      }
      else {
        if (cid < EMAX_NCHIP-1)
          axiif[cid+1].axi_wvalid   = 0; /* off */
      }
    }
  }
  else {
    if (cid < EMAX_NCHIP-1)
      axiif[cid+1].axi_wvalid   = 0; /* off */
    if ((reg_ctrl.i[cid].cmd&3) == CMD_RESET) {
      axiif[cid].wadr_recv      = 0;
      axiif[cid].radr_recv      = 0;
      axiif[cid].reqn           = 0;
      axiif[cid].creg           = 0;
      axiif[cid].srw            = 0;
      axiif[cid].sadr           = 0;
      axiif[cid].slen           = 0;
      axiif[cid].sreq           = 0;
      axiif[cid].axring_ful2    = 0;
      axiif[cid].axring_b_top   = 0;
      axiif[cid].axring_b_bot   = 0;
      axiif[cid].exring_deq_wait= 0;
    }
    if ((exring[cid].cycle&3) == 3) {
      reg_ctrl.i[cid].cmd = (reg_ctrl.i[cid].cmd&~3) | CMD_NOP;
      exring[cid].cmd_busy = 0;
    }
  }

  if (bri_ful2 && !axiif[cid].exring_deq_wait) { /* dequeued for next cycle */
    axiif[cid].axring_ful2--;
    axiif[cid].axring_b_bot = (axiif[cid].axring_b_bot + 1)%AXRING_BR_BUF;
  }

  if ((exring[cid].cycle&3) == 3 && (reg_ctrl.i[cid].cmd&3) == CMD_RESET)
    exring[cid].cycle = 0;
  else
    exring[cid].cycle++;

  return (0);
}

siml_unit_stage1(Uint cid, Uint i) /* stage-1 (BRIN->EX/TX) */
{
  /******************************************************************************************************************************************************************************************/
  /*      |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       | */
  /* clk _/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/ */
  /* cycle  7       0       1       2       3       4       5       6       7       0       1       2       3       4       5       6       7       0       1       2       3       4       */
  /* reg_ctrl.cmd -----------------<==EXEC=========X==NOP>================================================================================================================================= */
  /*              V-update                        V-update                        V-update                        V-update                        V-update                        V-update  */
  /* cmd         ----------------------------------<==EXEC=========================================================================================================================>------- */
  /* one_shot    __________________________________________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______ */
  /* unit1_exec  __________________________________/~~(l_row==0)~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________________________________ */
  /* unit1_stop  __________________________________________________________________________________________________________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______ */
  /* reg_ctrl.stat---------------------------------<==EXRING_BUSY==================================================================================>--------------------------------------- */

  /* reg_ctrl.cmd -----------------<==SCON=========X==NOP>================================================================================================================================= */
  /*              V-update                        V-update                        V-update                        V-update                        V-update                        V-update  */
  /* cmd         ----------------------------------<==SCON=========================================================================================>--------------------------------------- */
  /* scon_count  ----------------------------------<==conf.mapdist*2===============X==conf.mapdist*2-1=============X==0============================>======================================= */
  /*                                                      Ascon2  Ascon2  Ascon2  Ascon2  Ascon1  Ascon1  Ascon1  Ascon1                                                                    */
  /*                                                      ->br0   ->br1   ->br2   ->br3   br0->   br1->   br2->   br3->                                                                     */
  /* unit1_exec  __________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________________________________________________________________ */
  /* unit1_stop  __________________________________________________________________________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________________________________ */
  /* stage1out** --------------------------------------------------------------------------<== 0===X== 1===X== 2===X== 3===>--------------------------------------------------------------- */
  /* stage4out** ------------------------------------------<== 0===X== 1===X== 2===X== 3===>----------------------------------------------------------------------------------------------- */
  /* reg_ctrl.stat---------------------------------<==EXRING_BUSY==================================================>----------------------------------------------------------------------- */

  /******************************************************************************************************************************************************************************************/
  /*      |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       | */
  /* clk _/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/ */
  /* cycle  7       0       1       2       3       4       5       6       7       0       1       2       3       4       5       6       7       0       1       2       3       4       */
  /*              V-update                        V-update                        V-update                        V-update                        V-update                        V-update  */
  /* brout_valid __/~~(l_row==row0-1)~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

  /* ---- row[i]--------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */
  /*              V-update                        V-update                        V-update                        V-update                        V-update                        V-update  */
  /* cmd         --<==EXEC=========================================================================================================================================================>------- */
  /* one_shot    __________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______ */
  /* unit1_exec  __/~~(l_row==0)~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________________________________ */
  /* stage_forstat.0(stage2)_______________________________________________________________________________________________/L0=0~~~~~~~~~~~~~~~~~~~\_______________________________________ */
  /* stage_forstat.1(stage2)_______________________________________________________________________________________________________/L1=0~~~~~~~~~~~\_______________________________________ */
  /* unit1_forstat(stage2)_________________________________________________________________________________________________________________________/L1=0~~~\_______________________________ */
  /* unit1_arbrk(stage2)___________________________________________________________________________________________________________/~forstat=3~~~~~\_______________________________________ */
  /*                                                                                                                                           A³ÎÄê                                        */
  /* st2ex       __________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________________________ */
  /* st3ex       __________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________________ */
  /* st4ex       __________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________ */
  /* unit1_stop  __________________________________________________________________________________________________________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______ */
  /* stage1out** ----------<== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3==>-------------------------------- */
  /* stage2out   ----------<-------<== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3==>------------------------ */
  /* stage3out   ----------<---------------<== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3==>---------------- */
  /* stage4out   ----tx4dr-<-----------------------<== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3==>-------- */
  /* tr_valid    __________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______ */
  /* stage5out   -------brout------------------------------<==p0===X==p1===X==p2===X==p3===X==q0===X==q1===X==q2===X==q3===X==q0===X==q1===X==q2===X==q3===X==q0===X==q1===X==q2===X==q3==> */
  /*              V-update                        V-update                        V-update                        V-update                        V-update                        V-update  */
  /* brout_valid __________________________________________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

  /* ---- row[i+1]------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */
  /*              V-update                        V-update                        V-update                        V-update                        V-update                        V-update  */
  /* cmd         --<==EXEC=========================================================================================================================================================>------- */
  /* one_shot    __________________________________________________________________________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
  /* unit1_exec  __________________________________________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
  /* stage1out   --------------------------------------------------------------------------<== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3==> */

  /***folding****************************************************************************************************************/
  /*      |       |       |       |       |       |       |       |       |       |       |       |       |       |       | */
  /* clk _/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/ */
  /* cycle  7       0       1       2       3       4       5       6       7       0       1       2       3       4       */
  /*              V-update                        V-update                        V-update                        V-update  */
  /* brout_valid __/~~(l_row==row0-1)~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

  /* ---- row[i]----------------------------------------------------------------------------------------------------------- */
  /*              V-update                        V-update                        V-update                        V-update  */
  /* cmd         --<==EXEC=========================================================================================>------- */
  /* one_shot    __________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________________________________ */
  /* unit1_exec  __/~~(l_row==0)~~~~~~~~~~~~~~~~~~~\_______________________________________________________________________ */
  /* stage_forstat.0(stage2)/L0=0~~~~~~~~~~~~~~~~~~\_______________________________________________________________________ */
  /* stage_forstat.1(stage2)_______/L1=0~~~~~~~~~~~\_______________________________________________________________________ */
  /* unit1_forstat(stage2)_________________________/L1=0~~~\_______________________________________________________________ */
  /* unit1_arbrk(stage2)___________/~forstat=3~~~~~\_______________________________________________________________________ */
  /*                                           A³ÎÄê                                                                        */
  /* one_shot2   __________________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________________________ */
  /* st2ex       __________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________________________________________________________ */
  /* st3ex       __________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________________________________________________ */
  /* st4ex       __________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________________________________________ */
  /* unit1_stop  __________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________________________________ */
  /* stage1out** ----------<== 0===X== 1===X== 2===X== 3==>---------------------------------------------------------------- */
  /* stage2out   ----------<-------<== 0===X== 1===X== 2===X== 3==>-------------------------------------------------------- */
  /* stage3out   ----------<---------------<== 0===X== 1===X== 2===X== 3==>------------------------------------------------ */
  /* stage4out   ----tx4dr-<-----------------------<== 0===X== 1===X== 2===X== 3==>---------------------------------------- */
  /* tr_valid    __________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______ */
  /* stage5out   -------brout------------------------------<==p0===X==p1===X==p2===X==p3===X==q0===X==q1===X==q2===X==q3==> */
  /*              V-update                        V-update                        V-update                        V-update  */
  /* brout_valid __________________________________________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

  /* ---- row[i+1]--------------------------------------------------------------------------------------------------------- */
  /*              V-update                        V-update                        V-update                        V-update  */
  /* cmd         --<==EXEC=========================================================================================>------- */
  /* one_shot    __________________________________________________________________________________________________/~~~~~~~ */
  /* unit1_exec  __________________________________________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
  /* stage1out   --------------------------------------------------------------------------<== 0===X== 1===X== 2===X== 3==> */

  int pi = (i+EMAX_DEPTH-1)%EMAX_DEPTH;
  int xi; /* EXE-in: select i/pi by folding or not */
  int bi; /* EAB-in: select i/pi by folding or not */
  int oi; /* EAO-in: select i/pi by folding or not */
  int b  = (exring[cid].unit[i].cycle / EMAX_WIDTH)&1; /* 0,0,0,0,1,1,1,1 */
  int j  =  exring[cid].unit[i].cycle % EMAX_WIDTH;    /* 0,1,2,3,0,1,2,3 */
  int k;   /* tmp, and for STRQ/SFMA+STBR(lmea0strqcol) */
  int ofs; /*          for STRQ/SFMA+STBR(lmea0strqcol) */
  int s, sb, so;
  int unit1_exec_next, unit1_fold_next, unit1_forstat_next, unit1_forstat_fold_next, unit1_stop_next, unit2_exec_next, unit2_fold_next, unit2_forstat_next, unit2_stop_next;

  switch (exring[cid].unit[i].cmd) {
  case CMD_NOP:   /* nop */
    if ((exring[cid].unit[i].cycle&3) == 3) {
      switch (reg_ctrl.i[cid].cmd&3) {
      case CMD_RESET: /* reset */
        exring[cid].unit[i].l_row         = i; /* reset to default */
        exring[cid].unit[i].scon_count    = 0; /* reset to default */
        exring[cid].unit[i].one_shot      = 0; /* reset to default */
        exring[cid].unit[i].one_shot2     = 0; /* reset to default */
        exring[cid].unit[i].one_shot_fold = 0; /* reset to default */
        exring[cid].unit[i].one_shot_fold2= 0; /* reset to default */
        exring[cid].unit[i].unit1_exec    = 0; /* reset to default */
        exring[cid].unit[i].unit1_fold    = 0; /* reset to default */
        exring[cid].unit[i].unit1_forstat = 0; /* reset to default */
        exring[cid].unit[i].unit1_forstat2 = 0;/* reset to default */
        exring[cid].unit[i].unit1_forstat_fold  = 0;/* reset to default */
        exring[cid].unit[i].unit1_forstat_fold2 = 0;/* reset to default */
        exring[cid].unit[i].unit1_stop    = 0; /* reset to default */
        exring[cid].unit[i].unit2_exec    = 0; /* reset to default */
        exring[cid].unit[i].unit2_fold    = 0; /* reset to default */
        exring[cid].unit[i].unit2_forstat = 0; /* reset to default */
        exring[cid].unit[i].unit2_stop    = 0; /* reset to default */
        exring[cid].unit[i].stage2_exec   = 0; /* reset to default */
        exring[cid].unit[i].stage2_fold   = 0; /* reset to default */
        for (k=0; k<EMAX_WIDTH; k++)
          exring[cid].unit[i].cx[k]       = 0;
        exring[cid].unit[i].ex1           = 0LL;
        exring[cid].unit[i].ex2           = 0LL;
        exring[cid].unit[i].ex3           = 0LL;
        exring[cid].unit[i].ea0b          = 0;
        exring[cid].unit[i].ea0o          = 0LL;
        exring[cid].unit[i].ea1b          = 0;
        exring[cid].unit[i].ea1o          = 0LL;
        for (k=0; k<EMAX_WIDTH; k++)
          exring[cid].unit[i].tx[k]       = 0LL;
        break;
      case CMD_SCON:  /* scon */
	if (cid <= reg_ctrl.i[cid].mcid) {
	  exring[cid].unit[i].cmd = CMD_SCON;
	  exring[cid].unit[i].scon_count = reg_ctrl.i[cid].conf[i][0].cdw2.mapdist<<1;
	  exring[cid].unit[i].unit1_exec = 1;
	}
        break;
      case CMD_EXEC:  /* exec */
	if (cid <= reg_ctrl.i[cid].mcid) {
	  if (!reg_ctrl.i[cid].conf[i][0].cdw0.v)
	    exring[cid].unit[i].cmd = CMD_NOP;
	  else {
	    exring[cid].unit[i].cmd = CMD_EXEC;
	    if (exring[cid].unit[i].l_row == 0)
	      exring[cid].unit[i].unit1_exec = 1;
	  }
        }
        break;
      }
    }
    break;
  case CMD_SCON:  /* scon */
    if (exring[cid].unit[i].unit1_exec && (exring[cid].unit[i].scon_count & 1)) {
      *((Ull*)(&reg_ctrl.i[cid].conf[i][j])+0) = exring[cid].unit[pi].b[0][j].r[0];
      *((Ull*)(&reg_ctrl.i[cid].conf[i][j])+1) = exring[cid].unit[pi].b[0][j].r[1];
      *((Ull*)(&reg_ctrl.i[cid].conf[i][j])+2) = exring[cid].unit[pi].b[0][j].r[2];
      *((Ull*)(&reg_ctrl.i[cid].conf[i][j])+3) = exring[cid].unit[pi].b[0][j].r[3];
    }

    if ((exring[cid].unit[i].cycle&3) == 3) {
      if (exring[cid].unit[i].unit1_stop)
        exring[cid].unit[i].cmd = CMD_NOP;
      unit1_exec_next = (exring[cid].unit[i].scon_count <= 1) ? 0 : 1;
      unit1_stop_next = (exring[cid].unit[i].scon_count == 1) ? 1 : 0;
      if (exring[cid].unit[i].unit1_exec) {
        if (exring[cid].unit[i].scon_count & 1)
          exring[cid].unit[i].l_row = (exring[cid].unit[i].l_row+EMAX_DEPTH-1)%EMAX_DEPTH; /* adjust l_row */
        exring[cid].unit[i].scon_count--;
      }
      exring[cid].unit[i].unit1_exec = unit1_exec_next & ~unit1_stop_next;
      exring[cid].unit[i].unit1_stop =                    unit1_stop_next;
    }
    break;
  case CMD_EXEC:  /* exec */
    if ((exring[cid].unit[i].unit1_exec && (exring[cid].unit[i].l_row==0 || exring[cid].unit[(i+EMAX_DEPTH-1)%EMAX_DEPTH].brout_valid))   /* unit1   active */
      ||(exring[cid].unit[i].unit1_fold &&                                  exring[cid].unit[ i                         ].brout_valid)) { /* folding active */

      k   = (exring[cid].unit[i].lmea0sfma || exring[cid].unit[i].lmea0strq) ? exring[cid].unit[i].lmea0strqcol : j;

      if (reg_ctrl.i[cid].conf[i][j].cdw0.fold) {
        xi = i;  /* refer current br */
        bi = (reg_ctrl.i[cid].conf[i][k].cdw1.ea0bs&2)? i : pi; /* if ea0 takes eabbrs, shuold be i (else ea1(load) from pi) */
        oi = (reg_ctrl.i[cid].conf[i][j].cdw1.ea0os&1)? i : pi; /* if ea0 takes eaobrs, shuold be i (else ea1(load) from pi) */
      }
      else {
        xi = pi; /* normal */
        bi = pi; /* normal */
        oi = pi; /* normal */
      }
      s = reg_ctrl.i[cid].conf[i][j].cdw1.cs0;    exring[cid].unit[i].cx[0] = exring[cid].unit[xi].b[b][s/UNIT_WIDTH].r[s%UNIT_WIDTH];
      s = reg_ctrl.i[cid].conf[i][j].cdw1.cs1;    exring[cid].unit[i].cx[1] = exring[cid].unit[xi].b[b][s/UNIT_WIDTH].r[s%UNIT_WIDTH];
      s = reg_ctrl.i[cid].conf[i][j].cdw1.cs2;    exring[cid].unit[i].cx[2] = exring[cid].unit[xi].b[b][s/UNIT_WIDTH].r[s%UNIT_WIDTH];
      s = reg_ctrl.i[cid].conf[i][j].cdw1.cs3;    exring[cid].unit[i].cx[3] = exring[cid].unit[xi].b[b][s/UNIT_WIDTH].r[s%UNIT_WIDTH];

      s = reg_ctrl.i[cid].conf[i][j].cdw2.ts0;    exring[cid].unit[i].tx[0] = exring[cid].unit[pi].b[b][s/UNIT_WIDTH].r[s%UNIT_WIDTH];
      s = reg_ctrl.i[cid].conf[i][j].cdw2.ts1;    exring[cid].unit[i].tx[1] = exring[cid].unit[pi].b[b][s/UNIT_WIDTH].r[s%UNIT_WIDTH];
      s = reg_ctrl.i[cid].conf[i][j].cdw2.ts2;    exring[cid].unit[i].tx[2] = exring[cid].unit[pi].b[b][s/UNIT_WIDTH].r[s%UNIT_WIDTH];
      s = reg_ctrl.i[cid].conf[i][j].cdw2.ts3;    exring[cid].unit[i].tx[3] = exring[cid].unit[pi].b[b][s/UNIT_WIDTH].r[s%UNIT_WIDTH];

      s = reg_ctrl.i[cid].conf[i][j].cdw0.ex1brs; exring[cid].unit[i].ex1   = (j == 0 && reg_ctrl.i[cid].conf[i][j].cdw0.op1 == OP_FOR && (exring[cid].unit[i].stage_forstat & (1<<j)))
                                                                            ||(j == 1 && reg_ctrl.i[cid].conf[i][j].cdw0.op1 == OP_FOR && (exring[cid].unit[i].stage_forstat & (1<<j)))
                                                                            ||( reg_ctrl.i[cid].conf[i][j].cdw0.ex1s!=1)
						                            ||(!reg_ctrl.i[cid].conf[i][j].cdw0.fold && !exring[cid].unit[i].one_shot)
						                            ||( reg_ctrl.i[cid].conf[i][j].cdw0.fold && !exring[cid].unit[i].one_shot_fold)
						                            ||(!reg_ctrl.i[cid].conf[i][j].cdw0.fold && (reg_ctrl.i[cid].conf[i][j].cdw0.init&1)&&(exring[cid].unit[i].unit1_forstat&1))
						                            ||( reg_ctrl.i[cid].conf[i][j].cdw0.fold && (reg_ctrl.i[cid].conf[i][j].cdw0.init&1)&&(exring[cid].unit[i].unit1_forstat_fold&1))
                                                                            ? exring[cid].unit[xi].b[b][s/UNIT_WIDTH].r[s%UNIT_WIDTH] : exring[cid].unit[i].ex4dr_prev; /* self_loop */
      s = reg_ctrl.i[cid].conf[i][j].cdw0.ex2brs; exring[cid].unit[i].ex2   = exring[cid].unit[xi].b[b][s/UNIT_WIDTH].r[s%UNIT_WIDTH];
      s = reg_ctrl.i[cid].conf[i][j].cdw0.ex3brs; exring[cid].unit[i].ex3   = exring[cid].unit[xi].b[b][s/UNIT_WIDTH].r[s%UNIT_WIDTH];
#if 0
      //test016.c init 00000000 00000010 00000020 00000030 00000100 00000110 00000120 00000130 00000200 00000210 00000220 00000230 00000300 00000310 00000320 00000330
      //test016.c out  00000004 00000010 00000020 00000030 00000104 00000110 00000120 00000130 00000204 00000210 00000220 00000230 00000304 00000310 00000320 00000330
      if (i==2 && j==0) {
	s = reg_ctrl.i[cid].conf[i][j].cdw0.ex1brs;
	printf("%d: [%d][%d].cdw0.fold=%d one_shot=%d one_shot_fold=%d init=%d forstat=%d forstatfold=%d ex1=%08.8x mem=%08.8x exo=%08.8x ex2=%08.8x\n",
	       exring[cid].unit[i].cycle, i, j, reg_ctrl.i[cid].conf[i][j].cdw0.fold, exring[cid].unit[i].one_shot, exring[cid].unit[i].one_shot_fold,
	       reg_ctrl.i[cid].conf[i][j].cdw0.init, exring[cid].unit[i].unit1_forstat, exring[cid].unit[i].unit1_forstat_fold, (Uint)exring[cid].unit[i].ex1,
	       (Uint)exring[cid].unit[xi].b[b][s/UNIT_WIDTH].r[s%UNIT_WIDTH], (Uint)exring[cid].unit[i].ex4dr_prev, (Uint)exring[cid].unit[i].ex2);
      }
#endif
#if 0
//if (`conf_fold) begin
//  wire [`EXRING_ADDR_BITS-1:0]    ea0bs   = (!ea0loop||first) ? (kea0bfromBR ? bi eabout              : regvk_ea0b)|ofs : ea0out;
//  wire [`REG_DATA_BITS-1:0]       ea0os   = (!ea0loop||second)? (kea0ofromBR ? oi eaoout              : regvk_ea0o) : 0;
//  wire [`EXRING_ADDR_BITS-1:0]    ea1bs   = (!ea1loop||first) ? ( ea1bfromBR ? bi(eab0BR?eabout:eabin):  regv_ea1b) : ea1out;
//  wire [`REG_DATA_BITS-1:0]       ea1os   = (!ea1loop||second)? ( ea1ofromBR ? oi(eao0BR?eaoout:eaoin):  regv_ea1o) : 0;
//end
//else begin
//  wire [`EXRING_ADDR_BITS-1:0]    ea0bs   = (!ea0loop||first) ? (kea0bfromBR ? bi eabin               : regvk_ea0b)|ofs : ea0out;
//  wire [`REG_DATA_BITS-1:0]       ea0os   = (!ea0loop||second)? (kea0ofromBR ? oi eaoin               : regvk_ea0o) : 0;
//  wire [`EXRING_ADDR_BITS-1:0]    ea1bs   = (!ea1loop||first) ? ( ea1bfromBR ? bi eabin               :  regv_ea1b) : ea1out;
//  wire [`REG_DATA_BITS-1:0]       ea1os   = (!ea1loop||second)? ( ea1ofromBR ? oi eaoin               :  regv_ea1o) : 0;
//end
#endif
      /* eab/eao¤¬¾å¤«²¼¤«¤Ï,fold¤Èea0bs/ea0os¤Î¤ß¤Ë¤è¤Ã¤Æ·è¤Þ¤ë */
      s = reg_ctrl.i[cid].conf[i][k].cdw1.eabbrs; exring[cid].unit[i].eab   = exring[cid].unit[bi].b[b][s/UNIT_WIDTH].r[s%UNIT_WIDTH]; //¤³¤³¤¬SFMA+STBR¤Îµ¯¸»
      s = reg_ctrl.i[cid].conf[i][j].cdw1.eaobrs; exring[cid].unit[i].eao   = exring[cid].unit[oi].b[b][s/UNIT_WIDTH].r[s%UNIT_WIDTH];

#if 0
//    sb = reg_ctrl.i[cid].conf[i][k].cdw1.ea0bs;
//    if (sb & 1) { /* same as if (mex0op) */
//	if (!exring[cid].unit[i].one_shot||(reg_ctrl.i[cid].conf[i][j].cdw0.mex0init && (exring[cid].unit[i].unit1_forstat&1)))
//	  base = pi;
//	else
//	  base = ea04woofs_prev;
//    }
//    else
//	base = pi;
//
//    offs = pi;
#endif
      /* self_loop=0¤Î¾ì¹ç¤Î½é´üÃÍ¤Ïaddr+offs, self_loop=1¤«¤Ä½é²ó¤Ïaddr,°Ê¹ßaddr+offs */
      /* STRQ/SFMA+STBR¤ÏSTR¤ò4¥µ¥¤¥¯¥ëÊ¬³ä¼Â¹Ô */
      /* exring[cid].unit[i].lmea0strq ¤È exring[cid].unit[i].lmea0strqcol ¤Ï siml_unit_stage4()¤Ë¤ÆÀè¹Ô¥»¥Ã¥È */
      ofs = exring[cid].unit[i].lmea0strq ? (j<<3) : 0;
      sb = reg_ctrl.i[cid].conf[i][k].cdw1.ea0bs; exring[cid].unit[i].ea0b = (!(sb&1)||(!exring[cid].unit[i].one_shot||(reg_ctrl.i[cid].conf[i][j].cdw0.mex0init && (exring[cid].unit[i].unit1_forstat&1))))
						    ? (((sb&2)?exring[cid].unit[i].eab:reg_ctrl.i[cid].addr[i][k].ea0b)|ofs)
						    : exring[cid].unit[i].ea04woofs_prev; /* ¡ú */
      so = reg_ctrl.i[cid].conf[i][k].cdw1.ea0os; exring[cid].unit[i].ea0o = so
						    ? exring[cid].unit[i].eao
						    : reg_ctrl.i[cid].addr[i][k].ea0o;    /* ¡ú */
      sb = reg_ctrl.i[cid].conf[i][j].cdw1.ea1bs; exring[cid].unit[i].ea1b = (!(sb&1)||(!exring[cid].unit[i].one_shot||(reg_ctrl.i[cid].conf[i][j].cdw0.mex1init && (exring[cid].unit[i].unit1_forstat&1))))
						    ? ( (sb&2)?exring[cid].unit[i].eab:reg_ctrl.i[cid].addr[i][j].ea1b     )
						    : exring[cid].unit[i].ea14woofs_prev; /* ¡ú */
      so = reg_ctrl.i[cid].conf[i][j].cdw1.ea1os; exring[cid].unit[i].ea1o = so
						    ? exring[cid].unit[i].eao
						    : reg_ctrl.i[cid].addr[i][j].ea1o;    /* ¡ú */
    }

    if (exring[cid].unit[i].unit1_exec && (exring[cid].unit[i].l_row==0 || exring[cid].unit[(i+EMAX_DEPTH-1)%EMAX_DEPTH].brout_valid)) /* unit1   active */
      exring[cid].unit[i].stage2_exec = 1;
    else
      exring[cid].unit[i].stage2_exec = 0;

    if (exring[cid].unit[i].unit1_fold &&                                  exring[cid].unit[ i                         ].brout_valid) /* folding active */
      exring[cid].unit[i].stage2_fold = 1;
    else
      exring[cid].unit[i].stage2_fold = 0;

    exring[cid].unit[i].one_shot2           = exring[cid].unit[i].one_shot;
    exring[cid].unit[i].one_shot_fold2      = exring[cid].unit[i].one_shot_fold;
    exring[cid].unit[i].unit1_forstat2      = exring[cid].unit[i].unit1_forstat;
    exring[cid].unit[i].unit1_forstat_fold2 = exring[cid].unit[i].unit1_forstat_fold;

    if ((exring[cid].unit[i].cycle&3) == 3) {
      if ((!reg_ctrl.i[cid].conf[i][0].cdw0.fold && exring[cid].unit[i].unit2_stop)||(reg_ctrl.i[cid].conf[i][0].cdw0.fold && exring[cid].unit[i].unit2_fold && !exring[cid].unit[i].stage4_fold))
        exring[cid].unit[i].cmd = CMD_NOP;
      if (exring[cid].unit[i].unit1_exec && (exring[cid].unit[i].l_row==0 || exring[cid].unit[(i+EMAX_DEPTH-1)%EMAX_DEPTH].brout_valid)) /* active */
        exring[cid].unit[i].one_shot = 1;
      else
        exring[cid].unit[i].one_shot = 0;
      if (exring[cid].unit[i].unit1_fold) /* active */
        exring[cid].unit[i].one_shot_fold = 1;
      else
        exring[cid].unit[i].one_shot_fold = 0;
      unit1_exec_next    = exring[cid].unit[i].unit1_exec     | (reg_ctrl.i[cid].conf[i][0].cdw0.v && exring[cid].unit[i].l_row>0 && exring[cid].unit[(i+EMAX_DEPTH-1)%EMAX_DEPTH].unit2_exec);
      unit1_fold_next    = exring[cid].unit[i].unit2_exec     &  reg_ctrl.i[cid].conf[i][0].cdw0.fold;
      unit1_forstat_next = exring[cid].unit[i].l_row==0       ?  exring[cid].unit[i].stage_forstat                          : exring[cid].unit[(i+EMAX_DEPTH-1)%EMAX_DEPTH].unit2_forstat;
      unit1_forstat_fold_next = exring[cid].unit[i].unit2_forstat;
      unit1_stop_next    = exring[cid].unit[i].unit1_arbrk    | (reg_ctrl.i[cid].conf[i][0].cdw0.v && exring[cid].unit[i].l_row>0 && exring[cid].unit[(i+EMAX_DEPTH-1)%EMAX_DEPTH].unit2_stop);
      unit2_exec_next    = exring[cid].unit[i].unit1_exec;
      unit2_fold_next    = exring[cid].unit[i].unit1_fold     &  reg_ctrl.i[cid].conf[i][0].cdw0.fold;
      unit2_forstat_next = exring[cid].unit[i].unit1_forstat;
      unit2_stop_next    = exring[cid].unit[i].unit1_stop;
      exring[cid].unit[i].unit1_exec    = unit1_exec_next & ~unit1_stop_next;
      exring[cid].unit[i].unit1_fold    = unit1_fold_next;
      exring[cid].unit[i].unit1_forstat = unit1_forstat_next;
      exring[cid].unit[i].unit1_forstat_fold = unit1_forstat_fold_next;
      exring[cid].unit[i].unit1_stop    =                    unit1_stop_next;
      exring[cid].unit[i].unit2_exec    = unit2_exec_next & ~unit2_stop_next;
      exring[cid].unit[i].unit2_fold    = unit2_fold_next;
      exring[cid].unit[i].unit2_forstat = unit2_forstat_next;
      exring[cid].unit[i].unit2_stop    =                    unit2_stop_next;
    }
    break;
  }

  if ((exring[cid].unit[i].cycle&3) == 3 && (reg_ctrl.i[cid].cmd&3) == CMD_RESET)
    exring[cid].unit[i].cycle = 0;
  else
    exring[cid].unit[i].cycle++;

  return (0);
}

siml_unit_stage2(Uint cid, Uint i) /* stage-2 (EX/TX->1DR) */
{
  /******************************************************************************************************************************************************************************************/
  /* sample/test021.c #define spike01_core1(r, s)                                                                                                                                           */
  /* mo4(OP_LDRQ,  1,  BR[r][2], (Ull)b0,                  (Ull)bofs,        MSK_W1,    (Ull)b,          L*RMGRP,   0,      0,    (Ull)NULL,   L*RMGRP);                                    */
  /* mo4(OP_LDRQ,  1,  BR[r][1], (Ull)a[s][CHIP],          (Ull)cofs,        MSK_W1,    (Ull)a[s][CHIP], L,         0,      0,    (Ull)NULL,   L);                                          */
  /* exe(OP_NOP,      &AR[r][0], 0LL,           EXP_H3210, 0LL,              EXP_H3210, 0LL,             EXP_H3210, OP_NOP, 0LL,  OP_NOP,      0LL);                                        */
  /* mop(OP_LDBR,  1, &b00,      (Ull)c0[s][CHIP],         (Ull)oofs,        MSK_W0,    (Ull)c[s][CHIP], RMGRP/4,   0,      1,    (Ull)NULL,   RMGRP/4);                                    */
  /* ex4(OP_SFMA,     &b00,      INIT0?b00:b00, EXP_H3210, BR[r][1],         EXP_H3210, BR[r][2],        EXP_H3210, OP_NOP, 32LL, OP_NOP,      0LL);                                        */
  /* mop(OP_STBR,  1, &b00,      (Ull)oofs,                (Ull)c0[s][CHIP], MSK_D0,    (Ull)c[s][CHIP], RMGRP/4,   0,      1,    (Ull)NULL,   RMGRP/4)                                     */
  /******************************************************************************************************************************************************************************************/
  /*     SFMA Timing                       LD-B4               LD-A4               LD-C                                                                                                     */
  /*                  ¡Ý¡Ý¢®¢® t=3        ¡Ý¡Ý¢®¢® t=2        ¡Ý¡Ý¢®¢® t=1        ¡Ý¡Ý¢®¢®   t=0                                                                                            */
  /*                  ¡À¡¿¢¦¢¦            ¡À¡¿¢§¢¦            ¡À¡¿¢§¢¦            ¡À¡¿¢§¢¦                                                                                                  */
  /*                  ¢¢¢¢¢¢¢¢ t=4        ¢¢¢¢¢£¢¢ t=3        ¢¢¢¢¢£¢¢ t=2        ¢¢¢¢¢£¢¢   t=1                                                                                            */
  /*                  ¡Ý¡Ý     t=5        ¡Ý¡ÝB4   t=4        ¡Ý¡ÝA4   t=3        ¡Ý¡ÝC      t=2                                                                                            */
  /*                  |¢¦|¢¦¢¦            |¢¦|¢§¢¦            |¢¦|¢§¢¦            |¢¦|¢§¢¦                                                                                                  */
  /*                  ¢¢¢¢¢¢¢¢ t=6        ¢¢¢¢¢£¢¢ t=5        ¢¢¢¢¢£¢¢ t=4        ¢¢¢¢¢£¢¢   t=3                                                                                            */
  /*                  ¡Ý¡Ý                ¡Ý¡ÝB4              ¡Ý¡ÝA4              ¡Ý¡ÝC                                                                                                     */
  /*                  |¢¦|¢¦¢¦            |¢¦|¢§¢¦            |¢¦|¢§¢¦            |¢¦|¢§¢¦                                                                                                  */
  /*              ¨£¨¡¢¢¢¢¢¢¢¢ t=7    ¨£¨¡¢£¢£¢£¢£ t=6    ¨£¨¡¢£¢£¢£¢£ t=5    ¨£¨¡¢¢¢¢¢£¢¢   t=4                                                                                            */
  /*              ¨¢  ¡Ý¡Ý¡Ý¡Ý        ¨¢  B3B2B1B0        ¨¢  A3A2A1A0        ¨¢  ¡Ý¡ÝC ¡Ý                                                                                                  */
  /*              ¨¢B3A3C             ¨¢B2A2C             ¨¢B1A1C             ¨¢B0A0C                                                                                                       */
  /******************************************************************************************************************************************************************************************/
  /*              ¨¢  SFMA            ¨¢  SFMA            ¨¢  SFMA            ¨¢  SFMA  STBR                                                                                                */
  /*              ¨¦¢ª¡Ý¡Ý¢®¢® t=11   ¨¦¢ª¡Ý¡Ý¢®¢® t=10   ¨¦¢ª¡Ý¡Ý¢®¢® t=9    ¨¦¢ª¡Ý¡Ý¢®¢®   t=8                                                                                            */
  /*                  ¡À¡¿¢¦¢§            ¡À¡¿¢¦¢§            ¡À¡¿¢¦¢§            ¡À¡¿¢¢¢§                                                                                                  */
  /*                  ¢£¢£¢¢¢£ t=12       ¢£¢£¢¢¢£ t=11       ¢£¢£¢¢¢£ t=10       ¢£¢£¢¢¢£   t=9                                                                                            */
  /*                  ¡Ý¡Ý     t=13       ¡Ý¡Ý     t=12       ¡Ý¡Ý     t=11       ¡Ý¡Ý  C(st)t=10                                                                                           */
  /*              ¨£¢ª|¢§|¢¦¢§      ¢«¨¨¢ª|¢§|¢¦¢§      ¢«¨¨¢ª|¢§|¢¦¢§      ¢«¨¨¢ª|¢§|¢¦¢§                                                                                                  */
  /*              ¨¦¨¡¢¢¢£¢¢¢£ t=14   ¨¦¨¡¢¢¢£¢¢¢£ t=13   ¨¦¨¡¢¢¢£¢¢¢£ t=12   ¨¦¨¡¢¢¢£¢¢¢£   t=11                                                                                           */
  /*                  ¡Ý¡Ý¡ÝC(st)         ¡Ý¡Ý¡ÝC(st)         ¡Ý¡Ý¡ÝC(st)         ¡Ý¡Ý¡ÝC(st)                                                                                               */
  /*                  |¢¦|¢¦¢¦            |¢¦|¢¦¢¦            |¢¦|¢¦¢¦            |¢¦|¢¦¢¦                                                                                                  */
  /*                  ¢¢¢¢¢¢¢¢ t=15       ¢¢¢¢¢¢¢¢ t=14       ¢¢¢¢¢¢¢¢ t=13       ¢¢¢¢¢¢¢¢   t=12                                                                                           */
  /*                  ¡Ý¡Ý¡Ý¡Ý            ¡Ý¡Ý¡Ý¡Ý            ¡Ý¡Ý¡Ý¡Ý            ¡Ý¡Ý¡Ý¡Ý                                                                                                  */
  /******************************************************************************************************************************************************************************************/

  int    j = (exring[cid].unit[i].cycle+(EMAX_WIDTH-1)) % EMAX_WIDTH; /* 3,0,1,2,3,0,1,2 */
  int    k;
  Ull    c3, c2, c1, c0;
  Ushort pattern;
  Ull    cxd;
  Uint   op_nf1, op_ex1, op_ex2, op_ex3;
  Uint   init, one_shot2, forstat2;
  Ull    r1, r2, r3, r4, r5;
  Uint   exp1, exp2, exp3;
  Uint   ex1_retval;
  Ull    base0, offs0;
  Ull    base1, offs1;
  Uint   mex0op,   mex1op;   /* ¡ú */
  Uint   mex0init, mex1init; /* ¡ú */
  Uint   mex0dist, mex1dist; /* ¡ú */
  Uint   mex0ofs,  mex1ofs;  /* ¡ú */
  Uint   mexlimit;           /* ¡ú */
  Uint   mexblock;           /* ¡ú */

  switch (exring[cid].unit[i].cmd) {
  case CMD_NOP:   /* nop */
    if ((exring[cid].unit[i].cycle&3) == 3) {
      switch (reg_ctrl.i[cid].cmd&3) {
      case CMD_RESET: /* reset */
        exring[cid].unit[i].stage_forstat = 0;
        exring[cid].unit[i].unit1_arbrk = 0;
        exring[cid].unit[i].stage3_exec = 0;
        exring[cid].unit[i].stage3_fold = 0;
        exring[cid].unit[i].cx2dr       = 0;
        exring[cid].unit[i].ex2passr1   = 0;
        exring[cid].unit[i].ex2passr2   = 0;
        exring[cid].unit[i].ex2passr3   = 0;
        exring[cid].unit[i].ex2passr4   = 0;
        exring[cid].unit[i].ex2dr       = 0LL;
        exring[cid].unit[i].ex2dr_sfma0 = 0LL;
        exring[cid].unit[i].ex2dr_sfma1 = 0LL;
        exring[cid].unit[i].ex2dr_sfma2 = 0LL;
        exring[cid].unit[i].ex2dr_sfma3 = 0LL;
        exring[cid].unit[i].ex2dr_sfma4 = 0LL;
        exring[cid].unit[i].ex2dr_sfma5 = 0LL;
        exring[cid].unit[i].ex2dr_sfma6 = 0LL;
        exring[cid].unit[i].ex2dr_sfma7 = 0LL;
        exring[cid].unit[i].ea02dofs    = 0; /* ¡ú */
        exring[cid].unit[i].ea02dr      = 0;
        exring[cid].unit[i].ea12dofs    = 0; /* ¡ú */
        exring[cid].unit[i].ea12dr      = 0;
        exring[cid].unit[i].tx2dr[0]    = 0LL;
        exring[cid].unit[i].tx2dr[1]    = 0LL;
        exring[cid].unit[i].tx2dr[2]    = 0LL;
        exring[cid].unit[i].tx2dr[3]    = 0LL;
        break;
      }
    }
    break;
  case CMD_SCON:  /* scon */
    break;
  case CMD_EXEC:  /* exec */
    if (exring[cid].unit[i].stage2_exec || exring[cid].unit[i].stage2_fold) { /* active */
      /* cx -> cx2dr */
      c0      = exring[cid].unit[i].cx[0];
      c1      = exring[cid].unit[i].cx[1];
      c2      = exring[cid].unit[i].cx[2];
      c3      = exring[cid].unit[i].cx[3];
      pattern = reg_ctrl.i[cid].conf[i][j].cdw1.cex_tab;
      cex(OP_CEXE, &cxd, c3, c2, c1, c0, pattern);
      exring[cid].unit[i].cx2dr = cxd;

      /* ex -> ex2dr */
      op_nf1 = reg_ctrl.i[cid].conf[i][1].cdw0.op1 != OP_FOR;
      op_ex1 = reg_ctrl.i[cid].conf[i][j].cdw0.op1;
      op_ex2 = reg_ctrl.i[cid].conf[i][j].cdw0.op2;
      op_ex3 = reg_ctrl.i[cid].conf[i][j].cdw0.op3;
      init   = reg_ctrl.i[cid].conf[i][j].cdw0.init;
      one_shot2 = exring[cid].unit[i].one_shot2;
      forstat2 = exring[cid].unit[i].unit1_forstat2;
      r1     = exring[cid].unit[i].ex1;
      r2     = (op_ex1 == OP_FOR && j == 1 && !(exring[cid].unit[i].stage_forstat & 1)) /* LOOP1¤Ï¡¤LOOP0==0¤Î»þ¤Î¤ß¹¹¿· */
             ||((init & 2) && one_shot2 && !(forstat2 & 1)) ? 0LL /* init0 specified && LOOP0-not_end */ : exring[cid].unit[i].ex2;
      r3     = exring[cid].unit[i].ex3;
      r4     = reg_ctrl.i[cid].conf[i][j].cdw0.e2is==0 ? reg_ctrl.i[cid].conf[i][j].cdw3.e2imm
             : reg_ctrl.i[cid].conf[i][j].cdw0.e2is==1 ? exring[cid].unit[i].ex2
             :                                           exring[cid].unit[i].ex3;
      r5     = reg_ctrl.i[cid].conf[i][j].cdw0.e3is==0 ? reg_ctrl.i[cid].conf[i][j].cdw0.e3imm
             :                                           exring[cid].unit[i].ex3;
      exp1   = reg_ctrl.i[cid].conf[i][j].cdw0.ex1exp;
      exp2   = reg_ctrl.i[cid].conf[i][j].cdw0.ex2exp;
      exp3   = reg_ctrl.i[cid].conf[i][j].cdw0.ex3exp;

      if (reg_ctrl.i[cid].conf[i][j].cdw0.op1 == OP_SFMA) {
	exring[cid].unit[i].ex2passr1 = (Uchar)r1; /* OP_FSMA */
	exring[cid].unit[i].ex2passr2 = r2; /* OP_FSMA/x11_softu64_dist */
	exring[cid].unit[i].ex2passr3 = r3; /* OP_FSMA/x11_softu64_dist */
	exring[cid].unit[i].ex2passr4 = (Uchar)r4; /* OP_FSMA */
	ex1_retval = softu64(1, &exring[cid].unit[i].ex2dr_sfma0, NULL, NULL, r1, r2, r3, r4);
      }
      else
	ex1_retval = exe(op_ex1, &exring[cid].unit[i].ex2dr, r1, exp1, r2, exp2, r3, exp3, op_ex2, r4, op_ex3, r5);

      if ((exring[cid].unit[i].cycle&3) == 0)
        exring[cid].unit[i].unit1_arbrk = 0;
      else if (op_ex1 == OP_WHILE) {
        exring[cid].unit[i].unit1_arbrk = ex1_retval;
        /*printf("%d WHILE dst=%d arbrk=%d\n", j, (Uint)exring[cid].unit[i].ex2dr, exring[cid].unit[i].unit1_arbrk);*/
      }
      else if (op_ex1 == OP_FOR) {
        if (j == 0) { /* LOOP0 */
          exring[cid].unit[i].unit1_arbrk    = (op_nf1<<1 | ex1_retval)==3;
          exring[cid].unit[i].stage_forstat  =  op_nf1<<1 | ex1_retval;
        }
        else if (j == 1) { /* LOOP1 */
          exring[cid].unit[i].unit1_arbrk    = (ex1_retval<<1 | exring[cid].unit[i].stage_forstat)==3;
          exring[cid].unit[i].stage_forstat  =  ex1_retval<<1 | exring[cid].unit[i].stage_forstat;
        }
        /*printf("%d FOR stage_forstat=%d dst=%d arbrk=%d\n", j, exring[cid].unit[i].stage_forstat, (Uint)exring[cid].unit[i].ex2dr, exring[cid].unit[i].unit1_arbrk);*/
      }

      /* ea -> ea2dr */
      base0 = exring[cid].unit[i].ea0b;
      offs0 = exring[cid].unit[i].ea0o;
      base1 = exring[cid].unit[i].ea1b;
      offs1 = exring[cid].unit[i].ea1o;
      mex0op   = reg_ctrl.i[cid].conf[i][j].cdw0.mex0op;   /* ¡ú */
      mex1op   = reg_ctrl.i[cid].conf[i][j].cdw0.mex1op;   /* ¡ú */
      mex0init = reg_ctrl.i[cid].conf[i][j].cdw0.mex0init; /* ¡ú */
      mex1init = reg_ctrl.i[cid].conf[i][j].cdw0.mex1init; /* ¡ú */
      mex0dist = reg_ctrl.i[cid].conf[i][j].cdw0.mex0dist; /* ¡ú */
      mex1dist = reg_ctrl.i[cid].conf[i][j].cdw0.mex1dist; /* ¡ú */
      mexlimit = reg_ctrl.i[cid].conf[i][j].cdw0.mexlimit; /* ¡ú */

//    eag(&exring[cid].unit[i].ea02dr, base0, offs0, reg_ctrl.i[cid].conf[i][k].cdw1.ea0msk);
//    eag(&exring[cid].unit[i].ea12dr, base1, offs1, reg_ctrl.i[cid].conf[i][j].cdw1.ea1msk);

      if (mex0op) {  /* ¡ú */
	if ((!one_shot2)||(mex0init && (forstat2 & 1))) /* ¡ú */
	  mex0ofs = 0;        /* ¡ú */
	else {                /* ¡ú */
	  switch (mex0dist) { /* ¡ú */
	  case  0: mex0ofs =  0; break; /* ¡ú */
	  case  1: mex0ofs =  1; break; /* ¡ú */
	  case  2: mex0ofs =  2; break; /* ¡ú */
	  case  3: mex0ofs =  4; break; /* ¡ú */
	  case  4: mex0ofs =  8; break; /* ¡ú */
	  case  5: mex0ofs = 16; break; /* ¡ú */
	  case  6: mex0ofs = 32; break; /* ¡ú */
	  default: mex0ofs = 64; break; /* ¡ú */
	  }
	}
      }
      else           /* ¡ú */
	mex0ofs = 0; /* ¡ú */
      if (mex1op) {  /* ¡ú */
	if ((!one_shot2)||(mex1init && (forstat2 & 1))) /* ¡ú */
	  mex1ofs = 0;        /* ¡ú */
	else {                /* ¡ú */
	  switch (mex1dist) { /* ¡ú */
	  case  0: mex1ofs =  0; break; /* ¡ú */
	  case  1: mex1ofs =  1; break; /* ¡ú */
	  case  2: mex1ofs =  2; break; /* ¡ú */
	  case  3: mex1ofs =  4; break; /* ¡ú */
	  case  4: mex1ofs =  8; break; /* ¡ú */
	  case  5: mex1ofs = 16; break; /* ¡ú */
	  case  6: mex1ofs = 32; break; /* ¡ú */
	  default: mex1ofs = 64; break; /* ¡ú */
	  }
	}
      }
      else           /* ¡ú */
	mex1ofs = 0; /* ¡ú */
      switch (mexlimit) { /* ¡ú */
      case  0: mexblock =      0; break; /* ¡ú */
      case  1: mexblock =      8; break; /* ¡ú */
      case  2: mexblock =     16; break; /* ¡ú */
      case  3: mexblock =     32; break; /* ¡ú */
      case  4: mexblock =     64; break; /* ¡ú */
      case  5: mexblock =    128; break; /* ¡ú */
      case  6: mexblock =    256; break; /* ¡ú */
      case  7: mexblock =    512; break; /* ¡ú */
      case  8: mexblock =   1024; break; /* ¡ú */
      case  9: mexblock =   2048; break; /* ¡ú */
      case 10: mexblock =   4096; break; /* ¡ú */
      case 11: mexblock =   8192; break; /* ¡ú */
      case 12: mexblock =  16384; break; /* ¡ú */
      case 13: mexblock =  32768; break; /* ¡ú */
      case 14: mexblock =  65536; break; /* ¡ú */
      default: mexblock = 131072; break; /* ¡ú */
      }

//    mex(mex0op, &exring[cid].unit[i].ea02dr, base0, mex0ofs, exring[cid].unit[i].mexmr1d_prev, exring[cid].unit[i].mexmr0d_prev); /* ¡ú */
//    mex(mex1op, &exring[cid].unit[i].ea12dr, base1, mex1ofs, exring[cid].unit[i].mexmr1d_prev, exring[cid].unit[i].mexmr0d_prev); /* ¡ú */
      mex(mex1op, &exring[cid].unit[i].ea12dr, base1, mex1ofs, mex0op, &exring[cid].unit[i].ea02dr, base0, mex0ofs, mexblock, exring[cid].unit[i].mexmr1d_prev, exring[cid].unit[i].mexmr0d_prev); /* ¡ú */

      k = (exring[cid].unit[i].lmea0sfma || exring[cid].unit[i].lmea0strq) ? exring[cid].unit[i].lmea0strqcol : j;
      exring[cid].unit[i].ea02dofs = eam(offs0, reg_ctrl.i[cid].conf[i][k].cdw1.ea0msk); /* ¡ú */
      exring[cid].unit[i].ea12dofs = eam(offs1, reg_ctrl.i[cid].conf[i][j].cdw1.ea1msk); /* ¡ú */

      /* tx -> tx2dr */
      exring[cid].unit[i].tx2dr[0] = exring[cid].unit[i].tx[0];
      exring[cid].unit[i].tx2dr[1] = exring[cid].unit[i].tx[1];
      exring[cid].unit[i].tx2dr[2] = exring[cid].unit[i].tx[2];
      exring[cid].unit[i].tx2dr[3] = exring[cid].unit[i].tx[3];
    }

    if (exring[cid].unit[i].stage2_exec)
      exring[cid].unit[i].stage3_exec = 1;
    else
      exring[cid].unit[i].stage3_exec = 0;

    if (exring[cid].unit[i].stage2_fold)
      exring[cid].unit[i].stage3_fold = 1;
    else
      exring[cid].unit[i].stage3_fold = 0;

    exring[cid].unit[i].one_shot_fold3      = exring[cid].unit[i].one_shot_fold2;
    exring[cid].unit[i].unit1_forstat_fold3 = exring[cid].unit[i].unit1_forstat_fold2;

    break;
  }

  return (0);
}

siml_unit_stage3(Uint cid, Uint i) /* stage-3 (1DR->2DR) */
{
  int    j = (exring[cid].unit[i].cycle+(EMAX_WIDTH-2)) % EMAX_WIDTH; /* 2,3,0,1,2,3,0,1 */
  int    k;            /* ¡ú */
  Ull    base0, offs0; /* ¡ú */
  Ull    base1, offs1; /* ¡ú */

  switch (exring[cid].unit[i].cmd) {
  case CMD_NOP:   /* nop */
    if ((exring[cid].unit[i].cycle&3) == 3) {
      switch (reg_ctrl.i[cid].cmd&3) {
      case CMD_RESET: /* reset */
        exring[cid].unit[i].stage4_exec = 0;
        exring[cid].unit[i].stage4_fold = 0;
        exring[cid].unit[i].cx3dr     = 0;
        exring[cid].unit[i].ex3passr1 = 0;
        exring[cid].unit[i].ex3passr2 = 0;
        exring[cid].unit[i].ex3passr3 = 0;
        exring[cid].unit[i].ex3dr     = 0LL;
	exring[cid].unit[i].ea03woofs = 0; /* ¡ú */
        exring[cid].unit[i].ea03dr    = 0;
	exring[cid].unit[i].ea13woofs = 0; /* ¡ú */
        exring[cid].unit[i].ea13dr    = 0;
        exring[cid].unit[i].tx3dr[0]  = 0LL;
        exring[cid].unit[i].tx3dr[1]  = 0LL;
        exring[cid].unit[i].tx3dr[2]  = 0LL;
        exring[cid].unit[i].tx3dr[3]  = 0LL;
        break;
      }
    }
    break;
  case CMD_SCON:  /* scon */
    break;
  case CMD_EXEC:  /* exec */
    if (exring[cid].unit[i].stage3_exec || exring[cid].unit[i].stage3_fold) { /* active */
      /* cx2dr -> cx3dr */
      exring[cid].unit[i].cx3dr = exring[cid].unit[i].cx2dr;

      /* ex2dr -> ex3dr */
      if (reg_ctrl.i[cid].conf[i][j].cdw0.op1 == OP_SFMA) { /* stage3¤Ç¾è»»·ë²Ì¤ÎMAGNIÄ´À° */
	exring[cid].unit[i].ex3passr1 = exring[cid].unit[i].ex2passr1; /* OP_FSMA */
	exring[cid].unit[i].ex3passr2 = exring[cid].unit[i].ex2passr2; /* OP_FSMA/x11_softu64_dist */
	exring[cid].unit[i].ex3passr3 = exring[cid].unit[i].ex2passr3; /* OP_FSMA/x11_softu64_dist */
	softu64(2, &exring[cid].unit[i].ex2dr_sfma0, &exring[cid].unit[i].ex3dr, NULL, exring[cid].unit[i].ex2passr1, exring[cid].unit[i].ex2passr2, exring[cid].unit[i].ex2passr3, exring[cid].unit[i].ex2passr4);
      }
      else
	exring[cid].unit[i].ex3dr   = exring[cid].unit[i].ex2dr;

      /* ea2dr -> ea3dr */
//    exring[cid].unit[i].ea03dr = exring[cid].unit[i].ea02dr;
//    exring[cid].unit[i].ea13dr = exring[cid].unit[i].ea12dr;
      exring[cid].unit[i].ea03woofs = exring[cid].unit[i].ea02dr;   /* ¡ú */
      base0                         = exring[cid].unit[i].ea02dr;   /* ¡ú */
      offs0                         = exring[cid].unit[i].ea02dofs; /* ¡ú */
      exring[cid].unit[i].ea13woofs = exring[cid].unit[i].ea12dr;   /* ¡ú */
      base1                         = exring[cid].unit[i].ea12dr;   /* ¡ú */
      offs1                         = exring[cid].unit[i].ea12dofs; /* ¡ú */
      eag(&exring[cid].unit[i].ea03dr, base0, offs0);               /* ¡ú */
      eag(&exring[cid].unit[i].ea13dr, base1, offs1);               /* ¡ú */

      /* tx2dr -> tx3dr */
      exring[cid].unit[i].tx3dr[0] = exring[cid].unit[i].tx2dr[0];
      exring[cid].unit[i].tx3dr[1] = exring[cid].unit[i].tx2dr[1];
      exring[cid].unit[i].tx3dr[2] = exring[cid].unit[i].tx2dr[2];
      exring[cid].unit[i].tx3dr[3] = exring[cid].unit[i].tx2dr[3];
    }

    if (exring[cid].unit[i].stage3_exec)
      exring[cid].unit[i].stage4_exec = 1;
    else
      exring[cid].unit[i].stage4_exec = 0;

    if (exring[cid].unit[i].stage3_fold)
      exring[cid].unit[i].stage4_fold = 1;
    else
      exring[cid].unit[i].stage4_fold = 0;

    exring[cid].unit[i].one_shot_fold4      = exring[cid].unit[i].one_shot_fold3;
    exring[cid].unit[i].unit1_forstat_fold4 = exring[cid].unit[i].unit1_forstat_fold3;

    break;
  }

  return (0);
}

siml_unit_stage4_pre(Uint cid, Uint i) /* stage-4 (2DR->3DR)  (LMRING_BRIN->LMRING_TR) */
{
  int    pi = (i+EMAX_DEPTH-1)%EMAX_DEPTH;
  int    j  = (exring[cid].unit[i].cycle+(EMAX_WIDTH-3)) % EMAX_WIDTH; /* 1,2,3,0,1,2,3,0 */
  struct lmring_br *br = ((i%(EMAX_DEPTH/LMRING_MUX))==0)?(struct lmring_br*)&axiif[cid].axring_br[axiif[cid].axring_b_bot]
                                                         :(struct lmring_br*)&exring[cid].unit[pi].lmring_br[exring[cid].unit[pi].lmring_b_bot];
  Uint   ftag, ltag, fmask, lmask, aximask;

  ftag  = (reg_ctrl.i[cid].addr[i][br->col].top&~(sizeof(Ull)*UNIT_WIDTH-1)); /* & 0xffffffe0 */
  ltag  = (reg_ctrl.i[cid].addr[i][br->col].bot&~(sizeof(Ull)*UNIT_WIDTH-1)); /* & 0xffffffe0 */
  switch ((reg_ctrl.i[cid].addr[i][br->col].top/sizeof(Uint)) & (UNIT_WIDTH*2-1)) {
  case 0: fmask = 0xff; break;
  case 1: fmask = 0xfe; break;
  case 2: fmask = 0xfc; break;
  case 3: fmask = 0xf8; break;
  case 4: fmask = 0xf0; break;
  case 5: fmask = 0xe0; break;
  case 6: fmask = 0xc0; break;
  case 7: fmask = 0x80; break;
  }
  switch (~(reg_ctrl.i[cid].addr[i][br->col].bot/sizeof(Uint)) & (UNIT_WIDTH*2-1)) {
  case 0: lmask = 0xff; break;
  case 1: lmask = 0x7f; break;
  case 2: lmask = 0x3f; break;
  case 3: lmask = 0x1f; break;
  case 4: lmask = 0x0f; break;
  case 5: lmask = 0x07; break;
  case 6: lmask = 0x03; break;
  case 7: lmask = 0x01; break;
  }
  aximask = ((br->dm & 0xf0000000)?0x80:0) | ((br->dm & 0x0f000000)?0x40:0) | ((br->dm & 0x00f00000)?0x20:0) | ((br->dm & 0x000f0000)?0x10:0)
          | ((br->dm & 0x0000f000)?0x08:0) | ((br->dm & 0x00000f00)?0x04:0) | ((br->dm & 0x000000f0)?0x02:0) | ((br->dm & 0x0000000f)?0x01:0);
  exring[cid].unit[i].ranger_ok = (ftag< br->a && br->a< ltag) ? 0xff :
                                  (ftag==br->a && br->a< ltag) ? (fmask) :
                                  (ftag< br->a && br->a==ltag) ? (lmask) :
                                  (ftag==br->a && br->a==ltag) ? (fmask & lmask) : 0;
  exring[cid].unit[i].rangew_ok = (ftag< br->a && br->a< ltag) ? 0xff :
                                  (ftag==br->a && br->a< ltag) ? (fmask & aximask) :
                                  (ftag< br->a && br->a==ltag) ? (lmask & aximask) :
                                  (ftag==br->a && br->a==ltag) ? (fmask & lmask & aximask) : 0;

  exring[cid].unit[i].lmranger_ok    = (reg_ctrl.i[cid].conf[i][br->col].cdw2.lmm_axir && br->rw==0 && br->ty==4) ? exring[cid].unit[i].ranger_ok : 0; /* 8bits */
  exring[cid].unit[i].lmrangew_ok    = (reg_ctrl.i[cid].conf[i][br->col].cdw2.lmm_axiw && br->rw==1 && br->ty==4) ? exring[cid].unit[i].rangew_ok : 0; /* 8bits */
  exring[cid].unit[i].lmlddmqw_ok    = br->rw==1 && br->ty==3 && reg_ctrl.i[cid].conf[i][br->col].cdw1.ea1op == OP_LDDMQ; /* lmwd[0]->tr *//* LDDMQ¤ÏÆ±°ìslot¤Ë¹â¡¹1¤¬Á°Äó */
  exring[cid].unit[i].lmea0sfma      = reg_ctrl.i[cid].conf[i][0].cdw1.ea0op == OP_STBR && reg_ctrl.i[cid].conf[i][0].cdw0.op1 == OP_SFMA; /* SFMA+STBR */
  exring[cid].unit[i].lmea0strq      = reg_ctrl.i[cid].conf[i][0].cdw1.ea0op == OP_STRQ;                                                   /* STRQ */
  exring[cid].unit[i].lmea0strqcol   = 0; /* default (fixed to 0) */
                                     /* MEXÄÉ²ÃÁ°: ¹ÔÁ´ÂÎ¤Îea0       ¤Ï[i][0].fold=0¤Ê¤éstage4_exec, fold=1¤Ê¤éstage4_fold¤Ë½¾¤¦ */
                                     /* MEXÄÉ²Ã¸å: ¹ÔÁ´ÂÎ¤Îea0(load) ¤Ïstage4_exec¤Ë½¾¤¦                                         */
                                     /*            ¹ÔÁ´ÂÎ¤Îea0(store)¤Ï[i][0].fold=0¤Ê¤éstage4_exec, fold=1¤Ê¤éstage4_fold¤Ë½¾¤¦ */
  exring[cid].unit[i].lmring_ea0bsy  = ((reg_ctrl.i[cid].conf[i][j].cdw1.ea0op && reg_ctrl.i[cid].conf[i][j].cdw1.ea0op <= OP_IM_BUFRD) /* ¡ú */
                                     && exring[cid].unit[i].stage4_exec)/* op0 LD */                                                    /* ¡ú */
                                   || (((reg_ctrl.i[cid].conf[i][j].cdw1.ea0op && reg_ctrl.i[cid].conf[i][j].cdw1.ea0op <= OP_IM_BUFWR)||(exring[cid].unit[i].lmea0sfma||exring[cid].unit[i].lmea0strq))
                                     &&((!reg_ctrl.i[cid].conf[i][0].cdw0.fold && exring[cid].unit[i].stage4_exec) || (reg_ctrl.i[cid].conf[i][0].cdw0.fold && exring[cid].unit[i].stage4_fold)));/*op0 ST */
                                     /*            ¹ÔÁ´ÂÎ¤Îea1(load) ¤Ïstage4_exec¤Ë½¾¤¦ */
  exring[cid].unit[i].lmring_ea1bsy  =  (reg_ctrl.i[cid].conf[i][j].cdw1.ea1op && reg_ctrl.i[cid].conf[i][j].cdw1.ea1op <= OP_IM_BUFRD) /* ¡ú */
                                     && exring[cid].unit[i].stage4_exec;/* op1 LD */                                                    /* ¡ú */
  exring[cid].unit[i].lmring_ful     =(exring[cid].unit[i].lmring_ful2==LMRING_BR_BUF) || (exring[cid].unit[i].lmring_ful1 && exring[cid].unit[i].lmring_ful2==LMRING_BR_BUF-1); /* assign */
  exring[cid].unit[i].deq_wait       = exring[cid].unit[i].lmring_ful                  || (exring[cid].unit[i].lmranger_ok && exring[cid].unit[i].lmring_ea1bsy)
                                                                                       || (exring[cid].unit[i].lmrangew_ok && exring[cid].unit[i].lmring_ea0bsy)
                                                                                       || (exring[cid].unit[i].lmlddmqw_ok && br->col!=j                       );
  return (0);
}

siml_unit_stage4(Uint cid, Uint i) /* stage-4 (2DR->3DR)  (LMRING_BRIN->LMRING_TR) */
{
  /* LMM¤Îcolumn¥Þ¥ë¥Á¥¹¥ì¥Ã¥Ç¥¤¥ó¥°¤Ï,EXRING¤òÍ¥Àè¤·,LMRING¤Ï·ä´Ö¤ÇÆ°ºî¤µ¤»¤ë */
  /* Îã¤¨¤ÐSTRQ¤¬Â¸ºß¤¹¤ë¾ì¹ç,EXRING¤ÏÁ´cycle¤ò»È¤Ã¤Æ¤·¤Þ¤¦¤Î¤Ç,·ä´Ö¤¬¤Ê¤¯¤Ê¤ë */
  /* ·ä´Ö¤¬¤Ê¤¤¾ì¹ç¤ÎPLOAD¤Ï,EXRING´°Î»¸å¤ËÆ°ºî³«»Ï¤¹¤ë¤³¤È¤Ë¤Ê¤ë              */
  /* ¤³¤ÎÅÀ,TR¤âÆ±ÍÍ¤À¤¬,TR¤ÏÄ¾Á°¤ÎLDDMQ¤Ë·ä´Ö¤¬½Ð¤ë¤Î¤Ç,PLOAD¤ÈÆ±»þÆ°ºî²ÄÇ½   */

  /*                        EXEC   NOP               */
  /* lmring_brin                       -> lmring_tr  */
  /* cx3dr                             -> cx4dr      */
  /* ex3dr                             -> ex4dr      */
  /* lmwa/ea03dr,mwsa                  -> ea04dr/ma0 */
  /* lmra/ea13dr,mrsa                  -> ea14dr/ma1 */
  /* lmwd/ex3dr/tx3dr,trs              -> tx4dr      */
  /* lmwd/ex3dr/tx3dr,mws              -> mw         */

  /******************************************************************************************************************************************************************************************/
  /*      |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       | */
  /* clk _/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/ */
  /* cycle  7       0       1       2       3       4       5       6       7       0       1       2       3       4       5       6       7       0       1       2       3       4       */
  /*              V-update                        V-update                        V-update                        V-update                        V-update                        V-update  */
  /* brout_valid __/~~(l_row==row0-1)~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

  /* ---- row[i]--------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */
  /*              V-update                        V-update                        V-update                        V-update                        V-update                        V-update  */
  /* cmd         --<==EXEC=========================================================================================================================================================>------- */
  /* one_shot    __________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______ */
  /* unit1_exec  __/~~(l_row==0)~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________________________________ */
  /* unit1_stop  __________________________________________________________________________________________________________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______ */
  /* stage1out** ----------<== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3==> */
  /* stage2out   ------------------<== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2==> */
  /* stage3out   --------------------------<== 0===X== 1===X==*2*==X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1==> */

  /* ful0(prev)  ___________________________1_______2______/3~~~~~~~3~~~~~~~3~~~~~~~3~~~~~~~3~~~~~~~3~~~~~~~3~~~~~~~3~~~~~~~3~~~~~~~3~~~~~~~3~~~~~~~3~~~~~~~3~~~~~~~3~~~~~~~3~~~~~~~3~~~~~~ */
  /* dv/sq       __________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
  /* ia          --------------------------<==a0===X==a1===X==a2===>-----------------------<==a3===>-------<==a4===X==a5===X==a6===>---------------<==a7===>------------------------------- */
  /* di          --------------------------<==d0===X==d1===X==d2===>-----------------------<==d3===>-------<==d4===X==d5===X==d6===>---------------<==d7===>------------------------------- */
  /*                                                              A enq_en deq_en=~wait & ~ful                                                                                              */
  /*                                                                                        ful = (ful2==3)|(ful1&(ful2==2))                                                                */
  /* wait        __________________________________________/~~~~~~~\_______________________/~~~~~~~\_______________________/~~~~~~~\_______________________/~~~~~~~\_______________________ */
  /* ful1        ___________________________0_______0_______0_______0______/1~~~~~~\0______/1~~~~~~\0______/1~~~~~~~1~~~~~~~1~~~~~~\0_______0______/1~~~~~~\0_______0_______0_______0______ */
  /* ful2                                   3       3       2       2       2       2       1       1       1       1       2       3       2       2       3       2       2       2       */
  /* ful         ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______/~~~~~~~~~~~~~~~\_______________________________________/~~~~~~~~~~~~~~~\_______/~~~~~~~~~~~~~~~\_______________________ */
  /* ful1        __________________________________________________________/~~~~~~~\_______/~~~~~~~\_______/~~~~~~~~~~~~~~~~~~~~~~~\_______________/~~~~~~~\_______________________________ */
  /*                                                                                        dv <= deq_en                                                                                    */
  /* ea04dr      --------------------------------------------------<== 2===X==a0===>-------<==a1===X== 2===X==a2===X==a3===X==a4===X== 2===>-------<==a5===>-------<== 2===>--------------- */
  /* ia          ----------------------------------------------------------<==a0===>-------<==a1===>-------<==a2===X==a3===X==a4===>---------------<==a5===>------------------------------- */
  /* di          ----------------------------------------------------------<==d0===>-------<==d1===>-------<==d2===X==d3===X==d4===>---------------<==d5===>------------------------------- */

  /* stage4out   -------tx4dr----------------------<== 0===X== 1===X==*2*==X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0=== */
  /* tr_valid    __________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______ */

  /* ful2                                   3       3       2       2       2       2       1       1       1       1       2       3       2       2       3       2       2       2       */
  /* stage5out   -------brout------------------------------<==p0===X==p1===X=*p2*==X==p3===X==q0===X==q1===X==q2===X==q3===X==q0===X==q1===X==q2===X==q3===X==q0===X==q1===X==q2===X==q3==> */
  /*              V-update                        V-update                        V-update                        V-update                        V-update                        V-update  */
  /* brout_valid __________________________________________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

  /* ---- row[i+1]------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */
  /*              V-update                        V-update                        V-update                        V-update                        V-update                        V-update  */
  /* cmd         --<==EXEC=========================================================================================================================================================>------- */
  /* one_shot    __________________________________________________________________________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
  /* unit1_exec  __________________________________________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
  /* stage1out   --------------------------------------------------------------------------<== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3==> */

  /******************************************************************************************************************************************************************************************/
  /*      |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       | */
  /* clk _/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/ */
  /* one_shot    __________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\________________________________________ */
  /* stage1_r1   --<==s1==>--------------------------------------------------------<==s1==>------------------------------------------------------------------------------------------------ */
  /* stage1_r2/3 --<=s2/s3><=s2/s3><=s2/s3><=s2/s3>--------------------------------<=s2/s3><=s2/s3><=s2/s3><=s2/s3>------------------------------------------------------------------------ */
  /* stage2_sfma ----------<=MULT=><=MULT=><=MULT=><=MULT=>--------------------------------<=MULT=><=MULT=><=MULT=><=MULT=>---------------------------------------------------------------- */
  /* stage3_sfma ------------------<=MULT=><=MULT=><=MULT=><=MULT=>--------------------------------<=MULT=><=MULT=><=MULT=><=MULT=>-------------------------------------------------------- */
  /* one_shot4   __________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\________________ */
  /* stage4_s1   --------------------------<=s1===><=s1===><=s1===><=s1===><=s1===><=s1===><=s1===><=s1===><=s1===><=s1===><=s1===><=s1===><=s1===><=s1===><=s1===><=s1===>---------------- */
  /* stage4_sfma --------------------------<=ACC==><=ACC==><=ACC==><=ACC==><=ACC==><=ACC==><=ACC==><=ACC==><=ACC==><=ACC==><=ACC==><=ACC==><=ACC==><=ACC==><=ACC==><=ACC==>---------------- */
  /* stage5_st   ----------------------------------<=LMMW=>--------------------------------------------------------<=LMMW=>---------------------------------------------------------------- */

  int    pi = (i+EMAX_DEPTH-1)%EMAX_DEPTH;
  int    j  = (exring[cid].unit[i].cycle+(EMAX_WIDTH-3)) % EMAX_WIDTH; /* 1,2,3,0,1,2,3,0 */
  int    k; /* for STRQ(lmea0strqcol) */
  struct lmring_br *br = ((i%(EMAX_DEPTH/LMRING_MUX))==0)?(struct lmring_br*)&axiif[cid].axring_br[axiif[cid].axring_b_bot]
                                                         :(struct lmring_br*)&exring[cid].unit[pi].lmring_br[exring[cid].unit[pi].lmring_b_bot];
  int             ful2 = ((i%(EMAX_DEPTH/LMRING_MUX))==0)?axiif[cid].axring_ful2           /* axi      ful2 */
                                                         :exring[cid].unit[pi].lmring_ful2;/* previous ful2 */
  struct lmring_tr *tr = &exring[cid].unit[i].lmring_tr;
  int cdx = br->a / (REG_AREA_MASK+1) & (EMAX_NCHIP-1);               /* for registers */
  int idx = br->a & (sizeof(Ull)*UNIT_WIDTH*EMAX_WIDTH*EMAX_DEPTH-1); /* for registers */
  int row = idx/(sizeof(Ull)*UNIT_WIDTH*EMAX_WIDTH); /* max8K/128B=0........63 *//* 64 rows */
  int deq_wait = (((i%(EMAX_DEPTH/LMRING_MUX)==0)&&axiif[cid].exring_deq_wait)||((i%(EMAX_DEPTH/LMRING_MUX)>0)&&exring[cid].unit[i].deq_wait));

  /* LMRING_TR */
#if 0
  Ull   lmranger_ok   : 4; /* wire *//* lmringÍ×µá¤¬read &ty==4&adr[col]<>lmm_rangeÆâ */
  Ull   lmrangew_ok   : 4; /* wire *//* lmringÍ×µá¤¬write&ty==4&adr[col]<>lmm_rangeÆâ */
  Ull   lmlddmqw_ok   : 1; /* wire *//* lmringÍ×µá¤¬write&ty==3&op1[col]==LDDMQ */
  Ull   lmea0sfma     : 1; /* wire *//* sfma+ea0.stbrÂ¸ºß 4¥µ¥¤¥¯¥ë¤ËÊ¬¤±¤Æ¼Â¹Ô */
  Ull   lmea0strq     : 1; /* wire *//* ea0.strqÂ¸ºß      4¥µ¥¤¥¯¥ë¤ËÊ¬¤±¤Æ¼Â¹Ô */
  Ull   lmea0strqcol  : 2; /* wire *//* ea0.strq_colÈÖ¹æ  4¥µ¥¤¥¯¥ë¤ËÊ¬¤±¤Æ¼Â¹Ô */
  Ull   lmring_ea0bsy : 1; /* wire *//* ea0Í­¸ú */
  Ull   lmring_ea1bsy : 1; /* wire *//* ea1Í­¸ú */
  Ull   lmring_ful    : 1; /* wire *//* (ful2==3)|(ful1 & (ful2==2)) */
  Ull   deq_wait      : 1; /* wire *//* lmring_ful|(ranger_ok&ea1)|(rangew_ok&ea0)|(lddmqw_ok&col!=j) */
  Ull   lmring_ful1   : 1; /* 0:rw/ty/co/sq/a/di/dmÌµ¸ú, 1:rw/ty/co/sq/a/di/dmÍ­¸ú */
  struct lmring_tr {
    Ull   rw          : 1; /* 0:read, 1:write */
    Ull   ty          : 3; /* 0:reg/conf, 1:reg/breg, 2:reg/addr, 3:lddmq/tr, 4:lmm, 567:-- */
    Ull   col         : 2; /* logical col# for target lmm */
    Ull   sq          :16; /* sequential # for pipelined DMA */
    Ull   av          : 1; /* address valid */
    Ull   a           :31; /* logical addr reg/lmm */
    Ull   dm          :32; /* <- lmm wdata       */
    Ull   d[UNIT_WIDTH]  ; /* <- lmm wdata/rdata */
    Ull   merge       : 4; /* wordwise 0:merge_lmm, 1:pass_lmm */
  } lmring_tr;
  Ull   lmco          : 2; /* wire *//* -> col# */
  Ull   lmca          :18; /* wire *//* -> ea01dr *//* col#¤Ë¤è¤ë2bitÊäÀµÁ°addr */
  Ull   lmwm          :32; /* wire *//* <- axi   */
  Ull   lmwd[UNIT_WIDTH] ; /* wire *//* <- axi   */
#endif

  /* op0                      */
  /* OP_LDR          0x01 ... */
  /* OP_STR          0x11 ... */
  /* OP_LDDMQ        0x18     */
  /* OP_TR           0x19     */
  /* OP_IM_BUFWR     0x1e ----¤³¤³¤Þ¤Çeag¤È´³¾Ä */
  /* OP_IM_PREF      0x1f lmring-writeÆ°ºî ¼«¿È¤Ê¤Î¤Ç´³¾Ä¤·¤Ê¤¤ */

  /* op1                      */
  /* OP_LDR          0x01 ... */
  /* OP_IM_BUFRD     0x0e ----¤³¤³¤Þ¤Çeag¤È´³¾Ä */
  /* OP_IM_DRAIN     0x0f lmring-readÆ°ºî  ¼«¿È¤Ê¤Î¤Ç´³¾Ä¤·¤Ê¤¤ */
  /* OP_LDDMQ        0x18 lmring-readÆ°ºî  ¼«¿È¤Ê¤Î¤Ç´³¾Ä¤·¤Ê¤¤ */
  /* OP_TR           0x19 lmring-readÆ°ºî  ¼«¿È¤Ê¤Î¤Ç´³¾Ä¤·¤Ê¤¤ */

  /* lmmi[][]¤Ïmerge(copy=1)¤Î¤ß¤ÇsplitÌµ.DMAÂÐ¾Ý¤Ïv=1,copy=0¤Î¤ß¤À¤¬EXEC¤¬block¤·¤Æ¤¤¤Ê¤¤¸Â¤êLMM»²¾È²Ä */
  /* conf.lmm_mode¤Ïmerge/splitÍ­.EXEC»þ¤ËEA->LMM¤ÎºÝ¤Î¥¢¥É¥ì¥¹¥Þ¥¹¥¯¤òÀ©¸æ */

  /*  °Ê²¼¤Î¤è¤¦¤Ëtop/bot¤¬32Byte¥¢¥é¥¤¥ó¤µ¤ì¤Æ¤¤¤Ê¤¤¾ì¹ç,stage#2¤Ë¤âstage#3ÍÑ¥Ç¡¼¥¿¤¬¼è¤ê¹þ¤Þ¤ì¤Ê¤¤¤¿¤á¤Ë¤Ï,axi_adr¤Ë²Ã¤¨¤Æmask¤Î¹ÍÎ¸¤¬É¬Í× */
  /*  for stage#2            mask=0000000000000000 0000000000000000 FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF  axi_adr=00020    */
  /*   bot=0002f,top=00000   data=0003000300030002 0003000100030000 0002000700020006 0002000500020004                   */
  /*  for stage#3            mask=FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF 0000000000000000 0000000000000000  axi_adr=00020    */
  /*   bot=0005f,top=00030   data=0003000300030002 0003000100030000 0002000700020006 0002000500020004                   */

  /* Á´Í­¸úlmmi[i][j]¤ËÂÐ¤·, conf.lmm_mode¤Ï, exring¤«¤é¤ÎLD/ST¤¬»²¾È¤¹¤Ù¤­LMMÈÏ°Ï¤òcycle%4Ëè¤Ë»Ø¼¨ */
  /* Á´Í­¸úlmmi[i][j]¤ËÂÐ¤·, lmmi.bcas/copy¤Ï¡¤ARM¤¬Åö³ºLMMI¤òDMAÂÐ¾Ý¤È¤¹¤ë¤«¤ò»Ø¼¨ */
  /* lmm_mode¤Èbcas/copy¤ÏÏ¢Æ°¤¹¤ë */
  /* Á´Í­¸úlmmi[i][j]¤ËÂÐ¤·, lmm_mode/bcas/copy¤È¤ÏÆÈÎ©¤Ë, reg_ctrl.addr[i][j].top/bot¤¬lmring¸þ¤±¤ËÈÏ°Ï¤òÄó¼¨ */
  /* top/bot¤Ïcycle%4¤Î´Ö¤Ç½ÅÊ£¤¹¤ë¤³¤È¤â¤¢¤ë¤¬¡¤col¤Ç»ØÄê¤·¤¿cycle%4¤Îtop/bot¤ËHIT¤¹¤ì¤Ðlmring¤«¤éÎ®¤·¹þ¤á¤Ð¤è¤¤. */
  /* ¤¿¤À¤·¡¤½ñ¤­¹þ¤ß»þ¤Ë¤ÏLMM¤ÎÊ¬³ä¿ô¤òÈ½Äê¤¹¤ëÉ¬Í×¤¬¤¢¤ê,col¤ËÂÐ±þ¤¹¤ëconf.lmm_mode¤Î»²¾È¤¬É¬Í× */
  /* ¾å°Ì2bit¤Î¥Þ¥¹¥¯¤Ï¡¤EA04DR¤Ë½ñ¤­¹þ¤àºÝ¤ËÅ¬ÍÑ¤¹¤ë¡ÊLMRA/EA1,LMWA/EA0¤¬³Æ¡¹¶¦Í­¡Ë*/
  if (exring[cid].unit[i].cmd == CMD_NOP
      && (exring[cid].unit[i].cycle&3) == 3
      && (reg_ctrl.i[cid].cmd&3) == CMD_RESET) {
    exring[cid].unit[i].lmring_ful1    = 0;
    tr->rw   = 0;
    tr->ty   = 0;
    tr->col  = 0;
    tr->sq   = 0;
    tr->av   = 0;
    tr->a    = 0;
    tr->dm   = 0;
    tr->d[0] = 0LL;
    tr->d[1] = 0LL;
    tr->d[2] = 0LL;
    tr->d[3] = 0LL;
    tr->merge = 0;
  }
  else if (ful2 && !deq_wait) { /* for next cycle */
    exring[cid].unit[i].lmco    = br->col;  /* wire *//* -> col# */
    exring[cid].unit[i].lmca    = br->a;    /* wire *//* -> ea01dr *//* col#¤Ë¤è¤ë2bitÊäÀµÁ°addr */
    exring[cid].unit[i].lmwm    = br->dm & (((exring[cid].unit[i].rangew_ok&0x80)?0xf0000000:0) | ((exring[cid].unit[i].rangew_ok&0x40)?0x0f000000:0)
                                          | ((exring[cid].unit[i].rangew_ok&0x20)?0x00f00000:0) | ((exring[cid].unit[i].rangew_ok&0x10)?0x000f0000:0)
                                          | ((exring[cid].unit[i].rangew_ok&0x08)?0x0000f000:0) | ((exring[cid].unit[i].rangew_ok&0x04)?0x00000f00:0)
                                          | ((exring[cid].unit[i].rangew_ok&0x02)?0x000000f0:0) | ((exring[cid].unit[i].rangew_ok&0x01)?0x0000000f:0)); /* wire *//* <- axi */
    exring[cid].unit[i].lmwd[0] = br->d[0]; /* wire *//* <- axi   */
    exring[cid].unit[i].lmwd[1] = br->d[1]; /* wire *//* <- axi   */
    exring[cid].unit[i].lmwd[2] = br->d[2]; /* wire *//* <- axi   */
    exring[cid].unit[i].lmwd[3] = br->d[3]; /* wire *//* <- axi   */

    exring[cid].unit[i].lmring_ful1  = 1; /* next cycle */
    tr->rw   = br->rw;   /* next cycle */
    tr->ty   = br->ty;   /* next cycle */
    tr->col  = br->col;  /* next cycle */
    tr->sq   = br->sq;   /* next cycle */
    tr->av   = br->av || exring[cid].unit[i].lmranger_ok || exring[cid].unit[i].lmrangew_ok || (((reg_ctrl.i[cid].cmd>>16) == cdx || cdx == 0) && exring[cid].unit[i].l_row == row && br->ty<4);
    tr->a    = br->a;    /* next cycle */
    tr->dm   = br->dm;   /* next cycle */
    tr->d[0] = br->d[0]; /* next cycle */
    tr->d[1] = br->d[1]; /* next cycle */
    tr->d[2] = br->d[2]; /* next cycle */
    tr->d[3] = br->d[3]; /* next cycle */
    tr->merge = br->ty<4 ? 0 : exring[cid].unit[i].lmranger_ok; /* reg or read (VADDR/LDDMQ/TRANS) in range */
  }
  else
    exring[cid].unit[i].lmring_ful1  = 0; /* always dequeued */

  /* EX4DR EA04DR,MA0 EA14DR,MA1 TX4DR,MW */
#if 0
  Ull   ex4dr_prev       ; /* reg  *//* for siml-loop only */
  Ull   ex4dr            ; /* reg  *//* out for third-stage */
  Ull   ea04_lmask    :18; /* wire *//* offset */
  Ull   ea04_umask    : 2; /* wire *//* partition */
  Ull   ea04woofs_prev:18; /* reg  *//* for siml-loop only */
  Ull   ea04woofs     :18; /* reg  *//* for mex(&addr) feedback */
  Ull   ea04dr        :18; /* reg  */
  Ull   ea14_lmask    :18; /* wire *//* offset */
  Ull   ea14_umask    : 2; /* wire *//* partition */
  Ull   ea14woofs_prev:18; /* reg  *//* for siml-loop only */
  Ull   ea14woofs     :18; /* reg  *//* for mex(&addr) feedback */
  Ull   ea14dr        :18; /* reg  */
  Ull   tr_valid      : 1; /* TR¤Î¾õÂÖ¤òÉ½¼¨ */
  Ull   tx4dr[UNIT_WIDTH]; /* reg  */
  Ull   mwmux[UNIT_WIDTH]; /* wire for mw0[] */
  struct lmm {
    Ull   en0         : 1; /* internal reg       */
    Ull   en1         : 1; /* internal reg       */
    Ull   rw0         : 1; /* 0:read, 1:write    */
    Ull   rw1         : 1; /* 0:read,(1:write)   */
    Ull   ma0         :18; /* internal reg  addr(32B aligned)      */
    Ull   ma1         :18; /* internal reg  addr(32B aligned)      */
    Ull   mm0         :32; /* internal reg  mask */
    Ull   mw0[UNIT_WIDTH]; /* internal reg  data */
    Ull   mr0[UNIT_WIDTH]; /* internal wire data */
    Ull   mr1[UNIT_WIDTH]; /* internal wire data */
    Uchar m[LMEM_SIZE]   ; /* local memory       */
  } lmm;
#endif

  if (exring[cid].unit[i].cmd == CMD_NOP
      && (exring[cid].unit[i].cycle&3) == 3
      && (reg_ctrl.i[cid].cmd&3) == CMD_RESET) {
    exring[cid].unit[i].ex4dr_prev     = 0LL; /* siml-loop only */
    exring[cid].unit[i].ex4dr          = 0LL;
    exring[cid].unit[i].ea04woofs_prev = 0;   /* ¡úsiml-loop only */
    exring[cid].unit[i].ea04dr         = 0;
    exring[cid].unit[i].ea14woofs_prev = 0;   /* ¡úsiml-loop only */
    exring[cid].unit[i].ea14dr         = 0;
    exring[cid].unit[i].tx4dr[0]       = 0LL;
    exring[cid].unit[i].tx4dr[1]       = 0LL;
    exring[cid].unit[i].tx4dr[2]       = 0LL;
    exring[cid].unit[i].tx4dr[3]       = 0LL;
    exring[cid].unit[i].tr_valid       = 0;
    exring[cid].unit[i].lmm.en0        = 0;
    exring[cid].unit[i].lmm.en1        = 0;
    exring[cid].unit[i].lmm.rw0        = 0;
    exring[cid].unit[i].lmm.rw1        = 0;
    exring[cid].unit[i].lmm.ma0        = 0;
    exring[cid].unit[i].lmm.ma1        = 0;
    exring[cid].unit[i].lmm.mm0        = 0;
    exring[cid].unit[i].lmm.mw0[0]     = 0LL;
    exring[cid].unit[i].lmm.mw0[1]     = 0LL;
    exring[cid].unit[i].lmm.mw0[2]     = 0LL;
    exring[cid].unit[i].lmm.mw0[3]     = 0LL;
  }
  else {
    exring[cid].unit[i].ex4dr_prev     = exring[cid].unit[i].ex4dr; /* siml-loop only */
    if (exring[cid].unit[i].stage4_exec || exring[cid].unit[i].stage4_fold) { /* active */
      /* LD/ST¤Ë¤è¤ëLMM»²¾È¤Ïlmm_mode¤Î³ä¤êÅö¤Æcycle¤ò¸·Ì©¤Ë¼é¤ë. */
      /* ex3dr -> ex4dr */
      if (reg_ctrl.i[cid].conf[i][j].cdw0.op1 == OP_SFMA) { /* stage4¤ÇSFMA²Ã»» */
	if ((!exring[cid].unit[i].one_shot_fold4 || ((reg_ctrl.i[cid].conf[i][j].cdw0.init&1)&&(exring[cid].unit[i].unit1_forstat_fold4&1))) && j==0)
	  softu64(3, NULL, &exring[cid].unit[i].ex3dr, &exring[cid].unit[i].ex4dr, exring[cid].unit[i].ex3passr1, exring[cid].unit[i].ex3passr2, exring[cid].unit[i].ex3passr3, 0LL);
	else
	  softu64(3, NULL, &exring[cid].unit[i].ex3dr, &exring[cid].unit[i].ex4dr, exring[cid].unit[i].ex4dr,     exring[cid].unit[i].ex3passr2, exring[cid].unit[i].ex3passr3, 0LL);
      }
      else 
	exring[cid].unit[i].ex4dr  = exring[cid].unit[i].ex3dr;
    }

    /* lmca/ea3dr -> ea4dr */
    /* exring[cid].unit[i].lmranger_ok   = rw==0 && ty==4 && [br->col].top <= br->a && br->a <= [br->col].bot); */
    /* exring[cid].unit[i].lmrangew_ok   = rw==1 && ty==4 && [br->col].top <= br->a && br->a <= [br->col].bot); */
    /* exring[cid].unit[i].lmlddmqw_ok   = rw==1 && ty==3 && [br->col].op1==LDDMQ   */
    /* exring[cid].unit[i].lmea0sfma     = STBR[*];                                 */
    /* exring[cid].unit[i].lmea0strq     = STRQ[*];                                 */
    /* exring[cid].unit[i].lmea0strqcol  = STRQ[*];                                 */
    /* exring[cid].unit[i].lmring_ea0bsy = EXEC && (normal_op0||(STBR[*]||STRQ[*]));*/
    /* exring[cid].unit[i].lmring_ea1bsy = EXEC && (normal_op1);                    */
    /* exring[cid].unit[i].lmring_ful    = NEXT_TR+NEXT_BR==FULL;                   */
    /* exring[cid].unit[i].deq_wait      = lmring_ful | (ranger_ok & ea1bsy) | (rangew_ok & ea0bsy) | (lddmqw_ok & col!=j); */

    /* eaop0/eaop1¤òÄ¾ÀÜ¸¡ºº¤¹¤ë¤Î¤Ç,conf.mwsa¤Ï»È¤ï¤Ê¤¯¤ÆÎÉ¤¤ */
    /* (NOP | EXEC) & !deq_wait & lmrangew_ok¤Î»þ lmwa->ea04dr */
    /* lmring_ea0bsy                         ¤Î»þ,ea0d->ea04dr */
    exring[cid].unit[i].ea04woofs_prev = exring[cid].unit[i].ea04woofs; /* ¡úsiml-loop only */
    exring[cid].unit[i].ea04woofs      = exring[cid].unit[i].ea03woofs; /* ¡ú */
    if ((exring[cid].unit[i].cmd == CMD_NOP || exring[cid].unit[i].cmd == CMD_EXEC)
      && ful2 && !deq_wait && exring[cid].unit[i].lmrangew_ok) { /* axi->lmm_write */
      /* LMEM_SIZE(128KB)/4 = 32KB... lmm_mode=1:adr=(block=col&0)*(LMEM_SIZE/4) | (ofs=adr&(LMEM_SIZE/1-1)) */
      /*                              lmm_mode=2:adr=(block=col&2)*(LMEM_SIZE/4) | (ofs=adr&(LMEM_SIZE/2-1)) */
      /*                              lmm_mode=3:adr=(block=col&3)*(LMEM_SIZE/4) | (ofs=adr&(LMEM_SIZE/4-1)) */
      switch (reg_ctrl.i[cid].conf[i][exring[cid].unit[i].lmco].cdw2.lmm_mode) {
      case 0: exring[cid].unit[i].ea04_umask = LMEM_UMASK0; exring[cid].unit[i].ea04_lmask = LMEM_LMASK0; break;
      case 1: exring[cid].unit[i].ea04_umask = LMEM_UMASK1; exring[cid].unit[i].ea04_lmask = LMEM_LMASK1; break;
      case 2: exring[cid].unit[i].ea04_umask = LMEM_UMASK2; exring[cid].unit[i].ea04_lmask = LMEM_LMASK2; break;
      case 3: exring[cid].unit[i].ea04_umask = LMEM_UMASK3; exring[cid].unit[i].ea04_lmask = LMEM_LMASK3; break;
      }
      exring[cid].unit[i].ea04dr      = ((exring[cid].unit[i].lmco & exring[cid].unit[i].ea04_umask) * (LMEM_SIZE/4)) | (exring[cid].unit[i].lmca & exring[cid].unit[i].ea04_lmask);
      exring[cid].unit[i].lmm.en0     = 1;
      exring[cid].unit[i].lmm.rw0     = 1; /* write */
      exring[cid].unit[i].lmm.ma0     = (exring[cid].unit[i].ea04dr % LMEM_SIZE) & ~(sizeof(Ull)*UNIT_WIDTH-1);
      exring[cid].unit[i].lmm.mm0     = exring[cid].unit[i].lmwm;
      exring[cid].unit[i].lmm.mw0[0]  = exring[cid].unit[i].lmwd[0];
      exring[cid].unit[i].lmm.mw0[1]  = exring[cid].unit[i].lmwd[1];
      exring[cid].unit[i].lmm.mw0[2]  = exring[cid].unit[i].lmwd[2];
      exring[cid].unit[i].lmm.mw0[3]  = exring[cid].unit[i].lmwd[3];
#if 0
printf("====== WLMM row=%x br.col=%x ====axir=%x axiw=%x br->rw=%x br->ty=%x a=%08.8x lmranger_ok=%x lmrangew_ok=%x top=%08.8x bot=%08.8x\n", i, br->col, reg_ctrl.i[cid].conf[i][br->col].cdw2.lmm_axir, reg_ctrl.i[cid].conf[i][br->col].cdw2.lmm_axiw, br->rw, br->ty, br->a, exring[cid].unit[i].lmranger_ok, exring[cid].unit[i].lmrangew_ok, reg_ctrl.i[cid].addr[i][br->col].top, reg_ctrl.i[cid].addr[i][br->col].bot);
#endif
    }
    else if (exring[cid].unit[i].lmring_ea0bsy) { /* EXEC & ea0d->lmm_write/read (STRQ/SFMA¤ÎËè¥µ¥¤¥¯¥ë¼Â¹Ô¤â¤³¤³) */
      int kmex0;
      k = (exring[cid].unit[i].lmea0sfma || exring[cid].unit[i].lmea0strq) ? exring[cid].unit[i].lmea0strqcol : j;
      /* ¡ú¡ú¡ú¡ú¡ú¡ú¡ú¡ú¡ú¡ú¡úMEX0ÍÑ¤ÎADDR_MASK */
      /* conf[][col=2/3]mex0op¤Ï,conf[][col=1  ].lmm_mode¤ò»²¾È */
      /* conf[][col=1  ]mex0op¤Ï,conf[][col=0  ].lmm_mode¤ò»²¾È */
      kmex0 = ((reg_ctrl.i[cid].conf[i][2].cdw0.mex0op==OP_CMPA_LE || reg_ctrl.i[cid].conf[i][2].cdw0.mex0op==OP_CMPA_GE) && reg_ctrl.i[cid].conf[i][j].cdw1.ea0op < OP_IM_BUFWR) ? ((j==1||j==0)?0:1) : k; /* ¡ú */
      /* ¡ú¡ú¡ú¡ú¡ú¡ú¡ú¡ú¡ú¡ú¡úMEX0ÍÑ¤ÎLD */
      /* if (i==2 && k==2) printf("[2][2] mexop0=%d\n", reg_ctrl.i[cid].conf[i][k].cdw0.mex0op); */
      switch (reg_ctrl.i[cid].conf[i][kmex0].cdw2.lmm_mode) {   /* ¡ú */
      case 0: exring[cid].unit[i].ea04_umask = LMEM_UMASK0; exring[cid].unit[i].ea04_lmask = LMEM_LMASK0; break;
      case 1: exring[cid].unit[i].ea04_umask = LMEM_UMASK1; exring[cid].unit[i].ea04_lmask = LMEM_LMASK1; break;
      case 2: exring[cid].unit[i].ea04_umask = LMEM_UMASK2; exring[cid].unit[i].ea04_lmask = LMEM_LMASK2; break;
      case 3: exring[cid].unit[i].ea04_umask = LMEM_UMASK3; exring[cid].unit[i].ea04_lmask = LMEM_LMASK3; break;
      }
      exring[cid].unit[i].ea04dr   = ((kmex0 & exring[cid].unit[i].ea04_umask) * (LMEM_SIZE/4)) | (exring[cid].unit[i].ea03dr & exring[cid].unit[i].ea04_lmask); /* ¡ú */
      /* if (i==2 && k==2) printf("kmex0=%d lmm_mode=%d ea04dr=%08.8x\n", kmex0, reg_ctrl.i[cid].conf[i][kmex0].cdw2.lmm_mode, (Uint)exring[cid].unit[i].ea04dr); */
      exring[cid].unit[i].lmm.en0  = 1;
      exring[cid].unit[i].lmm.rw0  = (reg_ctrl.i[cid].conf[i][k].cdw1.ea0op & 0x10)!=0; /* read/write */
      exring[cid].unit[i].lmm.ma0  = (exring[cid].unit[i].ea04dr % LMEM_SIZE) & ~(sizeof(Ull)*UNIT_WIDTH-1);
                                        /* OP_TR   ¤Î¾ì¹ç,eag0->WRITE,eag1->READ */
                                        /* OP_LDDMQ¤Î¾ì¹ç,eag0->WRITE,eag1->READ */
      /* mws[0-3]:2; 0:lmwd1, 1:exdr, 2:ts1 */
      exring[cid].unit[i].mwmux[0] = reg_ctrl.i[cid].conf[i][k].cdw2.mws0==1 ? exring[cid].unit[i].ex4dr : exring[cid].unit[i].tx3dr[0]; /* ex3dr->ex4dr for SFMR+STBR */
      exring[cid].unit[i].mwmux[1] = reg_ctrl.i[cid].conf[i][k].cdw2.mws1==1 ? exring[cid].unit[i].ex4dr : exring[cid].unit[i].tx3dr[1]; /* ex3dr->ex4dr for SFMR+STBR */
      exring[cid].unit[i].mwmux[2] = reg_ctrl.i[cid].conf[i][k].cdw2.mws2==1 ? exring[cid].unit[i].ex4dr : exring[cid].unit[i].tx3dr[2]; /* ex3dr->ex4dr for SFMR+STBR */
      exring[cid].unit[i].mwmux[3] = reg_ctrl.i[cid].conf[i][k].cdw2.mws3==1 ? exring[cid].unit[i].ex4dr : exring[cid].unit[i].tx3dr[3]; /* ex3dr->ex4dr for SFMR+STBR */
      switch (reg_ctrl.i[cid].conf[i][k].cdw1.ea0op) {
      case OP_STR:
        exring[cid].unit[i].lmm.mm0 = (exring[cid].unit[i].lmm.mm0 & 0xfffffff0) | (((exring[cid].unit[i].ea03dr/sizeof(Ull) & (UNIT_WIDTH-1))==0 && (exring[cid].unit[i].cx3dr&1)) ? 0x0000000f : 0x00000000);
        exring[cid].unit[i].lmm.mm0 = (exring[cid].unit[i].lmm.mm0 & 0xffffff0f) | (((exring[cid].unit[i].ea03dr/sizeof(Ull) & (UNIT_WIDTH-1))==0 && (exring[cid].unit[i].cx3dr&2)) ? 0x000000f0 : 0x00000000);
        exring[cid].unit[i].lmm.mm0 = (exring[cid].unit[i].lmm.mm0 & 0xfffff0ff) | (((exring[cid].unit[i].ea03dr/sizeof(Ull) & (UNIT_WIDTH-1))==1 && (exring[cid].unit[i].cx3dr&1)) ? 0x00000f00 : 0x00000000);
        exring[cid].unit[i].lmm.mm0 = (exring[cid].unit[i].lmm.mm0 & 0xffff0fff) | (((exring[cid].unit[i].ea03dr/sizeof(Ull) & (UNIT_WIDTH-1))==1 && (exring[cid].unit[i].cx3dr&2)) ? 0x0000f000 : 0x00000000);
        exring[cid].unit[i].lmm.mm0 = (exring[cid].unit[i].lmm.mm0 & 0xfff0ffff) | (((exring[cid].unit[i].ea03dr/sizeof(Ull) & (UNIT_WIDTH-1))==2 && (exring[cid].unit[i].cx3dr&1)) ? 0x000f0000 : 0x00000000);
        exring[cid].unit[i].lmm.mm0 = (exring[cid].unit[i].lmm.mm0 & 0xff0fffff) | (((exring[cid].unit[i].ea03dr/sizeof(Ull) & (UNIT_WIDTH-1))==2 && (exring[cid].unit[i].cx3dr&2)) ? 0x00f00000 : 0x00000000);
        exring[cid].unit[i].lmm.mm0 = (exring[cid].unit[i].lmm.mm0 & 0xf0ffffff) | (((exring[cid].unit[i].ea03dr/sizeof(Ull) & (UNIT_WIDTH-1))==3 && (exring[cid].unit[i].cx3dr&1)) ? 0x0f000000 : 0x00000000);
        exring[cid].unit[i].lmm.mm0 = (exring[cid].unit[i].lmm.mm0 & 0x0fffffff) | (((exring[cid].unit[i].ea03dr/sizeof(Ull) & (UNIT_WIDTH-1))==3 && (exring[cid].unit[i].cx3dr&2)) ? 0xf0000000 : 0x00000000);
        exring[cid].unit[i].lmm.mw0[0] = exring[cid].unit[i].mwmux[0]; /* alignÉÔÍ× */
        exring[cid].unit[i].lmm.mw0[1] = exring[cid].unit[i].mwmux[1]; /* alignÉÔÍ× */
        exring[cid].unit[i].lmm.mw0[2] = exring[cid].unit[i].mwmux[2]; /* alignÉÔÍ× */
        exring[cid].unit[i].lmm.mw0[3] = exring[cid].unit[i].mwmux[3]; /* alignÉÔÍ× */
        break;
      case OP_STWR:
        exring[cid].unit[i].lmm.mm0 = (exring[cid].unit[i].lmm.mm0 & 0xffffff00) | (((exring[cid].unit[i].ea03dr/sizeof(Ull) & (UNIT_WIDTH-1))==0 && (exring[cid].unit[i].cx3dr&1)) ? 0x0000000f<<(exring[cid].unit[i].ea03dr & sizeof(int)) : 0x00000000);
        exring[cid].unit[i].lmm.mm0 = (exring[cid].unit[i].lmm.mm0 & 0xffff00ff) | (((exring[cid].unit[i].ea03dr/sizeof(Ull) & (UNIT_WIDTH-1))==1 && (exring[cid].unit[i].cx3dr&1)) ? 0x00000f00<<(exring[cid].unit[i].ea03dr & sizeof(int)) : 0x00000000);
        exring[cid].unit[i].lmm.mm0 = (exring[cid].unit[i].lmm.mm0 & 0xff00ffff) | (((exring[cid].unit[i].ea03dr/sizeof(Ull) & (UNIT_WIDTH-1))==2 && (exring[cid].unit[i].cx3dr&1)) ? 0x000f0000<<(exring[cid].unit[i].ea03dr & sizeof(int)) : 0x00000000);
        exring[cid].unit[i].lmm.mm0 = (exring[cid].unit[i].lmm.mm0 & 0x00ffffff) | (((exring[cid].unit[i].ea03dr/sizeof(Ull) & (UNIT_WIDTH-1))==3 && (exring[cid].unit[i].cx3dr&1)) ? 0x0f000000<<(exring[cid].unit[i].ea03dr & sizeof(int)) : 0x00000000);
        exring[cid].unit[i].lmm.mw0[0] = exring[cid].unit[i].mwmux[0]<<(exring[cid].unit[i].ea03dr & sizeof(int))*8; /* alignÉ¬Í× */
        exring[cid].unit[i].lmm.mw0[1] = exring[cid].unit[i].mwmux[1]<<(exring[cid].unit[i].ea03dr & sizeof(int))*8; /* alignÉ¬Í× */
        exring[cid].unit[i].lmm.mw0[2] = exring[cid].unit[i].mwmux[2]<<(exring[cid].unit[i].ea03dr & sizeof(int))*8; /* alignÉ¬Í× */
        exring[cid].unit[i].lmm.mw0[3] = exring[cid].unit[i].mwmux[3]<<(exring[cid].unit[i].ea03dr & sizeof(int))*8; /* alignÉ¬Í× */
        break;
#if 0
      case OP_STHR:
        exring[cid].unit[i].lmm.mm0 = (exring[cid].unit[i].lmm.mm0 & 0xffffff00) | (((exring[cid].unit[i].ea03dr/sizeof(Ull) & (UNIT_WIDTH-1))==0 && (exring[cid].unit[i].cx3dr&1)) ? 0x00000003<<(exring[cid].unit[i].ea03dr & (sizeof(int)|sizeof(short))) : 0x00000000);
        exring[cid].unit[i].lmm.mm0 = (exring[cid].unit[i].lmm.mm0 & 0xffff00ff) | (((exring[cid].unit[i].ea03dr/sizeof(Ull) & (UNIT_WIDTH-1))==1 && (exring[cid].unit[i].cx3dr&1)) ? 0x00000300<<(exring[cid].unit[i].ea03dr & (sizeof(int)|sizeof(short))) : 0x00000000);
        exring[cid].unit[i].lmm.mm0 = (exring[cid].unit[i].lmm.mm0 & 0xff00ffff) | (((exring[cid].unit[i].ea03dr/sizeof(Ull) & (UNIT_WIDTH-1))==2 && (exring[cid].unit[i].cx3dr&1)) ? 0x00030000<<(exring[cid].unit[i].ea03dr & (sizeof(int)|sizeof(short))) : 0x00000000);
        exring[cid].unit[i].lmm.mm0 = (exring[cid].unit[i].lmm.mm0 & 0x00ffffff) | (((exring[cid].unit[i].ea03dr/sizeof(Ull) & (UNIT_WIDTH-1))==3 && (exring[cid].unit[i].cx3dr&1)) ? 0x03000000<<(exring[cid].unit[i].ea03dr & (sizeof(int)|sizeof(short))) : 0x00000000);
        exring[cid].unit[i].lmm.mw0[0] = exring[cid].unit[i].mwmux[0]<<(exring[cid].unit[i].ea03dr & (sizeof(int)|sizeof(short)))*8; /* alignÉ¬Í× */
        exring[cid].unit[i].lmm.mw0[1] = exring[cid].unit[i].mwmux[1]<<(exring[cid].unit[i].ea03dr & (sizeof(int)|sizeof(short)))*8; /* alignÉ¬Í× */
        exring[cid].unit[i].lmm.mw0[2] = exring[cid].unit[i].mwmux[2]<<(exring[cid].unit[i].ea03dr & (sizeof(int)|sizeof(short)))*8; /* alignÉ¬Í× */
        exring[cid].unit[i].lmm.mw0[3] = exring[cid].unit[i].mwmux[3]<<(exring[cid].unit[i].ea03dr & (sizeof(int)|sizeof(short)))*8; /* alignÉ¬Í× */
        break;
#endif
      case OP_STBR:
        if (exring[cid].unit[i].lmea0sfma) { /* same */	}
	else                               { /* same */ }
	exring[cid].unit[i].lmm.mm0 = (exring[cid].unit[i].lmm.mm0 & 0xffffff00) | (((exring[cid].unit[i].ea03dr/sizeof(Ull) & (UNIT_WIDTH-1))==0 && (exring[cid].unit[i].cx3dr&1)) ? 0x00000001<<(exring[cid].unit[i].ea03dr & (sizeof(int)|sizeof(short)|sizeof(char))) : 0x00000000);
	exring[cid].unit[i].lmm.mm0 = (exring[cid].unit[i].lmm.mm0 & 0xffff00ff) | (((exring[cid].unit[i].ea03dr/sizeof(Ull) & (UNIT_WIDTH-1))==1 && (exring[cid].unit[i].cx3dr&1)) ? 0x00000100<<(exring[cid].unit[i].ea03dr & (sizeof(int)|sizeof(short)|sizeof(char))) : 0x00000000);
	exring[cid].unit[i].lmm.mm0 = (exring[cid].unit[i].lmm.mm0 & 0xff00ffff) | (((exring[cid].unit[i].ea03dr/sizeof(Ull) & (UNIT_WIDTH-1))==2 && (exring[cid].unit[i].cx3dr&1)) ? 0x00010000<<(exring[cid].unit[i].ea03dr & (sizeof(int)|sizeof(short)|sizeof(char))) : 0x00000000);
	exring[cid].unit[i].lmm.mm0 = (exring[cid].unit[i].lmm.mm0 & 0x00ffffff) | (((exring[cid].unit[i].ea03dr/sizeof(Ull) & (UNIT_WIDTH-1))==3 && (exring[cid].unit[i].cx3dr&1)) ? 0x01000000<<(exring[cid].unit[i].ea03dr & (sizeof(int)|sizeof(short)|sizeof(char))) : 0x00000000);
	exring[cid].unit[i].lmm.mw0[0] = exring[cid].unit[i].mwmux[0]<<(exring[cid].unit[i].ea03dr & (sizeof(int)|sizeof(short)|sizeof(char)))*8; /* alignÉ¬Í× */
	exring[cid].unit[i].lmm.mw0[1] = exring[cid].unit[i].mwmux[1]<<(exring[cid].unit[i].ea03dr & (sizeof(int)|sizeof(short)|sizeof(char)))*8; /* alignÉ¬Í× */
	exring[cid].unit[i].lmm.mw0[2] = exring[cid].unit[i].mwmux[2]<<(exring[cid].unit[i].ea03dr & (sizeof(int)|sizeof(short)|sizeof(char)))*8; /* alignÉ¬Í× */
	exring[cid].unit[i].lmm.mw0[3] = exring[cid].unit[i].mwmux[3]<<(exring[cid].unit[i].ea03dr & (sizeof(int)|sizeof(short)|sizeof(char)))*8; /* alignÉ¬Í× */
        break;
      case OP_IM_PREF:
      case OP_IM_BUFWR:
        exring[cid].unit[i].lmm.mm0    = 0xffffffff;         /* mask *//* »ÃÄêµ­½Ò */
        exring[cid].unit[i].lmm.mw0[0] = exring[cid].unit[i].mwmux[0]; /* »ÃÄêµ­½Ò */
        exring[cid].unit[i].lmm.mw0[1] = exring[cid].unit[i].mwmux[1]; /* »ÃÄêµ­½Ò */
        exring[cid].unit[i].lmm.mw0[2] = exring[cid].unit[i].mwmux[2]; /* »ÃÄêµ­½Ò */
        exring[cid].unit[i].lmm.mw0[3] = exring[cid].unit[i].mwmux[3]; /* »ÃÄêµ­½Ò */
        break;
      case OP_STRQ: /* OP_STRQ in target   slot */
        if (exring[cid].unit[i].lmea0strq) {
          /* STRQ¤ÏSTR¤ò4¥µ¥¤¥¯¥ëÊ¬³ä¼Â¹Ô */
          exring[cid].unit[i].lmm.mm0 = 0x000000ff << (j<<3);
          exring[cid].unit[i].lmm.mw0[j] = exring[cid].unit[i].mwmux[j]; /* alignÉÔÍ× */
        }
        break;
      default:
        break;
      }
      /*printf("===BUFWR write row=%d col=%d ea1d=%08.8x ea0d=%08.8x\n", i, j, (Uint)exring[cid].unit[i][j].ea1d, (Uint)exring[cid].unit[i][j].ea0d);*/
    }
    else
      exring[cid].unit[i].lmm.en0     = 0;

    /* (NOP | EXEC) & !deq_wait & lmranger_ok¤Î»þ lmra->ea14dr */
    /* lmring_ea1bsy                         ¤Î»þ,ea1d->ea14dr */
    exring[cid].unit[i].ea14woofs_prev = exring[cid].unit[i].ea14woofs; /* ¡úsiml-loop only */
    exring[cid].unit[i].ea14woofs      = exring[cid].unit[i].ea13woofs; /* ¡ú */
    if ((exring[cid].unit[i].cmd == CMD_NOP || exring[cid].unit[i].cmd == CMD_EXEC)
      && ful2 && !deq_wait && exring[cid].unit[i].lmranger_ok) { /* axi->lmm_read */
      /* LMEM_SIZE(128KB)/4 = 32KB... lmm_mode=1:adr=(block=col&0)*(LMEM_SIZE/4) | (ofs=adr&(LMEM_SIZE/1-1)) */
      /*                              lmm_mode=2:adr=(block=col&2)*(LMEM_SIZE/4) | (ofs=adr&(LMEM_SIZE/2-1)) */
      /*                              lmm_mode=3:adr=(block=col&3)*(LMEM_SIZE/4) | (ofs=adr&(LMEM_SIZE/4-1)) */
      switch (reg_ctrl.i[cid].conf[i][exring[cid].unit[i].lmco].cdw2.lmm_mode) {
      case 0: exring[cid].unit[i].ea14_umask = LMEM_UMASK0; exring[cid].unit[i].ea14_lmask = LMEM_LMASK0; break;
      case 1: exring[cid].unit[i].ea14_umask = LMEM_UMASK1; exring[cid].unit[i].ea14_lmask = LMEM_LMASK1; break;
      case 2: exring[cid].unit[i].ea14_umask = LMEM_UMASK2; exring[cid].unit[i].ea14_lmask = LMEM_LMASK2; break;
      case 3: exring[cid].unit[i].ea14_umask = LMEM_UMASK3; exring[cid].unit[i].ea14_lmask = LMEM_LMASK3; break;
      }
      exring[cid].unit[i].ea14dr      = ((exring[cid].unit[i].lmco & exring[cid].unit[i].ea14_umask) * (LMEM_SIZE/4)) | (exring[cid].unit[i].lmca & exring[cid].unit[i].ea14_lmask);
      exring[cid].unit[i].lmm.en1     = 1;
      exring[cid].unit[i].lmm.rw1     = 0; /* read */
      exring[cid].unit[i].lmm.ma1     = (exring[cid].unit[i].ea14dr % LMEM_SIZE) & ~(sizeof(Ull)*UNIT_WIDTH-1);
#if 0
printf("====== RLMM row=%x br.col=%x ====axir=%x axiw=%x br->rw=%x br->ty=%x a=%08.8x lmranger_ok=%x lmrangew_ok=%x top=%08.8x bot=%08.8x\n", i, br->col, reg_ctrl.i[cid].conf[i][br->col].cdw2.lmm_axir, reg_ctrl.i[cid].conf[i][br->col].cdw2.lmm_axiw, br->rw, br->ty, br->a, exring[cid].unit[i].lmranger_ok, exring[cid].unit[i].lmrangew_ok, reg_ctrl.i[cid].addr[i][br->col].top, reg_ctrl.i[cid].addr[i][br->col].bot);
#endif
    }
    else if (exring[cid].unit[i].lmring_ea1bsy) { /* EXEC & ea1d->lmm_read */
      int kmex1;
      /* ¡ú¡ú¡ú¡ú¡ú¡ú¡ú¡ú¡ú¡ú¡úMEX1ÍÑ¤ÎADDR_MASK */
      /* conf[][col=2/3]mex1op¤Ï,conf[][col=2/3].lmm_mode¤ò»²¾È */
      /* conf[][col=1  ]mex1op¤Ï,conf[][col=0  ].lmm_mode¤ò»²¾È */
      kmex1 = ((reg_ctrl.i[cid].conf[i][2].cdw0.mex0op==OP_CMPA_LE || reg_ctrl.i[cid].conf[i][2].cdw0.mex0op==OP_CMPA_GE) && reg_ctrl.i[cid].conf[i][j].cdw1.ea1op < OP_IM_BUFRD) ? ((j==1||j==0)?0:j) : j; /* ¡ú */
      /* ¡ú¡ú¡ú¡ú¡ú¡ú¡ú¡ú¡ú¡ú¡úMEX1ÍÑ¤ÎLD */
      /* if (i==2 && j==2) printf("[2][2] mexop1=%d\n", reg_ctrl.i[cid].conf[i][j].cdw0.mex1op); */
      switch (reg_ctrl.i[cid].conf[i][kmex1].cdw2.lmm_mode) {
      case 0: exring[cid].unit[i].ea14_umask = LMEM_UMASK0; exring[cid].unit[i].ea14_lmask = LMEM_LMASK0; break;
      case 1: exring[cid].unit[i].ea14_umask = LMEM_UMASK1; exring[cid].unit[i].ea14_lmask = LMEM_LMASK1; break;
      case 2: exring[cid].unit[i].ea14_umask = LMEM_UMASK2; exring[cid].unit[i].ea14_lmask = LMEM_LMASK2; break;
      case 3: exring[cid].unit[i].ea14_umask = LMEM_UMASK3; exring[cid].unit[i].ea14_lmask = LMEM_LMASK3; break;
      }
      exring[cid].unit[i].ea14dr   = ((kmex1 & exring[cid].unit[i].ea14_umask) * (LMEM_SIZE/4)) | (exring[cid].unit[i].ea13dr & exring[cid].unit[i].ea14_lmask);
      /* if (i==2 && k==2) printf("ea14dr=%08.8x\n", (Uint)exring[cid].unit[i].ea14dr); */
      exring[cid].unit[i].lmm.en1  = 1;
      exring[cid].unit[i].lmm.rw1  = 0; /* read */
      exring[cid].unit[i].lmm.ma1  = (exring[cid].unit[i].ea14dr % LMEM_SIZE) & ~(sizeof(Ull)*UNIT_WIDTH-1);
      /*printf("===NLOAD read row=%d col=%d ea1d=%08.8x ea0d=%08.8x\n", i, j, (Uint)exring[cid].unit[i][j].ea1d, (Uint)exring[cid].unit[i][j].ea0d);*/
    }
    else
      exring[cid].unit[i].lmm.en1  = 0;

    /* lmwd/tx3dr -> tx4dr */
    if (exring[cid].unit[i].stage4_exec) { /* active */
      /* 0:lmwd0, 1:exdr, 2:ts0 *//* 0:TR³°Éô½ñ¤­¹þ¤ßÍÑ, 1,2:EX/TS½ñ¤­¹þ¤ßÍÑ */
      if (reg_ctrl.i[cid].conf[i][j].cdw1.ea1op == OP_LDDMQ) {
        if (exring[cid].unit[i].lmlddmqw_ok && br->col == j) {
          exring[cid].unit[i].tx4dr[0] = exring[cid].unit[i].lmwd[0];
          exring[cid].unit[i].tx4dr[1] = exring[cid].unit[i].lmwd[1];
          exring[cid].unit[i].tx4dr[2] = exring[cid].unit[i].lmwd[2];
          exring[cid].unit[i].tx4dr[3] = exring[cid].unit[i].lmwd[3];
          exring[cid].unit[i].tr_valid = 1;
        }
        else
          exring[cid].unit[i].tr_valid = 0; /* no data for OP_LDDMQ */
      }
      else { /* trs==0¤Î¾ì¹ç,lmwd->tr¤À¤¬,LDDMQ°Ê³°¤Ë»ÈÍÑ¤¹¤ë¥±¡¼¥¹¤Ï¤Ê¤¤ */
        exring[cid].unit[i].tx4dr[0] = reg_ctrl.i[cid].conf[i][j].cdw2.trs0==1 ? exring[cid].unit[i].ex4dr : exring[cid].unit[i].tx3dr[0]; /* ex3dr->ex4dr for SFMR+STBR */
        exring[cid].unit[i].tx4dr[1] = reg_ctrl.i[cid].conf[i][j].cdw2.trs1==1 ? exring[cid].unit[i].ex4dr : exring[cid].unit[i].tx3dr[1]; /* ex3dr->ex4dr for SFMR+STBR */
        exring[cid].unit[i].tx4dr[2] = reg_ctrl.i[cid].conf[i][j].cdw2.trs2==1 ? exring[cid].unit[i].ex4dr : exring[cid].unit[i].tx3dr[2]; /* ex3dr->ex4dr for SFMR+STBR */
        exring[cid].unit[i].tx4dr[3] = reg_ctrl.i[cid].conf[i][j].cdw2.trs3==1 ? exring[cid].unit[i].ex4dr : exring[cid].unit[i].tx3dr[3]; /* ex3dr->ex4dr for SFMR+STBR */
        exring[cid].unit[i].tr_valid = 1;
      }
    }
  }

  return (0);
}

siml_unit_stage5(Uint cid, Uint i) /* stage-5 (3DR->BROUT)(LMRING_TR->LMRING_BROUT) */
{
  /******************************************************************************************************************************************************************************************/
  /*      |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       | */
  /* clk _/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/ */
  /* cycle  7       0       1       2       3       4       5       6       7       0       1       2       3       4       5       6       7       0       1       2       3       4       */
  /* reg_ctrl.cmd -----------------<==EXEC=========X==NOP>================================================================================================================================= */
  /*              V-update                        V-update                        V-update                        V-update                        V-update                        V-update  */
  /* cmd         ----------------------------------<==EXEC=========================================================================================================================>------- */
  /* one_shot    __________________________________________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______ */
  /* unit1_exec  __________________________________/~~(l_row==0)~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________________________________ */
  /* unit1_stop  __________________________________________________________________________________________________________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______ */
  /* reg_ctrl.stat---------------------------------<==EXRING_BUSY==================================================================================>--------------------------------------- */

  /* reg_ctrl.cmd -----------------<==SCON=========X==NOP>================================================================================================================================= */
  /*              V-update                        V-update                        V-update                        V-update                        V-update                        V-update  */
  /* cmd         ----------------------------------<==SCON=========================================================================================>--------------------------------------- */
  /* scon_count  ----------------------------------<==conf.mapdist*2===============X==conf.mapdist*2-1=============X==0============================>======================================= */
  /*                                                      Ascon2  Ascon2  Ascon2  Ascon2  Ascon1  Ascon1  Ascon1  Ascon1                                                                    */
  /*                                                      ->br0   ->br1   ->br2   ->br3   br0->   br1->   br2->   br3->                                                                     */
  /* unit1_exec  __________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________________________________________________________________ */
  /* unit1_stop  __________________________________________________________________________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________________________________ */
  /* stage1out** --------------------------------------------------------------------------<== 0===X== 1===X== 2===X== 3===>--------------------------------------------------------------- */
  /* stage4out** ------------------------------------------<== 0===X== 1===X== 2===X== 3===>----------------------------------------------------------------------------------------------- */
  /* reg_ctrl.stat---------------------------------<==EXRING_BUSY==================================================>----------------------------------------------------------------------- */

  /******************************************************************************************************************************************************************************************/
  /*      |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       |       | */
  /* clk _/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/~~~\___/ */
  /* cycle  7       0       1       2       3       4       5       6       7       0       1       2       3       4       5       6       7       0       1       2       3       4       */
  /*              V-update                        V-update                        V-update                        V-update                        V-update                        V-update  */
  /* brout_valid __/~~(l_row==row0-1)~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

  /* ---- row[i]--------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */
  /*              V-update                        V-update                        V-update                        V-update                        V-update                        V-update  */
  /* cmd         --<==EXEC=========================================================================================================================================================>------- */
  /* one_shot    __________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______ */
  /* unit1_exec  __/~~(l_row==0)~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______________________________________ */
  /* unit1_stop  __________________________________________________________________________________________________________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______ */
  /* stage1out** ----------<== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3==> */
  /* stage2out   ------------------<== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2==> */
  /* stage3out   --------------------------<== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1==> */
  /* stage4out   -------tx4dr----------------------<== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0==> */
  /* tr_valid    __________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\_______ */
  /* stage5out   -------brout------------------------------<==p0===X==p1===X==p2===X==p3===X==q0===X==q1===X==q2===X==q3===X==q0===X==q1===X==q2===X==q3===X==q0===X==q1===X==q2===X==q3==> */
  /*              V-update                        V-update                        V-update                        V-update                        V-update                        V-update  */
  /* brout_valid __________________________________________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

  /* ---- row[i+1]------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */
  /*              V-update                        V-update                        V-update                        V-update                        V-update                        V-update  */
  /* cmd         --<==EXEC=========================================================================================================================================================>------- */
  /* one_shot    __________________________________________________________________________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
  /* unit1_exec  __________________________________________________________________/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
  /* stage1out   --------------------------------------------------------------------------<== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3===X== 0===X== 1===X== 2===X== 3==> */

  int ni = (i+1)%EMAX_DEPTH;
  int b  =~(exring[cid].unit[i].cycle / EMAX_WIDTH)&1; /* 1,1,1,1,0,0,0,0 */
  int j  =  exring[cid].unit[i].cycle % EMAX_WIDTH;    /* 0,1,2,3,0,1,2,3 */
  int k;
  struct lmring_tr *tr = &exring[cid].unit[i].lmring_tr;
  struct lmring_br *br = &exring[cid].unit[i].lmring_br[exring[cid].unit[i].lmring_b_top];
  int          br_ful2 = exring[cid].unit[i].lmring_ful2;
  int         deq_wait = ((i+1)%(EMAX_DEPTH/LMRING_MUX)==0)?axiif[cid].deq_wait           /* axi  deq_wait */
                                                           :exring[cid].unit[ni].deq_wait;/* next deq_wait */
  int cdx = tr->a / (REG_AREA_MASK+1) & (EMAX_NCHIP-1);               /* for registers */
  int idx = tr->a & (sizeof(Ull)*UNIT_WIDTH*EMAX_WIDTH*EMAX_DEPTH-1); /* for registers */
  int row = idx/(sizeof(Ull)*UNIT_WIDTH*EMAX_WIDTH); /* max8K/128B=0........63 *//* 64 rows */
  int ofs = idx%(sizeof(Ull)*UNIT_WIDTH*EMAX_WIDTH); /* max8K%128B=0,8,...,120 *//* 16 regs */

  Ull a0, a1, d;

  /* LMRING_BR LMRD BR */
#if 0
  Ull   mr0mux        : 2; /* mr0[3-0] -> brs0     */
  Ull   mr1mux        : 2; /* mr1[3-0] -> brs1     */
  Ull   mr0d             ; /* muxed data for BR[0] */
  Ull   mr1d             ; /* muxed data for BR[1] */
  Ull   lmrd[UNIT_WIDTH] ; /* wire *//* -> axi   */
  Ull   brout_valid   : 1; /* BR¤Î¾õÂÖ¤òÉ½¼¨ ¾ï»þtr_valid¤ò1¦Ó¸å¤ËÅÁÈÂ */
  struct {Ull r[UNIT_WIDTH];} b[2][EMAX_WIDTH]; /* shadow_breg *//* constant¤ÏÎ¾Êý¤Ë¥»¥Ã¥È */

  Ull   lmring_ful2   : 2; /* 0:empty, 3:full */
  Ull   lmring_b_top  : 2; /* to be enqueued next */
  Ull   lmring_b_bot  : 2; /* to be dequeued next */
  struct lmring_br {
    Ull   rw          : 1; /* 0:read, 1:write */
    Ull   ty          : 3; /* 0:reg/conf, 1:reg/breg, 2:reg/addr, 3:lddmq/tr, 4:lmm, 567:-- */
    Ull   col         : 2; /* logical col# for target lmm */
    Ull   sq          :16; /* sequential # for pipelined DMA */
    Ull   av          : 1; /* address valid */
    Ull   a           :31; /* logical addr reg/lmm */
    Ull   dm          :32; /* <- lmm wdata       */
    Ull   d[UNIT_WIDTH]  ; /* <- lmm wdata/rdata */
  } lmring_br[LMRING_BR_BUF];
#endif

  a0   = exring[cid].unit[i].ea04dr;  /* not aligned (lower bits are user for load-select) */
  a1   = exring[cid].unit[i].ea14dr;  /* not aligned (lower bits are user for load-select) */
  exring[cid].unit[i].mr0mux = (a0/sizeof(Ull) & (UNIT_WIDTH-1)); /* mr0[3-0] -> mr0d */
  exring[cid].unit[i].mr0d   = exring[cid].unit[i].lmm.mr0[exring[cid].unit[i].mr0mux];
  exring[cid].unit[i].mr1mux = (a1/sizeof(Ull) & (UNIT_WIDTH-1)); /* mr1[3-0] -> mr0d */
  exring[cid].unit[i].mr1d   = exring[cid].unit[i].lmm.mr1[exring[cid].unit[i].mr1mux];

  exring[cid].unit[i].mexmr0d_prev = exring[cid].unit[i].mexmr0d; /* ¡ú */
  exring[cid].unit[i].mexmr0d      = exring[cid].unit[i].mr0d;    /* ¡ú */
  exring[cid].unit[i].mexmr1d_prev = exring[cid].unit[i].mexmr1d; /* ¡ú */
  exring[cid].unit[i].mexmr1d      = exring[cid].unit[i].mr1d;    /* ¡ú */

  for (k=0; k<UNIT_WIDTH; k++)
    exring[cid].unit[i].lmrd[k] = exring[cid].unit[i].lmm.mr1[k];

  if (exring[cid].unit[i].cmd == CMD_NOP
      && (exring[cid].unit[i].cycle&3) == 3
      && (reg_ctrl.i[cid].cmd&3) == CMD_RESET) {
    exring[cid].unit[i].brout_valid      = 0;
    for (k=0; k<EMAX_WIDTH; k++) {
      exring[cid].unit[i].b[0][k].r[0]   = 0LL;
      exring[cid].unit[i].b[0][k].r[1]   = 0LL;
      exring[cid].unit[i].b[0][k].r[2]   = 0LL;
      exring[cid].unit[i].b[0][k].r[3]   = 0LL;
      exring[cid].unit[i].b[1][k].r[0]   = 0LL;
      exring[cid].unit[i].b[1][k].r[1]   = 0LL;
      exring[cid].unit[i].b[1][k].r[2]   = 0LL;
      exring[cid].unit[i].b[1][k].r[3]   = 0LL;
    }
  }
  else if (exring[cid].unit[i].cmd == CMD_NOP && ((reg_ctrl.i[cid].cmd>>16) == cdx || cdx == 0) && exring[cid].unit[i].l_row == row && exring[cid].unit[i].lmring_ful1) { /* reg setup by PIO */
#if 0
if (tr->ty<4)
printf("====== WREG chip=%d cdx=%d l_row=%x row=%x cmd=%d ==== tr->ty=%x a=%08.8x dm=%08.8x d=%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x\n", (Uint)(reg_ctrl.i[cid].cmd>>16), cdx, exring[cid].unit[i].l_row, row, exring[cid].unit[i].cmd&3, tr->ty, tr->a, tr->dm, (Uint)(tr->d[3]>>32), (Uint)(tr->d[3]), (Uint)(tr->d[2]>>32), (Uint)(tr->d[2]), (Uint)(tr->d[1]>>32), (Uint)(tr->d[1]), (Uint)(tr->d[0]>>32), (Uint)(tr->d[0]));
#endif
    switch (tr->ty) {
    case 0: /* reg/conf */
      if      (tr->dm & 0x0000000f) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].conf[i]+ofs)+0) = tr->d[0];     }
      if      (tr->dm & 0x000000f0) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].conf[i]+ofs)+1) = tr->d[0]>>32; }
      if      (tr->dm & 0x00000f00) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].conf[i]+ofs)+2) = tr->d[1];     }
      if      (tr->dm & 0x0000f000) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].conf[i]+ofs)+3) = tr->d[1]>>32; }
      if      (tr->dm & 0x000f0000) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].conf[i]+ofs)+4) = tr->d[2];     }
      if      (tr->dm & 0x00f00000) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].conf[i]+ofs)+5) = tr->d[2]>>32; }
      if      (tr->dm & 0x0f000000) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].conf[i]+ofs)+6) = tr->d[3];     }
      if      (tr->dm & 0xf0000000) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].conf[i]+ofs)+7) = tr->d[3]>>32; }
      break;
    case 1: /* reg/breg */
      if      (tr->dm & 0x0000000f) { *((Uint*)((Uchar*)&exring[cid].unit[i].b[0]+ofs)+0) = tr->d[0];     *((Uint*)((Uchar*)&exring[cid].unit[i].b[1]+ofs)+0) = tr->d[0];     }
      if      (tr->dm & 0x000000f0) { *((Uint*)((Uchar*)&exring[cid].unit[i].b[0]+ofs)+1) = tr->d[0]>>32; *((Uint*)((Uchar*)&exring[cid].unit[i].b[1]+ofs)+1) = tr->d[0]>>32; }
      if      (tr->dm & 0x00000f00) { *((Uint*)((Uchar*)&exring[cid].unit[i].b[0]+ofs)+2) = tr->d[1];     *((Uint*)((Uchar*)&exring[cid].unit[i].b[1]+ofs)+2) = tr->d[1];     }
      if      (tr->dm & 0x0000f000) { *((Uint*)((Uchar*)&exring[cid].unit[i].b[0]+ofs)+3) = tr->d[1]>>32; *((Uint*)((Uchar*)&exring[cid].unit[i].b[1]+ofs)+3) = tr->d[1]>>32; }
      if      (tr->dm & 0x000f0000) { *((Uint*)((Uchar*)&exring[cid].unit[i].b[0]+ofs)+4) = tr->d[2];     *((Uint*)((Uchar*)&exring[cid].unit[i].b[1]+ofs)+4) = tr->d[2];     }
      if      (tr->dm & 0x00f00000) { *((Uint*)((Uchar*)&exring[cid].unit[i].b[0]+ofs)+5) = tr->d[2]>>32; *((Uint*)((Uchar*)&exring[cid].unit[i].b[1]+ofs)+5) = tr->d[2]>>32; }
      if      (tr->dm & 0x0f000000) { *((Uint*)((Uchar*)&exring[cid].unit[i].b[0]+ofs)+6) = tr->d[3];     *((Uint*)((Uchar*)&exring[cid].unit[i].b[1]+ofs)+6) = tr->d[3];     }
      if      (tr->dm & 0xf0000000) { *((Uint*)((Uchar*)&exring[cid].unit[i].b[0]+ofs)+7) = tr->d[3]>>32; *((Uint*)((Uchar*)&exring[cid].unit[i].b[1]+ofs)+7) = tr->d[3]>>32; }
      break;
    case 2: /* reg/addr */
      if      (tr->dm & 0x0000000f) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].addr[i]+ofs)+0) = tr->d[0];     }
      if      (tr->dm & 0x000000f0) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].addr[i]+ofs)+1) = tr->d[0]>>32; }
      if      (tr->dm & 0x00000f00) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].addr[i]+ofs)+2) = tr->d[1];     }
      if      (tr->dm & 0x0000f000) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].addr[i]+ofs)+3) = tr->d[1]>>32; }
      if      (tr->dm & 0x000f0000) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].addr[i]+ofs)+4) = tr->d[2];     }
      if      (tr->dm & 0x00f00000) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].addr[i]+ofs)+5) = tr->d[2]>>32; }
      if      (tr->dm & 0x0f000000) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].addr[i]+ofs)+6) = tr->d[3];     }
      if      (tr->dm & 0xf0000000) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].addr[i]+ofs)+7) = tr->d[3]>>32; }
      break;
    case 3: /* lddmq/tr */
      if      (tr->dm & 0x000000ff) { *((Ull *)((Uchar*)&reg_ctrl.i[cid].lddmrw[i]+ofs)+0) = tr->d[0];   }/* only write is implemented */
      if      (tr->dm & 0x0000ff00) { *((Ull *)((Uchar*)&reg_ctrl.i[cid].lddmrw[i]+ofs)+1) = tr->d[1];   }/* only write is implemented */
      if      (tr->dm & 0x00ff0000) { *((Ull *)((Uchar*)&reg_ctrl.i[cid].lddmrw[i]+ofs)+2) = tr->d[2];   }/* only write is implemented */
      if      (tr->dm & 0xff000000) { *((Ull *)((Uchar*)&reg_ctrl.i[cid].lddmrw[i]+ofs)+3) = tr->d[3];   }/* only write is implemented */
      break;
    }
  }
  else if (exring[cid].unit[i].cmd == CMD_SCON) {
    if (exring[cid].unit[i].unit1_exec && !(exring[cid].unit[i].scon_count & 1)) {
      exring[cid].unit[i].b[0][j].r[0] = *((Ull*)(&reg_ctrl.i[cid].conf[i][j])+0);
      exring[cid].unit[i].b[0][j].r[1] = *((Ull*)(&reg_ctrl.i[cid].conf[i][j])+1);
      exring[cid].unit[i].b[0][j].r[2] = *((Ull*)(&reg_ctrl.i[cid].conf[i][j])+2);
      exring[cid].unit[i].b[0][j].r[3] = *((Ull*)(&reg_ctrl.i[cid].conf[i][j])+3);
    }
  }
  else if (exring[cid].unit[i].cmd == CMD_EXEC) {
    if (((reg_ctrl.i[cid].cmd>>16) == cdx || cdx == 0) && exring[cid].unit[i].l_row == row && exring[cid].unit[i].lmring_ful1) { /* reg setup by PIO */
#if 0
if (tr->ty<4)
printf("====== WREG chip=%d cdx=%d l_row=%x row=%x cmd=%d ==== tr->ty=%x a=%08.8x dm=%08.8x d=%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x\n", (Uint)(reg_ctrl.i[cid].cmd>>16), cdx, exring[cid].unit[i].l_row, row, exring[cid].unit[i].cmd&3, tr->ty, tr->a, tr->dm, (Uint)(tr->d[3]>>32), (Uint)(tr->d[3]), (Uint)(tr->d[2]>>32), (Uint)(tr->d[2]), (Uint)(tr->d[1]>>32), (Uint)(tr->d[1]), (Uint)(tr->d[0]>>32), (Uint)(tr->d[0]));
#endif
      switch (tr->ty) {
      case 2: /* reg/addr */
        if      (tr->dm & 0x0000000f) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].addr[i]+ofs)+0) = tr->d[0];     }
        if      (tr->dm & 0x000000f0) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].addr[i]+ofs)+1) = tr->d[0]>>32; }
        if      (tr->dm & 0x00000f00) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].addr[i]+ofs)+2) = tr->d[1];     }
        if      (tr->dm & 0x0000f000) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].addr[i]+ofs)+3) = tr->d[1]>>32; }
        if      (tr->dm & 0x000f0000) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].addr[i]+ofs)+4) = tr->d[2];     }
        if      (tr->dm & 0x00f00000) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].addr[i]+ofs)+5) = tr->d[2]>>32; }
        if      (tr->dm & 0x0f000000) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].addr[i]+ofs)+6) = tr->d[3];     }
        if      (tr->dm & 0xf0000000) { *((Uint*)((Uchar*)&reg_ctrl.i[cid].addr[i]+ofs)+7) = tr->d[3]>>32; }
        break;
      case 3: /* lddmq/tr */
        if      (tr->dm & 0x000000ff) { *((Ull *)((Uchar*)&reg_ctrl.i[cid].lddmrw[i]+ofs)+0) = tr->d[0];   }/* only write is implemented */
        if      (tr->dm & 0x0000ff00) { *((Ull *)((Uchar*)&reg_ctrl.i[cid].lddmrw[i]+ofs)+1) = tr->d[1];   }/* only write is implemented */
        if      (tr->dm & 0x00ff0000) { *((Ull *)((Uchar*)&reg_ctrl.i[cid].lddmrw[i]+ofs)+2) = tr->d[2];   }/* only write is implemented */
        if      (tr->dm & 0xff000000) { *((Ull *)((Uchar*)&reg_ctrl.i[cid].lddmrw[i]+ofs)+3) = tr->d[3];   }/* only write is implemented */
        break;
      }
    }
    /* load mr -> br0 *//* brs0: 0:off, 1:mr10, 2:tr0, 3:mr0 */
    switch (reg_ctrl.i[cid].conf[i][j].cdw2.brs0) {
    case 0:                                                                    break; /* off */
    case 1: exring[cid].unit[i].b[b][j].r[0] = exring[cid].unit[i].lmm.mr1[0]; break; /* 1:mr10 alignÉÔÍ× */
    case 2: exring[cid].unit[i].b[b][j].r[0] = exring[cid].unit[i].tx4dr[0];   break; /* 2:tr0  alignÉÔÍ× */
    case 3:                                                                           /* 3:mr0  alignÉ¬Í× */
      switch (reg_ctrl.i[cid].conf[i][j].cdw1.ea0op) {
      case OP_LDR :
	switch (a0&7) { /* unaligned load */
	case 0:      exring[cid].unit[i].b[b][j].r[0] =                                            exring[cid].unit[i].mr0d;             break;
	default:     exring[cid].unit[i].b[b][j].r[0] = exring[cid].unit[i].mr1d << (8-(a0&7))*8 | exring[cid].unit[i].mr0d >> (a0&7)*8; break;
	}
	break;
      case OP_LDWR:  exring[cid].unit[i].b[b][j].r[0] = (Ull)            (Uint)((exring[cid].unit[i].mr0d >> ((a0 & (sizeof(int)                           ))*8)) & 0x00000000ffffffffLL); break;
#if 0
      case OP_LDHR:  exring[cid].unit[i].b[b][j].r[0] = (Ull)(Uint)    (Ushort)((exring[cid].unit[i].mr0d >> ((a0 & (sizeof(int)|sizeof(short)             ))*8)) & 0x000000000000ffffLL); break;
#endif
      case OP_LDBR:  exring[cid].unit[i].b[b][j].r[0] = (Ull)(Uint)     (Uchar)((exring[cid].unit[i].mr0d >> ((a0 & (sizeof(int)|sizeof(short)|sizeof(char)))*8)) & 0x00000000000000ffLL); break;
      }
    }
    /* load mr -> br1 *//* brs1: 0:off, 1:mr11, 2:tr1, 3:mr1 */
    switch (reg_ctrl.i[cid].conf[i][j].cdw2.brs1) {
    case 0:                                                                    break; /* off */
    case 1: exring[cid].unit[i].b[b][j].r[1] = exring[cid].unit[i].lmm.mr1[1]; break; /* 1:mr11 alignÉÔÍ× */
    case 2: exring[cid].unit[i].b[b][j].r[1] = exring[cid].unit[i].tx4dr[1];   break; /* 2:tr1  alignÉÔÍ× */
    case 3:                                                                           /* 3:mr1  alignÉ¬Í× */
      switch (reg_ctrl.i[cid].conf[i][j].cdw1.ea1op) {
      case OP_LDR:   
	switch (a1&7) { /* unaligned load */
	case 0:      exring[cid].unit[i].b[b][j].r[1] =                          exring[cid].unit[i].mr1d;             break;
	default:     exring[cid].unit[i].b[b][j].r[1] =                          exring[cid].unit[i].mr1d >> (a1&7)*8; break;
	}
        break;
      case OP_LDWR:  exring[cid].unit[i].b[b][j].r[1] = (Ull)            (Uint)((exring[cid].unit[i].mr1d >> ((a1 & (sizeof(int)                           ))*8)) & 0x00000000ffffffffLL); break;
#if 0
      case OP_LDHR:  exring[cid].unit[i].b[b][j].r[1] = (Ull)(Uint)    (Ushort)((exring[cid].unit[i].mr1d >> ((a1 & (sizeof(int)|sizeof(short)             ))*8)) & 0x000000000000ffffLL); break;
#endif
      case OP_LDBR:  exring[cid].unit[i].b[b][j].r[1] = (Ull)(Uint)     (Uchar)((exring[cid].unit[i].mr1d >> ((a1 & (sizeof(int)|sizeof(short)|sizeof(char)))*8)) & 0x00000000000000ffLL); break;
      }
    }
    /* load mr -> br2 *//* brs2: 0:off, 1:mr12, 2:tr2, 3:exdr    (brs3=3¤Î¾ì¹ç,ea0woofs¤ËÀÜÂ³) */
    switch (reg_ctrl.i[cid].conf[i][j].cdw2.brs2) {
    case 0:                                                                    break; /* off */
    case 1: exring[cid].unit[i].b[b][j].r[2] = exring[cid].unit[i].lmm.mr1[2]; break; /* 1:mr12 alignÉÔÍ× */
    case 2: exring[cid].unit[i].b[b][j].r[2] = exring[cid].unit[i].tx4dr[2];   break; /* 2:tr2  alignÉÔÍ× */
    case 3: if (reg_ctrl.i[cid].conf[i][j].cdw2.brs3!=3)
              exring[cid].unit[i].b[b][j].r[2] = exring[cid].unit[i].ex4dr;
            else
	      exring[cid].unit[i].b[b][j].r[2] = exring[cid].unit[i].ea04woofs;
            break; /* 3:ea0woofs */
    }
    /* load mr -> br3 *//* brs3: 0:off, 1:mr13, 2:tr3, 3:ea1woofs */
    switch (reg_ctrl.i[cid].conf[i][j].cdw2.brs3) {
    case 0:                                                                    break; /* off */
    case 1: exring[cid].unit[i].b[b][j].r[3] = exring[cid].unit[i].lmm.mr1[3]; break; /* 1:mr13 alignÉÔÍ× */
    case 2: exring[cid].unit[i].b[b][j].r[3] = exring[cid].unit[i].tx4dr[3];   break; /* 2:tr3  alignÉÔÍ× */
    case 3: exring[cid].unit[i].b[b][j].r[3] = exring[cid].unit[i].ea14woofs;  break; /* 3:ea1woofs */
    }
  }

  if ((exring[cid].unit[i].cycle&3) == 3) /* final */
    exring[cid].unit[i].brout_valid = exring[cid].unit[i].tr_valid;

  if (exring[cid].unit[i].cmd == CMD_NOP
      && (exring[cid].unit[i].cycle&3) == 3
      && (reg_ctrl.i[cid].cmd&3) == CMD_RESET) {
    exring[cid].unit[i].lmring_ful2    = 0;
    exring[cid].unit[i].lmring_b_top   = 0;
    exring[cid].unit[i].lmring_b_bot   = 0;
    for (k=0; k<LMRING_BR_BUF; k++) {
      exring[cid].unit[i].lmring_br[k].rw   = 0;
      exring[cid].unit[i].lmring_br[k].ty   = 0;
      exring[cid].unit[i].lmring_br[k].col  = 0;
      exring[cid].unit[i].lmring_br[k].sq   = 0;
      exring[cid].unit[i].lmring_br[k].av   = 0;
      exring[cid].unit[i].lmring_br[k].a    = 0;
      exring[cid].unit[i].lmring_br[k].dm   = 0;
      exring[cid].unit[i].lmring_br[k].d[0] = 0LL;
      exring[cid].unit[i].lmring_br[k].d[1] = 0LL;
      exring[cid].unit[i].lmring_br[k].d[2] = 0LL;
      exring[cid].unit[i].lmring_br[k].d[3] = 0LL;
    }
  }
  /* lmring_ful1¤Èlmring_ful2¤Ïreg, exring[cid].unit[ni].deq_wait¤Ïwire */
  /* ¸åÃÊ¤«¤ésiml¤¹¤ë¤Î¤Ç,[ni].deq_wait¤ÏºÇ¿· */
  /* exring[cid].unit[i].lmring_ful1¤Ï0¤ËÊÑ¹¹¤·Ä¾¸å¤Îsiml_unit_stage4()¤Ë¤Æ1¤ËÌá¤ë.¥Ï¡¼¥É¤âÆ±¤¸ */
  /* exring[cid].unit[i].lmring_ful2¤ò¤³¤³¤Ç¹¹¿·.¥Ï¡¼¥É¤âÆ±¤¸ */
  else {
    if (exring[cid].unit[i].lmring_ful1) { /* enqueue to lmring_br for next cycle */
      /* enqueue */
      br->rw   = tr->rw;   /* next cycle */
      br->ty   = tr->ty;   /* next cycle */
      br->col  = tr->col;  /* next cycle */
      br->sq   = tr->sq;   /* next cycle */
      br->av   = tr->av;
      br->a    = tr->a;    /* next cycle */
      br->dm   = tr->dm;   /* next cycle */
      *((Uint*)(&br->d[0])+0) = (tr->merge & 0x01) ? *((Uint*)(&exring[cid].unit[i].lmrd[0])+0) : *((Uint*)(&tr->d[0])+0); /* next cycle */
      *((Uint*)(&br->d[0])+1) = (tr->merge & 0x02) ? *((Uint*)(&exring[cid].unit[i].lmrd[0])+1) : *((Uint*)(&tr->d[0])+1); /* next cycle */
      *((Uint*)(&br->d[1])+0) = (tr->merge & 0x04) ? *((Uint*)(&exring[cid].unit[i].lmrd[1])+0) : *((Uint*)(&tr->d[1])+0); /* next cycle */
      *((Uint*)(&br->d[1])+1) = (tr->merge & 0x08) ? *((Uint*)(&exring[cid].unit[i].lmrd[1])+1) : *((Uint*)(&tr->d[1])+1); /* next cycle */
      *((Uint*)(&br->d[2])+0) = (tr->merge & 0x10) ? *((Uint*)(&exring[cid].unit[i].lmrd[2])+0) : *((Uint*)(&tr->d[2])+0); /* next cycle */
      *((Uint*)(&br->d[2])+1) = (tr->merge & 0x20) ? *((Uint*)(&exring[cid].unit[i].lmrd[2])+1) : *((Uint*)(&tr->d[2])+1); /* next cycle */
      *((Uint*)(&br->d[3])+0) = (tr->merge & 0x40) ? *((Uint*)(&exring[cid].unit[i].lmrd[3])+0) : *((Uint*)(&tr->d[3])+0); /* next cycle */
      *((Uint*)(&br->d[3])+1) = (tr->merge & 0x80) ? *((Uint*)(&exring[cid].unit[i].lmrd[3])+1) : *((Uint*)(&tr->d[3])+1); /* next cycle */
      exring[cid].unit[i].lmring_ful2++;
      exring[cid].unit[i].lmring_b_top = (exring[cid].unit[i].lmring_b_top + 1)%LMRING_BR_BUF;
    }
    if (br_ful2 && !deq_wait) { /* dequeued for next cycle */
      exring[cid].unit[i].lmring_ful2--;
      exring[cid].unit[i].lmring_b_bot = (exring[cid].unit[i].lmring_b_bot + 1)%LMRING_BR_BUF;
    }
  }

  return (0);
}

siml_unit_lmm(cid,  i) Uint cid, i;
{
  Uint a0al, a1al;
  Ull mm0[UNIT_WIDTH];
  int k;

  /* write:  cexdr,mw,ma                                -> LMM                      */
  /* read:   cexdr,exdr,mw/tr,ma/ea[01]dr,LMM -> mr[01] -> br,lmrd                  */
  /*        |         |         |         |         |         |         |         | */
  /* clk ___/~~~~\____/~~~~\____/~~~~\____/~~~~\____/~~~~\____/~~~~\____/~~~~\____/ */
  /* en  ____/~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\___________________ */
  /* rw  _______read___/~~~~~~write~~~~~~~~\______read_____________________________ */
  /* ma  -----<   A0  ><   A1   ><   A2   ><   A3   ><   A4   >-------------------- */
  /* mw  --------------<   W1   ><   W2   >---------------------------------------- */
  /* mr  --------------<   R0   >--------------------<   R3   ><   R4   >---------- */

#if 0
  struct lmm {
    Ull   en0         : 1; /* internal reg       */
    Ull   en1         : 1; /* internal reg       */
    Ull   rw0         : 1; /* 0:read, 1:write    */
    Ull   rw1         : 1; /* 0:read,(1:write)   */
    Ull   ma0         :18; /* internal reg  addr(32B aligned)      */
    Ull   ma1         :18; /* internal reg  addr(32B aligned)      */
    Ull   mm0         :32; /* internal reg  mask */
    Ull   mw0[UNIT_WIDTH]; /* internal reg  data */
    Ull   mr0[UNIT_WIDTH]; /* internal wire data */
    Ull   mr1[UNIT_WIDTH]; /* internal wire data */
    Uchar m[LMEM_SIZE]   ; /* local memory       */
  } lmm;
#endif

  a0al    = exring[cid].unit[i].lmm.ma0; /* 64B aligned */
  a1al    = exring[cid].unit[i].lmm.ma1; /* 64B aligned */
  mm0[0]  = ((exring[cid].unit[i].lmm.mm0 & 0x00000001) ? 0x00000000000000ffLL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x00000002) ? 0x000000000000ff00LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x00000004) ? 0x0000000000ff0000LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x00000008) ? 0x00000000ff000000LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x00000010) ? 0x000000ff00000000LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x00000020) ? 0x0000ff0000000000LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x00000040) ? 0x00ff000000000000LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x00000080) ? 0xff00000000000000LL : 0x0000000000000000LL);
  mm0[1]  = ((exring[cid].unit[i].lmm.mm0 & 0x00000100) ? 0x00000000000000ffLL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x00000200) ? 0x000000000000ff00LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x00000400) ? 0x0000000000ff0000LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x00000800) ? 0x00000000ff000000LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x00001000) ? 0x000000ff00000000LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x00002000) ? 0x0000ff0000000000LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x00004000) ? 0x00ff000000000000LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x00008000) ? 0xff00000000000000LL : 0x0000000000000000LL);
  mm0[2]  = ((exring[cid].unit[i].lmm.mm0 & 0x00010000) ? 0x00000000000000ffLL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x00020000) ? 0x000000000000ff00LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x00040000) ? 0x0000000000ff0000LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x00080000) ? 0x00000000ff000000LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x00100000) ? 0x000000ff00000000LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x00200000) ? 0x0000ff0000000000LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x00400000) ? 0x00ff000000000000LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x00800000) ? 0xff00000000000000LL : 0x0000000000000000LL);
  mm0[3]  = ((exring[cid].unit[i].lmm.mm0 & 0x01000000) ? 0x00000000000000ffLL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x02000000) ? 0x000000000000ff00LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x04000000) ? 0x0000000000ff0000LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x08000000) ? 0x00000000ff000000LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x10000000) ? 0x000000ff00000000LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x20000000) ? 0x0000ff0000000000LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x40000000) ? 0x00ff000000000000LL : 0x0000000000000000LL)
           |((exring[cid].unit[i].lmm.mm0 & 0x80000000) ? 0xff00000000000000LL : 0x0000000000000000LL);

  if (exring[cid].unit[i].lmm.en0) {
    if (exring[cid].unit[i].lmm.rw0==0) { /* lmm read enabled */
      for (k=0; k<UNIT_WIDTH; k++)
        exring[cid].unit[i].lmm.mr0[k] = *((Ull*)&exring[cid].unit[i].lmm.m[a0al]+k);
#if 0
      printf("%03.3d.%02.2d:LMM RD0 a=%08.8x d=%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x\n",
             cid, i, a0al,
             (Uint)(exring[cid].unit[i].lmm.mr0[3]>>32), (Uint)exring[cid].unit[i].lmm.mr0[3],
             (Uint)(exring[cid].unit[i].lmm.mr0[2]>>32), (Uint)exring[cid].unit[i].lmm.mr0[2],
             (Uint)(exring[cid].unit[i].lmm.mr0[1]>>32), (Uint)exring[cid].unit[i].lmm.mr0[1],
             (Uint)(exring[cid].unit[i].lmm.mr0[0]>>32), (Uint)exring[cid].unit[i].lmm.mr0[0]);
#endif
    }
    else { /* lmm write enabled */
      for (k=0; k<UNIT_WIDTH; k++)
        *((Ull*)&exring[cid].unit[i].lmm.m[a0al]+k) = (*((Ull*)&exring[cid].unit[i].lmm.m[a0al]+k) & ~mm0[k]) | (exring[cid].unit[i].lmm.mw0[k] & mm0[k]);
#if 0
      printf("%03.3d.%02.2d:LMM WR0 a=%08.8x m=%08.8x d=%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x\n",
             cid, i, a0al, exring[cid].unit[i].lmm.mm0,
             (Uint)(exring[cid].unit[i].lmm.mw0[3]>>32), (Uint)exring[cid].unit[i].lmm.mw0[3],
             (Uint)(exring[cid].unit[i].lmm.mw0[2]>>32), (Uint)exring[cid].unit[i].lmm.mw0[2],
             (Uint)(exring[cid].unit[i].lmm.mw0[1]>>32), (Uint)exring[cid].unit[i].lmm.mw0[1],
             (Uint)(exring[cid].unit[i].lmm.mw0[0]>>32), (Uint)exring[cid].unit[i].lmm.mw0[0]);
#endif
    }
  }

  if (exring[cid].unit[i].lmm.en1 && !exring[cid].unit[i].lmm.rw1) { /* lmm read enabled */
    for (k=0; k<UNIT_WIDTH; k++)
      exring[cid].unit[i].lmm.mr1[k] = *((Ull*)&exring[cid].unit[i].lmm.m[a1al]+k);
#if 0
      printf("%03.3d.%02.2d:LMM RD1 a=%08.8x d=%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x_%08.8x%08.8x\n",
             cid, i, a1al,
             (Uint)(exring[cid].unit[i].lmm.mr1[3]>>32), (Uint)exring[cid].unit[i].lmm.mr1[3],
             (Uint)(exring[cid].unit[i].lmm.mr1[2]>>32), (Uint)exring[cid].unit[i].lmm.mr1[2],
             (Uint)(exring[cid].unit[i].lmm.mr1[1]>>32), (Uint)exring[cid].unit[i].lmm.mr1[1],
             (Uint)(exring[cid].unit[i].lmm.mr1[0]>>32), (Uint)exring[cid].unit[i].lmm.mr1[0]);
#endif
  }

  return (0);
}

show_emax6_status(cid) Uint cid;
{
  int row, col, i;

  /* Black       0;30     Dark Gray     1;30 */
  /* Blue        0;34     Light Blue    1;34 */
  /* Green       0;32     Light Green   1;32 */
  /* Cyan        0;36     Light Cyan    1;36 */
  /* Red         0;31     Light Red     1;31 */
  /* Purple      0;35     Light Purple  1;35 */
  /* Brown       0;33     Yellow        1;33 */
  /* Light Gray  0;37     White         1;37 */

  /* show axiif */
  printf("=AXIIF[%02.2d]           axiarr-arv-arl-ara    =%d-%d-%08.8x-%08.8x          r_v=%d-%d d=%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x\n",
         cid, axiif[cid].axi_arready, axiif[cid].axi_arvalid, axiif[cid].axi_arlen, (Uint)axiif[cid].axi_araddr, axiif[cid].axi_rready, axiif[cid].axi_rvalid,
         (Uint)(axiif[cid].axi_rdata[3]>>32), (Uint)axiif[cid].axi_rdata[3], (Uint)(axiif[cid].axi_rdata[2]>>32), (Uint)axiif[cid].axi_rdata[2],
         (Uint)(axiif[cid].axi_rdata[1]>>32), (Uint)axiif[cid].axi_rdata[1], (Uint)(axiif[cid].axi_rdata[0]>>32), (Uint)axiif[cid].axi_rdata[0]);
  printf("=EXRING[%02.2d].cuy=%d%d_%d axiawr-awv-awl-awa-stb=%d-%d-%08.8x-%08.8x-%08.8x r_v=%d-%d d=%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x\n",
         cid, exring[cid].cmd_busy, exring[cid].unit_busy, exring[cid].cycle, axiif[cid].axi_awready, axiif[cid].axi_awvalid, axiif[cid].axi_awlen, (Uint)axiif[cid].axi_awaddr, axiif[cid].axi_wstrb, axiif[cid].axi_wready, axiif[cid].axi_wvalid,
         (Uint)(axiif[cid].axi_wdata[3]>>32), (Uint)axiif[cid].axi_wdata[3], (Uint)(axiif[cid].axi_wdata[2]>>32), (Uint)axiif[cid].axi_wdata[2],
         (Uint)(axiif[cid].axi_wdata[1]>>32), (Uint)axiif[cid].axi_wdata[1], (Uint)(axiif[cid].axi_wdata[0]>>32), (Uint)axiif[cid].axi_wdata[0]);

  printf(" reqn=%04.4x srw=%x sadr=%08.8x slen=%04.4x sreq=%04.4x ful2-top-bot=%d-%d-%d",
         axiif[cid].reqn,
         axiif[cid].srw,
         axiif[cid].sadr,
         axiif[cid].slen,
         axiif[cid].sreq,
         axiif[cid].axring_ful2,
         axiif[cid].axring_b_top,
         axiif[cid].axring_b_bot);

  for (col=0; col<AXRING_BR_BUF; col++) {
    if (col>0) printf("                                                           ");
    printf(" XBR[%d].rw-ty-col-sq-av=%x-%x-%x-%04.4x-%x a-dm=%08.8x-%08.8x d=%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x\n",
           col,
           (Uint)axiif[cid].axring_br[col].rw, (Uint)axiif[cid].axring_br[col].ty, axiif[cid].axring_br[col].col, axiif[cid].axring_br[col].sq, axiif[cid].axring_br[col].av, axiif[cid].axring_br[col].a, axiif[cid].axring_br[col].dm,
           (Uint)(axiif[cid].axring_br[col].d[3]>>32), (Uint)axiif[cid].axring_br[col].d[3], (Uint)(axiif[cid].axring_br[col].d[2]>>32), (Uint)axiif[cid].axring_br[col].d[2],
           (Uint)(axiif[cid].axring_br[col].d[1]>>32), (Uint)axiif[cid].axring_br[col].d[1], (Uint)(axiif[cid].axring_br[col].d[0]>>32), (Uint)axiif[cid].axring_br[col].d[0]);
  }

  for (row=0; row<EMAX_DEPTH; row++) {
    /* show conf */
    for (col=EMAX_WIDTH-1; col>=0; col--)
      printf("|conf[%d][%d]-------------------------------------------------", row, col);
    printf("\n");
    for (col=EMAX_WIDTH-1; col>=0; col--)
      printf("|\033[1;%dmv%d\033[0m op%02.2x_%x_%x e1/2/3=%x_%d%d/%x_%d/%x_%d 2i%08.8x%08.8x_%d 3i%02.2x_%d",
             reg_ctrl.i[cid].conf[row][col].cdw0.v?36:34, reg_ctrl.i[cid].conf[row][col].cdw0.v,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw0.op1,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw0.op2,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw0.op3,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw0.ex1brs,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw0.ex1s,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw0.ex1exp,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw0.ex2brs,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw0.ex2exp,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw0.ex3brs,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw0.ex3exp,
             (Uint)(reg_ctrl.i[cid].conf[row][col].cdw3.e2imm>>32),
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw3.e2imm,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw0.e2is,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw0.e3imm,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw0.e3is);
    printf("\n");
    for (col=EMAX_WIDTH-1; col>=0; col--)
      printf("|cop=%x%x%x%x_%04.4x ea1=%02.2x_%d_%d_%x ea0=%02.2x_%d_%d_%x eab=%x eao=%x        ",
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw1.cs3,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw1.cs2,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw1.cs1,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw1.cs0,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw1.cex_tab,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw1.ea1op,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw1.ea1bs,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw1.ea1os,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw1.ea1msk,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw1.ea0op,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw1.ea0bs,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw1.ea0os,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw1.ea0msk,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw1.eabbrs,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw1.eaobrs);
    printf("\n");
    for (col=EMAX_WIDTH-1; col>=0; col--)
      printf("|ts3-0=%x%x%x%x trs3-0=%x%x%x%x mwsa3210=%x%x%x%x%x brs3-0=%x%x%x%x md=%d lm=%d",
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw2.ts3,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw2.ts2,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw2.ts1,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw2.ts0,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw2.trs3,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw2.trs2,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw2.trs1,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw2.trs0,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw2.mwsa,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw2.mws3,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw2.mws2,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw2.mws1,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw2.mws0,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw2.brs3,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw2.brs2,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw2.brs1,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw2.brs0,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw2.mapdist,
             (Uint) reg_ctrl.i[cid].conf[row][col].cdw2.lmm_mode);
    printf("\n");

    /* show exring-stage1 */
    printf("-stage1-out: cmd=%d cycle=%d", exring[cid].unit[row].cmd, exring[cid].unit[row].cycle);
    printf(" l_row=%02.2d scan_count=%03.3d one_shot=%d \033[1;%dmu1exec=%d\033[0m \033[1;%dmu1fold=%d\033[0m \033[1;%dmu1stop=%d\033[0m \033[1;%dmu2exec=%d\033[0m \033[1;%dmu2fold=%d\033[0m \033[1;%dmu2stop=%d\033[0m\n",
           exring[cid].unit[row].l_row,
           exring[cid].unit[row].scon_count,
           exring[cid].unit[row].one_shot,
           exring[cid].unit[row].unit1_exec?36:34, exring[cid].unit[row].unit1_exec,
           exring[cid].unit[row].unit1_fold?36:34, exring[cid].unit[row].unit1_fold,
           exring[cid].unit[row].unit1_stop?31:34, exring[cid].unit[row].unit1_stop,
           exring[cid].unit[row].unit2_exec?36:34, exring[cid].unit[row].unit2_exec,
           exring[cid].unit[row].unit2_fold?36:34, exring[cid].unit[row].unit2_fold,
           exring[cid].unit[row].unit2_stop?31:34, exring[cid].unit[row].unit2_stop);

    printf("            ");
    printf(" ea31b/1o/0b/0o/top/bot=%05.5x/%05.5x/%05.5x/%05.5x/%08.8x/%08.8x", reg_ctrl.i[cid].addr[row][3].ea1b&0x3ffff, reg_ctrl.i[cid].addr[row][3].ea1o&0x3ffff, reg_ctrl.i[cid].addr[row][3].ea0b&0x3ffff, reg_ctrl.i[cid].addr[row][3].ea0o&0x3ffff, reg_ctrl.i[cid].addr[row][3].top,  reg_ctrl.i[cid].addr[row][3].bot);
    printf(" ea21b/1o/0b/0o/top/bot=%05.5x/%05.5x/%05.5x/%05.5x/%08.8x/%08.8x", reg_ctrl.i[cid].addr[row][2].ea1b&0x3ffff, reg_ctrl.i[cid].addr[row][2].ea1o&0x3ffff, reg_ctrl.i[cid].addr[row][2].ea0b&0x3ffff, reg_ctrl.i[cid].addr[row][2].ea0o&0x3ffff, reg_ctrl.i[cid].addr[row][2].top,  reg_ctrl.i[cid].addr[row][2].bot);
    printf("\n");
    printf("            ");
    printf(" ea11b/1o/0b/0o/top/bot=%05.5x/%05.5x/%05.5x/%05.5x/%08.8x/%08.8x", reg_ctrl.i[cid].addr[row][1].ea1b&0x3ffff, reg_ctrl.i[cid].addr[row][1].ea1o&0x3ffff, reg_ctrl.i[cid].addr[row][1].ea0b&0x3ffff, reg_ctrl.i[cid].addr[row][1].ea0o&0x3ffff, reg_ctrl.i[cid].addr[row][1].top,  reg_ctrl.i[cid].addr[row][1].bot);
    printf(" ea01b/1o/0b/0o/top/bot=%05.5x/%05.5x/%05.5x/%05.5x/%08.8x/%08.8x", reg_ctrl.i[cid].addr[row][0].ea1b&0x3ffff, reg_ctrl.i[cid].addr[row][0].ea1o&0x3ffff, reg_ctrl.i[cid].addr[row][0].ea0b&0x3ffff, reg_ctrl.i[cid].addr[row][0].ea0o&0x3ffff, reg_ctrl.i[cid].addr[row][0].top,  reg_ctrl.i[cid].addr[row][0].bot);
    printf("\n");

    printf("             cx=%d%d%d%d ex123=%08.8x%08.8x/%08.8x%08.8x/%08.8x%08.8x",
           (Uint)exring[cid].unit[row].cx[3],
           (Uint)exring[cid].unit[row].cx[2],
           (Uint)exring[cid].unit[row].cx[1],
           (Uint)exring[cid].unit[row].cx[0],
           (Uint)(exring[cid].unit[row].ex1>>32), (Uint)exring[cid].unit[row].ex1,
           (Uint)(exring[cid].unit[row].ex2>>32), (Uint)exring[cid].unit[row].ex2,
           (Uint)(exring[cid].unit[row].ex3>>32), (Uint)exring[cid].unit[row].ex3);
    printf(" ea1=%05.5x+%05.5x eab/o=%05.5x/%05.5x ea0=%05.5x+%05.5x st2ex=%d st2fd=%d",
           exring[cid].unit[row].ea1b,
           (Uint)exring[cid].unit[row].ea1o&0x3ffff, /* ËÜÅö¤Ï64bit¤«¤éMSK¤Ë¤è¤êÁªÂò */
           exring[cid].unit[row].eab,
           (Uint)exring[cid].unit[row].eao&0x3ffff,  /* ËÜÅö¤Ï64bit¤«¤éMSK¤Ë¤è¤êÁªÂò */
           exring[cid].unit[row].ea0b,
           (Uint)exring[cid].unit[row].ea0o&0x3ffff, /* ËÜÅö¤Ï64bit¤«¤éMSK¤Ë¤è¤êÁªÂò */
           exring[cid].unit[row].stage2_exec, exring[cid].unit[row].stage2_fold);
    printf(" tx3210   =%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x\n",
           (Uint)(exring[cid].unit[row].tx[3]>>32), (Uint)exring[cid].unit[row].tx[3],
           (Uint)(exring[cid].unit[row].tx[2]>>32), (Uint)exring[cid].unit[row].tx[2],
           (Uint)(exring[cid].unit[row].tx[1]>>32), (Uint)exring[cid].unit[row].tx[1],
           (Uint)(exring[cid].unit[row].tx[0]>>32), (Uint)exring[cid].unit[row].tx[0]);

    /* show exring-stage2 */
    printf("-stage2-out: cx2dr=%x ex2dr=%08.8x%08.8x \033[1;%dmstfstat=%d\033[0m \033[1;%dmu1fstat=%d\033[0m \033[1;%dmarbrk=%d\033[0m       ea12dr=%05.5x                      ea02dr=%05.5x    st3ex=%d st3fd=%d",
           exring[cid].unit[row].cx2dr,
           (Uint)(exring[cid].unit[row].ex2dr>>32), (Uint)exring[cid].unit[row].ex2dr,
           exring[cid].unit[row].stage_forstat?31:34, exring[cid].unit[row].stage_forstat,
           exring[cid].unit[row].unit1_forstat?31:34, exring[cid].unit[row].unit1_forstat,
           exring[cid].unit[row].unit1_arbrk?31:34, exring[cid].unit[row].unit1_arbrk,
           (Uint)exring[cid].unit[row].ea12dr,
           (Uint)exring[cid].unit[row].ea02dr,
           exring[cid].unit[row].stage3_exec, exring[cid].unit[row].stage3_fold);
    printf(" tx2dr3210=%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x\n",
           (Uint)(exring[cid].unit[row].tx2dr[3]>>32), (Uint)exring[cid].unit[row].tx2dr[3],
           (Uint)(exring[cid].unit[row].tx2dr[2]>>32), (Uint)exring[cid].unit[row].tx2dr[2],
           (Uint)(exring[cid].unit[row].tx2dr[1]>>32), (Uint)exring[cid].unit[row].tx2dr[1],
           (Uint)(exring[cid].unit[row].tx2dr[0]>>32), (Uint)exring[cid].unit[row].tx2dr[0]);

    /* show exring-stage3 */
    printf("-stage3-out: cx3dr=%x ex3dr=%08.8x%08.8x                                   ea13dr=%05.5x                      ea03dr=%05.5x    st4ex=%d st4fd=%d",
           exring[cid].unit[row].cx3dr,
           (Uint)(exring[cid].unit[row].ex3dr>>32), (Uint)exring[cid].unit[row].ex3dr,
           (Uint)exring[cid].unit[row].ea13dr,
           (Uint)exring[cid].unit[row].ea03dr,
           exring[cid].unit[row].stage4_exec, exring[cid].unit[row].stage4_fold);
    printf(" tx3dr3210=%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x\n",
           (Uint)(exring[cid].unit[row].tx3dr[3]>>32), (Uint)exring[cid].unit[row].tx3dr[3],
           (Uint)(exring[cid].unit[row].tx3dr[2]>>32), (Uint)exring[cid].unit[row].tx3dr[2],
           (Uint)(exring[cid].unit[row].tx3dr[1]>>32), (Uint)exring[cid].unit[row].tx3dr[1],
           (Uint)(exring[cid].unit[row].tx3dr[0]>>32), (Uint)exring[cid].unit[row].tx3dr[0]);

    /* show exring-stage4 */
    printf("-stage4-out:         ex4dr=%08.8x%08.8x          ",
           (Uint)(exring[cid].unit[row].ex4dr>>32), (Uint)exring[cid].unit[row].ex4dr);
    printf("       ea14um/lm=%x/%05.5x ea14dr=%05.5x    ea04um/lm=%x/%05.5x ea04dr=%05.5x    trval=%d        ",
           exring[cid].unit[row].ea14_umask,
           exring[cid].unit[row].ea14_lmask,
           exring[cid].unit[row].ea14dr,
           exring[cid].unit[row].ea04_umask,
           exring[cid].unit[row].ea04_lmask,
           exring[cid].unit[row].ea04dr,
           exring[cid].unit[row].tr_valid);
    printf(" tx4dr3210=%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x\n",
           (Uint)(exring[cid].unit[row].tx4dr[3]>>32), (Uint)exring[cid].unit[row].tx4dr[3],
           (Uint)(exring[cid].unit[row].tx4dr[2]>>32), (Uint)exring[cid].unit[row].tx4dr[2],
           (Uint)(exring[cid].unit[row].tx4dr[1]>>32), (Uint)exring[cid].unit[row].tx4dr[1],
           (Uint)(exring[cid].unit[row].tx4dr[0]>>32), (Uint)exring[cid].unit[row].tx4dr[0]);
    printf("                                                                                                                            rk-wk-dk-sf-s4-col-e1b-e0b-ful-dqw-fl1=%01.1x-%01.1x-%d-%d-%d-%d-%d-%d-%d-%d-%d\n",
           exring[cid].unit[row].lmranger_ok,
           exring[cid].unit[row].lmrangew_ok,
           exring[cid].unit[row].lmlddmqw_ok,
           exring[cid].unit[row].lmea0sfma,
           exring[cid].unit[row].lmea0strq,
           exring[cid].unit[row].lmea0strqcol,
           exring[cid].unit[row].lmring_ea1bsy,
           exring[cid].unit[row].lmring_ea0bsy,
           exring[cid].unit[row].lmring_ful,
           exring[cid].unit[row].deq_wait,
           exring[cid].unit[row].lmring_ful1);
    printf(" l1.e-rw-a    =%d-%d-%05.5x       mx=%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x",
           exring[cid].unit[row].lmm.en1,
           exring[cid].unit[row].lmm.rw1,
           exring[cid].unit[row].lmm.ma1,
           (Uint)(exring[cid].unit[row].mwmux[3]>>32), (Uint)exring[cid].unit[row].mwmux[3],
           (Uint)(exring[cid].unit[row].mwmux[2]>>32), (Uint)exring[cid].unit[row].mwmux[2],
           (Uint)(exring[cid].unit[row].mwmux[1]>>32), (Uint)exring[cid].unit[row].mwmux[1],
           (Uint)(exring[cid].unit[row].mwmux[0]>>32), (Uint)exring[cid].unit[row].mwmux[0]);
    printf("                       lmwd lmco-ca-m-d=%d-%05.5x-%08.8x-%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x\n",
           exring[cid].unit[row].lmco,
           exring[cid].unit[row].lmca,
           exring[cid].unit[row].lmwm,
           (Uint)(exring[cid].unit[row].lmwd[3]>>32), (Uint)exring[cid].unit[row].lmwd[3],
           (Uint)(exring[cid].unit[row].lmwd[2]>>32), (Uint)exring[cid].unit[row].lmwd[2],
           (Uint)(exring[cid].unit[row].lmwd[1]>>32), (Uint)exring[cid].unit[row].lmwd[1],
           (Uint)(exring[cid].unit[row].lmwd[0]>>32), (Uint)exring[cid].unit[row].lmwd[0]);
    printf(" l0.e-rw-a-m-d=%d-%d-%05.5x-%08.8x-%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x",
           exring[cid].unit[row].lmm.en0,
           exring[cid].unit[row].lmm.rw0,
           exring[cid].unit[row].lmm.ma0,
           exring[cid].unit[row].lmm.mm0,
           (Uint)(exring[cid].unit[row].lmm.mw0[3]>>32), (Uint)exring[cid].unit[row].lmm.mw0[3],
           (Uint)(exring[cid].unit[row].lmm.mw0[2]>>32), (Uint)exring[cid].unit[row].lmm.mw0[2],
           (Uint)(exring[cid].unit[row].lmm.mw0[1]>>32), (Uint)exring[cid].unit[row].lmm.mw0[1],
           (Uint)(exring[cid].unit[row].lmm.mw0[0]>>32), (Uint)exring[cid].unit[row].lmm.mw0[0]);
    printf(" LTR.rw-t-c-s-m-v-a-m-d=%x-%x-%x-%04.4x-%x-%x-%08.8x-%08.8x-%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x\n",
           exring[cid].unit[row].lmring_tr.rw,
           exring[cid].unit[row].lmring_tr.ty,
           exring[cid].unit[row].lmring_tr.col,
           exring[cid].unit[row].lmring_tr.sq,
           exring[cid].unit[row].lmring_tr.merge,
           exring[cid].unit[row].lmring_tr.av,
           exring[cid].unit[row].lmring_tr.a,
           exring[cid].unit[row].lmring_tr.dm,
           (Uint)(exring[cid].unit[row].lmring_tr.d[3]>>32), (Uint)exring[cid].unit[row].lmring_tr.d[3],
           (Uint)(exring[cid].unit[row].lmring_tr.d[2]>>32), (Uint)exring[cid].unit[row].lmring_tr.d[2],
           (Uint)(exring[cid].unit[row].lmring_tr.d[1]>>32), (Uint)exring[cid].unit[row].lmring_tr.d[1],
           (Uint)(exring[cid].unit[row].lmring_tr.d[0]>>32), (Uint)exring[cid].unit[row].lmring_tr.d[0]);

    /* show exring-stage5 */
    printf("-stage5-out:\n");
    for (col=0; col<EMAX_WIDTH; col++) {
      Uint lmm_mode, umask, lmask, toph, topl, both, botl; /* 18bits */
      lmm_mode = reg_ctrl.i[cid].conf[row][col].cdw2.lmm_mode;
      switch (lmm_mode) {
      case 0: umask = LMEM_UMASK0; lmask = LMEM_LMASK0; break;
      case 1: umask = LMEM_UMASK1; lmask = LMEM_LMASK1; break;
      case 2: umask = LMEM_UMASK2; lmask = LMEM_LMASK2; break;
      case 3: umask = LMEM_UMASK3; lmask = LMEM_LMASK3; break;
      }
      if (reg_ctrl.i[cid].conf[row][col].cdw1.ea0op == OP_IM_BUFWR
        ||reg_ctrl.i[cid].conf[row][col].cdw1.ea1op == OP_IM_BUFRD) { /* sequential from addr=0 (including OP_LDDMQ) */
        toph = ((col & umask) * (LMEM_SIZE/4)) | (sizeof(Ull)*15 & lmask);
        topl = ((col & umask) * (LMEM_SIZE/4)) | (0              & lmask);
        printf(" \033[1;33mlm%d(%d):%08.8x-%08.8x\033[0m", col, lmm_mode, toph, topl);
        for (i=0; i<sizeof(Ull)*16; i+=sizeof(Ull))
          printf("-%08.8x%08.8x", (Uint)(*(Ull*)&exring[cid].unit[row].lmm.m[toph-i]>>32), (Uint)(*(Ull*)&exring[cid].unit[row].lmm.m[toph-i]));
        printf("\n");
        both = ((col & umask) * (LMEM_SIZE/4)) | ((LMEM_SIZE-sizeof(Ull)               ) & lmask);
        botl = ((col & umask) * (LMEM_SIZE/4)) | ((LMEM_SIZE-sizeof(Ull)-sizeof(Ull)*15) & lmask);
        printf(" \033[1;33mlm%d(%d):%08.8x-%08.8x\033[0m", col, lmm_mode, both, botl);
        for (i=0; i<sizeof(Ull)*16; i+=sizeof(Ull))
          printf("-%08.8x%08.8x", (Uint)(*(Ull*)&exring[cid].unit[row].lmm.m[both-i]>>32), (Uint)(*(Ull*)&exring[cid].unit[row].lmm.m[both-i]));
        printf("\n");
      }
      else if ((reg_ctrl.i[cid].conf[row][col].cdw1.ea0op && reg_ctrl.i[cid].conf[row][col].cdw1.ea0op < OP_IM_BUFWR)
             ||(reg_ctrl.i[cid].conf[row][col].cdw1.ea1op && reg_ctrl.i[cid].conf[row][col].cdw1.ea1op < OP_IM_BUFRD)) {
        toph = ((col & umask) * (LMEM_SIZE/4)) | ((reg_ctrl.i[cid].addr[row][col].top+sizeof(Ull)*15) & lmask);
        topl = ((col & umask) * (LMEM_SIZE/4)) | ((reg_ctrl.i[cid].addr[row][col].top               ) & lmask);
        printf(" \033[1;33mlm%d(%d):%08.8x-%08.8x\033[0m", col, lmm_mode, toph, topl);
        for (i=0; i<sizeof(Ull)*16; i+=sizeof(Ull))
          printf("-%08.8x%08.8x", (Uint)(*(Ull*)&exring[cid].unit[row].lmm.m[toph-i]>>32), (Uint)(*(Ull*)&exring[cid].unit[row].lmm.m[toph-i]));
        printf("\n");
        both = ((col & umask) * (LMEM_SIZE/4)) | ((reg_ctrl.i[cid].addr[row][col].bot+1-sizeof(Ull)   ) & lmask);
        botl = ((col & umask) * (LMEM_SIZE/4)) | ((reg_ctrl.i[cid].addr[row][col].bot+1-sizeof(Ull)*16) & lmask);
        printf(" \033[1;33mlm%d(%d):%08.8x-%08.8x\033[0m", col, lmm_mode, both, botl);
        for (i=0; i<sizeof(Ull)*16; i+=sizeof(Ull))
          printf("-%08.8x%08.8x", (Uint)(*(Ull*)&exring[cid].unit[row].lmm.m[both-i]>>32), (Uint)(*(Ull*)&exring[cid].unit[row].lmm.m[both-i]));
        printf("\n");
      }
    }

    printf(" mr1=%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x",
           (Uint)(exring[cid].unit[row].lmm.mr1[3]>>32), (Uint)exring[cid].unit[row].lmm.mr1[3],
           (Uint)(exring[cid].unit[row].lmm.mr1[2]>>32), (Uint)exring[cid].unit[row].lmm.mr1[2],
           (Uint)(exring[cid].unit[row].lmm.mr1[1]>>32), (Uint)exring[cid].unit[row].lmm.mr1[1],
           (Uint)(exring[cid].unit[row].lmm.mr1[0]>>32), (Uint)exring[cid].unit[row].lmm.mr1[0]);
    printf(" mr0=%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x",
           (Uint)(exring[cid].unit[row].lmm.mr0[3]>>32), (Uint)exring[cid].unit[row].lmm.mr0[3],
           (Uint)(exring[cid].unit[row].lmm.mr0[2]>>32), (Uint)exring[cid].unit[row].lmm.mr0[2],
           (Uint)(exring[cid].unit[row].lmm.mr0[1]>>32), (Uint)exring[cid].unit[row].lmm.mr0[1],
           (Uint)(exring[cid].unit[row].lmm.mr0[0]>>32), (Uint)exring[cid].unit[row].lmm.mr0[0]);
    printf(" mr1mux-mr1d-mr0mux-mr0d=%d-%08.8x%08.8x-%d-%08.8x%08.8x\n",
           exring[cid].unit[row].mr1mux,
           (Uint)(exring[cid].unit[row].mr1d>>32), (Uint)exring[cid].unit[row].mr1d,
           exring[cid].unit[row].mr0mux,
           (Uint)(exring[cid].unit[row].mr0d>>32), (Uint)exring[cid].unit[row].mr0d);
    printf(" lmr=%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x\n",
           (Uint)(exring[cid].unit[row].lmrd[3]>>32), (Uint)exring[cid].unit[row].lmrd[3],
           (Uint)(exring[cid].unit[row].lmrd[2]>>32), (Uint)exring[cid].unit[row].lmrd[2],
           (Uint)(exring[cid].unit[row].lmrd[1]>>32), (Uint)exring[cid].unit[row].lmrd[1],
           (Uint)(exring[cid].unit[row].lmrd[0]>>32), (Uint)exring[cid].unit[row].lmrd[0]);

    printf(" bv%d", exring[cid].unit[row].brout_valid);
    for (col=EMAX_WIDTH-1; col>=0; col--) {
      printf(" 0%d=%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x",
             col,
             (Uint)(exring[cid].unit[row].b[0][col].r[3]>>32), (Uint)exring[cid].unit[row].b[0][col].r[3],
             (Uint)(exring[cid].unit[row].b[0][col].r[2]>>32), (Uint)exring[cid].unit[row].b[0][col].r[2],
             (Uint)(exring[cid].unit[row].b[0][col].r[1]>>32), (Uint)exring[cid].unit[row].b[0][col].r[1],
             (Uint)(exring[cid].unit[row].b[0][col].r[0]>>32), (Uint)exring[cid].unit[row].b[0][col].r[0]);
    }
    printf("\n");
    printf("    ");
    for (col=EMAX_WIDTH-1; col>=0; col--) {
      printf(" 1%d=%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x",
             col,
             (Uint)(exring[cid].unit[row].b[1][col].r[3]>>32), (Uint)exring[cid].unit[row].b[1][col].r[3],
             (Uint)(exring[cid].unit[row].b[1][col].r[2]>>32), (Uint)exring[cid].unit[row].b[1][col].r[2],
             (Uint)(exring[cid].unit[row].b[1][col].r[1]>>32), (Uint)exring[cid].unit[row].b[1][col].r[1],
             (Uint)(exring[cid].unit[row].b[1][col].r[0]>>32), (Uint)exring[cid].unit[row].b[1][col].r[0]);
    }
    printf("\n");

    printf(" fl2-btop-bbot=%d-%d-%d",
           exring[cid].unit[row].lmring_ful2,
           exring[cid].unit[row].lmring_b_top,
           exring[cid].unit[row].lmring_b_bot);

    for (col=0; col<LMRING_BR_BUF; col++) {
      if (col>0) printf("                    ");
      printf(" LBR[%d].rw-ty-col-sq-av-a-dm=%x-%x-%x-%04.4x-%x-%08.8x-%08.8x d=%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x-%08.8x%08.8x\n",
             col,
             exring[cid].unit[row].lmring_br[col].rw,
             exring[cid].unit[row].lmring_br[col].ty,
             exring[cid].unit[row].lmring_br[col].col,
             exring[cid].unit[row].lmring_br[col].sq,
             exring[cid].unit[row].lmring_br[col].av,
             exring[cid].unit[row].lmring_br[col].a,
             exring[cid].unit[row].lmring_br[col].dm,
             (Uint)(exring[cid].unit[row].lmring_br[col].d[3]>>32), (Uint)exring[cid].unit[row].lmring_br[col].d[3],
             (Uint)(exring[cid].unit[row].lmring_br[col].d[2]>>32), (Uint)exring[cid].unit[row].lmring_br[col].d[2],
             (Uint)(exring[cid].unit[row].lmring_br[col].d[1]>>32), (Uint)exring[cid].unit[row].lmring_br[col].d[1],
             (Uint)(exring[cid].unit[row].lmring_br[col].d[0]>>32), (Uint)exring[cid].unit[row].lmring_br[col].d[0]);
    }
  }
}
