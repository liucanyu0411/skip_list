// benchmark.c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "bptree.h"
#include "nodestore.h"

static uint64_t now_ns(void) {
#if defined(__APPLE__)
    // clock_gettime is available on modern macOS; if older, use mach_absolute_time
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
#endif
}

static void shuffle_int(int *a, int n, uint32_t seed) {
    uint32_t x = seed ? seed : 2463534242u;
    for (int i = n - 1; i > 0; --i) {
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        int j = (int)(x % (uint32_t)(i + 1));
        int tmp = a[i]; a[i] = a[j]; a[j] = tmp;
    }
}

static int *make_keys_seq(int n) {
    int *a = (int*)malloc(sizeof(int) * (size_t)n);
    if (!a) return NULL;
    for (int i = 0; i < n; ++i) a[i] = i;
    return a;
}

static int *make_queries_mix(int n, uint32_t seed) {
    // Half existing [0..n-1], half non-existing [n..2n-1]
    int *q = (int*)malloc(sizeof(int) * (size_t)n);
    if (!q) return NULL;
    for (int i = 0; i < n; ++i) {
        if ((i & 1) == 0) q[i] = i / 2;         // existing
        else q[i] = n + (i / 2);               // non-existing
    }
    shuffle_int(q, n, seed);
    return q;
}

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage:\n"
            "  %s --n N --m ORDER [--rounds R] [--seed S] [--impl array|list|skip] [--csv out.csv]\n"
            "\n"
            "Default: rounds=5 seed=1 impl=array csv=stdout\n",
            prog);
}

static NodeStoreKind parse_impl(const char *s) {
    if (!s) return NODESTORE_ARRAY;
    if (strcmp(s, "array") == 0) return NODESTORE_ARRAY;
    if (strcmp(s, "list") == 0)  return NODESTORE_LINKED;
    if (strcmp(s, "skip") == 0)  return NODESTORE_SKIPLIST;
    return NODESTORE_ARRAY;
}

int main(int argc, char **argv) {
    int n = 100000;
    int m = 64;
    int rounds = 5;
    uint32_t seed = 1;
    NodeStoreKind impl = NODESTORE_ARRAY;
    const char *csv_path = NULL;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--n") == 0 && i + 1 < argc) {
            n = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--m") == 0 && i + 1 < argc) {
            m = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--rounds") == 0 && i + 1 < argc) {
            rounds = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--impl") == 0 && i + 1 < argc) {
            impl = parse_impl(argv[++i]);
        } else if (strcmp(argv[i], "--csv") == 0 && i + 1 < argc) {
            csv_path = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    if (n <= 0 || m < 3 || rounds <= 0) {
        usage(argv[0]);
        return 1;
    }

    const NodeStoreOps* ops = nodestore_get_ops(impl);
    if (!ops) {
        fprintf(stderr, "Error: selected impl not available in nodestore_get_ops()\n");
        return 1;
    }

    FILE *out = stdout;
    if (csv_path) {
        out = fopen(csv_path, "w");
        if (!out) {
            fprintf(stderr, "Error: cannot open %s\n", csv_path);
            return 1;
        }
    }

    fprintf(out, "impl,M,N,round,insert_ns,search_ns,delete_ns,found_count\n");

    int *keys = make_keys_seq(n);
    int *queries = make_queries_mix(n, seed ^ 0x9e3779b9u);
    int *del_order = make_keys_seq(n);

    if (!keys || !queries || !del_order) {
        fprintf(stderr, "Error: malloc failed\n");
        free(keys); free(queries); free(del_order);
        if (out != stdout) fclose(out);
        return 1;
    }

    // To make results stable: shuffle insert order and delete order
    shuffle_int(keys, n, seed ^ 0xA341316Cu);
    shuffle_int(del_order, n, seed ^ 0xC8013EA4u);

    for (int r = 1; r <= rounds; ++r) {
        BPTree *t = bptree_create(m, ops);
        if (!t) {
            fprintf(stderr, "Error: bptree_create failed\n");
            free(keys); free(queries); free(del_order);
            if (out != stdout) fclose(out);
            return 1;
        }

        uint64_t t0 = now_ns();
        for (int i = 0; i < n; ++i) bptree_insert(t, keys[i]);
        uint64_t t1 = now_ns();

        int found = 0;
        for (int i = 0; i < n; ++i) found += bptree_search(t, queries[i]);
        uint64_t t2 = now_ns();

        for (int i = 0; i < n; ++i) bptree_delete(t, del_order[i]);
        uint64_t t3 = now_ns();

        fprintf(out, "%d,%d,%d,%d,%llu,%llu,%llu,%d\n",
                (int)impl, m, n, r,
                (unsigned long long)(t1 - t0),
                (unsigned long long)(t2 - t1),
                (unsigned long long)(t3 - t2),
                found);

        bptree_destroy(t);

        // reshuffle per round to avoid cache pattern bias
        uint32_t s2 = seed ^ (uint32_t)r * 2654435761u;
        shuffle_int(keys, n, s2 ^ 0xA341316Cu);
        shuffle_int(del_order, n, s2 ^ 0xC8013EA4u);
        shuffle_int(queries, n, s2 ^ 0x9e3779b9u);
    }

    free(keys);
    free(queries);
    free(del_order);

    if (out != stdout) fclose(out);
    return 0;
}
