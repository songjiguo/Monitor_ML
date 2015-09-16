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

	STREAM_TEST                = (0x001 << 9),
	STREAM_MAX                 = (0x001 << 10),   // max 10 different type of streams
};

// in pages
#define	STREAM_TEST_BUFF             32
#define	STREAM_THD_EVT_SEQUENC_BUFF  32
#define	STREAM_THD_EXEC_TIMING_BUFF  32
#define	STREAM_THD_INTERACTION_BUFF  32
#define	STREAM_THD_CONTEX_SWCH_BUFF  32
#define	STREAM_SPD_INVOCATIONS_BUFF  32
#define	STREAM_SPD_EXEC_TIMING_BUFF  32

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

static void
print_mlmpthdevtseq_info(struct mlmp_thdevtseq_entry *mlmpevt)
{
	assert(mlmpevt);
	if (mlmpevt->thd_id) printc ("thread %d \n", mlmpevt->thd_id);
	if (mlmpevt->time_stamp) printc ("@ %llu: ", mlmpevt->time_stamp);
	if (mlmpevt->inv_from_spd) printc ("invoke from %d \n", mlmpevt->inv_from_spd);
	if (mlmpevt->inv_into_spd) printc ("invoke into %d \n", mlmpevt->inv_into_spd);
	if (mlmpevt->ret_from_spd) printc ("return from %d \n", mlmpevt->ret_from_spd);
	if (mlmpevt->ret_back_spd) printc ("return back %d \n", mlmpevt->ret_back_spd);
	return;
}

/*************************************/
struct mlmp_thdtime_entry {
	unsigned int thd_id;
	unsigned long long time_stamp;
};

#ifndef __MLMPRING_THDTIME_DEFINED
#define __MLMPRING_THDTIME_DEFINED
CK_RING(mlmp_thdtime_entry, mlmpthdtimebuffer_ring);
CK_RING_INSTANCE(mlmpthdtimebuffer_ring) *mlmpthdtime_ring;
#endif

static void
print_mlmpthdtime_info(struct mlmp_thdtime_entry *mlmpevt)
{
	assert(mlmpevt);
	if (mlmpevt->thd_id) printc ("thread %d \n", mlmpevt->thd_id);
	if (mlmpevt->time_stamp) printc ("@ %llu: ", mlmpevt->time_stamp);
	return;
}

/*************************************/

struct mlmp_thdinteract_entry {
	int thd_id;
};

#ifndef __MLMPRING_THDINTERACT_DEFINED
#define __MLMPRING_THDINTERACT_DEFINED
CK_RING(mlmp_thdinteract_entry, mlmpthdinteractbuffer_ring);
CK_RING_INSTANCE(mlmpthdinteractbuffer_ring) *mlmpthdinteract_ring;
#endif

/*************************************/

struct mlmp_thdcs_entry {
	int thd_id;
};

#ifndef __MLMPRING_THDCS_DEFINED
#define __MLMPRING_THDCS_DEFINED
CK_RING(mlmp_thdcs_entry, mlmpthdcsbuffer_ring);
CK_RING_INSTANCE(mlmpthdcsbuffer_ring) *mlmpthdcs_ring;
#endif


/*************************************/

struct mlmp_spdinvnum_entry {
	unsigned int spd_id;
	unsigned int invocation_nums;
};

#ifndef __MLMPRING_SPDINVNUM_DEFINED
#define __MLMPRING_SPDINVNUMx_DEFINED
CK_RING(mlmp_spdinvnum_entry, mlmpspdinvnumbuffer_ring);
CK_RING_INSTANCE(mlmpspdinvnumbuffer_ring) *mlmpspdinvnum_ring;
#endif

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
