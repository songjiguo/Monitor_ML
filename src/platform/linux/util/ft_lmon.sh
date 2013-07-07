#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!l.o,a1;!te.o,a3;!lmon.o,a5;!lmon1.o, ;!lmon2.o, ;!va.o,a2:\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o|[faulthndlr_]llboot.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o;\
l.o-fprr.o|mm.o|print.o;\
te.o-print.o|fprr.o|mm.o|va.o;\
lmon.o-print.o|fprr.o|mm.o|l.o|va.o;\
\
lmon1.o-print.o|fprr.o|va.o|l.o|mm.o|lmon2.o|te.o|lmon.o;\
lmon2.o-print.o|fprr.o|va.o|l.o|mm.o|lmon.o\
" ./gen_client_stub
