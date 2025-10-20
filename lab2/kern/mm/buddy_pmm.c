#include <pmm.h>
#include <list.h>
#include <string.h>
#include <buddy_pmm.h>
#include <stdio.h>

typedef struct {
    size_t size;              
    size_t *longest;
    struct Page *base;
    unsigned int nr_free;
} buddy_tree_t;

static buddy_tree_t buddy_tree;

#define MAX_BUDDY_PAGES 16384
static size_t buddy_longest_static[2 * MAX_BUDDY_PAGES - 1];

#define LEFT_CHILD(idx)  ((idx)*2 + 1)
#define RIGHT_CHILD(idx) ((idx)*2 + 2)
#define PARENT(idx)      (((idx) - 1) / 2)

#define MAX(a, b) ((a) > (b) ? (a) : (b))

static inline size_t floor_power_of_2(size_t n) {
    if (n == 0) return 0;
    size_t p = 1;
    while (p <= n) p <<= 1;
    return p >> 1;
}

static inline size_t next_power_of_2(size_t n) {
    if (n == 0) return 1;
    size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

static void print_buddy_tree(int idx) {
    cprintf("Buddy Tree State %d:\n", idx);

    cprintf("%d\n", buddy_tree.longest[0]);

    cprintf("%d\t", buddy_tree.longest[1]);
    cprintf("%d\n", buddy_tree.longest[2]);

    cprintf("%d\t", buddy_tree.longest[3]);
    cprintf("%d\t", buddy_tree.longest[4]);
    cprintf("%d\t", buddy_tree.longest[5]);
    cprintf("%d\n\n", buddy_tree.longest[6]);
}

static void
buddy_init(void) {
    buddy_tree.size = 0;
    buddy_tree.longest = NULL;
    buddy_tree.base = NULL;
    buddy_tree.nr_free = 0;
}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    cprintf("============= Enter buddy_init_memmap =============\n");
    cprintf("[INFO] Total input pages: %lu\n", (unsigned long)n);
    assert(n > 0);

    // 向下取2的幂，得到可管理页面数
    size_t real_size = floor_power_of_2(n);
    if (real_size > MAX_BUDDY_PAGES) {
        cprintf("[WARN] real_size (%lu) exceeds MAX_BUDDY_PAGES (%d), truncating\n",
                (unsigned long)real_size, MAX_BUDDY_PAGES);
        real_size = MAX_BUDDY_PAGES;
    }

    cprintf("[INFO] Adjusted real_size (power-of-2): %lu\n", (unsigned long)real_size);
    cprintf("[INFO] Buddy tree base VA: %p\n", base);
    cprintf("[INFO] Buddy longest array (static) VA: %p\n", buddy_longest_static);

    // 初始化buddy_tree
    buddy_tree.base = base;
    buddy_tree.size = real_size;
    buddy_tree.longest = buddy_longest_static;
    buddy_tree.nr_free = real_size;

    // 初始化longest数组
    size_t total_nodes = 2 * real_size - 1;
    size_t node_size = real_size;
    size_t next_boundary = 1;

    cprintf("[INFO] Initializing longest array (%lu nodes)...\n", (unsigned long)total_nodes);
    for (size_t i = 0; i < total_nodes; i++) {
        buddy_tree.longest[i] = node_size;
        if (i + 1 == next_boundary) {
            node_size >>= 1;
            next_boundary = next_boundary * 2 + 1;
        }
    }

    cprintf("[INFO] longest[0] = %lu (root block size)\n", (unsigned long)buddy_tree.longest[0]);
    cprintf("[INFO] longest[1] = %lu (first level block size)\n", (unsigned long)buddy_tree.longest[1]);
    cprintf("[INFO] longest[2] = %lu (second level block size)\n", (unsigned long)buddy_tree.longest[2]);
    cprintf("[INFO] longest[3] = %lu (third level block size)\n", (unsigned long)buddy_tree.longest[3]);
    cprintf("[INFO] Buddy system initialized successfully!\n");
    cprintf("============= Exit buddy_init_memmap ==============\n");
}

static struct Page*
buddy_alloc_pages(size_t n) {
    assert(n > 0);

    // 向上取2的幂
    size_t npages = next_power_of_2(n);

    // 根节点可用性检查
    if (buddy_tree.longest[0] < npages)
        return NULL;

    // 遍历树
    size_t node_size;
    size_t idx = 0;
    for(node_size = buddy_tree.size; node_size != npages; node_size /= 2 ) {
        if (buddy_tree.longest[LEFT_CHILD(idx)] >= npages)
        idx = LEFT_CHILD(idx);
        else
        idx = RIGHT_CHILD(idx);
    }
    buddy_tree.nr_free -= npages;

    // 定位页面并标记为已分配
    size_t offset = (idx + 1) * node_size - buddy_tree.size;
    struct Page* page = buddy_tree.base + offset;
    ClearPageProperty(page);

    // 更新 longest
    buddy_tree.longest[idx] = 0;
    while (idx) {
        idx = PARENT(idx);
        buddy_tree.longest[idx] =
        MAX(buddy_tree.longest[LEFT_CHILD(idx)], buddy_tree.longest[RIGHT_CHILD(idx)]);
    }

    return page;
}

static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0);

    size_t npages = next_power_of_2(n);
    size_t node_size = npages;
    size_t offset = base - buddy_tree.base;
    size_t idx = (offset + buddy_tree.size) / node_size - 1;

    // 设置头页 PG_property
    SetPageProperty(base);
    buddy_tree.nr_free += npages;

    // 更新 longest 数组
    buddy_tree.longest[idx] = node_size;
    while (idx) {
        idx = PARENT(idx);
        node_size *= 2;

        size_t left_longest = buddy_tree.longest[LEFT_CHILD(idx)];
        size_t right_longest = buddy_tree.longest[RIGHT_CHILD(idx)];
        if (left_longest + right_longest == node_size)
            buddy_tree.longest[idx] = node_size;
        else
            buddy_tree.longest[idx] = MAX(left_longest, right_longest);
    }
}

static size_t
buddy_nr_free_pages(void) {
    return buddy_tree.nr_free;
}

static void
buddy_check(void) {
    cprintf("============= Enter buddy_check =============\n");

    struct Page *p1 = buddy_alloc_pages(16385);
    assert(p1 == NULL);
    print_buddy_tree(1);

    struct Page *p2 = buddy_alloc_pages(4090);
    assert(p2 != NULL);
    assert(!PageProperty(p2));
    print_buddy_tree(2);

    struct Page *p3 = buddy_alloc_pages(8190);
    assert(p3 != NULL);
    assert(!PageProperty(p3));
    print_buddy_tree(3);

    struct Page *p4 = buddy_alloc_pages(8190);
    assert(p4 == NULL);
    print_buddy_tree(4);

    buddy_free_pages(p2, 4090);
    print_buddy_tree(2);

    struct Page *p5 = buddy_alloc_pages(8190);
    assert(p5 != NULL);
    assert(!PageProperty(p5));
    print_buddy_tree(5);

    buddy_free_pages(p3, 8190);
    assert(buddy_tree.nr_free == buddy_tree.size - 8192);
    print_buddy_tree(3);

    buddy_free_pages(p5, 8190);
    assert(buddy_tree.nr_free == buddy_tree.size);
    print_buddy_tree(5);

    cprintf("============= Exit buddy_check ==============\n");
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
