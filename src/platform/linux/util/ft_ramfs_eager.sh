#!/bin/sh

./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!pfr.o, ;!mpool.o,a3;!sm.o,a4;!l.o,a1;!te.o,a3;!e.o,a4;!buf.o,a5;!tp.o,a6;!rfs.o, ;!ramfs_rec_test1.o,a10;!ramfs_rec_test2.o, ;!fi.o, ;!va.o,a2;!unique_map.o, :\
\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
\
l.o-fprr.o|mm.o|print.o;\
te.o-sm.o|print.o|fprr.o|mm.o|va.o;\
e.o-sm.o|fprr.o|print.o|mm.o|l.o|va.o;\
sm.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o|mpool.o;\
pfr.o-fprr.o|sm.o|mm.o|l.o|va.o|print.o|boot.o;\
buf.o-boot.o|sm.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o;\
tp.o-sm.o|buf.o|print.o|te.o|fprr.o|mm.o|va.o|mpool.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o;\
\
fi.o-sm.o|fprr.o|print.o|mm.o|va.o|te.o;\
\
rfs.o-sm.o|fprr.o|print.o|mm.o|buf.o|l.o|e.o|va.o|unique_map.o|pfr.o;\
unique_map.o-sm.o|fprr.o|print.o|mm.o|l.o|e.o|va.o|buf.o;\
ramfs_rec_test2.o-sm.o|fprr.o|print.o|mm.o|buf.o|va.o|l.o|rfs.o|e.o;\
ramfs_rec_test1.o-sm.o|fprr.o|print.o|mm.o|buf.o|va.o|l.o|rfs.o|e.o|ramfs_rec_test2.o|te.o|pfr.o\
" ./gen_client_stub
