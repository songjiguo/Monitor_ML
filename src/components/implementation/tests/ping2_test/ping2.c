#include <cos_component.h>
#include <print.h>
#include <cos_component.h>
#include <sched.h>
#include <pong_test.h>

#include <pingpong_test.h>

#define NUM 6

void ping2(void){
	int i = 0;
	printc("PING2 is called through\n");
	while(i++ < NUM) pong();
	return;
}

