/********************************************************
 * Filename: core/task.c
 * 
 * Author: parkjy, RTOSLab. SNU.
 * 
 * Description: task management.
 ********************************************************/
#include <core/eos.h>

#define READY		1
#define RUNNING		2
#define WAITING		3

/*
 * Queue (list) of tasks that are ready to run.
 */
static _os_node_t *_os_ready_queue[LOWEST_PRIORITY + 1];

/*
 * Pointer to TCB of running task
 */
static eos_tcb_t *_os_current_task;

int32u_t eos_create_task(eos_tcb_t *task, addr_t sblock_start, size_t sblock_size, void (*entry)(void *arg), void *arg, int32u_t priority) {

/* init TCB info */
	task->priority = priority;
	task->entry = entry;
	task->state = READY;

	task->period = 0;
	task->stack_base = sblock_start + sblock_size;
	task->stack_size = sblock_size;
	task->stack_ptr = _os_create_context(task->stack_base, task->stack_size, entry, arg);

/* regi TCB to TCB Array(Ready Queue) */
	task->rq_node.ptr_data = task;
	task->rq_node.priority = READY; // priority of rq_node can be used to seperate each states.
	// ready_queue[priority] --> (READYS...) -> (RUNNINGS...) -> (WAITINGS...)
	_os_add_node_priority(_os_ready_queue + priority, &(task->rq_node));
	_os_set_ready(priority);
/* info */
	PRINT("task: 0x%x, priority: %d\n", (int32u_t)task, priority);
}

int32u_t eos_destroy_task(eos_tcb_t *task) {
}

eos_tcb_t* get_next_task(){

	// dispatcher policy
	if(_os_multitasking){
		int32u_t 	i 	 	= _os_get_highest_priority();
		_os_node_t* 	_os_node 	= _os_ready_queue[i];
		eos_tcb_t*	_os_next_task 	= (eos_tcb_t*)_os_node->ptr_data; // it's always READY task.

		_os_remove_node(_os_ready_queue+i, _os_node);
		if(_os_ready_queue[i] == NULL || _os_ready_queue[i]->priority != READY){
			_os_unset_ready(i);
		}

		return _os_next_task;

	}
	return NULL;
}

void reposition_task(eos_tcb_t *task){

	// move task to rear of (<state> GROUP) of ready queue
	_os_remove_node(_os_ready_queue+task->priority, &(task->rq_node));
	task->rq_node.ptr_data = task;
	task->rq_node.priority = task->state;
	_os_add_node_priority(_os_ready_queue + task->priority, &(task->rq_node));

	// check multi level queue whether Queue has only WAITING, RUNNING state
	if(_os_ready_queue[task->priority]->priority == READY)
		_os_set_ready(task->priority);
	else
		_os_unset_ready(task->priority);
}

void eos_schedule() {
	static addr_t sp;

	// save task & check Comeback Routine
	if(_os_current_task != NULL && (sp = _os_save_context()) == NULL){
		return;
	}

	if(_os_current_task != NULL){
		_os_current_task->stack_ptr = sp;
		reposition_task(_os_current_task);
	}

	// change Current task
	_os_current_task = get_next_task();

	// restore task
	_os_restore_context(_os_current_task->stack_ptr);
}

void _os_schedule() {
	eos_schedule();
}

eos_tcb_t *eos_get_current_task() {
	return _os_current_task;
}

void eos_change_priority(eos_tcb_t *task, int32u_t priority) {
	task->priority = priority;
}

int32u_t eos_get_priority(eos_tcb_t *task) {
	return task->priority;
}

void eos_set_period(eos_tcb_t *task, int32u_t period){
	task->period = period;
}

int32u_t eos_get_period(eos_tcb_t *task) {
	return task->period;
}

int32u_t eos_suspend_task(eos_tcb_t *task) {
}

int32u_t eos_resume_task(eos_tcb_t *task) {
}

void eos_sleep(int32u_t tick) {

	eos_counter_t* 	counter		= eos_get_system_timer();
	eos_alarm_t* 	alarm		= &(_os_current_task->alarm);
	int32u_t 	timeout		= _os_current_task->period;
	void	 (*handler)(void *arg)	= _os_wakeup_sleeping_task;
	void 		*arg 		= _os_current_task;

	// set alerm
	eos_set_alarm(counter, alarm, timeout, handler, arg);

	// change task state to waiting
	_os_current_task->state = WAITING;

	// context switching
	eos_schedule();
}

void _os_init_task() {
	PRINT("initializing task module.\n");

	/* init current_task */
	_os_current_task = NULL;

	/* init multi-level ready_queue */
	int32u_t i;
	for (i = 0; i < LOWEST_PRIORITY; i++) {
		_os_ready_queue[i] = NULL;
	}
}

void _os_wait(_os_node_t **wait_queue) {
}

void _os_wakeup_single(_os_node_t **wait_queue, int32u_t queue_type) {
}

void _os_wakeup_all(_os_node_t **wait_queue, int32u_t queue_type) {
}

void _os_wakeup_sleeping_task(void *arg) {
	static eos_tcb_t* task;

	// get wakeup task
	task = (eos_tcb_t*)arg;

	// change task state to ready
	task->state = READY;
	reposition_task(task);
}
