/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef _KCOMPRESSD_H_
#define _KCOMPRESSD_H_

#include <linux/rwsem.h>
#include <linux/kfifo.h>
#include <linux/atomic.h>

typedef void (*compress_callback)(void *mem, struct bio *bio);

struct kcompress {
	struct task_struct *kcompressd;
	wait_queue_head_t kcompressd_wait;
	struct kfifo write_fifo;
	atomic_t running;
	/*
	 * Producer serialization gate for the SPSC kfifo.
	 *
	 * kfifo is only safe for a single producer; schedule_bio_write() may
	 * be called concurrently from any direct-reclaim / shmem-writeback
	 * context, so producers must serialize the "check space + copy entry"
	 * critical section. 1 = a producer holds the gate (queuing in
	 * progress); 0 = free. Consumers (kcompressd) do not touch it.
	 */
	atomic_t producer_busy;
};

int kcompressd_enabled(void);
int schedule_bio_write(void *mem, struct bio *bio, compress_callback cb);
#endif
