// nodestore_array.c
#include "nodestore.h"
#include <assert.h>
#include <stdlib.h>

struct NodeStore {
    int cap;
    int n;
    int* keys;
    void** vals;
};

static NodeStore* ns_create(int capacity) {
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
    return s;
}

static void ns_destroy(NodeStore* s) {
    if (!s) return;
    free(s->keys);
    free(s->vals);
    free(s);
}

static int ns_size(const NodeStore* s) { return s ? s->n : 0; }
static int ns_capacity(const NodeStore* s) { return s ? s->cap : 0; }
static void ns_clear(NodeStore* s) { if (s) s->n = 0; }

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

static int ns_lower_bound(const NodeStore* s, int key) {
    assert(s);
    int l = 0, r = s->n;
    while (l < r) {
        int m = l + (r - l) / 2;
        if (s->keys[m] < key) l = m + 1;
        else r = m;
    }
    return l;
}

static void ns_insert_at(NodeStore* s, int idx, int key, void* val) {
    assert(s);
    assert(idx >= 0 && idx <= s->n);
    assert(s->n < s->cap);
    for (int i = s->n; i > idx; --i) {
        s->keys[i] = s->keys[i - 1];
        s->vals[i] = s->vals[i - 1];
    }
    s->keys[idx] = key;
    s->vals[idx] = val;
    s->n++;
}

static void ns_erase_at(NodeStore* s, int idx) {
    assert(s);
    assert(idx >= 0 && idx < s->n);
    for (int i = idx; i < s->n - 1; ++i) {
        s->keys[i] = s->keys[i + 1];
        s->vals[i] = s->vals[i + 1];
    }
    s->n--;
}

static int ns_split(NodeStore* left, NodeStore* right) {
    assert(left && right);
    assert(right->n == 0);

    int n = left->n;
    int mid = n / 2;
    int move = n - mid;

    assert(move <= right->cap);

    for (int i = 0; i < move; ++i) {
        right->keys[i] = left->keys[mid + i];
        right->vals[i] = left->vals[mid + i];
    }
    right->n = move;
    left->n = mid;
    return right->keys[0];
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

const NodeStoreOps* nodestore_array_ops(void) { return &g_ops; }
