// skiplist.c
#include "skiplist.h"
#include <stdlib.h>
#include <string.h>

struct SkipNode {
    int key;
    void* val;
    int level;
    struct SkipNode** forward;
};

struct SkipList {
    int max_level;
    double p;
    int level;
    int n;
    struct SkipNode* header;
    unsigned rng;
};

static unsigned xorshift32(unsigned* s) {
    unsigned x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

static double rnd01(unsigned* s) {
    return (xorshift32(s) & 0xFFFFFFu) / 16777216.0;
}

static int random_level(SkipList* sl) {
    int lvl = 1;
    while (lvl < sl->max_level && rnd01(&sl->rng) < sl->p) lvl++;
    return lvl;
}

static SkipNode* node_create(int key, void* val, int level) {
    SkipNode* x = (SkipNode*)calloc(1, sizeof(SkipNode));
    if (!x) return NULL;
    x->key = key;
    x->val = val;
    x->level = level;
    x->forward = (SkipNode**)calloc((size_t)level, sizeof(SkipNode*));
    if (!x->forward) { free(x); return NULL; }
    return x;
}

SkipList* skiplist_create(int max_level, double p, unsigned seed) {
    if (max_level < 1) max_level = 1;
    if (p <= 0.0 || p >= 1.0) p = 0.5;

    SkipList* sl = (SkipList*)calloc(1, sizeof(SkipList));
    if (!sl) return NULL;

    sl->max_level = max_level;
    sl->p = p;
    sl->level = 1;
    sl->n = 0;
    sl->rng = (seed ? seed : 2463534242u);

    sl->header = node_create(0, NULL, max_level);
    if (!sl->header) { free(sl); return NULL; }
    return sl;
}

void skiplist_destroy(SkipList* sl) {
    if (!sl) return;
    SkipNode* x = sl->header->forward[0];
    while (x) {
        SkipNode* nx = x->forward[0];
        free(x->forward);
        free(x);
        x = nx;
    }
    free(sl->header->forward);
    free(sl->header);
    free(sl);
}

int skiplist_size(const SkipList* sl) { return sl ? sl->n : 0; }

SkipNode* skiplist_search(const SkipList* sl, int key) {
    if (!sl) return NULL;
    SkipNode* x = sl->header;
    for (int i = sl->level - 1; i >= 0; --i) {
        while (x->forward[i] && x->forward[i]->key < key) {
            x = x->forward[i];
        }
    }
    x = x->forward[0];
    if (x && x->key == key) return x;
    return NULL;
}

SkipNode* skiplist_first_ge(const SkipList* sl, int key) {
    if (!sl) return NULL;
    SkipNode* x = sl->header;
    for (int i = sl->level - 1; i >= 0; --i) {
        while (x->forward[i] && x->forward[i]->key < key) {
            x = x->forward[i];
        }
    }
    return x->forward[0];
}

int skiplist_insert(SkipList* sl, int key, void* val) {
    if (!sl) return 0;
    SkipNode** update = (SkipNode**)alloca((size_t)sl->max_level * sizeof(SkipNode*));
    SkipNode* x = sl->header;

    for (int i = sl->level - 1; i >= 0; --i) {
        while (x->forward[i] && x->forward[i]->key < key) x = x->forward[i];
        update[i] = x;
    }
    x = x->forward[0];

    if (x && x->key == key) {
        x->val = val;
        return 0;
    }

    int lvl = random_level(sl);
    if (lvl > sl->level) {
        for (int i = sl->level; i < lvl; ++i) update[i] = sl->header;
        sl->level = lvl;
    }

    SkipNode* nn = node_create(key, val, lvl);
    if (!nn) return 0;

    for (int i = 0; i < lvl; ++i) {
        nn->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = nn;
    }
    sl->n++;
    return 1;
}

int skiplist_erase(SkipList* sl, int key) {
    if (!sl) return 0;
    SkipNode** update = (SkipNode**)alloca((size_t)sl->max_level * sizeof(SkipNode*));
    SkipNode* x = sl->header;

    for (int i = sl->level - 1; i >= 0; --i) {
        while (x->forward[i] && x->forward[i]->key < key) x = x->forward[i];
        update[i] = x;
    }
    x = x->forward[0];
    if (!x || x->key != key) return 0;

    for (int i = 0; i < sl->level; ++i) {
        if (update[i]->forward[i] == x) update[i]->forward[i] = x->forward[i];
    }

    free(x->forward);
    free(x);
    sl->n--;

    while (sl->level > 1 && sl->header->forward[sl->level - 1] == NULL) sl->level--;
    return 1;
}

SkipNode* skiplist_first(const SkipList* sl) {
    if (!sl) return NULL;
    return sl->header->forward[0];
}

SkipNode* skipnode_next0(const SkipNode* x) {
    if (!x) return NULL;
    return x->forward[0];
}

int skipnode_key(const SkipNode* x) { return x ? x->key : 0; }
void* skipnode_val(const SkipNode* x) { return x ? x->val : NULL; }
void skipnode_set_val(SkipNode* x, void* v) { if (x) x->val = v; }
