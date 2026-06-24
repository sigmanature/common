/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM page_alloc

#if !defined(_TRACE_PAGE_ALLOC_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_PAGE_ALLOC_H

#include <linux/types.h>
#include <linux/tracepoint.h>

TRACE_EVENT(alloc_stall_lowwatermark,

	TP_PROTO(struct zone *zone, unsigned int order,
		 unsigned long mark, long free_pages,
		 unsigned int alloc_flags),

	TP_ARGS(zone, order, mark, free_pages, alloc_flags),

	TP_STRUCT__entry(
		__field(	int,		node_id		)
		__string(	name,		zone->name	)
		__field(	unsigned int,	order		)
		__field(	unsigned long,	mark		)
		__field(	long,		free_pages	)
		__field(	unsigned long,	gfp_flags	)
		__field(	pid_t,		pid		)
		__array(	char,		comm,	TASK_COMM_LEN	)
	),

	TP_fast_assign(
		__entry->node_id	= zone_to_nid(zone);
		__assign_str(name);
		__entry->order		= order;
		__entry->mark		= mark;
		__entry->free_pages	= free_pages;
		__entry->gfp_flags	= alloc_flags;
		__entry->pid		= current->pid;
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
	),

	TP_printk("node=%d zone=%s order=%u mark=%lu free=%ld gfp_flags=%#lx pid=%d comm=%s",
		__entry->node_id,
		__get_str(name),
		__entry->order,
		__entry->mark,
		__entry->free_pages,
		__entry->gfp_flags,
		__entry->pid,
		__entry->comm)
);

TRACE_EVENT(alloc_stall_fragment,

	TP_PROTO(struct zone *zone, unsigned int order,
		 unsigned int alloc_flags),

	TP_ARGS(zone, order, alloc_flags),

	TP_STRUCT__entry(
		__field(	int,		node_id		)
		__string(	name,		zone->name	)
		__field(	unsigned int,	order		)
		__field(	unsigned long,	gfp_flags	)
		__field(	pid_t,		pid		)
		__array(	char,		comm,	TASK_COMM_LEN	)
	),

	TP_fast_assign(
		__entry->node_id	= zone_to_nid(zone);
		__assign_str(name);
		__entry->order		= order;
		__entry->gfp_flags	= alloc_flags;
		__entry->pid		= current->pid;
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
	),

	TP_printk("node=%d zone=%s order=%u gfp_flags=%#lx pid=%d comm=%s",
		__entry->node_id,
		__get_str(name),
		__entry->order,
		__entry->gfp_flags,
		__entry->pid,
		__entry->comm)
);

#endif /* _TRACE_PAGE_ALLOC_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
