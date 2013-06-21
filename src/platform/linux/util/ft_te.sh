#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!l.o,a1;!te.o,a3;!sm.o,a4;!mpool.o,a3;!te_rec1.o, ;!pfr.o, ;!va.o,a2:\
\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o|[faulthndlr_]llboot.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o;\
l.o-fprr.o|mm.o|print.o;\
sm.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o|mpool.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
pfr.o-fprr.o|sm.o|mm.o|l.o|va.o|print.o|boot.o;\
te.o-print.o|fprr.o|mm.o|va.o|pfr.o;\
\
te_rec1.o-sm.o|print.o|fprr.o|va.o|l.o|mm.o|te.o\
" ./gen_client_stub
