#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!l.o,a1;!va.o,a2;!cpu.o, ;!po.o, :\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o;\
l.o-fprr.o|mm.o|print.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
po.o-print.o|fprr.o|l.o|mm.o;\
cpu.o-print.o|fprr.o|mm.o|po.o\
" ./gen_client_stub


