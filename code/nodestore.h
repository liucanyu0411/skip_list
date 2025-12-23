// nodestore.h
#ifndef NODESTORE_H
#define NODESTORE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NodeStore NodeStore;

typedef struct NodeStoreOps {
    NodeStore* (*create)(int capacity);
    void       (*destroy)(NodeStore* s);

    int        (*size)(const NodeStore* s);
    int        (*capacity)(const NodeStore* s);
    void       (*clear)(NodeStore* s);

    int        (*key_at)(const NodeStore* s, int idx);
    void*      (*val_at)(const NodeStore* s, int idx);
    void       (*set_val)(NodeStore* s, int idx, void* v);

    int        (*lower_bound)(const NodeStore* s, int key);

    void       (*insert_at)(NodeStore* s, int idx, int key, void* val);
    void       (*erase_at)(NodeStore* s, int idx);

    int        (*split)(NodeStore* left, NodeStore* right);
} NodeStoreOps;

typedef enum {
    NODESTORE_ARRAY   = 1,
    NODESTORE_LINKED  = 2,
    NODESTORE_SKIPLIST= 3
} NodeStoreKind;

const NodeStoreOps* nodestore_get_ops(NodeStoreKind kind);

#ifdef __cplusplus
}
#endif

#endif
