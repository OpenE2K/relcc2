# SPDX-License-Identifier: GPL-3.0-or-later
# gdb commands for dbg32.c (spec/abi/mode32.md, "C runtime"); dbg32.sh
# passes each line as its own -ex.  Echo lines introduce each group, so
# a failure shows which command failed.
set pagination off
set confirm off
set width 0
echo \n== break leaf, run\n
break leaf
run
echo \n== bt\n
bt
echo \n== info frame\n
info frame
echo \n== arguments\n
info args
p *p
p p->name
p c
p dd
echo \n== globals\n
p gpt
p/x gll
p glong
p/x gulong
p gf
p gld
p garr
p gstr
p gptr
p *(int *) gptr
p gfp
p tlsv
p sizeof (long)
p sizeof (gptr)
ptype struct pt
echo \n== registers\n
p $pc
p $sp
p/x $r0
p/x $r2
p/x $ip
info registers r0 r1 g13 pred0 ctpr1 usd_lo cwd
x/2i $pc
echo \n== info sharedlibrary\n
info sharedlibrary
echo \n== up: mid's locals\n
up
info locals
p local.name
down
echo \n== gcore\n
gcore dbg32.gcore
echo \n== finish\n
finish
echo \n== run again to mid, watch watched, continue\n
delete
break mid
run
watch watched
continue
bt 2
echo \n== continue to the end\n
delete
continue
