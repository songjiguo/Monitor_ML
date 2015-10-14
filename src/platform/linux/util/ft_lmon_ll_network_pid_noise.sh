#!/bin/sh

#httpt.o-sm.o|l.o|print.o|fprr.o|mm.o|buf.o|[server_]rotar.o|te.o|va.o|pfr.o;\
#rotar.o-sm.o|fprr.o|print.o|mm.o|buf.o|l.o|eg.o|va.o|initfs.o|pfr.o;\
# httperf --server=10.0.2.8 --port=200 --uri=/fs/bar --num-conns=7000

./cos_loader \
"c0.o, ;llboot.o, ;lllog.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!l.o,a1;!te.o,a3;!mon_p.o, ;!lmoncli1.o, ;!sm.o,a4;!eg.o,a5;!e.o,a4;!cfkml.o, ;\
!cmultiplexer.o, ;!mpool.o,a3;!buf.o,a5;!bufp.o, ;!lmonser1.o, ;!lmonser2.o, ;!va.o,a2;\
!cpid.o, '1:10:200:/bind:0:%d/listen:255';\
!port.o, ;!tif.o,a2;!tip.o, ;!tnet.o, ;!httpt.o,a8;\
!rfs.o, ;!initfs.o,a3;!unique_map.o, ;!popcgi.o, ;!noise_c.o, ;\
:\
c0.o-llboot.o;\
lllog.o-print.o|[parent_]llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o|lllog.o;\
mm.o-print.o|[faulthndlr_]llboot.o|[parent_]lllog.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o|lllog.o;\
\
l.o-fprr.o|mm.o|print.o|lllog.o|llboot.o;\
te.o-print.o|fprr.o|va.o|mm.o|l.o|lllog.o|llboot.o;\
e.o-sm.o|l.o|mm.o|va.o|fprr.o|print.o|lllog.o;\
eg.o-sm.o|fprr.o|print.o|mm.o|l.o|va.o|lllog.o;\
sm.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o|mpool.o|lllog.o;\
buf.o-boot.o|sm.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o|lllog.o;\
bufp.o-sm.o|fprr.o|print.o|l.o|mm.o|va.o|mpool.o|buf.o|lllog.o;\
mpool.o-print.o|fprr.o|mm.o|boot.o|va.o|l.o|lllog.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o|lllog.o|llboot.o;\
\
cfkml.o-sm.o|fprr.o|print.o|l.o|mm.o|te.o|va.o|cmultiplexer.o|lllog.o;\
cmultiplexer.o-sm.o|fprr.o|print.o|l.o|mm.o|te.o|va.o|lllog.o;\
\
cpid.o-sm.o|print.o|fprr.o|mm.o|va.o|l.o|httpt.o|te.o|[from_]tnet.o|buf.o|bufp.o|eg.o|lllog.o;\
\
httpt.o-sm.o|l.o|print.o|fprr.o|mm.o|buf.o|eg.o|bufp.o|[server_]rfs.o|te.o|va.o|lllog.o;\
initfs.o-fprr.o|print.o|mm.o|va.o|l.o|buf.o|bufp.o|lllog.o;\
\
tnet.o-sm.o|fprr.o|mm.o|print.o|l.o|te.o|eg.o|[parent_]tip.o|port.o|va.o|buf.o|bufp.o|lllog.o;\
tip.o-sm.o|[parent_]tif.o|va.o|fprr.o|print.o|te.o|l.o|eg.o|buf.o|bufp.o|mm.o|lllog.o;\
tif.o-sm.o|print.o|fprr.o|mm.o|l.o|va.o|te.o|eg.o|buf.o|bufp.o|lllog.o;\
port.o-sm.o|l.o|va.o|fprr.o|mm.o|print.o|lllog.o;\
\
rfs.o-sm.o|va.o|fprr.o|print.o|mm.o|buf.o|bufp.o|l.o|e.o|unique_map.o|lllog.o;\
unique_map.o-sm.o|va.o|fprr.o|print.o|mm.o|l.o|e.o|buf.o|bufp.o|lllog.o;\
popcgi.o-sm.o|fprr.o|print.o|mm.o|buf.o|bufp.o|va.o|l.o|[server_]rfs.o|eg.o|te.o|lllog.o;\
\
mon_p.o-print.o|fprr.o|mm.o|te.o|lllog.o|llboot.o|cfkml.o;\
noise_c.o-print.o|fprr.o|mm.o|te.o|lllog.o;\
\
lmoncli1.o-print.o|va.o|fprr.o|l.o|mm.o|lmonser1.o|te.o|lllog.o|llboot.o;\
lmonser2.o-print.o|l.o|va.o|fprr.o|mm.o|lllog.o|llboot.o;\
lmonser1.o-print.o|l.o|va.o|fprr.o|mm.o|lllog.o|buf.o|bufp.o|te.o|rfs.o|lmonser2.o|llboot.o\
" ./gen_client_stub
