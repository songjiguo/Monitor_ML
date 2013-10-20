#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;lllog.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!pfr.o, ;!mpool.o,a3;!sm.o,a4;!l.o,a1;!te.o,a3;!e.o,a4;!buf.o,a5;!tp.o,a6;!rfs.o, ;!ramfs_rec_test1.o,a10;!ramfs_rec_test2.o, ;!fi.o, ;!va.o,a2;!unique_map.o, :\
\
c0.o-llboot.o;\
lllog.o-print.o|[parent_]llboot.o;\
mm.o-[parent_]lllog.o|print.o|[faulthndlr_]llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o|[parent_]lllog.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o|lllog.o;\
\
l.o-fprr.o|mm.o|print.o|lllog.o;\
te.o-sm.o|print.o|fprr.o|mm.o|va.o|lllog.o;\
e.o-sm.o|va.o|fprr.o|print.o|mm.o|l.o|lllog.o;\
sm.o-print.o|va.o|fprr.o|mm.o|boot.o|l.o|mpool.o|lllog.o;\
pfr.o-fprr.o|sm.o|mm.o|l.o|print.o|boot.o|lllog.o;\
buf.o-sm.o|va.o|fprr.o|boot.o|print.o|l.o|mm.o|mpool.o|lllog.o;\
mpool.o-print.o|va.o|fprr.o|mm.o|boot.o|l.o|lllog.o;\
tp.o-sm.o|buf.o|print.o|va.o|te.o|fprr.o|mm.o|mpool.o|lllog.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|lllog.o;\
\
fi.o-sm.o|va.o|fprr.o|print.o|mm.o|te.o|lllog.o;\
\
rfs.o-sm.o|va.o|fprr.o|print.o|mm.o|buf.o|l.o|e.o|unique_map.o|pfr.o|lllog.o;\
unique_map.o-sm.o|va.o|fprr.o|print.o|mm.o|l.o|e.o|buf.o|lllog.o;\
ramfs_rec_test2.o-sm.o|va.o|fprr.o|print.o|mm.o|buf.o|l.o|rfs.o|e.o|lllog.o;\
ramfs_rec_test1.o-sm.o|va.o|fprr.o|print.o|mm.o|buf.o|l.o|rfs.o|e.o|ramfs_rec_test2.o|te.o|pfr.o|lllog.o\
" ./gen_client_stub
