#include "skiplist.h"

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

static SkipListNode* create_node(int level, int key) {
    SkipListNode* n = (SkipListNode*)malloc(sizeof(SkipListNode) + (size_t)level * sizeof(SkipListNode*));
    if (!n) return NULL;

    n->key = key;
    n->level = level;
    for (int i = 0; i < level; i++) n->forward[i] = NULL;
    return n;
}

static int random_level(const SkipList* sl) {
    int lvl = 1;
    while (((double)rand() / (double)RAND_MAX) < sl->p && lvl < sl->max_level) {
        lvl++;
    }
    return lvl;
}

SkipList* skiplist_create(int max_level, double p) {
    if (max_level <= 0 || max_level > SKIPLIST_MAX_LEVEL) return NULL;
    if (p <= 0.0 || p >= 1.0) return NULL;

    SkipList* sl = (SkipList*)malloc(sizeof(SkipList));
    if (!sl) return NULL;

    sl->max_level = max_level;
    sl->p = p;
    sl->level = 1;
    sl->size = 0;

    // header 是哨兵，key = INT_MIN；高度设为 max_level，保证每层都有起点
    sl->header = create_node(max_level, INT_MIN);
    if (!sl->header) {
        free(sl);
        return NULL;
    }
    return sl;
}

void skiplist_destroy(SkipList* sl) {
    if (!sl) return;

    // 从底层 L0 串起来释放所有真实节点
    SkipListNode* x = sl->header->forward[0];
    while (x) {
        SkipListNode* next = x->forward[0];
        free(x);
        x = next;
    }
    free(sl->header);
    free(sl);
}

bool skiplist_search(const SkipList* sl, int key) {
    if (!sl) return false;

    const SkipListNode* x = sl->header;
    for (int i = sl->level - 1; i >= 0; i--) {
        while (x->forward[i] && x->forward[i]->key < key) {
            x = x->forward[i];
        }
    }
    x = x->forward[0];
    return (x && x->key == key);
}

bool skiplist_insert(SkipList* sl, int key) {
    if (!sl) return false;

    SkipListNode* update[SKIPLIST_MAX_LEVEL];
    SkipListNode* x = sl->header;

    // 1) 找每层前驱 update[i]
    for (int i = sl->level - 1; i >= 0; i--) {
        while (x->forward[i] && x->forward[i]->key < key) {
            x = x->forward[i];
        }
        update[i] = x;
    }

    // 2) 检查重复
    x = x->forward[0];
    if (x && x->key == key) return false;

    // 3) 随机高度
    int lvl = random_level(sl);

    // 4) 如果新节点更高，补齐 update，并提升 sl->level
    if (lvl > sl->level) {
        for (int i = sl->level; i < lvl; i++) update[i] = sl->header;
        sl->level = lvl;
    }

    // 5) 创建节点并在 0..lvl-1 层插入
    SkipListNode* n = create_node(lvl, key);
    if (!n) return false;

    for (int i = 0; i < lvl; i++) {
        n->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = n;
    }

    sl->size++;
    return true;
}

bool skiplist_erase(SkipList* sl, int key) {
    if (!sl) return false;

    SkipListNode* update[SKIPLIST_MAX_LEVEL];
    SkipListNode* x = sl->header;

    // 1) 找每层前驱 update[i]
    for (int i = sl->level - 1; i >= 0; i--) {
        while (x->forward[i] && x->forward[i]->key < key) {
            x = x->forward[i];
        }
        update[i] = x;
    }

    // 2) 目标节点应在 L0 的 update[0]->forward[0]
    x = x->forward[0];
    if (!x || x->key != key) return false;

    // 3) 各层断开指针
    for (int i = 0; i < sl->level; i++) {
        if (update[i]->forward[i] == x) {
            update[i]->forward[i] = x->forward[i];
        }
    }

    free(x);
    sl->size--;

    // 4) 如果最高层空了，降低 sl->level
    while (sl->level > 1 && sl->header->forward[sl->level - 1] == NULL) {
        sl->level--;
    }

    return true;
}

void skiplist_print(const SkipList* sl) {
    if (!sl) return;
    printf("SkipList(size=%d, levels=%d)\n", sl->size, sl->level);
    for (int i = sl->level - 1; i >= 0; i--) {
        printf("L%d: ", i);
        const SkipListNode* x = sl->header->forward[i];
        while (x) {
            printf("%d ", x->key);
            x = x->forward[i];
        }
        printf("\n");
    }
}