#include <cos_component.h>
#include <evt.h>

long __sg_evt_create(spdid_t spdid)
{
	return evt_create(spdid);
}
