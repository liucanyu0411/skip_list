// bptree.c  (B+ Tree, Scheme A: parent key[i] = min(child[i+1]) copy-key semantics)
#include "bptree.h"
#include <assert.h>
#include <stdlib.h>

typedef struct BPTreeNode {
    int is_leaf;
    int max_keys;
    struct BPTreeNode* parent;
    struct BPTreeNode* next;    // leaf chain
    struct BPTreeNode* child0;  // internal: leftmost child
    NodeStore* store;           // internal: key[i], val[i]=child[i+1]; leaf: key[i], val unused
} BPTreeNode;

struct BPTree {
    int order_M;                // M (max children)
    int max_keys;               // M-1
    const NodeStoreOps* ops;
    BPTreeNode* root;
};

// -------------------- Node helpers --------------------

static BPTreeNode* node_create(BPTree* t, int is_leaf) {
    BPTreeNode* x = (BPTreeNode*)calloc(1, sizeof(BPTreeNode));
    assert(x);
    x->is_leaf = is_leaf;
    x->max_keys = t->max_keys;
    x->parent = 0;
    x->next = 0;
    x->child0 = 0;
    x->store = t->ops->create(t->max_keys + 1); // allow overflow then split
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

static int min_leaf_keys(const BPTree* t) {
    // ceil((M-1)/2) = ceil(max_keys/2)
    return (t->max_keys + 1) / 2;
}

static int min_internal_keys(const BPTree* t) {
    // ceil(M/2) - 1
    int min_children = (t->order_M + 1) / 2; // ceil(M/2)
    return min_children - 1;
}

static int store_set_key(const BPTree* t, NodeStore* s, int idx, int new_key) {
    int n = t->ops->size(s);
    if (idx < 0 || idx >= n) return 0;
    void* v = t->ops->val_at(s, idx);
    t->ops->erase_at(s, idx);
    t->ops->insert_at(s, idx, new_key, v);
    return 1;
}

static BPTreeNode* parent_child_at(const BPTree* t, const BPTreeNode* parent, int child_index) {
    // child_index in [0..nchildren-1], where nchildren = nkeys+1
    if (child_index == 0) return parent->child0;
    return (BPTreeNode*)t->ops->val_at(parent->store, child_index - 1);
}

static int parent_child_index(const BPTree* t, const BPTreeNode* parent, const BPTreeNode* child) {
    // return j such that parent_child_at(parent,j)==child, j in [0..n]
    if (parent->child0 == child) return 0;
    int n = t->ops->size(parent->store);
    for (int i = 0; i < n; ++i) {
        if ((BPTreeNode*)t->ops->val_at(parent->store, i) == child) return i + 1;
    }
    return -1;
}

// minimal key in subtree rooted at x (descend child0 until leaf)
static int subtree_first_key(const BPTree* t, const BPTreeNode* x) {
    const BPTreeNode* cur = x;
    while (cur && !cur->is_leaf) cur = cur->child0;
    assert(cur);
    assert(t->ops->size(cur->store) > 0);
    return t->ops->key_at(cur->store, 0);
}

// parent separator update: if x is parent's child j>0, then parent.key[j-1]=min(x)
static void update_parent_sep_if_needed(BPTree* t, BPTreeNode* x) {
    if (!x || !x->parent) return;
    int nmin;
    // x may be temporarily empty during delete; skip if no keys
    if (x->is_leaf) {
        if (t->ops->size(x->store) <= 0) return;
        nmin = t->ops->key_at(x->store, 0);
    } else {
        // internal: need subtree min (may assert if subtree empty)
        // if its leftmost leaf empty, tree is already broken; assume rebalance will prevent that
        nmin = subtree_first_key(t, x);
    }

    BPTreeNode* p = x->parent;
    int idx = parent_child_index(t, p, x);
    if (idx > 0) {
        store_set_key(t, p->store, idx - 1, nmin);
    }
}

// -------------------- Search helpers --------------------

// B+ tree descent uses upper_bound semantics (equal goes right)
static BPTreeNode* find_leaf(const BPTree* t, int key) {
    BPTreeNode* x = t->root;
    while (x && !x->is_leaf) {
        int n = t->ops->size(x->store);
        int idx = t->ops->lower_bound(x->store, key); // first >= key
        if (idx < n && t->ops->key_at(x->store, idx) == key) idx++; // make it upper_bound
        if (idx == 0) x = x->child0;
        else x = (BPTreeNode*)t->ops->val_at(x->store, idx - 1);
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

// -------------------- Insert: split / insert_into_parent --------------------

static void insert_into_parent(BPTree* t, BPTreeNode* left, int sep_key, BPTreeNode* right);

static void split_leaf(BPTree* t, BPTreeNode* leaf) {
    int total = t->ops->size(leaf->store);
    assert(total == t->max_keys + 1);

    // typical B+ leaf split: left gets ceil(total/2)
    int left_sz  = (total + 1) / 2;
    int right_sz = total - left_sz;

    int* keys = (int*)malloc(sizeof(int) * (size_t)total);
    assert(keys);

    for (int i = 0; i < total; ++i) keys[i] = t->ops->key_at(leaf->store, i);

    BPTreeNode* right = node_create(t, 1);
    right->parent = leaf->parent;

    t->ops->clear(leaf->store);
    t->ops->clear(right->store);

    for (int i = 0; i < left_sz; ++i)  t->ops->insert_at(leaf->store, i, keys[i], 0);
    for (int i = 0; i < right_sz; ++i) t->ops->insert_at(right->store, i, keys[left_sz + i], 0);

    right->next = leaf->next;
    leaf->next = right;

    // separator is min(right)
    int sep = t->ops->key_at(right->store, 0);
    free(keys);

    insert_into_parent(t, leaf, sep, right);
}

// split internal with copy-key semantics:
// parent key to insert = min(right) = subtree_first_key(right)
static void split_internal(BPTree* t, BPTreeNode* x) {
    int k = t->ops->size(x->store);
    assert(k == t->max_keys + 1);

    // materialize children and keys
    BPTreeNode** ch = (BPTreeNode**)malloc(sizeof(BPTreeNode*) * (size_t)(k + 1));
    int* keys = (int*)malloc(sizeof(int) * (size_t)k);
    assert(ch && keys);

    ch[0] = x->child0;
    for (int i = 0; i < k; ++i) {
        keys[i] = t->ops->key_at(x->store, i);
        ch[i + 1] = (BPTreeNode*)t->ops->val_at(x->store, i);
    }

    int nchildren = k + 1;
    int left_children = (nchildren + 1) / 2; // ceil(nchildren/2)
    int left_keys = left_children - 1;

    // right children start at index left_children
    BPTreeNode* right = node_create(t, 0);
    right->parent = x->parent;

    // rebuild left (x)
    t->ops->clear(x->store);
    x->child0 = ch[0];
    if (x->child0) x->child0->parent = x;

    for (int i = 0; i < left_keys; ++i) {
        BPTreeNode* c = ch[i + 1];
        t->ops->insert_at(x->store, i, keys[i], c);
        if (c) c->parent = x;
    }

    // rebuild right
    right->child0 = ch[left_children];
    if (right->child0) right->child0->parent = right;

    int rkeys = k - left_children; // keys[left_children .. k-1]
    for (int i = 0; i < rkeys; ++i) {
        int kk = keys[left_children + i];
        BPTreeNode* c = ch[left_children + 1 + i];
        t->ops->insert_at(right->store, i, kk, c);
        if (c) c->parent = right;
    }

    int sep_key = subtree_first_key(t, right);

    free(ch);
    free(keys);

    insert_into_parent(t, x, sep_key, right);
}

// Insert separator into parent at correct position determined by left's child index
static void insert_into_parent(BPTree* t, BPTreeNode* left, int sep_key, BPTreeNode* right) {
    BPTreeNode* parent = left->parent;

    if (!parent) {
        BPTreeNode* root = node_create(t, 0);
        root->child0 = left;
        left->parent = root;
        t->ops->insert_at(root->store, 0, sep_key, right); // key[0] = min(right), val[0]=right
        right->parent = root;
        t->root = root;
        return;
    }

    int j = parent_child_index(t, parent, left);
    assert(j >= 0);

    // parent.key[j] corresponds to child[j+1] (new right)
    t->ops->insert_at(parent->store, j, sep_key, right);
    right->parent = parent;

    if (node_overflow(t, parent)) split_internal(t, parent);
}

// -------------------- Delete: borrow / merge / rebalance --------------------

static void fix_root_after_delete(BPTree* t) {
    // if root internal has no keys, shrink height
    while (t->root && !t->root->is_leaf && t->ops->size(t->root->store) == 0) {
        BPTreeNode* old = t->root;
        BPTreeNode* nr = old->child0;
        if (nr) nr->parent = 0;
        t->root = nr;
        node_destroy(t, old);
    }
}

// leaf borrow from left: move left last key -> leaf front; update parent sep for leaf
static int borrow_from_left_leaf(BPTree* t, BPTreeNode* leaf, BPTreeNode* left, int leaf_idx_in_parent) {
    int ln = t->ops->size(left->store);
    if (ln <= min_leaf_keys(t)) return 0;

    int k = t->ops->key_at(left->store, ln - 1);
    t->ops->erase_at(left->store, ln - 1);

    t->ops->insert_at(leaf->store, 0, k, 0);

    // parent key[leaf_idx-1] = min(leaf)
    store_set_key(t, leaf->parent->store, leaf_idx_in_parent - 1, t->ops->key_at(leaf->store, 0));
    return 1;
}

// leaf borrow from right: move right first key -> leaf end; update parent sep for right
static int borrow_from_right_leaf(BPTree* t, BPTreeNode* leaf, BPTreeNode* right, int leaf_idx_in_parent) {
    int rn = t->ops->size(right->store);
    if (rn <= min_leaf_keys(t)) return 0;

    int k = t->ops->key_at(right->store, 0);
    t->ops->erase_at(right->store, 0);

    int ln = t->ops->size(leaf->store);
    t->ops->insert_at(leaf->store, ln, k, 0);

    // parent key[leaf_idx] = min(right) (if right not empty)
    if (t->ops->size(right->store) > 0) {
        store_set_key(t, leaf->parent->store, leaf_idx_in_parent, t->ops->key_at(right->store, 0));
    }
    return 1;
}

// merge leaf into left (left is left sibling), remove parent entry (idx-1)
static void merge_leaf_into_left(BPTree* t, BPTreeNode* left, BPTreeNode* leaf, int leaf_idx_in_parent) {
    int ln = t->ops->size(left->store);
    int n  = t->ops->size(leaf->store);
    for (int i = 0; i < n; ++i) {
        int k = t->ops->key_at(leaf->store, i);
        t->ops->insert_at(left->store, ln + i, k, 0);
    }
    left->next = leaf->next;

    assert(leaf_idx_in_parent > 0);
    t->ops->erase_at(left->parent->store, leaf_idx_in_parent - 1); // remove pointer to leaf

    node_destroy(t, leaf);
}

// merge right into leaf (leaf is left), remove parent entry (idx)
static void merge_right_leaf_into_leaf(BPTree* t, BPTreeNode* leaf, BPTreeNode* right, int leaf_idx_in_parent) {
    int ln = t->ops->size(leaf->store);
    int rn = t->ops->size(right->store);
    for (int i = 0; i < rn; ++i) {
        int k = t->ops->key_at(right->store, i);
        t->ops->insert_at(leaf->store, ln + i, k, 0);
    }
    leaf->next = right->next;

    t->ops->erase_at(leaf->parent->store, leaf_idx_in_parent); // remove pointer to right

    node_destroy(t, right);
}

// internal borrow from left (copy-key semantics):
// move left's last child to x's front; parent sep becomes min(x) after move.
static int borrow_from_left_internal(BPTree* t, BPTreeNode* x, BPTreeNode* left, int x_idx_in_parent) {
    int lkeys = t->ops->size(left->store);
    if (lkeys <= min_internal_keys(t)) return 0;

    // parent sep for x is key[x_idx-1] = min(x)
    int parent_sep = t->ops->key_at(x->parent->store, x_idx_in_parent - 1);

    // take left's last child (val[last]) and erase that entry
    BPTreeNode* borrow_child = (BPTreeNode*)t->ops->val_at(left->store, lkeys - 1);
    int borrow_child_min = subtree_first_key(t, borrow_child);
    t->ops->erase_at(left->store, lkeys - 1);

    // x: insert new key at front = old parent_sep, val = old child0
    BPTreeNode* old_c0 = x->child0;
    x->child0 = borrow_child;
    if (borrow_child) borrow_child->parent = x;

    t->ops->insert_at(x->store, 0, parent_sep, old_c0);
    if (old_c0) old_c0->parent = x;

    // update parent sep for x to new min(x) = min(borrow_child)
    store_set_key(t, x->parent->store, x_idx_in_parent - 1, borrow_child_min);
    return 1;
}

// internal borrow from right (copy-key semantics):
// move right's child0 to x's end; x appends parent's sep; parent sep becomes new min(right)
static int borrow_from_right_internal(BPTree* t, BPTreeNode* x, BPTreeNode* right, int x_idx_in_parent) {
    int rkeys = t->ops->size(right->store);
    if (rkeys <= min_internal_keys(t)) return 0;

    // parent sep for right is key[x_idx] = min(right)
    int parent_sep = t->ops->key_at(x->parent->store, x_idx_in_parent);

    // borrow right.child0
    BPTreeNode* borrow_child = right->child0;

    // right: shift child0 to old child1 (val[0]), erase entry 0
    BPTreeNode* new_c0 = (BPTreeNode*)t->ops->val_at(right->store, 0); // old child1
    int new_right_min = t->ops->key_at(right->store, 0);              // min(new_c0)
    t->ops->erase_at(right->store, 0);
    right->child0 = new_c0;
    if (new_c0) new_c0->parent = right;

    // x append: key = parent_sep (min(borrow_child)), val = borrow_child
    int xn = t->ops->size(x->store);
    t->ops->insert_at(x->store, xn, parent_sep, borrow_child);
    if (borrow_child) borrow_child->parent = x;

    // parent sep becomes min(right) after shift
    store_set_key(t, x->parent->store, x_idx_in_parent, new_right_min);
    return 1;
}

// merge internal x into left (left is left sibling), using parent sep key[x_idx-1]
static void merge_internal_into_left(BPTree* t, BPTreeNode* left, BPTreeNode* x, int x_idx_in_parent) {
    int sep = t->ops->key_at(left->parent->store, x_idx_in_parent - 1);

    int ln = t->ops->size(left->store);

    // append sep with val = x->child0 (because sep == min(x))
    t->ops->insert_at(left->store, ln, sep, x->child0);
    if (x->child0) x->child0->parent = left;

    // append x's (key,val) entries as-is
    int xn = t->ops->size(x->store);
    for (int i = 0; i < xn; ++i) {
        int k = t->ops->key_at(x->store, i);
        BPTreeNode* c = (BPTreeNode*)t->ops->val_at(x->store, i);
        t->ops->insert_at(left->store, ln + 1 + i, k, c);
        if (c) c->parent = left;
    }

    // remove parent entry that pointed to x
    t->ops->erase_at(left->parent->store, x_idx_in_parent - 1);

    node_destroy(t, x);
}

// merge right into x (x is left sibling), using parent sep key[x_idx]
static void merge_right_internal_into_x(BPTree* t, BPTreeNode* x, BPTreeNode* right, int x_idx_in_parent) {
    int sep = t->ops->key_at(x->parent->store, x_idx_in_parent);

    int xn = t->ops->size(x->store);

    // append sep with val = right->child0 (sep == min(right))
    t->ops->insert_at(x->store, xn, sep, right->child0);
    if (right->child0) right->child0->parent = x;

    int rn = t->ops->size(right->store);
    for (int i = 0; i < rn; ++i) {
        int k = t->ops->key_at(right->store, i);
        BPTreeNode* c = (BPTreeNode*)t->ops->val_at(right->store, i);
        t->ops->insert_at(x->store, xn + 1 + i, k, c);
        if (c) c->parent = x;
    }

    t->ops->erase_at(x->parent->store, x_idx_in_parent);

    node_destroy(t, right);
}

static void rebalance_after_delete(BPTree* t, BPTreeNode* x) {
    if (!x) return;

    if (x == t->root) {
        fix_root_after_delete(t);
        return;
    }

    BPTreeNode* parent = x->parent;
    int x_idx = parent_child_index(t, parent, x);
    assert(x_idx >= 0);

    int nkeys = t->ops->size(x->store);

    if (x->is_leaf) {
        // not underflow
        if (nkeys >= min_leaf_keys(t)) {
            update_parent_sep_if_needed(t, x); // leaf min may change after deletion
            return;
        }

        // underflow
        BPTreeNode* left  = (x_idx > 0) ? parent_child_at(t, parent, x_idx - 1) : NULL;
        BPTreeNode* right = (x_idx < t->ops->size(parent->store)) ? parent_child_at(t, parent, x_idx + 1) : NULL;

        if (left  && borrow_from_left_leaf(t, x, left, x_idx)) return;
        if (right && borrow_from_right_leaf(t, x, right, x_idx)) return;

        // merge
        if (left) {
            merge_leaf_into_left(t, left, x, x_idx);
            rebalance_after_delete(t, parent);
        } else if (right) {
            merge_right_leaf_into_leaf(t, x, right, x_idx);
            rebalance_after_delete(t, parent);
        }
        return;
    }

    // internal node
    if (nkeys >= min_internal_keys(t)) {
        // internal min can change if its child0 changed in previous ops; keep parent consistent
        update_parent_sep_if_needed(t, x);
        return;
    }

    BPTreeNode* left  = (x_idx > 0) ? parent_child_at(t, parent, x_idx - 1) : NULL;
    BPTreeNode* right = (x_idx < t->ops->size(parent->store)) ? parent_child_at(t, parent, x_idx + 1) : NULL;

    if (left  && borrow_from_left_internal(t, x, left, x_idx)) return;
    if (right && borrow_from_right_internal(t, x, right, x_idx)) return;

    // merge
    if (left) {
        merge_internal_into_left(t, left, x, x_idx);
        rebalance_after_delete(t, parent);
    } else if (right) {
        merge_right_internal_into_x(t, x, right, x_idx);
        rebalance_after_delete(t, parent);
    }
}

// -------------------- Public API --------------------

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
    if (leaf_find(t, leaf, key, &idx)) return; // no duplicates

    t->ops->insert_at(leaf->store, idx, key, 0);

    if (node_overflow(t, leaf)) split_leaf(t, leaf);

    // after insert, leaf min might change (if inserted at 0), update parent sep
    update_parent_sep_if_needed(t, leaf);
}

void bptree_delete(BPTree* t, int key) {
    if (!t || !t->root) return;

    BPTreeNode* leaf = find_leaf(t, key);
    if (!leaf) return;

    int idx = 0;
    if (!leaf_find(t, leaf, key, &idx)) return;

    // delete from leaf
    t->ops->erase_at(leaf->store, idx);

    // rebalance upward
    rebalance_after_delete(t, leaf);

    // ensure root shrink
    fix_root_after_delete(t);
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