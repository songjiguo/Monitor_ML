#ifndef COS_CONFIG_H
#define COS_CONFIG_H

#include "cpu_ghz.h"
#define CPU_TIMER_FREQ 100 // set in your linux .config

#define RUNTIME                5 // seconds

// After how many seconds should schedulers print out their information?
#define SCHED_PRINTOUT_PERIOD  59
#define COMPONENT_ASSERTIONS   1 // activate assertions in components?

#define LINUX_ON_IDLE          1 // should Linux be activated on Composite idle (set this if you want to stop Composite)

/* 
 * Should Composite run as highest priority?  Should NOT be set if
 * using networking (cnet).  Set if you want Composite run at the highest prio
 */
//#define LINUX_HIGHEST_PRIORITY 1

// cos kernel settings
#define COS_PRINT_MEASUREMENTS 1
#define COS_PRINT_SCHED_EVENTS 1
#define COS_ASSERTIONS_ACTIVE  1

/*** Console and output options ***/
/* 
 * Notes: If you are using composite as high priority and no idle to
 * linux, then the shell output will not appear until the Composite
 * system has exited.  Thus, you will want to make the memory size
 * large enough to buffer _all_ output.  Note that currently
 * COS_PRINT_MEM_SZ should not exceed around (1024*1024*3).
 *
 * If you have COS_PRINT_SHELL, you _will not see output_ unless you
 * run 
 * $~/transfer/print
 * after
 * # make
 * but before the runscript.
 */
/* print out to the shell? */
/* #define COS_PRINT_SHELL   1 */
/* how much should we buffer before sending an event to the shell? */
#define COS_PRINT_BUF_SZ  128
/* how large should the shared memory region be that will buffer print data? */
#define COS_PRINT_MEM_SZ  (4096)

/* print out to dmesg? */
#define COS_PRINT_DMESG 1

/**** Fault Tolerance Option ****/
/* Note: A separate python script needs run to switch different
 * versions of interfaces, such as mmgr, schduler, cbuf, torrent. To
 * use it, run python set_symbolic_link.py under composite/tools */

#define RECOVERY_ENABLE 1               // 1 enable the fault notification in Composite kernel, 0 disable
/* for ramfs (eager recovery), set recovery_enable to 0 */
/* for mm (eager recovery), set recovery_enable to 0 or 1? */
/* for scheduler, this does not make difference, so set it as 1 when scheduler */
/* for te, we need detect the fault in te for these blocked threads */

/* For now, set INITONCE in cos_laoder and comment out boot_spd_caps in booter/fail function */
/* to avoid the overhead of re-capability. Need fix later */
#define LAZY_RECOVERY   1               // 1 will enable the lazy recovery, 0 for eager recovery
#define SWIFI_ENABLE    0		// 1 enable the fault injection (swifi component), 0 disable

#endif /* COS_CONFIG_H */
