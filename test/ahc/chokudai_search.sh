#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/../.." && pwd)"
work_dir="$(mktemp -d)"
trap 'rm -rf -- "$work_dir"' EXIT

cxx="${CXX:-g++}"
common=(-std=c++20 -O2 -Wall -Wextra -I"$repo_root")
if [[ "${CHOKUDAI_SANITIZE:-0}" == "1" ]]; then
    common=(-std=c++20 -O1 -g -Wall -Wextra -I"$repo_root")
    common+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi

"$cxx" "${common[@]}" "$script_dir/chokudai_search.cpp" -o "$work_dir/chokudai.bin"
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1}" \
    "$work_dir/chokudai.bin" "$work_dir" "${CHOKUDAI_RANDOM_CASES:-160}"

python3 - "$work_dir" <<'PY'
import json
import pathlib
import sys

for name in ("fixed_history.json", "turn_history.json"):
    history = json.loads((pathlib.Path(sys.argv[1]) / name).read_text())
    assert history["INF"] == 1 << 28
    nodes = {node["node_id"]: node for node in history["nodes"]}
    assert nodes and len(nodes) == len(history["nodes"])
    for node in nodes.values():
        if node["parent_id"] >= 0:
            parent = nodes[node["parent_id"]]
            if node["status"] == 0:
                assert node["turn"] > parent["turn"]
        assert node["status"] in (0, 1, 2)
    for snapshot in history["snapshots"]:
        assert all(node_id in nodes for node_id in snapshot["active_node_ids"])
print("ChokudaiSearch history JSON checks passed")
PY
