#!/usr/bin/env bash
set -euo pipefail

# Environment controls: TURN_BENCH_WARMUP, TURN_BENCH_REPETITIONS,
# TURN_BENCH_TARGET_TRY_OPS, TURN_BENCH_CPU, and TURN_BENCH_RESULT.
# Additional arguments are forwarded to both benchmark binaries.

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/../../../.." && pwd)"
build_dir="$(mktemp -d "${TMPDIR:-/tmp}/beam-search-turn-microbench.XXXXXX")"
trap 'rm -rf -- "$build_dir"' EXIT

cxx="${CXX:-g++}"
source_file="$script_dir/beam_search_turn_microbench.cpp"
flags=(-std=c++20 -O3 -DNDEBUG -march=native -Wall -Wextra -Wno-sign-compare \
       -Wno-unused-parameter -I"$repo_root")
common_args=(
    --warmup "${TURN_BENCH_WARMUP:-1}"
    --repetitions "${TURN_BENCH_REPETITIONS:-5}"
    --target-try-ops "${TURN_BENCH_TARGET_TRY_OPS:-1000000}"
)
runner=()
if [[ -n "${TURN_BENCH_CPU:-}" ]]; then
    runner=(taskset -c "$TURN_BENCH_CPU")
fi

"$cxx" "${flags[@]}" -DTURN_BEAM_BENCH_BASELINE "$source_file" -o "$build_dir/baseline"
"$cxx" "${flags[@]}" -DTURN_BEAM_BENCH_OPTIMIZED "$source_file" -o "$build_dir/optimized"

for backend in baseline optimized; do
    "${runner[@]}" "$build_dir/$backend" "${common_args[@]}" "$@" > "$build_dir/$backend.tsv"
    awk -F '\t' 'NR > 1 {
        print $2,$3,$4,$5,$6,$7,$8,$9,$10,$16,$17,$18,$25,$26,$27,$28,$29,$30
    }' OFS='\t' "$build_dir/$backend.tsv" > "$build_dir/$backend.correctness.tsv"
done

diff -u "$build_dir/baseline.correctness.tsv" "$build_dir/optimized.correctness.tsv"

if [[ -n "${TURN_BENCH_RESULT:-}" ]]; then
    awk 'FNR == 1 && NR != 1 { next } { print }' \
        "$build_dir/baseline.tsv" "$build_dir/optimized.tsv" > "$TURN_BENCH_RESULT"
    echo "result=$TURN_BENCH_RESULT"
else
    awk 'FNR == 1 && NR != 1 { next } { print }' \
        "$build_dir/baseline.tsv" "$build_dir/optimized.tsv"
fi
