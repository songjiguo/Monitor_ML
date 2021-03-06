#ifndef MULTIPLEXER_H
#define MULTIPLEXER_H

// only called from fkml
vaddr_t multiplexer_init(spdid_t spdid, vaddr_t addr, int npages, int stream_type);
int multiplexer_retrieve_data(spdid_t spdid, int streams);

/**************************
  CRA ML Component        -- mlmp
**************************/
/* entry in the ring buffer between the ML component and the
 * Multiplexer component (stream....) */

enum stream_flags {
	STREAM_THD_EVT_SEQUENC     = (0x001 << 0),
	STREAM_THD_EXEC_TIMING     = (0x001 << 1),
	STREAM_THD_INTERACTION     = (0x001 << 2),
	STREAM_THD_CONTEX_SWCH     = (0x001 << 3),
	STREAM_SPD_INVOCATIONS     = (0x001 << 4),
	STREAM_SPD_EXEC_TIMING     = (0x001 << 5),

	STREAM_MAX                 = (0x001 << 10),   // max 10 different type of streams
};

// in pages
#define	STREAM_THD_EVT_SEQUENC_BUFF  32
#define	STREAM_THD_EXEC_TIMING_BUFF  8
#define	STREAM_THD_INTERACTION_BUFF  8
#define	STREAM_THD_CONTEX_SWCH_BUFF  8
#define	STREAM_SPD_INVOCATIONS_BUFF  8
#define	STREAM_SPD_EXEC_TIMING_BUFF  8

/************ stream 0 ***************/
/*************************************/
struct mlmp_entry {
	int para1;
	int para2;
	int para3;
	unsigned long long time_stamp;
};

#ifndef __MLMPRING_DEFINED
#define __MLMPRING_DEFINED
CK_RING(mlmp_entry, mlmpbuffer_ring);
CK_RING_INSTANCE(mlmpbuffer_ring) *mlmp_ring;
#endif

static void
print_mlmpentry_info(struct mlmp_entry *entry)
{
	assert(entry);
	printc("mlmp: para1 (%d) -- ", entry->para1);
	printc("para2 (%d) -- ", entry->para2);
	printc("para3 (%d) -- ", entry->para3);
	printc("time_stamp (%llu) \n", entry->time_stamp);
	
	return;
}

static void
mlmpevent_copy(struct mlmp_entry *to, struct mlmp_entry *from)
{
	assert(to && from);

	to->para1 = from->para1;
	to->para2 = from->para2;
	to->para3 = from->para3;
	to->time_stamp = from->time_stamp;

	return;
}

static int
notempty(struct mlmp_entry *entry)
{
	assert(entry);
	
	if (entry->para1 || entry->para2 || entry->para3
	    || entry->time_stamp) return 1;
	else return 0;
}

/*************************************/

struct mlmp_thdevtseq_entry {
	int thd_id;
	int inv_from_spd;
	int inv_into_spd;
	int ret_from_spd;
	int ret_back_spd;
	unsigned long long time_stamp;
};

#ifndef __MLMPRING_THDEVTSEQ_DEFINED
#define __MLMPRING_THDEVTSEQ_DEFINED
CK_RING(mlmp_thdevtseq_entry, mlmpthdevtseqbuffer_ring);
CK_RING_INSTANCE(mlmpthdevtseqbuffer_ring) *mlmpthdevtseq_ring;
#endif

// event sequence: S:
// thd,(x-> or ->x or <-y or y<-)@ttttt for "thread,inv, inv, ret and ret at time"

static void
print_mlmpthdevtseq_info(struct mlmp_thdevtseq_entry *mlmpevt)
{
	assert(mlmpevt);
	if (mlmpevt->thd_id) {
		printc ("S:%d,", mlmpevt->thd_id);
		if (mlmpevt->inv_from_spd) printc ("%d-> ", mlmpevt->inv_from_spd);
		if (mlmpevt->inv_into_spd) printc ("->%d ", mlmpevt->inv_into_spd);
		if (mlmpevt->ret_from_spd) printc ("<-%d ", mlmpevt->ret_from_spd);
		if (mlmpevt->ret_back_spd) printc ("%d<- ", mlmpevt->ret_back_spd);
		if (mlmpevt->time_stamp) printc ("@%llu\n", mlmpevt->time_stamp);
	}
	return;
}

/*************************************/
struct mlmp_thdtime_entry {
	unsigned int thd_id;
	unsigned long long exec_max;
	unsigned long long deadline_start;
};

#ifndef __MLMPRING_THDTIME_DEFINED
#define __MLMPRING_THDTIME_DEFINED
CK_RING(mlmp_thdtime_entry, mlmpthdtimebuffer_ring);
CK_RING_INSTANCE(mlmpthdtimebuffer_ring) *mlmpthdtime_ring;
#endif

// thread execution time: E:
// thd,xxxxx@ttttt for "thread executed xxxxx since ttttt"

static void
print_mlmpthdtime_info(struct mlmp_thdtime_entry *mlmpevt)
{
	assert(mlmpevt);
	if (mlmpevt->thd_id != 19) {   // hardcode to remove normal test, change back later
		printc ("E:%d,", mlmpevt->thd_id);
		if (mlmpevt->exec_max) printc ("%llu", mlmpevt->exec_max);
		if (mlmpevt->deadline_start) printc ("@%llu\n", mlmpevt->deadline_start);
	}
	return;
}

/*************************************/

struct mlmp_thdinteract_entry {
	int curr_thd;
	int int_thd;
	int int_spd;
	unsigned long long time_stamp;
};

#ifndef __MLMPRING_THDINTERACT_DEFINED
#define __MLMPRING_THDINTERACT_DEFINED
CK_RING(mlmp_thdinteract_entry, mlmpthdinteractbuffer_ring);
CK_RING_INSTANCE(mlmpthdinteractbuffer_ring) *mlmpthdinteract_ring;
#endif

// interrupts: I:
// thd,int_thd,spd@ttttt for "thd is interrupted by int_thd from spd at tttttt"

static void
print_mlmpthdinteract_info(struct mlmp_thdinteract_entry *mlmpevt)
{
	assert(mlmpevt);
	if (mlmpevt->curr_thd) {
		printc ("I:%d,", mlmpevt->curr_thd);
		if (mlmpevt->int_thd) printc ("%d,", mlmpevt->int_thd);
		if (mlmpevt->int_spd) printc ("%d", mlmpevt->int_spd);
		if (mlmpevt->time_stamp) printc ("@%llu\n", mlmpevt->time_stamp);
	}
	return;
}
/*************************************/

struct mlmp_thdcs_entry {
	int from_thd;
	int to_thd;
	unsigned long long time_stamp;
};

#ifndef __MLMPRING_THDCS_DEFINED
#define __MLMPRING_THDCS_DEFINED
CK_RING(mlmp_thdcs_entry, mlmpthdcsbuffer_ring);
CK_RING_INSTANCE(mlmpthdcsbuffer_ring) *mlmpthdcs_ring;
#endif

// context switch: C:
// from_thd,to_thd@ttttt for "from_thd is switching to to_thd at tttttt"

static void
print_mlmpthdcs_info(struct mlmp_thdcs_entry *mlmpevt)
{
	assert(mlmpevt);
	if (mlmpevt->from_thd) {
		printc ("C:%d,", mlmpevt->from_thd);
		if (mlmpevt->to_thd) printc ("%d,", mlmpevt->to_thd);
		if (mlmpevt->time_stamp) printc ("@%llu\n", mlmpevt->time_stamp);
	}
	return;
}

/*************************************/

struct mlmp_spdinvnum_entry {
	unsigned int thd_id;
	unsigned int invnum;
	unsigned int spd;
};

#ifndef __MLMPRING_SPDINVNUM_DEFINED
#define __MLMPRING_SPDINVNUMx_DEFINED
CK_RING(mlmp_spdinvnum_entry, mlmpspdinvnumbuffer_ring);
CK_RING_INSTANCE(mlmpspdinvnumbuffer_ring) *mlmpspdinvnum_ring;
#endif

// spd invocation number: V:
// thd,spd,xxxx for "thd has invoked spd for xxxx times"

static void
print_mlmpspdinvnum_info(struct mlmp_spdinvnum_entry *mlmpevt)
{
	assert(mlmpevt);
	//TODO: fix the overflow in spdinvnum later
	if (mlmpevt->invnum > 0 && mlmpevt->invnum < 10000 && mlmpevt->spd < 28) {
		printc ("V:%d,", mlmpevt->thd_id);
		if (mlmpevt->spd) printc ("%d,", mlmpevt->spd);
		if (mlmpevt->invnum) printc ("%d\n", mlmpevt->invnum);
	}

	return;
}

/*************************************/

struct mlmp_spdexec_entry {
	unsigned int spd_id;
	unsigned int spd_evt_seq[1024];
};

#ifndef __MLMPRING_SPDEXEC_DEFINED
#define __MLMPRING_SPDEXEC_DEFINED
CK_RING(mlmp_spdexec_entry, mlmpspdexecbuffer_ring);
CK_RING_INSTANCE(mlmpspdexecbuffer_ring) *mlmpspdexec_ring;
#endif

#endif /* MULTIPLEXER_H */
