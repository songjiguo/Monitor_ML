/**
 Jiguo: a component that holds the mapping of file unique file id and
 its name (torrent path, tcp connection.....)
 */

#include <cos_component.h>
#include <sched.h>
#include <cos_synchronization.h>
#include <print.h>
#include <cos_alloc.h>
#include <cos_map.h>
#include <cos_list.h>
#include <cos_debug.h>
#include <cbuf.h>
#include <mem_mgr_large.h>
#include <valloc.h>

#include <uniq_map.h>

static cos_lock_t uniq_map_lock;
#define LOCK() if (lock_take(&uniq_map_lock)) BUG();
#define UNLOCK() if (lock_release(&uniq_map_lock)) BUG();

typedef int uid;

COS_MAP_CREATE_STATIC(uniq_map_ids);

struct obj_id {
	uid id;
	char *name;
	struct obj_id *next, *prev;
};

struct obj_id obj_id_list;

/* public function. Only used by torrent for now, could be used by network... */
uid
uniq_map_set(spdid_t spdid, cbuf_t cb, int sz)
{
	uid ret_uid = 0;
	struct obj_id *item;
	char *str, *param;

	LOCK();

	str = (char *)cbuf2buf(cb, sz);
	assert(str);

	/* printc("get: string passed in size  %d\n", sz); */
	/* printc("get: string passed in  %s\n", str); */

	param = (char *)malloc(sz*sizeof(char));
	if (!param) {
		printc("failed to malloc\n");
		BUG();
	}
	memcpy(param, str, sz);

	/* printc("get after copy : string passed in  %s\n", param); */

	item  = (struct obj_id *)malloc(sizeof(struct obj_id));
	if (!item) {
		printc("failed to malloc\n");
		BUG();
	}
	
	/* unique id, starts from 0 */
	ret_uid = (uid)cos_map_add(&uniq_map_ids, item);
	
	item->name = param;
	item->id   = ret_uid;
	INIT_LIST(item, next, prev);
	ADD_LIST(&obj_id_list, item, next, prev);

	UNLOCK();

	/* printc("get fid for str %s\n", str); */
	return ret_uid;
}

/* lookup uid from the name string */
uid
uniq_map_lookup(spdid_t spdid, cbuf_t cb, int sz)
{
	uid ret = -1;
	struct obj_id *item, *list;
	char *str, *d_str;
	
	LOCK();
	
	str = (char *)cbuf2buf(cb, sz);
	assert(str);
	*(str+sz) = '\0';  	/* previous cbuf might still contain data */

	/* printc("look up string passed in size  %d\n", sz); */
	/* printc("look up string passed in  %s\n", str); */
	list = &obj_id_list;
	for (item = list->next;
	     item != list;
	     item = item->next){
		d_str = item->name;
		if (!strcmp(d_str, str)) {
			ret = item->id;
			/* printc("look up match %s\n", str); */
			break;
		}
	}

	/* printc("uniq_map_lookup  %d\n", ret); */

	UNLOCK();

	return ret;
}

void
uniq_map_del(spdid_t spdid, uid id)
{
	struct obj_id *item, *list;

	LOCK();

	cos_map_del(&uniq_map_ids, id);

	list = &obj_id_list;
	for (item = list->next;
	     item != list;
	     item = item->next){
		if (item->id == id) {
			REM_LIST(item, next, prev);
			break;
		}
	}

	UNLOCK();

     	return;
}


void 
cos_init(void *d)
{
	lock_static_init(&uniq_map_lock);
     
	cos_map_init_static(&uniq_map_ids);
	memset(&obj_id_list, 0, sizeof(struct obj_id)); /* initialize the head */
	INIT_LIST(&obj_id_list, next, prev);
	
	return;
}
 
