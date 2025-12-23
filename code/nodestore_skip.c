// nodestore_skip.c
#include "nodestore.h"
#include "skiplist.h"
#include <assert.h>
#include <stdlib.h>

struct NodeStore {
    int cap;
    SkipList* sl;
};

static NodeStore* ns_create(int capacity) {
    NodeStore* s = (NodeStore*)calloc(1, sizeof(NodeStore));
    if (!s) return NULL;
    s->cap = capacity;
    s->sl = skiplist_create(16, 0.5, 1234567u);
    if (!s->sl) { free(s); return NULL; }
    return s;
}

static void ns_destroy(NodeStore* s) {
    if (!s) return;
    skiplist_destroy(s->sl);
    free(s);
}

static int ns_size(const NodeStore* s) { return s ? skiplist_size(s->sl) : 0; }
static int ns_capacity(const NodeStore* s) { return s ? s->cap : 0; }

static void ns_clear(NodeStore* s) {
    if (!s) return;
    skiplist_destroy(s->sl);
    s->sl = skiplist_create(16, 0.5, 1234567u);
    assert(s->sl);
}

static SkipNode* at_index(const NodeStore* s, int idx) {
    assert(s);
    int n = skiplist_size(s->sl);
    assert(idx >= 0 && idx < n);
    SkipNode* x = skiplist_first(s->sl);
    for (int i = 0; i < idx; ++i) x = skipnode_next0(x);
    return x;
}

static int ns_key_at(const NodeStore* s, int idx) {
    SkipNode* x = at_index(s, idx);
    return skipnode_key(x);
}

static void* ns_val_at(const NodeStore* s, int idx) {
    SkipNode* x = at_index(s, idx);
    return skipnode_val(x);
}

static void ns_set_val(NodeStore* s, int idx, void* v) {
    SkipNode* x = at_index(s, idx);
    skipnode_set_val(x, v);
}

static int ns_lower_bound(const NodeStore* s, int key) {
    assert(s);
    int idx = 0;
    SkipNode* x = skiplist_first(s->sl);
    while (x && skipnode_key(x) < key) {
        x = skipnode_next0(x);
        idx++;
    }
    return idx;
}

static void ns_insert_at(NodeStore* s, int idx, int key, void* val) {
    (void)idx;
    assert(s);
    assert(skiplist_size(s->sl) < s->cap);
    skiplist_insert(s->sl, key, val);
}

static void ns_erase_at(NodeStore* s, int idx) {
    assert(s);
    SkipNode* x = at_index(s, idx);
    skiplist_erase(s->sl, skipnode_key(x));
}

static int ns_split(NodeStore* left, NodeStore* right) {
    assert(left && right);
    assert(skiplist_size(right->sl) == 0);

    int n = skiplist_size(left->sl);
    int mid = n / 2;
    if (mid == 0) mid = 1;

    // move from mid..end into right
    int idx = 0;
    SkipNode* x = skiplist_first(left->sl);
    while (x && idx < mid) { x = skipnode_next0(x); idx++; }

    assert(x);
    int sep = skipnode_key(x);

    // collect keys from mid..end first (because we'll delete from left)
    int move = n - mid;
    int* keys = (int*)malloc(sizeof(int) * (size_t)move);
    void** vals = (void**)malloc(sizeof(void*) * (size_t)move);
    assert(keys && vals);

    SkipNode* cur = x;
    for (int i = 0; i < move; ++i) {
        keys[i] = skipnode_key(cur);
        vals[i] = skipnode_val(cur);
        cur = skipnode_next0(cur);
    }

    for (int i = 0; i < move; ++i) {
        skiplist_insert(right->sl, keys[i], vals[i]);
        skiplist_erase(left->sl, keys[i]);
    }

    free(keys);
    free(vals);
    return sep;
}

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
