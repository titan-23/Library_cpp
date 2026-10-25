#!/usr/bin/env python3
"""Cache whether a lazy action is present in existing Data padding.

Generate a candidate without editing the source. Supports both fast and leaf
layouts; the public tree API is unchanged. --source and --output are required.
"""
import argparse
import re
from pathlib import Path


def prepare(source):
    if 'bool has_lazy;' in source:
        raise ValueError('input already has the lazy-presence cache')
    source = source.replace('            int8_t rev;\n', '            int8_t rev;\n            bool has_lazy;\n', 1)
    source = source.replace('Data() : rev(0) {}', 'Data() : rev(0), has_lazy(false) {}', 1)
    source = source.replace('lazy(f), rev(r) {}', 'lazy(f), rev(r), has_lazy(f != id()) {}', 1)
    # First rewrite readers, then add maintenance that still compares F.
    source = re.sub(r'(ma\.pool\[[^\]]+\]\.data)\.lazy != id\(\)', r'\1.has_lazy', source)
    source = re.sub(r'(ma\.pool\[[^\]]+\]\.data)\.lazy = id\(\);',
                    r'\1.lazy = id(); \1.has_lazy = false;', source)
    old = '''        if (ma.pool[node].tree.size > 1)
            ma.pool[node].data.lazy = composition(f, ma.pool[node].data.lazy);'''
    if old in source:
        source = source.replace(old, '''        if (ma.pool[node].tree.size > 1) {
            ma.pool[node].data.lazy = composition(f, ma.pool[node].data.lazy);
            ma.pool[node].data.has_lazy = ma.pool[node].data.lazy != id();
        }''', 1)
    else:
        source = source.replace('''        ma.pool[node].data.lazy = composition(f, ma.pool[node].data.lazy);''', '''        ma.pool[node].data.lazy = composition(f, ma.pool[node].data.lazy);
        ma.pool[node].data.has_lazy = ma.pool[node].data.lazy != id();''', 1)
    return source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.write_text(prepare(args.source.read_text()))
    print(args.output)


if __name__ == '__main__':
    main()
