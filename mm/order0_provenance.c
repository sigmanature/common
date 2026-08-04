// SPDX-License-Identifier: GPL-2.0
#include <linux/cpu.h>
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <linux/order0_provenance.h>
#include <linux/page_ext.h>
#include <linux/seq_file.h>

struct order0_folio_provenance {
	u8 root_source;
	u8 cow_depth;
};

enum order0_migratetype_bucket {
	ORDER0_MT_MOVABLE,
	ORDER0_MT_UNMOVABLE,
	ORDER0_MT_OTHER,
	ORDER0_MT_NR,
};

enum order0_pcp_flow {
	ORDER0_PCP_REFILL,
	ORDER0_PCP_FREE,
	ORDER0_PCP_ALLOC,
	ORDER0_PCP_DRAIN,
	ORDER0_PCP_FLOW_NR,
};

enum order0_cow_path {
	ORDER0_COW_PATH_WP,
	ORDER0_COW_PATH_FAULT,
	ORDER0_COW_PATH_NR,
};

enum order0_cow_parent_reason {
	ORDER0_COW_PARENT_NO_FOLIO,
	ORDER0_COW_PARENT_ZERO_PAGE,
	ORDER0_COW_PARENT_SPECIAL_PTE,
	ORDER0_COW_PARENT_NO_PAGE_EXT,
	ORDER0_COW_PARENT_ROOT_UNKNOWN,
	ORDER0_COW_PARENT_ROOT_INVALID,
	ORDER0_COW_PARENT_FALLBACK_ROOT,
	ORDER0_COW_PARENT_REAL_ROOT,
	ORDER0_COW_PARENT_REASON_NR,
};

enum order0_counter {
	ORDER0_COUNTER_TOTAL_BASE,
	ORDER0_COUNTER_SOURCE_BASE = ORDER0_COUNTER_TOTAL_BASE + ORDER0_MT_NR,
	ORDER0_COUNTER_COW_PARENT_ROOT_BASE = ORDER0_COUNTER_SOURCE_BASE +
		(ORDER0_SOURCE_NR - 1) * ORDER0_MT_NR,
	ORDER0_COUNTER_COW_PARENT_DIAG_BASE =
		ORDER0_COUNTER_COW_PARENT_ROOT_BASE +
		ORDER0_SOURCE_NR * ORDER0_MT_NR,
	ORDER0_COUNTER_COW_MTHP_SUCCESS =
		ORDER0_COUNTER_COW_PARENT_DIAG_BASE +
		ORDER0_COW_PATH_NR * ORDER0_COW_PARENT_REASON_NR,
	ORDER0_COUNTER_COW_MTHP_FALLBACK,
	ORDER0_COUNTER_COW_MTHP_VMA_SPAN_FALLBACK,
	ORDER0_COUNTER_COW_CHAIN_DEPTH_0,
	ORDER0_COUNTER_COW_CHAIN_DEPTH_1,
	ORDER0_COUNTER_COW_CHAIN_DEPTH_2,
	ORDER0_COUNTER_COW_CHAIN_DEPTH_3P,
	ORDER0_COUNTER_SPLIT_PARTIAL_UNMAP,
	ORDER0_COUNTER_PCP_BASE,
	ORDER0_COUNTER_NR = ORDER0_COUNTER_PCP_BASE +
		ORDER0_PCP_FLOW_NR * ORDER0_MT_NR,
};

static DEFINE_PER_CPU(unsigned long, order0_vmstat[ORDER0_COUNTER_NR]);

static const char * const order0_migratetype_names[ORDER0_MT_NR] = {
	[ORDER0_MT_MOVABLE] = "movable",
	[ORDER0_MT_UNMOVABLE] = "unmovable",
	[ORDER0_MT_OTHER] = "other",
};

static const char * const order0_source_names[ORDER0_SOURCE_NR] = {
	[ORDER0_SOURCE_UNKNOWN] = "unknown",
	[ORDER0_SOURCE_COW_FAULT] = "cow_fault",
	[ORDER0_SOURCE_WP_COW] = "wp_cow",
	[ORDER0_SOURCE_ANON_FIRST_FAULT] = "anon_first_fault",
	[ORDER0_SOURCE_FILECACHE] = "filecache",
	[ORDER0_SOURCE_SWAPIN] = "swapin",
	[ORDER0_SOURCE_SWAP_METADATA] = "swap_metadata",
	[ORDER0_SOURCE_SECRETMEM] = "secretmem",
	[ORDER0_SOURCE_VMALLOC] = "vmalloc",
	[ORDER0_SOURCE_DMABUF_HEAP] = "dmabuf_heap",
	[ORDER0_SOURCE_UFFD_MFILL] = "uffd_mfill",
	[ORDER0_SOURCE_SHMEM] = "shmem",
	[ORDER0_SOURCE_VIRTIO_DRM_SHMEM] = "virtio_drm_shmem",
	[ORDER0_SOURCE_PGTABLE] = "pgtable",
	[ORDER0_SOURCE_TLB_GATHER] = "tlb_gather",
	[ORDER0_SOURCE_PERCPU_BUFFER] = "percpu_buffer",
	[ORDER0_SOURCE_BINDER_BUFFER] = "binder_buffer",
	[ORDER0_SOURCE_PIPE_BUFFER] = "pipe_buffer",
	[ORDER0_SOURCE_SLAB] = "slab",
	[ORDER0_SOURCE_ZSMALLOC] = "zsmalloc",
	[ORDER0_SOURCE_MIGRATION] = "migration",
	[ORDER0_SOURCE_PAGE_FRAG] = "page_frag",
	[ORDER0_SOURCE_SPLICE] = "splice",
	[ORDER0_SOURCE_KSM] = "ksm",
	[ORDER0_SOURCE_MEMPOOL] = "mempool",
	[ORDER0_SOURCE_BALLOON] = "balloon",
	[ORDER0_SOURCE_ZERO_PAGE] = "zero_page",
};

static const char * const order0_pcp_flow_names[ORDER0_PCP_FLOW_NR] = {
	[ORDER0_PCP_REFILL] = "refill",
	[ORDER0_PCP_FREE] = "free",
	[ORDER0_PCP_ALLOC] = "alloc",
	[ORDER0_PCP_DRAIN] = "drain",
};

static const char * const order0_cow_path_names[ORDER0_COW_PATH_NR] = {
	[ORDER0_COW_PATH_WP] = "wp",
	[ORDER0_COW_PATH_FAULT] = "fault",
};

static const char * const order0_cow_parent_reason_names[
		ORDER0_COW_PARENT_REASON_NR] = {
	[ORDER0_COW_PARENT_NO_FOLIO] = "no_folio",
	[ORDER0_COW_PARENT_ZERO_PAGE] = "zero_page",
	[ORDER0_COW_PARENT_SPECIAL_PTE] = "special_pte",
	[ORDER0_COW_PARENT_NO_PAGE_EXT] = "no_page_ext",
	[ORDER0_COW_PARENT_ROOT_UNKNOWN] = "root_unknown",
	[ORDER0_COW_PARENT_ROOT_INVALID] = "root_invalid",
	[ORDER0_COW_PARENT_FALLBACK_ROOT] = "fallback_root",
	[ORDER0_COW_PARENT_REAL_ROOT] = "real_root",
};

static const enum zone_stat_item order0_pcp_zone_items[ORDER0_MT_NR] = {
	[ORDER0_MT_MOVABLE] = NR_PCP_ORDER0_MOVABLE,
	[ORDER0_MT_UNMOVABLE] = NR_PCP_ORDER0_UNMOVABLE,
	[ORDER0_MT_OTHER] = NR_PCP_ORDER0_OTHER,
};

static bool need_order0_provenance(void)
{
	return true;
}

struct page_ext_operations order0_provenance_ops = {
	.size = sizeof(struct order0_folio_provenance),
	.need = need_order0_provenance,
	.need_shared_flags = false,
};

static enum order0_migratetype_bucket
order0_migratetype_bucket(const struct page *page)
{
	enum migratetype migratetype;

	migratetype = get_pfnblock_migratetype(page, page_to_pfn(page));
	if (migratetype == MIGRATE_MOVABLE)
		return ORDER0_MT_MOVABLE;
	if (migratetype == MIGRATE_UNMOVABLE)
		return ORDER0_MT_UNMOVABLE;
	return ORDER0_MT_OTHER;
}

static enum order0_migratetype_bucket
order0_migratetype_bucket_from_value(int migratetype)
{
	if (migratetype == MIGRATE_MOVABLE)
		return ORDER0_MT_MOVABLE;
	if (migratetype == MIGRATE_UNMOVABLE)
		return ORDER0_MT_UNMOVABLE;
	return ORDER0_MT_OTHER;
}

static inline enum order0_counter
order0_total_counter(enum order0_migratetype_bucket bucket)
{
	return ORDER0_COUNTER_TOTAL_BASE + bucket;
}

static inline enum order0_counter
order0_source_counter(enum order0_alloc_source source,
			      enum order0_migratetype_bucket bucket)
{
	return ORDER0_COUNTER_SOURCE_BASE +
		(source - ORDER0_SOURCE_COW_FAULT) * ORDER0_MT_NR + bucket;
}

static inline enum order0_counter
order0_cow_parent_root_counter(enum order0_alloc_source source,
				       enum order0_migratetype_bucket bucket)
{
	return ORDER0_COUNTER_COW_PARENT_ROOT_BASE + source * ORDER0_MT_NR + bucket;
}

static inline enum order0_counter
order0_cow_parent_diag_counter(enum order0_cow_path path,
				       enum order0_cow_parent_reason reason)
{
	return ORDER0_COUNTER_COW_PARENT_DIAG_BASE +
		path * ORDER0_COW_PARENT_REASON_NR + reason;
}

static inline enum order0_counter
order0_pcp_counter(enum order0_pcp_flow flow,
			   enum order0_migratetype_bucket bucket)
{
	return ORDER0_COUNTER_PCP_BASE + flow * ORDER0_MT_NR + bucket;
}

static inline void order0_counter_add(enum order0_counter counter,
				      unsigned long amount)
{
	if (amount)
		this_cpu_add(order0_vmstat[counter], amount);
}

static inline void order0_counter_inc(enum order0_counter counter)
{
	this_cpu_inc(order0_vmstat[counter]);
}

static struct order0_folio_provenance *
order0_folio_provenance_get(const struct folio *folio,
			    struct page_ext **page_extp)
{
	struct page_ext *page_ext;

	page_ext = page_ext_get(&folio->page);
	if (!page_ext)
		return NULL;
	*page_extp = page_ext;
	return page_ext_data(page_ext, &order0_provenance_ops);
}

static void order0_count_source(const struct folio *folio,
				enum order0_alloc_source source)
{
	enum order0_migratetype_bucket bucket;

	if (folio_order(folio) || source <= ORDER0_SOURCE_UNKNOWN ||
	    source >= ORDER0_SOURCE_NR)
		return;
	bucket = order0_migratetype_bucket(&folio->page);
	order0_counter_inc(order0_source_counter(source, bucket));
}

static void order0_count_cow_parent_root(const struct folio *folio,
					 enum order0_alloc_source source)
{
	enum order0_migratetype_bucket bucket;

	if (folio_order(folio) || source >= ORDER0_SOURCE_NR)
		return;
	bucket = order0_migratetype_bucket(&folio->page);
	order0_counter_inc(order0_cow_parent_root_counter(source, bucket));
}

static void order0_count_cow_parent_diagnostic(
		const struct folio *folio, enum order0_cow_path path,
		enum order0_cow_parent_reason reason)
{
	if (folio_order(folio) || path >= ORDER0_COW_PATH_NR ||
	    reason >= ORDER0_COW_PARENT_REASON_NR)
		return;
	order0_counter_inc(order0_cow_parent_diag_counter(path, reason));
}

void order0_provenance_prepare_alloc(struct page *page, unsigned int order)
{
	struct page_ext *page_ext = NULL;
	struct order0_folio_provenance *provenance;

	provenance = order0_folio_provenance_get(page_folio(page), &page_ext);
	if (provenance) {
		provenance->root_source = ORDER0_SOURCE_UNKNOWN;
		provenance->cow_depth = 0;
		page_ext_put(page_ext);
	}
	if (!order)
		order0_counter_inc(order0_total_counter(
			order0_migratetype_bucket(page)));
}

void order0_provenance_record_root(struct folio *folio,
				   enum order0_alloc_source source)
{
	struct page_ext *page_ext = NULL;
	struct order0_folio_provenance *provenance;

	if (source >= ORDER0_SOURCE_NR)
		return;
	order0_count_source(folio, source);
	provenance = order0_folio_provenance_get(folio, &page_ext);
	if (!provenance)
		return;
	provenance->root_source = source;
	provenance->cow_depth = 0;
	page_ext_put(page_ext);
}
EXPORT_SYMBOL_GPL(order0_provenance_record_root);

void order0_provenance_record_migration(struct folio *new_folio,
					const struct folio *old_folio)
{
	struct page_ext *old_page_ext = NULL;
	struct page_ext *new_page_ext = NULL;
	struct order0_folio_provenance *old_provenance;
	struct order0_folio_provenance *new_provenance;
	enum order0_alloc_source root_source = ORDER0_SOURCE_MIGRATION;
	u8 cow_depth = 0;

	order0_count_source(new_folio, ORDER0_SOURCE_MIGRATION);
	old_provenance = old_folio ?
		order0_folio_provenance_get(old_folio, &old_page_ext) : NULL;
	if (old_provenance &&
	    old_provenance->root_source > ORDER0_SOURCE_UNKNOWN &&
	    old_provenance->root_source < ORDER0_SOURCE_NR) {
		root_source = old_provenance->root_source;
		cow_depth = old_provenance->cow_depth;
	}
	if (old_page_ext)
		page_ext_put(old_page_ext);

	new_provenance = order0_folio_provenance_get(new_folio, &new_page_ext);
	if (!new_provenance)
		return;
	new_provenance->root_source = root_source;
	new_provenance->cow_depth = cow_depth;
	page_ext_put(new_page_ext);
}
EXPORT_SYMBOL_GPL(order0_provenance_record_migration);

void order0_provenance_propagate_split(struct folio *new_folio,
					       const struct folio *old_folio)
{
	struct page_ext *old_page_ext = NULL;
	struct page_ext *new_page_ext = NULL;
	struct order0_folio_provenance *old_provenance;
	struct order0_folio_provenance *new_provenance;

	old_provenance = order0_folio_provenance_get(old_folio, &old_page_ext);
	new_provenance = order0_folio_provenance_get(new_folio, &new_page_ext);
	if (old_provenance && new_provenance) {
		new_provenance->root_source = old_provenance->root_source;
		new_provenance->cow_depth = old_provenance->cow_depth;
	}
	if (old_page_ext)
		page_ext_put(old_page_ext);
	if (new_page_ext)
		page_ext_put(new_page_ext);
}
EXPORT_SYMBOL_GPL(order0_provenance_propagate_split);

void order0_provenance_record_cow(struct folio *new_folio,
					const struct folio *old_folio,
					enum order0_alloc_source fallback_source,
					enum order0_cow_parent_hint parent_hint)
{
	struct page_ext *old_page_ext = NULL;
	struct page_ext *new_page_ext = NULL;
	struct order0_folio_provenance *old_provenance;
	struct order0_folio_provenance *new_provenance;
	enum order0_alloc_source root_source = fallback_source;
	enum order0_alloc_source parent_root_source = ORDER0_SOURCE_UNKNOWN;
	enum order0_cow_parent_reason parent_reason;
	u8 cow_depth = 1;
	u8 parent_cow_depth = 0;

	if (root_source >= ORDER0_SOURCE_NR)
		root_source = ORDER0_SOURCE_UNKNOWN;
	order0_count_source(new_folio, ORDER0_SOURCE_WP_COW);
	old_provenance = old_folio ?
		order0_folio_provenance_get(old_folio, &old_page_ext) : NULL;
	if (old_provenance &&
	    old_provenance->root_source > ORDER0_SOURCE_UNKNOWN &&
	    old_provenance->root_source < ORDER0_SOURCE_NR) {
		root_source = old_provenance->root_source;
		parent_root_source = old_provenance->root_source;
		parent_cow_depth = old_provenance->cow_depth;
		cow_depth = min_t(u8, parent_cow_depth + 1, 3);
	}
	if (!old_folio && parent_hint == ORDER0_COW_PARENT_HINT_ZERO_PAGE) {
		parent_reason = ORDER0_COW_PARENT_ZERO_PAGE;
		parent_root_source = ORDER0_SOURCE_ZERO_PAGE;
		root_source = ORDER0_SOURCE_ZERO_PAGE;
	} else if (!old_folio &&
		   parent_hint == ORDER0_COW_PARENT_HINT_SPECIAL_PTE) {
		parent_reason = ORDER0_COW_PARENT_SPECIAL_PTE;
	} else if (!old_folio)
		parent_reason = ORDER0_COW_PARENT_NO_FOLIO;
	else if (!old_provenance)
		parent_reason = ORDER0_COW_PARENT_NO_PAGE_EXT;
	else if (old_provenance->root_source == ORDER0_SOURCE_UNKNOWN)
		parent_reason = ORDER0_COW_PARENT_ROOT_UNKNOWN;
	else if (old_provenance->root_source >= ORDER0_SOURCE_NR)
		parent_reason = ORDER0_COW_PARENT_ROOT_INVALID;
	else if (old_provenance->root_source == ORDER0_SOURCE_COW_FAULT ||
		 old_provenance->root_source == ORDER0_SOURCE_WP_COW)
		parent_reason = ORDER0_COW_PARENT_FALLBACK_ROOT;
	else
		parent_reason = ORDER0_COW_PARENT_REAL_ROOT;
	if (old_page_ext)
		page_ext_put(old_page_ext);
	if (!folio_order(new_folio))
		order0_count_cow_parent_diagnostic(new_folio,
				ORDER0_COW_PATH_WP, parent_reason);
	order0_count_cow_parent_root(new_folio, parent_root_source);

	new_provenance = order0_folio_provenance_get(new_folio, &new_page_ext);
	if (new_provenance) {
		new_provenance->root_source = root_source;
		new_provenance->cow_depth = cow_depth;
		page_ext_put(new_page_ext);
	}
	if (folio_order(new_folio))
		return;
	switch (parent_cow_depth) {
	case 0:
		order0_counter_inc(ORDER0_COUNTER_COW_CHAIN_DEPTH_0);
		break;
	case 1:
		order0_counter_inc(ORDER0_COUNTER_COW_CHAIN_DEPTH_1);
		break;
	case 2:
		order0_counter_inc(ORDER0_COUNTER_COW_CHAIN_DEPTH_2);
		break;
	default:
		order0_counter_inc(ORDER0_COUNTER_COW_CHAIN_DEPTH_3P);
		break;
	}
}

void order0_provenance_inherit_root(struct folio *new_folio,
				    const struct folio *old_folio,
				    enum order0_alloc_source fallback_source)
{
	struct page_ext *old_page_ext = NULL;
	struct page_ext *new_page_ext = NULL;
	struct order0_folio_provenance *old_provenance;
	struct order0_folio_provenance *new_provenance;
	enum order0_alloc_source root_source = fallback_source;
	enum order0_alloc_source parent_root_source = ORDER0_SOURCE_UNKNOWN;
	enum order0_cow_parent_reason parent_reason;
	u8 cow_depth = 0;

	if (root_source >= ORDER0_SOURCE_NR)
		root_source = ORDER0_SOURCE_UNKNOWN;
	old_provenance = old_folio ?
		order0_folio_provenance_get(old_folio, &old_page_ext) : NULL;
	if (old_provenance &&
	    old_provenance->root_source > ORDER0_SOURCE_UNKNOWN &&
	    old_provenance->root_source < ORDER0_SOURCE_NR) {
		root_source = old_provenance->root_source;
		parent_root_source = old_provenance->root_source;
		cow_depth = old_provenance->cow_depth;
	}
	if (!old_folio)
		parent_reason = ORDER0_COW_PARENT_NO_FOLIO;
	else if (!old_provenance)
		parent_reason = ORDER0_COW_PARENT_NO_PAGE_EXT;
	else if (old_provenance->root_source == ORDER0_SOURCE_UNKNOWN)
		parent_reason = ORDER0_COW_PARENT_ROOT_UNKNOWN;
	else if (old_provenance->root_source >= ORDER0_SOURCE_NR)
		parent_reason = ORDER0_COW_PARENT_ROOT_INVALID;
	else if (old_provenance->root_source == ORDER0_SOURCE_COW_FAULT ||
		 old_provenance->root_source == ORDER0_SOURCE_WP_COW)
		parent_reason = ORDER0_COW_PARENT_FALLBACK_ROOT;
	else
		parent_reason = ORDER0_COW_PARENT_REAL_ROOT;
	if (old_page_ext)
		page_ext_put(old_page_ext);
	if (!folio_order(new_folio))
		order0_count_cow_parent_diagnostic(new_folio,
				ORDER0_COW_PATH_FAULT, parent_reason);

	/*
	 * do_cow_fault inherits the parent's root into per-folio metadata;
	 * keep the same parent-root birth aggregate the wp_cow path produces.
	 */
	order0_count_cow_parent_root(new_folio, parent_root_source);

	new_provenance = order0_folio_provenance_get(new_folio, &new_page_ext);
	if (new_provenance) {
		new_provenance->root_source = root_source;
		new_provenance->cow_depth = cow_depth;
		page_ext_put(new_page_ext);
	}
}

void order0_provenance_note_cow_mthp_success(void)
{
	order0_counter_inc(ORDER0_COUNTER_COW_MTHP_SUCCESS);
}

void order0_provenance_note_cow_mthp_fallback(void)
{
	order0_counter_inc(ORDER0_COUNTER_COW_MTHP_FALLBACK);
}

void order0_provenance_note_cow_mthp_vma_span_fallback(void)
{
	order0_counter_inc(ORDER0_COUNTER_COW_MTHP_VMA_SPAN_FALLBACK);
}

void order0_provenance_note_partial_unmap_split(void)
{
	order0_counter_inc(ORDER0_COUNTER_SPLIT_PARTIAL_UNMAP);
}

static void order0_pcp_account(struct zone *zone, int migratetype,
				unsigned long nr_pages, long delta,
				enum order0_pcp_flow flow)
{
	enum order0_migratetype_bucket bucket;

	if (!nr_pages)
		return;
	bucket = order0_migratetype_bucket_from_value(migratetype);
	mod_zone_page_state(zone, order0_pcp_zone_items[bucket], delta);
	order0_counter_add(order0_pcp_counter(flow, bucket), nr_pages);
}

void order0_provenance_pcp_refill(struct zone *zone, int migratetype,
				  unsigned long nr_pages)
{
	order0_pcp_account(zone, migratetype, nr_pages, nr_pages, ORDER0_PCP_REFILL);
}

void order0_provenance_pcp_free(struct zone *zone, int migratetype,
				unsigned long nr_pages)
{
	order0_pcp_account(zone, migratetype, nr_pages, nr_pages, ORDER0_PCP_FREE);
}

void order0_provenance_pcp_alloc(struct zone *zone, int migratetype,
				 unsigned long nr_pages)
{
	order0_pcp_account(zone, migratetype, nr_pages, -nr_pages, ORDER0_PCP_ALLOC);
}

void order0_provenance_pcp_drain(struct zone *zone, int migratetype,
				 unsigned long nr_pages)
{
	order0_pcp_account(zone, migratetype, nr_pages, -nr_pages, ORDER0_PCP_DRAIN);
}

unsigned long order0_provenance_pcp_order0_node(struct pglist_data *pgdat)
{
	unsigned long total = 0;
	int zoneid, cpu;

	for (zoneid = 0; zoneid < MAX_NR_ZONES; zoneid++) {
		struct zone *zone = &pgdat->node_zones[zoneid];

		if (!populated_zone(zone))
			continue;

		for_each_possible_cpu(cpu) {
			struct per_cpu_pages *pcp;

			pcp = per_cpu_ptr(zone->per_cpu_pageset, cpu);
			total += data_race(READ_ONCE(pcp->order0_count));
		}
	}

	return total;
}

unsigned long order0_provenance_pcp_order0_total(void)
{
	unsigned long total = 0;
	pg_data_t *pgdat;

	for_each_online_pgdat(pgdat)
		total += order0_provenance_pcp_order0_node(pgdat);

	return total;
}

static unsigned long order0_counter_sum(enum order0_counter counter)
{
	unsigned long total = 0;
	int cpu;

	for_each_possible_cpu(cpu)
		total += data_race(per_cpu(order0_vmstat[counter], cpu));
	return total;
}

static void order0_vmstat_print(struct seq_file *m, const char *name,
				 enum order0_counter counter)
{
	seq_printf(m, "%s %lu\n", name, order0_counter_sum(counter));
}

void order0_provenance_vmstat_show(struct seq_file *m)
{
	int bucket;
	int source;
	int flow;
	int path;
	int reason;

	for (bucket = 0; bucket < ORDER0_MT_NR; bucket++)
		seq_printf(m, "order0_alloc_total_%s %lu\n",
			   order0_migratetype_names[bucket],
			   order0_counter_sum(order0_total_counter(bucket)));
	for (source = ORDER0_SOURCE_COW_FAULT; source < ORDER0_SOURCE_NR; source++)
		for (bucket = 0; bucket < ORDER0_MT_NR; bucket++)
			seq_printf(m, "order0_src_%s_%s %lu\n",
				   order0_source_names[source],
				   order0_migratetype_names[bucket],
				   order0_counter_sum(order0_source_counter(source, bucket)));
	for (source = ORDER0_SOURCE_UNKNOWN; source < ORDER0_SOURCE_NR; source++)
		for (bucket = 0; bucket < ORDER0_MT_NR; bucket++)
			seq_printf(m, "order0_cow_parent_root_%s_%s %lu\n",
				   order0_source_names[source],
				   order0_migratetype_names[bucket],
					   order0_counter_sum(order0_cow_parent_root_counter(
						   source, bucket)));
	for (path = 0; path < ORDER0_COW_PATH_NR; path++)
		for (reason = 0; reason < ORDER0_COW_PARENT_REASON_NR; reason++)
			seq_printf(m, "order0_cow_parent_%s_%s %lu\n",
				   order0_cow_path_names[path],
				   order0_cow_parent_reason_names[reason],
				   order0_counter_sum(order0_cow_parent_diag_counter(
					   path, reason)));
	order0_vmstat_print(m, "order0_cow_mthp_success",
			    ORDER0_COUNTER_COW_MTHP_SUCCESS);
	order0_vmstat_print(m, "order0_cow_mthp_fallback",
			    ORDER0_COUNTER_COW_MTHP_FALLBACK);
	order0_vmstat_print(m, "order0_cow_mthp_vma_span_fallback",
			    ORDER0_COUNTER_COW_MTHP_VMA_SPAN_FALLBACK);
	order0_vmstat_print(m, "order0_cow_chain_depth_0",
			    ORDER0_COUNTER_COW_CHAIN_DEPTH_0);
	order0_vmstat_print(m, "order0_cow_chain_depth_1",
			    ORDER0_COUNTER_COW_CHAIN_DEPTH_1);
	order0_vmstat_print(m, "order0_cow_chain_depth_2",
			    ORDER0_COUNTER_COW_CHAIN_DEPTH_2);
	order0_vmstat_print(m, "order0_cow_chain_depth_3p",
			    ORDER0_COUNTER_COW_CHAIN_DEPTH_3P);
	order0_vmstat_print(m, "order0_split_partial_unmap",
			    ORDER0_COUNTER_SPLIT_PARTIAL_UNMAP);
	for (flow = 0; flow < ORDER0_PCP_FLOW_NR; flow++)
		for (bucket = 0; bucket < ORDER0_MT_NR; bucket++)
			seq_printf(m, "order0_pcp_%s_%s %lu\n",
				   order0_pcp_flow_names[flow],
				   order0_migratetype_names[bucket],
				   order0_counter_sum(order0_pcp_counter(flow, bucket)));
}
