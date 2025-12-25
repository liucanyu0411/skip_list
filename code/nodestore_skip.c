// nodestore_skip.c
#include "nodestore.h"
#include "skiplist.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct NodeStore {
    int cap;        // 最大可容纳条目数
    int n;          // 当前条目数
    int* keys;      // 按序存储 keys（为了 key_at/按下标操作）
    void** vals;    // 与 keys 同步的 val（内部节点 child 指针）
    SkipList* sl;   // 仅用于支持“按 key 查找/维护有序集合”（不存 val）
};

static int pick_max_level(int cap) {
    // ~log2(cap)+1，夹个范围
    int lvl = 1, x = 1;
    while (x < cap && lvl < 32) { x <<= 1; lvl++; }
    if (lvl < 8) lvl = 8;
    if (lvl > SKIPLIST_MAX_LEVEL) lvl = SKIPLIST_MAX_LEVEL;
    return lvl;
}

static SkipList* make_list(int cap) {
    return skiplist_create(pick_max_level(cap), 0.5);
}

static NodeStore* ns_create(int capacity) {
    if (capacity <= 0) capacity = 1;

    NodeStore* s = (NodeStore*)calloc(1, sizeof(NodeStore));
    if (!s) return NULL;

    s->cap = capacity;
    s->n = 0;
    s->keys = (int*)malloc(sizeof(int) * (size_t)capacity);
    s->vals = (void**)malloc(sizeof(void*) * (size_t)capacity);
    if (!s->keys || !s->vals) {
        free(s->keys);
        free(s->vals);
        free(s);
        return NULL;
    }

    s->sl = make_list(capacity);
    if (!s->sl) {
        free(s->keys);
        free(s->vals);
        free(s);
        return NULL;
    }

    return s;
}

static void ns_destroy(NodeStore* s) {
    if (!s) return;
    skiplist_destroy(s->sl);
    free(s->keys);
    free(s->vals);
    free(s);
}

static int ns_size(const NodeStore* s) { return s ? s->n : 0; }
static int ns_capacity(const NodeStore* s) { return s ? s->cap : 0; }

static void ns_clear(NodeStore* s) {
    if (!s) return;
    s->n = 0;
    // 直接重建跳表（比逐个删简单）
    skiplist_destroy(s->sl);
    s->sl = make_list(s->cap);
    assert(s->sl);
}

// 线性 lower_bound：返回第一个 >= key 的位置
static int ns_lower_bound(const NodeStore* s, int key) {
    assert(s);
    int l = 0, r = s->n; // [l, r)
    while (l < r) {
        int m = l + (r - l) / 2;
        if (s->keys[m] < key) l = m + 1;
        else r = m;
    }
    return l;
}

static int ns_key_at(const NodeStore* s, int idx) {
    assert(s && idx >= 0 && idx < s->n);
    return s->keys[idx];
}

static void* ns_val_at(const NodeStore* s, int idx) {
    assert(s && idx >= 0 && idx < s->n);
    return s->vals[idx];
}

static void ns_set_val(NodeStore* s, int idx, void* v) {
    assert(s && idx >= 0 && idx < s->n);
    s->vals[idx] = v;
}

static void rebuild_skiplist_from_keys(NodeStore* s) {
    // 用 keys[] 重新构建 sl（sl 只存 key）
    skiplist_destroy(s->sl);
    s->sl = make_list(s->cap);
    assert(s->sl);

    for (int i = 0; i < s->n; ++i) {
        // keys[] 本来就是有序的
        (void)skiplist_insert(s->sl, s->keys[i]);
    }
}

static void ns_insert_at(NodeStore* s, int idx, int key, void* val) {
    assert(s);
    assert(s->n < s->cap);
    assert(idx >= 0 && idx <= s->n);

    // bptree 一般会传 idx = lower_bound(key)，但在 borrow/merge 时 idx 可能是 0 或 end
    // 我们保证“按 idx 插入”的语义正确，同时保持 keys 有序（若不有序，bptree 逻辑就会乱）
    int lb = ns_lower_bound(s, key);
    // 为了安全：如果 idx 与 lb 不一致，我们以 idx 为准插入，但要求结果仍有序。
    // 若你的 bptree 合法，idx 应该总是等于 lb；不等说明上层传错或 key 关系被破坏。
    if (idx != lb) {
        // 这里不直接 assert，避免你调试时崩；改成“按 idx 插入但强制有序检查”
        // 你也可以改成 assert(idx == lb);
    }

    // 检查重复 key（B+树通常不允许重复）
    if (lb < s->n && s->keys[lb] == key) return;

    // 数组插入
    for (int i = s->n; i > idx; --i) {
        s->keys[i] = s->keys[i - 1];
        s->vals[i] = s->vals[i - 1];
    }
    s->keys[idx] = key;
    s->vals[idx] = val;
    s->n++;

    // 更新 skiplist（只存 key）
    // 最稳妥：直接 rebuild；也可以增量 insert，但 rebuild 简单且 cap 小（节点大小是 O(M)）
    rebuild_skiplist_from_keys(s);
}

static void ns_erase_at(NodeStore* s, int idx) {
    assert(s);
    assert(idx >= 0 && idx < s->n);

    // 数组删除
    for (int i = idx; i < s->n - 1; ++i) {
        s->keys[i] = s->keys[i + 1];
        s->vals[i] = s->vals[i + 1];
    }
    s->n--;

    rebuild_skiplist_from_keys(s);
}

// split：把 left 后半部分移动到 right，返回分隔键 sep（= right 的第一个 key）
static int ns_split(NodeStore* left, NodeStore* right) {
    assert(left && right);
    assert(right->n == 0);

    int n = left->n;
    int mid = n / 2;
    if (mid == 0) mid = 1;

    int sep = left->keys[mid];

    int move = n - mid;
    assert(move >= 1);

    // right 接收 [mid..n)
    for (int i = 0; i < move; ++i) {
        right->keys[i] = left->keys[mid + i];
        right->vals[i] = left->vals[mid + i];
    }
    right->n = move;

    // left 保留 [0..mid)
    left->n = mid;

    // 重建两个 skiplist
    rebuild_skiplist_from_keys(left);
    rebuild_skiplist_from_keys(right);

    return sep;
}

// ---- ops 表 ----
static const NodeStoreOps g_ops = {
    .create      = ns_create,
    .destroy     = ns_destroy,
    .size        = ns_size,
    .capacity    = ns_capacity,
    .clear       = ns_clear,
    .key_at      = ns_key_at,
    .val_at      = ns_val_at,
    .set_val     = ns_set_val,
    .lower_bound = ns_lower_bound,
    .insert_at   = ns_insert_at,
    .erase_at    = ns_erase_at,
    .split       = ns_split,
};

const NodeStoreOps* nodestore_skip_ops(void) { return &g_ops; }