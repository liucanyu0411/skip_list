// nodestore.c
#include "nodestore.h"

const NodeStoreOps* nodestore_array_ops(void);
const NodeStoreOps* nodestore_list_ops(void);
const NodeStoreOps* nodestore_skip_ops(void);

const NodeStoreOps* nodestore_get_ops(NodeStoreKind kind) {
    switch (kind) {
        case NODESTORE_ARRAY:    return nodestore_array_ops();
        case NODESTORE_LINKED:   return nodestore_list_ops();
        case NODESTORE_SKIPLIST: return nodestore_skip_ops();
        default: return 0;
    }
}
