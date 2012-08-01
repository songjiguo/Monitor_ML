//#define SIMPLE_PPONG
#define TEST_SWITCH_RECOVERY
//#define TEST_RETURN_RECOVERY
//#define TEST_INVOCATION_RECOVERY


//#define TEST_INTERRUPT_RECOVERY
/* interrupt case: 

1) switch to previous interrupted thread, this is similar to the switch case above
2) upcall to the scheduler and let it decide the next thread to run (how scheduler knows if spd has failed? )
3) execute a pending upcall (what is this? tailcall?)

Q: how to make a test case using pingpong?

*/

