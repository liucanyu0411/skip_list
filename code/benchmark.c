// benchmark.c
//
// Project 8 - Skip List / B+Tree NodeStore Benchmark
//
// This benchmark does NOT generate data internally.
// Instead, it reads provided input files:
//   - insert file:  a sequence of integer keys to insert
//   - search file:  a sequence of integer keys to query
//   - delete file:  a sequence of integer keys to delete
//
// File format:
//   - integers separated by any whitespace (space/tab/newline)
//   - lines starting with '#' are treated as comments (optional)
// Example insert file:
//   # insert keys
//   3 1 4 1 5 9
//
// Usage example:
//   ./bench --m 128 --impl skip --insert testcases/ins.txt --search testcases/q.txt --delete testcases/del.txt --rounds 5 --csv out.csv
//

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <inttypes.h>

#include "bptree.h"
#include "nodestore.h"

#ifndef BENCH_READ_BUF
#define BENCH_READ_BUF (1u << 16)  // 64 KiB
#endif

static uint64_t now_ns(void) {
    struct timespec ts;

#if defined(CLOCK_MONOTONIC)
    // Preferred: monotonic clock for benchmarking
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        // Fallback to timespec_get on failure
        timespec_get(&ts, TIME_UTC);
    }
#else
    // C11 fallback (not monotonic, but portable)
    timespec_get(&ts, TIME_UTC);
#endif

    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage:\n"
        "  %s --m ORDER --impl array|list|skip --insert INS.txt --search Q.txt --delete DEL.txt [options]\n"
        "\n"
        "Required arguments:\n"
        "  --m ORDER          B+ tree order M (M >= 3)\n"
        "  --impl KIND        NodeStore implementation: array | list | skip\n"
        "  --insert PATH      Keys to insert (plain text integers)\n"
        "  --search PATH      Keys to query  (plain text integers)\n"
        "  --delete PATH      Keys to delete (plain text integers)\n"
        "\n"
        "Optional arguments:\n"
        "  --rounds R         Repeat benchmark R times (default: 3)\n"
        "  --csv PATH         Write CSV output to PATH (default: stdout)\n"
        "  --tag STR          Extra label written to CSV (default: empty)\n"
        "  --help             Show this help\n"
        "\n"
        "Input file format:\n"
        "  - Integers separated by whitespace.\n"
        "  - Lines starting with '#' are treated as comments.\n"
        "\n"
        "CSV columns:\n"
        "  tag,impl,M,n_insert,n_search,n_delete,round,insert_ns,search_ns,delete_ns,found_count,height_after_insert\n",
        prog
    );
}

static NodeStoreKind parse_impl(const char *s) {
    if (!s) return NODESTORE_ARRAY;
    if (strcmp(s, "array") == 0) return NODESTORE_ARRAY;
    if (strcmp(s, "list") == 0)  return NODESTORE_LINKED;
    if (strcmp(s, "skip") == 0)  return NODESTORE_SKIPLIST;
    return 0;
}

static const char* impl_name(NodeStoreKind k) {
    switch (k) {
        case NODESTORE_ARRAY:    return "array";
        case NODESTORE_LINKED:   return "list";
        case NODESTORE_SKIPLIST: return "skip";
        default:                 return "unknown";
    }
}

static int push_int(int **arr, size_t *len, size_t *cap, int v) {
    if (*len == *cap) {
        size_t ncap = (*cap == 0) ? 1024 : (*cap * 2);
        int *tmp = (int*)realloc(*arr, ncap * sizeof(int));
        if (!tmp) return 0;
        *arr = tmp;
        *cap = ncap;
    }
    (*arr)[(*len)++] = v;
    return 1;
}

// Fast integer reader:
// - supports optional leading '-'
// - ignores comments starting with '#' 
static int read_int_file(const char *path, int **out_arr, size_t *out_len) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "Error: cannot open '%s': %s\n", path, strerror(errno));
        return 0;
    }

    char *buf = (char*)malloc(BENCH_READ_BUF);
    if (!buf) {
        fprintf(stderr, "Error: malloc failed while reading '%s'\n", path);
        fclose(fp);
        return 0;
    }

    int *arr = NULL;
    size_t len = 0, cap = 0;

    int in_num = 0;
    int sign = 1;
    long long val = 0;

    int in_comment = 0;

    while (1) {
        size_t n = fread(buf, 1, BENCH_READ_BUF, fp);
        if (n == 0) break;

        for (size_t i = 0; i < n; ++i) {
            unsigned char c = (unsigned char)buf[i];

            if (in_comment) {
                if (c == '\n' || c == '\r') in_comment = 0;
                continue;
            }

            if (!in_num && c == '#') {
                in_comment = 1;
                continue;
            }

            if (!in_num) {
                if (c == '-') {
                    in_num = 1;
                    sign = -1;
                    val = 0;
                } else if (c >= '0' && c <= '9') {
                    in_num = 1;
                    sign = 1;
                    val = (long long)(c - '0');
                } else {
                    // separator
                }
            } else {
                if (c >= '0' && c <= '9') {
                    val = val * 10 + (long long)(c - '0');
                } else {
                    long long x = (long long)sign * val;
                    if (x < (long long)INT32_MIN || x > (long long)INT32_MAX) {
                        fprintf(stderr, "Error: integer out of range in '%s'\n", path);
                        free(arr);
                        free(buf);
                        fclose(fp);
                        return 0;
                    }
                    if (!push_int(&arr, &len, &cap, (int)x)) {
                        fprintf(stderr, "Error: realloc failed while reading '%s'\n", path);
                        free(arr);
                        free(buf);
                        fclose(fp);
                        return 0;
                    }
                    in_num = 0;
                    sign = 1;
                    val = 0;

                    if (c == '#') in_comment = 1;
                }
            }
        }
    }

    // finalize last number if file ended while parsing
    if (in_num) {
        long long x = (long long)sign * val;
        if (x < (long long)INT32_MIN || x > (long long)INT32_MAX) {
            fprintf(stderr, "Error: integer out of range in '%s'\n", path);
            free(arr);
            free(buf);
            fclose(fp);
            return 0;
        }
        if (!push_int(&arr, &len, &cap, (int)x)) {
            fprintf(stderr, "Error: realloc failed while reading '%s'\n", path);
            free(arr);
            free(buf);
            fclose(fp);
            return 0;
        }
    }

    free(buf);
    fclose(fp);

    *out_arr = arr;
    *out_len = len;
    return 1;
}

int main(int argc, char **argv) {
    int m = 0;
    int rounds = 3;
    NodeStoreKind impl = 0;

    const char *path_insert = NULL;
    const char *path_search = NULL;
    const char *path_delete = NULL;

    const char *csv_path = NULL;
    const char *tag = "";

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--m") == 0 && i + 1 < argc) {
            m = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--rounds") == 0 && i + 1 < argc) {
            rounds = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--impl") == 0 && i + 1 < argc) {
            impl = parse_impl(argv[++i]);
        } else if (strcmp(argv[i], "--insert") == 0 && i + 1 < argc) {
            path_insert = argv[++i];
        } else if (strcmp(argv[i], "--search") == 0 && i + 1 < argc) {
            path_search = argv[++i];
        } else if (strcmp(argv[i], "--delete") == 0 && i + 1 < argc) {
            path_delete = argv[++i];
        } else if (strcmp(argv[i], "--csv") == 0 && i + 1 < argc) {
            csv_path = argv[++i];
        } else if (strcmp(argv[i], "--tag") == 0 && i + 1 < argc) {
            tag = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Error: unknown or incomplete argument '%s'\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    if (m < 3 || rounds <= 0 || !impl || !path_insert || !path_search || !path_delete) {
        usage(argv[0]);
        return 1;
    }

    const NodeStoreOps *ops = nodestore_get_ops(impl);
    if (!ops) {
        fprintf(stderr, "Error: nodestore_get_ops() does not support impl='%s'\n", impl_name(impl));
        return 1;
    }

    int *ins = NULL, *qry = NULL, *del = NULL;
    size_t n_ins = 0, n_qry = 0, n_del = 0;

    if (!read_int_file(path_insert, &ins, &n_ins)) return 1;
    if (!read_int_file(path_search, &qry, &n_qry)) { free(ins); return 1; }
    if (!read_int_file(path_delete, &del, &n_del)) { free(ins); free(qry); return 1; }

    if (n_ins == 0) fprintf(stderr, "Warning: insert file '%s' is empty.\n", path_insert);
    if (n_qry == 0) fprintf(stderr, "Warning: search file '%s' is empty.\n", path_search);
    if (n_del == 0) fprintf(stderr, "Warning: delete file '%s' is empty.\n", path_delete);

    FILE *out = stdout;
    if (csv_path) {
        out = fopen(csv_path, "w");
        if (!out) {
            fprintf(stderr, "Error: cannot open '%s' for write: %s\n", csv_path, strerror(errno));
            free(ins); free(qry); free(del);
            return 1;
        }
    }

    fprintf(out, "tag,impl,M,n_insert,n_search,n_delete,round,insert_ns,search_ns,delete_ns,found_count,height_after_insert\n");

    int tottime = 0;
    for (int r = 1; r <= rounds; ++r) {
        BPTree *t = bptree_create(m, ops);
        if (!t) {
            fprintf(stderr, "Error: bptree_create failed\n");
            free(ins); free(qry); free(del);
            if (out != stdout) fclose(out);
            return 1;
        }

        uint64_t t0 = now_ns();
        for (size_t i = 0; i < n_ins; ++i) bptree_insert(t, ins[i]);
        uint64_t t1 = now_ns();

        int found = 0;
        for (size_t i = 0; i < n_qry; ++i) found += bptree_search(t, qry[i]);
        uint64_t t2 = now_ns();

        for (size_t i = 0; i < n_del; ++i) bptree_delete(t, del[i]);
        uint64_t t3 = now_ns();

        int h = bptree_height(t);

        fprintf(out, "%s,%s,%d,%zu,%zu,%zu,%d,%llu,%llu,%llu,%d,%d,total time=%llu\n",
                tag, impl_name(impl), m, n_ins, n_qry, n_del, r,
                (t1 - t0), (t2 - t1), (t3 - t2), found, h, (t3 - t0));

        tottime += (t3 - t0);

        bptree_destroy(t);
    }

    fprintf(out, "total time for all rounds: %llu ns\n", tottime);

    if (out != stdout) fclose(out);
    free(ins);
    free(qry);
    free(del);
    return 0;
}
