// bptree.h
#ifndef BPTREE_H
#define BPTREE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nodestore.h"

typedef struct BPTree BPTree;

BPTree* bptree_create(int order_M, const NodeStoreOps* ops);
void    bptree_destroy(BPTree* t);

int     bptree_search(const BPTree* t, int key);
void    bptree_insert(BPTree* t, int key);
void    bptree_delete(BPTree* t, int key);

int     bptree_height(const BPTree* t);

#ifdef __cplusplus
}
#endif

#endif
