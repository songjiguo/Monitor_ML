#ifndef LOG_HARD_CODE
#define LOG_HARD_CODE

/*
  Hard code some spd, thread and function name here. 
  TODO: use name_space to solve this
*/

// Interrupt Threads (high priority)
#define TIMER_THD	6
#define NETWORK_THD	20

// Components
#define LOCK_SPD        7
#define SCHED_SPD	3
#define NETIF_SPD	25

// Function names
// TODO: define this for sched_block, sched_wakeup, lock_tak, lock_release


/* Borrow these from scheduler */
#define CPU_FREQUENCY  (CPU_GHZ*1000000000)
#define CYC_PER_USEC   (CPU_FREQUENCY/1000000)
#define TIMER_FREQ     (CPU_TIMER_FREQ)
#define CYC_PER_TICK   (CPU_FREQUENCY/TIMER_FREQ)
#define MS_PER_TICK    (1000/TIMER_FREQ)
#define USEC_PER_TICK  (1000*MS_PER_TICK)

#define NUM_PRIOS    32
#define PRIO_LOWEST  (NUM_PRIOS-1)
#define PRIO_HIGHEST 0

#endif /* LOG_HARD_CODE */
