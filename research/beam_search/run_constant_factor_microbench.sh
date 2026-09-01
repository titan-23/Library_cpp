#!/usr/bin/env bash
set -euo pipefail

bench_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd -- "$bench_dir/../.." && pwd)"
bench_bin="$(mktemp "${TMPDIR:-/tmp}/beam-search-constant-factor.XXXXXX")"
trap 'rm -f -- "$bench_bin"' EXIT
compiler="${CXX:-g++}"

"$compiler" -std=c++20 -O3 -DNDEBUG -march=native -Wall -Wextra -Wno-sign-compare -I"$repo_dir" \
    "$bench_dir/constant_factor_microbench.cpp" -o "$bench_bin"
"$bench_bin" "$@"
