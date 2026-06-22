/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM huge_memory

#if !defined(__HUGE_MEMORY_H) || defined(TRACE_HEADER_MULTI_READ)
#define __HUGE_MEMORY_H

#include  <linux/tracepoint.h>

#define SCAN_STATUS							\
	EM( SCAN_FAIL,			"failed")			\
	EM( SCAN_SUCCEED,		"succeeded")			\
	EM( SCAN_PMD_NULL,		"pmd_null")			\
	EM( SCAN_PMD_NONE,		"pmd_none")			\
	EM( SCAN_PMD_MAPPED,		"page_pmd_mapped")		\
	EM( SCAN_EXCEED_NONE_PTE,	"exceed_none_pte")		\
	EM( SCAN_EXCEED_SWAP_PTE,	"exceed_swap_pte")		\
	EM( SCAN_EXCEED_SHARED_PTE,	"exceed_shared_pte")		\
	EM( SCAN_PTE_NON_PRESENT,	"pte_non_present")		\
	EM( SCAN_PTE_UFFD_WP,		"pte_uffd_wp")			\
	EM( SCAN_PTE_MAPPED_HUGEPAGE,	"pte_mapped_hugepage")		\
	EM( SCAN_LACK_REFERENCED_PAGE,	"lack_referenced_page")		\
	EM( SCAN_PAGE_NULL,		"page_null")			\
	EM( SCAN_SCAN_ABORT,		"scan_aborted")			\
	EM( SCAN_PAGE_COUNT,		"not_suitable_page_count")	\
	EM( SCAN_PAGE_LRU,		"page_not_in_lru")		\
	EM( SCAN_PAGE_LOCK,		"page_locked")			\
	EM( SCAN_PAGE_ANON,		"page_not_anon")		\
	EM( SCAN_PAGE_COMPOUND,		"page_compound")		\
	EM( SCAN_ANY_PROCESS,		"no_process_for_page")		\
	EM( SCAN_VMA_NULL,		"vma_null")			\
	EM( SCAN_VMA_CHECK,		"vma_check_failed")		\
	EM( SCAN_ADDRESS_RANGE,		"not_suitable_address_range")	\
	EM( SCAN_DEL_PAGE_LRU,		"could_not_delete_page_from_lru")\
	EM( SCAN_ALLOC_HUGE_PAGE_FAIL,	"alloc_huge_page_failed")	\
	EM( SCAN_CGROUP_CHARGE_FAIL,	"ccgroup_charge_failed")	\
	EM( SCAN_TRUNCATED,		"truncated")			\
	EM( SCAN_PAGE_HAS_PRIVATE,	"page_has_private")		\
	EM( SCAN_STORE_FAILED,		"store_failed")			\
	EM( SCAN_COPY_MC,		"copy_poisoned_page")		\
	EMe(SCAN_PAGE_FILLED,		"page_filled")

#undef EM
#undef EMe
#define EM(a, b)	TRACE_DEFINE_ENUM(a);
#define EMe(a, b)	TRACE_DEFINE_ENUM(a);

SCAN_STATUS

#undef EM
#undef EMe
#define EM(a, b)	{a, b},
#define EMe(a, b)	{a, b}

TRACE_EVENT(mm_khugepaged_scan_pmd,

	TP_PROTO(struct mm_struct *mm, struct folio *folio,
		 int referenced, int none_or_zero, int status, int unmapped),

	TP_ARGS(mm, folio, referenced, none_or_zero, status, unmapped),

	TP_STRUCT__entry(
		__field(struct mm_struct *, mm)
		__field(unsigned long, pfn)
		__field(int, referenced)
		__field(int, none_or_zero)
		__field(int, status)
		__field(int, unmapped)
	),

	TP_fast_assign(
		__entry->mm = mm;
		__entry->pfn = folio ? folio_pfn(folio) : -1;
		__entry->referenced = referenced;
		__entry->none_or_zero = none_or_zero;
		__entry->status = status;
		__entry->unmapped = unmapped;
	),

	TP_printk("mm=%p, scan_pfn=0x%lx, referenced=%d, none_or_zero=%d, status=%s, unmapped=%d",
		__entry->mm,
		__entry->pfn,
		__entry->referenced,
		__entry->none_or_zero,
		__print_symbolic(__entry->status, SCAN_STATUS),
		__entry->unmapped)
);

TRACE_EVENT(mm_collapse_huge_page,

	TP_PROTO(struct mm_struct *mm, int isolated, int status),

	TP_ARGS(mm, isolated, status),

	TP_STRUCT__entry(
		__field(struct mm_struct *, mm)
		__field(int, isolated)
		__field(int, status)
	),

	TP_fast_assign(
		__entry->mm = mm;
		__entry->isolated = isolated;
		__entry->status = status;
	),

	TP_printk("mm=%p, isolated=%d, status=%s",
		__entry->mm,
		__entry->isolated,
		__print_symbolic(__entry->status, SCAN_STATUS))
);

TRACE_EVENT(mm_collapse_huge_page_isolate,

	TP_PROTO(struct folio *folio, int none_or_zero,
		 int referenced, int status),

	TP_ARGS(folio, none_or_zero, referenced, status),

	TP_STRUCT__entry(
		__field(unsigned long, pfn)
		__field(int, none_or_zero)
		__field(int, referenced)
		__field(int, status)
	),

	TP_fast_assign(
		__entry->pfn = folio ? folio_pfn(folio) : -1;
		__entry->none_or_zero = none_or_zero;
		__entry->referenced = referenced;
		__entry->status = status;
	),

	TP_printk("scan_pfn=0x%lx, none_or_zero=%d, referenced=%d, status=%s",
		__entry->pfn,
		__entry->none_or_zero,
		__entry->referenced,
		__print_symbolic(__entry->status, SCAN_STATUS))
);

TRACE_EVENT(mm_collapse_huge_page_swapin,

	TP_PROTO(struct mm_struct *mm, int swapped_in, int referenced, int ret),

	TP_ARGS(mm, swapped_in, referenced, ret),

	TP_STRUCT__entry(
		__field(struct mm_struct *, mm)
		__field(int, swapped_in)
		__field(int, referenced)
		__field(int, ret)
	),

	TP_fast_assign(
		__entry->mm = mm;
		__entry->swapped_in = swapped_in;
		__entry->referenced = referenced;
		__entry->ret = ret;
	),

	TP_printk("mm=%p, swapped_in=%d, referenced=%d, ret=%d",
		__entry->mm,
		__entry->swapped_in,
		__entry->referenced,
		__entry->ret)
);

TRACE_EVENT(mm_khugepaged_scan_file,

	TP_PROTO(struct mm_struct *mm, struct folio *folio, struct file *file,
		 int present, int swap, int result),

	TP_ARGS(mm, folio, file, present, swap, result),

	TP_STRUCT__entry(
		__field(struct mm_struct *, mm)
		__field(unsigned long, pfn)
		__string(filename, file->f_path.dentry->d_iname)
		__field(int, present)
		__field(int, swap)
		__field(int, result)
	),

	TP_fast_assign(
		__entry->mm = mm;
		__entry->pfn = folio ? folio_pfn(folio) : -1;
		__assign_str(filename);
		__entry->present = present;
		__entry->swap = swap;
		__entry->result = result;
	),

	TP_printk("mm=%p, scan_pfn=0x%lx, filename=%s, present=%d, swap=%d, result=%s",
		__entry->mm,
		__entry->pfn,
		__get_str(filename),
		__entry->present,
		__entry->swap,
		__print_symbolic(__entry->result, SCAN_STATUS))
);

TRACE_EVENT(mm_khugepaged_collapse_file,
	TP_PROTO(struct mm_struct *mm, struct folio *new_folio, pgoff_t index,
			unsigned long addr, bool is_shmem, struct file *file,
			int nr, int result),
	TP_ARGS(mm, new_folio, index, addr, is_shmem, file, nr, result),
	TP_STRUCT__entry(
		__field(struct mm_struct *, mm)
		__field(unsigned long, hpfn)
		__field(pgoff_t, index)
		__field(unsigned long, addr)
		__field(bool, is_shmem)
		__string(filename, file->f_path.dentry->d_iname)
		__field(int, nr)
		__field(int, result)
	),

	TP_fast_assign(
		__entry->mm = mm;
		__entry->hpfn = new_folio ? folio_pfn(new_folio) : -1;
		__entry->index = index;
		__entry->addr = addr;
		__entry->is_shmem = is_shmem;
		__assign_str(filename);
		__entry->nr = nr;
		__entry->result = result;
	),

	TP_printk("mm=%p, hpage_pfn=0x%lx, index=%ld, addr=%lx, is_shmem=%d, filename=%s, nr=%d, result=%s",
		__entry->mm,
		__entry->hpfn,
		__entry->index,
		__entry->addr,
		__entry->is_shmem,
		__get_str(filename),
		__entry->nr,
		__print_symbolic(__entry->result, SCAN_STATUS))
);

TRACE_EVENT(mm_folio_deferred_split,
	TP_PROTO(struct folio *folio, unsigned int reason),
	TP_ARGS(folio, reason),
	TP_STRUCT__entry(
		__field(unsigned long, pfn)
		__field(unsigned int, order)
		__field(unsigned int, reason)
	),
	TP_fast_assign(
		__entry->pfn = folio_pfn(folio);
		__entry->order = folio_order(folio);
		__entry->reason = reason;
	),
	TP_printk("pfn=0x%lx order=%u reason=%s",
		__entry->pfn, __entry->order,
		__print_symbolic(__entry->reason,
			{ 0, "PARTIALLY_MAPPED" }, { 1, "ZAP" },
			{ 2, "KHUGEPAGED" }))
);

TRACE_EVENT(mm_folio_split,
	TP_PROTO(struct folio *folio, unsigned int new_order,
		 int result, int reason, int is_anon),
	TP_ARGS(folio, new_order, result, reason, is_anon),
	TP_STRUCT__entry(
		__field(unsigned long, pfn)
		__field(unsigned int, order)
		__field(unsigned int, new_order)
		__field(int, result)
		__field(int, reason)
		__field(int, is_anon)
	),
	TP_fast_assign(
		__entry->pfn = folio_pfn(folio);
		__entry->order = folio_order(folio);
		__entry->new_order = new_order;
		__entry->result = result;
		__entry->reason = reason;
		__entry->is_anon = is_anon;
	),
	TP_printk("pfn=0x%lx order=%u new_order=%u result=%d reason=%s is_anon=%d",
		__entry->pfn, __entry->order, __entry->new_order,
		__entry->result,
		__print_symbolic(__entry->reason,
			{ 0, "UNKNOWN" }, { 1, "RECLAIM" }, { 2, "MADVISE" },
			{ 3, "MIGRATION" }, { 4, "UFFD" }, { 5, "MPROTECT" },
			{ 6, "MREMAP" }, { 7, "SWAP" }, { 8, "TRUNCATE" },
		{ 9, "DEFERRED" }),
		__entry->is_anon)
);

TRACE_EVENT(mm_madvise_dontneed,
	TP_PROTO(struct vm_area_struct *vma, unsigned long start,
		 unsigned long len),
	TP_ARGS(vma, start, len),
	TP_STRUCT__entry(
		__field(unsigned long, vma_start)
		__field(unsigned long, vma_end)
		__field(unsigned long, start)
		__field(unsigned long, len)
	),
	TP_fast_assign(
		__entry->vma_start = vma->vm_start;
		__entry->vma_end = vma->vm_end;
		__entry->start = start;
		__entry->len = len;
	),
	TP_printk("vma=0x%lx-0x%lx start=0x%lx len=%lu",
		__entry->vma_start, __entry->vma_end,
		__entry->start, __entry->len)
);

TRACE_EVENT(mm_folio_partial_unmap,
	TP_PROTO(struct folio *folio, struct page *page, int nr,
		 int nr_total),
	TP_ARGS(folio, page, nr, nr_total),
	TP_STRUCT__entry(
		__field(unsigned long, folio_pfn)
		__field(unsigned long, page_pfn)
		__field(int, nr)
		__field(int, nr_total)
	),
	TP_fast_assign(
		__entry->folio_pfn = folio_pfn(folio);
		__entry->page_pfn = page_to_pfn(page);
		__entry->nr = nr;
		__entry->nr_total = nr_total;
	),
	TP_printk("folio=0x%lx page=0x%lx(off=%ld) nr=%d/%d",
		__entry->folio_pfn, __entry->page_pfn,
		(long)(__entry->page_pfn - __entry->folio_pfn),
		__entry->nr, __entry->nr_total)
);

TRACE_EVENT(mm_shrink_partial_split,
	TP_PROTO(struct folio *folio),
	TP_ARGS(folio),
	TP_STRUCT__entry(
		__field(unsigned long, pfn)
		__field(unsigned int, order)
	),
	TP_fast_assign(
		__entry->pfn = folio_pfn(folio);
		__entry->order = folio_order(folio);
	),
	TP_printk("pfn=0x%lx order=%u reason=PARTIAL_RECLAIM",
		__entry->pfn, __entry->order)
);

TRACE_EVENT(mm_shrink_swap_split,
	TP_PROTO(struct folio *folio),
	TP_ARGS(folio),
	TP_STRUCT__entry(
		__field(unsigned long, pfn)
		__field(unsigned int, order)
	),
	TP_fast_assign(
		__entry->pfn = folio_pfn(folio);
		__entry->order = folio_order(folio);
	),
	TP_printk("pfn=0x%lx order=%u reason=SWAP_FALLBACK",
		__entry->pfn, __entry->order)
);

#endif /* __HUGE_MEMORY_H */
#include <trace/define_trace.h>
