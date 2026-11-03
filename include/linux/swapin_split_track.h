/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Track 16K anonymous VMA windows affected by splits to correlate 4K swapin
 * fallbacks with zram partial reads.
 *
 * The (anon_vma, 16K-aligned window) key remains stable across VMA splits
 * and fork operations.
 */
#ifndef _LINUX_SWAPIN_SPLIT_TRACK_H
#define _LINUX_SWAPIN_SPLIT_TRACK_H

#include <linux/types.h>

struct anon_vma;
struct vm_fault;

enum swapin_split_reason {
	SWSPLIT_R_OTHER = 0,
	SWSPLIT_R_OK,
	SWSPLIT_R_VMA_WINDOW,
	SWSPLIT_R_THP_ALLOW,
	SWSPLIT_R_PHASE,
	SWSPLIT_R_PTE_BATCH,
	SWSPLIT_R_PTE_START,
	SWSPLIT_R_PTE_NOT_SWAP,
	SWSPLIT_R_PTE_OFFSET,
	SWSPLIT_R_PTE_CGROUP,
	SWSPLIT_R_ZEROMAP,
	SWSPLIT_R_SWAPCACHE,
	SWSPLIT_R_ALLOC,
	SWSPLIT_R_CHARGE,
	SWSPLIT_R_PREENTRY,
	SWSPLIT_R_LOWER_ORDER,
};

#ifdef CONFIG_SWAP
void swapin_split_note_split(struct anon_vma *anon_vma, unsigned long win,
			     unsigned long addr, unsigned long orig_start,
			     unsigned long orig_end);
void swapin_split_note_fallback(struct vm_fault *vmf, unsigned long entry_val,
				int reason);
void swapin_split_note_success(struct vm_fault *vmf, unsigned long entry_val,
			       int order);

#else
static inline void swapin_split_note_split(struct anon_vma *anon_vma,
					   unsigned long win,
					   unsigned long addr,
					   unsigned long orig_start,
					   unsigned long orig_end) { }
static inline void swapin_split_note_fallback(struct vm_fault *vmf,
					      unsigned long entry_val,
					      int reason) { }
static inline void swapin_split_note_success(struct vm_fault *vmf,
					     unsigned long entry_val,
					     int order) { }
static inline void swapin_split_note_partial(u32 head, unsigned int offset) { }
#endif

#endif /* _LINUX_SWAPIN_SPLIT_TRACK_H */
