/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Andrew Leech
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <stdlib.h>

#include "py/mpconfig.h"
#include "py/mperrno.h"
// #include "py/mphal.h"

#if MICROPY_PY_BLUETOOTH && MICROPY_BLUETOOTH_ZEPHYR

#ifndef NO_QSTR
#include <zephyr/kernel.h>
#include <soc.h>
#include <zephyr/logging/log_msg.h>
#endif

void k_timer_init(struct k_timer *timer,
			 k_timer_expiry_t expiry_fn,
			 k_timer_stop_t stop_fn)
{
	// timer->expiry_fn = expiry_fn;
	// timer->stop_fn = stop_fn;
	// timer->status = 0U;

	// if (IS_ENABLED(CONFIG_MULTITHREADING)) {
	// 	z_waitq_init(&timer->wait_q);
	// }

	// z_init_timeout(&timer->timeout);

	// SYS_PORT_TRACING_OBJ_INIT(k_timer, timer);

	// timer->user_data = NULL;

	// z_object_init(timer);
}

void z_impl_k_timer_start(struct k_timer *timer, k_timeout_t duration,
			  k_timeout_t period)
{
	// SYS_PORT_TRACING_OBJ_FUNC(k_timer, start, timer, duration, period);

	// if (K_TIMEOUT_EQ(duration, K_FOREVER)) {
	// 	return;
	// }

	// /* z_add_timeout() always adds one to the incoming tick count
	//  * to round up to the next tick (by convention it waits for
	//  * "at least as long as the specified timeout"), but the
	//  * period interval is always guaranteed to be reset from
	//  * within the timer ISR, so no round up is desired.  Subtract
	//  * one.
	//  *
	//  * Note that the duration (!) value gets the same treatment
	//  * for backwards compatibility.  This is unfortunate
	//  * (i.e. k_timer_start() doesn't treat its initial sleep
	//  * argument the same way k_sleep() does), but historical.  The
	//  * timer_api test relies on this behavior.
	//  */
	// if (!K_TIMEOUT_EQ(period, K_FOREVER) && period.ticks != 0 &&
	//     Z_TICK_ABS(period.ticks) < 0) {
	// 	period.ticks = MAX(period.ticks - 1, 1);
	// }
	// if (Z_TICK_ABS(duration.ticks) < 0) {
	// 	duration.ticks = MAX(duration.ticks - 1, 0);
	// }

	// (void)z_abort_timeout(&timer->timeout);
	// timer->period = period;
	// timer->status = 0U;

	// z_add_timeout(&timer->timeout, z_timer_expiration_handler,
	// 	     duration);
}

void z_impl_k_timer_stop(struct k_timer *timer)
{
	// SYS_PORT_TRACING_OBJ_FUNC(k_timer, stop, timer);

	// bool inactive = (z_abort_timeout(&timer->timeout) != 0);

	// if (inactive) {
	// 	return;
	// }

	// if (timer->stop_fn != NULL) {
	// 	timer->stop_fn(timer);
	// }

	// if (IS_ENABLED(CONFIG_MULTITHREADING)) {
	// 	struct k_thread *pending_thread = z_unpend1_no_timeout(&timer->wait_q);

	// 	if (pending_thread != NULL) {
	// 		z_ready_thread(pending_thread);
	// 		z_reschedule_unlocked();
	// 	}
	// }
}



void z_impl_k_yield(void)
{
	// __ASSERT(!arch_is_in_isr(), "");

	// SYS_PORT_TRACING_FUNC(k_thread, yield);

	// k_spinlock_key_t key = k_spin_lock(&sched_spinlock);

	// if (!IS_ENABLED(CONFIG_SMP) ||
	//     z_is_thread_queued(_current)) {
	// 	dequeue_thread(_current);
	// }
	// queue_thread(_current);
	// update_cache(1);
	// z_swap(&sched_spinlock, key);
}

void z_impl_k_sem_give(struct k_sem *sem)
{
	// k_spinlock_key_t key = k_spin_lock(&lock);
	// struct k_thread *thread;

	// SYS_PORT_TRACING_OBJ_FUNC_ENTER(k_sem, give, sem);

	// thread = z_unpend_first_thread(&sem->wait_q);

	// if (thread != NULL) {
	// 	arch_thread_return_value_set(thread, 0);
	// 	z_ready_thread(thread);
	// } else {
	// 	sem->count += (sem->count != sem->limit) ? 1U : 0U;
	// 	handle_poll_events(sem);
	// }

	// z_reschedule(&lock, key);

	// SYS_PORT_TRACING_OBJ_FUNC_EXIT(k_sem, give, sem);
}

void *z_impl_k_queue_get(struct k_queue *queue, k_timeout_t timeout)
{
	// k_spinlock_key_t key = k_spin_lock(&queue->lock);
	// void *data;

	// SYS_PORT_TRACING_OBJ_FUNC_ENTER(k_queue, get, queue, timeout);

	// if (likely(!sys_sflist_is_empty(&queue->data_q))) {
	// 	sys_sfnode_t *node;

	// 	node = sys_sflist_get_not_empty(&queue->data_q);
	// 	data = z_queue_node_peek(node, true);
	// 	k_spin_unlock(&queue->lock, key);

	// 	SYS_PORT_TRACING_OBJ_FUNC_EXIT(k_queue, get, queue, timeout, data);

	// 	return data;
	// }

	// SYS_PORT_TRACING_OBJ_FUNC_BLOCKING(k_queue, get, queue, timeout);

	// if (K_TIMEOUT_EQ(timeout, K_NO_WAIT)) {
	// 	k_spin_unlock(&queue->lock, key);

	// 	SYS_PORT_TRACING_OBJ_FUNC_EXIT(k_queue, get, queue, timeout, NULL);

	// 	return NULL;
	// }

	// int ret = z_pend_curr(&queue->lock, key, &queue->wait_q, timeout);

	// SYS_PORT_TRACING_OBJ_FUNC_EXIT(k_queue, get, queue, timeout,
	// 	(ret != 0) ? NULL : _current->base.swap_data);

	return (ret != 0) ? NULL : _current->base.swap_data;
}


int z_impl_k_thread_name_set(struct k_thread *thread, const char *value)
{
// #ifdef CONFIG_THREAD_NAME
// 	if (thread == NULL) {
// 		thread = _current;
// 	}

// 	strncpy(thread->name, value, CONFIG_THREAD_MAX_NAME_LEN - 1);
// 	thread->name[CONFIG_THREAD_MAX_NAME_LEN - 1] = '\0';

// 	SYS_PORT_TRACING_OBJ_FUNC(k_thread, name_set, thread, 0);

// 	return 0;
// #else
	ARG_UNUSED(thread);
	ARG_UNUSED(value);

	SYS_PORT_TRACING_OBJ_FUNC(k_thread, name_set, thread, -ENOSYS);

	return -ENOSYS;
// #endif /* CONFIG_THREAD_NAME */
}

void z_fatal_error(unsigned int reason, const z_arch_esf_t *esf) {
}

void posix_exit(int exit_code)
{
	static int max_exit_code;

	max_exit_code = MAX(exit_code, max_exit_code);
	/*
	 * posix_soc_clean_up may not return if this is called from a SW thread,
	 * but instead it would get posix_exit() recalled again
	 * ASAP from the HW thread
	 */
	// posix_soc_clean_up();
	// hwm_cleanup();
	// native_cleanup_cmd_line();
	exit(max_exit_code);
}


void z_log_dropped(bool buffered)
{
	// atomic_inc(&dropped_cnt);
	// if (buffered) {
	// 	atomic_dec(&buffered_cnt);
	// }
}

void z_log_msg_commit(struct log_msg *msg)
{
	// union log_msg_generic *m = (union log_msg_generic *)msg;

	// msg->hdr.timestamp = timestamp_func();

	// if (IS_ENABLED(CONFIG_LOG_MODE_IMMEDIATE)) {
	// 	msg_process(m);

	// 	return;
	// }

	// mpsc_pbuf_commit(&log_buffer, &m->buf);
	// z_log_msg_post_finalize();
}


#endif
