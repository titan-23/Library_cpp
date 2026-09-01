#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/../.." && pwd)"
work_dir="$(mktemp -d)"
trap 'rm -rf -- "$work_dir"' EXIT

cxx="${CXX:-g++}"
common=(-std=c++20 -O2 -Wall -Wextra -I"$repo_root")
if [[ "${BEAM_SANITIZE:-0}" == "1" ]]; then
    common=(-std=c++20 -O1 -g -Wall -Wextra -I"$repo_root")
    common+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi
source_file="$script_dir/beam_search_differential.cpp"
random_cases="${BEAM_RANDOM_CASES:-120}"

"$cxx" "${common[@]}" -DTEST_BEAM_BASELINE "$source_file" -o "$work_dir/baseline.bin"
"$cxx" "${common[@]}" -DTEST_BEAM_OPTIMIZED "$source_file" -o "$work_dir/optimized.bin"
"$cxx" "${common[@]}" -DTEST_BEAM_PARENT "$source_file" -o "$work_dir/parent.bin"
"$cxx" "${common[@]}" -DTEST_BEAM_PARENT_COMPACT "$source_file" -o "$work_dir/parent_compact.bin"

run_backend() {
    local name="$1"
    local asan_options="${ASAN_OPTIONS:-detect_leaks=0}"
    local ubsan_options="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1}"
    if ! ASAN_OPTIONS="$asan_options" UBSAN_OPTIONS="$ubsan_options" \
        "$work_dir/$name.bin" "$work_dir" "$random_cases" > "$work_dir/$name.out" 2> "$work_dir/$name.err"; then
        tail -n 240 "$work_dir/$name.err" >&2
        return 1
    fi
}

run_backend baseline
run_backend optimized
run_backend parent
run_backend parent_compact

diff -u "$work_dir/baseline.out" "$work_dir/optimized.out"
diff -u "$work_dir/baseline.out" "$work_dir/parent.out"
diff -u "$work_dir/baseline.out" "$work_dir/parent_compact.out"
echo "beam_search differential test passed"
