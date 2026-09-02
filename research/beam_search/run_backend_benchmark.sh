#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
warmup="${1:-2}"
repetitions="${2:-7}"
result_path="${3:-$script_dir/benchmark_results.tsv}"
target_expanded="${4:-250000}"
build_dir="$(mktemp -d /tmp/beam-search-benchmark.XXXXXX)"
cxx="${CXX:-g++}"
source_file="$script_dir/backend_benchmark.cpp"
flags=(-std=c++20 -O3 -DNDEBUG -march=native -I"$repo_root")
runner=()
if [[ -n "${BEAM_BENCH_CPU:-}" ]]; then
    runner=(taskset -c "$BEAM_BENCH_CPU")
fi

"$cxx" "${flags[@]}" -DBEAM_BENCH_BASELINE "$source_file" -o "$build_dir/baseline"
"$cxx" "${flags[@]}" -DBEAM_BENCH_STANDARD "$source_file" -o "$build_dir/standard"
"$cxx" "${flags[@]}" -DBEAM_BENCH_PARENT "$source_file" -o "$build_dir/parent_oracle"
"$cxx" "${flags[@]}" -DBEAM_BENCH_PARENT_COMPACT "$source_file" -o "$build_dir/parent_compact"

for backend in baseline standard parent_oracle parent_compact; do
    "${runner[@]}" "$build_dir/$backend" --warmup 0 --repetitions 1 \
        --target-expanded 1 > "$build_dir/$backend.oracle.tsv"
    awk -F '\t' 'NR > 1 { print $2, $17, $26, $27, $28, $29, $30, $31 }' OFS='\t' \
        "$build_dir/$backend.oracle.tsv" > "$build_dir/$backend.oracle.digest"
done

diff -u "$build_dir/baseline.oracle.digest" "$build_dir/standard.oracle.digest"
diff -u "$build_dir/baseline.oracle.digest" "$build_dir/parent_oracle.oracle.digest"
diff -u "$build_dir/baseline.oracle.digest" "$build_dir/parent_compact.oracle.digest"

for backend in baseline standard parent_oracle parent_compact; do
    "${runner[@]}" "$build_dir/$backend" --warmup "$warmup" --repetitions "$repetitions" \
        --target-expanded "$target_expanded" > "$build_dir/$backend.tsv"
    awk -F '\t' 'NR > 1 { print $2, $17, $26, $27, $28, $29, $30, $31 }' OFS='\t' \
        "$build_dir/$backend.tsv" > "$build_dir/$backend.digest"
    diff -u "$build_dir/baseline.oracle.digest" "$build_dir/$backend.digest"
done

awk 'FNR == 1 && NR != 1 { next } { print }' "$build_dir/baseline.tsv" "$build_dir/standard.tsv" \
    "$build_dir/parent_oracle.tsv" "$build_dir/parent_compact.tsv" > "$result_path"

echo "result=$result_path"
echo "build_dir=$build_dir"
echo "cpu_affinity=${BEAM_BENCH_CPU:-none}"
