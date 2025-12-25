#ifndef SKIPLIST_H
#define SKIPLIST_H

#include <stdbool.h>

// 你可以按数据规模调大/调小（常用 16/32/64）
#define SKIPLIST_MAX_LEVEL 32

typedef struct SkipListNode {
    int key;
    int level;                      // 该节点 forward[] 长度
    struct SkipListNode* forward[]; // forward[i]：第 i 层的下一个节点
} SkipListNode;

typedef struct SkipList {
    int max_level;                  // <= SKIPLIST_MAX_LEVEL
    double p;                       // 提升概率（常用 0.5）
    int level;                      // 当前跳表实际层数（>=1）
    int size;                       // 元素数量
    SkipListNode* header;           // 头结点（哨兵）
} SkipList;

// 创建 / 销毁
SkipList* skiplist_create(int max_level, double p);
void skiplist_destroy(SkipList* sl);

// 基本操作
bool skiplist_search(const SkipList* sl, int key);
bool skiplist_insert(SkipList* sl, int key);  // 成功插入返回 true；重复 key 返回 false
bool skiplist_erase(SkipList* sl, int key);   // 删除成功 true；不存在 false

// 调试输出（可选）
void skiplist_print(const SkipList* sl);

#endif