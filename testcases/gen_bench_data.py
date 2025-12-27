#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Generate input data files for the benchmark that reads:
  --insert <file>  --search <file>  --delete <file>

File format:
  - integers separated by whitespace
  - optional comment lines starting with '#'
"""

from __future__ import annotations
import argparse
import random
import math
from typing import List, Set, Tuple, Optional


INT32_MIN = -(2**31)
INT32_MAX =  (2**31 - 1)


def clamp_int(x: int, lo: int, hi: int) -> int:
    if x < lo:
        return lo
    if x > hi:
        return hi
    return x


def write_ints(path: str, nums: List[int], header: str, per_line: int = 16) -> None:
    with open(path, "w", encoding="utf-8") as f:
        if header:
            for line in header.splitlines():
                f.write("# " + line.rstrip() + "\n")
        for i, v in enumerate(nums):
            f.write(str(v))
            if (i + 1) % per_line == 0:
                f.write("\n")
            else:
                f.write(" ")
        if len(nums) % per_line != 0:
            f.write("\n")


def gen_unique_uniform(rng: random.Random, n: int, lo: int, hi: int) -> List[int]:
    """Generate n unique ints in [lo, hi]."""
    width = hi - lo + 1
    if n > width:
        raise ValueError(f"unique_uniform: need n={n} <= range_size={width}")
    # For moderate/large ranges, sampling a set is fine.
    s: Set[int] = set()
    while len(s) < n:
        s.add(rng.randint(lo, hi))
    return list(s)


def gen_uniform(rng: random.Random, n: int, lo: int, hi: int) -> List[int]:
    return [rng.randint(lo, hi) for _ in range(n)]


def gen_normal(rng: random.Random, n: int, mean: float, std: float, lo: int, hi: int) -> List[int]:
    out: List[int] = []
    for _ in range(n):
        x = int(round(rng.gauss(mean, std)))
        out.append(clamp_int(x, lo, hi))
    return out


def gen_exponential(rng: random.Random, n: int, lambd: float, lo: int, hi: int) -> List[int]:
    """Shifted exponential: lo + Exp(lambd) then clamp."""
    out: List[int] = []
    for _ in range(n):
        x = lo + int(rng.expovariate(lambd))
        out.append(clamp_int(x, lo, hi))
    return out


def gen_pareto(rng: random.Random, n: int, alpha: float, lo: int, hi: int) -> List[int]:
    """
    Heavy-tail distribution using Pareto(alpha) (continuous), mapped to integers:
      x = lo + floor(Pareto(alpha) * scale)
    """
    width = hi - lo + 1
    scale = max(1, width // 20)
    out: List[int] = []
    for _ in range(n):
        x = lo + int(rng.paretovariate(alpha) * scale)
        out.append(clamp_int(x, lo, hi))
    return out


def gen_sorted_sequence(base: List[int]) -> List[int]:
    return sorted(base)


def gen_reverse_sequence(base: List[int]) -> List[int]:
    return sorted(base, reverse=True)


def gen_nearly_sorted(rng: random.Random, base: List[int], swap_frac: float) -> List[int]:
    """
    Start from sorted base, then do k random swaps.
    swap_frac in [0,1], k ≈ swap_frac * len(base)
    """
    a = sorted(base)
    n = len(a)
    k = int(round(swap_frac * n))
    if n <= 1 or k <= 0:
        return a
    for _ in range(k):
        i = rng.randrange(n)
        j = rng.randrange(n)
        a[i], a[j] = a[j], a[i]
    return a


def gen_clusters_normal(
    rng: random.Random,
    n: int,
    centers: List[float],
    std: float,
    lo: int,
    hi: int,
) -> List[int]:
    """Mixture of Gaussians: pick a center uniformly then sample N(center, std)."""
    out: List[int] = []
    for _ in range(n):
        c = centers[rng.randrange(len(centers))]
        x = int(round(rng.gauss(c, std)))
        out.append(clamp_int(x, lo, hi))
    return out


def make_insert_keys(args, rng: random.Random) -> List[int]:
    lo, hi = args.key_min, args.key_max
    n = args.n_insert

    mode = args.insert_dist
    if mode == "unique_uniform":
        base = gen_unique_uniform(rng, n, lo, hi)
    elif mode == "uniform":
        base = gen_uniform(rng, n, lo, hi)
    elif mode == "normal":
        base = gen_normal(rng, n, args.mean, args.std, lo, hi)
    elif mode == "exp":
        base = gen_exponential(rng, n, args.lambd, lo, hi)
    elif mode == "pareto":
        base = gen_pareto(rng, n, args.alpha, lo, hi)
    elif mode == "clusters":
        centers = [float(x) for x in args.centers.split(",") if x.strip() != ""]
        if not centers:
            raise ValueError("--centers is required for insert_dist=clusters, e.g. 100,1000,5000")
        base = gen_clusters_normal(rng, n, centers, args.std, lo, hi)
    elif mode == "sorted_unique":
        base = gen_unique_uniform(rng, n, lo, hi)
        base = gen_sorted_sequence(base)
    elif mode == "reverse_unique":
        base = gen_unique_uniform(rng, n, lo, hi)
        base = gen_reverse_sequence(base)
    elif mode == "nearly_sorted_unique":
        base = gen_unique_uniform(rng, n, lo, hi)
        base = gen_nearly_sorted(rng, base, args.swap_frac)
    else:
        raise ValueError(f"Unknown insert_dist: {mode}")

    return base


def make_delete_keys(args, rng: random.Random, insert_keys: List[int]) -> List[int]:
    mode = args.delete_mode
    n = args.n_delete if args.n_delete is not None else len(insert_keys)

    if mode == "shuffle_all":
        keys = insert_keys[:]  # delete inserted keys (best for correctness)
        rng.shuffle(keys)
        if n < len(keys):
            keys = keys[:n]
        return keys

    if mode == "in_order":
        keys = insert_keys[:]
        if n < len(keys):
            keys = keys[:n]
        return keys

    if mode == "random_subset":
        if n > len(insert_keys):
            raise ValueError("delete_mode=random_subset requires n_delete <= n_insert")
        idxs = list(range(len(insert_keys)))
        rng.shuffle(idxs)
        chosen = [insert_keys[i] for i in idxs[:n]]
        rng.shuffle(chosen)
        return chosen

    raise ValueError(f"Unknown delete_mode: {mode}")


def make_search_keys(
    args,
    rng: random.Random,
    insert_keys: List[int],
) -> Tuple[List[int], int]:
    """
    Generate search keys with a desired hit ratio:
      - hit part: sample from insert_keys
      - miss part: generate keys guaranteed not in insert_keys (by shifting outside range or retry)
    Returns (queries, expected_hits)
    """
    n = args.n_search
    hit = int(round(args.hit_ratio * n))
    miss = n - hit

    insert_set = set(insert_keys)

    # hits: sample with replacement if needed
    hits = [insert_keys[rng.randrange(len(insert_keys))] for _ in range(hit)] if insert_keys else []

    # misses: try to generate numbers that are not in insert_set
    lo, hi = args.key_min, args.key_max
    misses: List[int] = []

    miss_mode = args.search_miss_dist
    if miss_mode == "offset":
        # guaranteed miss by adding a big offset outside [lo,hi] if possible
        # If key range is full int32, offset may clamp; so we also check membership.
        offset = max(1000003, (hi - lo + 1) + 12345)
        for _ in range(miss):
            base = insert_keys[rng.randrange(len(insert_keys))] if insert_keys else rng.randint(lo, hi)
            x = base + offset
            x = clamp_int(x, INT32_MIN, INT32_MAX)
            # If still collides (rare), fall back to retry
            if x in insert_set:
                tries = 0
                while tries < 1000:
                    x2 = rng.randint(INT32_MIN, INT32_MAX)
                    if x2 not in insert_set:
                        x = x2
                        break
                    tries += 1
            misses.append(x)

    elif miss_mode == "uniform_retry":
        for _ in range(miss):
            tries = 0
            while True:
                x = rng.randint(lo, hi)
                if x not in insert_set:
                    misses.append(x)
                    break
                tries += 1
                if tries > 2000:
                    # fallback: jump outside range
                    x = rng.randint(INT32_MIN, INT32_MAX)
                    if x not in insert_set:
                        misses.append(x)
                        break

    elif miss_mode == "normal_retry":
        for _ in range(miss):
            tries = 0
            while True:
                x = int(round(rng.gauss(args.mean, args.std)))
                x = clamp_int(x, lo, hi)
                if x not in insert_set:
                    misses.append(x)
                    break
                tries += 1
                if tries > 2000:
                    x2 = rng.randint(INT32_MIN, INT32_MAX)
                    if x2 not in insert_set:
                        misses.append(x2)
                        break

    else:
        raise ValueError(f"Unknown search_miss_dist: {miss_mode}")

    queries = hits + misses
    rng.shuffle(queries)
    return queries, hit


def build_argparser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Generate input files for benchmark: insert/search/delete key sequences."
    )
    p.add_argument("--out-prefix", type=str, default="case1",
                   help="output file prefix; will write <prefix>_insert.txt, <prefix>_search.txt, <prefix>_delete.txt")
    p.add_argument("--seed", type=int, default=12345)

    # counts
    p.add_argument("--n-insert", type=int, required=True)
    p.add_argument("--n-search", type=int, required=True)
    p.add_argument("--n-delete", type=int, default=None,
                   help="defaults to n_insert; if smaller, deletes a subset")

    # key range
    p.add_argument("--key-min", type=int, default=0)
    p.add_argument("--key-max", type=int, default=10**9)

    # insert distributions
    p.add_argument("--insert-dist", type=str, default="unique_uniform",
                   choices=[
                       "unique_uniform", "uniform", "normal", "exp", "pareto", "clusters",
                       "sorted_unique", "reverse_unique", "nearly_sorted_unique"
                   ])
    p.add_argument("--mean", type=float, default=0.0, help="for normal/clusters")
    p.add_argument("--std", type=float, default=1000.0, help="for normal/clusters")
    p.add_argument("--lambd", type=float, default=1.0, help="for exp distribution")
    p.add_argument("--alpha", type=float, default=1.5, help="for pareto distribution")
    p.add_argument("--centers", type=str, default="", help="for clusters: comma-separated centers, e.g. 100,1000,5000")
    p.add_argument("--swap-frac", type=float, default=0.01, help="for nearly_sorted_unique: fraction of swaps")

    # delete behavior
    p.add_argument("--delete-mode", type=str, default="shuffle_all",
                   choices=["shuffle_all", "in_order", "random_subset"])

    # search behavior
    p.add_argument("--hit-ratio", type=float, default=0.5, help="fraction of search keys that should exist in insert set")
    p.add_argument("--search-miss-dist", type=str, default="offset",
                   choices=["offset", "uniform_retry", "normal_retry"],
                   help="how to generate miss queries")

    # formatting
    p.add_argument("--per-line", type=int, default=16)
    return p


def main() -> int:
    args = build_argparser().parse_args()

    if args.key_min > args.key_max:
        raise SystemExit("Error: --key-min must be <= --key-max")
    if args.n_insert <= 0 or args.n_search <= 0:
        raise SystemExit("Error: n must be positive")
    if not (0.0 <= args.hit_ratio <= 1.0):
        raise SystemExit("Error: --hit-ratio must be in [0,1]")
    if args.n_delete is not None and args.n_delete < 0:
        raise SystemExit("Error: --n-delete must be >= 0")

    rng = random.Random(args.seed)

    insert_keys = make_insert_keys(args, rng)

    # Delete and search use different RNG stream for repeatability
    rng_del = random.Random(args.seed ^ 0xC8013EA4)
    delete_keys = make_delete_keys(args, rng_del, insert_keys)

    rng_q = random.Random(args.seed ^ 0x9E3779B9)
    search_keys, expected_hits = make_search_keys(args, rng_q, insert_keys)

    prefix = args.out_prefix
    insert_path = f"{prefix}_insert.txt"
    search_path = f"{prefix}_search.txt"
    delete_path = f"{prefix}_delete.txt"

    common = (
        f"seed={args.seed}\n"
        f"key_range=[{args.key_min},{args.key_max}]\n"
    )

    write_ints(
        insert_path, insert_keys,
        header=(
            "INSERT keys\n" + common +
            f"n_insert={args.n_insert}\n"
            f"insert_dist={args.insert_dist}, mean={args.mean}, std={args.std}, lambd={args.lambd}, alpha={args.alpha}, centers={args.centers}, swap_frac={args.swap_frac}\n"
        ),
        per_line=args.per_line
    )

    write_ints(
        search_path, search_keys,
        header=(
            "SEARCH keys\n" + common +
            f"n_search={args.n_search}\n"
            f"hit_ratio={args.hit_ratio} (expected_hits~{expected_hits})\n"
            f"miss_dist={args.search_miss_dist}\n"
        ),
        per_line=args.per_line
    )

    write_ints(
        delete_path, delete_keys,
        header=(
            "DELETE keys\n" + common +
            f"n_delete={len(delete_keys)}\n"
            f"delete_mode={args.delete_mode}\n"
        ),
        per_line=args.per_line
    )

    print("Wrote:")
    print(" ", insert_path)
    print(" ", search_path)
    print(" ", delete_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())