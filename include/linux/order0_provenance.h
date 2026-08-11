/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_ORDER0_PROVENANCE_H
#define _LINUX_ORDER0_PROVENANCE_H

#include <linux/types.h>

struct folio;
struct page;
struct page_ext_operations;
struct seq_file;
struct zone;

enum order0_alloc_source {
	ORDER0_SOURCE_UNKNOWN,
	ORDER0_SOURCE_COW_FAULT,
	ORDER0_SOURCE_WP_COW,
	ORDER0_SOURCE_ANON_FIRST_FAULT,
	ORDER0_SOURCE_FILECACHE,
	ORDER0_SOURCE_SWAPIN,
	ORDER0_SOURCE_SWAP_METADATA,
	ORDER0_SOURCE_SECRETMEM,
	ORDER0_SOURCE_VMALLOC,
	ORDER0_SOURCE_DMABUF_HEAP,
	ORDER0_SOURCE_UFFD_MFILL,
	ORDER0_SOURCE_SHMEM,
	ORDER0_SOURCE_VIRTIO_DRM_SHMEM,
	ORDER0_SOURCE_PGTABLE,
	ORDER0_SOURCE_TLB_GATHER,
	ORDER0_SOURCE_PERCPU_BUFFER,
	ORDER0_SOURCE_BINDER_BUFFER,
	ORDER0_SOURCE_PIPE_BUFFER,
	ORDER0_SOURCE_SLAB,
	ORDER0_SOURCE_ZSMALLOC,
	ORDER0_SOURCE_MIGRATION,
	ORDER0_SOURCE_PAGE_FRAG,
	ORDER0_SOURCE_SPLICE,
	ORDER0_SOURCE_KSM,
	ORDER0_SOURCE_MEMPOOL,
	ORDER0_SOURCE_BALLOON,
	ORDER0_SOURCE_NR,
};

#ifdef CONFIG_MTHP_ORDER0_PROVENANCE
extern struct page_ext_operations order0_provenance_ops;

void order0_provenance_prepare_alloc(struct page *page, unsigned int order);
void order0_provenance_record_root(struct folio *folio,
				    enum order0_alloc_source source);
void order0_provenance_record_migration(struct folio *new_folio,
					const struct folio *old_folio);
void order0_provenance_propagate_split(struct folio *new_folio,
					       const struct folio *old_folio);
void order0_provenance_record_cow(struct folio *new_folio,
				   const struct folio *old_folio,
				   enum order0_alloc_source fallback_source);
void order0_provenance_inherit_root(struct folio *new_folio,
				    const struct folio *old_folio,
				    enum order0_alloc_source fallback_source);
void order0_provenance_note_cow_mthp_success(void);
void order0_provenance_note_cow_mthp_fallback(void);
void order0_provenance_note_cow_mthp_vma_span_fallback(void);
void order0_provenance_note_partial_unmap_split(void);
void order0_provenance_pcp_refill(struct zone *zone, int migratetype,
				  unsigned long nr_pages);
void order0_provenance_pcp_free(struct zone *zone, int migratetype,
				unsigned long nr_pages);
void order0_provenance_pcp_alloc(struct zone *zone, int migratetype,
				 unsigned long nr_pages);
void order0_provenance_pcp_drain(struct zone *zone, int migratetype,
				 unsigned long nr_pages);
void order0_provenance_vmstat_show(struct seq_file *m);
#else
static inline void order0_provenance_prepare_alloc(struct page *page,
						    unsigned int order)
{
}

static inline void order0_provenance_record_root(struct folio *folio,
					   enum order0_alloc_source source)
{
}

static inline void
order0_provenance_record_migration(struct folio *new_folio,
					const struct folio *old_folio)
{
}

static inline void
order0_provenance_propagate_split(struct folio *new_folio,
					       const struct folio *old_folio)
{
}

static inline void order0_provenance_record_cow(struct folio *new_folio,
					  const struct folio *old_folio,
					  enum order0_alloc_source fallback_source)
{
}

static inline void order0_provenance_inherit_root(struct folio *new_folio,
					    const struct folio *old_folio,
					    enum order0_alloc_source fallback_source)
{
}

static inline void order0_provenance_note_cow_mthp_success(void)
{
}

static inline void order0_provenance_note_cow_mthp_fallback(void)
{
}

static inline void order0_provenance_note_cow_mthp_vma_span_fallback(void)
{
}

static inline void order0_provenance_note_partial_unmap_split(void)
{
}

static inline void order0_provenance_pcp_refill(struct zone *zone,
					 int migratetype, unsigned long nr_pages)
{
}

static inline void order0_provenance_pcp_free(struct zone *zone,
					int migratetype, unsigned long nr_pages)
{
}

static inline void order0_provenance_pcp_alloc(struct zone *zone,
					 int migratetype, unsigned long nr_pages)
{
}

static inline void order0_provenance_pcp_drain(struct zone *zone,
				 int migratetype, unsigned long nr_pages)
{
}

static inline void order0_provenance_vmstat_show(struct seq_file *m)
{
}
#endif

#endif
