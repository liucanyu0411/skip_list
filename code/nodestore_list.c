// nodestore_list.c
#include "nodestore.h"
#include <assert.h>
#include <stdlib.h>

typedef struct ListNode {
    int key;
    void* val;
    struct ListNode* next;
} ListNode;

struct NodeStore {
    int cap;
    int n;
    ListNode* head;
};

static ListNode* node_new(int key, void* val) {
    ListNode* x = (ListNode*)malloc(sizeof(ListNode));
    if (!x) return NULL;
    x->key = key;
    x->val = val;
    x->next = NULL;
    return x;
}

static void list_free_all(ListNode* x) {
    while (x) {
        ListNode* nx = x->next;
        free(x);
        x = nx;
    }
}

static ListNode* list_at(const NodeStore* s, int idx) {
    assert(s && idx >= 0 && idx < s->n);
    ListNode* cur = s->head;
    for (int i = 0; i < idx; ++i) cur = cur->next;
    return cur;
}

static NodeStore* ns_create(int capacity) {
    NodeStore* s = (NodeStore*)calloc(1, sizeof(NodeStore));
    if (!s) return NULL;
    s->cap = capacity;
    s->n = 0;
    s->head = NULL;
    return s;
}

static void ns_destroy(NodeStore* s) {
    if (!s) return;
    list_free_all(s->head);
    free(s);
}

static int ns_size(const NodeStore* s) { return s ? s->n : 0; }
static int ns_capacity(const NodeStore* s) { return s ? s->cap : 0; }

static void ns_clear(NodeStore* s) {
    if (!s) return;
    list_free_all(s->head);
    s->head = NULL;
    s->n = 0;
}

static int ns_key_at(const NodeStore* s, int idx) {
    ListNode* x = list_at(s, idx);
    return x->key;
}

static void* ns_val_at(const NodeStore* s, int idx) {
    ListNode* x = list_at(s, idx);
    return x->val;
}

static void ns_set_val(NodeStore* s, int idx, void* v) {
    ListNode* x = list_at(s, idx);
    x->val = v;
}

static int ns_lower_bound(const NodeStore* s, int key) {
    assert(s);
    int idx = 0;
    for (ListNode* cur = s->head; cur; cur = cur->next, ++idx) {
        if (cur->key >= key) break;
    }
    return idx;
}

static void ns_insert_at(NodeStore* s, int idx, int key, void* val) {
    assert(s);
    assert(idx >= 0 && idx <= s->n);
    assert(s->n < s->cap);

    ListNode* nn = node_new(key, val);
    assert(nn);

    if (idx == 0) {
        nn->next = s->head;
        s->head = nn;
    } else {
        ListNode* prev = list_at(s, idx - 1);
        nn->next = prev->next;
        prev->next = nn;
    }
    s->n++;
}

static void ns_erase_at(NodeStore* s, int idx) {
    assert(s);
    assert(idx >= 0 && idx < s->n);

    ListNode* del = NULL;
    if (idx == 0) {
        del = s->head;
        s->head = del->next;
    } else {
        ListNode* prev = list_at(s, idx - 1);
        del = prev->next;
        prev->next = del->next;
    }
    free(del);
    s->n--;
}

static int ns_split(NodeStore* left, NodeStore* right) {
    assert(left && right);
    assert(right->n == 0);

    int n = left->n;
    int mid = n / 2;         // left keeps [0..mid-1], right gets [mid..n-1]
    if (mid == 0) mid = 1;

    ListNode* prev = NULL;
    ListNode* cur = left->head;
    for (int i = 0; i < mid; ++i) {
        prev = cur;
        cur = cur->next;
    }

    // cur is the first node for right
    right->head = cur;
    right->n = n - mid;

    // cut left
    if (prev) prev->next = NULL;
    left->n = mid;

    // separator key = first key in right
    assert(right->head);
    return right->head->key;
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

const NodeStoreOps* nodestore_list_ops(void) { return &g_ops; }
