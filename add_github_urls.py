#!/usr/bin/env python3
"""Add each titan_cpplib C++ file's GitHub URL to its first line."""

from __future__ import annotations

from codecs import BOM_UTF8
from pathlib import Path
from urllib.parse import quote


PROJECT_ROOT = Path(__file__).resolve().parent
TARGET_DIRECTORY = PROJECT_ROOT / "titan_cpplib"
GITHUB_REPOSITORY_URL = "https://github.com/titan-23/Library_cpp"
GITHUB_BLOB_URL = f"{GITHUB_REPOSITORY_URL}/blob/main"


def github_header(path: Path) -> bytes:
    relative_path = path.relative_to(PROJECT_ROOT).as_posix()
    url = f"{GITHUB_BLOB_URL}/{quote(relative_path, safe='/')}"
    return f"/// {url}".encode("utf-8")


def newline_used_by(data: bytes) -> bytes:
    first_lf = data.find(b"\n")
    if first_lf > 0 and data[first_lf - 1 : first_lf + 1] == b"\r\n":
        return b"\r\n"
    return b"\n"


def is_replaceable_url_header(line: bytes) -> bool:
    if not line.startswith(b"//"):
        return False
    comment = line.lstrip(b"/").lstrip()
    if line.startswith(b"///") and comment.startswith((b"http://", b"https://")):
        return True
    return comment.startswith(GITHUB_REPOSITORY_URL.encode("ascii"))


def replace_first_line(body: bytes, header: bytes) -> bytes:
    first_lf = body.find(b"\n")
    if first_lf == -1:
        return header + b"\n"

    newline = b"\r\n" if body[first_lf - 1 : first_lf + 1] == b"\r\n" else b"\n"
    return header + newline + body[first_lf + 1 :]


def add_header(path: Path) -> bool:
    original = path.read_bytes()
    bom = BOM_UTF8 if original.startswith(BOM_UTF8) else b""
    body = original[len(bom) :]
    header = github_header(path)

    first_line = body.splitlines()[0] if body else b""
    if first_line == header:
        return False

    if is_replaceable_url_header(first_line):
        updated = bom + replace_first_line(body, header)
    else:
        updated = bom + header + newline_used_by(body) + body

    path.write_bytes(updated)
    return True


def main() -> None:
    if not TARGET_DIRECTORY.is_dir():
        raise SystemExit(f"Directory not found: {TARGET_DIRECTORY}")

    changed = 0
    unchanged = 0
    for path in sorted(TARGET_DIRECTORY.rglob("*.cpp")):
        if add_header(path):
            changed += 1
            print(f"[updated] {path.relative_to(PROJECT_ROOT)}")
        else:
            unchanged += 1

    print(f"Updated: {changed}, unchanged: {unchanged}")


if __name__ == "__main__":
    main()
