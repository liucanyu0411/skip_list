// bptree.c
#include "bptree.h"
#include <assert.h>
#include <stdlib.h>

typedef struct BPTreeNode {
    int is_leaf;
    int max_keys;
    struct BPTreeNode* parent;
    struct BPTreeNode* next;    // leaf chain
    struct BPTreeNode* child0;  // internal: leftmost child
    NodeStore* store;           // internal: key[i] + val[i]=child[i+1]; leaf: key[i], val unused
} BPTreeNode;

struct BPTree {
    int order_M;
    int max_keys;
    const NodeStoreOps* ops;
    BPTreeNode* root;
};

static BPTreeNode* node_create(BPTree* t, int is_leaf) {
    BPTreeNode* x = (BPTreeNode*)calloc(1, sizeof(BPTreeNode));
    assert(x);
    x->is_leaf = is_leaf;
    x->max_keys = t->max_keys;
    x->parent = 0;
    x->next = 0;
    x->child0 = 0;
    x->store = t->ops->create(t->max_keys + 1);
    assert(x->store);
    return x;
}

static void node_destroy(BPTree* t, BPTreeNode* x) {
    if (!x) return;
    t->ops->destroy(x->store);
    free(x);
}

static int node_keys(const BPTree* t, const BPTreeNode* x) {
    (void)t;
    return t->ops->size(x->store);
}

static int node_overflow(const BPTree* t, const BPTreeNode* x) {
    return node_keys(t, x) > x->max_keys;
}

static BPTreeNode* find_leaf(const BPTree* t, int key) {
    BPTreeNode* x = t->root;
    while (x && !x->is_leaf) {
        int n = t->ops->size(x->store);
        int idx = t->ops->lower_bound(x->store, key);
        if (idx == 0) x = x->child0;
        else x = (BPTreeNode*)t->ops->val_at(x->store, idx - 1);
        (void)n;
    }
    return x;
}

static int leaf_find(const BPTree* t, const BPTreeNode* leaf, int key, int* out_idx) {
    int n = t->ops->size(leaf->store);
    int idx = t->ops->lower_bound(leaf->store, key);
    if (out_idx) *out_idx = idx;
    if (idx < n && t->ops->key_at(leaf->store, idx) == key) return 1;
    return 0;
}

static void insert_into_parent(BPTree* t, BPTreeNode* left, int sep_key, BPTreeNode* right);

static void split_leaf(BPTree* t, BPTreeNode* leaf) {
    int total = t->ops->size(leaf->store);
    assert(total == t->max_keys + 1);

    int left_sz = (total + 1) / 2;
    int right_sz = total - left_sz;

    int* keys = (int*)malloc(sizeof(int) * (size_t)total);
    assert(keys);

    for (int i = 0; i < total; ++i) keys[i] = t->ops->key_at(leaf->store, i);

    BPTreeNode* right = node_create(t, 1);
    right->parent = leaf->parent;

    t->ops->clear(leaf->store);
    t->ops->clear(right->store);

    for (int i = 0; i < left_sz; ++i) t->ops->insert_at(leaf->store, i, keys[i], 0);
    for (int i = 0; i < right_sz; ++i) t->ops->insert_at(right->store, i, keys[left_sz + i], 0);

    right->next = leaf->next;
    leaf->next = right;

    int sep = t->ops->key_at(right->store, 0);
    free(keys);

    insert_into_parent(t, leaf, sep, right);
}

static void split_internal(BPTree* t, BPTreeNode* x) {
    int k = t->ops->size(x->store);
    assert(k == t->max_keys + 1);

    int* keys = (int*)malloc(sizeof(int) * (size_t)k);
    BPTreeNode** ch = (BPTreeNode**)malloc(sizeof(BPTreeNode*) * (size_t)(k + 1));
    assert(keys && ch);

    ch[0] = x->child0;
    for (int i = 0; i < k; ++i) {
        keys[i] = t->ops->key_at(x->store, i);
        ch[i + 1] = (BPTreeNode*)t->ops->val_at(x->store, i);
    }

    int mid = k / 2;
    int up_key = keys[mid];

    BPTreeNode* right = node_create(t, 0);
    right->parent = x->parent;

    t->ops->clear(x->store);
    x->child0 = ch[0];
    if (x->child0) x->child0->parent = x;

    for (int i = 0; i < mid; ++i) {
        t->ops->insert_at(x->store, i, keys[i], ch[i + 1]);
        if (ch[i + 1]) ch[i + 1]->parent = x;
    }

    int rkeys = k - mid - 1;
    right->child0 = ch[mid + 1];
    if (right->child0) right->child0->parent = right;

    for (int i = 0; i < rkeys; ++i) {
        int kk = keys[mid + 1 + i];
        BPTreeNode* cr = ch[mid + 2 + i];
        t->ops->insert_at(right->store, i, kk, cr);
        if (cr) cr->parent = right;
    }

    free(keys);
    free(ch);

    insert_into_parent(t, x, up_key, right);
}

static void insert_into_parent(BPTree* t, BPTreeNode* left, int sep_key, BPTreeNode* right) {
    BPTreeNode* parent = left->parent;

    if (!parent) {
        BPTreeNode* root = node_create(t, 0);
        root->child0 = left;
        left->parent = root;
        t->ops->insert_at(root->store, 0, sep_key, right);
        right->parent = root;
        t->root = root;
        return;
    }

    int n = t->ops->size(parent->store);
    int pos = -1;

    if (parent->child0 == left) {
        pos = 0;
    } else {
        for (int i = 0; i < n; ++i) {
            if ((BPTreeNode*)t->ops->val_at(parent->store, i) == left) {
                pos = i + 1;
                break;
            }
        }
    }

    assert(pos >= 0 && pos <= n);
    t->ops->insert_at(parent->store, pos, sep_key, right);
    right->parent = parent;

    if (node_overflow(t, parent)) split_internal(t, parent);
}

BPTree* bptree_create(int order_M, const NodeStoreOps* ops) {
    if (order_M < 3) order_M = 3;
    if (!ops) ops = nodestore_get_ops(NODESTORE_ARRAY);

    BPTree* t = (BPTree*)calloc(1, sizeof(BPTree));
    assert(t);

    t->order_M = order_M;
    t->max_keys = order_M - 1;
    t->ops = ops;
    t->root = node_create(t, 1);
    return t;
}

static void destroy_subtree(BPTree* t, BPTreeNode* x) {
    if (!x) return;
    if (!x->is_leaf) {
        int n = t->ops->size(x->store);
        destroy_subtree(t, x->child0);
        for (int i = 0; i < n; ++i) {
            BPTreeNode* c = (BPTreeNode*)t->ops->val_at(x->store, i);
            destroy_subtree(t, c);
        }
    }
    node_destroy(t, x);
}

void bptree_destroy(BPTree* t) {
    if (!t) return;
    destroy_subtree(t, t->root);
    free(t);
}

int bptree_search(const BPTree* t, int key) {
    if (!t || !t->root) return 0;
    BPTreeNode* leaf = find_leaf(t, key);
    if (!leaf) return 0;
    return leaf_find(t, leaf, key, 0);
}

void bptree_insert(BPTree* t, int key) {
    if (!t || !t->root) return;
    BPTreeNode* leaf = find_leaf(t, key);
    assert(leaf);

    int idx = 0;
    if (leaf_find(t, leaf, key, &idx)) return;

    t->ops->insert_at(leaf->store, idx, key, 0);

    if (node_overflow(t, leaf)) split_leaf(t, leaf);
}

void bptree_delete(BPTree* t, int key) {
    if (!t || !t->root) return;
    BPTreeNode* leaf = find_leaf(t, key);
    if (!leaf) return;

    int idx = 0;
    if (!leaf_find(t, leaf, key, &idx)) return;
    t->ops->erase_at(leaf->store, idx);

    while (t->root && !t->root->is_leaf && t->ops->size(t->root->store) == 0) {
        BPTreeNode* old = t->root;
        BPTreeNode* nr = old->child0;
        if (nr) nr->parent = 0;
        t->root = nr;
        node_destroy(t, old);
    }
}

int bptree_height(const BPTree* t) {
    if (!t || !t->root) return 0;
    int h = 1;
    BPTreeNode* x = t->root;
    while (x && !x->is_leaf) {
        h++;
        x = x->child0;
    }
    return h;
}
