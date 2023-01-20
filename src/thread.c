// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2023 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#include <linux/completion.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/mutex.h>

#include "thread.h"

bool ipts_thread_should_stop(struct ipts_thread *thread)
{
	bool ret = false;

	if (!thread)
		return false;

	mutex_lock(&thread->lock);
	ret = thread->should_stop;
	mutex_unlock(&thread->lock);

	return ret;
}

static int ipts_thread_runner(void *data)
{
	int ret = 0;
	struct ipts_thread *thread = data;

	if (!thread)
		return -EFAULT;

	if (!thread->threadfn)
		return -EFAULT;

	ret = thread->threadfn(thread);
	complete_all(&thread->done);

	return ret;
}

int ipts_thread_start(struct ipts_thread *thread, int (*threadfn)(struct ipts_thread *thread),
		      void *data, const char *name)
{
	if (!thread)
		return -EFAULT;

	if (!threadfn)
		return -EFAULT;

	mutex_init(&thread->lock);
	init_completion(&thread->done);

	thread->data = data;
	thread->should_stop = false;
	thread->threadfn = threadfn;

	thread->thread = kthread_run(ipts_thread_runner, thread, name);
	return PTR_ERR_OR_ZERO(thread->thread);
}

int ipts_thread_stop(struct ipts_thread *thread)
{
	int ret = 0;

	if (!thread)
		return -EFAULT;

	if (!thread->thread)
		return 0;

	mutex_lock(&thread->lock);
	thread->should_stop = true;
	mutex_unlock(&thread->lock);

	wait_for_completion(&thread->done);
	ret = kthread_stop(thread->thread);

	thread->thread = NULL;
	thread->data = NULL;
	thread->threadfn = NULL;

	return ret;
}
