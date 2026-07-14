/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_MTHP_ALLOC_COUNTER_H
#define _LINUX_MTHP_ALLOC_COUNTER_H

#include <linux/gfp.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/string.h>
#include <linux/vmstat.h>

enum mthp_vma_alloc_source {
	MTHP_VMA_ALLOC_ANON_FAULT,
	MTHP_VMA_ALLOC_SWAPIN,
	MTHP_VMA_ALLOC_COW,
	MTHP_VMA_ALLOC_FILE_COW,
	MTHP_VMA_ALLOC_UFFD_COPY,
	MTHP_VMA_ALLOC_UFFD_ZEROPAGE,
	MTHP_VMA_ALLOC_KSM_COPY,
	MTHP_VMA_ALLOC_FORK_COPY,
};

enum mthp_file_alloc_source {
	MTHP_FILE_ALLOC_UNKNOWN,
	MTHP_FILE_ALLOC_READAHEAD,
	MTHP_FILE_ALLOC_GETFOLIO_MMAP,
	MTHP_FILE_ALLOC_GETFOLIO_WRITE,
	MTHP_FILE_ALLOC_GETFOLIO_OTHER,
	MTHP_FILE_ALLOC_BUFFERED_READ,
	MTHP_FILE_ALLOC_READ_CACHE,
	MTHP_FILE_ALLOC_EROFS,
	MTHP_FILE_ALLOC_F2FS_COMPRESS,
	MTHP_FILE_ALLOC_BTRFS,
	MTHP_FILE_ALLOC_FS_MISC,
};

enum mthp_vma_alloc_class {
	MTHP_VMA_CLASS_DALVIK,
	MTHP_VMA_CLASS_SCUDO,
	MTHP_VMA_CLASS_BIONIC,
	MTHP_VMA_CLASS_MTHP_VMA,
	MTHP_VMA_CLASS_HEAP,
	MTHP_VMA_CLASS_STACK,
	MTHP_VMA_CLASS_ANON_SHMEM,
	MTHP_VMA_CLASS_FILE_PRIVATE,
	MTHP_VMA_CLASS_OTHER,
};

enum mthp_order2_diag_reason {
	MTHP_ORDER2_DIAG_VMA_UNSUITABLE,
	MTHP_ORDER2_DIAG_PTE_OCCUPIED,
	MTHP_ORDER2_DIAG_ALLOC_FAIL,
	MTHP_ORDER2_DIAG_MEMCG_FAIL,
	MTHP_ORDER2_DIAG_UFFD_BLOCKED,
};

enum mthp_residual_order0_source {
	MTHP_RESIDUAL_ORDER0_BINDER_BUFFER,
	MTHP_RESIDUAL_ORDER0_TLB_GATHER_BATCH,
	MTHP_RESIDUAL_ORDER0_TLB_TABLE_BATCH,
	MTHP_RESIDUAL_ORDER0_PTE_ALLOC,
	MTHP_RESIDUAL_ORDER0_PMD_ALLOC,
	MTHP_RESIDUAL_ORDER0_VMALLOC,
	MTHP_RESIDUAL_ORDER0_SLUB_NEW_SLAB,
};

enum mthp_anon_order2_fallback_reason {
	MTHP_ANON_ORDER2_FALLBACK_UFFD_ARMED,
	MTHP_ANON_ORDER2_FALLBACK_VMA_NOT_ALLOWED,
	MTHP_ANON_ORDER2_FALLBACK_VMA_SUITABLE_FILTERED,
	MTHP_ANON_ORDER2_FALLBACK_PTE_RANGE_NOT_EMPTY,
	MTHP_ANON_ORDER2_FALLBACK_ALLOC_FAIL,
	MTHP_ANON_ORDER2_FALLBACK_MEMCG_FAIL,
};

enum mthp_suitable_fallback_subreason {
	MTHP_SUITABLE_FALLBACK_UNKNOWN,
	MTHP_SUITABLE_FALLBACK_TOO_SMALL,
	MTHP_SUITABLE_FALLBACK_LEFT_BOUNDARY,
	MTHP_SUITABLE_FALLBACK_RIGHT_BOUNDARY,
};

enum mthp_cow_order2_fallback_reason {
	MTHP_COW_ORDER2_FALLBACK_OLD_FOLIO_NOT_ORDER2,
	MTHP_COW_ORDER2_FALLBACK_FAULT_PAGE_NOT_IN_OLD_FOLIO,
	MTHP_COW_ORDER2_FALLBACK_UFFD_ARMED,
	MTHP_COW_ORDER2_FALLBACK_VMA_BOUNDARY,
	MTHP_COW_ORDER2_FALLBACK_VMA_NOT_ALLOWED,
	MTHP_COW_ORDER2_FALLBACK_VMA_SUITABLE_FILTERED,
	MTHP_COW_ORDER2_FALLBACK_ALLOC_FAIL,
	MTHP_COW_ORDER2_FALLBACK_MEMCG_FAIL,
	MTHP_COW_ORDER2_FALLBACK_COPY_FAIL,
	MTHP_COW_ORDER2_FALLBACK_PTE_CHANGED_OR_UNUSABLE,
};

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
static inline gfp_t mthp_gfp_source_clear(gfp_t gfp)
{
	return gfp & ~(__GFP_MTHP_SRC_ANON | __GFP_MTHP_SRC_FILE);
}

static inline gfp_t mthp_gfp_source_anon(gfp_t gfp)
{
	return mthp_gfp_source_clear(gfp) | __GFP_MTHP_SRC_ANON;
}

static inline gfp_t mthp_gfp_source_file(gfp_t gfp)
{
	return mthp_gfp_source_clear(gfp) | __GFP_MTHP_SRC_FILE;
}

static inline void mthp_count_order_pair(unsigned int order,
		enum vm_event_item order0_item, enum vm_event_item order2_item)
{
	if (order == 0)
		count_vm_event(order0_item);
	else if (order == 2)
		count_vm_event(order2_item);
}

static inline const char *mthp_vma_anon_name(struct vm_area_struct *vma)
{
	struct anon_vma_name *anon_name;

	if (!vma || !vma->vm_mm)
		return NULL;
	anon_name = anon_vma_name(vma);
	return anon_name ? anon_name->name : NULL;
}

static inline enum mthp_vma_alloc_class mthp_vma_alloc_class(struct vm_area_struct *vma)
{
	const char *name = mthp_vma_anon_name(vma);

	if (vma && vma->vm_file)
		return name ? MTHP_VMA_CLASS_ANON_SHMEM : MTHP_VMA_CLASS_FILE_PRIVATE;

	if (vma && vma_is_initial_heap(vma))
		return MTHP_VMA_CLASS_HEAP;
	if (vma && vma_is_initial_stack(vma))
		return MTHP_VMA_CLASS_STACK;

	if (name) {
		if (strstr(name, "dalvik") || strstr(name, "LinearAlloc"))
			return MTHP_VMA_CLASS_DALVIK;
		if (strstr(name, "scudo"))
			return MTHP_VMA_CLASS_SCUDO;
		if (strstr(name, "bionic") || strstr(name, "libc") ||
		    strstr(name, "malloc"))
			return MTHP_VMA_CLASS_BIONIC;
		if (strstr(name, "mthp_vma_"))
			return MTHP_VMA_CLASS_MTHP_VMA;
	}

	return MTHP_VMA_CLASS_OTHER;
}

static inline void mthp_count_vma_alloc_class(struct vm_area_struct *vma,
		unsigned int order)
{
	switch (mthp_vma_alloc_class(vma)) {
	case MTHP_VMA_CLASS_DALVIK:
		mthp_count_order_pair(order, VMA_ANON_ALLOC_ORDER0_CLASS_DALVIK,
					      VMA_ANON_ALLOC_ORDER2_CLASS_DALVIK);
		break;
	case MTHP_VMA_CLASS_SCUDO:
		mthp_count_order_pair(order, VMA_ANON_ALLOC_ORDER0_CLASS_SCUDO,
					      VMA_ANON_ALLOC_ORDER2_CLASS_SCUDO);
		break;
	case MTHP_VMA_CLASS_BIONIC:
		mthp_count_order_pair(order, VMA_ANON_ALLOC_ORDER0_CLASS_BIONIC,
					      VMA_ANON_ALLOC_ORDER2_CLASS_BIONIC);
		break;
	case MTHP_VMA_CLASS_MTHP_VMA:
		mthp_count_order_pair(order, VMA_ANON_ALLOC_ORDER0_CLASS_MTHP_VMA,
					      VMA_ANON_ALLOC_ORDER2_CLASS_MTHP_VMA);
		break;
	case MTHP_VMA_CLASS_HEAP:
		break;
	case MTHP_VMA_CLASS_STACK:
		mthp_count_order_pair(order, VMA_ANON_ALLOC_ORDER0_CLASS_STACK,
				      VMA_ANON_ALLOC_ORDER2_CLASS_STACK);
		break;
	case MTHP_VMA_CLASS_ANON_SHMEM:
		break;
	case MTHP_VMA_CLASS_FILE_PRIVATE:
		mthp_count_order_pair(order, VMA_ANON_ALLOC_ORDER0_CLASS_FILE_PRIVATE,
					      VMA_ANON_ALLOC_ORDER2_CLASS_FILE_PRIVATE);
		break;
	case MTHP_VMA_CLASS_OTHER:
		mthp_count_order_pair(order, VMA_ANON_ALLOC_ORDER0_CLASS_OTHER,
					      VMA_ANON_ALLOC_ORDER2_CLASS_OTHER);
		break;
	}
}

static inline void mthp_count_anon_order2_diag(struct vm_area_struct *vma,
		enum mthp_order2_diag_reason reason)
{
	enum mthp_vma_alloc_class class = mthp_vma_alloc_class(vma);

	switch (reason) {
	case MTHP_ORDER2_DIAG_VMA_UNSUITABLE:
		switch (class) {
		case MTHP_VMA_CLASS_SCUDO:
			count_vm_event(SCUDO_VMA_UNSUITABLE_ORDER2);
			break;
		case MTHP_VMA_CLASS_DALVIK:
			count_vm_event(DALVIK_VMA_UNSUITABLE_ORDER2);
			break;
		case MTHP_VMA_CLASS_BIONIC:
			count_vm_event(BIONIC_VMA_UNSUITABLE_ORDER2);
			break;
		case MTHP_VMA_CLASS_MTHP_VMA:
			count_vm_event(MTHP_VMA_UNSUITABLE_ORDER2);
			break;
		case MTHP_VMA_CLASS_STACK:
			count_vm_event(STACK_VMA_UNSUITABLE_ORDER2);
			break;
		case MTHP_VMA_CLASS_OTHER:
			count_vm_event(OTHER_VMA_UNSUITABLE_ORDER2);
			break;
		default:
			break;
		}
		break;
	case MTHP_ORDER2_DIAG_PTE_OCCUPIED:
		switch (class) {
		case MTHP_VMA_CLASS_SCUDO:
			count_vm_event(SCUDO_PTE_OCCUPIED_ORDER2);
			break;
		case MTHP_VMA_CLASS_DALVIK:
			count_vm_event(DALVIK_PTE_OCCUPIED_ORDER2);
			break;
		case MTHP_VMA_CLASS_BIONIC:
			count_vm_event(BIONIC_PTE_OCCUPIED_ORDER2);
			break;
		case MTHP_VMA_CLASS_MTHP_VMA:
			count_vm_event(MTHP_VMA_PTE_OCCUPIED_ORDER2);
			break;
		case MTHP_VMA_CLASS_STACK:
			count_vm_event(STACK_PTE_OCCUPIED_ORDER2);
			break;
		case MTHP_VMA_CLASS_OTHER:
			count_vm_event(OTHER_PTE_OCCUPIED_ORDER2);
			break;
		default:
			break;
		}
		break;
	case MTHP_ORDER2_DIAG_ALLOC_FAIL:
	case MTHP_ORDER2_DIAG_MEMCG_FAIL:
	case MTHP_ORDER2_DIAG_UFFD_BLOCKED:
		break;
	}
}

static inline void mthp_count_anon_order2_not_allowed_class(struct vm_area_struct *vma)
{
	switch (mthp_vma_alloc_class(vma)) {
	case MTHP_VMA_CLASS_DALVIK:
		count_vm_event(ANON_MTHP_ORDER2_NOT_ALLOWED_DALVIK);
		break;
	case MTHP_VMA_CLASS_SCUDO:
		count_vm_event(ANON_MTHP_ORDER2_NOT_ALLOWED_SCUDO);
		break;
	case MTHP_VMA_CLASS_BIONIC:
		count_vm_event(ANON_MTHP_ORDER2_NOT_ALLOWED_BIONIC);
		break;
	case MTHP_VMA_CLASS_MTHP_VMA:
		count_vm_event(ANON_MTHP_ORDER2_NOT_ALLOWED_MTHP_VMA);
		break;
	case MTHP_VMA_CLASS_STACK:
		count_vm_event(ANON_MTHP_ORDER2_NOT_ALLOWED_STACK);
		break;
	case MTHP_VMA_CLASS_OTHER:
		count_vm_event(ANON_MTHP_ORDER2_NOT_ALLOWED_OTHER);
		break;
	default:
		break;
	}
}

static inline enum mthp_suitable_fallback_subreason
mthp_anon_order2_suitable_subreason(struct vm_area_struct *vma,
		unsigned long addr)
{
	unsigned long hpage_size = PAGE_SIZE << 2;
	unsigned long haddr = ALIGN_DOWN(addr, hpage_size);

	if (!vma)
		return MTHP_SUITABLE_FALLBACK_UNKNOWN;
	if (vma->vm_end - vma->vm_start < hpage_size)
		return MTHP_SUITABLE_FALLBACK_TOO_SMALL;
	if (haddr < vma->vm_start)
		return MTHP_SUITABLE_FALLBACK_LEFT_BOUNDARY;
	if (haddr + hpage_size > vma->vm_end)
		return MTHP_SUITABLE_FALLBACK_RIGHT_BOUNDARY;
	return MTHP_SUITABLE_FALLBACK_UNKNOWN;
}

static inline void mthp_count_anon_order2_suitable_boundary(
		struct vm_area_struct *vma, unsigned long addr)
{
	switch (mthp_anon_order2_suitable_subreason(vma, addr)) {
	case MTHP_SUITABLE_FALLBACK_TOO_SMALL:
		count_vm_event(ANON_MTHP_ORDER2_VMA_TOO_SMALL);
		break;
	case MTHP_SUITABLE_FALLBACK_LEFT_BOUNDARY:
		count_vm_event(ANON_MTHP_ORDER2_VMA_LEFT_BOUNDARY);
		break;
	case MTHP_SUITABLE_FALLBACK_RIGHT_BOUNDARY:
		count_vm_event(ANON_MTHP_ORDER2_VMA_RIGHT_BOUNDARY);
		break;
	case MTHP_SUITABLE_FALLBACK_UNKNOWN:
		break;
	}
}

static inline void mthp_count_anon_fault_order0_class(struct vm_area_struct *vma)
{
	switch (mthp_vma_alloc_class(vma)) {
	case MTHP_VMA_CLASS_DALVIK:
		count_vm_event(ANON_FAULT_ORDER0_CLASS_DALVIK);
		break;
	case MTHP_VMA_CLASS_SCUDO:
		count_vm_event(ANON_FAULT_ORDER0_CLASS_SCUDO);
		break;
	case MTHP_VMA_CLASS_BIONIC:
		count_vm_event(ANON_FAULT_ORDER0_CLASS_BIONIC);
		break;
	case MTHP_VMA_CLASS_MTHP_VMA:
		count_vm_event(ANON_FAULT_ORDER0_CLASS_MTHP_VMA);
		break;
	case MTHP_VMA_CLASS_STACK:
		count_vm_event(ANON_FAULT_ORDER0_CLASS_STACK);
		break;
	case MTHP_VMA_CLASS_OTHER:
		count_vm_event(ANON_FAULT_ORDER0_CLASS_OTHER);
		break;
	default:
		break;
	}
}

static inline void mthp_count_residual_order0(enum mthp_residual_order0_source source)
{
	switch (source) {
	case MTHP_RESIDUAL_ORDER0_BINDER_BUFFER:
		count_vm_event(ORDER0_BINDER_BUFFER_PAGE);
		break;
	case MTHP_RESIDUAL_ORDER0_TLB_GATHER_BATCH:
		count_vm_event(ORDER0_TLB_GATHER_BATCH_PAGE);
		break;
	case MTHP_RESIDUAL_ORDER0_TLB_TABLE_BATCH:
		count_vm_event(ORDER0_TLB_TABLE_BATCH_PAGE);
		break;
	case MTHP_RESIDUAL_ORDER0_PTE_ALLOC:
		count_vm_event(ORDER0_PTE_ALLOC_PAGE);
		break;
	case MTHP_RESIDUAL_ORDER0_PMD_ALLOC:
		count_vm_event(ORDER0_PMD_ALLOC_PAGE);
		break;
	case MTHP_RESIDUAL_ORDER0_VMALLOC:
		count_vm_event(ORDER0_VMALLOC_PAGE);
		break;
	case MTHP_RESIDUAL_ORDER0_SLUB_NEW_SLAB:
		count_vm_event(ORDER0_SLUB_NEW_SLAB_PAGE);
		break;
		break;
		break;
		break;
		break;
		break;
	}
}

static inline void mthp_count_residual_order0_pages(enum mthp_residual_order0_source source,
		unsigned int pages)
{
	if (!pages)
		return;

	switch (source) {
	case MTHP_RESIDUAL_ORDER0_BINDER_BUFFER:
		count_vm_events(ORDER0_BINDER_BUFFER_PAGE, pages);
		break;
	case MTHP_RESIDUAL_ORDER0_TLB_GATHER_BATCH:
		count_vm_events(ORDER0_TLB_GATHER_BATCH_PAGE, pages);
		break;
	case MTHP_RESIDUAL_ORDER0_TLB_TABLE_BATCH:
		count_vm_events(ORDER0_TLB_TABLE_BATCH_PAGE, pages);
		break;
	case MTHP_RESIDUAL_ORDER0_PTE_ALLOC:
		count_vm_events(ORDER0_PTE_ALLOC_PAGE, pages);
		break;
	case MTHP_RESIDUAL_ORDER0_PMD_ALLOC:
		count_vm_events(ORDER0_PMD_ALLOC_PAGE, pages);
		break;
	case MTHP_RESIDUAL_ORDER0_VMALLOC:
		count_vm_events(ORDER0_VMALLOC_PAGE, pages);
		break;
	case MTHP_RESIDUAL_ORDER0_SLUB_NEW_SLAB:
		count_vm_events(ORDER0_SLUB_NEW_SLAB_PAGE, pages);
		break;
	}
}

static inline void mthp_count_anon_order2_fallback_reason(
		enum mthp_anon_order2_fallback_reason reason)
{
	switch (reason) {
	case MTHP_ANON_ORDER2_FALLBACK_UFFD_ARMED:
		count_vm_event(ANON_MTHP_ORDER2_UFFD_ARMED);
		break;
	case MTHP_ANON_ORDER2_FALLBACK_VMA_NOT_ALLOWED:
		count_vm_event(ANON_MTHP_ORDER2_VMA_NOT_ALLOWED);
		break;
	case MTHP_ANON_ORDER2_FALLBACK_VMA_SUITABLE_FILTERED:
		count_vm_event(ANON_MTHP_ORDER2_VMA_SUITABLE_FILTERED);
		break;
	case MTHP_ANON_ORDER2_FALLBACK_PTE_RANGE_NOT_EMPTY:
		count_vm_event(ANON_MTHP_ORDER2_PTE_RANGE_NOT_EMPTY);
		break;
	case MTHP_ANON_ORDER2_FALLBACK_ALLOC_FAIL:
		count_vm_event(ANON_MTHP_ORDER2_ALLOC_FAIL);
		break;
	case MTHP_ANON_ORDER2_FALLBACK_MEMCG_FAIL:
		count_vm_event(ANON_MTHP_ORDER2_MEMCG_FAIL);
		break;
	}
}

static inline void mthp_count_cow_order2_fallback_reason(
		enum mthp_cow_order2_fallback_reason reason)
{
	switch (reason) {
	case MTHP_COW_ORDER2_FALLBACK_OLD_FOLIO_NOT_ORDER2:
		count_vm_event(COW_MTHP_ORDER2_FALLBACK_OLD_FOLIO_NOT_ORDER2);
		break;
	case MTHP_COW_ORDER2_FALLBACK_FAULT_PAGE_NOT_IN_OLD_FOLIO:
	case MTHP_COW_ORDER2_FALLBACK_UFFD_ARMED:
	case MTHP_COW_ORDER2_FALLBACK_VMA_BOUNDARY:
		break;
	case MTHP_COW_ORDER2_FALLBACK_VMA_NOT_ALLOWED:
		count_vm_event(COW_MTHP_ORDER2_FALLBACK_VMA_NOT_ALLOWED);
		break;
	case MTHP_COW_ORDER2_FALLBACK_VMA_SUITABLE_FILTERED:
		break;
	case MTHP_COW_ORDER2_FALLBACK_ALLOC_FAIL:
		count_vm_event(COW_MTHP_ORDER2_FALLBACK_ALLOC_FAIL);
		break;
	case MTHP_COW_ORDER2_FALLBACK_MEMCG_FAIL:
	case MTHP_COW_ORDER2_FALLBACK_COPY_FAIL:
		break;
	case MTHP_COW_ORDER2_FALLBACK_PTE_CHANGED_OR_UNUSABLE:
		count_vm_event(COW_MTHP_ORDER2_FALLBACK_PTE_CHANGED_OR_UNUSABLE);
		break;
	}
}

static inline void mthp_count_zsmalloc_order(unsigned int order)
{
	mthp_count_order_pair(order, ORDER0_ZSMALLOC_PAGE, ORDER2_ZSMALLOC_PAGE);
}

static inline void mthp_count_dmabuf_order(unsigned int order)
{
	switch (order) {
	case 0:
		count_vm_event(DMABUF_ALLOC_ORDER0);
		break;
	}
}

static inline void mthp_count_fork_dup_task_struct(void)
{
	count_vm_event(FORK_DUP_TASK_STRUCT_EVENT);
}

static inline void mthp_count_fork_vmap_stack_pages(unsigned int pages)
{
	count_vm_events(FORK_VMAP_STACK_PAGES_EXPECTED, pages);
}

static inline void mthp_count_fork_nonvmap_stack_pages(unsigned int pages)
{
	count_vm_events(FORK_NONVMAP_STACK_ORDER_PAGES, pages);
}

static inline void mthp_count_vma_alloc(struct vm_area_struct *vma,
		unsigned int order, enum mthp_vma_alloc_source source)
{
	mthp_count_order_pair(order, VMA_ANON_ALLOC_ORDER0_TOTAL,
			      VMA_ANON_ALLOC_ORDER2_TOTAL);

	switch (source) {
		case MTHP_VMA_ALLOC_ANON_FAULT:
			mthp_count_order_pair(order, VMA_ANON_ALLOC_ORDER0_ANON_FAULT,
					      VMA_ANON_ALLOC_ORDER2_ANON_FAULT);
			if (order == 0)
				mthp_count_anon_fault_order0_class(vma);
			break;
	case MTHP_VMA_ALLOC_SWAPIN:
		mthp_count_order_pair(order, VMA_ANON_ALLOC_ORDER0_SWAPIN,
				      VMA_ANON_ALLOC_ORDER2_SWAPIN);
		break;
	case MTHP_VMA_ALLOC_COW:
		mthp_count_order_pair(order, VMA_ANON_ALLOC_ORDER0_COW,
				      VMA_ANON_ALLOC_ORDER2_COW);
		break;
	case MTHP_VMA_ALLOC_FILE_COW:
		mthp_count_order_pair(order, VMA_ANON_ALLOC_ORDER0_FILE_COW,
				      VMA_ANON_ALLOC_ORDER2_FILE_COW);
		break;
	case MTHP_VMA_ALLOC_UFFD_COPY:
		mthp_count_order_pair(order, VMA_ANON_ALLOC_ORDER0_UFFD_COPY,
				      VMA_ANON_ALLOC_ORDER2_UFFD_COPY);
		break;
	case MTHP_VMA_ALLOC_UFFD_ZEROPAGE:
		mthp_count_order_pair(order, VMA_ANON_ALLOC_ORDER0_UFFD_ZEROPAGE,
				      VMA_ANON_ALLOC_ORDER2_UFFD_ZEROPAGE);
		break;
	case MTHP_VMA_ALLOC_KSM_COPY:
		break;
	case MTHP_VMA_ALLOC_FORK_COPY:
		break;
	}

	mthp_count_vma_alloc_class(vma, order);
}

static inline void mthp_count_file_alloc(unsigned int order,
		enum mthp_file_alloc_source source)
{
	mthp_count_order_pair(order, FILE_ALLOC_ORDER0_TOTAL,
			      FILE_ALLOC_ORDER2_TOTAL);

	switch (source) {
	case MTHP_FILE_ALLOC_UNKNOWN:
		mthp_count_order_pair(order, FILE_ALLOC_ORDER0_UNKNOWN,
				      FILE_ALLOC_ORDER2_UNKNOWN);
		break;
	case MTHP_FILE_ALLOC_READAHEAD:
		mthp_count_order_pair(order, FILE_ALLOC_ORDER0_READAHEAD,
				      FILE_ALLOC_ORDER2_READAHEAD);
		break;
	case MTHP_FILE_ALLOC_GETFOLIO_MMAP:
		mthp_count_order_pair(order, FILE_ALLOC_ORDER0_GETFOLIO_MMAP,
				      FILE_ALLOC_ORDER2_GETFOLIO_MMAP);
		break;
	case MTHP_FILE_ALLOC_GETFOLIO_WRITE:
		mthp_count_order_pair(order, FILE_ALLOC_ORDER0_GETFOLIO_WRITE,
				      FILE_ALLOC_ORDER2_GETFOLIO_WRITE);
		break;
	case MTHP_FILE_ALLOC_GETFOLIO_OTHER:
		mthp_count_order_pair(order, FILE_ALLOC_ORDER0_GETFOLIO_OTHER,
				      FILE_ALLOC_ORDER2_GETFOLIO_OTHER);
		break;
	case MTHP_FILE_ALLOC_BUFFERED_READ:
		mthp_count_order_pair(order, FILE_ALLOC_ORDER0_BUFFERED_READ,
				      FILE_ALLOC_ORDER2_BUFFERED_READ);
		break;
	case MTHP_FILE_ALLOC_READ_CACHE:
		mthp_count_order_pair(order, FILE_ALLOC_ORDER0_READ_CACHE,
				      FILE_ALLOC_ORDER2_READ_CACHE);
		break;
	case MTHP_FILE_ALLOC_EROFS:
	case MTHP_FILE_ALLOC_F2FS_COMPRESS:
	case MTHP_FILE_ALLOC_BTRFS:
	case MTHP_FILE_ALLOC_FS_MISC:
		break;
	}
}

static inline void mthp_count_alloc_fail_source(gfp_t gfp, bool wmark)
{
	if (gfp & __GFP_MTHP_SRC_ANON)
		count_vm_event(wmark ? ALLOC_FAIL_WMARK_ANON :
			       ALLOC_FAIL_FRAGMENT_ANON);
	else if (gfp & __GFP_MTHP_SRC_FILE)
		count_vm_event(wmark ? ALLOC_FAIL_WMARK_FILE :
			       ALLOC_FAIL_FRAGMENT_FILE);
}

static inline void mthp_count_alloc_stall_source(gfp_t gfp, bool wmark)
{
	if (gfp & __GFP_MTHP_SRC_ANON)
		count_vm_event(wmark ? ALLOC_STALL_WMARK_ANON :
			       ALLOC_STALL_FRAGMENT_ANON);
	else if (gfp & __GFP_MTHP_SRC_FILE)
		count_vm_event(wmark ? ALLOC_STALL_WMARK_FILE :
			       ALLOC_STALL_FRAGMENT_FILE);
}

static inline struct folio *mthp_vma_alloc_folio_counted(gfp_t gfp,
		unsigned int order, struct vm_area_struct *vma, unsigned long addr,
		enum mthp_vma_alloc_source source)
{
	struct folio *folio;

	folio = vma_alloc_folio(mthp_gfp_source_anon(gfp), order, vma, addr);
	if (folio)
		mthp_count_vma_alloc(vma, order, source);
	return folio;
}

static inline struct folio *mthp_vma_alloc_zeroed_movable_folio_counted(
		struct vm_area_struct *vma, unsigned long addr,
		enum mthp_vma_alloc_source source)
{
	struct folio *folio;

	folio = vma_alloc_folio(mthp_gfp_source_anon(GFP_HIGHUSER_MOVABLE), 0,
				       vma, addr);
	if (folio && user_alloc_needs_zeroing())
		clear_user_highpage(&folio->page, addr);
	if (folio)
		mthp_count_vma_alloc(vma, folio_order(folio), source);
	return folio;
}

static inline struct folio *mthp_file_alloc_folio_counted(gfp_t gfp,
		unsigned int order, enum mthp_file_alloc_source source)
{
	struct folio *folio;

	folio = filemap_alloc_folio(mthp_gfp_source_file(gfp), order);
	if (folio)
		mthp_count_file_alloc(order, source);
	return folio;
}
#else
static inline gfp_t mthp_gfp_source_clear(gfp_t gfp)
{
	return gfp;
}

static inline gfp_t mthp_gfp_source_anon(gfp_t gfp)
{
	return gfp;
}

static inline gfp_t mthp_gfp_source_file(gfp_t gfp)
{
	return gfp;
}

static inline void mthp_count_vma_alloc(struct vm_area_struct *vma,
		unsigned int order, enum mthp_vma_alloc_source source)
{
}

static inline void mthp_count_file_alloc(unsigned int order,
		enum mthp_file_alloc_source source)
{
}

static inline void mthp_count_anon_order2_diag(struct vm_area_struct *vma,
		enum mthp_order2_diag_reason reason)
{
}

static inline void mthp_count_anon_order2_not_allowed_class(struct vm_area_struct *vma)
{
}

static inline void mthp_count_anon_order2_suitable_boundary(
		struct vm_area_struct *vma, unsigned long addr)
{
}

static inline void mthp_count_residual_order0(enum mthp_residual_order0_source source)
{
}

static inline void mthp_count_residual_order0_pages(enum mthp_residual_order0_source source,
		unsigned int pages)
{
}

static inline void mthp_count_anon_order2_fallback_reason(
		enum mthp_anon_order2_fallback_reason reason)
{
}

static inline void mthp_count_cow_order2_fallback_reason(
		enum mthp_cow_order2_fallback_reason reason)
{
}

static inline void mthp_count_zsmalloc_order(unsigned int order)
{
}

static inline void mthp_count_dmabuf_order(unsigned int order)
{
}

static inline void mthp_count_fork_dup_task_struct(void)
{
}

static inline void mthp_count_fork_vmap_stack_pages(unsigned int pages)
{
}

static inline void mthp_count_fork_nonvmap_stack_pages(unsigned int pages)
{
}

static inline void mthp_count_alloc_fail_source(gfp_t gfp, bool wmark)
{
}

static inline void mthp_count_alloc_stall_source(gfp_t gfp, bool wmark)
{
}

static inline struct folio *mthp_vma_alloc_folio_counted(gfp_t gfp,
		unsigned int order, struct vm_area_struct *vma, unsigned long addr,
		enum mthp_vma_alloc_source source)
{
	return vma_alloc_folio(gfp, order, vma, addr);
}

static inline struct folio *mthp_vma_alloc_zeroed_movable_folio_counted(
		struct vm_area_struct *vma, unsigned long addr,
		enum mthp_vma_alloc_source source)
{
	return vma_alloc_zeroed_movable_folio(vma, addr);
}

static inline struct folio *mthp_file_alloc_folio_counted(gfp_t gfp,
		unsigned int order, enum mthp_file_alloc_source source)
{
	return filemap_alloc_folio(gfp, order);
}
#endif

#endif /* _LINUX_MTHP_ALLOC_COUNTER_H */
