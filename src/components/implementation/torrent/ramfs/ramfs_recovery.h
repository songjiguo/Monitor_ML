/* 
 *  Jiguo Song: This is the server side interface code to support the
 *  fault tolerance.  (I moved it from s_interface since a few lib functions)

 *  Mai purpose here is
 *  1) tracking uncovered file
 *  2) introspect cbuf if needed
 *  3) re-write the most recent meta data back to the correct file
 */

#include <torrent.h>
#include <uniq_map.h>
#include <cbuf.h>

/* when select these, do not forget to change c_stub.c */
/* use ramfs_rec.sh */
/* ramfs_trest1 nad ramfs_test2 in implementation/test */

#define TSPLIT_FAULT
//#define TWRITE_FAULT
//#define TREAD_FAULT
//#define TRELEASE_FAULT

/* create an inconsistency list, so we can tell later when read/write
 * on an un-recovered file */

struct tor_list {
	int fid;
	int tid;
	struct tor_list *next, *prev;
};

struct tor_list *all_tor_list = NULL;

static int __twmeta(spdid_t spdid, td_t td, int cbid, int sz, int offset, int flag);

static void
find_del(int fid)
{
	struct tor_list *item;
	assert(all_tor_list);
	
	for (item = FIRST_LIST(all_tor_list, next, prev);
	     item != all_tor_list;
	     item = FIRST_LIST(item, next, prev)){
		if (fid == item->fid) {
			REM_LIST(item, next, prev);
			break;
		}
	}

	for (item = FIRST_LIST(all_tor_list, next, prev);
	     item != all_tor_list;
	     item = FIRST_LIST(item, next, prev)){
		/* printc("after delete : fid in the list %d\n", item->fid); */
	}
	
	return;
}

static int
restore_tor(int fid, td_t tid)
{
	int ret = 0;
	int cbid = 0, sz = 0, offset = 0;
	int nth_cb = 0, total_cbs = 0;

	struct torrent *tor;
	tor  = tor_lookup(tid);
	assert(tor);
	/* printc("before meta done: tor->offset %d\n", tor->offset); */
	
	total_cbs = cbuf_c_introspect(cos_spd_id(), fid, 0, CBUF_INTRO_TOT);
	/* printc("total cbs %d\n", total_cbs); */

	while(total_cbs--) {
		nth_cb++;
		cbid   = cbuf_c_introspect(cos_spd_id(), fid, nth_cb, CBUF_INTRO_CBID);
		if (!cbid) goto no_found;
		sz     = cbuf_c_introspect(cos_spd_id(), fid, nth_cb, CBUF_INTRO_SIZE);
		if (!sz) goto no_found;
		offset = cbuf_c_introspect(cos_spd_id(), fid, nth_cb, CBUF_INTRO_OFFSET);
	
		/* printc("found cbid %d size %d offset %d\n", cbid, sz, offset); */
		/* printc("meta write:: tor id %d for fid %d\n", tid, fid); */
		__twmeta(cos_spd_id(), tid, cbid, sz, offset, 1);	/* 1 for recovery now */
	}

	/* printc("after meta done: tor->offset %d\n", tor->offset); */
	tor->offset = 0;
	find_del(fid);
	/* printc("[restore meta done]\n"); */
done:
	return ret;
no_found: 
	/* printc("Not found the record\n"); */
	ret = -1;
	goto done;
}



static void
find_restore(int tid)
{
	struct tor_list *item;
	assert(all_tor_list);

	for (item = FIRST_LIST(all_tor_list, next, prev);
	     item != all_tor_list;
	     item = FIRST_LIST(item, next, prev)){
		if (tid == item->tid) {
			restore_tor(item->fid, tid); /* tid should be always unique */
			return;
		}
	}
	return;
}

static void
find_update(int fid, int val)
{
	struct tor_list *item;
	assert(all_tor_list);
	
	for (item = FIRST_LIST(all_tor_list, next, prev);
	     item != all_tor_list;
	     item = FIRST_LIST(item, next, prev)){
		if (fid == item->fid) {
			item->tid = val;
			break;
		}
	}

	/* for (item = FIRST_LIST(all_tor_list, next, prev); */
	/*      item != all_tor_list; */
	/*      item = FIRST_LIST(item, next, prev)){ */
	/* 	printc("fid in the list %d has tid %d\n", item->fid, item->tid); */
	/* } */
	
	return;
}


static void
build_tor_list()
{
	int total_tor = 0, nth_fid = 0;
	int fid;
	struct tor_list *item;

	total_tor = cbuf_c_introspect(cos_spd_id(), 0, 0, CBUF_INTRO_TOT_TOR);
	/* printc("total file number %d\n", total_tor); */

	while(total_tor--) {
		nth_fid++;
		fid   = cbuf_c_introspect(cos_spd_id(), 0, nth_fid, CBUF_INTRO_FID);
		item  = (struct tor_list *)malloc(sizeof(struct tor_list));
		if (!item) assert(0);
		
		item->fid = fid;
		item->tid = 0;
		INIT_LIST(item, next, prev);
		
		if (!all_tor_list) { /* initialize the head */
			all_tor_list = (struct tor_list *)malloc(sizeof(struct tor_list));
			INIT_LIST(all_tor_list, next, prev);
		}
		
		ADD_LIST(all_tor_list, item, next, prev);
		
		/* printc("FID: %d\n", fid); */
	}
	return;
}


/* 
 * find the full path as the unique map index, e.g, "foo/bar/who" is
 * different from "foo/boo/who" /
*/

static char *
find_fullpath(td_t td)
{
	struct fsobj *item, *fsp;
	struct torrent *p;
	char *name, *str, *end, *t_end;
	unsigned int end_l;

	p  = tor_lookup(td);
	fsp = p->data;

	if (unlikely(!fsp->unique_path)) {
		/* printc("unique path does not exist...\n"); */
		name = str = "";
		item  = fsp->parent;
		/* printc("before item name %s\n", item->name5B); */
		if (item->name == "") name = fsp->name;
		else {
			t_end = strchr(item->name, '/');
			if (t_end) *t_end = '\0';
			/* printc("after item name %s\n", item->name); */

			while(item->name != "") {
				str = strcat(item->name, "/");
				name = item->name;
				item  = item->parent;
			}
			name = strcat(name, fsp->name);
		}
		fsp->unique_path = malloc(strlen(name));
		memcpy(fsp->unique_path, name, strlen(name));
	} else {
		name = fsp->unique_path;
	}
	/* printc("unique path %s in fn with size %d\n", name, strlen(name)); */

	return name;
}



