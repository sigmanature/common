/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mmap

#if !defined(_TRACE_MMAP_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MMAP_H

#include <linux/tracepoint.h>

#define MMAP_VMA_TYPE_ANON	0
#define MMAP_VMA_TYPE_SHMEM	1
#define MMAP_VMA_TYPE_FILE	2

#define show_mmap_vma_type(type)					\
	__print_symbolic(type,						\
		{ MMAP_VMA_TYPE_ANON,	"anon" },			\
		{ MMAP_VMA_TYPE_SHMEM,	"shmem" },			\
		{ MMAP_VMA_TYPE_FILE,	"file" })

TRACE_EVENT(vm_unmapped_area,

	TP_PROTO(unsigned long addr, struct vm_unmapped_area_info *info),

	TP_ARGS(addr, info),

	TP_STRUCT__entry(
		__field(unsigned long,	addr)
		__field(unsigned long,	total_vm)
		__field(unsigned long,	flags)
		__field(unsigned long,	length)
		__field(unsigned long,	low_limit)
		__field(unsigned long,	high_limit)
		__field(unsigned long,	align_mask)
		__field(unsigned long,	align_offset)
	),

	TP_fast_assign(
		__entry->addr = addr;
		__entry->total_vm = current->mm->total_vm;
		__entry->flags = info->flags;
		__entry->length = info->length;
		__entry->low_limit = info->low_limit;
		__entry->high_limit = info->high_limit;
		__entry->align_mask = info->align_mask;
		__entry->align_offset = info->align_offset;
	),

	TP_printk("addr=0x%lx err=%ld total_vm=0x%lx flags=0x%lx len=0x%lx lo=0x%lx hi=0x%lx mask=0x%lx ofs=0x%lx",
		IS_ERR_VALUE(__entry->addr) ? 0 : __entry->addr,
		IS_ERR_VALUE(__entry->addr) ? __entry->addr : 0,
		__entry->total_vm, __entry->flags, __entry->length,
		__entry->low_limit, __entry->high_limit, __entry->align_mask,
		__entry->align_offset)
);

TRACE_EVENT(exit_mmap,
	TP_PROTO(struct mm_struct *mm),

	TP_ARGS(mm),

	TP_STRUCT__entry(
			__field(struct mm_struct *, mm)
			__field(struct maple_tree *, mt)
	),

	TP_fast_assign(
		       __entry->mm		= mm;
		       __entry->mt		= &mm->mm_mt;
	),

	TP_printk("mt_mod %p, DESTROY",
		  __entry->mt
	)
);

TRACE_EVENT(mmap_fixed_unaligned,
	TP_PROTO(unsigned long addr, unsigned long len, int vma_type),
	TP_ARGS(addr, len, vma_type),
	TP_STRUCT__entry(
		__field(unsigned long, addr)
		__field(unsigned long, len)
		__field(int, vma_type)
	),
	TP_fast_assign(
		__entry->addr = addr;
		__entry->len = len;
		__entry->vma_type = vma_type;
	),
	TP_printk("addr=0x%lx len=%lu vma_type=%s offset=0x%lx",
		__entry->addr, __entry->len,
		show_mmap_vma_type(__entry->vma_type),
		__entry->addr & ((PAGE_SIZE << 2) - 1))
);

TRACE_EVENT(mmap_unmapped_area_unaligned,
	TP_PROTO(unsigned long addr, unsigned long len,
		 int start_bad, int end_bad),
	TP_ARGS(addr, len, start_bad, end_bad),
	TP_STRUCT__entry(
		__field(unsigned long, addr)
		__field(unsigned long, len)
		__field(int, start_bad)
		__field(int, end_bad)
	),
	TP_fast_assign(
		__entry->addr = addr;
		__entry->len = len;
		__entry->start_bad = start_bad;
		__entry->end_bad = end_bad;
	),
	TP_printk("addr=0x%lx len=%lu start_bad=%d end_bad=%d",
		__entry->addr, __entry->len,
		__entry->start_bad, __entry->end_bad)
);

TRACE_EVENT(dup_mmap_vma_unaligned,
	TP_PROTO(unsigned long vm_start, unsigned long vm_end,
		 unsigned long vm_flags, pid_t parent_pid,
		 const char *parent_comm),
	TP_ARGS(vm_start, vm_end, vm_flags, parent_pid, parent_comm),
	TP_STRUCT__entry(
		__field(unsigned long, vm_start)
		__field(unsigned long, vm_end)
		__field(unsigned long, vm_flags)
		__field(pid_t, parent_pid)
		__string(parent_comm, parent_comm)
	),
	TP_fast_assign(
		__entry->vm_start = vm_start;
		__entry->vm_end = vm_end;
		__entry->vm_flags = vm_flags;
		__entry->parent_pid = parent_pid;
		__assign_str(parent_comm);
	),
	TP_printk("vm_start=0x%lx vm_end=0x%lx vm_flags=0x%lx parent_pid=%d parent_comm=%s start_off=0x%lx end_off=0x%lx",
		__entry->vm_start, __entry->vm_end, __entry->vm_flags,
		__entry->parent_pid, __get_str(parent_comm),
		__entry->vm_start & ((PAGE_SIZE << 2) - 1),
		__entry->vm_end & ((PAGE_SIZE << 2) - 1))
);

#endif

/* This part must be outside protection */
#include <trace/define_trace.h>
