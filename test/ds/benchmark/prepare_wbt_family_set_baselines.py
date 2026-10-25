#!/usr/bin/env python3
"""Make separately labeled, correctness-only set() baselines from raw snapshots."""

import argparse
import hashlib
import json
from pathlib import Path


PATCHES = {
    "lazy_wb_tree.cpp": [
        {
            "reason": "std::stack cannot be initialized from a single NodePtr initializer list.",
            "old": "            stack<NodePtr> path = {node};",
            "new": "            stack<NodePtr> path;\n            path.emplace(node);",
        },
    ],
    "persistent_seg_wbtree.cpp": [
        {
            "reason": "std::stack cannot be initialized from a single node index initializer list.",
            "old": "        stack<SizeType> path = {node};",
            "new": "        stack<SizeType> path;\n        path.emplace(node);",
        },
        {
            "reason": "std::stack provides emplace(), not emplace_back().",
            "old": "            path.emplace_back(node);",
            "new": "            path.emplace(node);",
        },
        {
            "reason": "Replacing the root must update the returned local root, not the null sentinel's child.",
            "old": "                path.emplace(node);\n                if (d) ma.tree[pnode].left = node;\n                else ma.tree[pnode].right = node;",
            "new": "                path.emplace(node);\n                if (!pnode) root = node;\n                else if (d) ma.tree[pnode].left = node;\n                else ma.tree[pnode].right = node;",
        },
    ],
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    here = Path(__file__).resolve().parent
    root = here.parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", type=Path, default=here / "baseline/wbt_family")
    parser.add_argument("--output-dir", type=Path, default=here / "baseline/wbt_family_corrected")
    args = parser.parse_args()
    if args.source_dir.resolve() == args.output_dir.resolve():
        parser.error("corrected baselines must be separate from raw snapshots")
    args.output_dir.mkdir(parents=True, exist_ok=True)

    def name(path):
        try:
            return str(path.resolve().relative_to(root))
        except ValueError:
            return str(path.resolve())

    manifest = {
        "label": "set-correctness-only",
        "generator": name(Path(__file__)),
        "scope": "Only set() stack construction, stack insertion spelling, and persistent root-target linking.",
        "notes": "Existing copies, duplicate path entries, updates and all other code are retained. Raw snapshots are unchanged.",
        "files": {},
    }
    for filename, replacements in PATCHES.items():
        source = args.source_dir / filename
        destination = args.output_dir / filename
        raw = source.read_bytes()
        corrected = raw.decode()
        for replacement in replacements:
            count = corrected.count(replacement["old"])
            if count != 1:
                raise RuntimeError(f"{filename}: expected one replacement target, found {count}")
            corrected = corrected.replace(replacement["old"], replacement["new"], 1)
        destination.write_bytes(corrected.encode())
        if source.read_bytes() != raw:
            raise RuntimeError(f"raw source changed during generation: {source}")
        manifest["files"][filename] = {
            "source": name(source), "source_sha256": digest(raw),
            "corrected": name(destination), "corrected_sha256": digest(destination.read_bytes()),
            "replacements": replacements,
        }
    (args.output_dir / "set_baseline_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
