#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/../.." && pwd)
source_file="$repo_root/research/beam_search/topology_microbench.cpp"
result_file="$repo_root/research/beam_search/topology_microbench_results.tsv"
build_dir=$(mktemp -d /tmp/beam-topology-microbench.XXXXXX)
trap 'rm -rf -- "$build_dir"' EXIT

cxx=${CXX:-g++}
samples=${TOPOLOGY_SAMPLES:-9}
target_units=${TOPOLOGY_TARGET_UNITS:-8000000}
cpu=${TOPOLOGY_CPU:-}

"$cxx" -std=c++20 -O3 -DNDEBUG -march=native -I"$repo_root" "$source_file" -o "$build_dir/benchmark"

command=("$build_dir/benchmark" --samples "$samples" --target-units "$target_units")
if [[ -n "$cpu" ]]; then
    command=(taskset -c "$cpu" "${command[@]}")
fi

{
    printf '# generated_at\t%s\n' "$(date --iso-8601=seconds)"
    printf '# compiler\t%s\n' "$($cxx --version | head -n 1)"
    printf '# kernel\t%s\n' "$(uname -srmo)"
    printf '# cpu_model\t%s\n' "$(lscpu | sed -n 's/^Model name:[[:space:]]*//p')"
    printf '# cache\t%s\n' "$(lscpu | sed -n '/^L1d cache:\|^L2 cache:\|^L3 cache:/p' | tr '\n' ';')"
    printf '# flags\t-std=c++20 -O3 -DNDEBUG -march=native\n'
    printf '# samples\t%s\n' "$samples"
    printf '# target_units\t%s\n' "$target_units"
    printf '# cpu\t%s\n' "${cpu:-unpinned}"
    printf '# source_sha256\t%s\n' "$(sha256sum "$source_file" | cut -d' ' -f1)"
    "${command[@]}"
} | tee "$result_file"
