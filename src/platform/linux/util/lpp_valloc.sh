#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;lllog.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!l.o,a1;!va.o,a2;!pi.o, ;!po.o, :\
c0.o-llboot.o;\
lllog.o-print.o|[parent_]llboot.o;\
mm.o-[parent_]lllog.o|print.o|[faulthndlr_]llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o|lllog.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o|lllog.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|lllog.o;\
l.o-fprr.o|mm.o|print.o|lllog.o;\
\
pi.o-print.o|fprr.o|po.o|mm.o|lllog.o;\
po.o-print.o|fprr.o|mm.o|lllog.o\
" ./gen_client_stub
