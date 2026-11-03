/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM zram

#if !defined(_TRACE_ZRAM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_ZRAM_H

#include <linux/tracepoint.h>

TRACE_EVENT(zram_partial_read,
	TP_PROTO(u32 zhead, unsigned int zoff),
	TP_ARGS(zhead, zoff),

	TP_STRUCT__entry(
		__field(u32, zhead)
		__field(unsigned int, zoff)
	),

	TP_fast_assign(
		__entry->zhead = zhead;
		__entry->zoff = zoff;
	),

	TP_printk("head=%u offset=%u", __entry->zhead, __entry->zoff)
);

#endif /* _TRACE_ZRAM_H */

#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE zram_trace
#include <trace/define_trace.h>
