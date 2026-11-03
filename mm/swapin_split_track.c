// SPDX-License-Identifier: GPL-2.0
/*
 * Correlate a 16K anon VMA window cut by split with the first 4K swapin
 * fallback and the zram partial reads that follow.
 *
 * Design:
 *   - split at addr A cuts exactly one 16K-aligned window when A is not
 *     16K-aligned.  Enqueue (anon_vma, win) in a hash, no need to walk or
 *     enqueue all VMAs.
 *   - swapin fallback looks up the same key.  A hit means the first 4K
 *     fallback happened on a window that was split after the 16K swapout.
 *   - zram partial read is keyed by the object head (index & ~3), and walks
 *     a second hash head -> split window.  The 2nd/3rd/4th partial reads are
 *     recorded as cascade of the same object.
 *   - a per-CPU-order ring keeps the event history for offline joining, so
 *     hash eviction does not silently lose the causal chain.
 */
#include <linux/swapin_split_track.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/hash.h>
#include <linux/init.h>
#include <linux/jump_label.h>
#include <linux/kstrtox.h>
#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/rmap.h>
#include <linux/seq_file.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/uaccess.h>

#define SWSPLIT_NR		4
#define SWSPLIT_WINDOW		(1UL << 14)
#define SWSPLIT_BUCKETS		(1U << 15)
#define SWSPLIT_HEAD_BUCKETS	(1U << 12)
#define SWSPLIT_MAX_ENTRIES	32768
#define SWSPLIT_RING		8192

enum swsplit_state {
	SWSPLIT_ST_SPLIT = 0,
	SWSPLIT_ST_FALLBACK,
};

enum swsplit_event_type {
	SWSPLIT_EV_SPLIT = 1,
	SWSPLIT_EV_FALLBACK_HIT,
	SWSPLIT_EV_FALLBACK_MISS,
	SWSPLIT_EV_FIRST_PARTIAL,
	SWSPLIT_EV_PARTIAL,
	SWSPLIT_EV_PARTIAL_UNKNOWN,
	SWSPLIT_EV_RESOLVE,
};

struct swsplit_event {
	u64 seq;
	u64 ts;
	u64 anon_vma;
	unsigned long win;
	unsigned long addr;
	unsigned long vma_start;
	unsigned long vma_end;
	u32 type;
	u32 head;
	u32 reason;
	u32 offset;
	u32 count;
};

struct swsplit_entry {
	struct hlist_node hnode;
	struct list_head lru;
	struct list_head free_node;
	struct anon_vma *anon_vma;
	unsigned long win;
	u32 head;
	u8 state;
	u8 split_off;
	u8 first_reason;
	u8 partial_count;
	u8 offset_mask;
	u64 ts_split;
	u64 ts_first;
};

struct swsplit_stats {
	u64 split_enqueued;
	u64 split_dup;
	u64 split_aligned;
	u64 split_uncovered;
	u64 null_anon;
	u64 null_vma;
	u64 alloc_fail;
	u64 evict;
	u64 fallback_hit;
	u64 fallback_miss;
	u64 resolve_16k;
	u64 resolve_other;
	u64 partial;
	u64 first_partial;
	u64 partial_unknown;
	u64 ring_lost;
};

static DEFINE_STATIC_KEY_FALSE(swapin_split_key);
static DEFINE_SPINLOCK(swsplit_lock);

static struct hlist_head swsplit_buckets[SWSPLIT_BUCKETS];
static LIST_HEAD(swsplit_lru);
static u32 swsplit_count;
static struct swsplit_stats swsplit_stats;

static struct swsplit_event swsplit_ring[SWSPLIT_RING];
static u32 swsplit_ring_tail;
static u32 swsplit_ring_count;
static u64 swsplit_seq;

/* head -> last fallback reason; lets userspace join zram partial heads */
#define SWSPLIT_FB_SIZE	32768
struct swsplit_fb {
	u32 head;
	u32 reason;
	u32 count;
	unsigned long win;
	u64 anon_vma;
	u64 ts;
	u8 used;
};
static struct swsplit_fb swsplit_fb[SWSPLIT_FB_SIZE];

static inline bool swsplit_on(void)
{
	return static_branch_unlikely(&swapin_split_key);
}

static u32 swsplit_bucket(struct anon_vma *anon_vma, unsigned long win)
{
	return (u32)hash_long((unsigned long)anon_vma ^ (win >> PAGE_SHIFT),
			      ilog2(SWSPLIT_BUCKETS));
}

static struct swsplit_entry *swsplit_find_locked(struct anon_vma *anon_vma,
						 unsigned long win)
{
	struct swsplit_entry *e;
	u32 b = swsplit_bucket(anon_vma, win);

	hlist_for_each_entry(e, &swsplit_buckets[b], hnode) {
		if (e->anon_vma == anon_vma && e->win == win)
			return e;
	}
	return NULL;
}

static void swsplit_unlink_locked(struct swsplit_entry *e)
{
	if (!hlist_unhashed(&e->hnode))
		hlist_del_init(&e->hnode);
	if (!list_empty(&e->lru))
		list_del_init(&e->lru);
}

static void swsplit_record_locked(u32 type, struct anon_vma *anon_vma,
				  unsigned long win, unsigned long addr,
				  unsigned long vma_start, unsigned long vma_end,
				  u32 head, u32 reason, u32 offset, u32 count)
{
	struct swsplit_event *ev;

	if (swsplit_ring_count == SWSPLIT_RING) {
		swsplit_ring_tail = (swsplit_ring_tail + 1) % SWSPLIT_RING;
		swsplit_ring_count--;
		swsplit_stats.ring_lost++;
	}
	ev = &swsplit_ring[(swsplit_ring_tail + swsplit_ring_count) %
			   SWSPLIT_RING];
	memset(ev, 0, sizeof(*ev));
	ev->seq = ++swsplit_seq;
	ev->ts = ktime_get_ns();
	ev->type = type;
	ev->anon_vma = (u64)(unsigned long)anon_vma;
	ev->win = win;
	ev->addr = addr;
	ev->vma_start = vma_start;
	ev->vma_end = vma_end;
	ev->head = head;
	ev->reason = reason;
	ev->offset = offset;
	ev->count = count;
	swsplit_ring_count++;
}

static void swsplit_free_list(struct list_head *head)
{
	struct swsplit_entry *e, *tmp;

	list_for_each_entry_safe(e, tmp, head, free_node) {
		list_del(&e->free_node);
		if (e->anon_vma)
			put_anon_vma(e->anon_vma);
		kfree(e);
	}
}

void swapin_split_note_split(struct anon_vma *anon_vma, unsigned long win,
			     unsigned long addr, unsigned long orig_start,
			     unsigned long orig_end)
{
	struct swsplit_entry *e, *victim;
	LIST_HEAD(free_list);
	unsigned long flags;

	if (!swsplit_on())
		return;
	if (!anon_vma) {
		swsplit_stats.null_anon++;
		return;
	}
	if (!(addr & (SWSPLIT_WINDOW - 1))) {
		swsplit_stats.split_aligned++;
		return;
	}
	win = ALIGN_DOWN(addr, SWSPLIT_WINDOW);
	if (win < orig_start || win + SWSPLIT_WINDOW > orig_end) {
		swsplit_stats.split_uncovered++;
		return;
	}

	e = kzalloc(sizeof(*e), GFP_KERNEL);
	if (!e) {
		swsplit_stats.alloc_fail++;
		return;
	}
	INIT_LIST_HEAD(&e->lru);
	INIT_LIST_HEAD(&e->free_node);
	e->anon_vma = anon_vma;
	e->win = win;
	e->split_off = (addr - win) >> PAGE_SHIFT;
	e->state = SWSPLIT_ST_SPLIT;
	e->ts_split = ktime_get_ns();
	get_anon_vma(anon_vma);

	spin_lock_irqsave(&swsplit_lock, flags);
	if (swsplit_find_locked(anon_vma, win)) {
		swsplit_stats.split_dup++;
		spin_unlock_irqrestore(&swsplit_lock, flags);
		put_anon_vma(anon_vma);
		kfree(e);
		return;
	}
	while (swsplit_count >= SWSPLIT_MAX_ENTRIES &&
	       !list_empty(&swsplit_lru)) {
		victim = list_first_entry(&swsplit_lru, struct swsplit_entry,
					  lru);
		swsplit_unlink_locked(victim);
		swsplit_count--;
		swsplit_stats.evict++;
		list_add_tail(&victim->free_node, &free_list);
	}
	hlist_add_head(&e->hnode, &swsplit_buckets[swsplit_bucket(anon_vma, win)]);
	list_add_tail(&e->lru, &swsplit_lru);
	swsplit_count++;
	swsplit_stats.split_enqueued++;
	/* Only the hash keeps the split state; the ring is reserved for
	 * first/cascade partial events so that 30k+ splits cannot flood it. */
	spin_unlock_irqrestore(&swsplit_lock, flags);

	swsplit_free_list(&free_list);
}

static void swsplit_note_fb_locked(u32 head, int reason, unsigned long win,
				 struct anon_vma *anon_vma)
{
	u32 start = hash_32(head, ilog2(SWSPLIT_FB_SIZE));
	struct swsplit_fb *e = NULL;
	int i;

	for (i = 0; i < 8; i++) {
		struct swsplit_fb *c = &swsplit_fb[(start + i) & (SWSPLIT_FB_SIZE - 1)];

		if (!c->used || c->head == head) {
			e = c;
			break;
		}
	}
	if (!e)
		e = &swsplit_fb[start];
	if (!e->used || e->head != head) {
		e->used = 1;
		e->head = head;
		e->count = 0;
	}
	e->reason = reason;
	e->count++;
	e->win = win;
	e->anon_vma = (u64)(unsigned long)anon_vma;
	e->ts = ktime_get_ns();
}

void swapin_split_note_fallback(struct vm_fault *vmf, unsigned long entry_val,
				int reason)
{
	struct swsplit_entry *e;
	swp_entry_t entry = { .val = entry_val };
	unsigned long win, flags;
	u32 head;

	if (!swsplit_on())
		return;
	if (!vmf || !vmf->vma || !vmf->vma->anon_vma) {
		swsplit_stats.null_vma++;
		return;
	}
	win = ALIGN_DOWN(vmf->address, SWSPLIT_WINDOW);
	head = swp_offset(entry) & ~(SWSPLIT_NR - 1);

	spin_lock_irqsave(&swsplit_lock, flags);
	swsplit_note_fb_locked(head, reason, win, vmf->vma->anon_vma);
	e = swsplit_find_locked(vmf->vma->anon_vma, win);
	if (e) {
		u32 off = (vmf->address - win) >> PAGE_SHIFT;

		if (off >= SWSPLIT_NR)
			off = SWSPLIT_NR - 1;
		e->head = head;
		if (e->state != SWSPLIT_ST_FALLBACK) {
			e->state = SWSPLIT_ST_FALLBACK;
			e->first_reason = reason;
			e->ts_first = ktime_get_ns();
		}
		e->partial_count++;
		e->offset_mask |= 1U << off;
		swsplit_stats.fallback_hit++;
		if (e->partial_count == 1) {
			swsplit_stats.first_partial++;
			swsplit_record_locked(SWSPLIT_EV_FIRST_PARTIAL,
					      e->anon_vma, e->win,
					      vmf->address,
					      vmf->vma->vm_start,
					      vmf->vma->vm_end, head, reason,
					      off, e->partial_count);
		} else {
			swsplit_stats.partial++;
			swsplit_record_locked(SWSPLIT_EV_PARTIAL, e->anon_vma,
					      e->win, vmf->address,
					      vmf->vma->vm_start,
					      vmf->vma->vm_end, head, reason,
					      off, e->partial_count);
		}
	} else {
		swsplit_stats.fallback_miss++;
	}
	spin_unlock_irqrestore(&swsplit_lock, flags);
}

void swapin_split_note_success(struct vm_fault *vmf, unsigned long entry_val,
			       int order)
{
	struct swsplit_entry *e;
	swp_entry_t entry = { .val = entry_val };
	unsigned long win, flags;
	u32 head;

	if (!swsplit_on())
		return;
	if (!vmf || !vmf->vma || !vmf->vma->anon_vma)
		return;
	win = ALIGN_DOWN(vmf->address, SWSPLIT_WINDOW);
	head = swp_offset(entry) & ~(SWSPLIT_NR - 1);

	spin_lock_irqsave(&swsplit_lock, flags);
	e = swsplit_find_locked(vmf->vma->anon_vma, win);
	if (!e) {
		spin_unlock_irqrestore(&swsplit_lock, flags);
		return;
	}
	if (order >= 2) {
		swsplit_unlink_locked(e);
		swsplit_count--;
		swsplit_stats.resolve_16k++;
		swsplit_record_locked(SWSPLIT_EV_RESOLVE, e->anon_vma, e->win,
				      vmf->address, vmf->vma->vm_start,
				      vmf->vma->vm_end, head,
				      SWSPLIT_R_OK, 0, order);
		spin_unlock_irqrestore(&swsplit_lock, flags);
		put_anon_vma(e->anon_vma);
		kfree(e);
		return;
	}
	e->head = head;
	if (e->state != SWSPLIT_ST_FALLBACK) {
		e->state = SWSPLIT_ST_FALLBACK;
		e->first_reason = SWSPLIT_R_LOWER_ORDER;
		e->ts_first = ktime_get_ns();
	}
	e->partial_count++;
	e->offset_mask |= 1U << ((vmf->address - win) >> PAGE_SHIFT);
	swsplit_stats.resolve_other++;
	if (e->partial_count == 1) {
		swsplit_stats.first_partial++;
		swsplit_record_locked(SWSPLIT_EV_FIRST_PARTIAL, e->anon_vma,
				      e->win, vmf->address,
				      vmf->vma->vm_start, vmf->vma->vm_end,
				      head, SWSPLIT_R_LOWER_ORDER,
				      (vmf->address - win) >> PAGE_SHIFT,
				      e->partial_count);
	} else {
		swsplit_stats.partial++;
		swsplit_record_locked(SWSPLIT_EV_PARTIAL, e->anon_vma, e->win,
				      vmf->address, vmf->vma->vm_start,
				      vmf->vma->vm_end, head,
				      SWSPLIT_R_LOWER_ORDER,
				      (vmf->address - win) >> PAGE_SHIFT,
				      e->partial_count);
	}
	spin_unlock_irqrestore(&swsplit_lock, flags);
}

static void swsplit_reset(void)
{
	struct swsplit_entry *e, *tmp;
	LIST_HEAD(free_list);
	unsigned long flags;

	spin_lock_irqsave(&swsplit_lock, flags);
	list_for_each_entry_safe(e, tmp, &swsplit_lru, lru) {
		swsplit_unlink_locked(e);
		swsplit_count--;
		list_add_tail(&e->free_node, &free_list);
	}
	swsplit_count = 0;
	swsplit_ring_tail = 0;
	swsplit_ring_count = 0;
	swsplit_seq = 0;
	memset(&swsplit_stats, 0, sizeof(swsplit_stats));
	spin_unlock_irqrestore(&swsplit_lock, flags);
	swsplit_free_list(&free_list);
}

/* ------------------------------------------------------------------ debugfs */

static const char *swsplit_reason_name(u32 reason)
{
	switch (reason) {
	case SWSPLIT_R_OK:		return "OK";
	case SWSPLIT_R_VMA_WINDOW:	return "VMA_WINDOW";
	case SWSPLIT_R_THP_ALLOW:	return "THP_ALLOW";
	case SWSPLIT_R_PHASE:		return "PHASE";
	case SWSPLIT_R_PTE_BATCH:	return "PTE_BATCH";
	case SWSPLIT_R_PTE_START:	return "PTE_START";
	case SWSPLIT_R_PTE_NOT_SWAP:	return "PTE_NOT_SWAP";
	case SWSPLIT_R_PTE_OFFSET:	return "PTE_OFFSET";
	case SWSPLIT_R_PTE_CGROUP:	return "PTE_CGROUP";
	case SWSPLIT_R_ZEROMAP:		return "ZEROMAP";
	case SWSPLIT_R_SWAPCACHE:	return "SWAPCACHE";
	case SWSPLIT_R_ALLOC:		return "ALLOC";
	case SWSPLIT_R_CHARGE:		return "CHARGE";
	case SWSPLIT_R_PREENTRY:	return "PREENTRY";
	case SWSPLIT_R_LOWER_ORDER:	return "LOWER_ORDER";
	default:			return "OTHER";
	}
}

static void *swsplit_ev_start(struct seq_file *m, loff_t *pos)
{
	return *pos < SWSPLIT_RING ? pos : NULL;
}

static void *swsplit_ev_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return *pos < SWSPLIT_RING ? pos : NULL;
}

static void swsplit_ev_stop(struct seq_file *m, void *v)
{
}

static int swsplit_ev_show(struct seq_file *m, void *v)
{
	loff_t i = *(loff_t *)v;
	struct swsplit_event ev;
	unsigned long flags;
	bool ok = false;

	spin_lock_irqsave(&swsplit_lock, flags);
	if (i < swsplit_ring_count) {
		ev = swsplit_ring[(swsplit_ring_tail + i) % SWSPLIT_RING];
		ok = true;
	}
	spin_unlock_irqrestore(&swsplit_lock, flags);
	if (!ok)
		return 0;
	seq_printf(m,
		   "%llu %llu type=%u anon=%llx win=%lx addr=%lx vma=[%lx,%lx) head=%u off=%u cnt=%u reason=%s(%u)\n",
		   ev.seq, ev.ts, ev.type, ev.anon_vma, ev.win, ev.addr,
		   ev.vma_start, ev.vma_end, ev.head, ev.offset, ev.count,
		   swsplit_reason_name(ev.reason), ev.reason);
	return 0;
}

static const struct seq_operations swsplit_ev_ops = {
	.start = swsplit_ev_start,
	.next = swsplit_ev_next,
	.stop = swsplit_ev_stop,
	.show = swsplit_ev_show,
};

static int swsplit_ev_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &swsplit_ev_ops);
}

static const struct file_operations swsplit_ev_fops = {
	.owner = THIS_MODULE,
	.open = swsplit_ev_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static void *swsplit_fb_start(struct seq_file *m, loff_t *pos)
{
	return *pos < SWSPLIT_FB_SIZE ? pos : NULL;
}

static void *swsplit_fb_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return *pos < SWSPLIT_FB_SIZE ? pos : NULL;
}

static void swsplit_fb_stop(struct seq_file *m, void *v)
{
}

static int swsplit_fb_show(struct seq_file *m, void *v)
{
	loff_t i = *(loff_t *)v;
	struct swsplit_fb e;
	unsigned long flags;

	spin_lock_irqsave(&swsplit_lock, flags);
	e = swsplit_fb[i];
	spin_unlock_irqrestore(&swsplit_lock, flags);
	if (!e.used)
		return 0;
	seq_printf(m, "head=%u reason=%s(%u) count=%u win=%lx anon=%llx ts=%llu\n",
		   e.head, swsplit_reason_name(e.reason), e.reason, e.count,
		   e.win, e.anon_vma, e.ts);
	return 0;
}

static const struct seq_operations swsplit_fb_ops = {
	.start = swsplit_fb_start,
	.next = swsplit_fb_next,
	.stop = swsplit_fb_stop,
	.show = swsplit_fb_show,
};

static int swsplit_fb_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &swsplit_fb_ops);
}

static const struct file_operations swsplit_fb_fops = {
	.owner = THIS_MODULE,
	.open = swsplit_fb_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int swsplit_stats_show(struct seq_file *m, void *v)
{
	struct swsplit_stats s;
	unsigned long flags;
	u32 active, ring;

	spin_lock_irqsave(&swsplit_lock, flags);
	s = swsplit_stats;
	active = swsplit_count;
	ring = swsplit_ring_count;
	spin_unlock_irqrestore(&swsplit_lock, flags);

	seq_printf(m, "active_entries %u\n", active);
	seq_printf(m, "ring_count %u\n", ring);

	seq_printf(m, "split_enqueued %llu\n", s.split_enqueued);
	seq_printf(m, "split_dup %llu\n", s.split_dup);
	seq_printf(m, "split_aligned %llu\n", s.split_aligned);
	seq_printf(m, "split_uncovered %llu\n", s.split_uncovered);
	seq_printf(m, "null_anon %llu\n", s.null_anon);
	seq_printf(m, "null_vma %llu\n", s.null_vma);
	seq_printf(m, "alloc_fail %llu\n", s.alloc_fail);
	seq_printf(m, "evict %llu\n", s.evict);
	seq_printf(m, "fallback_hit %llu\n", s.fallback_hit);
	seq_printf(m, "fallback_miss %llu\n", s.fallback_miss);
	seq_printf(m, "resolve_16k %llu\n", s.resolve_16k);
	seq_printf(m, "resolve_other %llu\n", s.resolve_other);
	seq_printf(m, "first_partial %llu\n", s.first_partial);
	seq_printf(m, "partial %llu\n", s.partial);
	seq_printf(m, "partial_unknown %llu\n", s.partial_unknown);
	seq_printf(m, "ring_lost %llu\n", s.ring_lost);
	return 0;
}

static int swsplit_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, swsplit_stats_show, NULL);
}

static const struct file_operations swsplit_stats_fops = {
	.owner = THIS_MODULE,
	.open = swsplit_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static ssize_t swsplit_enable_write(struct file *file, const char __user *ubuf,
				    size_t len, loff_t *ppos)
{
	char buf[8];
	int val;

	if (len >= sizeof(buf))
		return -EINVAL;
	if (copy_from_user(buf, ubuf, len))
		return -EFAULT;
	buf[len] = '\0';
	if (kstrtoint(buf, 0, &val))
		return -EINVAL;
	if (val)
		static_branch_enable(&swapin_split_key);
	else
		static_branch_disable(&swapin_split_key);
	return len;
}

static ssize_t swsplit_enable_read(struct file *file, char __user *ubuf,
				   size_t len, loff_t *ppos)
{
	char buf[4];
	int n = scnprintf(buf, sizeof(buf), "%d\n",
			  static_branch_unlikely(&swapin_split_key) ? 1 : 0);

	return simple_read_from_buffer(ubuf, len, ppos, buf, n);
}

static const struct file_operations swsplit_enable_fops = {
	.owner = THIS_MODULE,
	.read = swsplit_enable_read,
	.write = swsplit_enable_write,
	.llseek = default_llseek,
};

static ssize_t swsplit_reset_write(struct file *file, const char __user *ubuf,
				   size_t len, loff_t *ppos)
{
	swsplit_reset();
	return len;
}

static const struct file_operations swsplit_reset_fops = {
	.owner = THIS_MODULE,
	.write = swsplit_reset_write,
	.llseek = default_llseek,
};

EXPORT_SYMBOL_GPL(swapin_split_note_split);
EXPORT_SYMBOL_GPL(swapin_split_note_fallback);
EXPORT_SYMBOL_GPL(swapin_split_note_success);

static int __init swapin_split_track_init(void)
{
	struct dentry *dir;

	/* Experiment kernel: collect by default; debugfs still allows off. */
	static_branch_enable(&swapin_split_key);

	dir = debugfs_create_dir("swapin_split", NULL);
	if (IS_ERR_OR_NULL(dir))
		return 0;
	debugfs_create_file("enable", 0600, dir, NULL, &swsplit_enable_fops);
	debugfs_create_file("reset", 0200, dir, NULL, &swsplit_reset_fops);
	debugfs_create_file("stats", 0400, dir, NULL, &swsplit_stats_fops);
	debugfs_create_file("events", 0400, dir, NULL, &swsplit_ev_fops);
	debugfs_create_file("fallbacks", 0400, dir, NULL, &swsplit_fb_fops);
	return 0;
}
late_initcall(swapin_split_track_init);
