// skiplist.h
#ifndef SKIPLIST_H
#define SKIPLIST_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SkipList SkipList;
typedef struct SkipNode SkipNode;

SkipList* skiplist_create(int max_level, double p, unsigned seed);
void      skiplist_destroy(SkipList* sl);

int       skiplist_size(const SkipList* sl);

SkipNode* skiplist_search(const SkipList* sl, int key);
SkipNode* skiplist_first_ge(const SkipList* sl, int key);

int       skiplist_insert(SkipList* sl, int key, void* val); // returns 1 if inserted, 0 if existed(updated)
int       skiplist_erase(SkipList* sl, int key);             // returns 1 if erased, 0 if not found

SkipNode* skiplist_first(const SkipList* sl);
SkipNode* skipnode_next0(const SkipNode* x);
int       skipnode_key(const SkipNode* x);
void*     skipnode_val(const SkipNode* x);
void      skipnode_set_val(SkipNode* x, void* v);

#ifdef __cplusplus
}
#endif

#endif
