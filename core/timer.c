/********************************************************
 * Filename: core/timer.c
 *
 * Author: wsyoo, RTOSLab. SNU.
 * 
 * Description: 
 ********************************************************/
#include <core/eos.h>

static eos_counter_t system_timer;

int8u_t eos_init_counter(eos_counter_t *counter, int32u_t init_value) {
	counter->tick = init_value;
	counter->alarm_queue = NULL;
	return 0;
}

void eos_set_alarm(eos_counter_t* counter, eos_alarm_t* alarm, int32u_t timeout, void (*entry)(void *arg), void *arg) {

	 _os_remove_node(&(counter->alarm_queue), &(alarm->alarm_queue_node));

	if(timeout == 0 || entry == NULL)
		return;

// set alarm
	alarm->timeout = timeout;
	alarm->handler = entry;
	alarm->arg = arg;

// insert alarm to counter
	alarm->alarm_queue_node.ptr_data = alarm;
	alarm->alarm_queue_node.priority = counter->tick + timeout;
	_os_add_node_priority(&(counter->alarm_queue), &(alarm->alarm_queue_node));
}

eos_counter_t* eos_get_system_timer() {
	return &system_timer;
}

void eos_trigger_counter(eos_counter_t* counter) {
//	PRINT("tick\n");
	counter->tick++;

	_os_node_t *alarm_node = counter->alarm_queue;
	_os_node_t *next_node;

	int istimeout = 0;

	while(alarm_node != NULL && counter->tick == alarm_node->priority){
		next_node = alarm_node->next;

		eos_alarm_t* alarm = (eos_alarm_t*)alarm_node->ptr_data;

		// dequeue alarm from alarm_queue of counter & call callback-function
		eos_set_alarm(counter, alarm, 0, NULL, NULL); // remove alarm
		alarm->handler(alarm->arg);
		istimeout = 1;

		if(alarm_node == next_node)	break; // only one alarm in alarm queue
		alarm_node = next_node;
	}

	if(istimeout)
		eos_schedule();
}

/* Timer interrupt handler */
static void timer_interrupt_handler(int8s_t irqnum, void *arg) {
	/* trigger alarms */
	eos_trigger_counter(&system_timer);
}

void _os_init_timer() {
	eos_init_counter(&system_timer, 0);

	/* register timer interrupt handler */
	eos_set_interrupt_handler(IRQ_INTERVAL_TIMER0, timer_interrupt_handler, NULL);
}
