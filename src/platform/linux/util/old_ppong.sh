#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!pfr.o, ;!mpool.o,a3;!sm.o,a4;!l.o,a1;!pi.o, ;!pi2.o, ;!po.o, ;!b_po.o, ;!va.o,a2;!te.o,a5:\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o;\
l.o-fprr.o|mm.o|print.o;\
sm.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o|mpool.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
pfr.o-sm.o|mm.o|va.o|print.o|boot.o;\
te.o-sm.o|mm.o|va.o|print.o|fprr.o;\
\
pi.o-sm.o|print.o|fprr.o|l.o|mm.o|po.o|te.o|pfr.o|pi2.o;\
pi2.o-sm.o|print.o|fprr.o|l.o|mm.o|po.o;\
po.o-sm.o|print.o|fprr.o|l.o|mm.o|b_po.o|pfr.o;\
b_po.o-sm.o|print.o|fprr.o|l.o|mm.o\
" ./gen_client_stub
