#!/usr/bin/env python3
"""Freeze the pointer implementations and isolate their quoted helper includes.

The five files directly in the output directory are byte-exact snapshots.
Compile files from isolated/ to use those frozen helpers rather than future
production helpers. Existing snapshots are reused and never refreshed from
production; use a new output directory for a different snapshot generation.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re


FILES = (
    "wb_tree.cpp", "wb_tree_seg.cpp", "lazy_wb_tree.cpp",
    "_mutable_wbt_sequence.cpp", "_wbt_node_pool.cpp",
)
HELPERS = ("_mutable_wbt_sequence.cpp", "_wbt_node_pool.cpp")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    here = Path(__file__).resolve().parent
    root = here.parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", type=Path, default=root / "titan_cpplib/ds")
    parser.add_argument("--output-dir", type=Path, default=here / "baseline/wbt_family_pointer")
    args = parser.parse_args()
    if args.source_dir.resolve() == args.output_dir.resolve():
        parser.error("snapshot output must be separate from production")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    existing = [(args.output_dir / filename).exists() for filename in FILES]
    if any(existing) and not all(existing):
        raise RuntimeError("partial pointer snapshot exists; refusing to mix snapshot generations")
    if all(existing):
        raw = {filename: (args.output_dir / filename).read_bytes() for filename in FILES}
    else:
        raw = {filename: (args.source_dir / filename).read_bytes() for filename in FILES}
        if any((args.source_dir / filename).read_bytes() != data for filename, data in raw.items()):
            raise RuntimeError("production changed while reading the pointer snapshot")
    for filename in ("lazy_wb_tree.cpp", "_mutable_wbt_sequence.cpp"):
        if not re.search(rb"using\s+NodePtr\s*=\s*Node\s*\*", raw[filename]):
            raise RuntimeError(f"expected Node* representation in pointer snapshot: {filename}")

    manifest_path = args.output_dir / "pointer_baseline_manifest.json"
    if manifest_path.exists():
        previous = json.loads(manifest_path.read_text())
        for filename, data in raw.items():
            if previous["files"][filename]["raw_sha256"] != digest(data):
                raise RuntimeError(f"frozen raw snapshot was modified: {filename}")

    def name(path):
        try:
            return str(path.resolve().relative_to(root))
        except ValueError:
            return str(path.resolve())

    isolated_dir = args.output_dir / "isolated"
    isolated_dir.mkdir(exist_ok=True)
    manifest = {
        "label": "pointer-snapshot-include-redirect-only",
        "generator": name(Path(__file__)),
        "generator_sha256": digest(Path(__file__).read_bytes()),
        "policy": "Raw snapshots are byte-exact and reused on later invocations; production changes are not imported.",
        "scope": "Isolated copies rewrite only quoted local helper include paths to frozen sibling files.",
        "runner_baseline_dir": name(isolated_dir),
        "external_quoted_include_retained": "titan_cpplib/others/print.cpp",
        "files": {},
    }
    for filename, data in raw.items():
        raw_path = args.output_dir / filename
        if not raw_path.exists():
            raw_path.write_bytes(data)
        isolated = data
        rewrites = []
        for helper in HELPERS:
            old = f'#include "titan_cpplib/ds/{helper}"'
            new = f'#include "{helper}"'
            count = isolated.count(old.encode())
            if count:
                isolated = isolated.replace(old.encode(), new.encode())
                rewrites.append({"old": old, "new": new, "occurrences": count})
        isolated_path = isolated_dir / filename
        isolated_path.write_bytes(isolated)
        manifest["files"][filename] = {
            "original_path": name(args.source_dir / filename),
            "raw": name(raw_path), "raw_sha256": digest(data),
            "isolated": name(isolated_path), "isolated_sha256": digest(isolated),
            "include_rewrites": rewrites,
        }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
